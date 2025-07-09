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
  //std::unique_ptr<Impl> impl_;
  //ArtifactProjectManager();
  explicit ArtifactProjectManager(QObject* parent = nullptr);
  ~ArtifactProjectManager();
  ArtifactProjectManager(const ArtifactProjectManager&) = delete;
  ArtifactProjectManager& operator=(const ArtifactProjectManager&) = delete;
  //static std::shared_ptr<ArtifactProjectManager> getInstance();
 public:
  static ArtifactProjectManager& getInstance();
  void createProject();
  void createProject(const QString& projectName);
  void loadfromFile(const QString& fullpath);
  
  bool closeCurrentProject();

 //signals:
 // void projectSettingChanged();
 //public slots:

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