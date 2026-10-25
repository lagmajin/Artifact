module;
#include <entt/entt.hpp>

module Artifact.Application.Manager;





import Artifact.Application.Manager.Access;

namespace Artifact
{

 class ArtifactApplicationManager::Impl
 {
 private:
  class OwnedAccess
  {
  public:
    void* value;
    void (*destroy)(void*);

    OwnedAccess(void* accessValue, void (*destroyValue)(void*))
      : value(accessValue), destroy(destroyValue) {}
    ~OwnedAccess()
    {
      if (value && destroy) destroy(value);
    }
    OwnedAccess(const OwnedAccess&) = delete;
    OwnedAccess& operator=(const OwnedAccess&) = delete;
  };
  OwnedAccess effectManager_;
  OwnedAccess testProjectManager_;
  OwnedAccess toolManager_;
  OwnedAccess motionSketchTool_;
  OwnedAccess puppetTool_;
  OwnedAccess brushTool_;
  OwnedAccess manager_;
  OwnedAccess compositionManager_;

  entt::registry registry_;
  entt::dispatcher dispather_;
 public:

  Impl();
  ~Impl();
  entt::registry& registry();
ArtifactProjectManager* projectManager() const;
  ArtifactGlobalEffectManager* const effectManager();
  void* activeContext_;
  ArtifactLayerSelectionManager* layerSelectionManager();
  ArtifactToolManager* toolManager();
  ArtifactMotionSketchTool* motionSketchTool();
  ArtifactCompositionManager* compositionManager();
  ArtifactPuppetTool* puppetTool();
  ArtifactBrushTool* brushTool();
 };

ArtifactApplicationManager::Impl::Impl()
  : effectManager_(createApplicationGlobalEffectManager(), destroyApplicationGlobalEffectManager)
  , testProjectManager_(createApplicationTestProjectManager(), destroyApplicationTestProjectManager)
  , toolManager_(createApplicationToolManager(), destroyApplicationToolManager)
  , motionSketchTool_(createApplicationMotionSketchTool(), destroyApplicationMotionSketchTool)
  , puppetTool_(createApplicationPuppetTool(), destroyApplicationPuppetTool)
  , brushTool_(createApplicationBrushTool(), destroyApplicationBrushTool)
  , manager_(createApplicationEnvironmentVariableManager(), destroyApplicationEnvironmentVariableManager)
  , compositionManager_(createApplicationCompositionManager(), destroyApplicationCompositionManager)
  , activeContext_(createApplicationActiveContextService())
{
}

 ArtifactApplicationManager::Impl::~Impl()
 {
  // Playback is shut down explicitly by AppMain before static destruction.
  // Do not access function-local service singletons from another singleton's
  // destructor: their destruction order is not a valid lifetime contract.
  destroyApplicationActiveContextService(activeContext_);
 }

 entt::registry& ArtifactApplicationManager::Impl::registry()
 {
  return registry_;
 }

ArtifactGlobalEffectManager* const ArtifactApplicationManager::Impl::effectManager()
{
  return reinterpret_cast<ArtifactGlobalEffectManager*>(effectManager_.value);
}

ArtifactProjectManager* ArtifactApplicationManager::Impl::projectManager() const
 {
  return reinterpret_cast<ArtifactProjectManager*>(applicationProjectManagerOpaque());
 }

ArtifactLayerSelectionManager* ArtifactApplicationManager::Impl::layerSelectionManager()
 {
  return reinterpret_cast<ArtifactLayerSelectionManager*>(applicationLayerSelectionManagerOpaque());
 }

 ArtifactToolManager* ArtifactApplicationManager::Impl::toolManager()
 {
  return reinterpret_cast<ArtifactToolManager*>(toolManager_.value);
 }

 ArtifactMotionSketchTool* ArtifactApplicationManager::Impl::motionSketchTool()
 {
  return reinterpret_cast<ArtifactMotionSketchTool*>(motionSketchTool_.value);
 }

 ArtifactCompositionManager* ArtifactApplicationManager::Impl::compositionManager()
 {
  return reinterpret_cast<ArtifactCompositionManager*>(compositionManager_.value);
 }

 ArtifactPuppetTool* ArtifactApplicationManager::Impl::puppetTool()
 {
  return reinterpret_cast<ArtifactPuppetTool*>(puppetTool_.value);
 }

 ArtifactBrushTool* ArtifactApplicationManager::Impl::brushTool()
 {
  return reinterpret_cast<ArtifactBrushTool*>(brushTool_.value);
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



ArtifactProjectService* ArtifactApplicationManager::projectService() const
{
  return reinterpret_cast<ArtifactProjectService*>(applicationProjectServiceOpaque());
}
ArtifactGlobalEffectManager* const ArtifactApplicationManager::effectManager()
{
  return impl_->effectManager();
}

ArtifactActiveContextService* ArtifactApplicationManager::activeContextService()
{
  return reinterpret_cast<ArtifactActiveContextService*>(impl_->activeContext_);
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
