module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <memory>
#include <QVariant>
#include <QElapsedTimer>
#include <QSettings>
#include <QDebug>
#include <QtGlobal>
#include <vector>
#include "ExperimentalContourPosterShader.hpp"
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module PosterizeEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.GpuImageUpload;
import Image.ImageSurfaceView;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Graphics.SurfaceColorContract;
import Artifact.Render.DiligentDeviceManager;
import Core.Parallel;
import Memory.SharedPtr;

namespace Artifact {

namespace {
void applyContourPoster(const ImageF32x4RGBAWithCache& src,
                        ImageF32x4RGBAWithCache& dst, float levels,
                        float lineStrength, float lineRadiusPx) {
    float* pixels = dst.image().rgba32fData();
    if (!pixels) return;
    const int width = dst.image().width();
    const int height = dst.image().height();
    if (width <= 0 || height <= 0) return;
    const float count = std::clamp(levels, 2.0f, 64.0f);
    const float strength = std::clamp(lineStrength, 0.0f, 1.0f);
    const float radius = std::clamp(lineRadiusPx, 0.5f, 1.5f);
    const bool processed = ArtifactCore::withColorFloat4View(
        src.image().surfaceView(), [&](const auto& sourceView) {
        const bool written = ArtifactCore::withMutableColorFloat4View(
            dst.image().surfaceView(), pixels, [&](auto& view) {
            ArtifactCore::Parallel::ForPixels(0, height, width, height,
                [&](int y) {
                    const auto sourceRow = [&](int sy) {
                        return sourceView.row(std::clamp(sy, 0, height - 1));
                    };
                    const auto logicalPixel = [&](int sx, int sy) {
                        return sourceRow(sy)[std::clamp(sx, 0, width - 1)];
                    };
                    const auto domainAt = [&](int sx, int sy) {
                        const auto sample = logicalPixel(sx, sy);
                        if (sample.a <= 1.0e-6f) return 0.0f;
                        const float inverseAlpha = 1.0f / sample.a;
                        const float luminance = std::max(0.0f,
                            (sample.r * 0.2126f + sample.g * 0.7152f +
                             sample.b * 0.0722f) * inverseAlpha);
                        return std::sqrt(luminance);
                    };
                    const auto weightedDelta = [&](int sx, int sy, float centerDomain) {
                        const auto sample = logicalPixel(sx, sy);
                        const float alpha = std::clamp(sample.a, 0.0f, 1.0f);
                        if (alpha <= 1.0e-6f) return 0.0f;
                        const float u = std::clamp(
                            (alpha - 1.0e-6f) / 1.0e-6f, 0.0f, 1.0f);
                        const float confidence = u * u * (3.0f - 2.0f * u);
                        return alpha * confidence * (domainAt(sx, sy) - centerDomain);
                    };
                    auto row = view.row(y);
                    for (int x = 0; x < width; ++x) {
                        const auto center = row[x];
                        const float alpha = std::clamp(center.a, 0.0f, 1.0f);
                        if (alpha <= 1.0e-6f) continue;
                        const float inverseAlpha = 1.0f / alpha;
                        const float r = center.r * inverseAlpha;
                        const float g = center.g * inverseAlpha;
                        const float b = center.b * inverseAlpha;
                        const float luminance = std::max(0.0f,
                            r * 0.2126f + g * 0.7152f + b * 0.0722f);
                        const float domain = std::sqrt(luminance);
                        const float quantized = std::floor(domain * count + 0.5f) / count;
                        const float quantizedLuminance = quantized * quantized;
                        const float ratio = luminance > 1.0e-8f
                            ? quantizedLuminance / luminance : 0.0f;
                        const float gx = 0.5f * (
                            weightedDelta(x + 1, y, domain) -
                            weightedDelta(x - 1, y, domain));
                        const float gy = 0.5f * (
                            weightedDelta(x, y + 1, domain) -
                            weightedDelta(x, y - 1, domain));
                        const float gradient = std::sqrt(gx * gx + gy * gy);
                        const float phase = std::abs(std::fmod(domain * count + 0.5f, 1.0f) - 0.5f);
                        const float distance = gradient > 1.0e-6f
                            ? (phase / count) / gradient : 1.0e20f;
                        const float line = std::clamp(
                            (radius - distance) / (radius * 0.5f), 0.0f, 1.0f);
                        const float u = std::clamp((alpha - 1.0e-6f) / 1.0e-6f, 0.0f, 1.0f);
                        const float confidence = u * u * (3.0f - 2.0f * u);
                        const float processed = ratio * (1.0f - strength * line);
                        const float output = (1.0f - confidence) + confidence * processed;
                        row[x].r = center.r * output;
                        row[x].g = center.g * output;
                        row[x].b = center.b * output;
                    }
                });
            });
        (void)written;
    });
    (void)processed;
}
} // namespace

class ExperimentalContourPosterCPUImpl final : public ArtifactEffectImplBase {
public:
    float levels_ = 6.0f;
    float lineStrength_ = 0.8f;
    float lineRadiusPx_ = 1.0f;
    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        applyContourPoster(src, dst, levels_, lineStrength_, lineRadiusPx_);
    }
};

class PosterizeEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float levels_ = 4.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        const float n = std::max(2.0f, levels_);
        const float denom = n - 1.0f;

        const bool processed = ArtifactCore::withMutableColorFloat4View(
            dst.image().surfaceView(), pixels, [&](auto& view) {
            ArtifactCore::Parallel::ForPixels(0, height, width, height, [&](int y) {
            auto row = view.row(y);
            for (int x = 0; x < width; ++x) {
                auto pixel = row[x];
                // [0.0, 1.0] -> [0.0, n-1] -> floor+0.5 -> / (n-1)
                pixel.r = std::floor(pixel.r * denom + 0.5f) / denom;
                pixel.g = std::floor(pixel.g * denom + 0.5f) / denom;
                pixel.b = std::floor(pixel.b * denom + 0.5f) / denom;
            }
            });
        });
        (void)processed;
    }
};

class PosterizeEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float levels_ = 4.0f;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
    std::unique_ptr<ArtifactCore::ComputeExecutor> executor_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> stagingTex_;
    mutable bool pipelineReady_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        const auto previousDevice = device_;
        const auto previousContext = context_;
        SharedRenderDeviceLease lease;
        if (!lease.acquire(device_, context_)) {
            applyCPU(src, dst);
            return;
        }
        if (previousDevice != device_ || previousContext != context_) {
            executor_.reset();
            gpuContext_.reset();
            paramsCB_.Release();
            outputTex_.Release();
            stagingTex_.Release();
            pipelineReady_ = false;
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
            cbDesc.Name = "Posterize/ParamsCB";
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
            {Diligent::SHADER_TYPE_COMPUTE, "PosterizeParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };

        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "Posterize/PSO";
            desc.shaderSource = kPosterizeHlsl;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor_->build(desc) || !executor_->createShaderResourceBinding(true) ||
                !executor_->setBuffer("PosterizeParams", paramsCB_)) {
                applyCPU(src, dst);
                return;
            }
            pipelineReady_ = true;
        }

        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex, "Posterize/InputTexture")) {
            applyCPU(src, dst);
            return;
        }

        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "Posterize/OutputTexture";
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
        params.levels = std::max(2.0f, levels_);
        std::memcpy(mapped, &params, sizeof(params));
        context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);

        if (!executor_->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor_->setTextureView("g_OutputTexture", outputTex_->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }

        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        executor_->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (!readbackTexture(device_, context_, outputTex_, stagingTex_, dst, src.image().colorDescriptor(), "Posterize/StagingTexture")) {
            applyCPU(src, dst);
            return;
        }
        dst.image().setColorDescriptor(src.image().colorDescriptor());
    }

