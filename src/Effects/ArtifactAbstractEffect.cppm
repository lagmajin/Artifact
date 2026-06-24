module;

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
#include <QString>

module Artifact.Effect.Abstract;




import Utils.Id;
import Utils.String.UniString;
import Artifact.Effect.Context;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Artifact.Effect.ImplBase;
import Property.Abstract;

namespace Artifact {

using namespace ArtifactCore;

class ArtifactAbstractEffect::Impl {
public:
    bool enabled = true;
    ComputeMode mode = ComputeMode::AUTO;
    UniString id;
    UniString name;
    EffectPipelineStage pipelineStage = EffectPipelineStage::Rasterizer; // Default to Rasterizer for retrocompatibility
    std::shared_ptr<ArtifactEffectImplBase> cpuImpl_;
    std::shared_ptr<ArtifactEffectImplBase> gpuImpl_;
    EffectContext context_;
    bool maskEnabled = false;
    std::shared_ptr<ImageF32x4_RGBA> maskImage;
    QString maskLayerId;
    QString maskName;
    bool maskInverted = false;
    float maskOpacity = 1.0f;
};

ArtifactAbstractEffect::ArtifactAbstractEffect() : impl_(new Impl()) {}

ArtifactAbstractEffect::~ArtifactAbstractEffect() {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->release();
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->release();
    }
    delete impl_;
}

bool ArtifactAbstractEffect::initialize() {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setContext(impl_->context_);
        if (!impl_->cpuImpl_->initialize()) {
            return false;
        }
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setContext(impl_->context_);
        if (!impl_->gpuImpl_->initialize()) {
            return false;
        }
    }
    return true;
}

void ArtifactAbstractEffect::release() {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->release();
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->release();
    }
}

void ArtifactAbstractEffect::setEnabled(bool enabled) { impl_->enabled = enabled; }

bool ArtifactAbstractEffect::isEnabled() const { return impl_->enabled; }

ComputeMode ArtifactAbstractEffect::computeMode() const { return impl_->mode; }

void ArtifactAbstractEffect::setComputeMode(ComputeMode mode) { impl_->mode = mode; }

UniString ArtifactAbstractEffect::effectID() const { return impl_->id; }

void ArtifactAbstractEffect::setEffectID(const UniString& id) { impl_->id = id; }

UniString ArtifactAbstractEffect::displayName() const { return impl_->name; }

void ArtifactAbstractEffect::setDisplayName(const UniString& name) { impl_->name = name; }

EffectPipelineStage ArtifactAbstractEffect::pipelineStage() const { return impl_->pipelineStage; }

void ArtifactAbstractEffect::setPipelineStage(EffectPipelineStage stage) { impl_->pipelineStage = stage; }

bool ArtifactAbstractEffect::hasMask() const {
    return impl_->maskEnabled || !impl_->maskLayerId.isEmpty() || !impl_->maskName.isEmpty();
}

void ArtifactAbstractEffect::setMaskEnabled(bool enabled) { impl_->maskEnabled = enabled; }

bool ArtifactAbstractEffect::maskEnabled() const { return impl_->maskEnabled; }

void ArtifactAbstractEffect::setMaskImage(const std::shared_ptr<ImageF32x4_RGBA>& maskImage) {
    impl_->maskImage = maskImage;
}

std::shared_ptr<ImageF32x4_RGBA> ArtifactAbstractEffect::maskImage() const {
    return impl_->maskImage;
}

void ArtifactAbstractEffect::setMaskLayerId(const QString& layerId) { impl_->maskLayerId = layerId; }

QString ArtifactAbstractEffect::maskLayerId() const { return impl_->maskLayerId; }

void ArtifactAbstractEffect::setMaskName(const QString& name) { impl_->maskName = name; }

QString ArtifactAbstractEffect::maskName() const { return impl_->maskName; }

void ArtifactAbstractEffect::setMaskInverted(bool inverted) { impl_->maskInverted = inverted; }

bool ArtifactAbstractEffect::maskInverted() const { return impl_->maskInverted; }

void ArtifactAbstractEffect::setMaskOpacity(float opacity) {
    impl_->maskOpacity = std::clamp(opacity, 0.0f, 1.0f);
}

float ArtifactAbstractEffect::maskOpacity() const { return impl_->maskOpacity; }

void ArtifactAbstractEffect::applyCPUOnly(const ImageF32x4RGBAWithCache& src,
                                         ImageF32x4RGBAWithCache& dst) {
    const ComputeMode previousMode = impl_->mode;
    impl_->mode = ComputeMode::CPU;
    apply(src, dst);
    impl_->mode = previousMode;
}

