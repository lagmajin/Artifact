module;

#include <memory>
#include <utility>

#include <QJsonObject>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVariant>
#include <QStringList>

module Artifact.Layer.ParametricComposition;

import Artifact.Layer.Abstract;
import Artifact.Service.Project;
import Composition.ParametricComposition;
import Property.Abstract;
import Property.Group;

namespace Artifact {

namespace {

ArtifactCore::PropertyType propertyTypeForPublishedValue(const QVariant& value)
{
    switch (value.typeId()) {
    case QMetaType::Bool:
        return ArtifactCore::PropertyType::Boolean;
    case QMetaType::QString:
        return ArtifactCore::PropertyType::String;
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::UInt:
    case QMetaType::ULongLong:
        return ArtifactCore::PropertyType::Integer;
    case QMetaType::Float:
    case QMetaType::Double:
        return ArtifactCore::PropertyType::Double;
    default:
        return ArtifactCore::PropertyType::String;
    }
}

QString publishedControlPropertyPath(const QString& controlId)
{
    return QStringLiteral("published.") + controlId;
}

QString publishedControlSourcePath(const QString& controlId)
{
    return QStringLiteral("published.meta.") + controlId + QStringLiteral(".sourceParameterKey");
}

QString normalizedPublishedControlId(const QString& source)
{
    QString result;
    result.reserve(source.size());
    bool lastWasUnderscore = false;
    for (const QChar ch : source.trimmed().toLower()) {
        if (ch.isLetterOrNumber()) {
            result.append(ch);
            lastWasUnderscore = false;
        } else if (!result.isEmpty() && !lastWasUnderscore) {
            result.append(QChar('_'));
            lastWasUnderscore = true;
        }
    }
    while (result.endsWith(QChar('_'))) {
        result.chop(1);
    }
    return result.isEmpty() ? QStringLiteral("control") : result;
}

} // namespace

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

void ArtifactParametricCompositionLayer::setPublishedControlOverride(
    const QString& controlId, const QVariant& value)
{
    impl_->instance_.setPublishedControlOverride(controlId, value);
    Q_EMIT changed();
}

void ArtifactParametricCompositionLayer::clearPublishedControlOverride(const QString& controlId)
{
    impl_->instance_.clearPublishedControlOverride(controlId);
    Q_EMIT changed();
}

void ArtifactParametricCompositionLayer::applyDataRow(const QVariantMap& rowValues)
{
    impl_->instance_.applyDataRow(rowValues);
    Q_EMIT changed();
}

QVariantMap ArtifactParametricCompositionLayer::dataRowValues() const
{
    return impl_->instance_.dataRowValues();
}

bool ArtifactParametricCompositionLayer::addParameterDefinition(
    const QString& key,
    const QVariant& defaultValue,
    const QString& displayName)
{
    if (key.trimmed().isEmpty()) {
        return false;
    }

    auto currentDefinition = impl_->instance_.definition();
    auto updatedDefinition = std::make_shared<ParametricCompositionDefinition>(
        currentDefinition ? *currentDefinition
                          : makeDefaultParametricCompositionDefinition(
                                QStringLiteral("parametric.layer"),
                                QStringLiteral("Parametric Composition")));

    if (updatedDefinition->hasParameter(key)) {
        return false;
    }

    ParametricCompositionParameter parameter;
    parameter.key = key.trimmed();
    parameter.displayName =
        displayName.trimmed().isEmpty() ? parameter.key : displayName.trimmed();
    parameter.defaultValue = defaultValue;
    if (!updatedDefinition->addParameter(parameter)) {
        return false;
    }

    setDefinition(updatedDefinition);
    return true;
}

bool ArtifactParametricCompositionLayer::publishParameter(
    const QString& key,
    const QString& controlId,
    const QString& displayName)
{
    auto currentDefinition = impl_->instance_.definition();
    if (!currentDefinition || !currentDefinition->hasParameter(key)) {
        return false;
    }

    auto updatedDefinition =
        std::make_shared<ParametricCompositionDefinition>(*currentDefinition);
    ParametricCompositionPublishedControl control;
    control.sourceParameterKey = key;
    control.controlId = controlId.trimmed().isEmpty()
                            ? normalizedPublishedControlId(key)
                            : controlId.trimmed();
    control.displayName =
        displayName.trimmed().isEmpty() ? key : displayName.trimmed();

    const auto* parameter = updatedDefinition->parameter(key);
    if (parameter) {
        control.defaultValue = parameter->defaultValue;
        control.displayName = displayName.trimmed().isEmpty()
                                  ? parameter->displayName
                                  : displayName.trimmed();
        control.valueType = QString::fromLatin1(parameter->defaultValue.typeName());
    }

    if (updatedDefinition->hasPublishedControl(control.controlId)) {
        return false;
    }
    if (!updatedDefinition->addPublishedControl(control)) {
        return false;
    }

    setDefinition(updatedDefinition);
    return true;
}

bool ArtifactParametricCompositionLayer::unpublishControl(const QString& controlId)
{
    auto currentDefinition = impl_->instance_.definition();
    if (!currentDefinition || !currentDefinition->hasPublishedControl(controlId)) {
        return false;
    }

    auto updatedDefinition =
        std::make_shared<ParametricCompositionDefinition>(*currentDefinition);
    if (!updatedDefinition->removePublishedControl(controlId)) {
        return false;
    }

    setDefinition(updatedDefinition);
    return true;
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

    const auto def = impl_->instance_.definition();
    if (def) {
        auto publishedCountProp = persistentLayerProperty(
            QStringLiteral("parametric.publishedControlCount"),
            PropertyType::Integer,
            static_cast<int>(def->publishedControls().size()),
            -108);
        paramGroup.addProperty(publishedCountProp);
        auto dataBindingCountProp = persistentLayerProperty(
            QStringLiteral("parametric.dataBindingCount"),
            PropertyType::Integer,
            static_cast<int>(def->dataBindings().size()),
            -107);
        paramGroup.addProperty(dataBindingCountProp);
    }

    auto dataRowCountProp = persistentLayerProperty(
        QStringLiteral("parametric.dataRowValueCount"),
        PropertyType::Integer,
        static_cast<int>(impl_->instance_.dataRowValues().size()),
        -106);
    paramGroup.addProperty(dataRowCountProp);

    groups.push_back(paramGroup);

    if (const auto def = impl_->instance_.definition()) {
        PropertyGroup publishedGroup(QStringLiteral("Published Controls"));
        for (const auto& control : def->publishedControls()) {
            if (control.hidden) {
                continue;
            }

            const QVariant currentValue = impl_->instance_.publishedControlValue(
                control.controlId,
                control.defaultValue);
            const QString propertyPath = publishedControlPropertyPath(control.controlId);
            auto prop = persistentLayerProperty(
                propertyPath,
                propertyTypeForPublishedValue(currentValue.isValid() ? currentValue : control.defaultValue),
                currentValue,
                -60 + control.order);
            if (!control.displayName.isEmpty()) {
                prop->setDisplayName(control.displayName);
            }
            publishedGroup.addProperty(prop);

            auto sourceProp = persistentLayerProperty(
                publishedControlSourcePath(control.controlId),
                PropertyType::String,
                control.sourceParameterKey,
                2000 + control.order);
            if (!control.displayName.isEmpty()) {
                sourceProp->setDisplayName(control.displayName + QStringLiteral(" Source"));
            }
            publishedGroup.addProperty(sourceProp);
        }

        if (publishedGroup.propertyCount() > 0) {
            groups.push_back(publishedGroup);
        }
    }

    return groups;
}

bool ArtifactParametricCompositionLayer::setLayerPropertyValue(
    const QString& propertyPath, const QVariant& value)
{
    if (propertyPath.startsWith(QStringLiteral("published.")) &&
        !propertyPath.startsWith(QStringLiteral("published.meta."))) {
        const QString controlId = propertyPath.mid(QStringLiteral("published.").size());
        if (!controlId.isEmpty()) {
            setPublishedControlOverride(controlId, value);
            return true;
        }
    }
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
