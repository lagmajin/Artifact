#include <algorithm>
#include <cmath>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

import Artifact.Layer.Abstract;
import Animation.Transform3D;
import Frame.Rate;
import Property.Abstract;
import Property.SerializationBridge;
import Time.Rational;

namespace Artifact {

using namespace ArtifactCore;

QJsonObject serializeLayerTransform(const AnimatableTransform3D& transform) {
  QJsonObject object;
  object["px"] = transform.positionX();
  object["py"] = transform.positionY();
  object["pz"] = transform.positionZ();
  // Keep rx as the legacy single-angle (Z) field and persist the full model.
  object["rx"] = transform.rotationZ();
  object["rotationX"] = transform.rotationX();
  object["rotationY"] = transform.rotationY();
  object["rotationZ"] = transform.rotationZ();
  object["sx"] = transform.scaleX();
  object["sy"] = transform.scaleY();
  object["ax"] = transform.anchorX();
  object["ay"] = transform.anchorY();
  object["az"] = transform.anchorZ();
  object["autoOrientMode"] = static_cast<int>(transform.autoOrientMode());
  object["channelSchema"] = 1;
  object["initialRotation"] = transform.initialRotation();

  QJsonObject channels;
  for (int index = 0;
       index <= static_cast<int>(TransformChannel::AnchorZ); ++index) {
    const auto property = transform.channelProperty(
        static_cast<TransformChannel>(index));
    const auto serialized = PropertySerializationBridge::serializeProperty(property);
    QJsonObject entry;
    entry["value"] = serialized.value;
    entry["keyframes"] = serialized.keyframes;
    entry["expression"] = serialized.expression;
    entry["envelopes"] = serialized.envelopes;
    entry["metadata"] = serialized.metadata;
    channels[property->getName()] = entry;
  }
  object["channels"] = channels;

  QJsonArray spatialTangents;
  for (const auto& time : transform.getPositionKeyFrameTimes()) {
    PositionSpatialTangents tangent;
    if (!transform.positionKeyFrameSpatialTangentsAt(time, tangent)) continue;
    QJsonObject entry;
    entry["timeValue"] = time.value();
    entry["timeScale"] = time.scale();
    entry["inX"] = tangent.inTangent.x;
    entry["inY"] = tangent.inTangent.y;
    entry["outX"] = tangent.outTangent.x;
    entry["outY"] = tangent.outTangent.y;
    entry["linked"] = tangent.linked;
    spatialTangents.append(entry);
  }
  object["spatialTangents"] = spatialTangents;
  return object;
}

void restoreLayerTransform(const QJsonObject& object,
                           AnimatableTransform3D& transform,
                           double frameRate) {
  const auto finiteTransformValue = [](double value, double fallback) {
    return std::isfinite(value) ? value : fallback;
  };
  transform = AnimatableTransform3D{};
  transform.setKeyframeTimeScale(FrameRate::storageScaleForFps(frameRate));

  if (!object.value("channels").isObject()) {
    const auto restoreBase = [&](TransformChannel channel, const char* name,
                                 double fallback) {
      transform.channelProperty(channel)->setValue(finiteTransformValue(
          object.value(QLatin1String(name)).toDouble(fallback), fallback));
    };
    restoreBase(TransformChannel::PositionX, "px", 0.0);
    restoreBase(TransformChannel::PositionY, "py", 0.0);
    restoreBase(TransformChannel::PositionZ, "pz", 0.0);
    restoreBase(TransformChannel::RotationX, "rotationX", 0.0);
    restoreBase(TransformChannel::RotationY, "rotationY", 0.0);
    restoreBase(TransformChannel::Rotation,
                object.contains("rotationZ") ? "rotationZ" : "rx", 0.0);
    restoreBase(TransformChannel::ScaleX, "sx", 1.0);
    restoreBase(TransformChannel::ScaleY, "sy", 1.0);
    restoreBase(TransformChannel::AnchorX, "ax", 0.0);
    restoreBase(TransformChannel::AnchorY, "ay", 0.0);
    restoreBase(TransformChannel::AnchorZ, "az", 0.0);
    if (object.contains("autoOrientMode")) {
      const int mode = std::clamp(
          object["autoOrientMode"].toInt(),
          static_cast<int>(AutoOrientMode::Off),
          static_cast<int>(AutoOrientMode::AlongPathAtFrameStart));
      transform.setAutoOrientMode(static_cast<AutoOrientMode>(mode));
    }
    if (object.contains("rotationKeyframes") &&
        object["rotationKeyframes"].isArray()) {
      transform.clearRotationKeyFrames();
      for (const auto& value : object["rotationKeyframes"].toArray()) {
        if (!value.isObject()) continue;
        const QJsonObject keyframe = value.toObject();
        const RationalTime time(keyframe["frame"].toInteger(), 24);
        const bool hasAxes = keyframe.contains("x") || keyframe.contains("y") ||
                             keyframe.contains("z");
        if (hasAxes) {
          transform.setRotationX(time, static_cast<float>(finiteTransformValue(
              keyframe["x"].toDouble(0.0), 0.0)));
          transform.setRotationY(time, static_cast<float>(finiteTransformValue(
              keyframe["y"].toDouble(0.0), 0.0)));
          transform.setRotationZ(time, static_cast<float>(finiteTransformValue(
              keyframe["z"].toDouble(0.0), 0.0)));
        } else {
          transform.setRotationZ(time, static_cast<float>(finiteTransformValue(
              keyframe["value"].toDouble(), 0.0)));
        }
      }
    }
    if (object.contains("scaleKeyframes") && object["scaleKeyframes"].isArray()) {
      transform.clearScaleKeyFrames();
      for (const auto& value : object["scaleKeyframes"].toArray()) {
        if (!value.isObject()) continue;
        const QJsonObject keyframe = value.toObject();
        const RationalTime time(keyframe["frame"].toInteger(), 24);
        transform.setScale(time, static_cast<float>(finiteTransformValue(
                               keyframe["x"].toDouble(1.0), 1.0)),
                           static_cast<float>(finiteTransformValue(
                               keyframe["y"].toDouble(1.0), 1.0)));
      }
    }
    if (object.contains("positionKeyframes") &&
        object["positionKeyframes"].isArray()) {
      transform.clearPositionKeyFrames();
      for (const auto& value : object["positionKeyframes"].toArray()) {
        if (!value.isObject()) continue;
        const QJsonObject keyframe = value.toObject();
        const RationalTime time(keyframe["frame"].toInteger(), 24);
        transform.setPositionKeyFrameValueAt(
            time, static_cast<float>(finiteTransformValue(
                      keyframe["x"].toDouble(), 0.0)),
            static_cast<float>(finiteTransformValue(keyframe["y"].toDouble(), 0.0)));
        transform.setPositionKeyFrameInterpolationAt(
            time, static_cast<InterpolationType>(std::clamp(
                      keyframe["xInterpolation"].toInt(
                          static_cast<int>(InterpolationType::Linear)),
                      0, 32)),
            static_cast<InterpolationType>(std::clamp(
                      keyframe["yInterpolation"].toInt(
                          static_cast<int>(InterpolationType::Linear)),
                      0, 32)));
        if (keyframe.contains("inTangentX") || keyframe.contains("outTangentX")) {
          PositionSpatialTangents tangents;
          tangents.inTangent.x = static_cast<float>(finiteTransformValue(
              keyframe["inTangentX"].toDouble(), 0.0));
          tangents.inTangent.y = static_cast<float>(finiteTransformValue(
              keyframe["inTangentY"].toDouble(), 0.0));
          tangents.outTangent.x = static_cast<float>(finiteTransformValue(
              keyframe["outTangentX"].toDouble(), 0.0));
          tangents.outTangent.y = static_cast<float>(finiteTransformValue(
              keyframe["outTangentY"].toDouble(), 0.0));
          tangents.linked = keyframe["tangentsLinked"].toBool(true);
          transform.setPositionKeyFrameSpatialTangentsAt(time, tangents);
        }
      }
    }
    return;
  }

  transform.setInitialRotation(RationalTime(0, 1), finiteTransformValue(
      object.value("initialRotation").toDouble(), 0.0));
  const auto channels = object.value("channels").toObject();
  for (int index = 0;
       index <= static_cast<int>(TransformChannel::AnchorZ); ++index) {
    auto property = transform.channelProperty(static_cast<TransformChannel>(index));
    const auto entryValue = channels.value(property->getName());
    if (!entryValue.isObject()) continue;
    const auto entry = entryValue.toObject();
    SerializedProperty serialized;
    serialized.name = property->getName();
    serialized.type = static_cast<int>(PropertyType::Float);
    serialized.value = entry.value("value");
    serialized.keyframes = entry.value("keyframes").toArray();
    serialized.expression = entry.value("expression").toString();
    serialized.envelopes = entry.value("envelopes").toArray();
    serialized.metadata = entry.value("metadata").toObject();
    PropertySerializationBridge::deserializeProperty(property, serialized);
  }
  transform.setAutoOrientMode(static_cast<AutoOrientMode>(std::clamp(
      object.value("autoOrientMode").toInt(), 0, 2)));
  for (const auto& value : object.value("spatialTangents").toArray()) {
    const auto entry = value.toObject();
    const auto scale = entry.value("timeScale").toInteger();
    if (scale <= 0) continue;
    const RationalTime time(entry.value("timeValue").toInteger(), scale);
    PositionSpatialTangents tangent;
    tangent.inTangent.x = finiteTransformValue(entry.value("inX").toDouble(), 0.0);
    tangent.inTangent.y = finiteTransformValue(entry.value("inY").toDouble(), 0.0);
    tangent.outTangent.x = finiteTransformValue(entry.value("outX").toDouble(), 0.0);
    tangent.outTangent.y = finiteTransformValue(entry.value("outY").toDouble(), 0.0);
    tangent.linked = entry.value("linked").toBool(true);
    transform.setPositionKeyFrameSpatialTangentsAt(time, tangent);
  }
}

} // namespace Artifact
