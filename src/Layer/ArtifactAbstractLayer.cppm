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

QJsonObject serializeLayerModulationRouter(
    const Audio::Modulation::ModulationRouter& router);
void restoreLayerModulationRouter(
    const QJsonObject& object, Audio::Modulation::ModulationRouter& router);
QJsonObject serializeLayerTransform(
    const ArtifactCore::AnimatableTransform3D& transform);
void restoreLayerTransform(const QJsonObject& object,
                           ArtifactCore::AnimatableTransform3D& transform,
                           double frameRate);

using float4x4 = Diligent::float4x4;

W_OBJECT_IMPL(ArtifactAbstractLayer)

// Implemented in ArtifactAbstractLayerCollision.cppm (same module).
NamedVector<QPointF> layerCollisionPolygonLocalPoints(
    const ArtifactAbstractLayer* layer);
bool configureLiquidContainerPolygon(
    const ArtifactAbstractLayer* layer, ArtifactCore::LiquidSolver2D& liquid,
    int requestedOpeningEdge = -1,
    NamedVector<QPointF>* configuredPoints = nullptr,
    std::size_t* configuredOpeningEdge = nullptr);
QRectF layerCollisionLocalBounds(const ArtifactAbstractLayer* layer);
bool resolveLiquidPointAgainstCollisionLayer(
    const ArtifactAbstractLayer* layer, int64_t frameNumber,
    float particleRadius, float previousWorldX, float previousWorldY,
    float& worldX, float& worldY,
    float& worldVx, float& worldVy, float& collisionImpact);

// Implemented in ArtifactLayerTimelineSupport.cppm. These helpers use only
// the exported layer/composition APIs, so they do not require Impl visibility.
void applyCompositionTransformFields(
    const ArtifactAbstractLayer* layer, double& positionX, double& positionY,
    double& scaleX, double& scaleY);
double effectiveLayerFrameRate(const ArtifactAbstractLayer* layer);
int64_t currentTimelineFrame(const ArtifactAbstractLayer* layer);
RationalTime currentTimelineTime(const ArtifactAbstractLayer* layer);
RationalTime timelineTimeForFramePosition(const ArtifactAbstractLayer* layer,
                                          const FramePosition& position);
void applyMaskPropertyState(const ArtifactAbstractLayer* layer, int maskIndex,
                            LayerMask& mask);
NamedVector<TwoPointFiveDRenderPass> buildTwoPointFiveDRenderPasses(
    const ArtifactAbstractLayer* layer, const QMatrix4x4& baseTransform,
    bool enabled, float configuredDepth, float configuredCameraDistance,
    bool depthOfFieldEnabled, float focusDepth, float focusRange, float maxBlur,
    bool motionBlurEnabled, float shutterAngle, int configuredMotionSamples);
QTransform composeLayerGlobalTransformAt(const ArtifactAbstractLayer* layer,
                                         int64_t frameNumber,
                                         bool layoutComponentEnabled,
                                         bool layoutResponsiveEnabled);

namespace {
template <typename T> bool assignIfChanged(T &current, const T &next) {
  if (current == next) {
    return false;
  }
  current = next;
  return true;
}
} // namespace

void notifyLayerMutation(ArtifactAbstractLayer *layer, LayerDirtyFlag flag,
                         LayerDirtyReason reason) {
  if (!layer) {
    return;
  }
  layer->setDirty(flag);
  layer->addDirtyReason(reason);
  layer->changed();
}


namespace {
bool g_globalLayerCacheEnabled = true;
}

ArtifactAbstractLayerImpl::ArtifactAbstractLayerImpl() {
  // Avoid undefined draw bounds when a layer is queried before explicit size
  // assignment.
  sourceSize_ = Size_2D(1920, 1080);
  syncBuiltinComponentDescriptors();
}

ArtifactAbstractLayerImpl::~ArtifactAbstractLayerImpl() {}

void ArtifactAbstractLayerImpl::syncBuiltinComponentDescriptors() {
  physicsComponent_.settings().collisionEnabled =
      collisionComponentEnabled_;
  if (collisionComponentEnabled_ && !physicsComponent_.enabled()) {
    physicsComponent_.setEnabled(true);
    collisionOwnsPhysicsEnable_ = true;
  } else if (!collisionComponentEnabled_ &&
             collisionOwnsPhysicsEnable_) {
    physicsComponent_.setEnabled(false);
    collisionOwnsPhysicsEnable_ = false;
  }

  auto cloner = makeClonerComponentDescriptor(clonerComponentEnabled_);
  cloner.settings[QStringLiteral("mode")] = clonerMode_;
  cloner.settings[QStringLiteral("count")] = clonerCloneCount_;
  cloner.settings[QStringLiteral("seed")] = clonerSeed_;
  cloner.settings[QStringLiteral("timeOffsetStep")] =
      static_cast<double>(clonerTimeOffsetStep_);
  cloner.settings[QStringLiteral("sequenceEnabled")] =
      clonerSequenceEnabled_;
  cloner.settings[QStringLiteral("sequenceRate")] =
      static_cast<double>(clonerSequenceRate_);
  cloner.settings[QStringLiteral("sequenceSoftness")] =
      static_cast<double>(clonerSequenceSoftness_);
  componentHost_.upsert(std::move(cloner));

  auto layout = makeLayoutComponentDescriptor(layoutComponentEnabled_);
  layout.settings[QStringLiteral("mode")] = layoutMode_;
  layout.settings[QStringLiteral("responsiveEnabled")] = layoutResponsiveEnabled_;
  layout.settings[QStringLiteral("horizontalPin")] = layoutHorizontalPin_;
  layout.settings[QStringLiteral("verticalPin")] = layoutVerticalPin_;
  layout.settings[QStringLiteral("scaleMode")] = layoutScaleMode_;
  layout.settings[QStringLiteral("offsetX")] = static_cast<double>(layoutResponsiveOffsetX_);
  layout.settings[QStringLiteral("offsetY")] = static_cast<double>(layoutResponsiveOffsetY_);
  layout.settings[QStringLiteral("safeAreaEnabled")] = layoutSafeAreaEnabled_;
  layout.settings[QStringLiteral("safeAreaPaddingX")] =
      static_cast<double>(layoutSafeAreaPaddingX_);
  layout.settings[QStringLiteral("safeAreaPaddingY")] =
      static_cast<double>(layoutSafeAreaPaddingY_);
  layout.settings[QStringLiteral("gap")] = static_cast<double>(layoutGap_);
  layout.settings[QStringLiteral("maxPerRow")] = layoutMaxPerRow_;
  componentHost_.upsert(std::move(layout));

  auto crowd = makeCrowdComponentDescriptor(crowdComponentEnabled_);
  crowd.settings[QStringLiteral("cohesion")] =
      static_cast<double>(crowdCohesion_);
  crowd.settings[QStringLiteral("separation")] =
      static_cast<double>(crowdSeparation_);
  crowd.settings[QStringLiteral("alignment")] =
      static_cast<double>(crowdAlignment_);
  crowd.settings[QStringLiteral("maxSpeed")] =
      static_cast<double>(crowdMaxSpeed_);
  crowd.settings[QStringLiteral("jitter")] =
      static_cast<double>(crowdJitter_);
  componentHost_.upsert(std::move(crowd));

  auto motion =
      makeMotionDynamicsComponentDescriptor(
          physicsComponent_.enabled() || motionDynamicsEnabled_);
  motion.settings[QStringLiteral("layerSpringEnabled")] =
      physicsComponent_.enabled();
  motion.settings[QStringLiteral("followThroughEnabled")] =
      motionDynamicsEnabled_;
  componentHost_.upsert(std::move(motion));

  auto sequencePlayer = makeSequencePlayerComponentDescriptor(false);
  sequencePlayer.settings[QStringLiteral("sequenceSource")] =
      QStringLiteral("inline");
  sequencePlayer.settings[QStringLiteral("targetScope")] =
      QStringLiteral("children");
  sequencePlayer.settings[QStringLiteral("trigger")] =
      QStringLiteral("on-start");
  componentHost_.upsert(std::move(sequencePlayer));

  auto collision =
      makeCollisionComponentDescriptor(collisionComponentEnabled_);
  collision.settings[QStringLiteral("shape")] = collisionShape_;
  collision.settings[QStringLiteral("bodyType")] = collisionBodyType_;
  collision.settings[QStringLiteral("width")] =
      static_cast<double>(collisionWidth_);
  collision.settings[QStringLiteral("height")] =
      static_cast<double>(collisionHeight_);
  collision.settings[QStringLiteral("radius")] =
      static_cast<double>(collisionRadius_);
  collision.settings[QStringLiteral("offsetX")] =
      static_cast<double>(collisionOffsetX_);
  collision.settings[QStringLiteral("offsetY")] =
      static_cast<double>(collisionOffsetY_);
  collision.settings[QStringLiteral("floorY")] =
      static_cast<double>(collisionFloorY_);
  collision.settings[QStringLiteral("compositionBounds")] =
      collisionCompositionBounds_;
  collision.settings[QStringLiteral("displayColor")] =
      collisionDisplayColor_.name(QColor::HexArgb);
  componentHost_.upsert(std::move(collision));

  auto joint = makeJointComponentDescriptor(jointComponentEnabled_);
  joint.settings[QStringLiteral("type")] = jointType_;
  joint.settings[QStringLiteral("targetLayer")] = jointTargetLayerName_;
  joint.settings[QStringLiteral("targetAnchorY")] = jointTargetAnchorY_;
  joint.settings[QStringLiteral("targetAnchorX")] = jointTargetAnchorX_;
  joint.settings[QStringLiteral("ownerAnchorY")] = jointOwnerAnchorY_;
  joint.settings[QStringLiteral("ownerAnchorX")] = jointOwnerAnchorX_;
  joint.settings[QStringLiteral("length")] =
      static_cast<double>(jointLength_);
  joint.settings[QStringLiteral("stiffness")] =
      static_cast<double>(jointStiffness_);
  joint.settings[QStringLiteral("damping")] =
      static_cast<double>(jointDamping_);
  joint.settings[QStringLiteral("angleLimitEnabled")] = jointAngleLimitEnabled_;
  joint.settings[QStringLiteral("lowerAngle")] = static_cast<double>(jointLowerAngle_);
  joint.settings[QStringLiteral("upperAngle")] = static_cast<double>(jointUpperAngle_);
  joint.settings[QStringLiteral("axisX")] = static_cast<double>(jointAxisX_);
  joint.settings[QStringLiteral("axisY")] = static_cast<double>(jointAxisY_);
  joint.settings[QStringLiteral("linearLimitEnabled")] = jointLinearLimitEnabled_;
  joint.settings[QStringLiteral("lowerLimit")] = static_cast<double>(jointLowerLimit_);
  joint.settings[QStringLiteral("upperLimit")] = static_cast<double>(jointUpperLimit_);
  joint.settings[QStringLiteral("motorEnabled")] = jointMotorEnabled_;
  joint.settings[QStringLiteral("motorSpeed")] = static_cast<double>(jointMotorSpeed_);
  joint.settings[QStringLiteral("motorForce")] = static_cast<double>(jointMotorForce_);
  joint.settings[QStringLiteral("breakForce")] = static_cast<double>(jointBreakForce_);
  componentHost_.upsert(std::move(joint));

  auto fracture = makeFractureComponentDescriptor(fractureEnabled_);
  fracture.settings[QStringLiteral("preset")] = fracturePreset_;
  fracture.settings[QStringLiteral("shardCount")] = fractureShardCount_;
  fracture.settings[QStringLiteral("crackThreshold")] =
      static_cast<double>(fractureCrackThreshold_);
  fracture.settings[QStringLiteral("shatterThreshold")] =
      static_cast<double>(fractureShatterThreshold_);
  fracture.settings[QStringLiteral("shardDamping")] =
      static_cast<double>(fractureShardDamping_);
  fracture.settings[QStringLiteral("shardGravity")] =
      static_cast<double>(fractureShardGravity_);
  fracture.settings[QStringLiteral("impactSensitivity")] =
      static_cast<double>(fractureImpactSensitivity_);
  fracture.settings[QStringLiteral("preGenerate")] = fracturePreGenerate_;
  fracture.settings[QStringLiteral("triggerFrame")] =
      static_cast<qint64>(fractureTriggerFrame_);
  componentHost_.upsert(std::move(fracture));

  auto emitter = makeParticleEmitterComponentDescriptor(
      particleEmitterComponentEnabled_);
  emitter.settings[QStringLiteral("count")] = particleEmitterCount_;
  emitter.settings[QStringLiteral("speed")] =
      static_cast<double>(particleEmitterSpeed_);
  emitter.settings[QStringLiteral("lifetime")] =
      static_cast<double>(particleEmitterLifetime_);
  componentHost_.upsert(std::move(emitter));

  auto fluid = makeFluidComponentDescriptor(fluidComponentEnabled_);
  fluid.settings[QStringLiteral("mode")] = fluidMode_;
  fluid.settings[QStringLiteral("gridWidth")] = fluidGridWidth_;
  fluid.settings[QStringLiteral("gridHeight")] = fluidGridHeight_;
  fluid.settings[QStringLiteral("viscosity")] =
      static_cast<double>(fluidViscosity_);
  fluid.settings[QStringLiteral("diffusion")] =
      static_cast<double>(fluidDiffusion_);
  fluid.settings[QStringLiteral("buoyancy")] =
      static_cast<double>(fluidBuoyancy_);
  fluid.settings[QStringLiteral("vorticity")] =
      static_cast<double>(fluidVorticity_);
  fluid.settings[QStringLiteral("solverIterations")] = fluidSolverIterations_;
  fluid.settings[QStringLiteral("liquidFillAmount")] =
      static_cast<double>(liquidFillAmount_);
  fluid.settings[QStringLiteral("liquidInflowRate")] =
      static_cast<double>(liquidInflowRate_);
  fluid.settings[QStringLiteral("liquidInflowWidth")] =
      static_cast<double>(liquidInflowWidth_);
  fluid.settings[QStringLiteral("liquidInflowSpeed")] =
      static_cast<double>(liquidInflowSpeed_);
  fluid.settings[QStringLiteral("liquidInflowPosition")] =
      static_cast<double>(liquidInflowPosition_);
  fluid.settings[QStringLiteral("liquidOpeningEdge")] = liquidOpeningEdge_;
  fluid.settings[QStringLiteral("liquidSpillCullMargin")] =
      static_cast<double>(liquidSpillCullMargin_);
  fluid.settings[QStringLiteral("liquidGravity")] =
      static_cast<double>(liquidGravity_);
  fluid.settings[QStringLiteral("liquidSurfaceTension")] =
      static_cast<double>(liquidSurfaceTension_);
  fluid.settings[QStringLiteral("liquidParticleSpacing")] =
      static_cast<double>(liquidParticleSpacing_);
  fluid.settings[QStringLiteral("liquidSubsteps")] = liquidSubsteps_;
  fluid.settings[QStringLiteral("liquidSurfaceOpacity")] =
      static_cast<double>(liquidSurfaceOpacity_);
  fluid.settings[QStringLiteral("liquidEdgeOpacity")] =
      static_cast<double>(liquidEdgeOpacity_);
  fluid.settings[QStringLiteral("liquidFoamAmount")] =
      static_cast<double>(liquidFoamAmount_);
  fluid.settings[QStringLiteral("liquidContainerOpacity")] =
      static_cast<double>(liquidContainerOpacity_);
  fluid.settings[QStringLiteral("liquidContainerWidth")] =
      static_cast<double>(liquidContainerWidth_);
  QJsonObject descriptorLiquidColor;
  descriptorLiquidColor[QStringLiteral("r")] = liquidColor_.r();
  descriptorLiquidColor[QStringLiteral("g")] = liquidColor_.g();
  descriptorLiquidColor[QStringLiteral("b")] = liquidColor_.b();
  descriptorLiquidColor[QStringLiteral("a")] = liquidColor_.a();
  fluid.settings[QStringLiteral("liquidColor")] = descriptorLiquidColor;
  QJsonObject descriptorFoamColor;
  descriptorFoamColor[QStringLiteral("r")] = liquidFoamColor_.r();
  descriptorFoamColor[QStringLiteral("g")] = liquidFoamColor_.g();
  descriptorFoamColor[QStringLiteral("b")] = liquidFoamColor_.b();
  descriptorFoamColor[QStringLiteral("a")] = liquidFoamColor_.a();
  fluid.settings[QStringLiteral("liquidFoamColor")] = descriptorFoamColor;
  componentHost_.upsert(std::move(fluid));

  const QString& sourceComponentType = builtinSourceComponentType_;
  if (!sourceComponentType.isEmpty()) {
    LayerComponentDescriptor sourceDescriptor;
    sourceDescriptor.componentId = QStringLiteral("builtin.") + sourceComponentType;
    sourceDescriptor.typeId =
        QStringLiteral("artifact.component.") + sourceComponentType;
    sourceDescriptor.version = 1;
    sourceDescriptor.enabled = true;
    sourceDescriptor.phase = LayerComponentPhase::Source;
    sourceDescriptor.scope = LayerComponentScope::Layer;
    sourceDescriptor.order = 100;
    componentHost_.upsert(std::move(sourceDescriptor));
  }
}

