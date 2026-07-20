module;
#include <QSettings>
#include <QString>

export module Artifact.Asset.IntegrationSettings;

export namespace Artifact {

struct ArtifactAssetIntegrationSettings {
    bool openAssetIOEnabled = false;
    QString managerIdentifier;
    QString managerConfigPath;

    static ArtifactAssetIntegrationSettings load() {
        QSettings settings;
        settings.beginGroup(QStringLiteral("AssetIntegration/OpenAssetIO"));
        ArtifactAssetIntegrationSettings result;
        result.openAssetIOEnabled = settings.value(QStringLiteral("enabled"), false).toBool();
        result.managerIdentifier = settings.value(QStringLiteral("managerIdentifier")).toString();
        result.managerConfigPath = settings.value(QStringLiteral("managerConfigPath")).toString();
        settings.endGroup();
        return result;
    }

    void save() const {
        QSettings settings;
        settings.beginGroup(QStringLiteral("AssetIntegration/OpenAssetIO"));
        settings.setValue(QStringLiteral("enabled"), openAssetIOEnabled);
        settings.setValue(QStringLiteral("managerIdentifier"), managerIdentifier);
        settings.setValue(QStringLiteral("managerConfigPath"), managerConfigPath);
        settings.endGroup();
        settings.sync();
    }
};

} // namespace Artifact
