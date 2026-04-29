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
  bool removeRigBone(const ArtifactCore::Id& boneId);
  bool setRigBoneLocalTransform(const ArtifactCore::Id& boneId,
                                const ArtifactCore::BoneTransform& transform);
  void clearRigBones();
  int rigBoneCount() const;
  QString rigRootBoneName() const;
  QJsonObject toJson() const override;
  void fromJsonProperties(const QJsonObject& obj) override;
  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

 };












};