void ArtifactAbstractLayerImpl::syncBuiltinBoolsFromHost() {
  auto boolFromHost = [this](const QString& typeId) -> bool {
    const auto* d = componentHost_.findByType(typeId);
    return d ? d->enabled : false;
  };
  clonerComponentEnabled_ = boolFromHost(QStringLiteral("artifact.component.cloner"));
  layoutComponentEnabled_ = boolFromHost(QStringLiteral("artifact.component.layout"));
  crowdComponentEnabled_ = boolFromHost(QStringLiteral("artifact.component.crowd"));
  collisionComponentEnabled_ = boolFromHost(QStringLiteral("artifact.component.collision"));
  particleEmitterComponentEnabled_ = boolFromHost(QStringLiteral("artifact.component.particle-emitter"));
  fluidComponentEnabled_ = boolFromHost(QStringLiteral("artifact.component.fluid"));
  scriptComponentEnabled_ = boolFromHost(QStringLiteral("artifact.component.script"));
  if (auto* d = componentHost_.findByType(QStringLiteral("artifact.component.fracture"))) {
    fractureEnabled_ = d->enabled;
  }
  if (auto* d = componentHost_.findByType(QStringLiteral("artifact.component.motion-dynamics"))) {
    const bool layerSpring = d->settings.value(QStringLiteral("layerSpringEnabled")).toBool(false);
    const bool follow = d->settings.value(QStringLiteral("followThroughEnabled")).toBool(false);
    motionDynamicsEnabled_ = follow;
    if (!layerSpring && !follow) {
      motionDynamicsEnabled_ = false;
    }
  }
  physicsComponent_.settings().collisionEnabled = collisionComponentEnabled_;
  if (collisionComponentEnabled_ && !physicsComponent_.enabled()) {
    physicsComponent_.setEnabled(true);
    collisionOwnsPhysicsEnable_ = true;
  } else if (!collisionComponentEnabled_ && collisionOwnsPhysicsEnable_) {
    physicsComponent_.setEnabled(false);
    collisionOwnsPhysicsEnable_ = false;
  }
}

void ArtifactAbstractLayerImpl::goToStartFrame()
{
  currentFrame_ = startTime_.framePosition();
}

void ArtifactAbstractLayerImpl::goToEndFrame()
{
  const int64_t duration = std::max<int64_t>(
      1, outPoint_.framePosition() - inPoint_.framePosition());
  currentFrame_ = startTime_.framePosition() + duration - 1;
}

void ArtifactAbstractLayerImpl::goToNextFrame()
{
  const int64_t endFrame = startTime_.framePosition() + std::max<int64_t>(
      1, outPoint_.framePosition() - inPoint_.framePosition()) - 1;
  if (currentFrame_ < endFrame) {
    ++currentFrame_;
  }
}

void ArtifactAbstractLayerImpl::goToPrevFrame()
{
  const int64_t startFrame = startTime_.framePosition();
  if (currentFrame_ > startFrame) {
    --currentFrame_;
  }
}

bool ArtifactAbstractLayerImpl::is3D() const { return is3D_; }

ArtifactAbstractLayer::ArtifactAbstractLayer()
    : impl_(new ArtifactAbstractLayerImpl()) {
  impl_->id = Id(); // Generate new ID
  impl_->variants_.push_back(std::make_unique<LayerVariant>(this, "A"));
  impl_->activeVariantIndex_ = 0;
}

ArtifactAbstractLayer::~ArtifactAbstractLayer() { delete impl_; }

void ArtifactAbstractLayer::changed() {
  const auto publish = [this]() {
    const auto *comp = dynamic_cast<const ArtifactAbstractComposition *>(
        compositionObject());
    ArtifactCore::globalEventBus().publish(LayerChangedEvent{
        comp ? comp->id().toString() : QString{}, id().toString(),
        LayerChangedEvent::ChangeType::Modified});
  };

  if (QThread::currentThread() == thread()) {
    publish();
    return;
  }

  QMetaObject::invokeMethod(this, publish, Qt::QueuedConnection);
}

void ArtifactAbstractLayer::layerNoteChanged(QString note) {
  const auto publish = [this, note = std::move(note)]() {
    const auto *comp = dynamic_cast<const ArtifactAbstractComposition *>(
        compositionObject());
    ArtifactCore::globalEventBus().publish(LayerNoteChangedEvent{
        comp ? comp->id().toString() : QString{}, id().toString(), note});
  };

  if (QThread::currentThread() == thread()) {
    publish();
    return;
  }

  QMetaObject::invokeMethod(this, publish, Qt::QueuedConnection);
}

void ArtifactAbstractLayer::setVisible(bool visible /*=true*/) {
  if (!assignIfChanged(impl_->isVisible_, visible)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::VisibilityChanged);
}

void ArtifactAbstractLayer::Show() { setVisible(true); }

void ArtifactAbstractLayer::Hide() { setVisible(false); }

LAYER_BLEND_TYPE ArtifactAbstractLayer::layerBlendType() const {
  const auto* var = getActiveVariant();
  if (var && HasFlag(var->overrideFlags_, VariantOverrideFlags::BlendMode) && var->blendModeOverride.has_value()) {
      return var->blendModeOverride.value();
  }
  return impl_->blendMode_;
}

void ArtifactAbstractLayer::setBlendMode(LAYER_BLEND_TYPE type) {
  const auto normalizedType = static_cast<LAYER_BLEND_TYPE>(
      std::clamp(static_cast<int>(type), 0,
                 static_cast<int>(LAYER_BLEND_TYPE::BLEND_SILHOUETTE_LUMA)));
  if (impl_->activeVariantIndex_ != 0) {
      auto* var = getActiveVariant();
      if (var) {
          var->blendModeOverride = normalizedType;
          SetFlag(var->overrideFlags_, VariantOverrideFlags::BlendMode);
          setDirty(LayerDirtyFlag::Effect);
          addDirtyReason(LayerDirtyReason::PropertyChanged);
          Q_EMIT changed();
          return;
      }
  }

  if (impl_->blendMode_ == normalizedType) {
    return;
  }
  impl_->blendMode_ = normalizedType;
  setDirty(LayerDirtyFlag::Effect);
  addDirtyReason(LayerDirtyReason::PropertyChanged);
  Q_EMIT changed();
}

LayerID ArtifactAbstractLayer::id() const { return impl_->id; }

void ArtifactAbstractLayer::setId(const LayerID& id) {
  // IDs are restored before a layer is attached to a composition. Changing
  // one after attachment would leave MultiIndexContainer's by-ID index stale.
  if (impl_->composition_ || id.isNil()) {
    return;
  }
  impl_->id = id;
}

QString ArtifactAbstractLayer::layerName() const { return impl_->name_; }

void ArtifactAbstractLayer::setBuiltinLayerSourceComponentType(
    const QString &componentType) {
  const QString normalized = componentType.trimmed();
  if (impl_->builtinSourceComponentType_ == normalized) {
    return;
  }
  impl_->builtinSourceComponentType_ = normalized;
  impl_->syncBuiltinComponentDescriptors();
}

void ArtifactAbstractLayer::setLayerName(const QString &name) {
  if (!assignIfChanged(impl_->name_, name)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::PropertyChanged);
}

QString ArtifactAbstractLayer::layerNote() const { return impl_->layerNote_; }

void ArtifactAbstractLayer::setLayerNote(const QString &note) {
  if (!assignIfChanged(impl_->layerNote_, note)) {
    return;
  }
  layerNoteChanged(note);
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::PropertyChanged);
}

std::type_index ArtifactAbstractLayer::type_index() const {
  return impl_->type_index_;
}

void ArtifactAbstractLayer::goToStartFrame() { impl_->goToStartFrame(); }

void ArtifactAbstractLayer::goToEndFrame() { impl_->goToEndFrame(); }

void ArtifactAbstractLayer::goToNextFrame() { impl_->goToNextFrame(); }

void ArtifactAbstractLayer::goToPrevFrame() { impl_->goToPrevFrame(); }

void ArtifactAbstractLayer::goToFrame(int64_t frameNumber /*= 0*/) {
  // グローバルフレーム → レイヤー相対フレーム:
  // relativeFrame = globalFrame - inPoint + startTime
  impl_->currentFrame_ = frameNumber - impl_->inPoint_.framePosition() +
                         impl_->startTime_.framePosition();
  impl_->modulationRouter_.processAtFrame(
      frameNumber, static_cast<float>(effectiveLayerFrameRate(this)));
}

int64_t ArtifactAbstractLayer::currentFrame() const {
  return impl_->currentFrame_;
}

FramePosition ArtifactAbstractLayer::inPoint() const { return impl_->inPoint_; }
void ArtifactAbstractLayer::setInPoint(const FramePosition &pos) {
  if (impl_->isTimingLocked_) {
    return;
  }
  const FramePosition oldIn = impl_->inPoint_;
  const FramePosition oldOut = impl_->outPoint_;
  if (!assignIfChanged(impl_->inPoint_, pos)) {
    return;
  }
  for (auto it = impl_->propertyCache_.begin(); it != impl_->propertyCache_.end(); ++it) {
    const auto& property = it.value();
    if (property && property->isAnimatable()) {
      property->retimeKeyFramesForLayerPointChange(
          timelineTimeForFramePosition(this, oldIn),
          timelineTimeForFramePosition(this, oldOut),
          timelineTimeForFramePosition(this, pos),
          timelineTimeForFramePosition(this, oldOut));
    }
  }
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::TimelineChanged);
}
FramePosition ArtifactAbstractLayer::outPoint() const {
  return impl_->outPoint_;
}
void ArtifactAbstractLayer::setOutPoint(const FramePosition &pos) {
  if (impl_->isTimingLocked_) {
    return;
  }
  const FramePosition oldIn = impl_->inPoint_;
  const FramePosition oldOut = impl_->outPoint_;
  if (!assignIfChanged(impl_->outPoint_, pos)) {
    return;
  }
  for (auto it = impl_->propertyCache_.begin(); it != impl_->propertyCache_.end(); ++it) {
    const auto& property = it.value();
    if (property && property->isAnimatable()) {
      property->retimeKeyFramesForLayerPointChange(
          timelineTimeForFramePosition(this, oldIn),
          timelineTimeForFramePosition(this, oldOut),
          timelineTimeForFramePosition(this, oldIn),
          timelineTimeForFramePosition(this, pos));
    }
  }
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::TimelineChanged);
}
FramePosition ArtifactAbstractLayer::startTime() const {
  return impl_->startTime_;
}
void ArtifactAbstractLayer::setStartTime(const FramePosition &pos) {
  if (impl_->isTimingLocked_) {
    return;
  }
  if (!assignIfChanged(impl_->startTime_, pos)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::TimelineChanged);
}

void ArtifactAbstractLayer::setTimelineWindow(FramePosition inPoint, FramePosition outPoint) {
  if (impl_->isTimingLocked_) {
    return;
  }
  if (outPoint.framePosition() <= inPoint.framePosition()) {
    outPoint = FramePosition(inPoint.framePosition() + 1);
  }
  setInPoint(inPoint);
  setOutPoint(outPoint);
}

void ArtifactAbstractLayer::slideTimingBy(const qint64 deltaFrames) {
  if (impl_->isTimingLocked_) {
    return;
  }
  if (deltaFrames == 0) {
    return;
  }
  const FramePosition nextIn(impl_->inPoint_.framePosition() + deltaFrames);
  const FramePosition nextOut(impl_->outPoint_.framePosition() + deltaFrames);
  setTimelineWindow(nextIn, nextOut);
}

bool ArtifactAbstractLayer::isActiveAt(const FramePosition &pos) const {
  return pos.framePosition() >= impl_->inPoint_.framePosition() &&
         pos.framePosition() < impl_->outPoint_.framePosition();
}

bool ArtifactAbstractLayer::isGuide() const { return impl_->isGuide_; }
void ArtifactAbstractLayer::setGuide(bool guide) {
  if (!assignIfChanged(impl_->isGuide_, guide)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::Visibility,
                      LayerDirtyReason::VisibilityChanged);
}
bool ArtifactAbstractLayer::isSolo() const { return impl_->isSolo_; }
void ArtifactAbstractLayer::setSolo(bool solo) {
  if (!assignIfChanged(impl_->isSolo_, solo)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::Visibility,
                      LayerDirtyReason::PlaybackChanged);
}
bool ArtifactAbstractLayer::isLocked() const { return impl_->isLocked_; }
void ArtifactAbstractLayer::setLocked(bool locked) {
  if (!assignIfChanged(impl_->isLocked_, locked)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::Visibility,
                      LayerDirtyReason::PropertyChanged);
}
LayerCachePolicy ArtifactAbstractLayer::layerCachePolicy() const {
  return impl_->layerCachePolicy_;
}

void ArtifactAbstractLayer::setLayerCachePolicy(LayerCachePolicy policy) {
  const auto normalizedPolicy = static_cast<LayerCachePolicy>(
      std::clamp(static_cast<int>(policy), 0, 2));
  if (!assignIfChanged(impl_->layerCachePolicy_, normalizedPolicy)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::Property,
                      LayerDirtyReason::PropertyChanged);
}

bool ArtifactAbstractLayer::usesLayerCache() const {
  return isGlobalLayerCacheEnabled() &&
         impl_->layerCachePolicy_ != LayerCachePolicy::Disabled;
}

bool ArtifactAbstractLayer::isGlobalLayerCacheEnabled() {
  return g_globalLayerCacheEnabled;
}

void ArtifactAbstractLayer::setGlobalLayerCacheEnabled(bool enabled) {
  g_globalLayerCacheEnabled = enabled;
}
bool ArtifactAbstractLayer::isSelectionLocked() const { return impl_->isSelectionLocked_; }
void ArtifactAbstractLayer::setSelectionLocked(bool locked) {
  if (!assignIfChanged(impl_->isSelectionLocked_, locked)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::Visibility,
                      LayerDirtyReason::PropertyChanged);
}
bool ArtifactAbstractLayer::isTransformLocked() const { return impl_->isTransformLocked_; }
void ArtifactAbstractLayer::setTransformLocked(bool locked) {
  if (!assignIfChanged(impl_->isTransformLocked_, locked)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::Visibility,
                      LayerDirtyReason::PropertyChanged);
}
bool ArtifactAbstractLayer::isTimingLocked() const { return impl_->isTimingLocked_; }
void ArtifactAbstractLayer::setTimingLocked(bool locked) {
  if (!assignIfChanged(impl_->isTimingLocked_, locked)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::Visibility,
                      LayerDirtyReason::PropertyChanged);
}
bool ArtifactAbstractLayer::isShy() const { return impl_->isShy_; }
void ArtifactAbstractLayer::setShy(bool shy) {
  if (!assignIfChanged(impl_->isShy_, shy)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::Visibility,
                      LayerDirtyReason::PropertyChanged);
}

int ArtifactAbstractLayer::labelColorIndex() const {
  return impl_->labelColorIndex_;
}
void ArtifactAbstractLayer::setLabelColorIndex(int index) {
  const int normalizedIndex = std::clamp(index, 0, 7);
  if (impl_->labelColorIndex_ != normalizedIndex) {
    impl_->labelColorIndex_ = normalizedIndex;
    Q_EMIT changed();
  }
}

void ArtifactAbstractLayer::setDirty(LayerDirtyFlag flag) {
  impl_->dirtyFlags_ |= (uint32_t)flag;
  ++impl_->geometryRevision_;
  if (((uint32_t)flag & (uint32_t)LayerDirtyFlag::Effect) != 0) {
    impl_->effectRevision_.fetch_add(1, std::memory_order_release);
  }
  // A thumbnail is derived from layer content. Any mutation invalidates the
  // cached image so a later renderer cannot return a stale preview.
  impl_->thumbnailCache_ = QImage();
  impl_->thumbnailCacheSize_ = QSize();
}
std::uint64_t ArtifactAbstractLayer::effectRevision() const {
  return impl_ ? impl_->effectRevision_.load(std::memory_order_acquire) : 0;
}

bool ArtifactAbstractLayer::hasAnimatedEffectProperties() {
  if (!impl_) return false;
  const std::uint64_t revision =
      impl_->effectRevision_.load(std::memory_order_acquire);
  constexpr std::uint64_t kValid = 1u;
  constexpr std::uint64_t kAnimated = 2u;
  const std::uint64_t cached =
      impl_->animatedEffectPropertyState_.load(std::memory_order_acquire);
  if ((cached >> 2u) == revision && (cached & kValid) != 0) {
    return (cached & kAnimated) != 0;
  }

  bool animated = false;
  for (const auto& effect : getEffects()) {
    if (!effect || !effect->isEnabled()) continue;
    for (const auto& property : effect->editableProperties()) {
      if (!property) continue;
      const QString modulationPath =
          effect->modulationPropertyPath(property->getName());
      const bool hasModulation = !modulationPath.isEmpty() &&
          effect->modulationRouter().hasTarget(
              Audio::Modulation::modulationTargetId(
                  modulationPath.toStdString()));
      if (property->hasKeyFrames() || property->hasExpression() ||
          property->hasEnvelopes() || hasModulation) {
        animated = true;
        break;
      }
    }
    if (animated) break;
  }

  if (impl_->effectRevision_.load(std::memory_order_acquire) == revision) {
    const std::uint64_t state = (revision << 2u) | kValid |
                                (animated ? kAnimated : 0u);
    impl_->animatedEffectPropertyState_.store(state,
                                               std::memory_order_release);
  }
  return animated;
}
void ArtifactAbstractLayer::clearDirty(LayerDirtyFlag flag) {
  impl_->dirtyFlags_ &= ~(uint32_t)flag;
}
bool ArtifactAbstractLayer::isDirty(LayerDirtyFlag flag) const {
  return (impl_->dirtyFlags_ & (uint32_t)flag) != 0;
}
void ArtifactAbstractLayer::addDirtyReason(LayerDirtyReason reason) {
  impl_->dirtyReasonMask_ |= static_cast<uint64_t>(reason);
}
bool ArtifactAbstractLayer::hasDirtyReason(LayerDirtyReason reason) const {
  return (impl_->dirtyReasonMask_ & static_cast<uint64_t>(reason)) != 0;
}
uint64_t ArtifactAbstractLayer::dirtyReasonMask() const {
  return impl_->dirtyReasonMask_;
}
void ArtifactAbstractLayer::clearDirtyReasons() {
  impl_->dirtyReasonMask_ = static_cast<uint64_t>(LayerDirtyReason::None);
}

