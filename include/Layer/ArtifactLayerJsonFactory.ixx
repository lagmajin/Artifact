#pragma once

#include <memory>

class QJsonObject;

namespace Artifact {

class ArtifactAbstractLayer;

std::shared_ptr<ArtifactAbstractLayer> createArtifactLayerFromJson(const QJsonObject& json);

} // namespace Artifact
