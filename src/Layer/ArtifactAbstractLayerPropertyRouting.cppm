module;
#include <algorithm>
#include <atomic>
#include <QJsonDocument>
#include <QUuid>
#include <OpenImageIO/imageio.h>
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
import Core.ArtifactArray;
import Asset.Manager;
import AssetType;
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

namespace { std::atomic<std::uint64_t> revealPreparationGeneration{1}; }

std::uint64_t ArtifactAbstractLayer::revealPreparationEpoch() noexcept {
  return revealPreparationGeneration.load(std::memory_order_relaxed);
}

ArtifactAbstractLayer::RevealMapView ArtifactAbstractLayer::revealMapView() const {
  return {(impl_->revealMap_.size() == 0) ? nullptr : impl_->revealMap_.data(),
          (impl_->revealMap_.size() == 0) ? 0 : 512, (impl_->revealMap_.size() == 0) ? 0 : 512,
          impl_->revealMapRevision_, impl_->revealMapFailed_};
}

void ArtifactAbstractLayer::rebuildRevealMap() {
  // This API runs only at property mutation / restore boundaries, never for
  // animated Progress or Opacity. Fixed 512x512 RG float map: 2 MiB per layer.
  impl_->revealMapRevision_ = revealPreparationGeneration.fetch_add(1, std::memory_order_relaxed) + 1;
  impl_->revealMapFailed_ = false;
  impl_->revealMap_ = Array<float>{};
  if (!revealEnabled() || impl_->reveal_.pattern < 3) return;
  constexpr int extent = 512;
  impl_->revealMap_.resize(extent * extent * 2, 0.0f);
  if (impl_->reveal_.pattern == 3) {
    QPointF points[256]{};
    bool starts[256]{};
    float distances[256]{};
    int count = 0;
    if (impl_->revealStrokeData_.isEmpty()) {
      points[0] = QPointF(0.15, 0.65); points[1] = QPointF(0.85, 0.35);
      starts[0] = true; count = 2;
      if (impl_->revealBrushPreset_ == 1) {
        count = 6;
        for (int i = 0; i < 3; ++i) {
          points[i*2] = QPointF(0.15, 0.25+i*0.25);
          points[i*2+1] = QPointF(0.85, 0.15+i*0.25);
          starts[i*2] = true;
        }
      } else if (impl_->revealBrushPreset_ == 2) {
        count = 6;
        points[0] = QPointF(0.2,0.2); points[1] = QPointF(0.8,0.2);
        points[2] = QPointF(0.2,0.5); points[3] = QPointF(0.8,0.5);
        points[4] = QPointF(0.2,0.8); points[5] = QPointF(0.8,0.8);
      }
    } else {
      const auto document = QJsonDocument::fromJson(impl_->revealStrokeData_.toUtf8());
      const auto strokes = document.array();
      bool valid = document.isArray();
      for (const auto& stroke : strokes) {
        if (!stroke.isArray() || stroke.toArray().size() < 2) { valid = false; break; }
        bool first = true;
        for (const auto& value : stroke.toArray()) {
          const auto pair = value.toArray();
          if (count >= 256 || pair.size() != 2 || !pair[0].isDouble() || !pair[1].isDouble()) { valid = false; break; }
          const double x = pair[0].toDouble(), y = pair[1].toDouble();
          if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || x > 1 || y < 0 || y > 1) { valid = false; break; }
          points[count] = QPointF(x, y); starts[count] = first; ++count; first = false;
        }
        if (!valid) break;
      }
      if (!valid || count < 2) { impl_->revealMapFailed_ = true; impl_->revealMap_ = Array<float>{}; return; }
    }
    float total = 0.0f;
    for (int i = 1; i < count; ++i) {
      if (!starts[i]) total += static_cast<float>(std::hypot(points[i].x() - points[i-1].x(), points[i].y() - points[i-1].y()));
      distances[i] = total;
    }
    if (total <= 1e-6f) { impl_->revealMapFailed_ = true; impl_->revealMap_ = Array<float>{}; return; }
    for (int y = 0; y < extent; ++y) for (int x = 0; x < extent; ++x) {
      const QPointF p((x + 0.5) / extent, (y + 0.5) / extent);
      float timing = 1.0f, support = 0.0f;
      for (int i = 1; i < count; ++i) {
        if (starts[i]) continue;
        const QPointF delta = points[i] - points[i-1];
        const double length2 = delta.x()*delta.x() + delta.y()*delta.y();
        const QPointF offset = p - points[i-1];
        const double t = length2 <= 1e-12 ? 0.0 : std::clamp((offset.x()*delta.x() + offset.y()*delta.y()) / length2, 0.0, 1.0);
        const QPointF d = offset - delta*t;
        const double distance = std::hypot(d.x(), d.y());
        const float coverage = static_cast<float>(std::clamp((impl_->revealBrushRadius_ - distance) * extent + 0.5, 0.0, 1.0));
        if (coverage <= 0.0f) continue;
        support = std::max(support, coverage);
        timing = std::min(timing, (distances[i-1] + static_cast<float>(t*std::sqrt(length2))) / total);
      }
      const int offset = (y*extent+x)*2;
      impl_->revealMap_[offset] = timing; impl_->revealMap_[offset+1] = support;
    }
    return;
  }
  const auto resolveAsset = [&](QString& path, QUuid& id) {
    auto& manager = AssetManager::instance();
    if (id.isNull() && !path.isEmpty()) id = manager.acquireSource(path, AssetType::Image);
    if (id.isNull()) return;
    const auto sources = manager.sourceRegistrySnapshot().value(QStringLiteral("sources")).toArray();
    for (const auto& source : sources) {
      const auto object = source.toObject();
      if (QUuid(object.value(QStringLiteral("id")).toString()) == id) {
        path = object.value(QStringLiteral("path")).toString(path); break;
      }
    }
  };
  resolveAsset(impl_->revealTimingPath_, impl_->revealTimingAsset_);
  resolveAsset(impl_->revealSupportPath_, impl_->revealSupportAsset_);
  // Custom input is raw channel 0, with no sRGB/color transform. Support is
  // explicitly supplied as a second file; omission means full support.
  const auto loadChannel = [&](const QString& path, int outputChannel) {
    auto input = OIIO::ImageInput::open(path.toStdString());
    if (!input) return false;
    const auto spec = input->spec();
    if (spec.width <= 0 || spec.height <= 0 || spec.width > 2048 || spec.height > 2048 || spec.depth > 1 || spec.nchannels < 1 || spec.nchannels > 4) return false;
    Array<float> pixels;
    pixels.resize(static_cast<std::size_t>(spec.width)*spec.height);
    if (!input->read_image(0, 0, 0, 1, OIIO::TypeDesc::FLOAT, pixels.data())) return false;
    input->close();
    for (int y = 0; y < extent; ++y) for (int x = 0; x < extent; ++x) {
      const int sourceX = std::min(spec.width-1, static_cast<int>((x+0.5)*spec.width/extent));
      const int sourceY = std::min(spec.height-1, static_cast<int>((y+0.5)*spec.height/extent));
      const float value = pixels[sourceY*spec.width+sourceX];
      if (!std::isfinite(value)) return false;
      impl_->revealMap_[(y*extent+x)*2+outputChannel] = std::clamp(value, 0.0f, 1.0f);
    }
    return true;
  };
  if (impl_->revealSupportPath_.isEmpty()) {
    for (int i = 0; i < extent*extent; ++i) impl_->revealMap_[i*2+1] = 1.0f;
  }
  if (!loadChannel(impl_->revealTimingPath_, 0) ||
      (!impl_->revealSupportPath_.isEmpty() && !loadChannel(impl_->revealSupportPath_, 1))) {
    impl_->revealMapFailed_ = true; impl_->revealMap_ = Array<float>{};
  }
}

