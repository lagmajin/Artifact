module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>
#include <QColor>
#include <cmath>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Rasterizer.Bevel;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

using namespace ArtifactCore;

class BevelEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float strength_ = 1.0f;
    float softness_ = 2.0f;
    bool edgeMode_ = false; // false = alpha bevel, true = edge bevel
    QColor highlightColor_ = QColor(255, 255, 255, 255);
    QColor shadowColor_ = QColor(0, 0, 0, 255);

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) {
            dst = src;
            return;
        }
        dst = src;
        cv::Mat mat(dst.image().height(), dst.image().width(), CV_32FC4, dst.image().rgba32fData());

        std::vector<cv::Mat> channels;
        cv::split(mat, channels);
        cv::Mat color;
        cv::Mat alpha = channels[3];
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);

        // grayscale for edge detection
        cv::Mat srcGray;
        {
            std::vector<cv::Mat> tmp;
            cv::split(color, tmp);
            srcGray = tmp[0] * 0.299f + tmp[1] * 0.587f + tmp[2] * 0.114f;
            if (!alpha.empty()) srcGray = srcGray.mul(alpha);
        }

        const int ksize = std::max(1, static_cast<int>(std::round(softness_ * 2.0f + 1.0f))) | 1;
        cv::Mat blurred;
        cv::GaussianBlur(srcGray, blurred, cv::Size(ksize, ksize), softness_, softness_, cv::BORDER_REPLICATE);
        cv::Mat edge = srcGray - blurred;
        cv::Mat highlight, shadow;
        cv::threshold(edge, highlight, 0.0, 1.0, cv::THRESH_BINARY);
        cv::threshold(edge, shadow, 0.0, 1.0, cv::THRESH_BINARY_INV);
        cv::GaussianBlur(highlight, highlight, cv::Size(ksize, ksize), softness_, softness_, cv::BORDER_REPLICATE);
        cv::GaussianBlur(shadow, shadow, cv::Size(ksize, ksize), softness_, softness_, cv::BORDER_REPLICATE);

        cv::Mat result = color.clone();
        cv::Mat hc(highlight.size(), CV_32FC3);
        cv::Mat sc(shadow.size(), CV_32FC3);
        ArtifactCore::Parallel::For(0, hc.rows, [&](int y) {
            for (int x = 0; x < hc.cols; ++x) {
                float h = std::clamp(highlight.at<float>(y, x) * strength_, 0.0f, 1.0f);
                float s = std::clamp(shadow.at<float>(y, x) * strength_, 0.0f, 1.0f);
                hc.at<cv::Vec3f>(y, x) = cv::Vec3f(
                    highlightColor_.blueF(),
                    highlightColor_.greenF(),
                    highlightColor_.redF()
                ) * h;
                sc.at<cv::Vec3f>(y, x) = cv::Vec3f(
                    shadowColor_.blueF(),
                    shadowColor_.greenF(),
                    shadowColor_.redF()
                ) * s;
            }
        });
        result = result + hc + sc;
        result = cv::max(cv::Mat::zeros(result.size(), result.type()), result);

        std::vector<cv::Mat> out;
        cv::split(result, out);
        out.push_back(alpha);
        cv::merge(out, mat);
    }
};

