module;

#include <memory>



#include <QtCore/QObject>
#include <wobjectdefs.h>

export module ArtifactProjectManager;

import std;

export namespace Artifact {

 class IArtifactProjectManager {
 public:
  virtual ~IArtifactProjectManager() = default;
  virtual bool closeCurrentProject() = 0;
 };

 class ArtifactProjectManager {
 private:
  class Impl;
  //std::unique_ptr<Impl> impl_;
 public:
  ArtifactProjectManager();
  ~ArtifactProjectManager();
  bool closeCurrentProject();

 //signals:
 // void projectSettingChanged();
 //public slots:

 };

 extern "C" {
  bool projectManagerCurrentClose();
 };

};