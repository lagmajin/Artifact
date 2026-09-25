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
bool applyResponsiveLayoutConstraints(
    const ArtifactAbstractLayer* layer, double& positionX, double& positionY,
    double& scaleX, double& scaleY, double anchorX, double anchorY,
    bool componentEnabled, bool responsiveEnabled, int horizontalPinValue,
    int verticalPinValue, int scaleModeValue, bool safeAreaEnabled,
    double safeAreaPaddingX, double safeAreaPaddingY, double offsetX,
    double offsetY);
QPointF parentAutoLayoutOffset(const ArtifactAbstractLayer* layer,
                               const ArtifactAbstractLayerPtr& parent);
QTransform composeLayerGlobalTransformAt(const ArtifactAbstractLayer* layer,
                                         int64_t frameNumber,
                                         bool layoutComponentEnabled,
                                         bool layoutResponsiveEnabled);
QJsonObject serializeLayerModulationRouter(
    const Audio::Modulation::ModulationRouter& router);
void restoreLayerModulationRouter(
    const QJsonObject& object, Audio::Modulation::ModulationRouter& router);
QJsonObject serializeLayerTransform(
    const ArtifactCore::AnimatableTransform3D& transform);
void restoreLayerTransform(const QJsonObject& object,
                           ArtifactCore::AnimatableTransform3D& transform,
                           double frameRate);