class BevelEffectGPUImpl : public ArtifactEffectImplBase {
public:
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
        Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
        if (!acquireSharedRenderDeviceForCurrentBackend(device, context)) { applyCPU(src, dst); return; }
        const auto& image = src.image(); const float* pixels = image.rgba32fData();
        if (!pixels || image.width() <= 0 || image.height() <= 0) { applyCPU(src, dst); return; }
        Diligent::TextureDesc desc{}; desc.Name = "Bevel/Input"; desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = image.width(); desc.Height = image.height(); desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
        desc.MipLevels = 1; desc.ArraySize = 1; desc.SampleCount = 1; desc.Usage = Diligent::USAGE_IMMUTABLE;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        Diligent::TextureSubResData sub{}; sub.pData = pixels; sub.Stride = static_cast<Diligent::Uint64>(image.width()) * sizeof(float) * 4ull;
        Diligent::TextureData init{}; init.pSubResources = &sub; init.NumSubresources = 1;
        Diligent::RefCntAutoPtr<Diligent::ITexture> input; device->CreateTexture(desc, &init, &input);
        if (!input) { applyCPU(src, dst); return; }
        Diligent::TextureDesc outDesc = desc; outDesc.Name = "Bevel/Output"; outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE | Diligent::BIND_UNORDERED_ACCESS;
        Diligent::RefCntAutoPtr<Diligent::ITexture> output; device->CreateTexture(outDesc, nullptr, &output);
        if (!output) { applyCPU(src, dst); return; }
        struct Params { float strength, softness, edgeMode, pad; float highlight[3], highlightPad; float shadow[3], shadowPad; };
        Diligent::BufferDesc bd{}; bd.Name = "Bevel/Params"; bd.Size = sizeof(Params); bd.Usage = Diligent::USAGE_DYNAMIC;
        bd.BindFlags = Diligent::BIND_UNIFORM_BUFFER; bd.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        Diligent::RefCntAutoPtr<Diligent::IBuffer> params; device->CreateBuffer(bd, nullptr, &params);
        if (!params) { applyCPU(src, dst); return; }
        void* mapped = nullptr; context->MapBuffer(params, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
        if (!mapped) { applyCPU(src, dst); return; }
        Params p{cpuImpl_.strength_, cpuImpl_.softness_, cpuImpl_.edgeMode_ ? 1.0f : 0.0f, 0.0f,
            {cpuImpl_.highlightColor_.blueF(), cpuImpl_.highlightColor_.greenF(), cpuImpl_.highlightColor_.redF()}, 0.0f,
            {cpuImpl_.shadowColor_.blueF(), cpuImpl_.shadowColor_.greenF(), cpuImpl_.shadowColor_.redF()}, 0.0f};
        std::memcpy(mapped, &p, sizeof(p)); context->UnmapBuffer(params, Diligent::MAP_WRITE);
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "BevelParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        ArtifactCore::GpuContext gpuContext{device, context}; ArtifactCore::ComputeExecutor executor{gpuContext};
        ArtifactCore::ComputePipelineDesc pd{}; pd.name = "Bevel/PSO"; pd.shaderSource = kHlsl; pd.entryPoint = "main";
        pd.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; pd.variables = vars; pd.variableCount = 3;
        pd.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        if (!executor.build(pd) || !executor.createShaderResourceBinding(true) || !executor.setBuffer("BevelParams", params) ||
            !executor.setTextureView("g_InputTexture", input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor.setTextureView("g_OutputTexture", output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src, dst); return; }
        executor.dispatch(context, ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Diligent::TextureDesc sd = outDesc; sd.Name = "Bevel/Readback"; sd.Usage = Diligent::USAGE_STAGING; sd.BindFlags = Diligent::BIND_NONE; sd.CPUAccessFlags = Diligent::CPU_ACCESS_READ;
        Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(sd, nullptr, &staging); if (!staging) { applyCPU(src, dst); return; }
        context->CopyTexture(Diligent::CopyTextureAttribs(output, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION)); context->Flush(); context->WaitForIdle();
        Diligent::MappedTextureSubresource read{}; context->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, read);
        if (!read.pData || !read.Stride) { applyCPU(src, dst); return; }
        cv::Mat result(image.height(), image.width(), CV_32FC4, read.pData, read.Stride); dst.image().setFromCVMat(result, image.colorDescriptor()); context->UnmapTextureSubresource(staging, 0, 0);
    }
private:
    static constexpr const char* kHlsl = R"(
Texture2D<float4> g_InputTexture:register(t0); RWTexture2D<float4> g_OutputTexture:register(u0);
cbuffer BevelParams:register(b0){float g_Strength;float g_Softness;float g_EdgeMode;float g_Pad;float3 g_Highlight;float3 g_Shadow;}
float lum(float4 p){return dot(p.rgb,float3(0.114,0.587,0.299))*p.a;} float4 atp(int2 p,uint w,uint h){return g_InputTexture[uint2(clamp(p.x,0,(int)w-1),clamp(p.y,0,(int)h-1))];}
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;int2 p=int2(id.xy);float c=lum(atp(p,w,h));float e=c-(lum(atp(p+int2(-1,0),w,h))+lum(atp(p+int2(1,0),w,h))+lum(atp(p+int2(0,-1),w,h))+lum(atp(p+int2(0,1),w,h)))*0.25;float hi=saturate(e*g_Strength*max(0.25,g_Softness)),sh=saturate(-e*g_Strength*max(0.25,g_Softness));float4 px=g_InputTexture[id.xy];px.rgb=max(0,px.rgb+g_Highlight*hi+g_Shadow*sh);g_OutputTexture[id.xy]=px;}
)";
public:
    BevelEffectCPUImpl cpuImpl_;
};

BevelEffect::BevelEffect() {
    setDisplayName(UniString("Bevel"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<BevelEffectCPUImpl>());
    setGPUImpl(std::make_shared<BevelEffectGPUImpl>());
}
BevelEffect::~BevelEffect() = default;

float BevelEffect::strength() const { return strength_; }
void BevelEffect::setStrength(float v) { strength_ = std::clamp(v, 0.0f, 5.0f); syncImpls(); }
float BevelEffect::softness() const { return softness_; }
void BevelEffect::setSoftness(float v) { softness_ = std::max(0.0f, v); syncImpls(); }
bool BevelEffect::edgeMode() const { return edgeMode_; }
void BevelEffect::setEdgeMode(bool v) { edgeMode_ = v; syncImpls(); }

void BevelEffect::syncImpls() {
    if (auto* c = dynamic_cast<BevelEffectCPUImpl*>(cpuImpl().get())) {
        c->strength_ = strength_;
        c->softness_ = softness_;
        c->edgeMode_ = edgeMode_;
    }
    if (auto* g = dynamic_cast<BevelEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.strength_ = strength_;
        g->cpuImpl_.softness_ = softness_;
        g->cpuImpl_.edgeMode_ = edgeMode_;
    }
}

std::vector<AbstractProperty> BevelEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& a = props.emplace_back(); a.setName("Strength"); a.setType(PropertyType::Float); a.setValue(strength_);
    auto& s = props.emplace_back(); s.setName("Softness"); s.setType(PropertyType::Float); s.setValue(softness_);
    auto& e = props.emplace_back(); e.setName("Edge Mode"); e.setType(PropertyType::Boolean); e.setValue(edgeMode_);
    return props;
}

void BevelEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Strength") setStrength(v.toFloat());
    else if (k == "Softness") setSoftness(v.toFloat());
    else if (k == "Edge Mode") setEdgeMode(v.toBool());
}

}
