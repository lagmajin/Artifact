module;


module Artifact.Application.Manager;

import EnvironmentVariable;

namespace Artifact
{

class ArtifactApplicationManager::Impl
{
public:

 Impl();
 ~Impl();
};

ArtifactApplicationManager::Impl::Impl()
{

}

ArtifactApplicationManager::Impl::~Impl()
{

}

 ArtifactApplicationManager::ArtifactApplicationManager()
 {

 }

 ArtifactApplicationManager::~ArtifactApplicationManager()
 {

 }

 Artifact::ArtifactApplicationManager* ArtifactApplicationManager::instance()
 {
  static ArtifactApplicationManager* s_instance = new ArtifactApplicationManager();
  return s_instance;
 }

}