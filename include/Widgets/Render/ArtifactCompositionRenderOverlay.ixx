module;

#include <algorithm>
#include <cmath>
#include <vector>

#include <QApplication>
#include <QFont>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QSize>
#include <QVector>

export module Artifact.Widgets.CompositionRenderOverlay;

import Artifact.Render.IRenderer;
import Color.Float;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Widgets.PieMenu;

export namespace Artifact {

void drawCompositionRegionOverlay(ArtifactIRenderer *renderer,
                                  const ArtifactCompositionPtr &comp);

void drawAnchorCenterOverlay(ArtifactIRenderer *renderer,
                             const ArtifactAbstractLayerPtr &layer);

void drawSelectionOverlay(ArtifactIRenderer *renderer,
                          const ArtifactAbstractLayerPtr &layer);

void drawTrackerPinOverlay(ArtifactIRenderer *renderer,
                          float x,
                          float y,
                          float size,
                          const FloatColor &fillColor,
                          const FloatColor &accentColor,
                          bool selected = false,
                          const QString &label = QString(),
                          float opacity = 1.0f);

void drawCameraSelectionOverlay(ArtifactIRenderer *renderer,
                                const ArtifactAbstractLayerPtr &layer,
                                bool isActiveCamera);

void drawSelectionSummaryOverlay(ArtifactIRenderer *renderer,
                                 const ArtifactAbstractLayerPtr &layer,
                                 int overlayW,
                                 int overlayH);

void drawViewportDropGhostOverlay(ArtifactIRenderer *renderer,
                                  const ArtifactCompositionPtr &comp,
                                  float overlayW,
                                  float overlayH,
                                  const QRectF &ghostRect,
                                  const QString &ghostTitle,
                                  const QString &ghostHint,
                                  const QString &dropCandidateLabel);

void drawViewportInfoOverlay(ArtifactIRenderer *renderer,
                             int overlayW,
                             int overlayH,
                             const QString &title,
                             const QString &detail,
                             const QSize *restoreCanvasSize = nullptr);

void drawViewportStatusChipOverlay(ArtifactIRenderer *renderer,
                                   int overlayW,
                                   int overlayH,
                                   const QString &statusText,
                                   const QSize *restoreCanvasSize = nullptr);

void drawPaintLayerOnionSkinOverlay(ArtifactIRenderer *renderer,
                                    const ArtifactAbstractLayerPtr &paintLayer,
                                    const ArtifactCompositionPtr &comp,
                                    float overlayW, float overlayH,
                                    int frameCount, int opacityPercent);
void drawViewportSnapHintOverlay(ArtifactIRenderer *renderer,
                                 int overlayW,
                                 int overlayH,
                                 bool snapBypassed,
                                 const QString &snapTitle,
                                 const QString &snapDetail,
                                 int verticalCount = 0,
                                 int horizontalCount = 0,
                                 const QSize *restoreCanvasSize = nullptr);

void drawViewportCommandPaletteOverlay(ArtifactIRenderer *renderer,
                                       float overlayW,
                                       float overlayH,
                                       const QRectF &panel,
                                       const QString &queryText,
                                       const QStringList &items);

void drawViewportContextMenuOverlay(ArtifactIRenderer *renderer,
                                    float overlayW,
                                    float overlayH,
                                    const QRectF &panel,
                                    const QString &title,
                                    const QString &subtitle,
                                    const QStringList &items,
                                    const QVector<bool> &enabledItems,
                                    int selectedIndex);

void drawViewportPieMenuOverlay(ArtifactIRenderer *renderer,
                                float overlayW,
                                float overlayH,
                                const QRectF &rect,
                                const PieMenuModel &model,
                                int selectedIndex);

export inline void drawViewportCommandPaletteOverlay(ArtifactIRenderer *renderer,
                                                     float overlayWf,
                                                     float overlayHf,
                                                     const QRectF &panel,
                                                     const QString &queryText,
                                                     const QStringList &items)
{
  if (!renderer) {
    return;
  }

  renderer->drawSolidRect(0.0f, 0.0f, overlayWf, overlayHf,
                          FloatColor{0.0f, 0.0f, 0.0f, 0.22f}, 1.0f);
  renderer->drawOverlayPanel(static_cast<float>(panel.left()),
                             static_cast<float>(panel.top()),
                             static_cast<float>(panel.width()),
                             static_cast<float>(panel.height()),
                             FloatColor{0.055f, 0.065f, 0.078f, 0.96f},
                             FloatColor{0.35f, 0.50f, 0.70f, 0.90f});

  QFont titleFont = QApplication::font();
  titleFont.setPointSizeF(std::max(10.0, static_cast<double>(titleFont.pointSizeF()) + 1.0));
  titleFont.setWeight(QFont::DemiBold);
  QFont itemFont = QApplication::font();
  itemFont.setPointSizeF(std::max(9.0, static_cast<double>(itemFont.pointSizeF())));

  renderer->drawText(panel.adjusted(14.0, 8.0, -14.0, -panel.height() + 34.0),
                     QStringLiteral("Command Palette"), titleFont,
                     FloatColor{0.90f, 0.94f, 0.98f, 1.0f},
                     Qt::AlignLeft | Qt::AlignVCenter);
  renderer->drawText(QRectF(panel.left() + 14.0, panel.top() + 30.0,
                            panel.width() - 28.0, 18.0),
                     queryText, itemFont,
                     FloatColor{0.56f, 0.64f, 0.72f, 1.0f},
                     Qt::AlignLeft | Qt::AlignVCenter);

  const int count = std::min(8, static_cast<int>(items.size()));
  for (int i = 0; i < count; ++i) {
    const QRectF row(panel.left() + 10.0, panel.top() + 54.0 + i * 30.0,
                     panel.width() - 20.0, 28.0);
    if (i == 0) {
      renderer->drawSolidRect(static_cast<float>(row.left()),
                              static_cast<float>(row.top()),
                              static_cast<float>(row.width()),
                              static_cast<float>(row.height()),
                              FloatColor{0.16f, 0.28f, 0.44f, 0.86f}, 1.0f);
    }
    renderer->drawText(row.adjusted(10.0, 0.0, -8.0, 0.0), items.at(i),
                       itemFont,
                       FloatColor{0.88f, 0.91f, 0.94f, 1.0f},
                       Qt::AlignLeft | Qt::AlignVCenter);
  }
}

export inline void drawViewportContextMenuOverlay(ArtifactIRenderer *renderer,
                                                  float /*overlayW*/,
                                                  float /*overlayH*/,
                                                  const QRectF &panel,
                                                  const QString &title,
                                                  const QString &subtitle,
                                                  const QStringList &items,
                                                  const QVector<bool> &enabledItems,
                                                  int selectedIndex)
{
  if (!renderer) {
    return;
  }

  const bool hasTitle = !title.trimmed().isEmpty();
  const bool hasSubtitle = !subtitle.trimmed().isEmpty();
  const float headerH = hasTitle ? (hasSubtitle ? 54.0f : 36.0f) : 0.0f;
  renderer->drawOverlayPanel(static_cast<float>(panel.left()),
                             static_cast<float>(panel.top()),
                             static_cast<float>(panel.width()),
                             static_cast<float>(panel.height()),
                             FloatColor{0.060f, 0.068f, 0.078f, 0.97f},
                             FloatColor{0.30f, 0.34f, 0.40f, 0.96f});

  QFont itemFont = QApplication::font();
  itemFont.setPointSizeF(std::max(9.0, static_cast<double>(itemFont.pointSizeF())));
  if (hasTitle) {
    QFont titleFont = itemFont;
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 1.0);
    const QRectF titleRect(panel.left() + 12.0, panel.top() + 8.0,
                           panel.width() - 24.0, hasSubtitle ? 18.0 : 24.0);
    renderer->drawText(titleRect, title, titleFont,
                       FloatColor{0.94f, 0.96f, 0.98f, 1.0f},
                       Qt::AlignLeft | Qt::AlignVCenter);
    if (hasSubtitle) {
      const QRectF subtitleRect(panel.left() + 12.0, panel.top() + 26.0,
                                panel.width() - 24.0, 18.0);
      renderer->drawText(subtitleRect, subtitle, itemFont,
                         FloatColor{0.58f, 0.64f, 0.72f, 1.0f},
                         Qt::AlignLeft | Qt::AlignVCenter);
    }
    renderer->drawSolidRect(static_cast<float>(panel.left() + 10.0f),
                            static_cast<float>(panel.top() + headerH - 2.0f),
                            static_cast<float>(panel.width() - 20.0f), 1.0f,
                            FloatColor{0.20f, 0.24f, 0.29f, 0.9f}, 1.0f);
  }

