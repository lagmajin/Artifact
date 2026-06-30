module;
#include <utility>
#include <cstdint>
#include <QVector3D>

export module Artifact.Preview.Pipeline;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Utils.Id;
import Scene.SimulationSettings;

export namespace Artifact
{
 class ArtifactPreviewCompositionPipeline
 {
 private:
     class Impl;
     Impl* impl_ = nullptr;
 public:
 
  ArtifactPreviewCompositionPipeline();
  ~ArtifactPreviewCompositionPipeline();
  void setComposition(Artifact::ArtifactCompositionPtr composition);
  Artifact::ArtifactCompositionPtr composition() const;
  void setSelectedLayerId(const ArtifactCore::LayerID& id);
  void setCurrentFrame(int64_t frame);
  void render(ArtifactIRenderer* renderer);

  void setCrowdSettings(const ArtifactCore::CrowdSettings& s);
  const ArtifactCore::CrowdSettings& crowdSettings() const;
  ArtifactCore::CrowdSettings& crowdSettings();

  void setBoidsTarget(const QVector3D& pos);
  void clearBoidsTarget();

 };

};
