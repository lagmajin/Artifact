module;
#include <QList>
#include <QVariant>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Effect.Glow;




import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Glow; // ArtifactCoreのOpenCVグロー実装
import Property.Abstract;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

namespace {

void writeGlowResultToDestination(const cv::Mat& glowResult, ImageF32x4RGBAWithCache& dst) {
    if (glowResult.empty()) {
        return;
    }

    cv::Mat rgba8u;
    switch (glowResult.channels()) {
    case 4:
        cv::cvtColor(glowResult, rgba8u, cv::COLOR_BGRA2RGBA);
        break;
    case 3:
        cv::cvtColor(glowResult, rgba8u, cv::COLOR_BGR2RGBA);
        break;
    case 1:
        cv::cvtColor(glowResult, rgba8u, cv::COLOR_GRAY2RGBA);
        break;
    default:
        return;
    }

    cv::Mat rgba32f;
    rgba8u.convertTo(rgba32f, CV_32FC4, 1.0 / 255.0);
    dst.image().setFromRGBA32F(rgba32f.ptr<float>(), rgba32f.cols, rgba32f.rows);
}

} // namespace

void GlowEffectCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const ImageF32x4_RGBA& srcImage = src.image();
    const float* srcData = srcImage.rgba32fData();
    if (!srcData) {
        dst = src;
        return;
    }
    cv::Mat srcMat(srcImage.height(), srcImage.width(), CV_32FC4, const_cast<float*>(srcData));
    cv::Mat dstMat;

    // ArtifactCoreのOpenCVグロー実装を適用
    ArtifactCore::applySimpleGlow(
        srcMat,
        cv::Mat(), // マスクなし
        dstMat,
        cv::Scalar(255, 255, 255), // 白グロー
        glowGain_,
        layerCount_,
        baseSigma_,
        sigmaGrowth_,
        baseAlpha_,
        alphaFalloff_
    );

    // 結果をdstに設定
    if (dstMat.empty()) {
        dst = src;
        return;
    }
    writeGlowResultToDestination(dstMat, dst);
}

void GlowEffectGPUImpl::applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) {
        GlowEffectCPUImpl cpuImpl;
        cpuImpl.applyCPU(src, dst);
        return;
    }
    auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
    auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
    if (!paramsCB_) {
        Diligent::BufferDesc cbDesc;
        cbDesc.Name = "Glow/ParamsCB";
        cbDesc.Size = sizeof(ParamsCB);
        cbDesc.Usage = Diligent::USAGE_DYNAMIC;
        cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        device_->CreateBuffer(cbDesc, nullptr, &paramsCB_);
    }
    if (!paramsCB_) { GlowEffectCPUImpl cpuImpl; cpuImpl.applyCPU(src, dst); return; }
    static Diligent::ShaderResourceVariableDesc vars[] = {
        {Diligent::SHADER_TYPE_COMPUTE, "GlowParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };
    if (!pipelineReady_) {
        ArtifactCore::ComputePipelineDesc desc;
        desc.name = "Glow/PSO";
        desc.shaderSource = kGlowHlsl;
        desc.entryPoint = "main";
        desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
        desc.variables = vars;
        desc.variableCount = 3;
        desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("GlowParams", paramsCB_)) {
            GlowEffectCPUImpl cpuImpl; cpuImpl.applyCPU(src, dst); return;
        }
        pipelineReady_ = true;
    }
    Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
    if (!createTextureFromImage(src, device_, &inputTex, "Glow/InputTexture")) { GlowEffectCPUImpl cpuImpl; cpuImpl.applyCPU(src, dst); return; }
    Diligent::TextureDesc outDesc = inputTex->GetDesc();
    outDesc.Usage = Diligent::USAGE_DEFAULT;
    outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
    outDesc.Name = "Glow/OutputTexture";
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex;
    device_->CreateTexture(outDesc, nullptr, &outputTex);
    if (!outputTex) { GlowEffectCPUImpl cpuImpl; cpuImpl.applyCPU(src, dst); return; }
    void* mapped = nullptr;
    context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
    if (!mapped) { GlowEffectCPUImpl cpuImpl; cpuImpl.applyCPU(src, dst); return; }
    ParamsCB params{};
    params.glowGain = glowGain_;
    params.layerCount = layerCount_;
    params.baseSigma = baseSigma_;
    params.sigmaGrowth = sigmaGrowth_;
    params.baseAlpha = baseAlpha_;
    params.alphaFalloff = alphaFalloff_;
    std::memcpy(mapped, &params, sizeof(params));
    context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
    if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
        !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
        GlowEffectCPUImpl cpuImpl; cpuImpl.applyCPU(src, dst); return;
    }
    auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
    executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    if (!readbackTexture(device_, context_, outputTex, dst, "Glow/StagingTexture")) {
        GlowEffectCPUImpl cpuImpl; cpuImpl.applyCPU(src, dst); return;
    }
}

