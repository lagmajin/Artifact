module;
#include <utility>
#include <entt/entt.hpp>
export  module Artifact.Application.Manager;

namespace Artifact {
 class ArtifactProjectManager;
 class ArtifactProjectService;
 class ArtifactTestProjectManager;
 class ArtifactGlobalEffectManager;
 class ArtifactActiveContextService;
 class ArtifactLayerSelectionManager;
 class ArtifactToolManager;
 class ArtifactMotionSketchTool;
 class ArtifactPuppetTool;
 class ArtifactBrushTool;
 class ArtifactCompositionManager;
}

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
  ArtifactBrushTool* brushTool() const;
  ArtifactCompositionManager* compositionManager() const;
  
  entt::registry& registry();
 
 };

};
