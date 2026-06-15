module;
#include <entt/entt.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Application.Manager;





import EnvironmentVariable;
import Artifact.Effects.Manager;
import Artifact.Composition.Manager;
import Render.Queue.Manager;
import Artifact.Test.ProjectManager;
import Artifact.Project.Manager;
import Artifact.Service.Project;
import Artifact.Service.ActiveContext;
import Artifact.Tool.Manager;
import Artifact.Tool.MotionSketchTool;
import Artifact.Tool.PuppetTool;
import Artifact.Layers.Selection.Manager;
import Artifact.Composition.Manager;

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
  EnvironmentVariableManager manager_;
  ArtifactLayerSelectionManager selectionManager_;
  ArtifactCompositionManager compositionManager_;

  entt::registry registry_;
  entt::dispatcher dispather_;
 public:

  Impl();
  ~Impl();
  entt::registry& registry();
  ArtifactProjectService* projectService();
  ArtifactProjectManager* projectManager() const;
  ArtifactGlobalEffectManager* const effectManager();
  ArtifactActiveContextService* activeContext_=new ArtifactActiveContextService();
  ArtifactLayerSelectionManager* layerSelectionManager();
  ArtifactToolManager* toolManager();
  ArtifactMotionSketchTool* motionSketchTool();
  ArtifactCompositionManager* compositionManager();
  ArtifactPuppetTool* puppetTool();
 };

 ArtifactApplicationManager::Impl::Impl()
 {

 }

 ArtifactApplicationManager::Impl::~Impl()
 {
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

ArtifactProjectService* ArtifactApplicationManager::Impl::projectService()
{
  return ArtifactProjectService::instance();
}

ArtifactProjectManager* ArtifactApplicationManager::Impl::projectManager() const
 {
  static ArtifactProjectManager manager;
  return &manager;
 }

 ArtifactLayerSelectionManager* ArtifactApplicationManager::Impl::layerSelectionManager()
 {
  return &selectionManager_;
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

ArtifactProjectService* ArtifactApplicationManager::projectService() const
{
  return impl_->projectService();
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

 entt::registry& ArtifactApplicationManager::registry()
 {
  return impl_->registry();
 }

}