bool ArtifactAbstractLayer::revealEnabled() const {
  return impl_->reveal_.enabled && supportsReveal() && !is3D() && !isAdjustmentLayer();
}

LayerRevealSettings ArtifactAbstractLayer::revealSettings() const {
  auto settings = impl_->reveal_;
  settings.enabled = revealEnabled();
  if (settings.enabled && impl_->revealProgress_) {
    const double progress = evaluateAnimatedPropertyValue(
        *impl_->revealProgress_, currentTimelineTime(this)).toDouble();
    settings.progress = static_cast<float>(finiteClampedValue(progress, 1.0, 0.0, 1.0));
  }
  return settings;
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

namespace {
// Descriptor of a property the Inspector actually exposes. The routing
// setters below mutate impl_ fields, but the cached AbstractProperty keeps its
// own copy, and ArtifactPropertyWidget only records an undo command when
// before != after. Without a write-back the cached copy never moves, so every
// physics / component / fracture edit compared equal and Ctrl+Z did nothing.
// getLayerPropertyGroups() and getComponentPropertyGroups() already build these
// with the correct type, priority and clamped value; the table below mirrors
// those entries so the setters can refresh the cache in place.
struct RoutedPropertyMeta {
  PropertyType type;
  int priority;
  QVariant (*read)(const ArtifactAbstractLayerImpl& impl);
};

const QHash<QString, RoutedPropertyMeta>& routedPropertyMetaTable() {
  static const QHash<QString, RoutedPropertyMeta> table = {
      // --- Physics group (ArtifactAbstractLayerPropertyGroups.cppm) ---
      {QStringLiteral("physics.enabled"),
       {PropertyType::Boolean, -100,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.physicsComponent_.settings().enabled);
        }}},
      {QStringLiteral("physics.softBody.enabled"),
       {PropertyType::Boolean, -99,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.softBodyPhysicsEnabled_);
        }}},
      {QStringLiteral("physics.cloth3D.enabled"),
       {PropertyType::Boolean, -99,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.cloth3DPhysicsEnabled_);
        }}},
      {QStringLiteral("physics.stiffness"),
       {PropertyType::Float, -99,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(
              i.physicsComponent_.settings().stiffness));
        }}},
      {QStringLiteral("physics.material.enabled"),
       {PropertyType::Boolean, -98,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.materialPhysicsEnabled_);
        }}},
      {QStringLiteral("physics.damping"),
       {PropertyType::Float, -98,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(
              i.physicsComponent_.settings().damping));
        }}},
      {QStringLiteral("physics.followThroughGain"),
       {PropertyType::Float, -97,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(
              i.physicsComponent_.settings().followThroughGain));
        }}},
      {QStringLiteral("physics.material.preset"),
       {PropertyType::Integer, -97,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.materialPhysicsPreset_);
        }}},
      {QStringLiteral("physics.gravityY"),
       {PropertyType::Float, -96,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(
              i.physicsComponent_.settings().gravityY));
        }}},
      {QStringLiteral("physics.linearDamping"),
       {PropertyType::Float, -95,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(
              i.physicsComponent_.settings().linearDamping));
        }}},
      {QStringLiteral("physics.angularDamping"),
       {PropertyType::Float, -94,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(
              i.physicsComponent_.settings().angularDamping));
        }}},
      {QStringLiteral("physics.restitution"),
       {PropertyType::Float, -94,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(
              i.physicsComponent_.settings().restitution));
        }}},
      {QStringLiteral("physics.gravityScale"),
       {PropertyType::Float, -93,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(
              i.physicsComponent_.settings().gravityScale));
        }}},
      {QStringLiteral("physics.wiggleAmp"),
       {PropertyType::Float, -93,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(
              i.physicsComponent_.settings().wiggleAmp));
        }}},
      {QStringLiteral("physics.wiggleFreq"),
       {PropertyType::Float, -93,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(
              i.physicsComponent_.settings().wiggleFreq));
        }}},
      {QStringLiteral("physics.wind.enabled"),
       {PropertyType::Boolean, -92,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.physicsComponent_.settings().windEnabled);
        }}},
      {QStringLiteral("physics.wind.strength"),
       {PropertyType::Float, -92,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(
              i.physicsComponent_.settings().windStrength));
        }}},
      {QStringLiteral("physics.wind.torque"),
       {PropertyType::Float, -92,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(
              i.physicsComponent_.settings().windTorque));
        }}},
      {QStringLiteral("physics.wind.x"),
       {PropertyType::Float, -92,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(
              i.physicsComponent_.settings().windX));
        }}},
      {QStringLiteral("physics.wind.y"),
       {PropertyType::Float, -92,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(
              i.physicsComponent_.settings().windY));
        }}},
      {QStringLiteral("physics.initialVelocityY"),
       {PropertyType::Float, -92,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.clonePhysicsInitialVelocityY_));
        }}},
      {QStringLiteral("physics.maxBounces"),
       {PropertyType::Integer, -91,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.clonePhysicsMaxBounces_);
        }}},
      {QStringLiteral("physics.fallProfile"),
       {PropertyType::Integer, -96,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.physicsComponent_.settings().fallProfile);
        }}},
      // --- Motion group ---
      {QStringLiteral("motion.enabled"),
       {PropertyType::Boolean, -92,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.motionDynamicsEnabled_);
        }}},
      {QStringLiteral("motion.mode"),
       {PropertyType::Integer, -91,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.motionDynamicsMode_);
        }}},
      {QStringLiteral("motion.stiffness"),
       {PropertyType::Float, -90,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.motionDynamicsStiffness_));
        }}},
      {QStringLiteral("motion.damping"),
       {PropertyType::Float, -89,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.motionDynamicsDamping_));
        }}},
      {QStringLiteral("motion.mass"),
       {PropertyType::Float, -88,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.motionDynamicsMass_));
        }}},
      {QStringLiteral("motion.lagTau"),
       {PropertyType::Float, -87,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.motionDynamicsLagTau_));
        }}},
      {QStringLiteral("motion.clampOvershoot"),
       {PropertyType::Boolean, -86,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.motionDynamicsClampOvershoot_);
        }}},
      {QStringLiteral("motion.overshootLimit"),
       {PropertyType::Float, -85,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(
              i.motionDynamicsOvershootLimit_));
        }}},
      // --- Components group ---
      {QStringLiteral("component.script.enabled"),
       {PropertyType::Boolean, -100,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.scriptComponentEnabled_);
        }}},
      {QStringLiteral("component.cloner.enabled"),
       {PropertyType::Boolean, -90,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.clonerComponentEnabled_);
        }}},
      {QStringLiteral("component.collision.enabled"),
       {PropertyType::Boolean, -89,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.collisionComponentEnabled_);
        }}},
      {QStringLiteral("component.collision.displayColor"),
       {PropertyType::Color, -88,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.collisionDisplayColor_);
        }}},
      {QStringLiteral("component.crowd.enabled"),
       {PropertyType::Boolean, -88,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.crowdComponentEnabled_);
        }}},
      {QStringLiteral("component.particleEmitter.enabled"),
       {PropertyType::Boolean, -87,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.particleEmitterComponentEnabled_);
        }}},
      {QStringLiteral("component.fluid.enabled"),
       {PropertyType::Boolean, -86,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.fluidComponentEnabled_);
        }}},
      // --- Pyro group ---
      {QStringLiteral("component.pyro.enabled"),
       {PropertyType::Boolean, 0,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.pyroComponentEnabled_);
        }}},
      {QStringLiteral("component.pyro.gridWidth"),
       {PropertyType::Integer, 1,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.pyroGridWidth_);
        }}},
      {QStringLiteral("component.pyro.gridHeight"),
       {PropertyType::Integer, 1,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.pyroGridHeight_);
        }}},
      {QStringLiteral("component.pyro.gridDepth"),
       {PropertyType::Integer, 1,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.pyroGridDepth_);
        }}},
      {QStringLiteral("component.pyro.voxelSize"),
       {PropertyType::Float, 2,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroVoxelSize_));
        }}},
      {QStringLiteral("component.pyro.boundaryMode"),
       {PropertyType::Integer, 3,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<int>(i.pyroBoundaryMode_));
        }}},
      {QStringLiteral("component.pyro.sourceDensity"),
       {PropertyType::Float, 4,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroSourceDensity_));
        }}},
      {QStringLiteral("component.pyro.sourceTemperature"),
       {PropertyType::Float, 5,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroSourceTemperature_));
        }}},
      {QStringLiteral("component.pyro.sourceFuel"),
       {PropertyType::Float, 6,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroSourceFuel_));
        }}},
      {QStringLiteral("component.pyro.dissipation"),
       {PropertyType::Float, 7,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroDissipation_));
        }}},
      {QStringLiteral("component.pyro.coolingRate"),
       {PropertyType::Float, 8,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroCoolingRate_));
        }}},
      {QStringLiteral("component.pyro.buoyancy"),
       {PropertyType::Float, 9,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroBuoyancy_));
        }}},
      {QStringLiteral("component.pyro.vorticity"),
       {PropertyType::Float, 10,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroVorticity_));
        }}},
      {QStringLiteral("component.pyro.pressureIterations"),
       {PropertyType::Float, 11,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroPressureIterations_));
        }}},
      {QStringLiteral("component.pyro.advectionClamp"),
       {PropertyType::Float, 12,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroAdvectionClamp_));
        }}},
      {QStringLiteral("component.pyro.sourcePositionX"),
       {PropertyType::Float, 13,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroSourcePositionX_));
        }}},
      {QStringLiteral("component.pyro.sourcePositionY"),
       {PropertyType::Float, 14,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroSourcePositionY_));
        }}},
      {QStringLiteral("component.pyro.sourcePositionZ"),
       {PropertyType::Float, 15,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroSourcePositionZ_));
        }}},
      {QStringLiteral("component.pyro.sourceExtentX"),
       {PropertyType::Float, 16,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroSourceExtentX_));
        }}},
      {QStringLiteral("component.pyro.sourceExtentY"),
       {PropertyType::Float, 17,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroSourceExtentY_));
        }}},
      {QStringLiteral("component.pyro.sourceExtentZ"),
       {PropertyType::Float, 18,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroSourceExtentZ_));
        }}},
      {QStringLiteral("component.pyro.sourceVelocityX"),
       {PropertyType::Float, 19,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroSourceVelocityX_));
        }}},
      {QStringLiteral("component.pyro.sourceVelocityY"),
       {PropertyType::Float, 20,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroSourceVelocityY_));
        }}},
      {QStringLiteral("component.pyro.sourceVelocityZ"),
       {PropertyType::Float, 21,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.pyroSourceVelocityZ_));
        }}},
      // --- Fracture group ---
      {QStringLiteral("fracture.enabled"),
       {PropertyType::Boolean, -84,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.fractureEnabled_);
        }}},
      {QStringLiteral("fracture.preGenerate"),
       {PropertyType::Boolean, -835,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.fracturePreGenerate_);
        }}},
      {QStringLiteral("fracture.preset"),
       {PropertyType::Integer, -83,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.fracturePreset_);
        }}},
      {QStringLiteral("fracture.crackThreshold"),
       {PropertyType::Float, -82,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.fractureCrackThreshold_));
        }}},
      {QStringLiteral("fracture.shatterThreshold"),
       {PropertyType::Float, -81,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.fractureShatterThreshold_));
        }}},
      {QStringLiteral("fracture.shardCount"),
       {PropertyType::Integer, -80,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.fractureShardCount_);
        }}},
      {QStringLiteral("fracture.shardDamping"),
       {PropertyType::Float, -79,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.fractureShardDamping_));
        }}},
      {QStringLiteral("fracture.shardGravity"),
       {PropertyType::Float, -78,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.fractureShardGravity_));
        }}},
      {QStringLiteral("fracture.impactSensitivity"),
       {PropertyType::Float, -77,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.fractureImpactSensitivity_));
        }}},
      // --- Trail group ---
      {QStringLiteral("trail.enabled"),
       {PropertyType::Boolean, -76,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.motionTrailEnabled_);
        }}},
      {QStringLiteral("trail.length"),
       {PropertyType::Integer, -75,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(i.motionTrailLength_);
        }}},
      {QStringLiteral("trail.fade"),
       {PropertyType::Float, -74,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.motionTrailFade_));
        }}},
      {QStringLiteral("trail.width"),
       {PropertyType::Float, -73,
        [](const ArtifactAbstractLayerImpl& i) {
          return QVariant(static_cast<double>(i.motionTrailWidth_));
        }}},
  };
  return table;
}
} // namespace

