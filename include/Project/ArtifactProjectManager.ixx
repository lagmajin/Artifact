module;

#include <memory>


#include <QString>
#include <QObject>
//#include <folly/Singleton.h>
#include <wobjectdefs.h>
//#include <folly/Singleton.h>

//#include <pybind11/pybind11.h>

export module Project.Manager;

import std;

import Project;

import Utils;

namespace pybind11 {}//dummy
namespace folly{}//dummy

export namespace Artifact {
 
 using namespace folly;

 namespace py = pybind11;

 class IArtifactProjectManager {
 public:
  virtual ~IArtifactProjectManager() = default;
  virtual bool closeCurrentProject() = 0;
 };

 class ArtifactProjectManager :public QObject{
  W_OBJECT(ArtifactProjectManager)
 private:
  class Impl;
  Impl* Impl_;
  explicit ArtifactProjectManager(QObject* parent = nullptr);
  
  //ArtifactProjectManager(const ArtifactProjectManager&) = delete;
  //ArtifactProjectManager& operator=(const ArtifactProjectManager&) = delete;
  //static std::shared_ptr<ArtifactProjectManager> getInstance();
 protected:
  virtual ~ArtifactProjectManager();
 public:
  static ArtifactProjectManager& getInstance();
  void createProject();
  void createProject(const QString& projectName);
  void loadFromFile(const QString& fullpath);
  
  bool projectCreated() const;

  void createNewComposition();
  void createNewComposition(const QString& str);
  void createNewComposition(const QString, const QSize& size);
  bool closeCurrentProject();
  std::shared_ptr<ArtifactProject> getCurrentProjectSharedPtr();

  std::weak_ptr<ArtifactProject> getCurrentProjectWeakPtr();

	//Assets
  void addAssetFromFilePath(const QString& filePath);
  void addAssetsFromFilePaths(const QStringList& filePaths);

  //Directory
  void createPhysicalDirectory(const QString& directoryName);

 public:
  void newProjectCreated()
   W_SIGNAL(newProjectCreated)

   void newCompositionCreated()
   W_SIGNAL(newCompositionCreated)

  

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