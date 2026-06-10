#include "../../include/Layer/ArtifactLayerJsonFactory.ixx"

#include <QJsonObject>

import Artifact.Layer.Factory;

namespace Artifact {

std::shared_ptr<ArtifactAbstractLayer> createArtifactLayerFromJson(const QJsonObject& json) {
    return ArtifactLayerFactory::createFromJson(json);
}

} // namespace Artifact
