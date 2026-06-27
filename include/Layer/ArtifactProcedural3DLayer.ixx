module;

#include <memory>
#include <vector>
#include <QColor>
#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVariant>

export module Artifact.Layer.Procedural3D;

import Artifact.Layer.Abstract;

export namespace Artifact {

enum class Procedural3DLayerKind {
    Terrain = 0,
    PathTube = 1
};

class ArtifactProcedural3DLayer : public ArtifactAbstractLayer {
private:
    class Impl;
    Impl* impl_ = nullptr;
    void drawResolved(ArtifactIRenderer* renderer, int qualityOverride);

public:
    explicit ArtifactProcedural3DLayer(Procedural3DLayerKind kind = Procedural3DLayerKind::Terrain);
    ~ArtifactProcedural3DLayer() override;

    Procedural3DLayerKind generatorKind() const;
    void setGeneratorKind(Procedural3DLayerKind kind);

    void draw(ArtifactIRenderer* renderer) override;
    void drawLOD(ArtifactIRenderer* renderer, DetailLevel lod) override;
    QRectF localBounds() const override;
    QString debugState() const;
    void loadPreset(const QString& presetName);
    QStringList availablePresets() const;
    QJsonObject toJson() const override;
    static ArtifactAbstractLayerPtr fromJson(const QJsonObject& obj);
    void fromJsonProperties(const QJsonObject& obj) override;

    bool hasVideo() const override { return true; }
    bool hasAudio() const override { return false; }

    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;
};

std::shared_ptr<ArtifactProcedural3DLayer> createTerrainLayer();
std::shared_ptr<ArtifactProcedural3DLayer> createPathTubeLayer();

} // namespace Artifact
