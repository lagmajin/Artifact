module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>
#include <cstring>
#include <cmath>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Kaleidoscope;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

using namespace ArtifactCore;

static constexpr float kPI = 3.14159265359f;

class KaleidoscopeEffectCPUImpl : public ArtifactEffectImplBase {
public:
    int segments_ = 6;
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float rotation_ = 0.0f;
    float zoom_ = 1.0f;
    float feather_ = 0.0f;
    bool mirror_ = true;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) { dst = src; return; }
        const int w = srcImage.width();
        const int h = srcImage.height();
        dst = src;
        auto& dstImage = dst.image();
        float* dstPixels = dstImage.rgba32fData();
        if (!dstPixels) return;

        const float cx = centerX_ * w;
        const float cy = centerY_ * h;
        const float segAngle = 2.0f * kPI / std::max(2, segments_);
        const float rotRad = rotation_ * kPI / 180.0f;

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float dx = float(x) - cx;
                float dy = float(y) - cy;
                float r = std::sqrt(dx * dx + dy * dy);
                float theta = std::atan2(dy, dx);
                float folded = std::fmod(theta, segAngle);
                if (folded < 0) folded += segAngle;
                int segment = int(std::floor(theta / segAngle));
                if (theta < 0) segment = int(std::ceil(theta / segAngle)) - 1;

                float srcAngle;
                if (mirror_) {
                    folded = segAngle * 0.5f - std::abs(folded - segAngle * 0.5f);
                    srcAngle = folded;
                } else {
                    srcAngle = folded + segAngle * segment;
                }
                srcAngle += rotRad;

                float srcR = r / std::max(zoom_, 0.01f);
                float sx = cx + srcR * std::cos(srcAngle);
                float sy = cy + srcR * std::sin(srcAngle);

                int si = std::clamp(int(std::round(sx)), 0, w - 1);
                int sj = std::clamp(int(std::round(sy)), 0, h - 1);
                int srcIdx = (sj * w + si) * 4;
                int dstIdx = (y * w + x) * 4;
                if (feather_ > 0.001f) {
                    float edgeDist = r;
                    float maxR = std::sqrt(cx * cx + cy * cy) * 1.5f;
                    float fade = 1.0f - std::clamp((edgeDist - maxR * (1.0f - feather_)) / std::max(maxR * feather_, 0.001f), 0.0f, 1.0f);
                    dstPixels[dstIdx + 0] = dstPixels[dstIdx + 0] * (1.0f - fade) + pixels[srcIdx + 0] * fade;
                    dstPixels[dstIdx + 1] = dstPixels[dstIdx + 1] * (1.0f - fade) + pixels[srcIdx + 1] * fade;
                    dstPixels[dstIdx + 2] = dstPixels[dstIdx + 2] * (1.0f - fade) + pixels[srcIdx + 2] * fade;
                    dstPixels[dstIdx + 3] = pixels[srcIdx + 3];
                } else {
                    dstPixels[dstIdx + 0] = pixels[srcIdx + 0];
                    dstPixels[dstIdx + 1] = pixels[srcIdx + 1];
                    dstPixels[dstIdx + 2] = pixels[srcIdx + 2];
                    dstPixels[dstIdx + 3] = pixels[srcIdx + 3];
                }
            }
        }
    }
};

class KaleidoscopeEffectGPUImpl : public ArtifactEffectImplBase {
public:
    KaleidoscopeEffectCPUImpl cpuImpl_;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    mutable bool pipelineReady_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src, dst); return; }
        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if (!paramsCB_) {
            Diligent::BufferDesc cbDesc;
            cbDesc.Name = "Kaleidoscope/ParamsCB";
            cbDesc.Size = sizeof(ParamsCB);
            cbDesc.Usage = Diligent::USAGE_DYNAMIC;
            cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
            cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
            device_->CreateBuffer(cbDesc, nullptr, &paramsCB_);
        }
        if (!paramsCB_) { applyCPU(src, dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "KaleidoscopeParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "Kaleidoscope/PSO";
            desc.shaderSource = kKaleidoscopeHlsl;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("KaleidoscopeParams", paramsCB_)) { applyCPU(src, dst); return; }
            pipelineReady_ = true;
        }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex)) { applyCPU(src, dst); return; }
        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "Kaleidoscope/OutputTexture";
        Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex;
        device_->CreateTexture(outDesc, nullptr, &outputTex);
        if (!outputTex) { applyCPU(src, dst); return; }

        void* mapped = nullptr;
        context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
        if (!mapped) { applyCPU(src, dst); return; }
        ParamsCB params{};
        params.segments = static_cast<float>(cpuImpl_.segments_);
        params.centerX = cpuImpl_.centerX_;
        params.centerY = cpuImpl_.centerY_;
        params.rotation = cpuImpl_.rotation_;
        params.zoom = cpuImpl_.zoom_;
        params.feather = cpuImpl_.feather_;
        params.mirror = cpuImpl_.mirror_ ? 1.0f : 0.0f;
        std::memcpy(mapped, &params, sizeof(params));
        context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);

        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src, dst); return; }
        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex, dst)) { applyCPU(src, dst); return; }
        dst.image().setColorDescriptor(src.image().colorDescriptor());
    }

