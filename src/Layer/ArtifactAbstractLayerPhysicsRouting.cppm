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
#include <QPointF>
#include <QSet>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QVector>
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

bool ArtifactAbstractLayer::setComponentPhysicsPropertyValue(
    const QString &propertyPath, const QVariant &value) {
    const auto resyncActiveCollisionPhysics = [this]() {
      if (hasSoftBodyPhysics()) {
        syncSoftBodyPhysicsColliderToBounds();
      }
      if (hasRigidBodyPhysics()) {
        syncRigidBodyPhysicsToBounds();
      }
    };
    if (propertyPath == QStringLiteral("component.collision.enabled")) {
      impl_->collisionComponentEnabled_ = value.toBool();
      impl_->lastCollisionImpactFrame_ =
          std::numeric_limits<int64_t>::min();
      impl_->syncBuiltinComponentDescriptors();
      resyncActiveCollisionPhysics();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.displayColor")) {
      const QColor color = value.value<QColor>();
      if (!color.isValid()) {
        return false;
      }
      impl_->collisionDisplayColor_ = color;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.shape")) {
      impl_->collisionShape_ = std::clamp(value.toInt(), 0, 3);
      impl_->lastCollisionImpactFrame_ =
          std::numeric_limits<int64_t>::min();
      impl_->syncBuiltinComponentDescriptors();
      resyncActiveCollisionPhysics();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.width")) {
      impl_->collisionWidth_ = finiteClampedValue(
          value.toDouble(), impl_->collisionWidth_, 0.0, 100000.0);
      impl_->lastCollisionImpactFrame_ =
          std::numeric_limits<int64_t>::min();
      impl_->syncBuiltinComponentDescriptors();
      resyncActiveCollisionPhysics();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.height")) {
      impl_->collisionHeight_ = finiteClampedValue(
          value.toDouble(), impl_->collisionHeight_, 0.0, 100000.0);
      impl_->lastCollisionImpactFrame_ =
          std::numeric_limits<int64_t>::min();
      impl_->syncBuiltinComponentDescriptors();
      resyncActiveCollisionPhysics();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.radius")) {
      impl_->collisionRadius_ = finiteClampedValue(
          value.toDouble(), impl_->collisionRadius_, 0.0, 100000.0);
      impl_->lastCollisionImpactFrame_ =
          std::numeric_limits<int64_t>::min();
      impl_->syncBuiltinComponentDescriptors();
      resyncActiveCollisionPhysics();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.offsetX")) {
      impl_->collisionOffsetX_ = finiteClampedValue(
          value.toDouble(), impl_->collisionOffsetX_, -100000.0, 100000.0);
      impl_->lastCollisionImpactFrame_ =
          std::numeric_limits<int64_t>::min();
      impl_->syncBuiltinComponentDescriptors();
      resyncActiveCollisionPhysics();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.offsetY")) {
      impl_->collisionOffsetY_ = finiteClampedValue(
          value.toDouble(), impl_->collisionOffsetY_, -100000.0, 100000.0);
      impl_->lastCollisionImpactFrame_ =
          std::numeric_limits<int64_t>::min();
      impl_->syncBuiltinComponentDescriptors();
      resyncActiveCollisionPhysics();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.floorY")) {
      impl_->collisionFloorY_ = finiteClampedValue(
          value.toDouble(), impl_->collisionFloorY_, 0.0, 100000.0);
      impl_->lastCollisionImpactFrame_ =
          std::numeric_limits<int64_t>::min();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.compositionBounds")) {
      impl_->collisionCompositionBounds_ = value.toBool();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.joint.enabled")) {
      impl_->jointComponentEnabled_ = value.toBool();
      impl_->jointBroken_ = false;
      if (impl_->jointComponentEnabled_ && !hasRigidBodyPhysics()) {
        // A joint needs a simulated body; the rigid entry doubles as the
        // previously unreachable enableRigidBodyPhysics() call site.
        enableRigidBodyPhysics();
      }
      if (!impl_->jointComponentEnabled_) {
        if (auto* comp = dynamic_cast<ArtifactAbstractComposition*>(impl_->composition_.data())) {
          if (auto world = LayerPhysics::compositionRigidWorld(comp->id())) {
            world->removeLayerJoint(id());
          }
        }
        if (!impl_->collisionComponentEnabled_) disableRigidBodyPhysics();
      }
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.joint.type")) {
      impl_->jointType_ = std::clamp(value.toInt(), 0, 5);
      impl_->jointBroken_ = false;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.joint.targetLayer")) {
      QString target = value.toString().trimmed();
      if (auto *comp = dynamic_cast<ArtifactAbstractComposition *>(impl_->composition_.data());
          comp && !target.isEmpty()) {
        ArtifactAbstractLayerPtr resolved;
        for (const auto &candidate : comp->allLayer()) {
          if (candidate && candidate->id().toString() == target) { resolved = candidate; break; }
        }
        if (!resolved) {
          for (const auto &candidate : comp->allLayer()) {
            if (candidate && candidate->layerName() == target) {
              if (resolved) return false;
              resolved = candidate;
            }
          }
        }
        if (!resolved || resolved.get() == this) return false;
        target = resolved->id().toString();
      }
impl_->jointTargetLayerName_ =
          (target == id().toString()) ? QString() : target;
      impl_->jointBroken_ = false;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.joint.ownerAnchorX")) {
impl_->jointOwnerAnchorX_ = finiteClampedValue(value.toDouble(), impl_->jointOwnerAnchorX_, -100000.0, 100000.0);
      impl_->jointBroken_ = false;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect, LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.bodyType")) {
      impl_->collisionBodyType_ = std::clamp(value.toInt(), 0, 2);
      impl_->syncBuiltinComponentDescriptors();
      resyncActiveCollisionPhysics();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.joint.ownerAnchorY")) {
impl_->jointOwnerAnchorY_ = finiteClampedValue(value.toDouble(), impl_->jointOwnerAnchorY_, -100000.0, 100000.0);
      impl_->jointBroken_ = false;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect, LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.joint.targetAnchorX")) {
impl_->jointTargetAnchorX_ = finiteClampedValue(value.toDouble(), impl_->jointTargetAnchorX_, -100000.0, 100000.0);
      impl_->jointBroken_ = false;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect, LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.joint.targetAnchorY")) {
impl_->jointTargetAnchorY_ = finiteClampedValue(value.toDouble(), impl_->jointTargetAnchorY_, -100000.0, 100000.0);
      impl_->jointBroken_ = false;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect, LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.joint.length")) {
      impl_->jointLength_ = finiteClampedValue(
          value.toDouble(), impl_->jointLength_, 0.0, 100000.0);
      impl_->jointBroken_ = false;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.joint.stiffness")) {
      impl_->jointStiffness_ = finiteClampedValue(
          value.toDouble(), impl_->jointStiffness_, 0.0, 120.0);
      impl_->jointBroken_ = false;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.joint.damping")) {
      impl_->jointDamping_ = finiteClampedValue(
          value.toDouble(), impl_->jointDamping_, 0.0, 10.0);
      impl_->jointBroken_ = false;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.joint.angleLimitEnabled")) {
impl_->jointAngleLimitEnabled_ = value.toBool();
      impl_->jointBroken_ = false;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.joint.lowerAngle")) {
      impl_->jointLowerAngle_ = finiteClampedValue(
          value.toDouble(), impl_->jointLowerAngle_, -360.0, 360.0);
      impl_->jointBroken_ = false;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.joint.upperAngle")) {
      impl_->jointUpperAngle_ = finiteClampedValue(
          value.toDouble(), impl_->jointUpperAngle_, -360.0, 360.0);
      impl_->jointBroken_ = false;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    const auto setJointFloat = [this, &propertyPath, &value](const QString& expected,
        float& target, double minimum, double maximum) {
      if (propertyPath != expected) return false;
      target = finiteClampedValue(value.toDouble(), target, minimum, maximum);
      impl_->jointBroken_ = false;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect, LayerDirtyReason::PropertyChanged);
      return true;
    };
    if (setJointFloat(QStringLiteral("component.joint.axisX"), impl_->jointAxisX_, -1.0, 1.0) ||
        setJointFloat(QStringLiteral("component.joint.axisY"), impl_->jointAxisY_, -1.0, 1.0) ||
        setJointFloat(QStringLiteral("component.joint.lowerLimit"), impl_->jointLowerLimit_, -100000.0, 100000.0) ||
        setJointFloat(QStringLiteral("component.joint.upperLimit"), impl_->jointUpperLimit_, -100000.0, 100000.0) ||
        setJointFloat(QStringLiteral("component.joint.motorSpeed"), impl_->jointMotorSpeed_, -100000.0, 100000.0) ||
        setJointFloat(QStringLiteral("component.joint.motorForce"), impl_->jointMotorForce_, 0.0, 1000000.0) ||
        setJointFloat(QStringLiteral("component.joint.breakForce"), impl_->jointBreakForce_, 0.0, 1000000.0)) return true;
    if (propertyPath == QStringLiteral("component.joint.linearLimitEnabled") ||
        propertyPath == QStringLiteral("component.joint.motorEnabled")) {
      if (propertyPath == QStringLiteral("component.joint.linearLimitEnabled"))
        impl_->jointLinearLimitEnabled_ = value.toBool();
      else impl_->jointMotorEnabled_ = value.toBool();
      impl_->jointBroken_ = false;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect, LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.crowd.enabled")) {
      impl_->crowdComponentEnabled_ = value.toBool();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.crowd.cohesion")) {
      impl_->crowdCohesion_ = finiteClampedValue(
          value.toDouble(), impl_->crowdCohesion_, 0.0, 10.0);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.crowd.separation")) {
      impl_->crowdSeparation_ = finiteClampedValue(
          value.toDouble(), impl_->crowdSeparation_, 0.0, 10.0);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.crowd.alignment")) {
      impl_->crowdAlignment_ = finiteClampedValue(
          value.toDouble(), impl_->crowdAlignment_, 0.0, 10.0);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.crowd.maxSpeed")) {
      impl_->crowdMaxSpeed_ = finiteClampedValue(
          value.toDouble(), impl_->crowdMaxSpeed_, 0.0, 10000.0);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.crowd.jitter")) {
      impl_->crowdJitter_ = finiteClampedValue(
          value.toDouble(), impl_->crowdJitter_, 0.0, 10.0);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath ==
        QStringLiteral("component.particleEmitter.enabled")) {
      impl_->particleEmitterComponentEnabled_ = value.toBool();
      if (!impl_->particleEmitterComponentEnabled_) {
        impl_->componentParticles_.clear();
        impl_->componentParticlesLastFrame_ =
            std::numeric_limits<int64_t>::min();
      }
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.particleEmitter.count")) {
      impl_->particleEmitterCount_ = std::clamp(value.toInt(), 0, 100000);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.particleEmitter.speed")) {
      impl_->particleEmitterSpeed_ = finiteClampedValue(
          value.toDouble(), impl_->particleEmitterSpeed_, 0.0, 100000.0);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath ==
        QStringLiteral("component.particleEmitter.lifetime")) {
      impl_->particleEmitterLifetime_ =
          finiteClampedValue(value.toDouble(), impl_->particleEmitterLifetime_,
                             0.01, 3600.0);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.enabled")) {
      impl_->fluidComponentEnabled_ = value.toBool();
      if (impl_->fluidComponentEnabled_) {
        impl_->physicsComponent_.authoring().solverKind =
            PhysicsSolverKind::Fluid2D;
      } else if (impl_->physicsComponent_.authoring().solverKind ==
                 PhysicsSolverKind::Fluid2D) {
        impl_->physicsComponent_.authoring().solverKind =
            PhysicsSolverKind::Disabled;
      }
      if (!impl_->fluidComponentEnabled_) {
        impl_->fluidRuntime_.invalidateSmokeSimulation();
        impl_->fluidRuntime_.invalidateLiquidSimulation();
      }
      impl_->syncBuiltinComponentDescriptors();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.mode")) {
      impl_->fluidMode_ = std::clamp(value.toInt(), 0, 1);
      impl_->fluidRuntime_.invalidateSmokeSimulation();
      impl_->fluidRuntime_.invalidateLiquidSimulation();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.gridWidth")) {
      impl_->fluidGridWidth_ = std::clamp(value.toInt(), 8, 4096);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.gridHeight")) {
      impl_->fluidGridHeight_ = std::clamp(value.toInt(), 8, 4096);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.viscosity")) {
      impl_->fluidViscosity_ = finiteClampedValue(
          value.toDouble(), impl_->fluidViscosity_, 0.0, 1.0);
      if (impl_->fluidMode_ == 1) impl_->fluidRuntime_.invalidateLiquidSimulation();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.diffusion")) {
      impl_->fluidDiffusion_ = finiteClampedValue(
          value.toDouble(), impl_->fluidDiffusion_, 0.0, 1.0);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.buoyancy")) {
      impl_->fluidBuoyancy_ = finiteClampedValue(
          value.toDouble(), impl_->fluidBuoyancy_, -2.0, 2.0);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.vorticity")) {
      impl_->fluidVorticity_ = finiteClampedValue(
          value.toDouble(), impl_->fluidVorticity_, 0.0, 10.0);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.solverIterations")) {
      impl_->fluidSolverIterations_ = std::clamp(value.toInt(), 1, 256);
      if (impl_->fluidMode_ == 1) impl_->fluidRuntime_.invalidateLiquidSimulation();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.liquidFillAmount")) {
      impl_->liquidFillAmount_ = finiteClampedValue(
          value.toDouble(), impl_->liquidFillAmount_, 0.0, 1.0);
      impl_->fluidRuntime_.invalidateLiquidSimulation();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.liquidInflowRate")) {
      impl_->liquidInflowRate_ = finiteClampedValue(
          value.toDouble(), impl_->liquidInflowRate_, 0.0, 10000.0);
      impl_->fluidRuntime_.invalidateLiquidSimulation();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.liquidInflowWidth")) {
      impl_->liquidInflowWidth_ = finiteClampedValue(
          value.toDouble(), impl_->liquidInflowWidth_, 0.0, 1.0);
      impl_->fluidRuntime_.invalidateLiquidSimulation();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.liquidInflowSpeed")) {
      impl_->liquidInflowSpeed_ = finiteClampedValue(
          value.toDouble(), impl_->liquidInflowSpeed_, 0.0, 20.0);
      impl_->fluidRuntime_.invalidateLiquidSimulation();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath ==
        QStringLiteral("component.fluid.liquidInflowPosition")) {
      impl_->liquidInflowPosition_ = finiteClampedValue(
          value.toDouble(), impl_->liquidInflowPosition_, 0.0, 1.0);
      impl_->fluidRuntime_.invalidateLiquidSimulation();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.liquidOpeningEdge")) {
      impl_->liquidOpeningEdge_ = std::clamp(value.toInt(), -1, 511);
      impl_->fluidRuntime_.invalidateLiquidSimulation();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath ==
        QStringLiteral("component.fluid.liquidSpillCullMargin")) {
      impl_->liquidSpillCullMargin_ = finiteClampedValue(
          value.toDouble(), impl_->liquidSpillCullMargin_, 0.0, 1000000.0);
      impl_->fluidRuntime_.invalidateLiquidSimulation();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.liquidGravity")) {
      impl_->liquidGravity_ = finiteClampedValue(
          value.toDouble(), impl_->liquidGravity_, 0.0, 20.0);
      impl_->fluidRuntime_.invalidateLiquidSimulation();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath ==
        QStringLiteral("component.fluid.liquidSurfaceTension")) {
      impl_->liquidSurfaceTension_ = finiteClampedValue(
          value.toDouble(), impl_->liquidSurfaceTension_, 0.0, 1.0);
      impl_->fluidRuntime_.invalidateLiquidSimulation();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath ==
        QStringLiteral("component.fluid.liquidParticleSpacing")) {
      impl_->liquidParticleSpacing_ = finiteClampedValue(
          value.toDouble(), impl_->liquidParticleSpacing_, 0.025, 0.2);
      impl_->fluidRuntime_.invalidateLiquidSimulation();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.liquidSubsteps")) {
      impl_->liquidSubsteps_ = std::clamp(value.toInt(), 1, 8);
      impl_->fluidRuntime_.invalidateLiquidSimulation();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath ==
        QStringLiteral("component.fluid.liquidSurfaceOpacity")) {
      impl_->liquidSurfaceOpacity_ = finiteClampedValue(
          value.toDouble(), impl_->liquidSurfaceOpacity_, 0.0, 1.0);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath ==
        QStringLiteral("component.fluid.liquidEdgeOpacity")) {
      impl_->liquidEdgeOpacity_ = finiteClampedValue(
          value.toDouble(), impl_->liquidEdgeOpacity_, 0.0, 1.0);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath ==
        QStringLiteral("component.fluid.liquidFoamAmount")) {
      impl_->liquidFoamAmount_ = finiteClampedValue(
          value.toDouble(), impl_->liquidFoamAmount_, 0.0, 1.0);
      impl_->fluidRuntime_.invalidateLiquidSurface();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath ==
        QStringLiteral("component.fluid.liquidContainerOpacity")) {
      impl_->liquidContainerOpacity_ = finiteClampedValue(
          value.toDouble(), impl_->liquidContainerOpacity_, 0.0, 1.0);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath ==
        QStringLiteral("component.fluid.liquidContainerWidth")) {
      impl_->liquidContainerWidth_ = finiteClampedValue(
          value.toDouble(), impl_->liquidContainerWidth_, 0.0, 64.0);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.liquidColor")) {
      const QColor color = value.value<QColor>();
      if (!color.isValid()) return false;
      impl_->liquidColor_ = FloatColor(
          color.redF(), color.greenF(), color.blueF(), color.alphaF());
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.liquidFoamColor")) {
      const QColor color = value.value<QColor>();
      if (!color.isValid()) return false;
      impl_->liquidFoamColor_ = FloatColor(
          color.redF(), color.greenF(), color.blueF(), color.alphaF());
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    return false;
}

} // namespace Artifact
