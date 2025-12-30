module;

export module Artifact.Service.Application;

import Artifact.Service.ClipboardManager;

export namespace Artifact
{
 class ApplicationService
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ApplicationService();
  ~ApplicationService();

 };


};