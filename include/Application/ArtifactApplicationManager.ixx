module;
#include <QString>
#include <QApplication>
#include <entt/entt.hpp>
export  module Artifact.Application.Manager;


import EnvironmentVariable;
import Artifact.Effects.Manager;
import Artifact.Render.Manager;
import Artifact.Service.Project;
import Artifact.Test.ProjectManager;
import Artifact.Project.Manager;
import Artifact.Service.ActiveContext;


export namespace Artifact {

 class ArtifactApplicationManager
 {
 private:
  class Impl;
  Impl* impl_;
  ArtifactApplicationManager(const ArtifactApplicationManager&) = delete;
 public:
  ArtifactApplicationManager();
  ~ArtifactApplicationManager();
  static ArtifactApplicationManager* instance();
  ArtifactProjectManager* projectManager() const;
  ArtifactProjectService* projectService() const;
  ArtifactTestProjectManager* testProjectManager() const;
  GlobalEffectManager*const effectManager();
  ArtifactActiveContextService* activeContextService();
  
  //void copyImageToClipboard(const QImage& image);
 	
  //ArtifactProjectManager* const projectManager();
  entt::registry& registry();
 
 };













};