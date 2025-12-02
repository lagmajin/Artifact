module;

module Artifact.Test.ProjectManager;

import std;
//#include <boost/asio/prefer.hpp>

namespace Artifact
{
 class ArtifactTestProjectManager::Impl
 {
 private:
 	
 	
 public:
  Impl();
  ~Impl();
 };

 ArtifactTestProjectManager::Impl::Impl()
 {

 }

 ArtifactTestProjectManager::Impl::~Impl()
 {

 }

 ArtifactTestProjectManager::ArtifactTestProjectManager():impl_(new Impl)
 {

 }

 ArtifactTestProjectManager::~ArtifactTestProjectManager()
 {
  delete impl_;
 }

};