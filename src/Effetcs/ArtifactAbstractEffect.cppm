module;
#include <QString>
#include <opencv2/opencv.hpp>

module Artifact.Effect.Abstract;

import std;
import Utils.Id;
import Utils.String.UniString;
import Artifact.Effect.Context;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Artifact.Effect.ImplBase;

namespace Artifact {

class ArtifactAbstractEffect::Impl {
public:
    bool enabled = true;
    ComputeMode mode = ComputeMode::AUTO;
    UniString id;
    UniString name;
    std::shared_ptr<ArtifactEffectImplBase> cpuImpl_;
    std::shared_ptr<ArtifactEffectImplBase> gpuImpl_;
    EffectContext context_;
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
        dst = src;
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
        // どちらの実装も利用できない場合はコピー
        dst = src;
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

}