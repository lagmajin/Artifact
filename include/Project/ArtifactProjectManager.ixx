module;

#include <memory>
#include <QString>
#include <QStringList>
#include <QObject>
#include <wobjectdefs.h>

export module Artifact.Project.Manager;

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



import Utils;
import Utils.String.UniString;

import Artifact.Project;
export import Artifact.Project.Exporter;
import Artifact.Project.Importer;
import Composition.Settings;
import Artifact.Project.Result;
import Artifact.Composition.Result;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Project.Items;
import Artifact.Layer.InitParams;
import Artifact.Layer.Result;

namespace pybind11 {}//dummy
namespace folly {}//dummy
W_REGISTER_ARGTYPE(ArtifactCore::LayerID)
W_REGISTER_ARGTYPE(ArtifactCore::CompositionID)

export namespace Artifact {

 using namespace folly;
 namespace py = pybind11;

 class IArtifactProjectManager {
 public:
  virtual ~IArtifactProjectManager() = default;
  virtual bool closeCurrentProject() = 0;
 };

 class ArtifactProjectManager : public QObject {
  W_OBJECT(ArtifactProjectManager)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactProjectManager(QObject* parent = nullptr);
  ~ArtifactProjectManager();
  static ArtifactProjectManager& getInstance();

  /* Project management */
  void createProject();
  CreateProjectResult createProject(const UniString& name, bool force = false);
  void createProject(const QString& projectName, bool force = false);
  void loadFromFile(const QString& fullpath);
  ArtifactProjectExporterResult saveToFile(const QString& fullpath);
  QString currentProjectPath() const;
  QString currentProjectAssetsPath() const;
  QStringList copyFilesToProjectAssets(const QStringList& sourcePaths);
  QString relativeAssetPath(const QString& absoluteAssetPath) const;
  void createPhysicalDirectory(const QString& directoryName);
  bool isProjectCreated() const;
  bool isProjectClosed() const;
  bool closeCurrentProject();
  std::shared_ptr<ArtifactProject> getCurrentProjectSharedPtr();
  std::weak_ptr<ArtifactProject> getCurrentProjectWeakPtr();

  /* Composition management */
  void createComposition();
  CreateCompositionResult createComposition(const ArtifactCompositionInitParams& setting);
  CreateCompositionResult createComposition(const UniString& str);
  void createComposition(const QString, const QSize& size);
  void suppressDefaultCreate(bool v);
  int compositionCount() const;
  ArtifactCompositionPtr currentComposition();
  FindCompositionResult findComposition(const CompositionID& id);

  QVector<ProjectItem*> projectItems() const;

  /* Layer management */
  ArtifactLayerResult addLayerToCurrentComposition(ArtifactLayerInitParams& params);
  ArtifactLayerResult addLayerToComposition(const CompositionID& compositionId, ArtifactLayerInitParams& params);
  bool removeLayerFromComposition(const CompositionID& compositionId, const LayerID& layerId);
  ArtifactLayerResult duplicateLayerInComposition(const CompositionID& compositionId, const LayerID& layerId);
  CreateCompositionResult duplicateComposition(const CompositionID& compositionId);

  /* Assets */
  void addAssetFromFilePath(const QString& filePath);
  void addAssetsFromFilePaths(const QStringList& filePaths);
  void removeAllAssets();

 public:
  void projectCreated()
   W_SIGNAL(projectCreated);
  void projectClosed()
   W_SIGNAL(projectClosed);
  void projectChanged()
   W_SIGNAL(projectChanged);

  void compositionCreated(const CompositionID& id)
   W_SIGNAL(compositionCreated, id);

  void layerCreated(const CompositionID& compId, const LayerID& id)
   W_SIGNAL(layerCreated, compId, id);
  void layerRemoved(const CompositionID& compId, const LayerID& id)
   W_SIGNAL(layerRemoved, compId, id);
 };

extern "C" {
 bool projectManagerCurrentClose();
};
}
