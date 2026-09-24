module;

#include <limits>
#include <algorithm>
#include <cmath>

#include <QRectF>
#include <QVector3D>

module Artifact.Layer.FluidRuntimeState;

namespace Artifact {

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
  liquidInflowCarry_ = 0.0;
  invalidateLiquidSurface();
  liquidCheckpointFps_ = 0.0;
  liquidCheckpointCompositionRevision_ = 0;
  fluidPreviewParticles_.clear();
  fluidLastFrame_ = std::numeric_limits<std::int64_t>::min();
}

void LayerFluidRuntimeState::invalidateSmokeSimulation() {
  fluidSolver_.reset();
  fluidPreviewParticles_.clear();
  fluidLastFrame_ = std::numeric_limits<std::int64_t>::min();
}

void renderLayerSmokeRuntime(LayerFluidRuntimeState& state,
                             ArtifactIRenderer* renderer,
                             const QRectF& bounds,
                             const QMatrix4x4& baseTransform,
                             const float opacityScale,
                             const std::int64_t frame,
                             const double framesPerSecond,
                             const LayerSmokeRuntimeSettings& settings) {
  if (!renderer) return;
  const bool solverMismatch =
      !state.fluidSolver_ ||
      state.fluidSolver_->width() != settings.gridWidth ||
      state.fluidSolver_->height() != settings.gridHeight;
  if (solverMismatch) {
    state.fluidSolver_ = std::make_unique<ArtifactCore::FluidSolver2D>(
        settings.gridWidth, settings.gridHeight);
    state.fluidLastFrame_ = std::numeric_limits<std::int64_t>::min();
    state.fluidPreviewParticles_.clear();
  }
  if (!state.fluidSolver_) return;

  state.fluidSolver_->setViscosity(settings.viscosity);
  state.fluidSolver_->setDiffusion(settings.diffusion);
  state.fluidSolver_->setBuoyancy(settings.buoyancy);
  state.fluidSolver_->setVorticity(settings.vorticity);
  state.fluidSolver_->setSolverIterations(settings.solverIterations);

  if (state.fluidLastFrame_ == std::numeric_limits<std::int64_t>::min() ||
      frame < state.fluidLastFrame_ || frame - state.fluidLastFrame_ > 10) {
    state.fluidSolver_->reset();
    state.fluidLastFrame_ = frame;
  } else if (frame > state.fluidLastFrame_) {
    const int stepCount = static_cast<int>(std::min<std::int64_t>(
        frame - state.fluidLastFrame_, 8));
    const float dt = 1.0f / static_cast<float>(std::max(1.0, framesPerSecond));
    for (int step = 0; step < stepCount; ++step) {
      const int centerX = settings.gridWidth / 2;
      const int centerY = std::max(1, settings.gridHeight - 3);
      const float phase = static_cast<float>(state.fluidLastFrame_ + step) *
                          0.07f;
      const float swirlX = std::sin(phase) * 0.85f;
      const float injectDensity =
          1.0f + static_cast<float>(std::max(0, settings.emitterCount)) / 24.0f;
      const float injectVelocity =
          std::max(40.0f, settings.emitterSpeed) * 0.02f;
      state.fluidSolver_->addDensity(centerX, centerY, injectDensity);
      state.fluidSolver_->addVelocity(centerX, centerY, swirlX,
                                      -injectVelocity);
      state.fluidSolver_->update(dt);
    }
    state.fluidLastFrame_ = frame;
  }

  state.fluidPreviewParticles_.clear();
  if (bounds.isValid() && bounds.width() > 0.0 && bounds.height() > 0.0) {
    const int strideX = std::max(1, settings.gridWidth / 28);
    const int strideY = std::max(1, settings.gridHeight / 28);
    for (int gy = 0; gy < settings.gridHeight; gy += strideY) {
      for (int gx = 0; gx < settings.gridWidth; gx += strideX) {
        const float density = state.fluidSolver_->getDensity(gx, gy);
        if (density < 0.025f) continue;
        ArtifactCore::ParticleVertex particle{};
        const float u = static_cast<float>(gx) /
                        static_cast<float>(std::max(1, settings.gridWidth - 1));
        const float v = static_cast<float>(gy) /
                        static_cast<float>(std::max(1, settings.gridHeight - 1));
        particle.px = static_cast<float>(bounds.left() + u * bounds.width());
        particle.py = static_cast<float>(bounds.top() + v * bounds.height());
        particle.pz = 0.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        state.fluidSolver_->getVelocity(gx, gy, vx, vy);
        particle.vx = vx * 24.0f;
        particle.vy = vy * 24.0f;
        particle.vz = 0.0f;
        particle.r = 0.42f;
        particle.g = 0.72f;
        particle.b = 1.0f;
        particle.a = std::clamp(density * 0.45f, 0.04f, 0.65f);
        particle.size = 2.0f + density * 9.0f;
        particle.stretch =
            1.0f + std::min(std::sqrt(vx * vx + vy * vy), 2.5f);
        particle.rotation = std::atan2(vy, vx);
        particle.age = 0.0f;
        particle.lifetime = 1.0f;
        state.fluidPreviewParticles_.push_back(particle);
      }
    }
  }

  if (state.fluidPreviewParticles_.empty()) return;
  ArtifactCore::ParticleRenderData renderData;
  renderData.frameNumber = frame;
  renderData.options.blend = ArtifactCore::ParticleBlendPolicy::Additive;
  renderData.options.billboard =
      ArtifactCore::ParticleBillboardPolicy::VelocityAligned;
  renderData.particles.reserve(state.fluidPreviewParticles_.size());
  for (const auto& sourceParticle : state.fluidPreviewParticles_) {
    auto particle = sourceParticle;
    const QVector3D mapped = baseTransform.map(
        QVector3D(particle.px, particle.py, particle.pz));
    particle.px = mapped.x();
    particle.py = mapped.y();
    particle.pz = mapped.z();
    particle.a *= std::clamp(opacityScale, 0.0f, 1.0f);
    renderData.particles.push_back(particle);
  }
  renderer->drawParticles(renderData);
}

} // namespace Artifact
