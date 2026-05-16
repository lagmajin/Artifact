module;

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QPainter>
#include <QPen>
#include <QString>
#include <QStringList>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <memory>
#include <QVector>

module Artifact.Widgets.CompositionRenderOverlay;

import Color.Float;
import Artifact.Layer.Camera;
import Artifact.Layer.Shape;
import Layer.Blend;
import Artifact.Widgets.PieMenu;
import ArtifactCore.Utils.PerformanceProfiler;
import Artifact.Render.IRenderer;

namespace Artifact {
using namespace ArtifactCore;

namespace {

QString blendModeName(const ArtifactCore::BlendMode mode)
{
  switch (mode) {
  case ArtifactCore::BlendMode::Normal:
    return QStringLiteral("Normal");
  case ArtifactCore::BlendMode::Add:
    return QStringLiteral("Add");
  case ArtifactCore::BlendMode::Multiply:
    return QStringLiteral("Multiply");
  case ArtifactCore::BlendMode::Screen:
    return QStringLiteral("Screen");
  case ArtifactCore::BlendMode::Overlay:
    return QStringLiteral("Overlay");
  case ArtifactCore::BlendMode::Darken:
    return QStringLiteral("Darken");
  case ArtifactCore::BlendMode::Lighten:
    return QStringLiteral("Lighten");
  case ArtifactCore::BlendMode::ColorDodge:
    return QStringLiteral("ColorDodge");
  case ArtifactCore::BlendMode::ColorBurn:
    return QStringLiteral("ColorBurn");
  case ArtifactCore::BlendMode::HardLight:
    return QStringLiteral("HardLight");
  case ArtifactCore::BlendMode::SoftLight:
    return QStringLiteral("SoftLight");
  case ArtifactCore::BlendMode::Difference:
    return QStringLiteral("Difference");
  case ArtifactCore::BlendMode::Exclusion:
    return QStringLiteral("Exclusion");
  default:
    return QStringLiteral("Normal");
  }
}

QString layerOverlayDetailText(const ArtifactAbstractLayerPtr &layer)
{
  if (!layer) {
    return QString();
  }

  const QRectF bounds = layer->transformedBoundingBox();
  const QSizeF boundsSize = bounds.size();
  const QString typeLabel =
      layer->is3D() ? QStringLiteral("3D") : layer->className().toQString();
  const QString visibility = layer->isVisible() ? QStringLiteral("V")
                                                : QStringLiteral("H");
  const QString locked = layer->isLocked() ? QStringLiteral("L")
                                           : QStringLiteral("-");
  const QString maskText = layer->maskCount() > 0
                               ? QStringLiteral("%1 mask%2")
                                     .arg(layer->maskCount())
                                     .arg(layer->maskCount() == 1 ? QString()
                                                                 : QStringLiteral("s"))
                               : QStringLiteral("no masks");
  return QStringLiteral("%1 | %2 | O%3 | %4%5 | %6 | %7x%8")
      .arg(typeLabel)
      .arg(blendModeName(ArtifactCore::toBlendMode(layer->layerBlendType())))
      .arg(QString::number(std::clamp(layer->opacity() * 100.0f, 0.0f, 100.0f),
                           'f', 0))
      .arg(visibility)
      .arg(locked)
      .arg(maskText)
      .arg(QString::number(std::max(0.0, boundsSize.width()), 'f', 0))
      .arg(QString::number(std::max(0.0, boundsSize.height()), 'f', 0));
}

void drawLabelBox(QPainter &p, const QRectF &boxRect, const QColor &fill,
                  const QColor &border, const QString &title,
                  const QString &subtitle)
{
  if (!boxRect.isValid()) {
    return;
  }

  const QRectF outer = boxRect.normalized();
  const QRectF inner = outer.adjusted(6.0, 6.0, -6.0, -6.0);
  p.setPen(QPen(border, 2.0, Qt::DashLine));
  p.setBrush(fill);
  p.drawRoundedRect(outer, 8.0, 8.0);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(255, 255, 255, 18));
  p.drawRoundedRect(inner, 6.0, 6.0);

