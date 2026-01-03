module;

#include <memory>


#include <QString>
#include <QObject>
//#include <folly/Singleton.h>
#include <wobjectdefs.h>
//#include <folly/Singleton.h>

//#include <pybind11/pybind11.h>

export module Artifact.Project.Manager;

import std;
import Utils;

import Artifact.Project;
import Composition.Settings;
import Artifact.Composition.Result;
import Artifact.Composition.Abstract;


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
  Impl* Impl_;


  //ArtifactProjectManager(const ArtifactProjectManager&) = delete;
  //ArtifactProjectManager& operator=(const ArtifactProjectManager&) = delete;
  //static std::shared_ptr<ArtifactProjectManager> getInstance();
 protected:

 public:
  explicit ArtifactProjectManager(QObject* parent = nullptr);
  ~ArtifactProjectManager();
  static ArtifactProjectManager& getInstance();
  void createProject();
  void createProject(const QString& projectName, bool force = false);
  void loadFromFile(const QString& fullpath);

  bool isProjectCreated() const;
  bool isProjectClosed() const;

  void createComposition();
  CreateCompositionResult createComposition(const CompositionSettings& setting);
  void createComposition(const QString& str);
  void createComposition(const QString, const QSize& size);

  bool closeCurrentProject();
  std::shared_ptr<ArtifactProject> getCurrentProjectSharedPtr();

  std::weak_ptr<ArtifactProject> getCurrentProjectWeakPtr();
  //
  ArtifactCompositionPtr currentComposition();
  ArtifactCompositionPtr findComposition(const CompositionID& id);

  //Assets
  void addAssetFromFilePath(const QString& filePath);
  void addAssetsFromFilePaths(const QStringList& filePaths);

  //Directory
  void createPhysicalDirectory(const QString& directoryName);


 public:
  void projectCreated()
   W_SIGNAL(projectCreated);
  void projectClosed()
   W_SIGNAL(projectClosed);
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