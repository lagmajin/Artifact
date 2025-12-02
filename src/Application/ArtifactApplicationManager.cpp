module;
#include <entt/entt.hpp>

module Artifact.Application.Manager;

import std;

import EnvironmentVariable;
import Render.Queue.Manager;

import Artifact.Test.ProjectManager;

namespace Artifact
{

 class ArtifactApplicationManager::Impl
 {
 private:
  GlobalEffectManager effectManager_;
  
  entt::registry registry_;
  entt::dispatcher dispather_;
 public:

  Impl();
  ~Impl();
  entt::registry& registry();
  GlobalEffectManager* const effectManager();
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

GlobalEffectManager* const ArtifactApplicationManager::Impl::effectManager()
 {
 return &effectManager_;
 }

 ArtifactApplicationManager::ArtifactApplicationManager():impl_(new Impl())
 {

 }

 ArtifactApplicationManager::~ArtifactApplicationManager()
 {
  delete impl_;
 }

 ArtifactApplicationManager* ArtifactApplicationManager::instance()
 {
  static ArtifactApplicationManager* s_instance = new ArtifactApplicationManager();
  return s_instance;
 }

 GlobalEffectManager* const ArtifactApplicationManager::effectManager()
 {
  return impl_->effectManager();
 }

 entt::registry& ArtifactApplicationManager::registry()
 {
  return impl_->registry();
 }

}