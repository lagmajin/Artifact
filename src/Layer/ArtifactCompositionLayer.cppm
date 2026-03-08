module;

#include <QVariant>

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
module Artifact.Layer.Composition;

import Artifact.Composition.Abstract;
import Property.Abstract;
import Property.Group;

namespace Artifact {

 class ArtifactCompositionLayer::Impl
 {
 public:
  CompositionID id_;
 };

 ArtifactCompositionLayer::ArtifactCompositionLayer()
  : impl_(new Impl())
 {
 }

 ArtifactCompositionLayer::~ArtifactCompositionLayer()
 {
  delete impl_;
 }

 CompositionID ArtifactCompositionLayer::sourceCompositionId() const
 {
  return impl_->id_;
 }

 void ArtifactCompositionLayer::setCompositionId(const CompositionID& id)
 {
  impl_->id_ = id;
  Q_EMIT changed();
 }

 std::vector<ArtifactCore::PropertyGroup> ArtifactCompositionLayer::getLayerPropertyGroups() const
 {
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
  ArtifactCore::PropertyGroup compGroup(QStringLiteral("Composition"));

  auto idProp = std::make_shared<ArtifactCore::AbstractProperty>();
  idProp->setName(QStringLiteral("composition.sourceId"));
  idProp->setType(ArtifactCore::PropertyType::String);
  idProp->setValue(sourceCompositionId().toString());
  idProp->setDisplayPriority(-120);
  compGroup.addProperty(idProp);

  groups.push_back(compGroup);
  return groups;
 }

 bool ArtifactCompositionLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
 {
  if (propertyPath == QStringLiteral("composition.sourceId")) {
   setCompositionId(CompositionID(value.toString()));
   return true;
  }
  return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
 }

};
