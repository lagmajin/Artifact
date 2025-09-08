module;
#include <entt/entt.hpp>

module Artifact.Application.Manager;

import EnvironmentVariable;

namespace Artifact
{

 class ArtifactApplicationManager::Impl
 {
 private:

  entt::registry registry_;
  entt::dispatcher dispather_;
 public:

  Impl();
  ~Impl();
  entt::registry& registry();
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

 ArtifactApplicationManager::ArtifactApplicationManager():impl_(new Impl())
 {

 }

 ArtifactApplicationManager::~ArtifactApplicationManager()
 {
  delete impl_;
 }

 Artifact::ArtifactApplicationManager* ArtifactApplicationManager::instance()
 {
  static ArtifactApplicationManager* s_instance = new ArtifactApplicationManager();
  return s_instance;
 }

 entt::registry& ArtifactApplicationManager::registry()
 {
  return impl_->registry();
 }

}