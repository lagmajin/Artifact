module;
#include <memory>
#include <wobjectdefs.h>
#include <QJsonObject>
export module Artifact.Composition._2D;

import Utils.Id;
import Color.Float;
import Artifact.Layers;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;

export namespace Artifact {

using namespace ArtifactCore;

class ArtifactComposition2D : public ArtifactAbstractComposition {
private:
  class Impl;
  Impl* impl_;
  ArtifactComposition2D(const ArtifactComposition2D&) = delete;
  ArtifactComposition2D& operator=(const ArtifactComposition2D&) = delete;

public:
  explicit ArtifactComposition2D(const CompositionID& id, const ArtifactCompositionInitParams& params);
  ~ArtifactComposition2D();
};

using ArtifactComposition = ArtifactComposition2D;
using ArtifactComposition2DPtr = std::shared_ptr<ArtifactComposition2D>;

}