  const QFontMetrics fm(p.font());
  const int innerWidth = std::max(10, static_cast<int>(inner.width()) - 20);
  const QRect titleRect(static_cast<int>(inner.left()) + 10,
                        static_cast<int>(inner.top()) + 8, innerWidth,
                        fm.height() + 2);
  const QRect hintRect(static_cast<int>(inner.left()) + 10,
                       static_cast<int>(inner.top()) + 8 + fm.height() + 4,
                       innerWidth, fm.height() + 2);
  p.setPen(QColor(235, 245, 255));
  p.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
             fm.elidedText(title, Qt::ElideRight, titleRect.width()));
  p.setPen(QColor(180, 195, 210));
  p.drawText(hintRect, Qt::AlignLeft | Qt::AlignVCenter,
             fm.elidedText(subtitle, Qt::ElideRight, hintRect.width()));
}

void presentOverlayImage(ArtifactIRenderer *renderer, const QImage &overlayImage,
                         const QSize *restoreCanvasSize)
{
  if (!renderer) {
    return;
  }

  const float drawW = static_cast<float>(overlayImage.width());
  const float drawH = static_cast<float>(overlayImage.height());
  const auto prevZoom = renderer->getZoom();
  float prevPanX = 0.0f;
  float prevPanY = 0.0f;
  renderer->getPan(prevPanX, prevPanY);
  renderer->setCanvasSize(drawW, drawH);
  renderer->setZoom(1.0f);
  renderer->setPan(0.0f, 0.0f);
  renderer->drawSprite(0.0f, 0.0f, drawW, drawH, overlayImage, 1.0f);
  renderer->setZoom(prevZoom);
  renderer->setPan(prevPanX, prevPanY);

  if (restoreCanvasSize) {
    renderer->setCanvasSize(static_cast<float>(restoreCanvasSize->width()),
                           static_cast<float>(restoreCanvasSize->height()));
  }
}

} // namespace

#if 0
__declspec(dllexport) void drawViewportCommandPaletteOverlay(ArtifactIRenderer *renderer,
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

__declspec(dllexport) void drawViewportContextMenuOverlay(ArtifactIRenderer *renderer,
                                   float /*overlayW*/,
                                   float /*overlayH*/,
                                   const QRectF &panel,
                                   const QString &title,
                                   const QString &subtitle,
                                   const QStringList &items,
                                   const QVector<bool> &enabledItems)
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
    if (i == 0 && enabled) {
      renderer->drawSolidRect(static_cast<float>(row.left()),
                              static_cast<float>(row.top()),
                              static_cast<float>(row.width()),
                              static_cast<float>(row.height()),
                              FloatColor{0.15f, 0.22f, 0.31f, 0.80f}, 1.0f);
    }
    renderer->drawText(row.adjusted(10.0, 0.0, -8.0, 0.0), items.at(i),
                       itemFont,
                       enabled ? FloatColor{0.88f, 0.90f, 0.92f, 1.0f}
                               : FloatColor{0.52f, 0.56f, 0.62f, 1.0f},
                       Qt::AlignLeft | Qt::AlignVCenter);
  }
}

__declspec(dllexport) void drawViewportPieMenuOverlay(ArtifactIRenderer *renderer,
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

} // namespace

#endif

void drawCompositionRegionOverlay(ArtifactIRenderer *renderer,
                                  const ArtifactCompositionPtr &comp)
{
  if (!renderer || !comp) {
    return;
  }

  const QSize compSize = comp->settings().compositionSize();
  const float cw =
      static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);
  const float ch =
      static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);
  if (cw <= 0.0f || ch <= 0.0f) {
    return;
  }

  const FloatColor darkColor{0.02f, 0.02f, 0.02f, 0.85f};
  const FloatColor lightColor{0.42f, 0.68f, 0.96f, 0.95f};
  renderer->drawSolidLine({0.0f, 0.0f}, {cw, 0.0f}, darkColor, 1.0f);
  renderer->drawSolidLine({cw, 0.0f}, {cw, ch}, darkColor, 1.0f);
  renderer->drawSolidLine({cw, ch}, {0.0f, ch}, darkColor, 1.0f);
  renderer->drawSolidLine({0.0f, ch}, {0.0f, 0.0f}, darkColor, 1.0f);
  renderer->drawSolidLine({0.0f, 0.0f}, {cw, 0.0f}, lightColor, 1.0f);
  renderer->drawSolidLine({cw, 0.0f}, {cw, ch}, lightColor, 1.0f);
  renderer->drawSolidLine({cw, ch}, {0.0f, ch}, lightColor, 1.0f);
  renderer->drawSolidLine({0.0f, ch}, {0.0f, 0.0f}, lightColor, 1.0f);
}

