module;

#include <limits>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

#include <QRectF>
#include <QVector3D>

module Artifact.Layer.FluidRuntimeState;

namespace Artifact {

namespace {
constexpr int kSmokeRenderSamplesPerAxis = 40;
}

LayerFluidRuntimeState::LayerFluidRuntimeState() {
  liquidCheckpoints_.reserve(256);
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
    const std::size_t samplesX = static_cast<std::size_t>(std::clamp(
        settings.gridWidth, 1, kSmokeRenderSamplesPerAxis));
    const std::size_t samplesY = static_cast<std::size_t>(std::clamp(
        settings.gridHeight, 1, kSmokeRenderSamplesPerAxis));
    state.fluidRenderData_.particles.reserve(samplesX * samplesY);
    state.fluidLastFrame_ = std::numeric_limits<std::int64_t>::min();
    state.smokeStirRevision_ = 0;
    state.fluidRenderData_.particles.clear();
  }
  if (!state.fluidSolver_) return;

  state.fluidSolver_->setViscosity(settings.viscosity);
  state.fluidSolver_->setDiffusion(settings.diffusion);
  state.fluidSolver_->setBuoyancy(settings.buoyancy);
  state.fluidSolver_->setVorticity(settings.vorticity);
  state.fluidSolver_->setSolverIterations(settings.solverIterations);

  const float dt = 1.0f / static_cast<float>(std::max(1.0, framesPerSecond));
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
      const ArtifactCore::SmokeEmitter2D emitter{
          sourceX, sourceY, sourceRadius,
          settings.emitterDensity * emissionTimeScale,
          settings.emitterVelocityX +
              std::sin(swayPhase) * kSmokeEmitterSway,
          settings.emitterVelocityY, settings.emitterSwirl};
      injectSmokeEmitter2D(*state.fluidSolver_, emitter, emissionTimeScale);
    }
    if (settings.stirActive) {
      injectSmokeStir2D(*state.fluidSolver_,
          ArtifactCore::SmokeStir2D{settings.stirX, settings.stirY,
              settings.stirRadius, settings.stirVelocityX,
              settings.stirVelocityY,
              settings.stirStrength * emissionTimeScale});
    }
    state.fluidSolver_->update(dt);
    state.fluidSolver_->scaleDensity(densityRetention);
  };

  bool steppedThisCall = false;
  if (state.fluidLastFrame_ == std::numeric_limits<std::int64_t>::min() ||
      frame < state.fluidLastFrame_ || frame - state.fluidLastFrame_ > 10) {
    state.fluidSolver_->reset();
    state.fluidLastFrame_ = frame;
    advanceSmoke(frame);
    steppedThisCall = true;
  } else if (frame > state.fluidLastFrame_) {
    const int stepCount = static_cast<int>(std::min<std::int64_t>(
        frame - state.fluidLastFrame_, 8));
    for (int step = 0; step < stepCount; ++step) {
      advanceSmoke(state.fluidLastFrame_ + step + 1);
      steppedThisCall = true;
    }
    state.fluidLastFrame_ = frame;
  }

  const bool newStirSample = settings.stirActive &&
      settings.stirRevision != state.smokeStirRevision_;
  if (newStirSample && !steppedThisCall) {
    injectSmokeStir2D(*state.fluidSolver_,
        ArtifactCore::SmokeStir2D{settings.stirX, settings.stirY,
            settings.stirRadius, settings.stirVelocityX,
            settings.stirVelocityY, settings.stirStrength});
    state.fluidSolver_->update(dt);
  }
  state.smokeStirRevision_ = settings.stirRevision;
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
  if (state.fluidSolver_ && bounds.isValid() && bounds.width() > 0.0 &&
      bounds.height() > 0.0) {
    const int sampleCountX = std::clamp(
        settings.gridWidth, 1, kSmokeRenderSamplesPerAxis);
    const int sampleCountY = std::clamp(
        settings.gridHeight, 1, kSmokeRenderSamplesPerAxis);
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
        const float gridY, float& density, float& velocityX,
        float& velocityY) {
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
        float vx = 0.0f;
        float vy = 0.0f;
        sampleSmokeFieldBilinear(
            sampleGridX, sampleGridY, density, vx, vy);
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
        particle.r = settings.colorR;
        particle.g = settings.colorG;
        particle.b = settings.colorB;
        particle.a = std::clamp(
            std::sqrt(density) * 0.18f, 0.01f, 0.18f) * 0.79f;
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
