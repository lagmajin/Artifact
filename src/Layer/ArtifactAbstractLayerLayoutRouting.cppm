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
SharedPtr<ArtifactCore::AbstractProperty> transformChannelProperty(
    const ArtifactCore::AnimatableTransform3D& transform,
    const QString& path) {
  using ArtifactCore::TransformChannel;
  if (path == QStringLiteral("transform.position.x"))
    return transform.channelProperty(TransformChannel::PositionX);
  if (path == QStringLiteral("transform.position.y"))
    return transform.channelProperty(TransformChannel::PositionY);
  if (path == QStringLiteral("transform.position.z"))
    return transform.channelProperty(TransformChannel::PositionZ);
  if (path == QStringLiteral("transform.rotation") ||
      path == QStringLiteral("transform.rotation.z"))
    return transform.channelProperty(TransformChannel::Rotation);
  if (path == QStringLiteral("transform.rotation.x"))
    return transform.channelProperty(TransformChannel::RotationX);
  if (path == QStringLiteral("transform.rotation.y"))
    return transform.channelProperty(TransformChannel::RotationY);
  if (path == QStringLiteral("transform.scale.x"))
    return transform.channelProperty(TransformChannel::ScaleX);
  if (path == QStringLiteral("transform.scale.y"))
    return transform.channelProperty(TransformChannel::ScaleY);
  if (path == QStringLiteral("transform.scale.z"))
    return transform.channelProperty(TransformChannel::ScaleZ);
  if (path == QStringLiteral("transform.anchor.x"))
    return transform.channelProperty(TransformChannel::AnchorX);
  if (path == QStringLiteral("transform.anchor.y"))
    return transform.channelProperty(TransformChannel::AnchorY);
  if (path == QStringLiteral("transform.anchor.z"))
    return transform.channelProperty(TransformChannel::AnchorZ);
  return {};
}
}

