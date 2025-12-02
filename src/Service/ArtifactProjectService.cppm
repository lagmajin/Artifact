module;

module Artifact.Service.Project;

import std;

namespace Artifact
{
 class ArtifactProjectService::Impl
 {
 private:
 	
 	
 public:
  Impl();
  ~Impl();
 };

 ArtifactProjectService::Impl::Impl()
 {

 }

 ArtifactProjectService::Impl::~Impl()
 {

 }

 ArtifactProjectService::ArtifactProjectService():impl_(new Impl())
 {

 }

 ArtifactProjectService::~ArtifactProjectService()
 {
  delete impl_;
 }
	
	
};