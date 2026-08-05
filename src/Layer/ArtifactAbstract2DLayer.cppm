module;
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QString>
#include <QVector2D>
#include <QJsonObject>
#include <QtGlobal>
module Artifact.Layers.Abstract._2D;




import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Animation.Transform2D;
import ArtifactCore.Rig2D;
import Memory.SharedPtr;
import Time.Rational;
import Utils.Id;


namespace Artifact {

namespace {

ArtifactCore::RationalTime rigTimeForLayer(const ArtifactAbstractLayer* layer)
{
 if (!layer) {
  return ArtifactCore::RationalTime(0, 30);
 }
 int64_t fps = 30;
 if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {
  const double compFps = comp->frameRate().framerate();
  if (compFps > 0.0) {
   fps = static_cast<int64_t>(std::llround(compFps));
   if (fps <= 0) {
    fps = 30;
   }
  }
 }
 return ArtifactCore::RationalTime(layer->currentFrame(), fps);
}

void applyRigPropertyBindings(ArtifactAbstract2DLayer* layer)
{
 if (!layer) {
  return;
 }
 auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition());
 if (!comp) {
  return;
 }

 auto& rig = layer->rig2D();
 for (const auto& binding : rig.propertyBindings()) {
  if (!binding || !binding->enabled()) {
   continue;
  }

  auto control = rig.findControl(binding->controlId());
  if (!control || !control->enabled()) {
   continue;
  }

  auto targetLayer = comp->layerById(binding->targetLayerId());
  if (!targetLayer) {
   continue;
  }

  targetLayer->setLayerPropertyValue(binding->targetPropertyPath(), control->value());
 }
}

} // namespace

class ArtifactAbstract2DLayer::Impl {
 private:
  ArtifactCore::Rig2D rig2D_;

