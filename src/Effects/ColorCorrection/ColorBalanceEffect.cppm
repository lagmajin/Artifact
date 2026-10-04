module;
#include <algorithm>
#include <cmath>
#include <string>
#include <memory>
#include <vector>
#include <QVariant>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module ColorBalanceEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.ColorBalance;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageSurfaceView;
import Graphics.SurfaceColorContract;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Memory.SharedPtr;
import Core.Parallel;

namespace Artifact {

namespace {
constexpr auto kColorBalanceKey = gpuGenericKeyFromString("color_balance");
constexpr const char* kColorBalanceMath = R"(
float3 balanceColor(float3 c, float3 shadow, float3 midtone, float3 highlight,
                    float shadowRange, float highlightRange, float strength, float preserve) {
    const float3 weights = float3(0.2126, 0.7152, 0.0722);
    float lum = dot(c, weights);
    float shadowW = 1.0 - smoothstep(shadowRange-0.1, shadowRange+0.1, lum);
    float highlightW = smoothstep(highlightRange-0.1, highlightRange+0.1, lum);
    float midtoneW = saturate(1.0-shadowW-highlightW);
    strength = saturate(strength);
    float3 mixed = c + (shadow*shadowW + midtone*midtoneW + highlight*highlightW)*strength;
    float mixedLum = dot(mixed, weights);
    if (preserve > 0.5 && strength > 0.0 && mixedLum > 0.000001)
        mixed = lerp(mixed, saturate(mixed*(lum/mixedLum)), strength);
    return saturate(mixed);
}
)";
// Shader source concatenation is initialization work; parameters never rebuild it.
const std::string kColorBalanceResidentHlsl = std::string(kColorBalanceMath) + R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
[numthreads(8,8,1)] void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= g_Width || id.y >= g_Height) return;
    float4 pixel = g_InputTexture[id.xy];
    float3 shadow = float3(g_P0,g_P1,g_P2);
    float3 midtone = float3(g_P3,g_P4,g_P5);
    float3 highlight = float3(g_P6,g_P7,g_P8);
    if (pixel.a <= 0.0 || g_P11 <= 0.0 ||
        (all(shadow == 0.0) && all(midtone == 0.0) && all(highlight == 0.0))) {
        g_OutputTexture[id.xy] = pixel; return;
    }
    pixel.rgb = balanceColor(pixel.rgb/pixel.a,shadow,midtone,highlight,
                            g_P9,g_P10,g_P11,g_P12)*pixel.a;
    g_OutputTexture[id.xy] = pixel;
}
)";

bool isNeutral(const ColorBalanceSettings& settings) {
    return settings.masterStrength <= 0.0f ||
        (settings.shadowR == 0.0f && settings.shadowG == 0.0f && settings.shadowB == 0.0f &&
         settings.midtoneR == 0.0f && settings.midtoneG == 0.0f && settings.midtoneB == 0.0f &&
         settings.highlightR == 0.0f && settings.highlightG == 0.0f && settings.highlightB == 0.0f);
}

void applyColorBalanceCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst,
                          const ColorBalanceSettings& settings, const ColorBalanceProcessor& processor) {
    dst = src;
    if (isNeutral(settings)) return;
    float* pixels = dst.image().rgba32fData();
    if (!pixels) return;
    const auto descriptor = src.image().colorDescriptor();
    const bool premultiplied = descriptor.alphaMode == ArtifactCore::SurfaceAlphaMode::Premultiplied;
    const int width = dst.width();
    // Invalid/unknown layouts leave the copied input unchanged.
    (void)ArtifactCore::withMutableColorFloat4View(dst.image().surfaceView(), pixels, [&](const auto& view) {
        ArtifactCore::Parallel::For(0, dst.height(), width*dst.height(), [&](int y) {
            const auto row = view.row(y);
            for (int x = 0; x < width; ++x) {
                auto p = row[x];
                if (premultiplied && p.a <= 0.0f) continue;
                const float alpha = premultiplied ? p.a : 1.0f;
                float r = p.r/alpha, g = p.g/alpha, b = p.b/alpha;
                processor.applyPixel(r,g,b);
                p.r = r*alpha; p.g = g*alpha; p.b = b*alpha;
            }
        });
    });
}
} // namespace

class ColorBalanceEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ColorBalanceSettings settings_;
    ColorBalanceProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyColorBalanceCPU(src, dst, settings_, processor_);
    }
};

