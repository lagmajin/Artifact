module;

export module Artifact.Render.Manager;

export namespace Artifact {

 class ArtifactRenderManager {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactRenderManager();
  ~ArtifactRenderManager();
 };



};