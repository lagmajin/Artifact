module;

#include <QAbstractItemModel>
#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>


export module Artifact.Layers.Hierarchy.Model;

import Utils.Id;

export namespace Artifact {

using namespace ArtifactCore;

class ArtifactHierarchyModel : public QAbstractItemModel {
private:
  class Impl;
  Impl *impl_;

public:
  ArtifactHierarchyModel(QObject *parent = nullptr);
  ~ArtifactHierarchyModel();

  void setCompositionId(const CompositionID &compositionId);
  CompositionID currentCompositionId() const;

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  QModelIndex index(int row, int column,
                    const QModelIndex &parent = QModelIndex()) const override;

  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;

  QModelIndex parent(const QModelIndex &child) const override;

  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QStringList mimeTypes() const override;
  QMimeData *mimeData(const QModelIndexList &indexes) const override;
  bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                    int column, const QModelIndex &parent) override;
  bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                       int column, const QModelIndex &parent) const override;
};

} // namespace Artifact
