module;
#include <opencv2/opencv.hpp>

module Artifact.Effect.Glow;

import std;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Glow; // ArtifactCoreのOpenCVグロー実装
import Property.Abstract;
import QtCore;

namespace Artifact {

void GlowEffectCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const ImageF32x4_RGBA& srcImage = src.image();
    cv::Mat srcMat = srcImage.toCVMat();
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
    ImageF32x4_RGBA dstImage;
    dstImage.setFromCVMat(dstMat);
    //dst = ImageF32x4RGBAWithCache(dstImage);
}

void GlowEffectGPUImpl::applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    // 現在はCPUバックエンドにフォールバック
    // TODO: HLSLシェーダの実装
    GlowEffectCPUImpl cpuImpl;
    cpuImpl.applyCPU(src, dst);
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

    ArtifactCore::AbstractProperty gainProp;
    gainProp.setName("glowGain");
    gainProp.setType(ArtifactCore::PropertyType::Float);
    gainProp.setDefaultValue(QVariant(static_cast<double>(glowGain())));
    gainProp.setValue(QVariant(static_cast<double>(glowGain())));
    props.push_back(gainProp);

    ArtifactCore::AbstractProperty layerCountProp;
    layerCountProp.setName("layerCount");
    layerCountProp.setType(ArtifactCore::PropertyType::Integer);
    layerCountProp.setDefaultValue(QVariant(layerCount()));
    layerCountProp.setValue(QVariant(layerCount()));
    props.push_back(layerCountProp);

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