// ============================================================================
// Variants Management
// ============================================================================

size_t ArtifactAbstractLayer::getActiveVariantIndex() const {
    return impl_->activeVariantIndex_;
}

void ArtifactAbstractLayer::setActiveVariant(size_t index) {
    if (index < impl_->variants_.size() && impl_->activeVariantIndex_ != index) {
        impl_->activeVariantIndex_ = index;
        notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::PropertyChanged);
    }
}

LayerVariant* ArtifactAbstractLayer::getActiveVariant() const {
    if (impl_->activeVariantIndex_ < impl_->variants_.size()) {
        return impl_->variants_[impl_->activeVariantIndex_].get();
    }
    return nullptr;
}

LayerVariant* ArtifactAbstractLayer::createVariantFromCurrent(const ArtifactCore::String& newName) {
    auto newVariant = std::make_unique<LayerVariant>(this, newName);
    
    if (impl_->activeVariantIndex_ < impl_->variants_.size()) {
        auto* current = impl_->variants_[impl_->activeVariantIndex_].get();
        newVariant->overrideFlags_ = current->overrideFlags_;
        newVariant->transform2DOverride = current->transform2DOverride;
        newVariant->transform3DOverride = current->transform3DOverride;
        newVariant->opacityOverride = current->opacityOverride;
        newVariant->blendModeOverride = current->blendModeOverride;
    }
    
    impl_->variants_.push_back(std::move(newVariant));
    notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::PropertyChanged);
    return impl_->variants_.last()->get();
}

void ArtifactAbstractLayer::resetVariantOverride(VariantOverrideFlags specificFlag) {
    if (impl_->activeVariantIndex_ == 0) return; // Base (A) はリセット不可

    auto* var = getActiveVariant();
    if (!var) return;

    if (specificFlag == VariantOverrideFlags::None) {
        var->overrideFlags_ = VariantOverrideFlags::None;
        var->transform2DOverride.reset();
        var->transform3DOverride.reset();
        var->opacityOverride.reset();
        var->blendModeOverride.reset();
    } else {
        ClearFlag(var->overrideFlags_, specificFlag);
        
        if (HasFlag(specificFlag, VariantOverrideFlags::Transform)) {
            var->transform2DOverride.reset();
            var->transform3DOverride.reset();
        }
        if (HasFlag(specificFlag, VariantOverrideFlags::Opacity)) {
            var->opacityOverride.reset();
        }
        if (HasFlag(specificFlag, VariantOverrideFlags::BlendMode)) {
            var->blendModeOverride.reset();
        }
    }
    
    notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::PropertyChanged);
}

NamedVector<LayerVariant*> ArtifactAbstractLayer::getVariants() const {
    NamedVector<LayerVariant*> result{ContainerName{"Layer.VariantViews"}};
    result.reserve(impl_->variants_.size());
    for(auto& v : impl_->variants_) {
        result.push_back(v.get());
    }
    return result;
}

std::unique_ptr<LayerVariant> ArtifactAbstractLayer::extractVariant(size_t index) {
    if (index < impl_->variants_.size()) {
       auto var = std::move(impl_->variants_[index]);
       impl_->variants_.removeAt(index);
       if (impl_->activeVariantIndex_ >= impl_->variants_.size()) {
           impl_->activeVariantIndex_ = impl_->variants_.empty() ? 0 : impl_->variants_.size() - 1;
       } else if (impl_->activeVariantIndex_ >= index && impl_->activeVariantIndex_ > 0) {
           impl_->activeVariantIndex_--;
       }
       notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::PropertyChanged);
       return var;
    }
    return nullptr;
}

void ArtifactAbstractLayer::insertVariant(size_t index, std::unique_ptr<LayerVariant> variant) {
    if (!variant) return;
    if (index > impl_->variants_.size()) index = impl_->variants_.size();
    impl_->variants_.insert(index, std::move(variant));
    if (impl_->activeVariantIndex_ >= index) {
        impl_->activeVariantIndex_++;
    }
    notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::PropertyChanged);
}

void ArtifactAbstractLayer::setComposition(QObject *comp) {
  std::optional<int64_t> transformTimeScale;
  if (auto *composition = dynamic_cast<ArtifactAbstractComposition *>(comp)) {
    const double fps = composition->frameRate().framerate();
    transformTimeScale = ArtifactCore::FrameRate::storageScaleForFps(fps, 24);
  }

  {
    std::lock_guard<std::mutex> lock(impl_->compositionMutex_);
    impl_->composition_ = comp;
  }

  // Preserve a detached layer's current scale so its stored keyframe indices
  // are not reinterpreted while it is being moved between compositions.
  if (transformTimeScale.has_value()) {
    // Transform keyframes use the composition frame domain. This assignment
    // happens as a layer joins its composition, before timeline edits can
    // create keys, so 25/30/60 fps edits never collapse into legacy 24fps
    // storage buckets.
    impl_->transform_.setKeyframeTimeScale(*transformTimeScale);
    for (const auto &variant : impl_->variants_) {
      if (variant && variant->transform3DOverride.has_value()) {
        variant->transform3DOverride->setKeyframeTimeScale(*transformTimeScale);
      }
    }
  }
}

void ArtifactAbstractLayer::setComposition(void *comp) {
  setComposition(static_cast<QObject *>(comp));
}

void *ArtifactAbstractLayer::composition() const {
  std::lock_guard<std::mutex> lock(impl_->compositionMutex_);
  return impl_->composition_.data();
}

QObject *ArtifactAbstractLayer::compositionObject() const {
  std::lock_guard<std::mutex> lock(impl_->compositionMutex_);
  return impl_->composition_.data();
}

float ArtifactAbstractLayer::compositionFieldInfluenceAtCanvasPoint(
    const QPointF& canvasPosition, bool* affected) const {
  const auto channels = compositionFieldChannelsAtCanvasPoint(canvasPosition);
  if (affected) {
    *affected = channels.affected;
  }
  return channels.weight;
}

bool applyResponsiveLayoutConstraints(
    const ArtifactAbstractLayer* layer, double& positionX, double& positionY,
    double& scaleX, double& scaleY, double anchorX, double anchorY,
    bool componentEnabled, bool responsiveEnabled, int horizontalPinValue,
    int verticalPinValue, int scaleModeValue, bool safeAreaEnabled,
    double safeAreaPaddingX, double safeAreaPaddingY, double offsetX,
    double offsetY);
QPointF parentAutoLayoutOffset(const ArtifactAbstractLayer* layer,
                               const ArtifactAbstractLayerPtr& parent);

double ArtifactAbstractLayer::compositionFrameRate() const {
  return effectiveLayerFrameRate(this);
}

ArtifactAbstractLayerPtr ArtifactAbstractLayer::parentLayer() const {
  auto *composition = dynamic_cast<ArtifactAbstractComposition *>(compositionObject());
  if (!composition || impl_->parentLayerId_.isNil())
  return nullptr;
  return composition->layerById(impl_->parentLayerId_);
}

// Transform / opacity read path: keyframes and static values go through the
// cheap interpolateValue, but a property carrying an expression, envelope or
// external override must be evaluated so AE-style expressions actually drive
// the rendered transform. Building the evaluator is deferred to that case so
// the every-frame hot path stays allocation free for plain keyed properties.
QVariant evaluateAnimatedPropertyValue(const ArtifactCore::AbstractProperty& property,
                                       const ArtifactCore::RationalTime& time) {
  if (property.hasExpression()) {
    ArtifactCore::ExpressionEvaluator evaluator;
    return property.evaluateValue(time, &evaluator);
  }
  if (property.hasEnvelopes() || property.hasExternalOverride()) {
    return property.evaluateValue(time);
  }
  return property.interpolateValue(time);
}

// Reusable automation-clip overlay (Phase 2): non-destructive, weight-mixed
// over the base value. Dangling pattern references never break playback.
// No allocation beyond the (cold) pattern lookup; instances are read by ref.
float applyAutomationClipOverlay(const ArtifactAbstractLayer* layer,
                                 const QString& targetPath, float baseValue,
                                 double parentSeconds, double freeSeconds) {
  if (!layer || targetPath.isEmpty() || !std::isfinite(baseValue)) {
    return baseValue;
  }
  const auto& instances = layer->automationClipInstances();
  if (instances.empty()) {
    return baseValue;
  }
  auto* composition =
      dynamic_cast<ArtifactAbstractComposition*>(layer->compositionObject());
  if (!composition) {
    return baseValue;
  }
  const std::string target = targetPath.toStdString();
  float result = baseValue;
  for (const auto& instance : instances) {
    if (!instance.enabled || instance.targetPath != target) {
      continue;
    }
    const ArtifactCore::AutomationClipPattern* pattern =
        composition->findAutomationClipPattern(instance.patternId);
    if (!pattern) {
      continue;
    }
    const double clock =
        instance.timePolicy == ArtifactCore::AutomationClipTimePolicy::FreeTime
        ? freeSeconds : parentSeconds;
    result = ArtifactCore::applyAutomationClipInstance(result, *pattern, instance, clock);
  }
  return result;
}

bool ArtifactAbstractLayer::hasRigidBodyPhysics() const {
  auto world = LayerPhysics::rigidWorld(id());
  if (auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
          impl_->composition_.data());
      composition) {
    world = LayerPhysics::compositionRigidWorld(composition->id());
  }
  if (!world) return false;
  for (const auto& body : world->getBodies()) {
    if (body && body->cloneIndex >= -1 && body->ownerLayerId == id()) return true;
  }
  return false;
}

const FractureState& ArtifactAbstractLayer::fractureState() const {
  return impl_->fractureState_;
}

const NamedVector<FractureShardMotion>& ArtifactAbstractLayer::fractureShardMotions() const {
  return impl_->fractureState_.shards;
}

void ArtifactAbstractLayer::enableSoftBodyPhysics() {
  impl_->softBodyPhysicsEnabled_ = true;
  impl_->physicsComponent_.authoring().solverKind =
      PhysicsSolverKind::SoftBody2D;
  LayerPhysics::ensureSoftBody(id());
  const auto& settings = impl_->physicsComponent_.settings();
  LayerPhysics::configureSoftBody(
      id(), 0.0f, settings.gravityY * settings.gravityScale,
      settings.linearDamping);
  syncSoftBodyPhysicsColliderToBounds();
}

void ArtifactAbstractLayer::enableSoftBodyPhysicsGrid(int columns, int rows, float stiffness) {
  impl_->softBodyPhysicsEnabled_ = true;
  impl_->physicsComponent_.authoring().solverKind =
      PhysicsSolverKind::SoftBody2D;
  const QRectF bounds = localBounds();
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    enableSoftBodyPhysics();
    return;
  }

  // The grid starts inside its layer bounds.  Registering those same bounds
  // as a collider would expel every particle on the first solve, so leave
  // collision sources to explicitly registered external colliders.
  LayerPhysics::clearSoftBodyColliders(id());
  LayerPhysics::createSoftBodyGrid(
      id(),
      static_cast<float>(bounds.left()),
      static_cast<float>(bounds.top()),
      static_cast<float>(bounds.width()),
      static_cast<float>(bounds.height()),
      columns,
      rows,
      1.0f,
      stiffness,
      false);
  const auto& settings = impl_->physicsComponent_.settings();
  LayerPhysics::configureSoftBody(
      id(), 0.0f, settings.gravityY * settings.gravityScale,
      settings.linearDamping);
  LayerPhysics::setSoftBodyWind(
      id(), settings.windX, settings.windY,
      settings.windEnabled ? settings.windStrength : 0.0f);
}

void ArtifactAbstractLayer::disableSoftBodyPhysics() {
  impl_->softBodyPhysicsEnabled_ = false;
  if (impl_->physicsComponent_.authoring().solverKind ==
      PhysicsSolverKind::SoftBody2D) {
    impl_->physicsComponent_.authoring().solverKind =
        PhysicsSolverKind::Disabled;
  }
  LayerPhysics::removeSoftBody(id());
}

void ArtifactAbstractLayer::enableCloth3DPhysics() {
  impl_->cloth3DPhysicsEnabled_ = true;
  impl_->physicsComponent_.authoring().solverKind =
      PhysicsSolverKind::Cloth3D;
  LayerPhysics::ensureCloth3D(id());
}

void ArtifactAbstractLayer::enableCloth3DPhysicsGrid(int columns, int rows, float stiffness) {
  impl_->cloth3DPhysicsEnabled_ = true;
  impl_->physicsComponent_.authoring().solverKind =
      PhysicsSolverKind::Cloth3D;
  const QRectF bounds = localBounds();
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    enableCloth3DPhysics();
    return;
  }
  // 2D SoftBodyと同様、bounds自体をcollider化しない。外部colliderのみ使う。
  LayerPhysics::createCloth3DGrid(
      id(),
      static_cast<float>(bounds.left()),
      static_cast<float>(bounds.top()),
      static_cast<float>(bounds.width()),
      static_cast<float>(bounds.height()),
      0.0f,
      columns,
      rows,
      1.0f,
      stiffness,
      true);
  const auto& settings = impl_->physicsComponent_.settings();
  LayerPhysics::setCloth3DWind(
      id(), settings.windX, settings.windY, 0.0f,
      settings.windEnabled ? settings.windStrength : 0.0f);
}

void ArtifactAbstractLayer::disableCloth3DPhysics() {
  impl_->cloth3DPhysicsEnabled_ = false;
  if (impl_->physicsComponent_.authoring().solverKind ==
      PhysicsSolverKind::Cloth3D) {
    impl_->physicsComponent_.authoring().solverKind =
        PhysicsSolverKind::Disabled;
  }
  LayerPhysics::removeCloth3D(id());
}

void ArtifactAbstractLayer::enableRigidBodyPhysics() {
  impl_->physicsComponent_.authoring().solverKind =
      PhysicsSolverKind::RigidBody2D;
  auto world = LayerPhysics::rigidWorld(id());
  bool createdWorld = false;
  if (auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
          impl_->composition_.data());
      composition) {
    // A layer attached to a composition must never retain a private solver.
    LayerPhysics::removeRigidWorld(id());
    world = LayerPhysics::compositionRigidWorld(composition->id());
    if (!world) {
      world = LayerPhysics::ensureCompositionRigidWorld(composition->id());
      createdWorld = true;
    }
  } else if (!world) {
    world = LayerPhysics::ensureRigidWorld(id());
    createdWorld = true;
  }
  if (world && createdWorld) {
    world->setGravity(0.0f, impl_->physicsComponent_.settings().gravityY);
  }
  syncRigidBodyPhysicsToBounds();
}

void ArtifactAbstractLayer::disableRigidBodyPhysics() {
  if (auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
          impl_->composition_.data());
      composition) {
    if (auto world = LayerPhysics::compositionRigidWorld(composition->id())) {
      world->removeLayerJoint(id());
      for (const auto& body : world->getBodies()) {
        if (body && body->ownerLayerId == id()) {
          world->removeBody(body);
        }
      }
    }
  }
  LayerPhysics::removeRigidWorld(id());
  impl_->rigidBodyColliderShape_ = -1;
  impl_->rigidBodyColliderRestitution_ = -1.0f;
  clearRigidBodyContactState();
}

void ArtifactAbstractLayer::enableMaterialPhysics(int preset) {
  const QRectF bounds = localBounds();
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    return;
  }
  impl_->materialPhysicsEnabled_ = true;
  impl_->physicsComponent_.authoring().solverKind =
      PhysicsSolverKind::Mpm2D;
  impl_->materialPhysicsPreset_ = std::clamp(preset, 0, 3);
  LayerPhysics::createMaterialGrid(
      id(), static_cast<float>(bounds.left()), static_cast<float>(bounds.top()),
      static_cast<float>(bounds.width()), static_cast<float>(bounds.height()),
      20, 20, impl_->materialPhysicsPreset_);
}

void ArtifactAbstractLayer::disableMaterialPhysics() {
  impl_->materialPhysicsEnabled_ = false;
  if (impl_->physicsComponent_.authoring().solverKind ==
      PhysicsSolverKind::Mpm2D) {
    impl_->physicsComponent_.authoring().solverKind =
        PhysicsSolverKind::Disabled;
  }
  LayerPhysics::removeMaterialSolver(id());
}

void ArtifactAbstractLayer::syncSoftBodyPhysicsColliderToBounds() {
  const auto solver = LayerPhysics::ensureSoftBody(id());

  const QRectF bounds = layerCollisionLocalBounds(this);
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    LayerPhysics::clearSoftBodyColliders(id());
    return;
  }

  const NamedVector<QPointF> polygon =
      layerCollisionPolygonLocalPoints(this);
  LayerPhysics::clearSoftBodyColliders(id());
  ArtifactCore::SoftBodyCollider collider;
  if (polygon.size() >= 3) {
    collider.type = ArtifactCore::SoftBodyCollider::Type::Polygon;
    collider.polygonPoints.reserve(polygon.size() * 2U);
    for (const QPointF& point : polygon) {
      collider.polygonPoints.push_back(static_cast<float>(point.x()));
      collider.polygonPoints.push_back(static_cast<float>(point.y()));
    }
  } else {
    collider.type = ArtifactCore::SoftBodyCollider::Type::Box;
  }
  collider.x = static_cast<float>(bounds.center().x());
  collider.y = static_cast<float>(bounds.center().y());
  collider.width = static_cast<float>(bounds.width());
  collider.height = static_cast<float>(bounds.height());
  collider.restitution = std::clamp(
      impl_->physicsComponent_.settings().restitution, 0.0f, 1.0f);
  collider.friction = 0.15f;
  LayerPhysics::registerSoftBodyCollider(id(), collider);
  Q_UNUSED(solver);
}

