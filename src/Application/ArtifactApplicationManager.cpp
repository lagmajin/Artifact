module;
#include <entt/entt.hpp>

module Artifact.Application.Manager;

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




import EnvironmentVariable;
import Render.Queue.Manager;
import Artifact.Test.ProjectManager;
import Artifact.Project.Manager;
import Artifact.Service.ActiveContext;
import Artifact.Tool.Manager;
import Artifact.Layers.Selection.Manager;

namespace Artifact
{

 class ArtifactApplicationManager::Impl
 {
 private:
  ArtifactGlobalEffectManager effectManager_;
  ArtifactTestProjectManager testProjectManager_;
  ArtifactToolManager toolManager_;
  EnvironmentVariableManager manager_;
  ArtifactLayerSelectionManager selectionManager_;
 	
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

 entt::registry& ArtifactApplicationManager::registry()
 {
  return impl_->registry();
 }

}