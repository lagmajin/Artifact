module;
#include <wobjectdefs.h>
#include <QStandardItemModel>
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
export module Artifact.Project.Model;




import Artifact.Project;
import Utils.Id;
import Utils.String.UniString;


export namespace Artifact
{
 using namespace ArtifactCore;


 class ArtifactProjectModel:public QAbstractItemModel
 {
 private:
  class Impl;
  Impl* impl_;
  QModelIndex mapToSource(const QModelIndex& proxyIndex) const;
 public:
	 QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	 int rowCount(const QModelIndex& parent) const override;
	 int columnCount(const QModelIndex& parent) const override;
 	 QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
 	 QVariant data(const QModelIndex& index, int role) const override;
 	 QModelIndex parent(const QModelIndex& index) const override;
 	 Qt::ItemFlags flags(const QModelIndex &index) const override;
 	 Qt::DropActions supportedDropActions() const override;

 
 public:
  ArtifactProjectModel(QObject*parent=nullptr);
  ~ArtifactProjectModel();
  void setProject(const std::shared_ptr<ArtifactProject>& project);
  void onCompositionCreated(const ArtifactCore::CompositionID& id);
 };











};
