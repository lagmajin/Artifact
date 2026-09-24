#include <algorithm>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

import Artifact.Layer.Abstract;

namespace Artifact {

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

GuideDefinition GuideDefinition::fromJson(const QJsonObject& obj) {
  GuideDefinition guide;
  guide.guideId = obj.value(QStringLiteral("guideId")).toString();
  guide.name = obj.value(QStringLiteral("name")).toString();
  guide.purpose = obj.value(QStringLiteral("purpose")).toString();
  guide.orientation = static_cast<GuideOrientation>(obj.value(
      QStringLiteral("orientation")).toInt(static_cast<int>(
      GuideOrientation::Horizontal)));
  guide.position = obj.value(QStringLiteral("position")).toDouble(0.0);
  guide.start = obj.value(QStringLiteral("start")).toDouble(0.0);
  guide.end = obj.value(QStringLiteral("end")).toDouble(0.0);
  guide.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
  guide.priority = static_cast<GuidePriority>(obj.value(
      QStringLiteral("priority")).toInt(static_cast<int>(
      GuidePriority::Normal)));
  guide.semanticTag = static_cast<GuideSemanticTag>(obj.value(
      QStringLiteral("semanticTag")).toInt(static_cast<int>(
      GuideSemanticTag::Custom)));
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

GuideBinding GuideBinding::fromJson(const QJsonObject& obj) {
  GuideBinding binding;
  binding.guideId = obj.value(QStringLiteral("guideId")).toString();
  binding.role = obj.value(QStringLiteral("role")).toString();
  binding.offset = obj.value(QStringLiteral("offset")).toDouble(0.0);
  binding.follow = obj.value(QStringLiteral("follow")).toBool(false);
  binding.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
  binding.priority = static_cast<GuidePriority>(obj.value(
      QStringLiteral("priority")).toInt(static_cast<int>(
      GuidePriority::Normal)));
  return binding;
}

QJsonObject GuideSet::toJson() const {
  QJsonObject obj;
  obj.insert(QStringLiteral("ownerId"), ownerId);
  QJsonArray guideArray;
  for (const auto& guide : guides) guideArray.append(guide.toJson());
  obj.insert(QStringLiteral("guides"), guideArray);
  QJsonArray bindingArray;
  for (const auto& binding : bindings) bindingArray.append(binding.toJson());
  obj.insert(QStringLiteral("bindings"), bindingArray);
  return obj;
}

GuideSet GuideSet::fromJson(const QJsonObject& obj) {
  GuideSet set;
  set.ownerId = obj.value(QStringLiteral("ownerId")).toString();
  for (const auto& value : obj.value(QStringLiteral("guides")).toArray()) {
    set.guides.append(GuideDefinition::fromJson(value.toObject()));
  }
  for (const auto& value : obj.value(QStringLiteral("bindings")).toArray()) {
    set.bindings.append(GuideBinding::fromJson(value.toObject()));
  }
  return set;
}

QVector<GuideDefinition> GuideSet::guidesForSemanticTag(
    GuideSemanticTag tag) const {
  QVector<GuideDefinition> result;
  for (const auto& guide : guides) {
    if (guide.semanticTag == tag) result.append(guide);
  }
  return result;
}

QVector<GuideDefinition> GuideSet::enabledGuides() const {
  QVector<GuideDefinition> result;
  for (const auto& guide : guides) {
    if (guide.enabled) result.append(guide);
  }
  return result;
}

QVector<GuideBinding> GuideSet::enabledBindings() const {
  QVector<GuideBinding> result;
  for (const auto& binding : bindings) {
    if (binding.enabled) result.append(binding);
  }
  return result;
}

GuideDefinition* GuideSet::guideById(const QString& guideId) {
  for (auto& guide : guides) {
    if (guide.guideId == guideId) return &guide;
  }
  return nullptr;
}

void GuideSet::sortByPriority() {
  std::sort(guides.begin(), guides.end(),
            [](const GuideDefinition& a, const GuideDefinition& b) {
              return static_cast<int>(a.priority) >
                     static_cast<int>(b.priority);
            });
  std::sort(bindings.begin(), bindings.end(),
            [](const GuideBinding& a, const GuideBinding& b) {
              return static_cast<int>(a.priority) >
                     static_cast<int>(b.priority);
            });
}

} // namespace Artifact
