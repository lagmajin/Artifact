module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>
#include <QDebug>
#include <QImage>
#include <QMatrix4x4>
#include <QPointF>
#include <QSet>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QTransform>
#include <QVector>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QString>
#include <QStringList>
#include <wobjectcpp.h>
#include <wobjectimpl.h>

// JSON and QVariant used in serialization
#include <QColor>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariant>
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <limits>

module Artifact.Layer.Abstract;

import :Impl;
import Memory.SharedPtr;
import Utils;
import Layer.State;
import Animation.Transform2D;
import Animation.Dynamics;
import Frame.Position;
import Time.Rational;
import Frame.Rate;
import Artifact.Render.IRenderer;
import Animation.Value;
import Transform.Hlper;

import Time.TimeRemap;

import Artifact.Layer.Settings;
import Artifact.Layer.Physics;
import Artifact.Layer.Component.System;
import Artifact.Layer.Modifier;
import Artifact.Layer.Matte;
import Artifact.Layer.MaskMatteState;
import Artifact.Layer.Serialization;
import Artifact.Layer.ThumbnailSupport;
import Color.Float;
import Geometry.Fracture;
import Physics.Fluid;
import Physics.SoftBody;
import Physics2D;
import Layer.Matte;
import Artifact.Composition.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Render.ROI;
import Artifact.Effect.ImplBase;
import Artifact.Effect.Keying.ChromaKey;
import Artifact.Effect.Keying.LumaKey;
import Artifact.Effect.Keying.DifferenceKey;
import Artifact.Effect.Rasterizer.DifferenceMatte;
import Artifact.Effect.Rasterizer.PosterizeTime;
import Artifact.Effect.Keying.IBKKeyer;
import Artifact.Effect.Generator.Cloner;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Container.NamedVector;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import Graphics.ParticleData;
import Property.Abstract;
import Property.Group;
import Property.SerializationBridge;
import Script.Expression.Evaluator;
import Audio.Modulation.Router;
import Artifact.Event.Types;
import Event.Bus;
import Artifact.Layer.RuntimeSupport;
import Artifact.Layer.RuntimeRenderSupport;
import Artifact.Layer.FluidRuntimeState;
import Artifact.Layer.Abstract.Utilities;
import Artifact.Layer.PhysicsBridge;