private:
    struct ParamsCB {
        float levels = 4.0f;
        float pad[3] = {0.0f, 0.0f, 0.0f};
    };

    static constexpr const char* kPosterizeHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer PosterizeParams : register(b0) {
    float g_Levels;
    float3 g_Pad;
};

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dtid.x >= width || dtid.y >= height) return;

    float4 c = (g_InputTexture[dtid.xy]).bgra;
    float denom = max(1.0f, g_Levels - 1.0f);
    c.rgb = floor(c.rgb * denom + 0.5f) / denom;
    g_OutputTexture[dtid.xy] = (c).bgra;
}
)";

    PosterizeEffectCPUImpl cpuImpl_;

    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& img,
                                      Diligent::IRenderDevice* device,
                                      Diligent::ITexture** outTex,
                                      const char* name)
    {
        if (!device || !outTex) return false;
        const auto& image = img.image();
        const int width = image.width();
        const int height = image.height();
        if (width <= 0 || height <= 0) return false;
        const auto upload = ArtifactCore::makeGpuImageUploadBuffer(image.surfaceView(), ArtifactCore::GpuImageChannelOrder::Bgra);
        if (!upload.isValid()) return false;

        Diligent::TextureDesc desc;
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = static_cast<Diligent::Uint32>(width);
        desc.Height = static_cast<Diligent::Uint32>(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
        desc.SampleCount = 1;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        desc.Name = name;

        Diligent::TextureSubResData subres;
        subres.pData = upload.data();
        subres.Stride = static_cast<Diligent::Uint64>(upload.rowStride);
        Diligent::TextureData initData;
        initData.pSubResources = &subres;
        initData.NumSubresources = 1;

        device->CreateTexture(desc, &initData, outTex);
        return (*outTex != nullptr);
    }

    static bool readbackTexture(Diligent::IRenderDevice* device,
                               Diligent::IDeviceContext* ctx,
                               Diligent::ITexture* src,
                               Diligent::RefCntAutoPtr<Diligent::ITexture>& staging,
                               ImageF32x4RGBAWithCache& dst,
                               const ArtifactCore::SurfaceColorDescriptor& colorDescriptor,
                               const char* name)
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
        if (!staging || staging->GetDesc().Width != stagingDesc.Width ||
            staging->GetDesc().Height != stagingDesc.Height ||
            staging->GetDesc().Format != stagingDesc.Format) {
            staging.Release();
            device->CreateTexture(stagingDesc, nullptr, &staging);
        }
        if (!staging) return false;

        Diligent::CopyTextureAttribs copy(src, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                          staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->CopyTexture(copy);
        Diligent::MappedTextureSubresource mapped{};
        ctx->Flush();
        ctx->WaitForIdle();
        ctx->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, mapped);
        if (!mapped.pData || mapped.Stride == 0) return false;

        const size_t rowBytes = static_cast<size_t>(desc.Width) * sizeof(float) * 4ull;
        if (mapped.Stride < rowBytes) {
            ctx->UnmapTextureSubresource(staging, 0, 0);
            return false;
        }
        const bool copied = dst.image().setFromGpuBGRA32F(
            static_cast<const float*>(mapped.pData), static_cast<int>(desc.Width),
            static_cast<int>(desc.Height), static_cast<std::size_t>(mapped.Stride), colorDescriptor);
        ctx->UnmapTextureSubresource(staging, 0, 0);
        return copied;
    }
};

