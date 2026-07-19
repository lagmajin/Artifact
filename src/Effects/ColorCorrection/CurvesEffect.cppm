module;
#include <utility>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <cstring>
#include <vector>
#include <QVariant>
#include <QString>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module CurvesEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ColorCollection.ColorGrading;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Core.Parallel;

namespace Artifact {

class CurvesEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ColorCurves curves_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        Parallel::For(0, height, [&](int y) {
            auto curves = curves_;
            curves.process(pixels + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u, width, 1);
        });
    }
};

class CurvesEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ColorCurves curves_;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> lutTexture_;
    bool pipelineReady_ = false;
    bool lutDirty_ = true;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        Parallel::For(0, height, [&](int y) {
            auto curves = curves_;
            curves.process(pixels + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u, width, 1);
        });
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) {
            applyCPU(src, dst);
            return;
        }
        struct Guard {
            CurvesEffectGPUImpl* self{};
            ~Guard() {
                if (self) {
                    self->context_.Release();
                    self->device_.Release();
                    releaseSharedRenderDevice();
                }
            }
        } guard{this};

        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);

        if (lutDirty_ || !lutTexture_) {
            if (!buildLUTTexture()) {
                applyCPU(src, dst);
                return;
            }
        }

        if (!pipelineReady_) {
            static Diligent::ShaderResourceVariableDesc vars[] = {
                {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
                {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
                {Diligent::SHADER_TYPE_COMPUTE, "g_LUTTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            };

            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "Curves/PSO";
            desc.shaderSource = kCurvesHlsl;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor->build(desc) || !executor->createShaderResourceBinding(true)) {
                applyCPU(src, dst);
                return;
            }
            pipelineReady_ = true;
        }

        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex, "Curves/InputTexture")) {
            applyCPU(src, dst);
            return;
        }
        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "Curves/OutputTexture";
        Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex;
        device_->CreateTexture(outDesc, nullptr, &outputTex);
        if (!outputTex) {
            applyCPU(src, dst);
            return;
        }

        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS)) ||
            !executor->setTextureView("g_LUTTexture", lutTexture_->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))) {
            applyCPU(src, dst);
            return;
        }

        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (!readbackTexture(device_, context_, outputTex, dst, "Curves/StagingTexture")) {
            applyCPU(src, dst);
        }
        dst.image().setColorDescriptor(src.image().colorDescriptor());
    }

    void syncCurves(const ArtifactCore::ColorCurves& curves) {
        curves_ = curves;
        lutDirty_ = true;
    }

private:
    bool buildLUTTexture() {
        std::array<float, 256 * 4> lut{};
        if (!curves_.isDefault()) {
            curves_.buildLUT();
        }
        for (int i = 0; i < 256; ++i) {
            const float x = static_cast<float>(i) / 255.0f;
            lut[i * 4 + 0] = curves_.evaluateRed(curves_.evaluateMaster(x));
            lut[i * 4 + 1] = curves_.evaluateGreen(curves_.evaluateMaster(x));
            lut[i * 4 + 2] = curves_.evaluateBlue(curves_.evaluateMaster(x));
            lut[i * 4 + 3] = 1.0f;
        }
        Diligent::TextureDesc desc;
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = 256;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.SampleCount = 1;
        desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        desc.Name = "Curves/LUTTexture";
        Diligent::TextureSubResData sub{};
        sub.pData = lut.data();
        sub.Stride = sizeof(float) * 4ull * 256ull;
        Diligent::TextureData init{};
        init.pSubResources = &sub;
        init.NumSubresources = 1;
        device_->CreateTexture(desc, &init, &lutTexture_);
        lutDirty_ = false;
        return lutTexture_ != nullptr;
    }

    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src,
                                       Diligent::IRenderDevice* device,
                                       Diligent::ITexture** outTex,
                                       const char* name)
    {
        if (!device || !outTex) {
            return false;
        }
        const auto& img = src.image();
        const float* data = img.rgba32fData();
        if (!data || img.width() <= 0 || img.height() <= 0) {
            return false;
        }
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

    static bool readbackTexture(Diligent::IRenderDevice* device,
                                Diligent::IDeviceContext* ctx,
                                Diligent::ITexture* src,
                                ImageF32x4RGBAWithCache& dst,
                                const char* name)
    {
        if (!device || !ctx || !src) {
            return false;
        }
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
        if (!staging) {
            return false;
        }
        Diligent::CopyTextureAttribs copy(src, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                          staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->CopyTexture(copy);
        Diligent::MappedTextureSubresource mapped{};
        ctx->Flush();
        ctx->WaitForIdle();
        ctx->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, mapped);
        if (!mapped.pData || mapped.Stride == 0) {
            return false;
        }
        cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride);
        dst.image().setFromCVMat(temp);
        ctx->UnmapTextureSubresource(staging, 0, 0);
        return true;
    }

    static constexpr const char* kCurvesHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
