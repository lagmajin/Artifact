module;
#include <memory>
#include <vector>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QJsonObject>
#include <QRectF>

export module Artifact.Layer.MaterialContainer;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;

export namespace Artifact {

struct MaterialContainerSlot {
    QString slotId;
    QString name;
    bool enabled = true;
    ArtifactAbstractLayerPtr layer;

    QJsonObject toJson() const;
    static MaterialContainerSlot fromJson(const QJsonObject& obj);
};

class ArtifactMaterialContainerLayer : public ArtifactAbstractLayer {
public:
    ArtifactMaterialContainerLayer();
    ~ArtifactMaterialContainerLayer() override;

    bool isGroupLayer() const override;
    void setComposition(QObject* comp) override;
    void setComposition(void* comp) override;
    void draw(ArtifactIRenderer* renderer) override;

    int materialCount() const;
    int exposedIndex() const;
    void setExposedIndex(int index);

    ArtifactAbstractLayerPtr materialAt(int index) const;
    ArtifactAbstractLayerPtr exposedLayer() const;
    void addMaterial(ArtifactAbstractLayerPtr layer, const QString& name = QString());
    void insertMaterialAt(int index, ArtifactAbstractLayerPtr layer, const QString& name = QString());
    bool removeMaterialAt(int index);
    void clearMaterials();
    const std::vector<MaterialContainerSlot>& materials() const;

    QJsonObject toJson() const override;
    void fromJsonProperties(const QJsonObject& obj) override;
    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;
    QRectF localBounds() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Artifact