void ArtifactAbstractLayer::syncRigidBodyPhysicsToBounds() {
  auto world = LayerPhysics::rigidWorld(id());
  if (auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
          impl_->composition_.data());
      composition) {
    world = LayerPhysics::compositionRigidWorld(composition->id());
    if (!world) {
      world = LayerPhysics::ensureCompositionRigidWorld(composition->id());
    }
  }
  if (!world) {
    world = LayerPhysics::ensureRigidWorld(id());
  }

  const QRectF bounds = layerCollisionLocalBounds(this);
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    return;
  }

  // Use the authored pose, never the already simulated transform.
  const QTransform rest = getGlobalTransform4x4At(currentTimelineTime(this)).toTransform();
  const QPointF center = rest.map(bounds.center());
  const float angle = static_cast<float>(std::atan2(rest.m12(), rest.m11()));
  QTransform inverseRotation;
  inverseRotation.rotateRadians(-angle);
  const auto bodyLocalPoint = [&](const QPointF& point) {
    return inverseRotation.map(rest.map(point) - center);
  };
  const float cx = static_cast<float>(center.x());
  const float cy = static_cast<float>(center.y());
  const float w = static_cast<float>(bounds.width() * std::hypot(rest.m11(), rest.m12()));
  const float h = static_cast<float>(bounds.height() * std::hypot(rest.m21(), rest.m22()));
  if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(w) ||
      !std::isfinite(h) || w <= 0.0f || h <= 0.0f) return;
  impl_->rigidBodyRestTransform_ = rest;
  impl_->rigidBodyRestCenter_ = center;
  impl_->rigidBodyRestAngle_ = angle;
  impl_->lastRigidWindForceFrame_ = std::numeric_limits<int64_t>::min();
  const auto shapeProperty = getProperty(
      QStringLiteral("component.collision.shape"));
  const int shape = shapeProperty ? std::clamp(shapeProperty->getValue().toInt(), 0, 3) : 0;
  const float restitution = std::clamp(
      impl_->physicsComponent_.settings().restitution, 0.0f, 1.0f);
  float floorY = 0.0f;
  float floorWidth = std::max(4096.0f, w * 8.0f);
  if (auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
          impl_->composition_.data())) {
    const auto compositionSize = composition->settings().compositionSize();
    floorY = impl_->collisionFloorY_ > 0.0f
        ? impl_->collisionFloorY_
        : static_cast<float>(compositionSize.height());
    floorWidth = std::max(floorWidth,
                          static_cast<float>(compositionSize.width()) * 2.0f);
  }
  world->setStaticFloor(floorY, floorWidth);
  auto bodies = world->getBodies();
  ArtifactCore::SharedPtr<ArtifactCore::RigidBody2D> body;
  // cloneIndex == -2 marks joint static proxies; pick the layer's own
  // dynamic body (cloneIndex -1) rather than assuming vector order.
  for (const auto& candidate : bodies) {
    if (candidate && candidate->cloneIndex >= -1 &&
        candidate->ownerLayerId == id()) {
      body = candidate;
      break;
    }
  }
  if (body) {
    // Explicit authored edits also change size/anchor/scale. Rebuild only
    // this body; Box2D removes its attached joints, not unrelated ones.
    world->removeBody(body);
    body.reset();
  }
  if (!body) {
    if (shape == 2) {
      body = world->addDynamicCircle(
          cx, cy, std::max(w, h) * 0.5f, 1.0f, 0.3f,
          restitution);
    } else if (shape == 3) {
      const NamedVector<QPointF> polygon =
          layerCollisionPolygonLocalPoints(this);
      if (polygon.size() >= 3) {
        // Physics2D still owns its public std::vector polygon input. Keep the
        // conversion at that solver boundary rather than exposing it through
        // the layer collision API.
        std::vector<QPointF> hullSource;
        hullSource.reserve(polygon.size());
        for (const QPointF& point : polygon) {
          hullSource.push_back(point);
        }
        // Box2D v3 hulls accept at most 8 vertices; downsample longer
        // outlines so smooth shapes keep a representative polygon proxy.
        constexpr int kMaxRigidPolygonVertices = 8;
        if (hullSource.size() >
            static_cast<size_t>(kMaxRigidPolygonVertices)) {
          std::vector<QPointF> sampled;
          sampled.reserve(static_cast<size_t>(kMaxRigidPolygonVertices));
          for (int i = 0; i < kMaxRigidPolygonVertices; ++i) {
            const size_t index = static_cast<size_t>(
                i * static_cast<int>(polygon.size()) /
                kMaxRigidPolygonVertices);
            sampled.push_back(polygon[index]);
          }
          hullSource = std::move(sampled);
        }
        std::vector<QVector2D> vertices;
        vertices.reserve(hullSource.size());
        for (const QPointF& point : hullSource) {
          const QPointF local = bodyLocalPoint(point);
          vertices.emplace_back(
              static_cast<float>(local.x()), static_cast<float>(local.y()));
        }
        body = world->addPolygonBody(
            cx, cy, vertices, true, 1.0f, 0.3f, restitution);
      }
      if (!body) {
        body = world->addDynamicBox(
            cx, cy, w, h, 1.0f, 0.3f,
            restitution);
      }
    } else {
      body = world->addDynamicBox(
          cx, cy, w, h, 1.0f, 0.3f,
          restitution);
    }
  }
  if (body) {
    body->ownerLayerId = id();
    body->setTransform({cx, cy}, angle);
    impl_->rigidBodyColliderShape_ = shape;
    impl_->rigidBodyColliderRestitution_ = restitution;
    applyRigidBodyPhysicsSettings();
    body->setFixedRotation(false);
    body->setType(impl_->collisionBodyType_ == 1
        ? ArtifactCore::RigidBody2D::Type::Static
        : (impl_->collisionBodyType_ == 2
            ? ArtifactCore::RigidBody2D::Type::Kinematic
            : ArtifactCore::RigidBody2D::Type::Dynamic));
  }
}

void ArtifactAbstractLayer::syncKinematicRigidBodyToAuthoredTransform() {
  if (impl_->collisionBodyType_ == 0) return;
  auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
      impl_->composition_.data());
  if (!composition) return;
  const auto world = LayerPhysics::compositionRigidWorld(composition->id());
  if (!world) return;
  const QTransform authored = getGlobalTransformAt(currentTimelineFrame(this));
  const QPointF center = authored.map(layerCollisionLocalBounds(this).center());
  const float angle = static_cast<float>(std::atan2(authored.m12(), authored.m11()));
  if (!std::isfinite(static_cast<float>(center.x())) ||
      !std::isfinite(static_cast<float>(center.y())) || !std::isfinite(angle)) return;
  for (const auto& body : world->getBodies()) {
    if (!body || body->cloneIndex != -1 || body->ownerLayerId != id()) continue;
    body->setTransform(QVector2D(center), angle);
    body->setLinearVelocity({0.0f, 0.0f});
    body->setAngularVelocity(0.0f);
    body->setAwake(true);
    impl_->rigidBodyRestTransform_ = authored;
    impl_->rigidBodyRestCenter_ = center;
    impl_->rigidBodyRestAngle_ = angle;
    return;
  }
}

bool ArtifactAbstractLayer::beginRigidBodyMouseDrag(
    const QPointF& canvasPosition) {
  auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
      impl_->composition_.data());
  if (!composition || !std::isfinite(canvasPosition.x()) ||
      !std::isfinite(canvasPosition.y())) return false;
  const auto world = LayerPhysics::compositionRigidWorld(composition->id());
  if (!world) return false;
  for (const auto& body : world->getBodies()) {
    if (!body || body->cloneIndex != -1 || body->ownerLayerId != id() ||
        body->type() != ArtifactCore::RigidBody2D::Type::Dynamic) continue;
    const float mass = body->mass();
    const float maxForce = std::clamp(mass * 5000.0f, 10000.0f, 1.0e9f);
    return world->startMouseDrag(id(), body,
        QVector2D(static_cast<float>(canvasPosition.x()),
                  static_cast<float>(canvasPosition.y())),
        8.0f, 0.7f, maxForce);
  }
  return false;
}

bool ArtifactAbstractLayer::updateRigidBodyMouseDrag(
    const QPointF& canvasPosition) {
  auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
      impl_->composition_.data());
  if (!composition || !std::isfinite(canvasPosition.x()) ||
      !std::isfinite(canvasPosition.y())) return false;
  const auto world = LayerPhysics::compositionRigidWorld(composition->id());
  return world && world->updateMouseDrag(id(),
      QVector2D(static_cast<float>(canvasPosition.x()),
                static_cast<float>(canvasPosition.y())));
}

void ArtifactAbstractLayer::endRigidBodyMouseDrag() {
  auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
      impl_->composition_.data());
  if (!composition) return;
  if (const auto world = LayerPhysics::compositionRigidWorld(composition->id())) {
    world->endMouseDrag(id());
  }
}

bool ArtifactAbstractLayer::hasRigidBodyMouseDrag() const {
  auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
      impl_->composition_.data());
  if (!composition) return false;
  const auto world = LayerPhysics::compositionRigidWorld(composition->id());
  return world && world->hasMouseDrag(id());
}

void ArtifactAbstractLayer::beginRigidBodyContactStep() {
  impl_->rigidBodyContactState_.beginCount = 0;
  impl_->rigidBodyContactState_.endCount = 0;
  impl_->rigidBodyContactState_.hitCount = 0;
  impl_->rigidBodyContactState_.maxImpactSpeed = 0.0f;
}

void ArtifactAbstractLayer::recordRigidBodyContact(
    LayerRigidBodyContactPhase phase, const ArtifactCore::LayerID& otherLayerId,
    const QPointF& point, const QPointF& normal, float approachSpeed) {
  auto& state = impl_->rigidBodyContactState_;
  const QString key = otherLayerId.isNil()
      ? QStringLiteral("__composition_floor__") : otherLayerId.toString();
  state.lastOtherLayerId = otherLayerId;
  switch (phase) {
  case LayerRigidBodyContactPhase::Begin:
    ++state.beginCount;
    impl_->activeRigidBodyContactKeys_.insert(key);
    break;
  case LayerRigidBodyContactPhase::End:
    ++state.endCount;
    impl_->activeRigidBodyContactKeys_.remove(key);
    break;
  case LayerRigidBodyContactPhase::Hit:
    ++state.hitCount;
    state.lastPoint = point;
    state.lastNormal = normal;
    state.maxImpactSpeed = std::max(state.maxImpactSpeed,
        std::max(0.0f, approachSpeed));
    break;
  }
  state.activeContactCount = impl_->activeRigidBodyContactKeys_.size();
}

void ArtifactAbstractLayer::clearRigidBodyContactState() {
  impl_->rigidBodyContactState_ = {};
  impl_->activeRigidBodyContactKeys_.clear();
}

const LayerRigidBodyContactState& ArtifactAbstractLayer::rigidBodyContactState() const {
  return impl_->rigidBodyContactState_;
}

bool ArtifactAbstractLayer::isJointBroken() const { return impl_->jointBroken_; }
void ArtifactAbstractLayer::setJointBroken(bool broken) { impl_->jointBroken_ = broken; }

void ArtifactAbstractLayer::applyRigidBodyPhysicsSettings() {
  auto world = LayerPhysics::rigidWorld(id());
  if (auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
          impl_->composition_.data());
      composition) {
    world = LayerPhysics::compositionRigidWorld(composition->id());
  }
  if (!world) {
    return;
  }

  const auto& settings = impl_->physicsComponent_.settings();
  for (const auto& body : world->getBodies()) {
    if (!body || body->ownerLayerId != id()) {
      continue;
    }
    body->setLinearDamping(std::max(0.0f, settings.linearDamping));
    body->setAngularDamping(std::max(0.0f, settings.angularDamping));
    body->setGravityScale(std::clamp(settings.gravityScale, -10.0f, 10.0f));
  }
}

void ArtifactAbstractLayer::applyRigidBodyWorldGravity() {
  auto world = LayerPhysics::rigidWorld(id());
  if (auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
          impl_->composition_.data());
      composition) {
    world = LayerPhysics::compositionRigidWorld(composition->id());
  }
  if (world) {
    world->setGravity(0.0f, impl_->physicsComponent_.settings().gravityY);
  }
}

NamedVector<QPointF> ArtifactAbstractLayer::collisionOutlineLocalPoints()
    const {
  return {};
}

ArtifactCore::Collider2DEditState ArtifactAbstractLayer::collision2DEditState() const {
  ArtifactCore::Collider2DEditState state;
  state.enabled = impl_->collisionComponentEnabled_;
  state.shape = ArtifactCore::collider2DShapeFromPersistedValue(impl_->collisionShape_);
  state.sourceBounds = localBounds();
  state.width = impl_->collisionWidth_;
  state.height = impl_->collisionHeight_;
  state.radius = impl_->collisionRadius_;
  state.offset = QPointF(impl_->collisionOffsetX_, impl_->collisionOffsetY_);
  if (state.shape == ArtifactCore::Collider2DShape::Polygon) {
    state.sourceOutlinePointCount = static_cast<int>(collisionOutlineLocalPoints().size());
    state.rigidBodyPreviewPointCount = std::min(state.sourceOutlinePointCount, 8);
  }
  return state;
}

bool ArtifactAbstractLayer::setCollision2DEditState(
    const ArtifactCore::Collider2DEditState& state) {
  const auto finiteClamped = [](float value, float fallback, float minimum, float maximum) {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
  };
  const int shape = std::clamp(ArtifactCore::collider2DShapeToPersistedValue(state.shape), 0, 3);
  const float width = finiteClamped(state.width, impl_->collisionWidth_, 0.0f, 100000.0f);
  const float height = finiteClamped(state.height, impl_->collisionHeight_, 0.0f, 100000.0f);
  const float radius = finiteClamped(state.radius, impl_->collisionRadius_, 0.0f, 100000.0f);
  const float offsetX = finiteClamped(static_cast<float>(state.offset.x()), impl_->collisionOffsetX_, -100000.0f, 100000.0f);
  const float offsetY = finiteClamped(static_cast<float>(state.offset.y()), impl_->collisionOffsetY_, -100000.0f, 100000.0f);
  const bool changed = impl_->collisionComponentEnabled_ != state.enabled ||
      impl_->collisionShape_ != shape || impl_->collisionWidth_ != width ||
      impl_->collisionHeight_ != height || impl_->collisionRadius_ != radius ||
      impl_->collisionOffsetX_ != offsetX || impl_->collisionOffsetY_ != offsetY;
  if (!changed) return false;
  impl_->collisionComponentEnabled_ = state.enabled;
  impl_->collisionShape_ = shape;
  impl_->collisionWidth_ = width;
  impl_->collisionHeight_ = height;
  impl_->collisionRadius_ = radius;
  impl_->collisionOffsetX_ = offsetX;
  impl_->collisionOffsetY_ = offsetY;
  impl_->lastCollisionImpactFrame_ = std::numeric_limits<int64_t>::min();
  impl_->syncBuiltinComponentDescriptors();
  if (hasSoftBodyPhysics()) syncSoftBodyPhysicsColliderToBounds();
  if (hasRigidBodyPhysics()) syncRigidBodyPhysicsToBounds();
  notifyLayerMutation(this, LayerDirtyFlag::Effect, LayerDirtyReason::PropertyChanged);
  return true;
}

NamedVector<TwoPointFiveDRenderPass>
ArtifactAbstractLayer::twoPointFiveDRenderPasses(
    const QMatrix4x4 &baseTransform) const {
  return buildTwoPointFiveDRenderPasses(
      this, baseTransform, impl_->twoPointFiveDEnabled_,
      impl_->twoPointFiveDDepth_, impl_->twoPointFiveDCameraDistance_,
      impl_->twoPointFiveDDepthOfFieldEnabled_, impl_->twoPointFiveDFocusDepth_,
      impl_->twoPointFiveDFocusRange_, impl_->twoPointFiveDMaxBlur_,
      impl_->twoPointFiveDMotionBlurEnabled_,
      impl_->twoPointFiveDMotionBlurShutterAngle_,
      impl_->twoPointFiveDMotionBlurSamples_);
}

bool ArtifactAbstractLayer::isAdjustmentLayer() const {
  return impl_->isAdjustmentLayer_;
}
void ArtifactAbstractLayer::setAdjustmentLayer(bool isAdjustment) {
  if (impl_->isAdjustmentLayer_ != isAdjustment) {
    impl_->isAdjustmentLayer_ = isAdjustment;
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
  }
}

bool ArtifactAbstractLayer::isVisible() const { return impl_->isVisible_; }

void ArtifactAbstractLayer::setParentById(const LayerID &id) {
  if (id.isNil()) {
    clearParent();
    return;
  }

  if (id == this->id()) {
    qWarning("%s", qPrintable(QStringLiteral("[Layer] Reject self-parent: %1")
                                .arg(id.toString())));
    return;
  }

  if (impl_->composition_) {
    auto *composition = dynamic_cast<ArtifactAbstractComposition *>(impl_->composition_.data());
    auto parent = composition->layerById(id);
    if (!parent) {
      qWarning("%s", qPrintable(QStringLiteral("[Layer] Reject invalid parent id: %1")
                                  .arg(id.toString())));
      return;
    }

    LayerID cursor = id;
    int guard = 0;
    while (!cursor.isNil() && guard++ < 1024) {
      if (cursor == this->id()) {
        qWarning("%s", qPrintable(QStringLiteral("[Layer] Reject cyclic parent: %1")
                                    .arg(id.toString())));
        return;
      }
      auto node = composition->layerById(cursor);
      if (!node) {
        break;
      }
      cursor = node->parentLayerId();
    }
  }

  if (impl_->parentLayerId_ == id) {
    return;
  }

  impl_->parentLayerId_ = id;
  if (auto *composition = dynamic_cast<ArtifactAbstractComposition *>(impl_->composition_.data())) {
    composition->nodeStore().setParent(this->id().toString(), id.toString());
  }
  setDirty(LayerDirtyFlag::Transform);
  addDirtyReason(LayerDirtyReason::TransformChanged);
  qDebug("%s", qPrintable(QStringLiteral("[Layer] Parent set to: %1")
                            .arg(id.toString())));
  Q_EMIT changed();
}

