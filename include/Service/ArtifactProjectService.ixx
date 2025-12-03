module;
#include <wobjectdefs.h>
#include <QSize>
#include <QString>
#include <QObject>

export module Artifact.Service.Project;

import std;
import Utils;
import Project.Settings;
import Artifact.Composition.Abstract;

W_REGISTER_ARGTYPE(QSize)
W_REGISTER_ARGTYPE(QString)

export namespace Artifact
{
 using namespace ArtifactCore;

 class ArtifactProjectService;

 typedef std::shared_ptr<ArtifactProjectService> ArtifactProjectServicePtr;


 class ArtifactProjectService:public QObject
 {
 	W_OBJECT(ArtifactProjectService)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactProjectService(QObject*parent=nullptr);
  ~ArtifactProjectService();
  static ArtifactProjectService* instance();
  ArtifactCompositionWeakPtr findComposition(const CompositionID& id);
 public:	
  void projectSettingChanged(const ArtifactProjectSettings& setting);
  W_SLOT(projectSettingChanged)
 	
 };


};