class GlowEffect::Impl {
public:
    std::shared_ptr<GlowEffectCPUImpl> cpuImpl_;
    std::shared_ptr<GlowEffectGPUImpl> gpuImpl_;

    Impl() {
        cpuImpl_ = std::make_shared<GlowEffectCPUImpl>();
        gpuImpl_ = std::make_shared<GlowEffectGPUImpl>();
    }
};

GlowEffect::GlowEffect() : impl_(new Impl()) {
    setDisplayName(ArtifactCore::UniString("Glow (Rasterizer)"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(impl_->cpuImpl_);
    setGPUImpl(impl_->gpuImpl_);
}

GlowEffect::~GlowEffect() {
    delete impl_;
}

void GlowEffect::setGlowGain(float gain) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setGlowGain(gain);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setGlowGain(gain);
    }
}

float GlowEffect::glowGain() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->glowGain();
    }
    return 0.0f;
}

void GlowEffect::setLayerCount(int count) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setLayerCount(count);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setLayerCount(count);
    }
}

int GlowEffect::layerCount() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->layerCount();
    }
    return 0;
}

void GlowEffect::setBaseSigma(float sigma) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setBaseSigma(sigma);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setBaseSigma(sigma);
    }
}

float GlowEffect::baseSigma() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->baseSigma();
    }
    return 0.0f;
}

void GlowEffect::setSigmaGrowth(float growth) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setSigmaGrowth(growth);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setSigmaGrowth(growth);
    }
}

float GlowEffect::sigmaGrowth() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->sigmaGrowth();
    }
    return 0.0f;
}

void GlowEffect::setBaseAlpha(float alpha) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setBaseAlpha(alpha);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setBaseAlpha(alpha);
    }
}

float GlowEffect::baseAlpha() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->baseAlpha();
    }
    return 0.0f;
}

void GlowEffect::setAlphaFalloff(float falloff) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setAlphaFalloff(falloff);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setAlphaFalloff(falloff);
    }
}

float GlowEffect::alphaFalloff() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->alphaFalloff();
    }
    return 0.0f;
}

std::vector<ArtifactCore::AbstractProperty> GlowEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    props.reserve(2);

    auto& gainProp = props.emplace_back();
    gainProp.setName("glowGain");
    gainProp.setType(ArtifactCore::PropertyType::Float);
    gainProp.setDefaultValue(QVariant(static_cast<double>(glowGain())));
    gainProp.setValue(QVariant(static_cast<double>(glowGain())));

    auto& layerCountProp = props.emplace_back();
    layerCountProp.setName("layerCount");
    layerCountProp.setType(ArtifactCore::PropertyType::Integer);
    layerCountProp.setDefaultValue(QVariant(layerCount()));
    layerCountProp.setValue(QVariant(layerCount()));

    // Additional properties can be added similarly
    return props;
}

void GlowEffect::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    QString n = name.toQString();
    if (n == "glowGain") {
        setGlowGain(static_cast<float>(value.toDouble()));
    } else if (n == "layerCount") {
        setLayerCount(value.toInt());
    }
}

}
