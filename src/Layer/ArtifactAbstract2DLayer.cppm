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
module Artifact.Layers.Abstract._2D;




import Artifact.Layer.Abstract;
import Animation.Transform2D;
import ArtifactCore.Rig2D;


namespace Artifact {

 class ArtifactAbstract2DLayer::Impl {
 private:
  ArtifactCore::Rig2D rig2D_;

 public:
  Impl();
  ~Impl();
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

 ArtifactCore::Rig2D& ArtifactAbstract2DLayer::rig2D()
 {
  return impl_->rig2D_;
 }

 const ArtifactCore::Rig2D& ArtifactAbstract2DLayer::rig2D() const
 {
  return impl_->rig2D_;
 }

 ArtifactCore::Bone2D* ArtifactAbstract2DLayer::addRigBone(const QString& name,
                                                          const QString& parentName)
 {
  ArtifactCore::Bone2D* parent = nullptr;
  if (!parentName.isEmpty()) {
   parent = impl_->rig2D_.findBone(parentName);
  }
  return impl_->rig2D_.addBone(name, parent);
 }

 void ArtifactAbstract2DLayer::clearRigBones()
 {
  impl_->rig2D_.clearBones();
 }

 int ArtifactAbstract2DLayer::rigBoneCount() const
 {
  return static_cast<int>(impl_->rig2D_.bones().size());
 }

 QString ArtifactAbstract2DLayer::rigRootBoneName() const
 {
  const auto* rootBone = impl_->rig2D_.rootBone();
  return rootBone ? rootBone->name() : QString();
 }

 std::vector<ArtifactCore::PropertyGroup> ArtifactAbstract2DLayer::getLayerPropertyGroups() const
 {
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();

  ArtifactCore::PropertyGroup rigGroup(QStringLiteral("Rig"));
  auto makeProp = [this](const QString& name, ArtifactCore::PropertyType type,
                         const QVariant& value, int priority = 0) {
   return persistentLayerProperty(name, type, value, priority);
  };

  rigGroup.addProperty(makeProp(QStringLiteral("rig.boneCount"),
                                ArtifactCore::PropertyType::Integer,
                                static_cast<qint64>(rigBoneCount()),
                                -55));

  rigGroup.addProperty(makeProp(QStringLiteral("rig.rootBone"),
                                ArtifactCore::PropertyType::String,
                                rigRootBoneName(),
                                -54));

  groups.push_back(rigGroup);
  return groups;
 }

 bool ArtifactAbstract2DLayer::setLayerPropertyValue(const QString& propertyPath,
                                                     const QVariant& value)
 {
  if (propertyPath.startsWith(QStringLiteral("rig."))) {
   Q_UNUSED(value);
   return false;
  }
  return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
 }

};
