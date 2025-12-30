module;

module Artifact.Service.Application;

import std;
import Artifact.Application.Manager;
import Artifact.Service.ClipboardManager;

namespace Artifact
{
 class ApplicationService::Impl
 {
 private:;
 	
 public:
  Impl();
  ~Impl()=default;
  static ArtifactApplicationManager* instance();
 };

 ArtifactApplicationManager* ApplicationService::Impl::instance()
 {

  return ArtifactApplicationManager::instance();
 }

 ApplicationService::ApplicationService()
 {

 }

 ApplicationService::~ApplicationService()
 {

 }

};