  for (int i = 0; i < static_cast<int>(items.size()); ++i) {
    const QRectF row(panel.left() + 10.0, panel.top() + 12.0 + headerH + i * 28.0,
                     panel.width() - 20.0, 28.0);
    const bool enabled =
        i < static_cast<int>(enabledItems.size()) ? enabledItems.at(i) : true;
    if (items.at(i).trimmed().isEmpty()) {
      const float y = static_cast<float>(row.center().y());
      renderer->drawSolidRect(static_cast<float>(row.left() + 10.0f), y,
                              static_cast<float>(row.width() - 20.0f), 1.0f,
                              FloatColor{0.20f, 0.24f, 0.29f, 0.95f}, 1.0f);
      continue;
    }
    if (i == selectedIndex) {
      renderer->drawSolidRect(static_cast<float>(row.left()),
                              static_cast<float>(row.top()),
                              static_cast<float>(row.width()),
                              static_cast<float>(row.height()),
                              enabled ? FloatColor{0.15f, 0.22f, 0.31f, 0.80f}
                                      : FloatColor{0.12f, 0.16f, 0.22f, 0.62f},
                              1.0f);
    }
    renderer->drawText(row.adjusted(10.0, 0.0, -8.0, 0.0), items.at(i),
                       itemFont,
                       enabled ? FloatColor{0.88f, 0.90f, 0.92f, 1.0f}
                               : FloatColor{0.52f, 0.56f, 0.62f, 1.0f},
                       Qt::AlignLeft | Qt::AlignVCenter);
  }
}

