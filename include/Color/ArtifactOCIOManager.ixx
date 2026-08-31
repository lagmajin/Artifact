module;

#include "../../../ArtifactCore/include/Define/DllExportMacro.hpp"
#include <memory>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QJsonObject>
#include <wobjectdefs.h>

export module Artifact.Color.OCIOManager;

import Color.OCIOConfig;
import Color.ScienceManager;
import Image.ImageF32x4_RGBA;

export namespace Artifact {

/// OCIO Manager - bridges OCIOConfig (Core) with the Artifact color management layer.
/// Manages config lifecycle, preset switching, and synchronizes with ColorScienceManager.
class LIBRARY_DLL_API ArtifactOCIOManager : public QObject {
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

    // Viewer-only adjustments applied after the OCIO display transform.
    // They do not alter the project working-space or source interpretation.
    float viewerExposure() const;
    void setViewerExposure(float ev);
    float viewerGamma() const;
    void setViewerGamma(float gamma);

    // Apply to ColorScienceManager
    void syncToColorScienceManager(ArtifactColorScienceManager* mgr) const;

    /// Apply OCIO view transform (working→display) to an image.
    void applyViewTransformToImage(ArtifactCore::ImageF32x4_RGBA& image) const;

    /// Generate the OCIO HLSL program for the active display transform.
    /// The returned source is a function/library fragment; the renderer owns
    /// the compute-wrapper and resource binding stage.
    QString gpuViewTransformShader() const;
    /// Return shader source plus the OCIO texture/uniform binding metadata.
    QJsonObject gpuViewTransformDescriptor() const;
    /// Bake the active view transform to an RGB 3D LUT in [0,1] domain.
    bool bakeViewTransformLUT(int size, QVector<float>& rgbValues,
                              float domainMin = 0.0f,
                              float domainMax = 1.0f) const;

    /// Explicitly decode a source interpretation and convert RGB into the
    /// active working space. Alpha is preserved and no range clamp is applied.
    /// Empty source values mean that the caller has already supplied linear RGB.
    void applyInputTransformToWorkingImage(
        ArtifactCore::ImageF32x4_RGBA& image,
        const QString& sourceColorSpace,
        const QString& sourceTransferFunction) const;

    // Persistence
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& obj);

private:
    ArtifactOCIOManager();

    class Impl;
    Impl* impl_;

protected:
    ~ArtifactOCIOManager() override;
};

} // namespace Artifact