namespace Artifact {
using namespace ArtifactCore;
using LayerAbstractUtilities::finiteClampedValue;
using namespace LayerAbstractUtilities;
namespace LayerPhysics = LayerPhysicsBridge;

void notifyLayerMutation(ArtifactAbstractLayer* layer, LayerDirtyFlag flag,
                         LayerDirtyReason reason);
QVariant evaluateAnimatedPropertyValue(
    const ArtifactCore::AbstractProperty& property,
    const ArtifactCore::RationalTime& time);
float applyAutomationClipOverlay(const ArtifactAbstractLayer* layer,
                                 const QString& targetPath, float baseValue,
                                 double parentSeconds, double freeSeconds);
void applyCompositionTransformFields(const ArtifactAbstractLayer* layer,
                                     double& positionX, double& positionY,
                                     double& scaleX, double& scaleY);
double effectiveLayerFrameRate(const ArtifactAbstractLayer* layer);
int64_t currentTimelineFrame(const ArtifactAbstractLayer* layer);
RationalTime currentTimelineTime(const ArtifactAbstractLayer* layer);
RationalTime timelineTimeForFramePosition(const ArtifactAbstractLayer* layer,
                                          const FramePosition& position);
bool configureLiquidContainerPolygon(
    const ArtifactAbstractLayer* layer, ArtifactCore::LiquidSolver2D& liquid,
    int requestedOpeningEdge = -1,
    NamedVector<QPointF>* configuredPoints = nullptr,
    std::size_t* configuredOpeningEdge = nullptr);
QRectF layerCollisionLocalBounds(const ArtifactAbstractLayer* layer);
bool resolveLiquidPointAgainstCollisionLayer(
    const ArtifactAbstractLayer* layer, int64_t frameNumber,
    float particleRadius, float previousWorldX, float previousWorldY,
    float& worldX, float& worldY, float& worldVx, float& worldVy,
    float& collisionImpact);
QJsonObject serializeLayerModulationRouter(
    const Audio::Modulation::ModulationRouter& router);
void restoreLayerModulationRouter(
    const QJsonObject& object, Audio::Modulation::ModulationRouter& router);
QJsonObject serializeLayerTransform(
    const ArtifactCore::AnimatableTransform3D& transform);
void restoreLayerTransform(const QJsonObject& object,
                           ArtifactCore::AnimatableTransform3D& transform,
                           double frameRate);

void applyFragmentFieldsAndFloorCollision(
    const ArtifactAbstractLayer* layer, FractureShardMotion& shard,
    const QMatrix4x4& baseTransform, float deltaSeconds);
void syncFragmentDataset(const ArtifactAbstractLayer* layer,
                         const FractureState& fractureState,
                         const FractureResult* prefractureResult,
                         LayerEvaluationState& evaluationState);

const LayerEvaluationState& ArtifactAbstractLayer::layerEvaluationState() const {
  return impl_->componentEvaluationState_;
}

void ArtifactAbstractLayer::drawFractureOverlay(ArtifactIRenderer* renderer,
                                                const QMatrix4x4& baseTransform,
                                                const QSizeF& sourceSize,
                                                float opacityScale,
                                                Diligent::ITextureView* sourceTexture) {
  if (!renderer) {
    return;
  }
  Q_UNUSED(sourceSize);

  const int64_t frame = currentTimelineFrame(this);
  const bool discontinuousFractureFrame =
      impl_->fractureMotionLastFrame_ != std::numeric_limits<int64_t>::min() &&
      frame != impl_->fractureMotionLastFrame_ + 1 &&
      frame != impl_->fractureMotionLastFrame_;
  if (discontinuousFractureFrame) {
    resetFractureState();
  }
  if (impl_->fractureEnabled_ && impl_->fractureTriggerFrame_ >= 0) {
    if (frame < impl_->fractureTriggerFrame_) {
      if (impl_->fractureTriggerLastFrame_ >= impl_->fractureTriggerFrame_) {
        resetFractureState();
      }
      impl_->fractureTriggerLastFrame_ = frame;
    } else if (impl_->fractureTriggerLastFrame_ < impl_->fractureTriggerFrame_) {
      FractureImpact triggerImpact;
      triggerImpact.impulse = std::max(
          1.0f, impl_->fractureShatterThreshold_ * 1.25f);
      triggerImpact.stress = triggerImpact.impulse;
      triggerImpact.speed = triggerImpact.impulse;
      applyFractureImpact(triggerImpact);
      impl_->fractureTriggerLastFrame_ = frame;
    }
  }
  FractureRenderElement fractureElement;
  const bool resetMotionTrail =
      impl_->motionTrailLastFrame_ == std::numeric_limits<int64_t>::min() ||
      frame < impl_->motionTrailLastFrame_ ||
      frame - impl_->motionTrailLastFrame_ > 1;
  if (resetMotionTrail) {
    impl_->motionTrailHistory_.clear();
  }
  const bool appendMotionTrailSample =
      impl_->motionTrailLastFrame_ != frame;
  if (appendMotionTrailSample) {
    impl_->motionTrailLastFrame_ = frame;
  }
  const auto appendAndDrawMotionTrail =
      [&](const QString& entityKey, const QVector3D& position, float alpha) {
        if (!impl_->motionTrailEnabled_) {
          return;
        }
        auto& history = impl_->motionTrailHistory_[entityKey];
        const std::size_t capacity = static_cast<std::size_t>(
            std::clamp(impl_->motionTrailLength_, 2, 256));
        if (appendMotionTrailSample) {
          history.push(position, capacity);
        }
        if (history.count < 2) {
          return;
        }
        const float opacity = std::clamp(alpha * opacityScale, 0.0f, 1.0f);
        for (std::size_t sampleIndex = 1; sampleIndex < history.count;
             ++sampleIndex) {
          const std::size_t previous =
              (history.head + sampleIndex - 1) % history.samples.size();
          const std::size_t current =
              (history.head + sampleIndex) % history.samples.size();
          const float age = 1.0f - static_cast<float>(sampleIndex) /
                                      static_cast<float>(history.count);
          const float segmentAlpha = opacity * impl_->motionTrailFade_ * age;
          renderer->drawSolidLine(
              {history.samples[previous].x(), history.samples[previous].y()},
              {history.samples[current].x(), history.samples[current].y()},
              FloatColor(0.48f, 0.84f, 1.0f, segmentAlpha),
              std::max(0.1f, impl_->motionTrailWidth_));
        }
      };
  const QVector3D layerTrailPosition = baseTransform.map(
      QVector3D(static_cast<float>(localBounds().center().x()),
                static_cast<float>(localBounds().center().y()), 0.0f));
  appendAndDrawMotionTrail(QStringLiteral("layer"), layerTrailPosition, 1.0f);
  if (impl_->fluidComponentEnabled_) {
    const double fps = std::max(1.0, effectiveLayerFrameRate(this));
    if (impl_->fluidMode_ == 1) {
      impl_->fluidRuntime_.fluidSolver_.reset();
      const auto* liquidComposition =
          dynamic_cast<const ArtifactAbstractComposition*>(compositionObject());
      const uint64_t compositionRevision =
          liquidComposition ? liquidComposition->revision() : 0;
      if (!impl_->fluidRuntime_.liquidSolver_) {
        impl_->fluidRuntime_.liquidSolver_ =
            std::make_unique<ArtifactCore::LiquidSolver2D>();
        impl_->fluidRuntime_.fluidLastFrame_ = std::numeric_limits<int64_t>::min();
        impl_->fluidRuntime_.liquidCheckpoints_.clear();
        impl_->fluidRuntime_.liquidSpillParticles_.clear();
        impl_->fluidRuntime_.liquidInflowCarry_ = 0.0;
        impl_->fluidRuntime_.liquidSurfaceSnapshot_ = {};
        impl_->fluidRuntime_.liquidSurfaceFrame_ = std::numeric_limits<int64_t>::min();
        impl_->fluidRuntime_.liquidCheckpointFps_ = fps;
        impl_->fluidRuntime_.liquidCheckpointCompositionRevision_ = compositionRevision;
        impl_->fluidRuntime_.fluidPreviewParticles_.clear();
      }
      auto& liquid = *impl_->fluidRuntime_.liquidSolver_;
      NamedVector<QPointF> liquidContainerPoints{
          ContainerName{"Layer.LiquidContainerPoints"}};
      std::size_t liquidContainerOpeningEdge = 0;
      const bool hasPolygonLiquidContainer = configureLiquidContainerPolygon(
          this, liquid, impl_->liquidOpeningEdge_, &liquidContainerPoints,
          &liquidContainerOpeningEdge);
      if (std::abs(impl_->fluidRuntime_.liquidCheckpointFps_ - fps) > 1.0e-6 ||
          impl_->fluidRuntime_.liquidCheckpointCompositionRevision_ != compositionRevision) {
        impl_->fluidRuntime_.liquidCheckpoints_.clear();
        impl_->fluidRuntime_.liquidSpillParticles_.clear();
        impl_->fluidRuntime_.liquidInflowCarry_ = 0.0;
        impl_->fluidRuntime_.liquidSurfaceSnapshot_ = {};
        impl_->fluidRuntime_.liquidSurfaceFrame_ = std::numeric_limits<int64_t>::min();
        impl_->fluidRuntime_.fluidLastFrame_ = std::numeric_limits<int64_t>::min();
        impl_->fluidRuntime_.liquidCheckpointFps_ = fps;
        impl_->fluidRuntime_.liquidCheckpointCompositionRevision_ = compositionRevision;
      }
      liquid.setViscosity(impl_->fluidViscosity_);
      liquid.setSurfaceTension(impl_->liquidSurfaceTension_);
      liquid.setSubsteps(impl_->liquidSubsteps_);
      liquid.setSolverIterations(
          std::clamp(impl_->fluidSolverIterations_, 1, 12));

      const auto setGravityForFrame = [&](int64_t simulationFrame) {
        bool invertible = false;
        const QTransform inverseTransform =
            getGlobalTransformAt(simulationFrame).inverted(&invertible);
        QPointF localDown(0.0, 1.0);
        if (invertible) {
          const QPointF localOrigin = inverseTransform.map(QPointF(0.0, 0.0));
          const QPointF localWorldDown =
              inverseTransform.map(QPointF(0.0, 1.0));
          localDown = localWorldDown - localOrigin;
        }
        const double length = std::hypot(localDown.x(), localDown.y());
        if (!std::isfinite(length) || length < 1.0e-8) {
          localDown = QPointF(0.0, 1.0);
        } else {
          localDown /= length;
        }
        liquid.setGravity(
            static_cast<float>(localDown.x()) * impl_->liquidGravity_,
            static_cast<float>(localDown.y()) * impl_->liquidGravity_);
      };

      const auto storeCheckpoint = [&](int64_t checkpointFrame) {
        constexpr int64_t checkpointInterval = 30;
        constexpr std::size_t maxCheckpoints = 256;
        if (checkpointFrame < 0 || checkpointFrame % checkpointInterval != 0) {
          return;
        }
        LayerFluidRuntimeState::LiquidLayerCheckpoint snapshot{
            liquid.snapshot(), impl_->fluidRuntime_.liquidSpillParticles_,
            impl_->fluidRuntime_.liquidInflowCarry_};
        std::size_t insertionIndex = impl_->fluidRuntime_.liquidCheckpoints_.size();
        bool replaced = false;
        for (std::size_t i = 0; i < impl_->fluidRuntime_.liquidCheckpoints_.size(); ++i) {
          auto& entry = impl_->fluidRuntime_.liquidCheckpoints_[i];
          if (entry.frame == checkpointFrame) {
            entry.checkpoint = std::move(snapshot);
            replaced = true;
            break;
          }
          if (entry.frame > checkpointFrame) {
            insertionIndex = i;
            break;
          }
        }
        if (replaced) {
          // The existing checkpoint retains its sorted position.
        } else if (insertionIndex < impl_->fluidRuntime_.liquidCheckpoints_.size()) {
          impl_->fluidRuntime_.liquidCheckpoints_.insert(
              insertionIndex,
              LayerFluidRuntimeState::LiquidLayerCheckpointEntry{checkpointFrame,
                                                std::move(snapshot)});
        } else {
          impl_->fluidRuntime_.liquidCheckpoints_.add(
              LayerFluidRuntimeState::LiquidLayerCheckpointEntry{checkpointFrame,
                                                std::move(snapshot)});
        }
        while (impl_->fluidRuntime_.liquidCheckpoints_.size() > maxCheckpoints) {
          const std::size_t removeIndex =
              impl_->fluidRuntime_.liquidCheckpoints_[0].frame == 0 ? 1U : 0U;
          if (!impl_->fluidRuntime_.liquidCheckpoints_.removeAt(removeIndex)) break;
        }
      };

      const int64_t targetFrame = std::max<int64_t>(0, frame);
      const bool randomAccess =
          impl_->fluidRuntime_.fluidLastFrame_ == std::numeric_limits<int64_t>::min() ||
          targetFrame < impl_->fluidRuntime_.fluidLastFrame_ ||
          targetFrame - impl_->fluidRuntime_.fluidLastFrame_ > 10;
      const float dt = 1.0f / static_cast<float>(fps);
      QRectF liquidSpillCullBounds;
      const QSizeF liquidCompositionSize = compositionSizeHint();
      if (impl_->liquidSpillCullMargin_ > 0.0f &&
          liquidCompositionSize.isValid() &&
          liquidCompositionSize.width() > 0.0 &&
          liquidCompositionSize.height() > 0.0) {
        const qreal margin = impl_->liquidSpillCullMargin_;
        liquidSpillCullBounds =
            QRectF(QPointF(0.0, 0.0), liquidCompositionSize)
                .adjusted(-margin, -margin, margin, margin);
      }
      const auto advanceLiquidFrame = [&](int64_t simulationFrame) {
        impl_->fluidRuntime_.liquidInflowCarry_ +=
            static_cast<double>(impl_->liquidInflowRate_) * dt;
        const double wholeInflow = std::floor(impl_->fluidRuntime_.liquidInflowCarry_);
        const auto requestedInflow = static_cast<std::size_t>(
            std::min(wholeInflow, 4096.0));
        impl_->fluidRuntime_.liquidInflowCarry_ -= wholeInflow;
        if (requestedInflow > 0) {
          liquid.emitFromOpening(
              requestedInflow, impl_->liquidInflowWidth_,
              impl_->liquidInflowSpeed_, impl_->liquidInflowPosition_);
        }
        const float impactRetention = std::exp(-6.0f * dt);
        for (auto& spill : impl_->fluidRuntime_.liquidSpillParticles_) {
          spill.collisionImpact *= impactRetention;
          if (!std::isfinite(spill.collisionImpact) ||
              spill.collisionImpact < 0.0f) {
            spill.collisionImpact = 0.0f;
          }
        }
        ArtifactCore::LiquidSolver2D::applySpillInteractions(
            impl_->fluidRuntime_.liquidSpillParticles_, dt,
            impl_->liquidSurfaceTension_, impl_->fluidViscosity_);
        for (auto& spill : impl_->fluidRuntime_.liquidSpillParticles_) {
          spill.previousX = spill.x;
          spill.previousY = spill.y;
          spill.vy += spill.gravityY * dt;
          spill.x += spill.vx * dt;
          spill.y += spill.vy * dt;
        }
        impl_->fluidRuntime_.liquidSpillParticles_.removeIf(
            [&liquidSpillCullBounds](
                const LayerFluidRuntimeState::LiquidSpillParticle& spill) {
                  constexpr float worldLimit = 10000000.0f;
                  const bool outsideCullBounds =
                      liquidSpillCullBounds.isValid() &&
                      !liquidSpillCullBounds.contains(spill.x, spill.y);
                  return !std::isfinite(spill.x) || !std::isfinite(spill.y) ||
                         !std::isfinite(spill.vx) || !std::isfinite(spill.vy) ||
                         !std::isfinite(spill.collisionImpact) ||
                         std::abs(spill.x) > worldLimit ||
                         std::abs(spill.y) > worldLimit ||
                         outsideCullBounds;
                });
        if (liquidComposition) {
          const auto& collisionLayers = liquidComposition->allLayerRef();
          for (auto& spill : impl_->fluidRuntime_.liquidSpillParticles_) {
            for (const auto& collisionLayer : collisionLayers) {
              if (!collisionLayer || collisionLayer.get() == this) continue;
              if (resolveLiquidPointAgainstCollisionLayer(
                      collisionLayer.get(), simulationFrame,
                      spill.size * 0.5f, spill.previousX, spill.previousY,
                      spill.x, spill.y, spill.vx, spill.vy,
                      spill.collisionImpact)) {
                spill.previousX = spill.x;
                spill.previousY = spill.y;
              }
            }
          }
        }

        setGravityForFrame(simulationFrame);
        liquid.update(dt);
        auto escaped = liquid.takeEscapedParticles();
        if (escaped.empty()) return;

        const QRectF bounds = localBounds();
        if (!bounds.isValid() || bounds.width() <= 0.0 ||
            bounds.height() <= 0.0) {
          return;
        }
        const QTransform frameTransform =
            getGlobalTransformAt(simulationFrame);
        const QPointF mappedOrigin = frameTransform.map(QPointF(0.0, 0.0));
        const double scaleX = std::hypot(frameTransform.m11(),
                                         frameTransform.m12());
        const double scaleY = std::hypot(frameTransform.m21(),
                                         frameTransform.m22());
        const float screenScale = static_cast<float>(
            std::max(0.0001, std::min(scaleX, scaleY)));
        const float velocityScale = static_cast<float>(
            std::min(bounds.width(), bounds.height())) * screenScale;
        const float spillSize = std::max(
            2.0f, velocityScale * impl_->liquidParticleSpacing_ * 1.35f);
        constexpr std::size_t maxSpillParticles = 100000;
        for (const auto& source : escaped) {
          if (impl_->fluidRuntime_.liquidSpillParticles_.size() >= maxSpillParticles) break;
          const QPointF localPosition(
              bounds.left() + source.x * bounds.width(),
              bounds.top() + source.y * bounds.height());
          const QPointF localVelocity(source.vx * bounds.width(),
                                      source.vy * bounds.height());
          const QPointF worldPosition = frameTransform.map(localPosition);
          const QPointF worldVelocityPoint = frameTransform.map(localVelocity);
          const QPointF worldVelocity = worldVelocityPoint - mappedOrigin;
          impl_->fluidRuntime_.liquidSpillParticles_.push_back({
              static_cast<float>(worldPosition.x()),
              static_cast<float>(worldPosition.y()),
              static_cast<float>(worldPosition.x()),
              static_cast<float>(worldPosition.y()),
              static_cast<float>(worldVelocity.x()),
              static_cast<float>(worldVelocity.y()),
              impl_->liquidGravity_ * velocityScale,
              spillSize,
              source.collisionImpact * velocityScale});
        }
      };
      if (randomAccess) {
        int64_t replayFrame = 0;
        const LayerFluidRuntimeState::LiquidLayerCheckpointEntry* checkpoint = nullptr;
        for (const auto& candidate : impl_->fluidRuntime_.liquidCheckpoints_) {
          if (candidate.frame > targetFrame) break;
          checkpoint = &candidate;
        }
        if (checkpoint) {
          if (liquid.restore(checkpoint->checkpoint.container)) {
            replayFrame = checkpoint->frame;
            impl_->fluidRuntime_.liquidSpillParticles_ =
                checkpoint->checkpoint.spillParticles;
            impl_->fluidRuntime_.liquidInflowCarry_ = checkpoint->checkpoint.inflowCarry;
          } else {
            impl_->fluidRuntime_.liquidCheckpoints_.clear();
            impl_->fluidRuntime_.liquidSpillParticles_.clear();
            impl_->fluidRuntime_.liquidInflowCarry_ = 0.0;
          }
        }
        if (impl_->fluidRuntime_.liquidCheckpoints_.empty()) {
          liquid.reset(impl_->liquidFillAmount_,
                       impl_->liquidParticleSpacing_);
          impl_->fluidRuntime_.liquidSpillParticles_.clear();
          impl_->fluidRuntime_.liquidInflowCarry_ = 0.0;
          impl_->fluidRuntime_.liquidCheckpoints_.add(
              LayerFluidRuntimeState::LiquidLayerCheckpointEntry{
                  0, {liquid.snapshot(), {}, 0.0}});
          replayFrame = 0;
        }
        for (; replayFrame < targetFrame; ++replayFrame) {
          advanceLiquidFrame(replayFrame + 1);
          storeCheckpoint(replayFrame + 1);
        }
        impl_->fluidRuntime_.fluidLastFrame_ = targetFrame;
      } else if (targetFrame > impl_->fluidRuntime_.fluidLastFrame_) {
        for (int64_t stepFrame = impl_->fluidRuntime_.fluidLastFrame_ + 1;
             stepFrame <= targetFrame; ++stepFrame) {
          advanceLiquidFrame(stepFrame);
          storeCheckpoint(stepFrame);
        }
        impl_->fluidRuntime_.fluidLastFrame_ = targetFrame;
      }

      auto renderData = makeLiquid2DRenderData(
          liquid.snapshot(), localBounds(), impl_->liquidParticleSpacing_,
          frame);
      const QVector3D mappedVelocityOrigin =
          baseTransform.map(QVector3D(0.0f, 0.0f, 0.0f));
      const QVector3D mappedVelocityX =
          baseTransform.map(QVector3D(1.0f, 0.0f, 0.0f)) -
          mappedVelocityOrigin;
      const QVector3D mappedVelocityY =
          baseTransform.map(QVector3D(0.0f, 1.0f, 0.0f)) -
          mappedVelocityOrigin;
      // Layer color properties are stored in sRGB space; the main render
      // target is sRGB, so feed linear values to the GPU output path.
      const FloatColor linearLiquidColor = impl_->liquidColor_.toLinear();
      const float containerImpactScale = static_cast<float>(
          std::min(localBounds().width(), localBounds().height())) *
          std::max(0.0001f, std::min(mappedVelocityX.length(),
                                     mappedVelocityY.length()));
      for (auto& particle : renderData.particles) {
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
        const QVector3D mappedVelocity = baseTransform.map(
            QVector3D(particle.vx, particle.vy, particle.vz)) -
            mappedVelocityOrigin;
        particle.vx = mappedVelocity.x();
        particle.vy = mappedVelocity.y();
        particle.vz = mappedVelocity.z();
        particle.r = linearLiquidColor.r();
        particle.g = linearLiquidColor.g();
        particle.b = linearLiquidColor.b();
        particle.a *= impl_->liquidColor_.a();
        particle.a *= std::clamp(opacityScale, 0.0f, 1.0f);
      }
      const std::size_t containerParticleCount = renderData.particles.size();
      renderData.particles.reserve(renderData.particles.size() +
                                   impl_->fluidRuntime_.liquidSpillParticles_.size());
      for (const auto& spill : impl_->fluidRuntime_.liquidSpillParticles_) {
        ArtifactCore::ParticleVertex particle{};
        particle.px = spill.x;
        particle.py = spill.y;
        particle.pz = 0.0f;
        particle.ppx = particle.px;
        particle.ppy = particle.py;
        particle.ppz = particle.pz;
        particle.vx = spill.vx;
        particle.vy = spill.vy;
        particle.vz = 0.0f;
        particle.r = linearLiquidColor.r();
        particle.g = linearLiquidColor.g();
        particle.b = linearLiquidColor.b();
        particle.a = 0.82f * impl_->liquidColor_.a() *
            std::clamp(opacityScale, 0.0f, 1.0f);
        particle.size = spill.size;
        particle.stretch = 1.0f;
        particle.rotation = 0.0f;
        particle.age = 0.0f;
        particle.lifetime = 1.0f;
        renderData.particles.push_back(particle);
      }
      constexpr std::size_t maximumLiquidDetailParticles = 95000;
      if (renderData.particles.size() > maximumLiquidDetailParticles) {
        renderData.particles.resize(maximumLiquidDetailParticles);
      }
      auto& surfaceSamples = impl_->fluidRuntime_.liquidSurfaceSamples_;
      surfaceSamples.clear();
      if (surfaceSamples.capacity() < renderData.particles.size()) {
        surfaceSamples.reserve(renderData.particles.size());
      }
      for (std::size_t particleIndex = 0;
           particleIndex < renderData.particles.size(); ++particleIndex) {
        const auto& particle = renderData.particles[particleIndex];
        if (particle.a <= 0.0f || particle.size <= 0.0f) continue;
        float foamBias =
            (particleIndex < containerParticleCount ? 0.45f : 1.0f) *
            impl_->liquidFoamAmount_;
        float foamOffsetY = 0.0f;
        float collisionImpact = 0.0f;
        if (particleIndex < containerParticleCount) {
          const auto& containerParticles = liquid.particles();
          if (particleIndex < containerParticles.size()) {
            // For a rectangular pour, place falling-stream foam at the
            // nominal liquid surface while leaving its density sample in the
            // original position. This avoids a detached foam halo and keeps
            // the liquid mesh unchanged.
            if (!hasPolygonLiquidContainer) {
              const float nominalSurfaceY =
                  1.0f - impl_->liquidFillAmount_;
              const float particleY = containerParticles[particleIndex].y;
              const float foamReach = std::max(
                  0.55f, liquid.particleSpacing() * 8.0f);
              if (particleY < nominalSurfaceY - foamReach) {
                foamBias = 0.0f;
              } else if (particleY < nominalSurfaceY) {
                foamBias = std::max(
                    foamBias, impl_->liquidFoamAmount_ * 0.85f);
                const QRectF bounds = localBounds();
                const QVector3D foamSurfacePoint = baseTransform.map(
                    QVector3D(
                        static_cast<float>(bounds.left()) +
                            containerParticles[particleIndex].x *
                                static_cast<float>(bounds.width()),
                        static_cast<float>(bounds.top()) +
                            nominalSurfaceY *
                                static_cast<float>(bounds.height()),
                        0.0f));
                foamOffsetY = foamSurfacePoint.y() - particle.py;
              }
            }
            collisionImpact =
                containerParticles[particleIndex].collisionImpact *
                containerImpactScale;
          }
        } else {
          const std::size_t spillIndex =
              particleIndex - containerParticleCount;
          if (spillIndex < impl_->fluidRuntime_.liquidSpillParticles_.size()) {
            collisionImpact =
                impl_->fluidRuntime_.liquidSpillParticles_[spillIndex].collisionImpact;
          }
        }
        surfaceSamples.push_back({particle.px, particle.py, foamOffsetY,
                                  particle.size,
                                  particle.vx, particle.vy, foamBias,
                                  collisionImpact});
      }
      if (impl_->fluidRuntime_.liquidSurfaceFrame_ != frame) {
        ArtifactCore::LiquidSolver2D::buildSurfaceSnapshotInto(
            surfaceSamples,
            impl_->fluidRuntime_.liquidSurfaceSnapshot_,
            impl_->fluidRuntime_.liquidSurfaceWorkspace_);
        impl_->fluidRuntime_.liquidSurfaceFrame_ = frame;
      }
      const auto& surface = impl_->fluidRuntime_.liquidSurfaceSnapshot_;
      const std::size_t foamCapacity =
          100000 - std::min<std::size_t>(renderData.particles.size(), 100000);
      const std::size_t foamCount =
          std::min(foamCapacity, surface.foamPoints.size());
      float surfaceMinimumX = std::numeric_limits<float>::max();
      float surfaceMinimumY = std::numeric_limits<float>::max();
      float surfaceMaximumX = std::numeric_limits<float>::lowest();
      float surfaceMaximumY = std::numeric_limits<float>::lowest();
      const float velocityXLength = std::hypot(
          mappedVelocityX.x(), mappedVelocityX.y());
      const float velocityYLength = std::hypot(
          mappedVelocityY.x(), mappedVelocityY.y());
      const float liquidAxisXx = velocityXLength > 1.0e-6f
          ? mappedVelocityX.x() / velocityXLength : 1.0f;
      const float liquidAxisXy = velocityXLength > 1.0e-6f
          ? mappedVelocityX.y() / velocityXLength : 0.0f;
      const float liquidAxisYx = velocityYLength > 1.0e-6f
          ? mappedVelocityY.x() / velocityYLength : 0.0f;
      const float liquidAxisYy = velocityYLength > 1.0e-6f
          ? mappedVelocityY.y() / velocityYLength : 1.0f;
      float surfaceMinimumAxisX = std::numeric_limits<float>::max();
      float surfaceMinimumAxisY = std::numeric_limits<float>::max();
      float surfaceMaximumAxisX = std::numeric_limits<float>::lowest();
      float surfaceMaximumAxisY = std::numeric_limits<float>::lowest();
      const auto includeSurfacePoint = [&](float x, float y) {
        const float relativeX = x - mappedVelocityOrigin.x();
        const float relativeY = y - mappedVelocityOrigin.y();
        const float axisX = relativeX * liquidAxisXx +
                            relativeY * liquidAxisXy;
        const float axisY = relativeX * liquidAxisYx +
                            relativeY * liquidAxisYy;
        surfaceMinimumAxisX = std::min(surfaceMinimumAxisX, axisX);
        surfaceMaximumAxisX = std::max(surfaceMaximumAxisX, axisX);
        surfaceMinimumAxisY = std::min(surfaceMinimumAxisY, axisY);
        surfaceMaximumAxisY = std::max(surfaceMaximumAxisY, axisY);
      };
      for (const auto& triangle : surface.triangles) {
        surfaceMinimumX = std::min(
            surfaceMinimumX,
            std::min({triangle.a.x, triangle.b.x, triangle.c.x}));
        surfaceMinimumY = std::min(
            surfaceMinimumY,
            std::min({triangle.a.y, triangle.b.y, triangle.c.y}));
        surfaceMaximumX = std::max(
            surfaceMaximumX,
            std::max({triangle.a.x, triangle.b.x, triangle.c.x}));
        surfaceMaximumY = std::max(
            surfaceMaximumY,
            std::max({triangle.a.y, triangle.b.y, triangle.c.y}));
        includeSurfacePoint(triangle.a.x, triangle.a.y);
        includeSurfacePoint(triangle.b.x, triangle.b.y);
        includeSurfacePoint(triangle.c.x, triangle.c.y);
      }
      float reflectionCenterX =
          (surfaceMinimumAxisX + surfaceMaximumAxisX) * 0.5f;
      float reflectionCenterY =
          surfaceMinimumAxisY +
          (surfaceMaximumAxisY - surfaceMinimumAxisY) * 0.45f;
      float reflectionWeightSum = 0.0f;
      float weightedReflectionX = 0.0f;
      float weightedReflectionY = 0.0f;
      const float reflectionSurfaceHeight = std::max(
          1.0f, surfaceMaximumAxisY - surfaceMinimumAxisY);
      float reflectionAnchorY = reflectionCenterY;
      float reflectionTolerance = reflectionSurfaceHeight * 0.14f;
      if (!hasPolygonLiquidContainer) {
        const QRectF bounds = localBounds();
        const float nominalSurfaceY =
            static_cast<float>(bounds.top()) +
            (1.0f - impl_->liquidFillAmount_) *
                static_cast<float>(bounds.height());
        const QVector3D nominalSurfacePoint = baseTransform.map(
            QVector3D(static_cast<float>(bounds.left()), nominalSurfaceY,
                      0.0f));
        const float anchorRelativeX =
            nominalSurfacePoint.x() - mappedVelocityOrigin.x();
        const float anchorRelativeY =
            nominalSurfacePoint.y() - mappedVelocityOrigin.y();
        reflectionAnchorY = anchorRelativeX * liquidAxisYx +
                            anchorRelativeY * liquidAxisYy;
        reflectionTolerance = std::max(
            6.0f, velocityYLength * static_cast<float>(bounds.height()) *
                      0.14f);
      }
      for (const auto& segment : surface.contourSegments) {
        const float segmentX = segment.b.x - segment.a.x;
        const float segmentY = segment.b.y - segment.a.y;
        const float dx = std::abs(segmentX * liquidAxisXx +
                                  segmentY * liquidAxisXy);
        const float dy = std::abs(segmentX * liquidAxisYx +
                                  segmentY * liquidAxisYy);
        if (dx <= 1.0f || dy >= dx * 0.34f) continue;
        const float midpointX = (segment.a.x + segment.b.x) * 0.5f -
                                mappedVelocityOrigin.x();
        const float midpointY = (segment.a.y + segment.b.y) * 0.5f -
                                mappedVelocityOrigin.y();
        const float projectedMidpointX =
            midpointX * liquidAxisXx + midpointY * liquidAxisXy;
        const float projectedMidpointY =
            midpointX * liquidAxisYx + midpointY * liquidAxisYy;
        const float anchorDistance =
            std::abs(projectedMidpointY - reflectionAnchorY);
        if (anchorDistance > reflectionTolerance) continue;
        const float weight = dx * std::max(
            0.05f, 1.0f - anchorDistance / reflectionTolerance);
        weightedReflectionX += projectedMidpointX * weight;
        weightedReflectionY += projectedMidpointY * weight;
        reflectionWeightSum += weight;
      }
      if (reflectionWeightSum > 1.0e-4f) {
        reflectionCenterX = weightedReflectionX / reflectionWeightSum;
        reflectionCenterY = weightedReflectionY / reflectionWeightSum;
      }
      const bool drawSurface = impl_->liquidSurfaceOpacity_ > 0.0f;
      const bool drawEdge = impl_->liquidEdgeOpacity_ > 0.0f;
      const QRectF liquidClipBounds = localBounds();
      bool inverseLiquidTransformValid = false;
      const QMatrix4x4 compositionToLiquid = baseTransform.inverted(
          &inverseLiquidTransformValid);
      const bool clipToRectangularContainer =
          !hasPolygonLiquidContainer && liquidClipBounds.isValid() &&
          liquidClipBounds.width() > 0.0 &&
          liquidClipBounds.height() > 0.0 &&
          inverseLiquidTransformValid;
      if ((drawSurface || drawEdge) && !surface.triangles.empty()) {
        for (std::size_t particleIndex = 0;
             particleIndex < containerParticleCount; ++particleIndex) {
          auto& particle = renderData.particles[particleIndex];
          particle.a = 0.0f;
        }
      }
      if (!renderData.particles.empty()) {
        renderer->drawParticles(std::move(renderData));
      }
      if (!surface.triangles.empty()) {
        const float layerAlpha = std::clamp(opacityScale, 0.0f, 1.0f);
        if (drawSurface) {
          const FloatColor linearSurfaceFoamColor =
              impl_->liquidFoamColor_.toLinear();
          const float surfaceWidth = std::max(
              1.0f, surfaceMaximumAxisX - surfaceMinimumAxisX);
          const auto smoothBand = [](float distance, float halfWidth) {
            const float t = std::clamp(1.0f - distance / halfWidth,
                                       0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
          };
          const auto shadeLiquidVertex = [&](float x, float y,
                                             float coverage) {
            const float relativeX = x - mappedVelocityOrigin.x();
            const float relativeY = y - mappedVelocityOrigin.y();
            const float projectedX = relativeX * liquidAxisXx +
                                     relativeY * liquidAxisXy;
            const float projectedY = relativeX * liquidAxisYx +
                                     relativeY * liquidAxisYy;
            const float v = std::clamp(
                (projectedY - surfaceMinimumAxisY) /
                    reflectionSurfaceHeight,
                0.0f, 1.0f);
            const float directional = 0.48f + (1.0f - v) * 0.52f;
            const float opticalDepth = std::clamp(
                (coverage - 0.34f) * 0.5f, 0.0f, 1.0f);
            const float absorption = 1.0f - opticalDepth * 0.28f;
            // A broad, low-amplitude contact reflection keeps the highlight
            // coherent across the triangle mesh instead of exposing facets.
            const float contactBand =
                smoothBand(std::abs(projectedY - reflectionCenterY),
                           reflectionSurfaceHeight * 0.12f) *
                smoothBand(std::abs(projectedX - reflectionCenterX),
                           surfaceWidth * 0.40f) *
                0.21f;
            const float glintPhase =
                projectedX * 0.018f + projectedY * 0.012f +
                static_cast<float>(frame) * 0.004f;
            const float glintCycle = glintPhase - std::floor(glintPhase);
            const float glintWave =
                1.0f - std::abs(glintCycle * 2.0f - 1.0f);
            const float glintDistance = 1.0f - glintWave;
            const float glintRamp = std::clamp(
                1.0f - glintDistance / 0.38f, 0.0f, 1.0f);
            const float glint =
                glintRamp * glintRamp * (3.0f - 2.0f * glintRamp);
            const float upperSurfaceRamp = std::clamp(
                (v - 0.12f) / 0.45f, 0.0f, 1.0f);
            const float upperSurfaceMask =
                1.0f - upperSurfaceRamp * upperSurfaceRamp *
                    (3.0f - 2.0f * upperSurfaceRamp);
            const float movingGlint = glint * upperSurfaceMask * 0.25f;
            const float reflection = contactBand + movingGlint;
            const float baseRed =
                linearLiquidColor.r() * directional * absorption;
            const float baseGreen =
                linearLiquidColor.g() * directional * absorption;
            const float baseBlue =
                linearLiquidColor.b() * directional * absorption;
            return FloatColor(
                std::clamp(baseRed +
                               (linearSurfaceFoamColor.r() - baseRed) *
                                   reflection,
                           0.0f, 1.0f),
                std::clamp(baseGreen +
                               (linearSurfaceFoamColor.g() - baseGreen) *
                                   reflection,
                           0.0f, 1.0f),
                std::clamp(baseBlue +
                               (linearSurfaceFoamColor.b() - baseBlue) *
                                   reflection,
                           0.0f, 1.0f),
                impl_->liquidColor_.a() * impl_->liquidSurfaceOpacity_ *
                    layerAlpha * 0.96f);
          };
          struct ClippedLiquidVertex {
            QVector3D position;
            FloatColor color;
          };
          const auto drawClippedLiquidTriangle =
              [&](const ArtifactCore::LiquidSurfaceTriangle2D& triangle) {
            if (!clipToRectangularContainer) {
              renderer->drawGradientTriangleLocal(
                  {triangle.a.x, triangle.a.y},
                  {triangle.b.x, triangle.b.y},
                  {triangle.c.x, triangle.c.y},
                  shadeLiquidVertex(triangle.a.x, triangle.a.y,
                                    triangle.coverageA),
                  shadeLiquidVertex(triangle.b.x, triangle.b.y,
                                    triangle.coverageB),
                  shadeLiquidVertex(triangle.c.x, triangle.c.y,
                                    triangle.coverageC));
              return;
            }
            ClippedLiquidVertex polygon[8] = {
                {compositionToLiquid.map(
                     QVector3D(triangle.a.x, triangle.a.y, 0.0f)),
                 shadeLiquidVertex(triangle.a.x, triangle.a.y,
                                   triangle.coverageA)},
                {compositionToLiquid.map(
                     QVector3D(triangle.b.x, triangle.b.y, 0.0f)),
                 shadeLiquidVertex(triangle.b.x, triangle.b.y,
                                   triangle.coverageB)},
                {compositionToLiquid.map(
                     QVector3D(triangle.c.x, triangle.c.y, 0.0f)),
                 shadeLiquidVertex(triangle.c.x, triangle.c.y,
                                   triangle.coverageC)}};
            std::size_t polygonCount = 3;
            const auto clipAxis = [&](bool useX, float boundary,
                                      bool keepGreater) {
              ClippedLiquidVertex clipped[8]{};
              std::size_t clippedCount = 0;
              const auto coordinate = [useX](const QVector3D& point) {
                return useX ? point.x() : point.y();
              };
              const auto inside = [&](float value) {
                return keepGreater ? value >= boundary : value <= boundary;
              };
              for (std::size_t index = 0; index < polygonCount; ++index) {
                const auto& current = polygon[index];
                const auto& previous = polygon[
                    (index + polygonCount - 1) % polygonCount];
                const float currentCoordinate = coordinate(current.position);
                const float previousCoordinate = coordinate(previous.position);
                const bool currentInside = inside(currentCoordinate);
                const bool previousInside = inside(previousCoordinate);
                if (currentInside != previousInside && clippedCount < 8) {
                  const float denominator =
                      currentCoordinate - previousCoordinate;
                  const float t = std::abs(denominator) > 1.0e-8f
                      ? std::clamp((boundary - previousCoordinate) /
                                       denominator,
                                   0.0f, 1.0f)
                      : 0.5f;
                  clipped[clippedCount++] = {
                      previous.position +
                          (current.position - previous.position) * t,
                      FloatColor(
                          previous.color.r() +
                              (current.color.r() - previous.color.r()) * t,
                          previous.color.g() +
                              (current.color.g() - previous.color.g()) * t,
                          previous.color.b() +
                              (current.color.b() - previous.color.b()) * t,
                          previous.color.a() +
                              (current.color.a() - previous.color.a()) * t)};
                }
                if (currentInside && clippedCount < 8) {
                  clipped[clippedCount++] = current;
                }
              }
              polygonCount = clippedCount;
              for (std::size_t index = 0; index < polygonCount; ++index) {
                polygon[index] = clipped[index];
              }
            };
            clipAxis(true, static_cast<float>(liquidClipBounds.left()), true);
            clipAxis(true, static_cast<float>(liquidClipBounds.right()), false);
            // The rectangular liquid container is open at the top. Keep
            // stream geometry above it, while clipping the side walls and
            // closed bottom to the exact container interior.
            clipAxis(false, static_cast<float>(liquidClipBounds.bottom()),
                     false);
            for (std::size_t index = 1; index + 1 < polygonCount; ++index) {
              const auto p0 = baseTransform.map(polygon[0].position);
              const auto p1 = baseTransform.map(polygon[index].position);
              const auto p2 = baseTransform.map(polygon[index + 1].position);
              renderer->drawGradientTriangleLocal(
                  {p0.x(), p0.y()}, {p1.x(), p1.y()}, {p2.x(), p2.y()},
                  polygon[0].color, polygon[index].color,
                  polygon[index + 1].color);
            }
          };
          for (const auto& triangle : surface.triangles) {
            drawClippedLiquidTriangle(triangle);
          }
        }
        if (drawEdge) {
          const FloatColor linearFoamColor =
              impl_->liquidFoamColor_.toLinear();
          const FloatColor surfaceEdgeColor(
              linearLiquidColor.r() +
                  (1.0f - linearLiquidColor.r()) * 0.30f,
              linearLiquidColor.g() +
                  (1.0f - linearLiquidColor.g()) * 0.30f,
              linearLiquidColor.b() +
                  (1.0f - linearLiquidColor.b()) * 0.30f,
              impl_->liquidColor_.a() *
              impl_->liquidEdgeOpacity_ * layerAlpha * 0.32f);
          const FloatColor upperSurfaceSheenColor(
              linearLiquidColor.r() * 0.30f +
                  linearFoamColor.r() * 0.70f,
              linearLiquidColor.g() * 0.30f +
                  linearFoamColor.g() * 0.70f,
              linearLiquidColor.b() * 0.30f +
                  linearFoamColor.b() * 0.70f,
              impl_->liquidColor_.a() * impl_->liquidEdgeOpacity_ *
                  layerAlpha * 0.48f * impl_->liquidFoamAmount_);
          const float surfaceHeight = std::max(
              1.0f, surfaceMaximumY - surfaceMinimumY);
          const auto clipContourSegment = [&](QPointF& a, QPointF& b) {
            if (!clipToRectangularContainer) return true;
            const QVector3D localA = compositionToLiquid.map(
                QVector3D(static_cast<float>(a.x()),
                          static_cast<float>(a.y()), 0.0f));
            const QVector3D localB = compositionToLiquid.map(
                QVector3D(static_cast<float>(b.x()),
                          static_cast<float>(b.y()), 0.0f));
            const float left =
                static_cast<float>(liquidClipBounds.left()) + 1.25f;
            const float right =
                static_cast<float>(liquidClipBounds.right()) - 1.25f;
            const float bottom =
                static_cast<float>(liquidClipBounds.bottom()) - 1.25f;
            float tMinimum = 0.0f;
            float tMaximum = 1.0f;
            const auto clipPlane = [&](float from, float to,
                                       float boundary, bool keepGreater) {
              const float delta = to - from;
              if (std::abs(delta) <= 1.0e-8f) {
                return keepGreater ? from >= boundary : from <= boundary;
              }
              const float crossing = (boundary - from) / delta;
              if (keepGreater) {
                if (delta > 0.0f) {
                  tMinimum = std::max(tMinimum, crossing);
                } else {
                  tMaximum = std::min(tMaximum, crossing);
                }
              } else if (delta > 0.0f) {
                tMaximum = std::min(tMaximum, crossing);
              } else {
                tMinimum = std::max(tMinimum, crossing);
              }
              return tMinimum <= tMaximum;
            };
            if (!clipPlane(localA.x(), localB.x(), left, true) ||
                !clipPlane(localA.x(), localB.x(), right, false) ||
                !clipPlane(localA.y(), localB.y(), bottom, false)) {
              return false;
            }
            const QVector3D localDelta = localB - localA;
            const QVector3D clippedA = baseTransform.map(
                localA + localDelta * tMinimum);
            const QVector3D clippedB = baseTransform.map(
                localA + localDelta * tMaximum);
            a = QPointF(clippedA.x(), clippedA.y());
            b = QPointF(clippedB.x(), clippedB.y());
            return true;
          };
          for (const auto& segment : surface.contourSegments) {
            QPointF clippedA(segment.a.x, segment.a.y);
            QPointF clippedB(segment.b.x, segment.b.y);
            if (!clipContourSegment(clippedA, clippedB)) continue;
            renderer->drawThickLineLocal(
                {static_cast<float>(clippedA.x()),
                 static_cast<float>(clippedA.y())},
                {static_cast<float>(clippedB.x()),
                 static_cast<float>(clippedB.y())},
                0.72f, surfaceEdgeColor);
            const float dx = std::abs(segment.b.x - segment.a.x);
            const float dy = std::abs(segment.b.y - segment.a.y);
            const float midpointY =
                (segment.a.y + segment.b.y) * 0.5f;
            const float relativeHeight =
                (midpointY - surfaceMinimumY) / surfaceHeight;
            if (impl_->liquidFoamAmount_ > 0.0f &&
                dx > 1.0f && dy < dx * 0.34f &&
                relativeHeight >= 0.25f && relativeHeight <= 0.58f) {
              renderer->drawThickLineLocal(
                  {static_cast<float>(clippedA.x()),
                   static_cast<float>(clippedA.y())},
                  {static_cast<float>(clippedB.x()),
                   static_cast<float>(clippedB.y())},
                  1.6f + impl_->liquidFoamAmount_ * 2.4f,
                  upperSurfaceSheenColor);
            }
          }
        }
        ArtifactCore::ParticleRenderData foamRenderData;
        foamRenderData.frameNumber = frame;
        foamRenderData.options.blend =
            ArtifactCore::ParticleBlendPolicy::Alpha;
        foamRenderData.options.billboard =
            ArtifactCore::ParticleBillboardPolicy::VelocityAligned;
        constexpr std::size_t maximumCremaBubbles = 4096;
        const std::size_t cremaCapacity = std::min(
            maximumCremaBubbles, foamCapacity - foamCount);
        foamRenderData.particles.reserve(foamCount + cremaCapacity);
        const FloatColor linearFoamColor =
            impl_->liquidFoamColor_.toLinear();
        for (std::size_t foamIndex = 0; foamIndex < foamCount; ++foamIndex) {
          const auto& foam = surface.foamPoints[foamIndex];
          ArtifactCore::ParticleVertex particle{};
          particle.px = foam.position.x;
          particle.py = foam.position.y;
          particle.pz = 0.0f;
          particle.vx = foam.vx;
          particle.vy = foam.vy;
          particle.ppx = particle.px;
          particle.ppy = particle.py;
          particle.ppz = particle.pz;
          particle.r = linearFoamColor.r();
          particle.g = linearFoamColor.g();
          particle.b = linearFoamColor.b();
          particle.a = foam.alpha * impl_->liquidFoamColor_.a() * 0.48f *
              std::clamp(opacityScale, 0.0f, 1.0f);
          particle.size = foam.size * 0.42f;
          particle.stretch = 1.6f;
          particle.lifetime = 1.0f;
          foamRenderData.particles.push_back(particle);
        }
        // Add a restrained crema bead along the reconstructed liquid surface.
        // The particle solver only emits foam for fast or recently impacted
        // particles, so a settled coffee pool otherwise loses its visible
        // surface foam even when the Foam Amount control is non-zero.
        if (impl_->liquidFoamAmount_ > 0.0f && cremaCapacity > 0 &&
            !hasPolygonLiquidContainer) {
          std::size_t cremaCount = 0;
          for (const auto& segment : surface.contourSegments) {
            const float segmentX = segment.b.x - segment.a.x;
            const float segmentY = segment.b.y - segment.a.y;
            const float axisX = std::abs(
                segmentX * liquidAxisXx + segmentY * liquidAxisXy);
            const float axisY = std::abs(
                segmentX * liquidAxisYx + segmentY * liquidAxisYy);
            if (axisX <= 1.0f || axisY >= axisX * 0.34f) continue;
            const float midpointX =
                (segment.a.x + segment.b.x) * 0.5f -
                mappedVelocityOrigin.x();
            const float midpointY =
                (segment.a.y + segment.b.y) * 0.5f -
                mappedVelocityOrigin.y();
            const float projectedX =
                midpointX * liquidAxisXx + midpointY * liquidAxisXy;
            const float projectedY =
                midpointX * liquidAxisYx + midpointY * liquidAxisYy;
            if (std::abs(projectedY - reflectionAnchorY) >
                reflectionTolerance) {
              continue;
            }
            const auto cellX = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(std::floor(projectedX * 0.37f)));
            const auto cellY = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(std::floor(projectedY * 0.37f)));
            std::uint32_t beadHash = cellX * 73856093u ^ cellY * 19349663u;
            beadHash ^= beadHash >> 13;
            beadHash *= 1274126177u;
            beadHash ^= beadHash >> 16;
            if (beadHash % 100u >= 16u) continue;
            if (cremaCount >= cremaCapacity) break;
            const float beadVariation =
                static_cast<float>(beadHash & 0xffffu) / 65535.0f;
            ArtifactCore::ParticleVertex bubble{};
            bubble.px = midpointX + mappedVelocityOrigin.x();
            bubble.py = midpointY + mappedVelocityOrigin.y();
            bubble.pz = 0.0f;
            bubble.ppx = bubble.px;
            bubble.ppy = bubble.py;
            bubble.ppz = bubble.pz;
            bubble.r = linearFoamColor.r();
            bubble.g = linearFoamColor.g();
            bubble.b = linearFoamColor.b();
            bubble.a = (0.20f + beadVariation * 0.22f) *
                impl_->liquidFoamAmount_ *
                impl_->liquidFoamColor_.a() *
                std::clamp(opacityScale, 0.0f, 1.0f);
            bubble.size = 0.28f + beadVariation * 0.36f;
            bubble.stretch = 1.0f;
            bubble.lifetime = 1.0f;
            foamRenderData.particles.push_back(bubble);
            ++cremaCount;
          }
        }
        if (!foamRenderData.particles.empty()) {
          renderer->drawParticles(std::move(foamRenderData));
        }
      }
      if (impl_->liquidContainerOpacity_ > 0.0f &&
          impl_->liquidContainerWidth_ > 0.0f) {
        if (!hasPolygonLiquidContainer) {
          const QRectF bounds = localBounds();
          if (bounds.isValid() && bounds.width() > 0.0 &&
              bounds.height() > 0.0) {
            liquidContainerPoints.assign({
                bounds.topLeft(), bounds.topRight(),
                bounds.bottomRight(), bounds.bottomLeft()});
            liquidContainerOpeningEdge = 0;
          }
        }
        if (liquidContainerPoints.size() >= 3) {
          const float containerAlpha =
              impl_->liquidContainerOpacity_ *
              std::clamp(opacityScale, 0.0f, 1.0f);
          const FloatColor containerColor(
              std::clamp(impl_->liquidColor_.r() * 0.32f + 0.54f, 0.0f, 1.0f),
              std::clamp(impl_->liquidColor_.g() * 0.32f + 0.54f, 0.0f, 1.0f),
              std::clamp(impl_->liquidColor_.b() * 0.32f + 0.54f, 0.0f, 1.0f),
              containerAlpha);
          for (std::size_t edge = 0;
               edge < liquidContainerPoints.size(); ++edge) {
            if (edge == liquidContainerOpeningEdge) continue;
            const QPointF& localA = liquidContainerPoints[edge];
            const QPointF& localB = liquidContainerPoints[
                (edge + 1) % liquidContainerPoints.size()];
            const QVector3D worldA = baseTransform.map(QVector3D(
                static_cast<float>(localA.x()),
                static_cast<float>(localA.y()), 0.0f));
            const QVector3D worldB = baseTransform.map(QVector3D(
                static_cast<float>(localB.x()),
                static_cast<float>(localB.y()), 0.0f));
            renderer->drawThickLineLocal(
                {worldA.x(), worldA.y()}, {worldB.x(), worldB.y()},
                impl_->liquidContainerWidth_, containerColor);
          }
        }
      }
    } else {
      if (impl_->fluidRuntime_.liquidSolver_ || !impl_->fluidRuntime_.liquidCheckpoints_.empty()) {
        impl_->fluidRuntime_.invalidateLiquidSimulation();
      }
      renderLayerSmokeRuntime(
          impl_->fluidRuntime_, renderer, localBounds(), baseTransform,
          opacityScale, frame, fps,
          LayerSmokeRuntimeSettings{
              impl_->fluidGridWidth_, impl_->fluidGridHeight_,
              impl_->fluidViscosity_, impl_->fluidDiffusion_,
              impl_->fluidBuoyancy_, impl_->fluidVorticity_,
              impl_->fluidSolverIterations_, impl_->particleEmitterCount_,
              impl_->particleEmitterSpeed_});
    }
  } else {
    impl_->fluidRuntime_.invalidateSmokeSimulation();
    impl_->fluidRuntime_.invalidateLiquidSimulation();
  }