LayerID ArtifactAbstractLayer::parentLayerId() const {
  return impl_->parentLayerId_;
}

void ArtifactAbstractLayer::clearParent() {
  if (impl_->parentLayerId_.isNil()) {
    return;
  }
  impl_->parentLayerId_ = LayerID();
  if (auto *composition = dynamic_cast<ArtifactAbstractComposition *>(impl_->composition_.data())) {
    composition->nodeStore().setParent(this->id().toString(), QString{});
  }
  setDirty(LayerDirtyFlag::Transform);
  addDirtyReason(LayerDirtyReason::TransformChanged);
  Q_EMIT changed();
}

bool ArtifactAbstractLayer::hasParent() const {
  return !impl_->parentLayerId_.isNil();
}

bool ArtifactAbstractLayer::is3D() const { return impl_->is3D_; }

void ArtifactAbstractLayer::setIs3D(bool value) {
    if (!assignIfChanged(impl_->is3D_, value)) {
      return;
    }
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::PropertyChanged);
}

QString ArtifactAbstractLayer::projectionSourceLayerId() const {
  return impl_->projectionSourceLayerId_;
}

void ArtifactAbstractLayer::setProjectionSourceLayerId(const QString &layerId) {
    const QString trimmed = layerId.trimmed().left(1024);
    if (!assignIfChanged(impl_->projectionSourceLayerId_, trimmed)) {
      return;
    }
    notifyLayerMutation(this, LayerDirtyFlag::Property,
                        LayerDirtyReason::PropertyChanged);
}

bool ArtifactAbstractLayer::projectionEnabled() const {
  return impl_->projectionEnabled_;
}

void ArtifactAbstractLayer::setProjectionEnabled(bool enabled) {
    if (!assignIfChanged(impl_->projectionEnabled_, enabled)) {
      return;
    }
    notifyLayerMutation(this, LayerDirtyFlag::Property,
                        LayerDirtyReason::PropertyChanged);
}

void ArtifactAbstractLayer::setTimeRemapEnabled(bool enabled) {
  if (!impl_->timeRemapEffect_) impl_->timeRemapEffect_ = std::make_unique<ArtifactCore::TimeRemapEffect>();
  impl_->timeRemapEffect_->setEnabled(enabled);
  impl_->timeRemapEffect_->setHasAudio(hasAudio());
  notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::TimelineChanged);
}

void ArtifactAbstractLayer::clearTimeRemap() {
  if (!impl_->timeRemapEffect_) return;
  impl_->timeRemapEffect_.reset();
  notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::TimelineChanged);
}

void ArtifactAbstractLayer::setTimeRemapKey(int64_t compFrame, double sourceFrame) {
  setTimeRemapKey(compFrame, sourceFrame,
                  ArtifactCore::TimeRemapKeyframe::Interpolation::Linear);
}

void ArtifactAbstractLayer::setTimeRemapKey(
    int64_t compFrame, double sourceFrame,
    ArtifactCore::TimeRemapKeyframe::Interpolation interpolation) {
  if (!impl_->timeRemapEffect_) impl_->timeRemapEffect_ = std::make_unique<ArtifactCore::TimeRemapEffect>();
  double fps = 30.0;
  if (impl_->composition_) {
    auto* composition = dynamic_cast<ArtifactAbstractComposition*>(impl_->composition_.data());
    fps = composition->frameRate().framerate();
    if (fps <= 0.0) fps = 30.0;
  }
  ArtifactCore::TimeRemapKeyframe keyframe;
  keyframe.outputTime = static_cast<double>(compFrame) / fps;
  keyframe.sourceTime = sourceFrame / fps;
  keyframe.interpolation = interpolation;
  impl_->timeRemapEffect_->remap().addKeyframe(keyframe);
  impl_->timeRemapEffect_->remap().setFrameRate(ArtifactCore::FrameRate(fps));
  notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::TimelineChanged);
}

const QVector<ArtifactCore::TimeRemapKeyframe>& ArtifactAbstractLayer::timeRemapKeys() const {
  static const QVector<ArtifactCore::TimeRemapKeyframe> empty;
  return impl_->timeRemapEffect_ ? impl_->timeRemapEffect_->remap().keyframes() : empty;
}

void ArtifactAbstractLayer::setTimeRemapKeys(
    const QVector<ArtifactCore::TimeRemapKeyframe>& keys) {
  if (!impl_->timeRemapEffect_) impl_->timeRemapEffect_ = std::make_unique<ArtifactCore::TimeRemapEffect>();
  impl_->timeRemapEffect_->remap().setKeyframes(keys);
  impl_->timeRemapEffect_->setEnabled(true);
  impl_->timeRemapEffect_->setHasAudio(hasAudio());
  notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::TimelineChanged);
}

void ArtifactAbstractLayer::setTimeRemapFrameBlend(ArtifactCore::FrameBlendMode mode, float amount) {
  if (!impl_->timeRemapEffect_) impl_->timeRemapEffect_ = std::make_unique<ArtifactCore::TimeRemapEffect>();
  impl_->timeRemapEffect_->remap().setFrameBlendMode(mode);
  impl_->timeRemapEffect_->remap().setFrameBlendAmount(amount);
  notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::TimelineChanged);
}

ArtifactCore::FrameBlendMode ArtifactAbstractLayer::timeRemapFrameBlendMode() const {
  return impl_->timeRemapEffect_ ? impl_->timeRemapEffect_->remap().frameBlendMode()
                                 : ArtifactCore::FrameBlendMode::None;
}

float ArtifactAbstractLayer::timeRemapFrameBlendAmount() const {
  return impl_->timeRemapEffect_ ? impl_->timeRemapEffect_->remap().frameBlendAmount() : 0.0f;
}

bool ArtifactAbstractLayer::isStopMotionSamplingEnabled() const { return impl_->stopMotionSamplingEnabled_; }

void ArtifactAbstractLayer::setStopMotionSamplingEnabled(bool enabled) {
  if (!assignIfChanged(impl_->stopMotionSamplingEnabled_, enabled)) return;
  notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::TimelineChanged);
}

double ArtifactAbstractLayer::stopMotionSamplingFrameRate() const { return impl_->stopMotionSamplingFrameRate_; }

void ArtifactAbstractLayer::setStopMotionSamplingFrameRate(double frameRate) {
  const double clamped = std::clamp(frameRate, 1.0, 240.0);
  if (!assignIfChanged(impl_->stopMotionSamplingFrameRate_, clamped)) return;
  notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::TimelineChanged);
}

bool ArtifactAbstractLayer::hasSourceTimeMapping() const {
  return isTimeRemapEnabled() || isStopMotionSamplingEnabled();
}

bool ArtifactAbstractLayer::isTimeRemapEnabled() const {
  return impl_->timeRemapEffect_ && impl_->timeRemapEffect_->isEnabled();
}

double ArtifactAbstractLayer::getSourceFrameAtCompFrame(int64_t compFrame) const {
  if (!hasSourceTimeMapping()) return static_cast<double>(compFrame);
  double fps = 30.0;
  if (impl_->composition_) {
    auto* composition = dynamic_cast<ArtifactAbstractComposition*>(impl_->composition_.data());
    fps = composition->frameRate().framerate();
    if (fps <= 0.0) fps = 30.0;
  }
  double sourceFrame = static_cast<double>(compFrame - inPoint().framePosition() + startTime().framePosition());
  if (isTimeRemapEnabled()) {
    const double outputTime = static_cast<double>(compFrame) / fps;
    float blendFwd = 0.0f, blendBwd = 0.0f;
    sourceFrame = impl_->timeRemapEffect_->processFrame(outputTime, blendFwd, blendBwd);
  }
  if (!isStopMotionSamplingEnabled()) return sourceFrame;
  const double heldRate = std::clamp(impl_->stopMotionSamplingFrameRate_, 1.0, 240.0);
  return std::floor(sourceFrame * heldRate / fps) * fps / heldRate;
}

Size_2D ArtifactAbstractLayer::sourceSize() const { return impl_->sourceSize_; }

void ArtifactAbstractLayer::setSourceSize(const Size_2D& size) {
  impl_->sourceSize_ = size;
}

Size_2D ArtifactAbstractLayer::aabb() const {
  const auto bounds = transformedBoundingBox();
  if (bounds.width() <= 0 || bounds.height() <= 0) return Size_2D();
  Size_2D result;
  result.width = static_cast<int>(std::ceil(bounds.width()));
  result.height = static_cast<int>(std::ceil(bounds.height()));
  return result;
}

QRectF ArtifactAbstractLayer::localBounds() const {
  const auto size = sourceSize();
  if (size.width <= 0 || size.height <= 0) return QRectF();
  return QRectF(0.0, 0.0, static_cast<qreal>(size.width),
                static_cast<qreal>(size.height));
}

bool ArtifactAbstractLayer::getAudio(AudioSegment& outSegment,
                                     const FramePosition& start,
                                     int frameCount, int sampleRate) {
  Q_UNUSED(outSegment);
  Q_UNUSED(start);
  Q_UNUSED(frameCount);
  Q_UNUSED(sampleRate);
  return false;
}

QRectF ArtifactAbstractLayer::visualLocalBounds() const {
  const QRectF baseBounds = localBounds();
  if (!baseBounds.isValid() || baseBounds.width() <= 0.0 ||
      baseBounds.height() <= 0.0) {
    return QRectF();
  }
  QRectF visualBounds = baseBounds;
  const auto uniteCloneBounds = [&](const QTransform &cloneTransform) {
    const QRectF cloneBounds = cloneTransform.mapRect(baseBounds);
    if (cloneBounds.isValid() && cloneBounds.width() > 0.0 &&
        cloneBounds.height() > 0.0) {
      visualBounds = visualBounds.united(cloneBounds);
    }
  };
  const auto applyClonerComponentTransform2D = [this](QTransform &cloneTransform) {
    for (const auto &op : impl_->clonerTransforms_) {
      if (!op.enabled) {
        continue;
      }
      if (op.position.x() != 0.0f || op.position.y() != 0.0f) {
        cloneTransform.translate(op.position.x(), op.position.y());
      }
      if (op.rotation.z() != 0.0f) {
        cloneTransform.rotate(op.rotation.z());
      }
      if (op.scale.x() != 1.0f || op.scale.y() != 1.0f) {
        cloneTransform.scale(op.scale.x(), op.scale.y());
      }
    }
  };

  if (impl_->clonerComponentEnabled_) {
    const int mode = impl_->clonerMode_;
    if (mode == 5) {
      const int cols = std::max(1, impl_->clonerColumns_);
      const int rows = std::max(1, impl_->clonerRows_);
      const int depth = std::max(1, impl_->clonerDepth_);
      const QVector3D startPos(
          -((cols - 1) * impl_->clonerSpacingX_) * 0.5f,
          -((rows - 1) * impl_->clonerSpacingY_) * 0.5f,
          -((depth - 1) * impl_->clonerSpacingZ_) * 0.5f);
      for (int z = 0; z < depth; ++z) {
        for (int y = 0; y < rows; ++y) {
          for (int x = 0; x < cols; ++x) {
            QTransform cloneTransform;
            cloneTransform.translate(startPos.x() + impl_->clonerSpacingX_ * x,
                                     startPos.y() + impl_->clonerSpacingY_ * y);
            applyClonerComponentTransform2D(cloneTransform);
            uniteCloneBounds(cloneTransform);
          }
        }
      }
    } else if (mode == 6) {
      const int count = std::max(1, impl_->clonerRadialCount_);
      const float angleStep =
          count > 1 ? (impl_->clonerEndAngle_ - impl_->clonerStartAngle_) /
                          static_cast<float>(count - 1)
                    : 0.0f;
      constexpr float kPi = 3.14159265358979323846f;
      for (int i = 0; i < count; ++i) {
        const float angle =
            impl_->clonerStartAngle_ + angleStep * static_cast<float>(i);
        const float rad = angle * kPi / 180.0f;
        QTransform cloneTransform;
        cloneTransform.translate(std::cos(rad) * impl_->clonerRadius_,
                                 std::sin(rad) * impl_->clonerRadius_);
        if (impl_->clonerRotationStep_ != 0.0f) {
          cloneTransform.rotate(angle +
                                impl_->clonerRotationStep_ *
                                    static_cast<float>(i));
        }
        applyClonerComponentTransform2D(cloneTransform);
        uniteCloneBounds(cloneTransform);
      }
    } else if (mode == 3) {
      const int count = std::max(1, impl_->clonerCloneCount_);
      for (int i = 0; i < count; ++i) {
        const float cloneIndex = static_cast<float>(i);
        QTransform cloneTransform;
        cloneTransform.translate(
            impl_->clonerOffsetX_ * cloneIndex,
            impl_->clonerOffsetY_ * cloneIndex);
        applyClonerComponentTransform2D(cloneTransform);
        const QRectF jitterBounds = cloneTransform.mapRect(baseBounds).adjusted(
            -std::abs(impl_->clonerJitterX_), -std::abs(impl_->clonerJitterY_),
            std::abs(impl_->clonerJitterX_), std::abs(impl_->clonerJitterY_));
        if (jitterBounds.isValid() && jitterBounds.width() > 0.0 &&
            jitterBounds.height() > 0.0) {
          visualBounds = visualBounds.united(jitterBounds);
        }
      }
    } else {
      const int count = std::max(1, impl_->clonerCloneCount_);
      for (int i = 1; i <= count; ++i) {
        QTransform cloneTransform;
        cloneTransform.translate(impl_->clonerOffsetX_ * static_cast<float>(i),
                                 impl_->clonerOffsetY_ * static_cast<float>(i));
        if (impl_->clonerRotationStep_ != 0.0f) {
          cloneTransform.rotate(impl_->clonerRotationStep_ *
                                static_cast<float>(i));
        }
        applyClonerComponentTransform2D(cloneTransform);
        uniteCloneBounds(cloneTransform);
      }
    }
  }

  for (const auto &effect : getEffects()) {
    const auto cloner = ArtifactCore::dynamicPointerCast<ClonerGenerator>(effect);
    if (!cloner || !cloner->isEnabled()) {
      continue;
    }
    for (const auto &clone : cloner->generateCloneData()) {
      if (!clone.visible) {
        continue;
      }
      const QRectF cloneBounds = mapRectWithMatrix(clone.transform, baseBounds);
      if (cloneBounds.isValid() && cloneBounds.width() > 0.0 &&
          cloneBounds.height() > 0.0) {
        visualBounds = visualBounds.united(cloneBounds);
      }
    }
  }

  return visualBounds;
}

QRectF ArtifactAbstractLayer::transformedBoundingBox() const {
  auto parent = parentLayer();
  const LayerID parentId = impl_->parentLayerId_;
  const quint64 parentRevision = parent ? parent->impl_->geometryRevision_ : 0;
  const int64_t frame = impl_->currentFrame_;
  const auto layoutEnabledProperty = parent
      ? parent->getProperty(QStringLiteral("component.layout.enabled")) : nullptr;
  const auto participationProperty = getProperty(QStringLiteral("component.layout.mode"));
  const bool layoutManaged = layoutEnabledProperty &&
      layoutEnabledProperty->getValue().toBool() &&
      (!participationProperty || participationProperty->getValue().toInt() != 2);
  if (!layoutManaged && impl_->cachedBoundingBoxRevision_ == impl_->geometryRevision_ &&
      impl_->cachedBoundingBoxParentRevision_ == parentRevision &&
      impl_->cachedBoundingBoxFrame_ == frame &&
      impl_->cachedBoundingBoxParentId_ == parentId) return impl_->cachedBoundingBox_;
  const QRectF localRect = localBounds();
  impl_->cachedBoundingBox_ = (!localRect.isValid() || localRect.width() <= 0.0 || localRect.height() <= 0.0)
      ? QRectF() : getGlobalTransform().mapRect(visualLocalBounds());
  impl_->cachedBoundingBoxRevision_ = impl_->geometryRevision_;
  impl_->cachedBoundingBoxParentRevision_ = parentRevision;
  impl_->cachedBoundingBoxFrame_ = frame;
  impl_->cachedBoundingBoxParentId_ = parentId;
  return impl_->cachedBoundingBox_;
}

AnimatableTransform2D &ArtifactAbstractLayer::transform2D() {
  auto* var = getActiveVariant();
  if (var && HasFlag(var->overrideFlags_, VariantOverrideFlags::Transform) && var->transform2DOverride.has_value()) {
    return var->transform2DOverride.value();
  }
  return impl_->transform2d_;
}

const AnimatableTransform2D &ArtifactAbstractLayer::transform2D() const {
  const auto* var = getActiveVariant();
  if (var && HasFlag(var->overrideFlags_, VariantOverrideFlags::Transform) && var->transform2DOverride.has_value()) {
    return var->transform2DOverride.value();
  }
  return impl_->transform2d_;
}

