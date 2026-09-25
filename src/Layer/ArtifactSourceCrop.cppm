module;

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QTransform>

module Artifact.Layer.SourceCrop;

import Core.ArtifactMath;
import Serialization.Registry;
import Serialization.SchemaMigration;

namespace Artifact {

namespace {
const bool registeredSourceCrop = [] {
    ArtifactCore::Serialization::registerSerializableType<SourceCrop>();
    ArtifactCore::Serialization::SchemaMigrationRegistry::instance().registerMigration(
        QStringLiteral("ArtifactSourceCrop"), 0, 1,
        [](const QJsonObject& legacy) { return legacy; });
    return true;
}();
namespace {

QJsonArray pointToJson(const QPointF &point) {
  return QJsonArray{point.x(), point.y()};
}

QPointF pointFromJson(const QJsonArray &array, const QPointF &fallback) {
  if (array.size() >= 2) {
    if (array.at(0).isDouble() && array.at(1).isDouble()) {
      const double x = array.at(0).toDouble(fallback.x());
      const double y = array.at(1).toDouble(fallback.y());
      return ArtifactCore::artifactIsFinite(x) && ArtifactCore::artifactIsFinite(y) ? QPointF(x, y) : fallback;
    }
  }
  return fallback;
}

QJsonArray rectToJson(const QRectF &rect) {
  return QJsonArray{rect.x(), rect.y(), rect.width(), rect.height()};
}

QRectF rectFromJson(const QJsonArray &array, const QRectF &fallback) {
  if (array.size() >= 4) {
    if (array.at(0).isDouble() && array.at(1).isDouble() && array.at(2).isDouble() && array.at(3).isDouble()) {
      const double x = array.at(0).toDouble(fallback.x());
      const double y = array.at(1).toDouble(fallback.y());
      const double w = array.at(2).toDouble(fallback.width());
      const double h = array.at(3).toDouble(fallback.height());
      if (ArtifactCore::artifactIsFinite(x) && ArtifactCore::artifactIsFinite(y) && ArtifactCore::artifactIsFinite(w) && ArtifactCore::artifactIsFinite(h)) {
        return QRectF(x, y, w, h).normalized();
      }
    }
  }
  return fallback;
}

bool hasSourceSize(const QSizeF &size) {
  return ArtifactCore::artifactIsFinite(size.width()) && ArtifactCore::artifactIsFinite(size.height()) &&
         size.width() > 0.0 && size.height() > 0.0;
}

QRectF fullSourceRect(const QSizeF &size) {
  if (!hasSourceSize(size)) {
    return QRectF();
  }
  return QRectF(QPointF(0.0, 0.0), size);
}

QPointF clampAnchor(const QPointF &anchor) {
  const auto safe = [](double value, double fallback) {
    return ArtifactCore::artifactIsFinite(value) ? ArtifactCore::artifactClamp(value, 0.0, 1.0) : fallback;
  };
  return QPointF(safe(anchor.x(), 0.5), safe(anchor.y(), 0.5));
}

QRectF clampRectToSource(const QRectF &rect, const QRectF &sourceBounds) {
  if (!rect.isValid() || rect.width() <= 0.0 || rect.height() <= 0.0) {
    return sourceBounds;
  }

  QRectF result = rect.normalized();
  if (!sourceBounds.isValid()) {
    return result;
  }

  if (result.width() >= sourceBounds.width() && result.height() >= sourceBounds.height()) {
    return sourceBounds;
  }

  if (result.width() > sourceBounds.width()) {
    result.setWidth(sourceBounds.width());
  }
  if (result.height() > sourceBounds.height()) {
    result.setHeight(sourceBounds.height());
  }

  if (result.left() < sourceBounds.left()) {
    result.moveLeft(sourceBounds.left());
  }
  if (result.top() < sourceBounds.top()) {
    result.moveTop(sourceBounds.top());
  }
  if (result.right() > sourceBounds.right()) {
    result.moveRight(sourceBounds.right());
  }
  if (result.bottom() > sourceBounds.bottom()) {
    result.moveBottom(sourceBounds.bottom());
  }
  return result;
}

} // namespace

} // namespace

std::uint64_t SourceCrop::revision() const {
  return revision_;
}

bool SourceCrop::enabled() const {
  return enabled_;
}

void SourceCrop::setEnabled(bool enabled) {
  if (enabled_ == enabled) return;
  enabled_ = enabled;
  ++revision_;
}

QRectF SourceCrop::cropRect() const {
  return cropRect_;
}

void SourceCrop::setCropRect(const QRectF &rect) {
  QRectF next;
  if (!ArtifactCore::artifactIsFinite(rect.x()) || !ArtifactCore::artifactIsFinite(rect.y()) ||
      !ArtifactCore::artifactIsFinite(rect.width()) || !ArtifactCore::artifactIsFinite(rect.height())) {
    next = QRectF();
  } else {
    next = rect.normalized();
  }
  if (cropRect_ == next) return;
  cropRect_ = next;
  ++revision_;
}

QPointF SourceCrop::pan() const {
  return pan_;
}

void SourceCrop::setPan(const QPointF &pan) {
  const auto safe = [](double value) {
    return ArtifactCore::artifactIsFinite(value) ? ArtifactCore::artifactClamp(value, -1000000.0, 1000000.0) : 0.0;
  };
  const QPointF next(safe(pan.x()), safe(pan.y()));
  if (pan_ == next) return;
  pan_ = next;
  ++revision_;
}

double SourceCrop::zoom() const {
  return zoom_;
}

void SourceCrop::setZoom(double zoom) {
  const double next = !ArtifactCore::artifactIsFinite(zoom) || zoom <= 0.0
      ? 1.0 : ArtifactCore::artifactClamp(zoom, 0.001, 1000.0);
  if (zoom_ == next) return;
  zoom_ = next;
  ++revision_;
}

double SourceCrop::rotation() const {
  return rotation_;
}

void SourceCrop::setRotation(double rotation) {
  const double next = ArtifactCore::artifactIsFinite(rotation)
      ? ArtifactCore::artifactClamp(rotation, -360000.0, 360000.0)
      : 0.0;
  if (rotation_ == next) return;
  rotation_ = next;
  ++revision_;
}

QPointF SourceCrop::anchor() const {
  return anchor_;
}

void SourceCrop::setAnchor(const QPointF &anchor) {
  const QPointF next = clampAnchor(anchor);
  if (anchor_ == next) return;
  anchor_ = next;
  ++revision_;
}

bool SourceCrop::preserveAspect() const {
  return preserveAspect_;
}

void SourceCrop::setPreserveAspect(bool preserveAspect) {
  if (preserveAspect_ == preserveAspect) return;
  preserveAspect_ = preserveAspect;
  ++revision_;
}

void SourceCrop::reset() {
  if (!enabled_ && cropRect_ == QRectF() && pan_ == QPointF(0.0, 0.0) &&
      zoom_ == 1.0 && rotation_ == 0.0 && anchor_ == QPointF(0.5, 0.5) &&
      preserveAspect_) {
    return;
  }
  enabled_ = false;
  cropRect_ = QRectF();
  pan_ = QPointF(0.0, 0.0);
  zoom_ = 1.0;
  rotation_ = 0.0;
  anchor_ = QPointF(0.5, 0.5);
  preserveAspect_ = true;
  ++revision_;
}

void SourceCrop::clampToSource(const QSizeF &sourceSize) {
  const QRectF previousCropRect = cropRect_;
  const QPointF previousAnchor = anchor_;
  anchor_ = clampAnchor(anchor_);
  if (!hasSourceSize(sourceSize)) {
    if (cropRect_ != previousCropRect || anchor_ != previousAnchor) ++revision_;
    return;
  }

  const QRectF sourceBounds = fullSourceRect(sourceSize);
  if (!cropRect_.isValid() || cropRect_.width() <= 0.0 || cropRect_.height() <= 0.0) {
    cropRect_ = sourceBounds;
  } else {
    cropRect_ = clampRectToSource(cropRect_.normalized(), sourceBounds);
  }
  if (cropRect_ != previousCropRect || anchor_ != previousAnchor) ++revision_;
}

QRectF SourceCrop::effectiveCropRect(const QSizeF &sourceSize) const {
  if (!hasSourceSize(sourceSize)) {
    return QRectF();
  }

  const QRectF sourceBounds = fullSourceRect(sourceSize);
  QRectF baseRect = cropRect_;
  if (!baseRect.isValid() || baseRect.width() <= 0.0 || baseRect.height() <= 0.0) {
    baseRect = sourceBounds;
  } else {
    baseRect = clampRectToSource(baseRect.normalized(), sourceBounds);
  }

  const QPointF normalizedAnchor = clampAnchor(anchor_);
  const double safeZoom = ArtifactCore::artifactMax(zoom_, 1e-6);
  const QPointF baseAnchor = baseRect.topLeft() +
                             QPointF(baseRect.width() * normalizedAnchor.x(),
                                     baseRect.height() * normalizedAnchor.y());
  const QSizeF zoomedSize(baseRect.width() / safeZoom,
                          baseRect.height() / safeZoom);
  QPointF topLeft = baseAnchor - QPointF(zoomedSize.width() * normalizedAnchor.x(),
                                         zoomedSize.height() * normalizedAnchor.y());
  topLeft += pan_;

  QRectF zoomed(topLeft, zoomedSize);
  return zoomed.intersected(sourceBounds);
}

QTransform SourceCrop::sourceToOutputTransform(const QSizeF &sourceSize,
                                               const QSizeF &outputSize) const {
  if (!enabled_) {
    return QTransform();
  }
  if (!hasSourceSize(sourceSize) || !hasSourceSize(outputSize)) {
    return QTransform();
  }

  const QRectF crop = effectiveCropRect(sourceSize);
  if (!crop.isValid() || crop.width() <= 0.0 || crop.height() <= 0.0) {
    return QTransform();
  }

  const QPointF cropCenter = crop.center();
  const QPointF outputCenter(outputSize.width() * 0.5, outputSize.height() * 0.5);

  double scaleX = outputSize.width() / crop.width();
  double scaleY = outputSize.height() / crop.height();
  if (preserveAspect_) {
    const double uniformScale = ArtifactCore::artifactMin(scaleX, scaleY);
    scaleX = uniformScale;
    scaleY = uniformScale;
  }

  const double radians = rotation_ * (ArtifactCore::artifactAcos(-1.0) / 180.0);
  const double c = ArtifactCore::artifactCos(radians);
  const double s = ArtifactCore::artifactSin(radians);

  const double m11 = c * scaleX;
  const double m12 = -s * scaleY;
  const double m21 = s * scaleX;
  const double m22 = c * scaleY;
  const double dx = outputCenter.x() - (m11 * cropCenter.x() + m12 * cropCenter.y());
  const double dy = outputCenter.y() - (m21 * cropCenter.x() + m22 * cropCenter.y());

  return QTransform(m11, m12, m21, m22, dx, dy);
}

QJsonObject SourceCrop::toJson() const {
  QJsonObject obj;
  obj.insert(QStringLiteral("enabled"), enabled_);
  obj.insert(QStringLiteral("cropRect"), rectToJson(cropRect_));
  obj.insert(QStringLiteral("pan"), pointToJson(pan_));
  obj.insert(QStringLiteral("zoom"), zoom_);
  obj.insert(QStringLiteral("rotation"), rotation_);
  obj.insert(QStringLiteral("anchor"), pointToJson(anchor_));
  obj.insert(QStringLiteral("preserveAspect"), preserveAspect_);
  return obj;
}

void SourceCrop::fromJson(const QJsonObject &obj) {
  const std::uint64_t previousRevision = revision_;
  const bool previousEnabled = enabled_;
  const QRectF previousCropRect = cropRect_;
  const QPointF previousPan = pan_;
  const double previousZoom = zoom_;
  const double previousRotation = rotation_;
  const QPointF previousAnchor = anchor_;
  const bool previousPreserveAspect = preserveAspect_;
  enabled_ = obj.value(QStringLiteral("enabled")).toBool(false);
  cropRect_ = rectFromJson(obj.value(QStringLiteral("cropRect")).toArray(), QRectF());
  const QPointF storedPan = pointFromJson(
      obj.value(QStringLiteral("pan")).toArray(), QPointF(0.0, 0.0));
  zoom_ = obj.value(QStringLiteral("zoom")).toDouble(1.0);
  rotation_ = obj.value(QStringLiteral("rotation")).toDouble(0.0);
  anchor_ = pointFromJson(obj.value(QStringLiteral("anchor")).toArray(), QPointF(0.5, 0.5));
  preserveAspect_ = obj.value(QStringLiteral("preserveAspect")).toBool(true);
  setZoom(zoom_);
  setRotation(rotation_);
  setAnchor(anchor_);
  setPan(storedPan);
  if (!cropRect_.isValid() || cropRect_.width() <= 0.0 || cropRect_.height() <= 0.0) {
    cropRect_ = QRectF();
  }
  if (revision_ == previousRevision &&
      (enabled_ != previousEnabled || cropRect_ != previousCropRect ||
       pan_ != previousPan || zoom_ != previousZoom ||
       rotation_ != previousRotation || anchor_ != previousAnchor ||
       preserveAspect_ != previousPreserveAspect)) {
    ++revision_;
  }
}

} // namespace Artifact