QTransform ArtifactAbstractLayer::getLocalTransform() const {
  const auto &t = transform3D();
  const RationalTime time = currentTimelineTime(this);
  const int64_t frame = impl_->currentFrame_;
  const double fps = effectiveLayerFrameRate(this);
  const auto* var = getActiveVariant();
  bool hasTransVar = var && HasFlag(var->overrideFlags_, VariantOverrideFlags::Transform) && var->transform3DOverride.has_value();

  auto evaluateDouble = [this, &time, hasTransVar](const QString &propertyPath,
                                      double fallback) {
    if (hasTransVar) return fallback;
    const auto handle = getProperty(propertyPath);
    if (!handle) return fallback;
    const auto &property = *handle;
    const QVariant animatedValue = evaluateAnimatedPropertyValue(property, time);
    return animatedValue.isValid() ? animatedValue.toDouble() : fallback;
  };

  const bool useSpatialPosition = !hasTransVar && t.hasPositionSpatialTangents();
  double positionX = useSpatialPosition
      ? t.snapshotAt(time).positionX
      : evaluateDouble(QStringLiteral("transform.position.x"), t.positionX());
  double positionY = useSpatialPosition
      ? t.snapshotAt(time).positionY
      : evaluateDouble(QStringLiteral("transform.position.y"), t.positionY());
  double rotation =
      evaluateDouble(QStringLiteral("transform.rotation"), t.rotation());
  double scaleX =
      evaluateDouble(QStringLiteral("transform.scale.x"), t.scaleX());
  double scaleY =
      evaluateDouble(QStringLiteral("transform.scale.y"), t.scaleY());
  double anchorX =
      evaluateDouble(QStringLiteral("transform.anchor.x"), t.anchorX());
  double anchorY =
      evaluateDouble(QStringLiteral("transform.anchor.y"), t.anchorY());

  // Phase 1 motion modulation (2026-09-22): shared control-rate router output
  // applied non-destructively before dynamics/physics/layout, mirroring the
  // opacity() order (base -> keyframe -> mod Add -> mod Multiply). Skipped
  // entirely when no assignments exist (hot-path guard) or a variant
  // overrides the transform. processAtFrame is idempotent for the frame that
  // goToFrame() already advanced, so preview and render queue stay in sync.
  if (!hasTransVar && !impl_->modulationRouter_.empty()) {
    const int64_t timelineFrame = currentTimelineFrame(this);
    impl_->modulationRouter_.processAtFrame(
        timelineFrame, static_cast<float>(fps > 0.0 ? fps : 30.0));
    auto applyTransformModulation = [this](const char* channel, double& value) {
      const QString path = modulationPropertyPath(QString::fromLatin1(channel));
      if (path.isEmpty()) {
        return;
      }
      const auto target = Audio::Modulation::modulationTargetId(
          path.toStdString());
      if (impl_->modulationRouter_.hasTarget(target)) {
        const float modulated = impl_->modulationRouter_.targetValue(
            target, static_cast<float>(value));
        if (std::isfinite(modulated)) {
          value = static_cast<double>(modulated);
        }
      }
    };
    applyTransformModulation("transform.position.x", positionX);
    applyTransformModulation("transform.position.y", positionY);
    applyTransformModulation("transform.rotation", rotation);
    applyTransformModulation("transform.scale.x", scaleX);
    applyTransformModulation("transform.scale.y", scaleY);
  }

  // Phase 2 automation clips (same order as opacity(): keyframe -> modulation
  // -> clips -> dynamics). ParentFollow uses composition time, FreeTime uses
  // layer-local time; both are deterministic for preview and render queue.
  if (!hasTransVar && !impl_->automationClipInstances_.empty()) {
    const double clipParentSeconds = time.toDouble();
    const double clipFreeSeconds = fps > 0.0
        ? static_cast<double>(impl_->currentFrame_) / fps : 0.0;
    auto applyClipChannel = [&](const char* channel, double& value) {
      const float clipped = applyAutomationClipOverlay(
          this, QString::fromLatin1(channel), static_cast<float>(value),
          clipParentSeconds, clipFreeSeconds);
      if (std::isfinite(clipped)) {
        value = static_cast<double>(clipped);
      }
    };
    applyClipChannel("transform.position.x", positionX);
    applyClipChannel("transform.position.y", positionY);
    applyClipChannel("transform.rotation", rotation);
    applyClipChannel("transform.scale.x", scaleX);
    applyClipChannel("transform.scale.y", scaleY);
  }

  if (impl_->motionDynamicsEnabled_) {
    const bool needsReset = impl_->motionLastFrame_ == std::numeric_limits<int64_t>::min() ||
                            frame != impl_->motionLastFrame_ + 1;
    if (needsReset) {
      impl_->motionX_.reset(static_cast<float>(positionX));
      impl_->motionY_.reset(static_cast<float>(positionY));
      impl_->motionRotation_.reset(static_cast<float>(rotation));
      impl_->motionScaleX_.reset(static_cast<float>(scaleX));
      impl_->motionScaleY_.reset(static_cast<float>(scaleY));
    }

    DynamicsPreset preset{impl_->motionDynamicsStiffness_,
                          impl_->motionDynamicsDamping_,
                          impl_->motionDynamicsMass_};
    const float dt = static_cast<float>(1.0 / std::max(fps, 1.0));
    impl_->motionX_.mode = impl_->motionDynamicsMode_ == 2 ? DynamicsMode::LagFollow : DynamicsMode::Spring;
    impl_->motionY_.mode = impl_->motionDynamicsMode_ == 2 ? DynamicsMode::LagFollow : DynamicsMode::Spring;
    impl_->motionRotation_.mode = impl_->motionDynamicsMode_ == 2 ? DynamicsMode::LagFollow : DynamicsMode::Spring;
    impl_->motionScaleX_.mode = impl_->motionDynamicsMode_ == 2 ? DynamicsMode::LagFollow : DynamicsMode::Spring;
    impl_->motionScaleY_.mode = impl_->motionDynamicsMode_ == 2 ? DynamicsMode::LagFollow : DynamicsMode::Spring;
    impl_->motionX_.preset = preset;
    impl_->motionY_.preset = preset;
    impl_->motionRotation_.preset = preset;
    impl_->motionScaleX_.preset = preset;
    impl_->motionScaleY_.preset = preset;
    impl_->motionX_.lagTau = impl_->motionDynamicsLagTau_;
    impl_->motionY_.lagTau = impl_->motionDynamicsLagTau_;
    impl_->motionRotation_.lagTau = impl_->motionDynamicsLagTau_;
    impl_->motionScaleX_.lagTau = impl_->motionDynamicsLagTau_;
    impl_->motionScaleY_.lagTau = impl_->motionDynamicsLagTau_;
    impl_->motionX_.clampOvershootEnabled = impl_->motionDynamicsClampOvershoot_;
    impl_->motionY_.clampOvershootEnabled = impl_->motionDynamicsClampOvershoot_;
    impl_->motionRotation_.clampOvershootEnabled = impl_->motionDynamicsClampOvershoot_;
    impl_->motionScaleX_.clampOvershootEnabled = impl_->motionDynamicsClampOvershoot_;
    impl_->motionScaleY_.clampOvershootEnabled = impl_->motionDynamicsClampOvershoot_;
    impl_->motionX_.overshootLimit = impl_->motionDynamicsOvershootLimit_;
    impl_->motionY_.overshootLimit = impl_->motionDynamicsOvershootLimit_;
    impl_->motionRotation_.overshootLimit = impl_->motionDynamicsOvershootLimit_;
    impl_->motionScaleX_.overshootLimit = impl_->motionDynamicsOvershootLimit_;
    impl_->motionScaleY_.overshootLimit = impl_->motionDynamicsOvershootLimit_;

    positionX = impl_->motionX_.update(static_cast<float>(positionX), dt);
    positionY = impl_->motionY_.update(static_cast<float>(positionY), dt);
    rotation = impl_->motionRotation_.update(static_cast<float>(rotation), dt);
    scaleX = impl_->motionScaleX_.update(static_cast<float>(scaleX), dt);
    scaleY = impl_->motionScaleY_.update(static_cast<float>(scaleY), dt);
    impl_->motionLastFrame_ = frame;
  }

  const int64_t curFrame = currentTimelineFrame(this);
  if (!impl_->collisionComponentEnabled_ && !impl_->jointComponentEnabled_ &&
      hasRigidBodyPhysics()) {
    const_cast<ArtifactAbstractLayer*>(this)->disableRigidBodyPhysics();
  }
  if ((impl_->collisionComponentEnabled_ || impl_->jointComponentEnabled_) && !hasRigidBodyPhysics()) {
    const_cast<ArtifactAbstractLayer*>(this)->enableRigidBodyPhysics();
  }
  if (impl_->physicsComponent_.enabled() && !hasRigidBodyPhysics()) {
    if (impl_->collisionComponentEnabled_) {
      if (auto* composition =
              dynamic_cast<ArtifactAbstractComposition*>(
                  impl_->composition_.data())) {
        const auto compositionSize =
            composition->settings().compositionSize();
        const QRectF collisionBounds = layerCollisionLocalBounds(this);
        impl_->physicsComponent_.settings().floorY =
            static_cast<float>(compositionSize.height()) -
            static_cast<float>(std::max<qreal>(
                0.0, collisionBounds.bottom()));
      }
    }
    const double fps2 = effectiveLayerFrameRate(this);
    const RationalTime prevTime(curFrame - 1, fps2);
    auto evalAt = [this, &prevTime, &t](const QString &path, double fallback) {
      const auto it = impl_->propertyCache_.constFind(path);
      if (it == impl_->propertyCache_.constEnd() || !it.value()) {
        return fallback;
      }
      const QVariant v = it.value()->interpolateValue(prevTime);
      return v.isValid() ? v.toDouble() : fallback;
    };

    const LayerPhysicsFrameOutput physicsOutput = impl_->physicsComponent_.apply(
        LayerPhysicsFrameInput{
            positionX,
            positionY,
            rotation,
            evalAt(QStringLiteral("transform.position.x"), t.positionX()),
            evalAt(QStringLiteral("transform.position.y"), t.positionY()),
            evalAt(QStringLiteral("transform.rotation"), t.rotation()),
            time.toDouble(),
            fps2,
            curFrame});

    positionX = physicsOutput.positionX;
    positionY = physicsOutput.positionY;
    rotation = physicsOutput.rotation;
    if (physicsOutput.collided &&
        impl_->lastCollisionImpactFrame_ != curFrame) {
      impl_->lastCollisionImpactFrame_ = curFrame;
      FractureImpact impact;
      impact.impulse = std::max(
          0.0f, physicsOutput.collisionSpeed / 100.0f);
      impact.speed = physicsOutput.collisionSpeed;
      impact.stress = impact.impulse;
      const_cast<ArtifactAbstractLayer*>(this)->applyFractureImpact(impact);
    }
  }

  if (hasRigidBodyPhysics()) {
    auto world = LayerPhysics::rigidWorld(id());
    if (auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
            impl_->composition_.data());
        composition) {
      world = LayerPhysics::compositionRigidWorld(composition->id());
    }
    if (world) {
      for (const auto& candidate : world->getBodies()) {
        if (!candidate || candidate->cloneIndex < -1 ||
            candidate->ownerLayerId != id()) {
          continue;
        }
        const QVector2D bodyPos = candidate->position();
        const auto& physicsSettings = impl_->physicsComponent_.settings();
        if (physicsSettings.windEnabled &&
            impl_->lastRigidWindForceFrame_ != curFrame) {
          float fieldWeight = 1.0f;
          const auto fieldChannels =
              compositionFieldChannelsAtCanvasPoint(QPointF(bodyPos.x(), bodyPos.y()));
          if (fieldChannels.affected) {
            fieldWeight = fieldChannels.weight;
          }
          QVector2D windDirection(physicsSettings.windX, physicsSettings.windY);
          if (windDirection.lengthSquared() > 0.000001f) {
            windDirection.normalize();
          } else {
            windDirection = QVector2D();
          }
          const QVector2D windForce = windDirection *
              (physicsSettings.windStrength * fieldWeight);
          candidate->applyForce(windForce, bodyPos);
          candidate->applyTorque(
              physicsSettings.windTorque * physicsSettings.windStrength * fieldWeight);
          impl_->lastRigidWindForceFrame_ = curFrame;
        }
        return impl_->rigidBodyLocalTransform(this, candidate);
      }
    }
  }

  applyCompositionTransformFields(this, positionX, positionY, scaleX, scaleY);
  applyResponsiveLayoutConstraints(this, positionX, positionY, scaleX, scaleY,
                                   anchorX, anchorY, impl_->layoutComponentEnabled_,
                                   impl_->layoutResponsiveEnabled_,
                                   impl_->layoutHorizontalPin_, impl_->layoutVerticalPin_,
                                   impl_->layoutScaleMode_, impl_->layoutSafeAreaEnabled_,
                                   impl_->layoutSafeAreaPaddingX_, impl_->layoutSafeAreaPaddingY_,
                                   impl_->layoutResponsiveOffsetX_, impl_->layoutResponsiveOffsetY_);
  QTransform transform = makeLayerTransform2D(positionX, positionY, rotation, scaleX, scaleY,
                                              anchorX, anchorY);
  transform = impl_->modifiers_.apply(transform, localBounds(), time.toDouble());
  return transform;
}

