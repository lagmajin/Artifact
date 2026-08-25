module;

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QMatrix4x4>
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
#include <QVector3D>
#include <QVector4D>
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <array>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <memory>
#include <QVector>

module Artifact.Widgets.CompositionRenderOverlay;

import Color.Float;
import Settings.Accessibility;
import Artifact.Layer.Camera;
import Artifact.Layer.Video;
import Artifact.Layer.Shape;
import Artifact.Layer.Paint;
import Artifact.Layer.CloneEffectSupport;
import Artifact.Layers.Model3D;
import Mesh;
import Layer.Blend;
import Artifact.Widgets.PieMenu;
import ArtifactCore.Utils.PerformanceProfiler;
import Configuration.LayeredConfigStore;
import Tracking.MotionTracker;
import Artifact.Render.IRenderer;
import Memory.SharedPtr;

namespace Artifact {
using namespace ArtifactCore;

namespace {

QString overlayDebugTag(const QString& tag)
{
  return QStringLiteral("OVR:%1").arg(tag);
}

QString blendModeName(const ArtifactCore::BlendMode mode)
{
  return ArtifactCore::BlendModeUtils::toString(mode);
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
  QString detail = QStringLiteral("%1 | %2 | O%3 | %4%5 | %6 | %7x%8")
      .arg(typeLabel)
      .arg(blendModeName(ArtifactCore::toBlendMode(layer->layerBlendType())))
      .arg(QString::number(std::clamp(layer->opacity() * 100.0f, 0.0f, 100.0f),
                           'f', 0))
      .arg(visibility)
      .arg(locked)
      .arg(maskText)
      .arg(QString::number(std::max(0.0, boundsSize.width()), 'f', 0))
      .arg(QString::number(std::max(0.0, boundsSize.height()), 'f', 0));

  if (const auto videoLayer = ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(layer)) {
    const int trackerId = videoLayer->motionTrackerId();
    if (trackerId > 0) {
      const auto *tracker = ArtifactCore::TrackerManager::instance().tracker(trackerId);
      const QString trackerLabel = tracker
          ? QStringLiteral("Tracker #%1 | %2pt | %3rg | %4fr | conf %5")
                .arg(trackerId)
                .arg(tracker->trackPointCount())
                .arg(tracker->trackRegionCount())
                .arg(tracker->hasResult() ? static_cast<int>(tracker->result().frameCount()) : 0)
                .arg(tracker->averageConfidence(), 0, 'f', 2)
          : QStringLiteral("Tracker #%1 (missing)").arg(trackerId);
      detail += QStringLiteral(" | %1").arg(trackerLabel);
    }
  }

  return detail;
}

QString responsiveLayoutSummaryText(const ArtifactCompositionPtr &comp)
{
  if (!comp) {
    return QString();
  }

  const auto layout = comp->responsiveLayout();
  const QString activeId = comp->activeResponsiveLayoutVariantId();
  const auto activeVariantIt =
      std::find_if(layout.variants.begin(), layout.variants.end(),
                   [&activeId](const ResponsiveLayoutVariant &variant) {
                     return variant.variantId == activeId;
                   });
  if (activeVariantIt == layout.variants.end()) {
    return QStringLiteral("Responsive Layout: %1")
        .arg(activeId.isEmpty() ? QStringLiteral("default") : activeId);
  }

  const ResponsiveLayoutVariant &variant = *activeVariantIt;
  QStringList details;
  details << (variant.displayName.isEmpty() ? variant.variantId
                                            : variant.displayName);
  if (variant.baseSize.isValid()) {
    details << QStringLiteral("%1x%2")
                   .arg(variant.baseSize.width())
                   .arg(variant.baseSize.height());
  }
  if (variant.aspectRatio > 0.0) {
    details << QStringLiteral("%1:1")
                   .arg(QString::number(variant.aspectRatio, 'f', 2));
  }
  const QString guidePreset =
      variant.layoutRules.value(QStringLiteral("guidePreset")).toString().trimmed();
  if (!guidePreset.isEmpty()) {
    details << QStringLiteral("guide %1").arg(guidePreset);
  }
  const QString scaleMode =
      variant.layoutRules.value(QStringLiteral("scaleMode")).toString().trimmed();
  if (!scaleMode.isEmpty()) {
    details << QStringLiteral("scale %1").arg(scaleMode);
  }

  return QStringLiteral("Responsive Layout: %1").arg(details.join(QStringLiteral(" · ")));
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
  const float contrastScale = Accessibility::contrastScale();
  p.setPen(QPen(border, 2.0 * contrastScale, Qt::DashLine));
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

quint64 makeEdgeKey(int a, int b)
{
  const quint32 lo = static_cast<quint32>(std::min(a, b));
  const quint32 hi = static_cast<quint32>(std::max(a, b));
  return (static_cast<quint64>(lo) << 32) | static_cast<quint64>(hi);
}

void draw3DSelectionWireframeOverlayImpl(ArtifactIRenderer *renderer,
                                         const ArtifactAbstractLayerPtr &layer,
                                         const QMatrix4x4 *cameraView,
                                         const QMatrix4x4 *cameraProj)
{
  if (!renderer || !layer || !cameraView || !cameraProj) {
    return;
  }

  const auto modelLayer = ArtifactCore::dynamicPointerCast<Artifact3DLayer>(layer);
  if (!modelLayer) {
    return;
  }

  const ArtifactCore::Mesh &mesh = modelLayer->mesh();
  const auto positions = mesh.vertexAttributes().get<QVector3D>("position");
  if (!positions || positions->data().isEmpty() || mesh.polygonCount() <= 0) {
    return;
  }

  const auto &vertexPositions = positions->data();
  if (vertexPositions.isEmpty()) {
    return;
  }

  const QMatrix4x4 modelMatrix = modelLayer->getGlobalTransform4x4();
  // Selection edges use one unambiguous, topology-colored line. A shadow
  // pass makes each edge look doubled and obscures the quad boundary.
  const FloatColor wireColor{1.0f, 0.42f, 0.0f, 0.98f};
  const float thickness = 1.9f;
  const FloatColor vertexShadow{0.02f, 0.03f, 0.04f, 0.94f};
  const FloatColor vertexColor{1.0f, 0.52f, 0.0f, 1.0f};

  renderer->set3DCameraMatrices(*cameraView, *cameraProj);

  // Match Maya's default component display: only the visible side of the
  // source topology is drawn. The optional backface mode deliberately keeps
  // the same polygon-boundary path, so quads never gain render triangulation.
  const bool showBackfaces =
      ArtifactCore::LayeredConfigStore::instance().valueBool(
          QStringLiteral("Viewport/SelectionWireframe/ShowBackfaces"), false);
  bool inverseViewValid = false;
  const QMatrix4x4 inverseView = cameraView->inverted(&inverseViewValid);
  const QVector3D cameraPosition = inverseViewValid
      ? inverseView.map(QVector3D(0.0f, 0.0f, 0.0f))
      : QVector3D();

  std::unordered_set<quint64> visitedEdges;
  std::unordered_set<int> visibleVertices;
  const int kMaxOverlayPolygons = [&] {
    switch (renderer->detailLevel()) {
    case LODManager::DetailLevel::Low:
      return 4000;
    case LODManager::DetailLevel::Medium:
      return 12000;
    case LODManager::DetailLevel::High:
    default:
      return 24000;
    }
  }();
  const int polygonStride = std::max(
      1, (mesh.polygonCount() + kMaxOverlayPolygons - 1) / kMaxOverlayPolygons);
  const int sampledPolygonCount =
      (mesh.polygonCount() + polygonStride - 1) / polygonStride;
  visitedEdges.reserve(static_cast<size_t>(sampledPolygonCount) * 3u);
  visibleVertices.reserve(static_cast<size_t>(sampledPolygonCount) * 4u);

  auto drawEdge = [&](int a, int b) {
    if (a < 0 || b < 0 || a >= vertexPositions.size() || b >= vertexPositions.size()) {
      return;
    }

    const quint64 edgeKey = makeEdgeKey(a, b);
    if (!visitedEdges.insert(edgeKey).second) {
      return;
    }

    const QVector3D start = modelMatrix.map(vertexPositions[a]);
    const QVector3D end = modelMatrix.map(vertexPositions[b]);
    renderer->draw3DLine({start.x(), start.y(), start.z()},
                         {end.x(), end.y(), end.z()},
                         wireColor, thickness);
  };

  for (int polygonIndex = 0; polygonIndex < mesh.polygonCount(); polygonIndex += polygonStride) {
    const QVector<int> polygon = mesh.getPolygonVertices(polygonIndex);
    if (polygon.size() < 2) {
      continue;
    }

    bool hasValidIndices = true;
    for (const int vertexIndex : polygon) {
      if (vertexIndex < 0 || vertexIndex >= vertexPositions.size()) {
        hasValidIndices = false;
        break;
      }
    }
    if (!hasValidIndices) {
      continue;
    }

    if (!showBackfaces && inverseViewValid && polygon.size() >= 3) {
      const QVector3D origin = modelMatrix.map(vertexPositions[polygon[0]]);
      QVector3D faceNormal;
      for (int i = 1; i + 1 < polygon.size(); ++i) {
        const QVector3D edgeA =
            modelMatrix.map(vertexPositions[polygon[i]]) - origin;
        const QVector3D edgeB =
            modelMatrix.map(vertexPositions[polygon[i + 1]]) - origin;
        faceNormal = QVector3D::crossProduct(edgeA, edgeB);
        if (faceNormal.lengthSquared() > 1.0e-8f) {
          break;
        }
      }
      if (faceNormal.lengthSquared() > 1.0e-8f &&
          QVector3D::dotProduct(faceNormal, cameraPosition - origin) <= 0.0f) {
        continue;
      }
    }

    for (int i = 0; i < polygon.size(); ++i) {
      visibleVertices.insert(polygon[i]);
      drawEdge(polygon[i], polygon[(i + 1) % polygon.size()]);
    }
  }

  // Maya-style component cue: selected model vertices are small filled cards
  // facing the camera. They remain readable over the shaded surface without
  // introducing normal spikes or fake triangulation inside quad faces.
  if (inverseViewValid && !visibleVertices.empty()) {
    QVector3D cameraRight = inverseView.mapVector(QVector3D(1.0f, 0.0f, 0.0f));
    QVector3D cameraUp = inverseView.mapVector(QVector3D(0.0f, 1.0f, 0.0f));
    if (cameraRight.lengthSquared() > 1.0e-8f &&
        cameraUp.lengthSquared() > 1.0e-8f) {
      cameraRight.normalize();
      cameraUp.normalize();
      const QVector3D localBoundsExtent =
          mesh.boundingBoxMax() - mesh.boundingBoxMin();
      const float markerRadius = std::max(
          0.004f, modelMatrix.mapVector(localBoundsExtent).length() * 0.0045f);
      QVector3D towardCamera =
          inverseView.mapVector(QVector3D(0.0f, 0.0f, 1.0f));
      if (towardCamera.lengthSquared() > 1.0e-8f) {
        towardCamera.normalize();
      }
      const int maxVertexMarkers = renderer->detailLevel() == LODManager::DetailLevel::Low
          ? 1500
          : renderer->detailLevel() == LODManager::DetailLevel::Medium ? 5000 : 12000;
      const int markerStride = std::max(
          1, (static_cast<int>(visibleVertices.size()) + maxVertexMarkers - 1) /
                 maxVertexMarkers);
      int markerIndex = 0;
      for (const int vertexIndex : visibleVertices) {
        if ((markerIndex++ % markerStride) != 0 || vertexIndex < 0 ||
            vertexIndex >= vertexPositions.size()) {
          continue;
        }
        const QVector3D center =
            modelMatrix.map(vertexPositions[vertexIndex]) +
            towardCamera * (markerRadius * 0.12f);
        const auto drawMarker = [&](float radius, const FloatColor &color) {
          const QVector3D right = cameraRight * radius;
          const QVector3D up = cameraUp * radius;
          const QVector3D p0 = center - right - up;
          const QVector3D p1 = center + right - up;
          const QVector3D p2 = center + right + up;
          const QVector3D p3 = center - right + up;
          renderer->draw3DQuad({p0.x(), p0.y(), p0.z()},
                               {p1.x(), p1.y(), p1.z()},
                               {p2.x(), p2.y(), p2.z()},
                               {p3.x(), p3.y(), p3.z()}, color);
        };
        drawMarker(markerRadius * 1.45f, vertexShadow);
        drawMarker(markerRadius, vertexColor);
      }
    }
  }

  // Lines and vertex cards are buffered. Submit them while the selection
  // camera is still active; resetting first projects the queued geometry with
  // the default matrices instead of the composition camera.
  renderer->flushGizmo3D();
  renderer->reset3DCameraMatrices();
}

void draw3DSelectionBoundsOverlayImpl(ArtifactIRenderer *renderer,
                                      const ArtifactAbstractLayerPtr &layer,
                                      const QMatrix4x4 *cameraView,
                                      const QMatrix4x4 *cameraProj)
{
  if (!renderer || !layer || !cameraView || !cameraProj) {
    return;
  }
  const auto modelLayer = ArtifactCore::dynamicPointerCast<Artifact3DLayer>(layer);
  if (!modelLayer || modelLayer->fixedGeometry() == FixedGeometry3D::Plane) {
    return;
  }

  const auto &mesh = modelLayer->mesh();
  const QVector3D minB = mesh.boundingBoxMin();
  const QVector3D maxB = mesh.boundingBoxMax();
  if (!std::isfinite(minB.x()) || !std::isfinite(maxB.x()) ||
      maxB.x() <= minB.x() || maxB.y() <= minB.y() || maxB.z() <= minB.z()) {
    return;
  }

  const std::array<QVector3D, 8> corners = {
      QVector3D(minB.x(), minB.y(), minB.z()), QVector3D(maxB.x(), minB.y(), minB.z()),
      QVector3D(maxB.x(), maxB.y(), minB.z()), QVector3D(minB.x(), maxB.y(), minB.z()),
      QVector3D(minB.x(), minB.y(), maxB.z()), QVector3D(maxB.x(), minB.y(), maxB.z()),
      QVector3D(maxB.x(), maxB.y(), maxB.z()), QVector3D(minB.x(), maxB.y(), maxB.z())};
  const QMatrix4x4 modelMatrix = modelLayer->getGlobalTransform4x4();
  std::array<QVector3D, 8> world{};
  for (int i = 0; i < 8; ++i) {
    world[i] = modelMatrix.map(corners[i]);
  }
  static constexpr int edges[][2] = {
      {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
  renderer->set3DCameraMatrices(*cameraView, *cameraProj);
  const FloatColor shadow{0.02f, 0.03f, 0.04f, 0.9f};
  const FloatColor color{1.0f, 0.56f, 0.18f, 0.98f};
  for (const auto &edge : edges) {
    const auto &a = world[edge[0]];
    const auto &b = world[edge[1]];
    renderer->draw3DLine({a.x(), a.y(), a.z()}, {b.x(), b.y(), b.z()}, shadow, 3.6f);
    renderer->draw3DLine({a.x(), a.y(), a.z()}, {b.x(), b.y(), b.z()}, color, 1.9f);
  }
  // Bounds lines are buffered by PrimitiveRenderer3D. Keep the selection
  // camera active until they have been submitted.
  renderer->flushGizmo3D();
  renderer->reset3DCameraMatrices();
}

} // namespace

void draw3DSelectionWireframeOverlay(ArtifactIRenderer *renderer,
                                     const ArtifactAbstractLayerPtr &layer,
                                     const QMatrix4x4 *cameraView,
                                     const QMatrix4x4 *cameraProj)
{
  draw3DSelectionWireframeOverlayImpl(renderer, layer, cameraView, cameraProj);
}

void draw3DSelectionBoundsOverlay(ArtifactIRenderer *renderer,
                                  const ArtifactAbstractLayerPtr &layer,
                                  const QMatrix4x4 *cameraView,
                                  const QMatrix4x4 *cameraProj)
{
  draw3DSelectionBoundsOverlayImpl(renderer, layer, cameraView, cameraProj);
}

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

  const QSize compSize = comp->effectiveCompositionSize();
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

  const QString summary = responsiveLayoutSummaryText(comp);
  if (!summary.isEmpty()) {
    QFont badgeFont = QApplication::font();
    badgeFont.setPointSizeF(std::max(8.5, static_cast<double>(badgeFont.pointSizeF())));
    const QFontMetrics fm(badgeFont);
    const int badgeWidth = std::min(std::max(280, fm.horizontalAdvance(summary) + 28),
                                    std::max(180, static_cast<int>(cw) - 24));
    const int badgeHeight = fm.height() + 18;
    const QRectF badgeRect(12.0, 12.0, static_cast<float>(badgeWidth),
                           static_cast<float>(badgeHeight));
    renderer->drawOverlayPanel(static_cast<float>(badgeRect.left()),
                               static_cast<float>(badgeRect.top()),
                               static_cast<float>(badgeRect.width()),
                               static_cast<float>(badgeRect.height()),
                               FloatColor{0.05f, 0.06f, 0.08f, 0.90f},
                               FloatColor{0.31f, 0.55f, 0.84f, 0.95f});
    renderer->drawText(badgeRect.adjusted(12.0, 6.0, -12.0, -6.0), summary,
                       badgeFont, FloatColor{0.91f, 0.95f, 0.99f, 1.0f},
                       Qt::AlignLeft | Qt::AlignVCenter);
  }
}

void drawAnchorCenterOverlay(ArtifactIRenderer *renderer,
                             const ArtifactAbstractLayerPtr &layer,
                             const QMatrix4x4 *cameraView,
                             const QMatrix4x4 *cameraProj)
{
  if (!renderer || !layer) {
    return;
  }

  if (const auto modelLayer = ArtifactCore::dynamicPointerCast<Artifact3DLayer>(layer);
      modelLayer && modelLayer->fixedGeometry() != FixedGeometry3D::Plane) {
    draw3DSelectionBoundsOverlayImpl(renderer, layer, cameraView, cameraProj);
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

void drawTrackerPinOverlay(ArtifactIRenderer *renderer,
                           float x,
                           float y,
                           float size,
                           const FloatColor &fillColor,
                           const FloatColor &accentColor,
                           bool selected,
                           const QString &label,
                           float opacity)
{
  if (!renderer || size <= 0.0f) {
    return;
  }

  const float pinSize = std::max(2.0f, size);
  const float haloSize = pinSize * (selected ? 1.12f : 1.00f);
  const float ringSize = pinSize * 0.72f;
  const float coreSize = pinSize * 0.38f;
  const float crossSize = pinSize * 0.92f;
  const FloatColor shadowColor{0.0f, 0.0f, 0.0f,
                               std::clamp((selected ? 0.50f : 0.34f) * opacity,
                                          0.0f, 1.0f)};
  const FloatColor ringColor{
      accentColor.r(), accentColor.g(), accentColor.b(),
      std::clamp((selected ? 1.0f : 0.82f) * accentColor.a() * opacity,
                 0.0f, 1.0f)};
  const FloatColor coreColor{
      fillColor.r(), fillColor.g(), fillColor.b(),
      std::clamp(fillColor.a() * opacity, 0.0f, 1.0f)};
  const FloatColor crossColor{1.0f, 1.0f, 1.0f,
                              std::clamp((selected ? 0.98f : 0.76f) * opacity,
                                         0.0f, 1.0f)};

  renderer->drawCircle(x, y, haloSize * 0.58f, shadowColor, 0.0f, true);
  renderer->drawCircle(x, y, haloSize * 0.42f, ringColor, 1.6f, false);
  renderer->drawCircle(x, y, ringSize * 0.42f, coreColor, 0.0f, true);
  renderer->drawCrosshair(x, y, crossSize, crossColor);
  renderer->drawCircle(x, y, coreSize * 0.38f,
                       FloatColor{1.0f, 1.0f, 1.0f,
                                  std::clamp(0.92f * opacity, 0.0f, 1.0f)},
                       0.0f, true);

  if (!label.trimmed().isEmpty()) {
    QFont labelFont = QApplication::font();
    labelFont.setPointSizeF(std::max(8.0, static_cast<double>(labelFont.pointSizeF())));
    const QFontMetrics fm(labelFont);
    const QRectF labelRect(
        x + pinSize * 0.90f,
        y - std::max(10.0f, static_cast<float>(fm.height()) * 0.55f),
        std::max(48.0f, static_cast<float>(fm.horizontalAdvance(label)) + 10.0f),
        std::max(16.0f, static_cast<float>(fm.height()) + 4.0f));
    renderer->drawText(labelRect, label, labelFont,
                       FloatColor{0.95f, 0.97f, 1.0f,
                                  std::clamp(opacity, 0.0f, 1.0f)},
                       Qt::AlignLeft | Qt::AlignVCenter,
                       1.0f,
                       FloatColor{0.0f, 0.0f, 0.0f,
                                  std::clamp(0.86f * opacity, 0.0f, 1.0f)},
                       1.0f);
  }
}

void drawSelectionOverlay(ArtifactIRenderer *renderer,
                          const ArtifactAbstractLayerPtr &layer,
                          const QMatrix4x4 *cameraView,
                          const QMatrix4x4 *cameraProj)
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

  if (const auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer)) {
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
      const int nv = static_cast<int>(vertices.size());
      auto drawBezierOrLine = [&](const CustomPathVertex& va, const CustomPathVertex& vb) {
        const QPointF p0 = globalTransform.map(va.pos);
        if (va.outTangent != QPointF(0, 0) || vb.inTangent != QPointF(0, 0)) {
          const QPointF p1 = globalTransform.map(va.pos + va.outTangent);
          const QPointF p2 = globalTransform.map(vb.pos + vb.inTangent);
          const QPointF p3 = globalTransform.map(vb.pos);
          constexpr int steps = 18;
          Detail::float2 prev = {static_cast<float>(p0.x()), static_cast<float>(p0.y())};
          for (int k = 1; k <= steps; ++k) {
            const double t = static_cast<double>(k) / steps;
            const double u = 1.0 - t;
            const QPointF pt = p0 * (u * u * u) + p1 * (3.0 * u * u * t) + p2 * (3.0 * u * t * t) + p3 * (t * t * t);
            const Detail::float2 cur = {static_cast<float>(pt.x()), static_cast<float>(pt.y())};
            renderer->drawSolidLine(prev, cur, outerColor, 1.0f);
            prev = cur;
          }
        } else {
          const QPointF p1 = globalTransform.map(vb.pos);
          renderer->drawSolidLine(
              {static_cast<float>(p0.x()), static_cast<float>(p0.y())},
              {static_cast<float>(p1.x()), static_cast<float>(p1.y())},
              outerColor, 1.0f);
        }
      };
      for (int i = 0; i < nv; ++i) {
        renderer->drawPoint(static_cast<float>(globalTransform.map(vertices[i].pos).x()),
                            static_cast<float>(globalTransform.map(vertices[i].pos).y()),
                            nodeSize, nodeColor);
        if (i > 0) drawBezierOrLine(vertices[i - 1], vertices[i]);
      }
      if (shape->customPathClosed() && nv > 1) {
        drawBezierOrLine(vertices.back(), vertices.front());
      }
    }
  }

  const auto cloneInstances =
      cloneRenderInstances(layer.get(), layer->getGlobalTransform4x4());
  if (cloneInstances.size() <= 1) {
    return;
  }

  const FloatColor cloneOuterColor{1.0f, 0.66f, 0.24f, 0.62f};
  const FloatColor cloneInnerColor{0.10f, 0.05f, 0.02f, 0.38f};
  const auto closePoint = [](const QPointF &a, const QPointF &b) {
    return std::abs(a.x() - b.x()) < 0.01 && std::abs(a.y() - b.y()) < 0.01;
  };
  const auto drawCloneFrame = [&](const QPointF &a, const QPointF &b,
                                  const QPointF &c, const QPointF &d) {
    renderer->drawSolidLine({static_cast<float>(a.x()), static_cast<float>(a.y())},
                            {static_cast<float>(b.x()), static_cast<float>(b.y())},
                            cloneOuterColor, 1.35f);
    renderer->drawSolidLine({static_cast<float>(b.x()), static_cast<float>(b.y())},
                            {static_cast<float>(c.x()), static_cast<float>(c.y())},
                            cloneOuterColor, 1.35f);
    renderer->drawSolidLine({static_cast<float>(c.x()), static_cast<float>(c.y())},
                            {static_cast<float>(d.x()), static_cast<float>(d.y())},
                            cloneOuterColor, 1.35f);
    renderer->drawSolidLine({static_cast<float>(d.x()), static_cast<float>(d.y())},
                            {static_cast<float>(a.x()), static_cast<float>(a.y())},
                            cloneOuterColor, 1.35f);
    renderer->drawSolidLine({static_cast<float>(a.x()), static_cast<float>(a.y())},
                            {static_cast<float>(b.x()), static_cast<float>(b.y())},
                            cloneInnerColor, 0.65f);
    renderer->drawSolidLine({static_cast<float>(b.x()), static_cast<float>(b.y())},
                            {static_cast<float>(c.x()), static_cast<float>(c.y())},
                            cloneInnerColor, 0.65f);
    renderer->drawSolidLine({static_cast<float>(c.x()), static_cast<float>(c.y())},
                            {static_cast<float>(d.x()), static_cast<float>(d.y())},
                            cloneInnerColor, 0.65f);
    renderer->drawSolidLine({static_cast<float>(d.x()), static_cast<float>(d.y())},
                            {static_cast<float>(a.x()), static_cast<float>(a.y())},
                            cloneInnerColor, 0.65f);
  };

  for (const auto &instance : cloneInstances) {
    const QPointF &ctl = instance.canvasCorners[0];
    const QPointF &ctr = instance.canvasCorners[1];
    const QPointF &cbr = instance.canvasCorners[2];
    const QPointF &cbl = instance.canvasCorners[3];
    if (closePoint(ctl, tl) && closePoint(ctr, tr) && closePoint(cbr, br) &&
        closePoint(cbl, bl)) {
      continue;
    }
    drawCloneFrame(ctl, ctr, cbr, cbl);
  }
}

void drawSelectionFrameOverlay(ArtifactIRenderer *renderer,
                               const ArtifactAbstractLayerPtr &layer,
                               const FloatColor &color,
                               float thickness,
                               const QMatrix4x4 *cameraView,
                               const QMatrix4x4 *cameraProj,
                               bool showScaleHandles,
                               bool showRotationHandle,
                               float projectedHandleSize)
{
  if (!renderer || !layer) {
    return;
  }

  const QRectF localBounds = layer->localBounds();
  if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
      localBounds.height() <= 0.0) {
    return;
  }

