module;

#include <memory>



#include <QtCore/QObject>
#include <folly/Singleton.h>
#include <wobjectdefs.h>

//#include <pybind11/pybind11.h>

export module ArtifactProjectManager;

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
 private:
  class Impl;
  //std::unique_ptr<Impl> impl_;
 public:
  ArtifactProjectManager();
  ~ArtifactProjectManager();
  void createProject();
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