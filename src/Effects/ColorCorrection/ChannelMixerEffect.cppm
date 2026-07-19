module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QVariant>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module ChannelMixerEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.ChannelMixer;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Core.Parallel;

namespace Artifact {

class ChannelMixerEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ChannelMixerProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        const int width = dst.image().width();
        const int height = dst.image().height();
        ArtifactCore::Parallel::For(0, height, [&](int y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                processor_.applyPixel(pixel[0], pixel[1], pixel[2]);
            }
        });
    }
};

class ChannelMixerEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ChannelMixerProcessor processor_;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    mutable bool pipelineReady_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        const int width = dst.image().width();
        const int height = dst.image().height();
        ArtifactCore::Parallel::For(0, height, [&](int y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                processor_.applyPixel(pixel[0], pixel[1], pixel[2]);
            }
        });
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) {
            applyCPU(src, dst);
            return;
        }

        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);

        if (!paramsCB_) {
            Diligent::BufferDesc cbDesc;
            cbDesc.Name = "ChannelMixer/ParamsCB";
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
            {Diligent::SHADER_TYPE_COMPUTE, "ChannelMixerParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };

        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "ChannelMixer/PSO";
            desc.shaderSource = kChannelMixerHlsl;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor->build(desc) || !executor->createShaderResourceBinding(true) ||
                !executor->setBuffer("ChannelMixerParams", paramsCB_)) {
                applyCPU(src, dst);
                return;
            }
            pipelineReady_ = true;
        }

        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex, "ChannelMixer/InputTexture")) {
            applyCPU(src, dst);
            return;
        }

        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "ChannelMixer/OutputTexture";
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
        params.strength = processor_.settings().strength;
        params.monochrome = processor_.settings().monochrome ? 1.0f : 0.0f;
        params.preserveLuma = processor_.settings().preserveLuma ? 1.0f : 0.0f;
        const auto& m = processor_.settings().matrix;
        params.matrix[0][0] = m[0][0];
        params.matrix[0][1] = m[0][1];
        params.matrix[0][2] = m[0][2];
        params.matrix[1][0] = m[1][0];
        params.matrix[1][1] = m[1][1];
        params.matrix[1][2] = m[1][2];
        params.matrix[2][0] = m[2][0];
        params.matrix[2][1] = m[2][1];
        params.matrix[2][2] = m[2][2];
        std::memcpy(mapped, &params, sizeof(params));
        context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);

        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }

        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (!readbackTexture(device_, context_, outputTex, dst, "ChannelMixer/StagingTexture")) {
            applyCPU(src, dst);
            return;
        }
        dst.image().setColorDescriptor(src.image().colorDescriptor());
    }

private:
    struct ParamsCB {
        float strength = 1.0f;
        float monochrome = 0.0f;
        float preserveLuma = 0.0f;
        float pad0 = 0.0f;
        float matrix[3][3]{};
    };

    static constexpr const char* kChannelMixerHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer ChannelMixerParams : register(b0)
{
    float g_Strength;
    float g_Monochrome;
    float g_PreserveLuma;
    float g_Pad0;
    float3x3 g_Matrix;
};

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dtid.x >= width || dtid.y >= height) return;

    float4 px = g_InputTexture[dtid.xy];
    float3 src = px.rgb;
    float3 mixed = mul(src, g_Matrix);

    if (g_Monochrome > 0.5f) {
        float luma = dot(src, float3(0.299f, 0.587f, 0.114f));
        mixed = luma.xxx;
    } else if (g_PreserveLuma > 0.5f) {
        float srcLuma = dot(src, float3(0.299f, 0.587f, 0.114f));
        float mixedLuma = dot(mixed, float3(0.299f, 0.587f, 0.114f));
        mixed += (srcLuma - mixedLuma).xxx;
    }

    px.rgb = lerp(src, mixed, saturate(g_Strength));
    g_OutputTexture[dtid.xy] = px;
}
)";

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
        const size_t rowBytes = static_cast<size_t>(desc.Width) * sizeof(float) * 4ull;
        if (mapped.Stride < rowBytes) {
            ctx->UnmapTextureSubresource(staging, 0, 0);
            return false;
        }
        cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride);
        dst.image().setFromCVMat(temp);
        ctx->UnmapTextureSubresource(staging, 0, 0);
        return true;
    }
};

