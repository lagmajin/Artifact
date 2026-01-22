module;
#include <QString>

module Artifact.Effect.Abstract;

import std;
import Utils.Id;
import Utils.String.UniString;
import Artifact.Effect.Context;
import Image.ImageF32x4RGBAWithCache;

namespace Artifact {

class ArtifactAbstractEffect::Impl {
public:
    bool enabled = true;
    ComputeMode mode = ComputeMode::AUTO;
    UniString id;
    UniString name;
};

ArtifactAbstractEffect::ArtifactAbstractEffect() : impl_(new Impl()) {}

ArtifactAbstractEffect::~ArtifactAbstractEffect() { delete impl_; }

bool ArtifactAbstractEffect::initialize() { return true; }

void ArtifactAbstractEffect::release() {}

void ArtifactAbstractEffect::setEnabled(bool enabled) { impl_->enabled = enabled; }

bool ArtifactAbstractEffect::isEnabled() const { return impl_->enabled; }

ComputeMode ArtifactAbstractEffect::computeMode() const { return impl_->mode; }

void ArtifactAbstractEffect::setComputeMode(ComputeMode mode) { impl_->mode = mode; }

UniString ArtifactAbstractEffect::effectID() const { return impl_->id; }

void ArtifactAbstractEffect::setEffectID(const UniString& id) { impl_->id = id; }

UniString ArtifactAbstractEffect::displayName() const { return impl_->name; }

void ArtifactAbstractEffect::setDisplayName(const UniString& name) { impl_->name = name; }

}
