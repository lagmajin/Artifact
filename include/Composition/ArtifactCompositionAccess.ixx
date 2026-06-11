module;
#include <memory>
#include <QVector>

export module Artifact.Composition.Access;

import Frame.Position;
import Frame.Rate;
import Artifact.Layer.Abstract;
import Utils.Id;

export namespace Artifact {

class ArtifactAbstractCompositionAccess {
public:
  virtual ~ArtifactAbstractCompositionAccess() = default;
  virtual ArtifactCore::CompositionID id() const = 0;
  virtual ArtifactCore::FrameRate frameRate() const = 0;
  virtual ArtifactCore::FramePosition framePosition() const = 0;
  virtual ArtifactAbstractLayerPtr layerById(const ArtifactCore::LayerID &id) = 0;
  virtual QVector<ArtifactAbstractLayerPtr> allLayer() = 0;
};

} // namespace Artifact
