module;
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPen>
#include <QPolygonF>
#include <QtGlobal>
module Artifact.Widgets.Timeline.KeyframeIconHelper;

namespace Artifact {
namespace {
QString cacheKey(const KeyframeIconStyle& style)
{
  return QStringLiteral("%1x%2:%3:%4:%5:%6:%7:%8")
      .arg(style.size.width())
      .arg(style.size.height())
      .arg(style.fillColor.rgba(), 8, 16, QLatin1Char('0'))
      .arg(style.outlineColor.rgba(), 8, 16, QLatin1Char('0'))
      .arg(static_cast<int>(style.state))
      .arg(static_cast<int>(style.meaning))
      .arg(style.currentFrame ? 1 : 0)
      .arg(style.layerTimePinned ? 1 : 0);
}
}

QIcon makeKeyframeIcon(const KeyframeIconStyle& style)
{
  const int width = qMax(1, style.size.width());
  const int height = qMax(1, style.size.height());
  QPixmap pixmap(width, height);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QColor fill = style.fillColor;
  QColor outline = style.outlineColor;
  qreal penWidth = 1.2;

  if (style.state == KeyframeIconState::Disabled) {
    fill.setAlphaF(fill.alphaF() * 0.35);
    outline.setAlphaF(outline.alphaF() * 0.35);
    penWidth = 1.0;
  } else if (style.state == KeyframeIconState::Locked) {
    fill = fill.darker(110);
  } else if (style.state == KeyframeIconState::Selected) {
    penWidth = 1.6;
  }

  painter.setPen(QPen(outline, penWidth));
  painter.setBrush(fill);
  const qreal centerX = width * 0.5;
  const qreal pinSpace = style.layerTimePinned ? qMax<qreal>(2.0, height * 0.2) : 0.0;
  const qreal top = 1.0;
  const qreal bottom = qMax(top + 2.0, height - 1.0 - pinSpace);
  const qreal halfWidth = qMin(width * 0.31, (bottom - top) * 0.38);
  const qreal centerY = (top + bottom) * 0.5;
  const QPolygonF diamond{
      QPointF(centerX, top), QPointF(centerX + halfWidth, centerY),
      QPointF(centerX, bottom), QPointF(centerX - halfWidth, centerY)};
  painter.drawPolygon(diamond);

  if (style.currentFrame) {
    painter.setBrush(Qt::NoBrush);
    painter.drawPolygon(diamond);
  }

  if (style.layerTimePinned) {
    QColor pinColor = style.state == KeyframeIconState::Selected ? outline : fill;
    if (style.state == KeyframeIconState::Disabled) {
      pinColor.setAlphaF(pinColor.alphaF() * 0.35);
    }
    const qreal y = height - 1.25;
    const qreal pinHalfWidth = qMax<qreal>(2.0, halfWidth * 0.62);
    painter.setPen(QPen(pinColor, qMax<qreal>(1.0, height * 0.09),
                        Qt::SolidLine, Qt::FlatCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(centerX - pinHalfWidth, y),
                     QPointF(centerX + pinHalfWidth, y));
  }

  painter.end();
  return QIcon(pixmap);
}

QIcon cachedKeyframeIcon(const KeyframeIconStyle& style)
{
  static QHash<QString, QIcon> iconCache;
  const QString key = cacheKey(style);
  auto it = iconCache.constFind(key);
  if (it != iconCache.constEnd()) {
    return it.value();
  }
  const QIcon icon = makeKeyframeIcon(style);
  iconCache.insert(key, icon);
  return icon;
}

} // namespace Artifact
