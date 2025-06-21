module;

#include <memory>


#include <QtCore/QObject>
#include <wobjectdefs.h>

export module ArtifactProjectManager;

import std;

export namespace Artifact {

 class ArtifactProjectManagerPrivate;

 class ArtifactProjectManager {
 private:
  class Impl;
  //std::unique_ptr<Impl> impl_;
 public:
  ArtifactProjectManager();
  ~ArtifactProjectManager();
 //signals:
 // void projectSettingChanged();
 //public slots:

 };




};