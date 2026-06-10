#pragma once

#include <memory>
#include <QString>

namespace Artifact {

class ArtifactAbstractComposition;

std::shared_ptr<ArtifactAbstractComposition> resolveArtifactCompositionLayerSource(const QString& compositionId);

} // namespace Artifact