  const FloatColor shadow{0.01f, 0.02f, 0.03f, 0.88f};
  const float safeThickness = std::max(0.75f, thickness);

  // View-cube navigation rotates the composition plane even for ordinary 2D
  // layers. In that mode the selection frame must follow the supplied camera
  // matrices instead of falling back to the unrotated 2D canvas transform.
  if (layer->is3D() || (cameraView && cameraProj)) {
    // Keep the projected frame clear of the layer pixels. Besides improving
    // readability, this leaves a stable border-sized target for frame dragging.
    const qreal frameOutset = std::clamp(
        std::min(localBounds.width(), localBounds.height()) * 0.015, 6.0, 18.0);
    const QRectF bounds = localBounds.adjusted(
        -frameOutset, -frameOutset, frameOutset, frameOutset);
    // Match the matrix used by layer rendering and projected-frame hit tests.
    // The transitional Diligent matrix path has different row/column
    // conventions and projected the visible frame away from its 2D plane in
    // view-orientation mode.
    const QMatrix4x4 world = layer->getGlobalTransform4x4();
    const auto point = [&world](float x, float y) -> Detail::float3 {
      const QVector3D transformed = world.map(QVector3D(x, y, 0.0f));
      return Detail::float3(transformed.x(), transformed.y(), transformed.z());
    };
    const auto isVisibleInCamera = [&](qreal x, qreal y) {
      if (!cameraView || !cameraProj) {
        return true;
      }
      const QVector3D worldPoint = world.map(
          QVector3D(static_cast<float>(x), static_cast<float>(y), 0.0f));
      const QVector4D clip = (*cameraProj) * (*cameraView) *
                             QVector4D(worldPoint, 1.0f);
      if (!std::isfinite(clip.w()) || clip.w() <= 0.0f) {
        return false;
      }
      const float ndcZ = clip.z() / clip.w();
      return std::isfinite(ndcZ) && ndcZ >= -1.0f && ndcZ <= 1.0f;
    };
    const auto tl = point(static_cast<float>(bounds.left()),
                          static_cast<float>(bounds.top()));
    const auto tr = point(static_cast<float>(bounds.right()),
                          static_cast<float>(bounds.top()));
    const auto br = point(static_cast<float>(bounds.right()),
                          static_cast<float>(bounds.bottom()));
    const auto bl = point(static_cast<float>(bounds.left()),
                          static_cast<float>(bounds.bottom()));
    FloatColor clippedFrameColor = color;
    if (cameraView && cameraProj) {
      const std::array<QVector3D, 4> frameWorld{
          QVector3D(tl.x, tl.y, tl.z), QVector3D(tr.x, tr.y, tr.z),
          QVector3D(br.x, br.y, br.z), QVector3D(bl.x, bl.y, bl.z)};
      int visibleCorners = 0;
      for (const QVector3D &worldPoint : frameWorld) {
        const QVector4D clip = (*cameraProj) * (*cameraView) *
                               QVector4D(worldPoint, 1.0f);
        if (!std::isfinite(clip.w()) || clip.w() <= 0.0f) {
          continue;
        }
        const float ndcZ = clip.z() / clip.w();
        if (std::isfinite(ndcZ) && ndcZ >= -1.0f && ndcZ <= 1.0f) {
          ++visibleCorners;
        }
      }
      if (visibleCorners == 0) {
        return;
      }
      if (visibleCorners < static_cast<int>(frameWorld.size())) {
        clippedFrameColor = FloatColor{color.r(), color.g(), color.b(),
                                       color.a() * 0.48f};
      }
    }
    if (cameraView && cameraProj) {
      renderer->set3DCameraMatrices(*cameraView, *cameraProj);
    }
    // Thick 3D lines are expanded as world-space quads and become uneven at
    // low zoom or oblique angles. Native one-pixel lines stay aligned with the
    // 3D handle quads and keep stable coverage across camera orientations.
    const auto edge = [&](const Detail::float3 &a,
                          const Detail::float3 &b) {
      renderer->draw3DLine(a, b, clippedFrameColor, 1.0f);
    };
    edge(tl, tr);
    edge(tr, br);
    edge(br, bl);
    edge(bl, tl);

    // Keep the optional reference-mark path, but leave it disabled by default:
    // an interactive-looking X inside the frame is easily mistaken for a
    // draggable handle. It can still be enabled for diagnostics/configuration.
    static const bool showDiagonals =
        ArtifactCore::LayeredConfigStore::instance()
            .value(QStringLiteral("Viewport/ProjectedFrame/ShowDiagonals"))
            .toBool();
    if (showDiagonals) {
      const FloatColor diagonalColor{color.r(), color.g(), color.b(),
                                     color.a() * 0.28f};
      renderer->draw3DLine(tl, br, diagonalColor, 1.0f);
      renderer->draw3DLine(tr, bl, diagonalColor, 1.0f);
    }

    // A projected border alone is easy to confuse with the composition outline.
    // Add plane-aligned corner handles so the 3D frame remains identifiable and
    // readable at oblique view angles.
    const qreal handleSize = projectedHandleSize > 0.0f
        ? std::clamp(static_cast<qreal>(projectedHandleSize), 4.0, 96.0)
        : std::clamp(std::min(localBounds.width(), localBounds.height()) *
                         0.035,
                     20.0, 48.0);
    const qreal handleHalf = handleSize * 0.5;
    const qreal shadowHalf = handleHalf + std::max<qreal>(2.0, handleSize * 0.12);
    const auto handle = [&](qreal x, qreal y) {
      if (!isVisibleInCamera(x, y)) {
        return;
      }
      const auto quad = [&](qreal half, const FloatColor &fill) {
        renderer->draw3DQuad(
            point(static_cast<float>(x - half), static_cast<float>(y - half)),
            point(static_cast<float>(x + half), static_cast<float>(y - half)),
            point(static_cast<float>(x + half), static_cast<float>(y + half)),
            point(static_cast<float>(x - half), static_cast<float>(y + half)),
            fill);
      };
      quad(shadowHalf, shadow);
      quad(handleHalf, clippedFrameColor);
    };
    if (showScaleHandles) {
      handle(bounds.left(), bounds.top());
      handle(bounds.right(), bounds.top());
      handle(bounds.right(), bounds.bottom());
      handle(bounds.left(), bounds.bottom());
    }
    const qreal edgeHandleHalf = handleHalf * 0.85;
    const auto edgeHandle = [&](qreal x, qreal y) {
      if (!isVisibleInCamera(x, y)) {
        return;
      }
      const auto quad = [&](qreal half, const FloatColor &fill) {
        renderer->draw3DQuad(
            point(static_cast<float>(x - half), static_cast<float>(y - half)),
            point(static_cast<float>(x + half), static_cast<float>(y - half)),
            point(static_cast<float>(x + half), static_cast<float>(y + half)),
            point(static_cast<float>(x - half), static_cast<float>(y + half)),
            fill);
      };
      quad(edgeHandleHalf + 2.0, shadow);
      quad(edgeHandleHalf, clippedFrameColor);
    };
    if (showScaleHandles) {
      edgeHandle(bounds.center().x(), bounds.top());
      edgeHandle(bounds.center().x(), bounds.bottom());
      edgeHandle(bounds.left(), bounds.center().y());
      edgeHandle(bounds.right(), bounds.center().y());
    }

    // Rotation handle: keep it above the top edge with a stable local-space
    // offset so it stays separated from the top resize handle after projection.
    const qreal rotationOffset = std::clamp(
        std::min(localBounds.width(), localBounds.height()) * 0.08, 28.0, 64.0);
    const qreal rotationX = bounds.center().x();
    const qreal rotationY = bounds.top() - rotationOffset;
    const auto rotationHandle = [&](qreal x, qreal y) {
      if (!isVisibleInCamera(x, y)) {
        return;
      }
      const qreal rotationHalf = std::max<qreal>(8.0, edgeHandleHalf * 0.9);
      const auto quad = [&](qreal half, const FloatColor &fill) {
        renderer->draw3DQuad(
            point(static_cast<float>(x - half), static_cast<float>(y - half)),
            point(static_cast<float>(x + half), static_cast<float>(y - half)),
            point(static_cast<float>(x + half), static_cast<float>(y + half)),
            point(static_cast<float>(x - half), static_cast<float>(y + half)),
            fill);
      };
      quad(rotationHalf + 2.0, shadow);
      quad(rotationHalf, color);
      renderer->draw3DLine(
          point(static_cast<float>(bounds.center().x()),
                static_cast<float>(bounds.center().y())),
          point(static_cast<float>(rotationX), static_cast<float>(rotationY)),
          clippedFrameColor, std::max(1.0f, safeThickness));
    };
    if (showRotationHandle) {
      rotationHandle(rotationX, rotationY);
    }
    renderer->flushGizmo3D();
    if (cameraView && cameraProj) {
      renderer->reset3DCameraMatrices();
    }
    return;
  }