QTransform ArtifactAbstractLayerImpl::rigidBodyLocalTransform(
    const ArtifactAbstractLayer* layer,
    const ArtifactCore::SharedPtr<ArtifactCore::RigidBody2D>& body) const {
  const auto position = body->position();
  QTransform delta;
  delta.translate(position.x(), position.y());
  delta.rotateRadians(body->angle() - rigidBodyRestAngle_);
  delta.translate(-rigidBodyRestCenter_.x(), -rigidBodyRestCenter_.y());
  QTransform result = rigidBodyRestTransform_ * delta;
  if (const auto parent = layer->parentLayer()) {
    bool invertible = false;
    const auto inverseParent = parent->getGlobalTransform().inverted(&invertible);
    if (invertible) result = result * inverseParent;
    const QPointF offset = layoutComponentEnabled_ && layoutResponsiveEnabled_
        ? QPointF() : parentAutoLayoutOffset(layer, parent);
    result = QTransform::fromTranslate(-offset.x(), -offset.y()) * result;
  }
  return result;
}

QTransform ArtifactAbstractLayer::getGlobalTransform() const {
  auto parent = parentLayer();
  const LayerID parentId = impl_->parentLayerId_;
  const quint64 parentRevision = parent ? parent->impl_->geometryRevision_ : 0;
  const int64_t frame = impl_->currentFrame_;
  const auto layoutEnabledProperty = parent
                                         ? parent->getProperty(
                                               QStringLiteral("component.layout.enabled"))
                                         : nullptr;
  const auto participationProperty = getProperty(
      QStringLiteral("component.layout.mode"));
  const bool responsiveLayout = impl_->layoutComponentEnabled_ &&
                                impl_->layoutResponsiveEnabled_;
  const bool layoutManaged = layoutEnabledProperty &&
                             layoutEnabledProperty->getValue().toBool() &&
                             !responsiveLayout &&
                             (!participationProperty ||
                              participationProperty->getValue().toInt() != 2);
  const QPointF layoutOffset = layoutManaged
                                   ? parentAutoLayoutOffset(this, parent)
                                   : QPointF();
  if (!hasRigidBodyPhysics() && !(parent && parent->hasRigidBodyPhysics()) &&
      !layoutManaged && !responsiveLayout &&
      impl_->cachedGlobalTransformRevision_ == impl_->geometryRevision_ &&
      impl_->cachedGlobalTransformParentRevision_ == parentRevision &&
      impl_->cachedGlobalTransformFrame_ == frame &&
      impl_->cachedGlobalTransformParentId_ == parentId) {
    return impl_->cachedGlobalTransform_;
  }
  QTransform local = getLocalTransform();
  if (layoutManaged) {
    local = QTransform::fromTranslate(layoutOffset.x(), layoutOffset.y()) * local;
  }
  impl_->cachedGlobalTransform_ =
      parent ? combineLayerTransform2D(local, parent->getGlobalTransform())
             : local;
  impl_->cachedGlobalTransformRevision_ = impl_->geometryRevision_;
  impl_->cachedGlobalTransformParentRevision_ = parentRevision;
  impl_->cachedGlobalTransformFrame_ = frame;
  impl_->cachedGlobalTransformParentId_ = parentId;
  return impl_->cachedGlobalTransform_;
}

