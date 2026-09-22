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

namespace {
ArtifactLayerJsonFactory g_layerJsonFactory = nullptr;
std::mutex g_layerJsonFactoryMutex;

}

void setArtifactLayerJsonFactory(ArtifactLayerJsonFactory factory) {
  std::lock_guard lock(g_layerJsonFactoryMutex);
  g_layerJsonFactory = factory;
}

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

void notifyLayerMutation(ArtifactAbstractLayer *layer, LayerDirtyFlag flag,
                         LayerDirtyReason reason) {
  if (!layer) {
    return;
  }
  layer->setDirty(flag);
  layer->addDirtyReason(reason);
  layer->changed();
}

} // namespace


class ArtifactAbstractLayer::Impl {
public:
   bool is3D_ = false;
   bool projectionEnabled_ = false;
   QString projectionSourceLayerId_;
   bool twoPointFiveDEnabled_ = false;
  float twoPointFiveDDepth_ = 0.0f;
  float twoPointFiveDCameraDistance_ = 1000.0f;
  bool twoPointFiveDDepthOfFieldEnabled_ = false;
  float twoPointFiveDFocusDepth_ = 0.0f;
  float twoPointFiveDFocusRange_ = 250.0f;
  float twoPointFiveDMaxBlur_ = 12.0f;
  bool twoPointFiveDMotionBlurEnabled_ = false;
  float twoPointFiveDMotionBlurShutterAngle_ = 180.0f;
  int twoPointFiveDMotionBlurSamples_ = 4;
  bool isVisible_ = true;
  Id id;
  QString name_;
  QString layerNote_;
  QPointer<QObject> composition_;
  mutable std::mutex compositionMutex_;
  LayerID parentLayerId_;
  LAYER_BLEND_TYPE blendMode_ = LAYER_BLEND_TYPE::BLEND_NORMAL;
  LayerState state_;
  FramePosition inPoint_ = FramePosition(0);
  FramePosition outPoint_ = FramePosition(300); // Default 10s at 30fps
  FramePosition startTime_ = FramePosition(0);
  int64_t currentFrame_ = 0; // 現在のフレーム位置
  Audio::Modulation::ModulationRouter modulationRouter_;
  mutable QImage thumbnailCache_;
  mutable QSize thumbnailCacheSize_;
  int64_t currentFrame() const { return currentFrame_; }

  bool isLocked_ = false;
  bool isSelectionLocked_ = false;
  bool isTransformLocked_ = false;
  bool isTimingLocked_ = false;
  bool isGuide_ = false;
  bool isSolo_ = false;
  bool isShy_ = false;
  bool isAdjustmentLayer_ = false;
  LayerCachePolicy layerCachePolicy_ = LayerCachePolicy::Default;
  int labelColorIndex_ = 0;
  float opacity_ = 1.0f; // Opacity (0.0 - 1.0)
  LayerEffectEnvelope effectEnvelope_;

    // Physics component
    PhysicsLayerComponent physicsComponent_;
    float clonePhysicsInitialVelocityY_ = 0.0f;
    int clonePhysicsMaxBounces_ = 4;
    bool softBodyPhysicsEnabled_ = false;
    bool cloth3DPhysicsEnabled_ = false;
    bool materialPhysicsEnabled_ = false;
    int materialPhysicsPreset_ = 0;
    bool motionDynamicsEnabled_ = false;
    int motionDynamicsMode_ = 0;
    float motionDynamicsStiffness_ = 80.0f;
    float motionDynamicsDamping_ = 16.0f;
    float motionDynamicsMass_ = 1.0f;
    float motionDynamicsLagTau_ = 0.1f;
    bool motionDynamicsClampOvershoot_ = false;
    float motionDynamicsOvershootLimit_ = 0.3f;
    bool fractureEnabled_ = false;
    int fracturePreset_ = static_cast<int>(FracturePreset::Glass);
    float fractureCrackThreshold_ = 1.0f;
    float fractureShatterThreshold_ = 2.5f;
    int fractureShardCount_ = 16;
    float fractureShardDamping_ = 0.92f;
    float fractureShardGravity_ = 0.0f;
    float fractureImpactSensitivity_ = 1.0f;
    bool fracturePreGenerate_ = false;
    int64_t fractureTriggerFrame_ = -1;
    int64_t fractureTriggerLastFrame_ = std::numeric_limits<int64_t>::min();
    bool motionTrailEnabled_ = false;
    int motionTrailLength_ = 24;
    float motionTrailFade_ = 0.72f;
    float motionTrailWidth_ = 2.0f;
    QHash<QString, MotionTrailRingBuffer> motionTrailHistory_;
    int64_t motionTrailLastFrame_ = std::numeric_limits<int64_t>::min();
    bool fragmentVelocityStretchEnabled_ = false;
    float fragmentVelocityStretchStrength_ = 0.01f;
    float fragmentVelocityStretchMax_ = 3.0f;
    bool fragmentColorVariationEnabled_ = false;
    float fragmentColorVariation_ = 0.35f;
    bool fragmentClonerOutputEnabled_ = false;
    int fragmentClonerOutputCount_ = 1;
    float fragmentClonerOutputSpacingX_ = 24.0f;
    float fragmentClonerOutputSpacingY_ = 0.0f;
    float fragmentClonerOutputTimeOffsetFrames_ = 0.0f;
    FractureState fractureState_;
    FractureResult prefractureResult_;
    mutable int64_t fractureMotionLastFrame_ = std::numeric_limits<int64_t>::min();
    mutable DynamicsChannel1D motionX_;
    mutable DynamicsChannel1D motionY_;
    mutable DynamicsChannel1D motionRotation_;
    mutable DynamicsChannel1D motionScaleX_;
    mutable DynamicsChannel1D motionScaleY_;
    mutable int64_t motionLastFrame_ = std::numeric_limits<int64_t>::min();
    bool scriptComponentEnabled_ = false;
    bool clonerComponentEnabled_ = false;
    bool layoutComponentEnabled_ = false;
    bool collisionComponentEnabled_ = false;
    QColor collisionDisplayColor_ = QColor(61, 211, 192, 220);
    int collisionBodyType_ = 0; // 0=Dynamic, 1=Static, 2=Kinematic
    bool collisionOwnsPhysicsEnable_ = false;
    int collisionShape_ = 0;
    float collisionWidth_ = 0.0f;
    float collisionHeight_ = 0.0f;
    float collisionRadius_ = 0.0f;
    float collisionOffsetX_ = 0.0f;
    float collisionOffsetY_ = 0.0f;
    int rigidBodyColliderShape_ = -1;
    float rigidBodyColliderRestitution_ = -1.0f;
    QTransform rigidBodyRestTransform_;
    QPointF rigidBodyRestCenter_;
    float rigidBodyRestAngle_ = 0.0f;
    QTransform rigidBodyLocalTransform(const ArtifactAbstractLayer* layer,
        const ArtifactCore::SharedPtr<ArtifactCore::RigidBody2D>& body) const;
    float collisionFloorY_ = 0.0f;
    bool collisionCompositionBounds_ = false;
    bool jointComponentEnabled_ = false;
    int jointType_ = 0; // 0=Distance, 1=Pin, 2=Hinge, 3=Spring, 4=Rope, 5=Slider
    float jointOwnerAnchorX_ = 0, jointOwnerAnchorY_ = 0;
    float jointTargetAnchorX_ = 0, jointTargetAnchorY_ = 0;
    QString jointTargetLayerName_;
    float jointLength_ = 0.0f;   // 0 = capture the enable-time separation
    float jointStiffness_ = 5.0f; // b2 hertz (spring frequency)
    float jointDamping_ = 0.7f;   // b2 damping ratio
    bool jointAngleLimitEnabled_ = false;
    float jointLowerAngle_ = -45.0f;
    float jointUpperAngle_ = 45.0f;
    float jointAxisX_ = 1.0f;
    float jointAxisY_ = 0.0f;
    bool jointLinearLimitEnabled_ = false;
    float jointLowerLimit_ = -100.0f;
    float jointUpperLimit_ = 100.0f;
    bool jointMotorEnabled_ = false;
    float jointMotorSpeed_ = 0.0f;
    float jointMotorForce_ = 100.0f;
    float jointBreakForce_ = 0.0f;
    bool jointBroken_ = false;
    bool crowdComponentEnabled_ = false;
    float crowdCohesion_ = 0.5f;
    float crowdSeparation_ = 0.5f;
    float crowdAlignment_ = 0.5f;
    float crowdMaxSpeed_ = 120.0f;
    float crowdJitter_ = 0.1f;
    bool particleEmitterComponentEnabled_ = false;
    int particleEmitterCount_ = 16;
    float particleEmitterSpeed_ = 120.0f;
    float particleEmitterLifetime_ = 1.0f;
    bool fluidComponentEnabled_ = false;
    int fluidMode_ = 0; // 0=Smoke/Ink, 1=Liquid Container
    int fluidGridWidth_ = 128;
    int fluidGridHeight_ = 128;
    float fluidViscosity_ = 0.00001f;
    float fluidDiffusion_ = 0.00001f;
    float fluidBuoyancy_ = 0.05f;
    float fluidVorticity_ = 0.1f;
    int fluidSolverIterations_ = 20;
    float liquidFillAmount_ = 0.55f;
    float liquidInflowRate_ = 0.0f;
    float liquidInflowWidth_ = 0.25f;
    float liquidInflowSpeed_ = 0.8f;
    float liquidInflowPosition_ = 0.5f;
    int liquidOpeningEdge_ = -1;
    float liquidSpillCullMargin_ = 1024.0f;
    float liquidGravity_ = 1.8f;
    float liquidSurfaceTension_ = 0.15f;
    float liquidParticleSpacing_ = 0.055f;
    int liquidSubsteps_ = 3;
    float liquidSurfaceOpacity_ = 0.64f;
    float liquidEdgeOpacity_ = 0.42f;
    float liquidFoamAmount_ = 1.0f;
    float liquidContainerOpacity_ = 0.58f;
    float liquidContainerWidth_ = 2.5f;
    FloatColor liquidColor_ = FloatColor(0.10f, 0.50f, 0.98f, 1.0f);
    FloatColor liquidFoamColor_ = FloatColor(0.78f, 0.93f, 1.0f, 1.0f);
    LayerFluidRuntimeState fluidRuntime_;
    NamedVector<ArtifactCore::ParticleVertex> componentParticles_{
        ContainerName{"Layer.ComponentParticles"}};
    mutable int64_t componentParticlesLastFrame_ =
        std::numeric_limits<int64_t>::min();
    mutable int64_t lastCollisionImpactFrame_ =
        std::numeric_limits<int64_t>::min();
    mutable int64_t lastRigidWindForceFrame_ =
        std::numeric_limits<int64_t>::min();
    LayerRigidBodyContactState rigidBodyContactState_;
    QSet<QString> activeRigidBodyContactKeys_;
    LayerComponentHost componentHost_;
    QString builtinSourceComponentType_;
    ArtifactCore::Optional<LayerEvaluationState> authoritativeComponentState_;
    int64_t authoritativeComponentFrame_ =
        std::numeric_limits<int64_t>::min();
    LayerEvaluationState componentEvaluationState_;
    int layoutMode_ = 0;
    int layoutAnchorMode_ = 0;
    int layoutHorizontalPin_ = 0;
    int layoutVerticalPin_ = 0;
    int layoutScaleMode_ = 0;
    bool layoutResponsiveEnabled_ = false;
    float layoutResponsiveOffsetX_ = 0.0f;
    float layoutResponsiveOffsetY_ = 0.0f;
    bool layoutSafeAreaEnabled_ = false;
    float layoutSafeAreaPaddingX_ = 0.0f;
    float layoutSafeAreaPaddingY_ = 0.0f;
    int layoutStackDirection_ = 0;
    float layoutGap_ = 24.0f;
    int layoutMaxPerRow_ = 0;
    int clonerMode_ = 0;
    int clonerCloneCount_ = 3;
    float clonerTimeOffsetStep_ = 0.0f;
    bool clonerSequenceEnabled_ = false;
    float clonerSequenceRate_ = 8.0f;
    float clonerSequenceSoftness_ = 1.0f;
    float clonerOffsetX_ = 160.0f;
    float clonerOffsetY_ = 48.0f;
    float clonerOffsetZ_ = 0.0f;
    float clonerJitterX_ = 0.0f;
    float clonerJitterY_ = 0.0f;
    float clonerJitterZ_ = 0.0f;
    int clonerSeed_ = 0;
    int clonerColumns_ = 3;
    int clonerRows_ = 3;
    int clonerDepth_ = 1;
    float clonerSpacingX_ = 160.0f;
    float clonerSpacingY_ = 48.0f;
    float clonerSpacingZ_ = 0.0f;
    int clonerRadialCount_ = 8;
    float clonerRadius_ = 160.0f;
    float clonerStartAngle_ = 0.0f;
    float clonerEndAngle_ = 360.0f;
    float clonerRotationStep_ = 0.0f;
    float clonerOpacityDecay_ = 0.0f;
    NamedVector<ClonerTransformOperation> clonerTransforms_{
        ContainerName{"Layer.ClonerTransforms"}};
    NamedVector<LayerGeneratorDescriptor> extraGeneratorDescriptors_{
        ContainerName{"Layer.ExtraGenerators"}};
    NamedVector<LayerFieldDescriptor> extraFieldDescriptors_{
        ContainerName{"Layer.ExtraFields"}};
    NamedVector<LayerModifierDescriptor> extraCloneModifierDescriptors_{
        ContainerName{"Layer.ExtraCloneModifiers"}};
    QJsonObject scriptBinding_;

  // Mask and matte ownership is isolated to keep container instantiations out
  // of this already large implementation unit.
  LayerMaskMatteState maskMatteState_;

  uint32_t dirtyFlags_ = (uint32_t)LayerDirtyFlag::All;
  uint64_t dirtyReasonMask_ =
      static_cast<uint64_t>(LayerDirtyReason::PropertyChanged);
  mutable quint64 geometryRevision_ = 1;
  mutable quint64 cachedGlobalTransformRevision_ = 0;
  mutable quint64 cachedGlobalTransformParentRevision_ = 0;
  mutable int64_t cachedGlobalTransformFrame_ =
      std::numeric_limits<int64_t>::min();
  mutable LayerID cachedGlobalTransformParentId_;
  mutable QTransform cachedGlobalTransform_;
  mutable quint64 cachedBoundingBoxRevision_ = 0;
  mutable quint64 cachedBoundingBoxParentRevision_ = 0;
  mutable int64_t cachedBoundingBoxFrame_ = std::numeric_limits<int64_t>::min();
  mutable LayerID cachedBoundingBoxParentId_;
  mutable QRectF cachedBoundingBox_;

  // エフェクトコンテナ
  NamedVector<SharedPtr<ArtifactAbstractEffect>> effects_{
      ContainerName{"Layer.Effects"}};

  // レイヤーモディファイアコンテナ
  LayerModifierStack modifiers_;

  mutable QHash<QString, SharedPtr<AbstractProperty>> propertyCache_;
  mutable std::mutex propertyCacheMutex_;

  // Time remap
  std::unique_ptr<ArtifactCore::TimeRemapEffect> timeRemapEffect_;
  bool stopMotionSamplingEnabled_ = false;
  double stopMotionSamplingFrameRate_ = 12.0;

  // Variants
  NamedVector<std::unique_ptr<LayerVariant>> variants_{
      ContainerName{"Layer.Variants"}};
  size_t activeVariantIndex_ = 0;

public:
  Impl();
  ~Impl();
  void syncBuiltinComponentDescriptors();
  void syncBuiltinBoolsFromHost();
  std::type_index type_index_ = typeid(void);
  void goToStartFrame();
  void goToEndFrame();
  void goToNextFrame();
  void goToPrevFrame();

  bool is3D() const;
  AnimatableTransform3D transform_;
  AnimatableTransform2D transform2d_;
  ArtifactCore::AnimationLayerStackT<float> animationLayers_;
  QHash<QString, ArtifactCore::AnimationLayerStackT<float>> animationPropertyLayers_;
  Size_2D sourceSize_;

  // エフェクト管理メソッド
  void addEffect(SharedPtr<ArtifactAbstractEffect> effect);
  void removeEffect(const UniString &effectID);
  void clearEffects();
  std::vector<SharedPtr<ArtifactAbstractEffect>> getEffects() const;
  SharedPtr<ArtifactAbstractEffect>
  getEffect(const UniString &effectID) const;
  int effectCount() const;

  // モディファイア管理メソッド
  void addModifier(SharedPtr<ArtifactLayerModifier> modifier);
  void removeModifier(const QString& modifierId);
  void clearModifiers();
  std::vector<SharedPtr<ArtifactLayerModifier>> getModifiers() const;
  SharedPtr<ArtifactLayerModifier> getModifier(const QString& modifierId) const;
  int modifierCount() const;
  bool hasModifiers() const;

};

namespace {
bool g_globalLayerCacheEnabled = true;
}

ArtifactAbstractLayer::Impl::Impl() {
  // Avoid undefined draw bounds when a layer is queried before explicit size
  // assignment.
  sourceSize_ = Size_2D(1920, 1080);
  syncBuiltinComponentDescriptors();
}

ArtifactAbstractLayer::Impl::~Impl() {}

