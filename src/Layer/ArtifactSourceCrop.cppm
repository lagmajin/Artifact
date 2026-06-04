module;
#include <algorithm>
#include <cmath>

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QTransform>

module Artifact.Layer.SourceCrop;

namespace Artifact {
namespace {

QJsonArray pointToJson(const QPointF &point) {
  return QJsonArray{point.x(), point.y()};
}

QPointF pointFromJson(const QJsonArray &array, const QPointF &fallback) {
  if (array.size() >= 2) {
    if (array.at(0).isDouble() && array.at(1).isDouble()) {
      const double x = array.at(0).toDouble(fallback.x());
      const double y = array.at(1).toDouble(fallback.y());
      return QPointF(x, y);
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
      return QRectF(x, y, w, h).normalized();
    }
  }
  return fallback;
}

bool hasSourceSize(const QSizeF &size) {
  return size.width() > 0.0 && size.height() > 0.0;
}

QRectF fullSourceRect(const QSizeF &size) {
  if (!hasSourceSize(size)) {
    return QRectF();
  }
  return QRectF(QPointF(0.0, 0.0), size);
}

QPointF clampAnchor(const QPointF &anchor) {
  return QPointF(std::clamp(anchor.x(), 0.0, 1.0),
                 std::clamp(anchor.y(), 0.0, 1.0));
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

bool SourceCrop::enabled() const {
  return enabled_;
}

void SourceCrop::setEnabled(bool enabled) {
  enabled_ = enabled;
}

QRectF SourceCrop::cropRect() const {
  return cropRect_;
}

void SourceCrop::setCropRect(const QRectF &rect) {
  cropRect_ = rect.normalized();
}

QPointF SourceCrop::pan() const {
  return pan_;
}

void SourceCrop::setPan(const QPointF &pan) {
  pan_ = pan;
}

double SourceCrop::zoom() const {
  return zoom_;
}

void SourceCrop::setZoom(double zoom) {
  if (!std::isfinite(zoom) || zoom <= 0.0) {
    zoom_ = 1.0;
    return;
  }
  zoom_ = std::max(zoom, 1e-6);
}

double SourceCrop::rotation() const {
  return rotation_;
}

void SourceCrop::setRotation(double rotation) {
  rotation_ = std::isfinite(rotation) ? rotation : 0.0;
}

QPointF SourceCrop::anchor() const {
  return anchor_;
}

void SourceCrop::setAnchor(const QPointF &anchor) {
  anchor_ = clampAnchor(anchor);
}

bool SourceCrop::preserveAspect() const {
  return preserveAspect_;
}

void SourceCrop::setPreserveAspect(bool preserveAspect) {
  preserveAspect_ = preserveAspect;
}

void SourceCrop::reset() {
  enabled_ = false;
  cropRect_ = QRectF();
  pan_ = QPointF(0.0, 0.0);
  zoom_ = 1.0;
  rotation_ = 0.0;
  anchor_ = QPointF(0.5, 0.5);
  preserveAspect_ = true;
}

void SourceCrop::clampToSource(const QSizeF &sourceSize) {
  anchor_ = clampAnchor(anchor_);
  if (!hasSourceSize(sourceSize)) {
    return;
  }

  const QRectF sourceBounds = fullSourceRect(sourceSize);
  if (!cropRect_.isValid() || cropRect_.width() <= 0.0 || cropRect_.height() <= 0.0) {
    cropRect_ = sourceBounds;
    return;
  }

  cropRect_ = clampRectToSource(cropRect_.normalized(), sourceBounds);
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
  const double safeZoom = std::max(zoom_, 1e-6);
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

  const QRectF crop = effectiveCropRect(sourceSize);
  if (!crop.isValid() || crop.width() <= 0.0 || crop.height() <= 0.0 ||
      outputSize.width() <= 0.0 || outputSize.height() <= 0.0) {
    return QTransform();
  }

  const QPointF cropCenter = crop.center();
  const QPointF outputCenter(outputSize.width() * 0.5, outputSize.height() * 0.5);

  double scaleX = outputSize.width() / crop.width();
  double scaleY = outputSize.height() / crop.height();
  if (preserveAspect_) {
    const double uniformScale = std::min(scaleX, scaleY);
    scaleX = uniformScale;
    scaleY = uniformScale;
  }

  const double radians = rotation_ * (std::acos(-1.0) / 180.0);
  const double c = std::cos(radians);
  const double s = std::sin(radians);

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
  enabled_ = obj.value(QStringLiteral("enabled")).toBool(false);
  cropRect_ = rectFromJson(obj.value(QStringLiteral("cropRect")).toArray(), QRectF());
  pan_ = pointFromJson(obj.value(QStringLiteral("pan")).toArray(), QPointF(0.0, 0.0));
  zoom_ = obj.value(QStringLiteral("zoom")).toDouble(1.0);
  rotation_ = obj.value(QStringLiteral("rotation")).toDouble(0.0);
  anchor_ = pointFromJson(obj.value(QStringLiteral("anchor")).toArray(), QPointF(0.5, 0.5));
  preserveAspect_ = obj.value(QStringLiteral("preserveAspect")).toBool(true);
  setZoom(zoom_);
  setRotation(rotation_);
  setAnchor(anchor_);
  if (!cropRect_.isValid() || cropRect_.width() <= 0.0 || cropRect_.height() <= 0.0) {
    cropRect_ = QRectF();
  }
}

} // namespace Artifact
