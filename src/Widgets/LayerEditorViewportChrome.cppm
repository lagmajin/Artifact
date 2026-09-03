module;

#include <QRectF>
#include <QPointF>
#include <QSize>
#include <QString>

#include <algorithm>

module Artifact.Widgets.LayerEditor.ViewportChrome;

import Tool;
import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Memory.SharedPtr;

namespace Artifact {

QString layerEditorEditModeLabel(EditMode mode)
{
 switch (mode) {
 case EditMode::Transform: return QStringLiteral("Move");
 case EditMode::Mask: return QStringLiteral("Mask");
 case EditMode::Paint: return QStringLiteral("Paint");
 case EditMode::Shape: return QStringLiteral("Shape");
 case EditMode::View:
 default: return QStringLiteral("View");
 }
}

QString layerEditorDisplayModeLabel(DisplayMode mode)
{
 switch (mode) {
 case DisplayMode::Mask: return QStringLiteral("Mask");
 case DisplayMode::Alpha: return QStringLiteral("Alpha");
 case DisplayMode::Wireframe: return QStringLiteral("Wireframe");
 case DisplayMode::Color:
 default: return QStringLiteral("Final");
 }
}

bool layerEditorEditModeAvailable(const ArtifactAbstractLayerPtr& layer,
                                  EditMode mode)
{
 if (mode == EditMode::View) return true;
 if (!layer || !layer->isVisible() || layer->isLocked()) return false;
 if (mode == EditMode::Shape || mode == EditMode::Paint) {
  return static_cast<bool>(ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
      ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer)));
 }
 if (mode == EditMode::Mask) return layer->maskCount() > 0;
 return true;
}

QRectF layerEditorSurfaceModeRect(float viewportWidth, float viewportHeight)
{
 if (viewportWidth < 220.0f || viewportHeight < 112.0f) return {};
 const float width = viewportWidth >= 980.0f ? 430.0f
     : viewportWidth >= 720.0f ? 284.0f : std::max(1.0f, viewportWidth - 16.0f);
 const float x = viewportWidth >= 720.0f ? (viewportWidth - width) * 0.5f : 8.0f;
 return QRectF(x, 64.0f, width, 38.0f);
}

QRectF layerEditorSurfaceModeItemsRect(float viewportWidth, float viewportHeight)
{
 const QRectF panel = layerEditorSurfaceModeRect(viewportWidth, viewportHeight);
 if (panel.isEmpty() || panel.width() <= 284.0) return panel;
 return QRectF(panel.x(), panel.y(), 284.0, panel.height());
}

QRectF layerEditorSurfaceSoloRect(float viewportWidth, float viewportHeight)
{
 const QRectF panel = layerEditorSurfaceModeRect(viewportWidth, viewportHeight);
 if (panel.isEmpty() || panel.width() <= 284.0) return {};
 return QRectF(panel.right() - 78.0, panel.y(), 78.0, panel.height());
}

QRectF layerEditorEditToolRect(float viewportWidth, float viewportHeight)
{
 return viewportWidth >= 1000.0f && viewportHeight >= 300.0f
     ? QRectF(16.0f, 64.0f, 64.0f, 152.0f) : QRectF{};
}

QRectF layerEditorDisplayModeRect(float viewportWidth, float viewportHeight)
{
 return viewportWidth >= 860.0f && viewportHeight >= 160.0f
     ? QRectF(viewportWidth - 308.0f, 112.0f, 292.0f, 38.0f) : QRectF{};
}

QRectF layerEditorOrientationRect(float viewportWidth, float viewportHeight)
{
 return viewportWidth >= 860.0f && viewportHeight >= 112.0f
     ? QRectF(viewportWidth - 102.0f, 16.0f, 86.0f, 86.0f) : QRectF{};
}

QRectF layerEditorZoomRect(float viewportWidth, float viewportHeight)
{
 if (viewportWidth < 190.0f || viewportHeight < 64.0f) return {};
 const float width = viewportWidth >= 720.0f ? 216.0f
     : std::min(174.0f, viewportWidth - 16.0f);
 return QRectF((viewportWidth - width) * 0.5f, 16.0f, width, 38.0f);
}

