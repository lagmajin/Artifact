module;

module Artifact.Project;

import Artifact.Layer.Abstract;
import Artifact.Layers.Noise;

namespace Artifact {

bool isArtifactNoiseLayer(const ArtifactAbstractLayer* layer)
{
    return dynamic_cast<const ArtifactNoiseLayer*>(layer) != nullptr;
}

} // namespace Artifact
