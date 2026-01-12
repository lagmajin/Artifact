module;
#include <entt/entt.hpp>

module Artifact.Application.Manager;

import std;

import EnvironmentVariable;
import Render.Queue.Manager;
import Artifact.Test.ProjectManager;
import Artifact.Project.Manager;
import Artifact.Service.ActiveContext;
import Artifact.Tool.Manager;

namespace Artifact
{

 class ArtifactApplicationManager::Impl
 {
 private:
  ArtifactGlobalEffectManager effectManager_;
  ArtifactTestProjectManager testProjectManager_;
  ArtifactToolManager toolManager_;
  EnvironmentVariableManager manager_;
 	
  entt::registry registry_;
  entt::dispatcher dispather_;
 public:

  Impl();
  ~Impl();
  entt::registry& registry();
  ArtifactProjectManager* projectManager() const;
  ArtifactGlobalEffectManager* const effectManager();
  ArtifactActiveContextService* activeContext_=new ArtifactActiveContextService();
  //GlobalEffectManager
 };

 ArtifactApplicationManager::Impl::Impl()
 {

 }

 ArtifactApplicationManager::Impl::~Impl()
 {

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

 entt::registry& ArtifactApplicationManager::registry()
 {
  return impl_->registry();
 }

}