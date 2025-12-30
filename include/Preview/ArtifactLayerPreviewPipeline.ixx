module;
export module Artifact.Preview.Layer.Pipeline;

export namespace Artifact {

  class LayerPreviewPipeline {
  private:
   class Impl;

   Impl* impl_;
  public:
    LayerPreviewPipeline();
    ~LayerPreviewPipeline();
  };
}