void ArtifactAbstractLayer::Impl::syncBuiltinComponentDescriptors() {
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

void ArtifactAbstractLayer::Impl::syncBuiltinBoolsFromHost() {
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

void ArtifactAbstractLayer::Impl::goToStartFrame()
{
  currentFrame_ = startTime_.framePosition();
}

void ArtifactAbstractLayer::Impl::goToEndFrame()
{
  const int64_t duration = std::max<int64_t>(
      1, outPoint_.framePosition() - inPoint_.framePosition());
  currentFrame_ = startTime_.framePosition() + duration - 1;
}

void ArtifactAbstractLayer::Impl::goToNextFrame()
{
  const int64_t endFrame = startTime_.framePosition() + std::max<int64_t>(
      1, outPoint_.framePosition() - inPoint_.framePosition()) - 1;
  if (currentFrame_ < endFrame) {
    ++currentFrame_;
  }
}

void ArtifactAbstractLayer::Impl::goToPrevFrame()
{
  const int64_t startFrame = startTime_.framePosition();
  if (currentFrame_ > startFrame) {
    --currentFrame_;
  }
}

bool ArtifactAbstractLayer::Impl::is3D() const { return is3D_; }

ArtifactAbstractLayer::ArtifactAbstractLayer() : impl_(new Impl()) {
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
  // A thumbnail is derived from layer content. Any mutation invalidates the
  // cached image so a later renderer cannot return a stale preview.
  impl_->thumbnailCache_ = QImage();
  impl_->thumbnailCacheSize_ = QSize();
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

namespace {
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
} // namespace

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

QTransform ArtifactAbstractLayer::Impl::rigidBodyLocalTransform(
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
        const QVector3D mappedVelocity = baseTransform.map(
            QVector3D(particle.vx, particle.vy, particle.vz)) -
            mappedVelocityOrigin;
        particle.vx = mappedVelocity.x();
        particle.vy = mappedVelocity.y();
        particle.vz = mappedVelocity.z();
        particle.r = impl_->liquidColor_.r();
        particle.g = impl_->liquidColor_.g();
        particle.b = impl_->liquidColor_.b();
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
        particle.vx = spill.vx;
        particle.vy = spill.vy;
        particle.vz = 0.0f;
        particle.r = impl_->liquidColor_.r();
        particle.g = impl_->liquidColor_.g();
        particle.b = impl_->liquidColor_.b();
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
      NamedVector<ArtifactCore::LiquidSurfaceSample2D> surfaceSamples{
          ContainerName{"Layer.LiquidSurfaceSamples"}};
      surfaceSamples.reserve(renderData.particles.size());
      for (std::size_t particleIndex = 0;
           particleIndex < renderData.particles.size(); ++particleIndex) {
        const auto& particle = renderData.particles[particleIndex];
        if (particle.a <= 0.0f || particle.size <= 0.0f) continue;
        const float foamBias =
            (particleIndex < containerParticleCount ? 0.45f : 1.0f) *
            impl_->liquidFoamAmount_;
        float collisionImpact = 0.0f;
        if (particleIndex < containerParticleCount) {
          const auto& containerParticles = liquid.particles();
          if (particleIndex < containerParticles.size()) {
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
        surfaceSamples.push_back({particle.px, particle.py, particle.size,
                                  particle.vx, particle.vy, foamBias,
                                  collisionImpact});
      }
      if (impl_->fluidRuntime_.liquidSurfaceFrame_ != frame) {
        impl_->fluidRuntime_.liquidSurfaceSnapshot_ =
            ArtifactCore::LiquidSolver2D::buildSurfaceSnapshot(surfaceSamples);
        impl_->fluidRuntime_.liquidSurfaceFrame_ = frame;
      }
      const auto& surface = impl_->fluidRuntime_.liquidSurfaceSnapshot_;
      if (!surface.triangles.empty()) {
        const float layerAlpha = std::clamp(opacityScale, 0.0f, 1.0f);
        const bool drawSurface = impl_->liquidSurfaceOpacity_ > 0.0f;
        const bool drawEdge = impl_->liquidEdgeOpacity_ > 0.0f;
        if (drawSurface) {
          for (const auto& triangle : surface.triangles) {
            const float thickness =
                std::clamp(triangle.thickness, 0.0f, 1.0f);
            const float lighten = (1.0f - thickness) * 0.10f;
            const float darken = 1.0f - thickness * 0.18f;
            const FloatColor surfaceColor(
                std::clamp(impl_->liquidColor_.r() * darken + lighten,
                           0.0f, 1.0f),
                std::clamp(impl_->liquidColor_.g() * darken + lighten,
                           0.0f, 1.0f),
                std::clamp(impl_->liquidColor_.b() * darken + lighten,
                           0.0f, 1.0f),
                impl_->liquidColor_.a() *
                    impl_->liquidSurfaceOpacity_ * layerAlpha *
                    (0.38f + thickness * 0.62f));
            renderer->drawSolidTriangleLocal(
                {triangle.a.x, triangle.a.y},
                {triangle.b.x, triangle.b.y},
                {triangle.c.x, triangle.c.y}, surfaceColor);
          }
        }
        if (drawEdge) {
          const FloatColor surfaceEdgeColor(
              impl_->liquidColor_.r() +
                  (1.0f - impl_->liquidColor_.r()) * 0.62f,
              impl_->liquidColor_.g() +
                  (1.0f - impl_->liquidColor_.g()) * 0.62f,
              impl_->liquidColor_.b() +
                  (1.0f - impl_->liquidColor_.b()) * 0.62f,
              impl_->liquidColor_.a() *
                  impl_->liquidEdgeOpacity_ * layerAlpha);
          for (const auto& segment : surface.contourSegments) {
            renderer->drawThickLineLocal(
                {segment.a.x, segment.a.y},
                {segment.b.x, segment.b.y}, 1.35f, surfaceEdgeColor);
          }
        }
        if (drawSurface || drawEdge) {
          for (auto& particle : renderData.particles) {
            particle.a *= 0.32f;
            particle.size *= 0.72f;
          }
        }
        const std::size_t foamCapacity =
            100000 - std::min<std::size_t>(renderData.particles.size(), 100000);
        const std::size_t foamCount =
            std::min(foamCapacity, surface.foamPoints.size());
        renderData.particles.reserve(renderData.particles.size() + foamCount);
        for (std::size_t foamIndex = 0; foamIndex < foamCount; ++foamIndex) {
          const auto& foam = surface.foamPoints[foamIndex];
          ArtifactCore::ParticleVertex particle{};
          particle.px = foam.position.x;
          particle.py = foam.position.y;
          particle.pz = 0.0f;
          particle.r = impl_->liquidFoamColor_.r();
          particle.g = impl_->liquidFoamColor_.g();
          particle.b = impl_->liquidFoamColor_.b();
          particle.a = foam.alpha * impl_->liquidFoamColor_.a() *
              std::clamp(opacityScale, 0.0f, 1.0f);
          particle.size = foam.size;
          particle.stretch = 1.0f;
          particle.lifetime = 1.0f;
          renderData.particles.push_back(particle);
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
      if (!renderData.particles.empty()) {
        renderer->drawParticles(renderData);
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
      particle.a *= std::clamp(opacityScale, 0.0f, 1.0f);
      renderData.particles.push_back(particle);
    }
    if (!renderData.particles.empty()) {
      if (impl_->fractureEnabled_ &&
          !impl_->fractureState_.shards.empty()) {
        fractureElement.debris = std::move(renderData);
      } else {
        renderer->drawParticles(renderData);
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

  // Masks
  if (hasMasks()) {
    QJsonArray masksArr;
    for (int maskIndex = 0; maskIndex < maskCount(); ++maskIndex) {
      const auto layerMask = impl_->maskMatteState_.mask(maskIndex);
      QJsonObject mobj;
      mobj["enabled"] = layerMask.isEnabled();
      mobj["locked"] = layerMask.isLocked();
      {
        const auto c = layerMask.color();
        QJsonObject cobj;
        cobj["r"] = c.r();
        cobj["g"] = c.g();
        cobj["b"] = c.b();
        cobj["a"] = c.a();
        mobj["color"] = cobj;
      }

      QJsonArray pathsArr;
      for (int pathIndex = 0; pathIndex < layerMask.maskPathCount();
           ++pathIndex) {
        const auto path = layerMask.maskPath(pathIndex);
        QJsonObject pobj;

        // vertices: 各頂点は position / inTangent / outTangent（QPointF = x,y）
        QJsonArray vertsArr;
        for (int vi = 0; vi < path.vertexCount(); ++vi) {
          const auto v = path.vertex(vi);
          QJsonObject vobj;
          vobj["px"] = v.position.x();
          vobj["py"] = v.position.y();
          vobj["ix"] = v.inTangent.x();
          vobj["iy"] = v.inTangent.y();
          vobj["ox"] = v.outTangent.x();
          vobj["oy"] = v.outTangent.y();
          vertsArr.append(vobj);
        }
        pobj["vertices"] = vertsArr;
        pobj["closed"] = path.isClosed();
        pobj["opacity"] = static_cast<double>(path.opacity());
        pobj["feather"] = static_cast<double>(path.feather());
        pobj["featherHorizontal"] = static_cast<double>(path.featherHorizontal());
        pobj["featherVertical"] = static_cast<double>(path.featherVertical());
        pobj["featherInner"] = static_cast<double>(path.featherInner());
        pobj["featherOuter"] = static_cast<double>(path.featherOuter());
        pobj["falloff"] = static_cast<int>(path.falloff());
        pobj["expansion"] = static_cast<double>(path.expansion());
        pobj["inverted"] = path.isInverted();
        pobj["mode"] = static_cast<int>(path.mode());
        pobj["name"] = path.name().toQString();

        // animation keyframes
        if (path.hasAnimationKeyframes()) {
          QJsonArray kfArr;
          for (const auto &kf : path.animationKeyframes()) {
            QJsonObject kfobj;
            kfobj["frame"] = static_cast<qint64>(kf.frame);
            kfobj["closed"] = kf.closed;
            kfobj["opacity"] = static_cast<double>(kf.opacity);
            kfobj["feather"] = static_cast<double>(kf.feather);
            kfobj["featherHorizontal"] = static_cast<double>(kf.featherHorizontal);
            kfobj["featherVertical"] = static_cast<double>(kf.featherVertical);
            kfobj["featherInner"] = static_cast<double>(kf.featherInner);
            kfobj["featherOuter"] = static_cast<double>(kf.featherOuter);
            kfobj["falloff"] = static_cast<int>(kf.falloff);
            kfobj["expansion"] = static_cast<double>(kf.expansion);
            kfobj["inverted"] = kf.inverted;
            kfobj["mode"] = static_cast<int>(kf.mode);
            kfobj["name"] = kf.name.toQString();

            QJsonArray kfVertsArr;
            for (const auto &v : kf.vertices) {
              QJsonObject vobj;
              vobj["px"] = v.position.x();
              vobj["py"] = v.position.y();
              vobj["ix"] = v.inTangent.x();
              vobj["iy"] = v.inTangent.y();
              vobj["ox"] = v.outTangent.x();
              vobj["oy"] = v.outTangent.y();
              kfVertsArr.append(vobj);
            }
            kfobj["vertices"] = kfVertsArr;
            kfArr.append(kfobj);
          }
          pobj["animationKeyframes"] = kfArr;
        }
        pathsArr.append(pobj);
      }
      mobj["paths"] = pathsArr;
      masksArr.append(mobj);
    }
    obj["masks"] = masksArr;
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
  if (obj.value(QStringLiteral("modulation")).isObject()) {
    restoreLayerModulationRouter(obj.value(QStringLiteral("modulation")).toObject(),
                                 impl_->modulationRouter_);
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

  // Masks
  if (obj.contains("masks") && obj["masks"].isArray()) {
    impl_->maskMatteState_.clearMasks();
    const auto masksArr = obj["masks"].toArray();
    for (const auto &maskVal : masksArr) {
      if (!maskVal.isObject()) continue;
      const auto mobj = maskVal.toObject();

      LayerMask layerMask;
      if (mobj.contains("enabled")) {
        layerMask.setEnabled(mobj["enabled"].toBool(true));
      }
      if (mobj.contains("locked")) {
        layerMask.setLocked(mobj["locked"].toBool(false));
      }
      if (mobj.contains("color") && mobj["color"].isObject()) {
        const auto cobj = mobj["color"].toObject();
        const float r = static_cast<float>(cobj.value("r").toDouble(0.28));
        const float g = static_cast<float>(cobj.value("g").toDouble(0.88));
        const float b = static_cast<float>(cobj.value("b").toDouble(1.0));
        const float a = static_cast<float>(cobj.value("a").toDouble(0.95));
        layerMask.setColor(FloatColor{r, g, b, a});
      }

      if (mobj.contains("paths") && mobj["paths"].isArray()) {
        const auto pathsArr = mobj["paths"].toArray();
        for (const auto &pathVal : pathsArr) {
          if (!pathVal.isObject()) continue;
          const auto pobj = pathVal.toObject();

          MaskPath path;
          path.clearVertices();
          if (pobj.contains("vertices") && pobj["vertices"].isArray()) {
            const auto vertsArr = pobj["vertices"].toArray();
            for (const auto &vVal : vertsArr) {
              if (!vVal.isObject()) continue;
              const auto vobj = vVal.toObject();
              MaskVertex v;
              v.position = QPointF(vobj["px"].toDouble(), vobj["py"].toDouble());
              v.inTangent = QPointF(vobj["ix"].toDouble(), vobj["iy"].toDouble());
              v.outTangent = QPointF(vobj["ox"].toDouble(), vobj["oy"].toDouble());
              path.addVertex(v);
            }
          }
          if (pobj.contains("closed"))
            path.setClosed(pobj["closed"].toBool(true));
          if (pobj.contains("opacity"))
            path.setOpacity(static_cast<float>(pobj["opacity"].toDouble(1.0)));
          if (pobj.contains("feather"))
            path.setFeather(static_cast<float>(pobj["feather"].toDouble(0.0)));
          if (pobj.contains("featherHorizontal"))
            path.setFeatherHorizontal(static_cast<float>(pobj["featherHorizontal"].toDouble(0.0)));
          if (pobj.contains("featherVertical"))
            path.setFeatherVertical(static_cast<float>(pobj["featherVertical"].toDouble(0.0)));
          if (pobj.contains("featherInner"))
            path.setFeatherInner(static_cast<float>(pobj["featherInner"].toDouble(0.0)));
          if (pobj.contains("featherOuter"))
            path.setFeatherOuter(static_cast<float>(pobj["featherOuter"].toDouble(0.0)));
          if (pobj.contains("falloff"))
            path.setFalloff(static_cast<MaskFeatherFalloff>(pobj["falloff"].toInt(0)));
          if (pobj.contains("expansion"))
            path.setExpansion(static_cast<float>(pobj["expansion"].toDouble(0.0)));
          if (pobj.contains("inverted"))
            path.setInverted(pobj["inverted"].toBool(false));
          if (pobj.contains("mode"))
            path.setMode(static_cast<MaskMode>(pobj["mode"].toInt(static_cast<int>(MaskMode::Add))));
          if (pobj.contains("name"))
            path.setName(UniString::fromQString(pobj["name"].toString()));

          // animation keyframes
          if (pobj.contains("animationKeyframes") && pobj["animationKeyframes"].isArray()) {
            const auto kfArr = pobj["animationKeyframes"].toArray();
            for (const auto &kfVal : kfArr) {
              if (!kfVal.isObject()) continue;
              const auto kfobj = kfVal.toObject();

              MaskPathKeyframeSnapshot snap;
              snap.frame = static_cast<int64_t>(kfobj["frame"].toVariant().toLongLong());
              snap.closed = kfobj["closed"].toBool(true);
              snap.opacity = static_cast<float>(kfobj["opacity"].toDouble(1.0));
              snap.feather = static_cast<float>(kfobj["feather"].toDouble(0.0));
              snap.featherHorizontal = static_cast<float>(kfobj["featherHorizontal"].toDouble(0.0));
              snap.featherVertical = static_cast<float>(kfobj["featherVertical"].toDouble(0.0));
              snap.featherInner = static_cast<float>(kfobj["featherInner"].toDouble(0.0));
              snap.featherOuter = static_cast<float>(kfobj["featherOuter"].toDouble(0.0));
              snap.falloff = static_cast<MaskFeatherFalloff>(kfobj["falloff"].toInt(0));
              snap.expansion = static_cast<float>(kfobj["expansion"].toDouble(0.0));
              snap.inverted = kfobj["inverted"].toBool(false);
              snap.mode = static_cast<MaskMode>(kfobj["mode"].toInt(static_cast<int>(MaskMode::Add)));
              snap.name = UniString::fromQString(kfobj["name"].toString());

              if (kfobj.contains("vertices") && kfobj["vertices"].isArray()) {
                const auto kfVertsArr = kfobj["vertices"].toArray();
                for (const auto &vVal : kfVertsArr) {
                  if (!vVal.isObject()) continue;
                  const auto vobj = vVal.toObject();
                  MaskVertex v;
                  v.position = QPointF(vobj["px"].toDouble(), vobj["py"].toDouble());
                  v.inTangent = QPointF(vobj["ix"].toDouble(), vobj["iy"].toDouble());
                  v.outTangent = QPointF(vobj["ox"].toDouble(), vobj["oy"].toDouble());
                  snap.vertices.push_back(v);
                }
              }
              path.setAnimationKeyframe(snap.frame, snap);
            }
          }

          layerMask.addMaskPath(path);
        }
      }

      impl_->maskMatteState_.addMask(layerMask);
    }
    changed();
  }

  applyPropertiesFromJson(obj);
}

void ArtifactAbstractLayer::Impl::addEffect(
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

void ArtifactAbstractLayer::Impl::removeEffect(const UniString &effectID) {
  if (effects_.removeIf(
          [&effectID](const SharedPtr<ArtifactAbstractEffect>& effect) {
            return effect && effect->effectID() == effectID;
          }) != 0) {
    qDebug("%s", qPrintable(QStringLiteral("[ArtifactAbstractLayer] Effect removed: %1")
                              .arg(effectID.toQString())));
  }
}

void ArtifactAbstractLayer::Impl::clearEffects() {
  effects_.clear();
  qDebug("[ArtifactAbstractLayer] All effects cleared");
}

std::vector<SharedPtr<ArtifactAbstractEffect>>
ArtifactAbstractLayer::Impl::getEffects() const {
  return effects_.toStdVector();
}

SharedPtr<ArtifactAbstractEffect>
ArtifactAbstractLayer::Impl::getEffect(const UniString &effectID) const {
  for (const auto &effect : effects_) {
    if (effect && effect->effectID() == effectID) {
      return effect;
    }
  }
  return nullptr;
}

int ArtifactAbstractLayer::Impl::effectCount() const {
  return static_cast<int>(effects_.size());
}

void ArtifactAbstractLayer::Impl::addModifier(
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

void ArtifactAbstractLayer::Impl::removeModifier(const QString& modifierId) {
  modifiers_.remove(modifierId);
}

void ArtifactAbstractLayer::Impl::clearModifiers() {
  modifiers_.clear();
}

std::vector<SharedPtr<ArtifactLayerModifier>>
ArtifactAbstractLayer::Impl::getModifiers() const {
  return modifiers_.modifiers();
}

SharedPtr<ArtifactLayerModifier>
ArtifactAbstractLayer::Impl::getModifier(const QString& modifierId) const {
  return modifiers_.modifier(modifierId);
}

int ArtifactAbstractLayer::Impl::modifierCount() const {
  return modifiers_.count();
}

bool ArtifactAbstractLayer::Impl::hasModifiers() const {
  return !modifiers_.isEmpty();
}

void ArtifactAbstractLayer::addEffect(
    SharedPtr<ArtifactAbstractEffect> effect) {
  impl_->addEffect(effect);
}

void ArtifactAbstractLayer::removeEffect(const UniString &effectID) {
  impl_->removeEffect(effectID);
}

void ArtifactAbstractLayer::clearEffects() { impl_->clearEffects(); }

std::vector<SharedPtr<ArtifactAbstractEffect>>
ArtifactAbstractLayer::getEffects() const {
  return impl_->getEffects();
}

SharedPtr<ArtifactAbstractEffect>
ArtifactAbstractLayer::getEffect(const UniString &effectID) const {
  return impl_->getEffect(effectID);
}

int ArtifactAbstractLayer::effectCount() const { return impl_->effectCount(); }

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

std::vector<ArtifactCore::PropertyGroup>
ArtifactAbstractLayer::getLayerPropertyGroups() const {
  using namespace ArtifactCore;
  PropertyGroup layerGroup(QStringLiteral("Layer"));

  auto makeProp = [this](const QString &name, PropertyType type,
                         const QVariant &value, int priority = 0) {
    return persistentLayerProperty(name, type, value, priority);
  };

  layerGroup.addProperty(makeProp(QStringLiteral("layer.name"),
                                  PropertyType::String, layerName(), -200));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.visible"),
                                  PropertyType::Boolean, isVisible(), -190));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.locked"),
                                  PropertyType::Boolean, isLocked(), -180));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.selectionLocked"),
                                  PropertyType::Boolean, isSelectionLocked(), -179));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.transformLocked"),
                                  PropertyType::Boolean, isTransformLocked(), -178));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.timingLocked"),
                                  PropertyType::Boolean, isTimingLocked(), -177));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.guide"),
                                  PropertyType::Boolean, isGuide(), -170));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.solo"),
                                  PropertyType::Boolean, isSolo(), -160));
  auto cachePolicyProp = makeProp(QStringLiteral("layer.cachePolicy"),
                                  PropertyType::Integer,
                                  static_cast<int>(layerCachePolicy()),
                                  -159);
  cachePolicyProp->setDisplayLabel(QStringLiteral("Cache Policy"));
  layerGroup.addProperty(cachePolicyProp);
  layerGroup.addProperty(makeProp(QStringLiteral("layer.shy"),
                                  PropertyType::Boolean, isShy(), -150));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.labelColorIndex"),
                                  PropertyType::Integer, labelColorIndex(),
                                  -145));
  layerGroup.addProperty(makeProp(QStringLiteral("layer.is3D"),
                                  PropertyType::Boolean, is3D(), -144));
  auto twoPointFiveDEnabled = makeProp(QStringLiteral("layer.2_5d.enabled"),
      PropertyType::Boolean, impl_->twoPointFiveDEnabled_, -143);
  twoPointFiveDEnabled->setDisplayLabel(QStringLiteral("2.5D Lens"));
  twoPointFiveDEnabled->setTooltip(QStringLiteral("Uses a local 2D-stack lens; it does not use the composition 3D camera."));
  twoPointFiveDEnabled->setInlineHelp(QStringLiteral("Fake 3D; ignores the comp camera."));
  twoPointFiveDEnabled->setWhatsThis(QStringLiteral("Renders this layer with a local 2.5D lens for a pseudo-3D look.\nIt does NOT use the composition 3D camera or scene lights.\nFor real 3D, use a 3D layer type instead."));
  layerGroup.addProperty(twoPointFiveDEnabled);
  auto twoPointFiveDDepth = makeProp(QStringLiteral("layer.2_5d.depth"),
      PropertyType::Float, impl_->twoPointFiveDDepth_, -142);
  twoPointFiveDDepth->setDisplayLabel(QStringLiteral("2.5D Depth"));
  twoPointFiveDDepth->setSoftRange(-1000.0, 1000.0);
  layerGroup.addProperty(twoPointFiveDDepth);
  auto twoPointFiveDCameraDistance = makeProp(QStringLiteral("layer.2_5d.cameraDistance"),
      PropertyType::Float, impl_->twoPointFiveDCameraDistance_, -141);
  twoPointFiveDCameraDistance->setDisplayLabel(QStringLiteral("2.5D Camera Distance"));
  twoPointFiveDCameraDistance->setHardRange(1.0, 1000000.0);
  twoPointFiveDCameraDistance->setSoftRange(100.0, 5000.0);
  layerGroup.addProperty(twoPointFiveDCameraDistance);
  layerGroup.addProperty(makeProp(QStringLiteral("layer.2_5d.depthOfFieldEnabled"),
      PropertyType::Boolean, impl_->twoPointFiveDDepthOfFieldEnabled_, -140));
  auto twoPointFiveDFocusDepth = makeProp(QStringLiteral("layer.2_5d.focusDepth"),
      PropertyType::Float, impl_->twoPointFiveDFocusDepth_, -139);
  twoPointFiveDFocusDepth->setDisplayLabel(QStringLiteral("2.5D Focus Depth"));
  twoPointFiveDFocusDepth->setSoftRange(-1000.0, 1000.0);
  layerGroup.addProperty(twoPointFiveDFocusDepth);
  auto twoPointFiveDFocusRange = makeProp(QStringLiteral("layer.2_5d.focusRange"),
      PropertyType::Float, impl_->twoPointFiveDFocusRange_, -138);
  twoPointFiveDFocusRange->setDisplayLabel(QStringLiteral("2.5D Focus Range"));
  twoPointFiveDFocusRange->setHardRange(1.0, 1000000.0);
  layerGroup.addProperty(twoPointFiveDFocusRange);
  auto twoPointFiveDMaxBlur = makeProp(QStringLiteral("layer.2_5d.maxBlur"),
      PropertyType::Float, impl_->twoPointFiveDMaxBlur_, -137);
  twoPointFiveDMaxBlur->setDisplayLabel(QStringLiteral("2.5D Max Blur"));
  twoPointFiveDMaxBlur->setHardRange(0.0, 64.0);
  layerGroup.addProperty(twoPointFiveDMaxBlur);
  layerGroup.addProperty(makeProp(QStringLiteral("layer.2_5d.motionBlurEnabled"),
      PropertyType::Boolean, impl_->twoPointFiveDMotionBlurEnabled_, -136));
  auto twoPointFiveDShutter = makeProp(QStringLiteral("layer.2_5d.motionBlurShutterAngle"),
      PropertyType::Float, impl_->twoPointFiveDMotionBlurShutterAngle_, -135);
  twoPointFiveDShutter->setDisplayLabel(QStringLiteral("2.5D Motion Blur Shutter"));
  twoPointFiveDShutter->setHardRange(0.0, 720.0);
  layerGroup.addProperty(twoPointFiveDShutter);
  auto twoPointFiveDSamples = makeProp(QStringLiteral("layer.2_5d.motionBlurSamples"),
      PropertyType::Integer, impl_->twoPointFiveDMotionBlurSamples_, -134);
  twoPointFiveDSamples->setDisplayLabel(QStringLiteral("2.5D Motion Blur Samples"));
  twoPointFiveDSamples->setHardRange(2.0, 8.0);
  layerGroup.addProperty(twoPointFiveDSamples);

  auto opacityProp =
      makeProp(QStringLiteral("layer.opacity"), PropertyType::Float,
               static_cast<double>(opacity()), -140);
  opacityProp->setDisplayLabel(QStringLiteral("Opacity"));
  opacityProp->setHardRange(0.0, 1.0);
  opacityProp->setSoftRange(0.0, 1.0);
  opacityProp->setStep(0.01);
  opacityProp->setAnimatable(true);
  layerGroup.addProperty(opacityProp);

  // トランスフォームのプロパティグループ（優先度を高く設定）
  PropertyGroup transformGroup(QStringLiteral("Transform"));
  const auto &t3 = transform3D();
  const auto sz = sourceSize();
  QSize compositionSize(1920, 1080);
  if (const auto *composition =
          dynamic_cast<const ArtifactAbstractComposition *>(
              compositionObject())) {
    const QSize candidate = composition->settings().compositionSize();
    if (candidate.width() > 0 && candidate.height() > 0) {
      compositionSize = candidate;
    }
  }
  const int sourceWidth = std::max(1, sz.width);
  const int sourceHeight = std::max(1, sz.height);
  const double positionRangeX =
      static_cast<double>(std::max(compositionSize.width() * 2, 4096));
  const double positionRangeY =
      static_cast<double>(std::max(compositionSize.height() * 2, 4096));
  const double anchorRangeX =
      static_cast<double>(std::max(sourceWidth * 2, 2048));
  const double anchorRangeY =
      static_cast<double>(std::max(sourceHeight * 2, 2048));

  PropertyGroup initialGroup(QStringLiteral("Initial"));
  auto sourceWidthProp =
      makeProp(QStringLiteral("source.width"), PropertyType::Integer,
               sz.width, -500);
  sourceWidthProp->setDisplayLabel(QStringLiteral("Initial Width"));
  sourceWidthProp->setUnit(QStringLiteral("px"));
  sourceWidthProp->setTooltip(
      QStringLiteral("Base layer width. This value is not keyframeable."));
  sourceWidthProp->setHardRange(1.0, 16384.0);
  sourceWidthProp->setSoftRange(1.0, 4096.0);
  initialGroup.addProperty(sourceWidthProp);

  auto sourceHeightProp =
      makeProp(QStringLiteral("source.height"), PropertyType::Integer,
               sz.height, -499);
  sourceHeightProp->setDisplayLabel(QStringLiteral("Initial Height"));
  sourceHeightProp->setUnit(QStringLiteral("px"));
  sourceHeightProp->setTooltip(
      QStringLiteral("Base layer height. This value is not keyframeable."));
  sourceHeightProp->setHardRange(1.0, 16384.0);
  sourceHeightProp->setSoftRange(1.0, 4096.0);
  initialGroup.addProperty(sourceHeightProp);

  auto initialRotationProp =
      makeProp(QStringLiteral("transform.initialRotation"),
               PropertyType::Float, t3.initialRotation(), -498);
  initialRotationProp->setDisplayLabel(QStringLiteral("Initial Angle"));
  initialRotationProp->setUnit(QStringLiteral("deg"));
  initialRotationProp->setTooltip(
      QStringLiteral("Base layer angle. This value is not keyframeable."));
  initialRotationProp->setSoftRange(-180.0, 180.0);
  initialGroup.addProperty(initialRotationProp);

  auto posXProp = makeProp(QStringLiteral("transform.position.x"),
                           PropertyType::Float, t3.positionX(), -300);
  posXProp->setDisplayLabel(QStringLiteral("Position X"));
  posXProp->setUnit(QStringLiteral("px"));
  posXProp->setStep(1.0);
  posXProp->setSoftRange(-positionRangeX, positionRangeX);
  posXProp->setAnimatable(true);
  transformGroup.addProperty(posXProp);

  auto posYProp = makeProp(QStringLiteral("transform.position.y"),
                           PropertyType::Float, t3.positionY(), -299);
  posYProp->setDisplayLabel(QStringLiteral("Position Y"));
  posYProp->setUnit(QStringLiteral("px"));
  posYProp->setStep(1.0);
  posYProp->setSoftRange(-positionRangeY, positionRangeY);
  posYProp->setAnimatable(true);
  transformGroup.addProperty(posYProp);

  auto scaleXProp = makeProp(QStringLiteral("transform.scale.x"),
                             PropertyType::Float, t3.scaleX(), -298);
  scaleXProp->setDisplayLabel(QStringLiteral("Scale X"));
  scaleXProp->setAnimatable(true);
  scaleXProp->setStep(0.01);
  scaleXProp->setSoftRange(0.0, 2.0);  // Soft range for typical use (0-200%)
  transformGroup.addProperty(scaleXProp);

  auto scaleYProp = makeProp(QStringLiteral("transform.scale.y"),
                             PropertyType::Float, t3.scaleY(), -297);
  scaleYProp->setDisplayLabel(QStringLiteral("Scale Y"));
  scaleYProp->setAnimatable(true);
  scaleYProp->setStep(0.01);
  scaleYProp->setSoftRange(0.0, 2.0);  // Soft range for typical use (0-200%)
  transformGroup.addProperty(scaleYProp);

  auto rotationProp = makeProp(QStringLiteral("transform.rotation"),
                               PropertyType::Float, t3.rotation(), -296);
  rotationProp->setDisplayLabel(is3D() ? QStringLiteral("Rotation Z")
                                       : QStringLiteral("Rotation"));
  rotationProp->setUnit(QStringLiteral("deg"));
  rotationProp->setStep(1.0);
  rotationProp->setSoftRange(-180.0, 180.0);
  rotationProp->setAnimatable(true);
  transformGroup.addProperty(rotationProp);

  if (is3D()) {
    append3DTransformProperties(transformGroup, t3, positionRangeX,
                                anchorRangeX);
  }

  auto autoOrientProp =
      makeProp(QStringLiteral("transform.autoOrient"), PropertyType::Integer,
               static_cast<int>(t3.autoOrientMode()), -295);
  autoOrientProp->setDisplayLabel(QStringLiteral("Auto-Orient"));
  autoOrientProp->setTooltip(QStringLiteral(
      "Off: keep the current rotation.\n"
      "Along Path: rotate the layer to follow the motion path tangent.\n"
      "Along Path at Frame Start: use the tangent at the start of the current segment."));
  autoOrientProp->setInlineHelp(QStringLiteral("Auto-rotate along the motion path."));
  autoOrientProp->setWhatsThis(QStringLiteral("Rotates the layer automatically from its motion path.\nOff keeps keyframed rotation; Along Path follows the tangent.\nUseful for vehicles, arrows and characters walking a path."));
  transformGroup.addProperty(autoOrientProp);

  auto anchorXProp = makeProp(QStringLiteral("transform.anchor.x"),
                              PropertyType::Float, t3.anchorX(), -295);
  anchorXProp->setDisplayLabel(QStringLiteral("Anchor X"));
  anchorXProp->setUnit(QStringLiteral("px"));
  anchorXProp->setStep(1.0);
  anchorXProp->setSoftRange(-anchorRangeX, anchorRangeX);
  anchorXProp->setAnimatable(true);
  transformGroup.addProperty(anchorXProp);

  auto anchorYProp = makeProp(QStringLiteral("transform.anchor.y"),
                              PropertyType::Float, t3.anchorY(), -294);
  anchorYProp->setDisplayLabel(QStringLiteral("Anchor Y"));
  anchorYProp->setUnit(QStringLiteral("px"));
  anchorYProp->setStep(1.0);
  anchorYProp->setSoftRange(-anchorRangeY, anchorRangeY);
  anchorYProp->setAnimatable(true);
  transformGroup.addProperty(anchorYProp);

  auto inPointProp =
      makeProp(QStringLiteral("time.inPoint"), PropertyType::Integer,
               static_cast<qint64>(inPoint().framePosition()), -90);
  inPointProp->setUnit(QStringLiteral("frames"));
  inPointProp->setTooltip(QStringLiteral("Layer in-point on timeline"));
  inPointProp->setWhatsThis(QStringLiteral("First timeline frame where the layer is visible.\nDrag the clip head in the Timeline right pane to change it.\nCompare Start Time, which shifts the source content instead."));
  layerGroup.addProperty(inPointProp);

  auto outPointProp =
      makeProp(QStringLiteral("time.outPoint"), PropertyType::Integer,
               static_cast<qint64>(outPoint().framePosition()), -80);
  outPointProp->setUnit(QStringLiteral("frames"));
  outPointProp->setTooltip(QStringLiteral("Layer out-point on timeline"));
  outPointProp->setWhatsThis(QStringLiteral("First timeline frame where the layer is already gone (exclusive end).\nDrag the clip tail in the Timeline right pane to change it."));
  layerGroup.addProperty(outPointProp);

  auto startTimeProp =
      makeProp(QStringLiteral("time.startTime"), PropertyType::Integer,
               static_cast<qint64>(startTime().framePosition()), -70);
  startTimeProp->setUnit(QStringLiteral("frames"));
  startTimeProp->setTooltip(
      QStringLiteral("Layer start offset in source time"));
  startTimeProp->setInlineHelp(QStringLiteral("Shifts source content, not visibility."));
  startTimeProp->setWhatsThis(QStringLiteral("Offsets the source content against the timeline.\nIn/Out points decide WHEN the layer shows; Start Time decides WHAT part of the source shows."));
  layerGroup.addProperty(startTimeProp);

  // 物理演算プロパティグループ
  PropertyGroup physicsGroup(QStringLiteral("Physics"));

  auto physicsEnabledProp =
      makeProp(QStringLiteral("physics.enabled"), PropertyType::Boolean,
               impl_->physicsComponent_.settings().enabled, -100);
  physicsGroup.addProperty(physicsEnabledProp);

  auto softBodyEnabledProp =
      makeProp(QStringLiteral("physics.softBody.enabled"), PropertyType::Boolean,
               impl_->softBodyPhysicsEnabled_, -99);
  softBodyEnabledProp->setDisplayLabel(QStringLiteral("Soft Body Grid"));
  softBodyEnabledProp->setTooltip(
      QStringLiteral("Simulate rectangular Shape layers as a deformable grid."));
  physicsGroup.addProperty(softBodyEnabledProp);

  auto cloth3DEnabledProp =
      makeProp(QStringLiteral("physics.cloth3D.enabled"), PropertyType::Boolean,
               impl_->cloth3DPhysicsEnabled_, -99);
  cloth3DEnabledProp->setDisplayLabel(QStringLiteral("Cloth 3D Grid"));
  cloth3DEnabledProp->setTooltip(
      QStringLiteral("Simulate this layer as a 3D cloth grid (base slice)."));
  physicsGroup.addProperty(cloth3DEnabledProp);

  auto materialEnabledProp =
      makeProp(QStringLiteral("physics.material.enabled"), PropertyType::Boolean,
               impl_->materialPhysicsEnabled_, -98);
  materialEnabledProp->setDisplayLabel(QStringLiteral("Material Simulation"));
  materialEnabledProp->setTooltip(
      QStringLiteral("Use the continuum material solver for flesh, foam, rubber, or wood."));
  physicsGroup.addProperty(materialEnabledProp);

  auto materialPresetProp =
      makeProp(QStringLiteral("physics.material.preset"), PropertyType::Integer,
               impl_->materialPhysicsPreset_, -97);
  materialPresetProp->setDisplayLabel(QStringLiteral("Material Preset"));
  materialPresetProp->setTooltip(
      QStringLiteral("0=Flesh, 1=Foam, 2=Hard Rubber, 3=Wood."));
  materialPresetProp->setHardRange(0.0, 3.0);
  materialPresetProp->setSoftRange(0.0, 3.0);
  physicsGroup.addProperty(materialPresetProp);

  auto stiffnessProp =
      makeProp(QStringLiteral("physics.stiffness"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().stiffness), -99);
  stiffnessProp->setHardRange(0.0, 1000.0);
  stiffnessProp->setSoftRange(0.0, 500.0);
  stiffnessProp->setStep(1.0);
  physicsGroup.addProperty(stiffnessProp);

  auto dampingProp =
      makeProp(QStringLiteral("physics.damping"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().damping), -98);
  dampingProp->setHardRange(0.0, 100.0);
  dampingProp->setSoftRange(0.0, 50.0);
  dampingProp->setStep(0.1);
  physicsGroup.addProperty(dampingProp);

  auto followThroughProp =
      makeProp(QStringLiteral("physics.followThroughGain"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().followThroughGain), -97);
  followThroughProp->setHardRange(0.0, 2.0);
  followThroughProp->setSoftRange(0.0, 1.0);
  followThroughProp->setStep(0.01);
  physicsGroup.addProperty(followThroughProp);

  auto fallProfileProp =
      makeProp(QStringLiteral("physics.fallProfile"), PropertyType::Integer,
               impl_->physicsComponent_.settings().fallProfile, -96);
  fallProfileProp->setDisplayLabel(QStringLiteral("Fall Profile"));
  fallProfileProp->setTooltip(
      QStringLiteral("0=Custom, 1=Light, 2=Normal, 3=Heavy, 4=Floaty."));
  fallProfileProp->setHardRange(0.0, 4.0);
  fallProfileProp->setSoftRange(0.0, 4.0);
  fallProfileProp->setStep(1.0);
  physicsGroup.addProperty(fallProfileProp);

  auto gravityYProp =
      makeProp(QStringLiteral("physics.gravityY"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().gravityY), -96);
  gravityYProp->setDisplayLabel(QStringLiteral("World Gravity Y (Advanced)"));
  gravityYProp->setTooltip(
      QStringLiteral("Gravity used by falling rigid and soft-body layers."));
  gravityYProp->setUnit(QStringLiteral("px/s^2"));
  gravityYProp->setHardRange(-5000.0, 5000.0);
  gravityYProp->setSoftRange(-2000.0, 2000.0);
  gravityYProp->setStep(10.0);
  physicsGroup.addProperty(gravityYProp);

  auto linearDampingProp =
      makeProp(QStringLiteral("physics.linearDamping"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().linearDamping), -95);
  linearDampingProp->setDisplayLabel(QStringLiteral("Air Drag"));
  linearDampingProp->setTooltip(
      QStringLiteral("Linear air resistance applied to rigid and soft-body layers."));
  linearDampingProp->setHardRange(0.0, 50.0);
  linearDampingProp->setSoftRange(0.0, 10.0);
  linearDampingProp->setStep(0.1);
  physicsGroup.addProperty(linearDampingProp);

  auto angularDampingProp =
      makeProp(QStringLiteral("physics.angularDamping"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().angularDamping), -94);
  angularDampingProp->setDisplayLabel(QStringLiteral("Angular Drag"));
  angularDampingProp->setTooltip(
      QStringLiteral("Rotational air resistance applied to the Box2D body."));
  angularDampingProp->setHardRange(0.0, 50.0);
  angularDampingProp->setSoftRange(0.0, 10.0);
  angularDampingProp->setStep(0.1);
  physicsGroup.addProperty(angularDampingProp);

  auto gravityScaleProp =
      makeProp(QStringLiteral("physics.gravityScale"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().gravityScale), -93);
  gravityScaleProp->setDisplayLabel(QStringLiteral("Gravity Scale"));
  gravityScaleProp->setTooltip(
      QStringLiteral("Per-layer multiplier for the composition gravity."));
  gravityScaleProp->setHardRange(-10.0, 10.0);
  gravityScaleProp->setSoftRange(-2.0, 2.0);
  gravityScaleProp->setStep(0.01);
  physicsGroup.addProperty(gravityScaleProp);

  auto windEnabledProp =
      makeProp(QStringLiteral("physics.wind.enabled"), PropertyType::Boolean,
               impl_->physicsComponent_.settings().windEnabled, -92);
  windEnabledProp->setDisplayLabel(QStringLiteral("Wind Enabled"));
  windEnabledProp->setTooltip(
      QStringLiteral("Apply directional wind to this rigid body. Targeted Live Fields mask its strength."));
  physicsGroup.addProperty(windEnabledProp);

  auto windXProp =
      makeProp(QStringLiteral("physics.wind.x"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().windX), -92);
  windXProp->setDisplayLabel(QStringLiteral("Wind Direction X"));
  windXProp->setHardRange(-1.0, 1.0);
  windXProp->setSoftRange(-1.0, 1.0);
  windXProp->setStep(0.01);
  physicsGroup.addProperty(windXProp);

  auto windYProp =
      makeProp(QStringLiteral("physics.wind.y"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().windY), -92);
  windYProp->setDisplayLabel(QStringLiteral("Wind Direction Y"));
  windYProp->setHardRange(-1.0, 1.0);
  windYProp->setSoftRange(-1.0, 1.0);
  windYProp->setStep(0.01);
  physicsGroup.addProperty(windYProp);

  auto windStrengthProp =
      makeProp(QStringLiteral("physics.wind.strength"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().windStrength), -92);
  windStrengthProp->setDisplayLabel(QStringLiteral("Wind Strength"));
  windStrengthProp->setHardRange(0.0, 100000.0);
  windStrengthProp->setSoftRange(0.0, 5000.0);
  windStrengthProp->setStep(10.0);
  physicsGroup.addProperty(windStrengthProp);

  auto windTorqueProp =
      makeProp(QStringLiteral("physics.wind.torque"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().windTorque), -92);
  windTorqueProp->setDisplayLabel(QStringLiteral("Wind Spin"));
  windTorqueProp->setTooltip(
      QStringLiteral("Additional rotational force from wind; 0 leaves rotation to collisions."));
  windTorqueProp->setHardRange(-100.0, 100.0);
  windTorqueProp->setSoftRange(-10.0, 10.0);
  windTorqueProp->setStep(0.01);
  physicsGroup.addProperty(windTorqueProp);

  auto restitutionProp =
      makeProp(QStringLiteral("physics.restitution"), PropertyType::Float,
               static_cast<double>(
                   impl_->physicsComponent_.settings().restitution),
               -94);
  restitutionProp->setDisplayLabel(QStringLiteral("Collision Bounce"));
  restitutionProp->setHardRange(0.0, 1.0);
  restitutionProp->setSoftRange(0.0, 1.0);
  restitutionProp->setStep(0.01);
  physicsGroup.addProperty(restitutionProp);

  auto initialVelocityYProp =
      makeProp(QStringLiteral("physics.initialVelocityY"), PropertyType::Float,
               static_cast<double>(impl_->clonePhysicsInitialVelocityY_), -92);
  initialVelocityYProp->setDisplayLabel(QStringLiteral("Initial Velocity Y"));
  initialVelocityYProp->setUnit(QStringLiteral("px/s"));
  initialVelocityYProp->setHardRange(-5000.0, 5000.0);
  initialVelocityYProp->setSoftRange(-2000.0, 2000.0);
  initialVelocityYProp->setStep(10.0);
  physicsGroup.addProperty(initialVelocityYProp);

  auto maxBouncesProp =
      makeProp(QStringLiteral("physics.maxBounces"), PropertyType::Integer,
               impl_->clonePhysicsMaxBounces_, -91);
  maxBouncesProp->setDisplayLabel(QStringLiteral("Max Bounces"));
  maxBouncesProp->setHardRange(0.0, 32.0);
  maxBouncesProp->setSoftRange(0.0, 16.0);
  maxBouncesProp->setStep(1.0);
  physicsGroup.addProperty(maxBouncesProp);

  auto wiggleFreqProp =
      makeProp(QStringLiteral("physics.wiggleFreq"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().wiggleFreq), -93);
  wiggleFreqProp->setUnit(QStringLiteral("Hz"));
  wiggleFreqProp->setSoftRange(0.0, 10.0);
  physicsGroup.addProperty(wiggleFreqProp);

  auto wiggleAmpProp =
      makeProp(QStringLiteral("physics.wiggleAmp"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().wiggleAmp), -93);
  wiggleAmpProp->setSoftRange(0.0, 100.0);
  physicsGroup.addProperty(wiggleAmpProp);

  PropertyGroup motionGroup(QStringLiteral("Motion"));
  auto motionEnabledProp =
      makeProp(QStringLiteral("motion.enabled"), PropertyType::Boolean,
               impl_->motionDynamicsEnabled_, -92);
  motionEnabledProp->setDisplayLabel(QStringLiteral("Enable"));
  motionEnabledProp->setTooltip(
      QStringLiteral("Use animation dynamics for transform follow-through."));
  motionGroup.addProperty(motionEnabledProp);

  auto motionModeProp =
      makeProp(QStringLiteral("motion.mode"), PropertyType::Integer,
               impl_->motionDynamicsMode_, -91);
  motionModeProp->setDisplayLabel(QStringLiteral("Mode"));
  motionModeProp->setTooltip(
      QStringLiteral("0=Off, 1=Spring, 2=LagFollow. Choose the follow-through model."));
  motionGroup.addProperty(motionModeProp);

  auto motionStiffnessProp =
      makeProp(QStringLiteral("motion.stiffness"), PropertyType::Float,
               static_cast<double>(impl_->motionDynamicsStiffness_), -90);
  motionStiffnessProp->setHardRange(0.0, 1000.0);
  motionStiffnessProp->setSoftRange(0.0, 500.0);
  motionStiffnessProp->setDisplayLabel(QStringLiteral("Stiffness"));
  motionStiffnessProp->setTooltip(
      QStringLiteral("Spring strength used by the follow-through solver."));
  motionGroup.addProperty(motionStiffnessProp);

  auto motionDampingProp =
      makeProp(QStringLiteral("motion.damping"), PropertyType::Float,
               static_cast<double>(impl_->motionDynamicsDamping_), -89);
  motionDampingProp->setHardRange(0.0, 100.0);
  motionDampingProp->setSoftRange(0.0, 50.0);
  motionDampingProp->setDisplayLabel(QStringLiteral("Damping"));
  motionDampingProp->setTooltip(
      QStringLiteral("Energy loss per step. Higher values settle faster."));
  motionGroup.addProperty(motionDampingProp);

  auto motionMassProp =
      makeProp(QStringLiteral("motion.mass"), PropertyType::Float,
               static_cast<double>(impl_->motionDynamicsMass_), -88);
  motionMassProp->setHardRange(0.1, 100.0);
  motionMassProp->setSoftRange(0.1, 10.0);
  motionMassProp->setDisplayLabel(QStringLiteral("Mass"));
  motionMassProp->setTooltip(
      QStringLiteral("Mass used by the follow-through response."));
  motionGroup.addProperty(motionMassProp);

  auto motionLagTauProp =
      makeProp(QStringLiteral("motion.lagTau"), PropertyType::Float,
               static_cast<double>(impl_->motionDynamicsLagTau_), -87);
  motionLagTauProp->setHardRange(0.001, 10.0);
  motionLagTauProp->setSoftRange(0.01, 1.0);
  motionLagTauProp->setDisplayLabel(QStringLiteral("Lag Tau"));
  motionLagTauProp->setTooltip(
      QStringLiteral("Time constant used by the lag-follow mode."));
  motionLagTauProp->setInlineHelp(QStringLiteral("Bigger = slower, floatier follow."));
  motionLagTauProp->setWhatsThis(QStringLiteral("How sluggishly the layer follows in lag-follow mode.\nSmall values stick tightly; large values trail behind softly.\nPair with Clamp Overshoot to keep the trailing from swinging too far."));
  motionGroup.addProperty(motionLagTauProp);

  auto motionClampOvershootProp =
      makeProp(QStringLiteral("motion.clampOvershoot"), PropertyType::Boolean,
               impl_->motionDynamicsClampOvershoot_, -86);
  motionClampOvershootProp->setDisplayLabel(QStringLiteral("Clamp Overshoot"));
  motionClampOvershootProp->setTooltip(
      QStringLiteral("Keep the solver from overshooting too far."));
  motionClampOvershootProp->setInlineHelp(QStringLiteral("Caps the swing past the target."));
  motionClampOvershootProp->setWhatsThis(QStringLiteral("Limits how far the follow-through may swing past its target.\nOff allows free overshoot (springier, wilder); on caps it at Overshoot Limit."));
  motionGroup.addProperty(motionClampOvershootProp);

  auto motionOvershootLimitProp =
      makeProp(QStringLiteral("motion.overshootLimit"), PropertyType::Float,
               static_cast<double>(impl_->motionDynamicsOvershootLimit_), -85);
  motionOvershootLimitProp->setHardRange(0.0, 2.0);
  motionOvershootLimitProp->setSoftRange(0.0, 1.0);
  motionOvershootLimitProp->setDisplayLabel(QStringLiteral("Overshoot Limit"));
  motionOvershootLimitProp->setTooltip(
      QStringLiteral("Maximum overshoot ratio when clamping is enabled."));
  motionGroup.addProperty(motionOvershootLimitProp);

  PropertyGroup fractureGroup(QStringLiteral("Fracture"));
  auto fractureEnabledProp =
      makeProp(QStringLiteral("fracture.enabled"), PropertyType::Boolean,
               impl_->fractureEnabled_, -84);
  fractureEnabledProp->setDisplayLabel(QStringLiteral("Enable"));
  fractureEnabledProp->setTooltip(
      QStringLiteral("Enable fracture overlay, shard motion, and crack state."));
  fractureGroup.addProperty(fractureEnabledProp);

  auto fracturePreGenerateProp =
      makeProp(QStringLiteral("fracture.preGenerate"), PropertyType::Boolean,
               impl_->fracturePreGenerate_, -835);
  fracturePreGenerateProp->setDisplayLabel(QStringLiteral("Pre-generate Shards"));
  fracturePreGenerateProp->setTooltip(
      QStringLiteral("Prepare deterministic shard geometry before an impact so downstream components can use it."));
  fractureGroup.addProperty(fracturePreGenerateProp);

  auto fractureTriggerFrameProp = makeProp(
      QStringLiteral("fracture.triggerFrame"), PropertyType::Integer,
      static_cast<qint64>(impl_->fractureTriggerFrame_), -834);
  fractureTriggerFrameProp->setDisplayLabel(QStringLiteral("Trigger Frame"));
  fractureTriggerFrameProp->setTooltip(
      QStringLiteral("Start fracture at this composition frame. -1 disables the automatic trigger."));
  fractureTriggerFrameProp->setInlineHelp(QStringLiteral("-1 = break manually only."));
  fractureTriggerFrameProp->setWhatsThis(QStringLiteral("Composition frame where the layer breaks on its own.\n-1 disables the automatic trigger.\nPre-generate Shards first if downstream components need the pieces early."));
  fractureTriggerFrameProp->setHardRange(-1.0, 1000000.0);
  fractureGroup.addProperty(fractureTriggerFrameProp);

  auto fracturePresetProp =
      makeProp(QStringLiteral("fracture.preset"), PropertyType::Integer,
               impl_->fracturePreset_, -83);
  fracturePresetProp->setDisplayLabel(QStringLiteral("Profile"));
  fracturePresetProp->setTooltip(
      QStringLiteral("Base fracture profile. 0=Glass, 1=Concrete, 2=Stone, 3=Metal, 4=Wood, 5=Dust. Fine-tune the threshold and shard controls below."));
  fracturePresetProp->setHardRange(0.0, 5.0);
  fracturePresetProp->setSoftRange(0.0, 5.0);
  fractureGroup.addProperty(fracturePresetProp);

  auto fractureCrackThresholdProp =
      makeProp(QStringLiteral("fracture.crackThreshold"), PropertyType::Float,
               static_cast<double>(impl_->fractureCrackThreshold_), -82);
  fractureCrackThresholdProp->setHardRange(0.0, 1000.0);
  fractureCrackThresholdProp->setSoftRange(0.0, 100.0);
  fractureCrackThresholdProp->setDisplayLabel(QStringLiteral("Crack Threshold"));
  fractureCrackThresholdProp->setTooltip(
      QStringLiteral("Damage required before the layer enters a cracked state."));
  fractureGroup.addProperty(fractureCrackThresholdProp);

  auto fractureShatterThresholdProp =
      makeProp(QStringLiteral("fracture.shatterThreshold"), PropertyType::Float,
               static_cast<double>(impl_->fractureShatterThreshold_), -81);
  fractureShatterThresholdProp->setHardRange(0.0, 1000.0);
  fractureShatterThresholdProp->setSoftRange(0.0, 200.0);
  fractureShatterThresholdProp->setDisplayLabel(QStringLiteral("Shatter Threshold"));
  fractureShatterThresholdProp->setTooltip(
      QStringLiteral("Damage required before the layer spawns fracture shards."));
  fractureGroup.addProperty(fractureShatterThresholdProp);

  auto fractureShardCountProp =
      makeProp(QStringLiteral("fracture.shardCount"), PropertyType::Integer,
               impl_->fractureShardCount_, -80);
  fractureShardCountProp->setHardRange(1.0, 256.0);
  fractureShardCountProp->setSoftRange(4.0, 64.0);
  fractureShardCountProp->setDisplayLabel(QStringLiteral("Shard Count"));
  fractureShardCountProp->setTooltip(
      QStringLiteral("Number of shards spawned when the layer fractures."));
  fractureGroup.addProperty(fractureShardCountProp);

  auto fractureShardDampingProp =
      makeProp(QStringLiteral("fracture.shardDamping"), PropertyType::Float,
               static_cast<double>(impl_->fractureShardDamping_), -79);
  fractureShardDampingProp->setHardRange(0.0, 1.0);
  fractureShardDampingProp->setSoftRange(0.0, 1.0);
  fractureShardDampingProp->setDisplayLabel(QStringLiteral("Shard Damping"));
  fractureShardDampingProp->setTooltip(
      QStringLiteral("How quickly the spawned shards lose momentum."));
  fractureGroup.addProperty(fractureShardDampingProp);

  auto fractureShardGravityProp =
      makeProp(QStringLiteral("fracture.shardGravity"), PropertyType::Float,
               static_cast<double>(impl_->fractureShardGravity_), -78);
  fractureShardGravityProp->setUnit(QStringLiteral("px/s^2"));
  fractureShardGravityProp->setHardRange(-5000.0, 5000.0);
  fractureShardGravityProp->setSoftRange(-2000.0, 2000.0);
  fractureShardGravityProp->setDisplayLabel(QStringLiteral("Shard Gravity"));
  fractureShardGravityProp->setTooltip(
      QStringLiteral("Vertical gravity applied to fracture shards."));
  fractureGroup.addProperty(fractureShardGravityProp);

  auto fractureImpactSensitivityProp =
      makeProp(QStringLiteral("fracture.impactSensitivity"), PropertyType::Float,
               static_cast<double>(impl_->fractureImpactSensitivity_), -77);
  fractureImpactSensitivityProp->setHardRange(0.0, 10.0);
  fractureImpactSensitivityProp->setSoftRange(0.0, 2.0);
  fractureImpactSensitivityProp->setDisplayLabel(QStringLiteral("Impact Sensitivity"));
  fractureImpactSensitivityProp->setTooltip(
      QStringLiteral("How strongly incoming impacts contribute to fracture damage."));
  fractureGroup.addProperty(fractureImpactSensitivityProp);

  PropertyGroup trailGroup(QStringLiteral("Trail"));
  auto trailEnabledProp = makeProp(QStringLiteral("trail.enabled"),
      PropertyType::Boolean, impl_->motionTrailEnabled_, -76);
  trailEnabledProp->setDisplayLabel(QStringLiteral("Enable"));
  trailGroup.addProperty(trailEnabledProp);
  auto trailLengthProp = makeProp(QStringLiteral("trail.length"),
      PropertyType::Integer, impl_->motionTrailLength_, -75);
  trailLengthProp->setDisplayLabel(QStringLiteral("Length"));
  trailLengthProp->setHardRange(2.0, 256.0);
  trailLengthProp->setSoftRange(4.0, 64.0);
  trailGroup.addProperty(trailLengthProp);
  auto trailFadeProp = makeProp(QStringLiteral("trail.fade"),
      PropertyType::Float, static_cast<double>(impl_->motionTrailFade_), -74);
  trailFadeProp->setDisplayLabel(QStringLiteral("Fade"));
  trailFadeProp->setHardRange(0.0, 1.0);
  trailFadeProp->setSoftRange(0.0, 1.0);
  trailGroup.addProperty(trailFadeProp);
  auto trailWidthProp = makeProp(QStringLiteral("trail.width"),
      PropertyType::Float, static_cast<double>(impl_->motionTrailWidth_), -73);
  trailWidthProp->setDisplayLabel(QStringLiteral("Width"));
  trailWidthProp->setUnit(QStringLiteral("px"));
  trailWidthProp->setHardRange(0.1, 128.0);
  trailWidthProp->setSoftRange(0.5, 16.0);
  trailGroup.addProperty(trailWidthProp);

  PropertyGroup fragmentAppearanceGroup(QStringLiteral("Fragment Appearance"));
  auto velocityStretchEnabledProp = makeProp(
      QStringLiteral("fragment.velocityStretch.enabled"), PropertyType::Boolean,
      impl_->fragmentVelocityStretchEnabled_, -72);
  velocityStretchEnabledProp->setDisplayLabel(QStringLiteral("Velocity Stretch"));
  fragmentAppearanceGroup.addProperty(velocityStretchEnabledProp);
  auto velocityStretchStrengthProp = makeProp(
      QStringLiteral("fragment.velocityStretch.strength"), PropertyType::Float,
      static_cast<double>(impl_->fragmentVelocityStretchStrength_), -71);
  velocityStretchStrengthProp->setDisplayLabel(QStringLiteral("Stretch Strength"));
  velocityStretchStrengthProp->setHardRange(0.0, 1.0);
  velocityStretchStrengthProp->setSoftRange(0.0, 0.1);
  fragmentAppearanceGroup.addProperty(velocityStretchStrengthProp);
  auto velocityStretchMaxProp = makeProp(
      QStringLiteral("fragment.velocityStretch.max"), PropertyType::Float,
      static_cast<double>(impl_->fragmentVelocityStretchMax_), -70);
  velocityStretchMaxProp->setDisplayLabel(QStringLiteral("Max Stretch"));
  velocityStretchMaxProp->setHardRange(1.0, 32.0);
  velocityStretchMaxProp->setSoftRange(1.0, 8.0);
  fragmentAppearanceGroup.addProperty(velocityStretchMaxProp);
  auto colorVariationEnabledProp = makeProp(
      QStringLiteral("fragment.colorVariation.enabled"), PropertyType::Boolean,
      impl_->fragmentColorVariationEnabled_, -69);
  colorVariationEnabledProp->setDisplayLabel(QStringLiteral("Color Variation"));
  fragmentAppearanceGroup.addProperty(colorVariationEnabledProp);
  auto colorVariationProp = makeProp(
      QStringLiteral("fragment.colorVariation.amount"), PropertyType::Float,
      static_cast<double>(impl_->fragmentColorVariation_), -68);
  colorVariationProp->setDisplayLabel(QStringLiteral("Variation Amount"));
  colorVariationProp->setHardRange(0.0, 1.0);
  colorVariationProp->setSoftRange(0.0, 1.0);
  fragmentAppearanceGroup.addProperty(colorVariationProp);
  auto fragmentClonerOutputEnabledProp = makeProp(
      QStringLiteral("fragment.clonerOutput.enabled"), PropertyType::Boolean,
      impl_->fragmentClonerOutputEnabled_, -67);
  fragmentClonerOutputEnabledProp->setDisplayLabel(
      QStringLiteral("Fragment Cloner Output"));
  fragmentAppearanceGroup.addProperty(fragmentClonerOutputEnabledProp);
  auto fragmentClonerOutputCountProp = makeProp(
      QStringLiteral("fragment.clonerOutput.count"), PropertyType::Integer,
      impl_->fragmentClonerOutputCount_, -66);
  fragmentClonerOutputCountProp->setDisplayLabel(QStringLiteral("Clone Count"));
  fragmentClonerOutputCountProp->setHardRange(1.0, 256.0);
  fragmentClonerOutputCountProp->setSoftRange(1.0, 32.0);
  fragmentAppearanceGroup.addProperty(fragmentClonerOutputCountProp);
  auto fragmentClonerOutputSpacingXProp = makeProp(
      QStringLiteral("fragment.clonerOutput.spacingX"), PropertyType::Float,
      static_cast<double>(impl_->fragmentClonerOutputSpacingX_), -65);
  fragmentClonerOutputSpacingXProp->setDisplayLabel(QStringLiteral("Clone Spacing X"));
  fragmentClonerOutputSpacingXProp->setUnit(QStringLiteral("px"));
  fragmentAppearanceGroup.addProperty(fragmentClonerOutputSpacingXProp);
  auto fragmentClonerOutputSpacingYProp = makeProp(
      QStringLiteral("fragment.clonerOutput.spacingY"), PropertyType::Float,
      static_cast<double>(impl_->fragmentClonerOutputSpacingY_), -64);
  fragmentClonerOutputSpacingYProp->setDisplayLabel(QStringLiteral("Clone Spacing Y"));
  fragmentClonerOutputSpacingYProp->setUnit(QStringLiteral("px"));
  fragmentAppearanceGroup.addProperty(fragmentClonerOutputSpacingYProp);
  auto fragmentClonerOutputTimeOffsetProp = makeProp(
      QStringLiteral("fragment.clonerOutput.timeOffsetFrames"), PropertyType::Float,
      static_cast<double>(impl_->fragmentClonerOutputTimeOffsetFrames_), -63);
  fragmentClonerOutputTimeOffsetProp->setDisplayLabel(
      QStringLiteral("Time Offset"));
  fragmentClonerOutputTimeOffsetProp->setUnit(QStringLiteral("frames"));
  fragmentClonerOutputTimeOffsetProp->setHardRange(-10000.0, 10000.0);
  fragmentClonerOutputTimeOffsetProp->setSoftRange(-60.0, 60.0);
  fragmentAppearanceGroup.addProperty(fragmentClonerOutputTimeOffsetProp);

  // Component-specific groups are built by getComponentPropertyGroups().
  // Keeping them out of this path avoids constructing and then discarding a
  // large property graph for the normal layer inspector.
  auto isAdjustmentProp =
      makeProp(QStringLiteral("layer.isAdjustment"), PropertyType::Boolean,
               isAdjustmentLayer(), -50);
  isAdjustmentProp->setTooltip(
      QStringLiteral("Apply effects to all layers below"));
  isAdjustmentProp->setInlineHelp(QStringLiteral("Effects here apply to layers below."));
  isAdjustmentProp->setWhatsThis(QStringLiteral("Turns this layer into an adjustment layer.\nIts effects apply to every visible layer underneath it."));
  layerGroup.addProperty(isAdjustmentProp);

  std::vector<PropertyGroup> groups;
  groups.reserve(9 + static_cast<std::size_t>(maskCount()));
  groups.push_back(std::move(initialGroup));
  groups.push_back(std::move(transformGroup));
  groups.push_back(std::move(physicsGroup));
  groups.push_back(std::move(motionGroup));
  groups.push_back(std::move(fractureGroup));
  groups.push_back(std::move(trailGroup));
  groups.push_back(std::move(fragmentAppearanceGroup));
  for (std::size_t layerIndex = 0;
       layerIndex < impl_->animationLayers_.layerCount(); ++layerIndex) {
    const auto &animationLayer = impl_->animationLayers_.layer(layerIndex);
    PropertyGroup animationGroup(
        QStringLiteral("Animation Layer %1").arg(static_cast<int>(layerIndex + 1)));
    const QString prefix = QStringLiteral("animationLayers.%1").arg(
        static_cast<int>(layerIndex));
    auto weight = makeProp(prefix + QStringLiteral(".weight"), PropertyType::Float,
                            static_cast<double>(animationLayer.state.weight), -130);
    weight->setDisplayLabel(QStringLiteral("Weight"));
    weight->setHardRange(0.0, 1.0);
    weight->setSoftRange(0.0, 1.0);
    weight->setStep(0.01);
    animationGroup.addProperty(weight);
    // Do not instantiate generic interpolation merely to label an inspector
    // field. Frame evaluation stays in the renderer and animation editor.
    auto value = makeProp(prefix + QStringLiteral(".value"), PropertyType::Float,
                          static_cast<double>(animationLayer.values.current()), -131);
    value->setDisplayLabel(QStringLiteral("Value"));
    value->setSoftRange(0.0, 1.0);
    value->setStep(0.01);
    animationGroup.addProperty(value);
    auto interpolation = makeProp(
        prefix + QStringLiteral(".interpolation"), PropertyType::Integer,
        static_cast<int>(animationLayer.values.getKeyFrameInterpolationAt(
            FramePosition(currentFrame()))), -132);
    interpolation->setDisplayLabel(QStringLiteral("Interpolation"));
    interpolation->setHardRange(0.0, 32.0);
    interpolation->setStep(1.0);
    animationGroup.addProperty(interpolation);
    auto muted = makeProp(prefix + QStringLiteral(".muted"), PropertyType::Boolean,
                          animationLayer.state.muted, -129);
    muted->setDisplayLabel(QStringLiteral("Mute"));
    animationGroup.addProperty(muted);
    auto solo = makeProp(prefix + QStringLiteral(".solo"), PropertyType::Boolean,
                         animationLayer.state.solo, -128);
    solo->setDisplayLabel(QStringLiteral("Solo"));
    animationGroup.addProperty(solo);
    auto mode = makeProp(prefix + QStringLiteral(".blendMode"), PropertyType::Integer,
                         animationLayer.state.blendMode ==
                                 ArtifactCore::AnimationLayerBlendMode::Override
                             ? 1
                             : 0,
                         -127);
    mode->setDisplayLabel(QStringLiteral("Blend Mode (0 Additive / 1 Override)"));
    mode->setHardRange(0.0, 1.0);
    mode->setStep(1.0);
    animationGroup.addProperty(mode);
    groups.push_back(std::move(animationGroup));
  }
  for (auto propertyIt = impl_->animationPropertyLayers_.constBegin();
       propertyIt != impl_->animationPropertyLayers_.constEnd(); ++propertyIt) {
    const QString propertyPath = propertyIt.key();
    const auto &propertyStack = propertyIt.value();
    for (std::size_t layerIndex = 0; layerIndex < propertyStack.layerCount(); ++layerIndex) {
      const auto &animationLayer = propertyStack.layer(layerIndex);
      PropertyGroup animationGroup(
          QStringLiteral("Anim: %1 / Layer %2")
              .arg(propertyPath)
              .arg(static_cast<int>(layerIndex + 1)));
      const QString prefix = QStringLiteral("animationLayers.%1.%2").arg(
          propertyPath, QString::number(static_cast<int>(layerIndex)));
      auto weight = makeProp(prefix + QStringLiteral(".weight"), PropertyType::Float,
                              static_cast<double>(animationLayer.state.weight), -120);
      weight->setDisplayLabel(QStringLiteral("Weight"));
      weight->setHardRange(0.0, 1.0);
      weight->setSoftRange(0.0, 1.0);
      weight->setStep(0.01);
      animationGroup.addProperty(weight);
      auto value = makeProp(prefix + QStringLiteral(".value"), PropertyType::Float,
                            static_cast<double>(animationLayer.values.current()), -121);
      value->setDisplayLabel(QStringLiteral("Value"));
      value->setStep(0.01);
      animationGroup.addProperty(value);
      auto interpolation = makeProp(
          prefix + QStringLiteral(".interpolation"), PropertyType::Integer,
          static_cast<int>(animationLayer.values.getKeyFrameInterpolationAt(
              FramePosition(currentFrame()))), -122);
      interpolation->setDisplayLabel(QStringLiteral("Interpolation"));
      interpolation->setHardRange(0.0, 32.0);
      interpolation->setStep(1.0);
      animationGroup.addProperty(interpolation);
      auto muted = makeProp(prefix + QStringLiteral(".muted"), PropertyType::Boolean,
                            animationLayer.state.muted, -119);
      muted->setDisplayLabel(QStringLiteral("Mute"));
      animationGroup.addProperty(muted);
      auto solo = makeProp(prefix + QStringLiteral(".solo"), PropertyType::Boolean,
                           animationLayer.state.solo, -118);
      solo->setDisplayLabel(QStringLiteral("Solo"));
      animationGroup.addProperty(solo);
      auto mode = makeProp(prefix + QStringLiteral(".blendMode"), PropertyType::Integer,
                           animationLayer.state.blendMode ==
                                   ArtifactCore::AnimationLayerBlendMode::Override
                               ? 1
                               : 0,
                           -117);
      mode->setDisplayLabel(QStringLiteral("Blend Mode (0 Additive / 1 Override)"));
      mode->setHardRange(0.0, 1.0);
      mode->setStep(1.0);
      animationGroup.addProperty(mode);
      groups.push_back(std::move(animationGroup));
    }
  }
  groups.push_back(std::move(layerGroup));
  appendMaskPropertyGroups(groups);
  return groups;
}

