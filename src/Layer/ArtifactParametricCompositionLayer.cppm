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
import Artifact.Layer.Image;
import Artifact.Service.Project;
import Composition.ParametricComposition;
import Property.Abstract;
import Property.Group;
import Memory.SharedPtr;

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
        return ArtifactCore::PropertyType::Float;
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
    CompositionID sourceCompositionId_;
    struct InputSnapshot {
        LayerID layerId;
        QString propertyPath;
        QVariant value;
    };
    std::vector<InputSnapshot> inputSnapshots_;
    std::vector<ArtifactImageLayer*> imageOverrides_;
    bool inputScopeActive_ = false;
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

CompositionID ArtifactParametricCompositionLayer::sourceCompositionId() const
{
    return impl_->sourceCompositionId_;
}

void ArtifactParametricCompositionLayer::setCompositionId(const CompositionID& id)
{
    impl_->sourceCompositionId_ = id;
    Q_EMIT changed();
}

SharedPtr<ArtifactAbstractComposition>
ArtifactParametricCompositionLayer::sourceComposition() const
{
    auto* service = ArtifactProjectService::instance();
    if (!service || impl_->sourceCompositionId_.isNil()) {
        return nullptr;
    }
    const auto result = service->findComposition(impl_->sourceCompositionId_);
    return result.ptr.lock();
}

bool ArtifactParametricCompositionLayer::beginInputBindingScope()
{
    if (impl_->inputScopeActive_) {
        return false;
    }
    const auto source = sourceComposition();
    if (!source) {
        return false;
    }

    impl_->inputSnapshots_.clear();
    impl_->imageOverrides_.clear();
    impl_->inputScopeActive_ = true;
    for (const auto& binding : impl_->instance_.inputBindings()) {
        if (!binding.connected || binding.targetLayerId.trimmed().isEmpty() ||
            (binding.targetPropertyPath.trimmed().isEmpty() &&
             binding.kind != ParametricCompositionSlotKind::Image &&
             binding.kind != ParametricCompositionSlotKind::Matte)) {
            continue;
        }
        const auto target = source->layerById(LayerID(binding.targetLayerId));
        if (binding.kind == ParametricCompositionSlotKind::Image ||
            binding.kind == ParametricCompositionSlotKind::Matte) {
            auto imageTarget = ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(target);
            const auto& buffer = binding.kind == ParametricCompositionSlotKind::Image
                ? binding.image : binding.matte;
            if (!imageTarget || buffer.isEmpty()) {
                continue;
            }
            imageTarget->setTemporarySourceOverride(&buffer);
            impl_->imageOverrides_.push_back(imageTarget.get());
            continue;
        }
        const auto property = target
            ? target->getProperty(binding.targetPropertyPath)
            : SharedPtr<ArtifactCore::AbstractProperty>{};
        if (!target || !property) {
            continue;
        }

        QVariant injectedValue;
        switch (binding.kind) {
        case ParametricCompositionSlotKind::SourceLayer:
            injectedValue = binding.sourceLayerId.toString();
            break;
        case ParametricCompositionSlotKind::Text:
            injectedValue = binding.text;
            break;
        default:
            // Image/matte buffers need a renderer-specific surface binding;
            // do not force them through QVariant or mutate the source.
            continue;
        }
        impl_->inputSnapshots_.push_back(
            {LayerID(binding.targetLayerId), binding.targetPropertyPath,
             property->getValue()});
        if (!target->setLayerPropertyValue(binding.targetPropertyPath,
                                           injectedValue)) {
            endInputBindingScope();
            return false;
        }
    }
    return true;
}

void ArtifactParametricCompositionLayer::endInputBindingScope()
{
    if (!impl_->inputScopeActive_) {
        return;
    }
    const auto source = sourceComposition();
    if (source) {
        for (auto it = impl_->inputSnapshots_.rbegin();
             it != impl_->inputSnapshots_.rend(); ++it) {
            if (const auto target = source->layerById(it->layerId)) {
                target->setLayerPropertyValue(it->propertyPath, it->value);
            }
        }
    }
    impl_->inputSnapshots_.clear();
    for (auto* imageLayer : impl_->imageOverrides_) {
        if (imageLayer) {
            imageLayer->setTemporarySourceOverride(nullptr);
        }
    }
    impl_->imageOverrides_.clear();
    impl_->inputScopeActive_ = false;
}