private:
    struct ParamsCB {
        float segments = 6.0f;
        float centerX = 0.5f;
        float centerY = 0.5f;
        float rotation = 0.0f;
        float zoom = 1.0f;
        float feather = 0.0f;
        float mirror = 1.0f;
        float pad = 0.0f;
    };

    static constexpr const char* kKaleidoscopeHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer KaleidoscopeParams : register(b0) {
    float g_Segments, g_CenterX, g_CenterY, g_Rotation;
    float g_Zoom, g_Feather, g_Mirror, g_Pad;
};
static const float PI = 3.14159265;
float4 sampleBilinear(uint2 size, float2 uv) {
    float2 f = uv * float2(size);
    int2 p0 = int2(floor(f));
    int2 p1 = p0 + int2(1, 1);
    p0 = clamp(p0, int2(0, 0), int2(size - 1));
    p1 = clamp(p1, int2(0, 0), int2(size - 1));
    float2 t = frac(f);
    float4 c00 = g_InputTexture[p0];
    float4 c10 = g_InputTexture[int2(p1.x, p0.y)];
    float4 c01 = g_InputTexture[int2(p0.x, p1.y)];
    float4 c11 = g_InputTexture[p1];
    return lerp(lerp(c00, c10, t.x), lerp(c01, c11, t.x), t.y);
}
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint w, h;
    g_OutputTexture.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;
    float cx = g_CenterX * w;
    float cy = g_CenterY * h;
    float dx = float(dtid.x) - cx;
    float dy = float(dtid.y) - cy;
    float r = length(float2(dx, dy));
    float theta = atan2(dy, dx);
    float segAngle = 2.0 * PI / max(2.0, g_Segments);
    float folded = fmod(theta, segAngle);
    if (folded < 0) folded += segAngle;
    int seg = int(floor(theta / segAngle));
    float srcAngle;
    if (g_Mirror > 0.5) {
        folded = segAngle * 0.5 - abs(folded - segAngle * 0.5);
        srcAngle = folded;
    } else {
        srcAngle = folded + segAngle * seg;
    }
    srcAngle += g_Rotation * (PI / 180.0);
    float srcR = r / max(g_Zoom, 0.01);
    float sx = cx + srcR * cos(srcAngle);
    float sy = cy + srcR * sin(srcAngle);
    float2 uv = float2(sx / w, sy / h);
    float4 color = sampleBilinear(uint2(w, h), uv);
    if (g_Feather > 0.001) {
        float maxR = length(float2(max(cx, w - cx), max(cy, h - cy)));
        float fade = 1.0 - saturate((r - maxR * (1.0 - g_Feather)) / max(maxR * g_Feather, 0.001));
        float4 orig = g_InputTexture[dtid.xy];
        color = lerp(orig, color, fade);
    }
    g_OutputTexture[dtid.xy] = color;
}
)";

    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex) {
        const auto& img = src.image();
        const float* data = img.rgba32fData();
        if (!device || !outTex || !data || img.width() <= 0 || img.height() <= 0) return false;
        Diligent::TextureDesc desc;
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = img.width();
        desc.Height = img.height();
        desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleCount = 1;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        desc.Name = "Kaleidoscope/InputTexture";
        Diligent::TextureSubResData sub{};
        sub.pData = data;
        sub.Stride = static_cast<Diligent::Uint64>(img.width()) * sizeof(float) * 4ull;
        Diligent::TextureData init{};
        init.pSubResources = &sub;
        init.NumSubresources = 1;
        device->CreateTexture(desc, &init, outTex);
        return *outTex != nullptr;
    }

    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst) {
        if (!device || !ctx || !src) return false;
        const auto desc = src->GetDesc();
        Diligent::TextureDesc stagingDesc;
        stagingDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        stagingDesc.Width = desc.Width;
        stagingDesc.Height = desc.Height;
        stagingDesc.Format = desc.Format;
        stagingDesc.ArraySize = 1;
        stagingDesc.MipLevels = 1;
        stagingDesc.SampleCount = 1;
        stagingDesc.Usage = Diligent::USAGE_STAGING;
        stagingDesc.CPUAccessFlags = Diligent::CPU_ACCESS_READ;
        stagingDesc.Name = "Kaleidoscope/StagingTexture";
        Diligent::RefCntAutoPtr<Diligent::ITexture> staging;
        device->CreateTexture(stagingDesc, nullptr, &staging);
        if (!staging) return false;
        Diligent::CopyTextureAttribs copy(src, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                          staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->CopyTexture(copy);
        Diligent::MappedTextureSubresource mapped{};
        ctx->Flush();
        ctx->WaitForIdle();
        ctx->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, mapped);
        if (!mapped.pData || mapped.Stride == 0) return false;
        cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride);
        dst.image().setFromCVMat(temp);
        ctx->UnmapTextureSubresource(staging, 0, 0);
        return true;
    }
};

