module;

#include <QAbstractItemModel>
#include <QDataStream>
#include <QIcon>
#include <QMimeData>
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

module Artifact.Layers.Hierarchy.Model;

import Utils;
import Artifact.Composition.Abstract;
import Artifact.Project.Manager;
import Artifact.Layer.Abstract;
import Artifact.Layer.Group;
import Event.Bus;
import Artifact.Event.Types;

namespace Artifact {
using namespace ArtifactCore;

class ArtifactHierarchyModel::Impl {
public:
  ArtifactHierarchyModel *owner_ = nullptr;
  CompositionID currentCompositionId_;
  std::vector<ArtifactCore::EventBus::Subscription> subscriptions_;

  Impl(ArtifactHierarchyModel *owner) : owner_(owner) {
    auto &bus = ArtifactCore::globalEventBus();
    
    // コンポジション切り替えイベント
    subscriptions_.push_back(bus.subscribe<CurrentCompositionChangedEvent>([this](const CurrentCompositionChangedEvent &event) {
      owner_->setCompositionId(CompositionID(Id(event.compositionId)));
    }));

    // レイヤー変更イベントの購読
    subscriptions_.push_back(bus.subscribe<LayerChangedEvent>([this](const LayerChangedEvent &event) {
      if (CompositionID(Id(event.compositionId)) == currentCompositionId_) {
        owner_->beginResetModel();
        owner_->endResetModel();
      }
    }));
  }

  ArtifactCompositionPtr currentComposition() const {
    auto &pm = ArtifactProjectManager::getInstance();
    if (!currentCompositionId_.isNil()) {
      auto result = pm.findComposition(currentCompositionId_);
      if (result.success) return result.ptr.lock();
    }
    return pm.currentComposition();
  }

  std::vector<ArtifactAbstractLayerPtr> rootLayers() const {
    auto comp = currentComposition();
    if (!comp) return {};
    
    std::vector<ArtifactAbstractLayerPtr> result;
    auto allLayers = comp->allLayer();
    for (auto &layer : allLayers) {
      if (layer->parentLayerId().isNil()) {
        result.push_back(layer);
      }
    }
    return result;
  }
};

ArtifactHierarchyModel::ArtifactHierarchyModel(QObject *parent /*= nullptr*/)
    : QAbstractItemModel(parent), impl_(new Impl(this)) {}

ArtifactHierarchyModel::~ArtifactHierarchyModel() {
  delete impl_;
}

void ArtifactHierarchyModel::setCompositionId(const CompositionID &id) {
  beginResetModel();
  impl_->currentCompositionId_ = id;
  endResetModel();
}

CompositionID ArtifactHierarchyModel::currentCompositionId() const {
  return impl_->currentCompositionId_;
}

QVariant ArtifactHierarchyModel::headerData(int section,
                                            Qt::Orientation orientation,
                                            int role) const {
  if (orientation == Qt::Horizontal) {
    if (role == Qt::DisplayRole) {
      switch (section) {
      case 0: return QStringLiteral("V"); // Visibility
      case 1: return QStringLiteral("L"); // Lock
      case 2: return QStringLiteral("T"); // Type
      case 3: return QStringLiteral("名前");
      }
    }
  }
  return QVariant();
}

int ArtifactHierarchyModel::rowCount(const QModelIndex &parent) const {
  auto comp = impl_->currentComposition();
  if (!comp) return 0;

  if (!parent.isValid()) {
    return static_cast<int>(impl_->rootLayers().size());
  }

  auto parentLayer = static_cast<ArtifactAbstractLayer*>(parent.internalPointer());
  if (parentLayer && parentLayer->isGroupLayer()) {
    auto groupLayer = dynamic_cast<ArtifactGroupLayer*>(parentLayer);
    if (groupLayer) {
      return static_cast<int>(groupLayer->children().size());
    }
  }

  return 0;
}

int ArtifactHierarchyModel::columnCount(const QModelIndex &parent) const {
  return 4;
}

QModelIndex ArtifactHierarchyModel::index(int row, int column,
                                          const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent))
    return QModelIndex();

  auto comp = impl_->currentComposition();
  if (!comp) return QModelIndex();

  ArtifactAbstractLayerPtr targetLayer;

  if (!parent.isValid()) {
    auto roots = impl_->rootLayers();
    if (row < roots.size()) {
      targetLayer = roots[row];
    }
  } else {
    auto parentLayer = static_cast<ArtifactAbstractLayer*>(parent.internalPointer());
    if (parentLayer && parentLayer->isGroupLayer()) {
      auto groupLayer = dynamic_cast<ArtifactGroupLayer*>(parentLayer);
      if (groupLayer && row < groupLayer->children().size()) {
        targetLayer = groupLayer->children()[row];
      }
    }
  }

  if (targetLayer) {
    return createIndex(row, column, targetLayer.get());
  }

  return QModelIndex();
}

