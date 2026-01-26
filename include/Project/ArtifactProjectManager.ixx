module;

#include <memory>
#include <QString>
#include <QObject>
#include <wobjectdefs.h>

export module Artifact.Project.Manager;

import std;
import Utils;
import Utils.String.UniString;

import Artifact.Project;
import Composition.Settings;
import Artifact.Project.Result;
import Artifact.Composition.Result;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;

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

 class ArtifactProjectManager :public QObject {
  W_OBJECT(ArtifactProjectManager)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactProjectManager(QObject* parent = nullptr);
  ~ArtifactProjectManager();
  static ArtifactProjectManager& getInstance();
 	/*Project Func*/
  void createProject();
  CreateProjectResult createProject(const UniString& name,bool force=false);
  void createProject(const QString& projectName, bool force = false);
  void loadFromFile(const QString& fullpath);

  bool isProjectCreated() const;
  bool isProjectClosed() const;
  bool closeCurrentProject();
 	/*ProjectFunc*/
  std::shared_ptr<ArtifactProject> getCurrentProjectSharedPtr();

  std::weak_ptr<ArtifactProject> getCurrentProjectWeakPtr();
 	/*Compostion*/
  void createComposition();
  CreateCompositionResult createComposition(const ArtifactCompositionInitParams& setting);
  CreateCompositionResult createComposition(const UniString& str);
  void createComposition(const QString, const QSize& size);
  int compositionCount() const;
  ArtifactCompositionPtr currentComposition();
  FindCompositionResult findComposition(const CompositionID& id);

  QVector<ProjectItem*> projectItems() const;

  //Assets
  void addAssetFromFilePath(const QString& filePath);
  void addAssetsFromFilePaths(const QStringList& filePaths);
  void removeAllAssets();
 	
  //Directory
  void createPhysicalDirectory(const QString& directoryName);


 public:
  void projectCreated()
   W_SIGNAL(projectCreated);
  void projectClosed()
   W_SIGNAL(projectClosed);
  void projectChanged()
   W_SIGNAL(projectChanged);

  void compositionCreated(const CompositionID& id)
   W_SIGNAL(compositionCreated, id);

   void layerCreated(const LayerID& id)
   W_SIGNAL(layerCreated, id);

 };





 extern "C" {
  bool projectManagerCurrentClose();
 };

 /*
 PYBIND11_MODULE(my_module, m) {
  py::class_<ArtifactProjectManager>(m, "ArtifactProjectManager")
   .def(py::init<>())  // コンストラクタ公開
   .def("closeCurrentProject", &ArtifactProjectManager::closeCurrentProject)
   ;

   */
};