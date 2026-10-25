module;

#include <QJsonObject>

module Artifact.Project;

import Asset.Manager;

namespace Artifact {

QJsonObject sourceAssetRegistrySnapshot()
{
    return ArtifactCore::AssetManager::instance().sourceRegistrySnapshot();
}

} // namespace Artifact
