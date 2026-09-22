#include <QString>

import Artifact.Layer.Abstract;

namespace Artifact {

void ArtifactAbstractLayer::drawLOD(ArtifactIRenderer* renderer, DetailLevel) {
  draw(renderer);
}

UniString ArtifactAbstractLayer::className() const { return QString(); }

const ArtifactCore::ImageF32x4_RGBA*
ArtifactAbstractLayer::resolveLayerSourceOverride() const {
  return nullptr;
}

bool ArtifactAbstractLayer::isGroupLayer() const { return false; }
bool ArtifactAbstractLayer::isNullLayer() const { return false; }
bool ArtifactAbstractLayer::isConstructionLayer() const { return false; }
bool ArtifactAbstractLayer::isCompositionBackgroundLayer() const { return false; }
bool ArtifactAbstractLayer::shouldIncludeInFinalRender() const { return true; }
bool ArtifactAbstractLayer::isCloneLayer() const { return false; }
bool ArtifactAbstractLayer::hasAudio() const { return false; }
bool ArtifactAbstractLayer::hasVideo() const { return false; }

} // namespace Artifact