std::vector<ArtifactCore::PropertyGroup> ArtifactAbstractLayer::getComponentPropertyGroups() const {
  std::vector<ArtifactCore::PropertyGroup> groups;
  groups.reserve(16);
  auto makeProp = [this](const QString& name, PropertyType type, const QVariant& value, int prio) {
    return persistentLayerProperty(name, type, value, prio);
  };
  PropertyGroup componentGroup(QStringLiteral("Components"));
  componentGroup.addProperty(makeProp(QStringLiteral("component.script.enabled"), PropertyType::Boolean, impl_->scriptComponentEnabled_, -100));
  componentGroup.addProperty(makeProp(QStringLiteral("component.cloner.enabled"), PropertyType::Boolean, impl_->clonerComponentEnabled_, -90));
  componentGroup.addProperty(makeProp(QStringLiteral("component.collision.enabled"), PropertyType::Boolean, impl_->collisionComponentEnabled_, -89));
  componentGroup.addProperty(makeProp(QStringLiteral("component.collision.displayColor"), PropertyType::Color, impl_->collisionDisplayColor_, -88));
  componentGroup.addProperty(makeProp(QStringLiteral("component.crowd.enabled"), PropertyType::Boolean, impl_->crowdComponentEnabled_, -88));
  componentGroup.addProperty(makeProp(QStringLiteral("component.particleEmitter.enabled"), PropertyType::Boolean, impl_->particleEmitterComponentEnabled_, -87));
  componentGroup.addProperty(makeProp(QStringLiteral("component.fluid.enabled"), PropertyType::Boolean, impl_->fluidComponentEnabled_, -86));
  groups.push_back(std::move(componentGroup));
  return groups;
}

