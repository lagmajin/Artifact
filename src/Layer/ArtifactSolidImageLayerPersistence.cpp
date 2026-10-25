module;
#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVariant>

module Artifact.Layers.SolidImage;
import Artifact.Layer.Abstract;
import Artifact.Layers.Abstract._2D;
import Color.Float;
import Property.Abstract;
import Property.SerializationBridge;

namespace Artifact {
using namespace ArtifactCore;

QJsonObject ArtifactSolidImageLayer::toJson() const {
  QJsonObject obj = ArtifactAbstract2DLayer::toJson();
  obj["type"] = static_cast<int>(LayerType::Solid);
  const auto safeSource = sourceSize();
  obj["solidWidth"] = std::clamp(safeSource.width, 1, 16384);
    obj["solidHeight"] = std::clamp(safeSource.height, 1, 16384);
    obj["solidPixelAspectRatio"] = pixelAspectRatio();
    if (!sourceItemId().isEmpty()) {
      obj["solidSourceItemId"] = sourceItemId();
    }
  QJsonObject colorObj;
  if (const auto colorProperty = getProperty(QStringLiteral("solid.color"));
      colorProperty && colorProperty->hasKeyFrames()) {
    obj["solidColorKeyframes"] =
        ArtifactCore::PropertySerializationBridge::serializeProperty(colorProperty)
            .keyframes;
  }
  const auto c = color();
  colorObj["r"] = c.r();
  colorObj["g"] = c.g();
  colorObj["b"] = c.b();
  colorObj["a"] = c.a();
  obj["solidColor"] = colorObj;
  obj["solidFillType"] = static_cast<int>(fillType());
  QJsonObject gradientStartObj;
  const auto start = gradientStartColor();
  gradientStartObj["r"] = start.r();
  gradientStartObj["g"] = start.g();
  gradientStartObj["b"] = start.b();
  gradientStartObj["a"] = start.a();
  obj["solidGradientStartColor"] = gradientStartObj;
  QJsonObject gradientEndObj;
  const auto end = gradientEndColor();
  gradientEndObj["r"] = end.r();
  gradientEndObj["g"] = end.g();
  gradientEndObj["b"] = end.b();
  gradientEndObj["a"] = end.a();
  obj["solidGradientEndColor"] = gradientEndObj;
  obj["solidGradientAngleDegrees"] = gradientAngleDegrees();
  obj["solidGradientReverse"] = gradientReverse();
  obj["solidGradientCenterX"] = gradientCenterX();
  obj["solidGradientCenterY"] = gradientCenterY();
  obj["solidGradientScale"] = gradientScale();
  obj["solidGradientOffset"] = gradientOffset();
  return obj;
}

void ArtifactSolidImageLayer::fromJsonProperties(const QJsonObject &obj) {
    ArtifactAbstract2DLayer::fromJsonProperties(obj);
    setPixelAspectRatio(obj.value("solidPixelAspectRatio").toDouble(1.0));
    setSourceItemId(obj.value("solidSourceItemId").toString());
  if (obj.contains("solidWidth") || obj.contains("solidHeight")) {
    const int width = obj.value("solidWidth").toInt(sourceSize().width);
    const int height = obj.value("solidHeight").toInt(sourceSize().height);
    setSize(width, height);
  }
  if (obj.contains("solidColor") && obj["solidColor"].isObject()) {
    const auto colorObj = obj["solidColor"].toObject();
    bool restoredColorKeyframes = false;
    if (obj.contains("solidColorKeyframes") &&
        obj["solidColorKeyframes"].isArray()) {
      auto colorProperty = persistentLayerProperty(
          QStringLiteral("solid.color"), ArtifactCore::PropertyType::Color,
          QColor::fromRgbF(color().r(), color().g(), color().b(), color().a()),
          -120);
      ArtifactCore::SerializedProperty serialized;
      serialized.name = QStringLiteral("solid.color");
      serialized.type = static_cast<int>(ArtifactCore::PropertyType::Color);
      serialized.value = obj["solidColor"];
      serialized.keyframes = obj["solidColorKeyframes"].toArray();
      ArtifactCore::PropertySerializationBridge::deserializeProperty(
          colorProperty, serialized);
      restoredColorKeyframes = !serialized.keyframes.isEmpty();
    }
    if (!restoredColorKeyframes) {
      setColor(
          FloatColor(static_cast<float>(colorObj.value("r").toDouble(1.0)),
                     static_cast<float>(colorObj.value("g").toDouble(1.0)),
                     static_cast<float>(colorObj.value("b").toDouble(1.0)),
                     static_cast<float>(colorObj.value("a").toDouble(1.0))));
    }
  }
  setFillType(static_cast<ArtifactSolidFillType>(
      obj.value("solidFillType").toInt(static_cast<int>(ArtifactSolidFillType::Solid))));
  if (obj.contains("solidGradientStartColor") && obj["solidGradientStartColor"].isObject()) {
    const auto startObj = obj["solidGradientStartColor"].toObject();
    setGradientStartColor(FloatColor(
        static_cast<float>(startObj.value("r").toDouble(1.0)),
        static_cast<float>(startObj.value("g").toDouble(1.0)),
        static_cast<float>(startObj.value("b").toDouble(1.0)),
        static_cast<float>(startObj.value("a").toDouble(1.0))));
  }
  if (obj.contains("solidGradientEndColor") && obj["solidGradientEndColor"].isObject()) {
    const auto endObj = obj["solidGradientEndColor"].toObject();
    setGradientEndColor(FloatColor(
        static_cast<float>(endObj.value("r").toDouble(0.2)),
        static_cast<float>(endObj.value("g").toDouble(0.2)),
        static_cast<float>(endObj.value("b").toDouble(0.2)),
        static_cast<float>(endObj.value("a").toDouble(1.0))));
  }
  if (obj.contains("solidGradientAngleDegrees")) {
    setGradientAngleDegrees(
        static_cast<float>(obj.value("solidGradientAngleDegrees").toDouble(90.0)));
  }
  setGradientReverse(obj.value("solidGradientReverse").toBool(false));
  setGradientCenterX(static_cast<float>(obj.value("solidGradientCenterX").toDouble(0.5)));
  setGradientCenterY(static_cast<float>(obj.value("solidGradientCenterY").toDouble(0.5)));
  setGradientScale(static_cast<float>(obj.value("solidGradientScale").toDouble(1.0)));
  setGradientOffset(static_cast<float>(obj.value("solidGradientOffset").toDouble(0.0)));
}


} // namespace Artifact
