module;
#include <entt/entt.hpp>

module Artifact.Application.Manager;





import EnvironmentVariable;
import Artifact.Effects.Manager;
import Artifact.Test.ProjectManager;
import Artifact.Project.Manager;
import Artifact.Service.ActiveContext;
import Artifact.Tool.Manager;
import Artifact.Tool.MotionSketchTool;
import Artifact.Tool.PuppetTool;
import Artifact.Layers.Selection.Manager;
import Artifact.Tool.Brush;

namespace Artifact
{

 class ArtifactApplicationManager::Impl
 {
 private:
  ArtifactGlobalEffectManager effectManager_;
  ArtifactTestProjectManager testProjectManager_;
  ArtifactToolManager toolManager_;
  ArtifactMotionSketchTool motionSketchTool_;
  ArtifactPuppetTool puppetTool_;
  ArtifactBrushTool brushTool_;
  EnvironmentVariableManager manager_;
  ArtifactCompositionManager compositionManager_;

  entt::registry registry_;
  entt::dispatcher dispather_;
 public:

  Impl();
  ~Impl();
  entt::registry& registry();
ArtifactProjectManager* projectManager() const;
  ArtifactGlobalEffectManager* const effectManager();
  ArtifactActiveContextService* activeContext_=new ArtifactActiveContextService();
  ArtifactLayerSelectionManager* layerSelectionManager();
  ArtifactToolManager* toolManager();
  ArtifactMotionSketchTool* motionSketchTool();
  ArtifactCompositionManager* compositionManager();
  ArtifactPuppetTool* puppetTool();
  ArtifactBrushTool* brushTool();
 };

ArtifactApplicationManager::Impl::Impl()
{
}

 ArtifactApplicationManager::Impl::~Impl()
 {
  // Playback is shut down explicitly by AppMain before static destruction.
  // Do not access function-local service singletons from another singleton's
  // destructor: their destruction order is not a valid lifetime contract.
  delete activeContext_;
 }

 entt::registry& ArtifactApplicationManager::Impl::registry()
 {
  return registry_;
 }

ArtifactGlobalEffectManager* const ArtifactApplicationManager::Impl::effectManager()
{
  return &effectManager_;
}

ArtifactProjectManager* ArtifactApplicationManager::Impl::projectManager() const
 {
  static ArtifactProjectManager manager;
  return &manager;
 }

ArtifactLayerSelectionManager* ArtifactApplicationManager::Impl::layerSelectionManager()
 {
  return ArtifactLayerSelectionManager::instance();
 }

 ArtifactToolManager* ArtifactApplicationManager::Impl::toolManager()
 {
  return &toolManager_;
 }

 ArtifactMotionSketchTool* ArtifactApplicationManager::Impl::motionSketchTool()
 {
  return &motionSketchTool_;
 }

 ArtifactCompositionManager* ArtifactApplicationManager::Impl::compositionManager()
 {
  return &compositionManager_;
 }

 ArtifactPuppetTool* ArtifactApplicationManager::Impl::puppetTool()
 {
  return &puppetTool_;
 }

 ArtifactBrushTool* ArtifactApplicationManager::Impl::brushTool()
 {
  return &brushTool_;
 }

 ArtifactApplicationManager::ArtifactApplicationManager() :impl_(new Impl())
 {

 }

 ArtifactApplicationManager::~ArtifactApplicationManager()
 {
  delete impl_;
 }

 ArtifactApplicationManager* ArtifactApplicationManager::instance()
 {
  static ArtifactApplicationManager s_instance = ArtifactApplicationManager();
  return &s_instance;
 }



ArtifactGlobalEffectManager* const ArtifactApplicationManager::effectManager()
{
  return impl_->effectManager();
}

ArtifactActiveContextService* ArtifactApplicationManager::activeContextService()
{
  return impl_->activeContext_;
}

 ArtifactLayerSelectionManager* ArtifactApplicationManager::layerSelectionManager() const
 {
  return impl_->layerSelectionManager();
 }

 ArtifactToolManager* ArtifactApplicationManager::toolManager() const
 {
  return impl_->toolManager();
 }

 ArtifactMotionSketchTool* ArtifactApplicationManager::motionSketchTool() const
 {
  return impl_->motionSketchTool();
 }

 ArtifactCompositionManager* ArtifactApplicationManager::compositionManager() const
 {
  return impl_->compositionManager();
 }

 ArtifactPuppetTool* ArtifactApplicationManager::puppetTool() const
 {
  return impl_->puppetTool();
 }

 ArtifactBrushTool* ArtifactApplicationManager::brushTool() const
 {
  return impl_->brushTool();
 }

 entt::registry& ArtifactApplicationManager::registry()
 {
  return impl_->registry();
 }

}