bool ArtifactAbstractLayer::setComponentLayoutPropertyValue(
    const QString &propertyPath, const QVariant &value) {
    if (propertyPath == QStringLiteral("component.layout.enabled")) {
      const bool enabled = value.toBool();
      // syncBuiltinComponentDescriptors() rebuilds all twelve descriptors, so
      // re-entering with an unchanged value would repeat a few hundred
      // QJsonObject inserts for nothing. Undo/redo and expression re-evaluation
      // both land here with the value that is already in place.
      if (impl_->layoutComponentEnabled_ == enabled &&
          (enabled || !impl_->layoutResponsiveEnabled_)) {
        return true;
      }
      impl_->layoutComponentEnabled_ = enabled;
      if (!impl_->layoutComponentEnabled_) impl_->layoutResponsiveEnabled_ = false;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.layout.enabled"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.mode")) {
      const int mode = std::clamp(value.toInt(), 0, 2);
      if (impl_->layoutMode_ == mode) {
        return true;
      }
      impl_->layoutMode_ = mode;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.layout.mode"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.responsiveEnabled")) {
      const bool enabled = value.toBool();
      if (impl_->layoutResponsiveEnabled_ == enabled &&
          (!enabled || impl_->layoutComponentEnabled_)) {
        return true;
      }
      impl_->layoutResponsiveEnabled_ = enabled;
      if (impl_->layoutResponsiveEnabled_) impl_->layoutComponentEnabled_ = true;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.layout.responsiveEnabled"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.anchorMode")) {
      const int anchorMode = std::clamp(value.toInt(), 0, 2);
      if (impl_->layoutAnchorMode_ == anchorMode) {
        return true;
      }
      impl_->layoutAnchorMode_ = anchorMode;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.layout.anchorMode"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.horizontalPin")) {
      const int pin = std::clamp(value.toInt(), 0, 3);
      if (impl_->layoutHorizontalPin_ == pin) {
        return true;
      }
      impl_->layoutHorizontalPin_ = pin;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.layout.horizontalPin"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.verticalPin")) {
      const int pin = std::clamp(value.toInt(), 0, 3);
      if (impl_->layoutVerticalPin_ == pin) {
        return true;
      }
      impl_->layoutVerticalPin_ = pin;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.layout.verticalPin"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.scaleMode")) {
      const int scaleMode = std::clamp(value.toInt(), 0, 3);
      if (impl_->layoutScaleMode_ == scaleMode) {
        return true;
      }
      impl_->layoutScaleMode_ = scaleMode;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.layout.scaleMode"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.responsiveOffsetX")) {
      const float offsetX = static_cast<float>(finiteClampedValue(
          value.toDouble(), impl_->layoutResponsiveOffsetX_, -100000.0, 100000.0));
      if (impl_->layoutResponsiveOffsetX_ == offsetX) {
        return true;
      }
      impl_->layoutResponsiveOffsetX_ = offsetX;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.layout.responsiveOffsetX"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.responsiveOffsetY")) {
      const float offsetY = static_cast<float>(finiteClampedValue(
          value.toDouble(), impl_->layoutResponsiveOffsetY_, -100000.0, 100000.0));
      if (impl_->layoutResponsiveOffsetY_ == offsetY) {
        return true;
      }
      impl_->layoutResponsiveOffsetY_ = offsetY;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.layout.responsiveOffsetY"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.safeAreaEnabled")) {
      const bool enabled = value.toBool();
      if (impl_->layoutSafeAreaEnabled_ == enabled) {
        return true;
      }
      impl_->layoutSafeAreaEnabled_ = enabled;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.layout.safeAreaEnabled"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.safeAreaPaddingX")) {
      const double paddingX = finiteClampedValue(
          value.toDouble(), impl_->layoutSafeAreaPaddingX_, -100000.0,
          100000.0);
      if (impl_->layoutSafeAreaPaddingX_ == paddingX) {
        return true;
      }
      impl_->layoutSafeAreaPaddingX_ = paddingX;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.layout.safeAreaPaddingX"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.safeAreaPaddingY")) {
      const double paddingY = finiteClampedValue(
          value.toDouble(), impl_->layoutSafeAreaPaddingY_, -100000.0,
          100000.0);
      if (impl_->layoutSafeAreaPaddingY_ == paddingY) {
        return true;
      }
      impl_->layoutSafeAreaPaddingY_ = paddingY;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.layout.safeAreaPaddingY"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.stackDirection")) {
      const int direction = std::clamp(value.toInt(), 0, 1);
      if (impl_->layoutStackDirection_ == direction) {
        return true;
      }
      impl_->layoutStackDirection_ = direction;
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.layout.stackDirection"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.gap")) {
      const double gap = finiteClampedValue(
          value.toDouble(), impl_->layoutGap_, -100000.0, 100000.0);
      if (impl_->layoutGap_ == gap) {
        return true;
      }
      impl_->layoutGap_ = gap;
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.layout.gap"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.maxPerRow")) {
      const int maxPerRow = std::max(0, value.toInt());
      if (impl_->layoutMaxPerRow_ == maxPerRow) {
        return true;
      }
      impl_->layoutMaxPerRow_ = maxPerRow;
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.layout.maxPerRow"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.enabled")) {
      const bool enabled = value.toBool();
      if (impl_->clonerComponentEnabled_ == enabled) {
        return true;
      }
      impl_->clonerComponentEnabled_ = enabled;
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.enabled"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.mode")) {
      const int mode = value.toInt();
      if (impl_->clonerMode_ == mode) {
        return true;
      }
      impl_->clonerMode_ = mode;
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.mode"));
      return true;
    }
      if (propertyPath == QStringLiteral("component.cloner.cloneCount")) {
        impl_->clonerCloneCount_ = std::clamp(value.toInt(), 1, 256);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.cloneCount"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.timeOffsetStep")) {
      impl_->clonerTimeOffsetStep_ = finiteClampedValue(
          value.toDouble(), impl_->clonerTimeOffsetStep_, -10000.0, 10000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.timeOffsetStep"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.sequenceEnabled")) {
      impl_->clonerSequenceEnabled_ = value.toBool();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.sequenceEnabled"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.sequenceRate")) {
      impl_->clonerSequenceRate_ = finiteClampedValue(
          value.toDouble(), impl_->clonerSequenceRate_, 0.01, 240.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.sequenceRate"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.sequenceSoftness")) {
      impl_->clonerSequenceSoftness_ = finiteClampedValue(
          value.toDouble(), impl_->clonerSequenceSoftness_, 0.01, 32.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.sequenceSoftness"));
      return true;
    }
  if (propertyPath == QStringLiteral("component.cloner.offsetX")) {
    impl_->clonerOffsetX_ = finiteClampedValue(
        value.toDouble(), impl_->clonerOffsetX_, -100000.0, 100000.0);
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    syncRoutedPropertyCache(QStringLiteral("component.cloner.offsetX"));
    return true;
  }
    if (propertyPath == QStringLiteral("component.cloner.offsetY")) {
      impl_->clonerOffsetY_ = finiteClampedValue(
          value.toDouble(), impl_->clonerOffsetY_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.offsetY"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.offsetZ")) {
      impl_->clonerOffsetZ_ = finiteClampedValue(
          value.toDouble(), impl_->clonerOffsetZ_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.offsetZ"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.jitterX")) {
      impl_->clonerJitterX_ = finiteClampedValue(
          value.toDouble(), impl_->clonerJitterX_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.jitterX"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.jitterY")) {
      impl_->clonerJitterY_ = finiteClampedValue(
          value.toDouble(), impl_->clonerJitterY_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.jitterY"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.jitterZ")) {
      impl_->clonerJitterZ_ = finiteClampedValue(
          value.toDouble(), impl_->clonerJitterZ_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.jitterZ"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.seed")) {
      impl_->clonerSeed_ = value.toInt();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.seed"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.columns")) {
      impl_->clonerColumns_ = std::max(1, value.toInt());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.columns"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.rows")) {
      impl_->clonerRows_ = std::max(1, value.toInt());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.rows"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.depth")) {
      impl_->clonerDepth_ = std::max(1, value.toInt());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.depth"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.spacingX")) {
      impl_->clonerSpacingX_ = finiteClampedValue(
          value.toDouble(), impl_->clonerSpacingX_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.spacingX"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.spacingY")) {
      impl_->clonerSpacingY_ = finiteClampedValue(
          value.toDouble(), impl_->clonerSpacingY_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.spacingY"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.spacingZ")) {
      impl_->clonerSpacingZ_ = finiteClampedValue(
          value.toDouble(), impl_->clonerSpacingZ_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.spacingZ"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.radialCount")) {
      impl_->clonerRadialCount_ = std::max(1, value.toInt());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.radialCount"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.radius")) {
      impl_->clonerRadius_ = finiteClampedValue(
          value.toDouble(), impl_->clonerRadius_, -100000.0, 100000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.radius"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.startAngle")) {
      impl_->clonerStartAngle_ = finiteClampedValue(
          value.toDouble(), impl_->clonerStartAngle_, -360000.0, 360000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.startAngle"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.endAngle")) {
      impl_->clonerEndAngle_ = finiteClampedValue(
          value.toDouble(), impl_->clonerEndAngle_, -360000.0, 360000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.endAngle"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.rotationStep")) {
      impl_->clonerRotationStep_ = finiteClampedValue(
          value.toDouble(), impl_->clonerRotationStep_, -360000.0, 360000.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.rotationStep"));
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.opacityDecay")) {
      impl_->clonerOpacityDecay_ = finiteClampedValue(
          value.toDouble(), impl_->clonerOpacityDecay_, 0.0, 1.0);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      syncRoutedPropertyCache(QStringLiteral("component.cloner.opacityDecay"));
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
    if (property->hasKeyFrames()) {
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

} // namespace Artifact
