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

 }

 void ArtifactPreviewPipeline::Impl::setComposition(ArtifactCompositionPtr& ptr)
 {

 }

 ArtifactPreviewPipeline::ArtifactPreviewPipeline()
 {

 }

 ArtifactPreviewPipeline::~ArtifactPreviewPipeline()
 {

 }

 void ArtifactPreviewPipeline::renderFrame()
 {

 }

 void ArtifactPreviewPipeline::setComposition(ArtifactCompositionPtr& composition)
 {

 }

};