void drawAnchorCenterOverlay(ArtifactIRenderer *renderer,
                             const ArtifactAbstractLayerPtr &layer)
{
  if (!renderer || !layer) {
    return;
  }

  const QRectF localBounds = layer->localBounds();
  if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
      localBounds.height() <= 0.0) {
    return;
  }

  const QTransform globalTransform = layer->getGlobalTransform();
  const auto &t3d = layer->transform3D();
  const QPointF anchorLocal(t3d.anchorX(), t3d.anchorY());
  const QPointF centerLocal = localBounds.center();
  const QPointF anchorCanvas = globalTransform.map(anchorLocal);
  const QPointF centerCanvas = globalTransform.map(centerLocal);
  const float zoom = std::max(0.001f, renderer->getZoom());
  const float invZoom = 1.0f / zoom;
  const float pointSize = std::max(7.0f, 11.0f / zoom);
  const float crossSize = std::max(12.0f, 20.0f / zoom);
  const float lineWidth = std::max(1.5f, 2.4f / zoom);
  const FloatColor shadowColor{0.0f, 0.0f, 0.0f, 0.68f};
  const FloatColor anchorColor{1.0f, 0.72f, 0.20f, 0.99f};
  const FloatColor centerColor{0.22f, 0.86f, 1.0f, 0.99f};
  const FloatColor linkColor{0.94f, 0.95f, 0.99f, 0.82f};

  renderer->drawSolidLine({static_cast<float>(anchorCanvas.x()), static_cast<float>(anchorCanvas.y())},
                          {static_cast<float>(centerCanvas.x()), static_cast<float>(centerCanvas.y())},
                          shadowColor, lineWidth * 2.0f);
  renderer->drawSolidLine({static_cast<float>(anchorCanvas.x()), static_cast<float>(anchorCanvas.y())},
                          {static_cast<float>(centerCanvas.x()), static_cast<float>(centerCanvas.y())},
                          linkColor, lineWidth);

  renderer->drawPoint(static_cast<float>(anchorCanvas.x()),
                      static_cast<float>(anchorCanvas.y()), pointSize * 1.35f,
                      shadowColor);
  renderer->drawPoint(static_cast<float>(anchorCanvas.x()),
                      static_cast<float>(anchorCanvas.y()), pointSize,
                      anchorColor);
  renderer->drawCrosshair(static_cast<float>(anchorCanvas.x()),
                          static_cast<float>(anchorCanvas.y()), crossSize,
                          anchorColor);

  renderer->drawPoint(static_cast<float>(centerCanvas.x()),
                      static_cast<float>(centerCanvas.y()), pointSize * 1.15f,
                      shadowColor);
  renderer->drawPoint(static_cast<float>(centerCanvas.x()),
                      static_cast<float>(centerCanvas.y()), pointSize * 0.82f,
                      centerColor);
  renderer->drawCrosshair(static_cast<float>(centerCanvas.x()),
                          static_cast<float>(centerCanvas.y()), crossSize,
                          centerColor);

  QFont titleFont = QApplication::font();
  titleFont.setPointSizeF(std::max(10.0, static_cast<double>(titleFont.pointSizeF()) + 1.0));
  titleFont.setWeight(QFont::DemiBold);
  QFont detailFont = QApplication::font();
  detailFont.setPointSizeF(std::max(8.5, static_cast<double>(detailFont.pointSizeF())));
  const QFontMetrics titleFm(titleFont);
  const QFontMetrics detailFm(detailFont);

  const QString titleText = QStringLiteral("Anchor / Center");
  const QString boundsText = QStringLiteral("Layer Bounds");
  const QString anchorText = QStringLiteral("Anchor  %1 , %2")
                                 .arg(QString::number(anchorCanvas.x(), 'f', 1),
                                      QString::number(anchorCanvas.y(), 'f', 1));
  const QString centerText = QStringLiteral("Center   %1 , %2")
                                 .arg(QString::number(centerCanvas.x(), 'f', 1),
                                      QString::number(centerCanvas.y(), 'f', 1));
  const float panelWidthPx =
      std::max(244.0f,
               static_cast<float>(std::max(
                   titleFm.horizontalAdvance(titleText),
                   std::max(detailFm.horizontalAdvance(boundsText),
                            std::max(detailFm.horizontalAdvance(anchorText),
                                     detailFm.horizontalAdvance(centerText))))) +
                   34.0f);
  const float panelHeightPx =
      static_cast<float>(titleFm.height() + detailFm.height() * 3 + 32);
  const float panelWidth = panelWidthPx * invZoom;
  const float panelHeight = panelHeightPx * invZoom;
  const float panelInsetX = 12.0f * invZoom;
  const float panelTitleTop = 6.0f * invZoom;
  const float panelBodyTop = 24.0f * invZoom;
  const float panelGap = static_cast<float>(detailFm.height()) * invZoom;
  const QPointF panelOffset(
      anchorCanvas.x() >= centerCanvas.x() ? -panelWidth - 18.0f * invZoom
                                           : 18.0f * invZoom,
      anchorCanvas.y() >= centerCanvas.y() ? -panelHeight - 18.0f * invZoom
                                           : 18.0f * invZoom);
  const QRectF panelRect(anchorCanvas + panelOffset, QSizeF(panelWidth, panelHeight));

  renderer->drawOverlayPanel(static_cast<float>(panelRect.left()),
                             static_cast<float>(panelRect.top()),
                             static_cast<float>(panelRect.width()),
                             static_cast<float>(panelRect.height()),
                             FloatColor{0.04f, 0.05f, 0.07f, 0.88f},
                             FloatColor{0.18f, 0.75f, 0.95f, 0.90f});
  renderer->drawText(QRectF(panelRect.left() + panelInsetX,
                            panelRect.top() + panelTitleTop,
                            panelRect.width() - panelInsetX * 2.0f,
                            (titleFm.height() + 2.0f) * invZoom),
                     titleText, titleFont,
                     FloatColor{0.95f, 0.98f, 1.0f, 1.0f},
                     Qt::AlignLeft | Qt::AlignVCenter);
  renderer->drawText(QRectF(panelRect.left() + panelInsetX,
                            panelRect.top() + panelBodyTop,
                            panelRect.width() - panelInsetX * 2.0f,
                            (detailFm.height() + 2.0f) * invZoom),
                     boundsText, detailFont,
                     FloatColor{0.45f, 0.84f, 0.98f, 1.0f},
                     Qt::AlignLeft | Qt::AlignVCenter,
                     1.0f,
                     FloatColor{0.0f, 0.0f, 0.0f, 0.86f},
                     1.2f);
  renderer->drawText(QRectF(panelRect.left() + panelInsetX,
                            panelRect.top() + panelBodyTop + panelGap,
                            panelRect.width() - panelInsetX * 2.0f,
                            (detailFm.height() + 2.0f) * invZoom),
                     anchorText, detailFont,
                     FloatColor{1.0f, 0.86f, 0.40f, 1.0f},
                     Qt::AlignLeft | Qt::AlignVCenter,
                     1.0f,
                     FloatColor{0.0f, 0.0f, 0.0f, 0.86f},
                     1.2f);
  renderer->drawText(QRectF(panelRect.left() + panelInsetX,
                            panelRect.top() + panelBodyTop + panelGap * 2.0f,
                            panelRect.width() - panelInsetX * 2.0f,
                            (detailFm.height() + 2.0f) * invZoom),
                     centerText, detailFont,
                     FloatColor{0.32f, 0.88f, 1.0f, 1.0f},
                     Qt::AlignLeft | Qt::AlignVCenter,
                     1.0f,
                     FloatColor{0.0f, 0.0f, 0.0f, 0.86f},
                     1.2f);
}

