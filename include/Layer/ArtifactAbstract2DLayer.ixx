module;

#include "Define/DllExportMacro.hpp"
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QString>
#include <QVariant>
#include <QVector2D>
#include <QJsonObject>
export module Artifact.Layers.Abstract._2D;





import Artifact.Layer.Abstract;
import ArtifactCore.Rig2D;
import Utils.Id;


export namespace Artifact
{

 class   ArtifactAbstract2DLayer : public ArtifactAbstractLayer {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactAbstract2DLayer();
  ~ArtifactAbstract2DLayer();

  ArtifactCore::Rig2D& rig2D();
  const ArtifactCore::Rig2D& rig2D() const;
  ArtifactCore::Bone2D* addRigBone(const QString& name = QStringLiteral("Bone"),
                                   const QString& parentName = QString());
  ArtifactCore::Bone2D* addRigBone(const QString& name,
                                   const ArtifactCore::Id& parentId);
  ArtifactCore::RigControl2D* addRigSlider(const QString& name,
                                           double defaultValue = 0.0,
                                           double minValue = 0.0,
                                           double maxValue = 1.0);
  ArtifactCore::RigControl2D* addRigPoint(const QString& name,
                                          const QVector2D& defaultValue = QVector2D());
  ArtifactCore::RigControl2D* addRigAngle(const QString& name,
                                          double defaultValue = 0.0,
                                          double minValue = -180.0,
                                          double maxValue = 180.0);
  bool removeRigBone(const ArtifactCore::Id& boneId);
  bool setRigBoneLocalTransform(const ArtifactCore::Id& boneId,
                                const ArtifactCore::BoneTransform& transform);
  std::shared_ptr<ArtifactCore::ParentConstraint2D> addRigParentConstraint(
      const QString& name,
      const ArtifactCore::Id& targetBoneId,
      const ArtifactCore::Id& parentBoneId);
  std::shared_ptr<ArtifactCore::MapRangeConstraint2D> addRigMapRangeConstraint(
      const QString& name,
      const ArtifactCore::Id& controlId,
      const ArtifactCore::Id& targetBoneId);
  std::shared_ptr<ArtifactCore::AimConstraint2D> addRigAimConstraint(
      const QString& name,
      const ArtifactCore::Id& sourceBoneId,
      const ArtifactCore::Id& targetBoneId);
  std::shared_ptr<ArtifactCore::TwoBoneIKConstraint2D> addRigTwoBoneIKConstraint(
      const QString& name,
      const ArtifactCore::Id& upperBoneId,
      const ArtifactCore::Id& lowerBoneId,
      const ArtifactCore::Id& effectorBoneId,
      const ArtifactCore::Id& targetBoneId);
  void clearRigBones();
  int rigBoneCount() const;
  QString rigRootBoneName() const;
  QJsonObject toJson() const override;
  void fromJsonProperties(const QJsonObject& obj) override;
  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

 };












};
