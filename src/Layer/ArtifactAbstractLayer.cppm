module;
#include <algorithm>
#include <compare>
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
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QVector3D>
#include <QVector4D>
#include <QStringList>
#include <wobjectcpp.h>
#include <wobjectimpl.h>

// JSON and QVariant used in serialization
#include <QColor>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariant>
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <limits>

module Artifact.Layer.Abstract;

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
import Geometry.Fracture;
import Physics.Fluid;
import Physics.SoftBody;
import Physics.System;
import Physics.Mpm2D;
import Layer.Matte;
import Artifact.Composition.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
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
import Artifact.Event.Types;
import Event.Bus;

namespace Artifact {

using namespace ArtifactCore;

using float4x4 = Diligent::float4x4;

W_OBJECT_IMPL(ArtifactAbstractLayer)

QJsonObject layerEffectEnvelopeToJson(const LayerEffectEnvelope &envelope) {
  QJsonObject obj;
  obj["enabled"] = envelope.enabled;
  obj["entry"] = envelope.entry;
  obj["exit"] = envelope.exit;
  obj["timing"] = static_cast<int>(envelope.timing);
  obj["curve"] = static_cast<int>(envelope.curve);
  obj["durationFrames"] = static_cast<qint64>(envelope.durationFrames);
  obj["effectStart"] = static_cast<double>(envelope.effectStart);
  obj["effectEnd"] = static_cast<double>(envelope.effectEnd);
  return obj;
}

LayerEffectEnvelope layerEffectEnvelopeFromJson(const QJsonObject &obj) {
  LayerEffectEnvelope envelope;
  envelope.enabled = obj.value(QStringLiteral("enabled")).toBool(false);
  envelope.entry = obj.value(QStringLiteral("entry")).toBool(false);
  envelope.exit = obj.value(QStringLiteral("exit")).toBool(false);
  const int timing = std::clamp(obj.value(QStringLiteral("timing")).toInt(0), 0, 2);
  envelope.timing = static_cast<LayerEnvelopeTiming>(timing);
  const int curve = std::clamp(obj.value(QStringLiteral("curve")).toInt(0), 0, 4);
  envelope.curve = static_cast<LayerEnvelopeCurve>(curve);
  envelope.durationFrames = std::max<std::int64_t>(
      1, obj.value(QStringLiteral("durationFrames")).toVariant().toLongLong());
  envelope.effectStart = static_cast<float>(
      std::clamp(obj.value(QStringLiteral("effectStart")).toDouble(0.0), 0.0, 1.0));
  envelope.effectEnd = static_cast<float>(
      std::clamp(obj.value(QStringLiteral("effectEnd")).toDouble(1.0), 0.0, 1.0));
  return envelope;
}

float applyLayerEffectEnvelopeOpacity(const LayerEffectEnvelope &envelope,
                                      const float opacity,
                                      const std::int64_t currentFrame,
                                      const FramePosition &inPoint,
                                      const FramePosition &outPoint,
                                      const FramePosition &startTime) {
  if (!envelope.enabled || envelope.durationFrames <= 0) {
    return std::clamp(opacity, 0.0f, 1.0f);
  }

  const std::int64_t layerDuration =
      std::max<std::int64_t>(0, outPoint.framePosition() - inPoint.framePosition());
  const std::int64_t visibleFrame = currentFrame - startTime.framePosition();
  const std::int64_t clampedVisibleFrame =
      std::clamp<std::int64_t>(visibleFrame, 0, layerDuration);
  const std::int64_t duration =
      std::max<std::int64_t>(1, envelope.durationFrames);

  float multiplier = 1.0f;
  if (envelope.entry && clampedVisibleFrame < duration) {
    multiplier *= envelope.sample(clampedVisibleFrame, false).opacity;
  }
  if (envelope.exit) {
    const std::int64_t framesToEnd = layerDuration - clampedVisibleFrame;
    if (framesToEnd <= duration) {
      multiplier *= envelope.sample(framesToEnd, false).opacity;
    }
  }
  return std::clamp(opacity * multiplier, 0.0f, 1.0f);
}

bool isTimelineHiddenLayerPropertyGroup(const QString &groupName) {
  const QString normalized = groupName.trimmed();
  return normalized.compare(QStringLiteral("Transform"),
                            Qt::CaseInsensitive) != 0;
}

bool isTimelineExpandedByDefaultLayerPropertyGroup(const QString &groupName) {
  const QString normalized = groupName.trimmed();
  if (normalized.compare(QStringLiteral("Transform"), Qt::CaseInsensitive) == 0) {
    return true;
  }
  // The standard timeline property surface is Transform-only.
  return false;
}

bool isInspectorHiddenLayerPropertyGroup(const QString &groupName) {
  const QString normalized = groupName.trimmed();
  return normalized.compare(QStringLiteral("Rig"), Qt::CaseInsensitive) == 0 ||
         normalized.compare(QStringLiteral("Rig Controls"), Qt::CaseInsensitive) == 0;
}

bool isInspectorExpandedByDefaultLayerPropertyGroup(const QString &groupName) {
  return groupName.trimmed().compare(QStringLiteral("Initial"),
                                     Qt::CaseInsensitive) == 0;
}

bool isClonerLayerPropertyGroup(const QString &groupName) {
  return groupName.trimmed().compare(QStringLiteral("Cloner"),
                                     Qt::CaseInsensitive) == 0;
}

bool isSourceReframeLayerPropertyGroup(const QString &groupName) {
  return groupName.trimmed().compare(QStringLiteral("Source Reframe"),
                                     Qt::CaseInsensitive) == 0;
}

namespace {
struct ClonerTransformOperation {
  QString name = QStringLiteral("Transform");
  bool enabled = true;
  QVector3D position{0.0f, 0.0f, 0.0f};
  QVector3D rotation{0.0f, 0.0f, 0.0f};
  QVector3D scale{1.0f, 1.0f, 1.0f};
};

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
  const auto *comp =
      dynamic_cast<const ArtifactAbstractComposition *>(layer->compositionObject());
  ArtifactCore::globalEventBus().publish(LayerChangedEvent{
      comp ? comp->id().toString() : QString{},
      layer->id().toString(),
      LayerChangedEvent::ChangeType::Modified});
  Q_EMIT layer->changed();
}

void applyCompositionTransformFields(
    const ArtifactAbstractLayer* layer, double& positionX, double& positionY,
    double& scaleX, double& scaleY) {
  if (!layer) {
    return;
  }
  const auto* composition =
      dynamic_cast<const ArtifactAbstractComposition*>(layer->compositionObject());
  if (!composition) {
    return;
  }
  const auto adjustment = composition->evaluateTransformFields(
      layer->id(), QPointF(positionX, positionY));
  if (!adjustment.affected) {
    return;
  }
  positionX += adjustment.positionOffset.x();
  positionY += adjustment.positionOffset.y();
  scaleX *= adjustment.scaleMultiplier;
  scaleY *= adjustment.scaleMultiplier;
}

QRectF mapRectWithMatrix(const QMatrix4x4 &matrix, const QRectF &rect) {
  if (!rect.isValid() || rect.width() <= 0.0 || rect.height() <= 0.0) {
    return QRectF();
  }

  const QVector4D corners[] = {
      QVector4D(static_cast<float>(rect.left()), static_cast<float>(rect.top()),
                0.0f, 1.0f),
      QVector4D(static_cast<float>(rect.right()), static_cast<float>(rect.top()),
                0.0f, 1.0f),
      QVector4D(static_cast<float>(rect.right()),
                static_cast<float>(rect.bottom()), 0.0f, 1.0f),
      QVector4D(static_cast<float>(rect.left()),
                static_cast<float>(rect.bottom()), 0.0f, 1.0f)};

  float minX = std::numeric_limits<float>::infinity();
  float minY = std::numeric_limits<float>::infinity();
  float maxX = -std::numeric_limits<float>::infinity();
  float maxY = -std::numeric_limits<float>::infinity();

  for (const auto &corner : corners) {
    const QVector4D mapped = matrix * corner;
    minX = std::min(minX, mapped.x());
    minY = std::min(minY, mapped.y());
    maxX = std::max(maxX, mapped.x());
    maxY = std::max(maxY, mapped.y());
  }

  if (!std::isfinite(minX) || !std::isfinite(minY) || !std::isfinite(maxX) ||
      !std::isfinite(maxY) || maxX <= minX || maxY <= minY) {
    return QRectF();
  }

  return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

QMatrix4x4 matrixFromTransform2D(const QTransform& transform) {
  return QMatrix4x4(
      static_cast<float>(transform.m11()), static_cast<float>(transform.m21()), 0.0f, static_cast<float>(transform.m31()),
      static_cast<float>(transform.m12()), static_cast<float>(transform.m22()), 0.0f, static_cast<float>(transform.m32()),
      0.0f,                               0.0f,                               1.0f, 0.0f,
      static_cast<float>(transform.m13()), static_cast<float>(transform.m23()), 0.0f, static_cast<float>(transform.m33()));
}

QString slugifyEffectId(const QString &text) {
  QString slug;
  slug.reserve(text.size());
  bool lastWasDash = false;
  for (const QChar ch : text.trimmed().toLower()) {
    if (ch.isLetterOrNumber()) {
      slug.append(ch);
      lastWasDash = false;
    } else if (!slug.isEmpty() && !lastWasDash) {
      slug.append(QChar('-'));
      lastWasDash = true;
    }
  }
  while (slug.endsWith(QChar('-'))) {
    slug.chop(1);
  }
  if (slug.isEmpty()) {
    slug = QStringLiteral("effect");
  }
  return slug;
}

QString uniqueEffectIdForLayer(
    const std::vector<std::shared_ptr<ArtifactAbstractEffect>> &effects,
    const QString &displayName, const QString &preferredId) {
  QString baseId = preferredId.trimmed();
  if (baseId.isEmpty()) {
    baseId = slugifyEffectId(displayName);
  }
  if (baseId.isEmpty()) {
    baseId = QStringLiteral("effect");
  }

  auto idExists = [&effects](const QString &candidate) {
    return std::any_of(
        effects.begin(), effects.end(),
        [&candidate](const std::shared_ptr<ArtifactAbstractEffect> &effect) {
          return effect && effect->effectID().toQString() == candidate;
        });
  };

  if (!idExists(baseId)) {
    return baseId;
  }

  QString uniqueId = baseId;
  int suffix = 2;
  while (idExists(uniqueId)) {
    uniqueId = QStringLiteral("%1-%2").arg(baseId).arg(suffix++);
  }
  return uniqueId;
}

QString uniqueModifierIdForLayer(
    const std::vector<std::shared_ptr<ArtifactLayerModifier>> &modifiers,
    const QString &displayName, const QString &preferredId) {
  QString baseId = preferredId.trimmed();
  if (baseId.isEmpty()) {
    baseId = slugifyEffectId(displayName);
  }
  if (baseId.isEmpty()) {
    baseId = QStringLiteral("modifier");
  }

  auto idExists = [&modifiers](const QString &candidate) {
    return std::any_of(
        modifiers.begin(), modifiers.end(),
        [&candidate](const std::shared_ptr<ArtifactLayerModifier> &modifier) {
          return modifier && modifier->modifierId() == candidate;
        });
  };

  if (!idExists(baseId)) {
    return baseId;
  }

  QString uniqueId = baseId;
  int suffix = 2;
  while (idExists(uniqueId)) {
    uniqueId = QStringLiteral("%1-%2").arg(baseId).arg(suffix++);
  }
  return uniqueId;
}

double effectiveLayerFrameRate(const ArtifactAbstractLayer *layer) {
  if (!layer) {
    return 30.0;
  }
  auto *composition =
      dynamic_cast<ArtifactAbstractComposition *>(layer->compositionObject());
  if (!composition) {
    return 30.0;
  }
  const double fps = composition->frameRate().framerate();
  return fps > 0.0 ? fps : 30.0;
}

int64_t currentTimelineFrame(const ArtifactAbstractLayer *layer) {
  if (!layer) {
    return 0;
  }
  auto *composition =
      dynamic_cast<ArtifactAbstractComposition *>(layer->compositionObject());
  if (!composition) {
    return layer->currentFrame();
  }
  return composition->framePosition().framePosition();
}

RationalTime currentTimelineTime(const ArtifactAbstractLayer *layer) {
  return RationalTime(currentTimelineFrame(layer), effectiveLayerFrameRate(layer));
}

RationalTime timelineTimeForFramePosition(const ArtifactAbstractLayer *layer,
                                          const FramePosition &position) {
  return RationalTime(position.framePosition(), effectiveLayerFrameRate(layer));
}

struct MaskPropertyAddress {
  int maskIndex = -1;
  int pathIndex = -1;
  QString field;
};

std::optional<MaskPropertyAddress>
parseMaskPropertyPath(const QString &propertyPath) {
  const QStringList parts = propertyPath.split(QLatin1Char('.'), Qt::SkipEmptyParts);
  if (parts.size() < 3 || parts[0] != QStringLiteral("mask")) {
    return std::nullopt;
  }

  bool ok = false;
  const int maskIndex = parts[1].toInt(&ok);
  if (!ok || maskIndex < 0) {
    return std::nullopt;
  }

  if (parts.size() == 3 && parts[2] == QStringLiteral("enabled")) {
    return MaskPropertyAddress{maskIndex, -1, parts[2]};
  }

  if (parts.size() != 5 || parts[2] != QStringLiteral("path")) {
    return std::nullopt;
  }

  const int pathIndex = parts[3].toInt(&ok);
  if (!ok || pathIndex < 0) {
    return std::nullopt;
  }

  const QString field = parts[4];
  if (field == QStringLiteral("closed") ||
      field == QStringLiteral("opacity") ||
      field == QStringLiteral("feather") ||
      field == QStringLiteral("featherHorizontal") ||
      field == QStringLiteral("featherVertical") ||
      field == QStringLiteral("featherInner") ||
      field == QStringLiteral("featherOuter") ||
      field == QStringLiteral("expansion") ||
      field == QStringLiteral("inverted") ||
      field == QStringLiteral("mode") ||
      field == QStringLiteral("name")) {
    return MaskPropertyAddress{maskIndex, pathIndex, field};
  }

  return std::nullopt;
}

QString maskPropertyPrefix(const int maskIndex) {
  return QStringLiteral("mask.%1").arg(maskIndex);
}

QString maskPathPropertyPrefix(const int maskIndex, const int pathIndex) {
  return QStringLiteral("mask.%1.path.%2").arg(maskIndex).arg(pathIndex);
}

QRectF layerCollisionLocalBounds(const ArtifactAbstractLayer* layer) {
  if (!layer) {
    return QRectF();
  }

  const QRectF localBounds = layer->localBounds();
  if (!localBounds.isValid()) {
    return QRectF();
  }

  const auto collisionIntProperty = [layer](const QString& propertyPath,
                                            int fallback) {
    const auto property = layer->getProperty(propertyPath);
    return property ? property->getValue().toInt() : fallback;
  };
  const auto collisionFloatProperty = [layer](const QString& propertyPath,
                                              float fallback) {
    const auto property = layer->getProperty(propertyPath);
    return property ? property->getValue().toFloat() : fallback;
  };

  const int shape = collisionIntProperty(
      QStringLiteral("component.collision.shape"), 0);
  const float width = std::max(
      0.0f, collisionFloatProperty(
                QStringLiteral("component.collision.width"), 0.0f));
  const float height = std::max(
      0.0f, collisionFloatProperty(
                QStringLiteral("component.collision.height"), 0.0f));
  const float radius = std::max(
      0.0f, collisionFloatProperty(
                QStringLiteral("component.collision.radius"), 0.0f));
  const float offsetX = collisionFloatProperty(
      QStringLiteral("component.collision.offsetX"), 0.0f);
  const float offsetY = collisionFloatProperty(
      QStringLiteral("component.collision.offsetY"), 0.0f);
  const QPointF center = localBounds.center() +
                         QPointF(static_cast<qreal>(offsetX),
                                 static_cast<qreal>(offsetY));

  if (shape == 1) {
    const qreal boxWidth = width > 0.0f ? static_cast<qreal>(width)
                                        : localBounds.width();
    const qreal boxHeight = height > 0.0f ? static_cast<qreal>(height)
                                          : localBounds.height();
    return QRectF(center.x() - boxWidth * 0.5, center.y() - boxHeight * 0.5,
                  boxWidth, boxHeight);
  }

  if (shape == 2) {
    const qreal circleRadius =
        radius > 0.0f
            ? static_cast<qreal>(radius)
            : static_cast<qreal>(
                  std::max(localBounds.width(), localBounds.height()) * 0.5);
    return QRectF(center.x() - circleRadius, center.y() - circleRadius,
                  circleRadius * 2.0, circleRadius * 2.0);
  }

  return localBounds.translated(static_cast<qreal>(offsetX),
                                static_cast<qreal>(offsetY));
}

void applyMaskPropertyState(const ArtifactAbstractLayer *layer,
                            const int maskIndex, LayerMask &mask) {
  if (!layer) {
    return;
  }

  const RationalTime time = currentTimelineTime(layer);
  const auto resolveBool = [layer, time](const QString &propertyPath,
                                         const bool fallback) {
    const auto property = layer->getProperty(propertyPath);
    if (!property) {
      return fallback;
    }
    const QVariant value = property->evaluateValue(time);
    return value.isValid() ? value.toBool() : fallback;
  };
  const auto resolveInt = [layer, time](const QString &propertyPath,
                                        const int fallback) {
    const auto property = layer->getProperty(propertyPath);
    if (!property) {
      return fallback;
    }
    const QVariant value = property->evaluateValue(time);
    return value.isValid() ? value.toInt() : fallback;
  };
  const auto resolveDouble = [layer, time](const QString &propertyPath,
                                           const double fallback) {
    const auto property = layer->getProperty(propertyPath);
    if (!property) {
      return fallback;
    }
    const QVariant value = property->evaluateValue(time);
    return value.isValid() ? value.toDouble() : fallback;
  };
  const auto resolveString = [layer, time](const QString &propertyPath,
                                           const QString &fallback) {
    const auto property = layer->getProperty(propertyPath);
    if (!property) {
      return fallback;
    }
    const QVariant value = property->evaluateValue(time);
    return value.isValid() ? value.toString() : fallback;
  };

  const QString maskPrefix = maskPropertyPrefix(maskIndex);
  mask.setEnabled(resolveBool(maskPrefix + QStringLiteral(".enabled"),
                              mask.isEnabled()));

  for (int pathIndex = 0; pathIndex < mask.maskPathCount(); ++pathIndex) {
    MaskPath path = mask.maskPath(pathIndex);
    const QString pathPrefix = maskPathPropertyPrefix(maskIndex, pathIndex);
    path.setClosed(resolveBool(pathPrefix + QStringLiteral(".closed"),
                               path.isClosed()));
    path.setOpacity(static_cast<float>(
        resolveDouble(pathPrefix + QStringLiteral(".opacity"),
                      static_cast<double>(path.opacity()))));
    path.setFeather(static_cast<float>(
        resolveDouble(pathPrefix + QStringLiteral(".feather"),
                      static_cast<double>(path.feather()))));
    path.setFeatherHorizontal(static_cast<float>(
        resolveDouble(pathPrefix + QStringLiteral(".featherHorizontal"),
                      static_cast<double>(path.featherHorizontal()))));
    path.setFeatherVertical(static_cast<float>(
        resolveDouble(pathPrefix + QStringLiteral(".featherVertical"),
                      static_cast<double>(path.featherVertical()))));
    path.setFeatherInner(static_cast<float>(
        resolveDouble(pathPrefix + QStringLiteral(".featherInner"),
                      static_cast<double>(path.featherInner()))));
    path.setFeatherOuter(static_cast<float>(
        resolveDouble(pathPrefix + QStringLiteral(".featherOuter"),
                      static_cast<double>(path.featherOuter()))));
    path.setExpansion(static_cast<float>(
        resolveDouble(pathPrefix + QStringLiteral(".expansion"),
                      static_cast<double>(path.expansion()))));
    path.setInverted(resolveBool(pathPrefix + QStringLiteral(".inverted"),
                                 path.isInverted()));
    path.setMode(static_cast<MaskMode>(
        resolveInt(pathPrefix + QStringLiteral(".mode"),
                   static_cast<int>(path.mode()))));
    path.setName(UniString(resolveString(pathPrefix + QStringLiteral(".name"),
                                         path.name().toQString())
                               .toStdString()));
    mask.setMaskPath(pathIndex, path);
  }
}

struct ClonerTransformPropertyAddress {
  int index = -1;
  QString field;
};

std::optional<ClonerTransformPropertyAddress>
parseClonerTransformPropertyPath(const QString &propertyPath) {
  const QString prefix = QStringLiteral("component.cloner.transforms.");
  if (!propertyPath.startsWith(prefix, Qt::CaseInsensitive)) {
    return std::nullopt;
  }
  const QString tail = propertyPath.mid(prefix.size());
  const QStringList parts = tail.split(QLatin1Char('.'), Qt::SkipEmptyParts);
  if (parts.size() != 2) {
    return std::nullopt;
  }
  bool ok = false;
  const int index = parts[0].toInt(&ok);
  if (!ok || index < 0) {
    return std::nullopt;
  }
  return ClonerTransformPropertyAddress{index, parts[1]};
}

struct FractureShardRenderPrimitive {
  std::vector<Detail::float2> polygon;
  FloatColor color;
};

struct FractureRenderElement {
  ArtifactCore::ParticleRenderData debris;
  std::vector<FractureShardRenderPrimitive> shards;

  bool empty() const {
    return debris.particles.empty() && shards.empty();
  }
};

struct LayerComponentRuntimeSnapshotData {
  FractureState fractureState;
  std::vector<ArtifactCore::ParticleVertex> componentParticles;
  int64_t fractureMotionLastFrame = std::numeric_limits<int64_t>::min();
  int64_t componentParticlesLastFrame = std::numeric_limits<int64_t>::min();
  int64_t lastCollisionImpactFrame = std::numeric_limits<int64_t>::min();
};

QJsonObject componentSnapshotVectorToJson(const QVector3D& value) {
  return {{QStringLiteral("x"), value.x()},
          {QStringLiteral("y"), value.y()},
          {QStringLiteral("z"), value.z()}};
}

QVector3D componentSnapshotVectorFromJson(const QJsonValue& value) {
  const QJsonObject object = value.toObject();
  return {static_cast<float>(object.value(QStringLiteral("x")).toDouble()),
          static_cast<float>(object.value(QStringLiteral("y")).toDouble()),
          static_cast<float>(object.value(QStringLiteral("z")).toDouble())};
}

QString componentSnapshotFrameToJson(int64_t frame) {
  return QString::number(frame);
}

int64_t componentSnapshotFrameFromJson(const QJsonObject& object,
                                       const QString& key) {
  bool ok = false;
  const qlonglong value = object.value(key).toString().toLongLong(&ok);
  return ok ? static_cast<int64_t>(value)
            : std::numeric_limits<int64_t>::min();
}

struct MotionTrailRingBuffer {
  std::vector<QVector3D> samples;
  std::size_t head = 0;
  std::size_t count = 0;

  void clear() {
    head = 0;
    count = 0;
  }

  void push(const QVector3D& sample, const std::size_t capacity) {
    if (capacity == 0) {
      clear();
      return;
    }
    if (samples.size() != capacity) {
      samples.assign(capacity, sample);
      head = 0;
      count = 1;
      return;
    }
    const std::size_t writeIndex = (head + count) % capacity;
    samples[writeIndex] = sample;
    if (count < capacity) {
      ++count;
    } else {
      head = (head + 1) % capacity;
    }
  }
};

void submitFractureRenderElement(ArtifactIRenderer *renderer,
                                 const FractureRenderElement &element) {
  if (!renderer || element.empty()) {
    return;
  }
  if (!element.debris.particles.empty()) {
    renderer->drawParticles(element.debris);
  }
  for (const auto &shard : element.shards) {
    renderer->drawSolidPolygonLocal(shard.polygon, shard.color);
  }
}
} // namespace


class ArtifactAbstractLayer::Impl {
public:
   bool is3D_ = false;
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
    bool collisionOwnsPhysicsEnable_ = false;
    int collisionShape_ = 0;
    float collisionWidth_ = 0.0f;
    float collisionHeight_ = 0.0f;
    float collisionRadius_ = 0.0f;
    float collisionOffsetX_ = 0.0f;
    float collisionOffsetY_ = 0.0f;
    int rigidBodyColliderShape_ = -1;
    float rigidBodyColliderRestitution_ = -1.0f;
    float collisionFloorY_ = 0.0f;
    bool collisionCompositionBounds_ = false;
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
    int fluidGridWidth_ = 128;
    int fluidGridHeight_ = 128;
    float fluidViscosity_ = 0.00001f;
    float fluidDiffusion_ = 0.00001f;
    float fluidBuoyancy_ = 0.05f;
    float fluidVorticity_ = 0.1f;
    int fluidSolverIterations_ = 20;
    std::unique_ptr<ArtifactCore::FluidSolver2D> fluidSolver_;
    std::vector<ArtifactCore::ParticleVertex> fluidPreviewParticles_;
    mutable int64_t fluidLastFrame_ = std::numeric_limits<int64_t>::min();
    std::vector<ArtifactCore::ParticleVertex> componentParticles_;
    mutable int64_t componentParticlesLastFrame_ =
        std::numeric_limits<int64_t>::min();
    mutable int64_t lastCollisionImpactFrame_ =
        std::numeric_limits<int64_t>::min();
    LayerComponentHost componentHost_;
    std::optional<LayerEvaluationState> authoritativeComponentState_;
    int64_t authoritativeComponentFrame_ =
        std::numeric_limits<int64_t>::min();
    LayerEvaluationState componentEvaluationState_;
    int layoutMode_ = 0;
    int layoutAnchorMode_ = 0;
    int layoutHorizontalPin_ = 0;
    int layoutVerticalPin_ = 0;
    int layoutScaleMode_ = 0;
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
    std::vector<ClonerTransformOperation> clonerTransforms_;
    NamedVector<LayerGeneratorDescriptor> extraGeneratorDescriptors_{
        ContainerName{"Layer.ExtraGenerators"}};
    NamedVector<LayerFieldDescriptor> extraFieldDescriptors_{
        ContainerName{"Layer.ExtraFields"}};
    NamedVector<LayerModifierDescriptor> extraCloneModifierDescriptors_{
        ContainerName{"Layer.ExtraCloneModifiers"}};
    QJsonObject scriptBinding_;

  // Matte components (Asset-based track mattes)
  std::vector<LayerMatteReference> mattes_;

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
  std::vector<std::shared_ptr<ArtifactAbstractEffect>> effects_;

  // レイヤーモディファイアコンテナ
  LayerModifierStack modifiers_;

  // マスクコンテナ
  std::vector<LayerMask> masks_;
  mutable QHash<QString, std::shared_ptr<AbstractProperty>> propertyCache_;
  mutable std::mutex propertyCacheMutex_;

  // Time remap
  std::unique_ptr<ArtifactCore::TimeRemapEffect> timeRemapEffect_;

  // Variants
  std::vector<std::unique_ptr<LayerVariant>> variants_;
  size_t activeVariantIndex_ = 0;

public:
  Impl();
  ~Impl();
  void syncBuiltinComponentDescriptors();
  std::type_index type_index_ = typeid(void);
  void goToStartFrame();
  void goToEndFrame();
  void goToNextFrame();
  void goToPrevFrame();

