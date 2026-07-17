module;
#include <utility>
#include <cmath>
#include <memory>
#include <QList>
#include <opencv2/opencv.hpp>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module HueAndSaturation;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Core.Parallel;

namespace Artifact {

class HueAndSaturationCPUImpl : public ArtifactEffectImplBase {
public:
    float hueShift_ = 0.0f;
    float saturationScale_ = 1.0f;
    float lightnessShift_ = 0.0f;
    bool colorize_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* srcData = srcImage.rgba32fData();
        if (!srcData) {
            dst = src;
            return;
        }

        cv::Mat floatMat(srcImage.height(), srcImage.width(), CV_32FC4, const_cast<float*>(srcData));

        cv::Mat alpha;
        cv::Mat bgr = floatMat.clone();
        if (bgr.channels() != 4) {
            return;
        }
        std::vector<cv::Mat> channels;
        cv::split(bgr, channels);
        alpha = channels[3];
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, bgr);

        cv::Mat hsv;
        cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);

        ArtifactCore::Parallel::For(0, hsv.rows, [&](int y) {
            for (int x = 0; x < hsv.cols; ++x) {
                cv::Vec3f& pixel = hsv.at<cv::Vec3f>(y, x);
                if (colorize_) {
                    pixel[0] = std::fmod(hueShift_ + 360.0f, 360.0f);
                    pixel[1] = std::clamp(saturationScale_, 0.0f, 2.0f);
                    pixel[2] = std::clamp(pixel[2] + lightnessShift_, 0.0f, 1.0f);
                } else {
                    pixel[0] = std::fmod(pixel[0] + hueShift_ + 360.0f, 360.0f);
                    pixel[1] = std::clamp(pixel[1] * saturationScale_, 0.0f, 2.0f);
                    pixel[2] = std::clamp(pixel[2] + lightnessShift_, 0.0f, 1.0f);
                }
            }
        });

        cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
        cv::merge(std::vector<cv::Mat>{bgr, alpha}, floatMat);
        dst.image().setFromRGBA32F(floatMat.ptr<float>(), floatMat.cols, floatMat.rows);
    }
};

class HueAndSaturationGPUImpl : public ArtifactEffectImplBase {
public:
    float hueShift_ = 0.0f;
    float saturationScale_ = 1.0f;
    float lightnessShift_ = 0.0f;
    bool colorize_ = false;
    HueAndSaturationCPUImpl cpuFallback_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuFallback_.hueShift_ = hueShift_;
        cpuFallback_.saturationScale_ = saturationScale_;
        cpuFallback_.lightnessShift_ = lightnessShift_;
        cpuFallback_.colorize_ = colorize_;
        cpuFallback_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) {
            applyCPU(src, dst);
            return;
        }
        const auto lease = [this]() {
            struct Guard {
                HueAndSaturationGPUImpl* self;
                ~Guard() {
                    if (self) {
                        self->context_.Release();
                        self->device_.Release();
                        releaseSharedRenderDevice();
                    }
                }
            };
            return Guard{this};
        }();

        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);

        if (!paramsCB_) {
            Diligent::BufferDesc cbDesc;
            cbDesc.Name = "HueSat/ParamsCB";
            cbDesc.Size = sizeof(ParamsCB);
            cbDesc.Usage = Diligent::USAGE_DYNAMIC;
            cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
            cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
            device_->CreateBuffer(cbDesc, nullptr, &paramsCB_);
        }
        if (!paramsCB_) {
            applyCPU(src, dst);
            return;
        }

        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "HueSatParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "HueSat/PSO";
            desc.shaderSource = kHueSatHlsl;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor->build(desc) || !executor->createShaderResourceBinding(true) ||
                !executor->setBuffer("HueSatParams", paramsCB_)) {
                applyCPU(src, dst);
                return;
            }
            pipelineReady_ = true;
        }

        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex, "HueSat/InputTexture")) {
            applyCPU(src, dst);
            return;
        }
        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "HueSat/OutputTexture";
        Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex;
        device_->CreateTexture(outDesc, nullptr, &outputTex);
        if (!outputTex) {
            applyCPU(src, dst);
            return;
        }
        void* mapped = nullptr;
        context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
        if (!mapped) {
            applyCPU(src, dst);
            return;
        }
        ParamsCB params{};
        params.hueShift = hueShift_;
        params.saturationScale = saturationScale_;
        params.lightnessShift = lightnessShift_;
        params.colorize = colorize_ ? 1.0f : 0.0f;
        std::memcpy(mapped, &params, sizeof(params));
        context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }
        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex, dst, "HueSat/StagingTexture")) {
            applyCPU(src, dst);
        }
    }

    void setHue(float v) { hueShift_ = v; cpuFallback_.hueShift_ = v; }
    void setSaturation(float v) { saturationScale_ = v; cpuFallback_.saturationScale_ = v; }
    void setLightness(float v) { lightnessShift_ = v; cpuFallback_.lightnessShift_ = v; }
    void setColorize(bool v) { colorize_ = v; cpuFallback_.colorize_ = v; }

