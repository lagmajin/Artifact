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

namespace {
ArtifactLayerJsonFactory g_layerJsonFactory = nullptr;
std::mutex g_layerJsonFactoryMutex;

}

void setArtifactLayerJsonFactory(ArtifactLayerJsonFactory factory) {
  std::lock_guard lock(g_layerJsonFactoryMutex);
  g_layerJsonFactory = factory;
}

QJsonObject ArtifactAbstractLayer::toJson() const {

  QJsonObject obj;
  // Basic metadata
  obj["id"] = id().toString();
  obj["name"] = layerName();
  obj["layerNote"] = impl_->layerNote_;
  obj["type"] = static_cast<int>(LayerType::Unknown);
  obj["parentId"] = parentLayerId().toString();
  obj["inPoint"] = (qint64)impl_->inPoint_.framePosition();
  obj["outPoint"] = (qint64)impl_->outPoint_.framePosition();
  obj["startTime"] = (qint64)impl_->startTime_.framePosition();
  obj["isVisible"] = isVisible();
  obj["is3D"] = is3D();
  QJsonObject twoPointFiveD;
  twoPointFiveD["enabled"] = impl_->twoPointFiveDEnabled_;
  twoPointFiveD["depth"] = impl_->twoPointFiveDDepth_;
  twoPointFiveD["cameraDistance"] = impl_->twoPointFiveDCameraDistance_;
  twoPointFiveD["depthOfFieldEnabled"] = impl_->twoPointFiveDDepthOfFieldEnabled_;
  twoPointFiveD["focusDepth"] = impl_->twoPointFiveDFocusDepth_;
  twoPointFiveD["focusRange"] = impl_->twoPointFiveDFocusRange_;
  twoPointFiveD["maxBlur"] = impl_->twoPointFiveDMaxBlur_;
  twoPointFiveD["motionBlurEnabled"] = impl_->twoPointFiveDMotionBlurEnabled_;
  twoPointFiveD["motionBlurShutterAngle"] = impl_->twoPointFiveDMotionBlurShutterAngle_;
  twoPointFiveD["motionBlurSamples"] = impl_->twoPointFiveDMotionBlurSamples_;
  obj["twoPointFiveD"] = twoPointFiveD;
  QJsonObject projection;
  projection["enabled"] = impl_->projectionEnabled_;
  projection["sourceLayerId"] = impl_->projectionSourceLayerId_;
  obj["projection"] = projection;
  obj["blendMode"] = static_cast<int>(layerBlendType());
  obj["isLocked"] = impl_->isLocked_;
  obj["isSelectionLocked"] = impl_->isSelectionLocked_;
  obj["isTransformLocked"] = impl_->isTransformLocked_;
  obj["isTimingLocked"] = impl_->isTimingLocked_;
  obj["isGuide"] = impl_->isGuide_;
  obj["isSolo"] = impl_->isSolo_;
  obj["layerCachePolicy"] = static_cast<int>(impl_->layerCachePolicy_);
  obj["isShy"] = impl_->isShy_;
  obj["labelColorIndex"] = impl_->labelColorIndex_;
  obj["opacity"] = static_cast<double>(impl_->opacity_);
  if (impl_->timeRemapEffect_) {
    QJsonObject timeRemap;
    timeRemap["enabled"] = impl_->timeRemapEffect_->isEnabled();
    timeRemap["frameBlendMode"] = static_cast<int>(
        impl_->timeRemapEffect_->remap().frameBlendMode());
    timeRemap["frameBlendAmount"] = static_cast<double>(
        impl_->timeRemapEffect_->remap().frameBlendAmount());
    QJsonArray keys;
    for (const auto& key : impl_->timeRemapEffect_->remap().keyframes()) {
      QJsonObject entry;
      entry["outputTime"] = key.outputTime;
      entry["sourceTime"] = key.sourceTime;
      entry["interpolation"] = static_cast<int>(key.interpolation);
      entry["bezierHandleInX"] = key.bezierHandleInX;
      entry["bezierHandleInY"] = key.bezierHandleInY;
      entry["bezierHandleOutX"] = key.bezierHandleOutX;
      entry["bezierHandleOutY"] = key.bezierHandleOutY;
      keys.append(entry);
    }
    timeRemap["keys"] = keys;
    obj["timeRemap"] = timeRemap;
  }
  if (impl_->stopMotionSamplingEnabled_) {
    QJsonObject stopMotionSampling;
    stopMotionSampling["enabled"] = true;
    stopMotionSampling["frameRate"] = impl_->stopMotionSamplingFrameRate_;
    obj["stopMotionSampling"] = stopMotionSampling;
  }
  obj["effectEnvelope"] = layerEffectEnvelopeToJson(impl_->effectEnvelope_);
  if (!impl_->deformation2DData_.isEmpty()) {
    obj[QStringLiteral("deformation2D")] = impl_->deformation2DData_;
  }
  obj["animationLayers"] = impl_->animationLayers_.toJson();
  QJsonObject animationPropertyLayers;
  for (auto it = impl_->animationPropertyLayers_.constBegin();
       it != impl_->animationPropertyLayers_.constEnd(); ++it) {
    animationPropertyLayers[it.key()] = it.value().toJson();
  }
  obj["animationPropertyLayers"] = animationPropertyLayers;
  const QJsonObject modulation = serializeLayerModulationRouter(
      impl_->modulationRouter_);
  if (!modulation.isEmpty()) {
    obj[QStringLiteral("modulation")] = modulation;
  }
  if (!impl_->automationClipInstances_.empty()) {
    QJsonArray clips;
    for (const auto& instance : impl_->automationClipInstances_) {
      clips.append(ArtifactCore::automationClipInstanceToJson(instance));
    }
    obj[QStringLiteral("automationClipInstances")] = clips;
  }

  // Mattes
  QJsonArray mattesArr;
  for (const auto &matte : impl_->maskMatteState_.mattes()) {
    mattesArr.append(QJsonValue(matte.toJson()));
  }
  obj["mattes"] = mattesArr;

  obj["transform"] = serializeLayerTransform(transform3D());

  // Modifiers and effects
  QJsonArray modifiersArr;
  for (const auto &modifier : getModifiers()) {
    if (!modifier) {
      continue;
    }
    modifiersArr.append(serializeLayerModifier(*modifier));
  }
  obj["modifiers"] = modifiersArr;

  QJsonArray effectsArr;
  for (const auto &eff : getEffects()) {
    if (!eff)
      continue;
    QJsonObject eobj;
    eobj["id"] = eff->effectID().toQString();
    eobj["displayName"] = eff->displayName().toQString();
    eobj["enabled"] = eff->isEnabled();
    eobj["pipelineStage"] = static_cast<int>(eff->pipelineStage());

    QJsonArray propsArr;
    auto props = eff->getProperties();
    for (const auto &p : props) {
      QJsonObject pobj;
      pobj["name"] = p.getName();
      pobj["type"] = static_cast<int>(p.getType());
      // Serialize value depending on type
      switch (p.getType()) {
      case ArtifactCore::PropertyType::Float:
      case ArtifactCore::PropertyType::Integer:
      case ArtifactCore::PropertyType::Boolean:
      case ArtifactCore::PropertyType::String:
        pobj["value"] = QJsonValue::fromVariant(p.getValue());
        break;
      case ArtifactCore::PropertyType::Color: {
        QColor c = p.getColorValue();
        QJsonObject col;
        col["r"] = c.redF();
        col["g"] = c.greenF();
        col["b"] = c.blueF();
        col["a"] = c.alphaF();
        pobj["value"] = col;
        break;
      }
      default:
        pobj["value"] = QJsonValue();
        break;
      }
      if (const auto editable = eff->editableProperty(p.getName())) {
        const auto serialized =
            ArtifactCore::PropertySerializationBridge::serializeProperty(editable);
        if (!serialized.expression.isEmpty()) {
          pobj["expression"] = serialized.expression;
        }
        if (!serialized.keyframes.isEmpty()) {
          pobj["keyframes"] = serialized.keyframes;
        }
        if (!serialized.envelopes.isEmpty()) {
          pobj["envelopes"] = serialized.envelopes;
        }
      }
      propsArr.append(pobj);
    }
    eobj["properties"] = propsArr;
    effectsArr.append(eobj);
  }
  obj["effects"] = effectsArr;
  obj["isAdjustment"] = impl_->isAdjustmentLayer_;
  QJsonObject physicsObj = impl_->physicsComponent_.settings().toJson();
  physicsObj[QStringLiteral("cloneInitialVelocityY")] =
      static_cast<double>(impl_->clonePhysicsInitialVelocityY_);
  physicsObj[QStringLiteral("cloneMaxBounces")] = impl_->clonePhysicsMaxBounces_;
  obj["physics"] = physicsObj;
  // Keep legacy aliases for projects/readers that predate the nested form.
  obj[QStringLiteral("clonePhysicsInitialVelocityY")] =
      static_cast<double>(impl_->clonePhysicsInitialVelocityY_);
  obj[QStringLiteral("clonePhysicsMaxBounces")] =
      impl_->clonePhysicsMaxBounces_;
  obj["softBodyPhysicsEnabled"] = impl_->softBodyPhysicsEnabled_;
  obj["cloth3DPhysicsEnabled"] = impl_->cloth3DPhysicsEnabled_;
  obj["materialPhysicsEnabled"] = impl_->materialPhysicsEnabled_;
  obj["materialPhysicsPreset"] = impl_->materialPhysicsPreset_;
  QJsonObject motionObj;
  motionObj["enabled"] = impl_->motionDynamicsEnabled_;
  motionObj["mode"] = impl_->motionDynamicsMode_;
  motionObj["stiffness"] = static_cast<double>(impl_->motionDynamicsStiffness_);
  motionObj["damping"] = static_cast<double>(impl_->motionDynamicsDamping_);
  motionObj["mass"] = static_cast<double>(impl_->motionDynamicsMass_);
  motionObj["lagTau"] = static_cast<double>(impl_->motionDynamicsLagTau_);
  motionObj["clampOvershoot"] = impl_->motionDynamicsClampOvershoot_;
  motionObj["overshootLimit"] = static_cast<double>(impl_->motionDynamicsOvershootLimit_);
  obj["motion"] = motionObj;
  QJsonObject fractureObj;
  fractureObj["enabled"] = impl_->fractureEnabled_;
  fractureObj["preset"] = impl_->fracturePreset_;
  fractureObj["crackThreshold"] = static_cast<double>(impl_->fractureCrackThreshold_);
  fractureObj["shatterThreshold"] = static_cast<double>(impl_->fractureShatterThreshold_);
  fractureObj["shardCount"] = impl_->fractureShardCount_;
  fractureObj["shardDamping"] = static_cast<double>(impl_->fractureShardDamping_);
  fractureObj["shardGravity"] = static_cast<double>(impl_->fractureShardGravity_);
  fractureObj["impactSensitivity"] = static_cast<double>(impl_->fractureImpactSensitivity_);
  fractureObj["preGenerate"] = impl_->fracturePreGenerate_;
  fractureObj["triggerFrame"] = static_cast<qint64>(impl_->fractureTriggerFrame_);
  obj["fracture"] = fractureObj;
  QJsonObject trailObj;
  trailObj["enabled"] = impl_->motionTrailEnabled_;
  trailObj["length"] = impl_->motionTrailLength_;
  trailObj["fade"] = static_cast<double>(impl_->motionTrailFade_);
  trailObj["width"] = static_cast<double>(impl_->motionTrailWidth_);
  obj["trail"] = trailObj;
  QJsonObject fragmentAppearanceObj;
  fragmentAppearanceObj["velocityStretchEnabled"] =
      impl_->fragmentVelocityStretchEnabled_;
  fragmentAppearanceObj["velocityStretchStrength"] =
      static_cast<double>(impl_->fragmentVelocityStretchStrength_);
  fragmentAppearanceObj["velocityStretchMax"] =
      static_cast<double>(impl_->fragmentVelocityStretchMax_);
  fragmentAppearanceObj["colorVariationEnabled"] =
      impl_->fragmentColorVariationEnabled_;
  fragmentAppearanceObj["colorVariation"] =
      static_cast<double>(impl_->fragmentColorVariation_);
  fragmentAppearanceObj["clonerOutputEnabled"] =
      impl_->fragmentClonerOutputEnabled_;
  fragmentAppearanceObj["clonerOutputCount"] =
      impl_->fragmentClonerOutputCount_;
  fragmentAppearanceObj["clonerOutputSpacingX"] =
      static_cast<double>(impl_->fragmentClonerOutputSpacingX_);
  fragmentAppearanceObj["clonerOutputSpacingY"] =
      static_cast<double>(impl_->fragmentClonerOutputSpacingY_);
  fragmentAppearanceObj["clonerOutputTimeOffsetFrames"] =
      static_cast<double>(impl_->fragmentClonerOutputTimeOffsetFrames_);
  obj["fragmentAppearance"] = fragmentAppearanceObj;
  QJsonObject componentsObj;
  componentsObj["scriptEnabled"] = impl_->scriptComponentEnabled_;
  componentsObj["clonerEnabled"] = impl_->clonerComponentEnabled_;
  componentsObj["layoutEnabled"] = impl_->layoutComponentEnabled_;
  componentsObj["collisionEnabled"] = impl_->collisionComponentEnabled_;
  componentsObj["collisionBodyType"] = impl_->collisionBodyType_;
  componentsObj["collisionShape"] = impl_->collisionShape_;
  componentsObj["collisionWidth"] =
      static_cast<double>(impl_->collisionWidth_);
  componentsObj["collisionHeight"] =
      static_cast<double>(impl_->collisionHeight_);
  componentsObj["collisionRadius"] =
      static_cast<double>(impl_->collisionRadius_);
  componentsObj["collisionOffsetX"] =
      static_cast<double>(impl_->collisionOffsetX_);
  componentsObj["collisionOffsetY"] =
      static_cast<double>(impl_->collisionOffsetY_);
  componentsObj["collisionFloorY"] =
      static_cast<double>(impl_->collisionFloorY_);
  componentsObj["collisionCompositionBounds"] =
      impl_->collisionCompositionBounds_;
  componentsObj["collisionDisplayColor"] =
      impl_->collisionDisplayColor_.name(QColor::HexArgb);
  componentsObj["jointEnabled"] = impl_->jointComponentEnabled_;
  componentsObj["jointType"] = impl_->jointType_;
  componentsObj["jointTargetLayer"] = impl_->jointTargetLayerName_;
  componentsObj["jointTargetAnchorY"] = impl_->jointTargetAnchorY_;
  componentsObj["jointTargetAnchorX"] = impl_->jointTargetAnchorX_;
  componentsObj["jointOwnerAnchorY"] = impl_->jointOwnerAnchorY_;
  componentsObj["jointOwnerAnchorX"] = impl_->jointOwnerAnchorX_;
  componentsObj["jointLength"] =
      static_cast<double>(impl_->jointLength_);
  componentsObj["jointStiffness"] =
      static_cast<double>(impl_->jointStiffness_);
  componentsObj["jointDamping"] =
      static_cast<double>(impl_->jointDamping_);
  componentsObj["jointAngleLimitEnabled"] = impl_->jointAngleLimitEnabled_;
  componentsObj["jointLowerAngle"] = static_cast<double>(impl_->jointLowerAngle_);
  componentsObj["jointUpperAngle"] = static_cast<double>(impl_->jointUpperAngle_);
  componentsObj["jointAxisX"] = static_cast<double>(impl_->jointAxisX_);
  componentsObj["jointAxisY"] = static_cast<double>(impl_->jointAxisY_);
  componentsObj["jointLinearLimitEnabled"] = impl_->jointLinearLimitEnabled_;
  componentsObj["jointLowerLimit"] = static_cast<double>(impl_->jointLowerLimit_);
  componentsObj["jointUpperLimit"] = static_cast<double>(impl_->jointUpperLimit_);
  componentsObj["jointMotorEnabled"] = impl_->jointMotorEnabled_;
  componentsObj["jointMotorSpeed"] = static_cast<double>(impl_->jointMotorSpeed_);
  componentsObj["jointMotorForce"] = static_cast<double>(impl_->jointMotorForce_);
  componentsObj["jointBreakForce"] = static_cast<double>(impl_->jointBreakForce_);
  componentsObj["crowdEnabled"] = impl_->crowdComponentEnabled_;
  componentsObj["crowdCohesion"] =
      static_cast<double>(impl_->crowdCohesion_);
  componentsObj["crowdSeparation"] =
      static_cast<double>(impl_->crowdSeparation_);
  componentsObj["crowdAlignment"] =
      static_cast<double>(impl_->crowdAlignment_);
  componentsObj["crowdMaxSpeed"] =
      static_cast<double>(impl_->crowdMaxSpeed_);
  componentsObj["crowdJitter"] =
      static_cast<double>(impl_->crowdJitter_);
  componentsObj["particleEmitterEnabled"] =
      impl_->particleEmitterComponentEnabled_;
  componentsObj["particleEmitterCount"] = impl_->particleEmitterCount_;
  componentsObj["particleEmitterSpeed"] =
      static_cast<double>(impl_->particleEmitterSpeed_);
  componentsObj["particleEmitterLifetime"] =
      static_cast<double>(impl_->particleEmitterLifetime_);
  componentsObj["fluidEnabled"] = impl_->fluidComponentEnabled_;
  componentsObj["fluidMode"] = impl_->fluidMode_;
  componentsObj["fluidGridWidth"] = impl_->fluidGridWidth_;
  componentsObj["fluidGridHeight"] = impl_->fluidGridHeight_;
  componentsObj["fluidViscosity"] = static_cast<double>(impl_->fluidViscosity_);
  componentsObj["fluidDiffusion"] = static_cast<double>(impl_->fluidDiffusion_);
  componentsObj["fluidBuoyancy"] = static_cast<double>(impl_->fluidBuoyancy_);
  componentsObj["fluidVorticity"] = static_cast<double>(impl_->fluidVorticity_);
  componentsObj["fluidSolverIterations"] = impl_->fluidSolverIterations_;
  componentsObj["liquidFillAmount"] =
      static_cast<double>(impl_->liquidFillAmount_);
  componentsObj["liquidInflowRate"] =
      static_cast<double>(impl_->liquidInflowRate_);
  componentsObj["liquidInflowWidth"] =
      static_cast<double>(impl_->liquidInflowWidth_);
  componentsObj["liquidInflowSpeed"] =
      static_cast<double>(impl_->liquidInflowSpeed_);
  componentsObj["liquidInflowPosition"] =
      static_cast<double>(impl_->liquidInflowPosition_);
  componentsObj["liquidOpeningEdge"] = impl_->liquidOpeningEdge_;
  componentsObj["liquidSpillCullMargin"] =
      static_cast<double>(impl_->liquidSpillCullMargin_);
  componentsObj["liquidGravity"] =
      static_cast<double>(impl_->liquidGravity_);
  componentsObj["liquidSurfaceTension"] =
      static_cast<double>(impl_->liquidSurfaceTension_);
  componentsObj["liquidParticleSpacing"] =
      static_cast<double>(impl_->liquidParticleSpacing_);
  componentsObj["liquidSubsteps"] = impl_->liquidSubsteps_;
  componentsObj["liquidSurfaceOpacity"] =
      static_cast<double>(impl_->liquidSurfaceOpacity_);
  componentsObj["liquidEdgeOpacity"] =
      static_cast<double>(impl_->liquidEdgeOpacity_);
  componentsObj["liquidFoamAmount"] =
      static_cast<double>(impl_->liquidFoamAmount_);
  componentsObj["liquidContainerOpacity"] =
      static_cast<double>(impl_->liquidContainerOpacity_);
  componentsObj["liquidContainerWidth"] =
      static_cast<double>(impl_->liquidContainerWidth_);
  QJsonObject liquidColorObj;
  liquidColorObj["r"] = impl_->liquidColor_.r();
  liquidColorObj["g"] = impl_->liquidColor_.g();
  liquidColorObj["b"] = impl_->liquidColor_.b();
  liquidColorObj["a"] = impl_->liquidColor_.a();
  componentsObj["liquidColor"] = liquidColorObj;
  QJsonObject liquidFoamColorObj;
  liquidFoamColorObj["r"] = impl_->liquidFoamColor_.r();
  liquidFoamColorObj["g"] = impl_->liquidFoamColor_.g();
  liquidFoamColorObj["b"] = impl_->liquidFoamColor_.b();
  liquidFoamColorObj["a"] = impl_->liquidFoamColor_.a();
  componentsObj["liquidFoamColor"] = liquidFoamColorObj;
  componentsObj["layoutMode"] = impl_->layoutMode_;
  componentsObj["layoutAnchorMode"] = impl_->layoutAnchorMode_;
  componentsObj["layoutHorizontalPin"] = impl_->layoutHorizontalPin_;
  componentsObj["layoutVerticalPin"] = impl_->layoutVerticalPin_;
  componentsObj["layoutScaleMode"] = impl_->layoutScaleMode_;
  componentsObj["layoutResponsiveEnabled"] = impl_->layoutResponsiveEnabled_;
  componentsObj["layoutResponsiveOffsetX"] = static_cast<double>(impl_->layoutResponsiveOffsetX_);
  componentsObj["layoutResponsiveOffsetY"] = static_cast<double>(impl_->layoutResponsiveOffsetY_);
  componentsObj["layoutSafeAreaEnabled"] = impl_->layoutSafeAreaEnabled_;
  componentsObj["layoutSafeAreaPaddingX"] = static_cast<double>(impl_->layoutSafeAreaPaddingX_);
  componentsObj["layoutSafeAreaPaddingY"] = static_cast<double>(impl_->layoutSafeAreaPaddingY_);
  componentsObj["layoutStackDirection"] = impl_->layoutStackDirection_;
  componentsObj["layoutGap"] = static_cast<double>(impl_->layoutGap_);
  componentsObj["layoutMaxPerRow"] = impl_->layoutMaxPerRow_;
  componentsObj["clonerMode"] = impl_->clonerMode_;
  componentsObj["clonerCloneCount"] = impl_->clonerCloneCount_;
  componentsObj["clonerOffsetX"] = static_cast<double>(impl_->clonerOffsetX_);
  componentsObj["clonerOffsetY"] = static_cast<double>(impl_->clonerOffsetY_);
  componentsObj["clonerOffsetZ"] = static_cast<double>(impl_->clonerOffsetZ_);
  componentsObj["clonerJitterX"] = static_cast<double>(impl_->clonerJitterX_);
  componentsObj["clonerJitterY"] = static_cast<double>(impl_->clonerJitterY_);
  componentsObj["clonerJitterZ"] = static_cast<double>(impl_->clonerJitterZ_);
  componentsObj["clonerSeed"] = impl_->clonerSeed_;
  componentsObj["clonerColumns"] = impl_->clonerColumns_;
  componentsObj["clonerRows"] = impl_->clonerRows_;
  componentsObj["clonerDepth"] = impl_->clonerDepth_;
  componentsObj["clonerSpacingX"] = static_cast<double>(impl_->clonerSpacingX_);
  componentsObj["clonerSpacingY"] = static_cast<double>(impl_->clonerSpacingY_);
  componentsObj["clonerSpacingZ"] = static_cast<double>(impl_->clonerSpacingZ_);
  componentsObj["clonerRadialCount"] = impl_->clonerRadialCount_;
  componentsObj["clonerRadius"] = static_cast<double>(impl_->clonerRadius_);
  componentsObj["clonerStartAngle"] = static_cast<double>(impl_->clonerStartAngle_);
  componentsObj["clonerEndAngle"] = static_cast<double>(impl_->clonerEndAngle_);
  componentsObj["clonerRotationStep"] = static_cast<double>(impl_->clonerRotationStep_);
  componentsObj["clonerOpacityDecay"] = static_cast<double>(impl_->clonerOpacityDecay_);
  QJsonArray clonerTransformsArr;
  for (const auto &op : impl_->clonerTransforms_) {
    QJsonObject transformObj;
    transformObj["name"] = op.name;
    transformObj["enabled"] = op.enabled;
    transformObj["positionX"] = static_cast<double>(op.position.x());
    transformObj["positionY"] = static_cast<double>(op.position.y());
    transformObj["positionZ"] = static_cast<double>(op.position.z());
    transformObj["rotationX"] = static_cast<double>(op.rotation.x());
    transformObj["rotationY"] = static_cast<double>(op.rotation.y());
    transformObj["rotationZ"] = static_cast<double>(op.rotation.z());
    transformObj["scaleX"] = static_cast<double>(op.scale.x());
    transformObj["scaleY"] = static_cast<double>(op.scale.y());
    transformObj["scaleZ"] = static_cast<double>(op.scale.z());
    clonerTransformsArr.append(transformObj);
  }
  componentsObj["clonerTransforms"] = clonerTransformsArr;
  if (!impl_->extraGeneratorDescriptors_.isEmpty()) {
    QJsonArray generatorsArr;
    for (const auto& generator : impl_->extraGeneratorDescriptors_) {
      generatorsArr.append(toJsonObject(generator));
    }
    componentsObj["generators"] = generatorsArr;
  }
  if (!impl_->extraFieldDescriptors_.isEmpty()) {
    QJsonArray fieldsArr;
    for (const auto& field : impl_->extraFieldDescriptors_) {
      fieldsArr.append(toJsonObject(field));
    }
    componentsObj["fields"] = fieldsArr;
  }
  if (!impl_->extraCloneModifierDescriptors_.isEmpty()) {
    QJsonArray modifiersArr;
    for (const auto& modifier : impl_->extraCloneModifierDescriptors_) {
      modifiersArr.append(toJsonObject(modifier));
    }
    componentsObj["cloneModifiers"] = modifiersArr;
  }
  if (!impl_->scriptBinding_.isEmpty()) {
    componentsObj["scriptBinding"] = impl_->scriptBinding_;
  }
  obj["components"] = componentsObj;
  impl_->syncBuiltinComponentDescriptors();
  obj["componentGraph"] = impl_->componentHost_.toJson();

  QJsonArray variantsArr;
  for (const auto& varPtr : impl_->variants_) {
      if (!varPtr) continue;
      QJsonObject varObj;
       varObj["name"] = QString::fromStdString(ArtifactCore::toStdString(varPtr->name_));
      varObj["flags"] = static_cast<int>(varPtr->overrideFlags_);

      if (HasFlag(varPtr->overrideFlags_, VariantOverrideFlags::Opacity) && varPtr->opacityOverride.has_value()) {
          varObj["opacity"] = static_cast<double>(varPtr->opacityOverride.value());
      }
      if (HasFlag(varPtr->overrideFlags_, VariantOverrideFlags::BlendMode) && varPtr->blendModeOverride.has_value()) {
          varObj["blendMode"] = static_cast<int>(varPtr->blendModeOverride.value());
      }
      if (HasFlag(varPtr->overrideFlags_, VariantOverrideFlags::Transform) && varPtr->transform3DOverride.has_value()) {
          QJsonObject vtrans;
          const auto& vt3 = varPtr->transform3DOverride.value();
          vtrans["px"] = vt3.positionX();
          vtrans["py"] = vt3.positionY();
          vtrans["pz"] = vt3.positionZ();
          vtrans["rx"] = vt3.rotationZ();
          vtrans["rotationX"] = vt3.rotationX();
          vtrans["rotationY"] = vt3.rotationY();
          vtrans["rotationZ"] = vt3.rotationZ();
          vtrans["sx"] = vt3.scaleX();
          vtrans["sy"] = vt3.scaleY();
          vtrans["ax"] = vt3.anchorX();
          vtrans["ay"] = vt3.anchorY();
          vtrans["az"] = vt3.anchorZ();
          varObj["transform"] = vtrans;
      }
      variantsArr.append(varObj);
  }
  obj["variants"] = variantsArr;
  obj["activeVariantIndex"] = static_cast<int>(impl_->activeVariantIndex_);

  const QJsonArray masks = serializeLayerMasks(impl_->maskMatteState_.masks());
  if (!masks.isEmpty()) {
    obj["masks"] = masks;
  }

  return obj;
}

