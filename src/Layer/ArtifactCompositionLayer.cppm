module;

#include <QSize>
#include <QVariant>

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

module Artifact.Layer.Composition;

import Artifact.Composition.Abstract;
import Property.Abstract;
import Property.Group;
import Artifact.Service.Project;
import Composition.Settings;

namespace Artifact {

class ArtifactCompositionLayer::Impl {
public:
  CompositionID id_;
};

ArtifactCompositionLayer::ArtifactCompositionLayer() : impl_(new Impl()) {}

ArtifactCompositionLayer::~ArtifactCompositionLayer() { delete impl_; }

CompositionID ArtifactCompositionLayer::sourceCompositionId() const {
  return impl_->id_;
}

void ArtifactCompositionLayer::setCompositionId(const CompositionID &id) {
  impl_->id_ = id;
  Q_EMIT changed();
}

std::shared_ptr<ArtifactAbstractComposition>
ArtifactCompositionLayer::sourceComposition() const {
  auto *service = ArtifactProjectService::instance();
  if (!service)
    return nullptr;
  auto result = service->findComposition(impl_->id_);
  return result.ptr.lock();
}

QRectF ArtifactCompositionLayer::localBounds() const {
  if (auto comp = sourceComposition()) {
    const QSize size = comp->settings().compositionSize();
    return QRectF(0, 0, size.width(), size.height());
  }
  return QRectF(0, 0, 100, 100);
}

std::vector<ArtifactCore::PropertyGroup>
ArtifactCompositionLayer::getLayerPropertyGroups() const {
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
  ArtifactCore::PropertyGroup compGroup(QStringLiteral("Composition"));

  auto idProp = persistentLayerProperty(QStringLiteral("composition.sourceId"),
                                        ArtifactCore::PropertyType::String,
                                        sourceCompositionId().toString(), -120);
  compGroup.addProperty(idProp);

  groups.push_back(compGroup);
  return groups;
}

bool ArtifactCompositionLayer::setLayerPropertyValue(
    const QString &propertyPath, const QVariant &value) {
  if (propertyPath == QStringLiteral("composition.sourceId")) {
    setCompositionId(CompositionID(value.toString()));
    return true;
  }
  return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

}; // namespace Artifact