QTransform ArtifactAbstractLayer::getLocalTransformAt(int64_t frameNumber) const {
  const auto &t = transform3D();
  const RationalTime time(frameNumber,
      ArtifactCore::FrameRate::storageScaleForFps(effectiveLayerFrameRate(this)));
  const auto* var = getActiveVariant();
  bool hasTransVar = var && HasFlag(var->overrideFlags_, VariantOverrideFlags::Transform) && var->transform3DOverride.has_value();

  auto evaluateDouble = [this, &time, frameNumber, hasTransVar](const QString &propertyPath,
                                                                  double fallback) {
    if (hasTransVar) return fallback;
    double evaluated = fallback;
    if (const auto handle = getProperty(propertyPath)) {
      const auto &property = *handle;
      if (property.isAnimatable() || property.hasExpression() ||
          property.hasEnvelopes() || property.hasExternalOverride()) {
        const QVariant animatedValue = evaluateAnimatedPropertyValue(property, time);
        if (animatedValue.isValid()) {
          evaluated = animatedValue.toDouble();
        }
      }
    }
    if (const auto *stack = animationLayerStack(propertyPath);
        stack && stack->layerCount() > 0) {
      evaluated = stack->evaluateWithBase(FramePosition(frameNumber),
                                          static_cast<float>(evaluated));
    }
    return evaluated;
  };

  const bool useSpatialPosition = !hasTransVar && t.hasPositionSpatialTangents();
  double positionX = useSpatialPosition
      ? t.snapshotAt(time).positionX
      : evaluateDouble(QStringLiteral("transform.position.x"), t.positionXAt(time));
  double positionY = useSpatialPosition
      ? t.snapshotAt(time).positionY
      : evaluateDouble(QStringLiteral("transform.position.y"), t.positionYAt(time));
  if (useSpatialPosition) {
    if (const auto *stack = animationLayerStack(QStringLiteral("transform.position.x"));
        stack && stack->layerCount() > 0) {
      positionX = stack->evaluateWithBase(FramePosition(frameNumber),
                                           static_cast<float>(positionX));
    }
    if (const auto *stack = animationLayerStack(QStringLiteral("transform.position.y"));
        stack && stack->layerCount() > 0) {
      positionY = stack->evaluateWithBase(FramePosition(frameNumber),
                                           static_cast<float>(positionY));
    }
  }
  double rotation = evaluateDouble(QStringLiteral("transform.rotation"), t.rotationAt(time));
  double scaleX = evaluateDouble(QStringLiteral("transform.scale.x"), t.scaleXAt(time));
  double scaleY = evaluateDouble(QStringLiteral("transform.scale.y"), t.scaleYAt(time));
  const double anchorX = evaluateDouble(QStringLiteral("transform.anchor.x"), t.anchorXAt(time));
  const double anchorY = evaluateDouble(QStringLiteral("transform.anchor.y"), t.anchorYAt(time));

  const int64_t frame = currentTimelineFrame(this);
  const double fps = effectiveLayerFrameRate(this);
  if (impl_->motionDynamicsEnabled_) {
    const bool needsReset = impl_->motionLastFrame_ == std::numeric_limits<int64_t>::min() ||
                            frame != impl_->motionLastFrame_ + 1;
    if (needsReset) {
      impl_->motionX_.reset(static_cast<float>(positionX));
      impl_->motionY_.reset(static_cast<float>(positionY));
      impl_->motionRotation_.reset(static_cast<float>(rotation));
      impl_->motionScaleX_.reset(static_cast<float>(scaleX));
      impl_->motionScaleY_.reset(static_cast<float>(scaleY));
    }

    DynamicsPreset preset{impl_->motionDynamicsStiffness_,
                          impl_->motionDynamicsDamping_,
                          impl_->motionDynamicsMass_};
    const float dt = static_cast<float>(1.0 / std::max(fps, 1.0));
    impl_->motionX_.mode = impl_->motionDynamicsMode_ == 2 ? DynamicsMode::LagFollow : DynamicsMode::Spring;
    impl_->motionY_.mode = impl_->motionDynamicsMode_ == 2 ? DynamicsMode::LagFollow : DynamicsMode::Spring;
    impl_->motionRotation_.mode = impl_->motionDynamicsMode_ == 2 ? DynamicsMode::LagFollow : DynamicsMode::Spring;
    impl_->motionScaleX_.mode = impl_->motionDynamicsMode_ == 2 ? DynamicsMode::LagFollow : DynamicsMode::Spring;
    impl_->motionScaleY_.mode = impl_->motionDynamicsMode_ == 2 ? DynamicsMode::LagFollow : DynamicsMode::Spring;
    impl_->motionX_.preset = preset;
    impl_->motionY_.preset = preset;
    impl_->motionRotation_.preset = preset;
    impl_->motionScaleX_.preset = preset;
    impl_->motionScaleY_.preset = preset;
    impl_->motionX_.lagTau = impl_->motionDynamicsLagTau_;
    impl_->motionY_.lagTau = impl_->motionDynamicsLagTau_;
    impl_->motionRotation_.lagTau = impl_->motionDynamicsLagTau_;
    impl_->motionScaleX_.lagTau = impl_->motionDynamicsLagTau_;
    impl_->motionScaleY_.lagTau = impl_->motionDynamicsLagTau_;
    impl_->motionX_.clampOvershootEnabled = impl_->motionDynamicsClampOvershoot_;
    impl_->motionY_.clampOvershootEnabled = impl_->motionDynamicsClampOvershoot_;
    impl_->motionRotation_.clampOvershootEnabled = impl_->motionDynamicsClampOvershoot_;
    impl_->motionScaleX_.clampOvershootEnabled = impl_->motionDynamicsClampOvershoot_;
    impl_->motionScaleY_.clampOvershootEnabled = impl_->motionDynamicsClampOvershoot_;
    impl_->motionX_.overshootLimit = impl_->motionDynamicsOvershootLimit_;
    impl_->motionY_.overshootLimit = impl_->motionDynamicsOvershootLimit_;
    impl_->motionRotation_.overshootLimit = impl_->motionDynamicsOvershootLimit_;
    impl_->motionScaleX_.overshootLimit = impl_->motionDynamicsOvershootLimit_;
    impl_->motionScaleY_.overshootLimit = impl_->motionDynamicsOvershootLimit_;

    positionX = impl_->motionX_.update(static_cast<float>(positionX), dt);
    positionY = impl_->motionY_.update(static_cast<float>(positionY), dt);
    rotation = impl_->motionRotation_.update(static_cast<float>(rotation), dt);
    impl_->motionLastFrame_ = frame;
  }

  // Skip physics for random access evaluating (e.g. motion path rendering) to maintain determinism.

  applyCompositionTransformFields(this, positionX, positionY, scaleX, scaleY);
  applyResponsiveLayoutConstraints(this, positionX, positionY, scaleX, scaleY,
                                   anchorX, anchorY, impl_->layoutComponentEnabled_,
                                   impl_->layoutResponsiveEnabled_,
                                   impl_->layoutHorizontalPin_, impl_->layoutVerticalPin_,
                                   impl_->layoutScaleMode_, impl_->layoutSafeAreaEnabled_,
                                   impl_->layoutSafeAreaPaddingX_, impl_->layoutSafeAreaPaddingY_,
                                   impl_->layoutResponsiveOffsetX_, impl_->layoutResponsiveOffsetY_);
  QTransform transform = makeLayerTransform2D(positionX, positionY, rotation, scaleX, scaleY,
                                              anchorX, anchorY);
  transform = impl_->modifiers_.apply(transform, localBounds(), time.toDouble());
  return transform;
}

