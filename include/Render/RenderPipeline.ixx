module;

export module Artifact.Render.Pipeline;




export namespace Artifact
{
 class RenderPipeline
 {
 private:
  class Impl;
  Impl* impl_;

 public:
  RenderPipeline();
  ~RenderPipeline();
 };



};