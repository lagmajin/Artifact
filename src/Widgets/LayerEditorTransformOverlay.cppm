module;

#include <QApplication>
#include <QFont>
#include <QRectF>
#include <QSize>
#include <QString>

#include <algorithm>
#include <cmath>

module Artifact.Widgets.LayerEditor.TransformOverlay;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.ViewportChrome;
import Color.Float;

namespace Artifact {

void drawLayerEditorTransformHud(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    const QRectF& activeBounds, const QSize& viewportSize,
    const QSize& restoreCanvasSize)
{
 if (!renderer || !layer || !activeBounds.isValid() || activeBounds.isEmpty()) return;
 const Detail::float2 bottomRight = renderer->canvasToViewport(
     {static_cast<float>(activeBounds.right()), static_cast<float>(activeBounds.bottom())});
 if (!std::isfinite(bottomRight.x) || !std::isfinite(bottomRight.y)) return;

 const QString text = QStringLiteral("X %1    Y %2\nW %3    H %4")
     .arg(QString::number(activeBounds.x(), 'f', 0))
     .arg(QString::number(activeBounds.y(), 'f', 0))
     .arg(QString::number(activeBounds.width(), 'f', 0))
     .arg(QString::number(activeBounds.height(), 'f', 0));
 const float viewportWidth = static_cast<float>(std::max(1, viewportSize.width()));
 const float viewportHeight = static_cast<float>(std::max(1, viewportSize.height()));
 constexpr float width = 132.0f;
 constexpr float height = 54.0f;
 constexpr float gap = 8.0f;
 if (viewportWidth < width + gap * 2.0f || viewportHeight < height + gap * 2.0f) return;
 float x = bottomRight.x + gap;
 float y = bottomRight.y + gap;
 if (x + width > viewportWidth - gap) x = bottomRight.x - width - gap;
 if (y + height > viewportHeight - gap) y = bottomRight.y - height - gap;
 x = std::clamp(x, gap, std::max(gap, viewportWidth - width - gap));
 y = std::clamp(y, gap, std::max(gap, viewportHeight - height - gap));

 const QRectF reserved[] = {
     layerEditorZoomRect(viewportWidth, viewportHeight),
     layerEditorSurfaceModeRect(viewportWidth, viewportHeight),
     layerEditorEditToolRect(viewportWidth, viewportHeight),
     layerEditorDisplayModeRect(viewportWidth, viewportHeight),
     layerEditorOrientationRect(viewportWidth, viewportHeight),
     viewportHeight >= 220.0f
         ? QRectF(0.0f, std::max(0.0f, viewportHeight - 60.0f), viewportWidth, 60.0f)
         : QRectF{}};
 for (int pass = 0; pass < 2; ++pass) {
  for (const QRectF& area : reserved) {
   if (area.isEmpty() || !QRectF(x, y, width, height).intersects(area)) continue;
   const float below = static_cast<float>(area.bottom()) + gap;
   const float above = static_cast<float>(area.top()) - height - gap;
   if (below + height <= viewportHeight - 60.0f - gap) y = below;
   else if (above >= gap) y = above;
  }
 }
 y = std::clamp(y, gap, viewportHeight - height - gap);

 const float savedZoom = renderer->getZoom();
 float savedPanX = 0.0f;
 float savedPanY = 0.0f;
 renderer->getPan(savedPanX, savedPanY);
 renderer->setCanvasSize(viewportWidth, viewportHeight);
 renderer->setZoom(1.0f);
 renderer->setPan(0.0f, 0.0f);
 renderer->setUseExternalMatrices(false);
 renderer->drawRoundedPanel(x + 2.0f, y + 3.0f, width, height, 6.0f,
                            {0.0f, 0.0f, 0.0f, 0.30f}, {0.0f, 0.0f, 0.0f, 0.0f});
 renderer->drawRoundedPanel(x, y, width, height, 6.0f,
                            {0.045f, 0.055f, 0.067f, 0.90f},
                            {0.18f, 0.55f, 0.94f, 0.96f});
 QFont font = QApplication::font();
 font.setPixelSize(12);
 renderer->drawText(QRectF(x + 10.0f, y + 5.0f, width - 20.0f, height - 10.0f),
                    text, font, {0.93f, 0.95f, 0.98f, 1.0f},
                    Qt::AlignLeft | Qt::AlignVCenter, 1.0f);
 renderer->setZoom(savedZoom);
 renderer->setPan(savedPanX, savedPanY);
 if (restoreCanvasSize.width() > 0 && restoreCanvasSize.height() > 0)
  renderer->setCanvasSize(static_cast<float>(restoreCanvasSize.width()),
                          static_cast<float>(restoreCanvasSize.height()));
}

}
