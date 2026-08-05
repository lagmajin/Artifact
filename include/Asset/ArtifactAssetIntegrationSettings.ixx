module;
#include <QDir>
#include <QStandardPaths>
#include <QString>

export module Artifact.Asset.IntegrationSettings;

import Configuration.ConfigLayer;
import Configuration.LayeredConfigStore;

export namespace Artifact {

struct ArtifactAssetIntegrationSettings {
    bool openAssetIOEnabled = false;
    QString managerIdentifier;
    QString managerConfigPath;

    static ArtifactCore::LayeredConfigStore& configStore() {
        auto& store = ArtifactCore::LayeredConfigStore::instance();
        if (!store.isLoaded(ArtifactCore::ConfigLayer::User)) {
            const QString path = QDir(QStandardPaths::writableLocation(
                QStandardPaths::AppDataLocation)).filePath(QStringLiteral("settings.cbor"));
            store.loadLayer(ArtifactCore::ConfigLayer::User, path);
        }
        return store;
    }

    static ArtifactAssetIntegrationSettings load() {
        auto& settings = configStore();
        ArtifactAssetIntegrationSettings result;
        result.openAssetIOEnabled = settings.valueBool("asset.openassetio.enabled", false);
        result.managerIdentifier = settings.valueString("asset.openassetio.managerIdentifier");
        result.managerConfigPath = settings.valueString("asset.openassetio.managerConfigPath");
        return result;
    }

    void save() const {
        auto& settings = configStore();
        settings.setValue(ArtifactCore::ConfigLayer::User, "asset.openassetio.enabled", openAssetIOEnabled);
        settings.setValue(ArtifactCore::ConfigLayer::User, "asset.openassetio.managerIdentifier", managerIdentifier);
        settings.setValue(ArtifactCore::ConfigLayer::User, "asset.openassetio.managerConfigPath", managerConfigPath);
        settings.saveLayer(ArtifactCore::ConfigLayer::User);
    }
};

} // namespace Artifact