namespace {
SharedPtr<ArtifactCore::AbstractProperty> transformChannelProperty(
    const ArtifactCore::AnimatableTransform3D& transform, const QString& path) {
  if (path == QStringLiteral("transform.position.x"))
    return transform.channelProperty(ArtifactCore::TransformChannel::PositionX);
  if (path == QStringLiteral("transform.position.y"))
    return transform.channelProperty(ArtifactCore::TransformChannel::PositionY);
  if (path == QStringLiteral("transform.position.z"))
    return transform.channelProperty(ArtifactCore::TransformChannel::PositionZ);
  if (path == QStringLiteral("transform.rotation"))
    return transform.channelProperty(ArtifactCore::TransformChannel::Rotation);
  if (path == QStringLiteral("transform.rotation.z"))
    return transform.channelProperty(ArtifactCore::TransformChannel::Rotation);
  if (path == QStringLiteral("transform.rotation.x"))
    return transform.channelProperty(ArtifactCore::TransformChannel::RotationX);
  if (path == QStringLiteral("transform.rotation.y"))
    return transform.channelProperty(ArtifactCore::TransformChannel::RotationY);
  if (path == QStringLiteral("transform.scale.x"))
    return transform.channelProperty(ArtifactCore::TransformChannel::ScaleX);
  if (path == QStringLiteral("transform.scale.y"))
    return transform.channelProperty(ArtifactCore::TransformChannel::ScaleY);
  if (path == QStringLiteral("transform.scale.z"))
    return transform.channelProperty(ArtifactCore::TransformChannel::ScaleZ);
  if (path == QStringLiteral("transform.anchor.x"))
    return transform.channelProperty(ArtifactCore::TransformChannel::AnchorX);
  if (path == QStringLiteral("transform.anchor.y"))
    return transform.channelProperty(ArtifactCore::TransformChannel::AnchorY);
  if (path == QStringLiteral("transform.anchor.z"))
    return transform.channelProperty(ArtifactCore::TransformChannel::AnchorZ);
  return {};
}
}

