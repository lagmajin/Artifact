module;

#include <memory>
#include <utility>

#include <QJsonObject>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVariant>

module Artifact.Layer.ParametricComposition;

import Artifact.Layer.Abstract;
import Artifact.Service.Project;
import Composition.ParametricComposition;
import Property.Abstract;
import Property.Group;

namespace Artifact {

class ArtifactParametricCompositionLayer::Impl {
public:
    ParametricCompositionInstance instance_;
};

ArtifactParametricCompositionLayer::ArtifactParametricCompositionLayer()
    : impl_(new Impl())
{
}

ArtifactParametricCompositionLayer::~ArtifactParametricCompositionLayer()
{
    delete impl_;
}

ParametricCompositionInstance& ArtifactParametricCompositionLayer::parametricInstance()
{
    return impl_->instance_;
}

const ParametricCompositionInstance& ArtifactParametricCompositionLayer::parametricInstance() const
{
    return impl_->instance_;
}

void ArtifactParametricCompositionLayer::setDefinition(
    std::shared_ptr<const ParametricCompositionDefinition> definition)
{
    impl_->instance_.setDefinition(std::move(definition));
    Q_EMIT changed();
}

std::shared_ptr<const ParametricCompositionDefinition>
ArtifactParametricCompositionLayer::definition() const
{
    return impl_->instance_.definition();
}

void ArtifactParametricCompositionLayer::bindSlot(
    const QString& slotId, const ParametricCompositionInputBinding& binding)
{
    ParametricCompositionInputBinding b = binding;
    b.slotId = slotId;
    impl_->instance_.addInputBinding(b);
    Q_EMIT changed();
}

void ArtifactParametricCompositionLayer::unbindSlot(const QString& slotId)
{
    const auto& bindings = impl_->instance_.inputBindings();
    for (int i = 0; i < bindings.size(); ++i) {
        if (bindings[i].slotId == slotId) {
            impl_->instance_.removeInputBinding(i);
            break;
        }
    }
    Q_EMIT changed();
}

void ArtifactParametricCompositionLayer::clearBindings()
{
    impl_->instance_.clearInputBindings();
    Q_EMIT changed();
}

void ArtifactParametricCompositionLayer::setParamOverride(
    const QString& key, const QVariant& value)
{
    impl_->instance_.setParameterOverride(key, value);
    Q_EMIT changed();
}

void ArtifactParametricCompositionLayer::clearParamOverride(const QString& key)
{
    impl_->instance_.clearParameterOverride(key);
    Q_EMIT changed();
}

void ArtifactParametricCompositionLayer::draw(ArtifactIRenderer*)
{
    // Parametric composition layers are rendered through the composition view
    // drawing path, similar to precomp layers. The direct draw is a no-op.
}

QRectF ArtifactParametricCompositionLayer::localBounds() const
{
    if (auto def = impl_->instance_.definition()) {
        // Use definition metadata for bounds if available
        // For now, return a default size
        return QRectF(0, 0, 1920, 1080);
    }
    return QRectF(0, 0, 100, 100);
}

std::vector<PropertyGroup>
ArtifactParametricCompositionLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();

    PropertyGroup paramGroup(QStringLiteral("Parametric Composition"));

    // Show definition ID
    auto defIdProp = persistentLayerProperty(
        QStringLiteral("parametric.definitionId"),
        PropertyType::String,
        impl_->instance_.definition()
            ? impl_->instance_.definition()->definitionId()
            : QString(),
        -110);
    paramGroup.addProperty(defIdProp);

    // Show binding count
    auto bindingCountProp = persistentLayerProperty(
        QStringLiteral("parametric.bindingCount"),
        PropertyType::Integer,
        static_cast<int>(impl_->instance_.inputBindingCount()),
        -109);
    paramGroup.addProperty(bindingCountProp);

    groups.push_back(paramGroup);
    return groups;
}

bool ArtifactParametricCompositionLayer::setLayerPropertyValue(
    const QString& propertyPath, const QVariant& value)
{
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

QJsonObject ArtifactParametricCompositionLayer::toJson() const
{
    QJsonObject obj = ArtifactAbstractLayer::toJson();
    obj[QStringLiteral("type")] = static_cast<int>(LayerType::ParametricComposition);
    if (auto def = impl_->instance_.definition()) {
        obj[QStringLiteral("parametric.definitionId")] = def->definitionId();
        obj[QStringLiteral("parametric.displayName")] = def->displayName();
    }
    obj[QStringLiteral("parametric.instance")] = impl_->instance_.toJson();
    return obj;
}

} // namespace Artifact
