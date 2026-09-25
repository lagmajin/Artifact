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

bool ArtifactAbstractLayer::hasCachedAnimatedPropertiesWithPrefix(
    const QString &propertyPathPrefix) const {
  if (propertyPathPrefix.isEmpty()) return false;
  std::lock_guard<std::mutex> lock(impl_->propertyCacheMutex_);
  for (auto it = impl_->propertyCache_.cbegin();
       it != impl_->propertyCache_.cend(); ++it) {
    const auto &property = it.value();
    if (it.key().startsWith(propertyPathPrefix) && property &&
        (property->hasKeyFrames() || property->hasExpression())) {
      return true;
    }
  }
  return false;
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
      property->isAnimatable() && property->hasKeyFrames();
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
  const QStringList deformationParts = propertyPath.split(QLatin1Char('.'));
  if (deformationParts.size() == 3 &&
      deformationParts[0] == QStringLiteral("deformation2D") &&
      (deformationParts[2] == QStringLiteral("x") ||
       deformationParts[2] == QStringLiteral("y") ||
       deformationParts[2] == QStringLiteral("rotation") ||
       deformationParts[2] == QStringLiteral("weight"))) {
    const double coordinate = value.toDouble();
    if (!std::isfinite(coordinate)) return false;
    const double clampedValue = deformationParts[2] == QStringLiteral("weight")
        ? std::clamp(coordinate, 0.0, 1.0)
        : deformationParts[2] == QStringLiteral("rotation")
            ? std::clamp(coordinate, -180.0, 180.0) : coordinate;
    QJsonObject state = deformation2DData();
    const QString mode = state.value(QStringLiteral("mode")).toString();
    const QString controlsKey = mode == QStringLiteral("grid")
                                    ? QStringLiteral("gridControls")
                                    : QStringLiteral("pins");
    if ((deformationParts[2] == QStringLiteral("rotation") ||
         deformationParts[2] == QStringLiteral("weight")) &&
        mode == QStringLiteral("grid")) return false;
    QJsonArray controls = state.value(controlsKey).toArray();
    for (qsizetype index = 0; index < controls.size(); ++index) {
      if (!controls[index].isObject()) continue;
      QJsonObject control = controls[index].toObject();
      if (control.value(QStringLiteral("id")).toString() !=
          deformationParts[1]) continue;
      const auto cachedProperty = persistentLayerProperty(
          propertyPath, PropertyType::Float,
          QVariant(control.value(deformationParts[2]).toDouble()), 100);
      if (cachedProperty->isAnimatable() &&
          cachedProperty->hasKeyFrames()) {
        cachedProperty->setValue(QVariant(clampedValue));
        QJsonArray keys;
        for (const auto& key : cachedProperty->getKeyFrames()) {
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
        control[deformationParts[2] + QStringLiteral("Keys")] = keys;
        control[deformationParts[2]] = clampedValue;
        controls[index] = control;
        state[controlsKey] = controls;
        setDeformation2DData(state);
        return true;
      }
      control[deformationParts[2]] = clampedValue;
      controls[index] = control;
      state[controlsKey] = controls;
      setDeformation2DData(state);
      cachedProperty->setValue(QVariant(clampedValue));
      return true;
    }
    return false;
  }
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

} // namespace Artifact