SharedPtr<ArtifactCore::AbstractProperty>
ArtifactAbstractLayer::getProperty(const QString &name) const {
  if (auto channel = transformChannelProperty(transform3D(), name)) return channel;
  std::lock_guard<std::mutex> lock(impl_->propertyCacheMutex_);
  auto &cache = impl_->propertyCache_;
  auto it = cache.find(name);
  if (it != cache.end()) {
    return it.value();
  }
  return nullptr;
}

SharedPtr<ArtifactCore::AbstractProperty>
ArtifactAbstractLayer::persistentLayerProperty(const QString &propertyPath,
                                               PropertyType type,
                                               const QVariant &value,
                                               int priority) const {
  std::lock_guard<std::mutex> lock(impl_->propertyCacheMutex_);
  auto &cache = impl_->propertyCache_;
  const auto channel = transformChannelProperty(transform3D(), propertyPath);
  if (channel) cache.insert(propertyPath, channel);
  auto it = cache.find(propertyPath);
  if (it == cache.end() || !it.value()) {
    it = cache.insert(propertyPath, makeShared<AbstractProperty>());
  }
  auto property = it.value();
  const bool hasAnimatedValue =
      property->isAnimatable() && !property->getKeyFrames().empty();
  property->setName(propertyPath);
  property->setType(type);
  if (!channel && !hasAnimatedValue && !property->hasExpression()) {
    property->setValue(value);
  }
  property->setDisplayPriority(priority);
  if (type == PropertyType::Integer) {
    property->setStep(1);
  }
  return property;
}

void ArtifactAbstractLayer::removePersistentLayerPropertiesWithPrefix(
    const QString &propertyPathPrefix) const {
  if (propertyPathPrefix.isEmpty()) return;
  std::lock_guard<std::mutex> lock(impl_->propertyCacheMutex_);
  auto &cache = impl_->propertyCache_;
  for (auto it = cache.begin(); it != cache.end();) {
    if (it.key().startsWith(propertyPathPrefix)) {
      it = cache.erase(it);
    } else {
      ++it;
    }
  }
}

