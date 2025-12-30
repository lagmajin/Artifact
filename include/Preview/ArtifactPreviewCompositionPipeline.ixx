module;

export module Artifact.Preview.Pipeline;
import Artifact.Composition.Abstract;

export namespace Artifact
{
 class ArtifactPreviewCompositionPipeline
 {
 private:
     class Impl;
     Impl* impl_;
 public:
 
  ArtifactPreviewCompositionPipeline();
  ~ArtifactPreviewCompositionPipeline();
  void setComposition(ArtifactCompositionPtr& composition);
  void renderFrame();

 };

};