KaleidoscopeEffect::KaleidoscopeEffect() {
    setDisplayName(UniString("Kaleidoscope"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<KaleidoscopeEffectCPUImpl>());
    setGPUImpl(std::make_shared<KaleidoscopeEffectGPUImpl>());
}

KaleidoscopeEffect::~KaleidoscopeEffect() = default;

int KaleidoscopeEffect::segments() const { return segments_; }
void KaleidoscopeEffect::setSegments(int v) { segments_ = std::clamp(v, 2, 64); syncImpls(); }
float KaleidoscopeEffect::centerX() const { return centerX_; }
void KaleidoscopeEffect::setCenterX(float v) { centerX_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
float KaleidoscopeEffect::centerY() const { return centerY_; }
void KaleidoscopeEffect::setCenterY(float v) { centerY_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
float KaleidoscopeEffect::rotation() const { return rotation_; }
void KaleidoscopeEffect::setRotation(float v) { rotation_ = std::fmod(v, 360.0f); syncImpls(); }
float KaleidoscopeEffect::zoom() const { return zoom_; }
void KaleidoscopeEffect::setZoom(float v) { zoom_ = std::max(0.01f, v); syncImpls(); }
float KaleidoscopeEffect::feather() const { return feather_; }
void KaleidoscopeEffect::setFeather(float v) { feather_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
bool KaleidoscopeEffect::mirror() const { return mirror_; }
void KaleidoscopeEffect::setMirror(bool v) { mirror_ = v; syncImpls(); }

void KaleidoscopeEffect::syncImpls() {
    if (auto* c = dynamic_cast<KaleidoscopeEffectCPUImpl*>(cpuImpl().get())) {
        c->segments_ = segments_;
        c->centerX_ = centerX_;
        c->centerY_ = centerY_;
        c->rotation_ = rotation_;
        c->zoom_ = zoom_;
        c->feather_ = feather_;
        c->mirror_ = mirror_;
    }
    if (auto* g = dynamic_cast<KaleidoscopeEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.segments_ = segments_;
        g->cpuImpl_.centerX_ = centerX_;
        g->cpuImpl_.centerY_ = centerY_;
        g->cpuImpl_.rotation_ = rotation_;
        g->cpuImpl_.zoom_ = zoom_;
        g->cpuImpl_.feather_ = feather_;
        g->cpuImpl_.mirror_ = mirror_;
    }
}

std::vector<AbstractProperty> KaleidoscopeEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& s = props.emplace_back(); s.setName("Segments"); s.setType(PropertyType::Integer); s.setValue(segments_);
    auto& cx = props.emplace_back(); cx.setName("Center X"); cx.setType(PropertyType::Float); cx.setValue(centerX_);
    auto& cy = props.emplace_back(); cy.setName("Center Y"); cy.setType(PropertyType::Float); cy.setValue(centerY_);
    auto& r = props.emplace_back(); r.setName("Rotation"); r.setType(PropertyType::Float); r.setValue(rotation_);
    auto& z = props.emplace_back(); z.setName("Zoom"); z.setType(PropertyType::Float); z.setValue(zoom_);
    auto& f = props.emplace_back(); f.setName("Feather"); f.setType(PropertyType::Float); f.setValue(feather_);
    auto& m = props.emplace_back(); m.setName("Mirror"); m.setType(PropertyType::Boolean); m.setValue(mirror_);
    return props;
}

void KaleidoscopeEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Segments") setSegments(v.toInt());
    else if (k == "Center X") setCenterX(v.toFloat());
    else if (k == "Center Y") setCenterY(v.toFloat());
    else if (k == "Rotation") setRotation(v.toFloat());
    else if (k == "Zoom") setZoom(v.toFloat());
    else if (k == "Feather") setFeather(v.toFloat());
    else if (k == "Mirror") setMirror(v.toBool());
}

} // namespace Artifact