void ArtifactAbstractEffect::applyConfigured(const ImageF32x4RGBAWithCache& src,
                                             ImageF32x4RGBAWithCache& dst) {
    apply(src, dst);

    const bool outputMissing = dst.width() <= 0 || dst.height() <= 0;
    const bool outputSizeMismatch =
        !outputMissing && src.width() > 0 && src.height() > 0 &&
        (dst.width() != src.width() || dst.height() != src.height());
    if ((outputMissing || outputSizeMismatch) && impl_->cpuImpl_) {
        const ComputeMode previousMode = impl_->mode;
        impl_->mode = ComputeMode::CPU;
        apply(src, dst);
        impl_->mode = previousMode;
    }

    if (!impl_->maskEnabled || !impl_->maskImage) {
        return;
    }

    const ImageF32x4_RGBA& maskSrc = *impl_->maskImage;
    if (maskSrc.width() <= 0 || maskSrc.height() <= 0 ||
        dst.width() <= 0 || dst.height() <= 0 ||
        maskSrc.width() != dst.width() || maskSrc.height() != dst.height()) {
        return;
    }

    ImageF32x4_RGBA& dstImage = dst.image();
    const ImageF32x4_RGBA srcCopy = src.image().DeepCopy();
    const int width = dstImage.width();
    const int height = dstImage.height();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const FloatRGBA maskPixel = maskSrc.getPixel(x, y);
            float maskAlpha = maskPixel.a();
            if (impl_->maskInverted) {
                maskAlpha = 1.0f - maskAlpha;
            }
            maskAlpha = std::clamp(maskAlpha * impl_->maskOpacity, 0.0f, 1.0f);
            if (maskAlpha <= 0.0f) {
                dstImage.setPixel(x, y, srcCopy.getPixel(x, y));
                continue;
            }
            if (maskAlpha >= 1.0f) {
                continue;
            }

            const FloatRGBA basePixel = srcCopy.getPixel(x, y);
            const FloatRGBA effectPixel = dstImage.getPixel(x, y);
            const float inv = 1.0f - maskAlpha;
            dstImage.setPixel(
                x, y,
                FloatRGBA(
                    basePixel.r() * inv + effectPixel.r() * maskAlpha,
                    basePixel.g() * inv + effectPixel.g() * maskAlpha,
                    basePixel.b() * inv + effectPixel.b() * maskAlpha,
                    basePixel.a() * inv + effectPixel.a() * maskAlpha));
        }
    }
}

void ArtifactAbstractEffect::setContext(const EffectContext& context) {
    impl_->context_ = context;
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setContext(context);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setContext(context);
    }
}

void ArtifactAbstractEffect::apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
if (!impl_->enabled) {
    // Deep copy when effect is disabled
    dst = src.DeepCopy();
    return;
}

    ComputeMode mode = impl_->mode;
    
    // AUTOモードの場合はGPUサポート可否に応じて選択
    if (mode == ComputeMode::AUTO) {
        mode = supportsGPU() && impl_->gpuImpl_ ? ComputeMode::GPU : ComputeMode::CPU;
    }

    if (mode == ComputeMode::GPU && supportsGPU() && impl_->gpuImpl_) {
        // GPUバックエンドで処理
        impl_->gpuImpl_->applyGPU(src, dst);
    } else if (mode == ComputeMode::CPU && impl_->cpuImpl_) {
        // CPUバックエンドで処理
        impl_->cpuImpl_->applyCPU(src, dst);
    } else {
        // どちらの実装も利用できない場合はディープコピー
        dst = src.DeepCopy();
    }
}

void ArtifactAbstractEffect::setCPUImpl(std::shared_ptr<ArtifactEffectImplBase> impl) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->release();
    }
    impl_->cpuImpl_ = impl;
    if (impl_) {
        impl_->cpuImpl_->setContext(impl_->context_);
        impl_->cpuImpl_->initialize();
    }
}

void ArtifactAbstractEffect::setGPUImpl(std::shared_ptr<ArtifactEffectImplBase> impl) {
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->release();
    }
    impl_->gpuImpl_ = impl;
    if (impl_) {
        impl_->gpuImpl_->setContext(impl_->context_);
        impl_->gpuImpl_->initialize();
    }
}

std::shared_ptr<ArtifactEffectImplBase> ArtifactAbstractEffect::cpuImpl() const {
    return impl_->cpuImpl_;
}

std::shared_ptr<ArtifactEffectImplBase> ArtifactAbstractEffect::gpuImpl() const {
    return impl_->gpuImpl_;
}

std::vector<ArtifactCore::AbstractProperty> ArtifactAbstractEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    props.reserve(5);

    auto makeProp = [](const char* name, const QVariant& value) {
        ArtifactCore::AbstractProperty prop;
        prop.setName(QString::fromUtf8(name));
        prop.setValue(value);
        return prop;
    };

    props.push_back(makeProp("mask.enabled", impl_->maskEnabled));
    props.push_back(makeProp("mask.hasImage", static_cast<bool>(impl_->maskImage)));
    props.push_back(makeProp("mask.layerId", impl_->maskLayerId));
    props.push_back(makeProp("mask.name", impl_->maskName));
    props.push_back(makeProp("mask.inverted", impl_->maskInverted));
    props.push_back(makeProp("mask.opacity", impl_->maskOpacity));
    return props;
}

void ArtifactAbstractEffect::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("mask.enabled")) {
        setMaskEnabled(value.toBool());
        return;
    }
    if (key == QStringLiteral("mask.hasImage")) {
        Q_UNUSED(value);
        return;
    }
    if (key == QStringLiteral("mask.layerId")) {
        setMaskLayerId(value.toString());
        return;
    }
    if (key == QStringLiteral("mask.name")) {
        setMaskName(value.toString());
        return;
    }
    if (key == QStringLiteral("mask.inverted")) {
        setMaskInverted(value.toBool());
        return;
    }
    if (key == QStringLiteral("mask.opacity")) {
        setMaskOpacity(value.toFloat());
        return;
    }
    // Default: no-op. Subclasses override.
}

}
