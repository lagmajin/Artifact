module;

#include <memory>
#include <optional>
#include <vector>

#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QVariant>

export module Artifact.Layer.ParametricComposition;

import Artifact.Layer.Abstract;
import Composition.ParametricComposition;

export namespace Artifact {

class ArtifactParametricCompositionLayer : public ArtifactAbstractLayer {
public:
    ArtifactParametricCompositionLayer();
    ~ArtifactParametricCompositionLayer() override;

    // Parametric composition instance access
    ParametricCompositionInstance& parametricInstance();
    const ParametricCompositionInstance& parametricInstance() const;
    void setDefinition(std::shared_ptr<const ParametricCompositionDefinition> definition);
    std::shared_ptr<const ParametricCompositionDefinition> definition() const;

    // Convenience: add binding for a slot
    void bindSlot(const QString& slotId, const ParametricCompositionInputBinding& binding);
    void unbindSlot(const QString& slotId);
    void clearBindings();

    // Parameter overrides
    void setParamOverride(const QString& key, const QVariant& value);
    void clearParamOverride(const QString& key);
    void setPublishedControlOverride(const QString& controlId, const QVariant& value);
    void clearPublishedControlOverride(const QString& controlId);
    void applyDataRow(const QVariantMap& rowValues);
    QVariantMap dataRowValues() const;
    bool addParameterDefinition(const QString& key, const QVariant& defaultValue, const QString& displayName = QString());
    bool publishParameter(const QString& key, const QString& controlId = QString(), const QString& displayName = QString());
    bool unpublishControl(const QString& controlId);

    // Layer overrides
    void draw(ArtifactIRenderer* renderer) override;
    QRectF localBounds() const override;
    std::vector<PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;
    QJsonObject toJson() const override;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