export inline void drawViewportPieMenuOverlay(ArtifactIRenderer *renderer,
                                              float overlayWf,
                                              float overlayHf,
                                              const QRectF &rect,
                                              const PieMenuModel &model,
                                              int selectedIndex)
{
  if (!renderer || model.items.empty()) {
    return;
  }

  const float prevZoom = renderer->getZoom();
  float prevPanX = 0.0f;
  float prevPanY = 0.0f;
  renderer->getPan(prevPanX, prevPanY);
  renderer->setUseExternalMatrices(false);
  renderer->setCanvasSize(overlayWf, overlayHf);
  renderer->setZoom(1.0f);
  renderer->setPan(0.0f, 0.0f);

  const QPointF center = rect.center();
  const float outerRadius = rect.width() * 0.48f;
  const float innerRadius = rect.width() * 0.19f;
  const int count = static_cast<int>(model.items.size());
  const float sectorSize = 360.0f / static_cast<float>(std::max(1, count));
  renderer->drawSolidRect(0.0f, 0.0f, overlayWf, overlayHf,
                          FloatColor{0.0f, 0.0f, 0.0f, 0.16f}, 1.0f);
  renderer->drawCircle(static_cast<float>(center.x()),
                       static_cast<float>(center.y()), outerRadius + 8.0f,
                       FloatColor{0.08f, 0.10f, 0.13f, 0.94f}, 1.0f, true);
  renderer->drawCircle(static_cast<float>(center.x()),
                       static_cast<float>(center.y()), innerRadius - 2.0f,
                       FloatColor{0.05f, 0.06f, 0.08f, 0.98f}, 1.0f, true);

  QFont labelFont = QApplication::font();
  labelFont.setPointSizeF(std::max(9.0, static_cast<double>(labelFont.pointSizeF())));
  labelFont.setWeight(QFont::DemiBold);
  QFont titleFont = QApplication::font();
  titleFont.setPointSizeF(std::max(10.0, static_cast<double>(titleFont.pointSizeF()) + 1.0));
  titleFont.setWeight(QFont::DemiBold);

  for (int i = 0; i < count; ++i) {
    const auto &item = model.items[static_cast<size_t>(i)];
    const float startAngle = 90.0f - (i + 1) * sectorSize + sectorSize * 0.5f;
    const float endAngle = startAngle + sectorSize;
    const int steps = 10;
    std::vector<Detail::float2> polygon;
    polygon.reserve(static_cast<size_t>(steps + 3));
    polygon.push_back({static_cast<float>(center.x()),
                       static_cast<float>(center.y())});
    for (int s = 0; s <= steps; ++s) {
      const float t = static_cast<float>(s) / static_cast<float>(steps);
      const float ang =
          (startAngle + (endAngle - startAngle) * t) * static_cast<float>(M_PI) /
          180.0f;
      polygon.push_back({
          static_cast<float>(center.x() + std::cos(ang) * outerRadius),
          static_cast<float>(center.y() - std::sin(ang) * outerRadius)});
    }
    const bool selected = (i == selectedIndex);
    renderer->drawSolidPolygonLocal(
        polygon, selected ? FloatColor{0.18f, 0.34f, 0.52f, 0.95f}
                          : FloatColor{0.10f, 0.12f, 0.15f, 0.88f});

    std::vector<Detail::float2> innerEdge;
    innerEdge.reserve(static_cast<size_t>(steps + 3));
    for (int s = 0; s <= steps; ++s) {
      const float t = static_cast<float>(s) / static_cast<float>(steps);
      const float ang =
          (startAngle + (endAngle - startAngle) * t) * static_cast<float>(M_PI) /
          180.0f;
      innerEdge.push_back({
          static_cast<float>(center.x() + std::cos(ang) * innerRadius),
          static_cast<float>(center.y() - std::sin(ang) * innerRadius)});
    }
    renderer->drawSolidPolygonLocal(innerEdge,
                                    FloatColor{0.04f, 0.05f, 0.07f, 0.98f});

    const float midAngle =
        (startAngle + sectorSize * 0.5f) * static_cast<float>(M_PI) / 180.0f;
    const float labelRadius = (innerRadius + outerRadius) * 0.5f;
    const QPointF labelPos(center.x() + std::cos(midAngle) * labelRadius,
                           center.y() - std::sin(midAngle) * labelRadius);
    const QRectF textRect(labelPos.x() - sectorSize * 1.0f,
                          labelPos.y() - 14.0f, sectorSize * 2.0f, 28.0f);
    renderer->drawText(textRect, item.label, labelFont,
                       item.enabled ? FloatColor{0.92f, 0.95f, 0.98f, 1.0f}
                                    : FloatColor{0.55f, 0.58f, 0.62f, 1.0f},
                       Qt::AlignCenter);
  }

  renderer->drawCircle(static_cast<float>(center.x()),
                       static_cast<float>(center.y()), innerRadius - 4.0f,
                       FloatColor{0.03f, 0.04f, 0.06f, 1.0f}, 1.0f, true);
  renderer->drawText(QRectF(center.x() - innerRadius, center.y() - innerRadius,
                            innerRadius * 2.0f, innerRadius * 2.0f),
                     model.title.isEmpty() ? QStringLiteral("Menu")
                                           : model.title,
                     titleFont, FloatColor{0.95f, 0.97f, 0.99f, 1.0f},
                     Qt::AlignCenter);

  renderer->setZoom(prevZoom);
  renderer->setPan(prevPanX, prevPanY);
}

} // namespace Artifact