class ColorBalanceEffectGPUImpl : public ArtifactEffectImplBase {
public:
    // Retain one lease for the cached pipeline/resources; release it last.
    SharedRenderDeviceLease deviceLease_;
    ColorBalanceSettings settings_;
    ColorBalanceProcessor processor_;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
    std::unique_ptr<ArtifactCore::ComputeExecutor> executor_;
    mutable bool pipelineReady_ = false;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> stagingTex_;

    ColorBalanceEffectGPUImpl() { (void)colorBalanceHlsl(); }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyColorBalanceCPU(src, dst, settings_, processor_);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (isNeutral(settings_)) { dst = src; return; }
        if (!device_ && !deviceLease_.acquire(device_, context_)) {
            applyCPU(src, dst);
            return;
        }

        if (!gpuContext_) {
            gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
            executor_ = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext_);
        }
        if (!executor_) {
            applyCPU(src, dst);
            return;
        }

        if (!paramsCB_) {
            Diligent::BufferDesc cbDesc;
            cbDesc.Name = "ColorBalance/ParamsCB";
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
            {Diligent::SHADER_TYPE_COMPUTE, "ColorBalanceParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };

        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "ColorBalance/PSO";
            desc.shaderSource = colorBalanceHlsl().c_str();
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor_->build(desc) || !executor_->createShaderResourceBinding(true) ||
                !executor_->setBuffer("ColorBalanceParams", paramsCB_)) {
                applyCPU(src, dst);
                return;
            }
            pipelineReady_ = true;
        }

        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex, "ColorBalance/InputTexture")) {
            applyCPU(src, dst);
            return;
        }

        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "ColorBalance/OutputTexture";
        if (!outputTex_ || outputTex_->GetDesc().Width != outDesc.Width ||
            outputTex_->GetDesc().Height != outDesc.Height ||
            outputTex_->GetDesc().Format != outDesc.Format ||
            outputTex_->GetDesc().BindFlags != outDesc.BindFlags) {
            outputTex_.Release();
            device_->CreateTexture(outDesc, nullptr, &outputTex_);
        }
        if (!outputTex_) {
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
        params.shadowR = settings_.shadowR;
        params.shadowG = settings_.shadowG;
        params.shadowB = settings_.shadowB;
        params.midtoneR = settings_.midtoneR;
        params.midtoneG = settings_.midtoneG;
        params.midtoneB = settings_.midtoneB;
        params.highlightR = settings_.highlightR;
        params.highlightG = settings_.highlightG;
        params.highlightB = settings_.highlightB;
        params.shadowRange = settings_.shadowRange;
        params.highlightRange = settings_.highlightRange;
        params.masterStrength = settings_.masterStrength;
        params.preserveLuma = settings_.preserveLuma ? 1.0f : 0.0f;
        params.premultiplied = src.image().colorDescriptor().alphaMode ==
            ArtifactCore::SurfaceAlphaMode::Premultiplied ? 1.0f : 0.0f;
        params.bgra = src.image().colorDescriptor().channelOrder ==
            ArtifactCore::SurfaceChannelOrder::BGRA ? 1.0f : 0.0f;
        std::memcpy(mapped, &params, sizeof(params));
        context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);

        if (!executor_->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor_->setTextureView("g_OutputTexture", outputTex_->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }

        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        executor_->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (!readbackTexture(device_, context_, outputTex_, stagingTex_, dst, src.image().colorDescriptor(), "ColorBalance/StagingTexture")) {
            applyCPU(src, dst);
            return;
        }
        dst.image().setColorDescriptor(src.image().colorDescriptor());
    }

