module;
#include <algorithm>
#include <atomic>
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
#include <QTransform>
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

module Artifact.Layer.Abstract:Impl;

import Artifact.Layer.Abstract;

import Memory.SharedPtr;
import Utils;
import Utils.String.UniString;
import Layer.State;
import Animation.Transform2D;
import Animation.Transform3D;
import Animation.Dynamics;
import Size;
import Layer.Blend;
import Artifact.Animation.LayerEffectEnvelope;
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

using namespace Artifact;
using namespace ArtifactCore;

namespace Artifact {

class ArtifactAbstractLayerImpl {
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
  // Phase 2 automation-clip placements (pattern data lives on the composition).
  std::vector<ArtifactCore::AutomationClipInstance> automationClipInstances_;
  QJsonObject deformation2DData_;
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
  std::atomic<std::uint64_t> effectRevision_{1};
  std::atomic<std::uint64_t> animatedEffectPropertyState_{0};
  mutable std::mutex rasterizerOverscanCacheMutex_;
  mutable std::uint64_t cachedRasterizerOverscanRevision_ = 0;
  mutable float cachedRasterizerOverscanPixels_ = 0.0f;
  mutable bool cachedRasterizerOverscanValid_ = false;
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
  ArtifactAbstractLayerImpl();
  ~ArtifactAbstractLayerImpl();
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
  bool hasEnabledRasterizerEffect() const;
  bool hasEnabledFullFrameEffect() const;
  float enabledRasterizerOverscanPixels() const;
  float cachedRasterizerOverscanPixels() const;

  // モディファイア管理メソッド
  void addModifier(SharedPtr<ArtifactLayerModifier> modifier);
  void removeModifier(const QString& modifierId);
  void clearModifiers();
  std::vector<SharedPtr<ArtifactLayerModifier>> getModifiers() const;
  SharedPtr<ArtifactLayerModifier> getModifier(const QString& modifierId) const;
  int modifierCount() const;
  bool hasModifiers() const;

};

} // namespace Artifact
