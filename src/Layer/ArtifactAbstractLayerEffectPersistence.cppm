#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QString>
#include <QVariant>

import Artifact.Layer.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Service.Effect;
import Memory.SharedPtr;
import Property.SerializationBridge;

// Qt preprocessor macros are not carried by an imported IFC.
#undef QStringLiteral
#define QStringLiteral(text) QString::fromUtf8(text)

namespace Artifact {

using namespace ArtifactCore;

namespace {

// Effect IDs are uniquified per layer on insertion, so a second instance of a
// layer style serializes as `drop_shadow-2` while the factory only knows
// `drop_shadow`. Prefer the ID as written and only fall back to the base type
// when the catalog has no match, so a real effect ID that happens to end in
// digits is never rewritten.
QString resolveEffectFactoryId(const QString &effectId,
                               const QSet<QString> &knownEffectIds) {
  if (knownEffectIds.contains(effectId)) {
    return effectId;
  }
  const qsizetype dash = effectId.lastIndexOf(QLatin1Char('-'));
  if (dash > 0 && dash + 1 < effectId.size()) {
    bool numericSuffix = true;
    for (qsizetype i = dash + 1; i < effectId.size(); ++i) {
      if (!effectId.at(i).isDigit()) {
        numericSuffix = false;
        break;
      }
    }
    if (numericSuffix) {
      const QString base = effectId.left(dash);
      if (knownEffectIds.contains(base)) {
        return base;
      }
    }
  }
  return effectId;
}

} // namespace

void ArtifactAbstractLayer::applyPropertiesFromJson(const QJsonObject &obj) {
  // Default implementation: apply effect properties if matching effects exist
  // Subclasses should override to handle layer-specific fields
  if (!obj.contains("effects") || !obj["effects"].isArray())
    return;
  if (obj.contains("isAdjustment")) {
    setAdjustmentLayer(obj["isAdjustment"].toBool());
  }

  const auto arr = obj.value("effects").toArray();
  // Ids the effect service factory can actually build. Used to tell a real
  // effect type apart from a per-layer uniquified copy of one.
  QSet<QString> knownEffectIds;
  if (auto *factory = ArtifactEffectService::instance()) {
    const auto catalog = factory->availableEffects();
    knownEffectIds.reserve(static_cast<int>(catalog.size()));
    for (const auto &info : catalog) {
      knownEffectIds.insert(info.id.toString());
    }
  }
  for (const auto &ev : arr) {
    if (!ev.isObject())
      continue;
    auto eobj = ev.toObject();
    if (!eobj.contains("id"))
      continue;
    UniString eid(eobj["id"].toString().toStdString());
    auto eff = getEffect(eid);
    if (!eff) {
      // Rebuild the concrete effect type from the effect service factory rather
      // than an id list kept in sync by hand. Effects such as the layer styles
      // (drop_shadow / stroke / satin / effect.layerstyle.*) were previously
      // serialized but never recreated, so they were silently dropped on
      // reload even though the values were written correctly.
      const QString effectId = eobj.value(QStringLiteral("id")).toString();
      auto* service = ArtifactEffectService::instance();
      if (service) {
        auto created = service->createEffect(
            EffectID(resolveEffectFactoryId(effectId, knownEffectIds)));
        if (created) {
          eff = makeShared(created.release(), [](ArtifactAbstractEffect *p) { delete p; });
        }
      }
      if (eff) {
        // Restore the layer-unique id (factory created the base id).
        eff->setEffectID(eid);
        addEffect(eff);
      }
    }
    if (!eff)
      continue;
    if (eobj.contains(QStringLiteral("enabled"))) {
      eff->setEnabled(eobj.value(QStringLiteral("enabled")).toBool(true));
    }
    if (eobj.contains(QStringLiteral("pipelineStage"))) {
      eff->setPipelineStage(static_cast<EffectPipelineStage>(
          eobj.value(QStringLiteral("pipelineStage")).toInt(
              static_cast<int>(EffectPipelineStage::Rasterizer))));
    }
    setDirty(LayerDirtyFlag::Effect);
    if (!eobj.contains("properties") || !eobj["properties"].isArray())
      continue;
    auto props = eobj["properties"].toArray();
    for (const auto &pv : props) {
      if (!pv.isObject())
        continue;
      auto pobj = pv.toObject();
      QString name = pobj.value("name").toString();
      int t = pobj.value("type").toInt(
          static_cast<int>(ArtifactCore::PropertyType::String));
      ArtifactCore::PropertyType ptype =
          static_cast<ArtifactCore::PropertyType>(t);
      QVariant val;
      if (pobj.contains("value")) {
        if (ptype == ArtifactCore::PropertyType::Color &&
            pobj.value("value").isObject()) {
          auto col = pobj.value("value").toObject();
          double r = col.value("r").toDouble(0.0);
          double g = col.value("g").toDouble(0.0);
          double b = col.value("b").toDouble(0.0);
          double a = col.value("a").toDouble(1.0);
          QColor qc;
          qc.setRedF(static_cast<float>(r));
          qc.setGreenF(static_cast<float>(g));
          qc.setBlueF(static_cast<float>(b));
          qc.setAlphaF(static_cast<float>(a));
          val = QVariant(qc);
        } else {
          val = pobj.value("value").toVariant();
        }
      }
      eff->setPropertyValue(UniString(name.toStdString()), val);
      if (pobj.contains("keyframes") || pobj.contains("expression") ||
          pobj.contains("envelopes")) {
        auto editable = eff->editableProperty(name);
        if (editable) {
          ArtifactCore::SerializedProperty serialized;
          serialized.name = name;
          serialized.type = static_cast<int>(ptype);
          serialized.value = pobj.value("value");
          serialized.expression = pobj.value("expression").toString().trimmed().left(16384);
          serialized.keyframes = pobj.value("keyframes").toArray();
          serialized.envelopes = pobj.value("envelopes").toArray();
          ArtifactCore::PropertySerializationBridge::deserializeProperty(
              editable, serialized);
        }
      }
    }
  }
}
} // namespace Artifact

#undef QStringLiteral
