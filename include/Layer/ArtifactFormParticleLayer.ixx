module;
#include <memory>
#include <vector>
#include <cstdint>
#include <wobjectdefs.h>

#include <QObject>
#include <QColor>
#include <QJsonObject>
#include <QRectF>
#include <QStringList>
#include <QSize>
#include <QVariant>

export module Artifact.Layer.FormParticle;

import Artifact.Layer.Abstract;
import Artifact.Generator.Particle;
import Size;

export namespace Artifact {

struct FormParticleSettings {
    FormParticleSettings();

    enum class GeneratorMode {
        Grid2D = 0,
        Grid3D = 1,
        LayerMap = 2
    };

    enum class ColorMode {
        Solid = 0,
        AxisGradient = 1,
        SourceColor = 2
    };

    enum class OriginMode {
        Center = 0,
        TopLeft = 1,
        LayerBounds = 2
    };

    GeneratorMode generatorMode = GeneratorMode::Grid2D;
    ColorMode colorMode = ColorMode::Solid;
    OriginMode originMode = OriginMode::Center;

    int columns = 40;
    int rows = 22;
    int depth = 1;
    int maxParticles = 4096;

    float spacingX = 24.0f;
    float spacingY = 24.0f;
    float spacingZ = 24.0f;

    float particleSize = 4.0f;
    float particleOpacity = 1.0f;
    float noiseAmount = 0.0f;
    float noiseScale = 0.015f;
    float noiseSpeed = 0.5f;
    float noisePhase = 0.0f;
    float twistAmount = 0.0f;
    float falloff = 0.0f;

    std::uint32_t seed = 1;

    QString sourcePath;
    float sourceAlphaThreshold = 0.05f;
    float sourceLumaThreshold = 0.05f;

    QColor solidColor = QColor(255, 255, 255);
    QColor gradientStartColor = QColor(74, 196, 255);
    QColor gradientEndColor = QColor(255, 118, 202);

    ParticleRenderSettings renderSettings;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};

class ArtifactFormParticleLayer : public ArtifactAbstractLayer {
private:
    class Impl;
    Impl* impl_ = nullptr;

public:
    ArtifactFormParticleLayer();
    ~ArtifactFormParticleLayer() override;

    void draw(ArtifactIRenderer* renderer) override;
    QRectF localBounds() const override;
    QString debugState() const;
    QJsonObject toJson() const override;
    static ArtifactAbstractLayerPtr fromJson(const QJsonObject& obj);
    void fromJsonProperties(const QJsonObject& obj) override;

    bool hasVideo() const override { return true; }
    bool hasAudio() const override { return false; }

    void loadPreset(const QString& presetName);
    QStringList availablePresets() const;

    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;
    void applyPropertiesFromJson(const QJsonObject& obj);
};

std::shared_ptr<ArtifactFormParticleLayer> createFormParticleLayer();
std::shared_ptr<ArtifactFormParticleLayer> createFormParticleLayer(const QString& preset);

} // namespace Artifact