QTransform ArtifactAbstractLayer::getGlobalTransformAt(int64_t frameNumber) const {
  return composeLayerGlobalTransformAt(
      this, frameNumber, impl_->layoutComponentEnabled_,
      impl_->layoutResponsiveEnabled_);
}

QMatrix4x4 ArtifactAbstractLayer::getLocalTransform4x4() const {
  const auto &t = transform3D();
  const RationalTime time = currentTimelineTime(this);
  if (is3D()) {
    const auto snapshot = t.snapshotAt(time);
    QMatrix4x4 result;
    result.setToIdentity();
    result.translate(snapshot.positionX, snapshot.positionY, snapshot.positionZ);
    result.rotate(snapshot.rotationX, 1.0f, 0.0f, 0.0f);
    result.rotate(snapshot.rotationY, 0.0f, 1.0f, 0.0f);
    result.rotate(snapshot.rotationZ, 0.0f, 0.0f, 1.0f);
    result.scale(snapshot.scaleX, snapshot.scaleY, snapshot.scaleZ);
    result.translate(-snapshot.anchorX, -snapshot.anchorY, -snapshot.anchorZ);
    return result;
  }
  const int64_t frame = impl_->currentFrame_;
  const double fps = effectiveLayerFrameRate(this);
  auto evaluateDouble = [this, &time](const QString &propertyPath,
                                      double fallback) {
    const auto handle = getProperty(propertyPath);
    if (!handle) return fallback;
    const auto &property = *handle;
    const QVariant animatedValue = evaluateAnimatedPropertyValue(property, time);
    return animatedValue.isValid() ? animatedValue.toDouble() : fallback;
  };
  const bool useSpatialPosition = t.hasPositionSpatialTangents();
  double positionX = useSpatialPosition
      ? t.snapshotAt(time).positionX
      : evaluateDouble(QStringLiteral("transform.position.x"), t.positionX());
  double positionY = useSpatialPosition
      ? t.snapshotAt(time).positionY
      : evaluateDouble(QStringLiteral("transform.position.y"), t.positionY());
  const double positionZ = t.positionZAt(time);
  double rotation =
      evaluateDouble(QStringLiteral("transform.rotation"), t.rotation());
  double scaleX =
      evaluateDouble(QStringLiteral("transform.scale.x"), t.scaleX());
  double scaleY =
      evaluateDouble(QStringLiteral("transform.scale.y"), t.scaleY());
  const double anchorX =
      evaluateDouble(QStringLiteral("transform.anchor.x"), t.anchorXAt(time));
  const double anchorY =
      evaluateDouble(QStringLiteral("transform.anchor.y"), t.anchorYAt(time));
  const double anchorZ = t.anchorZAt(time);

  if (impl_->motionDynamicsEnabled_) {
    const bool needsReset = impl_->motionLastFrame_ == std::numeric_limits<int64_t>::min() ||
                            frame != impl_->motionLastFrame_ + 1;
    if (needsReset) {
      impl_->motionX_.reset(static_cast<float>(positionX));
      impl_->motionY_.reset(static_cast<float>(positionY));
      impl_->motionRotation_.reset(static_cast<float>(rotation));
      impl_->motionScaleX_.reset(static_cast<float>(scaleX));
      impl_->motionScaleY_.reset(static_cast<float>(scaleY));
    }

    DynamicsPreset preset{impl_->motionDynamicsStiffness_,
                          impl_->motionDynamicsDamping_,
                          impl_->motionDynamicsMass_};
    const float dt = static_cast<float>(1.0 / std::max(fps, 1.0));
    impl_->motionX_.mode = impl_->motionDynamicsMode_ == 2 ? DynamicsMode::LagFollow : DynamicsMode::Spring;
    impl_->motionY_.mode = impl_->motionDynamicsMode_ == 2 ? DynamicsMode::LagFollow : DynamicsMode::Spring;
    impl_->motionRotation_.mode = impl_->motionDynamicsMode_ == 2 ? DynamicsMode::LagFollow : DynamicsMode::Spring;
    impl_->motionScaleX_.mode = impl_->motionDynamicsMode_ == 2 ? DynamicsMode::LagFollow : DynamicsMode::Spring;
    impl_->motionScaleY_.mode = impl_->motionDynamicsMode_ == 2 ? DynamicsMode::LagFollow : DynamicsMode::Spring;
    impl_->motionX_.preset = preset;
    impl_->motionY_.preset = preset;
    impl_->motionRotation_.preset = preset;
    impl_->motionScaleX_.preset = preset;
    impl_->motionScaleY_.preset = preset;
    impl_->motionX_.lagTau = impl_->motionDynamicsLagTau_;
    impl_->motionY_.lagTau = impl_->motionDynamicsLagTau_;
    impl_->motionRotation_.lagTau = impl_->motionDynamicsLagTau_;
    impl_->motionScaleX_.lagTau = impl_->motionDynamicsLagTau_;
    impl_->motionScaleY_.lagTau = impl_->motionDynamicsLagTau_;
    impl_->motionX_.clampOvershootEnabled = impl_->motionDynamicsClampOvershoot_;
    impl_->motionY_.clampOvershootEnabled = impl_->motionDynamicsClampOvershoot_;
    impl_->motionRotation_.clampOvershootEnabled = impl_->motionDynamicsClampOvershoot_;
    impl_->motionScaleX_.clampOvershootEnabled = impl_->motionDynamicsClampOvershoot_;
    impl_->motionScaleY_.clampOvershootEnabled = impl_->motionDynamicsClampOvershoot_;
    impl_->motionX_.overshootLimit = impl_->motionDynamicsOvershootLimit_;
    impl_->motionY_.overshootLimit = impl_->motionDynamicsOvershootLimit_;
    impl_->motionRotation_.overshootLimit = impl_->motionDynamicsOvershootLimit_;
    impl_->motionScaleX_.overshootLimit = impl_->motionDynamicsOvershootLimit_;
    impl_->motionScaleY_.overshootLimit = impl_->motionDynamicsOvershootLimit_;

    positionX = impl_->motionX_.update(static_cast<float>(positionX), dt);
    positionY = impl_->motionY_.update(static_cast<float>(positionY), dt);
    rotation = impl_->motionRotation_.update(static_cast<float>(rotation), dt);
    impl_->motionLastFrame_ = frame;
  }

  if (!impl_->collisionComponentEnabled_ && !impl_->jointComponentEnabled_ &&
      hasRigidBodyPhysics()) {
    const_cast<ArtifactAbstractLayer*>(this)->disableRigidBodyPhysics();
  }
  if ((impl_->collisionComponentEnabled_ || impl_->jointComponentEnabled_) && !hasRigidBodyPhysics()) {
    const_cast<ArtifactAbstractLayer*>(this)->enableRigidBodyPhysics();
  }
  if (impl_->physicsComponent_.enabled() && !hasRigidBodyPhysics()) {
    if (impl_->collisionComponentEnabled_) {
      if (auto* composition =
              dynamic_cast<ArtifactAbstractComposition*>(
                  impl_->composition_.data())) {
        const auto compositionSize =
            composition->settings().compositionSize();
        const QRectF collisionBounds = layerCollisionLocalBounds(this);
        impl_->physicsComponent_.settings().floorY =
            static_cast<float>(compositionSize.height()) -
            static_cast<float>(std::max<qreal>(
                0.0, collisionBounds.bottom()));
      }
    }
    const double fps = effectiveLayerFrameRate(this);
    const int64_t curFrame = currentTimelineFrame(this);
    const RationalTime prevTime(curFrame - 1, fps);
    auto evalAt = [this, &prevTime, &t](const QString &path, double fallback) {
      const auto it = impl_->propertyCache_.constFind(path);
      if (it == impl_->propertyCache_.constEnd() || !it.value()) {
        return fallback;
      }
      const QVariant v = it.value()->interpolateValue(prevTime);
      return v.isValid() ? v.toDouble() : fallback;
    };

    const LayerPhysicsFrameOutput physicsOutput = impl_->physicsComponent_.apply(
        LayerPhysicsFrameInput{
            positionX,
            positionY,
            rotation,
            evalAt(QStringLiteral("transform.position.x"), t.positionXAt(prevTime)),
            evalAt(QStringLiteral("transform.position.y"), t.positionYAt(prevTime)),
            evalAt(QStringLiteral("transform.rotation"), t.rotationAt(prevTime)),
            time.toDouble(),
            fps,
            curFrame});

    positionX = physicsOutput.positionX;
    positionY = physicsOutput.positionY;
    rotation = physicsOutput.rotation;
    if (physicsOutput.collided &&
        impl_->lastCollisionImpactFrame_ != curFrame) {
      impl_->lastCollisionImpactFrame_ = curFrame;
      FractureImpact impact;
      impact.impulse = std::max(
          0.0f, physicsOutput.collisionSpeed / 100.0f);
      impact.speed = physicsOutput.collisionSpeed;
      impact.stress = impact.impulse;
      const_cast<ArtifactAbstractLayer*>(this)->applyFractureImpact(impact);
    }
  }

  if (hasRigidBodyPhysics()) {
    auto world = LayerPhysics::rigidWorld(id());
    if (auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
            impl_->composition_.data());
        composition) {
      world = LayerPhysics::compositionRigidWorld(composition->id());
    }
    if (world) {
      // cloneIndex == -2 marks joint static proxies; the layer's own dynamic
      // body keeps the default -1 and drives the transform.
      const auto bodies = world->getBodies();
      for (const auto& candidate : bodies) {
        if (!candidate || candidate->cloneIndex < -1 ||
            candidate->ownerLayerId != id()) {
          continue;
        }
        QMatrix4x4 result = matrixFromTransform2D(
            impl_->rigidBodyLocalTransform(this, candidate));
        result.translate(0.0f, 0.0f, static_cast<float>(positionZ));
        return result;
      }
    }
  }

  applyCompositionTransformFields(this, positionX, positionY, scaleX, scaleY);
  applyResponsiveLayoutConstraints(this, positionX, positionY, scaleX, scaleY,
                                   anchorX, anchorY, impl_->layoutComponentEnabled_,
                                   impl_->layoutResponsiveEnabled_,
                                   impl_->layoutHorizontalPin_, impl_->layoutVerticalPin_,
                                   impl_->layoutScaleMode_, impl_->layoutSafeAreaEnabled_,
                                   impl_->layoutSafeAreaPaddingX_, impl_->layoutSafeAreaPaddingY_,
                                   impl_->layoutResponsiveOffsetX_, impl_->layoutResponsiveOffsetY_);
  const QTransform local2D = impl_->modifiers_.apply(
      makeLayerTransform2D(positionX, positionY, rotation, scaleX, scaleY,
                           anchorX, anchorY),
      localBounds(), time.toDouble());

  Q_UNUSED(anchorZ);
  QMatrix4x4 result = matrixFromTransform2D(local2D);
  if (positionZ != 0.0) {
    result.translate(0.0f, 0.0f, static_cast<float>(positionZ));
  }
  return result;
}

} // namespace Artifact
