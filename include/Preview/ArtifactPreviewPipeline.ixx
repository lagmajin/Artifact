module;

export module Artifact.Preview.Pipeline;
import Artifact.Composition.Abstract;

export namespace Artifact
{
 class ArtifactPreviewPipeline
 {
 private:
     class Impl;
     Impl* impl_;
 public:
 
  ArtifactPreviewPipeline();
  ~ArtifactPreviewPipeline();
  void setComposition(ArtifactCompositionPtr& composition);
  void renderFrame();
 };

};