Texture2D<float4> g_LUTTexture : register(t1);
RWTexture2D<float4> g_OutputTexture : register(u0);

float sampleLUT(float value, int channel)
{
    value = saturate(value);
    float x = value * 255.0f;
    int x0 = (int)floor(x);
    int x1 = min(x0 + 1, 255);
    float t = x - x0;
    float a = g_LUTTexture.Load(int3(x0, 0, 0))[channel];
    float b = g_LUTTexture.Load(int3(x1, 0, 0))[channel];
    return lerp(a, b, t);
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dtid.x >= width || dtid.y >= height) return;

    float4 px = g_InputTexture[dtid.xy];
    px.r = sampleLUT(px.r, 0);
    px.g = sampleLUT(px.g, 1);
    px.b = sampleLUT(px.b, 2);
    g_OutputTexture[dtid.xy] = px;
}
)";
};

namespace {
std::vector<ArtifactCore::CurvePoint> makeSCurvePoints(float strength) {
    const float s = std::clamp(strength, 0.0f, 1.0f);
    const float lowY = 0.25f - 0.15f * s;
    const float highY = 0.75f + 0.15f * s;
    return {
        {0.0f, 0.0f},
        {0.25f, std::clamp(lowY, 0.0f, 1.0f)},
        {0.50f, 0.50f},
        {0.75f, std::clamp(highY, 0.0f, 1.0f)},
        {1.0f, 1.0f},
    };
}
}

CurvesEffect::CurvesEffect() {
    setEffectID(UniString("effect.colorcorrection.curves"));
    setDisplayName(UniString("Curves"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<CurvesEffectCPUImpl>());
    setGPUImpl(std::make_shared<CurvesEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    syncImpls();
}

CurvesEffect::~CurvesEffect() = default;

void CurvesEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 5);
    preset_ = static_cast<Preset>(clamped);
    syncImpls();
}

void CurvesEffect::setStrength(float strength) {
    strength_ = std::clamp(strength, 0.0f, 1.0f);
    syncImpls();
}

void CurvesEffect::setPosterizeLevels(int levels) {
    posterizeLevels_ = std::max(2, levels);
    syncImpls();
}

void CurvesEffect::applyPreset(ArtifactCore::ColorCurves& curves) const {
    curves.reset();
    switch (preset_) {
    case Preset::Identity:
        break;
    case Preset::SCurve:
        curves.setMasterCurve(makeSCurvePoints(strength_));
        break;
    case Preset::FadeIn:
        curves.applyFadeIn();
        break;
    case Preset::FadeOut:
        curves.applyFadeOut();
        break;
    case Preset::Invert:
        curves.applyInvert();
        break;
    case Preset::Posterize:
        curves.applyPosterize(std::max(2, posterizeLevels_));
        break;
    }
}

void CurvesEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<CurvesEffectCPUImpl*>(cpuImpl().get())) {
        applyPreset(cpu->curves_);
    }
    if (auto* gpu = dynamic_cast<CurvesEffectGPUImpl*>(gpuImpl().get())) {
        applyPreset(gpu->curves_);
        gpu->syncCurves(gpu->curves_);
    }
}

std::vector<AbstractProperty> CurvesEffect::getProperties() const {
    std::vector<AbstractProperty> props;

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    props.push_back(presetProp);

    AbstractProperty strengthProp;
    strengthProp.setName("Strength");
    strengthProp.setType(PropertyType::Float);
    strengthProp.setValue(QVariant(static_cast<double>(strength_)));
    props.push_back(strengthProp);

    AbstractProperty posterizeProp;
    posterizeProp.setName("Posterize Levels");
    posterizeProp.setType(PropertyType::Integer);
    posterizeProp.setValue(posterizeLevels_);
    props.push_back(posterizeProp);

    return props;
}

void CurvesEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QString("Preset")) {
        setPreset(value.toInt());
    } else if (key == QString("Strength")) {
        setStrength(value.toFloat());
    } else if (key == QString("Posterize Levels")) {
        setPosterizeLevels(value.toInt());
    }
}

} // namespace Artifact
