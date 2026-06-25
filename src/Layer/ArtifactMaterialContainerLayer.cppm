module;
#include <utility>
#include <algorithm>
#include <QPointF>
#include <QTransform>
#include <QJsonArray>
#include <QJsonObject>

module Artifact.Layer.MaterialContainer;

import std;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Color.Float;
import Property.Abstract;
import Property.Group;
import Utils.Id;

namespace Artifact {

QJsonObject MaterialContainerSlot::toJson() const {
    QJsonObject obj;
    obj["slotId"] = slotId;
    obj["name"] = name;
    obj["enabled"] = enabled;
    if (layer) {
        obj["layer"] = layer->toJson();
    }
    return obj;
}

MaterialContainerSlot MaterialContainerSlot::fromJson(const QJsonObject& obj) {
    MaterialContainerSlot slot;
    slot.slotId = obj.value("slotId").toString();
    slot.name = obj.value("name").toString();
    slot.enabled = obj.value("enabled").toBool(true);
    if (obj.contains("layer") && obj.value("layer").isObject()) {
        slot.layer = ArtifactAbstractLayer::fromJson(obj.value("layer").toObject());
    }
    return slot;
}

class ArtifactMaterialContainerLayer::Impl {
public:
    std::vector<MaterialContainerSlot> materials;
    int exposedIndex = 0;
};

ArtifactMaterialContainerLayer::ArtifactMaterialContainerLayer()
    : impl_(std::make_unique<Impl>()) {
    setLayerName(QStringLiteral("Material Container"));
}

ArtifactMaterialContainerLayer::~ArtifactMaterialContainerLayer() = default;

bool ArtifactMaterialContainerLayer::isGroupLayer() const {
    return false;
}

void ArtifactMaterialContainerLayer::setComposition(QObject* comp) {
    ArtifactAbstractLayer::setComposition(comp);
    for (auto& slot : impl_->materials) {
        if (slot.layer) {
            slot.layer->setComposition(comp);
        }
    }
}

void ArtifactMaterialContainerLayer::setComposition(void* comp) {
    ArtifactAbstractLayer::setComposition(comp);
    auto* composition = compositionObject();
    for (auto& slot : impl_->materials) {
        if (slot.layer) {
            slot.layer->setComposition(composition);
        }
    }
}

void ArtifactMaterialContainerLayer::draw(ArtifactIRenderer* renderer) {
    if (!renderer || !isVisible() || opacity() <= 0.0f) {
        return;
    }
    if (const auto exposed = exposedLayer()) {
        exposed->draw(renderer);
    }

    const QRectF bounds = localBounds();
    if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        return;
    }
    const QTransform transform = getGlobalTransform();
    const QPointF tl = transform.map(bounds.topLeft());
    const QPointF tr = transform.map(bounds.topRight());
    const QPointF br = transform.map(bounds.bottomRight());
    const QPointF bl = transform.map(bounds.bottomLeft());
    const ArtifactCore::FloatColor outerColor{0.94f, 0.68f, 0.20f, 0.92f};
    const ArtifactCore::FloatColor innerColor{0.24f, 0.16f, 0.06f, 0.70f};
    renderer->drawSolidLine({static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                            {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                            outerColor, 1.8f);
    renderer->drawSolidLine({static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                            {static_cast<float>(br.x()), static_cast<float>(br.y())},
                            outerColor, 1.8f);
    renderer->drawSolidLine({static_cast<float>(br.x()), static_cast<float>(br.y())},
                            {static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                            outerColor, 1.8f);
    renderer->drawSolidLine({static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                            {static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                            outerColor, 1.8f);
    renderer->drawSolidLine({static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                            {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                            innerColor, 0.8f);
}

int ArtifactMaterialContainerLayer::materialCount() const {
    return static_cast<int>(impl_->materials.size());
}

int ArtifactMaterialContainerLayer::exposedIndex() const {
    return impl_->exposedIndex;
}

void ArtifactMaterialContainerLayer::setExposedIndex(int index) {
    if (impl_->materials.empty()) {
        impl_->exposedIndex = 0;
        return;
    }
    impl_->exposedIndex = std::clamp(index, 0, static_cast<int>(impl_->materials.size()) - 1);
    Q_EMIT changed();
}

ArtifactAbstractLayerPtr ArtifactMaterialContainerLayer::materialAt(int index) const {
    if (index < 0 || index >= static_cast<int>(impl_->materials.size())) {
        return {};
    }
    return impl_->materials[static_cast<size_t>(index)].layer;
}

ArtifactAbstractLayerPtr ArtifactMaterialContainerLayer::exposedLayer() const {
    return materialAt(impl_->exposedIndex);
}

void ArtifactMaterialContainerLayer::addMaterial(ArtifactAbstractLayerPtr layer, const QString& name) {
    insertMaterialAt(static_cast<int>(impl_->materials.size()), std::move(layer), name);
}

void ArtifactMaterialContainerLayer::insertMaterialAt(int index, ArtifactAbstractLayerPtr layer, const QString& name) {
    if (!layer) {
        return;
    }
    if (index < 0) {
        index = 0;
    }
    if (index > static_cast<int>(impl_->materials.size())) {
        index = static_cast<int>(impl_->materials.size());
    }
    MaterialContainerSlot slot;
    slot.slotId = QStringLiteral("slot-%1").arg(layer->id().toQString());
    slot.name = name.isEmpty() ? layer->layerName() : name;
    slot.layer = std::move(layer);
    slot.layer->setComposition(compositionObject());
    slot.layer->clearParent();
    impl_->materials.insert(impl_->materials.begin() + index, std::move(slot));
    if (impl_->exposedIndex >= index) {
        ++impl_->exposedIndex;
    }
    Q_EMIT changed();
}

bool ArtifactMaterialContainerLayer::removeMaterialAt(int index) {
    if (index < 0 || index >= static_cast<int>(impl_->materials.size())) {
        return false;
    }
    impl_->materials.erase(impl_->materials.begin() + index);
    if (impl_->materials.empty()) {
        impl_->exposedIndex = 0;
    } else if (impl_->exposedIndex >= static_cast<int>(impl_->materials.size())) {
        impl_->exposedIndex = static_cast<int>(impl_->materials.size()) - 1;
    }
    Q_EMIT changed();
    return true;
}

void ArtifactMaterialContainerLayer::clearMaterials() {
    impl_->materials.clear();
    impl_->exposedIndex = 0;
    Q_EMIT changed();
}

const std::vector<MaterialContainerSlot>& ArtifactMaterialContainerLayer::materials() const {
    return impl_->materials;
}

QRectF ArtifactMaterialContainerLayer::localBounds() const {
    if (const auto exposed = exposedLayer()) {
        return exposed->localBounds();
    }
    return QRectF(0.0, 0.0, 100.0, 100.0);
}

QJsonObject ArtifactMaterialContainerLayer::toJson() const {
    QJsonObject obj = ArtifactAbstractLayer::toJson();
    obj["type"] = static_cast<int>(LayerType::MaterialContainer);
    obj["layerType"] = QStringLiteral("MaterialContainer");

    QJsonObject containerObj;
    containerObj["schemaVersion"] = 1;
    containerObj["exposedIndex"] = impl_->exposedIndex;
    QJsonArray slots;
    for (const auto& slot : impl_->materials) {
        slots.append(slot.toJson());
    }
    containerObj["slots"] = slots;
    obj["materialContainer"] = containerObj;
    return obj;
}

void ArtifactMaterialContainerLayer::fromJsonProperties(const QJsonObject& obj) {
    ArtifactAbstractLayer::fromJsonProperties(obj);
    clearMaterials();

    const QJsonObject containerObj = obj.value("materialContainer").toObject();
    impl_->exposedIndex = std::max(0, containerObj.value("exposedIndex").toInt(obj.value("exposedIndex").toInt(0)));
    QJsonArray arr;
    if (containerObj.contains("slots") && containerObj.value("slots").isArray()) {
        arr = containerObj.value("slots").toArray();
    } else if (obj.contains("materials") && obj.value("materials").isArray()) {
        arr = obj.value("materials").toArray();
    }
    if (!arr.isEmpty()) {
        for (const auto& v : arr) {
            auto slot = MaterialContainerSlot::fromJson(v.toObject());
            if (slot.layer) {
                slot.layer->setComposition(compositionObject());
            }
            impl_->materials.push_back(slot);
        }
    }
    if (!impl_->materials.empty()) {
        impl_->exposedIndex = std::clamp(impl_->exposedIndex, 0, static_cast<int>(impl_->materials.size()) - 1);
    } else {
        impl_->exposedIndex = 0;
    }
}

std::vector<ArtifactCore::PropertyGroup> ArtifactMaterialContainerLayer::getLayerPropertyGroups() const {
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
    ArtifactCore::PropertyGroup materialGroup(QStringLiteral("Material Container"));

    auto countProp = persistentLayerProperty(QStringLiteral("materialContainer.slotCount"),
                                             ArtifactCore::PropertyType::Integer,
                                             materialCount(), -110);
    countProp->setDisplayLabel(QStringLiteral("Slot Count"));
    materialGroup.addProperty(countProp);

    auto exposedProp = persistentLayerProperty(QStringLiteral("materialContainer.exposedIndex"),
                                               ArtifactCore::PropertyType::Integer,
                                               exposedIndex(), -100);
    exposedProp->setDisplayLabel(QStringLiteral("Exposed Index"));
    materialGroup.addProperty(exposedProp);

    groups.push_back(materialGroup);
    return groups;
}

bool ArtifactMaterialContainerLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value) {
    if (propertyPath == QStringLiteral("materialContainer.exposedIndex")) {
        setExposedIndex(value.toInt());
        return true;
    }
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

} // namespace Artifact
