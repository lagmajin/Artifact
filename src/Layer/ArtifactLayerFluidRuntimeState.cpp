module;

#include <limits>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

#include <QRectF>
#include <QVector3D>

module Artifact.Layer.FluidRuntimeState;

namespace Artifact {

namespace {
constexpr int kSmokeRenderSamplesPerAxis = 64;
constexpr int kSmokeSolverAdaptiveThresholdCells = 4608;
constexpr int kSmokeMaximumAdaptiveSolverIterations = 28;
constexpr std::int64_t kSmokeCheckpointInterval = 8;
constexpr std::int64_t kSmokeMaximumSeekReplayFrames = 120;
constexpr std::size_t kSmokeCheckpointMemoryBudget = 8U * 1024U * 1024U;
constexpr std::size_t kFluidSnapshotScalarArrayCount = 9;

std::uint64_t smokeCheckpointSettingsHash(
    const LayerSmokeRuntimeSettings& settings, const double framesPerSecond) {
  std::uint64_t hash = 1469598103934665603ULL;
  const auto mix = [&hash](const std::uint32_t value) {
    hash ^= value;
    hash *= 1099511628211ULL;
  };
  const auto mixFloat = [&mix](const float value) {
    mix(std::bit_cast<std::uint32_t>(value));
  };
  mix(static_cast<std::uint32_t>(settings.gridWidth));
  mix(static_cast<std::uint32_t>(settings.gridHeight));
  mixFloat(settings.viscosity);
  mixFloat(settings.diffusion);
  mixFloat(settings.buoyancy);
  mixFloat(settings.vorticity);
  mix(static_cast<std::uint32_t>(settings.solverIterations));
  mixFloat(settings.emitterX);
  mixFloat(settings.emitterY);
  mixFloat(settings.emitterRadius);
  mix(static_cast<std::uint32_t>(settings.emitterCount));
  mixFloat(settings.emitterDensity);
  mixFloat(settings.emitterVelocityX);
  mixFloat(settings.emitterVelocityY);
  mixFloat(settings.emitterSwirl);
  mix(static_cast<std::uint32_t>(settings.stirActive));
  mixFloat(settings.stirX);
  mixFloat(settings.stirY);
  mixFloat(settings.stirRadius);
  mixFloat(settings.stirVelocityX);
  mixFloat(settings.stirVelocityY);
  mixFloat(settings.stirStrength);
  mix(static_cast<std::uint32_t>(settings.stirRevision));
  mix(static_cast<std::uint32_t>(settings.stirRevision >> 32U));
  mixFloat(settings.densityDissipationPerSecond);
  mixFloat(settings.emitterTemperature);
  mixFloat(settings.thermalBuoyancy);
  mixFloat(settings.temperatureDissipationPerSecond);
  mix(static_cast<std::uint32_t>(settings.simulationStartFrame));
  mix(static_cast<std::uint32_t>(
      static_cast<std::uint64_t>(settings.simulationStartFrame) >> 32U));
  mix(static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(framesPerSecond)));
  mix(static_cast<std::uint32_t>(
      std::bit_cast<std::uint64_t>(framesPerSecond) >> 32U));
  return hash == 0 ? 1 : hash;
}

void storeSmokeCheckpoint(LayerFluidRuntimeState& state,
                          const std::int64_t frame,
                          const bool force = false) {
  if (!state.fluidSolver_ ||
      (!force && frame % kSmokeCheckpointInterval != 0)) return;
  const std::size_t bytes =
      state.fluidSolver_->width() * static_cast<std::size_t>(
          state.fluidSolver_->height()) * sizeof(float) *
      kFluidSnapshotScalarArrayCount;
  if (bytes > kSmokeCheckpointMemoryBudget) return;
  for (std::size_t index = 0; index < state.smokeCheckpoints_.size(); ++index) {
    if (state.smokeCheckpoints_[index].frame == frame) return;
  }
  // This full solver-state copy is the bounded exception that lets a later
  // seek reproduce sequential output. It runs every 8 simulation frames, is
  // capped at 8 MiB per layer, and currently allocates the snapshot payload.
  LayerFluidRuntimeState::SmokeLayerCheckpoint checkpoint;
  checkpoint.frame = frame;
  checkpoint.solver = state.fluidSolver_->snapshot();
  state.smokeCheckpointBytes_ += bytes;
  state.smokeCheckpoints_.push_back(std::move(checkpoint));
  while (state.smokeCheckpointBytes_ > kSmokeCheckpointMemoryBudget &&
         state.smokeCheckpoints_.size() > 1) {
    state.smokeCheckpointBytes_ -= std::min(
        state.smokeCheckpointBytes_,
        state.smokeCheckpoints_[0].solver.density.size() * sizeof(float) *
            kFluidSnapshotScalarArrayCount);
    state.smokeCheckpoints_.removeFirst();
  }
}

std::pair<int, int> smokeRenderSampleCounts(
    const int width, const int height) {
  const int safeWidth = std::max(1, width);
  const int safeHeight = std::max(1, height);
  const int longestAxis = std::max(safeWidth, safeHeight);
  if (longestAxis <= kSmokeRenderSamplesPerAxis) {
    return {safeWidth, safeHeight};
  }

  const float scale = static_cast<float>(kSmokeRenderSamplesPerAxis - 1) /
      static_cast<float>(longestAxis - 1);
  const int sampleCountX = safeWidth <= kSmokeRenderSamplesPerAxis
      ? safeWidth
      : std::clamp(static_cast<int>(std::round(
            static_cast<float>(safeWidth - 1) * scale)) + 1,
            1, kSmokeRenderSamplesPerAxis);
  const int sampleCountY = safeHeight <= kSmokeRenderSamplesPerAxis
      ? safeHeight
      : std::clamp(static_cast<int>(std::round(
            static_cast<float>(safeHeight - 1) * scale)) + 1,
            1, kSmokeRenderSamplesPerAxis);
  return {sampleCountX, sampleCountY};
}
}