private:
    struct alignas(16) ParamsCB {
        float shadowR, shadowG, shadowB, shadowRange;
        float midtoneR, midtoneG, midtoneB, highlightRange;
        float highlightR, highlightG, highlightB, masterStrength;
        float preserveLuma, premultiplied, bgra, pad = 0.0f;
    };
    static_assert(sizeof(ParamsCB) == 64);

    static const std::string& colorBalanceHlsl() {
        static const std::string source = std::string(kColorBalanceMath) + R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer ColorBalanceParams : register(b0) {
    float3 g_Shadow; float g_ShadowRange;
    float3 g_Midtone; float g_HighlightRange;
    float3 g_Highlight; float g_MasterStrength;
    float g_PreserveLuma; float g_Premultiplied; float g_Bgra; float g_Pad;
}
[numthreads(8,8,1)] void main(uint3 id : SV_DispatchThreadID) {
    uint width,height; g_OutputTexture.GetDimensions(width,height);
    if (id.x >= width || id.y >= height) return;
    float4 pixel = g_InputTexture[id.xy];
    if (g_Premultiplied > 0.5 && pixel.a <= 0.0) {g_OutputTexture[id.xy]=pixel;return;}
    float alpha = g_Premultiplied > 0.5 ? pixel.a : 1.0;
    float3 color = (g_Bgra > 0.5 ? pixel.bgr : pixel.rgb)/alpha;
    color = balanceColor(color,g_Shadow,g_Midtone,g_Highlight,
                         g_ShadowRange,g_HighlightRange,g_MasterStrength,g_PreserveLuma)*alpha;
    pixel.rgb = g_Bgra > 0.5 ? color.bgr : color;
    g_OutputTexture[id.xy] = pixel;
}
)";
        return source;
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
        // Preserve source storage order; the compatibility shader handles BGRA.
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
                                Diligent::RefCntAutoPtr<Diligent::ITexture>& staging,
                                ImageF32x4RGBAWithCache& dst,
                                const ArtifactCore::SurfaceColorDescriptor& colorDescriptor,
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
        if (!staging || staging->GetDesc().Width != stagingDesc.Width || staging->GetDesc().Height != stagingDesc.Height || staging->GetDesc().Format != stagingDesc.Format) {
            staging.Release();
            device->CreateTexture(stagingDesc, nullptr, &staging);
        }
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
        dst.image().setFromCVMat(temp, colorDescriptor);
        ctx->UnmapTextureSubresource(staging, 0, 0);
        return true;
    }
};

ColorBalanceEffect::ColorBalanceEffect() {
    setEffectID(UniString("effect.colorcorrection.colorbalance"));
    setDisplayName(UniString("Color Balance"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<ColorBalanceEffectCPUImpl>());
    setGPUImpl(ArtifactCore::makeShared<ColorBalanceEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    registerGpuGenericShader(kColorBalanceKey,
        GpuGenericShaderRecord{kColorBalanceResidentHlsl.c_str(), "main", GpuGenericResourceKind::Filter});
    applyPreset(preset_);
    syncImpls();
}

ColorBalanceEffect::~ColorBalanceEffect() = default;

std::uint32_t ColorBalanceEffect::gpuGenericKey() const { return kColorBalanceKey; }

bool ColorBalanceEffect::appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const {
    GpuSpatialEffectNode node;
    node.kind = GpuSpatialEffectKind::Generic;
    node.genericKey = kColorBalanceKey;
    node.parameters[0] = settings_.shadowR; node.parameters[1] = settings_.shadowG;
    node.parameters[2] = settings_.shadowB; node.parameters[3] = settings_.midtoneR;
    node.parameters[4] = settings_.midtoneG; node.parameters[5] = settings_.midtoneB;
    node.parameters[6] = settings_.highlightR; node.parameters[7] = settings_.highlightG;
    node.parameters[8] = settings_.highlightB; node.parameters[9] = settings_.shadowRange;
    node.parameters[10] = settings_.highlightRange; node.parameters[11] = settings_.masterStrength;
    node.parameters[12] = settings_.preserveLuma ? 1.0f : 0.0f;
    return stack.append(node);
}

void ColorBalanceEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Neutral:
        settings_ = ColorBalanceSettings::neutral();
        break;
    case Preset::CoolShadows:
        settings_ = ColorBalanceSettings::coolShadows();
        break;
    case Preset::WarmHighlights:
        settings_ = ColorBalanceSettings::warmHighlights();
        break;
    case Preset::Cinematic:
        settings_ = ColorBalanceSettings::cinematic();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void ColorBalanceEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 4);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void ColorBalanceEffect::setShadowBalance(float r, float g, float b) {
    preset_ = Preset::Custom;
    settings_.shadowR = std::isfinite(r) ? std::clamp(r, -1.0f, 1.0f) : 0.0f;
    settings_.shadowG = std::isfinite(g) ? std::clamp(g, -1.0f, 1.0f) : 0.0f;
    settings_.shadowB = std::isfinite(b) ? std::clamp(b, -1.0f, 1.0f) : 0.0f;
    syncImpls();
}

void ColorBalanceEffect::setMidtoneBalance(float r, float g, float b) {
    preset_ = Preset::Custom;
    settings_.midtoneR = std::isfinite(r) ? std::clamp(r, -1.0f, 1.0f) : 0.0f;
    settings_.midtoneG = std::isfinite(g) ? std::clamp(g, -1.0f, 1.0f) : 0.0f;
    settings_.midtoneB = std::isfinite(b) ? std::clamp(b, -1.0f, 1.0f) : 0.0f;
    syncImpls();
}

void ColorBalanceEffect::setHighlightBalance(float r, float g, float b) {
    preset_ = Preset::Custom;
    settings_.highlightR = std::isfinite(r) ? std::clamp(r, -1.0f, 1.0f) : 0.0f;
    settings_.highlightG = std::isfinite(g) ? std::clamp(g, -1.0f, 1.0f) : 0.0f;
    settings_.highlightB = std::isfinite(b) ? std::clamp(b, -1.0f, 1.0f) : 0.0f;
    syncImpls();
}

void ColorBalanceEffect::setShadowRange(float value) {
    preset_ = Preset::Custom;
    settings_.shadowRange = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.33f;
    syncImpls();
}

void ColorBalanceEffect::setHighlightRange(float value) {
    preset_ = Preset::Custom;
    settings_.highlightRange = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.66f;
    syncImpls();
}

void ColorBalanceEffect::setMasterStrength(float value) {
    preset_ = Preset::Custom;
    settings_.masterStrength = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 1.0f;
    syncImpls();
}

void ColorBalanceEffect::setPreserveLuma(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveLuma = value;
    syncImpls();
}

void ColorBalanceEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<ColorBalanceEffectCPUImpl*>(cpuImpl().get())) {
        cpu->settings_ = settings_;
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<ColorBalanceEffectGPUImpl*>(gpuImpl().get())) {
        gpu->settings_ = settings_;
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> ColorBalanceEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(14);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    auto addFloat = [&props](const char* name, float value, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };

    addFloat("Shadow R", settings_.shadowR, -20);
    addFloat("Shadow G", settings_.shadowG, -19);
    addFloat("Shadow B", settings_.shadowB, -18);
    addFloat("Midtone R", settings_.midtoneR, -10);
    addFloat("Midtone G", settings_.midtoneG, -9);
    addFloat("Midtone B", settings_.midtoneB, -8);
    addFloat("Highlight R", settings_.highlightR, 0);
    addFloat("Highlight G", settings_.highlightG, 1);
    addFloat("Highlight B", settings_.highlightB, 2);
    addFloat("Shadow Range", settings_.shadowRange, 10);
    addFloat("Highlight Range", settings_.highlightRange, 11);
    addFloat("Strength", settings_.masterStrength, 20);

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Luma");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveLuma);
    preserveProp.setDisplayPriority(30);
    props.push_back(preserveProp);

    return props;
}

void ColorBalanceEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    } else if (key == QStringLiteral("Shadow R")) {
        setShadowBalance(value.toFloat(), settings_.shadowG, settings_.shadowB);
    } else if (key == QStringLiteral("Shadow G")) {
        setShadowBalance(settings_.shadowR, value.toFloat(), settings_.shadowB);
    } else if (key == QStringLiteral("Shadow B")) {
        setShadowBalance(settings_.shadowR, settings_.shadowG, value.toFloat());
    } else if (key == QStringLiteral("Midtone R")) {
        setMidtoneBalance(value.toFloat(), settings_.midtoneG, settings_.midtoneB);
    } else if (key == QStringLiteral("Midtone G")) {
        setMidtoneBalance(settings_.midtoneR, value.toFloat(), settings_.midtoneB);
    } else if (key == QStringLiteral("Midtone B")) {
        setMidtoneBalance(settings_.midtoneR, settings_.midtoneG, value.toFloat());
    } else if (key == QStringLiteral("Highlight R")) {
        setHighlightBalance(value.toFloat(), settings_.highlightG, settings_.highlightB);
    } else if (key == QStringLiteral("Highlight G")) {
        setHighlightBalance(settings_.highlightR, value.toFloat(), settings_.highlightB);
    } else if (key == QStringLiteral("Highlight B")) {
        setHighlightBalance(settings_.highlightR, settings_.highlightG, value.toFloat());
    } else if (key == QStringLiteral("Shadow Range")) {
        setShadowRange(value.toFloat());
    } else if (key == QStringLiteral("Highlight Range")) {
        setHighlightRange(value.toFloat());
    } else if (key == QStringLiteral("Strength")) {
        setMasterStrength(value.toFloat());
    } else if (key == QStringLiteral("Preserve Luma")) {
        setPreserveLuma(value.toBool());
    }
}

} // namespace Artifact
