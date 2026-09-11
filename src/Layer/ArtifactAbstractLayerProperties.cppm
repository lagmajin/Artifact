module;
#include <algorithm>
#include <QJsonArray>
#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QVector>

module Artifact.Layer.Abstract;

namespace Artifact {

QRectF LayerBounds::boundsFor(LayerBoundsKind kind) const {
  switch (kind) {
    case LayerBoundsKind::Source:
      return sourceBounds;
    case LayerBoundsKind::Visible:
      return visibleBounds;
    case LayerBoundsKind::Effect:
      return effectBounds;
    case LayerBoundsKind::Mask:
      return maskBounds;
    case LayerBoundsKind::Layout:
      return layoutBounds;
  }
  return layoutBounds;
}

LayerBounds ArtifactAbstractLayer::contentBounds() const {
  const QRectF source = localBounds();
  const QRectF visible = transformedBoundingBox();
  LayerBounds bounds;
  bounds.sourceBounds = source;
  bounds.visibleBounds = visible;
  // Effects may read pixels outside the transformed layer rectangle. Apply
  // their shared ROI contract in stack order so bounds users (damage tracking,
  // cache invalidation and a future tiled renderer) get the same expansion.
  RenderROI effectROI(visible);
  for (const auto& effect : getEffects()) {
    if (!effect || !effect->isEnabled()) {
      continue;
    }
    const EffectROIHint hint = effect->roiHint();
    if (hint.requiresFullFrame) {
      // A layer cannot determine the composition canvas here. Preserve its
      // visible rectangle; the composition host promotes this to full-frame.
      continue;
    }
    effectROI = effect->expandedROI(effectROI);
  }
  bounds.effectBounds = effectROI.rect;
  bounds.maskBounds = visible;
  bounds.layoutBounds = source.isValid() ? source : visible;
  return bounds;
}

QJsonObject GuideDefinition::toJson() const {
  QJsonObject obj;
  obj.insert(QStringLiteral("guideId"), guideId);
  obj.insert(QStringLiteral("name"), name);
  obj.insert(QStringLiteral("purpose"), purpose);
  obj.insert(QStringLiteral("orientation"), static_cast<int>(orientation));
  obj.insert(QStringLiteral("position"), position);
  obj.insert(QStringLiteral("start"), start);
  obj.insert(QStringLiteral("end"), end);
  obj.insert(QStringLiteral("enabled"), enabled);
  obj.insert(QStringLiteral("priority"), static_cast<int>(priority));
  obj.insert(QStringLiteral("semanticTag"), static_cast<int>(semanticTag));
  return obj;
}

GuideDefinition GuideDefinition::fromJson(const QJsonObject &obj) {
  GuideDefinition guide;
  guide.guideId = obj.value(QStringLiteral("guideId")).toString();
  guide.name = obj.value(QStringLiteral("name")).toString();
  guide.purpose = obj.value(QStringLiteral("purpose")).toString();
  guide.orientation = static_cast<GuideOrientation>(
      obj.value(QStringLiteral("orientation")).toInt(static_cast<int>(GuideOrientation::Horizontal)));
  guide.position = obj.value(QStringLiteral("position")).toDouble(0.0);
  guide.start = obj.value(QStringLiteral("start")).toDouble(0.0);
  guide.end = obj.value(QStringLiteral("end")).toDouble(0.0);
  guide.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
  guide.priority = static_cast<GuidePriority>(
      obj.value(QStringLiteral("priority")).toInt(static_cast<int>(GuidePriority::Normal)));
  guide.semanticTag = static_cast<GuideSemanticTag>(
      obj.value(QStringLiteral("semanticTag")).toInt(static_cast<int>(GuideSemanticTag::Custom)));
  return guide;
}

QJsonObject GuideBinding::toJson() const {
  QJsonObject obj;
  obj.insert(QStringLiteral("guideId"), guideId);
  obj.insert(QStringLiteral("role"), role);
  obj.insert(QStringLiteral("offset"), offset);
  obj.insert(QStringLiteral("follow"), follow);
  obj.insert(QStringLiteral("enabled"), enabled);
  obj.insert(QStringLiteral("priority"), static_cast<int>(priority));
  return obj;
}

GuideBinding GuideBinding::fromJson(const QJsonObject &obj) {
  GuideBinding binding;
  binding.guideId = obj.value(QStringLiteral("guideId")).toString();
  binding.role = obj.value(QStringLiteral("role")).toString();
  binding.offset = obj.value(QStringLiteral("offset")).toDouble(0.0);
  binding.follow = obj.value(QStringLiteral("follow")).toBool(false);
  binding.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
  binding.priority = static_cast<GuidePriority>(
      obj.value(QStringLiteral("priority")).toInt(static_cast<int>(GuidePriority::Normal)));
  return binding;
}

QJsonObject GuideSet::toJson() const {
  QJsonObject obj;
  obj.insert(QStringLiteral("ownerId"), ownerId);
  QJsonArray guideArray;
  for (const auto &guide : guides) {
    guideArray.append(guide.toJson());
  }
  obj.insert(QStringLiteral("guides"), guideArray);
  QJsonArray bindingArray;
  for (const auto &binding : bindings) {
    bindingArray.append(binding.toJson());
  }
  obj.insert(QStringLiteral("bindings"), bindingArray);
  return obj;
}

GuideSet GuideSet::fromJson(const QJsonObject &obj) {
  GuideSet set;
  set.ownerId = obj.value(QStringLiteral("ownerId")).toString();
  for (const auto &value : obj.value(QStringLiteral("guides")).toArray()) {
    set.guides.append(GuideDefinition::fromJson(value.toObject()));
  }
  for (const auto &value : obj.value(QStringLiteral("bindings")).toArray()) {
    set.bindings.append(GuideBinding::fromJson(value.toObject()));
  }
  return set;
}

QVector<GuideDefinition> GuideSet::guidesForSemanticTag(GuideSemanticTag tag) const {
  QVector<GuideDefinition> result;
  for (const auto& g : guides) {
    if (g.semanticTag == tag) {
      result.append(g);
    }
  }
  return result;
}

QVector<GuideDefinition> GuideSet::enabledGuides() const {
  QVector<GuideDefinition> result;
  for (const auto& g : guides) {
    if (g.enabled) {
      result.append(g);
    }
  }
  return result;
}

QVector<GuideBinding> GuideSet::enabledBindings() const {
  QVector<GuideBinding> result;
  for (const auto& b : bindings) {
    if (b.enabled) {
      result.append(b);
    }
  }
  return result;
}

GuideDefinition* GuideSet::guideById(const QString& guideId) {
  for (auto& g : guides) {
    if (g.guideId == guideId) {
      return &g;
    }
  }
  return nullptr;
}

void GuideSet::sortByPriority() {
  std::sort(guides.begin(), guides.end(), [](const GuideDefinition& a, const GuideDefinition& b) {
    return static_cast<int>(a.priority) > static_cast<int>(b.priority);
  });
  std::sort(bindings.begin(), bindings.end(), [](const GuideBinding& a, const GuideBinding& b) {
    return static_cast<int>(a.priority) > static_cast<int>(b.priority);
  });
}


void ArtifactAbstractLayer::append3DTransformProperties(
    ArtifactCore::PropertyGroup &transformGroup,
    const ArtifactCore::AnimatableTransform3D &transform,
    double positionRange, double anchorRange) const {
  using namespace ArtifactCore;

  auto posZProp = persistentLayerProperty(
      QStringLiteral("transform.position.z"), PropertyType::Float,
      transform.positionZ(), -293);
  posZProp->setDisplayLabel(QStringLiteral("Position Z"));
  posZProp->setUnit(QStringLiteral("px"));
  posZProp->setStep(1.0);
  posZProp->setSoftRange(-positionRange, positionRange);
  posZProp->setAnimatable(true);
  transformGroup.addProperty(posZProp);

  auto rotXProp = persistentLayerProperty(
      QStringLiteral("transform.rotation.x"), PropertyType::Float,
      transform.rotationX(), -292);
  rotXProp->setDisplayLabel(QStringLiteral("Rotation X"));
  rotXProp->setUnit(QStringLiteral("deg"));
  rotXProp->setStep(1.0);
  rotXProp->setSoftRange(-180.0, 180.0);
  rotXProp->setAnimatable(true);
  transformGroup.addProperty(rotXProp);

  auto rotYProp = persistentLayerProperty(
      QStringLiteral("transform.rotation.y"), PropertyType::Float,
      transform.rotationY(), -291);
  rotYProp->setDisplayLabel(QStringLiteral("Rotation Y"));
  rotYProp->setUnit(QStringLiteral("deg"));
  rotYProp->setStep(1.0);
  rotYProp->setSoftRange(-180.0, 180.0);
  rotYProp->setAnimatable(true);
  transformGroup.addProperty(rotYProp);

  auto rotZProp = persistentLayerProperty(
      QStringLiteral("transform.rotation.z"), PropertyType::Float,
      transform.rotation(), -290);
  rotZProp->setDisplayLabel(QStringLiteral("Rotation Z (alias)"));
  rotZProp->setTooltip(QStringLiteral(
      "Same channel as Rotation; explicit Z path for expressions."));
  rotZProp->setUnit(QStringLiteral("deg"));
  rotZProp->setStep(1.0);
  rotZProp->setSoftRange(-180.0, 180.0);
  rotZProp->setAnimatable(true);
  transformGroup.addProperty(rotZProp);

  auto scaleZProp = persistentLayerProperty(
      QStringLiteral("transform.scale.z"), PropertyType::Float,
      transform.scaleZ(), -289);
  scaleZProp->setDisplayLabel(QStringLiteral("Scale Z"));
  scaleZProp->setAnimatable(true);
  scaleZProp->setStep(0.01);
  scaleZProp->setSoftRange(0.0, 2.0);
  transformGroup.addProperty(scaleZProp);

  auto anchorZProp = persistentLayerProperty(
      QStringLiteral("transform.anchor.z"), PropertyType::Float,
      transform.anchorZ(), -288);
  anchorZProp->setDisplayLabel(QStringLiteral("Anchor Z"));
  anchorZProp->setUnit(QStringLiteral("px"));
  anchorZProp->setStep(1.0);
  anchorZProp->setSoftRange(-anchorRange, anchorRange);
  anchorZProp->setAnimatable(true);
  transformGroup.addProperty(anchorZProp);
}

} // namespace Artifact