LayerFluidRuntimeState::LayerFluidRuntimeState() {
  liquidCheckpoints_.reserve(256);
  smokeCheckpoints_.reserve(64);
}

LayerFluidRuntimeState::~LayerFluidRuntimeState() = default;

void LayerFluidRuntimeState::invalidateLiquidSurface() {
  liquidSurfaceSnapshot_ = {};
  liquidSurfaceFrame_ = std::numeric_limits<std::int64_t>::min();
}

void LayerFluidRuntimeState::invalidateLiquidSimulation() {
  liquidSolver_.reset();
  liquidCheckpoints_.clear();
  liquidSpillParticles_.clear();
  liquidSurfaceWorkspace_.validSamples.clear();
  liquidSurfaceSamples_.clear();
  liquidInflowCarry_ = 0.0;
  invalidateLiquidSurface();
  liquidCheckpointFps_ = 0.0;
  liquidCheckpointCompositionRevision_ = 0;
  fluidRenderData_.particles.clear();
  fluidLastFrame_ = std::numeric_limits<std::int64_t>::min();
}

void LayerFluidRuntimeState::invalidateSmokeSimulation() {
  fluidSolver_.reset();
  smokeCheckpoints_.clear();
  smokeCheckpointBytes_ = 0;
  smokeCheckpointSettingsHash_ = 0;
  fluidRenderData_.particles.clear();
  fluidLastFrame_ = std::numeric_limits<std::int64_t>::min();
  smokeStirRevision_ = 0;
}

