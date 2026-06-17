module;

module Artifact.Preview.Layer.Pipeline;


namespace Artifact {

 class LayerPreviewPipeline::Impl {

 };



 LayerPreviewPipeline::LayerPreviewPipeline()
 {
  impl_ = new Impl();
 }

 LayerPreviewPipeline::~LayerPreviewPipeline()
 {
  delete impl_;
  impl_ = nullptr;
 }

};