void ArtifactParametricCompositionLayer::setDefinition(
    SharedPtr<const ParametricCompositionDefinition> definition)
{
    impl_->instance_.setDefinition(std::move(definition));
    Q_EMIT changed();
}

SharedPtr<const ParametricCompositionDefinition>
ArtifactParametricCompositionLayer::definition() const
{
    return impl_->instance_.definition();
}

void ArtifactParametricCompositionLayer::bindSlot(
    const QString& slotId, const ParametricCompositionInputBinding& binding)
{
    const QString normalizedSlotId = slotId.trimmed();
    if (normalizedSlotId.isEmpty()) {
        return;
    }
    ParametricCompositionInputBinding b = binding;
    b.slotId = normalizedSlotId;
    if (const auto def = impl_->instance_.definition(); def &&
        b.wouldCreateCycle(def->definitionId())) {
        return;
    }
    const auto& bindings = impl_->instance_.inputBindings();
    for (int i = 0; i < bindings.size(); ++i) {
        if (bindings[i].slotId == normalizedSlotId) {
            impl_->instance_.setInputBinding(i, b);
            Q_EMIT changed();
            return;
        }
    }
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
    auto updatedDefinition = ArtifactCore::makeShared<ParametricCompositionDefinition>(
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
        ArtifactCore::makeShared<ParametricCompositionDefinition>(*currentDefinition);
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
        ArtifactCore::makeShared<ParametricCompositionDefinition>(*currentDefinition);
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
        const auto source = sourceSize();
        if (source.width > 0 && source.height > 0) {
            return QRectF(0, 0, source.width, source.height);
        }
        return QRectF(0, 0, 1920, 1080);
    }
    return QRectF(0, 0, 100, 100);
}

std::vector<PropertyGroup>
ArtifactParametricCompositionLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();

    PropertyGroup paramGroup(QStringLiteral("Parametric Composition"));

    auto sourceIdProp = persistentLayerProperty(
        QStringLiteral("parametric.sourceCompositionId"),
        PropertyType::String,
        impl_->sourceCompositionId_.toString(),
        -120);
    paramGroup.addProperty(sourceIdProp);

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
                prop->setDisplayLabel(control.displayName);
            }
            publishedGroup.addProperty(prop);

            auto sourceProp = persistentLayerProperty(
                publishedControlSourcePath(control.controlId),
                PropertyType::String,
                control.sourceParameterKey,
                2000 + control.order);
            if (!control.displayName.isEmpty()) {
                sourceProp->setDisplayLabel(control.displayName + QStringLiteral(" Source"));
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
    if (propertyPath == QStringLiteral("parametric.sourceCompositionId")) {
        setCompositionId(CompositionID(value.toString()));
        return true;
    }
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
    obj[QStringLiteral("parametric.sourceCompositionId")] =
        impl_->sourceCompositionId_.toString();
    if (auto def = impl_->instance_.definition()) {
        obj[QStringLiteral("parametric.definitionId")] = def->definitionId();
        obj[QStringLiteral("parametric.displayName")] = def->displayName();
    }
    obj[QStringLiteral("parametric.instance")] = impl_->instance_.toJson();
    return obj;
}

void ArtifactParametricCompositionLayer::fromJsonProperties(const QJsonObject& obj)
{
    ArtifactAbstractLayer::fromJsonProperties(obj);
    impl_->sourceCompositionId_ = CompositionID(
        obj.value(QStringLiteral("parametric.sourceCompositionId")).toString());
    const QJsonObject instanceObject =
        obj.value(QStringLiteral("parametric.instance")).toObject();
    if (!instanceObject.isEmpty()) {
        impl_->instance_ = ParametricCompositionInstance::fromJson(instanceObject);
    }
}

} // namespace Artifact