  bool is3D() const;
  AnimatableTransform3D transform_;
  AnimatableTransform2D transform2d_;
  Size_2D sourceSize_;

  // エフェクト管理メソッド
  void addEffect(std::shared_ptr<ArtifactAbstractEffect> effect);
  void removeEffect(const UniString &effectID);
  void clearEffects();
  std::vector<std::shared_ptr<ArtifactAbstractEffect>> getEffects() const;
  std::shared_ptr<ArtifactAbstractEffect>
  getEffect(const UniString &effectID) const;
  int effectCount() const;

  // モディファイア管理メソッド
  void addModifier(std::shared_ptr<ArtifactLayerModifier> modifier);
  void removeModifier(const QString& modifierId);
  void clearModifiers();
  std::vector<std::shared_ptr<ArtifactLayerModifier>> getModifiers() const;
  std::shared_ptr<ArtifactLayerModifier> getModifier(const QString& modifierId) const;
  int modifierCount() const;
  bool hasModifiers() const;

  // マスク管理
  void addMask(const LayerMask &mask);
  void removeMask(int index);
  void setMask(int index, const LayerMask &mask);
  LayerMask getMask(int index) const;
  int maskCount() const;
  void clearMasks();
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

  auto collision =
      makeCollisionComponentDescriptor(collisionComponentEnabled_);
  collision.settings[QStringLiteral("shape")] = collisionShape_;
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
  componentHost_.upsert(std::move(collision));

  auto fracture = makeFractureComponentDescriptor(fractureEnabled_);
  fracture.settings[QStringLiteral("preset")] = fracturePreset_;
  fracture.settings[QStringLiteral("shardCount")] = fractureShardCount_;
  fracture.settings[QStringLiteral("crackThreshold")] =
      static_cast<double>(fractureCrackThreshold_);
  fracture.settings[QStringLiteral("shatterThreshold")] =
      static_cast<double>(fractureShatterThreshold_);
  fracture.settings[QStringLiteral("preGenerate")] = fracturePreGenerate_;
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
  componentHost_.upsert(std::move(fluid));
}

void ArtifactAbstractLayer::Impl::goToStartFrame() {}

void ArtifactAbstractLayer::Impl::goToEndFrame() {}

void ArtifactAbstractLayer::Impl::goToNextFrame() {}

void ArtifactAbstractLayer::Impl::goToPrevFrame() {}

bool ArtifactAbstractLayer::Impl::is3D() const { return is3D_; }

ArtifactAbstractLayer::ArtifactAbstractLayer() : impl_(new Impl()) {
  impl_->id = Id(); // Generate new ID
  impl_->variants_.push_back(std::make_unique<LayerVariant>(this, "A"));
  impl_->activeVariantIndex_ = 0;
}

ArtifactAbstractLayer::~ArtifactAbstractLayer() { delete impl_; }

void ArtifactAbstractLayer::setVisible(bool visible /*=true*/) {
  if (!assignIfChanged(impl_->isVisible_, visible)) {
    return;
  }
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::VisibilityChanged);
}

void ArtifactAbstractLayer::Show() { setVisible(true); }

void ArtifactAbstractLayer::Hide() { setVisible(false); }

void ArtifactAbstractLayer::drawLOD(ArtifactIRenderer *renderer, DetailLevel)
{
  draw(renderer);
}

LAYER_BLEND_TYPE ArtifactAbstractLayer::layerBlendType() const {
  const auto* var = getActiveVariant();
  if (var && HasFlag(var->overrideFlags_, VariantOverrideFlags::BlendMode) && var->blendModeOverride.has_value()) {
      return var->blendModeOverride.value();
  }
  return impl_->blendMode_;
}

void ArtifactAbstractLayer::setBlendMode(LAYER_BLEND_TYPE type) {
  if (impl_->activeVariantIndex_ != 0) {
      auto* var = getActiveVariant();
      if (var) {
          var->blendModeOverride = type;
          SetFlag(var->overrideFlags_, VariantOverrideFlags::BlendMode);
          setDirty(LayerDirtyFlag::Effect);
          addDirtyReason(LayerDirtyReason::PropertyChanged);
          Q_EMIT changed();
          return;
      }
  }

  if (impl_->blendMode_ == type) {
    return;
  }
  impl_->blendMode_ = type;
  setDirty(LayerDirtyFlag::Effect);
  addDirtyReason(LayerDirtyReason::PropertyChanged);
  Q_EMIT changed();
}

LayerID ArtifactAbstractLayer::id() const { return impl_->id; }

QString ArtifactAbstractLayer::layerName() const { return impl_->name_; }

UniString ArtifactAbstractLayer::className() const { return QString(""); }

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
  Q_EMIT layerNoteChanged(note);
  notifyLayerMutation(this, LayerDirtyFlag::All,
                      LayerDirtyReason::PropertyChanged);
}

std::type_index ArtifactAbstractLayer::type_index() const {
  return impl_->type_index_;
}

void ArtifactAbstractLayer::goToStartFrame() {}

void ArtifactAbstractLayer::goToEndFrame() {}

void ArtifactAbstractLayer::goToNextFrame() {}

void ArtifactAbstractLayer::goToPrevFrame() {}

void ArtifactAbstractLayer::goToFrame(int64_t frameNumber /*= 0*/) {
  // グローバルフレーム → レイヤー相対フレーム:
  // relativeFrame = globalFrame - inPoint + startTime
  impl_->currentFrame_ = frameNumber - impl_->inPoint_.framePosition() +
                         impl_->startTime_.framePosition();
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
  if (!assignIfChanged(impl_->layerCachePolicy_, policy)) {
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
  if (impl_->labelColorIndex_ != index) {
    impl_->labelColorIndex_ = index;
    Q_EMIT changed();
  }
}

void ArtifactAbstractLayer::setDirty(LayerDirtyFlag flag) {
  impl_->dirtyFlags_ |= (uint32_t)flag;
  ++impl_->geometryRevision_;
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

LayerVariant* ArtifactAbstractLayer::createVariantFromCurrent(const std::string& newName) {
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
    return impl_->variants_.back().get();
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

std::vector<LayerVariant*> ArtifactAbstractLayer::getVariants() const {
    std::vector<LayerVariant*> result;
    result.reserve(impl_->variants_.size());
    for(auto& v : impl_->variants_) {
        result.push_back(v.get());
    }
    return result;
}

std::unique_ptr<LayerVariant> ArtifactAbstractLayer::extractVariant(size_t index) {
    if (index < impl_->variants_.size()) {
       auto var = std::move(impl_->variants_[index]);
       impl_->variants_.erase(impl_->variants_.begin() + index);
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
    impl_->variants_.insert(impl_->variants_.begin() + index, std::move(variant));
    if (impl_->activeVariantIndex_ >= index) {
        impl_->activeVariantIndex_++;
    }
    notifyLayerMutation(this, LayerDirtyFlag::All, LayerDirtyReason::PropertyChanged);
}

void ArtifactAbstractLayer::setComposition(QObject *comp) {
  std::lock_guard<std::mutex> lock(impl_->compositionMutex_);
  impl_->composition_ = comp;
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

LayerFieldChannelSample
ArtifactAbstractLayer::compositionFieldChannelsAtCanvasPoint(
    const QPointF& canvasPosition) const {
  const auto* composition =
      dynamic_cast<const ArtifactAbstractComposition*>(compositionObject());
  if (!composition) {
    return {};
  }
  const auto sample =
      composition->evaluateFieldChannelsAtCanvasPoint(id(), canvasPosition);
  return LayerFieldChannelSample{
      std::clamp(static_cast<float>(sample.weight), 0.0f, 1.0f),
      static_cast<float>(sample.scaleMultiplier),
      static_cast<float>(sample.timeOffsetSeconds),
      sample.affected};
}

QSizeF ArtifactAbstractLayer::compositionSizeHint() const {
  auto* composition =
      dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
  if (!composition) {
    return {};
  }
  const auto size = composition->settings().compositionSize();
  return QSizeF(size.width(), size.height());
}

double ArtifactAbstractLayer::compositionFrameRate() const {
  return effectiveLayerFrameRate(this);
}

ArtifactAbstractLayerPtr ArtifactAbstractLayer::parentLayer() const {
  auto *composition = dynamic_cast<ArtifactAbstractComposition *>(compositionObject());
  if (!composition || impl_->parentLayerId_.isNil())
  return nullptr;
  return composition->layerById(impl_->parentLayerId_);
}

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
    const auto it = impl_->propertyCache_.constFind(propertyPath);
    if (it == impl_->propertyCache_.constEnd() || !it.value()) {
      return fallback;
    }
    const auto &property = *it.value();
    if (!property.isAnimatable() || property.getKeyFrames().empty()) {
      return fallback;
    }
    const QVariant animatedValue = property.interpolateValue(time);
    return animatedValue.isValid() ? animatedValue.toDouble() : fallback;
  };

  const bool useSpatialPosition = !hasTransVar && t.hasPositionSpatialTangents();
  double positionX = useSpatialPosition
      ? t.positionXAt(time)
      : evaluateDouble(QStringLiteral("transform.position.x"), t.positionX());
  double positionY = useSpatialPosition
      ? t.positionYAt(time)
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

  if (impl_->physicsComponent_.enabled()) {
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
    const int64_t curFrame = currentTimelineFrame(this);
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

  applyCompositionTransformFields(this, positionX, positionY, scaleX, scaleY);
  QTransform transform = makeLayerTransform2D(positionX, positionY, rotation, scaleX, scaleY,
                                              anchorX, anchorY);
  transform = impl_->modifiers_.apply(transform, localBounds(), time.toDouble());
  return transform;
}

QPointF parentAutoLayoutOffset(const ArtifactAbstractLayer *layer,
                               const ArtifactAbstractLayerPtr &parent) {
  if (!layer || !parent) {
    return QPointF();
  }
  const auto enabled = parent->getProperty(
      QStringLiteral("component.layout.enabled"));
  if (!enabled || !enabled->getValue().toBool()) {
    return QPointF();
  }
  const auto participation = layer->getProperty(
      QStringLiteral("component.layout.mode"));
  if (participation && participation->getValue().toInt() == 2) {
    return QPointF();
  }
  auto *composition = static_cast<ArtifactAbstractComposition *>(
      parent->composition());
  if (!composition) {
    return QPointF();
  }
  const auto direction = parent->getProperty(
      QStringLiteral("component.layout.stackDirection"));
  const auto alignmentProperty = parent->getProperty(
      QStringLiteral("component.layout.anchorMode"));
  const auto gapProperty = parent->getProperty(
      QStringLiteral("component.layout.gap"));
  const auto paddingXProperty = parent->getProperty(
      QStringLiteral("component.layout.safeAreaPaddingX"));
  const auto paddingYProperty = parent->getProperty(
      QStringLiteral("component.layout.safeAreaPaddingY"));
  const auto safeAreaEnabledProperty = parent->getProperty(
      QStringLiteral("component.layout.safeAreaEnabled"));
  const bool vertical = direction && direction->getValue().toInt() != 0;
  const int alignment = alignmentProperty
                            ? std::clamp(alignmentProperty->getValue().toInt(), 0, 2)
                            : 0;
  const qreal gap = gapProperty ? gapProperty->getValue().toDouble() : 0.0;
  const bool usePadding = safeAreaEnabledProperty &&
                          safeAreaEnabledProperty->getValue().toBool();
  const QRectF parentBounds = parent->localBounds();
  const qreal paddingX = usePadding && paddingXProperty
                             ? paddingXProperty->getValue().toDouble()
                             : 0.0;
  const qreal paddingY = usePadding && paddingYProperty
                             ? paddingYProperty->getValue().toDouble()
                             : 0.0;
  qreal cursorX = parentBounds.left() + paddingX;
  qreal cursorY = parentBounds.top() + paddingY;
  const auto siblings = composition->childLayersOf(parent->id());
  for (const auto &sibling : siblings) {
    if (!sibling) {
      continue;
    }
    const auto siblingParticipation = sibling->getProperty(
        QStringLiteral("component.layout.mode"));
    if (siblingParticipation &&
        siblingParticipation->getValue().toInt() == 2) {
      continue;
    }
    const QRectF bounds = sibling->visualLocalBounds();
    if (sibling.get() == layer) {
      qreal targetX = cursorX - bounds.left();
      qreal targetY = cursorY - bounds.top();
      if (vertical) {
        const qreal availableWidth = std::max<qreal>(
            0.0, parentBounds.width() - paddingX * 2.0);
        if (alignment == 1) {
          targetX = parentBounds.left() + paddingX +
                    (availableWidth - bounds.width()) * 0.5 - bounds.left();
        } else if (alignment == 2) {
          targetX = parentBounds.right() - paddingX - bounds.width() -
                    bounds.left();
        }
      } else {
        const qreal availableHeight = std::max<qreal>(
            0.0, parentBounds.height() - paddingY * 2.0);
        if (alignment == 1) {
          targetY = parentBounds.top() + paddingY +
                    (availableHeight - bounds.height()) * 0.5 - bounds.top();
        } else if (alignment == 2) {
          targetY = parentBounds.bottom() - paddingY - bounds.height() -
                    bounds.top();
        }
      }
      return QPointF(targetX, targetY);
    }
    if (vertical) {
      cursorY += std::max<qreal>(0.0, bounds.height()) + gap;
    } else {
      cursorX += std::max<qreal>(0.0, bounds.width()) + gap;
    }
  }
  return QPointF();
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
  const bool layoutManaged = layoutEnabledProperty &&
                             layoutEnabledProperty->getValue().toBool() &&
                             (!participationProperty ||
                              participationProperty->getValue().toInt() != 2);
  const QPointF layoutOffset = layoutManaged
                                   ? parentAutoLayoutOffset(this, parent)
                                   : QPointF();
  if (!layoutManaged &&
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
  const RationalTime time(frameNumber, effectiveLayerFrameRate(this));
  const auto* var = getActiveVariant();
  bool hasTransVar = var && HasFlag(var->overrideFlags_, VariantOverrideFlags::Transform) && var->transform3DOverride.has_value();

  auto evaluateDouble = [this, &time, hasTransVar](const QString &propertyPath, double fallback) {
    if (hasTransVar) return fallback;
    const auto it = impl_->propertyCache_.constFind(propertyPath);
    if (it == impl_->propertyCache_.constEnd() || !it.value()) {
      return fallback;
    }
    const auto &property = *it.value();
    if (!property.isAnimatable() || property.getKeyFrames().empty()) {
      return fallback;
    }
    const QVariant animatedValue = property.interpolateValue(time);
    return animatedValue.isValid() ? animatedValue.toDouble() : fallback;
  };

  const bool useSpatialPosition = !hasTransVar && t.hasPositionSpatialTangents();
  double positionX = useSpatialPosition
      ? t.positionXAt(time)
      : evaluateDouble(QStringLiteral("transform.position.x"), t.positionXAt(time));
  double positionY = useSpatialPosition
      ? t.positionYAt(time)
      : evaluateDouble(QStringLiteral("transform.position.y"), t.positionYAt(time));
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
  QTransform transform = makeLayerTransform2D(positionX, positionY, rotation, scaleX, scaleY,
                                              anchorX, anchorY);
  transform = impl_->modifiers_.apply(transform, localBounds(), time.toDouble());
  return transform;
}

QTransform ArtifactAbstractLayer::getGlobalTransformAt(int64_t frameNumber) const {
  QTransform local = getLocalTransformAt(frameNumber);
  auto parent = parentLayer();
  if (parent) {
    const QPointF layoutOffset = parentAutoLayoutOffset(this, parent);
    if (!layoutOffset.isNull()) {
      local = QTransform::fromTranslate(layoutOffset.x(), layoutOffset.y()) * local;
    }
    return combineLayerTransform2D(local, parent->getGlobalTransformAt(frameNumber)); // Time remapping on parent not considered here yet
  }
  return local;
}

QMatrix4x4 ArtifactAbstractLayer::getLocalTransform4x4() const {
  const auto &t = transform3D();
  const RationalTime time = currentTimelineTime(this);
  const int64_t frame = impl_->currentFrame_;
  const double fps = effectiveLayerFrameRate(this);
  auto evaluateDouble = [this, &time](const QString &propertyPath,
                                      double fallback) {
    const auto it = impl_->propertyCache_.constFind(propertyPath);
    if (it == impl_->propertyCache_.constEnd() || !it.value()) {
      return fallback;
    }
    const auto &property = *it.value();
    if (!property.isAnimatable() || property.getKeyFrames().empty()) {
      return fallback;
    }
    const QVariant animatedValue = property.interpolateValue(time);
    return animatedValue.isValid() ? animatedValue.toDouble() : fallback;
  };
  const bool useSpatialPosition = t.hasPositionSpatialTangents();
  double positionX = useSpatialPosition
      ? t.positionXAt(time)
      : evaluateDouble(QStringLiteral("transform.position.x"), t.positionX());
  double positionY = useSpatialPosition
      ? t.positionYAt(time)
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

  if (impl_->physicsComponent_.enabled()) {
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
    if (auto world = ArtifactCore::PhysicsSystem::instance().getRigidWorld(id())) {
      const auto bodies = world->getBodies();
      if (!bodies.empty() && bodies.front()) {
        const auto body = bodies.front();
        const QVector2D bodyPos = body->position();
        positionX = bodyPos.x();
        positionY = bodyPos.y();
        rotation = body->angle();
      }
    }
  }

  applyCompositionTransformFields(this, positionX, positionY, scaleX, scaleY);
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

bool ArtifactAbstractLayer::hasSoftBodyPhysics() const {
  return static_cast<bool>(ArtifactCore::PhysicsSystem::instance().getSoftBody(id()));
}

bool ArtifactAbstractLayer::hasRigidBodyPhysics() const {
  return static_cast<bool>(ArtifactCore::PhysicsSystem::instance().getRigidWorld(id()));
}

const FractureState& ArtifactAbstractLayer::fractureState() const {
  return impl_->fractureState_;
}

const std::vector<FractureShardMotion>& ArtifactAbstractLayer::fractureShardMotions() const {
  return impl_->fractureState_.shards;
}

void applyFragmentFieldsAndFloorCollision(
    const ArtifactAbstractLayer* layer, FractureShardMotion& shard,
    const QMatrix4x4& baseTransform, const float deltaSeconds) {
  if (!layer || !shard.active || deltaSeconds <= 0.0f) {
    return;
  }

  for (const auto& field : layer->layerFields()) {
    if (!field.enabled || field.strength == 0.0f) {
      continue;
    }
    const float sign = field.invert ? -1.0f : 1.0f;
    const float strength = field.strength * sign;
    const float centerX = static_cast<float>(
        field.settings.value(QStringLiteral("centerX")).toDouble(0.0));
    const float centerY = static_cast<float>(
        field.settings.value(QStringLiteral("centerY")).toDouble(0.0));
    QVector3D fromCenter(shard.position.x() - centerX,
                         shard.position.y() - centerY, 0.0f);
    QVector3D acceleration(0.0f, 0.0f, 0.0f);

    if (field.typeId == QStringLiteral("artifact.field.radial") ||
        field.typeId == QStringLiteral("artifact.field.sphere")) {
      const float radius = std::max(
          1.0f, static_cast<float>(field.settings
              .value(QStringLiteral("outerRadius"))
              .toDouble(field.settings.value(QStringLiteral("radius"))
                            .toDouble(160.0))));
      const float distance = fromCenter.length();
      if (distance <= radius && distance > 0.0001f) {
        fromCenter.normalize();
        acceleration = fromCenter * strength *
                       (1.0f - distance / radius) * 600.0f;
      }
    } else if (field.typeId == QStringLiteral("artifact.field.linear")) {
      const float angle = static_cast<float>(
          field.settings.value(QStringLiteral("angle")).toDouble(0.0)) *
          3.1415926535f / 180.0f;
      acceleration = QVector3D(std::cos(angle), std::sin(angle), 0.0f) *
                     strength * 600.0f;
    } else if (field.typeId == QStringLiteral("artifact.field.box")) {
      const float halfX = std::max(1.0f, static_cast<float>(
          field.settings.value(QStringLiteral("halfX")).toDouble(120.0)));
      const float halfY = std::max(1.0f, static_cast<float>(
          field.settings.value(QStringLiteral("halfY")).toDouble(120.0)));
      if (std::abs(fromCenter.x()) <= halfX &&
          std::abs(fromCenter.y()) <= halfY) {
        acceleration.setY(strength * 600.0f);
      }
    } else if (field.typeId == QStringLiteral("artifact.field.noise")) {
      const float phase = shard.position.x() * 0.013f +
                          shard.position.y() * 0.017f;
      acceleration = QVector3D(std::sin(phase), std::cos(phase), 0.0f) *
                     strength * 300.0f;
    }
    shard.velocity += acceleration * deltaSeconds;
  }

  const auto collisionEnabled =
      layer->getProperty(QStringLiteral("component.collision.enabled"));
  if (!collisionEnabled || !collisionEnabled->getValue().toBool()) {
    return;
  }
  const QSizeF compositionSize = layer->compositionSizeHint();
  if (!compositionSize.isValid()) {
    return;
  }
  const auto floorProperty =
      layer->getProperty(QStringLiteral("component.collision.floorY"));
  const float configuredFloor =
      floorProperty ? floorProperty->getValue().toFloat() : 0.0f;
  const float floorY = configuredFloor > 0.0f
                           ? configuredFloor
                           : static_cast<float>(compositionSize.height());
  const float shardRadius = std::max(2.0f, 5.0f * shard.scale);
  const auto restitutionProperty =
      layer->getProperty(QStringLiteral("physics.restitution"));
  const float restitution = std::clamp(
      restitutionProperty ? restitutionProperty->getValue().toFloat() : 0.25f,
      0.0f, 1.0f);
  QVector3D worldPosition = baseTransform.map(shard.position);
  if (worldPosition.y() + shardRadius > floorY) {
    shard.position.setY(shard.position.y() + floorY - shardRadius -
                        worldPosition.y());
    if (shard.velocity.y() > 0.0f) {
      shard.velocity.setY(-shard.velocity.y() * restitution);
      shard.velocity.setX(shard.velocity.x() * 0.92f);
    }
  }
  const auto boundsProperty =
      layer->getProperty(QStringLiteral("component.collision.compositionBounds"));
  if (!boundsProperty || !boundsProperty->getValue().toBool()) {
    return;
  }
  worldPosition = baseTransform.map(shard.position);
  const float compositionWidth = static_cast<float>(compositionSize.width());
  const float compositionHeight = static_cast<float>(compositionSize.height());
  if (worldPosition.x() - shardRadius < 0.0f ||
      worldPosition.x() + shardRadius > compositionWidth) {
    const float targetX = std::clamp(worldPosition.x(), shardRadius,
                                     compositionWidth - shardRadius);
    shard.position.setX(shard.position.x() + targetX - worldPosition.x());
    shard.velocity.setX(-shard.velocity.x() * restitution);
  }
  worldPosition = baseTransform.map(shard.position);
  if (worldPosition.y() - shardRadius < 0.0f ||
      worldPosition.y() + shardRadius > compositionHeight) {
    const float targetY = std::clamp(worldPosition.y(), shardRadius,
                                     compositionHeight - shardRadius);
    shard.position.setY(shard.position.y() + targetY - worldPosition.y());
    shard.velocity.setY(-shard.velocity.y() * restitution);
  }
}

void syncFragmentDataset(const ArtifactAbstractLayer* layer,
                         const FractureState& fractureState,
                         const FractureResult* prefractureResult,
                         LayerEvaluationState& evaluationState) {
  auto& fragments = evaluationState.fragments;
  auto& fragmentGeometry = evaluationState.fragmentGeometry;
  fragments.clear();
  fragmentGeometry.clear();
  if (!layer) {
    return;
  }
  fragments.reserve(fractureState.shards.size());
  fragmentGeometry.reserve(fractureState.shards.size());
  const QString ownerLayerId = layer->id().toString();
  for (std::size_t index = 0; index < fractureState.shards.size(); ++index) {
    const auto& shard = fractureState.shards[index];
    QMatrix4x4 fragmentTransform;
    fragmentTransform.setToIdentity();
    fragmentTransform.translate(shard.position);
    fragmentTransform.rotate(shard.rotation, 0.0f, 0.0f, 1.0f);
    fragmentTransform.scale(shard.scale);
    LayerFragmentState fragment;
    fragment.entityId = SimulationEntityId{
        ownerLayerId, QStringLiteral("component.fracture"),
        static_cast<std::uint64_t>(index), 0};
    fragment.sourceEntityId = SimulationEntityId{
        ownerLayerId, QStringLiteral("layer.source"), 0, 0};
    fragment.geometryHandle =
        QStringLiteral("fracture.shard.%1").arg(index);
    fragment.transform = fragmentTransform;
    fragment.linearVelocity = shard.velocity;
    fragment.angularVelocity = shard.angularVelocity;
    const FractureShard* sourceShard = nullptr;
    if (prefractureResult && prefractureResult->valid &&
        index < prefractureResult->shards.size()) {
      sourceShard = &prefractureResult->shards[index];
      fragment.mass = sourceShard->mass;
    }
    fragment.opacity = shard.opacity;
    fragment.age = shard.age;
    fragment.lifetime = shard.lifetime;
    fragment.active = shard.active;
    fragment.debris = shard.debris;
    LayerFragmentGeometry geometry;
    geometry.geometryHandle = fragment.geometryHandle;
    geometry.materialHandle = QStringLiteral("layer.source:%1").arg(ownerLayerId);
    if (sourceShard && sourceShard->polygon.size() >= 3) {
      const QVector3D centroid = sourceShard->sourceCentroid;
      const QRectF bounds = layer->localBounds();
      const float width = std::max(1.0f, static_cast<float>(bounds.width()));
      const float height = std::max(1.0f, static_cast<float>(bounds.height()));
      geometry.localPolygon.reserve(
          static_cast<std::size_t>(sourceShard->polygon.size()));
      geometry.localUV.reserve(
          static_cast<std::size_t>(sourceShard->polygon.size()));
      for (const QPointF& point : sourceShard->polygon) {
        geometry.localPolygon.emplace_back(
            static_cast<float>(point.x()) - centroid.x(),
            static_cast<float>(point.y()) - centroid.y());
        geometry.localUV.emplace_back(
            (static_cast<float>(point.x()) - static_cast<float>(bounds.left())) / width,
            (static_cast<float>(point.y()) - static_cast<float>(bounds.top())) / height);
      }
    } else {
      const float seed = static_cast<float>(index) * 1.61803398875f;
      const float skew = std::sin(seed) * 0.22f;
      const float pinch = 0.16f + std::cos(seed * 0.73f) * 0.07f;
      geometry.localPolygon = {
          QVector2D(-4.5f - skew * 10.0f, -10.0f - pinch * 2.5f),
          QVector2D(3.5f - skew * 8.0f, -9.2f - pinch * 1.5f),
          QVector2D(0.5f + skew * 1.5f, 0.8f + pinch * 0.8f),
          QVector2D(5.8f + skew * 11.0f, 9.5f - pinch * 0.5f),
          QVector2D(-6.2f + skew * 9.0f, 8.2f + pinch * 2.0f)};
      geometry.localUV.reserve(geometry.localPolygon.size());
      const QRectF bounds = layer->localBounds();
      const float width = std::max(1.0f, static_cast<float>(bounds.width()));
      const float height = std::max(1.0f, static_cast<float>(bounds.height()));
      for (const QVector2D& point : geometry.localPolygon) {
        geometry.localUV.emplace_back(
            std::clamp((shard.position.x() + point.x() -
                        static_cast<float>(bounds.left())) / width, 0.0f, 1.0f),
            std::clamp((shard.position.y() + point.y() -
                        static_cast<float>(bounds.top())) / height, 0.0f, 1.0f));
      }
    }
    fragments.push_back(std::move(fragment));
    fragmentGeometry.push_back(std::move(geometry));
  }
}

const LayerEvaluationState& ArtifactAbstractLayer::layerEvaluationState() const {
  return impl_->componentEvaluationState_;
}

void ArtifactAbstractLayer::drawFractureOverlay(ArtifactIRenderer* renderer,
                                                const QMatrix4x4& baseTransform,
                                                const QSizeF& sourceSize,
                                                float opacityScale) {
  if (!renderer) {
    return;
  }
  Q_UNUSED(sourceSize);

  const int64_t frame = currentTimelineFrame(this);
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
    const bool solverMismatch =
        !impl_->fluidSolver_ ||
        impl_->fluidSolver_->width() != impl_->fluidGridWidth_ ||
        impl_->fluidSolver_->height() != impl_->fluidGridHeight_;
    if (solverMismatch) {
      impl_->fluidSolver_ = std::make_unique<ArtifactCore::FluidSolver2D>(
          impl_->fluidGridWidth_, impl_->fluidGridHeight_);
      impl_->fluidLastFrame_ = std::numeric_limits<int64_t>::min();
      impl_->fluidPreviewParticles_.clear();
    }
    if (impl_->fluidSolver_) {
      impl_->fluidSolver_->setViscosity(impl_->fluidViscosity_);
      impl_->fluidSolver_->setDiffusion(impl_->fluidDiffusion_);
      impl_->fluidSolver_->setBuoyancy(impl_->fluidBuoyancy_);
      impl_->fluidSolver_->setVorticity(impl_->fluidVorticity_);
      impl_->fluidSolver_->setSolverIterations(impl_->fluidSolverIterations_);

      if (impl_->fluidLastFrame_ == std::numeric_limits<int64_t>::min() ||
          frame < impl_->fluidLastFrame_ ||
          frame - impl_->fluidLastFrame_ > 10) {
        impl_->fluidSolver_->reset();
        impl_->fluidLastFrame_ = frame;
      } else if (frame > impl_->fluidLastFrame_) {
        const int stepCount =
            std::min<int64_t>(frame - impl_->fluidLastFrame_, 8);
        const float dt = 1.0f / static_cast<float>(fps);
        for (int step = 0; step < stepCount; ++step) {
          const int centerX = impl_->fluidGridWidth_ / 2;
          const int centerY = std::max(1, impl_->fluidGridHeight_ - 3);
          const float phase = static_cast<float>(impl_->fluidLastFrame_ + step) *
                              0.07f;
          const float swirlX = std::sin(phase) * 0.85f;
          const float injectDensity =
              1.0f + std::max(0, impl_->particleEmitterCount_) / 24.0f;
          const float injectVelocity =
              std::max(40.0f, impl_->particleEmitterSpeed_) * 0.02f;
          impl_->fluidSolver_->addDensity(centerX, centerY, injectDensity);
          impl_->fluidSolver_->addVelocity(
              centerX, centerY, swirlX, -injectVelocity);
          impl_->fluidSolver_->update(dt);
        }
        impl_->fluidLastFrame_ = frame;
      }

      const QRectF bounds = localBounds();
      impl_->fluidPreviewParticles_.clear();
      if (bounds.isValid() && bounds.width() > 0.0 && bounds.height() > 0.0) {
        const int strideX = std::max(1, impl_->fluidGridWidth_ / 28);
        const int strideY = std::max(1, impl_->fluidGridHeight_ / 28);
        for (int gy = 0; gy < impl_->fluidGridHeight_; gy += strideY) {
          for (int gx = 0; gx < impl_->fluidGridWidth_; gx += strideX) {
            const float density = impl_->fluidSolver_->getDensity(gx, gy);
            if (density < 0.025f) {
              continue;
            }
            ArtifactCore::ParticleVertex particle{};
            const float u = static_cast<float>(gx) /
                            static_cast<float>(std::max(1, impl_->fluidGridWidth_ - 1));
            const float v = static_cast<float>(gy) /
                            static_cast<float>(std::max(1, impl_->fluidGridHeight_ - 1));
            particle.px = static_cast<float>(bounds.left() + u * bounds.width());
            particle.py =
                static_cast<float>(bounds.top() + v * bounds.height());
            particle.pz = 0.0f;
            float vx = 0.0f;
            float vy = 0.0f;
            impl_->fluidSolver_->getVelocity(gx, gy, vx, vy);
            particle.vx = vx * 24.0f;
            particle.vy = vy * 24.0f;
            particle.vz = 0.0f;
            particle.r = 0.42f;
            particle.g = 0.72f;
            particle.b = 1.0f;
            particle.a = std::clamp(density * 0.45f, 0.04f, 0.65f);
            particle.size = 2.0f + density * 9.0f;
            particle.stretch = 1.0f + std::min(std::sqrt(vx * vx + vy * vy), 2.5f);
            particle.rotation = std::atan2(vy, vx);
            particle.age = 0.0f;
            particle.lifetime = 1.0f;
            impl_->fluidPreviewParticles_.push_back(particle);
          }
        }
      }

      if (!impl_->fluidPreviewParticles_.empty()) {
        ArtifactCore::ParticleRenderData renderData;
        renderData.frameNumber = frame;
        renderData.options.blend =
            ArtifactCore::ParticleBlendPolicy::Additive;
        renderData.options.billboard =
            ArtifactCore::ParticleBillboardPolicy::VelocityAligned;
        renderData.particles.reserve(impl_->fluidPreviewParticles_.size());
        for (const auto& sourceParticle : impl_->fluidPreviewParticles_) {
          auto particle = sourceParticle;
          const QVector3D mapped = baseTransform.map(
              QVector3D(particle.px, particle.py, particle.pz));
          particle.px = mapped.x();
          particle.py = mapped.y();
          particle.pz = mapped.z();
          particle.a *= std::clamp(opacityScale, 0.0f, 1.0f);
          renderData.particles.push_back(particle);
        }
        renderer->drawParticles(renderData);
      }
    }
  } else {
    impl_->fluidSolver_.reset();
    impl_->fluidPreviewParticles_.clear();
    impl_->fluidLastFrame_ = std::numeric_limits<int64_t>::min();
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
      impl_->componentParticles_.erase(
          std::remove_if(
              impl_->componentParticles_.begin(),
              impl_->componentParticles_.end(),
              [](const ArtifactCore::ParticleVertex& particle) {
                return particle.age >= particle.lifetime;
              }),
          impl_->componentParticles_.end());
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
      fractureElement.shards.push_back(
          {std::move(clonePolygon), cloneColor});
    }
  }
  submitFractureRenderElement(renderer, fractureElement);
}

void ArtifactAbstractLayer::resetFractureState() {
  ArtifactCore::resetFractureState(impl_->fractureState_);
  impl_->prefractureResult_ = FractureResult{};
  impl_->componentEvaluationState_.fragments.clear();
  impl_->componentEvaluationState_.fragmentGeometry.clear();
  impl_->componentEvaluationState_.clearTransientEvents();
  impl_->fractureMotionLastFrame_ = std::numeric_limits<int64_t>::min();
  impl_->lastCollisionImpactFrame_ = std::numeric_limits<int64_t>::min();
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
  auto& physics = ArtifactCore::PhysicsSystem::instance();
  if (!physics.getSoftBody(id())) {
    physics.createSoftBody(id());
  }
  syncSoftBodyPhysicsColliderToBounds();
}

void ArtifactAbstractLayer::enableSoftBodyPhysicsGrid(int columns, int rows, float stiffness) {
  impl_->softBodyPhysicsEnabled_ = true;
  const QRectF bounds = localBounds();
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    enableSoftBodyPhysics();
    return;
  }

  auto& physics = ArtifactCore::PhysicsSystem::instance();
  // The grid starts inside its layer bounds.  Registering those same bounds
  // as a collider would expel every particle on the first solve, so leave
  // collision sources to explicitly registered external colliders.
  physics.clearSoftBodyColliders(id());
  physics.createSoftBodyGrid(
      id(),
      static_cast<float>(bounds.left()),
      static_cast<float>(bounds.top()),
      static_cast<float>(bounds.width()),
      static_cast<float>(bounds.height()),
      columns,
      rows,
      1.0f,
      stiffness,
      true);
}

void ArtifactAbstractLayer::disableSoftBodyPhysics() {
  impl_->softBodyPhysicsEnabled_ = false;
  ArtifactCore::PhysicsSystem::instance().unregisterSoftBody(id());
}

void ArtifactAbstractLayer::enableRigidBodyPhysics() {
  auto& physics = ArtifactCore::PhysicsSystem::instance();
  if (!physics.getRigidWorld(id())) {
    physics.createRigidWorld(id());
  }
  syncRigidBodyPhysicsToBounds();
}

void ArtifactAbstractLayer::disableRigidBodyPhysics() {
  ArtifactCore::PhysicsSystem::instance().unregisterRigidWorld(id());
  impl_->rigidBodyColliderShape_ = -1;
  impl_->rigidBodyColliderRestitution_ = -1.0f;
}

void ArtifactAbstractLayer::enableMaterialPhysics(int preset) {
  const QRectF bounds = localBounds();
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    return;
  }
  impl_->materialPhysicsEnabled_ = true;
  impl_->materialPhysicsPreset_ = std::clamp(preset, 0, 3);
  ArtifactCore::PhysicsSystem::instance().createMaterialGrid(
      id(), static_cast<float>(bounds.left()), static_cast<float>(bounds.top()),
      static_cast<float>(bounds.width()), static_cast<float>(bounds.height()),
      20, 20, static_cast<ArtifactCore::MpmMaterialPreset>(impl_->materialPhysicsPreset_));
}

void ArtifactAbstractLayer::disableMaterialPhysics() {
  impl_->materialPhysicsEnabled_ = false;
  ArtifactCore::PhysicsSystem::instance().unregisterMaterialSolver(id());
}

void ArtifactAbstractLayer::syncSoftBodyPhysicsColliderToBounds() {
  auto& physics = ArtifactCore::PhysicsSystem::instance();
  auto solver = physics.getSoftBody(id());
  if (!solver) {
    solver = physics.createSoftBody(id());
  }

  const QRectF bounds = layerCollisionLocalBounds(this);
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    physics.clearSoftBodyColliders(id());
    return;
  }

  physics.clearSoftBodyColliders(id());
  ArtifactCore::SoftBodyCollider collider;
  collider.type = ArtifactCore::SoftBodyCollider::Type::Box;
  collider.x = static_cast<float>(bounds.center().x());
  collider.y = static_cast<float>(bounds.center().y());
  collider.width = static_cast<float>(bounds.width());
  collider.height = static_cast<float>(bounds.height());
  collider.restitution = std::clamp(
      impl_->physicsComponent_.settings().restitution, 0.0f, 1.0f);
  collider.friction = 0.15f;
  physics.registerSoftBodyCollider(id(), collider);
  Q_UNUSED(solver);
}

void ArtifactAbstractLayer::syncRigidBodyPhysicsToBounds() {
  auto& physics = ArtifactCore::PhysicsSystem::instance();
  auto world = physics.getRigidWorld(id());
  if (!world) {
    world = physics.createRigidWorld(id());
  }

  const QRectF bounds = layerCollisionLocalBounds(this);
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    return;
  }

  const float cx = static_cast<float>(bounds.center().x());
  const float cy = static_cast<float>(bounds.center().y());
  const float w = static_cast<float>(bounds.width());
  const float h = static_cast<float>(bounds.height());
  const auto shapeProperty = getProperty(
      QStringLiteral("component.collision.shape"));
  const int shape = shapeProperty ? std::clamp(shapeProperty->getValue().toInt(), 0, 2) : 0;
  const float restitution = std::clamp(
      impl_->physicsComponent_.settings().restitution, 0.0f, 1.0f);
  auto bodies = world->getBodies();
  std::shared_ptr<ArtifactCore::RigidBody2D> body;
  if (!bodies.empty()) {
    body = bodies.front();
    if (body && ((impl_->rigidBodyColliderShape_ >= 0 &&
                  impl_->rigidBodyColliderShape_ != shape) ||
                 (impl_->rigidBodyColliderRestitution_ >= 0.0f &&
                  !qFuzzyCompare(impl_->rigidBodyColliderRestitution_, restitution)))) {
      world->removeBody(body);
      body.reset();
    }
    if (body) {
      body->setTransform({cx, cy}, body->angle());
      body->setLinearVelocity({0.0f, 0.0f});
      body->setAngularVelocity(0.0f);
    }
  }
  if (!body) {
    if (shape == 2) {
      body = world->addDynamicCircle(
          cx, cy, std::max(w, h) * 0.5f, 1.0f, 0.3f,
          restitution);
    } else {
      body = world->addDynamicBox(
          cx, cy, w, h, 1.0f, 0.3f,
          restitution);
    }
  }
  if (body) {
    impl_->rigidBodyColliderShape_ = shape;
    impl_->rigidBodyColliderRestitution_ = restitution;
    body->setLinearDamping(0.02f);
    body->setAngularDamping(0.02f);
    body->setFixedRotation(false);
  }
}