ArtifactAbstractLayerPtr
ArtifactAbstractLayer::fromJson(const QJsonObject &obj) {
  ArtifactLayerJsonFactory factory = nullptr;
  {
    std::lock_guard lock(g_layerJsonFactoryMutex);
    factory = g_layerJsonFactory;
  }
  if (factory) {
    return factory(obj);
  }
  // Default: base class is abstract and cannot be instantiated here.
  // Subclasses should implement their own fromJson factory. Return nullptr
  // to indicate this layer cannot be constructed generically.
  Q_UNUSED(obj);
  return ArtifactAbstractLayerPtr();
}


void ArtifactAbstractLayer::fromJsonProperties(const QJsonObject &obj) {
  impl_->deformation2DData_ = obj.value(QStringLiteral("deformation2D")).toObject();
  if (obj.value(QStringLiteral("modulation")).isObject()) {
    restoreLayerModulationRouter(obj.value(QStringLiteral("modulation")).toObject(),
                                 impl_->modulationRouter_);
  }
  impl_->automationClipInstances_.clear();
  if (obj.value(QStringLiteral("automationClipInstances")).isArray()) {
    for (const auto& value :
         obj.value(QStringLiteral("automationClipInstances")).toArray()) {
      if (!value.isObject()) {
        continue;
      }
      ArtifactCore::AutomationClipInstance instance;
      if (ArtifactCore::automationClipInstanceFromJson(value.toObject(), instance)) {
        impl_->automationClipInstances_.push_back(std::move(instance));
      }
    }
  }
  if (obj.contains("animationLayers") && obj["animationLayers"].isObject())
    impl_->animationLayers_.fromJson(obj["animationLayers"].toObject());
  if (obj.contains("animationPropertyLayers") &&
      obj["animationPropertyLayers"].isObject()) {
    impl_->animationPropertyLayers_.clear();
    const auto propertyLayers = obj["animationPropertyLayers"].toObject();
    for (auto it = propertyLayers.constBegin(); it != propertyLayers.constEnd(); ++it) {
      auto &stack = impl_->animationPropertyLayers_[it.key()];
      stack.fromJson(it.value().toObject());
    }
  }
  if (obj.contains("name"))
    setLayerName(obj["name"].toString());
  else if (obj.contains("layerName"))
    setLayerName(obj["layerName"].toString());
  if (obj.contains("layerNote"))
    setLayerNote(obj["layerNote"].toString());
  if (obj.contains("inPoint"))
    setInPoint(FramePosition(obj["inPoint"].toVariant().toLongLong()));
  if (obj.contains("outPoint"))
    setOutPoint(FramePosition(obj["outPoint"].toVariant().toLongLong()));
  if (obj.contains("startTime"))
    setStartTime(FramePosition(obj["startTime"].toVariant().toLongLong()));
  if (obj.contains("isVisible"))
    setVisible(obj["isVisible"].toBool());
  if (obj.contains("is3D"))
    setIs3D(obj["is3D"].toBool());
  if (obj.value(QStringLiteral("twoPointFiveD")).isObject()) {
    const QJsonObject twoPointFiveD = obj.value(QStringLiteral("twoPointFiveD")).toObject();
    impl_->twoPointFiveDEnabled_ = twoPointFiveD.value(QStringLiteral("enabled")).toBool(false);
    impl_->twoPointFiveDDepth_ = static_cast<float>(twoPointFiveD.value(QStringLiteral("depth")).toDouble(0.0));
    impl_->twoPointFiveDCameraDistance_ = std::clamp(static_cast<float>(twoPointFiveD.value(QStringLiteral("cameraDistance")).toDouble(1000.0)), 1.0f, 1000000.0f);
    impl_->twoPointFiveDDepthOfFieldEnabled_ = twoPointFiveD.value(QStringLiteral("depthOfFieldEnabled")).toBool(false);
    impl_->twoPointFiveDFocusDepth_ = static_cast<float>(twoPointFiveD.value(QStringLiteral("focusDepth")).toDouble(0.0));
    impl_->twoPointFiveDFocusRange_ = std::clamp(static_cast<float>(twoPointFiveD.value(QStringLiteral("focusRange")).toDouble(250.0)), 1.0f, 1000000.0f);
    impl_->twoPointFiveDMaxBlur_ = std::clamp(static_cast<float>(twoPointFiveD.value(QStringLiteral("maxBlur")).toDouble(12.0)), 0.0f, 64.0f);
    impl_->twoPointFiveDMotionBlurEnabled_ = twoPointFiveD.value(QStringLiteral("motionBlurEnabled")).toBool(false);
    impl_->twoPointFiveDMotionBlurShutterAngle_ = std::clamp(static_cast<float>(twoPointFiveD.value(QStringLiteral("motionBlurShutterAngle")).toDouble(180.0)), 0.0f, 720.0f);
    impl_->twoPointFiveDMotionBlurSamples_ = std::clamp(twoPointFiveD.value(QStringLiteral("motionBlurSamples")).toInt(4), 2, 8);
  if (obj.value(QStringLiteral("projection")).isObject()) {
    const QJsonObject projection = obj.value(QStringLiteral("projection")).toObject();
    impl_->projectionEnabled_ = projection.value(QStringLiteral("enabled")).toBool(false);
    impl_->projectionSourceLayerId_ =
        projection.value(QStringLiteral("sourceLayerId")).toString().trimmed().left(1024);
  }
  }
  if (obj.contains("isLocked"))
    setLocked(obj["isLocked"].toBool());
  if (obj.contains("isSelectionLocked"))
    setSelectionLocked(obj["isSelectionLocked"].toBool());
  if (obj.contains("isTransformLocked"))
    setTransformLocked(obj["isTransformLocked"].toBool());
  if (obj.contains("isTimingLocked"))
    setTimingLocked(obj["isTimingLocked"].toBool());
  if (obj.contains("isGuide"))
    setGuide(obj["isGuide"].toBool());
  if (obj.contains("isSolo"))
    setSolo(obj["isSolo"].toBool());
  if (obj.contains("layerCachePolicy"))
    setLayerCachePolicy(static_cast<LayerCachePolicy>(
        obj["layerCachePolicy"].toInt(static_cast<int>(LayerCachePolicy::Default))));
  if (obj.contains("isShy"))
    setShy(obj["isShy"].toBool());
  if (obj.contains("labelColorIndex"))
    setLabelColorIndex(obj["labelColorIndex"].toInt(0));
  if (obj.contains("opacity"))
    setOpacity(static_cast<float>(obj["opacity"].toDouble(1.0)));
  if (obj.value(QStringLiteral("timeRemap")).isObject()) {
    const QJsonObject timeRemap = obj.value(QStringLiteral("timeRemap")).toObject();
    QVector<ArtifactCore::TimeRemapKeyframe> keys;
    const auto jsonKeys = timeRemap.value(QStringLiteral("keys")).toArray();
    keys.reserve(jsonKeys.size());
    for (const QJsonValue& value : jsonKeys) {
      if (!value.isObject()) continue;
      const QJsonObject entry = value.toObject();
      const double outputTime = entry.value(QStringLiteral("outputTime")).toDouble();
      const double sourceTime = entry.value(QStringLiteral("sourceTime")).toDouble();
      if (!std::isfinite(outputTime) || !std::isfinite(sourceTime)) continue;
      ArtifactCore::TimeRemapKeyframe key;
      key.outputTime = outputTime;
      key.sourceTime = sourceTime;
      key.interpolation = static_cast<ArtifactCore::TimeRemapKeyframe::Interpolation>(
          std::clamp(entry.value(QStringLiteral("interpolation")).toInt(), 0, 5));
      key.bezierHandleInX = static_cast<float>(entry.value(QStringLiteral("bezierHandleInX")).toDouble());
      key.bezierHandleInY = static_cast<float>(entry.value(QStringLiteral("bezierHandleInY")).toDouble());
      key.bezierHandleOutX = static_cast<float>(entry.value(QStringLiteral("bezierHandleOutX")).toDouble());
      key.bezierHandleOutY = static_cast<float>(entry.value(QStringLiteral("bezierHandleOutY")).toDouble());
      keys.append(key);
    }
    if (!keys.isEmpty() || timeRemap.value(QStringLiteral("enabled")).toBool(false)) {
      setTimeRemapKeys(keys);
      setTimeRemapEnabled(timeRemap.value(QStringLiteral("enabled")).toBool(true));
      setTimeRemapFrameBlend(
          static_cast<ArtifactCore::FrameBlendMode>(std::clamp(
              timeRemap.value(QStringLiteral("frameBlendMode")).toInt(), 0, 3)),
          static_cast<float>(timeRemap.value(QStringLiteral("frameBlendAmount")).toDouble(0.5)));
    }
  }
  if (obj.value(QStringLiteral("stopMotionSampling")).isObject()) {
    const QJsonObject stopMotionSampling =
        obj.value(QStringLiteral("stopMotionSampling")).toObject();
    setStopMotionSamplingFrameRate(
        stopMotionSampling.value(QStringLiteral("frameRate")).toDouble(12.0));
    setStopMotionSamplingEnabled(
        stopMotionSampling.value(QStringLiteral("enabled")).toBool(false));
  }
  if (obj.contains("effectEnvelope") && obj["effectEnvelope"].isObject())
    setEffectEnvelope(layerEffectEnvelopeFromJson(obj["effectEnvelope"].toObject()));
  if (obj.contains("blendMode")) {
    const int mode = obj["blendMode"].toInt(
        static_cast<int>(LAYER_BLEND_TYPE::BLEND_NORMAL));
    setBlendMode(static_cast<LAYER_BLEND_TYPE>(mode));
  }

  // Mattes
  if (obj.contains("mattes") && obj["mattes"].isArray()) {
    auto mattesArr = obj["mattes"].toArray();
    impl_->maskMatteState_.clearMatteReferences();
    for (const auto &matteVal : mattesArr) {
      if (matteVal.isObject()) {
        LayerMatteReference matte;
        matte.fromJson(matteVal.toObject());
        impl_->maskMatteState_.addMatteReference(matte);
      }
    }
  }

  if (obj.contains("parentId")) {
    const QString parentId = obj["parentId"].toString();
    if (parentId.isEmpty())
      clearParent();
    else
      setParentById(LayerID(parentId));
  }

  if (obj.contains("transform") && obj["transform"].isObject()) {
    restoreLayerTransform(obj["transform"].toObject(), transform3D(),
                          effectiveLayerFrameRate(this));
  }

  if (obj.contains("modifiers") && obj["modifiers"].isArray()) {
      impl_->modifiers_.clear();
      QJsonArray arr = obj["modifiers"].toArray();
      for (const auto& mv : arr) {
          if (!mv.isObject()) {
              continue;
          }
          auto modifier = deserializeLayerModifier(mv.toObject());
          if (modifier) {
              impl_->modifiers_.add(std::move(modifier));
          }
      }
  }

  const auto finiteClamped = [](double value, double fallback,
                                double minimum, double maximum) {
      return std::isfinite(value)
          ? std::clamp(value, minimum, maximum)
          : fallback;
  };
  if (obj.contains("physics") && obj["physics"].isObject()) {
      const QJsonObject physicsObj = obj["physics"].toObject();
      impl_->physicsComponent_.settings().fromJson(physicsObj);
      impl_->clonePhysicsInitialVelocityY_ = static_cast<float>(finiteClamped(
          physicsObj.value(QStringLiteral("cloneInitialVelocityY"))
              .toDouble(impl_->clonePhysicsInitialVelocityY_),
          impl_->clonePhysicsInitialVelocityY_, -5000.0, 5000.0));
      impl_->clonePhysicsMaxBounces_ = std::clamp(
          physicsObj.value(QStringLiteral("cloneMaxBounces"))
              .toInt(impl_->clonePhysicsMaxBounces_), 0, 32);
      impl_->physicsComponent_.reset();
      // Seed the property cache so clone physics timing reads the loaded
      // values even before the first Inspector rebuild.
      persistentLayerProperty(
          QStringLiteral("physics.initialVelocityY"), PropertyType::Float,
          QVariant(static_cast<double>(impl_->clonePhysicsInitialVelocityY_)),
          -92);
      persistentLayerProperty(QStringLiteral("physics.maxBounces"),
                              PropertyType::Integer,
                              QVariant(impl_->clonePhysicsMaxBounces_), -91);
  }
  if (obj.contains(QStringLiteral("clonePhysicsInitialVelocityY"))) {
    const double value = obj.value(QStringLiteral("clonePhysicsInitialVelocityY"))
                             .toDouble(impl_->clonePhysicsInitialVelocityY_);
    impl_->clonePhysicsInitialVelocityY_ = static_cast<float>(
        std::isfinite(value)
            ? std::clamp(value, -5000.0, 5000.0)
            : impl_->clonePhysicsInitialVelocityY_);
    persistentLayerProperty(
        QStringLiteral("physics.initialVelocityY"), PropertyType::Float,
        QVariant(static_cast<double>(impl_->clonePhysicsInitialVelocityY_)),
        -92);
  }
  if (obj.contains(QStringLiteral("clonePhysicsMaxBounces"))) {
    impl_->clonePhysicsMaxBounces_ = std::clamp(
        obj.value(QStringLiteral("clonePhysicsMaxBounces"))
            .toInt(impl_->clonePhysicsMaxBounces_), 0, 32);
    persistentLayerProperty(QStringLiteral("physics.maxBounces"),
                            PropertyType::Integer,
                            QVariant(impl_->clonePhysicsMaxBounces_), -91);
  }
  if (obj.contains("softBodyPhysicsEnabled") &&
      obj["softBodyPhysicsEnabled"].toBool(false)) {
      enableSoftBodyPhysicsGrid();
  }
  if (obj.contains("cloth3DPhysicsEnabled") &&
      obj["cloth3DPhysicsEnabled"].toBool(false)) {
      enableCloth3DPhysicsGrid();
  }
  if (obj.contains("materialPhysicsEnabled") &&
      obj["materialPhysicsEnabled"].toBool(false)) {
      enableMaterialPhysics(obj["materialPhysicsPreset"].toInt(0));
  }
  if (obj.contains("motion") && obj["motion"].isObject()) {
      const QJsonObject motionObj = obj["motion"].toObject();
      impl_->motionDynamicsEnabled_ = motionObj.value(QStringLiteral("enabled")).toBool(false);
      impl_->motionDynamicsMode_ = std::clamp(
          motionObj.value(QStringLiteral("mode")).toInt(0), 0, 2);
      impl_->motionDynamicsStiffness_ = static_cast<float>(
          finiteClamped(motionObj.value(QStringLiteral("stiffness")).toDouble(80.0),
                        80.0, 0.0, 1000.0));
      impl_->motionDynamicsDamping_ = static_cast<float>(
          finiteClamped(motionObj.value(QStringLiteral("damping")).toDouble(16.0),
                        16.0, 0.0, 100.0));
      impl_->motionDynamicsMass_ = static_cast<float>(
          finiteClamped(motionObj.value(QStringLiteral("mass")).toDouble(1.0),
                        1.0, 0.1, 100.0));
      impl_->motionDynamicsLagTau_ = static_cast<float>(
          finiteClamped(motionObj.value(QStringLiteral("lagTau")).toDouble(0.1),
                        0.1, 0.001, 10.0));
      impl_->motionDynamicsClampOvershoot_ =
          motionObj.value(QStringLiteral("clampOvershoot")).toBool(false);
      impl_->motionDynamicsOvershootLimit_ = static_cast<float>(
          finiteClamped(motionObj.value(QStringLiteral("overshootLimit")).toDouble(0.3),
                        0.3, 0.0, 2.0));
      impl_->motionLastFrame_ = std::numeric_limits<int64_t>::min();
  }
  if (obj.contains("fracture") && obj["fracture"].isObject()) {
      const QJsonObject fractureObj = obj["fracture"].toObject();
      impl_->fractureEnabled_ = fractureObj.value(QStringLiteral("enabled")).toBool(false);
      impl_->fracturePreset_ = std::clamp(fractureObj.value(QStringLiteral("preset")).toInt(static_cast<int>(FracturePreset::Glass)), 0, static_cast<int>(FracturePreset::Dust));
      impl_->fractureCrackThreshold_ = static_cast<float>(
          finiteClamped(fractureObj.value(QStringLiteral("crackThreshold")).toDouble(1.0), 1.0, 0.0, 1000.0));
      impl_->fractureShatterThreshold_ = static_cast<float>(
          finiteClamped(fractureObj.value(QStringLiteral("shatterThreshold")).toDouble(2.5), 2.5, 0.0, 1000.0));
      impl_->fractureShardCount_ = std::clamp(
          fractureObj.value(QStringLiteral("shardCount")).toInt(16), 1, 256);
      impl_->fractureShardDamping_ = static_cast<float>(
          finiteClamped(fractureObj.value(QStringLiteral("shardDamping")).toDouble(0.92), 0.92, 0.0, 1.0));
      impl_->fractureShardGravity_ = static_cast<float>(
          finiteClamped(fractureObj.value(QStringLiteral("shardGravity")).toDouble(0.0), 0.0, -5000.0, 5000.0));
      impl_->fractureImpactSensitivity_ = static_cast<float>(
          finiteClamped(fractureObj.value(QStringLiteral("impactSensitivity")).toDouble(1.0), 1.0, 0.0, 10.0));
      impl_->fracturePreGenerate_ =
          fractureObj.value(QStringLiteral("preGenerate")).toBool(false);
      impl_->fractureTriggerFrame_ = fractureObj.contains(
          QStringLiteral("triggerFrame"))
          ? static_cast<int64_t>(fractureObj.value(
                QStringLiteral("triggerFrame")).toVariant().toLongLong())
          : -1;
      // Runtime fracture state is reconstructed from authoring settings and
      // timeline; it is intentionally not restored from project data.
      resetFractureState();
  }
  if (obj.contains("trail") && obj["trail"].isObject()) {
      const QJsonObject trailObj = obj["trail"].toObject();
      impl_->motionTrailEnabled_ = trailObj.value(QStringLiteral("enabled")).toBool(false);
      impl_->motionTrailLength_ = std::clamp(trailObj.value(QStringLiteral("length")).toInt(24), 2, 256);
      impl_->motionTrailFade_ = static_cast<float>(
          finiteClamped(trailObj.value(QStringLiteral("fade")).toDouble(0.72), 0.72, 0.0, 1.0));
      impl_->motionTrailWidth_ = static_cast<float>(
          finiteClamped(trailObj.value(QStringLiteral("width")).toDouble(2.0), 2.0, 0.1, 128.0));
      impl_->motionTrailHistory_.clear();
      impl_->motionTrailLastFrame_ = std::numeric_limits<int64_t>::min();
  }
  if (obj.contains("fragmentAppearance") &&
      obj["fragmentAppearance"].isObject()) {
      const QJsonObject appearanceObj = obj["fragmentAppearance"].toObject();
      impl_->fragmentVelocityStretchEnabled_ =
          appearanceObj.value(QStringLiteral("velocityStretchEnabled")).toBool(false);
      impl_->fragmentVelocityStretchStrength_ = static_cast<float>(
          finiteClamped(appearanceObj.value(QStringLiteral("velocityStretchStrength")).toDouble(0.01),
                        0.01, 0.0, 1.0));
      impl_->fragmentVelocityStretchMax_ = static_cast<float>(
          finiteClamped(appearanceObj.value(QStringLiteral("velocityStretchMax")).toDouble(3.0),
                        3.0, 1.0, 32.0));
      impl_->fragmentColorVariationEnabled_ =
          appearanceObj.value(QStringLiteral("colorVariationEnabled")).toBool(false);
      impl_->fragmentColorVariation_ = static_cast<float>(
          finiteClamped(appearanceObj.value(QStringLiteral("colorVariation")).toDouble(0.35),
                        0.35, 0.0, 1.0));
      impl_->fragmentClonerOutputEnabled_ = appearanceObj.value(
          QStringLiteral("clonerOutputEnabled")).toBool(false);
      impl_->fragmentClonerOutputCount_ = std::clamp(appearanceObj.value(
          QStringLiteral("clonerOutputCount")).toInt(1), 1, 256);
      impl_->fragmentClonerOutputSpacingX_ = static_cast<float>(
          finiteClamped(appearanceObj.value(QStringLiteral("clonerOutputSpacingX")).toDouble(24.0),
                        24.0, -100000.0, 100000.0));
      impl_->fragmentClonerOutputSpacingY_ = static_cast<float>(
          finiteClamped(appearanceObj.value(QStringLiteral("clonerOutputSpacingY")).toDouble(0.0),
                        0.0, -100000.0, 100000.0));
      impl_->fragmentClonerOutputTimeOffsetFrames_ = static_cast<float>(
          finiteClamped(appearanceObj.value(QStringLiteral("clonerOutputTimeOffsetFrames")).toDouble(0.0),
                        0.0, -10000.0, 10000.0));
  }
  // A reused layer instance must not retain component activation from the
  // previously restored JSON when the new snapshot has no component block.
  if (!obj.contains("components") || !obj["components"].isObject()) {
      impl_->scriptComponentEnabled_ = false;
      impl_->clonerComponentEnabled_ = false;
      impl_->layoutComponentEnabled_ = false;
      impl_->collisionComponentEnabled_ = false;
      impl_->jointComponentEnabled_ = false;
      impl_->jointBroken_ = false;
      impl_->crowdComponentEnabled_ = false;
      impl_->particleEmitterComponentEnabled_ = false;
      impl_->fluidComponentEnabled_ = false;
      impl_->extraCloneModifierDescriptors_.clear();
      impl_->scriptBinding_ = {};
  }
  if (obj.contains("components") && obj["components"].isObject()) {
      const QJsonObject componentsObj = obj["components"].toObject();
        impl_->scriptComponentEnabled_ =
            componentsObj.value(QStringLiteral("scriptEnabled")).toBool(false);
        impl_->clonerComponentEnabled_ =
            componentsObj.value(QStringLiteral("clonerEnabled")).toBool(false);
        impl_->layoutComponentEnabled_ =
            componentsObj.value(QStringLiteral("layoutEnabled")).toBool(false);
        impl_->collisionComponentEnabled_ =
            componentsObj.value(QStringLiteral("collisionEnabled")).toBool(false);
        impl_->collisionBodyType_ = std::clamp(
            componentsObj.value(QStringLiteral("collisionBodyType")).toInt(0), 0, 2);
        impl_->collisionShape_ = std::clamp(
            componentsObj.value(QStringLiteral("collisionShape")).toInt(0), 0,
            3);
        impl_->collisionWidth_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("collisionWidth")).toDouble(0.0),
            0.0, 0.0, 100000.0));
        impl_->collisionHeight_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("collisionHeight")).toDouble(0.0),
            0.0, 0.0, 100000.0));
        impl_->collisionRadius_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("collisionRadius")).toDouble(0.0),
            0.0, 0.0, 100000.0));
        impl_->collisionOffsetX_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("collisionOffsetX")).toDouble(0.0),
            0.0, -100000.0, 100000.0));
        impl_->collisionOffsetY_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("collisionOffsetY")).toDouble(0.0),
            0.0, -100000.0, 100000.0));
        impl_->collisionFloorY_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("collisionFloorY")).toDouble(0.0),
            0.0, 0.0, 100000.0));
        impl_->collisionCompositionBounds_ = componentsObj.value(
            QStringLiteral("collisionCompositionBounds")).toBool(false);
        const QColor collisionDisplayColor(
            componentsObj.value(QStringLiteral("collisionDisplayColor"))
                .toString());
        if (collisionDisplayColor.isValid()) {
          impl_->collisionDisplayColor_ = collisionDisplayColor;
        }
        impl_->jointComponentEnabled_ = componentsObj.value(
            QStringLiteral("jointEnabled")).toBool(false);
        impl_->jointType_ = std::clamp(
            componentsObj.value(QStringLiteral("jointType")).toInt(0), 0, 5);
        impl_->jointTargetLayerName_ = componentsObj.value(
            QStringLiteral("jointTargetLayer")).toString();
        impl_->jointTargetAnchorY_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointTargetAnchorY")).toDouble(0.0), 0.0, -100000.0, 100000.0));
        impl_->jointTargetAnchorX_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointTargetAnchorX")).toDouble(0.0), 0.0, -100000.0, 100000.0));
        impl_->jointOwnerAnchorY_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointOwnerAnchorY")).toDouble(0.0), 0.0, -100000.0, 100000.0));
        impl_->jointOwnerAnchorX_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointOwnerAnchorX")).toDouble(0.0), 0.0, -100000.0, 100000.0));
        impl_->jointLength_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointLength")).toDouble(0.0),
            0.0, 0.0, 100000.0));
        impl_->jointStiffness_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointStiffness")).toDouble(5.0),
            5.0, 0.0, 120.0));
        impl_->jointDamping_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointDamping")).toDouble(0.7),
            0.7, 0.0, 10.0));
        impl_->jointAngleLimitEnabled_ = componentsObj.value(
            QStringLiteral("jointAngleLimitEnabled")).toBool(false);
        impl_->jointLowerAngle_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointLowerAngle")).toDouble(-45.0),
            -45.0, -360.0, 360.0));
        impl_->jointUpperAngle_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointUpperAngle")).toDouble(45.0),
            45.0, -360.0, 360.0));
        impl_->jointAxisX_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointAxisX")).toDouble(1.0), 1.0, -1.0, 1.0));
        impl_->jointAxisY_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointAxisY")).toDouble(0.0), 0.0, -1.0, 1.0));
        impl_->jointLinearLimitEnabled_ = componentsObj.value(
            QStringLiteral("jointLinearLimitEnabled")).toBool(false);
        impl_->jointLowerLimit_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointLowerLimit")).toDouble(-100.0), -100.0, -100000.0, 100000.0));
        impl_->jointUpperLimit_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointUpperLimit")).toDouble(100.0), 100.0, -100000.0, 100000.0));
        impl_->jointMotorEnabled_ = componentsObj.value(
            QStringLiteral("jointMotorEnabled")).toBool(false);
        impl_->jointMotorSpeed_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointMotorSpeed")).toDouble(0.0), 0.0, -100000.0, 100000.0));
        impl_->jointMotorForce_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointMotorForce")).toDouble(100.0), 100.0, 0.0, 1000000.0));
        impl_->jointBreakForce_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("jointBreakForce")).toDouble(0.0), 0.0, 0.0, 1000000.0));
        impl_->jointBroken_ = false;
        impl_->crowdComponentEnabled_ =
            componentsObj.value(QStringLiteral("crowdEnabled")).toBool(false);
        impl_->crowdCohesion_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("crowdCohesion")).toDouble(0.5),
            0.5, 0.0, 10.0));
        impl_->crowdSeparation_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("crowdSeparation")).toDouble(0.5),
            0.5, 0.0, 10.0));
        impl_->crowdAlignment_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("crowdAlignment")).toDouble(0.5),
            0.5, 0.0, 10.0));
        impl_->crowdMaxSpeed_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("crowdMaxSpeed")).toDouble(120.0),
            120.0, 0.0, 10000.0));
        impl_->crowdJitter_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("crowdJitter")).toDouble(0.1),
            0.1, 0.0, 10.0));
        impl_->particleEmitterComponentEnabled_ =
            componentsObj.value(QStringLiteral("particleEmitterEnabled"))
                .toBool(false);
        impl_->particleEmitterCount_ = std::clamp(
            componentsObj.value(QStringLiteral("particleEmitterCount")).toInt(16),
            0, 100000);
        impl_->particleEmitterSpeed_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("particleEmitterSpeed"))
                .toDouble(120.0),
            120.0, 0.0, 100000.0));
        impl_->particleEmitterLifetime_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("particleEmitterLifetime"))
                .toDouble(1.0),
            1.0, 0.01, 3600.0));
        impl_->fluidComponentEnabled_ =
            componentsObj.value(QStringLiteral("fluidEnabled")).toBool(false);
        if (impl_->fluidComponentEnabled_ &&
            impl_->physicsComponent_.authoring().solverKind ==
                PhysicsSolverKind::Disabled) {
            impl_->physicsComponent_.authoring().solverKind =
                PhysicsSolverKind::Fluid2D;
        }
        impl_->fluidMode_ = std::clamp(
            componentsObj.value(QStringLiteral("fluidMode")).toInt(0), 0, 1);
        impl_->fluidGridWidth_ = std::clamp(
            componentsObj.value(QStringLiteral("fluidGridWidth")).toInt(128),
            8, 4096);
        impl_->fluidGridHeight_ = std::clamp(
            componentsObj.value(QStringLiteral("fluidGridHeight")).toInt(128),
            8, 4096);
        impl_->fluidViscosity_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("fluidViscosity"))
                .toDouble(0.00001),
            0.00001, 0.0, 1.0));
        impl_->fluidDiffusion_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("fluidDiffusion"))
                .toDouble(0.00001),
            0.00001, 0.0, 1.0));
        impl_->fluidBuoyancy_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("fluidBuoyancy"))
                .toDouble(0.05),
            0.05, -2.0, 2.0));
        impl_->fluidVorticity_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("fluidVorticity"))
                .toDouble(0.1),
            0.1, 0.0, 10.0));
        impl_->fluidSolverIterations_ = std::clamp(
            componentsObj.value(QStringLiteral("fluidSolverIterations")).toInt(20),
            1, 256);
        impl_->liquidFillAmount_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("liquidFillAmount")).toDouble(0.55),
            0.55, 0.0, 1.0));
        impl_->liquidInflowRate_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("liquidInflowRate")).toDouble(0.0),
            0.0, 0.0, 10000.0));
        impl_->liquidInflowWidth_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("liquidInflowWidth")).toDouble(0.25),
            0.25, 0.0, 1.0));
        impl_->liquidInflowSpeed_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("liquidInflowSpeed")).toDouble(0.8),
            0.8, 0.0, 20.0));
        impl_->liquidInflowPosition_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("liquidInflowPosition"))
                .toDouble(0.5),
            0.5, 0.0, 1.0));
        impl_->liquidOpeningEdge_ = std::clamp(
            componentsObj.value(QStringLiteral("liquidOpeningEdge")).toInt(-1),
            -1, 511);
        impl_->liquidSpillCullMargin_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("liquidSpillCullMargin"))
                .toDouble(1024.0),
            1024.0, 0.0, 1000000.0));
        impl_->liquidGravity_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("liquidGravity")).toDouble(1.8),
            1.8, 0.0, 20.0));
        impl_->liquidSurfaceTension_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("liquidSurfaceTension")).toDouble(0.15),
            0.15, 0.0, 1.0));
        impl_->liquidParticleSpacing_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("liquidParticleSpacing")).toDouble(0.055),
            0.055, 0.025, 0.2));
        impl_->liquidSubsteps_ = std::clamp(
            componentsObj.value(QStringLiteral("liquidSubsteps")).toInt(3),
            1, 8);
        impl_->liquidSurfaceOpacity_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("liquidSurfaceOpacity"))
                .toDouble(0.64),
            0.64, 0.0, 1.0));
        impl_->liquidEdgeOpacity_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("liquidEdgeOpacity"))
                .toDouble(0.42),
            0.42, 0.0, 1.0));
        impl_->liquidFoamAmount_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("liquidFoamAmount"))
                .toDouble(1.0),
            1.0, 0.0, 1.0));
        impl_->liquidContainerOpacity_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("liquidContainerOpacity"))
                .toDouble(0.58),
            0.58, 0.0, 1.0));
        impl_->liquidContainerWidth_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("liquidContainerWidth"))
                .toDouble(2.5),
            2.5, 0.0, 64.0));
        const auto restoreLiquidColor = [&](const QString& key,
                                            const FloatColor& fallback) {
          const QJsonObject color =
              componentsObj.value(key).toObject();
          return FloatColor(
              static_cast<float>(finiteClamped(
                  color.value(QStringLiteral("r")).toDouble(fallback.r()),
                  fallback.r(), 0.0, 1.0)),
              static_cast<float>(finiteClamped(
                  color.value(QStringLiteral("g")).toDouble(fallback.g()),
                  fallback.g(), 0.0, 1.0)),
              static_cast<float>(finiteClamped(
                  color.value(QStringLiteral("b")).toDouble(fallback.b()),
                  fallback.b(), 0.0, 1.0)),
              static_cast<float>(finiteClamped(
                  color.value(QStringLiteral("a")).toDouble(fallback.a()),
                  fallback.a(), 0.0, 1.0)));
        };
        impl_->liquidColor_ = restoreLiquidColor(
            QStringLiteral("liquidColor"),
            FloatColor(0.10f, 0.50f, 0.98f, 1.0f));
        impl_->liquidFoamColor_ = restoreLiquidColor(
            QStringLiteral("liquidFoamColor"),
            FloatColor(0.78f, 0.93f, 1.0f, 1.0f));
        impl_->fluidRuntime_.invalidateLiquidSimulation();
        impl_->layoutMode_ = std::clamp(
            componentsObj.value(QStringLiteral("layoutMode")).toInt(0), 0, 2);
        impl_->layoutAnchorMode_ = std::clamp(
            componentsObj.value(QStringLiteral("layoutAnchorMode")).toInt(0),
            0, 2);
        impl_->layoutHorizontalPin_ =
            componentsObj.value(QStringLiteral("layoutHorizontalPin")).toInt(0);
        impl_->layoutVerticalPin_ =
            componentsObj.value(QStringLiteral("layoutVerticalPin")).toInt(0);
        impl_->layoutScaleMode_ =
            componentsObj.value(QStringLiteral("layoutScaleMode")).toInt(0);
        impl_->layoutResponsiveEnabled_ = componentsObj.value(
            QStringLiteral("layoutResponsiveEnabled")).toBool(false);
        impl_->layoutResponsiveOffsetX_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("layoutResponsiveOffsetX")).toDouble(0.0),
            0.0, -100000.0, 100000.0));
        impl_->layoutResponsiveOffsetY_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("layoutResponsiveOffsetY")).toDouble(0.0),
            0.0, -100000.0, 100000.0));
        impl_->layoutSafeAreaEnabled_ =
            componentsObj.value(QStringLiteral("layoutSafeAreaEnabled")).toBool(false);
        impl_->layoutSafeAreaPaddingX_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("layoutSafeAreaPaddingX")).toDouble(0.0),
            0.0, -100000.0, 100000.0));
        impl_->layoutSafeAreaPaddingY_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("layoutSafeAreaPaddingY")).toDouble(0.0),
            0.0, -100000.0, 100000.0));
        impl_->layoutStackDirection_ = std::clamp(
            componentsObj.value(QStringLiteral("layoutStackDirection")).toInt(0),
            0, 1);
        impl_->layoutGap_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("layoutGap")).toDouble(24.0),
            24.0, -100000.0, 100000.0));
        impl_->layoutMaxPerRow_ =
            std::max(0, componentsObj.value(QStringLiteral("layoutMaxPerRow")).toInt(0));
        impl_->clonerMode_ =
            componentsObj.value(QStringLiteral("clonerMode")).toInt(0);
        impl_->clonerCloneCount_ = std::clamp(
            componentsObj.value(QStringLiteral("clonerCloneCount")).toInt(3),
            1, 256);
        impl_->clonerOffsetX_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("clonerOffsetX")).toDouble(160.0),
            160.0, -100000.0, 100000.0));
        impl_->clonerOffsetY_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("clonerOffsetY")).toDouble(48.0),
            48.0, -100000.0, 100000.0));
        impl_->clonerOffsetZ_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("clonerOffsetZ")).toDouble(0.0),
            0.0, -100000.0, 100000.0));
        impl_->clonerJitterX_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("clonerJitterX")).toDouble(0.0),
            0.0, -100000.0, 100000.0));
        impl_->clonerJitterY_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("clonerJitterY")).toDouble(0.0),
            0.0, -100000.0, 100000.0));
        impl_->clonerJitterZ_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("clonerJitterZ")).toDouble(0.0),
            0.0, -100000.0, 100000.0));
        impl_->clonerSeed_ =
            componentsObj.value(QStringLiteral("clonerSeed")).toInt(0);
        impl_->clonerColumns_ = std::max(
            1, componentsObj.value(QStringLiteral("clonerColumns")).toInt(3));
        impl_->clonerRows_ = std::max(
            1, componentsObj.value(QStringLiteral("clonerRows")).toInt(3));
        impl_->clonerDepth_ = std::max(
            1, componentsObj.value(QStringLiteral("clonerDepth")).toInt(1));
        impl_->clonerSpacingX_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("clonerSpacingX")).toDouble(160.0),
            160.0, -100000.0, 100000.0));
        impl_->clonerSpacingY_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("clonerSpacingY")).toDouble(48.0),
            48.0, -100000.0, 100000.0));
        impl_->clonerSpacingZ_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("clonerSpacingZ")).toDouble(0.0),
            0.0, -100000.0, 100000.0));
        impl_->clonerRadialCount_ = std::max(
            1, componentsObj.value(QStringLiteral("clonerRadialCount")).toInt(8));
        impl_->clonerRadius_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("clonerRadius")).toDouble(160.0),
            160.0, -100000.0, 100000.0));
        impl_->clonerStartAngle_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("clonerStartAngle")).toDouble(0.0),
            0.0, -360000.0, 360000.0));
        impl_->clonerEndAngle_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("clonerEndAngle")).toDouble(360.0),
            360.0, -360000.0, 360000.0));
        impl_->clonerRotationStep_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("clonerRotationStep")).toDouble(0.0),
            0.0, -360000.0, 360000.0));
        impl_->clonerOpacityDecay_ = static_cast<float>(finiteClamped(
            componentsObj.value(QStringLiteral("clonerOpacityDecay")).toDouble(0.0),
            0.0, 0.0, 1.0));
        impl_->clonerTransforms_.clear();
        if (componentsObj.contains(QStringLiteral("clonerTransforms")) &&
            componentsObj.value(QStringLiteral("clonerTransforms")).isArray()) {
          const QJsonArray transformArr =
              componentsObj.value(QStringLiteral("clonerTransforms")).toArray();
          impl_->clonerTransforms_.reserve(static_cast<size_t>(transformArr.size()));
          for (const auto &entry : transformArr) {
            if (!entry.isObject()) {
              continue;
            }
            const QJsonObject transformObj = entry.toObject();
            ClonerTransformOperation op;
            op.name = transformObj.value(QStringLiteral("name"))
                          .toString(QStringLiteral("Transform"));
            op.enabled =
                transformObj.value(QStringLiteral("enabled")).toBool(true);
            op.position.setX(static_cast<float>(finiteClamped(
                transformObj.value(QStringLiteral("positionX")).toDouble(0.0),
                0.0, -100000.0, 100000.0)));
            op.position.setY(static_cast<float>(finiteClamped(
                transformObj.value(QStringLiteral("positionY")).toDouble(0.0),
                0.0, -100000.0, 100000.0)));
            op.position.setZ(static_cast<float>(finiteClamped(
                transformObj.value(QStringLiteral("positionZ")).toDouble(0.0),
                0.0, -100000.0, 100000.0)));
            op.rotation.setX(static_cast<float>(finiteClamped(
                transformObj.value(QStringLiteral("rotationX")).toDouble(0.0),
                0.0, -360000.0, 360000.0)));
            op.rotation.setY(static_cast<float>(finiteClamped(
                transformObj.value(QStringLiteral("rotationY")).toDouble(0.0),
                0.0, -360000.0, 360000.0)));
            op.rotation.setZ(static_cast<float>(finiteClamped(
                transformObj.value(QStringLiteral("rotationZ")).toDouble(0.0),
                0.0, -360000.0, 360000.0)));
            op.scale.setX(static_cast<float>(finiteClamped(
                transformObj.value(QStringLiteral("scaleX")).toDouble(1.0),
                1.0, -100000.0, 100000.0)));
            op.scale.setY(static_cast<float>(finiteClamped(
                transformObj.value(QStringLiteral("scaleY")).toDouble(1.0),
                1.0, -100000.0, 100000.0)));
            op.scale.setZ(static_cast<float>(finiteClamped(
                transformObj.value(QStringLiteral("scaleZ")).toDouble(1.0),
                1.0, -100000.0, 100000.0)));
            impl_->clonerTransforms_.push_back(op);
          }
        } else {
          const bool hasLegacyTransform =
              componentsObj.contains(QStringLiteral("clonerTransformPositionX")) ||
              componentsObj.contains(QStringLiteral("clonerTransformPositionY")) ||
              componentsObj.contains(QStringLiteral("clonerTransformPositionZ")) ||
              componentsObj.contains(QStringLiteral("clonerTransformRotationX")) ||
              componentsObj.contains(QStringLiteral("clonerTransformRotationY")) ||
              componentsObj.contains(QStringLiteral("clonerTransformRotationZ")) ||
              componentsObj.contains(QStringLiteral("clonerTransformScaleX")) ||
              componentsObj.contains(QStringLiteral("clonerTransformScaleY")) ||
              componentsObj.contains(QStringLiteral("clonerTransformScaleZ"));
          if (hasLegacyTransform) {
            ClonerTransformOperation op;
            op.name = QStringLiteral("Transform 1");
            op.position.setX(static_cast<float>(finiteClamped(
                componentsObj.value(QStringLiteral("clonerTransformPositionX")).toDouble(0.0),
                0.0, -100000.0, 100000.0)));
            op.position.setY(static_cast<float>(finiteClamped(
                componentsObj.value(QStringLiteral("clonerTransformPositionY")).toDouble(0.0),
                0.0, -100000.0, 100000.0)));
            op.position.setZ(static_cast<float>(finiteClamped(
                componentsObj.value(QStringLiteral("clonerTransformPositionZ")).toDouble(0.0),
                0.0, -100000.0, 100000.0)));
            op.rotation.setX(static_cast<float>(finiteClamped(
                componentsObj.value(QStringLiteral("clonerTransformRotationX")).toDouble(0.0),
                0.0, -360000.0, 360000.0)));
            op.rotation.setY(static_cast<float>(finiteClamped(
                componentsObj.value(QStringLiteral("clonerTransformRotationY")).toDouble(0.0),
                0.0, -360000.0, 360000.0)));
            op.rotation.setZ(static_cast<float>(finiteClamped(
                componentsObj.value(QStringLiteral("clonerTransformRotationZ")).toDouble(0.0),
                0.0, -360000.0, 360000.0)));
            op.scale.setX(static_cast<float>(finiteClamped(
                componentsObj.value(QStringLiteral("clonerTransformScaleX")).toDouble(1.0),
                1.0, -100000.0, 100000.0)));
            op.scale.setY(static_cast<float>(finiteClamped(
                componentsObj.value(QStringLiteral("clonerTransformScaleY")).toDouble(1.0),
                1.0, -100000.0, 100000.0)));
            op.scale.setZ(static_cast<float>(finiteClamped(
                componentsObj.value(QStringLiteral("clonerTransformScaleZ")).toDouble(1.0),
                1.0, -100000.0, 100000.0)));
            impl_->clonerTransforms_.push_back(op);
          }
        }
        constexpr qsizetype kMaxExtraComponentDescriptors = 1024;
        impl_->extraGeneratorDescriptors_.clear();
        if (componentsObj.contains(QStringLiteral("generators")) &&
            componentsObj.value(QStringLiteral("generators")).isArray()) {
          const auto generatorsArr =
              componentsObj.value(QStringLiteral("generators")).toArray();
          impl_->extraGeneratorDescriptors_.reserve(
              static_cast<size_t>(std::min(
                  generatorsArr.size(), kMaxExtraComponentDescriptors)));
          qsizetype generatorCount = 0;
          for (const auto& generatorValue : generatorsArr) {
            if (generatorCount++ >= kMaxExtraComponentDescriptors) break;
            if (!generatorValue.isObject()) {
              continue;
            }
            const auto descriptor =
                layerGeneratorDescriptorFromJson(generatorValue.toObject());
            if (descriptor.has_value()) {
              impl_->extraGeneratorDescriptors_.add(*descriptor);
            }
          }
        }
        impl_->extraFieldDescriptors_.clear();
        if (componentsObj.contains(QStringLiteral("fields")) &&
            componentsObj.value(QStringLiteral("fields")).isArray()) {
          const auto fieldsArr =
              componentsObj.value(QStringLiteral("fields")).toArray();
          impl_->extraFieldDescriptors_.reserve(
              static_cast<size_t>(std::min(
                  fieldsArr.size(), kMaxExtraComponentDescriptors)));
          qsizetype fieldCount = 0;
          for (const auto& fieldValue : fieldsArr) {
            if (fieldCount++ >= kMaxExtraComponentDescriptors) break;
            if (!fieldValue.isObject()) {
              continue;
            }
            const auto descriptor =
                layerFieldDescriptorFromJson(fieldValue.toObject());
            if (descriptor.has_value()) {
              impl_->extraFieldDescriptors_.add(*descriptor);
            }
          }
        }
        impl_->extraCloneModifierDescriptors_.clear();
        if (componentsObj.contains(QStringLiteral("cloneModifiers")) &&
            componentsObj.value(QStringLiteral("cloneModifiers")).isArray()) {
          const auto modifiersArr =
              componentsObj.value(QStringLiteral("cloneModifiers")).toArray();
          impl_->extraCloneModifierDescriptors_.reserve(
              static_cast<size_t>(std::min(
                  modifiersArr.size(), kMaxExtraComponentDescriptors)));
          qsizetype modifierCount = 0;
          for (const auto& modifierValue : modifiersArr) {
            if (modifierCount++ >= kMaxExtraComponentDescriptors) break;
            if (!modifierValue.isObject()) {
              continue;
            }
            const auto descriptor =
                layerModifierDescriptorFromJson(modifierValue.toObject());
            if (descriptor.has_value()) {
              impl_->extraCloneModifierDescriptors_.add(*descriptor);
            }
          }
        }
        impl_->scriptBinding_ = componentsObj.value(QStringLiteral("scriptBinding")).toObject();
    }
  const bool hasComponentGraph =
      obj.contains(QStringLiteral("componentGraph")) &&
      obj.value(QStringLiteral("componentGraph")).isArray();
  if (hasComponentGraph) {
    impl_->componentHost_.fromJson(
        obj.value(QStringLiteral("componentGraph")).toArray());
    impl_->syncBuiltinBoolsFromHost();
    // New component-only documents may not carry the legacy top-level
    // `fracture` object. In that case restore fracture settings from the
    // descriptor graph; legacy documents keep their top-level values.
    if (!obj.contains(QStringLiteral("fracture"))) {
      if (const auto *fracture = impl_->componentHost_.findByType(
              QStringLiteral("artifact.component.fracture"))) {
        const auto &settings = fracture->settings;
        impl_->fracturePreset_ = std::clamp(
            settings.value(QStringLiteral("preset"))
                .toInt(impl_->fracturePreset_),
            0, static_cast<int>(FracturePreset::Dust));
        impl_->fractureShardCount_ = std::clamp(
            settings.value(QStringLiteral("shardCount"))
                .toInt(impl_->fractureShardCount_),
            1, 256);
        impl_->fractureCrackThreshold_ = static_cast<float>(finiteClamped(
            settings.value(QStringLiteral("crackThreshold"))
                .toDouble(impl_->fractureCrackThreshold_),
            impl_->fractureCrackThreshold_, 0.0, 1000.0));
        impl_->fractureShatterThreshold_ = static_cast<float>(finiteClamped(
            settings.value(QStringLiteral("shatterThreshold"))
                .toDouble(impl_->fractureShatterThreshold_),
            impl_->fractureShatterThreshold_, 0.0, 1000.0));
        impl_->fractureShardDamping_ = static_cast<float>(finiteClamped(
            settings.value(QStringLiteral("shardDamping"))
                .toDouble(impl_->fractureShardDamping_),
            impl_->fractureShardDamping_, 0.0, 1.0));
        impl_->fractureShardGravity_ = static_cast<float>(finiteClamped(
            settings.value(QStringLiteral("shardGravity"))
                .toDouble(impl_->fractureShardGravity_),
            impl_->fractureShardGravity_, -5000.0, 5000.0));
        impl_->fractureImpactSensitivity_ = static_cast<float>(finiteClamped(
            settings.value(QStringLiteral("impactSensitivity"))
                .toDouble(impl_->fractureImpactSensitivity_),
            impl_->fractureImpactSensitivity_, 0.0, 10.0));
        impl_->fracturePreGenerate_ =
            settings.value(QStringLiteral("preGenerate"))
                .toBool(impl_->fracturePreGenerate_);
        if (settings.contains(QStringLiteral("triggerFrame"))) {
          impl_->fractureTriggerFrame_ =
              settings.value(QStringLiteral("triggerFrame"))
                  .toVariant()
                  .toLongLong();
        }
      }
    }
  } else {
    impl_->componentHost_.fromJson(QJsonArray{});
    impl_->syncBuiltinComponentDescriptors();
  }

  if (obj.contains("variants") && obj["variants"].isArray()) {
      impl_->variants_.clear();
      QJsonArray arr = obj["variants"].toArray();
      for (int i = 0; i < arr.size(); ++i) {
          QJsonObject varObj = arr[i].toObject();
          auto newVariant = std::make_unique<LayerVariant>(this, varObj["name"].toString("A").toStdString());
          newVariant->overrideFlags_ = static_cast<VariantOverrideFlags>(varObj["flags"].toInt(0));

          if (varObj.contains("opacity")) {
              const double opacity = varObj["opacity"].toDouble(1.0);
              newVariant->opacityOverride = std::isfinite(opacity)
                  ? static_cast<float>(std::clamp(opacity, 0.0, 1.0))
                  : 1.0f;
          }
          if (varObj.contains("blendMode")) {
              newVariant->blendModeOverride = static_cast<LAYER_BLEND_TYPE>(std::clamp(
                  varObj["blendMode"].toInt(), 0,
                  static_cast<int>(LAYER_BLEND_TYPE::BLEND_SILHOUETTE_LUMA)));
          }
          if (varObj.contains("transform") && varObj["transform"].isObject()) {
              QJsonObject vtrans = varObj["transform"].toObject();
              AnimatableTransform3D vt3;
              const auto finiteVariantTransformValue = [](
                  double value, double fallback) {
                  return std::isfinite(value) ? value : fallback;
              };
              RationalTime t0(0, 1);
              if (vtrans.contains("px")) {
                  vt3.setPosition(
                      t0,
                      finiteVariantTransformValue(vtrans["px"].toDouble(), 0.0),
                      finiteVariantTransformValue(vtrans["py"].toDouble(0.0), 0.0));
              }
              if (vtrans.contains("pz")) {
                  vt3.setPositionZ(
                      t0, finiteVariantTransformValue(vtrans["pz"].toDouble(), 0.0));
              }
              const bool hasVariantRotationAxes =
                  vtrans.contains("rotationX") || vtrans.contains("rotationY") ||
                  vtrans.contains("rotationZ");
              if (hasVariantRotationAxes) {
                  vt3.setRotationX(t0, static_cast<float>(finiteVariantTransformValue(
                      vtrans["rotationX"].toDouble(0.0), 0.0)));
                  vt3.setRotationY(t0, static_cast<float>(finiteVariantTransformValue(
                      vtrans["rotationY"].toDouble(0.0), 0.0)));
                  vt3.setRotationZ(t0, static_cast<float>(finiteVariantTransformValue(
                      vtrans["rotationZ"].toDouble(0.0), 0.0)));
              } else if (vtrans.contains("rx")) {
                  vt3.setRotationZ(t0, static_cast<float>(finiteVariantTransformValue(
                      vtrans["rx"].toDouble(), 0.0)));
              }
              if (vtrans.contains("sx")) {
                  vt3.setScale(
                      t0,
                      finiteVariantTransformValue(vtrans["sx"].toDouble(1.0), 1.0),
                      finiteVariantTransformValue(vtrans["sy"].toDouble(1.0), 1.0));
              }
              if (vtrans.contains("ax")) {
                  vt3.setAnchor(
                      t0,
                      finiteVariantTransformValue(vtrans["ax"].toDouble(), 0.0),
                      finiteVariantTransformValue(vtrans["ay"].toDouble(0.0), 0.0),
                      finiteVariantTransformValue(vtrans["az"].toDouble(0.0), 0.0));
              }
              newVariant->transform3DOverride = vt3;
          }

          impl_->variants_.push_back(std::move(newVariant));
      }
  }
  if (impl_->variants_.empty()) {
      impl_->variants_.push_back(std::make_unique<LayerVariant>(this, "A"));
  }

  if (obj.contains("activeVariantIndex")) {
      impl_->activeVariantIndex_ = obj["activeVariantIndex"].toInt(0);
      if (impl_->activeVariantIndex_ >= impl_->variants_.size()) {
          impl_->activeVariantIndex_ = 0;
      }
  }

  if (obj.contains("masks") && obj["masks"].isArray()) {
    impl_->maskMatteState_.setMasks(deserializeLayerMasks(obj["masks"].toArray()));
    changed();
  }

  applyPropertiesFromJson(obj);
}

} // namespace Artifact