void advanceLayerSmokeRuntime(
    LayerFluidRuntimeState& state, const std::int64_t frame,
    const double framesPerSecond,
    const LayerSmokeRuntimeSettings& settings) {
  const bool solverMismatch =
      !state.fluidSolver_ ||
      state.fluidSolver_->width() != settings.gridWidth ||
      state.fluidSolver_->height() != settings.gridHeight;
  if (solverMismatch) {
    state.fluidSolver_ = std::make_unique<ArtifactCore::FluidSolver2D>(
        settings.gridWidth, settings.gridHeight);
    const auto [samplesX, samplesY] = smokeRenderSampleCounts(
        settings.gridWidth, settings.gridHeight);
    state.fluidRenderData_.particles.reserve(
        static_cast<std::size_t>(samplesX) *
        static_cast<std::size_t>(samplesY));
    state.fluidLastFrame_ = std::numeric_limits<std::int64_t>::min();
    state.smokeStirRevision_ = 0;
    state.smokeCheckpoints_.clear();
    state.smokeCheckpointBytes_ = 0;
    state.smokeCheckpointSettingsHash_ = 0;
    state.fluidRenderData_.particles.clear();
  }
  if (!state.fluidSolver_) return;

  const std::uint64_t checkpointSettingsHash =
      smokeCheckpointSettingsHash(settings, framesPerSecond);
  const bool checkpointSettingsChanged =
      state.smokeCheckpointSettingsHash_ != 0 &&
      state.smokeCheckpointSettingsHash_ != checkpointSettingsHash;
  if (checkpointSettingsChanged) {
    state.smokeCheckpoints_.clear();
    state.smokeCheckpointBytes_ = 0;
  }
  state.smokeCheckpointSettingsHash_ = checkpointSettingsHash;

  state.fluidSolver_->setViscosity(settings.viscosity);
  state.fluidSolver_->setDiffusion(settings.diffusion);
  state.fluidSolver_->setBuoyancy(settings.buoyancy);
  state.fluidSolver_->setThermalBuoyancy(settings.thermalBuoyancy);
  state.fluidSolver_->setTemperatureDissipationPerSecond(
      settings.temperatureDissipationPerSecond);
  state.fluidSolver_->setVorticity(settings.vorticity);
  const int baseSolverIterations = std::max(1, settings.solverIterations);
  state.fluidSolver_->setSolverIterations(baseSolverIterations);
  state.fluidSolver_->setAdaptiveIterations(true);
  state.fluidSolver_->setHighResThresholdCells(
      kSmokeSolverAdaptiveThresholdCells);
  state.fluidSolver_->setMaxAdaptiveIterations(std::max(
      kSmokeMaximumAdaptiveSolverIterations, baseSolverIterations));

  const float dt = 1.0f / static_cast<float>(std::max(1.0, framesPerSecond));
  const int solverSubstepCount = std::max(1, static_cast<int>(
      std::ceil(dt / 0.05f)));
  const float solverSubstepDt = dt / static_cast<float>(solverSubstepCount);
  const float densityDissipationPerSecond = std::isfinite(
      settings.densityDissipationPerSecond)
      ? std::clamp(settings.densityDissipationPerSecond, 0.0f, 10.0f)
      : 2.5f;
  const float densityRetention = std::exp(-densityDissipationPerSecond * dt);
  constexpr float kSmokeReferenceFrameRate = 30.0f;
  const float emissionTimeScale = dt * kSmokeReferenceFrameRate;
  constexpr float kSmokeEmitterSway = 0.04f;
  const auto advanceSmoke = [&](const std::int64_t simulationFrame) {
    constexpr float kTwoPi = 6.28318530717958647692f;
    const float elapsedSeconds = static_cast<float>(simulationFrame) /
                                 static_cast<float>(
                                     std::max(1.0, framesPerSecond));
    const float swayPhase = elapsedSeconds * (kTwoPi * 0.22f);
    const int emitterCount = std::clamp(settings.emitterCount, 1, 64);
    const float sourceRadius = settings.emitterRadius /
        std::sqrt(static_cast<float>(emitterCount));
    const float placementRadius =
        std::max(0.0f, settings.emitterRadius - sourceRadius);
    constexpr float kGoldenAngle = 2.39996322972865332f;
    for (int source = 0; source < emitterCount; ++source) {
      float sourceX = settings.emitterX;
      float sourceY = settings.emitterY;
      if (emitterCount > 1) {
        const float radialFraction = std::sqrt(
            (static_cast<float>(source) + 0.5f) /
            static_cast<float>(emitterCount));
        const float angle = static_cast<float>(source) * kGoldenAngle;
        const float offset = placementRadius * radialFraction;
        sourceX += std::cos(angle) * offset;
        sourceY += std::sin(angle) * offset;
      }
      // The emitter's independent heat field drives buoyancy and the fire tint.
      const ArtifactCore::SmokeEmitter2D thermalEmitter{
          sourceX, sourceY, sourceRadius,
          settings.emitterDensity * emissionTimeScale,
          settings.emitterVelocityX +
              std::sin(swayPhase) * kSmokeEmitterSway,
          settings.emitterVelocityY, settings.emitterSwirl,
          settings.emitterTemperature * emissionTimeScale};
      injectSmokeEmitter2D(*state.fluidSolver_, thermalEmitter, 1.0f);
    }
    if (settings.stirActive) {
      injectSmokeStir2D(*state.fluidSolver_,
          ArtifactCore::SmokeStir2D{settings.stirX, settings.stirY,
              settings.stirRadius, settings.stirVelocityX,
              settings.stirVelocityY,
              settings.stirStrength * emissionTimeScale});
    }
    for (int substep = 0; substep < solverSubstepCount; ++substep) {
      state.fluidSolver_->update(solverSubstepDt);
    }
    state.fluidSolver_->scaleDensity(densityRetention);
  };

  bool steppedThisCall = false;
  const bool uninitialized =
      state.fluidLastFrame_ == std::numeric_limits<std::int64_t>::min();
  const bool discontinuous = !uninitialized &&
      (frame < state.fluidLastFrame_ || frame - state.fluidLastFrame_ > 8);
  if (uninitialized) {
    state.fluidSolver_->reset();
    const bool canReplayFromStart = frame >= settings.simulationStartFrame &&
        frame - settings.simulationStartFrame <=
            kSmokeMaximumSeekReplayFrames;
    const std::int64_t firstFrame = canReplayFromStart
        ? settings.simulationStartFrame
        : frame;
    state.fluidLastFrame_ = firstFrame - 1;
    for (std::int64_t replayFrame = firstFrame;
         replayFrame <= frame; ++replayFrame) {
      advanceSmoke(replayFrame);
      state.fluidLastFrame_ = replayFrame;
      storeSmokeCheckpoint(state, replayFrame);
    }
    steppedThisCall = true;
  } else if (discontinuous) {
    const std::int64_t previousFrame = state.fluidLastFrame_;
    std::size_t nearestCheckpointIndex = state.smokeCheckpoints_.size();
    std::int64_t nearestCheckpointFrame =
        std::numeric_limits<std::int64_t>::min();
    for (std::size_t index = 0; index < state.smokeCheckpoints_.size(); ++index) {
      const auto checkpointFrame = state.smokeCheckpoints_[index].frame;
      if (checkpointFrame <= frame && checkpointFrame > nearestCheckpointFrame) {
        nearestCheckpointFrame = checkpointFrame;
        nearestCheckpointIndex = index;
      }
    }
    bool restoredCheckpoint = false;
    if (nearestCheckpointIndex < state.smokeCheckpoints_.size() &&
        frame - nearestCheckpointFrame <= kSmokeMaximumSeekReplayFrames) {
      const auto& checkpoint = state.smokeCheckpoints_[nearestCheckpointIndex];
      restoredCheckpoint =
          state.fluidSolver_->restoreSnapshot(checkpoint.solver);
      if (restoredCheckpoint) state.fluidLastFrame_ = checkpoint.frame;
    }
    if (restoredCheckpoint) {
      if (frame < previousFrame) {
        while (!state.smokeCheckpoints_.empty() &&
               state.smokeCheckpoints_[state.smokeCheckpoints_.size() - 1].frame > frame) {
          const auto& removed =
              state.smokeCheckpoints_[state.smokeCheckpoints_.size() - 1];
          state.smokeCheckpointBytes_ -= std::min(
              state.smokeCheckpointBytes_,
              removed.solver.density.size() * sizeof(float) *
                  kFluidSnapshotScalarArrayCount);
          state.smokeCheckpoints_.removeAt(
              state.smokeCheckpoints_.size() - 1);
        }
      }
      for (std::int64_t replayFrame = state.fluidLastFrame_ + 1;
           replayFrame <= frame; ++replayFrame) {
        advanceSmoke(replayFrame);
        state.fluidLastFrame_ = replayFrame;
        storeSmokeCheckpoint(state, replayFrame);
        steppedThisCall = true;
      }
    } else if (frame > state.fluidLastFrame_ &&
               frame - state.fluidLastFrame_ <=
                   kSmokeMaximumSeekReplayFrames) {
      for (std::int64_t replayFrame = state.fluidLastFrame_ + 1;
           replayFrame <= frame; ++replayFrame) {
        advanceSmoke(replayFrame);
        state.fluidLastFrame_ = replayFrame;
        storeSmokeCheckpoint(state, replayFrame);
        steppedThisCall = true;
      }
    } else {
      state.fluidSolver_->reset();
      state.smokeCheckpoints_.clear();
      state.smokeCheckpointBytes_ = 0;
      state.fluidLastFrame_ = frame;
      advanceSmoke(frame);
      steppedThisCall = true;
    }
  } else if (frame > state.fluidLastFrame_) {
    for (std::int64_t replayFrame = state.fluidLastFrame_ + 1;
         replayFrame <= frame; ++replayFrame) {
      advanceSmoke(replayFrame);
      state.fluidLastFrame_ = replayFrame;
      storeSmokeCheckpoint(state, replayFrame);
      steppedThisCall = true;
    }
  }

  const bool newStirSample = settings.stirActive &&
      settings.stirRevision != state.smokeStirRevision_;
  if (newStirSample && !steppedThisCall) {
    injectSmokeStir2D(*state.fluidSolver_,
        ArtifactCore::SmokeStir2D{settings.stirX, settings.stirY,
            settings.stirRadius, settings.stirVelocityX,
            settings.stirVelocityY, settings.stirStrength});
    for (int substep = 0; substep < solverSubstepCount; ++substep) {
      state.fluidSolver_->update(solverSubstepDt);
    }
  }
  state.smokeStirRevision_ = settings.stirRevision;
  storeSmokeCheckpoint(
      state, state.fluidLastFrame_, checkpointSettingsChanged);
}

