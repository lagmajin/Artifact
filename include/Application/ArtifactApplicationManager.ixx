module;
#include <utility>
#include <entt/entt.hpp>
#include <QString>
#include <QApplication>
export  module Artifact.Application.Manager;


import EnvironmentVariable;
import Artifact.Effects.Manager;
import Artifact.Render.Manager;
import Artifact.Service.Project;
import Artifact.Test.ProjectManager;
import Artifact.Project.Manager;
import Artifact.Service.ActiveContext;
import Artifact.Layers.Selection.Manager;
import Artifact.Tool.Manager;
import Artifact.Tool.MotionSketchTool;
import Artifact.Tool.PuppetTool;
import Artifact.Composition.Manager;


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
  ArtifactGlobalEffectManager*const effectManager();
  ArtifactActiveContextService* activeContextService();
  ArtifactLayerSelectionManager* layerSelectionManager() const;
  ArtifactToolManager* toolManager() const;
  ArtifactMotionSketchTool* motionSketchTool() const;
  ArtifactPuppetTool* puppetTool() const;
  ArtifactCompositionManager* compositionManager() const;
  
  entt::registry& registry();
 
 };

};
