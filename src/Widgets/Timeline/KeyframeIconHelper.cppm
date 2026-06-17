module;
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPen>
#include <QtGlobal>
module Artifact.Widgets.Timeline.KeyframeIconHelper;

namespace Artifact {
namespace {
QString cacheKey(const KeyframeIconStyle& style)
{
  return QStringLiteral("%1x%2:%3:%4:%5:%6:%7")
      .arg(style.size.width())
      .arg(style.size.height())
      .arg(style.fillColor.rgba(), 8, 16, QLatin1Char('0'))
      .arg(style.outlineColor.rgba(), 8, 16, QLatin1Char('0'))
      .arg(static_cast<int>(style.state))
      .arg(static_cast<int>(style.meaning))
      .arg(style.currentFrame ? 1 : 0);
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
  painter.translate(width * 0.5, height * 0.5);
  painter.rotate(45.0);

  const QRectF square(-width * 0.24, -height * 0.24, width * 0.48, height * 0.48);
  painter.drawRoundedRect(square, 0.8, 0.8);

  if (style.currentFrame) {
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(square.adjusted(-1.0, -1.0, 1.0, 1.0), 1.2, 1.2);
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