  if (impl_->particleEmitterComponentEnabled_ &&
      !impl_->componentParticles_.empty()) {
    const double fps = std::max(1.0, effectiveLayerFrameRate(this));
    if (impl_->componentParticlesLastFrame_ ==
        std::numeric_limits<int64_t>::min()) {
      impl_->componentParticlesLastFrame_ = frame;
    } else if (frame < impl_->componentParticlesLastFrame_ ||
               frame - impl_->componentParticlesLastFrame_ > 10) {
      impl_->componentParticles_.clear();
      impl_->componentParticlesLastFrame_ = frame;
    } else if (frame > impl_->componentParticlesLastFrame_) {
      const float dt = static_cast<float>(
          frame - impl_->componentParticlesLastFrame_) /
                       static_cast<float>(fps);
      for (auto& particle : impl_->componentParticles_) {
        particle.age += dt;
        particle.ppx = particle.px;
        particle.ppy = particle.py;
        particle.ppz = particle.pz;
        particle.px += particle.vx * dt;
        particle.py += particle.vy * dt;
        particle.pz += particle.vz * dt;
        particle.vy += impl_->physicsComponent_.settings().gravityY * dt;
      }
      impl_->componentParticlesLastFrame_ = frame;
      impl_->componentParticles_.removeIf(
          [](const ArtifactCore::ParticleVertex& particle) {
            return particle.age >= particle.lifetime;
          });
    }

    ArtifactCore::ParticleRenderData renderData;
    renderData.frameNumber = frame;
    renderData.options.blend =
        ArtifactCore::ParticleBlendPolicy::Additive;
    renderData.options.billboard =
        ArtifactCore::ParticleBillboardPolicy::VelocityAligned;
    renderData.particles.reserve(impl_->componentParticles_.size());
    for (const auto& sourceParticle : impl_->componentParticles_) {
      auto particle = sourceParticle;
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
    if (!renderData.particles.empty()) {
      if (impl_->fractureEnabled_ &&
          !impl_->fractureState_.shards.empty()) {
        fractureElement.debris = std::move(renderData);
      } else {
        renderer->drawParticles(std::move(renderData));
      }
    }
  }

  if (impl_->fractureEnabled_ && impl_->fracturePreGenerate_ &&
      impl_->fractureState_.shards.empty()) {
    FractureSettings prefractureSettings = makeFracturePreset(
        static_cast<FracturePreset>(std::clamp(
            impl_->fracturePreset_, 0, static_cast<int>(FracturePreset::Dust))));
    prefractureSettings.shardCount = std::max(1, impl_->fractureShardCount_);
    const auto physicsLod = LayerPhysics::fractureLodScale();
    prefractureSettings.shardCount = std::max(
        1, static_cast<int>(std::lround(
            static_cast<float>(prefractureSettings.shardCount) *
            std::clamp(physicsLod.shards, 0.125f, 1.0f))));
    prefractureSettings.debrisCount = std::max(
        0, static_cast<int>(std::lround(
            static_cast<float>(prefractureSettings.debrisCount) *
            std::clamp(physicsLod.debris, 0.0f, 1.0f))));
    FractureEffect prefracture;
    prefracture.setSourceBounds(localBounds());
    prefracture.setImpactPoint(localBounds().center());
    prefracture.setSettings(prefractureSettings);
    if (prefracture.generate()) {
      impl_->prefractureResult_ = prefracture.result();
      impl_->fractureState_.kind = FractureStateKind::Shattered;
      impl_->fractureState_.shards.reserve(
          impl_->prefractureResult_.shards.size());
      for (const FractureShard& sourceShard : impl_->prefractureResult_.shards) {
        FractureShardMotion shard;
        shard.position = sourceShard.sourceCentroid;
        shard.scale = sourceShard.scale;
        shard.opacity = sourceShard.opacity;
        shard.lifetime = sourceShard.lifetime;
        shard.active = sourceShard.active;
        shard.debris = sourceShard.debris;
        impl_->fractureState_.shards.push_back(std::move(shard));
      }
    }
  }

  if (!impl_->fractureEnabled_ || impl_->fractureState_.shards.empty()) {
    impl_->componentEvaluationState_.fragments.clear();
    impl_->componentEvaluationState_.fragmentGeometry.clear();
    return;
  }

  const bool needsReset = impl_->fractureMotionLastFrame_ == std::numeric_limits<int64_t>::min() ||
                          frame != impl_->fractureMotionLastFrame_ + 1;
  if (needsReset) {
    impl_->fractureMotionLastFrame_ = frame;
  }

  FractureSettings settings;
  settings.gravity = impl_->fractureShardGravity_;
  settings.damping = impl_->fractureShardDamping_;
  settings.impulseStrength = impl_->fractureImpactSensitivity_ * 120.0f;
  settings.angularStrength = 8.0f;
  settings.lifetimeMin = 0.8f;
  settings.lifetimeMax = 2.5f;
  settings.edgeJitter = 0.12f;
  settings.shardCount = std::max(1, static_cast<int>(impl_->fractureState_.shards.size()));
  const auto physicsLod = LayerPhysics::fractureLodScale();
  settings.shardCount = std::max(
      1, static_cast<int>(std::lround(
          static_cast<float>(settings.shardCount) *
          std::clamp(physicsLod.shards, 0.125f, 1.0f))));
  settings.debrisCount = std::max(
      0, static_cast<int>(std::lround(
          48.0f * std::clamp(physicsLod.debris, 0.0f, 1.0f))));
  const float dt = needsReset ? 0.0f : (1.0f / std::max(1.0, effectiveLayerFrameRate(this)));
  for (auto& shard : impl_->fractureState_.shards) {
    ArtifactCore::stepFractureShardMotion(shard, dt, settings);
    applyFragmentFieldsAndFloorCollision(this, shard, baseTransform, dt);
  }
  impl_->fractureMotionLastFrame_ = frame;

  syncFragmentDataset(this, impl_->fractureState_,
                      &impl_->prefractureResult_,
                      impl_->componentEvaluationState_);

  const auto& evaluationState = impl_->componentEvaluationState_;
  for (const auto& fragment : evaluationState.fragments) {
    if (!fragment.active || fragment.opacity <= 0.0f) {
      continue;
    }
    const auto geometry = std::find_if(
        evaluationState.fragmentGeometry.begin(),
        evaluationState.fragmentGeometry.end(),
        [&](const LayerFragmentGeometry& candidate) {
          return candidate.geometryHandle == fragment.geometryHandle;
        });
    if (geometry == evaluationState.fragmentGeometry.end() ||
        geometry->localPolygon.size() < 3U) {
      continue;
    }
    QMatrix4x4 fragmentTransform = fragment.transform;
    if (impl_->fragmentVelocityStretchEnabled_) {
      const float speed = fragment.linearVelocity.length();
      if (speed > 0.001f) {
        const float stretch = std::clamp(
            1.0f + speed * impl_->fragmentVelocityStretchStrength_, 1.0f,
            std::max(1.0f, impl_->fragmentVelocityStretchMax_));
        const QVector3D center = fragment.transform.column(3).toVector3D();
        const float angleDegrees = std::atan2(
            fragment.linearVelocity.y(), fragment.linearVelocity.x()) *
            180.0f / 3.14159265358979323846f;
        QMatrix4x4 stretchTransform;
        stretchTransform.translate(center);
        stretchTransform.rotate(angleDegrees, 0.0f, 0.0f, 1.0f);
        stretchTransform.scale(stretch, 1.0f, 1.0f);
        stretchTransform.rotate(-angleDegrees, 0.0f, 0.0f, 1.0f);
        stretchTransform.translate(-center);
        fragmentTransform = stretchTransform * fragmentTransform;
      }
    }
    fragmentTransform = baseTransform * fragmentTransform;
    appendAndDrawMotionTrail(
        fragment.entityId.ownerLayerId + QStringLiteral(":") +
            QString::number(fragment.entityId.localId),
        fragmentTransform.column(3).toVector3D(), fragment.opacity);
    std::vector<Detail::float2> shardPoly;
    shardPoly.reserve(geometry->localPolygon.size());
    for (const QVector2D& point : geometry->localPolygon) {
      const QVector4D canvasPoint =
          fragmentTransform * QVector4D(point.x(), point.y(), 0.0f, 1.0f);
      shardPoly.push_back({canvasPoint.x(), canvasPoint.y()});
    }
    const float alpha =
        std::clamp(fragment.opacity * opacityScale, 0.0f, 1.0f);
    FloatColor shardColor(0.92f, 0.96f, 1.0f, alpha * 0.42f);
    if (impl_->fragmentColorVariationEnabled_) {
      const uint seed = qHash(fragment.geometryHandle);
      const float hue = static_cast<float>(seed % 360U) / 360.0f;
      const QColor varied = QColor::fromHsvF(
          hue, 0.30, 1.0,
          std::clamp(0.42f + static_cast<float>((seed >> 9U) % 48U) /
                                  100.0f,
                     0.0f, 1.0f));
      const float mix = std::clamp(impl_->fragmentColorVariation_, 0.0f, 1.0f);
      shardColor = FloatColor(
          0.92f + (varied.redF() - 0.92f) * mix,
          0.96f + (varied.greenF() - 0.96f) * mix,
          1.0f + (varied.blueF() - 1.0f) * mix,
          alpha * (0.42f + (varied.alphaF() - 0.42f) * mix));
    }
    const int cloneCount = impl_->fragmentClonerOutputEnabled_
        ? std::clamp(impl_->fragmentClonerOutputCount_, 1, 256)
        : 1;
    const QVector3D cloneOrigin = baseTransform.map(QVector3D());
    const QVector3D cloneStep = baseTransform.map(QVector3D(
        impl_->fragmentClonerOutputSpacingX_,
        impl_->fragmentClonerOutputSpacingY_, 0.0f)) - cloneOrigin;
    const float cloneTimeStepSeconds =
        impl_->fragmentClonerOutputTimeOffsetFrames_ /
        static_cast<float>(std::max(1.0, effectiveLayerFrameRate(this)));
    const QVector3D cloneVelocityStep = baseTransform.map(
        fragment.linearVelocity * cloneTimeStepSeconds) - cloneOrigin;
    for (int cloneIndex = 0; cloneIndex < cloneCount; ++cloneIndex) {
      std::vector<Detail::float2> clonePolygon = shardPoly;
      const float cloneTimeIndex = static_cast<float>(cloneIndex);
      const float offsetX = (cloneStep.x() + cloneVelocityStep.x()) *
                            cloneTimeIndex;
      const float offsetY = (cloneStep.y() + cloneVelocityStep.y()) *
                            cloneTimeIndex;
      for (auto& point : clonePolygon) {
        point.x += offsetX;
        point.y += offsetY;
      }
      const float cloneOpacity = 1.0f -
          0.18f * static_cast<float>(cloneIndex) /
              static_cast<float>(std::max(1, cloneCount - 1));
      const FloatColor cloneColor(
          shardColor.r(), shardColor.g(), shardColor.b(),
          shardColor.a() * cloneOpacity);
      std::vector<Detail::float2> cloneUV;
      cloneUV.reserve(geometry->localUV.size());
      for (const QVector2D& uv : geometry->localUV) {
        cloneUV.push_back({uv.x(), uv.y()});
      }
      fractureElement.shards.push_back(
          {std::move(clonePolygon), std::move(cloneUV), cloneColor});
    }
  }
  submitFractureRenderElement(renderer, fractureElement, sourceTexture);
}

void ArtifactAbstractLayer::resetFractureState() {
  ArtifactCore::resetFractureState(impl_->fractureState_);
  impl_->prefractureResult_ = FractureResult{};
  impl_->componentEvaluationState_.fragments.clear();
  impl_->componentEvaluationState_.fragmentGeometry.clear();
  impl_->componentEvaluationState_.clearTransientEvents();
  impl_->fractureMotionLastFrame_ = std::numeric_limits<int64_t>::min();
  impl_->fractureTriggerLastFrame_ = std::numeric_limits<int64_t>::min();
  impl_->lastCollisionImpactFrame_ = std::numeric_limits<int64_t>::min();
  impl_->lastRigidWindForceFrame_ = std::numeric_limits<int64_t>::min();
  if (impl_->particleEmitterComponentEnabled_) {
    impl_->componentParticles_.clear();
    impl_->componentParticlesLastFrame_ =
        std::numeric_limits<int64_t>::min();
  }
}

void ArtifactAbstractLayer::applyFractureImpact(const FractureImpact& impact) {
  if (!impl_->fractureEnabled_ &&
      !impl_->particleEmitterComponentEnabled_) {
    return;
  }

  auto& evaluationState = impl_->componentEvaluationState_;
  evaluationState.pendingFractures.clear();
  evaluationState.pendingParticleSpawns.clear();
  const QString ownerLayerId = id().toString();
  const QRectF impactBounds = localBounds();
  const QVector3D impactPosition(
      static_cast<float>(impactBounds.center().x()),
      static_cast<float>(impactBounds.center().y()), 0.0f);
  evaluationState.pendingFractures.push_back(LayerFractureEvent{
      SimulationEntityId{ownerLayerId, QStringLiteral("layer.source"), 0, 0},
      impactPosition, QVector3D(0.0f, impact.impulse, 0.0f), impact.stress,
      static_cast<std::uint32_t>(std::max(1, impl_->fractureShardCount_))});

  const auto emitImpactParticles = [this, &impact]() {
    if (!impl_->particleEmitterComponentEnabled_ ||
        impl_->particleEmitterCount_ <= 0) {
      return;
    }
    const QRectF bounds = localBounds();
    const QPointF center = bounds.isValid() ? bounds.center() : QPointF();
    const std::uint32_t seed =
        static_cast<std::uint32_t>(
            qHash(id().toString()) ^
            static_cast<uint>(currentTimelineFrame(this)));
    impl_->componentEvaluationState_.pendingParticleSpawns.push_back(
        LayerParticleSpawnEvent{
            SimulationEntityId{id().toString(),
                               QStringLiteral("component.fracture"), 0, 0},
            QVector3D(static_cast<float>(center.x()),
                      static_cast<float>(center.y()), 0.0f),
            QVector3D(0.0f, impact.speed, 0.0f),
            static_cast<std::uint32_t>(impl_->particleEmitterCount_), seed});
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> angleDistribution(
        0.0f, 2.0f * 3.1415926535f);
    std::uniform_real_distribution<float> speedDistribution(0.35f, 1.0f);
    std::uniform_real_distribution<float> sizeDistribution(2.0f, 7.0f);
    const float impactScale =
        std::max(0.25f, std::min(4.0f, impact.impulse));
    const auto& fragments = impl_->componentEvaluationState_.fragments;
    const bool emitFromFragments = impl_->fractureEnabled_ && !fragments.empty();

    QVector3D debrisColor(1.0f, 0.72f, 0.28f);
    switch (static_cast<FracturePreset>(impl_->fracturePreset_)) {
    case FracturePreset::Glass:
      debrisColor = QVector3D(0.62f, 0.88f, 1.0f);
      break;
    case FracturePreset::Concrete:
      debrisColor = QVector3D(0.62f, 0.58f, 0.52f);
      break;
    case FracturePreset::Stone:
      debrisColor = QVector3D(0.50f, 0.43f, 0.35f);
      break;
    case FracturePreset::Metal:
      debrisColor = QVector3D(1.0f, 0.76f, 0.34f);
      break;
    case FracturePreset::Wood:
      debrisColor = QVector3D(0.58f, 0.34f, 0.16f);
      break;
    case FracturePreset::Dust:
      debrisColor = QVector3D(0.74f, 0.66f, 0.52f);
      break;
    }

    impl_->componentParticles_.reserve(
        impl_->componentParticles_.size() +
        static_cast<std::size_t>(impl_->particleEmitterCount_));
    for (int index = 0; index < impl_->particleEmitterCount_; ++index) {
      const float angle = angleDistribution(rng);
      const float speed = impl_->particleEmitterSpeed_ *
                          speedDistribution(rng) * impactScale *
                          (emitFromFragments ? 0.35f : 1.0f);
      QVector3D sourcePosition(static_cast<float>(center.x()),
                               static_cast<float>(center.y()), 0.0f);
      QVector3D sourceVelocity;
      float sourceScale = 1.0f;
      if (emitFromFragments) {
        const auto& fragment = fragments[
            static_cast<std::size_t>(index) % fragments.size()];
        sourcePosition = fragment.transform.column(3).toVector3D();
        sourceVelocity = fragment.linearVelocity;
        sourceScale = std::max(
            0.25f, fragment.transform.column(0).toVector3D().length());
      }

      ArtifactCore::ParticleVertex particle{};
      particle.px = sourcePosition.x();
      particle.py = sourcePosition.y();
      particle.pz = sourcePosition.z();
      particle.ppx = particle.px;
      particle.ppy = particle.py;
      particle.ppz = particle.pz;
      particle.vx = sourceVelocity.x() + std::cos(angle) * speed;
      particle.vy = sourceVelocity.y() + std::sin(angle) * speed;
      particle.vz = sourceVelocity.z();
      particle.r = debrisColor.x();
      particle.g = debrisColor.y();
      particle.b = debrisColor.z();
      particle.a = 1.0f;
      particle.size = sizeDistribution(rng) * sourceScale;
      const float particleSpeed =
          std::sqrt(particle.vx * particle.vx + particle.vy * particle.vy);
      particle.stretch =
          std::clamp(1.0f + particleSpeed / 320.0f, 1.0f, 3.0f);
      particle.rotation = std::atan2(particle.vy, particle.vx);
      particle.age = 0.0f;
      particle.lifetime = impl_->particleEmitterLifetime_;
      impl_->componentParticles_.push_back(particle);
    }
    impl_->componentParticlesLastFrame_ = currentTimelineFrame(this);
  };

  if (!impl_->fractureEnabled_) {
    emitImpactParticles();
    return;
  }

  FractureSettings settings;
  settings = makeFracturePreset(static_cast<FracturePreset>(
      std::clamp(impl_->fracturePreset_, 0, static_cast<int>(FracturePreset::Dust))));
  settings.shardCount = std::max(1, impl_->fractureShardCount_);
  settings.crackThreshold = impl_->fractureCrackThreshold_;
  settings.shatterThreshold = impl_->fractureShatterThreshold_;
  settings.debrisCount = 48;
  settings.impulseStrength = impl_->fractureImpactSensitivity_ * 120.0f;
  settings.angularStrength = 8.0f;
  settings.gravity = 0.0f;
  settings.damping = impl_->fractureShardDamping_;
  settings.lifetimeMin = 0.8f;
  settings.lifetimeMax = 2.5f;
  settings.debrisLifetimeMin = 0.25f;
  settings.debrisLifetimeMax = 1.2f;
  settings.impactRadius = 96.0f;
  settings.edgeJitter = 0.12f;
  settings.cellJitter = 0.18f;
  settings.debrisRatio = 0.35f;
  settings.protectedCenterRadius = 0.0f;
  settings.seed = 0;
  settings.preserveSourceFill = true;
  settings.gravity = impl_->fractureShardGravity_;
  settings.shardCount = std::max(1, settings.shardCount);
  settings.debrisCount = std::max(0, settings.debrisCount);
  settings.lifetimeMin = std::max(0.01f, settings.lifetimeMin);
  settings.lifetimeMax = std::max(settings.lifetimeMin, settings.lifetimeMax);
  ArtifactCore::applyFractureImpact(impl_->fractureState_, settings, impact);
  ArtifactCore::primeFractureShardMotion(impl_->fractureState_, settings, impact, localBounds());
  if (!impl_->fractureState_.shards.empty() &&
      (!impl_->prefractureResult_.valid ||
       impl_->prefractureResult_.shards.size() !=
           impl_->fractureState_.shards.size())) {
    FractureEffect prefracture;
    prefracture.setSourceBounds(localBounds());
    prefracture.setImpactPoint(localBounds().center());
    prefracture.setSettings(settings);
    if (prefracture.generate()) {
      impl_->prefractureResult_ = prefracture.result();
    }
  }
  syncFragmentDataset(this, impl_->fractureState_,
                      &impl_->prefractureResult_,
                      impl_->componentEvaluationState_);
  emitImpactParticles();
}

} // namespace Artifact
