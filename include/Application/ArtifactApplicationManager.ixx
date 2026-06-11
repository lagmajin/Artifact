module;
#include <utility>
#include <entt/entt.hpp>
#include <QString>
#include <QApplication>
export  module Artifact.Application.Manager;

export namespace Artifact {

 class ArtifactProjectManager;
 class ArtifactProjectService;
 class ArtifactTestProjectManager;
 class ArtifactGlobalEffectManager;
 class ArtifactActiveContextService;
 class ArtifactLayerSelectionManager;
 class ArtifactToolManager;
 class ArtifactMotionSketchTool;
 class ArtifactPuppetTool;
 class ArtifactCompositionManager;

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
