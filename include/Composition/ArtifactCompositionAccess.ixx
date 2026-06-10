module;
#include <memory>
#include <QVector>

export module Artifact.Composition.Access;

import Frame.Position;
import Frame.Rate;
import Utils.Id;

export namespace Artifact {

class ArtifactAbstractLayer;

class ArtifactAbstractCompositionAccess {
public:
  virtual ~ArtifactAbstractCompositionAccess() = default;
  virtual FrameRate frameRate() const = 0;
  virtual FramePosition framePosition() const = 0;
  virtual std::shared_ptr<ArtifactAbstractLayer> layerById(const LayerID &id) = 0;
  virtual QVector<std::shared_ptr<ArtifactAbstractLayer>> allLayer() = 0;
};

} // namespace Artifact
