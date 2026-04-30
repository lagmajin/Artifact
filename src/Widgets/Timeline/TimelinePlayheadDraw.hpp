#pragma once

#include <algorithm>
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace Artifact::TimelinePlayheadDraw {

inline QColor playheadColor() {
  return QColor(255, 92, 92);
}

inline void drawPlayhead(QPainter& painter, const qreal x, const qreal stemTop,
                         const qreal stemBottom, const bool drawHead = true,
                         const qreal headTop = 0.0,
                         const qreal maxHeadHeight = 10.0,
                         const qreal headWidth = 12.0) {
  const QColor color = playheadColor();
  painter.setRenderHint(QPainter::Antialiasing, true);

  qreal lineTop = stemTop;
  if (drawHead) {
    const qreal availableHeight = std::max<qreal>(0.0, stemBottom - headTop);
    const qreal headHeight =
        std::min<qreal>(maxHeadHeight, std::max<qreal>(0.0, availableHeight - 1.0));
    if (headHeight >= 3.0) {
      QPainterPath headPath;
      headPath.moveTo(x, headTop + headHeight);
      headPath.lineTo(x - headWidth * 0.5, headTop);
      headPath.lineTo(x + headWidth * 0.5, headTop);
      headPath.closeSubpath();
      painter.setPen(QPen(QColor(18, 18, 18, 150), 1));
      painter.setBrush(color);
      painter.drawPath(headPath);
      lineTop = headTop + headHeight + 1.0;
    }
  }

  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(color, 2, Qt::SolidLine, Qt::FlatCap));
  painter.drawLine(QPointF(x, lineTop), QPointF(x, stemBottom));
}

}  // namespace Artifact::TimelinePlayheadDraw
