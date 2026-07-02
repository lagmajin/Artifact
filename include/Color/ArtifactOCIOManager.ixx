module;

#include <memory>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <wobjectdefs.h>

export module Artifact.Color.OCIOManager;

import Color.OCIOConfig;
import Color.ScienceManager;
import Image.ImageF32x4_RGBA;

export namespace Artifact {

/// OCIO Manager - bridges OCIOConfig (Core) with the Artifact color management layer.
/// Manages config lifecycle, preset switching, and synchronizes with ColorScienceManager.
class ArtifactOCIOManager : public QObject {
    W_OBJECT(ArtifactOCIOManager)

public:
    static ArtifactOCIOManager* instance();

    // Config management
    bool setActivePreset(const QString& presetName); // "ACES", "sRGB", "Rec709", "Rec2020", "Custom"
    bool loadConfigFile(const QString& path);
    bool loadConfig(const ArtifactCore::OCIOConfig& config);
    void clearConfig();

    // Access
    const ArtifactCore::OCIOConfig* activeConfig() const;
    QString activePresetName() const;
    bool hasActiveConfig() const;

    // Queries for UI
    QStringList availablePresets() const;
    QStringList availableWorkingSpaces() const;
    QStringList availableDisplays() const;
    QStringList availableViews(const QString& display) const;

    // Active settings
    QString workingSpace() const;
    void setWorkingSpace(const QString& cs);
    QString display() const;
    void setDisplay(const QString& display);
    QString view() const;
    void setView(const QString& view);
    QString looks() const;
    void setLooks(const QString& looks);

    // Apply to ColorScienceManager
    void syncToColorScienceManager(ArtifactColorScienceManager* mgr) const;

    /// Apply OCIO view transform (working→display) to an image.
    void applyViewTransformToImage(ArtifactCore::ImageF32x4_RGBA& image) const;

    // Persistence
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& obj);

    // Signals
    void configChanged() W_SIGNAL(configChanged);
    void workingSpaceChanged(const QString& cs) W_SIGNAL(workingSpaceChanged, cs);
    void displayViewChanged(const QString& display, const QString& view) W_SIGNAL(displayViewChanged, display, view);

private:
    ArtifactOCIOManager();

    class Impl;
    Impl* impl_;

protected:
    ~ArtifactOCIOManager() override;
};

} // namespace Artifact