namespace {
const ArtifactCore::ParticleRenderData& prepareLayerSmokeRenderDataImpl(
    LayerFluidRuntimeState& state, const QRectF& bounds,
    const QMatrix4x4& baseTransform, const float opacityScale,
    const std::int64_t frame, const LayerSmokeRuntimeSettings& settings,
    const bool sampleVelocity) {
  auto& renderData = state.fluidRenderData_;
  renderData.particles.clear();
  renderData.frameNumber = frame;
  renderData.options.blend = ArtifactCore::ParticleBlendPolicy::Alpha;
  renderData.options.billboard =
      ArtifactCore::ParticleBillboardPolicy::ScreenAligned;
  renderData.options.falloff =
      ArtifactCore::ParticleFalloffMode::GaussianDensity;
  if (state.fluidSolver_ && bounds.isValid() && bounds.width() > 0.0 &&
      bounds.height() > 0.0) {
    const auto [sampleCountX, sampleCountY] = smokeRenderSampleCounts(
        settings.gridWidth, settings.gridHeight);
    const float sampleStepX = static_cast<float>(
        std::max(1, settings.gridWidth - 1)) /
        static_cast<float>(std::max(1, sampleCountX - 1));
    const float sampleStepY = static_cast<float>(
        std::max(1, settings.gridHeight - 1)) /
        static_cast<float>(std::max(1, sampleCountY - 1));
    const float sampleSpacingX =
        static_cast<float>(bounds.width()) /
        static_cast<float>(std::max(1, sampleCountX - 1));
    const float sampleSpacingY =
        static_cast<float>(bounds.height()) /
        static_cast<float>(std::max(1, sampleCountY - 1));
    const float sampleSpacing = std::max(sampleSpacingX, sampleSpacingY);
    const auto sampleSmokeFieldBilinear = [&](const float gridX,
        const float gridY, float& density, float& temperature,
        float& velocityX, float& velocityY) {
      const float clampedX = std::clamp(
          gridX, 0.0f, static_cast<float>(settings.gridWidth - 1));
      const float clampedY = std::clamp(
          gridY, 0.0f, static_cast<float>(settings.gridHeight - 1));
      const int x0 = static_cast<int>(std::floor(clampedX));
      const int y0 = static_cast<int>(std::floor(clampedY));
      const int x1 = std::min(x0 + 1, settings.gridWidth - 1);
      const int y1 = std::min(y0 + 1, settings.gridHeight - 1);
      const float tx = clampedX - static_cast<float>(x0);
      const float ty = clampedY - static_cast<float>(y0);
      float density00 = state.fluidSolver_->getDensity(x0, y0);
      float density10 = state.fluidSolver_->getDensity(x1, y0);
      float density01 = state.fluidSolver_->getDensity(x0, y1);
      float density11 = state.fluidSolver_->getDensity(x1, y1);
      const auto bilinear = [tx, ty](const float value00,
          const float value10, const float value01, const float value11) {
        return std::lerp(std::lerp(value00, value10, tx),
                         std::lerp(value01, value11, tx), ty);
      };
      density = bilinear(density00, density10, density01, density11);
      const float temperature00 = state.fluidSolver_->getTemperature(x0, y0);
      const float temperature10 = state.fluidSolver_->getTemperature(x1, y0);
      const float temperature01 = state.fluidSolver_->getTemperature(x0, y1);
      const float temperature11 = state.fluidSolver_->getTemperature(x1, y1);
      temperature = bilinear(
          temperature00, temperature10, temperature01, temperature11);
      if (sampleVelocity) {
        float velocityX00 = 0.0f;
        float velocityY00 = 0.0f;
        float velocityX10 = 0.0f;
        float velocityY10 = 0.0f;
        float velocityX01 = 0.0f;
        float velocityY01 = 0.0f;
        float velocityX11 = 0.0f;
        float velocityY11 = 0.0f;
        state.fluidSolver_->getVelocity(
            x0, y0, velocityX00, velocityY00);
        state.fluidSolver_->getVelocity(
            x1, y0, velocityX10, velocityY10);
        state.fluidSolver_->getVelocity(
            x0, y1, velocityX01, velocityY01);
        state.fluidSolver_->getVelocity(
            x1, y1, velocityX11, velocityY11);
        velocityX = bilinear(
            velocityX00, velocityX10, velocityX01, velocityX11);
        velocityY = bilinear(
            velocityY00, velocityY10, velocityY01, velocityY11);
      } else {
        velocityX = 0.0f;
        velocityY = 0.0f;
      }
    };
    for (int sampleY = 0; sampleY < sampleCountY; ++sampleY) {
      const float baseGridY = static_cast<float>(sampleY) * sampleStepY;
      for (int sampleX = 0; sampleX < sampleCountX; ++sampleX) {
        const float baseGridX = static_cast<float>(sampleX) * sampleStepX;
        std::uint32_t sampleHash =
            static_cast<std::uint32_t>(sampleX) * 0x9E3779B9u ^
            static_cast<std::uint32_t>(sampleY) * 0x85EBCA6Bu;
        sampleHash ^= sampleHash >> 16u;
        sampleHash *= 0x7FEB352Du;
        sampleHash ^= sampleHash >> 15u;
        const float jitterX =
            static_cast<float>(sampleHash & 0xFFFFu) *
                (2.0f / 65535.0f) - 1.0f;
        const float jitterY =
            static_cast<float>(sampleHash >> 16u) *
                (2.0f / 65535.0f) - 1.0f;
        constexpr float kSmokeSampleJitter = 0.30f;
        const float sampleGridX = std::clamp(
            baseGridX + jitterX * sampleStepX * kSmokeSampleJitter,
            0.0f, static_cast<float>(settings.gridWidth - 1));
        const float sampleGridY = std::clamp(
            baseGridY + jitterY * sampleStepY * kSmokeSampleJitter,
            0.0f, static_cast<float>(settings.gridHeight - 1));
        float density = 0.0f;
        float temperature = 0.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        sampleSmokeFieldBilinear(
            sampleGridX, sampleGridY, density, temperature, vx, vy);
        if (density < 0.025f) continue;
        ArtifactCore::ParticleVertex particle{};
        const float u = sampleGridX /
                        static_cast<float>(std::max(1, settings.gridWidth - 1));
        const float v = sampleGridY /
                        static_cast<float>(std::max(1, settings.gridHeight - 1));
        particle.px = static_cast<float>(bounds.left() + u * bounds.width());
        particle.py = static_cast<float>(bounds.top() + v * bounds.height());
        particle.pz = 0.0f;
        particle.ppx = particle.px;
        particle.ppy = particle.py;
        particle.ppz = particle.pz;
        particle.vx = vx * 24.0f;
        particle.vy = vy * 24.0f;
        particle.vz = 0.0f;
        const float fireMix = std::clamp(temperature * 0.5f, 0.0f, 1.0f);
        const float fireRed = 1.0f;
        const float fireGreen = std::clamp(0.16f + temperature * 0.24f,
                                           0.16f, 0.72f);
        const float fireBlue = std::clamp(0.015f + temperature * 0.035f,
                                          0.015f, 0.12f);
        particle.r = std::lerp(settings.colorR, fireRed, fireMix);
        particle.g = std::lerp(settings.colorG, fireGreen, fireMix);
        particle.b = std::lerp(settings.colorB, fireBlue, fireMix);
        particle.a = std::clamp(
            std::sqrt(density) * 0.18f, 0.01f, 0.18f) * 0.79f * 1.42f;
        particle.size = sampleSpacing *
            (0.118f + density * 0.02f) * 1.125f;
        particle.stretch = 1.0f;
        particle.rotation = 0.0f;
        particle.age = 0.0f;
        particle.lifetime = 1.0f;
        const QVector3D mapped = baseTransform.map(
            QVector3D(particle.px, particle.py, particle.pz));
        particle.px = mapped.x();
        particle.py = mapped.y();
        particle.pz = mapped.z();
        const QVector3D mappedPrev = baseTransform.map(
            QVector3D(particle.ppx, particle.ppy, particle.ppz));
        particle.ppx = mappedPrev.x();
        particle.ppy = mappedPrev.y();
        particle.ppz = mappedPrev.z();
        particle.a *= std::clamp(opacityScale, 0.0f, 1.0f);
        renderData.particles.push_back(particle);
      }
    }
  }

  return renderData;
}
} // namespace

const ArtifactCore::ParticleRenderData& prepareLayerSmokeRenderData(
    LayerFluidRuntimeState& state, const QRectF& bounds,
    const QMatrix4x4& baseTransform, const float opacityScale,
    const std::int64_t frame, const LayerSmokeRuntimeSettings& settings) {
  return prepareLayerSmokeRenderDataImpl(
      state, bounds, baseTransform, opacityScale, frame, settings, true);
}

void renderLayerSmokeRuntime(LayerFluidRuntimeState& state,
                             ArtifactIRenderer* renderer,
                             const QRectF& bounds,
                             const QMatrix4x4& baseTransform,
                             const float opacityScale,
                             const std::int64_t frame,
                             const double framesPerSecond,
                             const LayerSmokeRuntimeSettings& settings) {
  advanceLayerSmokeRuntime(state, frame, framesPerSecond, settings);
  if (!renderer || !state.fluidSolver_) return;
  const auto& renderData = prepareLayerSmokeRenderDataImpl(
      state, bounds, baseTransform, opacityScale, frame, settings, false);
  if (renderData.particles.empty()) return;
  renderer->drawParticles(std::move(state.fluidRenderData_));
}

} // namespace Artifact