AnimatableTransform3D &ArtifactAbstractLayer::transform3D() {
  auto* var = getActiveVariant();
  if (var && HasFlag(var->overrideFlags_, VariantOverrideFlags::Transform) && var->transform3DOverride.has_value()) {
    return var->transform3DOverride.value();
  }
  return impl_->transform_;
}

const AnimatableTransform3D &ArtifactAbstractLayer::transform3D() const {
  const auto* var = getActiveVariant();
  if (var && HasFlag(var->overrideFlags_, VariantOverrideFlags::Transform) && var->transform3DOverride.has_value()) {
    return var->transform3DOverride.value();
  }
  return impl_->transform_;
}

int64_t ArtifactAbstractLayer::keyframeTimeScale() const {
  // Single source of truth for the frame domain transform keys are stored in.
  // setComposition()/setFrameRate() pin it to the composition frame rate; VP
  // and undo paths must ask here instead of re-deriving it, or a keyed frame
  // can be addressed at the wrong instant.
  return std::max<int64_t>(1, impl_->transform_.keyframeTimeScale());
}

RationalTime ArtifactAbstractLayer::keyframeTimeAtFrame(int64_t frame) const {
  return RationalTime(frame, keyframeTimeScale());
}

RationalTime ArtifactAbstractLayer::currentKeyframeTime() const {
  return keyframeTimeAtFrame(currentTimelineFrame(this));
}

ArtifactCore::AnimationLayerStackT<float> &ArtifactAbstractLayer::animationLayers() {
  return impl_->animationLayers_;
}

const ArtifactCore::AnimationLayerStackT<float> &ArtifactAbstractLayer::animationLayers() const {
  return impl_->animationLayers_;
}

ArtifactCore::AnimationLayerStackT<float> &ArtifactAbstractLayer::animationLayerStack(
    const QString &propertyPath) {
  return impl_->animationPropertyLayers_[propertyPath];
}

const ArtifactCore::AnimationLayerStackT<float> *ArtifactAbstractLayer::animationLayerStack(
    const QString &propertyPath) const {
  const auto it = impl_->animationPropertyLayers_.constFind(propertyPath);
  return it == impl_->animationPropertyLayers_.constEnd() ? nullptr : &it.value();
}

QJsonObject ArtifactAbstractLayer::animationLayersSnapshot() const {
  QJsonObject snapshot;
  snapshot[QStringLiteral("shared")] = impl_->animationLayers_.toJson();
  QJsonObject propertyLayers;
  for (auto it = impl_->animationPropertyLayers_.constBegin();
       it != impl_->animationPropertyLayers_.constEnd(); ++it) {
    propertyLayers[it.key()] = it.value().toJson();
  }
  snapshot[QStringLiteral("properties")] = propertyLayers;
  return snapshot;
}

void ArtifactAbstractLayer::restoreAnimationLayersSnapshot(const QJsonObject &snapshot) {
  impl_->animationLayers_.fromJson(snapshot.value(QStringLiteral("shared")).toObject());
  impl_->animationPropertyLayers_.clear();
  const auto propertyLayers = snapshot.value(QStringLiteral("properties")).toObject();
  for (auto it = propertyLayers.constBegin(); it != propertyLayers.constEnd(); ++it) {
    auto &stack = impl_->animationPropertyLayers_[it.key()];
    stack.fromJson(it.value().toObject());
  }
  notifyLayerMutation(this, LayerDirtyFlag::Property,
                      LayerDirtyReason::PropertyChanged);
}

QJsonObject ArtifactAbstractLayer::deformation2DData() const {
  return impl_->deformation2DData_;
}

void ArtifactAbstractLayer::setDeformation2DData(const QJsonObject &data) {
  if (impl_->deformation2DData_ == data) {
    return;
  }
  impl_->deformation2DData_ = data;
  notifyLayerMutation(this, LayerDirtyFlag::Effect,
                      LayerDirtyReason::EffectChanged);
}

bool ArtifactAbstractLayer::syncDeformation2DControlProperty(
    const QString& propertyPath) {
  const QStringList parts = propertyPath.split(QLatin1Char('.'));
  if (parts.size() != 3 || parts[0] != QStringLiteral("deformation2D") ||
      (parts[2] != QStringLiteral("x") && parts[2] != QStringLiteral("y") &&
       parts[2] != QStringLiteral("rotation") &&
       parts[2] != QStringLiteral("weight"))) return false;
  const auto property = getProperty(propertyPath);
  if (!property) return false;
  QJsonObject state = deformation2DData();
  const QString mode = state.value(QStringLiteral("mode")).toString();
  const QString controlsKey = mode == QStringLiteral("grid")
                                  ? QStringLiteral("gridControls")
                                  : QStringLiteral("pins");
  if ((parts[2] == QStringLiteral("rotation") ||
       parts[2] == QStringLiteral("weight")) && mode == QStringLiteral("grid")) {
    return false;
  }
  QJsonArray controls = state.value(controlsKey).toArray();
  for (qsizetype index = 0; index < controls.size(); ++index) {
    if (!controls[index].isObject()) continue;
    QJsonObject control = controls[index].toObject();
    if (control.value(QStringLiteral("id")).toString() != parts[1]) continue;
    QJsonArray keys;
    for (const auto& key : property->getKeyFrames()) {
      keys.append(QJsonObject{
          {QStringLiteral("frame"), QString::number(
               key.time.rescaledTo(keyframeTimeScale()))},
          {QStringLiteral("value"), QJsonValue::fromVariant(key.value)},
          {QStringLiteral("interpolation"),
           static_cast<int>(key.interpolation)},
          {QStringLiteral("cp1_x"), key.cp1_x},
          {QStringLiteral("cp1_y"), key.cp1_y},
          {QStringLiteral("cp2_x"), key.cp2_x},
          {QStringLiteral("cp2_y"), key.cp2_y},
          {QStringLiteral("roving"), key.roving},
          {QStringLiteral("anchor"), static_cast<int>(key.anchor)},
          {QStringLiteral("colorLabel"),
           static_cast<int>(key.colorLabel)}});
    }
    control[parts[2] + QStringLiteral("Keys")] = keys;
    if (property->hasKeyFrames()) {
      control[parts[2]] =
          QJsonValue::fromVariant(property->getKeyFrames().front().value);
    }
    controls[index] = control;
    state[controlsKey] = controls;
    setDeformation2DData(state);
    return true;
  }
  return false;
}

void ArtifactAbstractLayer::bakeAnimationLayersAtCurrentFrame() {
  bakeAnimationLayersOverRange(currentFrame(), currentFrame());
}

void ArtifactAbstractLayer::bakeAnimationLayersOverRange(int64_t startFrame,
                                                         int64_t endFrame,
                                                         int64_t step) {
  if (step <= 0) step = 1;
  if (endFrame < startFrame) std::swap(startFrame, endFrame);
  const auto bakeStack = [startFrame, endFrame, step](auto& stack, auto baseAt) {
    if (stack.layerCount() == 0) return;
    ArtifactCore::AnimationLayerState state;
    state.blendMode = ArtifactCore::AnimationLayerBlendMode::Override;
    NamedVector<std::pair<FramePosition, float>> samples{
        ContainerName{"Layer.AnimationBakeSamples"}};
    samples.reserve(static_cast<std::size_t>((endFrame - startFrame) / step + 1));
    for (int64_t frameNumber = startFrame; frameNumber <= endFrame;
         frameNumber += step) {
      const FramePosition frame(frameNumber);
      samples.emplace_back(frame, stack.evaluateWithBase(frame, baseAt(frame)));
      if (frameNumber > endFrame - step) break;
    }
    stack.clear();
    const std::size_t layerIndex = stack.addLayer(state);
    auto& values = stack.layer(layerIndex).values;
    values.setCurrent(samples.front()->second);
    for (const auto& sample : samples) {
      values.addKeyFrame(sample.first, sample.second);
    }
  };

  bakeStack(impl_->animationLayers_, [this](const FramePosition&) {
    return impl_->opacity_;
  });
  for (auto it = impl_->animationPropertyLayers_.begin();
       it != impl_->animationPropertyLayers_.end(); ++it) {
    bakeStack(it.value(), [&it](const FramePosition& frame) {
      return it.value().base(frame);
    });
  }
  notifyLayerMutation(this, LayerDirtyFlag::Property,
                      LayerDirtyReason::PropertyChanged);
}

QVector3D ArtifactAbstractLayer::position3D() const {
  const auto time = currentTimelineTime(this);
  return QVector3D(impl_->transform_.positionXAt(time),
                   impl_->transform_.positionYAt(time),
                   impl_->transform_.positionZAt(time));
}

void ArtifactAbstractLayer::setPosition3D(const QVector3D &pos) {
  const auto time = currentTimelineTime(this);
  impl_->transform_.setPosition(time, pos.x(), pos.y());
  impl_->transform_.setPositionZ(time, pos.z());
  changed();
  if (hasRigidBodyPhysics()) {
    syncRigidBodyPhysicsToBounds();
  }
}

QVector3D ArtifactAbstractLayer::rotation3D() const {
  const auto time = currentTimelineTime(this);
  const auto snapshot = impl_->transform_.snapshotAt(time);
  return QVector3D(snapshot.rotationX, snapshot.rotationY,
                   snapshot.rotationZ);
}

void ArtifactAbstractLayer::setRotation3D(const QVector3D &rot) {
  const auto time = currentTimelineTime(this);
  impl_->transform_.setRotationX(time, rot.x());
  impl_->transform_.setRotationY(time, rot.y());
  impl_->transform_.setRotationZ(time, rot.z());
  changed();
  if (hasRigidBodyPhysics()) {
    syncRigidBodyPhysicsToBounds();
  }
}

void ArtifactAbstractLayerImpl::addEffect(
    SharedPtr<ArtifactAbstractEffect> effect) {
  if (!effect)
    return;
  const QString currentId = effect->effectID().toQString().trimmed();
  const QString uniqueId = LayerAbstractUtilities::uniqueEffectIdForLayer(
      effects_, effect->displayName().toQString(), currentId);
  if (currentId.isEmpty() || currentId != uniqueId) {
    effect->setEffectID(UniString::fromQString(uniqueId));
  }
  effects_.push_back(effect);
  qDebug("%s", qPrintable(QStringLiteral("[ArtifactAbstractLayer] Effect added: %1 id=%2")
                            .arg(effect->displayName().toQString(),
                                 effect->effectID().toQString())));
}

void ArtifactAbstractLayerImpl::removeEffect(const UniString &effectID) {
  if (effects_.removeIf(
          [&effectID](const SharedPtr<ArtifactAbstractEffect>& effect) {
            return effect && effect->effectID() == effectID;
          }) != 0) {
    qDebug("%s", qPrintable(QStringLiteral("[ArtifactAbstractLayer] Effect removed: %1")
                              .arg(effectID.toQString())));
  }
}

void ArtifactAbstractLayerImpl::clearEffects() {
  effects_.clear();
  qDebug("[ArtifactAbstractLayer] All effects cleared");
}

std::vector<SharedPtr<ArtifactAbstractEffect>>
ArtifactAbstractLayerImpl::getEffects() const {
  return effects_.toStdVector();
}

SharedPtr<ArtifactAbstractEffect>
ArtifactAbstractLayerImpl::getEffect(const UniString &effectID) const {
  for (const auto &effect : effects_) {
    if (effect && effect->effectID() == effectID) {
      return effect;
    }
  }
  return nullptr;
}

int ArtifactAbstractLayerImpl::effectCount() const {
  return static_cast<int>(effects_.size());
}

bool ArtifactAbstractLayerImpl::hasEnabledRasterizerEffect() const {
  for (const auto &effect : effects_) {
    if (effect && effect->isEnabled() &&
        effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
      return true;
    }
  }
  return false;
}

bool ArtifactAbstractLayerImpl::hasEnabledFullFrameEffect() const {
  for (const auto &effect : effects_) {
    if (effect && effect->isEnabled() && effect->roiHint().requiresFullFrame) {
      return true;
    }
  }
  return false;
}

float ArtifactAbstractLayerImpl::enabledRasterizerOverscanPixels() const {
  float expansion = 0.0f;
  for (const auto &effect : effects_) {
    if (!effect || !effect->isEnabled() || !effect->allowOverscan() ||
        effect->pipelineStage() != EffectPipelineStage::Rasterizer) {
      continue;
    }
    const EffectROIHint hint = effect->roiHint();
    if (!hint.requiresFullFrame) {
      expansion += std::max(0.0f, hint.expansionPixels);
    }
  }
  return expansion;
}

float ArtifactAbstractLayerImpl::cachedRasterizerOverscanPixels() const {
  std::lock_guard<std::mutex> lock(rasterizerOverscanCacheMutex_);
  const std::uint64_t revision =
      effectRevision_.load(std::memory_order_acquire);
  if (cachedRasterizerOverscanValid_ &&
      cachedRasterizerOverscanRevision_ == revision) {
    return cachedRasterizerOverscanPixels_;
  }

  const float expansion = enabledRasterizerOverscanPixels();
  if (effectRevision_.load(std::memory_order_acquire) == revision) {
    cachedRasterizerOverscanPixels_ = expansion;
    cachedRasterizerOverscanRevision_ = revision;
    cachedRasterizerOverscanValid_ = true;
  }
  return expansion;
}

void ArtifactAbstractLayerImpl::addModifier(
    SharedPtr<ArtifactLayerModifier> modifier) {
  if (!modifier) {
    return;
  }

  const QString currentId = modifier->modifierId().trimmed();
  const QString uniqueId = LayerAbstractUtilities::uniqueModifierIdForLayer(
      modifiers_.modifiers(), modifier->displayName(), currentId);
  if (currentId.isEmpty() || currentId != uniqueId) {
    modifier->setModifierId(uniqueId);
  }
  modifiers_.add(std::move(modifier));
}

void ArtifactAbstractLayerImpl::removeModifier(const QString& modifierId) {
  modifiers_.remove(modifierId);
}

void ArtifactAbstractLayerImpl::clearModifiers() {
  modifiers_.clear();
}

std::vector<SharedPtr<ArtifactLayerModifier>>
ArtifactAbstractLayerImpl::getModifiers() const {
  return modifiers_.modifiers();
}

SharedPtr<ArtifactLayerModifier>
ArtifactAbstractLayerImpl::getModifier(const QString& modifierId) const {
  return modifiers_.modifier(modifierId);
}

int ArtifactAbstractLayerImpl::modifierCount() const {
  return modifiers_.count();
}

bool ArtifactAbstractLayerImpl::hasModifiers() const {
  return !modifiers_.isEmpty();
}

void ArtifactAbstractLayer::addEffect(
    SharedPtr<ArtifactAbstractEffect> effect) {
  impl_->addEffect(effect);
  setDirty(LayerDirtyFlag::Effect);
}

void ArtifactAbstractLayer::removeEffect(const UniString &effectID) {
  impl_->removeEffect(effectID);
  setDirty(LayerDirtyFlag::Effect);
}

void ArtifactAbstractLayer::clearEffects() {
  impl_->clearEffects();
  setDirty(LayerDirtyFlag::Effect);
}

std::vector<SharedPtr<ArtifactAbstractEffect>>
ArtifactAbstractLayer::getEffects() const {
  return impl_->getEffects();
}

SharedPtr<ArtifactAbstractEffect>
ArtifactAbstractLayer::getEffect(const UniString &effectID) const {
  return impl_->getEffect(effectID);
}

int ArtifactAbstractLayer::effectCount() const { return impl_->effectCount(); }

bool ArtifactAbstractLayer::hasEnabledRasterizerEffect() const {
  return impl_->hasEnabledRasterizerEffect();
}

bool ArtifactAbstractLayer::hasEnabledFullFrameEffect() const {
  return impl_->hasEnabledFullFrameEffect();
}

float ArtifactAbstractLayer::enabledRasterizerOverscanPixels() const {
  if (hasAnimatedEffectProperties()) {
    return impl_->enabledRasterizerOverscanPixels();
  }
  return impl_->cachedRasterizerOverscanPixels();
}

void ArtifactAbstractLayer::addModifier(
    SharedPtr<ArtifactLayerModifier> modifier) {
  impl_->addModifier(std::move(modifier));
  notifyLayerMutation(this, LayerDirtyFlag::Transform,
                      LayerDirtyReason::PropertyChanged);
}

void ArtifactAbstractLayer::removeModifier(const QString& modifierId) {
  impl_->removeModifier(modifierId);
  notifyLayerMutation(this, LayerDirtyFlag::Transform,
                      LayerDirtyReason::PropertyChanged);
}

void ArtifactAbstractLayer::clearModifiers() {
  impl_->clearModifiers();
  notifyLayerMutation(this, LayerDirtyFlag::Transform,
                      LayerDirtyReason::PropertyChanged);
}

std::vector<SharedPtr<ArtifactLayerModifier>>
ArtifactAbstractLayer::getModifiers() const {
  return impl_->getModifiers();
}

SharedPtr<ArtifactLayerModifier>
ArtifactAbstractLayer::getModifier(const QString& modifierId) const {
  return impl_->getModifier(modifierId);
}

int ArtifactAbstractLayer::modifierCount() const { return impl_->modifierCount(); }

bool ArtifactAbstractLayer::hasModifiers() const { return impl_->hasModifiers(); }

NamedVector<LayerComponentDescriptor>
ArtifactAbstractLayer::layerComponents() const {
  impl_->syncBuiltinComponentDescriptors();
  return impl_->componentHost_.components();
}

NamedVector<LayerComponentDescriptor>
ArtifactAbstractLayer::enabledLayerComponents(
    const LayerComponentPhase phase) const {
  impl_->syncBuiltinComponentDescriptors();
  return impl_->componentHost_.enabledForPhase(phase);
}