QMatrix4x4 ArtifactAbstractLayer::getGlobalTransform4x4() const {
  QMatrix4x4 local = getLocalTransform4x4();
  auto parent = parentLayer();
  if (parent) {
    return combineLayerTransform3D(local, parent->getGlobalTransform4x4());
  }
  return local;
}

float4x4 ArtifactAbstractLayer::getLocalTransformMatrix() const {
  // Transitional boundary: preserve the established layer evaluation
  // (physics, modifiers and animated property overrides), then leave Qt math
  // before entering render/gizmo code.
  const QMatrix4x4 source = getLocalTransform4x4();
  return float4x4{
      source(0, 0), source(0, 1), source(0, 2), source(0, 3),
      source(1, 0), source(1, 1), source(1, 2), source(1, 3),
      source(2, 0), source(2, 1), source(2, 2), source(2, 3),
      source(3, 0), source(3, 1), source(3, 2), source(3, 3)};
}

float4x4 ArtifactAbstractLayer::getGlobalTransformMatrix() const {
  const float4x4 local = getLocalTransformMatrix();
  if (const auto parent = parentLayer()) {
    return parent->getGlobalTransformMatrix() * local;
  }
  return local;
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
    qWarning() << "[Layer] Reject self-parent:" << id.toString();
    return;
  }

  if (impl_->composition_) {
    auto *composition = dynamic_cast<ArtifactAbstractComposition *>(impl_->composition_.data());
    auto parent = composition->layerById(id);
    if (!parent) {
      qWarning() << "[Layer] Reject invalid parent id:" << id.toString();
      return;
    }

    LayerID cursor = id;
    int guard = 0;
    while (!cursor.isNil() && guard++ < 1024) {
      if (cursor == this->id()) {
        qWarning() << "[Layer] Reject cyclic parent:" << id.toString();
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
  setDirty(LayerDirtyFlag::Transform);
  addDirtyReason(LayerDirtyReason::TransformChanged);
  qDebug() << "[Layer] Parent set to:" << id.toString();
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
  setDirty(LayerDirtyFlag::Transform);
  addDirtyReason(LayerDirtyReason::TransformChanged);
  Q_EMIT changed();
}

bool ArtifactAbstractLayer::hasParent() const {
  return !impl_->parentLayerId_.isNil();
}

bool ArtifactAbstractLayer::isGroupLayer() const {
  return false;
}

bool ArtifactAbstractLayer::is3D() const { return impl_->is3D_; }

void ArtifactAbstractLayer::setIs3D(bool value) {
    if (!assignIfChanged(impl_->is3D_, value)) {
      return;
    }
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::PropertyChanged);
}

void ArtifactAbstractLayer::setTimeRemapEnabled(bool enabled) {
    if (!impl_->timeRemapEffect_) {
        impl_->timeRemapEffect_ = std::make_unique<ArtifactCore::TimeRemapEffect>();
    }
    impl_->timeRemapEffect_->setEnabled(enabled);
    impl_->timeRemapEffect_->setHasAudio(hasAudio());
    notifyLayerMutation(this, LayerDirtyFlag::All,
                        LayerDirtyReason::TimelineChanged);
}

void ArtifactAbstractLayer::clearTimeRemap() {
    if (!impl_->timeRemapEffect_) {
        return;
    }
    impl_->timeRemapEffect_.reset();
    notifyLayerMutation(this, LayerDirtyFlag::All,
                        LayerDirtyReason::TimelineChanged);
}

void ArtifactAbstractLayer::setTimeRemapKey(int64_t compFrame,
                                            double sourceFrame) {
    setTimeRemapKey(compFrame, sourceFrame,
                    ArtifactCore::TimeRemapKeyframe::Interpolation::Linear);
}

void ArtifactAbstractLayer::setTimeRemapKey(
    int64_t compFrame,
    double sourceFrame,
    ArtifactCore::TimeRemapKeyframe::Interpolation interpolation) {
    if (!impl_->timeRemapEffect_) {
        impl_->timeRemapEffect_ = std::make_unique<ArtifactCore::TimeRemapEffect>();
    }

    double fps = 30.0;
    if (impl_->composition_) {
        auto *composition = dynamic_cast<ArtifactAbstractComposition *>(impl_->composition_.data());
        fps = composition->frameRate().framerate();
        if (fps <= 0.0) {
            fps = 30.0;
        }
    }

    const double outputTime = static_cast<double>(compFrame) / fps;
    const double sourceTime = sourceFrame / fps;

    ArtifactCore::TimeRemapKeyframe kf;
    kf.outputTime = outputTime;
    kf.sourceTime = sourceTime;
    kf.interpolation = interpolation;

    impl_->timeRemapEffect_->remap().addKeyframe(kf);
    impl_->timeRemapEffect_->remap().setFrameRate(ArtifactCore::FrameRate(fps));
    notifyLayerMutation(this, LayerDirtyFlag::All,
                        LayerDirtyReason::TimelineChanged);
}

bool ArtifactAbstractLayer::isTimeRemapEnabled() const {
    return impl_->timeRemapEffect_ && impl_->timeRemapEffect_->isEnabled();
}

double ArtifactAbstractLayer::getSourceFrameAtCompFrame(int64_t compFrame) const {
    if (!isTimeRemapEnabled()) {
        return static_cast<double>(compFrame);
    }

    double fps = 30.0;
    if (impl_->composition_) {
        auto *composition = dynamic_cast<ArtifactAbstractComposition *>(impl_->composition_.data());
        fps = composition->frameRate().framerate();
        if (fps <= 0.0) {
            fps = 30.0;
        }
    }

    const double outputTime = static_cast<double>(compFrame) / fps;
    float blendFwd = 0.0f, blendBwd = 0.0f;
    return impl_->timeRemapEffect_->processFrame(outputTime, blendFwd, blendBwd);
}

bool ArtifactAbstractLayer::isNullLayer() const { return false; }

bool ArtifactAbstractLayer::isConstructionLayer() const { return false; }

bool ArtifactAbstractLayer::isCompositionBackgroundLayer() const { return false; }

bool ArtifactAbstractLayer::shouldIncludeInFinalRender() const { return true; }

bool ArtifactAbstractLayer::isCloneLayer() const { return false; }

bool ArtifactAbstractLayer::hasAudio() const { return false; }

bool ArtifactAbstractLayer::hasVideo() const { return false; }

Size_2D ArtifactAbstractLayer::sourceSize() const { return impl_->sourceSize_; }

void ArtifactAbstractLayer::setSourceSize(const Size_2D &size) {
  impl_->sourceSize_ = size;
}

Size_2D ArtifactAbstractLayer::aabb() const {
  const auto bounds = transformedBoundingBox();
  if (bounds.width() <= 0 || bounds.height() <= 0) {
    return Size_2D();
  }
  Size_2D result;
  result.width = static_cast<int>(std::ceil(bounds.width()));
  result.height = static_cast<int>(std::ceil(bounds.height()));
  return result;
}

QRectF LayerBounds::boundsFor(LayerBoundsKind kind) const {
  switch (kind) {
    case LayerBoundsKind::Source:
      return sourceBounds;
    case LayerBoundsKind::Visible:
      return visibleBounds;
    case LayerBoundsKind::Effect:
      return effectBounds;
    case LayerBoundsKind::Mask:
      return maskBounds;
    case LayerBoundsKind::Layout:
      return layoutBounds;
  }
  return layoutBounds;
}

LayerBounds ArtifactAbstractLayer::contentBounds() const {
  const QRectF source = localBounds();
  const QRectF visible = transformedBoundingBox();
  LayerBounds bounds;
  bounds.sourceBounds = source;
  bounds.visibleBounds = visible;
  bounds.effectBounds = visible;
  bounds.maskBounds = visible;
  bounds.layoutBounds = source.isValid() ? source : visible;
  return bounds;
}

QJsonObject GuideDefinition::toJson() const {
  QJsonObject obj;
  obj.insert(QStringLiteral("guideId"), guideId);
  obj.insert(QStringLiteral("name"), name);
  obj.insert(QStringLiteral("purpose"), purpose);
  obj.insert(QStringLiteral("orientation"), static_cast<int>(orientation));
  obj.insert(QStringLiteral("position"), position);
  obj.insert(QStringLiteral("start"), start);
  obj.insert(QStringLiteral("end"), end);
  obj.insert(QStringLiteral("enabled"), enabled);
  obj.insert(QStringLiteral("priority"), static_cast<int>(priority));
  obj.insert(QStringLiteral("semanticTag"), static_cast<int>(semanticTag));
  return obj;
}

GuideDefinition GuideDefinition::fromJson(const QJsonObject &obj) {
  GuideDefinition guide;
  guide.guideId = obj.value(QStringLiteral("guideId")).toString();
  guide.name = obj.value(QStringLiteral("name")).toString();
  guide.purpose = obj.value(QStringLiteral("purpose")).toString();
  guide.orientation = static_cast<GuideOrientation>(
      obj.value(QStringLiteral("orientation")).toInt(static_cast<int>(GuideOrientation::Horizontal)));
  guide.position = obj.value(QStringLiteral("position")).toDouble(0.0);
  guide.start = obj.value(QStringLiteral("start")).toDouble(0.0);
  guide.end = obj.value(QStringLiteral("end")).toDouble(0.0);
  guide.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
  guide.priority = static_cast<GuidePriority>(
      obj.value(QStringLiteral("priority")).toInt(static_cast<int>(GuidePriority::Normal)));
  guide.semanticTag = static_cast<GuideSemanticTag>(
      obj.value(QStringLiteral("semanticTag")).toInt(static_cast<int>(GuideSemanticTag::Custom)));
  return guide;
}

QJsonObject GuideBinding::toJson() const {
  QJsonObject obj;
  obj.insert(QStringLiteral("guideId"), guideId);
  obj.insert(QStringLiteral("role"), role);
  obj.insert(QStringLiteral("offset"), offset);
  obj.insert(QStringLiteral("follow"), follow);
  obj.insert(QStringLiteral("enabled"), enabled);
  obj.insert(QStringLiteral("priority"), static_cast<int>(priority));
  return obj;
}

GuideBinding GuideBinding::fromJson(const QJsonObject &obj) {
  GuideBinding binding;
  binding.guideId = obj.value(QStringLiteral("guideId")).toString();
  binding.role = obj.value(QStringLiteral("role")).toString();
  binding.offset = obj.value(QStringLiteral("offset")).toDouble(0.0);
  binding.follow = obj.value(QStringLiteral("follow")).toBool(false);
  binding.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
  binding.priority = static_cast<GuidePriority>(
      obj.value(QStringLiteral("priority")).toInt(static_cast<int>(GuidePriority::Normal)));
  return binding;
}

QJsonObject GuideSet::toJson() const {
  QJsonObject obj;
  obj.insert(QStringLiteral("ownerId"), ownerId);
  QJsonArray guideArray;
  for (const auto &guide : guides) {
    guideArray.append(guide.toJson());
  }
  obj.insert(QStringLiteral("guides"), guideArray);
  QJsonArray bindingArray;
  for (const auto &binding : bindings) {
    bindingArray.append(binding.toJson());
  }
  obj.insert(QStringLiteral("bindings"), bindingArray);
  return obj;
}

GuideSet GuideSet::fromJson(const QJsonObject &obj) {
  GuideSet set;
  set.ownerId = obj.value(QStringLiteral("ownerId")).toString();
  for (const auto &value : obj.value(QStringLiteral("guides")).toArray()) {
    set.guides.append(GuideDefinition::fromJson(value.toObject()));
  }
  for (const auto &value : obj.value(QStringLiteral("bindings")).toArray()) {
    set.bindings.append(GuideBinding::fromJson(value.toObject()));
  }
  return set;
}

QVector<GuideDefinition> GuideSet::guidesForSemanticTag(GuideSemanticTag tag) const {
  QVector<GuideDefinition> result;
  for (const auto& g : guides) {
    if (g.semanticTag == tag) {
      result.append(g);
    }
  }
  return result;
}

QVector<GuideDefinition> GuideSet::enabledGuides() const {
  QVector<GuideDefinition> result;
  for (const auto& g : guides) {
    if (g.enabled) {
      result.append(g);
    }
  }
  return result;
}

QVector<GuideBinding> GuideSet::enabledBindings() const {
  QVector<GuideBinding> result;
  for (const auto& b : bindings) {
    if (b.enabled) {
      result.append(b);
    }
  }
  return result;
}

GuideDefinition* GuideSet::guideById(const QString& guideId) {
  for (auto& g : guides) {
    if (g.guideId == guideId) {
      return &g;
    }
  }
  return nullptr;
}

void GuideSet::sortByPriority() {
  std::sort(guides.begin(), guides.end(), [](const GuideDefinition& a, const GuideDefinition& b) {
    return static_cast<int>(a.priority) > static_cast<int>(b.priority);
  });
  std::sort(bindings.begin(), bindings.end(), [](const GuideBinding& a, const GuideBinding& b) {
    return static_cast<int>(a.priority) > static_cast<int>(b.priority);
  });
}

QRectF ArtifactAbstractLayer::contentBounds(LayerBoundsKind kind) const {
  return contentBounds().boundsFor(kind);
}

QRectF ArtifactAbstractLayer::sourceBounds() const {
  return contentBounds(LayerBoundsKind::Source);
}

QRectF ArtifactAbstractLayer::visibleBounds() const {
  return contentBounds(LayerBoundsKind::Visible);
}

QString ArtifactAbstractLayer::contentBoundsSummary() const {
  const LayerBounds bounds = contentBounds();
  const auto rectString = [](const QRectF &rect) {
    return rect.isValid()
               ? QStringLiteral("%1,%2 %3x%4")
                     .arg(rect.x(), 0, 'f', 1)
                     .arg(rect.y(), 0, 'f', 1)
                     .arg(rect.width(), 0, 'f', 1)
                     .arg(rect.height(), 0, 'f', 1)
               : QStringLiteral("invalid");
  };

  return QStringLiteral("source=%1 visible=%2 effect=%3 mask=%4 layout=%5")
      .arg(rectString(bounds.sourceBounds),
           rectString(bounds.visibleBounds),
           rectString(bounds.effectBounds),
           rectString(bounds.maskBounds),
           rectString(bounds.layoutBounds));
}

QRectF ArtifactAbstractLayer::effectBounds() const {
  return contentBounds(LayerBoundsKind::Effect);
}

QRectF ArtifactAbstractLayer::maskBounds() const {
  return contentBounds(LayerBoundsKind::Mask);
}

QRectF ArtifactAbstractLayer::layoutBounds() const {
  return contentBounds(LayerBoundsKind::Layout);
}

QRectF ArtifactAbstractLayer::localBounds() const {
  const auto size = sourceSize();
  if (size.width <= 0 || size.height <= 0) {
    return QRectF();
  }
  return QRectF(0.0, 0.0, static_cast<qreal>(size.width),
                static_cast<qreal>(size.height));
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
    const auto cloner = std::dynamic_pointer_cast<ClonerGenerator>(effect);
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

bool ArtifactAbstractLayer::getAudio(AudioSegment &outSegment,
                                     const FramePosition &start, int frameCount,
                                     int sampleRate) {
  // Default implementation: no audio
  Q_UNUSED(outSegment);
  Q_UNUSED(start);
  Q_UNUSED(frameCount);
  Q_UNUSED(sampleRate);
  return false;
}

QRectF ArtifactAbstractLayer::transformedBoundingBox() const {
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
  const bool layoutManaged = layoutEnabledProperty &&
                             layoutEnabledProperty->getValue().toBool() &&
                             (!participationProperty ||
                              participationProperty->getValue().toInt() != 2);
  if (!layoutManaged &&
      impl_->cachedBoundingBoxRevision_ == impl_->geometryRevision_ &&
      impl_->cachedBoundingBoxParentRevision_ == parentRevision &&
      impl_->cachedBoundingBoxFrame_ == frame &&
      impl_->cachedBoundingBoxParentId_ == parentId) {
    return impl_->cachedBoundingBox_;
  }
  const QRectF localRect = localBounds();
  if (!localRect.isValid() || localRect.width() <= 0.0 ||
      localRect.height() <= 0.0) {
    impl_->cachedBoundingBox_ = QRectF();
  } else {
    impl_->cachedBoundingBox_ = getGlobalTransform().mapRect(visualLocalBounds());
  }
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
  return QVector3D(impl_->transform_.rotationAt(time), 0,
                   0); // Only X rotation for now
}

void ArtifactAbstractLayer::setRotation3D(const QVector3D &rot) {
  const auto time = currentTimelineTime(this);
  impl_->transform_.setRotation(time, rot.x());
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
  obj["effectEnvelope"] = layerEffectEnvelopeToJson(impl_->effectEnvelope_);

  // Mattes
  QJsonArray mattesArr;
  for (const auto &matte : impl_->mattes_) {
    mattesArr.append(QJsonValue(matte.toJson()));
  }
  obj["mattes"] = mattesArr;

  // Transform
  QJsonObject trans;
  const auto &t3 = transform3D();
  trans["px"] = t3.positionX();
  trans["py"] = t3.positionY();
  trans["pz"] = t3.positionZ();
  trans["rx"] = t3.rotation(); // Currently only 1 rotation in ixx outline
  trans["sx"] = t3.scaleX();
  trans["sy"] = t3.scaleY();
  trans["ax"] = t3.anchorX();
  trans["ay"] = t3.anchorY();
  trans["az"] = t3.anchorZ();
  QJsonArray positionKeyframes;
  for (const auto &time : t3.getPositionKeyFrameTimes()) {
    const auto frame = time.rescaledTo(24);
    QJsonObject keyframe;
    keyframe["frame"] = static_cast<qint64>(frame);
    keyframe["x"] = t3.positionXAt(time);
    keyframe["y"] = t3.positionYAt(time);
    keyframe["xInterpolation"] = static_cast<int>(
        t3.positionXKeyFrameInterpolationAt(time));
    keyframe["yInterpolation"] = static_cast<int>(
        t3.positionYKeyFrameInterpolationAt(time));
    ArtifactCore::PositionSpatialTangents tangents;
    if (t3.positionKeyFrameSpatialTangentsAt(time, tangents)) {
      keyframe["inTangentX"] = tangents.inTangent.x;
      keyframe["inTangentY"] = tangents.inTangent.y;
      keyframe["outTangentX"] = tangents.outTangent.x;
      keyframe["outTangentY"] = tangents.outTangent.y;
      keyframe["tangentsLinked"] = tangents.linked;
    }
    positionKeyframes.append(keyframe);
  }
  if (!positionKeyframes.isEmpty()) {
    trans["positionKeyframes"] = positionKeyframes;
  }
  obj["transform"] = trans;

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
  obj["physics"] = impl_->physicsComponent_.settings().toJson();
  obj["clonePhysicsInitialVelocityY"] =
      static_cast<double>(impl_->clonePhysicsInitialVelocityY_);
  obj["clonePhysicsMaxBounces"] = impl_->clonePhysicsMaxBounces_;
  obj["softBodyPhysicsEnabled"] = impl_->softBodyPhysicsEnabled_;
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
  fractureObj["stateKind"] = static_cast<int>(impl_->fractureState_.kind);
  fractureObj["stateDamage"] = static_cast<double>(impl_->fractureState_.damage);
  fractureObj["stateLastImpact"] = static_cast<double>(impl_->fractureState_.lastImpact);
  fractureObj["stateCrackProgress"] = static_cast<double>(impl_->fractureState_.crackProgress);
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
  componentsObj["fluidGridWidth"] = impl_->fluidGridWidth_;
  componentsObj["fluidGridHeight"] = impl_->fluidGridHeight_;
  componentsObj["fluidViscosity"] = static_cast<double>(impl_->fluidViscosity_);
  componentsObj["fluidDiffusion"] = static_cast<double>(impl_->fluidDiffusion_);
  componentsObj["fluidBuoyancy"] = static_cast<double>(impl_->fluidBuoyancy_);
  componentsObj["fluidVorticity"] = static_cast<double>(impl_->fluidVorticity_);
  componentsObj["fluidSolverIterations"] = impl_->fluidSolverIterations_;
  componentsObj["layoutMode"] = impl_->layoutMode_;
  componentsObj["layoutAnchorMode"] = impl_->layoutAnchorMode_;
  componentsObj["layoutHorizontalPin"] = impl_->layoutHorizontalPin_;
  componentsObj["layoutVerticalPin"] = impl_->layoutVerticalPin_;
  componentsObj["layoutScaleMode"] = impl_->layoutScaleMode_;
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
      varObj["name"] = QString::fromStdString(varPtr->name_);
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
          vtrans["rx"] = vt3.rotation();
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
      const auto layerMask = impl_->getMask(maskIndex);
      QJsonObject mobj;
      mobj["enabled"] = layerMask.isEnabled();

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
  // Default: base class is abstract and cannot be instantiated here.
  // Subclasses should implement their own fromJson factory. Return nullptr
  // to indicate this layer cannot be constructed generically.
  Q_UNUSED(obj);
  return ArtifactAbstractLayerPtr();
}

void ArtifactAbstractLayer::applyPropertiesFromJson(const QJsonObject &obj) {
  // Default implementation: apply effect properties if matching effects exist
  // Subclasses should override to handle layer-specific fields
  if (!obj.contains("effects") || !obj["effects"].isArray())
    return;
  if (obj.contains("isAdjustment")) {
    setAdjustmentLayer(obj["isAdjustment"].toBool());
  }

  const auto arr = obj.value("effects").toArray();
  for (const auto &ev : arr) {
    if (!ev.isObject())
      continue;
    auto eobj = ev.toObject();
    if (!eobj.contains("id"))
      continue;
    UniString eid(eobj["id"].toString().toStdString());
    auto eff = getEffect(eid);
    if (!eff)
      continue;
    if (!eobj.contains("properties") || !eobj["properties"].isArray())
      continue;
    auto props = eobj["properties"].toArray();
    for (const auto &pv : props) {
      if (!pv.isObject())
        continue;
      auto pobj = pv.toObject();
      QString name = pobj.value("name").toString();
      int t = pobj.value("type").toInt(
          static_cast<int>(ArtifactCore::PropertyType::String));
      ArtifactCore::PropertyType ptype =
          static_cast<ArtifactCore::PropertyType>(t);
      QVariant val;
      if (pobj.contains("value")) {
        if (ptype == ArtifactCore::PropertyType::Color &&
            pobj.value("value").isObject()) {
          auto col = pobj.value("value").toObject();
          double r = col.value("r").toDouble(0.0);
          double g = col.value("g").toDouble(0.0);
          double b = col.value("b").toDouble(0.0);
          double a = col.value("a").toDouble(1.0);
          QColor qc;
          qc.setRedF(static_cast<float>(r));
          qc.setGreenF(static_cast<float>(g));
          qc.setBlueF(static_cast<float>(b));
          qc.setAlphaF(static_cast<float>(a));
          val = QVariant(qc);
        } else {
          val = pobj.value("value").toVariant();
        }
      }
      eff->setPropertyValue(UniString(name.toStdString()), val);
      if (pobj.contains("keyframes") || pobj.contains("expression") ||
          pobj.contains("envelopes")) {
        auto editable = eff->editableProperty(name);
        if (editable) {
          ArtifactCore::SerializedProperty serialized;
          serialized.name = name;
          serialized.type = static_cast<int>(ptype);
          serialized.value = pobj.value("value");
          serialized.expression = pobj.value("expression").toString();
          serialized.keyframes = pobj.value("keyframes").toArray();
          serialized.envelopes = pobj.value("envelopes").toArray();
          ArtifactCore::PropertySerializationBridge::deserializeProperty(
              editable, serialized);
        }
      }
    }
  }
}

void ArtifactAbstractLayer::fromJsonProperties(const QJsonObject &obj) {
  if (obj.contains("name"))
    setLayerName(obj["name"].toString());
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
    impl_->mattes_.clear();
    for (const auto &matteVal : mattesArr) {
      if (matteVal.isObject()) {
        LayerMatteReference matte;
        matte.fromJson(matteVal.toObject());
        impl_->mattes_.push_back(matte);
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
    QJsonObject trans = obj["transform"].toObject();
    auto &t3 = transform3D();
    // Time zero only needs a stable scale; avoid implying a fake fps.
    RationalTime t0(0, 1);
    if (trans.contains("px"))
      t3.setPosition(t0, trans["px"].toDouble(), trans["py"].toDouble());
    if (trans.contains("pz"))
      t3.setPositionZ(t0, trans["pz"].toDouble());
    if (trans.contains("rx"))
      t3.setRotation(t0, trans["rx"].toDouble());
    if (trans.contains("sx"))
      t3.setScale(t0, trans["sx"].toDouble(), trans["sy"].toDouble());
    if (trans.contains("ax"))
      t3.setAnchor(t0, trans["ax"].toDouble(), trans["ay"].toDouble(),
                   trans["az"].toDouble());
    if (trans.contains("positionKeyframes") &&
        trans["positionKeyframes"].isArray()) {
      t3.clearPositionKeyFrames();
      for (const auto &value : trans["positionKeyframes"].toArray()) {
        if (!value.isObject()) {
          continue;
        }
        const QJsonObject keyframe = value.toObject();
        const ArtifactCore::RationalTime time(
            keyframe["frame"].toInteger(), 24);
        t3.setPositionKeyFrameValueAt(
            time, static_cast<float>(keyframe["x"].toDouble()),
            static_cast<float>(keyframe["y"].toDouble()));
        t3.setPositionKeyFrameInterpolationAt(
            time, static_cast<ArtifactCore::InterpolationType>(
                      keyframe["xInterpolation"].toInt(
                          static_cast<int>(ArtifactCore::InterpolationType::Linear))),
            static_cast<ArtifactCore::InterpolationType>(
                      keyframe["yInterpolation"].toInt(
                          static_cast<int>(ArtifactCore::InterpolationType::Linear))));
        if (keyframe.contains("inTangentX") ||
            keyframe.contains("outTangentX")) {
          ArtifactCore::PositionSpatialTangents tangents;
          tangents.inTangent.x = static_cast<float>(
              keyframe["inTangentX"].toDouble());
          tangents.inTangent.y = static_cast<float>(
              keyframe["inTangentY"].toDouble());
          tangents.outTangent.x = static_cast<float>(
              keyframe["outTangentX"].toDouble());
          tangents.outTangent.y = static_cast<float>(
              keyframe["outTangentY"].toDouble());
          tangents.linked = keyframe["tangentsLinked"].toBool(true);
          t3.setPositionKeyFrameSpatialTangentsAt(time, tangents);
        }
      }
    }
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

  if (obj.contains("physics") && obj["physics"].isObject()) {
      impl_->physicsComponent_.settings().fromJson(obj["physics"].toObject());
      impl_->physicsComponent_.reset();
  }
  impl_->clonePhysicsInitialVelocityY_ = static_cast<float>(
      obj.value(QStringLiteral("clonePhysicsInitialVelocityY"))
          .toDouble(impl_->clonePhysicsInitialVelocityY_));
  impl_->clonePhysicsMaxBounces_ = std::clamp(
      obj.value(QStringLiteral("clonePhysicsMaxBounces"))
          .toInt(impl_->clonePhysicsMaxBounces_), 0, 32);
  if (obj.contains("softBodyPhysicsEnabled") &&
      obj["softBodyPhysicsEnabled"].toBool(false)) {
      enableSoftBodyPhysicsGrid();
  }
  if (obj.contains("materialPhysicsEnabled") &&
      obj["materialPhysicsEnabled"].toBool(false)) {
      enableMaterialPhysics(obj["materialPhysicsPreset"].toInt(0));
  }
  if (obj.contains("motion") && obj["motion"].isObject()) {
      const QJsonObject motionObj = obj["motion"].toObject();
      impl_->motionDynamicsEnabled_ = motionObj.value(QStringLiteral("enabled")).toBool(false);
      impl_->motionDynamicsMode_ = motionObj.value(QStringLiteral("mode")).toInt(0);
      impl_->motionDynamicsStiffness_ = static_cast<float>(
          std::clamp(motionObj.value(QStringLiteral("stiffness")).toDouble(80.0), 0.0, 1000.0));
      impl_->motionDynamicsDamping_ = static_cast<float>(
          std::clamp(motionObj.value(QStringLiteral("damping")).toDouble(16.0), 0.0, 100.0));
      impl_->motionDynamicsMass_ = static_cast<float>(
          std::clamp(motionObj.value(QStringLiteral("mass")).toDouble(1.0), 0.1, 100.0));
      impl_->motionDynamicsLagTau_ = static_cast<float>(
          std::clamp(motionObj.value(QStringLiteral("lagTau")).toDouble(0.1), 0.001, 10.0));
      impl_->motionDynamicsClampOvershoot_ =
          motionObj.value(QStringLiteral("clampOvershoot")).toBool(false);
      impl_->motionDynamicsOvershootLimit_ = static_cast<float>(
          std::clamp(motionObj.value(QStringLiteral("overshootLimit")).toDouble(0.3), 0.0, 2.0));
      impl_->motionLastFrame_ = std::numeric_limits<int64_t>::min();
  }
  if (obj.contains("fracture") && obj["fracture"].isObject()) {
      const QJsonObject fractureObj = obj["fracture"].toObject();
      impl_->fractureEnabled_ = fractureObj.value(QStringLiteral("enabled")).toBool(false);
      impl_->fracturePreset_ = std::clamp(fractureObj.value(QStringLiteral("preset")).toInt(static_cast<int>(FracturePreset::Glass)), 0, static_cast<int>(FracturePreset::Dust));
      impl_->fractureCrackThreshold_ = static_cast<float>(
          std::clamp(fractureObj.value(QStringLiteral("crackThreshold")).toDouble(1.0), 0.0, 1000.0));
      impl_->fractureShatterThreshold_ = static_cast<float>(
          std::clamp(fractureObj.value(QStringLiteral("shatterThreshold")).toDouble(2.5), 0.0, 1000.0));
      impl_->fractureShardCount_ = std::max(1, fractureObj.value(QStringLiteral("shardCount")).toInt(16));
      impl_->fractureShardDamping_ = static_cast<float>(
          std::clamp(fractureObj.value(QStringLiteral("shardDamping")).toDouble(0.92), 0.0, 1.0));
      impl_->fractureShardGravity_ = static_cast<float>(
          std::clamp(fractureObj.value(QStringLiteral("shardGravity")).toDouble(0.0), -5000.0, 5000.0));
      impl_->fractureImpactSensitivity_ = static_cast<float>(
          std::clamp(fractureObj.value(QStringLiteral("impactSensitivity")).toDouble(1.0), 0.0, 10.0));
      impl_->fracturePreGenerate_ =
          fractureObj.value(QStringLiteral("preGenerate")).toBool(false);
      impl_->fractureTriggerFrame_ = fractureObj.contains(
          QStringLiteral("triggerFrame"))
          ? static_cast<int64_t>(fractureObj.value(
                QStringLiteral("triggerFrame")).toVariant().toLongLong())
          : -1;
      impl_->fractureState_.kind = static_cast<FractureStateKind>(
          fractureObj.value(QStringLiteral("stateKind")).toInt(static_cast<int>(FractureStateKind::Intact)));
      impl_->fractureState_.damage = static_cast<float>(
          fractureObj.value(QStringLiteral("stateDamage")).toDouble(0.0));
      impl_->fractureState_.lastImpact = static_cast<float>(
          fractureObj.value(QStringLiteral("stateLastImpact")).toDouble(0.0));
      impl_->fractureState_.crackProgress = static_cast<float>(
          fractureObj.value(QStringLiteral("stateCrackProgress")).toDouble(0.0));
  }
  if (obj.contains("trail") && obj["trail"].isObject()) {
      const QJsonObject trailObj = obj["trail"].toObject();
      impl_->motionTrailEnabled_ = trailObj.value(QStringLiteral("enabled")).toBool(false);
      impl_->motionTrailLength_ = std::clamp(trailObj.value(QStringLiteral("length")).toInt(24), 2, 256);
      impl_->motionTrailFade_ = static_cast<float>(
          std::clamp(trailObj.value(QStringLiteral("fade")).toDouble(0.72), 0.0, 1.0));
      impl_->motionTrailWidth_ = static_cast<float>(
          std::clamp(trailObj.value(QStringLiteral("width")).toDouble(2.0), 0.1, 128.0));
      impl_->motionTrailHistory_.clear();
      impl_->motionTrailLastFrame_ = std::numeric_limits<int64_t>::min();
  }
  if (obj.contains("fragmentAppearance") &&
      obj["fragmentAppearance"].isObject()) {
      const QJsonObject appearanceObj = obj["fragmentAppearance"].toObject();
      impl_->fragmentVelocityStretchEnabled_ =
          appearanceObj.value(QStringLiteral("velocityStretchEnabled")).toBool(false);
      impl_->fragmentVelocityStretchStrength_ = static_cast<float>(std::clamp(
          appearanceObj.value(QStringLiteral("velocityStretchStrength")).toDouble(0.01),
          0.0, 1.0));
      impl_->fragmentVelocityStretchMax_ = static_cast<float>(std::clamp(
          appearanceObj.value(QStringLiteral("velocityStretchMax")).toDouble(3.0),
          1.0, 32.0));
      impl_->fragmentColorVariationEnabled_ =
          appearanceObj.value(QStringLiteral("colorVariationEnabled")).toBool(false);
      impl_->fragmentColorVariation_ = static_cast<float>(std::clamp(
          appearanceObj.value(QStringLiteral("colorVariation")).toDouble(0.35),
          0.0, 1.0));
      impl_->fragmentClonerOutputEnabled_ = appearanceObj.value(
          QStringLiteral("clonerOutputEnabled")).toBool(false);
      impl_->fragmentClonerOutputCount_ = std::clamp(appearanceObj.value(
          QStringLiteral("clonerOutputCount")).toInt(1), 1, 256);
      impl_->fragmentClonerOutputSpacingX_ = static_cast<float>(std::clamp(
          appearanceObj.value(QStringLiteral("clonerOutputSpacingX")).toDouble(24.0),
          -100000.0, 100000.0));
      impl_->fragmentClonerOutputSpacingY_ = static_cast<float>(std::clamp(
          appearanceObj.value(QStringLiteral("clonerOutputSpacingY")).toDouble(0.0),
          -100000.0, 100000.0));
      impl_->fragmentClonerOutputTimeOffsetFrames_ = static_cast<float>(std::clamp(
          appearanceObj.value(QStringLiteral("clonerOutputTimeOffsetFrames")).toDouble(0.0),
          -10000.0, 10000.0));
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
        impl_->collisionShape_ = std::clamp(
            componentsObj.value(QStringLiteral("collisionShape")).toInt(0), 0,
            2);
        impl_->collisionWidth_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("collisionWidth")).toDouble(0.0),
            0.0, 100000.0));
        impl_->collisionHeight_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("collisionHeight")).toDouble(0.0),
            0.0, 100000.0));
        impl_->collisionRadius_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("collisionRadius")).toDouble(0.0),
            0.0, 100000.0));
        impl_->collisionOffsetX_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("collisionOffsetX")).toDouble(0.0),
            -100000.0, 100000.0));
        impl_->collisionOffsetY_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("collisionOffsetY")).toDouble(0.0),
            -100000.0, 100000.0));
        impl_->collisionFloorY_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("collisionFloorY")).toDouble(0.0),
            0.0, 100000.0));
        impl_->collisionCompositionBounds_ = componentsObj.value(
            QStringLiteral("collisionCompositionBounds")).toBool(false);
        impl_->crowdComponentEnabled_ =
            componentsObj.value(QStringLiteral("crowdEnabled")).toBool(false);
        impl_->crowdCohesion_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("crowdCohesion")).toDouble(0.5),
            0.0, 10.0));
        impl_->crowdSeparation_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("crowdSeparation")).toDouble(0.5),
            0.0, 10.0));
        impl_->crowdAlignment_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("crowdAlignment")).toDouble(0.5),
            0.0, 10.0));
        impl_->crowdMaxSpeed_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("crowdMaxSpeed")).toDouble(120.0),
            0.0, 10000.0));
        impl_->crowdJitter_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("crowdJitter")).toDouble(0.1),
            0.0, 10.0));
        impl_->particleEmitterComponentEnabled_ =
            componentsObj.value(QStringLiteral("particleEmitterEnabled"))
                .toBool(false);
        impl_->particleEmitterCount_ = std::clamp(
            componentsObj.value(QStringLiteral("particleEmitterCount")).toInt(16),
            0, 100000);
        impl_->particleEmitterSpeed_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("particleEmitterSpeed"))
                .toDouble(120.0),
            0.0, 100000.0));
        impl_->particleEmitterLifetime_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("particleEmitterLifetime"))
                .toDouble(1.0),
            0.01, 3600.0));
        impl_->fluidComponentEnabled_ =
            componentsObj.value(QStringLiteral("fluidEnabled")).toBool(false);
        impl_->fluidGridWidth_ = std::clamp(
            componentsObj.value(QStringLiteral("fluidGridWidth")).toInt(128),
            8, 4096);
        impl_->fluidGridHeight_ = std::clamp(
            componentsObj.value(QStringLiteral("fluidGridHeight")).toInt(128),
            8, 4096);
        impl_->fluidViscosity_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("fluidViscosity"))
                .toDouble(0.00001),
            0.0, 1.0));
        impl_->fluidDiffusion_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("fluidDiffusion"))
                .toDouble(0.00001),
            0.0, 1.0));
        impl_->fluidBuoyancy_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("fluidBuoyancy"))
                .toDouble(0.05),
            -2.0, 2.0));
        impl_->fluidVorticity_ = static_cast<float>(std::clamp(
            componentsObj.value(QStringLiteral("fluidVorticity"))
                .toDouble(0.1),
            0.0, 10.0));
        impl_->fluidSolverIterations_ = std::clamp(
            componentsObj.value(QStringLiteral("fluidSolverIterations")).toInt(20),
            1, 256);
        impl_->layoutMode_ =
            componentsObj.value(QStringLiteral("layoutMode")).toInt(0);
        impl_->layoutAnchorMode_ =
            componentsObj.value(QStringLiteral("layoutAnchorMode")).toInt(0);
        impl_->layoutHorizontalPin_ =
            componentsObj.value(QStringLiteral("layoutHorizontalPin")).toInt(0);
        impl_->layoutVerticalPin_ =
            componentsObj.value(QStringLiteral("layoutVerticalPin")).toInt(0);
        impl_->layoutScaleMode_ =
            componentsObj.value(QStringLiteral("layoutScaleMode")).toInt(0);
        impl_->layoutSafeAreaEnabled_ =
            componentsObj.value(QStringLiteral("layoutSafeAreaEnabled")).toBool(false);
        impl_->layoutSafeAreaPaddingX_ = static_cast<float>(
            componentsObj.value(QStringLiteral("layoutSafeAreaPaddingX")).toDouble(0.0));
        impl_->layoutSafeAreaPaddingY_ = static_cast<float>(
            componentsObj.value(QStringLiteral("layoutSafeAreaPaddingY")).toDouble(0.0));
        impl_->layoutStackDirection_ =
            componentsObj.value(QStringLiteral("layoutStackDirection")).toInt(0);
        impl_->layoutGap_ = static_cast<float>(
            componentsObj.value(QStringLiteral("layoutGap")).toDouble(24.0));
        impl_->layoutMaxPerRow_ =
            std::max(0, componentsObj.value(QStringLiteral("layoutMaxPerRow")).toInt(0));
        impl_->clonerMode_ =
            componentsObj.value(QStringLiteral("clonerMode")).toInt(0);
        impl_->clonerCloneCount_ = std::max(
            1, componentsObj.value(QStringLiteral("clonerCloneCount")).toInt(3));
        impl_->clonerOffsetX_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerOffsetX")).toDouble(160.0));
        impl_->clonerOffsetY_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerOffsetY")).toDouble(48.0));
        impl_->clonerOffsetZ_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerOffsetZ")).toDouble(0.0));
        impl_->clonerJitterX_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerJitterX")).toDouble(0.0));
        impl_->clonerJitterY_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerJitterY")).toDouble(0.0));
        impl_->clonerJitterZ_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerJitterZ")).toDouble(0.0));
        impl_->clonerSeed_ =
            componentsObj.value(QStringLiteral("clonerSeed")).toInt(0);
        impl_->clonerColumns_ = std::max(
            1, componentsObj.value(QStringLiteral("clonerColumns")).toInt(3));
        impl_->clonerRows_ = std::max(
            1, componentsObj.value(QStringLiteral("clonerRows")).toInt(3));
        impl_->clonerDepth_ = std::max(
            1, componentsObj.value(QStringLiteral("clonerDepth")).toInt(1));
        impl_->clonerSpacingX_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerSpacingX")).toDouble(160.0));
        impl_->clonerSpacingY_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerSpacingY")).toDouble(48.0));
        impl_->clonerSpacingZ_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerSpacingZ")).toDouble(0.0));
        impl_->clonerRadialCount_ = std::max(
            1, componentsObj.value(QStringLiteral("clonerRadialCount")).toInt(8));
        impl_->clonerRadius_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerRadius")).toDouble(160.0));
        impl_->clonerStartAngle_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerStartAngle")).toDouble(0.0));
        impl_->clonerEndAngle_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerEndAngle")).toDouble(360.0));
        impl_->clonerRotationStep_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerRotationStep")).toDouble(0.0));
        impl_->clonerOpacityDecay_ = static_cast<float>(
            componentsObj.value(QStringLiteral("clonerOpacityDecay")).toDouble(0.0));
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
            op.position.setX(static_cast<float>(
                transformObj.value(QStringLiteral("positionX")).toDouble(0.0)));
            op.position.setY(static_cast<float>(
                transformObj.value(QStringLiteral("positionY")).toDouble(0.0)));
            op.position.setZ(static_cast<float>(
                transformObj.value(QStringLiteral("positionZ")).toDouble(0.0)));
            op.rotation.setX(static_cast<float>(
                transformObj.value(QStringLiteral("rotationX")).toDouble(0.0)));
            op.rotation.setY(static_cast<float>(
                transformObj.value(QStringLiteral("rotationY")).toDouble(0.0)));
            op.rotation.setZ(static_cast<float>(
                transformObj.value(QStringLiteral("rotationZ")).toDouble(0.0)));
            op.scale.setX(static_cast<float>(
                transformObj.value(QStringLiteral("scaleX")).toDouble(1.0)));
            op.scale.setY(static_cast<float>(
                transformObj.value(QStringLiteral("scaleY")).toDouble(1.0)));
            op.scale.setZ(static_cast<float>(
                transformObj.value(QStringLiteral("scaleZ")).toDouble(1.0)));
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
            op.position.setX(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformPositionX")).toDouble(0.0)));
            op.position.setY(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformPositionY")).toDouble(0.0)));
            op.position.setZ(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformPositionZ")).toDouble(0.0)));
            op.rotation.setX(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformRotationX")).toDouble(0.0)));
            op.rotation.setY(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformRotationY")).toDouble(0.0)));
            op.rotation.setZ(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformRotationZ")).toDouble(0.0)));
            op.scale.setX(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformScaleX")).toDouble(1.0)));
            op.scale.setY(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformScaleY")).toDouble(1.0)));
            op.scale.setZ(static_cast<float>(
                componentsObj.value(QStringLiteral("clonerTransformScaleZ")).toDouble(1.0)));
            impl_->clonerTransforms_.push_back(op);
          }
        }
        impl_->extraGeneratorDescriptors_.clear();
        if (componentsObj.contains(QStringLiteral("generators")) &&
            componentsObj.value(QStringLiteral("generators")).isArray()) {
          const auto generatorsArr =
              componentsObj.value(QStringLiteral("generators")).toArray();
          impl_->extraGeneratorDescriptors_.reserve(
              static_cast<size_t>(generatorsArr.size()));
          for (const auto& generatorValue : generatorsArr) {
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
              static_cast<size_t>(fieldsArr.size()));
          for (const auto& fieldValue : fieldsArr) {
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
              static_cast<size_t>(modifiersArr.size()));
          for (const auto& modifierValue : modifiersArr) {
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
  if (obj.contains(QStringLiteral("componentGraph")) &&
      obj.value(QStringLiteral("componentGraph")).isArray()) {
    impl_->componentHost_.fromJson(
        obj.value(QStringLiteral("componentGraph")).toArray());
  }
  impl_->syncBuiltinComponentDescriptors();

  if (obj.contains("variants") && obj["variants"].isArray()) {
      impl_->variants_.clear();
      QJsonArray arr = obj["variants"].toArray();
      for (int i = 0; i < arr.size(); ++i) {
          QJsonObject varObj = arr[i].toObject();
          auto newVariant = std::make_unique<LayerVariant>(this, varObj["name"].toString("A").toStdString());
          newVariant->overrideFlags_ = static_cast<VariantOverrideFlags>(varObj["flags"].toInt(0));
          
          if (varObj.contains("opacity")) {
              newVariant->opacityOverride = static_cast<float>(varObj["opacity"].toDouble(1.0));
          }
          if (varObj.contains("blendMode")) {
              newVariant->blendModeOverride = static_cast<LAYER_BLEND_TYPE>(varObj["blendMode"].toInt());
          }
          if (varObj.contains("transform") && varObj["transform"].isObject()) {
              QJsonObject vtrans = varObj["transform"].toObject();
              AnimatableTransform3D vt3;
              RationalTime t0(0, 1);
              if (vtrans.contains("px")) vt3.setPosition(t0, vtrans["px"].toDouble(), vtrans["py"].toDouble(0));
              if (vtrans.contains("pz")) vt3.setPositionZ(t0, vtrans["pz"].toDouble());
              if (vtrans.contains("rx")) vt3.setRotation(t0, vtrans["rx"].toDouble());
              if (vtrans.contains("sx")) vt3.setScale(t0, vtrans["sx"].toDouble(), vtrans["sy"].toDouble(1.0));
              if (vtrans.contains("ax")) vt3.setAnchor(t0, vtrans["ax"].toDouble(), vtrans["ay"].toDouble(), vtrans["az"].toDouble());
              newVariant->transform3DOverride = vt3;
          }
          
          impl_->variants_.push_back(std::move(newVariant));
      }
  } else if (impl_->variants_.empty()) {
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
    impl_->clearMasks();
    const auto masksArr = obj["masks"].toArray();
    for (const auto &maskVal : masksArr) {
      if (!maskVal.isObject()) continue;
      const auto mobj = maskVal.toObject();

      LayerMask layerMask;
      if (mobj.contains("enabled")) {
        layerMask.setEnabled(mobj["enabled"].toBool(true));
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

      impl_->addMask(layerMask);
    }
    changed();
  }

  applyPropertiesFromJson(obj);
}

void ArtifactAbstractLayer::Impl::addEffect(
    std::shared_ptr<ArtifactAbstractEffect> effect) {
  if (!effect)
    return;
  const QString currentId = effect->effectID().toQString().trimmed();
  const QString uniqueId = uniqueEffectIdForLayer(
      effects_, effect->displayName().toQString(), currentId);
  if (currentId.isEmpty() || currentId != uniqueId) {
    effect->setEffectID(UniString::fromQString(uniqueId));
  }
  effects_.push_back(effect);
  qDebug() << "[ArtifactAbstractLayer] Effect added:"
           << effect->displayName().toQString() << "id="
           << effect->effectID().toQString();
}

void ArtifactAbstractLayer::Impl::removeEffect(const UniString &effectID) {
  auto it = std::remove_if(
      effects_.begin(), effects_.end(),
      [&effectID](const std::shared_ptr<ArtifactAbstractEffect> &e) {
        return e && e->effectID() == effectID;
      });
  if (it != effects_.end()) {
    effects_.erase(it, effects_.end());
    qDebug() << "[ArtifactAbstractLayer] Effect removed:"
             << effectID.toQString();
  }
}

void ArtifactAbstractLayer::Impl::clearEffects() {
  effects_.clear();
  qDebug() << "[ArtifactAbstractLayer] All effects cleared";
}

std::vector<std::shared_ptr<ArtifactAbstractEffect>>
ArtifactAbstractLayer::Impl::getEffects() const {
  return effects_;
}

std::shared_ptr<ArtifactAbstractEffect>
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
    std::shared_ptr<ArtifactLayerModifier> modifier) {
  if (!modifier) {
    return;
  }

  const QString currentId = modifier->modifierId().trimmed();
  const QString uniqueId = uniqueModifierIdForLayer(
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

std::vector<std::shared_ptr<ArtifactLayerModifier>>
ArtifactAbstractLayer::Impl::getModifiers() const {
  return modifiers_.modifiers();
}

std::shared_ptr<ArtifactLayerModifier>
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
    std::shared_ptr<ArtifactAbstractEffect> effect) {
  impl_->addEffect(effect);
}

void ArtifactAbstractLayer::removeEffect(const UniString &effectID) {
  impl_->removeEffect(effectID);
}

void ArtifactAbstractLayer::clearEffects() { impl_->clearEffects(); }

std::vector<std::shared_ptr<ArtifactAbstractEffect>>
ArtifactAbstractLayer::getEffects() const {
  return impl_->getEffects();
}

std::shared_ptr<ArtifactAbstractEffect>
ArtifactAbstractLayer::getEffect(const UniString &effectID) const {
  return impl_->getEffect(effectID);
}

int ArtifactAbstractLayer::effectCount() const { return impl_->effectCount(); }

void ArtifactAbstractLayer::addModifier(
    std::shared_ptr<ArtifactLayerModifier> modifier) {
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

std::vector<std::shared_ptr<ArtifactLayerModifier>>
ArtifactAbstractLayer::getModifiers() const {
  return impl_->getModifiers();
}

std::shared_ptr<ArtifactLayerModifier>
ArtifactAbstractLayer::getModifier(const QString& modifierId) const {
  return impl_->getModifier(modifierId);
}

int ArtifactAbstractLayer::modifierCount() const { return impl_->modifierCount(); }

bool ArtifactAbstractLayer::hasModifiers() const { return impl_->hasModifiers(); }

std::vector<LayerComponentDescriptor>
ArtifactAbstractLayer::layerComponents() const {
  impl_->syncBuiltinComponentDescriptors();
  return impl_->componentHost_.components();
}

std::vector<LayerComponentDescriptor>
ArtifactAbstractLayer::enabledLayerComponents(
    const LayerComponentPhase phase) const {
  impl_->syncBuiltinComponentDescriptors();
  return impl_->componentHost_.enabledForPhase(phase);
}

std::vector<LayerGeneratorDescriptor>
ArtifactAbstractLayer::layerGenerators() const {
  impl_->syncBuiltinComponentDescriptors();

  std::vector<LayerGeneratorDescriptor> generators;
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

std::vector<LayerFieldDescriptor>
ArtifactAbstractLayer::layerFields() const {
  std::vector<LayerFieldDescriptor> fields;
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

std::vector<LayerModifierDescriptor>
ArtifactAbstractLayer::layerCloneModifiers() const {
  std::vector<LayerModifierDescriptor> modifiers;

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

std::vector<LayerComponentValidationIssue>
ArtifactAbstractLayer::validateLayerComponents() const {
  impl_->syncBuiltinComponentDescriptors();
  return impl_->componentHost_.validate();
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

std::optional<LayerEvaluationState>
ArtifactAbstractLayer::authoritativeComponentEvaluationState() const {
  if (!impl_->authoritativeComponentState_ ||
      impl_->authoritativeComponentFrame_ != impl_->currentFrame_) {
    return std::nullopt;
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
  auto snapshot = std::make_shared<LayerComponentRuntimeSnapshotData>();
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
  const auto data = std::static_pointer_cast<
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

QJsonObject ArtifactAbstractLayer::serializeComponentRuntimeSnapshot(
    const LayerComponentRuntimeSnapshot& snapshot) const {
  if (!snapshot.isValid()) {
    return {};
  }
  const auto data = std::static_pointer_cast<
      const LayerComponentRuntimeSnapshotData>(snapshot.storage);
  if (!data) {
    return {};
  }

  QJsonObject fracture;
  fracture.insert(QStringLiteral("kind"),
                  static_cast<int>(data->fractureState.kind));
  fracture.insert(QStringLiteral("damage"), data->fractureState.damage);
  fracture.insert(QStringLiteral("lastImpact"), data->fractureState.lastImpact);
  fracture.insert(QStringLiteral("crackProgress"),
                  data->fractureState.crackProgress);
  QJsonArray shards;
  for (const auto& shard : data->fractureState.shards) {
    QJsonObject object;
    object.insert(QStringLiteral("position"),
                  componentSnapshotVectorToJson(shard.position));
    object.insert(QStringLiteral("velocity"),
                  componentSnapshotVectorToJson(shard.velocity));
    object.insert(QStringLiteral("angularVelocity"),
                  componentSnapshotVectorToJson(shard.angularVelocity));
    object.insert(QStringLiteral("rotation"), shard.rotation);
    object.insert(QStringLiteral("scale"), shard.scale);
    object.insert(QStringLiteral("opacity"), shard.opacity);
    object.insert(QStringLiteral("age"), shard.age);
    object.insert(QStringLiteral("lifetime"), shard.lifetime);
    object.insert(QStringLiteral("active"), shard.active);
    object.insert(QStringLiteral("debris"), shard.debris);
    shards.append(object);
  }
  fracture.insert(QStringLiteral("shards"), shards);

  QJsonArray particles;
  for (const auto& particle : data->componentParticles) {
    QJsonObject object;
    object.insert(QStringLiteral("px"), particle.px);
    object.insert(QStringLiteral("py"), particle.py);
    object.insert(QStringLiteral("pz"), particle.pz);
    object.insert(QStringLiteral("vx"), particle.vx);
    object.insert(QStringLiteral("vy"), particle.vy);
    object.insert(QStringLiteral("vz"), particle.vz);
    object.insert(QStringLiteral("r"), particle.r);
    object.insert(QStringLiteral("g"), particle.g);
    object.insert(QStringLiteral("b"), particle.b);
    object.insert(QStringLiteral("a"), particle.a);
    object.insert(QStringLiteral("size"), particle.size);
    object.insert(QStringLiteral("stretch"), particle.stretch);
    object.insert(QStringLiteral("rotation"), particle.rotation);
    object.insert(QStringLiteral("age"), particle.age);
    object.insert(QStringLiteral("lifetime"), particle.lifetime);
    object.insert(QStringLiteral("spriteFrame"), particle.spriteFrame);
    object.insert(QStringLiteral("spriteRows"), particle.spriteRows);
    object.insert(QStringLiteral("spriteCols"), particle.spriteCols);
    particles.append(object);
  }

  QJsonObject object;
  object.insert(QStringLiteral("version"), 1);
  object.insert(QStringLiteral("fracture"), fracture);
  object.insert(QStringLiteral("particles"), particles);
  object.insert(QStringLiteral("fractureMotionLastFrame"),
                componentSnapshotFrameToJson(data->fractureMotionLastFrame));
  object.insert(QStringLiteral("componentParticlesLastFrame"),
                componentSnapshotFrameToJson(
                    data->componentParticlesLastFrame));
  object.insert(QStringLiteral("lastCollisionImpactFrame"),
                componentSnapshotFrameToJson(
                    data->lastCollisionImpactFrame));
  return object;
}

LayerComponentRuntimeSnapshot
ArtifactAbstractLayer::deserializeComponentRuntimeSnapshot(
    const QJsonObject& object) const {
  if (object.value(QStringLiteral("version")).toInt() != 1) {
    return {};
  }

  auto data = std::make_shared<LayerComponentRuntimeSnapshotData>();
  const QJsonObject fracture =
      object.value(QStringLiteral("fracture")).toObject();
  const int kind = fracture.value(QStringLiteral("kind")).toInt();
  if (kind < static_cast<int>(FractureStateKind::Intact) ||
      kind > static_cast<int>(FractureStateKind::Shattered)) {
    return {};
  }
  data->fractureState.kind = static_cast<FractureStateKind>(kind);
  data->fractureState.damage = static_cast<float>(
      fracture.value(QStringLiteral("damage")).toDouble());
  data->fractureState.lastImpact = static_cast<float>(
      fracture.value(QStringLiteral("lastImpact")).toDouble());
  data->fractureState.crackProgress = static_cast<float>(
      fracture.value(QStringLiteral("crackProgress")).toDouble());
  const QJsonArray shards = fracture.value(QStringLiteral("shards")).toArray();
  constexpr qsizetype kMaxPersistedShards = 65536;
  if (shards.size() > kMaxPersistedShards) {
    return {};
  }
  data->fractureState.shards.reserve(
      static_cast<std::size_t>(shards.size()));
  for (const auto& value : shards) {
    if (!value.isObject()) {
      return {};
    }
    const QJsonObject object = value.toObject();
    FractureShardMotion shard;
    shard.position = componentSnapshotVectorFromJson(
        object.value(QStringLiteral("position")));
    shard.velocity = componentSnapshotVectorFromJson(
        object.value(QStringLiteral("velocity")));
    shard.angularVelocity = componentSnapshotVectorFromJson(
        object.value(QStringLiteral("angularVelocity")));
    shard.rotation = static_cast<float>(
        object.value(QStringLiteral("rotation")).toDouble());
    shard.scale = static_cast<float>(
        object.value(QStringLiteral("scale")).toDouble(1.0));
    shard.opacity = static_cast<float>(
        object.value(QStringLiteral("opacity")).toDouble(1.0));
    shard.age = static_cast<float>(
        object.value(QStringLiteral("age")).toDouble());
    shard.lifetime = static_cast<float>(
        object.value(QStringLiteral("lifetime")).toDouble(1.0));
    shard.active = object.value(QStringLiteral("active")).toBool(true);
    shard.debris = object.value(QStringLiteral("debris")).toBool(false);
    data->fractureState.shards.push_back(shard);
  }

  const QJsonArray particles =
      object.value(QStringLiteral("particles")).toArray();
  constexpr qsizetype kMaxPersistedParticles = 1000000;
  if (particles.size() > kMaxPersistedParticles) {
    return {};
  }
  data->componentParticles.reserve(
      static_cast<std::size_t>(particles.size()));
  for (const auto& value : particles) {
    if (!value.isObject()) {
      return {};
    }
    const QJsonObject object = value.toObject();
    ArtifactCore::ParticleVertex particle{};
    particle.px = static_cast<float>(object.value(QStringLiteral("px")).toDouble());
    particle.py = static_cast<float>(object.value(QStringLiteral("py")).toDouble());
    particle.pz = static_cast<float>(object.value(QStringLiteral("pz")).toDouble());
    particle.vx = static_cast<float>(object.value(QStringLiteral("vx")).toDouble());
    particle.vy = static_cast<float>(object.value(QStringLiteral("vy")).toDouble());
    particle.vz = static_cast<float>(object.value(QStringLiteral("vz")).toDouble());
    particle.r = static_cast<float>(object.value(QStringLiteral("r")).toDouble());
    particle.g = static_cast<float>(object.value(QStringLiteral("g")).toDouble());
    particle.b = static_cast<float>(object.value(QStringLiteral("b")).toDouble());
    particle.a = static_cast<float>(object.value(QStringLiteral("a")).toDouble());
    particle.size = static_cast<float>(object.value(QStringLiteral("size")).toDouble(1.0));
    particle.stretch = static_cast<float>(object.value(QStringLiteral("stretch")).toDouble(1.0));
    particle.rotation = static_cast<float>(object.value(QStringLiteral("rotation")).toDouble());
    particle.age = static_cast<float>(object.value(QStringLiteral("age")).toDouble());
    particle.lifetime = static_cast<float>(object.value(QStringLiteral("lifetime")).toDouble(1.0));
    particle.spriteFrame = object.value(QStringLiteral("spriteFrame")).toInt();
    particle.spriteRows = object.value(QStringLiteral("spriteRows")).toInt(1);
    particle.spriteCols = object.value(QStringLiteral("spriteCols")).toInt(1);
    data->componentParticles.push_back(particle);
  }

  data->fractureMotionLastFrame = componentSnapshotFrameFromJson(
      object, QStringLiteral("fractureMotionLastFrame"));
  data->componentParticlesLastFrame = componentSnapshotFrameFromJson(
      object, QStringLiteral("componentParticlesLastFrame"));
  data->lastCollisionImpactFrame = componentSnapshotFrameFromJson(
      object, QStringLiteral("lastCollisionImpactFrame"));
  const std::size_t estimatedBytes =
      sizeof(LayerComponentRuntimeSnapshotData) +
      data->componentParticles.size() * sizeof(ArtifactCore::ParticleVertex) +
      data->fractureState.shards.size() * sizeof(FractureShardMotion);
  return {std::move(data), estimatedBytes};
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
  rotationProp->setDisplayLabel(QStringLiteral("Rotation"));
  rotationProp->setUnit(QStringLiteral("deg"));
  rotationProp->setStep(1.0);
  rotationProp->setSoftRange(-180.0, 180.0);
  rotationProp->setAnimatable(true);
  transformGroup.addProperty(rotationProp);

  auto autoOrientProp =
      makeProp(QStringLiteral("transform.autoOrient"), PropertyType::Integer,
               static_cast<int>(t3.autoOrientMode()), -295);
  autoOrientProp->setDisplayLabel(QStringLiteral("Auto-Orient"));
  autoOrientProp->setTooltip(QStringLiteral(
      "Off: keep the current rotation.\n"
      "Along Path: rotate the layer to follow the motion path tangent.\n"
      "Along Path at Frame Start: use the tangent at the start of the current segment."));
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
  layerGroup.addProperty(inPointProp);

  auto outPointProp =
      makeProp(QStringLiteral("time.outPoint"), PropertyType::Integer,
               static_cast<qint64>(outPoint().framePosition()), -80);
  outPointProp->setUnit(QStringLiteral("frames"));
  outPointProp->setTooltip(QStringLiteral("Layer out-point on timeline"));
  layerGroup.addProperty(outPointProp);

  auto startTimeProp =
      makeProp(QStringLiteral("time.startTime"), PropertyType::Integer,
               static_cast<qint64>(startTime().framePosition()), -70);
  startTimeProp->setUnit(QStringLiteral("frames"));
  startTimeProp->setTooltip(
      QStringLiteral("Layer start offset in source time"));
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

  auto gravityYProp =
      makeProp(QStringLiteral("physics.gravityY"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().gravityY), -96);
  gravityYProp->setUnit(QStringLiteral("px/s^2"));
  gravityYProp->setHardRange(-5000.0, 5000.0);
  gravityYProp->setSoftRange(-2000.0, 2000.0);
  gravityYProp->setStep(10.0);
  physicsGroup.addProperty(gravityYProp);

  auto linearDampingProp =
      makeProp(QStringLiteral("physics.linearDamping"), PropertyType::Float,
               static_cast<double>(impl_->physicsComponent_.settings().linearDamping), -95);
  linearDampingProp->setHardRange(0.0, 50.0);
  linearDampingProp->setSoftRange(0.0, 10.0);
  linearDampingProp->setStep(0.1);
  physicsGroup.addProperty(linearDampingProp);

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
  motionGroup.addProperty(motionLagTauProp);

  auto motionClampOvershootProp =
      makeProp(QStringLiteral("motion.clampOvershoot"), PropertyType::Boolean,
               impl_->motionDynamicsClampOvershoot_, -86);
  motionClampOvershootProp->setDisplayLabel(QStringLiteral("Clamp Overshoot"));
  motionClampOvershootProp->setTooltip(
      QStringLiteral("Keep the solver from overshooting too far."));
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

  PropertyGroup componentGroup(QStringLiteral("Components"));
  auto scriptComponentEnabledProp =
      makeProp(QStringLiteral("component.script.enabled"),
               PropertyType::Boolean, impl_->scriptComponentEnabled_, -100);
  scriptComponentEnabledProp->setDisplayLabel(QStringLiteral("Script Component Enabled"));
  componentGroup.addProperty(scriptComponentEnabledProp);

  auto clonerComponentEnabledProp =
      makeProp(QStringLiteral("component.cloner.enabled"),
               PropertyType::Boolean, impl_->clonerComponentEnabled_, -90);
  clonerComponentEnabledProp->setDisplayLabel(QStringLiteral("Cloner Enabled"));
  componentGroup.addProperty(clonerComponentEnabledProp);
  auto collisionComponentEnabledProp =
      makeProp(QStringLiteral("component.collision.enabled"),
               PropertyType::Boolean, impl_->collisionComponentEnabled_, -89);
  collisionComponentEnabledProp->setDisplayLabel(
      QStringLiteral("Collision Enabled"));
  componentGroup.addProperty(collisionComponentEnabledProp);
  PropertyGroup collisionGroup(QStringLiteral("Collision"));
  auto collisionShapeProp =
      makeProp(QStringLiteral("component.collision.shape"),
               PropertyType::Integer, impl_->collisionShape_, -88);
  collisionShapeProp->setDisplayLabel(QStringLiteral("Shape"));
  collisionShapeProp->setTooltip(
      QStringLiteral("0=Auto Bounds, 1=Box, 2=Circle."));
  collisionShapeProp->setHardRange(0.0, 2.0);
  collisionShapeProp->setSoftRange(0.0, 2.0);
  collisionGroup.addProperty(collisionShapeProp);
  auto collisionWidthProp =
      makeProp(QStringLiteral("component.collision.width"),
               PropertyType::Float,
               static_cast<double>(impl_->collisionWidth_), -87);
  collisionWidthProp->setDisplayLabel(QStringLiteral("Width"));
  collisionWidthProp->setHardRange(0.0, 100000.0);
  collisionWidthProp->setSoftRange(0.0, 4096.0);
  collisionGroup.addProperty(collisionWidthProp);
  auto collisionHeightProp =
      makeProp(QStringLiteral("component.collision.height"),
               PropertyType::Float,
               static_cast<double>(impl_->collisionHeight_), -86);
  collisionHeightProp->setDisplayLabel(QStringLiteral("Height"));
  collisionHeightProp->setHardRange(0.0, 100000.0);
  collisionHeightProp->setSoftRange(0.0, 4096.0);
  collisionGroup.addProperty(collisionHeightProp);
  auto collisionRadiusProp =
      makeProp(QStringLiteral("component.collision.radius"),
               PropertyType::Float,
               static_cast<double>(impl_->collisionRadius_), -85);
  collisionRadiusProp->setDisplayLabel(QStringLiteral("Radius"));
  collisionRadiusProp->setHardRange(0.0, 100000.0);
  collisionRadiusProp->setSoftRange(0.0, 2048.0);
  collisionGroup.addProperty(collisionRadiusProp);
  auto collisionOffsetXProp =
      makeProp(QStringLiteral("component.collision.offsetX"),
               PropertyType::Float,
               static_cast<double>(impl_->collisionOffsetX_), -84);
  collisionOffsetXProp->setDisplayLabel(QStringLiteral("Offset X"));
  collisionOffsetXProp->setHardRange(-100000.0, 100000.0);
  collisionOffsetXProp->setSoftRange(-4096.0, 4096.0);
  collisionGroup.addProperty(collisionOffsetXProp);
  auto collisionOffsetYProp =
      makeProp(QStringLiteral("component.collision.offsetY"),
               PropertyType::Float,
               static_cast<double>(impl_->collisionOffsetY_), -83);
  collisionOffsetYProp->setDisplayLabel(QStringLiteral("Offset Y"));
  collisionOffsetYProp->setHardRange(-100000.0, 100000.0);
  collisionOffsetYProp->setSoftRange(-4096.0, 4096.0);
  collisionGroup.addProperty(collisionOffsetYProp);
  auto collisionFloorYProp =
      makeProp(QStringLiteral("component.collision.floorY"),
               PropertyType::Float,
               static_cast<double>(impl_->collisionFloorY_), -82);
  collisionFloorYProp->setDisplayLabel(QStringLiteral("Floor Y"));
  collisionFloorYProp->setTooltip(
      QStringLiteral("0 uses the composition bottom; positive values set an explicit floor."));
  collisionFloorYProp->setHardRange(0.0, 100000.0);
  collisionFloorYProp->setSoftRange(0.0, 4096.0);
  collisionGroup.addProperty(collisionFloorYProp);
  auto collisionCompositionBoundsProp =
      makeProp(QStringLiteral("component.collision.compositionBounds"),
               PropertyType::Boolean, impl_->collisionCompositionBounds_, -81);
  collisionCompositionBoundsProp->setDisplayLabel(
      QStringLiteral("Composition Bounds"));
  collisionCompositionBoundsProp->setTooltip(
      QStringLiteral("Bounce fracture fragments from the composition edges."));
  collisionGroup.addProperty(collisionCompositionBoundsProp);
  auto crowdComponentEnabledProp =
      makeProp(QStringLiteral("component.crowd.enabled"),
               PropertyType::Boolean, impl_->crowdComponentEnabled_, -88);
  crowdComponentEnabledProp->setDisplayLabel(QStringLiteral("Crowd Enabled"));
  componentGroup.addProperty(crowdComponentEnabledProp);
  auto particleEmitterComponentEnabledProp =
      makeProp(QStringLiteral("component.particleEmitter.enabled"),
               PropertyType::Boolean,
               impl_->particleEmitterComponentEnabled_, -87);
  particleEmitterComponentEnabledProp->setDisplayLabel(
      QStringLiteral("Particle Emitter Enabled"));
  componentGroup.addProperty(particleEmitterComponentEnabledProp);
  auto fluidComponentEnabledProp =
      makeProp(QStringLiteral("component.fluid.enabled"),
               PropertyType::Boolean, impl_->fluidComponentEnabled_, -86);
  fluidComponentEnabledProp->setDisplayLabel(QStringLiteral("Fluid Enabled"));
  componentGroup.addProperty(fluidComponentEnabledProp);
  PropertyGroup layoutGroup(QStringLiteral("Layout"));
  PropertyGroup clonerGroup(QStringLiteral("Cloner"));
  PropertyGroup crowdGroup(QStringLiteral("Crowd"));
  PropertyGroup particleEmitterGroup(QStringLiteral("Particle Emitter"));
  PropertyGroup fluidGroup(QStringLiteral("Fluid"));

  auto addCrowdFloat = [&](const QString& name, const QString& label,
                           float value, double hardMax, int order) {
    auto prop = makeProp(name, PropertyType::Float,
                         static_cast<double>(value), order);
    prop->setDisplayLabel(label);
    prop->setHardRange(0.0, hardMax);
    prop->setSoftRange(0.0, std::min(10.0, hardMax));
    crowdGroup.addProperty(prop);
  };
  addCrowdFloat(QStringLiteral("component.crowd.cohesion"),
                QStringLiteral("Cohesion"), impl_->crowdCohesion_, 10.0, -86);
  addCrowdFloat(QStringLiteral("component.crowd.separation"),
                QStringLiteral("Separation"), impl_->crowdSeparation_, 10.0,
                -85);
  addCrowdFloat(QStringLiteral("component.crowd.alignment"),
                QStringLiteral("Alignment"), impl_->crowdAlignment_, 10.0,
                -84);
  addCrowdFloat(QStringLiteral("component.crowd.maxSpeed"),
                QStringLiteral("Max Speed"), impl_->crowdMaxSpeed_, 10000.0,
                -83);
  addCrowdFloat(QStringLiteral("component.crowd.jitter"),
                QStringLiteral("Jitter"), impl_->crowdJitter_, 10.0, -82);

  auto particleCountProp =
      makeProp(QStringLiteral("component.particleEmitter.count"),
               PropertyType::Integer, impl_->particleEmitterCount_, -81);
  particleCountProp->setDisplayLabel(QStringLiteral("Burst Count"));
  particleCountProp->setHardRange(0.0, 100000.0);
  particleCountProp->setSoftRange(0.0, 1024.0);
  particleEmitterGroup.addProperty(particleCountProp);
  auto particleSpeedProp =
      makeProp(QStringLiteral("component.particleEmitter.speed"),
               PropertyType::Float,
               static_cast<double>(impl_->particleEmitterSpeed_), -80);
  particleSpeedProp->setDisplayLabel(QStringLiteral("Initial Speed"));
  particleSpeedProp->setHardRange(0.0, 100000.0);
  particleSpeedProp->setSoftRange(0.0, 2000.0);
  particleEmitterGroup.addProperty(particleSpeedProp);
  auto particleLifetimeProp =
      makeProp(QStringLiteral("component.particleEmitter.lifetime"),
               PropertyType::Float,
               static_cast<double>(impl_->particleEmitterLifetime_), -79);
  particleLifetimeProp->setDisplayLabel(QStringLiteral("Lifetime"));
  particleLifetimeProp->setUnit(QStringLiteral("s"));
  particleLifetimeProp->setHardRange(0.01, 3600.0);
  particleLifetimeProp->setSoftRange(0.1, 30.0);
  particleEmitterGroup.addProperty(particleLifetimeProp);
  auto fluidGridWidthProp =
      makeProp(QStringLiteral("component.fluid.gridWidth"),
               PropertyType::Integer, impl_->fluidGridWidth_, -78);
  fluidGridWidthProp->setDisplayLabel(QStringLiteral("Grid Width"));
  fluidGridWidthProp->setHardRange(8.0, 4096.0);
  fluidGridWidthProp->setSoftRange(16.0, 512.0);
  fluidGroup.addProperty(fluidGridWidthProp);
  auto fluidGridHeightProp =
      makeProp(QStringLiteral("component.fluid.gridHeight"),
               PropertyType::Integer, impl_->fluidGridHeight_, -77);
  fluidGridHeightProp->setDisplayLabel(QStringLiteral("Grid Height"));
  fluidGridHeightProp->setHardRange(8.0, 4096.0);
  fluidGridHeightProp->setSoftRange(16.0, 512.0);
  fluidGroup.addProperty(fluidGridHeightProp);
  auto fluidViscosityProp =
      makeProp(QStringLiteral("component.fluid.viscosity"),
               PropertyType::Float,
               static_cast<double>(impl_->fluidViscosity_), -76);
  fluidViscosityProp->setDisplayLabel(QStringLiteral("Viscosity"));
  fluidViscosityProp->setSoftRange(0.0, 1.0);
  fluidGroup.addProperty(fluidViscosityProp);
  auto fluidDiffusionProp =
      makeProp(QStringLiteral("component.fluid.diffusion"),
               PropertyType::Float,
               static_cast<double>(impl_->fluidDiffusion_), -75);
  fluidDiffusionProp->setDisplayLabel(QStringLiteral("Diffusion"));
  fluidDiffusionProp->setSoftRange(0.0, 1.0);
  fluidGroup.addProperty(fluidDiffusionProp);
  auto fluidBuoyancyProp =
      makeProp(QStringLiteral("component.fluid.buoyancy"),
               PropertyType::Float,
               static_cast<double>(impl_->fluidBuoyancy_), -74);
  fluidBuoyancyProp->setDisplayLabel(QStringLiteral("Buoyancy"));
  fluidBuoyancyProp->setSoftRange(-2.0, 2.0);
  fluidGroup.addProperty(fluidBuoyancyProp);
  auto fluidVorticityProp =
      makeProp(QStringLiteral("component.fluid.vorticity"),
               PropertyType::Float,
               static_cast<double>(impl_->fluidVorticity_), -73);
  fluidVorticityProp->setDisplayLabel(QStringLiteral("Vorticity"));
  fluidVorticityProp->setSoftRange(0.0, 10.0);
  fluidGroup.addProperty(fluidVorticityProp);
  auto fluidSolverIterationsProp =
      makeProp(QStringLiteral("component.fluid.solverIterations"),
               PropertyType::Integer, impl_->fluidSolverIterations_, -72);
  fluidSolverIterationsProp->setDisplayLabel(
      QStringLiteral("Solver Iterations"));
  fluidSolverIterationsProp->setHardRange(1.0, 256.0);
  fluidSolverIterationsProp->setSoftRange(1.0, 64.0);
  fluidGroup.addProperty(fluidSolverIterationsProp);
  auto layoutComponentEnabledProp =
      makeProp(QStringLiteral("component.layout.enabled"),
               PropertyType::Boolean, impl_->layoutComponentEnabled_, -89);
  layoutComponentEnabledProp->setDisplayLabel(QStringLiteral("Layout Enabled"));
  layoutGroup.addProperty(layoutComponentEnabledProp);
  auto layoutModeProp = makeProp(QStringLiteral("component.layout.mode"),
                                 PropertyType::Integer,
                                 impl_->layoutMode_, -88);
  layoutModeProp->setDisplayLabel(QStringLiteral("Layout Mode"));
  layoutModeProp->setTooltip(
      QStringLiteral("0=Flow, 1=Offset, 2=Absolute."));
  layoutModeProp->setHardRange(0.0, 2.0);
  layoutGroup.addProperty(layoutModeProp);
  auto layoutAlignmentProp = makeProp(
      QStringLiteral("component.layout.anchorMode"), PropertyType::Integer,
      impl_->layoutAnchorMode_, -87);
  layoutAlignmentProp->setDisplayLabel(QStringLiteral("Cross Alignment"));
  layoutAlignmentProp->setTooltip(
      QStringLiteral("0=Start, 1=Center, 2=End."));
  layoutAlignmentProp->setHardRange(0.0, 2.0);
  layoutGroup.addProperty(layoutAlignmentProp);
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.horizontalPin"),
                PropertyType::Integer, impl_->layoutHorizontalPin_, -86));
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.verticalPin"),
                PropertyType::Integer, impl_->layoutVerticalPin_, -85));
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.scaleMode"),
                PropertyType::Integer, impl_->layoutScaleMode_, -84));
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.safeAreaEnabled"),
                PropertyType::Boolean, impl_->layoutSafeAreaEnabled_, -83));
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.safeAreaPaddingX"),
                PropertyType::Float,
                static_cast<double>(impl_->layoutSafeAreaPaddingX_), -82));
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.safeAreaPaddingY"),
                PropertyType::Float,
                static_cast<double>(impl_->layoutSafeAreaPaddingY_), -81));
  auto layoutDirectionProp = makeProp(
      QStringLiteral("component.layout.stackDirection"),
      PropertyType::Integer, impl_->layoutStackDirection_, -80);
  layoutDirectionProp->setDisplayLabel(QStringLiteral("Direction"));
  layoutDirectionProp->setTooltip(QStringLiteral("0=Horizontal, 1=Vertical."));
  layoutDirectionProp->setHardRange(0.0, 1.0);
  layoutGroup.addProperty(layoutDirectionProp);
  auto layoutGapProp = makeProp(QStringLiteral("component.layout.gap"),
                                PropertyType::Float,
                                static_cast<double>(impl_->layoutGap_), -79);
  layoutGapProp->setDisplayLabel(QStringLiteral("Gap"));
  layoutGapProp->setSoftRange(0.0, 256.0);
  layoutGroup.addProperty(layoutGapProp);
  layoutGroup.addProperty(
      makeProp(QStringLiteral("component.layout.maxPerRow"),
                PropertyType::Integer, impl_->layoutMaxPerRow_, -78));
  auto clonerModeProp =
      makeProp(QStringLiteral("component.cloner.mode"),
               PropertyType::Integer, impl_->clonerMode_, -85);
  clonerModeProp->setDisplayLabel(QStringLiteral("Mode"));
  clonerGroup.addProperty(clonerModeProp);
  auto clonerCloneCountProp =
      makeProp(QStringLiteral("component.cloner.cloneCount"),
                PropertyType::Integer, impl_->clonerCloneCount_, -80);
  clonerCloneCountProp->setDisplayLabel(QStringLiteral("Count"));
  clonerCloneCountProp->setHardRange(1.0, 256.0);
  clonerCloneCountProp->setSoftRange(1.0, 32.0);
  clonerGroup.addProperty(clonerCloneCountProp);
  auto clonerTimeOffsetStepProp =
      makeProp(QStringLiteral("component.cloner.timeOffsetStep"),
               PropertyType::Float,
               static_cast<double>(impl_->clonerTimeOffsetStep_), -79);
  clonerTimeOffsetStepProp->setDisplayLabel(QStringLiteral("Time Offset Step"));
  clonerTimeOffsetStepProp->setUnit(QStringLiteral("s"));
  clonerTimeOffsetStepProp->setSoftRange(-5.0, 5.0);
  clonerGroup.addProperty(clonerTimeOffsetStepProp);
  auto clonerSequenceEnabledProp =
      makeProp(QStringLiteral("component.cloner.sequenceEnabled"),
               PropertyType::Boolean, impl_->clonerSequenceEnabled_, -78);
  clonerSequenceEnabledProp->setDisplayLabel(QStringLiteral("Sequence Enabled"));
  clonerGroup.addProperty(clonerSequenceEnabledProp);
  auto clonerSequenceRateProp =
      makeProp(QStringLiteral("component.cloner.sequenceRate"),
               PropertyType::Float,
               static_cast<double>(impl_->clonerSequenceRate_), -77);
  clonerSequenceRateProp->setDisplayLabel(QStringLiteral("Sequence Rate"));
  clonerSequenceRateProp->setSoftRange(0.1, 60.0);
  clonerGroup.addProperty(clonerSequenceRateProp);
  auto clonerSequenceSoftnessProp =
      makeProp(QStringLiteral("component.cloner.sequenceSoftness"),
               PropertyType::Float,
               static_cast<double>(impl_->clonerSequenceSoftness_), -76);
  clonerSequenceSoftnessProp->setDisplayLabel(QStringLiteral("Sequence Softness"));
  clonerSequenceSoftnessProp->setSoftRange(0.1, 16.0);
  clonerGroup.addProperty(clonerSequenceSoftnessProp);
  auto clonerOffsetXProp =
      makeProp(QStringLiteral("component.cloner.offsetX"),
               PropertyType::Float,
               static_cast<double>(impl_->clonerOffsetX_), -70);
  clonerOffsetXProp->setDisplayLabel(QStringLiteral("Offset X"));
  clonerOffsetXProp->setSoftRange(-1000.0, 1000.0);
  clonerGroup.addProperty(clonerOffsetXProp);
  auto clonerOffsetYProp =
      makeProp(QStringLiteral("component.cloner.offsetY"),
               PropertyType::Float,
               static_cast<double>(impl_->clonerOffsetY_), -60);
  clonerOffsetYProp->setDisplayLabel(QStringLiteral("Offset Y"));
  clonerOffsetYProp->setSoftRange(-1000.0, 1000.0);
  clonerGroup.addProperty(clonerOffsetYProp);
  auto clonerOffsetZProp =
      makeProp(QStringLiteral("component.cloner.offsetZ"), PropertyType::Float,
               static_cast<double>(impl_->clonerOffsetZ_), -59);
  clonerOffsetZProp->setDisplayLabel(QStringLiteral("Offset Z"));
  clonerGroup.addProperty(clonerOffsetZProp);
  auto clonerJitterXProp =
      makeProp(QStringLiteral("component.cloner.jitterX"), PropertyType::Float,
               static_cast<double>(impl_->clonerJitterX_), -58);
  clonerJitterXProp->setDisplayLabel(QStringLiteral("Jitter X"));
  clonerGroup.addProperty(clonerJitterXProp);
  auto clonerJitterYProp =
      makeProp(QStringLiteral("component.cloner.jitterY"), PropertyType::Float,
               static_cast<double>(impl_->clonerJitterY_), -57);
  clonerJitterYProp->setDisplayLabel(QStringLiteral("Jitter Y"));
  clonerGroup.addProperty(clonerJitterYProp);
  auto clonerJitterZProp =
      makeProp(QStringLiteral("component.cloner.jitterZ"), PropertyType::Float,
               static_cast<double>(impl_->clonerJitterZ_), -56);
  clonerJitterZProp->setDisplayLabel(QStringLiteral("Jitter Z"));
  clonerGroup.addProperty(clonerJitterZProp);
  auto clonerSeedProp =
      makeProp(QStringLiteral("component.cloner.seed"), PropertyType::Integer,
               impl_->clonerSeed_, -55);
  clonerSeedProp->setDisplayLabel(QStringLiteral("Seed"));
  clonerGroup.addProperty(clonerSeedProp);
  auto clonerColumnsProp =
      makeProp(QStringLiteral("component.cloner.columns"), PropertyType::Integer,
               impl_->clonerColumns_, -54);
  clonerColumnsProp->setDisplayLabel(QStringLiteral("Columns"));
  clonerGroup.addProperty(clonerColumnsProp);
  auto clonerRowsProp =
      makeProp(QStringLiteral("component.cloner.rows"), PropertyType::Integer,
               impl_->clonerRows_, -53);
  clonerRowsProp->setDisplayLabel(QStringLiteral("Rows"));
  clonerGroup.addProperty(clonerRowsProp);
  auto clonerDepthProp =
      makeProp(QStringLiteral("component.cloner.depth"), PropertyType::Integer,
               impl_->clonerDepth_, -52);
  clonerDepthProp->setDisplayLabel(QStringLiteral("Depth"));
  clonerGroup.addProperty(clonerDepthProp);
  auto clonerSpacingXProp =
      makeProp(QStringLiteral("component.cloner.spacingX"), PropertyType::Float,
               static_cast<double>(impl_->clonerSpacingX_), -51);
  clonerSpacingXProp->setDisplayLabel(QStringLiteral("Spacing X"));
  clonerGroup.addProperty(clonerSpacingXProp);
  auto clonerSpacingYProp =
      makeProp(QStringLiteral("component.cloner.spacingY"), PropertyType::Float,
               static_cast<double>(impl_->clonerSpacingY_), -50);
  clonerSpacingYProp->setDisplayLabel(QStringLiteral("Spacing Y"));
  clonerGroup.addProperty(clonerSpacingYProp);
  auto clonerSpacingZProp =
      makeProp(QStringLiteral("component.cloner.spacingZ"), PropertyType::Float,
               static_cast<double>(impl_->clonerSpacingZ_), -49);
  clonerSpacingZProp->setDisplayLabel(QStringLiteral("Spacing Z"));
  clonerGroup.addProperty(clonerSpacingZProp);
  auto clonerRadialCountProp =
      makeProp(QStringLiteral("component.cloner.radialCount"), PropertyType::Integer,
               impl_->clonerRadialCount_, -48);
  clonerRadialCountProp->setDisplayLabel(QStringLiteral("Count"));
  clonerGroup.addProperty(clonerRadialCountProp);
  auto clonerRadiusProp =
      makeProp(QStringLiteral("component.cloner.radius"), PropertyType::Float,
               static_cast<double>(impl_->clonerRadius_), -47);
  clonerRadiusProp->setDisplayLabel(QStringLiteral("Radius"));
  clonerGroup.addProperty(clonerRadiusProp);
  auto clonerStartAngleProp =
      makeProp(QStringLiteral("component.cloner.startAngle"), PropertyType::Float,
               static_cast<double>(impl_->clonerStartAngle_), -46);
  clonerStartAngleProp->setDisplayLabel(QStringLiteral("Start Angle"));
  clonerGroup.addProperty(clonerStartAngleProp);
  auto clonerEndAngleProp =
      makeProp(QStringLiteral("component.cloner.endAngle"), PropertyType::Float,
               static_cast<double>(impl_->clonerEndAngle_), -45);
  clonerEndAngleProp->setDisplayLabel(QStringLiteral("End Angle"));
  clonerGroup.addProperty(clonerEndAngleProp);
  auto clonerRotationStepProp =
      makeProp(QStringLiteral("component.cloner.rotationStep"), PropertyType::Float,
               static_cast<double>(impl_->clonerRotationStep_), -44);
  clonerRotationStepProp->setDisplayLabel(QStringLiteral("Rotation Step"));
  clonerGroup.addProperty(clonerRotationStepProp);
  auto clonerOpacityDecayProp =
      makeProp(QStringLiteral("component.cloner.opacityDecay"), PropertyType::Float,
               static_cast<double>(impl_->clonerOpacityDecay_), -43);
  clonerOpacityDecayProp->setDisplayLabel(QStringLiteral("Opacity Decay"));
  clonerGroup.addProperty(clonerOpacityDecayProp);

  std::vector<PropertyGroup> generatorGroups;
  generatorGroups.reserve(impl_->extraGeneratorDescriptors_.count());
  for (std::size_t generatorIndex = 0;
       generatorIndex < impl_->extraGeneratorDescriptors_.count();
       ++generatorIndex) {
    const auto* descriptor = impl_->extraGeneratorDescriptors_.at(generatorIndex);
    if (!descriptor) {
      continue;
    }
    const int orderBase = -120 - static_cast<int>(generatorIndex) * 20;
    const QString generatorPrefix =
        QStringLiteral("component.generators.%1.")
            .arg(static_cast<int>(generatorIndex));
    PropertyGroup generatorGroup(
        QStringLiteral("Generator %1").arg(static_cast<int>(generatorIndex) + 1));

    auto enabledProp =
        makeProp(generatorPrefix + QStringLiteral("enabled"),
                 PropertyType::Boolean, descriptor->enabled, orderBase);
    enabledProp->setDisplayLabel(QStringLiteral("Enabled"));
    generatorGroup.addProperty(enabledProp);

    auto timeOffsetStepProp =
        makeProp(generatorPrefix + QStringLiteral("timeOffsetStep"),
                 PropertyType::Float,
                 descriptor->settings.value(QStringLiteral("timeOffsetStep"))
                     .toDouble(0.0),
                 orderBase + 1);
    timeOffsetStepProp->setDisplayLabel(QStringLiteral("Time Offset Step"));
    timeOffsetStepProp->setUnit(QStringLiteral("s"));
    timeOffsetStepProp->setSoftRange(-5.0, 5.0);
    generatorGroup.addProperty(timeOffsetStepProp);

    auto sequenceEnabledProp =
        makeProp(generatorPrefix + QStringLiteral("sequenceEnabled"),
                 PropertyType::Boolean,
                 descriptor->settings.value(QStringLiteral("sequenceEnabled"))
                     .toBool(),
                 orderBase + 2);
    sequenceEnabledProp->setDisplayLabel(QStringLiteral("Sequence Enabled"));
    generatorGroup.addProperty(sequenceEnabledProp);

    auto sequenceRateProp =
        makeProp(generatorPrefix + QStringLiteral("sequenceRate"),
                 PropertyType::Float,
                 descriptor->settings.value(QStringLiteral("sequenceRate"))
                     .toDouble(8.0),
                 orderBase + 3);
    sequenceRateProp->setDisplayLabel(QStringLiteral("Sequence Rate"));
    sequenceRateProp->setSoftRange(0.1, 60.0);
    generatorGroup.addProperty(sequenceRateProp);

    auto sequenceSoftnessProp =
        makeProp(generatorPrefix + QStringLiteral("sequenceSoftness"),
                 PropertyType::Float,
                 descriptor->settings.value(QStringLiteral("sequenceSoftness"))
                     .toDouble(1.0),
                 orderBase + 4);
    sequenceSoftnessProp->setDisplayLabel(QStringLiteral("Sequence Softness"));
    sequenceSoftnessProp->setSoftRange(0.1, 16.0);
    generatorGroup.addProperty(sequenceSoftnessProp);

    if (descriptor->typeId ==
        QStringLiteral("artifact.generator.cloner.radial")) {
      auto radialCountProp =
          makeProp(generatorPrefix + QStringLiteral("radialCount"),
                   PropertyType::Integer,
                       descriptor->settings.value(QStringLiteral("radialCount"))
                           .toInt(8),
                   orderBase + 5);
      radialCountProp->setDisplayLabel(QStringLiteral("Count"));
      radialCountProp->setHardRange(1.0, 2048.0);
      radialCountProp->setSoftRange(1.0, 64.0);
      generatorGroup.addProperty(radialCountProp);

      auto radiusProp =
          makeProp(generatorPrefix + QStringLiteral("radius"),
                   PropertyType::Float,
                       descriptor->settings.value(QStringLiteral("radius"))
                           .toDouble(160.0),
                   orderBase + 6);
      radiusProp->setDisplayLabel(QStringLiteral("Radius"));
      radiusProp->setSoftRange(0.0, 2000.0);
      generatorGroup.addProperty(radiusProp);

      auto startAngleProp =
          makeProp(generatorPrefix + QStringLiteral("startAngle"),
                   PropertyType::Float,
                       descriptor->settings.value(QStringLiteral("startAngle"))
                           .toDouble(0.0),
                   orderBase + 7);
      startAngleProp->setDisplayLabel(QStringLiteral("Start Angle"));
      startAngleProp->setSoftRange(-360.0, 360.0);
      generatorGroup.addProperty(startAngleProp);

      auto endAngleProp =
          makeProp(generatorPrefix + QStringLiteral("endAngle"),
                   PropertyType::Float,
                       descriptor->settings.value(QStringLiteral("endAngle"))
                           .toDouble(360.0),
                   orderBase + 8);
      endAngleProp->setDisplayLabel(QStringLiteral("End Angle"));
      endAngleProp->setSoftRange(-360.0, 720.0);
      generatorGroup.addProperty(endAngleProp);
    } else {
      auto columnsProp =
          makeProp(generatorPrefix + QStringLiteral("columns"),
                   PropertyType::Integer,
                       descriptor->settings.value(QStringLiteral("columns"))
                           .toInt(3),
                   orderBase + 5);
      columnsProp->setDisplayLabel(QStringLiteral("Columns"));
      columnsProp->setHardRange(1.0, 256.0);
      columnsProp->setSoftRange(1.0, 16.0);
      generatorGroup.addProperty(columnsProp);

      auto rowsProp =
          makeProp(generatorPrefix + QStringLiteral("rows"),
                   PropertyType::Integer,
                       descriptor->settings.value(QStringLiteral("rows"))
                           .toInt(3),
                   orderBase + 6);
      rowsProp->setDisplayLabel(QStringLiteral("Rows"));
      rowsProp->setHardRange(1.0, 256.0);
      rowsProp->setSoftRange(1.0, 16.0);
      generatorGroup.addProperty(rowsProp);

      auto depthProp =
          makeProp(generatorPrefix + QStringLiteral("depth"),
                   PropertyType::Integer,
                       descriptor->settings.value(QStringLiteral("depth"))
                           .toInt(1),
                   orderBase + 7);
      depthProp->setDisplayLabel(QStringLiteral("Depth"));
      depthProp->setHardRange(1.0, 256.0);
      depthProp->setSoftRange(1.0, 8.0);
      generatorGroup.addProperty(depthProp);

      auto spacingXProp =
          makeProp(generatorPrefix + QStringLiteral("spacingX"),
                   PropertyType::Float,
                       descriptor->settings.value(QStringLiteral("spacingX"))
                           .toDouble(160.0),
                   orderBase + 8);
      spacingXProp->setDisplayLabel(QStringLiteral("Spacing X"));
      spacingXProp->setSoftRange(-2000.0, 2000.0);
      generatorGroup.addProperty(spacingXProp);

      auto spacingYProp =
          makeProp(generatorPrefix + QStringLiteral("spacingY"),
                   PropertyType::Float,
                       descriptor->settings.value(QStringLiteral("spacingY"))
                           .toDouble(48.0),
                   orderBase + 9);
      spacingYProp->setDisplayLabel(QStringLiteral("Spacing Y"));
      spacingYProp->setSoftRange(-2000.0, 2000.0);
      generatorGroup.addProperty(spacingYProp);

      auto spacingZProp =
          makeProp(generatorPrefix + QStringLiteral("spacingZ"),
                   PropertyType::Float,
                       descriptor->settings.value(QStringLiteral("spacingZ"))
                           .toDouble(0.0),
                   orderBase + 10);
      spacingZProp->setDisplayLabel(QStringLiteral("Spacing Z"));
      spacingZProp->setSoftRange(-2000.0, 2000.0);
      generatorGroup.addProperty(spacingZProp);
    }

    generatorGroups.push_back(std::move(generatorGroup));
  }

  std::vector<PropertyGroup> fieldGroups;
  fieldGroups.reserve(impl_->extraFieldDescriptors_.count());
  for (std::size_t fieldIndex = 0;
       fieldIndex < impl_->extraFieldDescriptors_.count(); ++fieldIndex) {
    const auto* descriptor = impl_->extraFieldDescriptors_.at(fieldIndex);
    if (!descriptor) {
      continue;
    }
    const int orderBase = -320 - static_cast<int>(fieldIndex) * 20;
    const QString fieldPrefix =
        QStringLiteral("component.fields.%1.")
            .arg(static_cast<int>(fieldIndex));
    PropertyGroup fieldGroup(
        QStringLiteral("Field %1").arg(static_cast<int>(fieldIndex) + 1));

    auto enabledProp =
        makeProp(fieldPrefix + QStringLiteral("enabled"), PropertyType::Boolean,
                 descriptor->enabled, orderBase);
    enabledProp->setDisplayLabel(QStringLiteral("Enabled"));
    fieldGroup.addProperty(enabledProp);

    auto strengthProp =
        makeProp(fieldPrefix + QStringLiteral("strength"), PropertyType::Float,
                 static_cast<double>(descriptor->strength), orderBase + 1);
    strengthProp->setDisplayLabel(QStringLiteral("Strength"));
    strengthProp->setSoftRange(0.0, 2.0);
    fieldGroup.addProperty(strengthProp);

    auto invertProp =
        makeProp(fieldPrefix + QStringLiteral("invert"), PropertyType::Boolean,
                 descriptor->invert, orderBase + 2);
    invertProp->setDisplayLabel(QStringLiteral("Invert"));
    fieldGroup.addProperty(invertProp);

    auto centerXProp =
        makeProp(fieldPrefix + QStringLiteral("centerX"), PropertyType::Float,
                 descriptor->settings.value(QStringLiteral("centerX")).toDouble(0.0),
                 orderBase + 3);
    centerXProp->setDisplayLabel(QStringLiteral("Center X"));
    fieldGroup.addProperty(centerXProp);

    auto centerYProp =
        makeProp(fieldPrefix + QStringLiteral("centerY"), PropertyType::Float,
                 descriptor->settings.value(QStringLiteral("centerY")).toDouble(0.0),
                 orderBase + 4);
    centerYProp->setDisplayLabel(QStringLiteral("Center Y"));
    fieldGroup.addProperty(centerYProp);

    auto angleProp =
        makeProp(fieldPrefix + QStringLiteral("angle"), PropertyType::Float,
                 descriptor->settings.value(QStringLiteral("angle")).toDouble(0.0),
                 orderBase + 5);
    angleProp->setDisplayLabel(QStringLiteral("Angle"));
    angleProp->setUnit(QStringLiteral("deg"));
    angleProp->setSoftRange(-180.0, 180.0);
    fieldGroup.addProperty(angleProp);

    auto summaryProp =
        makeProp(fieldPrefix + QStringLiteral("summary"), PropertyType::String,
                 QStringLiteral("%1 cx=%2 cy=%3 a=%4")
                     .arg(descriptor->typeId.section('.', -1))
                     .arg(descriptor->settings.value(QStringLiteral("centerX")).toDouble(0.0), 0, 'f', 0)
                     .arg(descriptor->settings.value(QStringLiteral("centerY")).toDouble(0.0), 0, 'f', 0)
                     .arg(descriptor->settings.value(QStringLiteral("angle")).toDouble(0.0), 0, 'f', 0),
                 orderBase + 6);
    summaryProp->setDisplayLabel(QStringLiteral("Preview"));
    summaryProp->setTooltip(
        QStringLiteral("Compact field authoring summary for center and direction."));
    fieldGroup.addProperty(summaryProp);

    if (descriptor->typeId == QStringLiteral("artifact.field.solid")) {
      auto valueProp =
          makeProp(fieldPrefix + QStringLiteral("value"), PropertyType::Float,
                   descriptor->settings.value(QStringLiteral("value")).toDouble(1.0),
                   orderBase + 7);
      valueProp->setDisplayLabel(QStringLiteral("Value"));
      valueProp->setSoftRange(0.0, 1.0);
      fieldGroup.addProperty(valueProp);
    } else if (descriptor->typeId == QStringLiteral("artifact.field.sphere")) {
      auto radiusProp =
          makeProp(fieldPrefix + QStringLiteral("radius"), PropertyType::Float,
                   descriptor->settings.value(QStringLiteral("radius")).toDouble(160.0),
                   orderBase + 7);
      radiusProp->setDisplayLabel(QStringLiteral("Radius"));
      radiusProp->setSoftRange(0.0, 2000.0);
      fieldGroup.addProperty(radiusProp);

      auto falloffProp =
          makeProp(fieldPrefix + QStringLiteral("falloffWidth"), PropertyType::Float,
                   descriptor->settings.value(QStringLiteral("falloffWidth")).toDouble(40.0),
                   orderBase + 8);
      falloffProp->setDisplayLabel(QStringLiteral("Falloff Width"));
      falloffProp->setSoftRange(0.0, 1000.0);
      fieldGroup.addProperty(falloffProp);
    } else if (descriptor->typeId == QStringLiteral("artifact.field.box")) {
      auto halfXProp =
          makeProp(fieldPrefix + QStringLiteral("halfX"), PropertyType::Float,
                   descriptor->settings.value(QStringLiteral("halfX")).toDouble(120.0),
                   orderBase + 7);
      halfXProp->setDisplayLabel(QStringLiteral("Half X"));
      fieldGroup.addProperty(halfXProp);
      auto halfYProp =
          makeProp(fieldPrefix + QStringLiteral("halfY"), PropertyType::Float,
                   descriptor->settings.value(QStringLiteral("halfY")).toDouble(120.0),
                   orderBase + 8);
      halfYProp->setDisplayLabel(QStringLiteral("Half Y"));
      fieldGroup.addProperty(halfYProp);
      auto halfZProp =
          makeProp(fieldPrefix + QStringLiteral("halfZ"), PropertyType::Float,
                   descriptor->settings.value(QStringLiteral("halfZ")).toDouble(120.0),
                   orderBase + 9);
      halfZProp->setDisplayLabel(QStringLiteral("Half Z"));
      fieldGroup.addProperty(halfZProp);
      auto falloffProp =
          makeProp(fieldPrefix + QStringLiteral("falloffWidth"), PropertyType::Float,
                   descriptor->settings.value(QStringLiteral("falloffWidth")).toDouble(40.0),
                   orderBase + 10);
      falloffProp->setDisplayLabel(QStringLiteral("Falloff Width"));
      fieldGroup.addProperty(falloffProp);
    } else if (descriptor->typeId == QStringLiteral("artifact.field.linear")) {
      auto lengthProp =
          makeProp(fieldPrefix + QStringLiteral("length"), PropertyType::Float,
                   descriptor->settings.value(QStringLiteral("length")).toDouble(320.0),
                   orderBase + 7);
      lengthProp->setDisplayLabel(QStringLiteral("Length"));
      lengthProp->setSoftRange(1.0, 4000.0);
      fieldGroup.addProperty(lengthProp);
      auto smoothProp =
          makeProp(fieldPrefix + QStringLiteral("useSmoothstep"),
                   PropertyType::Boolean,
                   descriptor->settings.value(QStringLiteral("useSmoothstep")).toBool(true),
                   orderBase + 8);
      smoothProp->setDisplayLabel(QStringLiteral("Smoothstep"));
      fieldGroup.addProperty(smoothProp);
    } else if (descriptor->typeId == QStringLiteral("artifact.field.radial")) {
      auto innerProp =
          makeProp(fieldPrefix + QStringLiteral("innerRadius"), PropertyType::Float,
                   descriptor->settings.value(QStringLiteral("innerRadius")).toDouble(0.0),
                   orderBase + 7);
      innerProp->setDisplayLabel(QStringLiteral("Inner Radius"));
      fieldGroup.addProperty(innerProp);
      auto outerProp =
          makeProp(fieldPrefix + QStringLiteral("outerRadius"), PropertyType::Float,
                   descriptor->settings.value(QStringLiteral("outerRadius")).toDouble(160.0),
                   orderBase + 8);
      outerProp->setDisplayLabel(QStringLiteral("Outer Radius"));
      fieldGroup.addProperty(outerProp);
    } else if (descriptor->typeId == QStringLiteral("artifact.field.noise")) {
      auto scaleProp =
          makeProp(fieldPrefix + QStringLiteral("scale"), PropertyType::Float,
                   descriptor->settings.value(QStringLiteral("scale")).toDouble(120.0),
                   orderBase + 7);
      scaleProp->setDisplayLabel(QStringLiteral("Scale"));
      fieldGroup.addProperty(scaleProp);
      auto amplitudeProp =
          makeProp(fieldPrefix + QStringLiteral("amplitude"), PropertyType::Float,
                   descriptor->settings.value(QStringLiteral("amplitude")).toDouble(1.0),
                   orderBase + 8);
      amplitudeProp->setDisplayLabel(QStringLiteral("Amplitude"));
      fieldGroup.addProperty(amplitudeProp);
      auto octavesProp =
          makeProp(fieldPrefix + QStringLiteral("octaves"), PropertyType::Integer,
                   descriptor->settings.value(QStringLiteral("octaves")).toInt(3),
                   orderBase + 9);
      octavesProp->setDisplayLabel(QStringLiteral("Octaves"));
      fieldGroup.addProperty(octavesProp);
    }

    fieldGroups.push_back(std::move(fieldGroup));
  }

  std::vector<PropertyGroup> cloneModifierGroups;
  const auto cloneModifiers = layerCloneModifiers();
  cloneModifierGroups.reserve(cloneModifiers.size());
  for (std::size_t modifierIndex = 0; modifierIndex < cloneModifiers.size();
       ++modifierIndex) {
    const auto& descriptor = cloneModifiers[modifierIndex];
    const int orderBase = -420 - static_cast<int>(modifierIndex) * 20;
    const bool isCompatModifier =
        descriptor.modifierId.startsWith(QStringLiteral("modifier.compat."));
    const QString modifierPrefix = isCompatModifier
        ? (descriptor.typeId == QStringLiteral("artifact.modifier.time-offset")
               ? QStringLiteral("component.cloner.modifiers.compat.timeOffset.")
               : QStringLiteral("component.cloner.modifiers.compat.sequence."))
        : QStringLiteral("component.cloneModifiers.%1.")
              .arg(static_cast<int>(modifierIndex) - 2);

    PropertyGroup modifierGroup(
        QStringLiteral("Clone Modifier %1")
            .arg(static_cast<int>(modifierIndex) + 1));

    auto enabledProp =
        makeProp(modifierPrefix + QStringLiteral("enabled"),
                 PropertyType::Boolean, descriptor.enabled, orderBase);
    enabledProp->setDisplayLabel(QStringLiteral("Enabled"));
    modifierGroup.addProperty(enabledProp);

    if (descriptor.typeId == QStringLiteral("artifact.modifier.time-offset")) {
      auto stepProp =
          makeProp(modifierPrefix + QStringLiteral("step"), PropertyType::Float,
                   descriptor.settings.value(QStringLiteral("step")).toDouble(0.0),
                   orderBase + 1);
      stepProp->setDisplayLabel(QStringLiteral("Step"));
      stepProp->setUnit(QStringLiteral("s"));
      stepProp->setSoftRange(-5.0, 5.0);
      stepProp->setTooltip(
          QStringLiteral("Positive values delay each successive clone, including its physics fall."));
      modifierGroup.addProperty(stepProp);
    } else if (descriptor.typeId ==
               QStringLiteral("artifact.modifier.sequence")) {
      auto rateProp =
          makeProp(modifierPrefix + QStringLiteral("rate"), PropertyType::Float,
                   descriptor.settings.value(QStringLiteral("rate")).toDouble(8.0),
                   orderBase + 1);
      rateProp->setDisplayLabel(QStringLiteral("Rate"));
      rateProp->setSoftRange(0.1, 60.0);
      modifierGroup.addProperty(rateProp);

      auto softnessProp = makeProp(
          modifierPrefix + QStringLiteral("softness"), PropertyType::Float,
          descriptor.settings.value(QStringLiteral("softness")).toDouble(1.0),
          orderBase + 2);
      softnessProp->setDisplayLabel(QStringLiteral("Softness"));
      softnessProp->setSoftRange(0.1, 16.0);
      modifierGroup.addProperty(softnessProp);
    } else if (descriptor.typeId == QStringLiteral("artifact.modifier.plain") ||
               descriptor.typeId == QStringLiteral("artifact.modifier.random")) {
      const bool random = descriptor.typeId == QStringLiteral("artifact.modifier.random");
      auto numberProp = [&](const QString& name, const QString& label,
                            double fallback, int order, const QString& unit,
                            double softMin, double softMax) {
        auto prop = makeProp(modifierPrefix + name, PropertyType::Float,
                             descriptor.settings.value(name).toDouble(fallback),
                             orderBase + order);
        prop->setDisplayLabel(label);
        if (!unit.isEmpty()) {
          prop->setUnit(unit);
        }
        prop->setSoftRange(softMin, softMax);
        modifierGroup.addProperty(prop);
      };
      numberProp(QStringLiteral("positionX"), QStringLiteral("Position X"), 0.0, 1,
                 QStringLiteral("px"), -1000.0, 1000.0);
      numberProp(QStringLiteral("positionY"), QStringLiteral("Position Y"), 0.0, 2,
                 QStringLiteral("px"), -1000.0, 1000.0);
      numberProp(QStringLiteral("positionZ"), QStringLiteral("Position Z"), 0.0, 3,
                 QStringLiteral("px"), -1000.0, 1000.0);
      numberProp(QStringLiteral("rotationZ"), QStringLiteral("Rotation Z"), 0.0, 4,
                 QStringLiteral("deg"), -360.0, 360.0);
      if (random) {
        numberProp(QStringLiteral("scaleVariance"), QStringLiteral("Scale Variance"), 0.0, 5,
                   QString(), 0.0, 1.0);
        auto seedProp = makeProp(modifierPrefix + QStringLiteral("seed"),
                                 PropertyType::Integer,
                                 descriptor.settings.value(QStringLiteral("seed")).toInt(1),
                                 orderBase + 6);
        seedProp->setDisplayLabel(QStringLiteral("Seed"));
        seedProp->setHardRange(-2147483647.0, 2147483647.0);
        modifierGroup.addProperty(seedProp);
      } else {
        numberProp(QStringLiteral("scaleX"), QStringLiteral("Scale X"), 1.0, 5,
                   QString(), 0.0, 4.0);
        numberProp(QStringLiteral("scaleY"), QStringLiteral("Scale Y"), 1.0, 6,
                   QString(), 0.0, 4.0);
        numberProp(QStringLiteral("scaleZ"), QStringLiteral("Scale Z"), 1.0, 7,
                   QString(), 0.0, 4.0);
      }
      auto strengthProp = makeProp(
          modifierPrefix + QStringLiteral("strength"), PropertyType::Float,
          descriptor.settings.value(QStringLiteral("strength")).toDouble(1.0),
          orderBase + (random ? 7 : 8));
      strengthProp->setDisplayLabel(QStringLiteral("Strength"));
      strengthProp->setHardRange(0.0, 1.0);
      strengthProp->setSoftRange(0.0, 1.0);
      modifierGroup.addProperty(strengthProp);
    }

    cloneModifierGroups.push_back(std::move(modifierGroup));
  }
  for (int transformIndex = 0;
       transformIndex < static_cast<int>(impl_->clonerTransforms_.size());
       ++transformIndex) {
    const auto &op = impl_->clonerTransforms_[static_cast<size_t>(transformIndex)];
    const QString prefix =
        QStringLiteral("component.cloner.transforms.%1.").arg(transformIndex);
    const QString title = op.name.trimmed().isEmpty()
                              ? QStringLiteral("Transform %1").arg(transformIndex + 1)
                              : op.name.trimmed();
    auto nameProp = makeProp(prefix + QStringLiteral("name"),
                             PropertyType::String, title, -42 - transformIndex * 10);
    nameProp->setDisplayLabel(QStringLiteral("Name"));
    clonerGroup.addProperty(nameProp);
    auto enabledProp = makeProp(prefix + QStringLiteral("enabled"),
                                PropertyType::Boolean, op.enabled,
                                -41 - transformIndex * 10);
    enabledProp->setDisplayLabel(QStringLiteral("Enabled"));
    clonerGroup.addProperty(enabledProp);
    auto posXProp = makeProp(prefix + QStringLiteral("positionX"),
                             PropertyType::Float,
                             static_cast<double>(op.position.x()),
                             -40 - transformIndex * 10);
    posXProp->setDisplayLabel(QStringLiteral("Position X"));
    clonerGroup.addProperty(posXProp);
    auto posYProp = makeProp(prefix + QStringLiteral("positionY"),
                             PropertyType::Float,
                             static_cast<double>(op.position.y()),
                             -39 - transformIndex * 10);
    posYProp->setDisplayLabel(QStringLiteral("Position Y"));
    clonerGroup.addProperty(posYProp);
    auto posZProp = makeProp(prefix + QStringLiteral("positionZ"),
                             PropertyType::Float,
                             static_cast<double>(op.position.z()),
                             -38 - transformIndex * 10);
    posZProp->setDisplayLabel(QStringLiteral("Position Z"));
    clonerGroup.addProperty(posZProp);
    auto rotXProp = makeProp(prefix + QStringLiteral("rotationX"),
                             PropertyType::Float,
                             static_cast<double>(op.rotation.x()),
                             -37 - transformIndex * 10);
    rotXProp->setDisplayLabel(QStringLiteral("Rotation X"));
    clonerGroup.addProperty(rotXProp);
    auto rotYProp = makeProp(prefix + QStringLiteral("rotationY"),
                             PropertyType::Float,
                             static_cast<double>(op.rotation.y()),
                             -36 - transformIndex * 10);
    rotYProp->setDisplayLabel(QStringLiteral("Rotation Y"));
    clonerGroup.addProperty(rotYProp);
    auto rotZProp = makeProp(prefix + QStringLiteral("rotationZ"),
                             PropertyType::Float,
                             static_cast<double>(op.rotation.z()),
                             -35 - transformIndex * 10);
    rotZProp->setDisplayLabel(QStringLiteral("Rotation Z"));
    clonerGroup.addProperty(rotZProp);
    auto scaleXProp = makeProp(prefix + QStringLiteral("scaleX"),
                               PropertyType::Float,
                               static_cast<double>(op.scale.x()),
                               -34 - transformIndex * 10);
    scaleXProp->setDisplayLabel(QStringLiteral("Scale X"));
    clonerGroup.addProperty(scaleXProp);
    auto scaleYProp = makeProp(prefix + QStringLiteral("scaleY"),
                               PropertyType::Float,
                               static_cast<double>(op.scale.y()),
                               -33 - transformIndex * 10);
    scaleYProp->setDisplayLabel(QStringLiteral("Scale Y"));
    clonerGroup.addProperty(scaleYProp);
    auto scaleZProp = makeProp(prefix + QStringLiteral("scaleZ"),
                               PropertyType::Float,
                               static_cast<double>(op.scale.z()),
                               -32 - transformIndex * 10);
    scaleZProp->setDisplayLabel(QStringLiteral("Scale Z"));
    clonerGroup.addProperty(scaleZProp);
  }

  auto isAdjustmentProp =
      makeProp(QStringLiteral("layer.isAdjustment"), PropertyType::Boolean,
               isAdjustmentLayer(), -50);
  isAdjustmentProp->setTooltip(
      QStringLiteral("Apply effects to all layers below"));
  layerGroup.addProperty(isAdjustmentProp);

  std::vector<PropertyGroup> maskGroups;
  maskGroups.reserve(static_cast<size_t>(maskCount()));
  for (int maskIndex = 0; maskIndex < maskCount(); ++maskIndex) {
    const LayerMask resolvedMask = mask(maskIndex);
    PropertyGroup maskGroup(QStringLiteral("Mask %1").arg(maskIndex + 1));

    auto maskEnabledProp =
        makeProp(maskPropertyPrefix(maskIndex) + QStringLiteral(".enabled"),
                 PropertyType::Boolean, resolvedMask.isEnabled(),
                 -240 - maskIndex);
    maskEnabledProp->setAnimatable(true);
    maskEnabledProp->setDisplayLabel(QStringLiteral("Enabled"));
    maskGroup.addProperty(maskEnabledProp);

    for (int pathIndex = 0; pathIndex < resolvedMask.maskPathCount();
         ++pathIndex) {
      const MaskPath path = resolvedMask.maskPath(pathIndex);
      const QString pathPrefix = maskPathPropertyPrefix(maskIndex, pathIndex);
      const QString pathLabel =
          QStringLiteral("Path %1").arg(pathIndex + 1);

      auto closedProp = makeProp(pathPrefix + QStringLiteral(".closed"),
                                 PropertyType::Boolean, path.isClosed(),
                                 -230 - pathIndex);
      closedProp->setAnimatable(true);
      closedProp->setDisplayLabel(pathLabel + QStringLiteral(" Closed"));
      maskGroup.addProperty(closedProp);

      auto opacityProp = makeProp(pathPrefix + QStringLiteral(".opacity"),
                                  PropertyType::Float,
                                  static_cast<double>(path.opacity()),
                                  -229 - pathIndex);
      opacityProp->setAnimatable(true);
      opacityProp->setHardRange(0.0, 1.0);
      opacityProp->setSoftRange(0.0, 1.0);
      opacityProp->setStep(0.01);
      opacityProp->setDisplayLabel(pathLabel + QStringLiteral(" Opacity"));
      maskGroup.addProperty(opacityProp);

      auto featherProp = makeProp(pathPrefix + QStringLiteral(".feather"),
                                  PropertyType::Float,
                                  static_cast<double>(path.feather()),
                                  -228 - pathIndex);
      featherProp->setAnimatable(true);
      featherProp->setSoftRange(0.0, 128.0);
      featherProp->setStep(0.5);
      featherProp->setDisplayLabel(pathLabel + QStringLiteral(" Feather"));
      maskGroup.addProperty(featherProp);

      auto fhProp = makeProp(pathPrefix + QStringLiteral(".featherHorizontal"),
                             PropertyType::Float,
                             static_cast<double>(path.featherHorizontal()),
                             -232 - pathIndex);
      fhProp->setAnimatable(true);
      fhProp->setSoftRange(0.0, 128.0);
      fhProp->setStep(0.5);
      fhProp->setDisplayLabel(pathLabel + QStringLiteral(" Feather H"));
      maskGroup.addProperty(fhProp);

      auto fvProp = makeProp(pathPrefix + QStringLiteral(".featherVertical"),
                             PropertyType::Float,
                             static_cast<double>(path.featherVertical()),
                             -233 - pathIndex);
      fvProp->setAnimatable(true);
      fvProp->setSoftRange(0.0, 128.0);
      fvProp->setStep(0.5);
      fvProp->setDisplayLabel(pathLabel + QStringLiteral(" Feather V"));
      maskGroup.addProperty(fvProp);

      auto fiProp = makeProp(pathPrefix + QStringLiteral(".featherInner"),
                             PropertyType::Float,
                             static_cast<double>(path.featherInner()),
                             -234 - pathIndex);
      fiProp->setAnimatable(true);
      fiProp->setSoftRange(0.0, 128.0);
      fiProp->setStep(0.5);
      fiProp->setDisplayLabel(pathLabel + QStringLiteral(" Feather Inner"));
      maskGroup.addProperty(fiProp);

      auto foProp = makeProp(pathPrefix + QStringLiteral(".featherOuter"),
                             PropertyType::Float,
                             static_cast<double>(path.featherOuter()),
                             -235 - pathIndex);
      foProp->setAnimatable(true);
      foProp->setSoftRange(0.0, 128.0);
      foProp->setStep(0.5);
      foProp->setDisplayLabel(pathLabel + QStringLiteral(" Feather Outer"));
      maskGroup.addProperty(foProp);

      auto expansionProp = makeProp(pathPrefix + QStringLiteral(".expansion"),
                                    PropertyType::Float,
                                    static_cast<double>(path.expansion()),
                                    -227 - pathIndex);
      expansionProp->setAnimatable(true);
      expansionProp->setSoftRange(-256.0, 256.0);
      expansionProp->setStep(0.5);
      expansionProp->setDisplayLabel(pathLabel + QStringLiteral(" Expansion"));
      maskGroup.addProperty(expansionProp);

      auto invertedProp = makeProp(pathPrefix + QStringLiteral(".inverted"),
                                   PropertyType::Boolean, path.isInverted(),
                                   -226 - pathIndex);
      invertedProp->setAnimatable(true);
      invertedProp->setDisplayLabel(pathLabel + QStringLiteral(" Inverted"));
      maskGroup.addProperty(invertedProp);

      auto modeProp = makeProp(pathPrefix + QStringLiteral(".mode"),
                               PropertyType::Integer,
                               static_cast<int>(path.mode()),
                               -225 - pathIndex);
      modeProp->setAnimatable(true);
      modeProp->setTooltip(
          QStringLiteral("0=Add,1=Subtract,2=Intersect,3=Difference"));
      modeProp->setDisplayLabel(pathLabel + QStringLiteral(" Mode"));
      maskGroup.addProperty(modeProp);
    }

    maskGroups.push_back(std::move(maskGroup));
  }

  std::vector<PropertyGroup> groups;
  groups.reserve(9 + maskGroups.size());
  groups.push_back(std::move(initialGroup));
  groups.push_back(std::move(transformGroup));
  groups.push_back(std::move(physicsGroup));
  groups.push_back(std::move(motionGroup));
  groups.push_back(std::move(fractureGroup));
  groups.push_back(std::move(trailGroup));
  groups.push_back(std::move(fragmentAppearanceGroup));
  groups.push_back(std::move(componentGroup));
  groups.push_back(std::move(collisionGroup));
  groups.push_back(std::move(layoutGroup));
  groups.push_back(std::move(clonerGroup));
  for (auto &group : generatorGroups) {
    groups.push_back(std::move(group));
  }
  for (auto &group : fieldGroups) {
    groups.push_back(std::move(group));
  }
  for (auto &group : cloneModifierGroups) {
    groups.push_back(std::move(group));
  }
  groups.push_back(std::move(crowdGroup));
  groups.push_back(std::move(particleEmitterGroup));
  groups.push_back(std::move(fluidGroup));
  groups.push_back(std::move(layerGroup));
  for (auto &group : maskGroups) {
    groups.push_back(std::move(group));
  }
  return groups;
}

std::shared_ptr<ArtifactCore::AbstractProperty>
ArtifactAbstractLayer::getProperty(const QString &name) const {
  std::lock_guard<std::mutex> lock(impl_->propertyCacheMutex_);
  auto &cache = impl_->propertyCache_;
  auto it = cache.find(name);
  if (it != cache.end()) {
    return it.value();
  }
  return nullptr;
}

std::shared_ptr<ArtifactCore::AbstractProperty>
ArtifactAbstractLayer::persistentLayerProperty(const QString &propertyPath,
                                               PropertyType type,
                                               const QVariant &value,
                                               int priority) const {
  std::lock_guard<std::mutex> lock(impl_->propertyCacheMutex_);
  auto &cache = impl_->propertyCache_;
  auto it = cache.find(propertyPath);
  if (it == cache.end() || !it.value()) {
    it = cache.insert(propertyPath, std::make_shared<AbstractProperty>());
  }
  auto property = it.value();
  const bool hasAnimatedValue =
      property->isAnimatable() && !property->getKeyFrames().empty();
  property->setName(propertyPath);
  property->setType(type);
  if (!hasAnimatedValue && !property->hasExpression()) {
    property->setValue(value);
  }
  property->setDisplayPriority(priority);
  if (type == PropertyType::Integer) {
    property->setStep(1);
  }
  return property;
}

bool ArtifactAbstractLayer::setLayerPropertyValue(const QString &propertyPath,
                                                  const QVariant &value) {
  if (propertyPath == QStringLiteral("layer.name")) {
    setLayerName(value.toString());
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
        maskAddress->maskIndex >= impl_->maskCount()) {
      return false;
    }

    if (maskAddress->pathIndex < 0) {
      LayerMask mask = impl_->getMask(maskAddress->maskIndex);
      if (maskAddress->field == QStringLiteral("enabled")) {
        mask.setEnabled(value.toBool());
        impl_->setMask(maskAddress->maskIndex, mask);
        notifyLayerMutation(this, LayerDirtyFlag::Mask,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      return false;
    }

    LayerMask mask = impl_->getMask(maskAddress->maskIndex);
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
    impl_->setMask(maskAddress->maskIndex, mask);
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
    impl_->physicsComponent_.settings().stiffness = static_cast<float>(value.toDouble());
    return true;
  }
  if (propertyPath == QStringLiteral("physics.damping")) {
    impl_->physicsComponent_.settings().damping = static_cast<float>(value.toDouble());
    return true;
  }
  if (propertyPath == QStringLiteral("physics.followThroughGain")) {
    impl_->physicsComponent_.settings().followThroughGain = static_cast<float>(value.toDouble());
    return true;
  }
  if (propertyPath == QStringLiteral("physics.gravityY")) {
    impl_->physicsComponent_.settings().gravityY = static_cast<float>(value.toDouble());
    impl_->physicsComponent_.reset();
    return true;
  }
  if (propertyPath == QStringLiteral("physics.linearDamping")) {
    impl_->physicsComponent_.settings().linearDamping = static_cast<float>(value.toDouble());
    impl_->physicsComponent_.reset();
    return true;
  }
  if (propertyPath == QStringLiteral("physics.restitution")) {
    impl_->physicsComponent_.settings().restitution =
        static_cast<float>(
            std::clamp(value.toDouble(), 0.0, 1.0));
    if (hasSoftBodyPhysics()) {
      syncSoftBodyPhysicsColliderToBounds();
    }
    if (hasRigidBodyPhysics()) {
      syncRigidBodyPhysicsToBounds();
    }
    return true;
  }
  if (propertyPath == QStringLiteral("physics.initialVelocityY")) {
    impl_->clonePhysicsInitialVelocityY_ = static_cast<float>(
        std::clamp(value.toDouble(), -5000.0, 5000.0));
    return true;
  }
  if (propertyPath == QStringLiteral("physics.maxBounces")) {
    impl_->clonePhysicsMaxBounces_ = std::clamp(value.toInt(), 0, 32);
    return true;
  }
  if (propertyPath == QStringLiteral("physics.wiggleFreq")) {
    impl_->physicsComponent_.settings().wiggleFreq = static_cast<float>(value.toDouble());
    return true;
  }
  if (propertyPath == QStringLiteral("physics.wiggleAmp")) {
    impl_->physicsComponent_.settings().wiggleAmp = static_cast<float>(value.toDouble());
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
    impl_->motionDynamicsStiffness_ = static_cast<float>(std::clamp(value.toDouble(), 0.0, 1000.0));
    return true;
  }
  if (propertyPath == QStringLiteral("motion.damping")) {
    impl_->motionDynamicsDamping_ = static_cast<float>(std::clamp(value.toDouble(), 0.0, 100.0));
    return true;
  }
  if (propertyPath == QStringLiteral("motion.mass")) {
    impl_->motionDynamicsMass_ = static_cast<float>(std::clamp(value.toDouble(), 0.1, 100.0));
    return true;
  }
  if (propertyPath == QStringLiteral("motion.lagTau")) {
    impl_->motionDynamicsLagTau_ = static_cast<float>(std::clamp(value.toDouble(), 0.001, 10.0));
    return true;
  }
  if (propertyPath == QStringLiteral("motion.clampOvershoot")) {
    impl_->motionDynamicsClampOvershoot_ = value.toBool();
    return true;
  }
  if (propertyPath == QStringLiteral("motion.overshootLimit")) {
    impl_->motionDynamicsOvershootLimit_ = static_cast<float>(std::clamp(value.toDouble(), 0.0, 2.0));
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
    impl_->motionTrailFade_ = static_cast<float>(std::clamp(value.toDouble(), 0.0, 1.0));
    return true;
  }
  if (propertyPath == QStringLiteral("trail.width")) {
    impl_->motionTrailWidth_ = static_cast<float>(std::clamp(value.toDouble(), 0.1, 128.0));
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.velocityStretch.enabled")) {
    impl_->fragmentVelocityStretchEnabled_ = value.toBool();
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.velocityStretch.strength")) {
    impl_->fragmentVelocityStretchStrength_ = static_cast<float>(
        std::clamp(value.toDouble(), 0.0, 1.0));
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.velocityStretch.max")) {
    impl_->fragmentVelocityStretchMax_ = static_cast<float>(
        std::clamp(value.toDouble(), 1.0, 32.0));
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.colorVariation.enabled")) {
    impl_->fragmentColorVariationEnabled_ = value.toBool();
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.colorVariation.amount")) {
    impl_->fragmentColorVariation_ = static_cast<float>(
        std::clamp(value.toDouble(), 0.0, 1.0));
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
    impl_->fragmentClonerOutputSpacingX_ = static_cast<float>(
        std::clamp(value.toDouble(), -100000.0, 100000.0));
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.clonerOutput.spacingY")) {
    impl_->fragmentClonerOutputSpacingY_ = static_cast<float>(
        std::clamp(value.toDouble(), -100000.0, 100000.0));
    return true;
  }
  if (propertyPath == QStringLiteral("fragment.clonerOutput.timeOffsetFrames")) {
    impl_->fragmentClonerOutputTimeOffsetFrames_ = static_cast<float>(
        std::clamp(value.toDouble(), -10000.0, 10000.0));
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.enabled")) {
    impl_->fractureEnabled_ = value.toBool();
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
    impl_->fractureTriggerFrame_ = std::max<int64_t>(-1, value.toLongLong());
    impl_->fractureTriggerLastFrame_ = std::numeric_limits<int64_t>::min();
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.preset")) {
    impl_->fracturePreset_ = std::clamp(value.toInt(), 0, static_cast<int>(FracturePreset::Dust));
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.crackThreshold")) {
    impl_->fractureCrackThreshold_ = static_cast<float>(std::clamp(value.toDouble(), 0.0, 1000.0));
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.shatterThreshold")) {
    impl_->fractureShatterThreshold_ = static_cast<float>(std::clamp(value.toDouble(), 0.0, 1000.0));
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.shardCount")) {
    impl_->fractureShardCount_ = std::max(1, value.toInt());
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.shardDamping")) {
    impl_->fractureShardDamping_ = static_cast<float>(std::clamp(value.toDouble(), 0.0, 1.0));
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.shardGravity")) {
    impl_->fractureShardGravity_ = static_cast<float>(std::clamp(value.toDouble(), -5000.0, 5000.0));
    return true;
  }
  if (propertyPath == QStringLiteral("fracture.impactSensitivity")) {
    impl_->fractureImpactSensitivity_ = static_cast<float>(std::clamp(value.toDouble(), 0.0, 10.0));
    return true;
  }

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
        descriptor.typeId = QStringLiteral("artifact.generator.cloner.grid");
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
          return setSetting(field, value.toDouble());
        }
        if (field == QStringLiteral("sequenceEnabled")) {
          return setSetting(field, value.toBool());
        }
        if (field == QStringLiteral("sequenceRate")) {
          return setSetting(field, std::clamp(value.toDouble(), 0.1, 240.0));
        }
        if (field == QStringLiteral("sequenceSoftness")) {
          return setSetting(field, std::clamp(value.toDouble(), 0.1, 32.0));
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
          return setSetting(field, value.toDouble());
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
          return setSetting(fieldName, value.toDouble());
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
        descriptor.settings[QStringLiteral("rotationZ")] = 0.0;
        descriptor.settings[QStringLiteral("scaleVariance")] = 0.0;
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
        impl_->clonerTimeOffsetStep_ = static_cast<float>(value.toDouble());
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
        impl_->clonerSequenceRate_ =
            static_cast<float>(std::clamp(value.toDouble(), 0.1, 240.0));
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
        return true;
      }
      if (field == QStringLiteral("softness")) {
        impl_->clonerSequenceSoftness_ =
            static_cast<float>(std::clamp(value.toDouble(), 0.1, 32.0));
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
        if (field == QStringLiteral("enabled")) {
          descriptor->enabled = value.toBool();
          notifyLayerMutation(this, LayerDirtyFlag::Effect,
                              LayerDirtyReason::PropertyChanged);
          return true;
        }
        if (field == QStringLiteral("step")) {
          return setSetting(field, value.toDouble());
        }
        if (field == QStringLiteral("rate")) {
          return setSetting(field, std::clamp(value.toDouble(), 0.1, 240.0));
        }
        if (field == QStringLiteral("softness")) {
          return setSetting(field, std::clamp(value.toDouble(), 0.1, 32.0));
        }
        if (field == QStringLiteral("seed")) {
          return setSetting(field, value.toInt());
        }
        if (field == QStringLiteral("strength")) {
          return setSetting(field, std::clamp(value.toDouble(), 0.0, 1.0));
        }
        if (field == QStringLiteral("positionX") ||
            field == QStringLiteral("positionY") ||
            field == QStringLiteral("positionZ") ||
            field == QStringLiteral("rotationZ") ||
            field == QStringLiteral("scaleX") ||
            field == QStringLiteral("scaleY") ||
            field == QStringLiteral("scaleZ") ||
            field == QStringLiteral("scaleVariance")) {
          return setSetting(field, value.toDouble());
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
        impl_->clonerTransforms_.erase(
            impl_->clonerTransforms_.begin() + index);
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
            impl_->clonerTransforms_.begin() + index + 1, copy);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
      }
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.transforms.moveUp")) {
      const int index = value.toInt();
      if (index > 0 &&
          index < static_cast<int>(impl_->clonerTransforms_.size())) {
        std::swap(impl_->clonerTransforms_[static_cast<size_t>(index)],
                  impl_->clonerTransforms_[static_cast<size_t>(index - 1)]);
        notifyLayerMutation(this, LayerDirtyFlag::Effect,
                            LayerDirtyReason::PropertyChanged);
      }
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.transforms.moveDown")) {
      const int index = value.toInt();
      if (index >= 0 &&
          index + 1 < static_cast<int>(impl_->clonerTransforms_.size())) {
        std::swap(impl_->clonerTransforms_[static_cast<size_t>(index)],
                  impl_->clonerTransforms_[static_cast<size_t>(index + 1)]);
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
        op.position.setX(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("positionY")) {
        op.position.setY(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("positionZ")) {
        op.position.setZ(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("rotationX")) {
        op.rotation.setX(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("rotationY")) {
        op.rotation.setY(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("rotationZ")) {
        op.rotation.setZ(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("scaleX")) {
        op.scale.setX(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("scaleY")) {
        op.scale.setY(static_cast<float>(value.toDouble()));
      } else if (field == QStringLiteral("scaleZ")) {
        op.scale.setZ(static_cast<float>(value.toDouble()));
      } else {
        return false;
      }
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
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
    if (propertyPath == QStringLiteral("component.collision.shape")) {
      impl_->collisionShape_ = std::clamp(value.toInt(), 0, 2);
      impl_->lastCollisionImpactFrame_ =
          std::numeric_limits<int64_t>::min();
      impl_->syncBuiltinComponentDescriptors();
      resyncActiveCollisionPhysics();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.width")) {
      impl_->collisionWidth_ = static_cast<float>(
          std::clamp(value.toDouble(), 0.0, 100000.0));
      impl_->lastCollisionImpactFrame_ =
          std::numeric_limits<int64_t>::min();
      impl_->syncBuiltinComponentDescriptors();
      resyncActiveCollisionPhysics();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.height")) {
      impl_->collisionHeight_ = static_cast<float>(
          std::clamp(value.toDouble(), 0.0, 100000.0));
      impl_->lastCollisionImpactFrame_ =
          std::numeric_limits<int64_t>::min();
      impl_->syncBuiltinComponentDescriptors();
      resyncActiveCollisionPhysics();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.radius")) {
      impl_->collisionRadius_ = static_cast<float>(
          std::clamp(value.toDouble(), 0.0, 100000.0));
      impl_->lastCollisionImpactFrame_ =
          std::numeric_limits<int64_t>::min();
      impl_->syncBuiltinComponentDescriptors();
      resyncActiveCollisionPhysics();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.offsetX")) {
      impl_->collisionOffsetX_ = static_cast<float>(
          std::clamp(value.toDouble(), -100000.0, 100000.0));
      impl_->lastCollisionImpactFrame_ =
          std::numeric_limits<int64_t>::min();
      impl_->syncBuiltinComponentDescriptors();
      resyncActiveCollisionPhysics();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.offsetY")) {
      impl_->collisionOffsetY_ = static_cast<float>(
          std::clamp(value.toDouble(), -100000.0, 100000.0));
      impl_->lastCollisionImpactFrame_ =
          std::numeric_limits<int64_t>::min();
      impl_->syncBuiltinComponentDescriptors();
      resyncActiveCollisionPhysics();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.collision.floorY")) {
      impl_->collisionFloorY_ = static_cast<float>(
          std::clamp(value.toDouble(), 0.0, 100000.0));
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
    if (propertyPath == QStringLiteral("component.crowd.enabled")) {
      impl_->crowdComponentEnabled_ = value.toBool();
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.crowd.cohesion")) {
      impl_->crowdCohesion_ =
          static_cast<float>(std::clamp(value.toDouble(), 0.0, 10.0));
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.crowd.separation")) {
      impl_->crowdSeparation_ =
          static_cast<float>(std::clamp(value.toDouble(), 0.0, 10.0));
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.crowd.alignment")) {
      impl_->crowdAlignment_ =
          static_cast<float>(std::clamp(value.toDouble(), 0.0, 10.0));
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.crowd.maxSpeed")) {
      impl_->crowdMaxSpeed_ =
          static_cast<float>(std::clamp(value.toDouble(), 0.0, 10000.0));
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.crowd.jitter")) {
      impl_->crowdJitter_ =
          static_cast<float>(std::clamp(value.toDouble(), 0.0, 10.0));
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
      impl_->particleEmitterSpeed_ =
          static_cast<float>(std::clamp(value.toDouble(), 0.0, 100000.0));
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath ==
        QStringLiteral("component.particleEmitter.lifetime")) {
      impl_->particleEmitterLifetime_ =
          static_cast<float>(std::clamp(value.toDouble(), 0.01, 3600.0));
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.enabled")) {
      impl_->fluidComponentEnabled_ = value.toBool();
      if (!impl_->fluidComponentEnabled_) {
        impl_->fluidSolver_.reset();
        impl_->fluidPreviewParticles_.clear();
        impl_->fluidLastFrame_ = std::numeric_limits<int64_t>::min();
      }
      impl_->syncBuiltinComponentDescriptors();
      Q_EMIT changed();
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
      impl_->fluidViscosity_ =
          static_cast<float>(std::clamp(value.toDouble(), 0.0, 1.0));
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.diffusion")) {
      impl_->fluidDiffusion_ =
          static_cast<float>(std::clamp(value.toDouble(), 0.0, 1.0));
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.buoyancy")) {
      impl_->fluidBuoyancy_ =
          static_cast<float>(std::clamp(value.toDouble(), -2.0, 2.0));
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.vorticity")) {
      impl_->fluidVorticity_ =
          static_cast<float>(std::clamp(value.toDouble(), 0.0, 10.0));
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.fluid.solverIterations")) {
      impl_->fluidSolverIterations_ = std::clamp(value.toInt(), 1, 256);
      impl_->syncBuiltinComponentDescriptors();
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.enabled")) {
      impl_->layoutComponentEnabled_ = value.toBool();
      impl_->syncBuiltinComponentDescriptors();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.mode")) {
      impl_->layoutMode_ = value.toInt();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.anchorMode")) {
      impl_->layoutAnchorMode_ = value.toInt();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.horizontalPin")) {
      impl_->layoutHorizontalPin_ = value.toInt();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.verticalPin")) {
      impl_->layoutVerticalPin_ = value.toInt();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.scaleMode")) {
      impl_->layoutScaleMode_ = value.toInt();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.safeAreaEnabled")) {
      impl_->layoutSafeAreaEnabled_ = value.toBool();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.safeAreaPaddingX")) {
      impl_->layoutSafeAreaPaddingX_ = static_cast<float>(value.toDouble());
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.safeAreaPaddingY")) {
      impl_->layoutSafeAreaPaddingY_ = static_cast<float>(value.toDouble());
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.stackDirection")) {
      impl_->layoutStackDirection_ = value.toInt();
      Q_EMIT changed();
      return true;
    }
    if (propertyPath == QStringLiteral("component.layout.gap")) {
      impl_->layoutGap_ = static_cast<float>(value.toDouble());
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
      impl_->clonerCloneCount_ = std::max(1, value.toInt());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.timeOffsetStep")) {
      impl_->clonerTimeOffsetStep_ = static_cast<float>(value.toDouble());
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
      impl_->clonerSequenceRate_ =
          std::max(0.01f, static_cast<float>(value.toDouble()));
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.sequenceSoftness")) {
      impl_->clonerSequenceSoftness_ =
          std::max(0.01f, static_cast<float>(value.toDouble()));
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
  if (propertyPath == QStringLiteral("component.cloner.offsetX")) {
    impl_->clonerOffsetX_ = static_cast<float>(value.toDouble());
    notifyLayerMutation(this, LayerDirtyFlag::Effect,
                        LayerDirtyReason::PropertyChanged);
    return true;
  }
    if (propertyPath == QStringLiteral("component.cloner.offsetY")) {
      impl_->clonerOffsetY_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.offsetZ")) {
      impl_->clonerOffsetZ_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.jitterX")) {
      impl_->clonerJitterX_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.jitterY")) {
      impl_->clonerJitterY_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.jitterZ")) {
      impl_->clonerJitterZ_ = static_cast<float>(value.toDouble());
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
      impl_->clonerSpacingX_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.spacingY")) {
      impl_->clonerSpacingY_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.spacingZ")) {
      impl_->clonerSpacingZ_ = static_cast<float>(value.toDouble());
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
      impl_->clonerRadius_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.startAngle")) {
      impl_->clonerStartAngle_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.endAngle")) {
      impl_->clonerEndAngle_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.rotationStep")) {
      impl_->clonerRotationStep_ = static_cast<float>(value.toDouble());
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
    if (propertyPath == QStringLiteral("component.cloner.opacityDecay")) {
      impl_->clonerOpacityDecay_ = std::clamp(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
      notifyLayerMutation(this, LayerDirtyFlag::Effect,
                          LayerDirtyReason::PropertyChanged);
      return true;
    }
  auto &t3 = transform3D();
  const RationalTime currentTime = currentTimelineTime(this);
  const auto propertyHasKeys = [this](const QString &path) {
    const auto property = getProperty(path);
    return property && property->isAnimatable() &&
           !property->getKeyFrames().empty();
  };

  if (propertyPath == QStringLiteral("transform.initialRotation")) {
    t3.setInitialRotation(currentTime, static_cast<float>(value.toDouble()));
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

  if (propertyPath == QStringLiteral("transform.position.x")) {
    const float x = static_cast<float>(value.toDouble());
    if (propertyHasKeys(propertyPath)) {
      const float initialX = t3.positionX() - t3.positionXAt(currentTime);
      t3.setPosition(currentTime, x - initialX, t3.positionYAt(currentTime));
    } else {
      t3.removePositionKeyFrameAt(currentTime);
      t3.setInitialPosition(currentTime, x, t3.positionY());
    }
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.position.y")) {
    const float y = static_cast<float>(value.toDouble());
    if (propertyHasKeys(propertyPath)) {
      const float initialY = t3.positionY() - t3.positionYAt(currentTime);
      t3.setPosition(currentTime, t3.positionXAt(currentTime), y - initialY);
    } else {
      t3.removePositionKeyFrameAt(currentTime);
      t3.setInitialPosition(currentTime, t3.positionX(), y);
    }
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.scale.x")) {
    if (propertyHasKeys(propertyPath)) {
      t3.setScale(currentTime, value.toDouble(), t3.scaleY());
    } else {
      t3.removeScaleKeyFrameAt(currentTime);
      t3.setInitialScale(currentTime, value.toDouble(), t3.scaleY());
    }
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.scale.y")) {
    if (propertyHasKeys(propertyPath)) {
      t3.setScale(currentTime, t3.scaleX(), value.toDouble());
    } else {
      t3.removeScaleKeyFrameAt(currentTime);
      t3.setInitialScale(currentTime, t3.scaleX(), value.toDouble());
    }
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.rotation")) {
    if (propertyHasKeys(propertyPath)) {
      t3.setRotation(currentTime, value.toDouble());
    } else {
      t3.removeRotationKeyFrameAt(currentTime);
      t3.setInitialRotation(currentTime, value.toDouble());
    }
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.autoOrient")) {
    t3.setAutoOrientMode(static_cast<AutoOrientMode>(value.toInt()));
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.anchor.x")) {
    if (propertyHasKeys(propertyPath)) {
      t3.setAnchor(currentTime, value.toDouble(), t3.anchorYAt(currentTime),
                   t3.anchorZAt(currentTime));
    } else {
      t3.setCurrentAnchor(value.toDouble(), t3.anchorY(), t3.anchorZ());
    }
    notifyLayerMutation(this, LayerDirtyFlag::Transform,
                        LayerDirtyReason::TransformChanged);
    return true;
  }
  if (propertyPath == QStringLiteral("transform.anchor.y")) {
    if (propertyHasKeys(propertyPath)) {
      t3.setAnchor(currentTime, t3.anchorXAt(currentTime), value.toDouble(),
                   t3.anchorZAt(currentTime));
    } else {
      t3.setCurrentAnchor(t3.anchorX(), value.toDouble(), t3.anchorZ());
    }
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
    const int width = std::max(1, value.toInt());
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
    const int height = std::max(1, value.toInt());
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
  // サムネイル用に黒いイメージを作成（プレースホルダー実装）
  const QSize targetSize(std::max(1, width), std::max(1, height));
  if (!impl_->thumbnailCache_.isNull() &&
      impl_->thumbnailCacheSize_ == targetSize) {
    return impl_->thumbnailCache_;
  }

  QImage thumbnail(targetSize.width(), targetSize.height(),
                   QImage::Format_ARGB32);
  thumbnail.fill(QColor(0, 0, 0, 255)); // 黒で塗りつぶし
  impl_->thumbnailCache_ = thumbnail;
  impl_->thumbnailCacheSize_ = targetSize;

  // TODO: 実際のレイヤーコンテンツをサムネイルにレンダリング
  qDebug() << "[Thumbnail] Generated placeholder thumbnail:" << width << "x"
           << height;

  return impl_->thumbnailCache_;
}

// -- Mask Impl methods --

void ArtifactAbstractLayer::Impl::addMask(const LayerMask &mask) {
  masks_.push_back(mask);
  qDebug() << "[ArtifactAbstractLayer] Mask added, count:" << masks_.size();
}

void ArtifactAbstractLayer::Impl::removeMask(int index) {
  if (index >= 0 && index < static_cast<int>(masks_.size())) {
    masks_.erase(masks_.begin() + index);
    qDebug() << "[ArtifactAbstractLayer] Mask removed at index:" << index;
  }
}

void ArtifactAbstractLayer::Impl::setMask(int index, const LayerMask &mask) {
  if (index >= 0 && index < static_cast<int>(masks_.size()))
    masks_[index] = mask;
}

LayerMask ArtifactAbstractLayer::Impl::getMask(int index) const {
  if (index >= 0 && index < static_cast<int>(masks_.size()))
    return masks_[index];
  return {};
}

int ArtifactAbstractLayer::Impl::maskCount() const {
  return static_cast<int>(masks_.size());
}

void ArtifactAbstractLayer::Impl::clearMasks() { masks_.clear(); }

// -- Mask public methods --

void ArtifactAbstractLayer::addMask(const LayerMask &mask) {
  impl_->addMask(mask);
}

void ArtifactAbstractLayer::removeMask(int index) { impl_->removeMask(index); }

void ArtifactAbstractLayer::setMask(int index, const LayerMask &mask) {
  impl_->setMask(index, mask);
}

LayerMask ArtifactAbstractLayer::mask(int index) const {
  LayerMask resolved = impl_->getMask(index);
  applyMaskPropertyState(this, index, resolved);
  return resolved;
}

int ArtifactAbstractLayer::maskCount() const { return impl_->maskCount(); }

void ArtifactAbstractLayer::clearMasks() { impl_->clearMasks(); }

bool ArtifactAbstractLayer::hasMasks() const { return impl_->maskCount() > 0; }

std::vector<LayerMatteReference> ArtifactAbstractLayer::matteReferences() const {
  return impl_->mattes_;
}

void ArtifactAbstractLayer::setMatteReferences(const std::vector<LayerMatteReference>& refs) {
  impl_->mattes_ = refs;
}

void ArtifactAbstractLayer::addMatteReference(const LayerMatteReference& ref) {
  impl_->mattes_.push_back(ref);
}

void ArtifactAbstractLayer::clearMatteReferences() {
  impl_->mattes_.clear();
}

MatteStack ArtifactAbstractLayer::buildMatteStack() const {
    MatteStack stack;
    for (const auto& ref : impl_->mattes_) {
        if (ref.enabled && !ref.sourceLayerId.isNil()) {
            stack.addNode(ref.toCoreMatteNode());
        }
    }
    return stack;
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
    const auto it =
        impl_->propertyCache_.constFind(QStringLiteral("layer.opacity"));
    if (it != impl_->propertyCache_.constEnd() && it.value()) {
      const auto &property = *it.value();
      if (property.isAnimatable() && !property.getKeyFrames().empty()) {
        const RationalTime time = currentTimelineTime(this);
        const QVariant animatedValue = property.interpolateValue(time);
        if (animatedValue.isValid()) {
          baseOpacity = static_cast<float>(animatedValue.toDouble());
        }
      }
    }
  }
  return applyLayerEffectEnvelopeOpacity(impl_->effectEnvelope_, baseOpacity,
                                         impl_->currentFrame_,
                                         impl_->inPoint_,
                                         impl_->outPoint_,
                                         impl_->startTime_);
}

void ArtifactAbstractLayer::setOpacity(float value) {
  const float clamped = std::clamp(value, 0.0f, 1.0f);
  
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
  if (auto it = impl_->propertyCache_.find(QStringLiteral("layer.opacity"));
      it != impl_->propertyCache_.end() && it.value()) {
    auto& prop = *it.value();
    if (prop.isAnimatable() && !prop.getKeyFrames().empty()) {
        const RationalTime time = currentTimelineTime(this);
        prop.addKeyFrame(time, clamped);
        changed = true;
    } else {
        if (impl_->opacity_ != clamped) {
            impl_->opacity_ = clamped;
            prop.setValue(clamped);
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


