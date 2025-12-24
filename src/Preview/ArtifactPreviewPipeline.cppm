module;
#include <QVector>
module Artifact.Preview.Pipeline;

import std;
import Artifact.Composition.Abstract;

namespace Artifact
{
 class ArtifactPreviewPipeline::Impl
 {
  ArtifactCompositionPtr composition_;
  
 public:
  Impl();
  ~Impl();
  void renderFrame();
  ArtifactCompositionPtr composition() const { return composition_; }
  void setComposition(ArtifactCompositionPtr& ptr);
 };

 ArtifactPreviewPipeline::Impl::Impl()
 {

 }

 ArtifactPreviewPipeline::Impl::~Impl()
 {

 }

 void ArtifactPreviewPipeline::Impl::renderFrame()
 {
  if (!composition_)
   return;

  // レイヤー取得（描画順なら allLayer、逆順なら allLayerReversed）
  auto layers = composition_->allLayer(); // or allLayerReversed()

  for (auto& layer : layers)
  {
   if (!layer)
	continue;

   if (layer->isVisible())
   {
	return;
   }

   if (layer->isAdjustmentLayer())
   {

   }
   else {
   }

  }
 }

 void ArtifactPreviewPipeline::Impl::setComposition(ArtifactCompositionPtr& ptr)
 {

 }

 ArtifactPreviewPipeline::ArtifactPreviewPipeline() :impl_(new Impl())
 {

 }

 ArtifactPreviewPipeline::~ArtifactPreviewPipeline()
 {

 }

 void ArtifactPreviewPipeline::renderFrame()
 {
  impl_->renderFrame();

 }

 void ArtifactPreviewPipeline::setComposition(ArtifactCompositionPtr& composition)
 {

 }

};