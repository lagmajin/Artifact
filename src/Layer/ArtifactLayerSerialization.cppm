module;
#include <cstdint>
#include <vector>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>

module Artifact.Layer.Serialization;

import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Color.Float;
import Utils.String.UniString;

namespace Artifact {
namespace {

QJsonObject serializeVertex(const MaskVertex& vertex) {
  QJsonObject result;
  result["px"] = vertex.position.x();
  result["py"] = vertex.position.y();
  result["ix"] = vertex.inTangent.x();
  result["iy"] = vertex.inTangent.y();
  result["ox"] = vertex.outTangent.x();
  result["oy"] = vertex.outTangent.y();
  return result;
}

MaskVertex deserializeVertex(const QJsonObject& object) {
  MaskVertex result;
  result.position = QPointF(object["px"].toDouble(), object["py"].toDouble());
  result.inTangent = QPointF(object["ix"].toDouble(), object["iy"].toDouble());
  result.outTangent = QPointF(object["ox"].toDouble(), object["oy"].toDouble());
  return result;
}

QJsonObject serializePath(const MaskPath& path) {
  QJsonObject result;
  QJsonArray vertices;
  for (int index = 0; index < path.vertexCount(); ++index) {
    vertices.append(serializeVertex(path.vertex(index)));
  }
  result["vertices"] = vertices;
  result["closed"] = path.isClosed();
  result["opacity"] = path.opacity();
  result["feather"] = path.feather();
  result["featherHorizontal"] = path.featherHorizontal();
  result["featherVertical"] = path.featherVertical();
  result["featherInner"] = path.featherInner();
  result["featherOuter"] = path.featherOuter();
  result["falloff"] = static_cast<int>(path.falloff());
  result["expansion"] = path.expansion();
  result["inverted"] = path.isInverted();
  result["mode"] = static_cast<int>(path.mode());
  result["name"] = path.name().toQString();
  QJsonArray keyframes;
  for (const auto& keyframe : path.animationKeyframes()) {
    QJsonObject keyframeObject;
    keyframeObject["frame"] = static_cast<qint64>(keyframe.frame);
    keyframeObject["closed"] = keyframe.closed;
    keyframeObject["opacity"] = keyframe.opacity;
    keyframeObject["feather"] = keyframe.feather;
    keyframeObject["featherHorizontal"] = keyframe.featherHorizontal;
    keyframeObject["featherVertical"] = keyframe.featherVertical;
    keyframeObject["featherInner"] = keyframe.featherInner;
    keyframeObject["featherOuter"] = keyframe.featherOuter;
    keyframeObject["falloff"] = static_cast<int>(keyframe.falloff);
    keyframeObject["expansion"] = keyframe.expansion;
    keyframeObject["inverted"] = keyframe.inverted;
    keyframeObject["mode"] = static_cast<int>(keyframe.mode);
    keyframeObject["name"] = keyframe.name.toQString();
    QJsonArray keyframeVertices;
    for (const auto& vertex : keyframe.vertices) {
      keyframeVertices.append(serializeVertex(vertex));
    }
    keyframeObject["vertices"] = keyframeVertices;
    keyframes.append(keyframeObject);
  }
  if (!keyframes.isEmpty()) result["animationKeyframes"] = keyframes;
  return result;
}

MaskPath deserializePath(const QJsonObject& object) {
  MaskPath result;
  result.clearVertices();
  for (const auto& value : object["vertices"].toArray()) {
    if (value.isObject()) result.addVertex(deserializeVertex(value.toObject()));
  }
  result.setClosed(object["closed"].toBool(true));
  result.setOpacity(static_cast<float>(object["opacity"].toDouble(1.0)));
  result.setFeather(static_cast<float>(object["feather"].toDouble()));
  result.setFeatherHorizontal(static_cast<float>(object["featherHorizontal"].toDouble()));
  result.setFeatherVertical(static_cast<float>(object["featherVertical"].toDouble()));
  result.setFeatherInner(static_cast<float>(object["featherInner"].toDouble()));
  result.setFeatherOuter(static_cast<float>(object["featherOuter"].toDouble()));
  result.setFalloff(static_cast<MaskFeatherFalloff>(object["falloff"].toInt()));
  result.setExpansion(static_cast<float>(object["expansion"].toDouble()));
  result.setInverted(object["inverted"].toBool());
  result.setMode(static_cast<MaskMode>(object["mode"].toInt()));
  result.setName(UniString::fromQString(object["name"].toString()));
  for (const auto& value : object["animationKeyframes"].toArray()) {
    if (!value.isObject()) continue;
    const QJsonObject keyframeObject = value.toObject();
    MaskPathKeyframeSnapshot keyframe;
    keyframe.frame = static_cast<int64_t>(keyframeObject["frame"].toVariant().toLongLong());
    keyframe.closed = keyframeObject["closed"].toBool(true);
    keyframe.opacity = static_cast<float>(keyframeObject["opacity"].toDouble(1.0));
    keyframe.feather = static_cast<float>(keyframeObject["feather"].toDouble());
    keyframe.featherHorizontal = static_cast<float>(keyframeObject["featherHorizontal"].toDouble());
    keyframe.featherVertical = static_cast<float>(keyframeObject["featherVertical"].toDouble());
    keyframe.featherInner = static_cast<float>(keyframeObject["featherInner"].toDouble());
    keyframe.featherOuter = static_cast<float>(keyframeObject["featherOuter"].toDouble());
    keyframe.falloff = static_cast<MaskFeatherFalloff>(keyframeObject["falloff"].toInt());
    keyframe.expansion = static_cast<float>(keyframeObject["expansion"].toDouble());
    keyframe.inverted = keyframeObject["inverted"].toBool();
    keyframe.mode = static_cast<MaskMode>(keyframeObject["mode"].toInt());
    keyframe.name = UniString::fromQString(keyframeObject["name"].toString());
    for (const auto& vertexValue : keyframeObject["vertices"].toArray()) {
      if (vertexValue.isObject()) keyframe.vertices.push_back(deserializeVertex(vertexValue.toObject()));
    }
    result.setAnimationKeyframe(keyframe.frame, keyframe);
  }
  return result;
}

} // namespace

QJsonArray serializeLayerMasks(const std::vector<LayerMask>& masks) {
  QJsonArray result;
  for (const auto& mask : masks) {
    QJsonObject object;
    object["enabled"] = mask.isEnabled();
    object["locked"] = mask.isLocked();
    const auto color = mask.color();
    object["color"] = QJsonObject{{"r", color.r()}, {"g", color.g()},
                                  {"b", color.b()}, {"a", color.a()}};
    QJsonArray paths;
    for (int index = 0; index < mask.maskPathCount(); ++index) {
      paths.append(serializePath(mask.maskPath(index)));
    }
    object["paths"] = paths;
    result.append(object);
  }
  return result;
}

std::vector<LayerMask> deserializeLayerMasks(const QJsonArray& masks) {
  std::vector<LayerMask> result;
  result.reserve(static_cast<std::size_t>(masks.size()));
  for (const auto& value : masks) {
    if (!value.isObject()) continue;
    const QJsonObject object = value.toObject();
    LayerMask mask;
    mask.setEnabled(object["enabled"].toBool(true));
    mask.setLocked(object["locked"].toBool(false));
    const QJsonObject color = object["color"].toObject();
    if (!color.isEmpty()) {
      mask.setColor(FloatColor(static_cast<float>(color["r"].toDouble(0.28)),
                               static_cast<float>(color["g"].toDouble(0.88)),
                               static_cast<float>(color["b"].toDouble(1.0)),
                               static_cast<float>(color["a"].toDouble(0.95))));
    }
    for (const auto& pathValue : object["paths"].toArray()) {
      if (pathValue.isObject()) mask.addMaskPath(deserializePath(pathValue.toObject()));
    }
    result.push_back(std::move(mask));
  }
  return result;
}

} // namespace Artifact
