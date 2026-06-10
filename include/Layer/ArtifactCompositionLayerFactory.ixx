#pragma once

#include <memory>

namespace Artifact {

class ArtifactAbstractLayer;

std::shared_ptr<ArtifactAbstractLayer> createArtifactCompositionLayer();

} // namespace Artifact
