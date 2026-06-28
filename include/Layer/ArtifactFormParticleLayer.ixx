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
import Size;

export namespace Artifact {

enum class FormParticleBlendMode {
    Additive = 0,
    Subtractive = 1,
    Alpha = 2,
    Screen = 3,
    Multiply = 4
};

enum class FormParticleBillboardMode {
    None = 0,
    ScreenAligned = 1,
    ViewPlane = 2,
    VelocityAligned = 3
};

enum class FormParticleSortMode {
    None = 0,
    Distance = 1,
    OldestFirst = 2,
    YoungestFirst = 3
};

struct FormParticleRenderSettings {
    FormParticleBlendMode blendMode = FormParticleBlendMode::Alpha;
    FormParticleBillboardMode billboardMode =
        FormParticleBillboardMode::ScreenAligned;
    FormParticleSortMode sortMode = FormParticleSortMode::None;
    bool depthTest = false;
    bool depthWrite = false;
    bool softParticles = false;
};

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

    FormParticleRenderSettings renderSettings;

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