bool ArtifactAbstractLayer::setLayerPropertyValue(const QString &propertyPath,
                                                  const QVariant &value) {
  const QStringList animationParts = propertyPath.split(QLatin1Char('.'));
  if (animationParts.size() >= 4 &&
      animationParts[0] == QStringLiteral("animationLayers")) {
    bool indexOk = false;
    const int layerIndex = animationParts[animationParts.size() - 2].toInt(&indexOk);
    const QString field = animationParts.back();
    const QString animationPropertyPath =
        animationParts.mid(1, animationParts.size() - 3).join(QLatin1Char('.'));
    auto stackIt = impl_->animationPropertyLayers_.find(animationPropertyPath);
    if (indexOk && stackIt != impl_->animationPropertyLayers_.end() &&
        layerIndex >= 0 &&
        static_cast<std::size_t>(layerIndex) < stackIt->layerCount()) {
      auto &state = stackIt->layer(static_cast<std::size_t>(layerIndex)).state;
      auto &animationLayer = stackIt->layer(static_cast<std::size_t>(layerIndex));
      if (field == QStringLiteral("value")) {
        const double rawValue = value.toDouble();
        if (!std::isfinite(rawValue)) {
          return false;
        }
        animationLayer.values.addKeyFrame(
            FramePosition(currentFrame()), static_cast<float>(rawValue));
      } else if (field == QStringLiteral("interpolation")) {
        animationLayer.values.setKeyFrameInterpolationAt(
            FramePosition(currentFrame()),
            static_cast<ArtifactCore::InterpolationType>(
                std::clamp(value.toInt(), 0, 32)));
      } else if (field == QStringLiteral("weight")) {
        const double rawWeight = value.toDouble();
        const double fallbackWeight = std::isfinite(state.weight)
                                          ? std::clamp<double>(state.weight, 0.0,
                                                               1.0)
                                          : 1.0;
        state.weight = static_cast<float>(
            std::isfinite(rawWeight)
                ? std::clamp(rawWeight, 0.0, 1.0)
                : fallbackWeight);
      } else if (field == QStringLiteral("muted")) {
        state.muted = value.toBool();
      } else if (field == QStringLiteral("solo")) {
        state.solo = value.toBool();
      } else if (field == QStringLiteral("blendMode")) {
        state.blendMode = value.toInt() == 1
                              ? ArtifactCore::AnimationLayerBlendMode::Override
                              : ArtifactCore::AnimationLayerBlendMode::Additive;
      } else {
        return false;
      }
      notifyLayerMutation(this, LayerDirtyFlag::Property,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    return false;
  }
  if (animationParts.size() == 3 && animationParts[0] == QStringLiteral("animationLayers")) {
    bool ok = false;
    const int layerIndex = animationParts[1].toInt(&ok);
    if (ok && layerIndex >= 0 &&
        static_cast<std::size_t>(layerIndex) < impl_->animationLayers_.layerCount()) {
      auto &state = impl_->animationLayers_.layer(static_cast<std::size_t>(layerIndex)).state;
      auto &animationLayer = impl_->animationLayers_.layer(static_cast<std::size_t>(layerIndex));
      if (animationParts[2] == QStringLiteral("value")) {
        const double rawValue = value.toDouble();
        if (!std::isfinite(rawValue)) {
          return false;
        }
        animationLayer.values.addKeyFrame(
            FramePosition(currentFrame()), static_cast<float>(rawValue));
      } else if (animationParts[2] == QStringLiteral("interpolation")) {
        animationLayer.values.setKeyFrameInterpolationAt(
            FramePosition(currentFrame()),
            static_cast<ArtifactCore::InterpolationType>(
                std::clamp(value.toInt(), 0, 32)));
      } else if (animationParts[2] == QStringLiteral("weight")) {
        const double rawWeight = value.toDouble();
        const double fallbackWeight = std::isfinite(state.weight)
                                          ? std::clamp<double>(state.weight, 0.0,
                                                               1.0)
                                          : 1.0;
        state.weight = static_cast<float>(
            std::isfinite(rawWeight)
                ? std::clamp(rawWeight, 0.0, 1.0)
                : fallbackWeight);
      } else if (animationParts[2] == QStringLiteral("muted")) {
        state.muted = value.toBool();
      } else if (animationParts[2] == QStringLiteral("solo")) {
        state.solo = value.toBool();
      } else if (animationParts[2] == QStringLiteral("blendMode")) {
        state.blendMode = value.toInt() == 1
                              ? ArtifactCore::AnimationLayerBlendMode::Override
                              : ArtifactCore::AnimationLayerBlendMode::Additive;
      } else {
        return false;
      }
      notifyLayerMutation(this, LayerDirtyFlag::Property,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    return false;
  }
  if (propertyPath == QStringLiteral("layer.name")) {
    setLayerName(value.toString());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.note")) {
    setLayerNote(value.toString());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.parent")) {
    const ArtifactCore::LayerID parentId(value.toString());
    if (parentId.isNil()) {
      clearParent();
    } else {
      setParentById(parentId);
    }
    return true;
  }
  if (propertyPath == QStringLiteral("layer.visible")) {
    setVisible(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.locked")) {
    setLocked(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.selectionLocked")) {
    setSelectionLocked(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.transformLocked")) {
    setTransformLocked(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.timingLocked")) {
    setTimingLocked(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.guide")) {
    setGuide(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.solo")) {
    setSolo(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.cachePolicy")) {
    setLayerCachePolicy(static_cast<LayerCachePolicy>(value.toInt()));
    return true;
  }
  if (propertyPath == QStringLiteral("layer.shy")) {
    setShy(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.labelColorIndex")) {
    setLabelColorIndex(value.toInt());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.is3D")) {
    setIs3D(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("projection.enabled")) {
    setProjectionEnabled(value.toBool());
    return true;
  }
  if (propertyPath == QStringLiteral("projection.sourceLayerId")) {
    setProjectionSourceLayerId(value.toString());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.2_5d.enabled")) {
    impl_->twoPointFiveDEnabled_ = value.toBool();
    return true;
  }
  if (propertyPath == QStringLiteral("layer.2_5d.depth")) {
    impl_->twoPointFiveDDepth_ = static_cast<float>(value.toDouble());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.2_5d.cameraDistance")) {
    impl_->twoPointFiveDCameraDistance_ = std::clamp(static_cast<float>(value.toDouble()), 1.0f, 1000000.0f);
    return true;
  }
  if (propertyPath == QStringLiteral("layer.2_5d.depthOfFieldEnabled")) {
    impl_->twoPointFiveDDepthOfFieldEnabled_ = value.toBool();
    return true;
  }
  if (propertyPath == QStringLiteral("layer.2_5d.focusDepth")) {
    impl_->twoPointFiveDFocusDepth_ = static_cast<float>(value.toDouble());
    return true;
  }
  if (propertyPath == QStringLiteral("layer.2_5d.focusRange")) {
    impl_->twoPointFiveDFocusRange_ = std::clamp(static_cast<float>(value.toDouble()), 1.0f, 1000000.0f);
    return true;
  }
  if (propertyPath == QStringLiteral("layer.2_5d.maxBlur")) {
    impl_->twoPointFiveDMaxBlur_ = std::clamp(static_cast<float>(value.toDouble()), 0.0f, 64.0f);
    return true;
  }
  if (propertyPath == QStringLiteral("layer.2_5d.motionBlurEnabled")) {
    impl_->twoPointFiveDMotionBlurEnabled_ = value.toBool();
    return true;
  }
  if (propertyPath == QStringLiteral("layer.2_5d.motionBlurShutterAngle")) {
    impl_->twoPointFiveDMotionBlurShutterAngle_ = std::clamp(static_cast<float>(value.toDouble()), 0.0f, 720.0f);
    return true;
  }
  if (propertyPath == QStringLiteral("layer.2_5d.motionBlurSamples")) {
    impl_->twoPointFiveDMotionBlurSamples_ = std::clamp(value.toInt(), 2, 8);
    return true;
  }
  if (propertyPath == QStringLiteral("layer.isAdjustment")) {
    setAdjustmentLayer(value.toBool());
    return true;
  }

  if (propertyPath == QStringLiteral("layer.opacity")) {
    setOpacity(static_cast<float>(value.toDouble()));
    return true;
  }

  if (const auto maskAddress = parseMaskPropertyPath(propertyPath)) {
    if (maskAddress->maskIndex < 0 ||
        maskAddress->maskIndex >= impl_->maskMatteState_.maskCount()) {
      return false;
    }

    if (maskAddress->pathIndex < 0) {
      LayerMask mask = impl_->maskMatteState_.mask(maskAddress->maskIndex);
      if (maskAddress->field == QStringLiteral("enabled")) {
        mask.setEnabled(value.toBool());
        impl_->maskMatteState_.setMask(maskAddress->maskIndex, mask);
        notifyLayerMutation(this, LayerDirtyFlag::Mask,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      if (maskAddress->field == QStringLiteral("locked")) {
        mask.setLocked(value.toBool());
        impl_->maskMatteState_.setMask(maskAddress->maskIndex, mask);
        notifyLayerMutation(this, LayerDirtyFlag::Mask,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }

    LayerMask mask = impl_->maskMatteState_.mask(maskAddress->maskIndex);
    if (maskAddress->pathIndex >= mask.maskPathCount()) {
      return false;
    }

    MaskPath path = mask.maskPath(maskAddress->pathIndex);
    if (maskAddress->field == QStringLiteral("closed")) {
      path.setClosed(value.toBool());
    } else if (maskAddress->field == QStringLiteral("opacity")) {
      path.setOpacity(static_cast<float>(value.toDouble()));
    } else if (maskAddress->field == QStringLiteral("feather")) {
      path.setFeather(static_cast<float>(value.toDouble()));
    } else if (maskAddress->field == QStringLiteral("featherHorizontal")) {
      path.setFeatherHorizontal(static_cast<float>(value.toDouble()));
    } else if (maskAddress->field == QStringLiteral("featherVertical")) {
      path.setFeatherVertical(static_cast<float>(value.toDouble()));
    } else if (maskAddress->field == QStringLiteral("featherInner")) {
      path.setFeatherInner(static_cast<float>(value.toDouble()));
    } else if (maskAddress->field == QStringLiteral("featherOuter")) {
      path.setFeatherOuter(static_cast<float>(value.toDouble()));
    } else if (maskAddress->field == QStringLiteral("falloff")) {
      path.setFalloff(static_cast<MaskFeatherFalloff>(value.toInt()));
    } else if (maskAddress->field == QStringLiteral("expansion")) {
      path.setExpansion(static_cast<float>(value.toDouble()));
    } else if (maskAddress->field == QStringLiteral("inverted")) {
      path.setInverted(value.toBool());
    } else if (maskAddress->field == QStringLiteral("mode")) {
      path.setMode(static_cast<MaskMode>(value.toInt()));
    } else if (maskAddress->field == QStringLiteral("name")) {
      path.setName(UniString(value.toString().toStdString()));
    } else {
      return false;
    }
    mask.setMaskPath(maskAddress->pathIndex, path);
    impl_->maskMatteState_.setMask(maskAddress->maskIndex, mask);
    notifyLayerMutation(this, LayerDirtyFlag::Mask,
                        LayerDirtyReason::PropertyChanged);
    return true;
  }

  // Physics properties
  if (propertyPath == QStringLiteral("physics.softBody.enabled")) {
    if (value.toBool()) {
      enableSoftBodyPhysicsGrid();
    } else {
      disableSoftBodyPhysics();
    }
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.cloth3D.enabled")) {
    if (value.toBool()) {
      enableCloth3DPhysicsGrid();
    } else {
      disableCloth3DPhysics();
    }
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.material.enabled")) {
    if (value.toBool()) {
      enableMaterialPhysics(impl_->materialPhysicsPreset_);
    } else {
      disableMaterialPhysics();
    }
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.material.preset")) {
    impl_->materialPhysicsPreset_ = std::clamp(value.toInt(), 0, 3);
    if (impl_->materialPhysicsEnabled_) {
      enableMaterialPhysics(impl_->materialPhysicsPreset_);
    }
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.enabled")) {
    const bool enabled = value.toBool();
    impl_->physicsComponent_.setEnabled(enabled);
    if (enabled) {
      impl_->collisionOwnsPhysicsEnable_ = false;
    }
    impl_->syncBuiltinComponentDescriptors();
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("physics.stiffness")) {
    impl_->physicsComponent_.settings().stiffness = finiteClampedValue(
        value.toDouble(), impl_->physicsComponent_.settings().stiffness, 0.0,
        1000.0);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.damping")) {
    impl_->physicsComponent_.settings().damping = finiteClampedValue(
        value.toDouble(), impl_->physicsComponent_.settings().damping, 0.0,
        100.0);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.followThroughGain")) {
    impl_->physicsComponent_.settings().followThroughGain = finiteClampedValue(
        value.toDouble(), impl_->physicsComponent_.settings().followThroughGain,
        0.0, 2.0);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.gravityY")) {
    impl_->physicsComponent_.settings().gravityY = finiteClampedValue(
        value.toDouble(), impl_->physicsComponent_.settings().gravityY,
        -5000.0, 5000.0);
    impl_->physicsComponent_.reset();
    applyRigidBodyWorldGravity();
    if (hasSoftBodyPhysics()) {
      const auto& settings = impl_->physicsComponent_.settings();
      LayerPhysics::configureSoftBody(
          id(), 0.0f, settings.gravityY * settings.gravityScale,
          settings.linearDamping);
    }
    return true;
  }
  if (propertyPath == QStringLiteral("physics.fallProfile")) {
    const int profile = std::clamp(value.toInt(), 0, 4);
    auto& settings = impl_->physicsComponent_.settings();
    settings.fallProfile = profile;
    switch (profile) {
    case 1: // Light
      settings.gravityScale = 0.65f;
      settings.linearDamping = 0.10f;
      settings.angularDamping = 0.12f;
      break;
    case 2: // Normal
      settings.gravityScale = 1.0f;
      settings.linearDamping = 0.35f;
      settings.angularDamping = 0.35f;
      break;
    case 3: // Heavy
      settings.gravityScale = 1.8f;
      settings.linearDamping = 0.12f;
      settings.angularDamping = 0.16f;
      break;
    case 4: // Floaty
      settings.gravityScale = 0.35f;
      settings.linearDamping = 2.5f;
      settings.angularDamping = 2.0f;
      break;
    default:
      break;
    }
    impl_->physicsComponent_.reset();
    applyRigidBodyPhysicsSettings();
    if (hasSoftBodyPhysics()) {
      LayerPhysics::configureSoftBody(
          id(), 0.0f, settings.gravityY * settings.gravityScale,
          settings.linearDamping);
    }
    persistentLayerProperty(QStringLiteral("physics.gravityScale"),
                            PropertyType::Float,
                            QVariant(static_cast<double>(settings.gravityScale)),
                            -93);
    persistentLayerProperty(QStringLiteral("physics.linearDamping"),
                            PropertyType::Float,
                            QVariant(static_cast<double>(settings.linearDamping)),
                            -95);
    persistentLayerProperty(QStringLiteral("physics.angularDamping"),
                            PropertyType::Float,
                            QVariant(static_cast<double>(settings.angularDamping)),
                            -94);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.linearDamping")) {
    impl_->physicsComponent_.settings().linearDamping = finiteClampedValue(
        value.toDouble(), impl_->physicsComponent_.settings().linearDamping, 0.0,
        50.0);
    impl_->physicsComponent_.settings().fallProfile = 0;
    impl_->physicsComponent_.reset();
    applyRigidBodyPhysicsSettings();
    if (hasSoftBodyPhysics()) {
      const auto& settings = impl_->physicsComponent_.settings();
      LayerPhysics::configureSoftBody(
          id(), 0.0f, settings.gravityY * settings.gravityScale,
          settings.linearDamping);
    }
    persistentLayerProperty(QStringLiteral("physics.fallProfile"),
                            PropertyType::Integer, QVariant(0), -96);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.angularDamping")) {
    impl_->physicsComponent_.settings().angularDamping = finiteClampedValue(
        value.toDouble(), impl_->physicsComponent_.settings().angularDamping, 0.0,
        50.0);
    impl_->physicsComponent_.settings().fallProfile = 0;
    applyRigidBodyPhysicsSettings();
    persistentLayerProperty(QStringLiteral("physics.fallProfile"),
                            PropertyType::Integer, QVariant(0), -96);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.gravityScale")) {
    impl_->physicsComponent_.settings().gravityScale = finiteClampedValue(
        value.toDouble(), impl_->physicsComponent_.settings().gravityScale,
        -10.0, 10.0);
    impl_->physicsComponent_.settings().fallProfile = 0;
    impl_->physicsComponent_.reset();
    applyRigidBodyPhysicsSettings();
    if (hasSoftBodyPhysics()) {
      const auto& settings = impl_->physicsComponent_.settings();
      LayerPhysics::configureSoftBody(
          id(), 0.0f, settings.gravityY * settings.gravityScale,
          settings.linearDamping);
    }
    persistentLayerProperty(QStringLiteral("physics.fallProfile"),
                            PropertyType::Integer, QVariant(0), -96);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.wind.enabled")) {
    impl_->physicsComponent_.settings().windEnabled = value.toBool();
    impl_->lastRigidWindForceFrame_ = std::numeric_limits<int64_t>::min();
    const auto& settings = impl_->physicsComponent_.settings();
    LayerPhysics::setSoftBodyWind(
        id(), settings.windX, settings.windY,
        settings.windEnabled ? settings.windStrength : 0.0f);
    LayerPhysics::setCloth3DWind(
        id(), settings.windX, settings.windY, 0.0f,
        settings.windEnabled ? settings.windStrength : 0.0f);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.wind.x")) {
    impl_->physicsComponent_.settings().windX = finiteClampedValue(
        value.toDouble(), impl_->physicsComponent_.settings().windX, -1.0, 1.0);
    const auto& settings = impl_->physicsComponent_.settings();
    LayerPhysics::setSoftBodyWind(
        id(), settings.windX, settings.windY,
        settings.windEnabled ? settings.windStrength : 0.0f);
    LayerPhysics::setCloth3DWind(
        id(), settings.windX, settings.windY, 0.0f,
        settings.windEnabled ? settings.windStrength : 0.0f);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.wind.y")) {
    impl_->physicsComponent_.settings().windY = finiteClampedValue(
        value.toDouble(), impl_->physicsComponent_.settings().windY, -1.0, 1.0);
    const auto& settings = impl_->physicsComponent_.settings();
    LayerPhysics::setSoftBodyWind(
        id(), settings.windX, settings.windY,
        settings.windEnabled ? settings.windStrength : 0.0f);
    LayerPhysics::setCloth3DWind(
        id(), settings.windX, settings.windY, 0.0f,
        settings.windEnabled ? settings.windStrength : 0.0f);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.wind.strength")) {
    impl_->physicsComponent_.settings().windStrength = finiteClampedValue(
        value.toDouble(), impl_->physicsComponent_.settings().windStrength, 0.0, 100000.0);
    const auto& settings = impl_->physicsComponent_.settings();
    LayerPhysics::setSoftBodyWind(
        id(), settings.windX, settings.windY,
        settings.windEnabled ? settings.windStrength : 0.0f);
    LayerPhysics::setCloth3DWind(
        id(), settings.windX, settings.windY, 0.0f,
        settings.windEnabled ? settings.windStrength : 0.0f);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.wind.torque")) {
    impl_->physicsComponent_.settings().windTorque = finiteClampedValue(
        value.toDouble(), impl_->physicsComponent_.settings().windTorque, -100.0, 100.0);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.restitution")) {
    impl_->physicsComponent_.settings().restitution = finiteClampedValue(
        value.toDouble(), impl_->physicsComponent_.settings().restitution, 0.0,
        1.0);
    if (hasSoftBodyPhysics()) {
      syncSoftBodyPhysicsColliderToBounds();
    }
    if (hasRigidBodyPhysics()) {
      syncRigidBodyPhysicsToBounds();
    }
    return true;
  }
  if (propertyPath == QStringLiteral("physics.initialVelocityY")) {
    impl_->clonePhysicsInitialVelocityY_ = finiteClampedValue(
        value.toDouble(), impl_->clonePhysicsInitialVelocityY_, -5000.0,
        5000.0);
    // Keep the cached property in sync: applyClonePhysicsTiming reads this
    // path through getProperty(), and programmatic writers bypass the
    // editor's own propertyPtr->setValue() step.
    persistentLayerProperty(propertyPath, PropertyType::Float,
                            QVariant(static_cast<double>(
                                impl_->clonePhysicsInitialVelocityY_)),
                            -92);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.maxBounces")) {
    impl_->clonePhysicsMaxBounces_ = std::clamp(value.toInt(), 0, 32);
    persistentLayerProperty(propertyPath, PropertyType::Integer,
                            QVariant(impl_->clonePhysicsMaxBounces_), -91);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.wiggleFreq")) {
    impl_->physicsComponent_.settings().wiggleFreq = finiteClampedValue(
        value.toDouble(), impl_->physicsComponent_.settings().wiggleFreq, 0.0,
        60.0);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.wiggleAmp")) {
    impl_->physicsComponent_.settings().wiggleAmp = finiteClampedValue(
        value.toDouble(), impl_->physicsComponent_.settings().wiggleAmp, 0.0,
        10000.0);
    return true;
  }
  if (propertyPath == QStringLiteral("motion.enabled")) {
    impl_->motionDynamicsEnabled_ = value.toBool();
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("motion.mode")) {
    impl_->motionDynamicsMode_ = std::clamp(value.toInt(), 0, 2);
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("motion.stiffness")) {
    impl_->motionDynamicsStiffness_ = finiteClampedValue(
        value.toDouble(), impl_->motionDynamicsStiffness_, 0.0, 1000.0);
    return true;
  }
  if (propertyPath == QStringLiteral("motion.damping")) {
    impl_->motionDynamicsDamping_ = finiteClampedValue(
        value.toDouble(), impl_->motionDynamicsDamping_, 0.0, 100.0);
    return true;
  }
  if (propertyPath == QStringLiteral("motion.mass")) {
    impl_->motionDynamicsMass_ = finiteClampedValue(
        value.toDouble(), impl_->motionDynamicsMass_, 0.1, 100.0);
    return true;
  }
  if (propertyPath == QStringLiteral("motion.lagTau")) {
    impl_->motionDynamicsLagTau_ = finiteClampedValue(
        value.toDouble(), impl_->motionDynamicsLagTau_, 0.001, 10.0);
    return true;
  }
  if (propertyPath == QStringLiteral("motion.clampOvershoot")) {
    impl_->motionDynamicsClampOvershoot_ = value.toBool();
    return true;
  }
  if (propertyPath == QStringLiteral("motion.overshootLimit")) {
    impl_->motionDynamicsOvershootLimit_ = finiteClampedValue(
        value.toDouble(), impl_->motionDynamicsOvershootLimit_, 0.0, 2.0);
    return true;
  }
  if (propertyPath == QStringLiteral("trail.enabled")) {
    impl_->motionTrailEnabled_ = value.toBool();
    impl_->motionTrailHistory_.clear();
    impl_->motionTrailLastFrame_ = std::numeric_limits<int64_t>::min();
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("trail.length")) {
    impl_->motionTrailLength_ = std::clamp(value.toInt(), 2, 256);
    impl_->motionTrailHistory_.clear();
    return true;
  }
  if (propertyPath == QStringLiteral("trail.fade")) {
    impl_->motionTrailFade_ = finiteClampedValue(
        value.toDouble(), impl_->motionTrailFade_, 0.0, 1.0);
    return true;
  }
  if (propertyPath == QStringLiteral("trail.width")) {
    impl_->motionTrailWidth_ = finiteClampedValue(
        value.toDouble(), impl_->motionTrailWidth_, 0.1, 128.0);
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.velocityStretch.enabled")) {
    impl_->fragmentVelocityStretchEnabled_ = value.toBool();
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.velocityStretch.strength")) {
    impl_->fragmentVelocityStretchStrength_ = finiteClampedValue(
        value.toDouble(), impl_->fragmentVelocityStretchStrength_, 0.0, 1.0);
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.velocityStretch.max")) {
    impl_->fragmentVelocityStretchMax_ = finiteClampedValue(
        value.toDouble(), impl_->fragmentVelocityStretchMax_, 1.0, 32.0);
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.colorVariation.enabled")) {
    impl_->fragmentColorVariationEnabled_ = value.toBool();
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.colorVariation.amount")) {
    impl_->fragmentColorVariation_ = finiteClampedValue(
        value.toDouble(), impl_->fragmentColorVariation_, 0.0, 1.0);
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.clonerOutput.enabled")) {
    impl_->fragmentClonerOutputEnabled_ = value.toBool();
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.clonerOutput.count")) {
    impl_->fragmentClonerOutputCount_ = std::clamp(value.toInt(), 1, 256);
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.clonerOutput.spacingX")) {
    impl_->fragmentClonerOutputSpacingX_ = finiteClampedValue(
        value.toDouble(), impl_->fragmentClonerOutputSpacingX_, -100000.0,
        100000.0);
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.clonerOutput.spacingY")) {
    impl_->fragmentClonerOutputSpacingY_ = finiteClampedValue(
        value.toDouble(), impl_->fragmentClonerOutputSpacingY_, -100000.0,
        100000.0);
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.clonerOutput.timeOffsetFrames")) {
    impl_->fragmentClonerOutputTimeOffsetFrames_ = finiteClampedValue(
        value.toDouble(), impl_->fragmentClonerOutputTimeOffsetFrames_,
        -10000.0, 10000.0);
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.enabled")) {
    const bool enabled = value.toBool();
    if (impl_->fractureEnabled_ != enabled) {
      impl_->fractureEnabled_ = enabled;
      resetFractureState();
    }
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.preGenerate")) {
    const bool enabled = value.toBool();
    if (impl_->fracturePreGenerate_ != enabled) {
      impl_->fracturePreGenerate_ = enabled;
      resetFractureState();
    }
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.triggerFrame")) {
    const int64_t triggerFrame = std::max<int64_t>(-1, value.toLongLong());
    if (impl_->fractureTriggerFrame_ != triggerFrame) {
      impl_->fractureTriggerFrame_ = triggerFrame;
      resetFractureState();
    }
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.preset")) {
    const int preset = std::clamp(value.toInt(), 0, static_cast<int>(FracturePreset::Dust));
    if (impl_->fracturePreset_ != preset) {
      impl_->fracturePreset_ = preset;
      resetFractureState();
    }
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.crackThreshold")) {
    const float threshold = finiteClampedValue(
        value.toDouble(), impl_->fractureCrackThreshold_, 0.0, 1000.0);
    if (impl_->fractureCrackThreshold_ != threshold) {
      impl_->fractureCrackThreshold_ = threshold;
      resetFractureState();
    }
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.shatterThreshold")) {
    const float threshold = finiteClampedValue(
        value.toDouble(), impl_->fractureShatterThreshold_, 0.0, 1000.0);
    if (impl_->fractureShatterThreshold_ != threshold) {
      impl_->fractureShatterThreshold_ = threshold;
      resetFractureState();
    }
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.shardCount")) {
    const int shardCount = std::clamp(value.toInt(), 1, 256);
    if (impl_->fractureShardCount_ != shardCount) {
      impl_->fractureShardCount_ = shardCount;
      resetFractureState();
    }
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.shardDamping")) {
    const float damping = finiteClampedValue(
        value.toDouble(), impl_->fractureShardDamping_, 0.0, 1.0);
    if (impl_->fractureShardDamping_ != damping) {
      impl_->fractureShardDamping_ = damping;
      resetFractureState();
    }
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.shardGravity")) {
    const float gravity = finiteClampedValue(
        value.toDouble(), impl_->fractureShardGravity_, -5000.0, 5000.0);
    if (impl_->fractureShardGravity_ != gravity) {
      impl_->fractureShardGravity_ = gravity;
      resetFractureState();
    }
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.impactSensitivity")) {
    const float sensitivity = finiteClampedValue(
        value.toDouble(), impl_->fractureImpactSensitivity_, 0.0, 10.0);
    if (impl_->fractureImpactSensitivity_ != sensitivity) {
      impl_->fractureImpactSensitivity_ = sensitivity;
      resetFractureState();
    }
    Q_EMIT changed();
    return true;
  }

  if (propertyPath.startsWith(QStringLiteral("component."))) {
    return setComponentDescriptorPropertyValue(propertyPath, value) ||
           setComponentPhysicsPropertyValue(propertyPath, value) ||
           setComponentLayoutPropertyValue(propertyPath, value);
  }
  return setTransformTimeSourcePropertyValue(propertyPath, value);
}

bool ArtifactAbstractLayer::setComponentDescriptorPropertyValue(
    const QString &propertyPath, const QVariant &value) {
  if (propertyPath == QStringLiteral("component.script.enabled")) {
    impl_->scriptComponentEnabled_ = value.toBool();
    Q_EMIT changed();
    return true;
  }
    if (propertyPath == QStringLiteral("component.generators.add")) {
      const QString requestedType = value.toString().trimmed().toLower();
      LayerGeneratorDescriptor descriptor;
      const int nextIndex =
          static_cast<int>(impl_->extraGeneratorDescriptors_.count()) + 1;
      descriptor.generatorId =
          QStringLiteral("generator.extra.%1").arg(nextIndex);
      descriptor.enabled = true;
      descriptor.order = 1000 + nextIndex * 10;
      if (requestedType == QStringLiteral("radial")) {
        descriptor.typeId = QStringLiteral("artifact.generator.cloner.radial");
        descriptor.settings[QStringLiteral("radialCount")] = 8;
        descriptor.settings[QStringLiteral("radius")] = 160.0;
        descriptor.settings[QStringLiteral("startAngle")] = 0.0;
        descriptor.settings[QStringLiteral("endAngle")] = 360.0;
      } else {
        descriptor.typeId = requestedType == QStringLiteral("matrix")
            ? QStringLiteral("artifact.generator.cloner.matrix")
            : QStringLiteral("artifact.generator.cloner.grid");
        descriptor.settings[QStringLiteral("columns")] = 3;
        descriptor.settings[QStringLiteral("rows")] = 3;
        descriptor.settings[QStringLiteral("depth")] = 1;
        descriptor.settings[QStringLiteral("spacingX")] = 160.0;
        descriptor.settings[QStringLiteral("spacingY")] = 48.0;
        descriptor.settings[QStringLiteral("spacingZ")] = 0.0;
      }
      descriptor.settings[QStringLiteral("timeOffsetStep")] = 0.0;
      descriptor.settings[QStringLiteral("sequenceEnabled")] = false;
      descriptor.settings[QStringLiteral("sequenceRate")] = 8.0;
      descriptor.settings[QStringLiteral("sequenceSoftness")] = 1.0;
      impl_->extraGeneratorDescriptors_.add(std::move(descriptor));
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.generators.removeLast")) {
      if (impl_->extraGeneratorDescriptors_.isEmpty()) {
        return false;
      }
      impl_->extraGeneratorDescriptors_.takeLast();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.generators.remove")) {
      const QString generatorId = value.toString().trimmed();
      if (generatorId.isEmpty() ||
          generatorId == QStringLiteral("generator.compat.cloner.0")) {
        return false;
      }
      for (std::size_t i = 0; i < impl_->extraGeneratorDescriptors_.count(); ++i) {
        const auto* generator = impl_->extraGeneratorDescriptors_.at(i);
        if (!generator || generator->generatorId != generatorId) {
          continue;
        }
        impl_->extraGeneratorDescriptors_.removeAt(i);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.generators.moveUp")) {
      const QString generatorId = value.toString().trimmed();
      for (std::size_t i = 1; i < impl_->extraGeneratorDescriptors_.count(); ++i) {
        auto* current = impl_->extraGeneratorDescriptors_.at(i);
        auto* previous = impl_->extraGeneratorDescriptors_.at(i - 1);
        if (!current || !previous || current->generatorId != generatorId) {
          continue;
        }
        std::swap(*current, *previous);
        previous->order = 1000 + static_cast<int>((i - 1) + 1) * 10;
        current->order = 1000 + static_cast<int>(i + 1) * 10;
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.generators.moveDown")) {
      if (impl_->extraGeneratorDescriptors_.count() < 2) {
        return false;
      }
      const QString generatorId = value.toString().trimmed();
      for (std::size_t i = 0; i + 1 < impl_->extraGeneratorDescriptors_.count(); ++i) {
        auto* current = impl_->extraGeneratorDescriptors_.at(i);
        auto* next = impl_->extraGeneratorDescriptors_.at(i + 1);
        if (!current || !next || current->generatorId != generatorId) {
          continue;
        }
        std::swap(*current, *next);
        current->order = 1000 + static_cast<int>(i + 1) * 10;
        next->order = 1000 + static_cast<int>((i + 1) + 1) * 10;
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath.startsWith(QStringLiteral("component.generators."))) {
      const QStringList parts = propertyPath.split(QLatin1Char('.'));
      if (parts.size() == 4) {
        bool ok = false;
        const int generatorIndex = parts[2].toInt(&ok);
        if (!ok || generatorIndex < 0 ||
            generatorIndex >=
                static_cast<int>(impl_->extraGeneratorDescriptors_.count())) {
          return false;
        }
        auto* descriptor = impl_->extraGeneratorDescriptors_.at(
            static_cast<std::size_t>(generatorIndex));
        if (!descriptor) {
          return false;
        }
        const QString field = parts[3];
        auto setSetting = [&](const QString& key, const QJsonValue& settingValue) {
          descriptor->settings.insert(key, settingValue);
          notifyLayerMutation(this, LayerDirtyFlag::Effect,
                              LayerDirtyReason::PropertyChanged);
          return true;
        };
        auto setFiniteOnlySetting = [&](const QString &key, double raw,
                                       double fallback) {
          const double existing =
              descriptor->settings.value(key).toDouble(fallback);
          const double safeExisting =
              std::isfinite(existing) ? existing : fallback;
          return setSetting(key, std::isfinite(raw) ? raw : safeExisting);
        };
        auto setFiniteSetting = [&](const QString &key, double raw,
                                    double fallback, double minimum,
                                    double maximum) {
          return setSetting(
              key, finiteClampedValue(
                      raw, descriptor->settings.value(key).toDouble(fallback),
                      minimum, maximum));
        };
        if (field == QStringLiteral("enabled")) {
          descriptor->enabled = value.toBool();
          notifyLayerMutation(this, LayerDirtyFlag::Effect,
                              LayerDirtyReason::PropertyChanged);
          return true;
        }
        if (field == QStringLiteral("type")) {
          return false;
        }
        if (field == QStringLiteral("timeOffsetStep")) {
          return setSetting(
              field, finiteClampedValue(
                         value.toDouble(),
                         descriptor->settings.value(field).toDouble(0.0),
                         -10000.0, 10000.0));
        }
        if (field == QStringLiteral("sequenceEnabled")) {
          return setSetting(field, value.toBool());
        }
        if (field == QStringLiteral("sequenceRate")) {
          return setSetting(
              field, finiteClampedValue(
                         value.toDouble(),
                         descriptor->settings.value(field).toDouble(8.0),
                         0.1, 240.0));
        }
        if (field == QStringLiteral("sequenceSoftness")) {
          return setSetting(
              field, finiteClampedValue(
                         value.toDouble(),
                         descriptor->settings.value(field).toDouble(1.0),
                         0.1, 32.0));
        }
        if (field == QStringLiteral("columns") ||
            field == QStringLiteral("rows") ||
            field == QStringLiteral("depth") ||
            field == QStringLiteral("radialCount")) {
          return setSetting(field, std::max(1, value.toInt()));
        }
        if (field == QStringLiteral("spacingX") ||
            field == QStringLiteral("spacingY") ||
            field == QStringLiteral("spacingZ") ||
            field == QStringLiteral("radius") ||
            field == QStringLiteral("startAngle") ||
            field == QStringLiteral("endAngle")) {
          const double fallback =
              descriptor->settings.value(field).toDouble(0.0);
          const bool angle = field == QStringLiteral("startAngle") ||
                             field == QStringLiteral("endAngle");
          return setSetting(
              field, finiteClampedValue(value.toDouble(), fallback,
                                        angle ? -360000.0 : -100000.0,
                                        angle ? 360000.0 : 100000.0));
        }
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.fields.add")) {
      const QString requestedType = value.toString().trimmed().toLower();
      LayerFieldDescriptor descriptor;
      const int nextIndex =
          static_cast<int>(impl_->extraFieldDescriptors_.count()) + 1;
      descriptor.fieldId = QStringLiteral("field.extra.%1").arg(nextIndex);
      descriptor.enabled = true;
      descriptor.order = 1000 + nextIndex * 10;
      descriptor.blendMode = QStringLiteral("normal");
      descriptor.strength = 1.0f;
      descriptor.invert = false;
      descriptor.settings[QStringLiteral("centerX")] = 0.0;
      descriptor.settings[QStringLiteral("centerY")] = 0.0;
      descriptor.settings[QStringLiteral("angle")] = 0.0;
      if (requestedType == QStringLiteral("sphere")) {
        descriptor.typeId = QStringLiteral("artifact.field.sphere");
        descriptor.settings[QStringLiteral("radius")] = 160.0;
        descriptor.settings[QStringLiteral("falloffWidth")] = 40.0;
      } else if (requestedType == QStringLiteral("box")) {
        descriptor.typeId = QStringLiteral("artifact.field.box");
        descriptor.settings[QStringLiteral("halfX")] = 120.0;
        descriptor.settings[QStringLiteral("halfY")] = 120.0;
        descriptor.settings[QStringLiteral("halfZ")] = 120.0;
        descriptor.settings[QStringLiteral("falloffWidth")] = 40.0;
      } else if (requestedType == QStringLiteral("linear")) {
        descriptor.typeId = QStringLiteral("artifact.field.linear");
        descriptor.settings[QStringLiteral("length")] = 320.0;
        descriptor.settings[QStringLiteral("useSmoothstep")] = true;
      } else if (requestedType == QStringLiteral("radial")) {
        descriptor.typeId = QStringLiteral("artifact.field.radial");
        descriptor.settings[QStringLiteral("innerRadius")] = 0.0;
        descriptor.settings[QStringLiteral("outerRadius")] = 160.0;
      } else if (requestedType == QStringLiteral("noise")) {
        descriptor.typeId = QStringLiteral("artifact.field.noise");
        descriptor.settings[QStringLiteral("scale")] = 120.0;
        descriptor.settings[QStringLiteral("amplitude")] = 1.0;
        descriptor.settings[QStringLiteral("octaves")] = 3;
      } else {
        descriptor.typeId = QStringLiteral("artifact.field.solid");
        descriptor.settings[QStringLiteral("value")] = 1.0;
      }
      impl_->extraFieldDescriptors_.add(std::move(descriptor));
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fields.remove")) {
      const QString fieldId = value.toString().trimmed();
      for (std::size_t i = 0; i < impl_->extraFieldDescriptors_.count(); ++i) {
        const auto* field = impl_->extraFieldDescriptors_.at(i);
        if (!field || field->fieldId != fieldId) {
          continue;
        }
        impl_->extraFieldDescriptors_.removeAt(i);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.fields.moveUp")) {
      const QString fieldId = value.toString().trimmed();
      for (std::size_t i = 1; i < impl_->extraFieldDescriptors_.count(); ++i) {
        auto* current = impl_->extraFieldDescriptors_.at(i);
        auto* previous = impl_->extraFieldDescriptors_.at(i - 1);
        if (!current || !previous || current->fieldId != fieldId) {
          continue;
        }
        std::swap(*current, *previous);
        previous->order = 1000 + static_cast<int>((i - 1) + 1) * 10;
        current->order = 1000 + static_cast<int>(i + 1) * 10;
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.fields.moveDown")) {
      if (impl_->extraFieldDescriptors_.count() < 2) {
        return false;
      }
      const QString fieldId = value.toString().trimmed();
      for (std::size_t i = 0; i + 1 < impl_->extraFieldDescriptors_.count(); ++i) {
        auto* current = impl_->extraFieldDescriptors_.at(i);
        auto* next = impl_->extraFieldDescriptors_.at(i + 1);
        if (!current || !next || current->fieldId != fieldId) {
          continue;
        }
        std::swap(*current, *next);
        current->order = 1000 + static_cast<int>(i + 1) * 10;
        next->order = 1000 + static_cast<int>((i + 1) + 1) * 10;
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath.startsWith(QStringLiteral("component.fields."))) {
      const QStringList parts = propertyPath.split(QLatin1Char('.'));
      if (parts.size() == 4) {
        bool ok = false;
        const int fieldIndex = parts[2].toInt(&ok);
        if (!ok || fieldIndex < 0 ||
            fieldIndex >= static_cast<int>(impl_->extraFieldDescriptors_.count())) {
          return false;
        }
        auto* descriptor =
            impl_->extraFieldDescriptors_.at(static_cast<std::size_t>(fieldIndex));
        if (!descriptor) {
          return false;
        }
        const QString fieldName = parts[3];
        auto setSetting = [&](const QString& key, const QJsonValue& settingValue) {
          descriptor->settings.insert(key, settingValue);
          notifyLayerMutation(this, LayerDirtyFlag::Effect,
                              LayerDirtyReason::PropertyChanged);
          return true;
        };
        auto setFiniteOnlySetting = [&](const QString &key, double raw,
                                       double fallback) {
          const double existing =
              descriptor->settings.value(key).toDouble(fallback);
          const double safeExisting =
              std::isfinite(existing) ? existing : fallback;
          return setSetting(key, std::isfinite(raw) ? raw : safeExisting);
        };
        if (fieldName == QStringLiteral("enabled")) {
          descriptor->enabled = value.toBool();
        } else if (fieldName == QStringLiteral("strength")) {
          const double strength = value.toDouble();
          descriptor->strength = std::isfinite(strength)
                                     ? static_cast<float>(
                                           std::clamp(strength, 0.0, 1.0))
                                     : 1.0f;
        } else if (fieldName == QStringLiteral("invert")) {
          descriptor->invert = value.toBool();
        } else if (fieldName == QStringLiteral("value") ||
                   fieldName == QStringLiteral("centerX") ||
                   fieldName == QStringLiteral("centerY") ||
                   fieldName == QStringLiteral("angle") ||
                   fieldName == QStringLiteral("radius") ||
                   fieldName == QStringLiteral("falloffWidth") ||
                   fieldName == QStringLiteral("halfX") ||
                   fieldName == QStringLiteral("halfY") ||
                   fieldName == QStringLiteral("halfZ") ||
                   fieldName == QStringLiteral("length") ||
                   fieldName == QStringLiteral("innerRadius") ||
                   fieldName == QStringLiteral("outerRadius") ||
                   fieldName == QStringLiteral("scale") ||
                   fieldName == QStringLiteral("amplitude")) {
          return setFiniteOnlySetting(fieldName, value.toDouble(), 0.0);
        } else if (fieldName == QStringLiteral("octaves")) {
          return setSetting(fieldName, std::max(1, value.toInt()));
        } else if (fieldName == QStringLiteral("useSmoothstep")) {
          return setSetting(fieldName, value.toBool());
        } else if (fieldName == QStringLiteral("summary")) {
          return true;
        } else {
          return false;
        }
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.cloneModifiers.add")) {
      const QString requestedType = value.toString().trimmed().toLower();
      LayerModifierDescriptor descriptor;
      const int nextIndex =
          static_cast<int>(impl_->extraCloneModifierDescriptors_.count()) + 1;
      descriptor.modifierId =
          QStringLiteral("modifier.extra.%1").arg(nextIndex);
      descriptor.enabled = true;
      descriptor.order = 1000 + nextIndex * 10;
      if (requestedType == QStringLiteral("sequence")) {
        descriptor.typeId = QStringLiteral("artifact.modifier.sequence");
        descriptor.settings[QStringLiteral("rate")] = 8.0;
        descriptor.settings[QStringLiteral("softness")] = 1.0;
      } else if (requestedType == QStringLiteral("plain")) {
        descriptor.typeId = QStringLiteral("artifact.modifier.plain");
        descriptor.settings[QStringLiteral("positionX")] = 0.0;
        descriptor.settings[QStringLiteral("positionY")] = 0.0;
        descriptor.settings[QStringLiteral("positionZ")] = 0.0;
        descriptor.settings[QStringLiteral("rotationX")] = 0.0;
        descriptor.settings[QStringLiteral("rotationY")] = 0.0;
        descriptor.settings[QStringLiteral("rotationZ")] = 0.0;
        descriptor.settings[QStringLiteral("scaleX")] = 1.0;
        descriptor.settings[QStringLiteral("scaleY")] = 1.0;
        descriptor.settings[QStringLiteral("scaleZ")] = 1.0;
        descriptor.settings[QStringLiteral("strength")] = 1.0;
      } else if (requestedType == QStringLiteral("random")) {
        descriptor.typeId = QStringLiteral("artifact.modifier.random");
        descriptor.settings[QStringLiteral("seed")] = 1;
        descriptor.settings[QStringLiteral("positionX")] = 0.0;
        descriptor.settings[QStringLiteral("positionY")] = 0.0;
        descriptor.settings[QStringLiteral("positionZ")] = 0.0;
        descriptor.settings[QStringLiteral("rotationX")] = 0.0;
        descriptor.settings[QStringLiteral("rotationY")] = 0.0;
        descriptor.settings[QStringLiteral("rotationZ")] = 0.0;
        descriptor.settings[QStringLiteral("scaleVariance")] = 0.0;
        descriptor.settings[QStringLiteral("strength")] = 1.0;
      } else if (requestedType == QStringLiteral("step")) {
        descriptor.typeId = QStringLiteral("artifact.modifier.step");
        descriptor.settings[QStringLiteral("positionX")] = 0.0;
        descriptor.settings[QStringLiteral("positionY")] = 0.0;
        descriptor.settings[QStringLiteral("positionZ")] = 0.0;
        descriptor.settings[QStringLiteral("rotationX")] = 0.0;
        descriptor.settings[QStringLiteral("rotationY")] = 0.0;
        descriptor.settings[QStringLiteral("rotationZ")] = 0.0;
        descriptor.settings[QStringLiteral("scaleX")] = 1.0;
        descriptor.settings[QStringLiteral("scaleY")] = 1.0;
        descriptor.settings[QStringLiteral("scaleZ")] = 1.0;
        descriptor.settings[QStringLiteral("strength")] = 1.0;
      } else if (requestedType == QStringLiteral("formula")) {
        descriptor.typeId = QStringLiteral("artifact.modifier.formula");
        descriptor.settings[QStringLiteral("amplitudeX")] = 0.0;
        descriptor.settings[QStringLiteral("amplitudeY")] = 0.0;
        descriptor.settings[QStringLiteral("amplitudeZ")] = 0.0;
        descriptor.settings[QStringLiteral("frequency")] = 1.0;
        descriptor.settings[QStringLiteral("phase")] = 0.0;
        descriptor.settings[QStringLiteral("indexPhase")] = 0.0;
        descriptor.settings[QStringLiteral("strength")] = 1.0;
      } else if (requestedType == QStringLiteral("spline")) {
        descriptor.typeId = QStringLiteral("artifact.modifier.spline");
        descriptor.settings[QStringLiteral("startX")] = 0.0;
        descriptor.settings[QStringLiteral("startY")] = 0.0;
        descriptor.settings[QStringLiteral("startZ")] = 0.0;
        descriptor.settings[QStringLiteral("controlX")] = 0.0;
        descriptor.settings[QStringLiteral("controlY")] = 0.0;
        descriptor.settings[QStringLiteral("controlZ")] = 0.0;
        descriptor.settings[QStringLiteral("endX")] = 0.0;
        descriptor.settings[QStringLiteral("endY")] = 0.0;
        descriptor.settings[QStringLiteral("endZ")] = 0.0;
        descriptor.settings[QStringLiteral("indexScale")] = 0.1;
        descriptor.settings[QStringLiteral("strength")] = 1.0;
      } else {
        descriptor.typeId = QStringLiteral("artifact.modifier.time-offset");
        descriptor.settings[QStringLiteral("step")] = 0.0;
      }
      impl_->extraCloneModifierDescriptors_.add(std::move(descriptor));
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloneModifiers.remove")) {
      const QString modifierId = value.toString().trimmed();
      for (std::size_t i = 0; i < impl_->extraCloneModifierDescriptors_.count(); ++i) {
        const auto* modifier = impl_->extraCloneModifierDescriptors_.at(i);
        if (!modifier || modifier->modifierId != modifierId) {
          continue;
        }
        impl_->extraCloneModifierDescriptors_.removeAt(i);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.cloneModifiers.moveUp")) {
      const QString modifierId = value.toString().trimmed();
      for (std::size_t i = 1; i < impl_->extraCloneModifierDescriptors_.count(); ++i) {
        auto* current = impl_->extraCloneModifierDescriptors_.at(i);
        auto* previous = impl_->extraCloneModifierDescriptors_.at(i - 1);
        if (!current || !previous || current->modifierId != modifierId) {
          continue;
        }
        std::swap(*current, *previous);
        previous->order = 1000 + static_cast<int>((i - 1) + 1) * 10;
        current->order = 1000 + static_cast<int>(i + 1) * 10;
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.cloneModifiers.moveDown")) {
      if (impl_->extraCloneModifierDescriptors_.count() < 2) {
        return false;
      }
      const QString modifierId = value.toString().trimmed();
      for (std::size_t i = 0; i + 1 < impl_->extraCloneModifierDescriptors_.count(); ++i) {
        auto* current = impl_->extraCloneModifierDescriptors_.at(i);
        auto* next = impl_->extraCloneModifierDescriptors_.at(i + 1);
        if (!current || !next || current->modifierId != modifierId) {
          continue;
        }
        std::swap(*current, *next);
        current->order = 1000 + static_cast<int>(i + 1) * 10;
        next->order = 1000 + static_cast<int>((i + 1) + 1) * 10;
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath.startsWith(
            QStringLiteral("component.cloner.modifiers.compat.timeOffset."))) {
      const QString field =
          propertyPath.mid(QStringLiteral(
                               "component.cloner.modifiers.compat.timeOffset.")
                               .size());
      if (field == QStringLiteral("enabled")) {
        return true;
      }
      if (field == QStringLiteral("step")) {
        impl_->clonerTimeOffsetStep_ = finiteClampedValue(
            value.toDouble(), impl_->clonerTimeOffsetStep_, -10000.0, 10000.0);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath.startsWith(
            QStringLiteral("component.cloner.modifiers.compat.sequence."))) {
      const QString field =
          propertyPath.mid(QStringLiteral(
                               "component.cloner.modifiers.compat.sequence.")
                               .size());
      if (field == QStringLiteral("enabled")) {
        impl_->clonerSequenceEnabled_ = value.toBool();
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      if (field == QStringLiteral("rate")) {
        impl_->clonerSequenceRate_ = finiteClampedValue(
            value.toDouble(), impl_->clonerSequenceRate_, 0.1, 240.0);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      if (field == QStringLiteral("softness")) {
        impl_->clonerSequenceSoftness_ = finiteClampedValue(
            value.toDouble(), impl_->clonerSequenceSoftness_, 0.1, 32.0);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }
    if (propertyPath.startsWith(QStringLiteral("component.cloneModifiers."))) {
      const QStringList parts = propertyPath.split(QLatin1Char('.'));
      if (parts.size() == 4) {
        bool ok = false;
        const int modifierIndex = parts[2].toInt(&ok);
        if (!ok || modifierIndex < 0 ||
            modifierIndex >=
                static_cast<int>(impl_->extraCloneModifierDescriptors_.count())) {
          return false;
        }
        auto* descriptor = impl_->extraCloneModifierDescriptors_.at(
            static_cast<std::size_t>(modifierIndex));
        if (!descriptor) {
          return false;
        }
        const QString field = parts[3];
        auto setSetting = [&](const QString& key, const QJsonValue& settingValue) {
          descriptor->settings.insert(key, settingValue);
          notifyLayerMutation(this, LayerDirtyFlag::Effect,
                              LayerDirtyReason::PropertyChanged);
          return true;
        };
        auto setFiniteOnlySetting = [&](const QString& key, double raw,
                                       double fallback) {
          const double existing =
              descriptor->settings.value(key).toDouble(fallback);
          const double safeExisting =
              std::isfinite(existing) ? existing : fallback;
          return setSetting(key, std::isfinite(raw) ? raw : safeExisting);
        };
        auto setFiniteSetting = [&](const QString& key, double raw,
                                   double fallback, double minimum,
                                   double maximum) {
          return setSetting(
              key, finiteClampedValue(
                      raw, descriptor->settings.value(key).toDouble(fallback),
                      minimum, maximum));
        };
        if (field == QStringLiteral("enabled")) {
          descriptor->enabled = value.toBool();
          notifyLayerMutation(this, LayerDirtyFlag::Effect,
                              LayerDirtyReason::PropertyChanged);
          return true;
        }
        if (field == QStringLiteral("step")) {
          return setFiniteSetting(field, value.toDouble(), 0.0, -10000.0,
                                  10000.0);
        }
        if (field == QStringLiteral("rate")) {
          return setFiniteSetting(field, value.toDouble(), 8.0, 0.1, 240.0);
        }
        if (field == QStringLiteral("softness")) {
          return setFiniteSetting(field, value.toDouble(), 1.0, 0.1, 32.0);
        }
        if (field == QStringLiteral("seed")) {
          return setSetting(field, value.toInt());
        }
        if (field == QStringLiteral("strength")) {
          return setFiniteSetting(field, value.toDouble(), 1.0, 0.0, 1.0);
        }
        if (field == QStringLiteral("frequency")) {
          return setFiniteSetting(field, value.toDouble(), 1.0, 0.0, 60.0);
        }
        if (field == QStringLiteral("phase") ||
            field == QStringLiteral("indexPhase")) {
          return setFiniteSetting(field, value.toDouble(), 0.0,
                                  -6.28318530718, 6.28318530718);
        }
        if (field == QStringLiteral("indexScale")) {
          return setFiniteSetting(field, value.toDouble(), 0.1, 0.0001, 1.0);
        }
        if (field == QStringLiteral("amplitudeX") ||
            field == QStringLiteral("amplitudeY") ||
            field == QStringLiteral("amplitudeZ") ||
            field == QStringLiteral("startX") ||
            field == QStringLiteral("startY") ||
            field == QStringLiteral("startZ") ||
            field == QStringLiteral("controlX") ||
            field == QStringLiteral("controlY") ||
            field == QStringLiteral("controlZ") ||
            field == QStringLiteral("endX") ||
            field == QStringLiteral("endY") ||
            field == QStringLiteral("endZ")) {
          return setFiniteSetting(field, value.toDouble(), 0.0, -10000.0,
                                  10000.0);
        }
        if (field == QStringLiteral("positionX") ||
            field == QStringLiteral("positionY") ||
            field == QStringLiteral("positionZ") ||
            field == QStringLiteral("rotationX") ||
            field == QStringLiteral("rotationY") ||
            field == QStringLiteral("rotationZ") ||
            field == QStringLiteral("scaleX") ||
            field == QStringLiteral("scaleY") ||
            field == QStringLiteral("scaleZ") ||
            field == QStringLiteral("scaleVariance") ||
            field == QStringLiteral("amplitudeX") ||
            field == QStringLiteral("amplitudeY") ||
            field == QStringLiteral("amplitudeZ") ||
            field == QStringLiteral("frequency") ||
            field == QStringLiteral("phase") ||
            field == QStringLiteral("indexPhase") ||
            field == QStringLiteral("startX") ||
            field == QStringLiteral("startY") ||
            field == QStringLiteral("startZ") ||
            field == QStringLiteral("controlX") ||
            field == QStringLiteral("controlY") ||
            field == QStringLiteral("controlZ") ||
            field == QStringLiteral("endX") ||
            field == QStringLiteral("endY") ||
            field == QStringLiteral("endZ") ||
            field == QStringLiteral("indexScale")) {
          return setFiniteOnlySetting(field, value.toDouble(), 0.0);
        }
      }
      return false;
    }
    if (propertyPath == QStringLiteral("component.cloner.transforms.add")) {
      ClonerTransformOperation op;
      op.name = QStringLiteral("Transform %1")
                    .arg(static_cast<int>(impl_->clonerTransforms_.size()) + 1);
      impl_->clonerTransforms_.push_back(op);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.transforms.remove")) {
      const int index = value.toInt();
      if (index >= 0 &&
          index < static_cast<int>(impl_->clonerTransforms_.size())) {
        impl_->clonerTransforms_.removeAt(static_cast<size_t>(index));
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
      }
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.transforms.duplicate")) {
      const int index = value.toInt();
      if (index >= 0 &&
          index < static_cast<int>(impl_->clonerTransforms_.size())) {
        auto copy = impl_->clonerTransforms_[static_cast<size_t>(index)];
        copy.name = copy.name.trimmed().isEmpty()
                        ? QStringLiteral("Transform %1")
                              .arg(static_cast<int>(impl_->clonerTransforms_.size()) + 1)
                        : copy.name + QStringLiteral(" Copy");
        impl_->clonerTransforms_.insert(
            static_cast<size_t>(index + 1), copy);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
      }
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.transforms.moveUp")) {
      const int index = value.toInt();
      if (index > 0 &&
          index < static_cast<int>(impl_->clonerTransforms_.size())) {
        impl_->clonerTransforms_.swapItemsAt(
            static_cast<size_t>(index), static_cast<size_t>(index - 1));
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
      }
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.transforms.moveDown")) {
      const int index = value.toInt();
      if (index >= 0 &&
          index + 1 < static_cast<int>(impl_->clonerTransforms_.size())) {
        impl_->clonerTransforms_.swapItemsAt(
            static_cast<size_t>(index), static_cast<size_t>(index + 1));
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
      }
      return true;
    }
    if (const auto clonerTransformAddress =
            parseClonerTransformPropertyPath(propertyPath)) {
      if (clonerTransformAddress->index < 0 ||
          clonerTransformAddress->index >=
              static_cast<int>(impl_->clonerTransforms_.size())) {
        return false;
      }
      auto &op = impl_->clonerTransforms_[static_cast<size_t>(
          clonerTransformAddress->index)];
      const QString field = clonerTransformAddress->field;
      if (field == QStringLiteral("name")) {
        op.name = value.toString();
      } else if (field == QStringLiteral("enabled")) {
        op.enabled = value.toBool();
      } else if (field == QStringLiteral("positionX")) {
        op.position.setX(finiteClampedValue(
            value.toDouble(), op.position.x(), -100000.0, 100000.0));
      } else if (field == QStringLiteral("positionY")) {
        op.position.setY(finiteClampedValue(
            value.toDouble(), op.position.y(), -100000.0, 100000.0));
      } else if (field == QStringLiteral("positionZ")) {
        op.position.setZ(finiteClampedValue(
            value.toDouble(), op.position.z(), -100000.0, 100000.0));
      } else if (field == QStringLiteral("rotationX")) {
        op.rotation.setX(finiteClampedValue(
            value.toDouble(), op.rotation.x(), -360000.0, 360000.0));
      } else if (field == QStringLiteral("rotationY")) {
        op.rotation.setY(finiteClampedValue(
            value.toDouble(), op.rotation.y(), -360000.0, 360000.0));
      } else if (field == QStringLiteral("rotationZ")) {
        op.rotation.setZ(finiteClampedValue(
            value.toDouble(), op.rotation.z(), -360000.0, 360000.0));
      } else if (field == QStringLiteral("scaleX")) {
        op.scale.setX(finiteClampedValue(
            value.toDouble(), op.scale.x(), -100000.0, 100000.0));
      } else if (field == QStringLiteral("scaleY")) {
        op.scale.setY(finiteClampedValue(
            value.toDouble(), op.scale.y(), -100000.0, 100000.0));
      } else if (field == QStringLiteral("scaleZ")) {
        op.scale.setZ(finiteClampedValue(
            value.toDouble(), op.scale.z(), -100000.0, 100000.0));
      } else {
        return false;
      }
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    return false;
}

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

bool ArtifactAbstractLayer::setComponentLayoutPropertyValue(
    const QString &propertyPath, const QVariant &value) {
    if (propertyPath == QStringLiteral("component.layout.enabled")) {
      impl_->layoutComponentEnabled_ = value.toBool();
      if (!impl_->layoutComponentEnabled_) impl_->layoutResponsiveEnabled_ = false;
      impl_->syncBuiltinComponentDescriptors();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.mode")) {
      impl_->layoutMode_ = std::clamp(value.toInt(), 0, 2);
      impl_->syncBuiltinComponentDescriptors();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.responsiveEnabled")) {
      impl_->layoutResponsiveEnabled_ = value.toBool();
      if (impl_->layoutResponsiveEnabled_) impl_->layoutComponentEnabled_ = true;
      impl_->syncBuiltinComponentDescriptors();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.anchorMode")) {
      impl_->layoutAnchorMode_ = std::clamp(value.toInt(), 0, 2);
      impl_->syncBuiltinComponentDescriptors();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.horizontalPin")) {
      impl_->layoutHorizontalPin_ = std::clamp(value.toInt(), 0, 3);
      impl_->syncBuiltinComponentDescriptors();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.verticalPin")) {
      impl_->layoutVerticalPin_ = std::clamp(value.toInt(), 0, 3);
      impl_->syncBuiltinComponentDescriptors();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.scaleMode")) {
      impl_->layoutScaleMode_ = std::clamp(value.toInt(), 0, 3);
      impl_->syncBuiltinComponentDescriptors();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.responsiveOffsetX")) {
      impl_->layoutResponsiveOffsetX_ = static_cast<float>(finiteClampedValue(
          value.toDouble(), impl_->layoutResponsiveOffsetX_, -100000.0, 100000.0));
      impl_->syncBuiltinComponentDescriptors();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.responsiveOffsetY")) {
      impl_->layoutResponsiveOffsetY_ = static_cast<float>(finiteClampedValue(
          value.toDouble(), impl_->layoutResponsiveOffsetY_, -100000.0, 100000.0));
      impl_->syncBuiltinComponentDescriptors();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.safeAreaEnabled")) {
      impl_->layoutSafeAreaEnabled_ = value.toBool();
      impl_->syncBuiltinComponentDescriptors();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.safeAreaPaddingX")) {
      impl_->layoutSafeAreaPaddingX_ = finiteClampedValue(
          value.toDouble(), impl_->layoutSafeAreaPaddingX_, -100000.0,
          100000.0);
      impl_->syncBuiltinComponentDescriptors();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.safeAreaPaddingY")) {
      impl_->layoutSafeAreaPaddingY_ = finiteClampedValue(
          value.toDouble(), impl_->layoutSafeAreaPaddingY_, -100000.0,
          100000.0);
      impl_->syncBuiltinComponentDescriptors();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.stackDirection")) {
      impl_->layoutStackDirection_ = std::clamp(value.toInt(), 0, 1);
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.gap")) {
      impl_->layoutGap_ = finiteClampedValue(
          value.toDouble(), impl_->layoutGap_, -100000.0, 100000.0);
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.maxPerRow")) {
      impl_->layoutMaxPerRow_ = std::max(0, value.toInt());
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.enabled")) {
      impl_->clonerComponentEnabled_ = value.toBool();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.mode")) {
      impl_->clonerMode_ = value.toInt();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
      if (propertyPath == QStringLiteral("component.cloner.cloneCount")) {
        impl_->clonerCloneCount_ = std::clamp(value.toInt(), 1, 256);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.timeOffsetStep")) {
      impl_->clonerTimeOffsetStep_ = finiteClampedValue(
          value.toDouble(), impl_->clonerTimeOffsetStep_, -10000.0, 10000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.sequenceEnabled")) {
      impl_->clonerSequenceEnabled_ = value.toBool();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.sequenceRate")) {
      impl_->clonerSequenceRate_ = finiteClampedValue(
          value.toDouble(), impl_->clonerSequenceRate_, 0.01, 240.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.sequenceSoftness")) {
      impl_->clonerSequenceSoftness_ = finiteClampedValue(
          value.toDouble(), impl_->clonerSequenceSoftness_, 0.01, 32.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
  if (propertyPath == QStringLiteral("component.cloner.offsetX")) {
    impl_->clonerOffsetX_ = finiteClampedValue(
        value.toDouble(), impl_->clonerOffsetX_, -100000.0, 100000.0);
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    return true;
  }
    if (propertyPath == QStringLiteral("component.cloner.offsetY")) {
      impl_->clonerOffsetY_ = finiteClampedValue(
          value.toDouble(), impl_->clonerOffsetY_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.offsetZ")) {
      impl_->clonerOffsetZ_ = finiteClampedValue(
          value.toDouble(), impl_->clonerOffsetZ_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.jitterX")) {
      impl_->clonerJitterX_ = finiteClampedValue(
          value.toDouble(), impl_->clonerJitterX_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.jitterY")) {
      impl_->clonerJitterY_ = finiteClampedValue(
          value.toDouble(), impl_->clonerJitterY_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.jitterZ")) {
      impl_->clonerJitterZ_ = finiteClampedValue(
          value.toDouble(), impl_->clonerJitterZ_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.seed")) {
      impl_->clonerSeed_ = value.toInt();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.columns")) {
      impl_->clonerColumns_ = std::max(1, value.toInt());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.rows")) {
      impl_->clonerRows_ = std::max(1, value.toInt());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.depth")) {
      impl_->clonerDepth_ = std::max(1, value.toInt());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.spacingX")) {
      impl_->clonerSpacingX_ = finiteClampedValue(
          value.toDouble(), impl_->clonerSpacingX_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.spacingY")) {
      impl_->clonerSpacingY_ = finiteClampedValue(
          value.toDouble(), impl_->clonerSpacingY_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.spacingZ")) {
      impl_->clonerSpacingZ_ = finiteClampedValue(
          value.toDouble(), impl_->clonerSpacingZ_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.radialCount")) {
      impl_->clonerRadialCount_ = std::max(1, value.toInt());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.radius")) {
      impl_->clonerRadius_ = finiteClampedValue(
          value.toDouble(), impl_->clonerRadius_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.startAngle")) {
      impl_->clonerStartAngle_ = finiteClampedValue(
          value.toDouble(), impl_->clonerStartAngle_, -360000.0, 360000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.endAngle")) {
      impl_->clonerEndAngle_ = finiteClampedValue(
          value.toDouble(), impl_->clonerEndAngle_, -360000.0, 360000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.rotationStep")) {
      impl_->clonerRotationStep_ = finiteClampedValue(
          value.toDouble(), impl_->clonerRotationStep_, -360000.0, 360000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.opacityDecay")) {
      impl_->clonerOpacityDecay_ = finiteClampedValue(
          value.toDouble(), impl_->clonerOpacityDecay_, 0.0, 1.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
  return false;
}

bool ArtifactAbstractLayer::setTransformTimeSourcePropertyValue(
    const QString &propertyPath, const QVariant &value) {
  auto &t3 = transform3D();
  const RationalTime currentTime = currentTimelineTime(this);
  const auto finiteTransformInput = [](double raw, double fallback) {
    if (std::isfinite(raw)) {
      return raw;
    }
    return std::isfinite(fallback) ? fallback : 0.0;
  };


  if (impl_->fluidComponentEnabled_ && impl_->fluidMode_ == 1 &&
      propertyPath.startsWith(QStringLiteral("transform."))) {
    impl_->fluidRuntime_.invalidateLiquidSimulation();
  }

  if (propertyPath == QStringLiteral("transform.initialRotation")) {
    t3.setInitialRotation(
        currentTime, finiteTransformInput(value.toDouble(), t3.initialRotation()));
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }

  // トランスフォームのプロパティ
  if (propertyPath.startsWith(QStringLiteral("transform."))) {
      if (impl_->activeVariantIndex_ != 0) {
          auto* var = getActiveVariant();
          if (var && !var->transform3DOverride.has_value()) {
              var->transform3DOverride = impl_->transform_;
              SetFlag(var->overrideFlags_, VariantOverrideFlags::Transform);
          }
      }
  }

  if (auto property = transformChannelProperty(t3, propertyPath)) {
    const double number = value.toDouble();
    if (!std::isfinite(number)) return false;
    if (!property->getKeyFrames().empty()) {
      const auto keys = property->getKeyFrames();
      const auto existing = std::find_if(keys.begin(), keys.end(),
          [&currentTime](const auto& key) { return key.time == currentTime; });
      if (existing != keys.end()) {
        const auto& key = *existing;
        property->addKeyFrame(currentTime, number, key.interpolation,
            key.cp1_x, key.cp1_y, key.cp2_x, key.cp2_y, key.roving);
        property->setKeyFrameAnchorAt(currentTime, key.anchor);
        property->setKeyFrameColorLabelAt(currentTime, key.colorLabel);
      } else {
        property->addKeyFrame(currentTime, number);
      }
    } else {
      property->setValue(number);
    }
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.autoOrient")) {
    t3.setAutoOrientMode(static_cast<AutoOrientMode>(std::clamp(
        value.toInt(), static_cast<int>(AutoOrientMode::Off),
        static_cast<int>(AutoOrientMode::AlongPathAtFrameStart))));
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("time.inPoint")) {
    setInPoint(FramePosition(value.toLongLong()));
    return true;
  }
  if (propertyPath == QStringLiteral("time.outPoint")) {
    setOutPoint(FramePosition(value.toLongLong()));
    return true;
  }
  if (propertyPath == QStringLiteral("time.startTime")) {
    setStartTime(FramePosition(value.toLongLong()));
    return true;
  }
  if (propertyPath == QStringLiteral("source.width")) {
    const auto cur = sourceSize();
    const int width = std::clamp(value.toInt(), 1, 16384);
    if (cur.width == width) {
      return true;
    }
    setSourceSize(Size_2D(width, cur.height));
    notifyLayerMutation(this, LayerDirtyFlag::Source,
                        LayerDirtyReason::SourceChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("source.height")) {
    const auto cur = sourceSize();
    const int height = std::clamp(value.toInt(), 1, 16384);
    if (cur.height == height) {
      return true;
    }
    setSourceSize(Size_2D(cur.width, height));
    notifyLayerMutation(this, LayerDirtyFlag::Source,
                        LayerDirtyReason::SourceChanged);
    return true;
  }
  return false;
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
  notifyLayerMutation(this, LayerDirtyFlag::Property,
                      LayerDirtyReason::PropertyChanged);
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
      if ((property->isAnimatable() && !property->getKeyFrames().empty()) ||
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
    if (property->isAnimatable() && !property->getKeyFrames().empty()) {
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