QModelIndex ArtifactHierarchyModel::parent(const QModelIndex &child) const {
  if (!child.isValid()) return QModelIndex();

  auto childLayer = static_cast<ArtifactAbstractLayer*>(child.internalPointer());
  if (!childLayer) return QModelIndex();

  auto parentId = childLayer->parentLayerId();
  if (parentId.isNil()) return QModelIndex();

  auto comp = impl_->currentComposition();
  if (!comp) return QModelIndex();

  auto parentLayer = comp->layerById(parentId);
  if (!parentLayer) return QModelIndex();

  // Find grand parent to determine row of the parent
  auto grandParentId = parentLayer->parentLayerId();
  int row = 0;
  if (grandParentId.isNil()) {
    auto roots = impl_->rootLayers();
    auto it = std::find_if(roots.begin(), roots.end(), [&](const ArtifactAbstractLayerPtr& l) { return l->id() == parentId; });
    row = (it != roots.end()) ? (int)std::distance(roots.begin(), it) : 0;
    return createIndex(row, 0, parentLayer.get());
  } else {
    auto grandParent = comp->layerById(grandParentId);
    if (grandParent && grandParent->isGroupLayer()) {
      auto group = dynamic_cast<ArtifactGroupLayer*>(grandParent.get());
      auto &children = group->children();
      auto it = std::find_if(children.begin(), children.end(), [&](const ArtifactAbstractLayerPtr& l) { return l->id() == parentId; });
      row = (it != children.end()) ? (int)std::distance(children.begin(), it) : 0;
      return createIndex(row, 0, parentLayer.get());
    }
  }

  return QModelIndex();
}

QVariant ArtifactHierarchyModel::data(const QModelIndex &index,
                                      int role) const {
  if (!index.isValid()) return QVariant();

  auto layer = static_cast<ArtifactAbstractLayer*>(index.internalPointer());
  if (!layer) return QVariant();

  if (role == Qt::DisplayRole) {
    if (index.column() == 3) {
      return layer->layerName();
    }
  }

  if (role == Qt::DecorationRole) {
    if (index.column() == 0) {
      return layer->isVisible() ? QIcon(resolveIconPath("visibility.png")) : QIcon(resolveIconPath("visibility_off.png"));
    }
    if (index.column() == 2) {
      if (layer->isGroupLayer()) {
        return QIcon(resolveIconPath("folder.png"));
      }
      return QIcon(resolveIconPath("image_layer.png"));
    }
  }

  if (role == Qt::BackgroundRole) {
    if (index.row() % 2 == 0)
      return QColor(35, 35, 35);
    else
      return QColor(40, 40, 40);
  }

  if (role == Qt::ForegroundRole) {
    return QColor(220, 220, 220);
  }

  return QVariant();
}

Qt::ItemFlags ArtifactHierarchyModel::flags(const QModelIndex &index) const {
  Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);
  if (index.isValid()) {
    defaultFlags |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
  }
  return defaultFlags;
}

QStringList ArtifactHierarchyModel::mimeTypes() const {
  QStringList types;
  types << "application/vnd.artifact.layer.index";
  return types;
}

QMimeData *
ArtifactHierarchyModel::mimeData(const QModelIndexList &indexes) const {
  QMimeData *mimeData = new QMimeData();
  QByteArray encodedData;
  QDataStream stream(&encodedData, QIODevice::WriteOnly);

  for (const QModelIndex &index : indexes) {
    if (index.isValid() && index.column() == 0) {
      stream << index.row();
    }
  }

  mimeData->setData("application/vnd.artifact.layer.index", encodedData);
  return mimeData;
}

bool ArtifactHierarchyModel::dropMimeData(const QMimeData *data,
                                          Qt::DropAction action, int row,
                                          int column,
                                          const QModelIndex &parent) {
  if (action == Qt::IgnoreAction) {
    return true;
  }
  if (!data->hasFormat("application/vnd.artifact.layer.index")) {
    return false;
  }
  if (column > 0) {
    return false;
  }
  emit layoutChanged();
  return true;
}

bool ArtifactHierarchyModel::canDropMimeData(const QMimeData *data,
                                             Qt::DropAction action, int row,
                                             int column,
                                             const QModelIndex &parent) const {
  if (!data->hasFormat("application/vnd.artifact.layer.index")) {
    return false;
  }
  if (column > 0) {
    return false;
  }
  return true;
}

} // namespace Artifact