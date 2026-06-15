module;
#include <QList>
#include <QVariant>
#include <QVector>
#include <cmath>
#include <opencv2/core/mat.hpp>
#include <QtConcurrent>
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
module Artifact.Effect.LensDistortion;

import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import ImageProcessing.Distortion;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

void LensDistortionEffectCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const ImageF32x4_RGBA& srcImage = src.image();
    int width = srcImage.width();
    int height = srcImage.height();
    if (width <= 0 || height <= 0) return;

    float cx = centerX_ * static_cast<float>(width);
    float cy = centerY_ * static_cast<float>(height);
    float maxR = std::min(static_cast<float>(width), static_cast<float>(height)) * 0.5f;
    float k = distortion_ / 100.0f;
    if (invertDistortion_) k = -k;
    float zm = zoom_;

    std::vector<FloatRGBA> rowResults(static_cast<size_t>(width) * height);
    QVector<int> rows(height);
    std::iota(rows.begin(), rows.end(), 0);

    QtConcurrent::blockingMap(rows, [&](int y) {
        for (int x = 0; x < width; x++) {
            float dx = static_cast<float>(x) - cx;
            float dy = static_cast<float>(y) - cy;
            float r = std::sqrt(dx * dx + dy * dy) / maxR;

            float rDistorted = r * (1.0f + k * r * r);
            rDistorted /= zm;

            float srcX, srcY;
            if (r > 1e-6f) {
                float scale = (rDistorted * maxR) / (r * maxR);
                srcX = cx + dx * scale;
                srcY = cy + dy * scale;
            } else {
                srcX = cx;
                srcY = cy;
            }

            FloatRGBA pixel = sampleBilinear(srcImage, srcX, srcY);
            size_t idx = static_cast<size_t>(y) * width + x;
            rowResults[idx] = pixel;
        }
    });

    ImageF32x4_RGBA dstImage;
    dstImage.resize(width, height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t srcIdx = static_cast<size_t>(y) * width + x;
            dstImage.setPixel(x, y, rowResults[srcIdx]);
        }
    }

    dst = ImageF32x4RGBAWithCache(dstImage);
}

void LensDistortionEffectGPUImpl::applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { LensDistortionEffectCPUImpl cpuImpl; cpuImpl.setDistortion(distortion_); cpuImpl.setCenterX(centerX_); cpuImpl.setCenterY(centerY_); cpuImpl.setInvertDistortion(invertDistortion_); cpuImpl.setZoom(zoom_); cpuImpl.applyCPU(src, dst); return; }
    auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
    auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
    if (!paramsCB_) { Diligent::BufferDesc cbDesc; cbDesc.Name="LensDistortion/ParamsCB"; cbDesc.Size=sizeof(ParamsCB); cbDesc.Usage=Diligent::USAGE_DYNAMIC; cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER; cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(cbDesc,nullptr,&paramsCB_); }
    if (!paramsCB_) { LensDistortionEffectCPUImpl cpuImpl; cpuImpl.setDistortion(distortion_); cpuImpl.setCenterX(centerX_); cpuImpl.setCenterY(centerY_); cpuImpl.setInvertDistortion(invertDistortion_); cpuImpl.setZoom(zoom_); cpuImpl.applyCPU(src, dst); return; }
    static Diligent::ShaderResourceVariableDesc vars[] = { {Diligent::SHADER_TYPE_COMPUTE, "LensDistortionParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}, {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}, {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}, };
    if (!pipelineReady_) { ArtifactCore::ComputePipelineDesc desc; desc.name="LensDistortion/PSO"; desc.shaderSource=kLensDistortionHlsl; desc.entryPoint="main"; desc.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; desc.variables=vars; desc.variableCount=3; desc.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("LensDistortionParams", paramsCB_)) { LensDistortionEffectCPUImpl cpuImpl; cpuImpl.setDistortion(distortion_); cpuImpl.setCenterX(centerX_); cpuImpl.setCenterY(centerY_); cpuImpl.setInvertDistortion(invertDistortion_); cpuImpl.setZoom(zoom_); cpuImpl.applyCPU(src, dst); return; } pipelineReady_=true; }
    Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex; if (!createTextureFromImage(src, device_, &inputTex, "LensDistortion/InputTexture")) { LensDistortionEffectCPUImpl cpuImpl; cpuImpl.setDistortion(distortion_); cpuImpl.setCenterX(centerX_); cpuImpl.setCenterY(centerY_); cpuImpl.setInvertDistortion(invertDistortion_); cpuImpl.setZoom(zoom_); cpuImpl.applyCPU(src, dst); return; }
    Diligent::TextureDesc outDesc=inputTex->GetDesc(); outDesc.Usage=Diligent::USAGE_DEFAULT; outDesc.BindFlags=Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; outDesc.Name="LensDistortion/OutputTexture"; Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex; device_->CreateTexture(outDesc,nullptr,&outputTex); if (!outputTex) { LensDistortionEffectCPUImpl cpuImpl; cpuImpl.setDistortion(distortion_); cpuImpl.setCenterX(centerX_); cpuImpl.setCenterY(centerY_); cpuImpl.setInvertDistortion(invertDistortion_); cpuImpl.setZoom(zoom_); cpuImpl.applyCPU(src, dst); return; }
    void* mapped=nullptr; context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped); if (!mapped) { LensDistortionEffectCPUImpl cpuImpl; cpuImpl.setDistortion(distortion_); cpuImpl.setCenterX(centerX_); cpuImpl.setCenterY(centerY_); cpuImpl.setInvertDistortion(invertDistortion_); cpuImpl.setZoom(zoom_); cpuImpl.applyCPU(src, dst); return; }
    ParamsCB params{}; params.distortion=distortion_; params.centerX=centerX_; params.centerY=centerY_; params.invert=invertDistortion_?1.0f:0.0f; params.zoom=zoom_; std::memcpy(mapped,&params,sizeof(params)); context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
    if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) || !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { LensDistortionEffectCPUImpl cpuImpl; cpuImpl.setDistortion(distortion_); cpuImpl.setCenterX(centerX_); cpuImpl.setCenterY(centerY_); cpuImpl.setInvertDistortion(invertDistortion_); cpuImpl.setZoom(zoom_); cpuImpl.applyCPU(src, dst); return; }
    auto attribs=ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1); executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    if (!readbackTexture(device_, context_, outputTex, dst, "LensDistortion/StagingTexture")) { LensDistortionEffectCPUImpl cpuImpl; cpuImpl.setDistortion(distortion_); cpuImpl.setCenterX(centerX_); cpuImpl.setCenterY(centerY_); cpuImpl.setInvertDistortion(invertDistortion_); cpuImpl.setZoom(zoom_); cpuImpl.applyCPU(src, dst); return; }
}