void drawSelectionOverlay(ArtifactIRenderer *renderer,
                          const ArtifactAbstractLayerPtr &layer)
{
  if (!renderer || !layer) {
    return;
  }

  const QRectF localBounds = layer->localBounds();
  if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
      localBounds.height() <= 0.0) {
    return;
  }

  const QTransform globalTransform = layer->getGlobalTransform();
  const QPointF tl = globalTransform.map(localBounds.topLeft());
  const QPointF tr = globalTransform.map(localBounds.topRight());
  const QPointF br = globalTransform.map(localBounds.bottomRight());
  const QPointF bl = globalTransform.map(localBounds.bottomLeft());

  const FloatColor outerColor{0.15f, 0.95f, 1.0f, 0.92f};
  const FloatColor innerColor{0.02f, 0.08f, 0.10f, 0.72f};
  renderer->drawSolidLine({static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                          {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                          outerColor, 1.8f);
  renderer->drawSolidLine({static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                          {static_cast<float>(br.x()), static_cast<float>(br.y())},
                          outerColor, 1.8f);
  renderer->drawSolidLine({static_cast<float>(br.x()), static_cast<float>(br.y())},
                          {static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                          outerColor, 1.8f);
  renderer->drawSolidLine({static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                          {static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                          outerColor, 1.8f);
  renderer->drawSolidLine({static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                          {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                          innerColor, 0.8f);
  renderer->drawSolidLine({static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                          {static_cast<float>(br.x()), static_cast<float>(br.y())},
                          innerColor, 0.8f);
  renderer->drawSolidLine({static_cast<float>(br.x()), static_cast<float>(br.y())},
                          {static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                          innerColor, 0.8f);
  renderer->drawSolidLine({static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                          {static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                          innerColor, 0.8f);

  const float zoom = std::max(0.001f, renderer->getZoom());
  const float nodeSize = std::max(4.5f, 7.5f / zoom);
  const FloatColor nodeColor{1.0f, 0.94f, 0.32f, 0.98f};

  if (const auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) {
    const auto type = shape->shapeType();
    if (type == ShapeType::Polygon) {
      const auto points = shape->customPolygonPoints();
      if (!points.empty()) {
        QPointF prev = globalTransform.map(points.front());
        for (size_t i = 1; i < points.size(); ++i) {
          const QPointF cur = globalTransform.map(points[i]);
          renderer->drawSolidLine(
              {static_cast<float>(prev.x()), static_cast<float>(prev.y())},
              {static_cast<float>(cur.x()), static_cast<float>(cur.y())},
              outerColor, 1.2f);
          prev = cur;
        }
        if (shape->customPolygonClosed() && points.size() > 1) {
          const QPointF first = globalTransform.map(points.front());
          renderer->drawSolidLine(
              {static_cast<float>(prev.x()), static_cast<float>(prev.y())},
              {static_cast<float>(first.x()), static_cast<float>(first.y())},
              outerColor, 1.2f);
        }
        for (const auto &pt : points) {
          const QPointF canvasPt = globalTransform.map(pt);
          renderer->drawPoint(static_cast<float>(canvasPt.x()),
                              static_cast<float>(canvasPt.y()), nodeSize,
                              nodeColor);
        }
      }
    } else if (!shape->customPathVertices().empty()) {
      const auto vertices = shape->customPathVertices();
      QPointF prev;
      bool hasPrev = false;
      for (const auto &vertex : vertices) {
        const QPointF canvasPt = globalTransform.map(vertex.pos);
        renderer->drawPoint(static_cast<float>(canvasPt.x()),
                            static_cast<float>(canvasPt.y()), nodeSize,
                            nodeColor);
        if (hasPrev) {
          renderer->drawSolidLine(
              {static_cast<float>(prev.x()), static_cast<float>(prev.y())},
              {static_cast<float>(canvasPt.x()), static_cast<float>(canvasPt.y())},
              outerColor, 1.0f);
        }
        prev = canvasPt;
        hasPrev = true;
      }
      if (shape->customPathClosed() && vertices.size() > 1) {
        const QPointF first = globalTransform.map(vertices.front().pos);
        renderer->drawSolidLine(
            {static_cast<float>(prev.x()), static_cast<float>(prev.y())},
            {static_cast<float>(first.x()), static_cast<float>(first.y())},
            outerColor, 1.0f);
      }
    }
  }
}

void drawCameraSelectionOverlay(ArtifactIRenderer *renderer,
                                const ArtifactAbstractLayerPtr &layer,
                                bool isActiveCamera)
{
  if (!renderer || !layer) {
    return;
  }

  const auto camera = std::dynamic_pointer_cast<ArtifactCameraLayer>(layer);
  if (!camera) {
    return;
  }

  const QRectF localBounds = layer->localBounds();
  if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
      localBounds.height() <= 0.0) {
    return;
  }

  const QTransform globalTransform = layer->getGlobalTransform();
  const QPointF tl = globalTransform.map(localBounds.topLeft());
  const QPointF tr = globalTransform.map(localBounds.topRight());
  const QPointF br = globalTransform.map(localBounds.bottomRight());
  const QPointF panelAnchor = QPointF(
      std::min(tl.x(), tr.x()),
      std::min(tl.y(), br.y()) - 52.0);

  const FloatColor fillColor =
      isActiveCamera ? FloatColor{0.08f, 0.18f, 0.12f, 0.95f}
                     : FloatColor{0.06f, 0.08f, 0.11f, 0.94f};
  const FloatColor outlineColor =
      isActiveCamera ? FloatColor{0.30f, 0.82f, 0.48f, 0.92f}
                     : FloatColor{0.28f, 0.56f, 0.82f, 0.88f};

  renderer->drawOverlayPanel(static_cast<float>(panelAnchor.x()),
                             static_cast<float>(panelAnchor.y()), 178.0f, 44.0f,
                             fillColor, outlineColor);

  QFont titleFont = QApplication::font();
  titleFont.setPointSizeF(std::max(10.0, static_cast<double>(titleFont.pointSizeF()) + 1.0));
  titleFont.setWeight(QFont::DemiBold);
  QFont detailFont = QApplication::font();
  detailFont.setPointSizeF(std::max(8.5, static_cast<double>(detailFont.pointSizeF())));

  const QString modeText =
      camera->projectionMode() == ProjectionMode::Orthographic
          ? QStringLiteral("Orthographic")
          : QStringLiteral("Perspective");
  const QString lensText = camera->projectionMode() == ProjectionMode::Orthographic
                               ? QStringLiteral("Ortho %1 x %2")
                                     .arg(camera->orthoWidth(), 0, 'f', 0)
                                     .arg(camera->orthoHeight(), 0, 'f', 0)
                               : (camera->useManualFov()
                                      ? QStringLiteral("FOV %1 deg")
                                            .arg(camera->fov(), 0, 'f', 1)
                                      : QStringLiteral("Zoom %1 px")
                                            .arg(camera->zoom(), 0, 'f', 0));
  const QString dofText =
      camera->depthOfField() ? QStringLiteral("DOF On") : QStringLiteral("DOF Off");

  renderer->drawText(QRectF(panelAnchor.x() + 12.0, panelAnchor.y() + 6.0,
                            154.0, 16.0),
                     QStringLiteral("Camera"), titleFont,
                     isActiveCamera ? FloatColor{0.88f, 0.98f, 0.92f, 1.0f}
                                    : FloatColor{0.90f, 0.94f, 0.98f, 1.0f},
                     Qt::AlignLeft | Qt::AlignVCenter);
  renderer->drawText(QRectF(panelAnchor.x() + 12.0, panelAnchor.y() + 22.0,
                            154.0, 14.0),
                     QStringLiteral("%1 | %2 | %3")
                         .arg(modeText, lensText, dofText),
                     detailFont,
                     isActiveCamera ? FloatColor{0.74f, 0.94f, 0.82f, 1.0f}
                                    : FloatColor{0.74f, 0.82f, 0.90f, 1.0f},
                     Qt::AlignLeft | Qt::AlignVCenter);
}

void drawSelectionSummaryOverlay(ArtifactIRenderer *renderer,
                                 const ArtifactAbstractLayerPtr &layer,
                                 int overlayW,
                                 int overlayH)
{
  if (!renderer || !layer) {
    return;
  }

  QFont font = QApplication::font();
  font.setPointSizeF(std::max(9.0, static_cast<double>(font.pointSizeF())));
  const QFontMetrics fm(font);
  const QString title =
      layer->layerName().trimmed().isEmpty() ? QStringLiteral("Selection")
                                             : layer->layerName().trimmed();
  const QString detail = layerOverlayDetailText(layer);
  const int lineHeight = fm.height();
  const int contentWidth =
      std::max(fm.horizontalAdvance(title),
               detail.isEmpty() ? 0 : fm.horizontalAdvance(detail));
  QRect labelRect(12, 12, contentWidth + 24, lineHeight * 2 + 12);
  if (labelRect.bottom() > overlayH - 8) {
    labelRect.moveBottom(overlayH - 8);
  }
  if (labelRect.right() > overlayW - 8) {
    labelRect.moveRight(overlayW - 8);
  }

  renderer->drawOverlayPanel(static_cast<float>(labelRect.left()),
                             static_cast<float>(labelRect.top()),
                             static_cast<float>(labelRect.width()),
                             static_cast<float>(labelRect.height()),
                             FloatColor{0.05f, 0.07f, 0.10f, 0.88f},
                             FloatColor{0.20f, 0.72f, 0.92f, 0.90f});
  renderer->drawText(labelRect.adjusted(10, 6, -10, -6), title, font,
                     FloatColor{0.92f, 0.96f, 1.0f, 1.0f},
                     Qt::AlignLeft | Qt::AlignTop);
  if (!detail.isEmpty()) {
    renderer->drawText(labelRect.adjusted(10, 6 + lineHeight, -10, -6),
                       detail, font,
                       FloatColor{0.72f, 0.79f, 0.86f, 1.0f},
                       Qt::AlignLeft | Qt::AlignTop);
  }
}

void drawViewportDropGhostOverlay(ArtifactIRenderer *renderer,
                                  const ArtifactCompositionPtr &comp,
                                  float overlayWf,
                                  float overlayHf,
                                  const QRectF &ghostRect,
                                  const QString &ghostTitle,
                                  const QString &ghostHint,
                                  const QString &dropCandidateLabel)
{
  if (!renderer) {
    return;
  }

  const int overlayW = std::max(1, static_cast<int>(std::ceil(overlayWf)));
  const int overlayH = std::max(1, static_cast<int>(std::ceil(overlayHf)));

  QImage overlayImage(overlayW, overlayH, QImage::Format_ARGB32_Premultiplied);
  overlayImage.fill(Qt::transparent);

  QPainter p(&overlayImage);
  p.setRenderHint(QPainter::Antialiasing, true);
  QFont font = QApplication::font();
  font.setPointSizeF(std::max(9.0, static_cast<double>(font.pointSizeF())));
  p.setFont(font);

  p.fillRect(QRectF(0.0, 0.0, overlayWf, overlayHf),
             QColor(60, 120, 240, 30));
  p.setPen(QPen(QColor(100, 160, 255, 180), 2.0, Qt::DashLine));
  p.setBrush(Qt::NoBrush);
  p.drawRect(QRectF(4.0, 4.0, overlayWf - 8.0, overlayHf - 8.0));

  drawLabelBox(p, ghostRect, QColor(30, 40, 60, 165),
               QColor(220, 235, 255, 220), ghostTitle, ghostHint);

  if (!dropCandidateLabel.isEmpty()) {
    const QFontMetrics fm(p.font());
    const int labelW = std::min(
        overlayW - 24,
        std::max(180, fm.horizontalAdvance(dropCandidateLabel) + 24));
    const int labelH = fm.height() + 12;
    const QRect labelRect(std::max(12, overlayW / 2 - labelW / 2),
                          std::max(8, overlayH / 2 - labelH / 2), labelW,
                          labelH);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 30, 60, 200));
    p.drawRoundedRect(labelRect, 6, 6);
    p.setPen(QColor(200, 220, 255));
    p.drawText(labelRect, Qt::AlignCenter,
               fm.elidedText(dropCandidateLabel, Qt::ElideMiddle,
                             labelRect.width() - 16));
  }

  p.end();
  const QSize compSize = comp ? comp->settings().compositionSize() : QSize();
  const QSize fallbackSize(compSize.width() > 0 ? compSize.width() : 1920,
                           compSize.height() > 0 ? compSize.height() : 1080);
  presentOverlayImage(renderer, overlayImage, comp ? &fallbackSize : nullptr);
}

void drawViewportInfoOverlay(ArtifactIRenderer *renderer,
                             int overlayW,
                             int overlayH,
                             const QString &title,
                             const QString &detail,
                             const QSize *restoreCanvasSize)
{
  if (!renderer) {
    return;
  }

  QImage overlayImage(overlayW, overlayH, QImage::Format_ARGB32_Premultiplied);
  overlayImage.fill(Qt::transparent);

  QPainter p(&overlayImage);
  p.setRenderHint(QPainter::Antialiasing, true);
  QFont font = QApplication::font();
  font.setPointSizeF(std::max(9.0, static_cast<double>(font.pointSizeF())));
  p.setFont(font);

  const QFontMetrics fm(p.font());
  const int lineHeight = fm.height();
  const int contentWidth =
      std::max(fm.horizontalAdvance(title),
               detail.isEmpty() ? 0 : fm.horizontalAdvance(detail));
  const int contentHeight =
      detail.isEmpty() ? lineHeight : lineHeight * 2 + 4;
  QRect labelRect(12, 12, contentWidth + 24, contentHeight + 12);
  if (labelRect.right() > overlayW - 8) {
    labelRect.moveRight(overlayW - 8);
  }
  if (labelRect.bottom() > overlayH - 8) {
    labelRect.moveBottom(overlayH - 8);
  }
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(8, 10, 14, 210));
  p.drawRoundedRect(labelRect, 7, 7);
  p.setPen(QColor(232, 238, 244));
  p.drawText(labelRect.adjusted(10, 6, -10, -6), Qt::AlignLeft | Qt::AlignTop,
             title);
  if (!detail.isEmpty()) {
    p.setPen(QColor(178, 190, 204));
    const QRect detailRect = labelRect.adjusted(10, 6 + lineHeight, -10, -6);
    p.drawText(detailRect, Qt::AlignLeft | Qt::AlignTop,
               fm.elidedText(detail, Qt::ElideRight, detailRect.width()));
  }

  p.end();
  presentOverlayImage(renderer, overlayImage, restoreCanvasSize);
}

void drawViewportStatusChipOverlay(ArtifactIRenderer *renderer,
                                   int overlayW,
                                   int overlayH,
                                   const QString &statusText,
                                   const QSize *restoreCanvasSize)
{
  if (!renderer) {
    return;
  }

  QImage overlayImage(overlayW, overlayH, QImage::Format_ARGB32_Premultiplied);
  overlayImage.fill(Qt::transparent);

  QPainter p(&overlayImage);
  p.setRenderHint(QPainter::Antialiasing, true);
  QFont font = QApplication::font();
  font.setPointSizeF(std::max(9.0, static_cast<double>(font.pointSizeF())));
  p.setFont(font);

  const QFontMetrics fm(p.font());
  const int chipH = fm.height() + 12;
  const int chipW = fm.horizontalAdvance(statusText) + 24;
  QRect chipRect(overlayW - chipW - 12, 12, chipW, chipH);
  if (chipRect.left() < 8) {
    chipRect.setLeft(8);
  }
  p.setPen(QPen(QColor(76, 102, 132, 180), 1.0));
  p.setBrush(QColor(12, 16, 22, 210));
  p.drawRoundedRect(chipRect, 10, 10);
  p.setPen(QColor(226, 235, 243));
  p.drawText(chipRect, Qt::AlignCenter, statusText);

  p.end();
  presentOverlayImage(renderer, overlayImage, restoreCanvasSize);
}

void drawViewportSnapHintOverlay(ArtifactIRenderer *renderer,
                                 int overlayW,
                                 int overlayH,
                                 bool snapBypassed,
                                 const QString &snapTitle,
                                 const QString &snapDetail,
                                 int verticalCount,
                                 int horizontalCount,
                                 const QSize *restoreCanvasSize)
{
  if (!renderer) {
    return;
  }

  QImage overlayImage(overlayW, overlayH, QImage::Format_ARGB32_Premultiplied);
  overlayImage.fill(Qt::transparent);

  QPainter p(&overlayImage);
  p.setRenderHint(QPainter::Antialiasing, true);
  QFont font = QApplication::font();
  font.setPointSizeF(std::max(9.0, static_cast<double>(font.pointSizeF())));
  p.setFont(font);

  QString title = snapTitle;
  if (!snapBypassed) {
    QStringList parts;
    if (verticalCount > 0) {
      parts << QStringLiteral("X");
    }
    if (horizontalCount > 0) {
      parts << QStringLiteral("Y");
    }
    if (!parts.isEmpty()) {
      title += QStringLiteral(" - ");
      title += parts.join(QStringLiteral("/"));
    }
  }

  const QFontMetrics fm(p.font());
  const int lineHeight = fm.height();
  const int contentWidth = std::max(fm.horizontalAdvance(title),
                                    fm.horizontalAdvance(snapDetail));
  QRect labelRect(12, overlayH - (lineHeight * 2 + 28), contentWidth + 24,
                  lineHeight * 2 + 12);
  if (labelRect.bottom() > overlayH - 8) {
    labelRect.moveBottom(overlayH - 8);
  }
  if (labelRect.left() < 8) {
    labelRect.moveLeft(8);
  }
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(8, 10, 14, 210));
  p.drawRoundedRect(labelRect, 7, 7);
  p.setPen(QColor(232, 238, 244));
  p.drawText(labelRect.adjusted(10, 6, -10, -6), Qt::AlignLeft | Qt::AlignTop,
             title);
  p.setPen(QColor(178, 190, 204));
  const QRect detailRect = labelRect.adjusted(10, 6 + lineHeight, -10, -6);
  p.drawText(detailRect, Qt::AlignLeft | Qt::AlignTop,
             fm.elidedText(snapDetail, Qt::ElideRight, detailRect.width()));

  p.end();
  presentOverlayImage(renderer, overlayImage, restoreCanvasSize);
}

namespace {
[[maybe_unused]] auto* const kForceCommandPaletteOverlayLink =
    &Artifact::drawViewportCommandPaletteOverlay;
[[maybe_unused]] auto* const kForceContextMenuOverlayLink =
    &Artifact::drawViewportContextMenuOverlay;
[[maybe_unused]] auto* const kForcePieMenuOverlayLink =
    &Artifact::drawViewportPieMenuOverlay;
} // namespace

} // namespace Artifact
