module;

export module Artifact.Test.ProjectManager;

import std;


export namespace Artifact
{
 class ArtifactTestProjectManager;
	
 typedef std::shared_ptr<ArtifactTestProjectManager> ArtifactTestProjectManagerPtr;
	
 class ArtifactTestProjectManager
 {
 private:
  class Impl;
  Impl* impl_;
  ArtifactTestProjectManager& operator=(const ArtifactTestProjectManager&) = delete;
 	
 public:
  ArtifactTestProjectManager();
  ~ArtifactTestProjectManager();
 };





}