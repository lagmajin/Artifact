#pragma once

#include <algorithm>
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace Artifact::TimelinePlayheadDraw {

inline void enableTimelinePainterHints(QPainter& painter) {
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
}

inline QColor playheadColor() {
  return QColor(85, 214, 245);
}

inline void drawPlayhead(QPainter& painter, const qreal x, const qreal stemTop,
                         const qreal stemBottom, const bool drawHead = true,
                         const qreal headTop = 0.0,
                         const qreal maxHeadHeight = 10.0,
                         const qreal headWidth = 12.0) {
  const QColor color = playheadColor();
  enableTimelinePainterHints(painter);

  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(color, 2, Qt::SolidLine, Qt::FlatCap));
  painter.drawLine(QPointF(x, stemTop), QPointF(x, stemBottom));

  if (drawHead) {
    const qreal availableHeight = std::max<qreal>(0.0, stemBottom - headTop);
    const qreal headHeight =
        std::min<qreal>(maxHeadHeight, std::max<qreal>(0.0, availableHeight - 1.0));
    if (headHeight >= 6.0) {
      // Precision Blade: a low-profile machined cap with an ice-cyan center
      // keeps the exact frame visible without competing with amber keyframes.
      const qreal halfWidth = headWidth * 0.5;
      const qreal bladeHalf = std::clamp(headWidth * 0.105, 1.1, 1.65);
      const qreal shoulderY = headTop + headHeight * 0.58;
      const qreal tipY = headTop + headHeight;
      QPainterPath headPath;
      headPath.moveTo(x - halfWidth, headTop);
      headPath.lineTo(x + halfWidth, headTop);
      headPath.lineTo(x + halfWidth * 0.64, shoulderY);
      headPath.lineTo(x + bladeHalf, shoulderY);
      headPath.lineTo(x, tipY);
      headPath.lineTo(x - bladeHalf, shoulderY);
      headPath.lineTo(x - halfWidth * 0.62, shoulderY);
      headPath.closeSubpath();
      painter.setPen(QPen(QColor(14, 16, 18, 210), 1.25,
                          Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      painter.setBrush(QColor(174, 184, 196));
      painter.drawPath(headPath);

      QPainterPath bladePath;
      bladePath.moveTo(x - bladeHalf, headTop + 1.0);
      bladePath.lineTo(x + bladeHalf, headTop + 1.0);
      bladePath.lineTo(x + bladeHalf * 0.72, shoulderY);
      bladePath.lineTo(x, tipY - 0.5);
      bladePath.lineTo(x - bladeHalf * 0.72, shoulderY);
      bladePath.closeSubpath();
      painter.setPen(Qt::NoPen);
      painter.setBrush(color);
      painter.drawPath(bladePath);
    }
  }
}

}  // namespace Artifact::TimelinePlayheadDraw