NamedVector<LayerGeneratorDescriptor>
ArtifactAbstractLayer::layerGenerators() const {
  impl_->syncBuiltinComponentDescriptors();

  NamedVector<LayerGeneratorDescriptor> generators{
      ContainerName{"Layer.GeneratorDescriptors"}};
  const auto* cloner =
      impl_->componentHost_.find(QStringLiteral("builtin.cloner"));
  if (cloner && cloner->enabled) {
    LayerGeneratorDescriptor generator;
    generator.generatorId = QStringLiteral("generator.compat.cloner.0");
    generator.version = cloner->version;
    generator.enabled = cloner->enabled;
    generator.order = cloner->order;

    const int clonerMode = cloner->settings
                               .value(QStringLiteral("mode"))
                               .toInt(impl_->clonerMode_);
    switch (clonerMode) {
    case 5:
      generator.typeId = QStringLiteral("artifact.generator.cloner.grid");
      break;
    case 6:
      generator.typeId = QStringLiteral("artifact.generator.cloner.radial");
      break;
    default:
      generator.typeId = QStringLiteral("artifact.generator.cloner.linear");
      break;
    }

    generator.settings = cloner->settings;
    generator.settings[QStringLiteral("legacySourceComponentId")] =
        cloner->componentId;
    generator.settings[QStringLiteral("legacyMode")] = clonerMode;
    generator.settings[QStringLiteral("timeOffsetStep")] =
        static_cast<double>(impl_->clonerTimeOffsetStep_);
    generator.settings[QStringLiteral("sequenceEnabled")] =
        impl_->clonerSequenceEnabled_;
    generator.settings[QStringLiteral("sequenceRate")] =
        static_cast<double>(impl_->clonerSequenceRate_);
    generator.settings[QStringLiteral("sequenceSoftness")] =
        static_cast<double>(impl_->clonerSequenceSoftness_);

    if (!impl_->clonerTransforms_.empty()) {
      QJsonArray transformArray;
      for (const auto& op : impl_->clonerTransforms_) {
        QJsonObject transformObj;
        transformObj[QStringLiteral("name")] = op.name;
        transformObj[QStringLiteral("enabled")] = op.enabled;
        transformObj[QStringLiteral("positionX")] =
            static_cast<double>(op.position.x());
        transformObj[QStringLiteral("positionY")] =
            static_cast<double>(op.position.y());
        transformObj[QStringLiteral("positionZ")] =
            static_cast<double>(op.position.z());
        transformObj[QStringLiteral("rotationX")] =
            static_cast<double>(op.rotation.x());
        transformObj[QStringLiteral("rotationY")] =
            static_cast<double>(op.rotation.y());
        transformObj[QStringLiteral("rotationZ")] =
            static_cast<double>(op.rotation.z());
        transformObj[QStringLiteral("scaleX")] =
            static_cast<double>(op.scale.x());
        transformObj[QStringLiteral("scaleY")] =
            static_cast<double>(op.scale.y());
        transformObj[QStringLiteral("scaleZ")] =
            static_cast<double>(op.scale.z());
        transformArray.append(transformObj);
      }
      generator.settings[QStringLiteral("transformStack")] = transformArray;
    }

    generators.push_back(std::move(generator));
  }
  for (const auto& extraGenerator : impl_->extraGeneratorDescriptors_) {
    if (!extraGenerator.enabled) {
      continue;
    }
    generators.push_back(extraGenerator);
  }
  std::stable_sort(
      generators.begin(), generators.end(),
      [](const LayerGeneratorDescriptor& a, const LayerGeneratorDescriptor& b) {
        if (a.order != b.order) {
          return a.order < b.order;
        }
        return a.generatorId < b.generatorId;
      });
  return generators;
}

NamedVector<LayerFieldDescriptor>
ArtifactAbstractLayer::layerFields() const {
  NamedVector<LayerFieldDescriptor> fields{
      ContainerName{"Layer.FieldDescriptors"}};
  fields.reserve(impl_->extraFieldDescriptors_.count());
  for (const auto& extraField : impl_->extraFieldDescriptors_) {
    if (!extraField.enabled) {
      continue;
    }
    auto normalized = extraField;
    normalized.blendMode = normalized.blendMode.trimmed();
    if (normalized.blendMode.isEmpty()) {
      normalized.blendMode = QStringLiteral("normal");
    }
    normalized.strength = std::isfinite(normalized.strength)
                              ? std::clamp(normalized.strength, 0.0f, 1.0f)
                              : 1.0f;
    fields.push_back(std::move(normalized));
  }
  std::stable_sort(
      fields.begin(), fields.end(),
      [](const LayerFieldDescriptor& lhs, const LayerFieldDescriptor& rhs) {
        if (lhs.order != rhs.order) {
          return lhs.order < rhs.order;
        }
        return lhs.fieldId < rhs.fieldId;
      });
  return fields;
}

NamedVector<LayerModifierDescriptor>
ArtifactAbstractLayer::layerCloneModifiers() const {
  NamedVector<LayerModifierDescriptor> modifiers{
      ContainerName{"Layer.CloneModifierDescriptors"}};

  LayerModifierDescriptor timeOffsetModifier;
  timeOffsetModifier.modifierId = QStringLiteral("modifier.compat.timeOffset.0");
  timeOffsetModifier.typeId = QStringLiteral("artifact.modifier.time-offset");
  timeOffsetModifier.enabled = true;
  timeOffsetModifier.order = 0;
  timeOffsetModifier.settings[QStringLiteral("step")] = impl_->clonerTimeOffsetStep_;
  modifiers.push_back(std::move(timeOffsetModifier));

  LayerModifierDescriptor sequenceModifier;
  sequenceModifier.modifierId = QStringLiteral("modifier.compat.sequence.0");
  sequenceModifier.typeId = QStringLiteral("artifact.modifier.sequence");
  sequenceModifier.enabled = impl_->clonerSequenceEnabled_;
  sequenceModifier.order = 10;
  sequenceModifier.settings[QStringLiteral("enabled")] = impl_->clonerSequenceEnabled_;
  sequenceModifier.settings[QStringLiteral("rate")] = impl_->clonerSequenceRate_;
  sequenceModifier.settings[QStringLiteral("softness")] = impl_->clonerSequenceSoftness_;
  modifiers.push_back(std::move(sequenceModifier));

  for (const auto& extraModifier : impl_->extraCloneModifierDescriptors_) {
    if (!extraModifier.enabled) {
      continue;
    }
    modifiers.push_back(extraModifier);
  }

  std::stable_sort(
      modifiers.begin(), modifiers.end(),
      [](const LayerModifierDescriptor& lhs, const LayerModifierDescriptor& rhs) {
        if (lhs.order != rhs.order) {
          return lhs.order < rhs.order;
        }
        return lhs.modifierId < rhs.modifierId;
      });
  return modifiers;
}

NamedVector<QString> ArtifactAbstractLayer::clonerTransformNames() const {
  NamedVector<QString> names{ContainerName{"Layer.ClonerTransformNames"}};
  names.reserve(impl_->clonerTransforms_.size());
  for (std::size_t index = 0; index < impl_->clonerTransforms_.size(); ++index) {
    const auto& operation = impl_->clonerTransforms_[index];
    names.push_back(operation.name.trimmed().isEmpty()
                        ? QStringLiteral("Transform %1").arg(static_cast<int>(index) + 1)
                        : operation.name.trimmed());
  }
  return names;
}

QJsonArray ArtifactAbstractLayer::clonerTransformsSnapshot() const {
  QJsonArray snapshot;
  for (const auto &op : impl_->clonerTransforms_) {
    snapshot.append(QJsonObject{
        {QStringLiteral("name"), op.name},
        {QStringLiteral("enabled"), op.enabled},
        {QStringLiteral("positionX"), static_cast<double>(op.position.x())},
        {QStringLiteral("positionY"), static_cast<double>(op.position.y())},
        {QStringLiteral("positionZ"), static_cast<double>(op.position.z())},
        {QStringLiteral("rotationX"), static_cast<double>(op.rotation.x())},
        {QStringLiteral("rotationY"), static_cast<double>(op.rotation.y())},
        {QStringLiteral("rotationZ"), static_cast<double>(op.rotation.z())},
        {QStringLiteral("scaleX"), static_cast<double>(op.scale.x())},
        {QStringLiteral("scaleY"), static_cast<double>(op.scale.y())},
        {QStringLiteral("scaleZ"), static_cast<double>(op.scale.z())}});
  }
  return snapshot;
}

bool ArtifactAbstractLayer::restoreClonerTransformsSnapshot(
    const QJsonArray &snapshot) {
  const auto finiteClamped = [](double value, double fallback,
                                double minimum, double maximum) {
    if (!std::isfinite(value)) {
      return fallback;
    }
    return std::clamp(value, minimum, maximum);
  };
  NamedVector<ClonerTransformOperation> restored{
      ContainerName{"Layer.ClonerTransforms"}};
  restored.reserve(static_cast<size_t>(snapshot.size()));
  for (const auto &entry : snapshot) {
    if (!entry.isObject()) {
      return false;
    }
    const auto object = entry.toObject();
    ClonerTransformOperation op;
    op.name = object.value(QStringLiteral("name"))
                  .toString(QStringLiteral("Transform"));
    op.enabled = object.value(QStringLiteral("enabled")).toBool(true);
    op.position.setX(static_cast<float>(finiteClamped(
        object.value(QStringLiteral("positionX")).toDouble(0.0), 0.0,
        -100000.0, 100000.0)));
    op.position.setY(static_cast<float>(finiteClamped(
        object.value(QStringLiteral("positionY")).toDouble(0.0), 0.0,
        -100000.0, 100000.0)));
    op.position.setZ(static_cast<float>(finiteClamped(
        object.value(QStringLiteral("positionZ")).toDouble(0.0), 0.0,
        -100000.0, 100000.0)));
    op.rotation.setX(static_cast<float>(finiteClamped(
        object.value(QStringLiteral("rotationX")).toDouble(0.0), 0.0,
        -360000.0, 360000.0)));
    op.rotation.setY(static_cast<float>(finiteClamped(
        object.value(QStringLiteral("rotationY")).toDouble(0.0), 0.0,
        -360000.0, 360000.0)));
    op.rotation.setZ(static_cast<float>(finiteClamped(
        object.value(QStringLiteral("rotationZ")).toDouble(0.0), 0.0,
        -360000.0, 360000.0)));
    op.scale.setX(static_cast<float>(finiteClamped(
        object.value(QStringLiteral("scaleX")).toDouble(1.0), 1.0,
        -100000.0, 100000.0)));
    op.scale.setY(static_cast<float>(finiteClamped(
        object.value(QStringLiteral("scaleY")).toDouble(1.0), 1.0,
        -100000.0, 100000.0)));
    op.scale.setZ(static_cast<float>(finiteClamped(
        object.value(QStringLiteral("scaleZ")).toDouble(1.0), 1.0,
        -100000.0, 100000.0)));
    restored.push_back(std::move(op));
  }
  impl_->clonerTransforms_ = std::move(restored);
  notifyLayerMutation(this, LayerDirtyFlag::Effect,
                      LayerDirtyReason::PropertyChanged);
  return clonerTransformsSnapshot() == snapshot;
}

QJsonObject ArtifactAbstractLayer::componentDescriptorSnapshot() const {
  QJsonArray generators;
  for (const auto &descriptor : impl_->extraGeneratorDescriptors_) {
    generators.append(toJsonObject(descriptor));
  }
  QJsonArray fields;
  for (const auto &descriptor : impl_->extraFieldDescriptors_) {
    fields.append(toJsonObject(descriptor));
  }
  QJsonArray modifiers;
  for (const auto &descriptor : impl_->extraCloneModifierDescriptors_) {
    modifiers.append(toJsonObject(descriptor));
  }
  return QJsonObject{{QStringLiteral("generators"), generators},
                     {QStringLiteral("fields"), fields},
                     {QStringLiteral("cloneModifiers"), modifiers},
                     {QStringLiteral("clonerTransforms"),
                      clonerTransformsSnapshot()}};
}

bool ArtifactAbstractLayer::restoreComponentDescriptorSnapshot(
    const QJsonObject &snapshot) {
  constexpr qsizetype kMaxDescriptors = 1024;
  const auto generatorsValue = snapshot.value(QStringLiteral("generators"));
  const auto fieldsValue = snapshot.value(QStringLiteral("fields"));
  const auto modifiersValue = snapshot.value(QStringLiteral("cloneModifiers"));
  const auto transformsValue =
      snapshot.value(QStringLiteral("clonerTransforms"));
  if (!generatorsValue.isArray() || !fieldsValue.isArray() ||
      !modifiersValue.isArray() || !transformsValue.isArray() ||
      generatorsValue.toArray().size() > kMaxDescriptors ||
      fieldsValue.toArray().size() > kMaxDescriptors ||
      modifiersValue.toArray().size() > kMaxDescriptors) {
    return false;
  }
  NamedVector<LayerGeneratorDescriptor> generators{
      ContainerName{"Layer.RestoredGeneratorDescriptors"}};
  for (const auto &value : generatorsValue.toArray()) {
    if (!value.isObject()) return false;
    const auto descriptor = layerGeneratorDescriptorFromJson(value.toObject());
    if (!descriptor.has_value()) return false;
    generators.push_back(*descriptor);
  }
  NamedVector<LayerFieldDescriptor> fields{
      ContainerName{"Layer.RestoredFieldDescriptors"}};
  for (const auto &value : fieldsValue.toArray()) {
    if (!value.isObject()) return false;
    const auto descriptor = layerFieldDescriptorFromJson(value.toObject());
    if (!descriptor.has_value()) return false;
    fields.push_back(*descriptor);
  }
  NamedVector<LayerModifierDescriptor> modifiers{
      ContainerName{"Layer.RestoredCloneModifierDescriptors"}};
  for (const auto &value : modifiersValue.toArray()) {
    if (!value.isObject()) return false;
    const auto descriptor = layerModifierDescriptorFromJson(value.toObject());
    if (!descriptor.has_value()) return false;
    modifiers.push_back(*descriptor);
  }
  const auto transforms = transformsValue.toArray();
  if (!restoreClonerTransformsSnapshot(transforms)) {
    return false;
  }
  impl_->extraGeneratorDescriptors_.clear();
  impl_->extraFieldDescriptors_.clear();
  impl_->extraCloneModifierDescriptors_.clear();
  for (auto &descriptor : generators) {
    impl_->extraGeneratorDescriptors_.add(std::move(descriptor));
  }
  for (auto &descriptor : fields) {
    impl_->extraFieldDescriptors_.add(std::move(descriptor));
  }
  for (auto &descriptor : modifiers) {
    impl_->extraCloneModifierDescriptors_.add(std::move(descriptor));
  }
  notifyLayerMutation(this, LayerDirtyFlag::Effect,
                      LayerDirtyReason::PropertyChanged);
  return componentDescriptorSnapshot() == snapshot;
}

NamedVector<LayerComponentValidationIssue>
ArtifactAbstractLayer::validateLayerComponents() const {
  impl_->syncBuiltinComponentDescriptors();
  auto issues = impl_->componentHost_.validate();
  const auto validateIds = [&issues](const auto& descriptors,
                                     const QString& kind,
                                     const auto& idOf,
                                     const auto& typeOf) {
    for (std::size_t i = 0; i < descriptors.count(); ++i) {
      const auto* descriptor = descriptors.at(i);
      if (!descriptor) {
        continue;
      }
      const QString id = idOf(*descriptor).trimmed();
      const QString type = typeOf(*descriptor).trimmed();
      if (id.isEmpty() || type.isEmpty()) {
        issues.push_back({id,
                          QStringLiteral("%1 descriptor id and type must be non-empty.")
                              .arg(kind),
                          true});
      }
      for (std::size_t j = i + 1; j < descriptors.count(); ++j) {
        const auto* other = descriptors.at(j);
        if (other && id == idOf(*other).trimmed()) {
          issues.push_back({id,
                            QStringLiteral("Duplicate %1 descriptor id.").arg(kind),
                            true});
        }
      }
    }
  };
  validateIds(impl_->extraGeneratorDescriptors_, QStringLiteral("Generator"),
              [](const auto& descriptor) { return descriptor.generatorId; },
              [](const auto& descriptor) { return descriptor.typeId; });
  validateIds(impl_->extraFieldDescriptors_, QStringLiteral("Field"),
              [](const auto& descriptor) { return descriptor.fieldId; },
              [](const auto& descriptor) { return descriptor.typeId; });
  validateIds(impl_->extraCloneModifierDescriptors_, QStringLiteral("Modifier"),
              [](const auto& descriptor) { return descriptor.modifierId; },
              [](const auto& descriptor) { return descriptor.typeId; });
  return issues;
}

