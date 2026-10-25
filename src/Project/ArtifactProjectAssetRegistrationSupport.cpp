module;

#include <QString>
#include <QUuid>

module Artifact.Project;

import Asset.Database;
import Asset.Importer;
import AssetType;

namespace Artifact {

QUuid registerProjectAsset(const QString& path, const QUuid& preferredId)
{
    const AssetType detectedType = AssetImporter::detectType(path);
    if (detectedType == AssetType::Unknown) {
        return preferredId;
    }
    const QUuid registeredId = preferredId.isNull()
        ? AssetDatabase::instance().registerAsset(path, detectedType)
        : AssetDatabase::instance().registerAsset(
              path, detectedType, preferredId);
    return registeredId.isNull() ? preferredId : registeredId;
}

void clearProjectAssetDatabase()
{
    AssetDatabase::instance().clear();
}

} // namespace Artifact
