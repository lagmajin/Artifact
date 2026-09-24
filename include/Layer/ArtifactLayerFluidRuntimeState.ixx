module;

#include <cstdint>
#include <limits>
#include <memory>

#include <QMatrix4x4>
#include <QRectF>

export module Artifact.Layer.FluidRuntimeState;

import Container.NamedVector;
import Artifact.Render.IRenderer;
import Graphics.ParticleData;
import Physics.Fluid;

export namespace Artifact {

class LayerFluidRuntimeState final {
public:
  using LiquidSpillParticle = ArtifactCore::LiquidSpillParticle2D;

  struct LiquidLayerCheckpoint {
    ArtifactCore::LiquidSnapshot2D container;
    ArtifactCore::NamedVector<LiquidSpillParticle> spillParticles{
        ArtifactCore::ContainerName{"Layer.LiquidCheckpointSpillParticles"}};
    double inflowCarry = 0.0;
  };

  struct LiquidLayerCheckpointEntry {
    std::int64_t frame = 0;
    LiquidLayerCheckpoint checkpoint;
  };

  LayerFluidRuntimeState();
  ~LayerFluidRuntimeState();

  LayerFluidRuntimeState(const LayerFluidRuntimeState&) = delete;
  LayerFluidRuntimeState& operator=(const LayerFluidRuntimeState&) = delete;
  LayerFluidRuntimeState(LayerFluidRuntimeState&&) = delete;
  LayerFluidRuntimeState& operator=(LayerFluidRuntimeState&&) = delete;

  void invalidateLiquidSurface();
  void invalidateLiquidSimulation();
  void invalidateSmokeSimulation();

  std::unique_ptr<ArtifactCore::FluidSolver2D> fluidSolver_;
  std::unique_ptr<ArtifactCore::LiquidSolver2D> liquidSolver_;
  ArtifactCore::NamedVector<LiquidLayerCheckpointEntry> liquidCheckpoints_{
      ArtifactCore::ContainerName{"Layer.LiquidCheckpoints"}};
  ArtifactCore::NamedVector<LiquidSpillParticle> liquidSpillParticles_{
      ArtifactCore::ContainerName{"Layer.LiquidSpillParticles"}};
  double liquidInflowCarry_ = 0.0;
  ArtifactCore::LiquidSurfaceSnapshot2D liquidSurfaceSnapshot_;
  std::int64_t liquidSurfaceFrame_ =
      std::numeric_limits<std::int64_t>::min();
  double liquidCheckpointFps_ = 0.0;
  std::uint64_t liquidCheckpointCompositionRevision_ = 0;
  ArtifactCore::NamedVector<ArtifactCore::ParticleVertex>
      fluidPreviewParticles_{
          ArtifactCore::ContainerName{"Layer.FluidPreviewParticles"}};
  std::int64_t fluidLastFrame_ =
      std::numeric_limits<std::int64_t>::min();
};

struct LayerSmokeRuntimeSettings {
  int gridWidth = 128;
  int gridHeight = 128;
  float viscosity = 0.00001f;
  float diffusion = 0.00001f;
  float buoyancy = 0.05f;
  float vorticity = 0.1f;
  int solverIterations = 20;
  int emitterCount = 16;
  float emitterSpeed = 120.0f;
};

void renderLayerSmokeRuntime(LayerFluidRuntimeState& state,
                             ArtifactIRenderer* renderer,
                             const QRectF& localBounds,
                             const QMatrix4x4& baseTransform,
                             float opacityScale,
                             std::int64_t frame,
                             double framesPerSecond,
                             const LayerSmokeRuntimeSettings& settings);

} // namespace Artifact
