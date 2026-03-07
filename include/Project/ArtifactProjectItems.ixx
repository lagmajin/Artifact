module;
#include <QVector>
#include <QColor>
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
export module Artifact.Project.Items;




import Utils.Id;
import Utils.String.UniString;

export namespace Artifact {

 using namespace ArtifactCore;

 enum class eProjectItemType {
  Unknown,
  Folder,
  Composition,
  Footage,
  Solid
 };



 class ProjectItem {
 public:
  virtual ~ProjectItem() = default;
  virtual eProjectItemType type() const = 0;

  Id id;
  UniString name;
  ProjectItem* parent = nullptr;
  QVector<ProjectItem*> children; // non-owning pointers; ownership managed by project
 };

 // 2. ۃNXiKvȃf[^̂ݕێj
 class FootageItem : public ProjectItem {
 public:
  eProjectItemType type() const override { return eProjectItemType::Footage; }
  QString filePath;
  
 };

class FolderItem : public ProjectItem {
 public:
  eProjectItemType type() const override { return eProjectItemType::Folder; }
  FolderItem* addChildFolder(const UniString& name);
};

 class SolidItem : public ProjectItem {
  eProjectItemType type() const override { return eProjectItemType::Solid; }
  QColor color;

 };

class CompositionItem : public ProjectItem {
 public:
  eProjectItemType type() const override { return eProjectItemType::Composition; }
  ArtifactCore::CompositionID compositionId;
};

};