 public:
  Impl();
  ~Impl();
  ArtifactCore::Rig2D& rig2D() { return rig2D_; }
  const ArtifactCore::Rig2D& rig2D() const { return rig2D_; }
 };

 ArtifactAbstract2DLayer::Impl::Impl()
 {

 }

 ArtifactAbstract2DLayer::Impl::~Impl()
 {

 }

 ArtifactAbstract2DLayer::ArtifactAbstract2DLayer() :impl_(new Impl())
 {

 }

 ArtifactAbstract2DLayer::~ArtifactAbstract2DLayer()
 {
  delete impl_;
 }

 void ArtifactAbstract2DLayer::goToFrame(int64_t frameNumber)
 {
  ArtifactAbstractLayer::goToFrame(frameNumber);
  impl_->rig2D().evaluate(rigTimeForLayer(this));
  applyRigPropertyBindings(this);
 }

 ArtifactCore::Rig2D& ArtifactAbstract2DLayer::rig2D()
 {
  return impl_->rig2D();
 }

 const ArtifactCore::Rig2D& ArtifactAbstract2DLayer::rig2D() const
 {
  return impl_->rig2D();
 }

 ArtifactCore::Bone2D* ArtifactAbstract2DLayer::addRigBone(const QString& name,
                                                          const QString& parentName)
 {
  ArtifactCore::Bone2D* parent = nullptr;
  if (!parentName.isEmpty()) {
   parent = impl_->rig2D().findBone(parentName);
  }
  return impl_->rig2D().addBone(name, parent);
 }

 ArtifactCore::Bone2D* ArtifactAbstract2DLayer::addRigBone(const QString& name,
                                                           const ArtifactCore::Id& parentId)
 {
  return impl_->rig2D().addBone(name, parentId);
 }

 bool ArtifactAbstract2DLayer::removeRigBone(const ArtifactCore::Id& boneId)
 {
  return impl_->rig2D().removeBone(boneId);
 }

 bool ArtifactAbstract2DLayer::setRigBoneLocalTransform(
     const ArtifactCore::Id& boneId,
     const ArtifactCore::BoneTransform& transform)
 {
  return impl_->rig2D().setBoneLocalTransform(boneId, transform);
 }

 ArtifactCore::RigControl2D* ArtifactAbstract2DLayer::addRigSlider(const QString& name,
                                                                   double defaultValue,
                                                                   double minValue,
                                                                   double maxValue)
 {
  return impl_->rig2D().addSlider(name, defaultValue, minValue, maxValue);
 }

 ArtifactCore::RigControl2D* ArtifactAbstract2DLayer::addRigPoint(const QString& name,
                                                                  const QVector2D& defaultValue)
 {
  return impl_->rig2D().addPoint(name, defaultValue);
 }

 ArtifactCore::RigControl2D* ArtifactAbstract2DLayer::addRigAngle(const QString& name,
                                                                   double defaultValue,
                                                                   double minValue,
                                                                   double maxValue)
 {
  return impl_->rig2D().addAngle(name, defaultValue, minValue, maxValue);
 }

 bool ArtifactAbstract2DLayer::setRigControlValue(const ArtifactCore::Id& controlId,
                                                  const QVariant& value)
 {
  ArtifactCore::RigControl2D* control = impl_->rig2D().findControl(controlId);
  if (!control) return false;
  control->setValue(value);
  impl_->rig2D().evaluate(rigTimeForLayer(this));
  applyRigPropertyBindings(this);
  setDirty(LayerDirtyFlag::Property);
  return true;
 }

 QVariant ArtifactAbstract2DLayer::rigControlValue(const ArtifactCore::Id& controlId) const
 {
  const ArtifactCore::RigControl2D* control = impl_->rig2D().findControl(controlId);
  return control ? control->value() : QVariant();
 }

 ArtifactCore::SharedPtr<ArtifactCore::ParentConstraint2D> ArtifactAbstract2DLayer::addRigParentConstraint(
     const QString& name,
     const ArtifactCore::Id& targetBoneId,
     const ArtifactCore::Id& parentBoneId)
 {
  auto constraint = ArtifactCore::makeShared<ArtifactCore::ParentConstraint2D>(name, targetBoneId, parentBoneId);
  return ArtifactCore::staticPointerCast<ArtifactCore::ParentConstraint2D>(impl_->rig2D().addConstraint(constraint));
 }

 ArtifactCore::SharedPtr<ArtifactCore::MapRangeConstraint2D> ArtifactAbstract2DLayer::addRigMapRangeConstraint(
     const QString& name,
     const ArtifactCore::Id& controlId,
     const ArtifactCore::Id& targetBoneId)
 {
  auto constraint = ArtifactCore::makeShared<ArtifactCore::MapRangeConstraint2D>(name, controlId, targetBoneId);
  return ArtifactCore::staticPointerCast<ArtifactCore::MapRangeConstraint2D>(impl_->rig2D().addConstraint(constraint));
 }

 ArtifactCore::SharedPtr<ArtifactCore::AimConstraint2D> ArtifactAbstract2DLayer::addRigAimConstraint(
     const QString& name,
     const ArtifactCore::Id& sourceBoneId,
     const ArtifactCore::Id& targetBoneId)
 {
  auto constraint = ArtifactCore::makeShared<ArtifactCore::AimConstraint2D>(name, sourceBoneId, targetBoneId);
  return ArtifactCore::staticPointerCast<ArtifactCore::AimConstraint2D>(impl_->rig2D().addConstraint(constraint));
 }

 ArtifactCore::SharedPtr<ArtifactCore::TwoBoneIKConstraint2D> ArtifactAbstract2DLayer::addRigTwoBoneIKConstraint(
     const QString& name,
     const ArtifactCore::Id& upperBoneId,
     const ArtifactCore::Id& lowerBoneId,
     const ArtifactCore::Id& effectorBoneId,
     const ArtifactCore::Id& targetBoneId)
 {
  auto constraint = ArtifactCore::makeShared<ArtifactCore::TwoBoneIKConstraint2D>(
      name, upperBoneId, lowerBoneId, effectorBoneId, targetBoneId);
  return ArtifactCore::staticPointerCast<ArtifactCore::TwoBoneIKConstraint2D>(impl_->rig2D().addConstraint(constraint));
 }

 ArtifactCore::SharedPtr<ArtifactCore::RigPropertyBinding2D> ArtifactAbstract2DLayer::addRigPropertyBinding(
     const QString& name,
     const ArtifactCore::Id& controlId,
     const QString& targetPropertyPath)
 {
  auto binding = ArtifactCore::makeShared<ArtifactCore::RigPropertyBinding2D>(
      name, controlId, ArtifactCore::LayerID(id()), targetPropertyPath);
  return ArtifactCore::staticPointerCast<ArtifactCore::RigPropertyBinding2D>(impl_->rig2D().addPropertyBinding(binding));
 }

 void ArtifactAbstract2DLayer::clearRigBones()
 {
  impl_->rig2D().clearBones();
 }

 int ArtifactAbstract2DLayer::rigBoneCount() const
 {
  return static_cast<int>(impl_->rig2D().bones().size());
 }

 QString ArtifactAbstract2DLayer::rigRootBoneName() const
 {
  const auto* rootBone = impl_->rig2D().rootBone();
  return rootBone ? rootBone->name() : QString();
 }

 QJsonObject ArtifactAbstract2DLayer::toJson() const
 {
  QJsonObject obj = ArtifactAbstractLayer::toJson();
  if (!impl_->rig2D().bones().isEmpty() ||
      !impl_->rig2D().controls().isEmpty() ||
      !impl_->rig2D().constraints().isEmpty()) {
   obj["rig2D"] = impl_->rig2D().toJson();
  }
  return obj;
 }

 void ArtifactAbstract2DLayer::fromJsonProperties(const QJsonObject& obj)
 {
  ArtifactAbstractLayer::fromJsonProperties(obj);
  if (obj.contains("rig2D") && obj.value("rig2D").isObject()) {
   QJsonObject rigObject = obj.value("rig2D").toObject();
   const auto capArray = [&rigObject](const QString& key, const qsizetype maximum) {
    const QJsonArray source = rigObject.value(key).toArray();
    if (source.size() <= maximum) {
     return;
    }
    QJsonArray capped;
    capped.reserve(static_cast<int>(maximum));
    for (qsizetype i = 0; i < maximum; ++i) {
     capped.append(source.at(i));
    }
    rigObject.insert(key, capped);
   };
   capArray(QStringLiteral("bones"), 4096);
   capArray(QStringLiteral("controls"), 1024);
   capArray(QStringLiteral("constraints"), 4096);
   capArray(QStringLiteral("propertyBindings"), 4096);
   capArray(QStringLiteral("smartBones"), 1024);
   if (rigObject.value(QStringLiteral("skinMesh")).isObject()) {
    const QJsonObject sourceMesh = rigObject.value(QStringLiteral("skinMesh")).toObject();
    const int restoredBoneCount = rigObject.value(QStringLiteral("bones")).toArray().size();
    QJsonObject safeMesh;
    QJsonArray safeVertices;
    const QJsonArray vertices = sourceMesh.value(QStringLiteral("vertices")).toArray();
    for (qsizetype i = 0; i < vertices.size() && safeVertices.size() < 1000000; ++i) {
     if (!vertices.at(i).isObject()) continue;
     const QJsonObject vertex = vertices.at(i).toObject();
     const QJsonArray weights = vertex.value(QStringLiteral("w")).toArray();
     const QJsonArray boneIndices = vertex.value(QStringLiteral("bi")).toArray();
     if (weights.size() < 4 || boneIndices.size() < 4 ||
         !std::isfinite(vertex.value(QStringLiteral("px")).toDouble()) ||
         !std::isfinite(vertex.value(QStringLiteral("py")).toDouble()) ||
         !std::isfinite(vertex.value(QStringLiteral("u")).toDouble()) ||
         !std::isfinite(vertex.value(QStringLiteral("v")).toDouble())) {
      continue;
     }
     bool validWeights = true;
     QJsonArray safeWeights;
     std::array<double, 4> normalizedWeights{};
     double weightTotal = 0.0;
     bool hasValidBone = false;
     QJsonArray safeBoneIndices;
     for (int component = 0; component < 4; ++component) {
      const double weight = weights.at(component).toDouble();
      if (!std::isfinite(weight)) {
       validWeights = false;
       break;
      }
      const int boneIndex = boneIndices.at(component).toInt(-1);
      const bool validBone = boneIndex >= 0 && boneIndex < restoredBoneCount;
      hasValidBone = hasValidBone || validBone;
      normalizedWeights[component] = validBone
          ? std::clamp(weight, 0.0, 1.0) : 0.0;
      weightTotal += normalizedWeights[component];
      safeBoneIndices.append(
          validBone ? boneIndex : -1);
     }
     if (!validWeights) continue;
     if (weightTotal > 0.0 && std::isfinite(weightTotal)) {
      for (const double weight : normalizedWeights) {
       safeWeights.append(weight / weightTotal);
      }
     } else if (hasValidBone) {
      safeWeights.append(1.0);
      safeWeights.append(0.0);
      safeWeights.append(0.0);
      safeWeights.append(0.0);
     } else {
      for (int component = 0; component < 4; ++component) {
       safeWeights.append(0.0);
      }
     }
     QJsonObject safeVertex = vertex;
     safeVertex.insert(QStringLiteral("px"), std::clamp(vertex.value(QStringLiteral("px")).toDouble(), -1000000.0, 1000000.0));
     safeVertex.insert(QStringLiteral("py"), std::clamp(vertex.value(QStringLiteral("py")).toDouble(), -1000000.0, 1000000.0));
     safeVertex.insert(QStringLiteral("u"), std::clamp(vertex.value(QStringLiteral("u")).toDouble(), -100000.0, 100000.0));
     safeVertex.insert(QStringLiteral("v"), std::clamp(vertex.value(QStringLiteral("v")).toDouble(), -100000.0, 100000.0));
     safeVertex.insert(QStringLiteral("w"), safeWeights);
     safeVertex.insert(QStringLiteral("bi"), safeBoneIndices);
     safeVertices.append(safeVertex);
    }
    safeMesh.insert(QStringLiteral("vertices"), safeVertices);
    QJsonArray safeTriangles;
    const QJsonArray triangles = sourceMesh.value(QStringLiteral("triangles")).toArray();
    const qint64 safeVertexCount = safeVertices.size();
    for (qsizetype i = 0; i < triangles.size() && safeTriangles.size() < 3000000; ++i) {
     const qint64 triangle = triangles.at(i).toInteger(-1);
     if (triangle >= 0 && triangle < safeVertexCount) {
      safeTriangles.append(triangle);
     }
    }
    while (safeTriangles.size() % 3 != 0) {
     safeTriangles.removeLast();
    }
    safeMesh.insert(QStringLiteral("triangles"), safeTriangles);
    rigObject.insert(QStringLiteral("skinMesh"), safeMesh);
   }
   impl_->rig2D() = ArtifactCore::Rig2D::fromJson(rigObject);
  }
 }

 std::vector<ArtifactCore::PropertyGroup> ArtifactAbstract2DLayer::getLayerPropertyGroups() const
 {
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();

  ArtifactCore::PropertyGroup rigGroup(QStringLiteral("Rig"));
  ArtifactCore::PropertyGroup rigBoneGroup(QStringLiteral("Rig Bones"));
  ArtifactCore::PropertyGroup rigControlGroup(QStringLiteral("Rig Controls"));
  auto makeProp = [this](const QString& name, ArtifactCore::PropertyType type,
                         const QVariant& value, int priority = 0) {
   return persistentLayerProperty(name, type, value, priority);
  };
  auto makeRigControlProp = [this](const QString& path,
                                   const QString& displayLabel,
                                   ArtifactCore::PropertyType type,
                                   const QVariant& value,
                                   int priority,
                                   const QVariant& minValue = QVariant(),
                                   const QVariant& maxValue = QVariant()) {
   auto property = persistentLayerProperty(path, type, value, priority);
   property->setDisplayLabel(displayLabel);
   if (minValue.isValid()) {
    property->setMinValue(minValue);
   }
   if (maxValue.isValid()) {
    property->setMaxValue(maxValue);
   }
   return property;
  };

  rigGroup.addProperty(makeProp(QStringLiteral("rig.boneCount"),
                                ArtifactCore::PropertyType::Integer,
                                static_cast<qint64>(rigBoneCount()),
                                -55));

  rigGroup.addProperty(makeProp(QStringLiteral("rig.controlCount"),
                                ArtifactCore::PropertyType::Integer,
                                static_cast<qint64>(rig2D().controlCount()),
                                -54));

  rigGroup.addProperty(makeProp(QStringLiteral("rig.rootBone"),
                                ArtifactCore::PropertyType::String,
                                rigRootBoneName(),
                                -53));

  rigGroup.addProperty(makeProp(QStringLiteral("rig.constraintCount"),
                                ArtifactCore::PropertyType::Integer,
                                static_cast<qint64>(rig2D().constraintCount()),
                                -52));

  rigGroup.addProperty(makeProp(QStringLiteral("rig.bindingCount"),
                                ArtifactCore::PropertyType::Integer,
                                static_cast<qint64>(rig2D().propertyBindingCount()),
                                -51));

  for (const auto* bone : rig2D().bones()) {
   if (!bone) {
    continue;
   }
   const auto& transform = bone->localTransform();
   const QString prefix = QStringLiteral("rig.bone.%1").arg(bone->id().toString());
   auto addBoneProperty = [&](const QString& suffix, const QString& label,
                              double value, const QString& unit, int priority) {
    auto property = makeProp(prefix + suffix, ArtifactCore::PropertyType::Float,
                             value, priority);
    property->setDisplayLabel(bone->name() + QStringLiteral(" ") + label);
    if (!unit.isEmpty()) {
     property->setUnit(unit);
    }
    rigBoneGroup.addProperty(property);
   };
   addBoneProperty(QStringLiteral(".positionX"), QStringLiteral("Position X"),
                   transform.position.x(), QStringLiteral("px"), -51);
   addBoneProperty(QStringLiteral(".positionY"), QStringLiteral("Position Y"),
                   transform.position.y(), QStringLiteral("px"), -50);
   addBoneProperty(QStringLiteral(".rotation"), QStringLiteral("Rotation"),
                   transform.rotation, QStringLiteral("deg"), -49);
   addBoneProperty(QStringLiteral(".scaleX"), QStringLiteral("Scale X"),
                   transform.scale.x(), QStringLiteral("factor"), -48);
   addBoneProperty(QStringLiteral(".scaleY"), QStringLiteral("Scale Y"),
                   transform.scale.y(), QStringLiteral("factor"), -47);
  }

  for (const auto* control : rig2D().controls()) {
   if (!control) {
    continue;
   }

   const QString controlId = control->id().toString();
   const QString controlPrefix = QStringLiteral("rig.control.%1").arg(controlId);
   switch (control->kind()) {
   case ArtifactCore::RigControlKind::Point: {
    const QVector2D pointValue = control->value().value<QVector2D>();
    auto xProp = makeRigControlProp(controlPrefix + QStringLiteral(".x"),
                                    control->name() + QStringLiteral(" X"),
                                    ArtifactCore::PropertyType::Float,
                                    static_cast<double>(pointValue.x()),
                                    -51,
                                    control->minValue().canConvert<QVector2D>()
                                        ? QVariant::fromValue(control->minValue().value<QVector2D>().x())
                                        : QVariant(),
                                    control->maxValue().canConvert<QVector2D>()
                                        ? QVariant::fromValue(control->maxValue().value<QVector2D>().x())
                                        : QVariant());
    xProp->setUnit(QStringLiteral("px"));
    rigControlGroup.addProperty(xProp);

    auto yProp = makeRigControlProp(controlPrefix + QStringLiteral(".y"),
                                    control->name() + QStringLiteral(" Y"),
                                    ArtifactCore::PropertyType::Float,
                                    static_cast<double>(pointValue.y()),
                                    -50,
                                    control->minValue().canConvert<QVector2D>()
                                        ? QVariant::fromValue(control->minValue().value<QVector2D>().y())
                                        : QVariant(),
                                    control->maxValue().canConvert<QVector2D>()
                                        ? QVariant::fromValue(control->maxValue().value<QVector2D>().y())
                                        : QVariant());
    yProp->setUnit(QStringLiteral("px"));
    rigControlGroup.addProperty(yProp);
    break;
   }
   case ArtifactCore::RigControlKind::Angle:
   case ArtifactCore::RigControlKind::Slider:
   default: {
    const double scalarValue = control->value().toDouble();
    auto prop = makeRigControlProp(controlPrefix,
                                   control->name(),
                                   ArtifactCore::PropertyType::Float,
                                   scalarValue,
                                   -51,
                                   control->minValue(),
                                   control->maxValue());
    if (control->kind() == ArtifactCore::RigControlKind::Angle) {
     prop->setUnit(QStringLiteral("deg"));
    }
    rigControlGroup.addProperty(prop);
    break;
   }
   }
  }

  groups.push_back(rigGroup);
  if (rigBoneGroup.propertyCount() > 0) {
   groups.push_back(rigBoneGroup);
  }
  if (rigControlGroup.propertyCount() > 0) {
   groups.push_back(rigControlGroup);
  }
  return groups;
 }

 bool ArtifactAbstract2DLayer::setLayerPropertyValue(const QString& propertyPath,
                                                     const QVariant& value)
 {
  if (propertyPath.startsWith(QStringLiteral("rig."))) {
   if (propertyPath.startsWith(QStringLiteral("rig.bone."))) {
    const QString bonePath = propertyPath.mid(QStringLiteral("rig.bone.").size());
    const int separatorIndex = bonePath.indexOf(QLatin1Char('.'));
    if (separatorIndex <= 0) {
     return false;
    }
    const ArtifactCore::Id boneId(bonePath.left(separatorIndex));
    const QString channel = bonePath.mid(separatorIndex + 1);
    ArtifactCore::BoneTransform transform;
    if (!rig2D().boneLocalTransform(boneId, &transform)) {
     return false;
    }
    const float numericValue = static_cast<float>(value.toDouble());
    if (!std::isfinite(numericValue)) {
     return false;
    }
    if (channel == QStringLiteral("positionX")) {
     transform.position.setX(numericValue);
    } else if (channel == QStringLiteral("positionY")) {
     transform.position.setY(numericValue);
    } else if (channel == QStringLiteral("rotation")) {
     transform.rotation = numericValue;
    } else if (channel == QStringLiteral("scaleX")) {
     transform.scale.setX(numericValue);
    } else if (channel == QStringLiteral("scaleY")) {
     transform.scale.setY(numericValue);
    } else {
     return false;
    }
    if (!setRigBoneLocalTransform(boneId, transform)) {
     return false;
    }
    rig2D().evaluate(rigTimeForLayer(this));
    applyRigPropertyBindings(this);
    setDirty(LayerDirtyFlag::Property);
    return true;
   }
   if (propertyPath.startsWith(QStringLiteral("rig.control."))) {
    const QString controlPath = propertyPath.mid(QStringLiteral("rig.control.").size());
    const int separatorIndex = controlPath.indexOf(QLatin1Char('.'));
    const QString controlIdString = separatorIndex >= 0 ? controlPath.left(separatorIndex) : controlPath;
    const QString controlChannel = separatorIndex >= 0 ? controlPath.mid(separatorIndex + 1) : QString();

    ArtifactCore::RigControl2D* control = nullptr;
    if (!controlIdString.isEmpty()) {
     control = rig2D().findControl(ArtifactCore::Id(controlIdString));
    }
    if (!control) {
     return false;
    }

    if (control->kind() == ArtifactCore::RigControlKind::Point) {
     QVector2D pointValue = control->value().value<QVector2D>();
     if (controlChannel.compare(QStringLiteral("x"), Qt::CaseInsensitive) == 0) {
      pointValue.setX(static_cast<float>(value.toDouble()));
     } else if (controlChannel.compare(QStringLiteral("y"), Qt::CaseInsensitive) == 0) {
      pointValue.setY(static_cast<float>(value.toDouble()));
     } else {
      pointValue = value.value<QVector2D>();
     }
     control->setValue(QVariant::fromValue(pointValue));
     impl_->rig2D().evaluate(rigTimeForLayer(this));
     applyRigPropertyBindings(this);
     setDirty(LayerDirtyFlag::Property);
     return true;
    }

    control->setValue(value);
    impl_->rig2D().evaluate(rigTimeForLayer(this));
    applyRigPropertyBindings(this);
    setDirty(LayerDirtyFlag::Property);
    return true;
   }

   return false;
  }
  return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
 }

};
