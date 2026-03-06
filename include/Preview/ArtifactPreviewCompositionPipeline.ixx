module;

export module Artifact.Preview.Pipeline;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;

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
  void setComposition(Artifact::ArtifactCompositionPtr composition);
  Artifact::ArtifactCompositionPtr composition() const;
  void setSelectedLayerId(const ArtifactCore::LayerID& id);
  void setCurrentFrame(int64_t frame);
  void render(ArtifactIRenderer* renderer);

 };

};