ChannelMixerEffect::ChannelMixerEffect() {
    setEffectID(UniString("effect.colorcorrection.channelmixer"));
    setDisplayName(UniString("Channel Mixer"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ChannelMixerEffectCPUImpl>());
    setGPUImpl(std::make_shared<ChannelMixerEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

ChannelMixerEffect::~ChannelMixerEffect() = default;

void ChannelMixerEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Identity:
        settings_ = ArtifactCore::ChannelMixerSettings::identityMix();
        break;
    case Preset::Warm:
        settings_ = ArtifactCore::ChannelMixerSettings::warm();
        break;
    case Preset::Cool:
        settings_ = ArtifactCore::ChannelMixerSettings::cool();
        break;
    case Preset::CrossProcess:
        settings_ = ArtifactCore::ChannelMixerSettings::crossProcess();
        break;
    case Preset::Monochrome:
        settings_ = ArtifactCore::ChannelMixerSettings::monochromeMix();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void ChannelMixerEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 5);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void ChannelMixerEffect::setStrength(float value) {
    preset_ = Preset::Custom;
    settings_.strength = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void ChannelMixerEffect::setMonochrome(bool value) {
    preset_ = Preset::Custom;
    settings_.monochrome = value;
    syncImpls();
}

void ChannelMixerEffect::setPreserveLuma(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveLuma = value;
    syncImpls();
}

void ChannelMixerEffect::setMatrix(float rr, float rg, float rb,
                                   float gr, float gg, float gb,
                                   float br, float bg, float bb) {
    preset_ = Preset::Custom;
    settings_.matrix = {{{rr, rg, rb}, {gr, gg, gb}, {br, bg, bb}}};
    syncImpls();
}

void ChannelMixerEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<ChannelMixerEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<ChannelMixerEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> ChannelMixerEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(14);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    AbstractProperty strengthProp;
    strengthProp.setName("Strength");
    strengthProp.setType(PropertyType::Float);
    strengthProp.setValue(QVariant(static_cast<double>(settings_.strength)));
    strengthProp.setDisplayPriority(-25);
    props.push_back(strengthProp);

    auto addFloat = [&props](const char* name, float value, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };

    addFloat("Red From Red", settings_.matrix[0][0], -20);
    addFloat("Red From Green", settings_.matrix[0][1], -19);
    addFloat("Red From Blue", settings_.matrix[0][2], -18);
    addFloat("Green From Red", settings_.matrix[1][0], -10);
    addFloat("Green From Green", settings_.matrix[1][1], -9);
    addFloat("Green From Blue", settings_.matrix[1][2], -8);
    addFloat("Blue From Red", settings_.matrix[2][0], 0);
    addFloat("Blue From Green", settings_.matrix[2][1], 1);
    addFloat("Blue From Blue", settings_.matrix[2][2], 2);

    AbstractProperty monoProp;
    monoProp.setName("Monochrome");
    monoProp.setType(PropertyType::Boolean);
    monoProp.setValue(settings_.monochrome);
    monoProp.setDisplayPriority(10);
    props.push_back(monoProp);

    AbstractProperty lumaProp;
    lumaProp.setName("Preserve Luma");
    lumaProp.setType(PropertyType::Boolean);
    lumaProp.setValue(settings_.preserveLuma);
    lumaProp.setDisplayPriority(11);
    props.push_back(lumaProp);

    return props;
}

void ChannelMixerEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    } else if (key == QStringLiteral("Strength")) {
        setStrength(value.toFloat());
    } else if (key == QStringLiteral("Monochrome")) {
        setMonochrome(value.toBool());
    } else if (key == QStringLiteral("Preserve Luma")) {
        setPreserveLuma(value.toBool());
    } else if (key == QStringLiteral("Red From Red")) {
        setMatrix(value.toFloat(), settings_.matrix[0][1], settings_.matrix[0][2],
                  settings_.matrix[1][0], settings_.matrix[1][1], settings_.matrix[1][2],
                  settings_.matrix[2][0], settings_.matrix[2][1], settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Red From Green")) {
        setMatrix(settings_.matrix[0][0], value.toFloat(), settings_.matrix[0][2],
                  settings_.matrix[1][0], settings_.matrix[1][1], settings_.matrix[1][2],
                  settings_.matrix[2][0], settings_.matrix[2][1], settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Red From Blue")) {
        setMatrix(settings_.matrix[0][0], settings_.matrix[0][1], value.toFloat(),
                  settings_.matrix[1][0], settings_.matrix[1][1], settings_.matrix[1][2],
                  settings_.matrix[2][0], settings_.matrix[2][1], settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Green From Red")) {
        setMatrix(settings_.matrix[0][0], settings_.matrix[0][1], settings_.matrix[0][2],
                  value.toFloat(), settings_.matrix[1][1], settings_.matrix[1][2],
                  settings_.matrix[2][0], settings_.matrix[2][1], settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Green From Green")) {
        setMatrix(settings_.matrix[0][0], settings_.matrix[0][1], settings_.matrix[0][2],
                  settings_.matrix[1][0], value.toFloat(), settings_.matrix[1][2],
                  settings_.matrix[2][0], settings_.matrix[2][1], settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Green From Blue")) {
        setMatrix(settings_.matrix[0][0], settings_.matrix[0][1], settings_.matrix[0][2],
                  settings_.matrix[1][0], settings_.matrix[1][1], value.toFloat(),
                  settings_.matrix[2][0], settings_.matrix[2][1], settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Blue From Red")) {
        setMatrix(settings_.matrix[0][0], settings_.matrix[0][1], settings_.matrix[0][2],
                  settings_.matrix[1][0], settings_.matrix[1][1], settings_.matrix[1][2],
                  value.toFloat(), settings_.matrix[2][1], settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Blue From Green")) {
        setMatrix(settings_.matrix[0][0], settings_.matrix[0][1], settings_.matrix[0][2],
                  settings_.matrix[1][0], settings_.matrix[1][1], settings_.matrix[1][2],
                  settings_.matrix[2][0], value.toFloat(), settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Blue From Blue")) {
        setMatrix(settings_.matrix[0][0], settings_.matrix[0][1], settings_.matrix[0][2],
                  settings_.matrix[1][0], settings_.matrix[1][1], settings_.matrix[1][2],
                  settings_.matrix[2][0], settings_.matrix[2][1], value.toFloat());
    }
}

} // namespace Artifact
