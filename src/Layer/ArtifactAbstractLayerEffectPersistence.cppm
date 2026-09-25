#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVariant>

import Artifact.Layer.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Effect.Keying.ChromaKey;
import Artifact.Effect.Keying.DifferenceKey;
import Artifact.Effect.Keying.IBKKeyer;
import Artifact.Effect.Keying.LumaKey;
import Artifact.Effect.Rasterizer.DifferenceMatte;
import Artifact.Effect.Rasterizer.PosterizeTime;
import Memory.SharedPtr;
import Property.SerializationBridge;

// Qt preprocessor macros are not carried by an imported IFC.
#define QStringLiteral(text) QString::fromUtf8(text)

namespace Artifact {

using namespace ArtifactCore;

void ArtifactAbstractLayer::applyPropertiesFromJson(const QJsonObject &obj) {
  // Default implementation: apply effect properties if matching effects exist
  // Subclasses should override to handle layer-specific fields
  if (!obj.contains("effects") || !obj["effects"].isArray())
    return;
  if (obj.contains("isAdjustment")) {
    setAdjustmentLayer(obj["isAdjustment"].toBool());
  }

  const auto arr = obj.value("effects").toArray();
  for (const auto &ev : arr) {
    if (!ev.isObject())
      continue;
    auto eobj = ev.toObject();
    if (!eobj.contains("id"))
      continue;
    UniString eid(eobj["id"].toString().toStdString());
    auto eff = getEffect(eid);
    if (!eff) {
      const QString effectId = eobj.value(QStringLiteral("id")).toString();
      if (effectId == QStringLiteral("chroma_key") ||
          effectId == QStringLiteral("Effect.Keying.ChromaKey")) {
        eff = makeShared<ChromaKeyEffect>();
      } else if (effectId == QStringLiteral("luma_key") ||
                 effectId == QStringLiteral("Effect.Keying.LumaKey")) {
        eff = makeShared<LumaKeyEffect>();
      } else if (effectId == QStringLiteral("difference_key") ||
                 effectId == QStringLiteral("Effect.Keying.DifferenceKey")) {
        eff = makeShared<DifferenceKeyEffect>();
      } else if (effectId == QStringLiteral("difference_matte") ||
                 effectId == QStringLiteral("Effect.Rasterizer.DifferenceMatte")) {
        eff = makeShared<DifferenceMatteEffect>();
      } else if (effectId == QStringLiteral("posterize_time") ||
                 effectId == QStringLiteral("Effect.Rasterizer.PosterizeTime")) {
        eff = makeShared<PosterizeTimeEffect>();
      } else if (effectId == QStringLiteral("ibk_keyer") ||
                 effectId == QStringLiteral("Effect.Keying.IBKKeyer")) {
        eff = makeShared<IBKKeyerEffect>();
      }
      if (eff) {
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
