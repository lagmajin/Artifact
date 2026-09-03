module;

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QRectF>
#include <QSize>
#include <QString>

#include <algorithm>

module Artifact.Widgets.LayerEditor.ViewportChromeRenderer;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.ViewportChrome;
import Color.Float;
import Layer.Blend;
import Tool;

namespace Artifact {
namespace {

const FloatColor kImpactParentColor{0.30f, 0.86f, 0.58f, 0.88f};
const FloatColor kImpactChildColor{0.24f, 0.66f, 1.0f, 0.82f};
const FloatColor kImpactMatteColor{0.78f, 0.42f, 1.0f, 0.90f};
const FloatColor kImpactDependentColor{1.0f, 0.58f, 0.16f, 0.88f};

bool layerEditorToolEnabled(const LayerEditorViewportChromeState& state,
                            EditMode mode)
{
 switch (mode) {
 case EditMode::View: return state.viewToolEnabled;
 case EditMode::Transform: return state.transformToolEnabled;
 case EditMode::Shape:
 case EditMode::Paint: return state.shapeToolEnabled;
 case EditMode::Mask: return state.maskToolEnabled;
 }
 return false;
}

}

void drawLayerEditorViewportChrome(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    const LayerEditorViewportChromeState& state)
{
 if (!renderer) return;

 const QSize viewportSize = state.viewportSize;
 const float viewportW = static_cast<float>(std::max(1, viewportSize.width()));
 const float viewportH = static_cast<float>(std::max(1, viewportSize.height()));
 const float currentZoom = renderer->getZoom();
 float currentPanX = 0.0f;
 float currentPanY = 0.0f;
 renderer->getPan(currentPanX, currentPanY);

 renderer->setCanvasSize(viewportW, viewportH);
 renderer->setZoom(1.0f);
 renderer->setPan(0.0f, 0.0f);
 renderer->setUseExternalMatrices(false);

 const FloatColor panelFill{0.045f, 0.055f, 0.067f, 0.90f};
 const FloatColor panelStroke{0.22f, 0.25f, 0.29f, 0.96f};
 const FloatColor textColor{0.93f, 0.95f, 0.98f, 1.0f};
 const FloatColor mutedText{0.68f, 0.72f, 0.77f, 1.0f};
 const FloatColor blueAccent{0.18f, 0.55f, 0.94f, 1.0f};
 const FloatColor amberAccent{1.0f, 0.52f, 0.10f, 1.0f};
 const FloatColor selectedFill{0.16f, 0.32f, 0.50f, 0.96f};
 const auto drawChromePanel = [&](float x, float y, float width,
                                  float height, float radius,
                                  const FloatColor& fill,
                                  const FloatColor& stroke) {
  renderer->drawRoundedPanel(
      x + 2.0f, y + 3.0f, width, height, radius,
      FloatColor{0.0f, 0.0f, 0.0f, 0.30f},
      FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
  renderer->drawRoundedPanel(x, y, width, height, radius, fill, stroke);
 };

 QFont uiFont = QApplication::font();
 uiFont.setPixelSize(12);
 QFont compactFont = uiFont;
 compactFont.setPixelSize(11);

 const QRectF surfacePanel = layerEditorSurfaceModeRect(viewportW, viewportH);
 if (!surfacePanel.isEmpty()) {
  const QRectF surfaceItems = layerEditorSurfaceModeItemsRect(viewportW, viewportH);
  const float surfacePanelX = static_cast<float>(surfacePanel.x());
  const float surfacePanelY = static_cast<float>(surfacePanel.y());
  const float surfacePanelW = static_cast<float>(surfacePanel.width());
  const float surfacePanelH = static_cast<float>(surfacePanel.height());
  const float surfaceItemsX = static_cast<float>(surfaceItems.x());
  const float surfaceItemsW = static_cast<float>(surfaceItems.width());
  constexpr float surfaceInset = 4.0f;
  const float surfaceItemW = (surfaceItemsW - surfaceInset * 2.0f) / 3.0f;
  struct SurfaceItem {
   LayerEditorSurfaceMode mode;
   const char* label;
  };
  const SurfaceItem surfaces[] = {
      {LayerEditorSurfaceMode::Edit, "Edit"},
      {LayerEditorSurfaceMode::Inspect, "Inspect"},
      {LayerEditorSurfaceMode::Impact, "Impact"},
  };
  drawChromePanel(surfacePanelX, surfacePanelY,
                  surfacePanelW, surfacePanelH,
                  6.0f, panelFill, panelStroke);
  for (int i = 0; i < 3; ++i) {
   const float itemX = surfacePanelX + surfaceInset +
       surfaceItemW * static_cast<float>(i);
   const bool selected = state.surfaceMode == surfaces[i].mode;
   const bool hovered = state.hoveredControl == 30 + i;
   if (hovered) {
    const FloatColor accent = surfaces[i].mode == LayerEditorSurfaceMode::Impact
        ? amberAccent : blueAccent;
    renderer->drawRoundedPanel(
        itemX, surfacePanelY + surfaceInset,
        surfaceItemW - 2.0f, surfacePanelH - surfaceInset * 2.0f,
        4.0f,
        FloatColor{0.18f, 0.21f, 0.25f, 0.94f},
        selected ? accent : panelStroke);
   }
   const FloatColor itemAccent = surfaces[i].mode == LayerEditorSurfaceMode::Impact
       ? amberAccent : blueAccent;
   renderer->drawText(
       QRectF(itemX + 3.0f, surfacePanelY + 3.0f,
              surfaceItemW - 8.0f, surfacePanelH - 6.0f),
       QString::fromLatin1(surfaces[i].label), compactFont,
       selected ? itemAccent : mutedText, Qt::AlignCenter);
   if (i < 2) {
    const float separatorX = itemX + surfaceItemW - 1.0f;
    renderer->drawSolidLine(
        {separatorX, surfacePanelY + 10.0f},
        {separatorX, surfacePanelY + surfacePanelH - 10.0f},
        FloatColor{0.34f, 0.37f, 0.41f, 0.72f}, 1.0f);
   }
  }
  if (surfacePanelW > surfaceItemsW) {
   const float dividerX = surfaceItemsX + surfaceItemsW + 2.0f;
   renderer->drawSolidLine(
       {dividerX, surfacePanelY + 9.0f},
       {dividerX, surfacePanelY + surfacePanelH - 9.0f},
       FloatColor{0.30f, 0.33f, 0.37f, 0.68f}, 1.0f);
   const bool soloActive = layer && layer->isSolo();
   const QRectF soloRect = layerEditorSurfaceSoloRect(viewportW, viewportH);
   if (state.hoveredControl == 43 && layer && !soloRect.isEmpty()) {
    renderer->drawRoundedPanel(
        static_cast<float>(soloRect.x() + 3.0),
        static_cast<float>(soloRect.y() + 4.0),
        static_cast<float>(soloRect.width() - 6.0),
        static_cast<float>(soloRect.height() - 8.0),
        4.0f,
        FloatColor{0.18f, 0.21f, 0.25f, 0.94f},
        soloActive ? amberAccent : panelStroke);
   }
   renderer->drawText(
       QRectF(dividerX + 10.0f, surfacePanelY + 3.0f,
              48.0f, surfacePanelH - 6.0f),
       layerEditorDisplayModeLabel(state.displayMode), compactFont, textColor,
       Qt::AlignLeft | Qt::AlignVCenter);
   renderer->drawCircle(dividerX + 57.0f,
                         surfacePanelY + surfacePanelH * 0.5f,
                         3.0f,
                         FloatColor{0.10f, 0.88f, 0.48f, 1.0f},
                         1.0f, true);
   renderer->drawText(
       QRectF(dividerX + 73.0f, surfacePanelY + 3.0f,
              40.0f, surfacePanelH - 6.0f),
       QStringLiteral("Solo"), compactFont,
       soloActive ? amberAccent : mutedText,
       Qt::AlignLeft | Qt::AlignVCenter);
   renderer->drawCircle(dividerX + 116.0f,
                         surfacePanelY + surfacePanelH * 0.5f,
                         3.0f,
                         soloActive
                             ? amberAccent
                             : FloatColor{0.46f, 0.49f, 0.54f, 1.0f},
                         1.0f, true);
  }

  if (state.surfaceMode == LayerEditorSurfaceMode::Edit) {
   const QRectF toolPanel = layerEditorEditToolRect(viewportW, viewportH);
   if (!toolPanel.isEmpty()) {
  const float toolPanelX = static_cast<float>(toolPanel.x());
  const float toolPanelY = static_cast<float>(toolPanel.y());
  const float toolPanelW = static_cast<float>(toolPanel.width());
  const float toolPanelH = static_cast<float>(toolPanel.height());
  constexpr float toolInset = 4.0f;
  constexpr float toolItemH = 36.0f;
  struct ToolItem {
   EditMode mode;
   const char* label;
  };
  const ToolItem tools[] = {
      {EditMode::View, "View"},
      {EditMode::Transform, "Move"},
      {EditMode::Shape, "Shape"},
      {EditMode::Mask, "Mask"},
  };
  drawChromePanel(toolPanelX, toolPanelY, toolPanelW, toolPanelH,
                  6.0f, panelFill, panelStroke);
  for (int i = 0; i < 4; ++i) {
   const float itemY = toolPanelY + toolInset +
       toolItemH * static_cast<float>(i);
   const bool selected = state.editMode == tools[i].mode;
   const bool hovered = state.hoveredControl == i;
   const bool enabled = layerEditorToolEnabled(state, tools[i].mode);
   if (selected || (enabled && hovered)) {
    renderer->drawRoundedPanel(toolPanelX + toolInset, itemY,
                                toolPanelW - toolInset * 2.0f,
                                toolItemH - 2.0f,
                                4.0f,
                                selected && enabled
                                    ? selectedFill
                                    : selected
                                        ? FloatColor{0.12f, 0.14f, 0.17f, 0.90f}
                                    : FloatColor{0.18f, 0.21f, 0.25f, 0.94f},
                                selected && enabled
                                    ? blueAccent
                                    : FloatColor{0.34f, 0.37f, 0.41f, 0.88f});
   }
   renderer->drawText(
       QRectF(toolPanelX + toolInset + 3.0f, itemY + 2.0f,
              toolPanelW - toolInset * 2.0f - 6.0f,
              toolItemH - 6.0f),
       QString::fromLatin1(tools[i].label), compactFont,
       !enabled ? FloatColor{0.40f, 0.43f, 0.47f, 0.78f}
                : selected ? textColor : mutedText,
       Qt::AlignCenter);
  }
   }
  } else {
   const float infoPanelW = std::min(
       410.0f, std::max(1.0f, viewportW - 16.0f));
   const float infoPanelX = (viewportW - infoPanelW) * 0.5f;
   const float infoPanelY = surfacePanelY + 96.0f;
   const bool compactInfo = infoPanelW < 320.0f;
   const float infoPanelH = compactInfo ? 70.0f
       : state.surfaceMode == LayerEditorSurfaceMode::Inspect ? 146.0f : 112.0f;
   const FloatColor infoAccent = state.surfaceMode == LayerEditorSurfaceMode::Inspect
       ? blueAccent : amberAccent;
   drawChromePanel(infoPanelX, infoPanelY,
                   infoPanelW, infoPanelH,
                   7.0f, panelFill, infoAccent);
      QFont titleFont = uiFont;
   titleFont.setBold(true);
   const QFontMetrics titleMetrics(titleFont);
   const QString visibleTitle = titleMetrics.elidedText(
       state.surfaceInfoTitle, Qt::ElideMiddle,
       static_cast<int>(infoPanelW - 24.0f));
   renderer->drawText(
       QRectF(infoPanelX + 12.0f, infoPanelY + 7.0f,
              infoPanelW - 24.0f, 22.0f),
       visibleTitle, titleFont, infoAccent,
       Qt::AlignLeft | Qt::AlignVCenter);
   const QString visibleBody = compactInfo
       ? QFontMetrics(compactFont).elidedText(
             state.surfaceInfoBody.section(QStringLiteral("\n"), 0, 0),
             Qt::ElideRight,
             std::max(1, static_cast<int>(infoPanelW - 24.0f)))
       : state.surfaceInfoBody;
   renderer->drawText(
       QRectF(infoPanelX + 12.0f, infoPanelY + 31.0f,
              infoPanelW - 24.0f,
              !compactInfo && state.surfaceMode == LayerEditorSurfaceMode::Impact
                  ? infoPanelH - 59.0f : infoPanelH - 38.0f),
       visibleBody, compactFont, textColor,
       Qt::AlignLeft | Qt::AlignTop);
   if (!compactInfo && state.surfaceMode == LayerEditorSurfaceMode::Impact) {
    struct LinkLegendItem {
     const char* label;
     FloatColor color;
     float width;
    };
    const LinkLegendItem legend[] = {
        {"Parent", kImpactParentColor, 72.0f},
        {"Child", kImpactChildColor, 66.0f},
        {"Matte", kImpactMatteColor, 66.0f},
        {"Used by", kImpactDependentColor, 82.0f},
    };
    float legendX = infoPanelX + 12.0f;
    const float legendY = infoPanelY + infoPanelH - 21.0f;
    for (const auto& item : legend) {
     renderer->drawCircle(legendX + 4.0f, legendY + 8.0f,
                           3.0f, item.color, 1.0f, true);
     renderer->drawText(
         QRectF(legendX + 11.0f, legendY,
                item.width - 11.0f, 16.0f),
         QString::fromLatin1(item.label), compactFont, mutedText,
         Qt::AlignLeft | Qt::AlignVCenter);
     legendX += item.width;
    }
   }
  }
 }

 const QRectF zoomPanel = layerEditorZoomRect(viewportW, viewportH);
 if (!zoomPanel.isEmpty()) {
 const float zoomPanelW = static_cast<float>(zoomPanel.width());
 const float zoomPanelH = static_cast<float>(zoomPanel.height());
 const float zoomPanelX = static_cast<float>(zoomPanel.x());
 const float zoomPanelY = static_cast<float>(zoomPanel.y());
 drawChromePanel(zoomPanelX, zoomPanelY, zoomPanelW, zoomPanelH,
                 6.0f, panelFill, panelStroke);
 if (state.hoveredControl >= 20 && state.hoveredControl <= 23) {
  const int zoomIndex = state.hoveredControl - 20;
  const float segmentX = layerEditorZoomStop(zoomPanelW, zoomIndex);
  const float segmentW = layerEditorZoomStop(zoomPanelW, zoomIndex + 1) -
      segmentX;
  renderer->drawRoundedPanel(zoomPanelX + segmentX + 2.0f,
                              zoomPanelY + 4.0f,
                              segmentW - 4.0f,
                              zoomPanelH - 8.0f,
                              4.0f,
                              FloatColor{0.18f, 0.21f, 0.25f, 0.94f},
                              panelStroke);
 }
 for (int i = 1; i < 4; ++i) {
  const float separatorX = layerEditorZoomStop(zoomPanelW, i);
  renderer->drawSolidLine(
      {zoomPanelX + separatorX, zoomPanelY + 8.0f},
      {zoomPanelX + separatorX, zoomPanelY + zoomPanelH - 8.0f},
      FloatColor{0.30f, 0.33f, 0.37f, 0.72f}, 1.0f);
 }
 const QString zoomLabels[] = {
     QStringLiteral("−"),
     QStringLiteral("%1%").arg(
         QString::number(currentZoom * 100.0f, 'f', 0)),
     QStringLiteral("+"),
     QStringLiteral("Fit")};
 for (int i = 0; i < 4; ++i) {
  const float segmentX = layerEditorZoomStop(zoomPanelW, i);
  const float segmentW = layerEditorZoomStop(zoomPanelW, i + 1) - segmentX;
  renderer->drawText(
      QRectF(zoomPanelX + segmentX, zoomPanelY + 3.0f,
             segmentW, zoomPanelH - 6.0f),
      zoomLabels[i], i == 3 ? compactFont : uiFont,
      i == 1 ? textColor : mutedText, Qt::AlignCenter);
 }
 }

 const QRectF modePanel = layerEditorDisplayModeRect(viewportW, viewportH);
 if (!modePanel.isEmpty()) {
  const float modePanelW = static_cast<float>(modePanel.width());
  const float modePanelH = static_cast<float>(modePanel.height());
  const float modePanelX = static_cast<float>(modePanel.x());
  const float modePanelY = static_cast<float>(modePanel.y());
  struct DisplayItem {
   DisplayMode mode;
   const char* label;
  };
  const DisplayItem displayItems[] = {
      {DisplayMode::Color, "Final"},
      {DisplayMode::Alpha, "Alpha"},
      {DisplayMode::Mask, "Mask"},
      {DisplayMode::Wireframe, "Wire"},
  };
  constexpr float modeInset = 4.0f;
  constexpr float modeItemW = 71.0f;
  drawChromePanel(modePanelX, modePanelY, modePanelW, modePanelH,
                  6.0f, panelFill, panelStroke);
  for (int i = 0; i < 4; ++i) {
   const float itemX = modePanelX + modeInset + modeItemW * static_cast<float>(i);
   const bool selected = state.displayMode == displayItems[i].mode;
   const bool hovered = state.hoveredControl == 10 + i;
   if (selected || hovered) {
    renderer->drawRoundedPanel(itemX, modePanelY + modeInset,
                                modeItemW - 2.0f, modePanelH - modeInset * 2.0f,
                                4.0f,
                                selected
                                    ? selectedFill
                                    : FloatColor{0.18f, 0.21f, 0.25f, 0.94f},
                                selected ? blueAccent : panelStroke);
   }
   renderer->drawText(
       QRectF(itemX + 2.0f, modePanelY + 3.0f,
              modeItemW - 6.0f, modePanelH - 6.0f),
       QString::fromLatin1(displayItems[i].label), compactFont,
       selected ? textColor : mutedText, Qt::AlignCenter);
   if (i == 0 && selected) {
    renderer->drawCircle(itemX + modeItemW - 10.0f,
                          modePanelY + modePanelH * 0.5f,
                          3.0f,
                          FloatColor{0.10f, 0.88f, 0.48f, 1.0f},
                          1.0f, true);
    }
  }
  for (int i = 1; i < 4; ++i) {
   const float separatorX = modePanelX + modeInset +
       modeItemW * static_cast<float>(i) - 1.0f;
   renderer->drawSolidLine(
       {separatorX, modePanelY + 9.0f},
       {separatorX, modePanelY + modePanelH - 9.0f},
       FloatColor{0.30f, 0.33f, 0.37f, 0.68f}, 1.0f);
  }
 }

 const QRectF cubeRect = layerEditorOrientationRect(viewportW, viewportH);
 if (!cubeRect.isEmpty()) {
   const bool layerIs3D = layer && layer->is3D();
   const float cubeSize = static_cast<float>(cubeRect.width());
   const float cubeX = static_cast<float>(cubeRect.x());
   const float cubeY = static_cast<float>(cubeRect.y());
   const float cubeCenterX = cubeX + cubeSize * 0.5f;
   const float cubeCenterY = cubeY + 36.0f;
   const FloatColor axisX{0.96f, 0.28f, 0.22f, 1.0f};
   const FloatColor axisY{0.20f, 0.86f, 0.38f, 1.0f};
   const FloatColor axisZ{0.20f, 0.58f, 1.0f, 1.0f};
   drawChromePanel(cubeX, cubeY, cubeSize, cubeSize,
                   6.0f, panelFill, panelStroke);
   renderer->drawSolidLine(
       {cubeCenterX, cubeY + 18.0f},
       {cubeCenterX, cubeY + 8.0f}, axisY, 1.5f);
   renderer->drawSolidLine(
       {cubeX + cubeSize - 12.0f, cubeCenterY},
       {cubeX + cubeSize - 4.0f, cubeCenterY}, axisX, 1.5f);
   renderer->drawSolidLine(
       {cubeCenterX, cubeY + 54.0f},
       {cubeCenterX, cubeY + 62.0f}, axisZ, 1.5f);
   QFont axisFont = compactFont;
   axisFont.setPixelSize(10);
   axisFont.setBold(true);
   renderer->drawText(QRectF(cubeCenterX - 7.0f, cubeY,
                              14.0f, 13.0f),
                       QStringLiteral("Y"), axisFont, axisY,
                       Qt::AlignCenter);
   renderer->drawText(QRectF(cubeX + cubeSize - 15.0f,
                              cubeCenterY - 7.0f, 14.0f, 14.0f),
                       QStringLiteral("X"), axisFont, axisX,
                       Qt::AlignCenter);
   renderer->drawText(QRectF(cubeCenterX - 7.0f, cubeY + 56.0f,
                              14.0f, 13.0f),
                       QStringLiteral("Z"), axisFont, axisZ,
                       Qt::AlignCenter);
   renderer->drawRoundedPanel(cubeX + 12.0f, cubeY + 18.0f,
                               cubeSize - 24.0f, 36.0f,
                               4.0f,
                               selectedFill,
                               blueAccent);
   renderer->drawText(QRectF(cubeX + 12.0f, cubeY + 18.0f,
                              cubeSize - 24.0f, 36.0f),
                       layerIs3D ? QStringLiteral("Layer")
                                 : QStringLiteral("Front"),
                       compactFont, textColor, Qt::AlignCenter);
   renderer->drawText(QRectF(cubeX + 8.0f, cubeY + 66.0f,
                              cubeSize - 16.0f, 18.0f),
                       layerIs3D ? QStringLiteral("3D")
                                 : QStringLiteral("2D"),
                       compactFont,
                       mutedText, Qt::AlignCenter);
 }

 QString canvasStateTitle;
 QString canvasStateDetail;
 if (!layer) {
  canvasStateTitle = QStringLiteral("Select a layer to inspect");
  canvasStateDetail = QStringLiteral("Select a layer to open it in Layer Solo View");
 } else {
  if (!layer->isVisible()) {
   canvasStateTitle = QStringLiteral("Layer is hidden");
   canvasStateDetail = QStringLiteral("Enable Visible in the layer state controls");
  } else if (!state.layerActive) {
   canvasStateTitle = QStringLiteral("Layer is outside the current frame");
   canvasStateDetail = QStringLiteral("Move the playhead into the layer range");
  } else if (layer->opacity() <= 0.0f) {
   canvasStateTitle = QStringLiteral("Layer is transparent");
   canvasStateDetail = QStringLiteral("Raise opacity above 0% to preview it");
  }
 }
 if (!canvasStateTitle.isEmpty() && viewportW >= 260.0f &&
     viewportH >= 300.0f) {
  const float statePanelW = std::min(340.0f, viewportW - 32.0f);
  constexpr float statePanelH = 66.0f;
  const float statePanelX = (viewportW - statePanelW) * 0.5f;
  const float contentTop = state.surfaceMode == LayerEditorSurfaceMode::Edit
      ? 132.0f
      : state.surfaceMode == LayerEditorSurfaceMode::Inspect ? 320.0f : 288.0f;
  const float contentBottom = viewportH - 76.0f;
  if (contentBottom - contentTop >= statePanelH) {
   const float statePanelY = std::clamp(
       (viewportH - statePanelH) * 0.5f,
       contentTop, contentBottom - statePanelH);
   const FloatColor stateAccent = layer
       ? amberAccent : FloatColor{0.38f, 0.43f, 0.49f, 0.96f};
   drawChromePanel(statePanelX, statePanelY,
                   statePanelW, statePanelH,
                   7.0f, panelFill, stateAccent);
   QFont stateTitleFont = uiFont;
   stateTitleFont.setBold(true);
   renderer->drawText(
       QRectF(statePanelX + 14.0f, statePanelY + 7.0f,
              statePanelW - 28.0f, 23.0f),
       QFontMetrics(stateTitleFont).elidedText(
           canvasStateTitle, Qt::ElideRight,
           std::max(1, static_cast<int>(statePanelW - 28.0f))),
       stateTitleFont, textColor,
       Qt::AlignCenter);
   renderer->drawText(
       QRectF(statePanelX + 14.0f, statePanelY + 32.0f,
              statePanelW - 28.0f, 22.0f),
       QFontMetrics(compactFont).elidedText(
           canvasStateDetail, Qt::ElideRight,
           std::max(1, static_cast<int>(statePanelW - 28.0f))),
       compactFont, mutedText,
       Qt::AlignCenter);
  }
 }

 if (viewportH >= 220.0f) {
 const float bottomY = std::max(68.0f, viewportH - 52.0f);
 const float cardH = 36.0f;
 const float edge = 14.0f;
 const bool compactLayout = viewportW < 980.0f;
 const bool singleCardLayout = viewportW < 560.0f;
 const float leftW = singleCardLayout
     ? std::max(1.0f, viewportW - edge * 2.0f)
     : compactLayout
     ? std::max(180.0f, (viewportW - edge * 2.0f - 12.0f) * 0.5f)
     : std::clamp(viewportW * 0.29f, 260.0f, 410.0f);
 const QRectF stateCard = layerEditorStateCardRect(viewportW, viewportH);
 const float centerW = static_cast<float>(stateCard.width());
 const float rightW = compactLayout
     ? leftW
     : std::clamp(viewportW * 0.31f, 280.0f, 430.0f);

 QString layerName = QStringLiteral("No layer selected");
 QString layerType = QStringLiteral("—");
 QString detailText = QStringLiteral("Opacity: —   |   Blend: —   |   Cache: Idle");
 bool solo = false;
 bool active = false;
 bool cacheEnabled = false;
 bool cacheDirty = false;
 if (layer) {
  layerName = state.layerName;
  layerType = state.layerType;
  solo = layer->isSolo();
  active = state.layerActive;
  cacheEnabled = layer->usesLayerCache();
  cacheDirty = layer->isDirty();
  const QString cacheLabel = !cacheEnabled
      ? QStringLiteral("Off")
      : cacheDirty ? QStringLiteral("Dirty") : QStringLiteral("Ready");
  detailText = QStringLiteral("Opacity: %1%   |   Blend: %2   |   Cache: %3")
      .arg(QString::number(std::clamp(layer->opacity() * 100.0f, 0.0f, 100.0f),
                           'f', 0))
      .arg(ArtifactCore::BlendModeUtils::toString(
          ArtifactCore::toBlendMode(layer->layerBlendType())))
      .arg(cacheLabel);
 }

 const float leftX = edge;
 const float centerX = static_cast<float>(stateCard.x());
 const float rightX = std::max(edge, viewportW - rightW - edge);
 const QFontMetrics compactMetrics(compactFont);
 const QString leftText = compactMetrics.elidedText(
     QStringLiteral("Layer: %1   |   %2   |   %3   |   %4")
         .arg(layerName, layerType,
              solo ? QStringLiteral("Solo") : QStringLiteral("Solo off"),
              active ? QStringLiteral("Active") : QStringLiteral("Inactive")),
     Qt::ElideMiddle, std::max(1, static_cast<int>(leftW - 24.0f)));
 drawChromePanel(leftX, bottomY, leftW, cardH,
                 6.0f, panelFill, panelStroke);
 renderer->drawText(
     QRectF(leftX + 12.0f, bottomY + 3.0f, leftW - 24.0f, cardH - 6.0f),
     leftText,
     compactFont, textColor, Qt::AlignLeft | Qt::AlignVCenter);

 if (!compactLayout && layer) {
  drawChromePanel(centerX, bottomY, centerW, cardH,
                  6.0f, panelFill,
                  solo ? amberAccent : panelStroke);
  const float stateItemW = centerW / 3.0f;
  const QString stateLabels[] = {
      layer && layer->isVisible() ? QStringLiteral("Visible")
                                  : QStringLiteral("Hidden"),
      layer && layer->isLocked() ? QStringLiteral("Locked")
                                 : QStringLiteral("Unlocked"),
      solo ? QStringLiteral("Solo") : QStringLiteral("Solo off")};
  const bool stateActive[] = {
      layer && layer->isVisible(), layer && layer->isLocked(), solo};
  for (int i = 0; i < 3; ++i) {
   const float itemX = centerX + stateItemW * static_cast<float>(i);
   const bool hovered = state.hoveredControl == 40 + i;
   if (stateActive[i] || hovered) {
    const FloatColor accent = i == 0 ? blueAccent : amberAccent;
    renderer->drawRoundedPanel(
        itemX + 3.0f, bottomY + 4.0f,
        stateItemW - 6.0f, cardH - 8.0f, 4.0f,
        stateActive[i] ? FloatColor{0.10f, 0.25f, 0.38f, 0.92f}
                       : FloatColor{0.18f, 0.21f, 0.25f, 0.94f},
        stateActive[i] ? accent : panelStroke);
   }
   renderer->drawText(
       QRectF(itemX + 4.0f, bottomY + 3.0f,
              stateItemW - 8.0f, cardH - 6.0f),
       stateLabels[i], compactFont,
       stateActive[i] ? (i == 0 ? textColor : amberAccent) : mutedText,
       Qt::AlignCenter);
  }
 }

 if (!singleCardLayout) {
  drawChromePanel(rightX, bottomY, rightW, cardH,
                  6.0f, panelFill, panelStroke);
  renderer->drawText(
      QRectF(rightX + 12.0f, bottomY + 3.0f,
             rightW - 30.0f, cardH - 6.0f),
      compactMetrics.elidedText(detailText, Qt::ElideRight,
                                static_cast<int>(rightW - 30.0f)),
      compactFont, textColor, Qt::AlignLeft | Qt::AlignVCenter);
  renderer->drawCircle(rightX + rightW - 14.0f, bottomY + cardH * 0.5f,
                        4.0f,
                        cacheEnabled && !cacheDirty
                            ? FloatColor{0.10f, 0.88f, 0.48f, 1.0f}
                            : cacheEnabled
                                ? amberAccent
                                : FloatColor{0.46f, 0.49f, 0.54f, 1.0f},
                        1.0f, true);
 }
 }

 renderer->setZoom(currentZoom);
 renderer->setPan(currentPanX, currentPanY);
 if (state.restoreCanvasSize.width() > 0 && state.restoreCanvasSize.height() > 0) {
  renderer->setCanvasSize(static_cast<float>(state.restoreCanvasSize.width()),
                          static_cast<float>(state.restoreCanvasSize.height()));
 }
}


}
