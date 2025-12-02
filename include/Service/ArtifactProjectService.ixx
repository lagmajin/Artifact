module;


export module Artifact.Service.Project;

import std;

export namespace Artifact
{
	
	class ArtifactProjectService;
	
	typedef std::shared_ptr<ArtifactProjectService> ArtifactProjectServicePtr;
	

 class ArtifactProjectService
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactProjectService();
  ~ArtifactProjectService();
 };


};