class LensDistortionEffect::Impl {
public:
    std::shared_ptr<LensDistortionEffectCPUImpl> cpuImpl_;
    std::shared_ptr<LensDistortionEffectGPUImpl> gpuImpl_;

    Impl() {
        cpuImpl_ = std::make_shared<LensDistortionEffectCPUImpl>();
        gpuImpl_ = std::make_shared<LensDistortionEffectGPUImpl>();
    }
};

LensDistortionEffect::LensDistortionEffect() : impl_(new Impl()) {
    setDisplayName(ArtifactCore::UniString("Lens Distortion"));
    setPipelineStage(EffectPipelineStage::GeometryTransform);
    setCPUImpl(impl_->cpuImpl_);
    setGPUImpl(impl_->gpuImpl_);
}

LensDistortionEffect::~LensDistortionEffect() {
    delete impl_;
}

void LensDistortionEffect::setDistortion(float v) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setDistortion(v);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setDistortion(v);
}
float LensDistortionEffect::distortion() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->distortion() : 0.0f;
}

void LensDistortionEffect::setCenterX(float cx) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setCenterX(cx);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setCenterX(cx);
}
float LensDistortionEffect::centerX() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->centerX() : 0.5f;
}

void LensDistortionEffect::setCenterY(float cy) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setCenterY(cy);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setCenterY(cy);
}
float LensDistortionEffect::centerY() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->centerY() : 0.5f;
}

void LensDistortionEffect::setInvertDistortion(bool v) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setInvertDistortion(v);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setInvertDistortion(v);
}
bool LensDistortionEffect::invertDistortion() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->invertDistortion() : false;
}

void LensDistortionEffect::setZoom(float v) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setZoom(v);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setZoom(v);
}
float LensDistortionEffect::zoom() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->zoom() : 1.0f;
}

std::vector<ArtifactCore::AbstractProperty> LensDistortionEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    props.reserve(5);

    auto& distProp = props.emplace_back();
    distProp.setName("distortion");
    distProp.setType(ArtifactCore::PropertyType::Float);
    distProp.setDefaultValue(QVariant(0.0));
    distProp.setValue(QVariant(static_cast<double>(distortion())));
    distProp.setSoftRange(QVariant(-100.0), QVariant(100.0));

    auto& cxProp = props.emplace_back();
    cxProp.setName("centerX");
    cxProp.setType(ArtifactCore::PropertyType::Float);
    cxProp.setDefaultValue(QVariant(0.5));
    cxProp.setValue(QVariant(static_cast<double>(centerX())));
    cxProp.setSoftRange(QVariant(0.0), QVariant(1.0));

    auto& cyProp = props.emplace_back();
    cyProp.setName("centerY");
    cyProp.setType(ArtifactCore::PropertyType::Float);
    cyProp.setDefaultValue(QVariant(0.5));
    cyProp.setValue(QVariant(static_cast<double>(centerY())));
    cyProp.setSoftRange(QVariant(0.0), QVariant(1.0));

    auto& invProp = props.emplace_back();
    invProp.setName("invertDistortion");
    invProp.setType(ArtifactCore::PropertyType::Boolean);
    invProp.setDefaultValue(QVariant(false));
    invProp.setValue(QVariant(invertDistortion()));

    auto& zoomProp = props.emplace_back();
    zoomProp.setName("zoom");
    zoomProp.setType(ArtifactCore::PropertyType::Float);
    zoomProp.setDefaultValue(QVariant(1.0));
    zoomProp.setValue(QVariant(static_cast<double>(zoom())));
    zoomProp.setSoftRange(QVariant(0.1), QVariant(3.0));

    return props;
}

void LensDistortionEffect::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    QString n = name.toQString();
    if (n == "distortion") {
        setDistortion(static_cast<float>(value.toDouble()));
    } else if (n == "centerX") {
        setCenterX(static_cast<float>(value.toDouble()));
    } else if (n == "centerY") {
        setCenterY(static_cast<float>(value.toDouble()));
    } else if (n == "invertDistortion") {
        setInvertDistortion(value.toBool());
    } else if (n == "zoom") {
        setZoom(static_cast<float>(value.toDouble()));
    }
}

}
