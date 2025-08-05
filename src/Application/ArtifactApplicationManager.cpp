module;


module Artifact.Application.Manager;


namespace Artifact
{

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