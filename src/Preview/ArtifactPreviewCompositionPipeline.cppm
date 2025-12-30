module;
#include <QVector>
module Artifact.Preview.Pipeline;

import std;
import Artifact.Composition.Abstract;

namespace Artifact
{
 class ArtifactPreviewCompositionPipeline::Impl
 {
  ArtifactCompositionPtr composition_;
  
 public:
  Impl();
  ~Impl();
  void renderFrame();
  ArtifactCompositionPtr composition() const { return composition_; }
  void setComposition(ArtifactCompositionPtr& ptr);
 };

 ArtifactPreviewCompositionPipeline::Impl::Impl()
 {

 }

 ArtifactPreviewCompositionPipeline::Impl::~Impl()
 {

 }

 void ArtifactPreviewCompositionPipeline::Impl::renderFrame()
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

 void ArtifactPreviewCompositionPipeline::Impl::setComposition(ArtifactCompositionPtr& ptr)
 {

 }

 ArtifactPreviewCompositionPipeline::ArtifactPreviewCompositionPipeline() :impl_(new Impl())
 {

 }

 ArtifactPreviewCompositionPipeline::~ArtifactPreviewCompositionPipeline()
 {

 }

 void ArtifactPreviewCompositionPipeline::renderFrame()
 {
  impl_->renderFrame();

 }

 void ArtifactPreviewCompositionPipeline::setComposition(ArtifactCompositionPtr& composition)
 {

 }

};