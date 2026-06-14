module;
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <vector>
#include <QVariant>
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import DiligentCore\Common/interface/RefCntAutoPtr.hpp;
import DiligentCore/Graphics/GraphicsEngine/interface/Texture.h;

module LevelsEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.LevelsCurves;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

class LevelsEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::LevelsEffect processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                processor_.applyPixel(pixel[0], pixel[1], pixel[2]);
            }
        }
    }
};

class LevelsEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::LevelsEffect processor_;
    ArtifactCore::LevelsSettings settings_;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
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
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                processor_.applyPixel(pixel[0], pixel[1], pixel[2]);
            }
        }
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) {
            applyCPU(src, dst);
            return;
        }

        struct Guard {
            LevelsEffectGPUImpl* self{};
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
            desc.name = "Levels/PSO";
            desc.shaderSource = kLevelsHlsl;
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
        if (!createTextureFromImage(src, device_, &inputTex, "Levels/InputTexture")) {
            applyCPU(src, dst);
            return;
        }

        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "Levels/OutputTexture";
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

        if (!readbackTexture(device_, context_, outputTex, dst, "Levels/StagingTexture")) {
            applyCPU(src, dst);
        }
    }

    void syncSettings(const ArtifactCore::LevelsSettings& settings) {
        settings_ = settings;
        processor_.setSettings(settings_);
        lutDirty_ = true;
    }

private:
    bool buildLUTTexture() {
        std::array<float, 256 * 4> lut{};
        for (int i = 0; i < 256; ++i) {
            const float x = static_cast<float>(i) / 255.0f;
            float r = x, g = x, b = x;
            if (settings_.perChannel) {
                auto transform = [](float v, const ArtifactCore::ChannelLevelsSettings& c) {
                    const float clampedInput = std::clamp(v, static_cast<float>(c.inputBlack), static_cast<float>(c.inputWhite));
                    const float range = std::max(0.0001f, static_cast<float>(c.inputWhite - c.inputBlack));
                    float normalized = (clampedInput - static_cast<float>(c.inputBlack)) / range;
                    if (c.inputGamma != 1.0) {
                        normalized = std::pow(std::clamp(normalized, 0.0f, 1.0f), 1.0f / static_cast<float>(c.inputGamma));
                    }
                    return static_cast<float>(c.outputBlack + normalized * (c.outputWhite - c.outputBlack));
                };
                r = transform(x, settings_.red);
                g = transform(x, settings_.green);
                b = transform(x, settings_.blue);
            } else {
                const float clampedInput = std::clamp(x, static_cast<float>(settings_.inputBlack), static_cast<float>(settings_.inputWhite));
                const float range = std::max(0.0001f, static_cast<float>(settings_.inputWhite - settings_.inputBlack));
                float normalized = (clampedInput - static_cast<float>(settings_.inputBlack)) / range;
                if (settings_.inputGamma != 1.0) {
                    normalized = std::pow(std::clamp(normalized, 0.0f, 1.0f), 1.0f / static_cast<float>(settings_.inputGamma));
                }
                r = g = b = static_cast<float>(settings_.outputBlack + normalized * (settings_.outputWhite - settings_.outputBlack));
            }
            lut[i * 4 + 0] = r;
            lut[i * 4 + 1] = g;
            lut[i * 4 + 2] = b;
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
        desc.Name = "Levels/LUTTexture";
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

    static constexpr const char* kLevelsHlsl = R"(
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

LevelsEffect::LevelsEffect() {
    setEffectID(UniString("effect.colorcorrection.levels"));
    setDisplayName(UniString("Levels"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<LevelsEffectCPUImpl>());
    setGPUImpl(std::make_shared<LevelsEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

LevelsEffect::~LevelsEffect() = default;

void LevelsEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Normal:
        settings_ = ArtifactCore::LevelsSettings::normal();
        break;
    case Preset::HighContrast:
        settings_ = ArtifactCore::LevelsSettings::highContrast();
        break;
    case Preset::LowContrast:
        settings_ = ArtifactCore::LevelsSettings::lowContrast();
        break;
    case Preset::Brighten:
        settings_ = ArtifactCore::LevelsSettings::brighten();
        break;
    case Preset::Darken:
        settings_ = ArtifactCore::LevelsSettings::darken();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void LevelsEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 5);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void LevelsEffect::setInputBlack(float value) {
    preset_ = Preset::Custom;
    settings_.inputBlack = value;
    syncImpls();
}

void LevelsEffect::setInputWhite(float value) {
    preset_ = Preset::Custom;
    settings_.inputWhite = value;
    syncImpls();
}

void LevelsEffect::setInputGamma(float value) {
    preset_ = Preset::Custom;
    settings_.inputGamma = std::clamp(static_cast<double>(value), 0.01, 10.0);
    syncImpls();
}

void LevelsEffect::setOutputBlack(float value) {
    preset_ = Preset::Custom;
    settings_.outputBlack = value;
    syncImpls();
}

void LevelsEffect::setOutputWhite(float value) {
    preset_ = Preset::Custom;
    settings_.outputWhite = value;
    syncImpls();
}

void LevelsEffect::setPerChannel(bool value) {
    preset_ = Preset::Custom;
    settings_.perChannel = value;
    syncImpls();
}

void LevelsEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<LevelsEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<LevelsEffectGPUImpl*>(gpuImpl().get())) {
        gpu->syncSettings(settings_);
    }
}

std::vector<AbstractProperty> LevelsEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(20);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    auto addFloat = [&props](const char* name, double value, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(value));
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };

    addFloat("Input Black", settings_.inputBlack, -20);
    addFloat("Input White", settings_.inputWhite, -19);
    addFloat("Input Gamma", settings_.inputGamma, -18);
    addFloat("Output Black", settings_.outputBlack, -10);
    addFloat("Output White", settings_.outputWhite, -9);

    AbstractProperty perChannelProp;
    perChannelProp.setName("Per Channel");
    perChannelProp.setType(PropertyType::Boolean);
    perChannelProp.setValue(settings_.perChannel);
    perChannelProp.setDisplayPriority(0);
    props.push_back(perChannelProp);

    addFloat("Red Input Black", settings_.red.inputBlack, 10);
    addFloat("Red Input White", settings_.red.inputWhite, 11);
    addFloat("Red Input Gamma", settings_.red.inputGamma, 12);
    addFloat("Red Output Black", settings_.red.outputBlack, 13);
    addFloat("Red Output White", settings_.red.outputWhite, 14);

    addFloat("Green Input Black", settings_.green.inputBlack, 20);
    addFloat("Green Input White", settings_.green.inputWhite, 21);
    addFloat("Green Input Gamma", settings_.green.inputGamma, 22);
    addFloat("Green Output Black", settings_.green.outputBlack, 23);
    addFloat("Green Output White", settings_.green.outputWhite, 24);

    addFloat("Blue Input Black", settings_.blue.inputBlack, 30);
    addFloat("Blue Input White", settings_.blue.inputWhite, 31);
    addFloat("Blue Input Gamma", settings_.blue.inputGamma, 32);
    addFloat("Blue Output Black", settings_.blue.outputBlack, 33);
    addFloat("Blue Output White", settings_.blue.outputWhite, 34);

    return props;
}

void LevelsEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QString("Preset")) {
        setPreset(value.toInt());
    } else if (key == QString("Input Black")) {
        setInputBlack(static_cast<float>(value.toDouble()));
    } else if (key == QString("Input White")) {
        setInputWhite(static_cast<float>(value.toDouble()));
    } else if (key == QString("Input Gamma")) {
        setInputGamma(static_cast<float>(value.toDouble()));
    } else if (key == QString("Output Black")) {
        setOutputBlack(static_cast<float>(value.toDouble()));
    } else if (key == QString("Output White")) {
        setOutputWhite(static_cast<float>(value.toDouble()));
    } else if (key == QString("Per Channel")) {
        setPerChannel(value.toBool());
    } else if (key == QString("Red Input Black")) {
        preset_ = Preset::Custom;
        settings_.red.inputBlack = value.toDouble();
        syncImpls();
    } else if (key == QString("Red Input White")) {
        preset_ = Preset::Custom;
        settings_.red.inputWhite = value.toDouble();
        syncImpls();
    } else if (key == QString("Red Input Gamma")) {
        preset_ = Preset::Custom;
        settings_.red.inputGamma = value.toDouble();
        syncImpls();
    } else if (key == QString("Red Output Black")) {
        preset_ = Preset::Custom;
        settings_.red.outputBlack = value.toDouble();
        syncImpls();
    } else if (key == QString("Red Output White")) {
        preset_ = Preset::Custom;
        settings_.red.outputWhite = value.toDouble();
        syncImpls();
    } else if (key == QString("Green Input Black")) {
        preset_ = Preset::Custom;
        settings_.green.inputBlack = value.toDouble();
        syncImpls();
    } else if (key == QString("Green Input White")) {
        preset_ = Preset::Custom;
        settings_.green.inputWhite = value.toDouble();
        syncImpls();
    } else if (key == QString("Green Input Gamma")) {
        preset_ = Preset::Custom;
        settings_.green.inputGamma = value.toDouble();
        syncImpls();
    } else if (key == QString("Green Output Black")) {
        preset_ = Preset::Custom;
        settings_.green.outputBlack = value.toDouble();
        syncImpls();
    } else if (key == QString("Green Output White")) {
        preset_ = Preset::Custom;
        settings_.green.outputWhite = value.toDouble();
        syncImpls();
    } else if (key == QString("Blue Input Black")) {
        preset_ = Preset::Custom;
        settings_.blue.inputBlack = value.toDouble();
        syncImpls();
    } else if (key == QString("Blue Input White")) {
        preset_ = Preset::Custom;
        settings_.blue.inputWhite = value.toDouble();
        syncImpls();
    } else if (key == QString("Blue Input Gamma")) {
        preset_ = Preset::Custom;
        settings_.blue.inputGamma = value.toDouble();
        syncImpls();
    } else if (key == QString("Blue Output Black")) {
        preset_ = Preset::Custom;
        settings_.blue.outputBlack = value.toDouble();
        syncImpls();
    } else if (key == QString("Blue Output White")) {
        preset_ = Preset::Custom;
        settings_.blue.outputWhite = value.toDouble();
        syncImpls();
    }
}

} // namespace Artifact