private:
    struct ParamsCB {
        float hueShift = 0.0f;
        float saturationScale = 1.0f;
        float lightnessShift = 0.0f;
        float colorize = 0.0f;
    };

    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex, const char* name)
    {
        if (!device || !outTex) return false;
        const auto& img = src.image();
        const float* data = img.rgba32fData();
        if (!data || img.width() <= 0 || img.height() <= 0) return false;
        Diligent::TextureDesc desc;
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = static_cast<Diligent::Uint32>(img.width());
        desc.Height = static_cast<Diligent::Uint32>(img.height());
        desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleCount = 1;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        desc.Name = name;
        Diligent::TextureSubResData sub{};
        sub.pData = data;
        sub.Stride = static_cast<Diligent::Uint64>(img.width()) * sizeof(float) * 4ull;
        Diligent::TextureData init{};
        init.pSubResources = &sub;
        init.NumSubresources = 1;
        device->CreateTexture(desc, &init, outTex);
        return *outTex != nullptr;
    }
    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst, const char* name)
    {
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
        stagingDesc.Name = name;
        Diligent::RefCntAutoPtr<Diligent::ITexture> staging;
        device->CreateTexture(stagingDesc, nullptr, &staging);
        if (!staging) return false;
        Diligent::CopyTextureAttribs copy(src, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
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
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    bool pipelineReady_ = false;
    static constexpr const char* kHueSatHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer HueSatParams : register(b0) { float g_HueShift; float g_SaturationScale; float g_LightnessShift; float g_Colorize; };
float3 rgb2hsv(float3 c)
{
    float4 K = float4(0.0, -1.0/3.0, 2.0/3.0, -1.0);
    float4 p = (c.g < c.b) ? float4(c.bg, K.wz) : float4(c.gb, K.xy);
    float4 q = (c.r < p.x) ? float4(p.xyw, c.r) : float4(c.r, p.yzx);
    float d = q.x - min(q.w, q.y);
    float e = 1e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}
float3 hsv2rgb(float3 c)
{
    float4 K = float4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y);
}
[numthreads(8,8,1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    g_OutputTexture.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;
    float4 px = g_InputTexture[dtid.xy];
    float3 hsv = rgb2hsv(px.rgb);
    if (g_Colorize > 0.5f) {
        hsv.x = frac((g_HueShift + 360.0f) / 360.0f);
        hsv.y = saturate(g_SaturationScale);
    } else {
        hsv.x = frac(hsv.x + g_HueShift / 360.0f);
        hsv.y = saturate(hsv.y * g_SaturationScale);
    }
    hsv.z = saturate(hsv.z + g_LightnessShift);
    px.rgb = hsv2rgb(hsv);
    g_OutputTexture[dtid.xy] = px;
}
)";
    HueAndSaturationCPUImpl cpuImpl_;
};

HueAndSaturation::HueAndSaturation() {
    setEffectID(UniString("effect.colorcorrection.hsl"));
    setDisplayName(UniString("Hue / Saturation"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<HueAndSaturationCPUImpl>());
    setGPUImpl(std::make_shared<HueAndSaturationGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

HueAndSaturation::~HueAndSaturation() = default;

void HueAndSaturation::syncImpls() {
    if (auto* cpu = dynamic_cast<HueAndSaturationCPUImpl*>(cpuImpl().get())) {
        cpu->hueShift_ = hueShift_;
        cpu->saturationScale_ = saturationScale_;
        cpu->lightnessShift_ = lightnessShift_;
        cpu->colorize_ = colorize_;
    }
    if (auto* gpu = dynamic_cast<HueAndSaturationGPUImpl*>(gpuImpl().get())) {
        gpu->setHue(hueShift_);
        gpu->setSaturation(saturationScale_);
        gpu->setLightness(lightnessShift_);
        gpu->setColorize(colorize_);
    }
}

std::vector<AbstractProperty> HueAndSaturation::getProperties() const {
    std::vector<AbstractProperty> props(4);

    props[0].setName("Hue");
    props[0].setType(PropertyType::Float);
    props[0].setValue(QVariant(static_cast<double>(hueShift_)));

    props[1].setName("Saturation");
    props[1].setType(PropertyType::Float);
    props[1].setValue(QVariant(static_cast<double>(saturationScale_)));

    props[2].setName("Lightness");
    props[2].setType(PropertyType::Float);
    props[2].setValue(QVariant(static_cast<double>(lightnessShift_)));

    props[3].setName("Colorize");
    props[3].setType(PropertyType::Boolean);
    props[3].setValue(colorize_);

    return props;
}

void HueAndSaturation::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QString("Hue")) {
        hueShift_ = std::clamp(value.toFloat(), -180.0f, 180.0f);
        syncImpls();
    } else if (key == QString("Saturation")) {
        saturationScale_ = std::clamp(value.toFloat(), 0.0f, 2.0f);
        syncImpls();
    } else if (key == QString("Lightness")) {
        lightnessShift_ = std::clamp(value.toFloat(), -1.0f, 1.0f);
        syncImpls();
    } else if (key == QString("Colorize")) {
        colorize_ = value.toBool();
        syncImpls();
    }
}

} // namespace Artifact