void ArtifactAbstractLayer::setAuthoritativeComponentEvaluationState(
    const LayerEvaluationState& state, const std::int64_t frame) {
  const bool emitterEnabled = impl_->particleEmitterComponentEnabled_;
  for (const auto& fractureEvent : state.pendingFractures) {
    FractureImpact impact;
    impact.impulse = std::max(
        fractureEvent.damage, fractureEvent.impulse.length() / 64.0f);
    impact.speed = fractureEvent.impulse.length();
    impact.stress = std::max(fractureEvent.damage, impact.impulse);
    impact.area = 1.0f;
    const int originalShardCount = impl_->fractureShardCount_;
    impl_->fractureShardCount_ = std::clamp(
        static_cast<int>(fractureEvent.requestedFragmentCount), 1, 4096);
    // Particle spawn is consumed from the explicit Emit-phase queue below.
    // Suppress the legacy coupled emission inside applyFractureImpact().
    impl_->particleEmitterComponentEnabled_ = false;
    applyFractureImpact(impact);
    impl_->particleEmitterComponentEnabled_ = emitterEnabled;
    impl_->fractureShardCount_ = originalShardCount;
  }

  if (emitterEnabled) {
    const auto debrisColor = [this]() {
      switch (static_cast<FracturePreset>(impl_->fracturePreset_)) {
      case FracturePreset::Glass:
        return QVector3D(0.62f, 0.88f, 1.0f);
      case FracturePreset::Concrete:
        return QVector3D(0.62f, 0.58f, 0.52f);
      case FracturePreset::Stone:
        return QVector3D(0.50f, 0.43f, 0.35f);
      case FracturePreset::Metal:
        return QVector3D(1.0f, 0.76f, 0.34f);
      case FracturePreset::Wood:
        return QVector3D(0.58f, 0.34f, 0.16f);
      case FracturePreset::Dust:
        return QVector3D(0.74f, 0.66f, 0.52f);
      }
      return QVector3D(1.0f, 0.72f, 0.28f);
    }();
    bool invertible = false;
    const QMatrix4x4 worldToLocal = getGlobalTransform4x4().inverted(&invertible);
    for (const auto& spawnEvent : state.pendingParticleSpawns) {
      const std::uint32_t spawnCount = std::min<std::uint32_t>(
          spawnEvent.count, 100000U);
      if (spawnCount == 0U) {
        continue;
      }
      const QVector3D sourcePosition = invertible
          ? worldToLocal.map(spawnEvent.position)
          : spawnEvent.position;
      const QVector3D sourceVelocity = invertible
          ? worldToLocal.mapVector(spawnEvent.velocity)
          : spawnEvent.velocity;
      std::mt19937 rng(spawnEvent.seed);
      std::uniform_real_distribution<float> angleJitter(
          -0.65f, 0.65f);
      std::uniform_real_distribution<float> speedScale(0.5f, 1.0f);
      std::uniform_real_distribution<float> sizeDistribution(2.0f, 7.0f);
      const float baseAngle = std::atan2(
          sourceVelocity.y(), sourceVelocity.x());
      const float baseSpeed = std::max(
          1.0f, std::sqrt(sourceVelocity.x() * sourceVelocity.x() +
                          sourceVelocity.y() * sourceVelocity.y()));
      impl_->componentParticles_.reserve(
          impl_->componentParticles_.size() + spawnCount);
      for (std::uint32_t index = 0; index < spawnCount; ++index) {
        const float angle = baseAngle + angleJitter(rng);
        const float speed = baseSpeed * speedScale(rng);
        ArtifactCore::ParticleVertex particle{};
        particle.px = sourcePosition.x();
        particle.py = sourcePosition.y();
        particle.pz = sourcePosition.z();
        particle.vx = std::cos(angle) * speed;
        particle.vy = std::sin(angle) * speed;
        particle.vz = sourceVelocity.z();
        particle.r = debrisColor.x();
        particle.g = debrisColor.y();
        particle.b = debrisColor.z();
        particle.a = 1.0f;
        particle.size = sizeDistribution(rng);
        particle.stretch = std::clamp(
            1.0f + speed / 320.0f, 1.0f, 3.0f);
        particle.rotation = angle;
        particle.age = 0.0f;
        particle.lifetime = impl_->particleEmitterLifetime_;
        impl_->componentParticles_.push_back(particle);
      }
      impl_->componentParticlesLastFrame_ = currentTimelineFrame(this);
    }
  }
  impl_->authoritativeComponentState_ = state;
  impl_->authoritativeComponentFrame_ = frame;
}

ArtifactCore::Optional<LayerEvaluationState>
ArtifactAbstractLayer::authoritativeComponentEvaluationState() const {
  if (!impl_->authoritativeComponentState_ ||
      impl_->authoritativeComponentFrame_ != impl_->currentFrame_) {
    return {};
  }
  return impl_->authoritativeComponentState_;
}

void ArtifactAbstractLayer::clearAuthoritativeComponentEvaluationState() {
  impl_->authoritativeComponentState_.reset();
  impl_->authoritativeComponentFrame_ =
      std::numeric_limits<int64_t>::min();
}

LayerComponentRuntimeSnapshot
ArtifactAbstractLayer::captureComponentRuntimeSnapshot() const {
  auto snapshot = makeShared<LayerComponentRuntimeSnapshotData>();
  snapshot->fractureState = impl_->fractureState_;
  snapshot->componentParticles = impl_->componentParticles_;
  snapshot->fractureMotionLastFrame = impl_->fractureMotionLastFrame_;
  snapshot->componentParticlesLastFrame = impl_->componentParticlesLastFrame_;
  snapshot->lastCollisionImpactFrame = impl_->lastCollisionImpactFrame_;
  const std::size_t estimatedBytes =
      sizeof(LayerComponentRuntimeSnapshotData) +
      snapshot->componentParticles.size() *
          sizeof(ArtifactCore::ParticleVertex) +
      snapshot->fractureState.shards.size() *
          sizeof(FractureShardMotion);
  return {std::move(snapshot), estimatedBytes};
}

bool ArtifactAbstractLayer::restoreComponentRuntimeSnapshot(
    const LayerComponentRuntimeSnapshot& snapshot) {
  if (!snapshot.isValid()) {
    return false;
  }
  const auto data = staticPointerCast<
      const LayerComponentRuntimeSnapshotData>(snapshot.storage);
  if (!data) {
    return false;
  }
  impl_->fractureState_ = data->fractureState;
  impl_->componentParticles_ = data->componentParticles;
  impl_->fractureMotionLastFrame_ = data->fractureMotionLastFrame;
  impl_->componentParticlesLastFrame_ = data->componentParticlesLastFrame;
  impl_->lastCollisionImpactFrame_ = data->lastCollisionImpactFrame;
  return true;
}


QJsonObject ArtifactAbstractLayer::scriptBinding() const {
  return impl_->scriptBinding_;
}

void ArtifactAbstractLayer::setScriptBinding(const QJsonObject& binding) {
  impl_->scriptBinding_ = binding;
  notifyLayerMutation(this, LayerDirtyFlag::Property,
                      LayerDirtyReason::PropertyChanged);
}

void ArtifactAbstractLayer::clearScriptBinding() {
  if (impl_->scriptBinding_.isEmpty()) {
    return;
  }
  impl_->scriptBinding_ = QJsonObject{};
  notifyLayerMutation(this, LayerDirtyFlag::Property,
                      LayerDirtyReason::PropertyChanged);
}

bool ArtifactAbstractLayer::hasScriptBinding() const {
  return !impl_->scriptBinding_.isEmpty();
}

QImage ArtifactAbstractLayer::getThumbnail(int width, int height) const {
  const QSize targetSize(std::clamp(width, 1, 16384),
                         std::clamp(height, 1, 16384));
  if (!impl_->thumbnailCache_.isNull() &&
      impl_->thumbnailCacheSize_ == targetSize) {
    return impl_->thumbnailCache_;
  }

  const auto currentSourceSize = sourceSize();
  impl_->thumbnailCache_ =
      buildLayerThumbnail(targetSize, layerName(), currentSourceSize.width,
                          currentSourceSize.height);
  impl_->thumbnailCacheSize_ = targetSize;

  return impl_->thumbnailCache_;
}

// -- Mask public methods --

void ArtifactAbstractLayer::addMask(const LayerMask &mask) {
  impl_->maskMatteState_.addMask(mask);
}

void ArtifactAbstractLayer::removeMask(int index) {
  impl_->maskMatteState_.removeMask(index);
}

bool ArtifactAbstractLayer::moveMask(int fromIndex, int toIndex) {
  return impl_->maskMatteState_.moveMask(fromIndex, toIndex);
}

void ArtifactAbstractLayer::setMask(int index, const LayerMask &mask) {
  impl_->maskMatteState_.setMask(index, mask);
}

LayerMask ArtifactAbstractLayer::mask(int index) const {
  LayerMask resolved = impl_->maskMatteState_.mask(index);
  applyMaskPropertyState(this, index, resolved);
  return resolved;
}

const LayerMask* ArtifactAbstractLayer::maskView(int index) const noexcept {
  return impl_->maskMatteState_.maskView(index);
}

const LayerMask* ArtifactAbstractLayer::resolvedMaskView(
    int index, LayerMask& resolvedStorage) const {
  const LayerMask* baseMask = maskView(index);
  if (!baseMask) {
    return nullptr;
  }
  if (!hasTimeVaryingMaskProperties(index)) {
    return baseMask;
  }
  resolvedStorage = mask(index);
  return &resolvedStorage;
}

bool ArtifactAbstractLayer::hasTimeVaryingMaskProperties(int index) const {
  if (index < 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->propertyCacheMutex_);
  for (auto it = impl_->propertyCache_.cbegin();
       it != impl_->propertyCache_.cend(); ++it) {
    const QString& path = it.key();
    if (path.size() < 7 || path[0] != QLatin1Char('m') ||
        path[1] != QLatin1Char('a') || path[2] != QLatin1Char('s') ||
        path[3] != QLatin1Char('k') || path[4] != QLatin1Char('.')) {
      continue;
    }

    int parsedIndex = 0;
    qsizetype cursor = 5;
    bool hasDigits = false;
    bool indexMatches = true;
    while (cursor < path.size() && path[cursor] >= QLatin1Char('0') &&
           path[cursor] <= QLatin1Char('9')) {
      hasDigits = true;
      const int digit = path[cursor].unicode() - QLatin1Char('0').unicode();
      if (parsedIndex > (std::numeric_limits<int>::max() - digit) / 10) {
        indexMatches = false;
        break;
      }
      parsedIndex = parsedIndex * 10 + digit;
      ++cursor;
    }
    if (!indexMatches || !hasDigits || cursor >= path.size() ||
        path[cursor] != QLatin1Char('.') || parsedIndex != index) {
      continue;
    }

    const auto& property = it.value();
    if (property && (property->hasKeyFrames() || property->hasExpression())) {
      return true;
    }
  }
  return false;
}

int ArtifactAbstractLayer::maskCount() const {
  return impl_->maskMatteState_.maskCount();
}

std::uint64_t ArtifactAbstractLayer::maskRevision() const {
  return impl_->maskMatteState_.maskRevision();
}

void ArtifactAbstractLayer::clearMasks() {
  impl_->maskMatteState_.clearMasks();
}

bool ArtifactAbstractLayer::hasMasks() const {
  return impl_->maskMatteState_.maskCount() > 0;
}

std::vector<LayerMatteReference> ArtifactAbstractLayer::matteReferences() const {
  return impl_->maskMatteState_.matteReferences();
}

int ArtifactAbstractLayer::matteReferenceCount() const {
  return static_cast<int>(impl_->maskMatteState_.mattes().size());
}

bool ArtifactAbstractLayer::hasEnabledExternalMatteReference() const {
  return enabledExternalMatteReferenceCount() > 0;
}

int ArtifactAbstractLayer::enabledExternalMatteReferenceCount() const {
  const auto &references = impl_->maskMatteState_.mattes();
  const auto targetId = id();
  int count = 0;
  for (const auto &reference : references) {
    if (reference.enabled && !reference.sourceLayerId.isNil() &&
        reference.sourceLayerId != targetId) {
      ++count;
    }
  }
  return count;
}

void ArtifactAbstractLayer::setMatteReferences(const std::vector<LayerMatteReference>& refs) {
  impl_->maskMatteState_.setMatteReferences(refs);
}

void ArtifactAbstractLayer::addMatteReference(const LayerMatteReference& ref) {
  impl_->maskMatteState_.addMatteReference(ref);
}

void ArtifactAbstractLayer::clearMatteReferences() {
  impl_->maskMatteState_.clearMatteReferences();
}

// Opacity
const LayerEffectEnvelope& ArtifactAbstractLayer::effectEnvelope() const {
  return impl_->effectEnvelope_;
}

void ArtifactAbstractLayer::setEffectEnvelope(const LayerEffectEnvelope& envelope) {
  impl_->effectEnvelope_ = envelope;
  const auto dirtyFlags = static_cast<LayerDirtyFlag>(
      static_cast<uint32_t>(LayerDirtyFlag::Property) |
      static_cast<uint32_t>(LayerDirtyFlag::Effect));
  setDirty(dirtyFlags);
  addDirtyReason(LayerDirtyReason::PropertyChanged);
  addDirtyReason(LayerDirtyReason::EffectChanged);
  changed();
}

float ArtifactAbstractLayer::opacity() const {
  float baseOpacity = impl_->opacity_;
  const auto* var = getActiveVariant();
  if (var && HasFlag(var->overrideFlags_, VariantOverrideFlags::Opacity) && var->opacityOverride.has_value()) {
      baseOpacity = var->opacityOverride.value();
  } else {
    const SharedPtr<AbstractProperty> property =
        getProperty(QStringLiteral("layer.opacity"));
    if (property) {
      if ((property->isAnimatable() && property->hasKeyFrames()) ||
          property->hasExpression() || property->hasEnvelopes() ||
          property->hasExternalOverride()) {
        const RationalTime time = currentTimelineTime(this);
        const QVariant animatedValue = evaluateAnimatedPropertyValue(*property, time);
        if (animatedValue.isValid()) {
          baseOpacity = static_cast<float>(animatedValue.toDouble());
        }
      }
    }
  }
  if (impl_->animationLayers_.layerCount() > 0) {
    baseOpacity = impl_->animationLayers_.evaluateWithBase(
        FramePosition(impl_->currentFrame_), baseOpacity);
  }
  const QString modulationPath = modulationPropertyPath(
      QStringLiteral("layer.opacity"));
  if (!modulationPath.isEmpty()) {
    const auto frame = currentTimelineFrame(this);
    const auto frameRate = effectiveLayerFrameRate(this);
    impl_->modulationRouter_.processAtFrame(
        frame, static_cast<float>(frameRate));
    const auto target = Audio::Modulation::modulationTargetId(
        modulationPath.toStdString());
    if (impl_->modulationRouter_.hasTarget(target)) {
      const float modulated = impl_->modulationRouter_.targetValue(
          target, baseOpacity);
      if (std::isfinite(modulated)) {
        baseOpacity = modulated;
      }
    }
  }
  // Phase 2 automation clips: weight-mixed over the modulated base, still
  // before the effect envelope so envelope timing ownership is unchanged.
  if (!impl_->automationClipInstances_.empty()) {
    const double clipParentSeconds = currentTimelineTime(this).toDouble();
    const double clipFps = effectiveLayerFrameRate(this);
    const double clipFreeSeconds = clipFps > 0.0
        ? static_cast<double>(impl_->currentFrame_) / clipFps : 0.0;
    baseOpacity = applyAutomationClipOverlay(
        this, QStringLiteral("layer.opacity"), baseOpacity,
        clipParentSeconds, clipFreeSeconds);
  }
  const float evaluatedOpacity = applyLayerEffectEnvelopeOpacity(
      impl_->effectEnvelope_, baseOpacity, impl_->currentFrame_,
      impl_->inPoint_, impl_->outPoint_, impl_->startTime_);
  return std::isfinite(evaluatedOpacity)
             ? std::clamp(evaluatedOpacity, 0.0f, 1.0f)
             : 1.0f;
}

Audio::Modulation::ModulationRouter& ArtifactAbstractLayer::modulationRouter() {
  return impl_->modulationRouter_;
}

const std::vector<ArtifactCore::AutomationClipInstance>&
ArtifactAbstractLayer::automationClipInstances() const {
  return impl_->automationClipInstances_;
}

void ArtifactAbstractLayer::setAutomationClipInstances(
    const std::vector<ArtifactCore::AutomationClipInstance>& instances) {
  impl_->automationClipInstances_ = instances;
  notifyLayerMutation(this, LayerDirtyFlag::Property,
                      LayerDirtyReason::PropertyChanged);
}

QString ArtifactAbstractLayer::modulationPropertyPath(
    const QString& propertyPath) const {
  const QString layerId = impl_->id.toString().trimmed();
  const QString property = propertyPath.trimmed();
  if (layerId.isEmpty() || property.isEmpty()) {
    return {};
  }
  return QStringLiteral("layer.%1.%2").arg(layerId, property);
}

void ArtifactAbstractLayer::setOpacity(float value) {
  const float fallback = std::isfinite(impl_->opacity_)
                             ? std::clamp(impl_->opacity_, 0.0f, 1.0f)
                             : 1.0f;
  const float clamped = std::isfinite(value)
                            ? std::clamp(value, 0.0f, 1.0f)
                            : fallback;
  
  if (impl_->activeVariantIndex_ != 0) {
      auto* var = getActiveVariant();
      if (var) {
          var->opacityOverride = clamped;
          SetFlag(var->overrideFlags_, VariantOverrideFlags::Opacity);
          notifyLayerMutation(this, LayerDirtyFlag::Property,
                              LayerDirtyReason::PropertyChanged);
          return;
      }
  }

  bool changed = false;
  const SharedPtr<AbstractProperty> property =
      getProperty(QStringLiteral("layer.opacity"));
  if (property) {
    if (property->isAnimatable() && property->hasKeyFrames()) {
        const RationalTime time = currentTimelineTime(this);
        property->addKeyFrame(time, clamped);
        changed = true;
    } else {
        if (impl_->opacity_ != clamped) {
            impl_->opacity_ = clamped;
            property->setValue(clamped);
            changed = true;
        }
    }
  } else {
    if (impl_->opacity_ != clamped) {
      impl_->opacity_ = clamped;
      changed = true;
    }
  }

  if (changed) {
    notifyLayerMutation(this, LayerDirtyFlag::Property,
                        LayerDirtyReason::PropertyChanged);
  }
}

} // namespace Artifact
