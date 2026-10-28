module;

#include <cstdint>
#include <cstddef>
#include <limits>
#include <memory>

#include <QMatrix4x4>
#include <QRectF>

export module Artifact.Layer.FluidRuntimeState;

import Container.NamedVector;
import Container.Debug;
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

  struct SmokeLayerCheckpoint {
    std::int64_t frame = 0;
    ArtifactCore::FluidSnapshot2D solver;
    ArtifactCore::NamedVector<float> flameLifetime{
        ArtifactCore::ContainerName{"Layer.SmokeCheckpointFlameLifetime"}};
    bool flameLifetimeActive = false;
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
  ArtifactCore::NamedVector<SmokeLayerCheckpoint> smokeCheckpoints_{
      ArtifactCore::ContainerName{"Layer.SmokeCheckpoints"}};
  ArtifactCore::NamedVector<float> flameLifetime_{
      ArtifactCore::ContainerName{"Layer.SmokeFlameLifetime"}};
  ArtifactCore::NamedVector<float> flameLifetimeScratch_{
      ArtifactCore::ContainerName{"Layer.SmokeFlameLifetimeScratch"}};
  bool flameLifetimeActive_ = false;
  ArtifactCore::NamedVector<LiquidSpillParticle> liquidSpillParticles_{
      ArtifactCore::ContainerName{"Layer.LiquidSpillParticles"}};
  double liquidInflowCarry_ = 0.0;
  ArtifactCore::LiquidSurfaceSnapshot2D liquidSurfaceSnapshot_;
  ArtifactCore::LiquidSurfaceWorkspace2D liquidSurfaceWorkspace_;
  ArtifactCore::NamedVector<ArtifactCore::LiquidSurfaceSample2D>
      liquidSurfaceSamples_{
          ArtifactCore::ContainerName{"Layer.LiquidSurfaceSamples"}};
  std::int64_t liquidSurfaceFrame_ =
      std::numeric_limits<std::int64_t>::min();
  double liquidCheckpointFps_ = 0.0;
  std::uint64_t liquidCheckpointCompositionRevision_ = 0;
  ArtifactCore::ParticleRenderData fluidRenderData_;
  ArtifactCore::ParticleRenderData fluidFireRenderData_;
  std::int64_t fluidLastFrame_ =
      std::numeric_limits<std::int64_t>::min();
  std::int64_t smokeSimulationStartFrame_ = 0;
  std::uint64_t smokeStirRevision_ = 0;
  std::size_t smokeCheckpointBytes_ = 0;
  std::uint64_t smokeCheckpointSettingsHash_ = 0;
};

struct LayerSmokeRuntimeSettings {
  int gridWidth = 128;
  int gridHeight = 128;
  float viscosity = 0.00001f;
  float diffusion = 0.00001f;
  float buoyancy = 0.05f;
  float vorticity = 0.1f;
  int solverIterations = 20;
  float emitterX = 0.5f;
  float emitterY = 0.9f;
  float emitterRadius = 0.10f;
  int emitterCount = 7;
  float emitterDensity = 0.16f;
  float emitterVelocityX = 0.0f;
  float emitterVelocityY = -1.0f;
  float emitterSwirl = 0.0f;
  float colorR = 0.42f;
  float colorG = 0.72f;
  float colorB = 1.0f;
  bool stirActive = false;
  float stirX = 0.5f;
  float stirY = 0.5f;
  float stirRadius = 0.035f;
  float stirVelocityX = 0.0f;
  float stirVelocityY = 0.0f;
  float stirStrength = 1.0f;
  std::uint64_t stirRevision = 0;
  float densityDissipationPerSecond = 2.5f;
  std::int64_t simulationStartFrame = 0;
  float emitterTemperature = 0.0f;
  float thermalBuoyancy = 0.0f;
  float temperatureDissipationPerSecond = 1.5f;
  float flameLifetimeSeconds = 0.65f;
  float topOutflowRatePerSecond = 100.0f;
  float topOutflowBandFraction = 0.35f;
  float volumeDetailStrength = 0.0f;
  float smokeTurbulence = 0.0f;
  float flameTurbulence = 0.0f;
  float smokeTurbulenceScale = 18.0f;
  float flameTurbulenceScale = 7.0f;
  int smokeTurbulenceOctaves = 1;
  bool highOrderDensityAdvection = false;
  float smokeDenseColorR = 0.20f;
  float smokeDenseColorG = 0.24f;
  float smokeDenseColorB = 0.29f;
  float smokeDensityRampStart = 0.12f;
  float smokeDensityRampEnd = 0.45f;
  float smokeDensityRampStrength = 0.0f;
  float smokeScatteringBrightness = 1.0f;
  float smokeShadowDensity = 0.8f;
  float smokeVolumeDetailFrequency = 0.42f;
  float smokeVolumeDetailContrast = 0.55f;
};

void advanceLayerSmokeRuntime(LayerFluidRuntimeState& state,
                              std::int64_t frame,
                              double framesPerSecond,
                              const LayerSmokeRuntimeSettings& settings);
const ArtifactCore::ParticleRenderData& prepareLayerSmokeRenderData(
    LayerFluidRuntimeState& state, const QRectF& localBounds,
    const QMatrix4x4& baseTransform, float opacityScale,
    std::int64_t frame, const LayerSmokeRuntimeSettings& settings);
const ArtifactCore::ParticleRenderData& prepareLayerFireRenderData(
    LayerFluidRuntimeState& state, const QRectF& localBounds,
    const QMatrix4x4& baseTransform, float opacityScale,
    std::int64_t frame, const LayerSmokeRuntimeSettings& settings);
void renderLayerSmokeRuntime(LayerFluidRuntimeState& state,
                             ArtifactIRenderer* renderer,
                             const QRectF& localBounds,
                             const QMatrix4x4& baseTransform,
                             float opacityScale,
                             std::int64_t frame,
                             double framesPerSecond,
                             const LayerSmokeRuntimeSettings& settings);

} // namespace Artifact
