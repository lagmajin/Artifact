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
import Utils.Id;


namespace Artifact {

namespace {

ArtifactCore::RationalTime rigTimeForLayer(const ArtifactAbstractLayer* layer)
{
 if (!layer) {
  return ArtifactCore::RationalTime(0, 30.0);
 }
 double fps = 30.0;
 if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {
  const double compFps = comp->frameRate().framerate();
  if (compFps > 0.0) {
   fps = compFps;
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

 std::shared_ptr<ArtifactCore::ParentConstraint2D> ArtifactAbstract2DLayer::addRigParentConstraint(
     const QString& name,
     const ArtifactCore::Id& targetBoneId,
     const ArtifactCore::Id& parentBoneId)
 {
  auto constraint = std::make_shared<ArtifactCore::ParentConstraint2D>(name, targetBoneId, parentBoneId);
  return std::static_pointer_cast<ArtifactCore::ParentConstraint2D>(impl_->rig2D().addConstraint(constraint));
 }

 std::shared_ptr<ArtifactCore::MapRangeConstraint2D> ArtifactAbstract2DLayer::addRigMapRangeConstraint(
     const QString& name,
     const ArtifactCore::Id& controlId,
     const ArtifactCore::Id& targetBoneId)
 {
  auto constraint = std::make_shared<ArtifactCore::MapRangeConstraint2D>(name, controlId, targetBoneId);
  return std::static_pointer_cast<ArtifactCore::MapRangeConstraint2D>(impl_->rig2D().addConstraint(constraint));
 }

 std::shared_ptr<ArtifactCore::AimConstraint2D> ArtifactAbstract2DLayer::addRigAimConstraint(
     const QString& name,
     const ArtifactCore::Id& sourceBoneId,
     const ArtifactCore::Id& targetBoneId)
 {
  auto constraint = std::make_shared<ArtifactCore::AimConstraint2D>(name, sourceBoneId, targetBoneId);
  return std::static_pointer_cast<ArtifactCore::AimConstraint2D>(impl_->rig2D().addConstraint(constraint));
 }

 std::shared_ptr<ArtifactCore::TwoBoneIKConstraint2D> ArtifactAbstract2DLayer::addRigTwoBoneIKConstraint(
     const QString& name,
     const ArtifactCore::Id& upperBoneId,
     const ArtifactCore::Id& lowerBoneId,
     const ArtifactCore::Id& effectorBoneId,
     const ArtifactCore::Id& targetBoneId)
 {
  auto constraint = std::make_shared<ArtifactCore::TwoBoneIKConstraint2D>(
      name, upperBoneId, lowerBoneId, effectorBoneId, targetBoneId);
  return std::static_pointer_cast<ArtifactCore::TwoBoneIKConstraint2D>(impl_->rig2D().addConstraint(constraint));
 }

 std::shared_ptr<ArtifactCore::RigPropertyBinding2D> ArtifactAbstract2DLayer::addRigPropertyBinding(
     const QString& name,
     const ArtifactCore::Id& controlId,
     const QString& targetPropertyPath)
 {
  auto binding = std::make_shared<ArtifactCore::RigPropertyBinding2D>(
      name, controlId, ArtifactCore::LayerID(id()), targetPropertyPath);
  return std::static_pointer_cast<ArtifactCore::RigPropertyBinding2D>(impl_->rig2D().addPropertyBinding(binding));
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
   impl_->rig2D() = ArtifactCore::Rig2D::fromJson(obj.value("rig2D").toObject());
  }
 }

 std::vector<ArtifactCore::PropertyGroup> ArtifactAbstract2DLayer::getLayerPropertyGroups() const
 {
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();

  ArtifactCore::PropertyGroup rigGroup(QStringLiteral("Rig"));
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
  if (rigControlGroup.propertyCount() > 0) {
   groups.push_back(rigControlGroup);
  }
  return groups;
 }

 bool ArtifactAbstract2DLayer::setLayerPropertyValue(const QString& propertyPath,
                                                     const QVariant& value)
 {
  if (propertyPath.startsWith(QStringLiteral("rig."))) {
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
     return true;
    }

    control->setValue(value);
    return true;
   }

   return false;
  }
  return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
 }

};
