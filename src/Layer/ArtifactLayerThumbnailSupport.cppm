module;
#include <algorithm>
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <QRect>
#include <QRectF>

module Artifact.Layer.ThumbnailSupport;

namespace Artifact {

QImage buildLayerThumbnail(const QSize &targetSize, const QString &layerName,
                           int sourceWidth, int sourceHeight) {
  QImage thumbnail(targetSize, QImage::Format_ARGB32_Premultiplied);
  thumbnail.fill(QColor(27, 31, 39));
  QPainter painter(&thumbnail);
  painter.setRenderHint(QPainter::Antialiasing, true);
  constexpr int cell = 8;
  for (int y = 0; y < targetSize.height(); y += cell) {
    for (int x = 0; x < targetSize.width(); x += cell) {
      if (((x / cell) + (y / cell)) % 2 == 0) {
        painter.fillRect(QRect(x, y, cell, cell), QColor(42, 47, 57));
      }
    }
  }
  const QColor accent(116, 169, 255);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(14, 18, 25, 210));
  painter.drawRoundedRect(
      QRectF(4.0, 4.0, std::max(0, targetSize.width() - 8),
             std::max(0, targetSize.height() - 8)),
      4.0, 4.0);
  painter.setBrush(accent);
  painter.drawRoundedRect(
      QRectF(6.0, 6.0, std::max(0, targetSize.width() - 12), 3.0), 1.5,
      1.5);
  painter.setPen(QPen(QColor(150, 171, 199), 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(
      QRectF(1.0, 1.0, std::max(0, targetSize.width() - 2),
             std::max(0, targetSize.height() - 2)),
      3.0, 3.0);
  QFont font;
  font.setBold(true);
  font.setPointSizeF(std::max<qreal>(7.0, targetSize.height() * 0.075));
  painter.setFont(font);
  painter.setPen(QColor(236, 241, 248));
  painter.drawText(
      thumbnail.rect().adjusted(8, 8, -8, -8),
      Qt::AlignCenter | Qt::TextWordWrap,
      QStringLiteral("%1\n%2 × %3")
          .arg(layerName.isEmpty() ? QStringLiteral("Layer") : layerName)
          .arg(sourceWidth)
          .arg(sourceHeight));
  return thumbnail;
}

} // namespace Artifact
