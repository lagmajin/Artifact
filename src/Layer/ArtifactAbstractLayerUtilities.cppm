module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include <QJsonObject>
#include <QMatrix4x4>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QTransform>
#include <QVariant>
#include <QVector4D>

export module Artifact.Layer.Abstract.Utilities;

import Artifact.Animation.LayerEffectEnvelope;
import Artifact.Effect.Abstract;
import Artifact.Layer.Modifier;
import Frame.Position;
import Memory.SharedPtr;

export namespace Artifact::LayerAbstractUtilities {

using namespace ArtifactCore;

float finiteClampedValue(const double raw, const double fallback,
                         const double minimum, const double maximum) {
  const double safeFallback = std::isfinite(fallback)
                                  ? std::clamp(fallback, minimum, maximum)
                                  : minimum;
  return static_cast<float>(std::isfinite(raw)
                                ? std::clamp(raw, minimum, maximum)
                                : safeFallback);
}

QJsonObject layerEffectEnvelopeToJson(const LayerEffectEnvelope& envelope) {
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

LayerEffectEnvelope layerEffectEnvelopeFromJson(const QJsonObject& obj) {
  LayerEffectEnvelope envelope;
  envelope.enabled = obj.value(QStringLiteral("enabled")).toBool(false);
  envelope.entry = obj.value(QStringLiteral("entry")).toBool(false);
  envelope.exit = obj.value(QStringLiteral("exit")).toBool(false);
  envelope.timing = static_cast<LayerEnvelopeTiming>(
      std::clamp(obj.value(QStringLiteral("timing")).toInt(0), 0, 2));
  envelope.curve = static_cast<LayerEnvelopeCurve>(
      std::clamp(obj.value(QStringLiteral("curve")).toInt(0), 0, 4));
  envelope.durationFrames = std::max<std::int64_t>(
      1, obj.value(QStringLiteral("durationFrames")).toVariant().toLongLong());
  envelope.effectStart = static_cast<float>(
      std::clamp(obj.value(QStringLiteral("effectStart")).toDouble(0.0), 0.0, 1.0));
  envelope.effectEnd = static_cast<float>(
      std::clamp(obj.value(QStringLiteral("effectEnd")).toDouble(1.0), 0.0, 1.0));
  return envelope;
}

float applyLayerEffectEnvelopeOpacity(const LayerEffectEnvelope& envelope,
                                      float opacity, std::int64_t currentFrame,
                                      const FramePosition& inPoint,
                                      const FramePosition& outPoint,
                                      const FramePosition& startTime) {
  if (!envelope.enabled || envelope.durationFrames <= 0) {
    return std::clamp(opacity, 0.0f, 1.0f);
  }
  const std::int64_t layerDuration = std::max<std::int64_t>(
      0, outPoint.framePosition() - inPoint.framePosition());
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

bool computeTimelineHiddenLayerPropertyGroup(const QString& groupName) {
  return groupName.trimmed().compare(QStringLiteral("Transform"),
                                     Qt::CaseInsensitive) != 0;
}

bool computeTimelineExpandedByDefaultLayerPropertyGroup(const QString& groupName) {
  return groupName.trimmed().compare(QStringLiteral("Transform"),
                                     Qt::CaseInsensitive) == 0;
}

bool computeInspectorHiddenLayerPropertyGroup(const QString& groupName) {
  const QString normalized = groupName.trimmed();
  return normalized.compare(QStringLiteral("Rig"), Qt::CaseInsensitive) == 0 ||
         normalized.compare(QStringLiteral("Rig Controls"),
                            Qt::CaseInsensitive) == 0;
}

bool computeInspectorExpandedByDefaultLayerPropertyGroup(const QString& groupName) {
  return groupName.trimmed().compare(QStringLiteral("Initial"),
                                     Qt::CaseInsensitive) == 0;
}

bool computeClonerLayerPropertyGroup(const QString& groupName) {
  return groupName.trimmed().compare(QStringLiteral("Cloner"),
                                     Qt::CaseInsensitive) == 0;
}

bool computeSourceReframeLayerPropertyGroup(const QString& groupName) {
  return groupName.trimmed().compare(QStringLiteral("Source Reframe"),
                                     Qt::CaseInsensitive) == 0;
}

QRectF mapRectWithMatrix(const QMatrix4x4& matrix, const QRectF& rect) {
  if (!rect.isValid() || rect.width() <= 0.0 || rect.height() <= 0.0) return {};
  const QVector4D corners[] = {
      {static_cast<float>(rect.left()), static_cast<float>(rect.top()), 0.0f, 1.0f},
      {static_cast<float>(rect.right()), static_cast<float>(rect.top()), 0.0f, 1.0f},
      {static_cast<float>(rect.right()), static_cast<float>(rect.bottom()), 0.0f, 1.0f},
      {static_cast<float>(rect.left()), static_cast<float>(rect.bottom()), 0.0f, 1.0f}};
  float minX = std::numeric_limits<float>::infinity();
  float minY = std::numeric_limits<float>::infinity();
  float maxX = -std::numeric_limits<float>::infinity();
  float maxY = -std::numeric_limits<float>::infinity();
  for (const auto& corner : corners) {
    const QVector4D mapped = matrix * corner;
    minX = std::min(minX, mapped.x());
    minY = std::min(minY, mapped.y());
    maxX = std::max(maxX, mapped.x());
    maxY = std::max(maxY, mapped.y());
  }
  if (!std::isfinite(minX) || !std::isfinite(minY) || !std::isfinite(maxX) ||
      !std::isfinite(maxY) || maxX <= minX || maxY <= minY) return {};
  return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

QMatrix4x4 matrixFromTransform2D(const QTransform& transform) {
  return QMatrix4x4(
      static_cast<float>(transform.m11()), static_cast<float>(transform.m21()), 0.0f, static_cast<float>(transform.m31()),
      static_cast<float>(transform.m12()), static_cast<float>(transform.m22()), 0.0f, static_cast<float>(transform.m32()),
      0.0f, 0.0f, 1.0f, 0.0f,
      static_cast<float>(transform.m13()), static_cast<float>(transform.m23()), 0.0f, static_cast<float>(transform.m33()));
}

QString slugifyEffectId(const QString& text) {
  QString slug;
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
  while (slug.endsWith(QChar('-'))) slug.chop(1);
  return slug.isEmpty() ? QStringLiteral("effect") : slug;
}

} // namespace Artifact::LayerAbstractUtilities