void ArtifactAbstractLayer::syncRoutedPropertyCache(
    const QString &propertyPath) const {
  const auto &table = routedPropertyMetaTable();
  const auto meta = table.constFind(propertyPath);
  if (meta == table.cend()) {
    return;
  }
  persistentLayerProperty(propertyPath, meta->type, meta->read(*impl_),
                          meta->priority);
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
  if (propertyPath.startsWith(QStringLiteral("reveal."))) {
    auto& r = impl_->reveal_;
    if (propertyPath == QStringLiteral("reveal.enabled")) {
      if (r.enabled != value.toBool()) revealPreparationGeneration.fetch_add(1, std::memory_order_relaxed);
      r.enabled = value.toBool();
    }
    else if (propertyPath == QStringLiteral("reveal.pattern")) r.pattern = std::clamp(value.toInt(), 0, 4);
    else if (propertyPath == QStringLiteral("reveal.progress")) {
      r.progress = static_cast<float>(finiteClampedValue(value.toDouble(), 1.0, 0.0, 1.0));
      impl_->revealProgress_ = persistentLayerProperty(propertyPath, PropertyType::Float, r.progress);
      impl_->revealProgress_->setAnimatable(true);
      impl_->revealProgress_->setValue(r.progress);
      if (impl_->revealProgress_->hasKeyFrames()) {
        impl_->revealProgress_->addKeyFrame(currentTimelineTime(this), r.progress);
      }
    }
    else if (propertyPath == QStringLiteral("reveal.softness")) r.softness = static_cast<float>(finiteClampedValue(value.toDouble(), 0.0, 0.0, 1.0));
    else if (propertyPath == QStringLiteral("reveal.reverse")) r.reverse = value.toBool();
    else if (propertyPath == QStringLiteral("reveal.angle")) r.angle = static_cast<float>(finiteClampedValue(value.toDouble(), 0.0, -360.0, 360.0));
    else if (propertyPath == QStringLiteral("reveal.centerX")) r.centerX = static_cast<float>(finiteClampedValue(value.toDouble(), 0.5, 0.0, 1.0));
    else if (propertyPath == QStringLiteral("reveal.centerY")) r.centerY = static_cast<float>(finiteClampedValue(value.toDouble(), 0.5, 0.0, 1.0));
    else if (propertyPath == QStringLiteral("reveal.seed")) r.seed = std::clamp(value.toInt(), 0, 16777215);
    else if (propertyPath == QStringLiteral("reveal.brushRadius")) impl_->revealBrushRadius_ = static_cast<float>(finiteClampedValue(value.toDouble(), 0.16, 0.005, 0.5));
    else if (propertyPath == QStringLiteral("reveal.brushPreset")) {
      impl_->revealBrushPreset_ = std::clamp(value.toInt(), 0, 2);
    }
    else if (propertyPath == QStringLiteral("reveal.strokeData")) {
      if (value.toString().size() > 32768) return false;
      impl_->revealStrokeData_ = value.toString();
    }
    else if (propertyPath == QStringLiteral("reveal.timingPath")) {
      AssetManager::instance().releaseSource(impl_->revealTimingAsset_);
      impl_->revealTimingAsset_ = {};
      impl_->revealTimingPath_ = value.toString().left(32768);
    }
    else if (propertyPath == QStringLiteral("reveal.supportPath")) {
      AssetManager::instance().releaseSource(impl_->revealSupportAsset_);
      impl_->revealSupportAsset_ = {};
      impl_->revealSupportPath_ = value.toString().left(32768);
    }
    else return false;
    if (propertyPath == QStringLiteral("reveal.enabled") ||
        propertyPath == QStringLiteral("reveal.pattern") ||
        propertyPath == QStringLiteral("reveal.brushRadius") ||
        propertyPath == QStringLiteral("reveal.brushPreset") ||
        propertyPath == QStringLiteral("reveal.strokeData") ||
        propertyPath == QStringLiteral("reveal.timingPath") ||
        propertyPath == QStringLiteral("reveal.supportPath")) rebuildRevealMap();
    if (const auto property = getProperty(propertyPath)) {
      QVariant stored = value;
      if (propertyPath == QStringLiteral("reveal.enabled")) stored = r.enabled;
      else if (propertyPath == QStringLiteral("reveal.pattern")) stored = r.pattern;
      else if (propertyPath == QStringLiteral("reveal.softness")) stored = r.softness;
      else if (propertyPath == QStringLiteral("reveal.reverse")) stored = r.reverse;
      else if (propertyPath == QStringLiteral("reveal.angle")) stored = r.angle;
      else if (propertyPath == QStringLiteral("reveal.centerX")) stored = r.centerX;
      else if (propertyPath == QStringLiteral("reveal.centerY")) stored = r.centerY;
      else if (propertyPath == QStringLiteral("reveal.seed")) stored = r.seed;
      else if (propertyPath == QStringLiteral("reveal.brushRadius")) stored = impl_->revealBrushRadius_;
      else if (propertyPath == QStringLiteral("reveal.brushPreset")) stored = impl_->revealBrushPreset_;
      else if (propertyPath == QStringLiteral("reveal.timingPath")) stored = impl_->revealTimingPath_;
      else if (propertyPath == QStringLiteral("reveal.supportPath")) stored = impl_->revealSupportPath_;
      if (propertyPath != QStringLiteral("reveal.progress")) property->setValue(stored);
    }
    notifyLayerMutation(this, LayerDirtyFlag::Property, LayerDirtyReason::PropertyChanged);
    return true;
  }
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
    const QString controlsKey =
        LayerAbstractUtilities::deformationControlsKey(mode);
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
               static_cast<int>(key.colorLabel)},
              {QStringLiteral("soft"), key.soft}});
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
      path.setFeather({static_cast<float>(value.toDouble())});
    } else if (maskAddress->field == QStringLiteral("featherHorizontal")) {
      path.setFeatherHorizontal({static_cast<float>(value.toDouble())});
    } else if (maskAddress->field == QStringLiteral("featherVertical")) {
      path.setFeatherVertical({static_cast<float>(value.toDouble())});
    } else if (maskAddress->field == QStringLiteral("featherInner")) {
      path.setFeatherInner({static_cast<float>(value.toDouble())});
    } else if (maskAddress->field == QStringLiteral("featherOuter")) {
      path.setFeatherOuter({static_cast<float>(value.toDouble())});
    } else if (maskAddress->field == QStringLiteral("falloff")) {
      path.setFalloff(static_cast<MaskFeatherFalloff>(value.toInt()));
    } else if (maskAddress->field == QStringLiteral("expansion")) {
      path.setExpansion({static_cast<float>(value.toDouble())});
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
    syncRoutedPropertyCache(QStringLiteral("physics.softBody.enabled"));
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
    syncRoutedPropertyCache(QStringLiteral("physics.cloth3D.enabled"));
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
    syncRoutedPropertyCache(QStringLiteral("physics.material.enabled"));
    return true;
  }
  if (propertyPath == QStringLiteral("physics.material.preset")) {
    impl_->materialPhysicsPreset_ = std::clamp(value.toInt(), 0, 3);
    if (impl_->materialPhysicsEnabled_) {
      enableMaterialPhysics(impl_->materialPhysicsPreset_);
    }
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    syncRoutedPropertyCache(QStringLiteral("physics.material.preset"));
    return true;
  }
  if (propertyPath == QStringLiteral("physics.enabled")) {
    const bool enabled = value.toBool();
    // syncBuiltinComponentDescriptors() rebuilds every descriptor, so skip it
    // when the flag would not move. collisionComponentEnabled_ can force
    // physics on independently, so compare the state that would result rather
    // than the requested value alone.
    const bool alreadyEnabled = impl_->physicsComponent_.enabled();
    const bool wouldBeEnabled = enabled || impl_->collisionComponentEnabled_;
    if (alreadyEnabled == wouldBeEnabled &&
        (!enabled || !impl_->collisionOwnsPhysicsEnable_)) {
      return true;
    }
    impl_->physicsComponent_.setEnabled(enabled);
    if (enabled) {
      impl_->collisionOwnsPhysicsEnable_ = false;
    }
    impl_->syncBuiltinComponentDescriptors();
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    syncRoutedPropertyCache(QStringLiteral("physics.enabled"));
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
    const bool enabled = value.toBool();
    if (impl_->motionDynamicsEnabled_ == enabled) {
      return true;
    }
    impl_->motionDynamicsEnabled_ = enabled;
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    syncRoutedPropertyCache(QStringLiteral("motion.enabled"));
    return true;
  }
  if (propertyPath == QStringLiteral("motion.mode")) {
    const int mode = std::clamp(value.toInt(), 0, 2);
    if (impl_->motionDynamicsMode_ == mode) {
      return true;
    }
    impl_->motionDynamicsMode_ = mode;
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    syncRoutedPropertyCache(QStringLiteral("motion.mode"));
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
    const bool enabled = value.toBool();
    if (impl_->motionTrailEnabled_ == enabled) {
      return true;
    }
    impl_->motionTrailEnabled_ = enabled;
    impl_->motionTrailHistory_.clear();
    impl_->motionTrailLastFrame_ = std::numeric_limits<int64_t>::min();
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    syncRoutedPropertyCache(QStringLiteral("trail.enabled"));
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
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    syncRoutedPropertyCache(QStringLiteral("fracture.enabled"));
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.preGenerate")) {
    const bool enabled = value.toBool();
    if (impl_->fracturePreGenerate_ != enabled) {
      impl_->fracturePreGenerate_ = enabled;
      resetFractureState();
    }
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    syncRoutedPropertyCache(QStringLiteral("fracture.preGenerate"));
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.triggerFrame")) {
    const int64_t triggerFrame = std::max<int64_t>(-1, value.toLongLong());
    if (impl_->fractureTriggerFrame_ != triggerFrame) {
      impl_->fractureTriggerFrame_ = triggerFrame;
      resetFractureState();
    }
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.preset")) {
    const int preset = std::clamp(value.toInt(), 0, static_cast<int>(FracturePreset::Dust));
    if (impl_->fracturePreset_ != preset) {
      impl_->fracturePreset_ = preset;
      resetFractureState();
    }
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    syncRoutedPropertyCache(QStringLiteral("fracture.preset"));
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.crackThreshold")) {
    const float threshold = finiteClampedValue(
        value.toDouble(), impl_->fractureCrackThreshold_, 0.0, 1000.0);
    if (impl_->fractureCrackThreshold_ != threshold) {
      impl_->fractureCrackThreshold_ = threshold;
      resetFractureState();
    }
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    syncRoutedPropertyCache(QStringLiteral("fracture.crackThreshold"));
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.shatterThreshold")) {
    const float threshold = finiteClampedValue(
        value.toDouble(), impl_->fractureShatterThreshold_, 0.0, 1000.0);
    if (impl_->fractureShatterThreshold_ != threshold) {
      impl_->fractureShatterThreshold_ = threshold;
      resetFractureState();
    }
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    syncRoutedPropertyCache(QStringLiteral("fracture.shatterThreshold"));
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.shardCount")) {
    const int shardCount = std::clamp(value.toInt(), 1, 256);
    if (impl_->fractureShardCount_ != shardCount) {
      impl_->fractureShardCount_ = shardCount;
      resetFractureState();
    }
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    syncRoutedPropertyCache(QStringLiteral("fracture.shardCount"));
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.shardDamping")) {
    const float damping = finiteClampedValue(
        value.toDouble(), impl_->fractureShardDamping_, 0.0, 1.0);
    if (impl_->fractureShardDamping_ != damping) {
      impl_->fractureShardDamping_ = damping;
      resetFractureState();
    }
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    syncRoutedPropertyCache(QStringLiteral("fracture.shardDamping"));
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.shardGravity")) {
    const float gravity = finiteClampedValue(
        value.toDouble(), impl_->fractureShardGravity_, -5000.0, 5000.0);
    if (impl_->fractureShardGravity_ != gravity) {
      impl_->fractureShardGravity_ = gravity;
      resetFractureState();
    }
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    syncRoutedPropertyCache(QStringLiteral("fracture.shardGravity"));
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.impactSensitivity")) {
    const float sensitivity = finiteClampedValue(
        value.toDouble(), impl_->fractureImpactSensitivity_, 0.0, 10.0);
    if (impl_->fractureImpactSensitivity_ != sensitivity) {
      impl_->fractureImpactSensitivity_ = sensitivity;
      resetFractureState();
    }
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    syncRoutedPropertyCache(QStringLiteral("fracture.impactSensitivity"));
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