PosterizeEffect::PosterizeEffect() {
    setEffectID(UniString("effect.colorcorrection.posterize"));
    setDisplayName(UniString("Posterize"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<PosterizeEffectCPUImpl>());
    setGPUImpl(ArtifactCore::makeShared<PosterizeEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

PosterizeEffect::~PosterizeEffect() = default;

void PosterizeEffect::syncImpls() {
    if (auto cpu = ArtifactCore::dynamicPointerCast<PosterizeEffectCPUImpl>(cpuImpl())) {
        cpu->levels_ = levels_;
    }
    if (auto gpu = ArtifactCore::dynamicPointerCast<PosterizeEffectGPUImpl>(gpuImpl())) {
        gpu->levels_ = levels_;
    }
}

std::vector<AbstractProperty> PosterizeEffect::getProperties() const {
    std::vector<AbstractProperty> props(1);

    props[0].setName("Levels");
    props[0].setType(ArtifactCore::PropertyType::Float);
    props[0].setValue(QVariant(static_cast<double>(levels_)));
    return props;
}

void PosterizeEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    if (name == "Levels") setLevels(value.toFloat());
}

ExperimentalContourPosterEffect::ExperimentalContourPosterEffect() {
    setEffectID(UniString("effect.colorcorrection.contour_poster"));
    setDisplayName(UniString("Contour Poster"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<ExperimentalContourPosterCPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    registerGpuGenericShader(kGpuGenericKey,
        GpuGenericShaderRecord{kExperimentalContourPosterResidentShader,
                               "main", GpuGenericResourceKind::Filter});
}

ExperimentalContourPosterEffect::~ExperimentalContourPosterEffect() = default;

void ExperimentalContourPosterEffect::setLevels(float value) {
    levels_ = std::isfinite(value) ? std::clamp(value, 2.0f, 64.0f) : 6.0f;
    syncImpls();
}

void ExperimentalContourPosterEffect::setLineStrength(float value) {
    lineStrength_ = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.8f;
    syncImpls();
}

void ExperimentalContourPosterEffect::setLineRadiusPx(float value) {
    lineRadiusPx_ = std::isfinite(value) ? std::clamp(value, 0.5f, 1.5f) : 1.0f;
    syncImpls();
}

void ExperimentalContourPosterEffect::syncImpls() {
    if (auto cpu = ArtifactCore::dynamicPointerCast<ExperimentalContourPosterCPUImpl>(cpuImpl()))
    {
        cpu->levels_ = levels_;
        cpu->lineStrength_ = lineStrength_;
        cpu->lineRadiusPx_ = lineRadiusPx_;
    }
}

std::vector<AbstractProperty> ExperimentalContourPosterEffect::getProperties() const {
    std::vector<AbstractProperty> props(3);
    props[0].setName("Levels");
    props[0].setType(ArtifactCore::PropertyType::Float);
    props[0].setValue(QVariant(static_cast<double>(levels_)));
    props[0].setDefaultValue(QVariant(6.0));
    props[0].setMinValue(QVariant(2.0));
    props[0].setMaxValue(QVariant(64.0));
    props[0].setHardRange(QVariant(2.0), QVariant(64.0));
    props[1].setName("Line Strength");
    props[1].setType(ArtifactCore::PropertyType::Float);
    props[1].setValue(QVariant(static_cast<double>(lineStrength_)));
    props[1].setDefaultValue(QVariant(0.8));
    props[1].setMinValue(QVariant(0.0));
    props[1].setMaxValue(QVariant(1.0));
    props[1].setHardRange(QVariant(0.0), QVariant(1.0));
    props[2].setName("Line Radius");
    props[2].setType(ArtifactCore::PropertyType::Float);
    props[2].setValue(QVariant(static_cast<double>(lineRadiusPx_)));
    props[2].setDefaultValue(QVariant(1.0));
    props[2].setMinValue(QVariant(0.5));
    props[2].setMaxValue(QVariant(1.5));
    props[2].setHardRange(QVariant(0.5), QVariant(1.5));
    return props;
}

void ExperimentalContourPosterEffect::setPropertyValue(
    const UniString& name, const QVariant& value) {
    if (name == "Levels") {
        setLevels(value.toFloat());
    } else if (name == "Line Strength") {
        setLineStrength(value.toFloat());
    } else if (name == "Line Radius") {
        setLineRadiusPx(value.toFloat());
    } else {
        ArtifactAbstractEffect::setPropertyValue(name, value);
    }
}

} // namespace Artifact