float layerEditorZoomStop(float panelWidth, int stop)
{
 switch (stop) {
 case 1: return panelWidth * 0.24f;
 case 2: return panelWidth * 0.61f;
 case 3: return panelWidth * 0.805f;
 case 4: return panelWidth;
 default: return 0.0f;
 }
}

int layerEditorZoomControlIndex(float panelWidth, float relativeX)
{
 if (relativeX < layerEditorZoomStop(panelWidth, 1)) return 0;
 if (relativeX < layerEditorZoomStop(panelWidth, 2)) return 1;
 if (relativeX < layerEditorZoomStop(panelWidth, 3)) return 2;
 return 3;
}

QRectF layerEditorStateCardRect(float viewportWidth, float viewportHeight)
{
 if (viewportWidth < 980.0f || viewportHeight < 220.0f) return {};
 const float width = std::clamp(viewportWidth * 0.22f, 220.0f, 310.0f);
 return QRectF((viewportWidth - width) * 0.5f,
               std::max(68.0f, viewportHeight - 52.0f), width, 36.0f);
}

QString layerEditorChromeToolTip(int control)
{
 switch (control) {
 case 0: return QStringLiteral("View mode");
 case 1: return QStringLiteral("Transform layer");
 case 2: return QStringLiteral("Edit shape path");
 case 3: return QStringLiteral("Edit layer mask");
 case 10: return QStringLiteral("Show final color (Alt+C)");
 case 11: return QStringLiteral("Show alpha channel (Alt+A)");
 case 12: return QStringLiteral("Show mask overlay (Alt+M)");
 case 13: return QStringLiteral("Show wireframe (Alt+W)");
 case 20: return QStringLiteral("Zoom out (−)");
 case 21: return QStringLiteral("Reset zoom to 100% (1)");
 case 22: return QStringLiteral("Zoom in (+)");
 case 23: return QStringLiteral("Fit layer to viewport (F)");
 case 30: return QStringLiteral("Edit surface (Alt+1)");
 case 31: return QStringLiteral("Inspect layer values (Alt+2)");
 case 32: return QStringLiteral("Inspect layer relationships (Alt+3)");
 case 40: return QStringLiteral("Toggle layer visibility (Alt+V)");
 case 41: return QStringLiteral("Toggle layer lock (Alt+L)");
 case 42:
 case 43: return QStringLiteral("Toggle layer solo (Alt+S)");
 default: return {};
 }
}

int layerEditorChromeControlAt(const QPointF& position,
                               const QSize& viewportSize,
                               LayerEditorSurfaceMode surfaceMode,
                               bool hasLayer)
{
 const float width = static_cast<float>(viewportSize.width());
 const float height = static_cast<float>(viewportSize.height());
 const QRectF surface = layerEditorSurfaceModeRect(width, height);
 const QRectF surfaceItems = layerEditorSurfaceModeItemsRect(width, height);
 if (!surface.isEmpty()) {
  if (hasLayer && layerEditorSurfaceSoloRect(width, height).contains(position)) return 43;
  if (surfaceItems.contains(position)) {
   const float itemWidth = static_cast<float>((surfaceItems.width() - 8.0) / 3.0);
   return 30 + std::clamp(
       static_cast<int>((position.x() - surfaceItems.left() - 4.0) / itemWidth), 0, 2);
  }
  const QRectF tools = layerEditorEditToolRect(width, height);
  if (surfaceMode == LayerEditorSurfaceMode::Edit && tools.contains(position))
   return std::clamp(static_cast<int>((position.y() - tools.top() - 4.0) / 36.0), 0, 3);
 }
 const QRectF display = layerEditorDisplayModeRect(width, height);
 if (display.contains(position))
  return 10 + std::clamp(
      static_cast<int>((position.x() - display.left() - 4.0) / 71.0), 0, 3);
 const QRectF state = layerEditorStateCardRect(width, height);
 if (hasLayer && state.contains(position))
  return 40 + std::clamp(
      static_cast<int>((position.x() - state.left()) / (state.width() / 3.0)), 0, 2);
 const QRectF zoom = layerEditorZoomRect(width, height);
 if (zoom.contains(position))
  return 20 + layerEditorZoomControlIndex(
      static_cast<float>(zoom.width()), static_cast<float>(position.x() - zoom.left()));
 return -1;
}

}
