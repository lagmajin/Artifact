module;

export module Artifact.Composition.Access;

export import Artifact.Layer.Abstract;
import Frame.Position;
import Frame.Rate;
import Utils.Id;

export namespace Artifact {

class ArtifactAbstractCompositionAccess {
public:
  virtual ~ArtifactAbstractCompositionAccess() = default;
  virtual ArtifactCore::CompositionID id() const = 0;
  virtual ArtifactCore::FrameRate frameRate() const = 0;
  virtual ArtifactCore::FramePosition framePosition() const = 0;
  virtual ArtifactAbstractLayerPtr layerById(const ArtifactCore::LayerID& id) const = 0;
};

} // namespace Artifact