  const QRectF bounds = localBounds;
  const QTransform transform = layer->getGlobalTransform();
  const QPointF tl = transform.map(bounds.topLeft());
  const QPointF tr = transform.map(bounds.topRight());
  const QPointF br = transform.map(bounds.bottomRight());
  const QPointF bl = transform.map(bounds.bottomLeft());
  const auto edge = [&](const QPointF &a, const QPointF &b) {
    renderer->drawSolidLine({static_cast<float>(a.x()), static_cast<float>(a.y())},
                            {static_cast<float>(b.x()), static_cast<float>(b.y())},
                            shadow, safeThickness + 1.6f);
    renderer->drawSolidLine({static_cast<float>(a.x()), static_cast<float>(a.y())},
                            {static_cast<float>(b.x()), static_cast<float>(b.y())},
                            color, safeThickness);
  };
  edge(tl, tr);
  edge(tr, br);
  edge(br, bl);
  edge(bl, tl);
}

void drawClonerFrameOverlay(ArtifactIRenderer *renderer,
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

  const FloatColor outerColor{0.96f, 0.56f, 0.18f, 0.90f};
  const FloatColor innerColor{0.18f, 0.10f, 0.04f, 0.64f};

  renderer->drawSolidLine({static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                          {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                          outerColor, 1.7f);
  renderer->drawSolidLine({static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                          {static_cast<float>(br.x()), static_cast<float>(br.y())},
                          outerColor, 1.7f);
  renderer->drawSolidLine({static_cast<float>(br.x()), static_cast<float>(br.y())},
                          {static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                          outerColor, 1.7f);
  renderer->drawSolidLine({static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                          {static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                          outerColor, 1.7f);
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
}

void drawCameraSelectionOverlay(ArtifactIRenderer *renderer,
                                const ArtifactAbstractLayerPtr &layer,
                                bool isActiveCamera)
{
  if (!renderer || !layer) {
    return;
  }

  const auto camera = ArtifactCore::dynamicPointerCast<ArtifactCameraLayer>(layer);
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
                               : QStringLiteral("%1mm | FOV %2")
                                     .arg(camera->focalLength(), 0, 'f', 1)
                                     .arg(camera->fov(), 0, 'f', 1);
  const QString dofText =
      camera->depthOfField() ? QStringLiteral("DOF On") : QStringLiteral("DOF Off");
  const QString motionBlurText =
      camera->motionBlur()
          ? QStringLiteral("MB %1%").arg(camera->blurAmount(), 0, 'f', 0)
          : QStringLiteral("MB Off");

  renderer->drawText(QRectF(panelAnchor.x() + 12.0, panelAnchor.y() + 6.0,
                            154.0, 16.0),
                     QStringLiteral("Camera"), titleFont,
                     isActiveCamera ? FloatColor{0.88f, 0.98f, 0.92f, 1.0f}
                                    : FloatColor{0.90f, 0.94f, 0.98f, 1.0f},
                     Qt::AlignLeft | Qt::AlignVCenter);
  renderer->drawText(QRectF(panelAnchor.x() + 12.0, panelAnchor.y() + 22.0,
                            154.0, 14.0),
                     QStringLiteral("%1 | %2 | %3 | %4")
                         .arg(modeText, lensText, dofText, motionBlurText),
                     detailFont,
                     isActiveCamera ? FloatColor{0.74f, 0.94f, 0.82f, 1.0f}
                                    : FloatColor{0.74f, 0.82f, 0.90f, 1.0f},
                     Qt::AlignLeft | Qt::AlignVCenter);
}

void drawCameraPoiOverlay(ArtifactIRenderer *renderer,
                          const ArtifactAbstractLayerPtr &layer,
                          const QMatrix4x4 &cameraView,
                          const QMatrix4x4 &cameraProj)
{
  if (!renderer || !layer) {
    return;
  }
  const auto camera = ArtifactCore::dynamicPointerCast<ArtifactCameraLayer>(layer);
  if (!camera || !camera->pointOfInterestEnabled()) {
    return;
  }

  // The POI is authored in the camera's parent (usually world) space, which
  // matches the space the camera transform maps into.
  const QVector3D poi = camera->pointOfInterest();
  renderer->set3DCameraMatrices(cameraView, cameraProj);

  const FloatColor shadow{0.02f, 0.03f, 0.04f, 0.85f};
  const FloatColor poiColor{0.55f, 0.95f, 1.0f, 0.98f};
  constexpr float kCrossExtent = 12.0f;
  const auto line = [&](float ax, float ay, float az,
                        float bx, float by, float bz, float thickness) {
    renderer->draw3DLine({ax, ay, az}, {bx, by, bz}, shadow, thickness * 2.4f);
    renderer->draw3DLine({ax, ay, az}, {bx, by, bz}, poiColor, thickness);
  };
  line(poi.x() - kCrossExtent, poi.y(), poi.z(),
       poi.x() + kCrossExtent, poi.y(), poi.z(), 1.6f);
  line(poi.x(), poi.y() - kCrossExtent, poi.z(),
       poi.x(), poi.y() + kCrossExtent, poi.z(), 1.6f);
  line(poi.x(), poi.y(), poi.z() - kCrossExtent,
       poi.x(), poi.y(), poi.z() + kCrossExtent, 1.6f);
  renderer->drawGizmoRing(float3{poi.x(), poi.y(), poi.z()},
                          float3{0.0f, 0.0f, 1.0f}, kCrossExtent * 0.8f,
                          poiColor, 1.6f);

  renderer->flushGizmo3D();
  renderer->reset3DCameraMatrices();
}

void drawSelectionSummaryOverlay(ArtifactIRenderer *renderer,
                                 const ArtifactAbstractLayerPtr &layer,
                                 int selectedCount,
                                 const QStringList &selectedNames,
                                 int overlayW,
                                 int overlayH);

void drawSelectionSummaryOverlay(ArtifactIRenderer *renderer,
                                 const ArtifactAbstractLayerPtr &layer,
                                 int overlayW,
                                 int overlayH)
{
  drawSelectionSummaryOverlay(renderer, layer, 1, QStringList{}, overlayW,
                              overlayH);
}

void drawSelectionSummaryOverlay(ArtifactIRenderer *renderer,
                                 const ArtifactAbstractLayerPtr &layer,
                                 int selectedCount,
                                 const QStringList &selectedNames,
                                 int overlayW,
                                 int overlayH)
{
  if (!renderer || !layer) {
    return;
  }

  QFont font = QApplication::font();
  font.setPointSizeF(std::max(9.0, static_cast<double>(font.pointSizeF())));
  const QFontMetrics fm(font);
  const QString layerName =
      layer->layerName().trimmed().isEmpty() ? QStringLiteral("Selection")
                                             : layer->layerName().trimmed();
  QStringList lines;
  if (selectedCount > 1) {
    const QString title =
        QStringLiteral("%1 layers selected").arg(selectedCount);
    lines << title;

    QStringList compactNames = selectedNames;
    compactNames.removeAll(QString());
    if (compactNames.isEmpty()) {
      compactNames << layerName;
    }
    const QString namesLine =
        QStringLiteral("Current: %1").arg(compactNames.join(QStringLiteral(", ")));
    lines << namesLine;
    lines << QStringLiteral("Duplicate and transform will affect the whole selection");
  } else {
    lines << layerName;
    const QString detail = layerOverlayDetailText(layer);
    if (!detail.isEmpty()) {
      lines << detail;
    }
  }

  const int lineHeight = fm.height();
  int contentWidth = 0;
  for (const QString &line : lines) {
    contentWidth = std::max(contentWidth, fm.horizontalAdvance(line));
  }
  const int lineCount = std::max(1, static_cast<int>(lines.size()));
  QRect labelRect(12, overlayH - (lineHeight * lineCount + 24), contentWidth + 24,
                  lineHeight * lineCount + 12);
  if (labelRect.bottom() > overlayH - 12) {
    labelRect.moveBottom(overlayH - 12);
  }
  if (labelRect.top() < 12) {
    labelRect.moveTop(12);
  }
  if (labelRect.right() > overlayW - 8) {
    labelRect.moveRight(overlayW - 8);
  }

  renderer->drawOverlayPanel(static_cast<float>(labelRect.left()),
                             static_cast<float>(labelRect.top()),
                             static_cast<float>(labelRect.width()),
                             static_cast<float>(labelRect.height()),
                             FloatColor{0.05f, 0.07f, 0.10f, 0.88f},
                             selectedCount > 1
                                 ? FloatColor{0.96f, 0.62f, 0.20f, 0.92f}
                                 : FloatColor{0.24f, 0.76f, 1.0f, 0.90f});
  for (int i = 0; i < lines.size(); ++i) {
    const QRect lineRect = labelRect.adjusted(10, 6 + lineHeight * i, -10, -6);
    const FloatColor textColor =
        i == 0 ? FloatColor{0.92f, 0.96f, 1.0f, 1.0f}
               : (i == lines.size() - 1 && selectedCount > 1)
                     ? FloatColor{1.0f, 0.86f, 0.68f, 1.0f}
                     : FloatColor{0.72f, 0.79f, 0.86f, 1.0f};
    const QString displayLine =
        i == 0
            ? QStringLiteral("%1 %2")
                  .arg(overlayDebugTag(QStringLiteral("SEL")),
                       fm.elidedText(lines[i], Qt::ElideRight,
                                     std::max(0, lineRect.width() - 56)))
            : fm.elidedText(lines[i], Qt::ElideRight, lineRect.width());
    renderer->drawText(lineRect,
                       displayLine,
                       font, textColor, Qt::AlignLeft | Qt::AlignTop);
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

  QFont font = QApplication::font();
  font.setPointSizeF(std::max(9.0, static_cast<double>(font.pointSizeF())));
  const QFontMetrics fm(font);
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

  renderer->drawRoundedPanel(static_cast<float>(labelRect.left()),
                             static_cast<float>(labelRect.top()),
                             static_cast<float>(labelRect.width()),
                             static_cast<float>(labelRect.height()),
                             7.0f,
                             FloatColor{0.03f, 0.04f, 0.06f, 0.82f},
                             FloatColor{0.18f, 0.86f, 0.48f, 0.92f},
                             1.0f,
                             1.0f);
  renderer->drawText(labelRect.adjusted(10, 6, -10, -6),
                     QStringLiteral("%1 %2").arg(overlayDebugTag(QStringLiteral("INFO")), title),
                     font,
                     FloatColor{0.92f, 0.96f, 1.0f, 1.0f},
                     Qt::AlignLeft | Qt::AlignTop);
  if (!detail.isEmpty()) {
    const QRect detailRect = labelRect.adjusted(10, 6 + lineHeight, -10, -6);
    renderer->drawText(detailRect, fm.elidedText(detail, Qt::ElideRight, detailRect.width()),
                       font, FloatColor{0.70f, 0.75f, 0.80f, 1.0f},
                       Qt::AlignLeft | Qt::AlignTop);
  }
  (void)restoreCanvasSize;
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

  QFont font = QApplication::font();
  font.setPointSizeF(std::max(9.0, static_cast<double>(font.pointSizeF())));
  const QFontMetrics fm(font);
  const int chipH = fm.height() + 12;
  const int chipW = fm.horizontalAdvance(statusText) + 24;
  QRect chipRect(overlayW - chipW - 12, 12, chipW, chipH);
  if (chipRect.left() < 8) {
    chipRect.setLeft(8);
  }

  renderer->drawRoundedPanel(static_cast<float>(chipRect.left()),
                             static_cast<float>(chipRect.top()),
                             static_cast<float>(chipRect.width()),
                             static_cast<float>(chipRect.height()),
                             10.0f,
                             FloatColor{0.05f, 0.06f, 0.09f, 0.82f},
                             FloatColor{0.84f, 0.58f, 0.22f, 0.90f},
                             1.0f,
                             1.0f);
  renderer->drawText(chipRect,
                     QStringLiteral("%1 %2").arg(overlayDebugTag(QStringLiteral("STAT")), statusText),
                     font,
                     FloatColor{0.90f, 0.94f, 0.97f, 1.0f},
                     Qt::AlignCenter);
  (void)restoreCanvasSize;
}

void drawAudioWaveformOverlay(ArtifactIRenderer *renderer,
                              const std::vector<float> &peaks,
                              const std::vector<float> &rms,
                              float overlayW,
                              float overlayH) {
  if (!renderer || peaks.empty() || overlayW <= 0.0f || overlayH <= 0.0f) return;
  const float panelH = std::min(96.0f, std::max(48.0f, overlayH * 0.18f));
  const float panelY = overlayH - panelH - 12.0f;
  renderer->drawOverlayPanel(
      12.0f, panelY, overlayW - 24.0f, panelH,
      FloatColor{0.03f, 0.05f, 0.08f, 0.78f},
      FloatColor{0.35f, 0.75f, 1.0f, 0.9f}, 1.0f);

  const float left = 20.0f;
  const float right = std::max(left + 1.0f, overlayW - 20.0f);
  const float centerY = panelY + panelH * 0.5f;
  const float halfHeight = panelH * 0.38f;
  std::vector<Detail::float2> peakPoints;
  peakPoints.reserve(peaks.size());
  for (std::size_t i = 0; i < peaks.size(); ++i) {
    const float t = peaks.size() > 1
                        ? static_cast<float>(i) / static_cast<float>(peaks.size() - 1)
                        : 0.0f;
    const float amplitude = std::clamp(peaks[i], 0.0f, 1.0f);
    peakPoints.push_back({left + (right - left) * t,
                          centerY - halfHeight * amplitude});
  }
  renderer->drawPolyline(peakPoints, FloatColor{0.35f, 0.85f, 1.0f, 0.95f}, 1.5f);
  if (rms.size() == peaks.size()) {
    std::vector<Detail::float2> rmsPoints;
    rmsPoints.reserve(rms.size());
    for (std::size_t i = 0; i < rms.size(); ++i) {
      const float t = rms.size() > 1
                          ? static_cast<float>(i) / static_cast<float>(rms.size() - 1)
                          : 0.0f;
      rmsPoints.push_back({left + (right - left) * t,
                           centerY + halfHeight * std::clamp(rms[i], 0.0f, 1.0f)});
    }
    renderer->drawPolyline(rmsPoints, FloatColor{0.25f, 1.0f, 0.55f, 0.78f}, 1.0f);
  }
}

void drawAudioSpectrumOverlay(ArtifactIRenderer *renderer,
                              const std::vector<float> &spectrum,
                              float overlayW,
                              float overlayH) {
  if (!renderer || spectrum.empty() || overlayW <= 0.0f || overlayH <= 0.0f) return;
  const float panelW = std::min(220.0f, std::max(120.0f, overlayW * 0.24f));
  const float panelH = 82.0f;
  const float panelX = overlayW - panelW - 12.0f;
  const float panelY = 12.0f;
  renderer->drawOverlayPanel(
      panelX, panelY, panelW, panelH,
      FloatColor{0.03f, 0.05f, 0.08f, 0.78f},
      FloatColor{0.75f, 0.45f, 1.0f, 0.9f}, 1.0f);
  const float barW = std::max(1.0f, (panelW - 16.0f) / static_cast<float>(spectrum.size()));
  for (std::size_t i = 0; i < spectrum.size(); ++i) {
    const float magnitude = std::clamp(spectrum[i], 0.0f, 1.0f);
    const float height = (panelH - 16.0f) * magnitude;
    renderer->drawSolidRect(panelX + 8.0f + static_cast<float>(i) * barW,
                            panelY + panelH - 8.0f - height,
                            std::max(0.5f, barW - 1.0f), height,
                            FloatColor{0.65f, 0.35f, 1.0f, 0.9f});
  }
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

  QFont font = QApplication::font();
  font.setPointSizeF(std::max(9.0, static_cast<double>(font.pointSizeF())));

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

  const QFontMetrics fm(font);
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

  renderer->drawRoundedPanel(static_cast<float>(labelRect.left()),
                             static_cast<float>(labelRect.top()),
                             static_cast<float>(labelRect.width()),
                             static_cast<float>(labelRect.height()),
                             7.0f,
                             FloatColor{0.03f, 0.04f, 0.06f, 0.82f},
                             FloatColor{0.88f, 0.34f, 0.78f, 0.92f},
                             1.0f,
                             1.0f);
  renderer->drawText(labelRect.adjusted(10, 6, -10, -6),
                     QStringLiteral("%1 %2").arg(overlayDebugTag(QStringLiteral("SNAP")), title),
                     font,
                     FloatColor{0.92f, 0.96f, 1.0f, 1.0f},
                     Qt::AlignLeft | Qt::AlignTop);
  const QRect detailRect = labelRect.adjusted(10, 6 + lineHeight, -10, -6);
  renderer->drawText(detailRect, fm.elidedText(snapDetail, Qt::ElideRight, detailRect.width()),
                     font, FloatColor{0.70f, 0.75f, 0.80f, 1.0f},
                     Qt::AlignLeft | Qt::AlignTop);
  (void)restoreCanvasSize;
}

namespace {
[[maybe_unused]] auto* const kForceCommandPaletteOverlayLink =
    &Artifact::drawViewportCommandPaletteOverlay;
[[maybe_unused]] auto* const kForceContextMenuOverlayLink =
    &Artifact::drawViewportContextMenuOverlay;
[[maybe_unused]] auto* const kForcePieMenuOverlayLink =
    &Artifact::drawViewportPieMenuOverlay;
} // namespace


void drawPaintLayerOnionSkinOverlay(ArtifactIRenderer *renderer,
                                     const ArtifactAbstractLayerPtr &paintLayer,
                                     const ArtifactCompositionPtr &comp,
                                     float overlayW, float overlayH,
                                     int frameCount, int opacityPercent)
{
    if (!renderer || !paintLayer || !comp) return;
    auto* paint = dynamic_cast<ArtifactPaintLayer*>(paintLayer.get());
    if (!paint) return;
    if (frameCount <= 0) return;

    const float alpha = std::clamp(opacityPercent / 100.0f, 0.05f, 0.8f);
    auto currentFrame = comp->framePosition();
    auto bounds = paintLayer->localBounds();

    for (int i = -frameCount; i <= frameCount; ++i) {
        if (i == 0) continue;
        FramePosition f(std::max<int64_t>(0, currentFrame.framePosition() + i));
        auto* buf = paint->frameBuffer(f);
        if (!buf || buf->isEmpty()) continue;

        float fade = 1.0f - static_cast<float>(std::abs(i)) / (frameCount + 1);
        const QImage frameImage = buf->toQImage();
        renderer->drawSprite(
            static_cast<float>(bounds.x()),
            static_cast<float>(bounds.y()),
            static_cast<float>(bounds.width()),
            static_cast<float>(bounds.height()),
            frameImage, alpha * fade);
    }
}
} // namespace Artifact
