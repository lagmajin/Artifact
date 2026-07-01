module;
#include <DeviceContext.h>
#define NOMINMAX
#define QT_NO_KEYWORDS

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QDebug>
#include <QElapsedTimer>
#include <QFont>
#include <QFontMetrics>
#include <QFuture>
#include <QHash>
#include <QImage>
#include <QLinearGradient>
#include <QLoggingCategory>
#include <QMatrix4x4>
#include <QMetaObject>
#include <QMutex>
#include <QPainter>
#include <QPointer>
#include <QRectF>
#include <QSizeF>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QTransform>
#include <QVector3D>
#include <QVector4D>
#include <QVector>
#include <deque>
#include <QtConcurrent>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <utility>
#include <vector>
#include <wobjectimpl.h>

module Artifact.Widgets.CompositionRenderController;

import Artifact.Render.IRenderer;
import Artifact.Grid.System;
import Artifact.Render.GPUTextureCacheManager;
import Artifact.Render.CompositionViewDrawing;
import Artifact.Render.CompositionRenderer;
import Artifact.Render.Queue.Service;
import Artifact.Render.Config;
import Artifact.Render.ROI;
import Artifact.Render.Context;
import Artifact.Widgets.CompositionRenderOverlay;
import Artifact.Preview.Pipeline;
import Core.Camera;
import Frame.Debug;
import Core.Diagnostics.Trace;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Clone;
import Artifact.Layer.CloneEffectSupport;
import Artifact.Layer.Camera;
import Artifact.Layer.Light;
import Core.Light;
import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Video;
import Video.VideoFrame;
import Artifact.Layer.Solid2D;
import Artifact.Layers.SolidImage;
import Artifact.Layer.Text;
import Artifact.Layer.Composition;
import Artifact.Layer.Shape;
import Layer.Matte;
import Artifact.Layers.Model3D;
import Artifact.Render.Offscreen;
import Image.ImageF32x4_RGBA;
import Frame.Position;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Service.ActiveContext;
import Artifact.Widgets.TransformGizmo;
import Artifact.Widgets.TextGizmo;
import Artifact.Widgets.Gizmo3D;
import Artifact.Widgets.PointTrackerGizmo;
import Artifact.Tool.PointTracker;
import Tracking.MotionTracker;
import Track.NccTracker;
import Artifact.Render.OffscreenComposition;
import Artifact.Widgets.PieMenu;
import UI.View.Orientation.Navigator;
import Geometry.CameraGuide;
import Math.Interpolate;
import Artifact.Tool.Manager;
import Artifact.Tool.MotionSketchTool;
import Artifact.Tool.PuppetTool;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Project.Health;
import Utils.Id;
import Time.Rational;
import Artifact.Render.Pipeline;
import Graphics.LayerBlendPipeline;
import Graphics.GPUcomputeContext;
import Widgets.Utils.CSS;
import Core.Diagnostics.Trace;

import Artifact.Service.Project;
import Artifact.Service.Playback; // 追加
import Application.AppSettings;
import Playback.State;
import Thread.Helper;
import Thread.PreciseTicker;
import Event.Bus;
import Artifact.Event.Types;
import Undo.UndoManager;
import Frame.Position;
import Color.Float;
import Image;
import CvUtils;
import ArtifactCore.Utils.PerformanceProfiler;

namespace Artifact {

W_OBJECT_IMPL(CompositionRenderController)

bool isLayerEffectivelyVisible(const ArtifactAbstractLayerPtr &layer);

namespace {
Q_LOGGING_CATEGORY(compositionViewLog, "artifact.compositionview")

EffectContext makeControllerEffectContext(ArtifactAbstractLayer* layer, const QRectF& roi = QRectF()) {
  EffectContext ctx;
  ctx.roi = roi;
  ctx.isInteractive = true;
  if (!layer) {
    return ctx;
  }

  if (auto* composition =
          static_cast<ArtifactAbstractComposition*>(layer->composition())) {
    const auto compositionFrame = composition->framePosition().framePosition();
    ctx.compositionFrame = compositionFrame;
    ctx.layerFrame = compositionFrame - layer->inPoint().framePosition() +
                     layer->startTime().framePosition();
    ctx.frameRate = composition->frameRate().framerate();
    if (ctx.frameRate > 0.0) {
      ctx.timeSeconds = static_cast<double>(compositionFrame) / ctx.frameRate;
    }
  } else {
    ctx.layerFrame = layer->currentFrame();
    ctx.compositionFrame = ctx.layerFrame;
  }

  return ctx;
}

QImage ensurePreviewImage(const QImage& image) {
  if (image.isNull()) {
    return {};
  }
  return image.format() == QImage::Format_RGBA8888
             ? image
             : image.convertToFormat(QImage::Format_RGBA8888);
}

QString previewPixelStats(const QImage& source) {
  const QImage image = ensurePreviewImage(source);
  if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
    return QStringLiteral("state=unavailable");
  }

  constexpr qint64 kMaxSamples = 65536;
  const qint64 pixelCount =
      static_cast<qint64>(image.width()) * static_cast<qint64>(image.height());
  const int step =
      std::max(1, static_cast<int>(std::ceil(
                      std::sqrt(static_cast<double>(pixelCount) /
                                static_cast<double>(kMaxSamples)))));
  std::array<int, 4> minimum = {255, 255, 255, 255};
  std::array<int, 4> maximum = {0, 0, 0, 0};
  qint64 samples = 0;
  qint64 rgbZero = 0;
  qint64 rgbFull = 0;
  qint64 blueDominant = 0;

  for (int y = 0; y < image.height(); y += step) {
    const auto* row = image.constScanLine(y);
    for (int x = 0; x < image.width(); x += step) {
      const auto* pixel = row + x * 4;
      for (int channel = 0; channel < 4; ++channel) {
        const int value = pixel[channel];
        minimum[channel] = std::min(minimum[channel], value);
        maximum[channel] = std::max(maximum[channel], value);
      }
      rgbZero += (pixel[0] == 0) + (pixel[1] == 0) + (pixel[2] == 0);
      rgbFull += (pixel[0] == 255) + (pixel[1] == 255) + (pixel[2] == 255);
      if (pixel[2] >= 240 && pixel[2] >= pixel[0] + 32 &&
          pixel[2] >= pixel[1] + 32) {
        ++blueDominant;
      }
      ++samples;
    }
  }

  const double rgbDenominator =
      std::max(1.0, static_cast<double>(samples) * 3.0);
  const double sampleDenominator =
      std::max(1.0, static_cast<double>(samples));
  return QStringLiteral(
             "state=sampled domain=post-readback-sdr samples=%1 step=%2 "
             "min=%3,%4,%5,%6 max=%7,%8,%9,%10 "
             "rgbZeroPct=%11 rgbFullPct=%12 blueDominantPct=%13")
      .arg(samples)
      .arg(step)
      .arg(minimum[0])
      .arg(minimum[1])
      .arg(minimum[2])
      .arg(minimum[3])
      .arg(maximum[0])
      .arg(maximum[1])
      .arg(maximum[2])
      .arg(maximum[3])
      .arg(QString::number(100.0 * static_cast<double>(rgbZero) /
                               rgbDenominator,
                           'f', 2))
      .arg(QString::number(100.0 * static_cast<double>(rgbFull) /
                               rgbDenominator,
                           'f', 2))
      .arg(QString::number(100.0 * static_cast<double>(blueDominant) /
                               sampleDenominator,
                           'f', 2));
}

QImage makePreviewDiffImage(const QImage& before, const QImage& after) {
  if (before.isNull() || after.isNull() || before.size() != after.size()) {
    return {};
  }

  const QImage lhs = ensurePreviewImage(before);
  const QImage rhs = ensurePreviewImage(after);
  if (lhs.isNull() || rhs.isNull() || lhs.size() != rhs.size()) {
    return {};
  }

  QImage diff(lhs.size(), QImage::Format_RGBA8888);
  for (int y = 0; y < lhs.height(); ++y) {
    const QRgb* lhsRow = reinterpret_cast<const QRgb*>(lhs.constScanLine(y));
    const QRgb* rhsRow = reinterpret_cast<const QRgb*>(rhs.constScanLine(y));
    QRgb* outRow = reinterpret_cast<QRgb*>(diff.scanLine(y));
    for (int x = 0; x < lhs.width(); ++x) {
      outRow[x] = qRgba(std::abs(qRed(lhsRow[x]) - qRed(rhsRow[x])),
                        std::abs(qGreen(lhsRow[x]) - qGreen(rhsRow[x])),
                        std::abs(qBlue(lhsRow[x]) - qBlue(rhsRow[x])),
                        255);
    }
  }
  return diff;
}

void addSnapshotPreview(ArtifactCore::FrameDebugSnapshot& snapshot,
                        const QString& key, const QString& label,
                        const QString& note, const QImage& beforeImage,
                        const QImage& afterImage, const QImage& alphaImage) {
  ArtifactCore::FrameDebugImagePreviewRecord preview;
  preview.key = key;
  preview.label = label;
  preview.note = note;
  preview.beforeImage = ensurePreviewImage(beforeImage);
  preview.afterImage = ensurePreviewImage(afterImage);
  preview.alphaImage = alphaImage;
  preview.diffImage =
      makePreviewDiffImage(preview.beforeImage, preview.afterImage);
  snapshot.previews.push_back(std::move(preview));
}

void drawClonerFrameOverlay(ArtifactIRenderer* renderer,
                            const ArtifactAbstractLayerPtr& layer)
{
  if (!renderer || !layer) {
    return;
  }

  const auto cloneLayer = std::dynamic_pointer_cast<ArtifactCloneLayer>(layer);
  if (!cloneLayer) {
    return;
  }

  const QRectF localBounds = layer->localBounds();
  if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
      localBounds.height() <= 0.0) {
    return;
  }

  const auto clones = cloneLayer->generateCloneData();
  if (clones.empty()) {
    return;
  }

  const QTransform globalTransform = layer->getGlobalTransform();
  const FloatColor outerColor{0.96f, 0.56f, 0.18f, 0.90f};
  const FloatColor innerColor{0.18f, 0.10f, 0.04f, 0.64f};

  const auto mapClonePoint = [&](const QMatrix4x4& cloneTransform,
                                 const QPointF& point) -> QPointF {
    const QVector4D mapped =
        cloneTransform * QVector4D(static_cast<float>(point.x()),
                                   static_cast<float>(point.y()), 0.0f, 1.0f);
    return globalTransform.map(
        QPointF(static_cast<qreal>(mapped.x()), static_cast<qreal>(mapped.y())));
  };

  for (const auto& clone : clones) {
    if (!clone.visible) {
      continue;
    }

    const QPointF tl = mapClonePoint(clone.transform, localBounds.topLeft());
    const QPointF tr = mapClonePoint(clone.transform, localBounds.topRight());
    const QPointF br =
        mapClonePoint(clone.transform, localBounds.bottomRight());
    const QPointF bl = mapClonePoint(clone.transform, localBounds.bottomLeft());

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
}

enum class QuickMaskPreset {
  Full = 0,
  LeftHalf,
  RightHalf,
  TopHalf,
  BottomHalf,
  CenterHalf,
  QuarterTL,
  QuarterTR,
  Count
};

constexpr std::array<QuickMaskPreset, static_cast<size_t>(QuickMaskPreset::Count)> kQuickMaskPresetOrder = {
    QuickMaskPreset::Full,
    QuickMaskPreset::LeftHalf,
    QuickMaskPreset::RightHalf,
    QuickMaskPreset::TopHalf,
    QuickMaskPreset::BottomHalf,
    QuickMaskPreset::CenterHalf,
    QuickMaskPreset::QuarterTL,
    QuickMaskPreset::QuarterTR,
};

QRectF quickMaskPresetRect(QuickMaskPreset preset, const QRectF &bounds)
{
  const QRectF n = bounds.normalized();
  const qreal x = n.x();
  const qreal y = n.y();
  const qreal w = n.width();
  const qreal h = n.height();
  const qreal halfW = w * 0.5;
  const qreal halfH = h * 0.5;
  switch (preset) {
  case QuickMaskPreset::Full:
    return n;
  case QuickMaskPreset::LeftHalf:
    return QRectF(x, y, halfW, h);
  case QuickMaskPreset::RightHalf:
    return QRectF(x + halfW, y, halfW, h);
  case QuickMaskPreset::TopHalf:
    return QRectF(x, y, w, halfH);
  case QuickMaskPreset::BottomHalf:
    return QRectF(x, y + halfH, w, halfH);
  case QuickMaskPreset::CenterHalf:
    return QRectF(x + w * 0.25, y + h * 0.25, halfW, halfH);
  case QuickMaskPreset::QuarterTL:
    return QRectF(x, y, halfW, halfH);
  case QuickMaskPreset::QuarterTR:
    return QRectF(x + halfW, y, halfW, halfH);
  case QuickMaskPreset::Count:
    break;
  }
  return n;
}

bool renderCrashTraceEnabled()
{
  static const bool enabled =
      qEnvironmentVariableIsSet("ARTIFACT_RENDER_TRACE_CRASH") &&
      qEnvironmentVariable("ARTIFACT_RENDER_TRACE_CRASH") != QStringLiteral("0");
  return enabled;
}

void renderCrashTrace(const char* phase, quint64 frame, const QString& detail = {})
{
  if (!renderCrashTraceEnabled()) {
    return;
  }
  qInfo().noquote() << QStringLiteral("[CompositionView][CrashTrace] frame=%1 phase=%2 %3")
                           .arg(frame)
                           .arg(QString::fromLatin1(phase))
                           .arg(detail);
}

QVector<QPointF> maskSegmentPolyline(const MaskVertex &start,
                                     const MaskVertex &end,
                                     int subdivisions);

constexpr size_t lineDebugKindCount()
{
  return static_cast<size_t>(LineDebugKind::Unknown);
}

size_t lineDebugKindIndex(LineDebugKind kind)
{
  const size_t index = static_cast<size_t>(kind);
  return index < lineDebugKindCount() ? index : lineDebugKindCount();
}

bool lineDebugKindVisible(
    const std::array<bool, lineDebugKindCount()> &visibility,
    LineDebugKind kind)
{
  const size_t index = lineDebugKindIndex(kind);
  if (index >= visibility.size()) {
    return false;
  }
  return visibility[index];
}

void drawTaggedSolidLine(ArtifactIRenderer *renderer, const QPointF &a,
                         const QPointF &b, const FloatColor &color,
                         float thickness, bool enabled)
{
  if (!renderer || !enabled) {
    return;
  }
  renderer->drawSolidLine({static_cast<float>(a.x()), static_cast<float>(a.y())},
                          {static_cast<float>(b.x()), static_cast<float>(b.y())},
                          color, thickness);
}

void drawTaggedRectOutline(ArtifactIRenderer *renderer, const QRectF &rect,
                           const FloatColor &color, bool enabled)
{
  if (!renderer || !enabled || !rect.isValid()) {
    return;
  }
  const QPointF tl = rect.topLeft();
  const QPointF tr = rect.topRight();
  const QPointF br = rect.bottomRight();
  const QPointF bl = rect.bottomLeft();
  drawTaggedSolidLine(renderer, tl, tr, color, 1.0f, enabled);
  drawTaggedSolidLine(renderer, tr, br, color, 1.0f, enabled);
  drawTaggedSolidLine(renderer, br, bl, color, 1.0f, enabled);
  drawTaggedSolidLine(renderer, bl, tl, color, 1.0f, enabled);
}

void drawRectOutline(ArtifactIRenderer *renderer, const QRectF &rect,
                     const FloatColor &color, float thickness)
{
  if (!renderer || !rect.isValid() || rect.width() <= 0.0 ||
      rect.height() <= 0.0) {
    return;
  }

  const QPointF tl = rect.topLeft();
  const QPointF tr = rect.topRight();
  const QPointF br = rect.bottomRight();
  const QPointF bl = rect.bottomLeft();
  renderer->drawSolidLine({static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                          {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                          color, thickness);
  renderer->drawSolidLine({static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                          {static_cast<float>(br.x()), static_cast<float>(br.y())},
                          color, thickness);
  renderer->drawSolidLine({static_cast<float>(br.x()), static_cast<float>(br.y())},
                          {static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                          color, thickness);
  renderer->drawSolidLine({static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                          {static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                          color, thickness);
}

QRectF maskPathCanvasBounds(const MaskPath &path, const QTransform &globalTransform)
{
  QRectF bounds;
  bool hasPoint = false;
  const int vertexCount = path.vertexCount();
  if (vertexCount <= 0) {
    return bounds;
  }

  const auto includePoint = [&bounds, &globalTransform, &hasPoint](const QPointF &point) {
    const QPointF canvasPoint = globalTransform.map(point);
    if (!hasPoint) {
      bounds = QRectF(canvasPoint, QSizeF(0.0, 0.0));
      hasPoint = true;
    } else {
      bounds = bounds.united(QRectF(canvasPoint, QSizeF(0.0, 0.0)));
    }
  };

  for (int v = 0; v < vertexCount; ++v) {
    const MaskVertex vertex = path.vertex(v);
    includePoint(vertex.position);
    includePoint(vertex.position + vertex.inTangent);
    includePoint(vertex.position + vertex.outTangent);
    if (v > 0) {
      const QVector<QPointF> samples =
          maskSegmentPolyline(path.vertex(v - 1), vertex, 18);
      for (const QPointF &sample : samples) {
        includePoint(sample);
      }
    }
  }

  if (path.isClosed() && vertexCount > 1) {
    const QVector<QPointF> samples =
        maskSegmentPolyline(path.vertex(vertexCount - 1), path.vertex(0), 18);
    for (const QPointF &sample : samples) {
      includePoint(sample);
    }
  }

  return bounds.normalized();
}

void drawEffectHitboxOverlay(ArtifactIRenderer *renderer,
                             const ArtifactCompositionPtr &comp,
                             const ArtifactAbstractLayerPtr &selectedLayer)
{
  if (!renderer || !comp || !selectedLayer) {
    return;
  }

  const FloatColor layerColor{1.0f, 0.78f, 0.24f, 0.96f};
  const FloatColor layerFill{1.0f, 0.78f, 0.24f, 0.10f};
  const FloatColor maskColor{0.28f, 0.88f, 1.0f, 0.95f};
  const FloatColor maskFill{0.28f, 0.88f, 1.0f, 0.08f};
  const FloatColor matteColor{1.0f, 0.42f, 0.88f, 0.92f};
  const FloatColor matteFill{1.0f, 0.42f, 0.88f, 0.07f};
  const QTransform globalTransform = selectedLayer->getGlobalTransform();

  const QRectF localBounds = selectedLayer->localBounds();
  if (localBounds.isValid() && localBounds.width() > 0.0 &&
      localBounds.height() > 0.0) {
    const QPointF tl = globalTransform.map(localBounds.topLeft());
    const QPointF tr = globalTransform.map(localBounds.topRight());
    const QPointF br = globalTransform.map(localBounds.bottomRight());
    const QPointF bl = globalTransform.map(localBounds.bottomLeft());
    renderer->drawSolidLine({static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                            {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                            layerColor, 2.2f);
    renderer->drawSolidLine({static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                            {static_cast<float>(br.x()), static_cast<float>(br.y())},
                            layerColor, 2.2f);
    renderer->drawSolidLine({static_cast<float>(br.x()), static_cast<float>(br.y())},
                            {static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                            layerColor, 2.2f);
    renderer->drawSolidLine({static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                            {static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                            layerColor, 2.2f);
    renderer->drawSolidRectTransformed(
        static_cast<float>(localBounds.left()),
        static_cast<float>(localBounds.top()),
        static_cast<float>(localBounds.width()),
        static_cast<float>(localBounds.height()), globalTransform, layerFill,
        1.0f);
  }

  for (int maskIndex = 0; maskIndex < selectedLayer->maskCount(); ++maskIndex) {
    const LayerMask mask = selectedLayer->mask(maskIndex);
    if (!mask.isEnabled()) {
      continue;
    }

    for (int pathIndex = 0; pathIndex < mask.maskPathCount(); ++pathIndex) {
      const QRectF maskBounds =
          maskPathCanvasBounds(mask.maskPath(pathIndex), globalTransform);
      if (!maskBounds.isValid() || maskBounds.width() <= 0.0 ||
          maskBounds.height() <= 0.0) {
        continue;
      }
      renderer->drawSolidRect(static_cast<float>(maskBounds.left()),
                              static_cast<float>(maskBounds.top()),
                              static_cast<float>(maskBounds.width()),
                              static_cast<float>(maskBounds.height()),
                              maskFill, 1.0f);
      drawRectOutline(renderer, maskBounds, maskColor, 1.4f);
    }
  }

  const auto matteRefs = selectedLayer->matteReferences();
  for (const auto &matteRef : matteRefs) {
    if (!matteRef.enabled || matteRef.sourceLayerId.isNil()) {
      continue;
    }

    const auto matteLayer = comp->layerById(matteRef.sourceLayerId);
    if (!matteLayer) {
      continue;
    }

    const QRectF matteBounds = matteLayer->transformedBoundingBox().normalized();
    if (!matteBounds.isValid() || matteBounds.width() <= 0.0 ||
        matteBounds.height() <= 0.0) {
      continue;
    }

    renderer->drawSolidRect(static_cast<float>(matteBounds.left()),
                            static_cast<float>(matteBounds.top()),
                            static_cast<float>(matteBounds.width()),
                            static_cast<float>(matteBounds.height()),
                            matteFill, 1.0f);
    drawRectOutline(renderer, matteBounds, matteColor, 1.6f);
  }
}

double clamp01(double value);

FloatColor densityHeatmapColor(float normalized)
{
  const float t = std::clamp(normalized, 0.0f, 1.0f);
  const FloatColor cool{0.18f, 0.76f, 1.0f, 1.0f};
  const FloatColor mid{1.0f, 0.76f, 0.24f, 1.0f};
  const FloatColor hot{1.0f, 0.24f, 0.22f, 1.0f};
  if (t < 0.5f) {
    const float u = t / 0.5f;
    return FloatColor{
        cool.r() + (mid.r() - cool.r()) * u,
        cool.g() + (mid.g() - cool.g()) * u,
        cool.b() + (mid.b() - cool.b()) * u,
        1.0f};
  }
  const float u = (t - 0.5f) / 0.5f;
  return FloatColor{
      mid.r() + (hot.r() - mid.r()) * u,
      mid.g() + (hot.g() - mid.g()) * u,
      mid.b() + (hot.b() - mid.b()) * u,
      1.0f};
}

QString densityAxisDisplayName(const QString &axis)
{
  if (axis == QStringLiteral("visual")) {
    return QStringLiteral("visual");
  }
  if (axis == QStringLiteral("information")) {
    return QStringLiteral("info");
  }
  if (axis == QStringLiteral("luminance")) {
    return QStringLiteral("luma");
  }
  if (axis == QStringLiteral("motion")) {
    return QStringLiteral("motion");
  }
  return QStringLiteral("density");
}

QString densityWarningForDominantAxis(const QString &axis, double score);
QString densityNextActionForAxis(const QString &axis);

FloatColor densityAxisColor(const QString &axis)
{
  if (axis == QStringLiteral("visual")) {
    return FloatColor{0.18f, 0.76f, 1.0f, 1.0f};
  }
  if (axis == QStringLiteral("information")) {
    return FloatColor{1.0f, 0.76f, 0.24f, 1.0f};
  }
  if (axis == QStringLiteral("luminance")) {
    return FloatColor{0.92f, 0.94f, 0.98f, 1.0f};
  }
  if (axis == QStringLiteral("motion")) {
    return FloatColor{1.0f, 0.24f, 0.22f, 1.0f};
  }
  return FloatColor{0.72f, 0.80f, 0.88f, 1.0f};
}

void drawVisualDensityOverlay(ArtifactIRenderer *renderer,
                              const ArtifactCompositionPtr &comp,
                              const ArtifactAbstractLayerPtr &selectedLayer,
                              const FramePosition &currentFrame)
{
  if (!renderer || !comp) {
    return;
  }

  const QSize compSize = comp->settings().compositionSize();
  const float canvasW = static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);
  const float canvasH = static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);
  if (canvasW <= 0.0f || canvasH <= 0.0f) {
    return;
  }

  constexpr int kColumns = 12;
  constexpr int kRows = 8;
  std::array<float, kColumns * kRows> density{};
  float peakDensity = 0.0f;

  const QRectF canvasRect(0.0, 0.0, canvasW, canvasH);
  const float cellW = canvasW / static_cast<float>(kColumns);
  const float cellH = canvasH / static_cast<float>(kRows);
  const float cellArea = std::max(1.0f, cellW * cellH);

  const auto &layers = comp->allLayerRef();
  int visibleLayerCount = 0;
  int textLayerCount = 0;
  int videoLayerCount = 0;
  int particleLayerCount = 0;
  for (const auto &layer : layers) {
    if (!layer || !isLayerEffectivelyVisible(layer) ||
        !layer->isActiveAt(currentFrame)) {
      continue;
    }

    ++visibleLayerCount;
    if (dynamic_cast<ArtifactTextLayer *>(layer.get())) {
      ++textLayerCount;
    }
    if (dynamic_cast<ArtifactVideoLayer *>(layer.get())) {
      ++videoLayerCount;
    }
    if (layer->isParticleLayer()) {
      ++particleLayerCount;
    }

    const QRectF layerBounds = layer->transformedBoundingBox().normalized();
    if (!layerBounds.isValid() || layerBounds.width() <= 0.0 ||
        layerBounds.height() <= 0.0) {
      continue;
    }

    const QRectF clippedBounds = layerBounds.intersected(canvasRect);
    if (clippedBounds.isEmpty()) {
      continue;
    }

    float weight = 1.0f;
    if (dynamic_cast<ArtifactTextLayer *>(layer.get())) {
      weight += 0.35f;
    }
    weight += std::min(0.65f, static_cast<float>(layer->maskCount()) * 0.10f);
    weight += std::min(0.70f, static_cast<float>(layer->effectCount()) * 0.08f);
    weight += std::min(0.45f,
                       static_cast<float>(layer->matteReferences().size()) * 0.12f);
    if (selectedLayer && selectedLayer->id() == layer->id()) {
      weight += 0.35f;
    }

    const int minCol = std::clamp(
        static_cast<int>(std::floor(clippedBounds.left() / cellW)), 0,
        kColumns - 1);
    const int maxCol = std::clamp(
        static_cast<int>(std::floor((clippedBounds.right() - 1.0) / cellW)), 0,
        kColumns - 1);
    const int minRow = std::clamp(
        static_cast<int>(std::floor(clippedBounds.top() / cellH)), 0,
        kRows - 1);
    const int maxRow = std::clamp(
        static_cast<int>(std::floor((clippedBounds.bottom() - 1.0) / cellH)), 0,
        kRows - 1);

    for (int row = minRow; row <= maxRow; ++row) {
      for (int col = minCol; col <= maxCol; ++col) {
        const QRectF cellRect(static_cast<float>(col) * cellW,
                              static_cast<float>(row) * cellH, cellW, cellH);
        const QRectF overlap = cellRect.intersected(clippedBounds);
        if (overlap.isEmpty()) {
          continue;
        }
        const float coverage =
            static_cast<float>((overlap.width() * overlap.height()) / cellArea);
        const int index = row * kColumns + col;
        density[static_cast<std::size_t>(index)] += weight * coverage;
        peakDensity = std::max(peakDensity, density[static_cast<std::size_t>(index)]);
      }
    }
  }

  const double visualScore =
      clamp01(visibleLayerCount * 0.05 + textLayerCount * 0.03 +
              (selectedLayer ? 0.08 : 0.0) +
              (selectedLayer ? static_cast<double>(selectedLayer->effectCount()) * 0.04
                             : 0.0) +
              (selectedLayer ? static_cast<double>(selectedLayer->maskCount()) * 0.05
                             : 0.0) +
              (selectedLayer ? static_cast<double>(selectedLayer->matteReferences().size()) * 0.05
                             : 0.0));
  const double informationScore =
      clamp01(textLayerCount * 0.06 + layers.size() * 0.02 +
              (visibleLayerCount > 12 ? 0.08 : 0.0));
  const double luminanceScore =
      clamp01(textLayerCount * 0.07 + (selectedLayer ? 0.10 : 0.0));
  const double motionScore =
      clamp01(videoLayerCount * 0.08 + particleLayerCount * 0.10 +
              std::min(1.0, static_cast<double>(layers.size()) / 20.0) * 0.15);
  const std::array<std::pair<const char *, double>, 4> densityScores{{
      {"visual", visualScore},
      {"information", informationScore},
      {"luminance", luminanceScore},
      {"motion", motionScore},
  }};
  const auto dominantIt = std::max_element(
      densityScores.begin(), densityScores.end(),
      [](const auto &a, const auto &b) { return a.second < b.second; });
  const QString dominantAxis = QString::fromLatin1(dominantIt->first);
  const double dominantScore = dominantIt->second;

  if (peakDensity <= 0.0f) {
    return;
  }

  const float panelW = canvasW > 260.0f
                           ? std::min(310.0f, std::max(220.0f, canvasW * 0.28f))
                           : std::max(140.0f, canvasW - 24.0f);
  const float panelH = canvasH > 140.0f ? 126.0f : std::max(92.0f, canvasH - 24.0f);
  const float panelX = std::max(12.0f, canvasW - panelW - 12.0f);
  const float panelY = 12.0f;

  renderer->drawOverlayPanel(panelX, panelY, panelW, panelH,
                             FloatColor{0.05f, 0.07f, 0.11f, 0.90f},
                             FloatColor{0.34f, 0.54f, 0.76f, 0.92f});

  QFont titleFont = QApplication::font();
  titleFont.setPointSizeF(std::max(10.0, static_cast<double>(titleFont.pointSizeF()) + 1.0));
  titleFont.setWeight(QFont::DemiBold);
  QFont detailFont = QApplication::font();
  detailFont.setPointSizeF(std::max(8.5, static_cast<double>(detailFont.pointSizeF())));

  renderer->drawText(QRectF(panelX + 12.0f, panelY + 8.0f, panelW - 24.0f, 18.0f),
                     QStringLiteral("Visual Density Heatmap"), titleFont,
                     FloatColor{0.94f, 0.97f, 1.0f, 1.0f},
                     Qt::AlignLeft | Qt::AlignVCenter);
  renderer->drawText(
      QRectF(panelX + 12.0f, panelY + 26.0f, panelW - 24.0f, 15.0f),
      QStringLiteral("%1 layers  |  dominant: %2 (%3)")
          .arg(static_cast<int>(layers.size()))
          .arg(densityAxisDisplayName(dominantAxis))
          .arg(QString::number(dominantScore * 100.0, 'f', 0)),
      detailFont, FloatColor{0.70f, 0.78f, 0.86f, 1.0f},
      Qt::AlignLeft | Qt::AlignVCenter);
  renderer->drawText(
      QRectF(panelX + 12.0f, panelY + 41.0f, panelW - 24.0f, 13.0f),
      QStringLiteral("warn: %1 | next: %2")
          .arg(densityWarningForDominantAxis(dominantAxis, dominantScore),
               densityNextActionForAxis(dominantAxis)),
      detailFont, FloatColor{0.62f, 0.72f, 0.82f, 1.0f},
      Qt::AlignLeft | Qt::AlignVCenter);
  if (selectedLayer) {
    renderer->drawText(
        QRectF(panelX + 12.0f, panelY + 55.0f, panelW - 24.0f, 13.0f),
        QStringLiteral("focus: %1  |  masks %2  effects %3  mattes %4")
            .arg(selectedLayer->layerName().trimmed().isEmpty()
                     ? QStringLiteral("<selected>")
                     : selectedLayer->layerName().trimmed())
            .arg(selectedLayer->maskCount())
            .arg(selectedLayer->effectCount())
            .arg(static_cast<int>(selectedLayer->matteReferences().size())),
        detailFont, FloatColor{0.62f, 0.72f, 0.82f, 1.0f},
        Qt::AlignLeft | Qt::AlignVCenter);
  }

  const float heatTop = panelY + (selectedLayer ? 72.0f : 58.0f);
  const float heatLeft = panelX + 12.0f;
  const float heatWidth = panelW - 24.0f;
  const float heatHeight = std::max(18.0f, panelH - (selectedLayer ? 92.0f : 78.0f));
  const float legendCellW = heatWidth / static_cast<float>(kColumns);
  const float legendCellH = heatHeight / static_cast<float>(kRows);
  for (int row = 0; row < kRows; ++row) {
    for (int col = 0; col < kColumns; ++col) {
      const int index = row * kColumns + col;
      const float value = density[static_cast<std::size_t>(index)];
      if (value <= 0.0f) {
        continue;
      }
      const float normalized = std::clamp(value / peakDensity, 0.0f, 1.0f);
      const FloatColor color = densityHeatmapColor(normalized);
      const float alpha = 0.05f + normalized * 0.30f;
      renderer->drawSolidRect(
          heatLeft + static_cast<float>(col) * legendCellW,
          heatTop + static_cast<float>(row) * legendCellH, legendCellW + 0.5f,
          legendCellH + 0.5f,
          FloatColor{color.r(), color.g(), color.b(), alpha}, 1.0f);
    }
  }

  if (selectedLayer) {
    const QRectF selectedBounds = selectedLayer->transformedBoundingBox().normalized();
    const QRectF clippedSelected = selectedBounds.intersected(canvasRect);
    if (clippedSelected.isValid() && clippedSelected.width() > 0.0 &&
        clippedSelected.height() > 0.0) {
      const FloatColor focusColor =
          selectedLayer->maskCount() > 0
              ? FloatColor{1.0f, 0.78f, 0.24f, 0.96f}
              : FloatColor{0.96f, 0.96f, 0.98f, 0.92f};
      renderer->drawRectOutline(static_cast<float>(clippedSelected.left()),
                                static_cast<float>(clippedSelected.top()),
                                static_cast<float>(clippedSelected.width()),
                                static_cast<float>(clippedSelected.height()),
                                focusColor);
      renderer->drawSolidRect(
          static_cast<float>(clippedSelected.left()),
          static_cast<float>(clippedSelected.top()),
          static_cast<float>(clippedSelected.width()),
          static_cast<float>(clippedSelected.height()),
          FloatColor{focusColor.r(), focusColor.g(), focusColor.b(), 0.05f}, 1.0f);
    }
  }

  const float barY = heatTop + heatHeight + 6.0f;
  const float barX = panelX + 12.0f;
  const float barW = panelW - 24.0f;
  const float barH = 10.0f;
  const float labelW = 54.0f;
  const float meterW = std::max(18.0f, barW - labelW - 6.0f);
  const std::array<std::pair<const char *, double>, 4> densityMeters{{
      {"visual", visualScore},
      {"info", informationScore},
      {"luma", luminanceScore},
      {"motion", motionScore},
  }};
  for (std::size_t i = 0; i < densityMeters.size(); ++i) {
    const QString axis = QString::fromLatin1(densityMeters[i].first);
    const double score = densityMeters[i].second;
    const FloatColor axisColor = densityAxisColor(axis);
    const float y = barY + static_cast<float>(i) * 12.0f;
    renderer->drawText(QRectF(barX, y - 1.0f, labelW, 11.0f), axis,
                       detailFont, FloatColor{0.72f, 0.81f, 0.90f, 1.0f},
                       Qt::AlignLeft | Qt::AlignVCenter);
    renderer->drawSolidRect(barX + labelW, y, meterW, barH,
                            FloatColor{0.10f, 0.14f, 0.19f, 0.88f}, 1.0f);
    renderer->drawSolidRect(barX + labelW, y, meterW * static_cast<float>(score),
                            barH, FloatColor{axisColor.r(), axisColor.g(),
                                             axisColor.b(), 0.78f},
                            1.0f);
    if (axis == densityAxisDisplayName(dominantAxis)) {
      renderer->drawRectOutline(barX + labelW, y, meterW, barH,
                                FloatColor{axisColor.r(), axisColor.g(),
                                           axisColor.b(), 0.96f});
    }
  }
}

QString renderBackendToString(ArtifactRenderQueueService::RenderBackend backend)
{
  switch (backend) {
  case ArtifactRenderQueueService::RenderBackend::CPU:
    return QStringLiteral("cpu");
  case ArtifactRenderQueueService::RenderBackend::GPU:
    return QStringLiteral("gpu");
  case ArtifactRenderQueueService::RenderBackend::Auto:
  default:
    return QStringLiteral("auto");
  }
}

double clamp01(double value)
{
  return std::clamp(value, 0.0, 1.0);
}

QString densityLevelFromScore(double score)
{
  if (score >= 0.85) {
    return QStringLiteral("high");
  }
  if (score >= 0.55) {
    return QStringLiteral("medium");
  }
  return QStringLiteral("low");
}

QString densityWarningForDominantAxis(const QString &axis, double score);
QString densityNextActionForAxis(const QString &axis);

QString densityWarningForDominantAxis(const QString &axis, double score)
{
  const QString level = densityLevelFromScore(score);
  if (level == QStringLiteral("low")) {
    return QStringLiteral("density is readable");
  }

  if (axis == QStringLiteral("visual")) {
    return QStringLiteral("visual density is high");
  }
  if (axis == QStringLiteral("information")) {
    return QStringLiteral("information density is high");
  }
  if (axis == QStringLiteral("luminance")) {
    return QStringLiteral("luminance density is high");
  }
  if (axis == QStringLiteral("motion")) {
    return QStringLiteral("motion density is high");
  }
  return QStringLiteral("density is high");
}

QString densityNextActionForAxis(const QString &axis)
{
  if (axis == QStringLiteral("visual")) {
    return QStringLiteral("collapse repeated regions or add spacing");
  }
  if (axis == QStringLiteral("information")) {
    return QStringLiteral("reduce labels or move notes into debugger");
  }
  if (axis == QStringLiteral("luminance")) {
    return QStringLiteral("soften bright clusters or separate highlights");
  }
  if (axis == QStringLiteral("motion")) {
    return QStringLiteral("spread key activity or add rest frames");
  }
  return QStringLiteral("reduce the busiest region");
}

QString toolTypeToOverlayLabel(ToolType toolType)
{
  switch (toolType) {
  case ToolType::Selection:
    return QStringLiteral("Select");
  case ToolType::Hand:
    return QStringLiteral("Hand");
  case ToolType::Zoom:
    return QStringLiteral("Zoom");
  case ToolType::Rotation:
    return QStringLiteral("Rotate");
  case ToolType::AnchorPoint:
    return QStringLiteral("Anchor");
  case ToolType::Pen:
    return QStringLiteral("Mask");
  case ToolType::Text:
    return QStringLiteral("Text");
  case ToolType::Shape:
    return QStringLiteral("Shape");
  case ToolType::Rectangle:
    return QStringLiteral("Rect");
  case ToolType::Ellipse:
    return QStringLiteral("Ellipse");
  case ToolType::Move:
    return QStringLiteral("Move");
  case ToolType::Scale:
    return QStringLiteral("Scale");
  case ToolType::Ripple:
    return QStringLiteral("Ripple");
  case ToolType::Rolling:
    return QStringLiteral("Rolling");
  case ToolType::Slip:
    return QStringLiteral("Slip");
  case ToolType::Slide:
    return QStringLiteral("Slide");
  case ToolType::MotionSketch:
    return QStringLiteral("Sketch");
  case ToolType::Puppet:
    return QStringLiteral("Puppet");
  }
  return QStringLiteral("Tool");
}

qint64 latestTimerMs(const std::string& timerName)
{
  const auto latest = ArtifactCore::PerformanceRegistry::instance().getLatestSamples();
  const auto it = latest.find(timerName);
  if (it == latest.end()) {
    return 0;
  }
  return static_cast<qint64>(std::llround(it->second.durationMs));
}

QString playbackStateToString(PlaybackState state)
{
  switch (state) {
  case PlaybackState::Playing:
    return QStringLiteral("playing");
  case PlaybackState::Paused:
    return QStringLiteral("paused");
  case PlaybackState::Stopped:
  default:
    return QStringLiteral("stopped");
  }
}

bool floatColorEquals(const FloatColor &lhs, const FloatColor &rhs,
                      float epsilon = 0.0f) {
  if (epsilon <= 0.0f) {
    return lhs.r() == rhs.r() && lhs.g() == rhs.g() && lhs.b() == rhs.b() &&
           lhs.a() == rhs.a();
  }
  return std::abs(lhs.r() - rhs.r()) <= epsilon &&
         std::abs(lhs.g() - rhs.g()) <= epsilon &&
         std::abs(lhs.b() - rhs.b()) <= epsilon &&
         std::abs(lhs.a() - rhs.a()) <= epsilon;
}

QImage makeSolidColorSprite(const FloatColor &color) {
  QImage image(1, 1, QImage::Format_RGBA8888);
  image.fill(QColor::fromRgbF(color.r(), color.g(), color.b(), color.a()));
  return image;
}

QImage makeMayaGradientSprite(const FloatColor &baseColor) {
  constexpr int kWidth = 4;
  constexpr int kHeight = 4096; // 1024→4096 to reduce banding
  QImage image(kWidth, kHeight, QImage::Format_RGBA64); // 16-bit per channel
  image.fill(Qt::transparent);

  QPainter painter(&image);
  QLinearGradient gradient(0.0, 0.0, 0.0, static_cast<qreal>(kHeight));
  // Use more color stops for smoother transitions
  gradient.setColorAt(0.00, QColor::fromRgbF(0.26f, 0.32f, 0.38f, 1.0f));
  gradient.setColorAt(0.07, QColor::fromRgbF(0.24f, 0.30f, 0.36f, 1.0f));
  gradient.setColorAt(0.14, QColor::fromRgbF(0.21f, 0.27f, 0.33f, 1.0f));
  gradient.setColorAt(0.22, QColor::fromRgbF(0.19f, 0.25f, 0.30f, 1.0f));
  gradient.setColorAt(0.30, QColor::fromRgbF(0.17f, 0.22f, 0.27f, 1.0f));
  gradient.setColorAt(0.41, QColor::fromRgbF(0.15f, 0.20f, 0.25f, 1.0f));
  gradient.setColorAt(0.52, QColor::fromRgbF(0.13f, 0.17f, 0.22f, 1.0f));
  gradient.setColorAt(0.64, QColor::fromRgbF(0.12f, 0.15f, 0.20f, 1.0f));
  gradient.setColorAt(0.76, QColor::fromRgbF(0.10f, 0.13f, 0.17f, 1.0f));
  gradient.setColorAt(0.88, QColor::fromRgbF(0.09f, 0.12f, 0.15f, 1.0f));
  gradient.setColorAt(1.00, QColor::fromRgbF(0.08f, 0.10f, 0.13f, 1.0f));
  painter.fillRect(image.rect(), gradient);

  QLinearGradient glow(0.0, 0.0, static_cast<qreal>(kWidth), static_cast<qreal>(kHeight));
  QColor tint = QColor::fromRgbF(baseColor.r(), baseColor.g(), baseColor.b(), 1.0f);
  tint.setAlpha(72);
  glow.setColorAt(0.0, tint.lighter(112));
  QColor tintDark = tint.darker(140);
  tintDark.setAlpha(28);
  glow.setColorAt(1.0, tintDark);
  painter.fillRect(image.rect(), glow);
  return image;
}

enum class SelectionMode { Replace, Add, Toggle };

enum class LayerDragMode { None, Move, ScaleTL, ScaleTR, ScaleBL, ScaleBR };

enum class RectangleToolMode { None, Mask, Shape };

enum class MaskHandleType { None, InTangent, OutTangent };

QRectF dragRectFromPoints(const QPointF &start, const QPointF &end)
{
  return QRectF(start, end).normalized();
}

bool isMeaningfulDragRect(const QRectF &rect)
{
  return rect.width() >= 1.0 && rect.height() >= 1.0;
}

std::array<QPointF, 4> rectCorners(const QRectF &rect)
{
  return {rect.topLeft(), rect.topRight(), rect.bottomRight(),
          rect.bottomLeft()};
}

QString uniqueLayerNameForCurrentComposition(const QString &baseName)
{
  const QString trimmedBase = baseName.trimmed().isEmpty()
                                  ? QStringLiteral("Layer 1")
                                  : baseName.trimmed();

  QSet<QString> occupied;
  if (auto *service = ArtifactProjectService::instance()) {
    if (auto comp = service->currentComposition().lock()) {
      for (const auto &layer : comp->allLayer()) {
        if (!layer) {
          continue;
        }
        const QString name = layer->layerName().trimmed();
        if (!name.isEmpty()) {
          occupied.insert(name);
        }
      }
    }
  }

  if (!occupied.contains(trimmedBase)) {
    return trimmedBase;
  }

  QString prefix = trimmedBase;
  int startNumber = 2;
  int end = trimmedBase.size();
  while (end > 0 && trimmedBase.at(end - 1).isDigit()) {
    --end;
  }
  if (end < trimmedBase.size()) {
    int start = end;
    while (start > 0 && trimmedBase.at(start - 1).isSpace()) {
      --start;
    }
    bool ok = false;
    const int current = trimmedBase.mid(end).toInt(&ok);
    if (ok) {
      prefix = trimmedBase.left(start);
      startNumber = current + 1;
    }
  }
  if (prefix == trimmedBase && !prefix.endsWith(QLatin1Char(' '))) {
    prefix += QLatin1Char(' ');
  }
  for (int index = startNumber; index < 10000; ++index) {
    const QString candidate = prefix + QString::number(index);
    if (!occupied.contains(candidate)) {
      return candidate;
    }
  }
  return trimmedBase;
}

QColor toQColor(const FloatColor &color) {
  return QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
}

int compositionPreviewIntervalMs(const ArtifactCompositionPtr &comp) {
  const double fps = comp ? comp->frameRate().framerate() : 0.0;
  if (fps <= 0.0) {
    return 16;
  }
  return std::max(1, static_cast<int>(std::lround(1000.0 / fps)));
}

QString layerSurfaceDependencyKey(ArtifactAbstractLayer *layer) {
  if (!layer) {
    return QString();
  }

  QString key;
  key += QStringLiteral("|masks=%1").arg(layer->maskCount());
  for (int m = 0; m < layer->maskCount(); ++m) {
    const LayerMask mask = layer->mask(m);
    key += QStringLiteral("|mask%1:enabled=%2:paths=%3")
               .arg(m)
               .arg(mask.isEnabled() ? 1 : 0)
               .arg(mask.maskPathCount());
    for (int p = 0; p < mask.maskPathCount(); ++p) {
      const MaskPath path = mask.maskPath(p);
      key += QStringLiteral(":p%1 closed=%2 mode=%3 opacity=%4 feather=%5 fH=%6 fV=%7 fI=%8 fO=%9 expansion=%10 inverted=%11 vertices=%12")
                 .arg(p)
                 .arg(path.isClosed() ? 1 : 0)
                 .arg(static_cast<int>(path.mode()))
                 .arg(path.opacity(), 0, 'f', 4)
                 .arg(path.feather(), 0, 'f', 4)
                 .arg(path.featherHorizontal(), 0, 'f', 4)
                 .arg(path.featherVertical(), 0, 'f', 4)
                 .arg(path.featherInner(), 0, 'f', 4)
                 .arg(path.featherOuter(), 0, 'f', 4)
                 .arg(path.expansion(), 0, 'f', 4)
                 .arg(path.isInverted() ? 1 : 0)
                 .arg(path.vertexCount());
      for (int v = 0; v < path.vertexCount(); ++v) {
        const MaskVertex vertex = path.vertex(v);
        key += QStringLiteral("[%1,%2,%3,%4,%5,%6]")
                   .arg(vertex.position.x(), 0, 'f', 3)
                   .arg(vertex.position.y(), 0, 'f', 3)
                   .arg(vertex.inTangent.x(), 0, 'f', 3)
                   .arg(vertex.inTangent.y(), 0, 'f', 3)
                   .arg(vertex.outTangent.x(), 0, 'f', 3)
                   .arg(vertex.outTangent.y(), 0, 'f', 3);
      }
    }
  }

  const auto mattes = layer->matteReferences();
  key += QStringLiteral("|mattes=%1").arg(static_cast<qulonglong>(mattes.size()));
  for (size_t i = 0; i < mattes.size(); ++i) {
    const auto &ref = mattes[i];
    key += QStringLiteral("|matte%1:id=%2:src=%3:enabled=%4:type=%5:blend=%6:fit=%7:opacity=%8:invert=%9")
               .arg(static_cast<qulonglong>(i))
               .arg(ref.id.toString())
               .arg(ref.sourceLayerId.toString())
               .arg(ref.enabled ? 1 : 0)
               .arg(static_cast<int>(ref.type))
               .arg(static_cast<int>(ref.blendMode))
               .arg(static_cast<int>(ref.fitMode))
               .arg(ref.opacity, 0, 'f', 4)
               .arg(ref.invert ? 1 : 0);
  }

  const auto effects = layer->getEffects();
  key += QStringLiteral("|effects=%1").arg(static_cast<qulonglong>(effects.size()));
  for (size_t i = 0; i < effects.size(); ++i) {
    const auto &effect = effects[i];
    if (!effect) {
      key += QStringLiteral("|effect%1:null").arg(static_cast<qulonglong>(i));
      continue;
    }
    key += QStringLiteral("|effect%1:enabled=%2:stage=%3:name=%4")
               .arg(static_cast<qulonglong>(i))
               .arg(effect->isEnabled() ? 1 : 0)
               .arg(static_cast<int>(effect->pipelineStage()))
               .arg(effect->displayName().toQString());
  }

  return key;
}

bool buildRasterizedSurfaceBuffer(ArtifactAbstractLayer *targetLayer,
                                  const QImage &surface,
                                  ArtifactCore::ImageF32x4_RGBA *outBuffer) {
  if (!targetLayer || surface.isNull() || !outBuffer) {
    return false;
  }

  const bool hasMasks = targetLayer->hasMasks();
  const auto effects = targetLayer->getEffects();
  bool hasRasterizerEffect = false;
  for (const auto &effect : effects) {
    if (effect && effect->isEnabled() &&
        effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
      hasRasterizerEffect = true;
      break;
    }
  }

  if (!hasRasterizerEffect && !hasMasks) {
    return false;
  }

  cv::Mat mat = ArtifactCore::CvUtils::qImageToCvMat(surface, true);
  if (mat.channels() == 3) {
      if (mat.type() == CV_8UC3) cv::cvtColor(mat, mat, cv::COLOR_BGR2BGRA);
      else if (mat.type() == CV_32FC3) cv::cvtColor(mat, mat, cv::COLOR_BGR2BGRA);
  } else if (mat.channels() == 1) {
      if (mat.type() == CV_8UC1) cv::cvtColor(mat, mat, cv::COLOR_GRAY2BGRA);
      else if (mat.type() == CV_32FC1) cv::cvtColor(mat, mat, cv::COLOR_GRAY2BGRA);
  }
  if (mat.type() != CV_32FC4) {
    mat.convertTo(mat, CV_32FC4, 1.0 / 255.0);
  }

  if (hasRasterizerEffect) {
    ArtifactCore::ImageF32x4_RGBA cpuImage;
    cpuImage.setFromCVMat(mat);
    ArtifactCore::ImageF32x4RGBAWithCache current(cpuImage);

    for (const auto &effect : effects) {
      if (!effect || !effect->isEnabled() ||
          effect->pipelineStage() != EffectPipelineStage::Rasterizer) {
        continue;
      }

      ArtifactCore::ImageF32x4RGBAWithCache next;
      effect->setContext(makeControllerEffectContext(
          targetLayer,
          QRectF(0.0, 0.0, static_cast<qreal>(current.width()),
                 static_cast<qreal>(current.height()))));
      effect->applyConfigured(current, next);
      current = next;
    }
    mat = current.image().toCVMat();
  }

  if (hasMasks) {
    // Mask vertices are in layer-local space (centered at 0,0).
    // Translate to pixel space: pixel = localPos - localBounds.topLeft()
    const QRectF lb = targetLayer->localBounds();
    const float scaleX = static_cast<float>(mat.cols) / std::max(1.0f, static_cast<float>(lb.width()));
    const float scaleY = static_cast<float>(mat.rows) / std::max(1.0f, static_cast<float>(lb.height()));
    const float maskOffsetX = static_cast<float>(-lb.x() * scaleX);
    const float maskOffsetY = static_cast<float>(-lb.y() * scaleY);
    if (compositionViewLog().isDebugEnabled()) {
      qCDebug(compositionViewLog)
          << "[MaskTrace] rasterize begin"
          << "layer=" << targetLayer->id().toString()
          << "name=" << targetLayer->layerName()
          << "size=" << QSize(mat.cols, mat.rows)
          << "localBounds=" << lb
          << "maskCount=" << targetLayer->maskCount()
          << "offset=" << QPointF(maskOffsetX, maskOffsetY)
          << "scale=" << QPointF(scaleX, scaleY);
    }
    for (int m = 0; m < targetLayer->maskCount(); ++m) {
      LayerMask mask = targetLayer->mask(m);
      mask.applyToImage(mat.cols, mat.rows, &mat, maskOffsetX, maskOffsetY, scaleX, scaleY);
    }
    if (compositionViewLog().isDebugEnabled()) {
      cv::Mat alpha;
      std::vector<cv::Mat> channels;
      cv::split(mat, channels);
      if (!channels.empty()) {
        alpha = channels.back();
        double alphaMin = 0.0;
        double alphaMax = 0.0;
        cv::minMaxLoc(alpha, &alphaMin, &alphaMax);
        qCDebug(compositionViewLog)
            << "[MaskTrace] rasterize end"
            << "layer=" << targetLayer->id().toString()
            << "name=" << targetLayer->layerName()
            << "alphaMin=" << alphaMin
            << "alphaMax=" << alphaMax
            << "hasMasks=" << hasMasks
            << "hasRasterizerEffect=" << hasRasterizerEffect;
      }
    }
  }

  outBuffer->setFromCVMat(mat);
  return true;
}

QPointF maskHandlePosition(const MaskPath& path, int vertexIndex, MaskHandleType handleType)
{
  const MaskVertex vertex = path.vertex(vertexIndex);
  switch (handleType) {
  case MaskHandleType::InTangent:
    return vertex.position + vertex.inTangent;
  case MaskHandleType::OutTangent:
    return vertex.position + vertex.outTangent;
  case MaskHandleType::None:
    break;
  }
  return vertex.position;
}

QPointF cubicBezierPoint(const QPointF &p0, const QPointF &p1,
                         const QPointF &p2, const QPointF &p3, double t) {
  const double u = 1.0 - t;
  const double tt = t * t;
  const double uu = u * u;
  const double uuu = uu * u;
  const double ttt = tt * t;
  return p0 * uuu + p1 * (3.0 * uu * t) + p2 * (3.0 * u * tt) + p3 * ttt;
}

QVector<QPointF> maskSegmentPolyline(const MaskVertex &start,
                                     const MaskVertex &end,
                                     int subdivisions = 18) {
  QVector<QPointF> points;
  points.reserve(std::max(2, subdivisions + 1));
  const QPointF p0 = start.position;
  const QPointF p1 = start.position + start.outTangent;
  const QPointF p2 = end.position + end.inTangent;
  const QPointF p3 = end.position;
  for (int i = 0; i <= subdivisions; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(subdivisions);
    points.push_back(cubicBezierPoint(p0, p1, p2, p3, t));
  }
  return points;
}

bool hitTestMaskHandle(const ArtifactAbstractLayerPtr& layer,
                       const QPointF& canvasPos,
                       float threshold,
                       int& outMaskIndex,
                       int& outPathIndex,
                       int& outVertexIndex,
                       MaskHandleType& outHandleType)
{
  if (!layer) {
    return false;
  }

  const QTransform globalTransform = layer->getGlobalTransform();
  bool invertible = false;
  const QTransform invTransform = globalTransform.inverted(&invertible);
  if (!invertible) {
    return false;
  }

  const QPointF localPos = invTransform.map(canvasPos);
  const float thresholdSq = threshold * threshold;
  for (int m = 0; m < layer->maskCount(); ++m) {
    const LayerMask mask = layer->mask(m);
    if (!mask.isEnabled()) {
      continue;
    }
    for (int p = 0; p < mask.maskPathCount(); ++p) {
      const MaskPath path = mask.maskPath(p);
      for (int v = 0; v < path.vertexCount(); ++v) {
        const MaskVertex vertex = path.vertex(v);
        for (MaskHandleType handleType : {MaskHandleType::InTangent, MaskHandleType::OutTangent}) {
          if ((handleType == MaskHandleType::InTangent && vertex.inTangent == QPointF(0, 0)) ||
              (handleType == MaskHandleType::OutTangent && vertex.outTangent == QPointF(0, 0))) {
            continue;
          }
          const QPointF handlePos = maskHandlePosition(path, v, handleType);
          const QPointF delta = handlePos - localPos;
          if (QPointF::dotProduct(delta, delta) <= thresholdSq) {
            outMaskIndex = m;
            outPathIndex = p;
            outVertexIndex = v;
            outHandleType = handleType;
            return true;
          }
        }
      }
    }
  }

  return false;
}

bool hitTestMaskSegment(const ArtifactAbstractLayerPtr& layer,
                        const QPointF& canvasPos,
                        float threshold,
                        int& outMaskIndex,
                        int& outPathIndex,
                        int& outSegmentIndex)
{
  if (!layer) {
    return false;
  }

  const QTransform globalTransform = layer->getGlobalTransform();
  bool invertible = false;
  const QTransform invTransform = globalTransform.inverted(&invertible);
  if (!invertible) {
    return false;
  }

  const QPointF localPos = invTransform.map(canvasPos);
  const float thresholdSq = threshold * threshold;
  const auto distanceToSegmentSq = [](const QPointF &p, const QPointF &a,
                                      const QPointF &b) {
    const QPointF ab = b - a;
    const double abLenSq = QPointF::dotProduct(ab, ab);
    if (abLenSq <= std::numeric_limits<double>::epsilon()) {
      const QPointF delta = p - a;
      return QPointF::dotProduct(delta, delta);
    }

    const QPointF ap = p - a;
    const double t =
        std::clamp(QPointF::dotProduct(ap, ab) / abLenSq, 0.0, 1.0);
    const QPointF proj = a + ab * t;
    const QPointF delta = p - proj;
    return QPointF::dotProduct(delta, delta);
  };

  for (int m = 0; m < layer->maskCount(); ++m) {
    const LayerMask mask = layer->mask(m);
    if (!mask.isEnabled()) {
      continue;
    }
    for (int p = 0; p < mask.maskPathCount(); ++p) {
      const MaskPath path = mask.maskPath(p);
      const int vertexCount = path.vertexCount();
      if (vertexCount < 2) {
        continue;
      }

      const int segmentCount = path.isClosed() ? vertexCount : vertexCount - 1;
      for (int s = 0; s < segmentCount; ++s) {
        const MaskVertex startVertex = path.vertex(s);
        const MaskVertex endVertex = path.vertex((s + 1) % vertexCount);
        const QVector<QPointF> samples =
            maskSegmentPolyline(startVertex, endVertex, 14);
        for (int i = 1; i < samples.size(); ++i) {
          if (distanceToSegmentSq(localPos, samples[i - 1], samples[i]) <=
              thresholdSq) {
            outMaskIndex = m;
            outPathIndex = p;
            outSegmentIndex = s;
            return true;
          }
        }
      }
    }
  }

  return false;
}

QPointF closestPointOnMaskSegment(const MaskVertex &start, const MaskVertex &end,
                                  const QPointF &localPos) {
  const QVector<QPointF> samples = maskSegmentPolyline(start, end, 18);
  QPointF best = start.position;
  double bestDistSq = std::numeric_limits<double>::max();
  for (int i = 1; i < samples.size(); ++i) {
    const QPointF a = samples[i - 1];
    const QPointF b = samples[i];
    const QPointF ab = b - a;
    const double abLenSq = QPointF::dotProduct(ab, ab);
    QPointF proj;
    if (abLenSq <= std::numeric_limits<double>::epsilon()) {
      proj = a;
    } else {
      const QPointF ap = localPos - a;
      const double t =
          std::clamp(QPointF::dotProduct(ap, ab) / abLenSq, 0.0, 1.0);
      proj = a + ab * t;
    }
    const QPointF delta = localPos - proj;
    const double distSq = QPointF::dotProduct(delta, delta);
    if (distSq < bestDistSq) {
      bestDistSq = distSq;
      best = proj;
    }
  }
  return best;
}

bool insertVertexOnMaskSegment(const ArtifactAbstractLayerPtr &layer,
                               int maskIndex, int pathIndex, int segmentIndex,
                               const QPointF &localPos) {
  if (!layer || maskIndex < 0 || pathIndex < 0 || segmentIndex < 0) {
    return false;
  }

  LayerMask mask = layer->mask(maskIndex);
  MaskPath path = mask.maskPath(pathIndex);
  const int vertexCount = path.vertexCount();
  if (vertexCount < 2 || segmentIndex >= vertexCount) {
    return false;
  }

  const MaskVertex startVertex = path.vertex(segmentIndex);
  const MaskVertex endVertex = path.vertex((segmentIndex + 1) % vertexCount);
  const QPointF insertPos =
      closestPointOnMaskSegment(startVertex, endVertex, localPos);

  MaskVertex newVertex;
  newVertex.position = insertPos;
  newVertex.inTangent = QPointF(0, 0);
  newVertex.outTangent = QPointF(0, 0);
  path.insertVertex(segmentIndex + 1, newVertex);
  mask.setMaskPath(pathIndex, path);
  layer->setMask(maskIndex, mask);
  return true;
}

bool layerUsesTextGizmo(const ArtifactAbstractLayerPtr &layer) {
  return layer && dynamic_cast<ArtifactTextLayer *>(layer.get()) != nullptr;
}

void setMaskVertexHandle(MaskVertex &vertex, MaskHandleType handleType,
                         const QPointF &handleDelta, bool breakTangents) {
  const QPointF mirroredDelta(-handleDelta.x(), -handleDelta.y());
  if (handleType == MaskHandleType::InTangent) {
    vertex.inTangent = handleDelta;
    if (!breakTangents) {
      vertex.outTangent = mirroredDelta;
    }
  } else if (handleType == MaskHandleType::OutTangent) {
    vertex.outTangent = handleDelta;
    if (!breakTangents) {
      vertex.inTangent = mirroredDelta;
    }
  }
}

void drawMaskSquareMarker(ArtifactIRenderer *renderer,
                          const Detail::float2 &center,
                          float size,
                          const FloatColor &color,
                          const FloatColor *shadowColor = nullptr,
                          float shadowExpand = 0.0f)
{
  if (!renderer) {
    return;
  }

  const auto drawSquareOutline = [&](float squareSize,
                                     const FloatColor &outlineColor,
                                     float thickness) {
    const float half = squareSize * 0.5f;
    const Detail::float2 tl{center.x - half, center.y - half};
    const Detail::float2 tr{center.x + half, center.y - half};
    const Detail::float2 br{center.x + half, center.y + half};
    const Detail::float2 bl{center.x - half, center.y + half};
    renderer->drawThickLineLocal(tl, tr, thickness, outlineColor);
    renderer->drawThickLineLocal(tr, br, thickness, outlineColor);
    renderer->drawThickLineLocal(br, bl, thickness, outlineColor);
    renderer->drawThickLineLocal(bl, tl, thickness, outlineColor);
  };

  if (shadowColor) {
    const float shadowSize = size + shadowExpand;
    const float shadowHalf = shadowSize * 0.5f;
    renderer->drawSolidRect(center.x - shadowHalf, center.y - shadowHalf,
                            shadowSize, shadowSize, *shadowColor, 0.25f);
    drawSquareOutline(shadowSize, *shadowColor, 1.75f);
  }

  const float half = size * 0.5f;
  renderer->drawSolidRect(center.x - half, center.y - half, size, size,
                          color, 0.20f);
  drawSquareOutline(size, color, 1.5f);
}

bool isScaleHandle(TransformGizmo::HandleType handle) {
  switch (handle) {
  case TransformGizmo::HandleType::Scale_TL:
  case TransformGizmo::HandleType::Scale_TR:
  case TransformGizmo::HandleType::Scale_BL:
  case TransformGizmo::HandleType::Scale_BR:
  case TransformGizmo::HandleType::Scale_T:
  case TransformGizmo::HandleType::Scale_B:
  case TransformGizmo::HandleType::Scale_L:
  case TransformGizmo::HandleType::Scale_R:
    return true;
  default:
    return false;
  }
}

bool layerNeedsFrameSyncForCompositionView(ArtifactAbstractLayer *layer) {
  if (!layer) {
    return false;
  }

  // Animated playback-critical layers still need their frame propagated.
  if (dynamic_cast<ArtifactVideoLayer *>(layer) ||
      layer->isParticleLayer() ||
      dynamic_cast<ArtifactCompositionLayer *>(layer)) {
    return true;
  }

  if (layer->isTimeRemapEnabled() || layer->hasMasks() ||
      layer->hasModifiers() ||
      !layer->getEffects().empty()) {
    return true;
  }

  const auto &transform = layer->transform3D();
  if (transform.getPositionKeyFrameCount() > 0 ||
      transform.getRotationKeyFrameCount() > 0 ||
      transform.getScaleKeyFrameCount() > 0) {
    return true;
  }

  if (auto *solidImage = dynamic_cast<ArtifactSolidImageLayer *>(layer)) {
    if (const auto property =
            solidImage->getProperty(QStringLiteral("solid.color"));
        property && !property->getKeyFrames().empty()) {
      return true;
    }
  }

  if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(layer)) {
    if (textLayer->animatorCount() > 0) {
      return true;
    }
  }

  return false;
}
} // namespace

bool isLayerEffectivelyVisible(const ArtifactAbstractLayerPtr &layer) {
  ArtifactAbstractLayerPtr current = layer;
  int guard = 0;
  while (current && guard < 64) {
    if (!current->isVisible()) {
      return false;
    }
    current = current->parentLayer();
    ++guard;
  }
  return static_cast<bool>(layer);
}

ArtifactCore::Light makeSceneLightFromLayer(const ArtifactLightLayer* layer,
                                            const RationalTime& time)
{
  ArtifactCore::Light light;
  if (!layer) {
    return light;
  }

  switch (layer->lightType()) {
  case LightType::Point:
    light.setType(ArtifactCore::LightType::Point);
    break;
  case LightType::Spot:
    light.setType(ArtifactCore::LightType::Spot);
    break;
  case LightType::Parallel:
    light.setType(ArtifactCore::LightType::Directional);
    break;
  case LightType::Ambient:
    light.setType(ArtifactCore::LightType::Ambient);
    break;
  }

  const auto color = layer->color();
  const float intensity = std::max(0.0f, layer->intensity() / 100.0f);
  light.setColor(ArtifactCore::float3<float>{color.r(), color.g(), color.b()});
  light.setIntensity(intensity);

  const auto& t3 = layer->transform3D();
  const QVector3D position(
      static_cast<float>(t3.positionXAt(time)),
      static_cast<float>(t3.positionYAt(time)),
      static_cast<float>(t3.positionZAt(time)));
  const QMatrix4x4 globalMat = layer->getGlobalTransform4x4();
  QVector3D direction = globalMat.mapVector(QVector3D(0.0f, 0.0f, 1.0f));
  if (direction.lengthSquared() <= 0.000001f) {
    direction = QVector3D(0.0f, 0.0f, 1.0f);
  } else {
    direction.normalize();
  }

  light.setPosition(ArtifactCore::float3<float>{position.x(), position.y(), position.z()});
  light.setDirection(
      ArtifactCore::float3<float>{direction.x(), direction.y(), direction.z()});

  if (layer->lightType() == LightType::Point || layer->lightType() == LightType::Spot) {
    light.setRange(std::max(10.0f, layer->shadowRadius() * 10.0f));
  }
  if (layer->lightType() == LightType::Spot) {
    light.setSpotAngle(45.0f);
  }

  return light;
}

struct SceneLightEntry {
  ArtifactCore::Light light;
  const ArtifactLightLayer* source = nullptr;
};

QSet<QString> parseLightLinkIdSet(const QString& text)
{
  QSet<QString> ids;
  for (const auto& token : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
    const QString id = token.trimmed();
    if (!id.isEmpty()) {
      ids.insert(id);
    }
  }
  return ids;
}

bool lightAppliesToLayer(const ArtifactLightLayer* light,
                         const ArtifactAbstractLayer* targetLayer)
{
  if (!light || !targetLayer) {
    return false;
  }

  const QString targetId = targetLayer->id().toString();
  switch (light->lightLinkMode()) {
  case LightLinkMode::All:
    return true;
  case LightLinkMode::IncludeOnly:
    return parseLightLinkIdSet(light->linkedLayerIdsText()).contains(targetId);
  case LightLinkMode::ExcludeList:
    return !parseLightLinkIdSet(light->excludedLayerIdsText()).contains(targetId);
  }
  return true;
}

std::vector<ArtifactCore::Light> filterSceneLightsForLayer(
    const ArtifactAbstractLayer* targetLayer,
    const std::vector<SceneLightEntry>& sceneLights)
{
  std::vector<ArtifactCore::Light> filtered;
  if (!targetLayer) {
    return filtered;
  }

  const auto* modelLayer = dynamic_cast<const Artifact3DLayer*>(targetLayer);
  if (modelLayer && !modelLayer->affectedByLights()) {
    return filtered;
  }

  filtered.reserve(sceneLights.size());
  for (const auto& entry : sceneLights) {
    if (!entry.source || !entry.source->isVisible()) {
      continue;
    }
    if (lightAppliesToLayer(entry.source, targetLayer)) {
      filtered.push_back(entry.light);
    }
  }
  return filtered;
}

QString buildLayerSurfaceCacheKey(ArtifactAbstractLayer *layer,
                                  const QImage &surface, int64_t frameNumber) {
  if (!layer) {
    return QString();
  }

  QString key = layer->id().toString();
  key +=
      QStringLiteral("|size=%1x%2").arg(surface.width()).arg(surface.height());
  key += layerSurfaceDependencyKey(layer);

  if (auto *solid2D = dynamic_cast<ArtifactSolid2DLayer *>(layer)) {
    const QRectF bounds = solid2D->localBounds();
    key += QStringLiteral("|solid2D|color=%1|bounds=%2x%3")
               .arg(QStringLiteral("%1,%2,%3,%4")
                        .arg(solid2D->color().r(), 0, 'f', 4)
                        .arg(solid2D->color().g(), 0, 'f', 4)
                        .arg(solid2D->color().b(), 0, 'f', 4)
                        .arg(solid2D->color().a(), 0, 'f', 4))
               .arg(bounds.width(), 0, 'f', 2)
               .arg(bounds.height(), 0, 'f', 2);
    return key;
  }

  if (auto *solidImage = dynamic_cast<ArtifactSolidImageLayer *>(layer)) {
    const QRectF bounds = solidImage->localBounds();
    key += QStringLiteral("|solidImage|color=%1|bounds=%2x%3")
               .arg(QStringLiteral("%1,%2,%3,%4")
                        .arg(solidImage->color().r(), 0, 'f', 4)
                        .arg(solidImage->color().g(), 0, 'f', 4)
                        .arg(solidImage->color().b(), 0, 'f', 4)
                        .arg(solidImage->color().a(), 0, 'f', 4))
               .arg(bounds.width(), 0, 'f', 2)
               .arg(bounds.height(), 0, 'f', 2);
    return key;
  }

  if (auto *imageLayer = dynamic_cast<ArtifactImageLayer *>(layer)) {
    key += QStringLiteral("|image|src=%1|fit=%2|size=%3x%4")
               .arg(imageLayer->sourcePath())
               .arg(imageLayer->fitToLayer() ? 1 : 0)
               .arg(surface.width())
               .arg(surface.height());
    return key;
  }

  if (auto *svgLayer = dynamic_cast<ArtifactSvgLayer *>(layer)) {
    key += QStringLiteral("|svg|src=%1|fit=%2|size=%3x%4")
               .arg(svgLayer->sourcePath())
               .arg(svgLayer->fitToLayer() ? 1 : 0)
               .arg(surface.width())
               .arg(surface.height());
    return key;
  }

  if (auto *videoLayer = dynamic_cast<ArtifactVideoLayer *>(layer)) {
    key += QStringLiteral("|video|src=%1|frame=%2|proxy=%3|size=%4x%5")
               .arg(videoLayer->sourcePath())
               .arg(frameNumber)
               .arg(static_cast<int>(videoLayer->proxyQuality()))
               .arg(surface.width())
               .arg(surface.height());
    return key;
  }

  if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(layer)) {
    const bool animated = textLayer->animatorCount() > 0;
    key += QStringLiteral("|text|value=%1|family=%2|size=%3|color=%4|stroke=%5|strokeEnabled=%6|strokeWidth=%7|shadow=%8|shadowEnabled=%9|shadowOffset=%10,%11|shadowBlur=%12|tracking=%13|leading=%14|bold=%15|italic=%16|underline=%17|strike=%18|hAlign=%19|vAlign=%20|wrap=%21|layout=%22|maxWidth=%23|boxHeight=%24|paragraphSpacing=%25|animators=%26%27|surface=%28x%29")
               .arg(textLayer->text().toQString())
               .arg(textLayer->fontFamily().toQString())
               .arg(textLayer->fontSize(), 0, 'f', 3)
               .arg(QStringLiteral("%1,%2,%3,%4")
                        .arg(textLayer->textColor().r(), 0, 'f', 4)
                        .arg(textLayer->textColor().g(), 0, 'f', 4)
                        .arg(textLayer->textColor().b(), 0, 'f', 4)
                        .arg(textLayer->textColor().a(), 0, 'f', 4))
               .arg(QStringLiteral("%1,%2,%3,%4")
                        .arg(textLayer->strokeColor().r(), 0, 'f', 4)
                        .arg(textLayer->strokeColor().g(), 0, 'f', 4)
                        .arg(textLayer->strokeColor().b(), 0, 'f', 4)
                        .arg(textLayer->strokeColor().a(), 0, 'f', 4))
               .arg(textLayer->isStrokeEnabled() ? 1 : 0)
               .arg(textLayer->strokeWidth(), 0, 'f', 3)
               .arg(QStringLiteral("%1,%2,%3,%4")
                        .arg(textLayer->shadowColor().r(), 0, 'f', 4)
                        .arg(textLayer->shadowColor().g(), 0, 'f', 4)
                        .arg(textLayer->shadowColor().b(), 0, 'f', 4)
                        .arg(textLayer->shadowColor().a(), 0, 'f', 4))
               .arg(textLayer->isShadowEnabled() ? 1 : 0)
               .arg(textLayer->shadowOffsetX(), 0, 'f', 3)
               .arg(textLayer->shadowOffsetY(), 0, 'f', 3)
               .arg(textLayer->shadowBlur(), 0, 'f', 3)
               .arg(textLayer->tracking(), 0, 'f', 3)
               .arg(textLayer->leading(), 0, 'f', 3)
               .arg(textLayer->isBold() ? 1 : 0)
               .arg(textLayer->isItalic() ? 1 : 0)
               .arg(textLayer->isUnderline() ? 1 : 0)
               .arg(textLayer->isStrikethrough() ? 1 : 0)
               .arg(static_cast<int>(textLayer->horizontalAlignment()))
               .arg(static_cast<int>(textLayer->verticalAlignment()))
               .arg(static_cast<int>(textLayer->wrapMode()))
               .arg(static_cast<int>(textLayer->layoutMode()))
               .arg(textLayer->maxWidth(), 0, 'f', 3)
               .arg(textLayer->boxHeight(), 0, 'f', 3)
               .arg(textLayer->paragraphSpacing(), 0, 'f', 3)
               .arg(textLayer->animatorCount())
               .arg(animated ? QStringLiteral("|frame=%1").arg(frameNumber)
                             : QString())
               .arg(surface.width())
               .arg(surface.height());
    return key;
  }

  return key;
}

QRectF viewportRectToCanvasRect(ArtifactIRenderer *renderer,
                                const QPointF &startViewportPos,
                                const QPointF &endViewportPos) {
  if (!renderer) {
    return QRectF();
  }

  const auto a =
      renderer->viewportToCanvas({static_cast<float>(startViewportPos.x()),
                                  static_cast<float>(startViewportPos.y())});
  const auto b =
      renderer->viewportToCanvas({static_cast<float>(endViewportPos.x()),
                                  static_cast<float>(endViewportPos.y())});
  return QRectF(QPointF(std::min(a.x, b.x), std::min(a.y, b.y)),
                QPointF(std::max(a.x, b.x), std::max(a.y, b.y)));
}

DetailLevel detailLevelFromZoom(float zoom) {
  if (zoom < 0.50f) {
    return DetailLevel::Low;
  }
  if (zoom < 0.90f) {
    return DetailLevel::Medium;
  }
  return DetailLevel::High;
}

SelectionMode selectionModeFromModifiers(Qt::KeyboardModifiers modifiers) {
  if (modifiers.testFlag(Qt::ControlModifier)) {
    return SelectionMode::Toggle;
  }
  if (modifiers.testFlag(Qt::ShiftModifier)) {
    return SelectionMode::Add;
  }
  return SelectionMode::Replace;
}

QStringList selectedLayerIdList() {
  QStringList ids;
  auto *app = ArtifactApplicationManager::instance();
  auto *selection = app ? app->layerSelectionManager() : nullptr;
  if (!selection) {
    return ids;
  }

  for (const auto &layer : selection->selectedLayersInOrder()) {
    if (layer) {
      ids.push_back(layer->id().toString());
    }
  }
  return ids;
}

bool isLayerSelected(const QStringList &selectedIds,
                     const ArtifactAbstractLayerPtr &layer) {
  if (!layer) {
    return false;
  }
  const QString layerId = layer->id().toString();
  for (const auto &selectedId : selectedIds) {
    if (selectedId == layerId) {
      return true;
    }
  }
  return false;
}

enum class MotionPathSampleKind { Keyframe, Current };

struct MotionPathSample {
  QPointF position;
  MotionPathSampleKind kind = MotionPathSampleKind::Keyframe;
  int64_t framePosition = -1;
};

struct MotionPathPositionSnapshot {
  bool hasPositionKey = false;
  float x = 0.0f;
  float y = 0.0f;
};

struct MotionPathInterpolationSnapshot {
  bool hasPositionKey = false;
  int xInterpolation = 0;
  int yInterpolation = 0;
};

class MotionPathUndoCommand final : public UndoCommand {
public:
  MotionPathUndoCommand(ArtifactAbstractLayerPtr layer, int64_t frame,
                        MotionPathPositionSnapshot before,
                        MotionPathPositionSnapshot after)
      : layer_(layer), frame_(frame), before_(before), after_(after) {}

  void undo() override { apply(before_); }
  void redo() override { apply(after_); }
  QString label() const override { return QStringLiteral("Move Motion Path Keyframe"); }

private:
  void apply(const MotionPathPositionSnapshot &snapshot) {
    auto layer = layer_.lock();
    if (!layer) {
      return;
    }

    const ArtifactCore::RationalTime time(frame_, 24);
    auto &t3d = layer->transform3D();
    if (snapshot.hasPositionKey) {
      t3d.setPositionKeyFrameValueAt(time, snapshot.x, snapshot.y);
    } else {
      t3d.removePositionKeyFrameAt(time);
    }
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    if (auto *comp = static_cast<ArtifactAbstractComposition *>(layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  ArtifactAbstractLayerWeak layer_;
  int64_t frame_ = 0;
  MotionPathPositionSnapshot before_;
  MotionPathPositionSnapshot after_;
};

class MotionPathInterpolationUndoCommand final : public UndoCommand {
public:
  MotionPathInterpolationUndoCommand(ArtifactAbstractLayerPtr layer,
                                     int64_t frame,
                                     MotionPathInterpolationSnapshot before,
                                     MotionPathInterpolationSnapshot after)
      : layer_(layer), frame_(frame), before_(before), after_(after) {}

  void undo() override { apply(before_); }
  void redo() override { apply(after_); }
  QString label() const override {
    return QStringLiteral("Change Motion Path Interpolation");
  }

private:
  void apply(const MotionPathInterpolationSnapshot &snapshot) {
    auto layer = layer_.lock();
    if (!layer) {
      return;
    }

    const ArtifactCore::RationalTime time(frame_, 24);
    auto &t3d = layer->transform3D();
    if (!snapshot.hasPositionKey ||
        !t3d.hasPositionKeyFrameAt(time)) {
      return;
    }

    const auto xInterp =
        static_cast<ArtifactCore::InterpolationType>(snapshot.xInterpolation);
    const auto yInterp =
        static_cast<ArtifactCore::InterpolationType>(snapshot.yInterpolation);
    t3d.setPositionKeyFrameInterpolationAt(time, xInterp, yInterp);
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    if (auto *comp =
            static_cast<ArtifactAbstractComposition *>(layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  ArtifactAbstractLayerWeak layer_;
  int64_t frame_ = 0;
  MotionPathInterpolationSnapshot before_;
  MotionPathInterpolationSnapshot after_;
};

QVector3D unprojectClipCorner(const QMatrix4x4 &invViewProj, float x, float y,
                              float z) {
  const QVector4D clipPos(x, y, z, 1.0f);
  const QVector4D worldPos = invViewProj * clipPos;
  if (std::abs(worldPos.w()) < 1e-6f) {
    return {};
  }
  return {worldPos.x() / worldPos.w(), worldPos.y() / worldPos.w(),
          worldPos.z() / worldPos.w()};
}

CompositionRenderController::CameraFrustumVisual
buildCameraFrustumVisual(const ArtifactCompositionPtr &comp,
                         const LayerID &selectedLayerId) {
  CompositionRenderController::CameraFrustumVisual visual;
  if (!comp || selectedLayerId.isNil()) {
    return visual;
  }

  auto layer = comp->layerById(selectedLayerId);
  auto *layerPtr = layer.get();
  const auto camera = dynamic_cast<ArtifactCameraLayer *>(layerPtr);
  if (!camera) {
    return visual;
  }

  const auto size = comp->settings().compositionSize();
  const float width =
      static_cast<float>(size.width() > 0 ? size.width() : 1920);
  const float height =
      static_cast<float>(size.height() > 0 ? size.height() : 1080);
  const float aspect = std::max(0.001f, width / std::max(0.001f, height));

  const QMatrix4x4 view = camera->viewMatrix();
  const QMatrix4x4 proj = camera->projectionMatrix(aspect);
  bool invertible = false;
  const QMatrix4x4 invViewProj = (proj * view).inverted(&invertible);
  if (!invertible) {
    return visual;
  }

  visual.valid = true;
  visual.layerId = camera->id();
  visual.cameraPosition = view.inverted().map(QVector3D(0.0f, 0.0f, 0.0f));
  visual.viewMatrix = view;
  visual.projectionMatrix = proj;
  visual.guide = makeNukeStyleCameraGuidePrimitive();
  visual.aspect = aspect;
  visual.zoom = camera->zoom();

  visual.nearPlaneCorners.reserve(4);
  visual.farPlaneCorners.reserve(4);
  visual.nearPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, -1.0f, -1.0f, -1.0f));
  visual.nearPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, 1.0f, -1.0f, -1.0f));
  visual.nearPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, 1.0f, 1.0f, -1.0f));
  visual.nearPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, -1.0f, 1.0f, -1.0f));
  visual.farPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, -1.0f, -1.0f, 1.0f));
  visual.farPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, 1.0f, -1.0f, 1.0f));
  visual.farPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, 1.0f, 1.0f, 1.0f));
  visual.farPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, -1.0f, 1.0f, 1.0f));
  return visual;
}

CompositionRenderController::CameraFrustumVisual
buildCameraFrustumVisual(const ArtifactCameraLayer *camera,
                         const ArtifactCompositionPtr &comp) {
  CompositionRenderController::CameraFrustumVisual visual;
  if (!camera || !comp) {
    return visual;
  }

  const auto size = comp->settings().compositionSize();
  const float width =
      static_cast<float>(size.width() > 0 ? size.width() : 1920);
  const float height =
      static_cast<float>(size.height() > 0 ? size.height() : 1080);
  const float aspect = std::max(0.001f, width / std::max(0.001f, height));

  const QMatrix4x4 view = camera->viewMatrix();
  const QMatrix4x4 proj = camera->projectionMatrix(aspect);
  bool invertible = false;
  const QMatrix4x4 invViewProj = (proj * view).inverted(&invertible);
  if (!invertible) {
    return visual;
  }

  visual.valid = true;
  visual.layerId = camera->id();
  visual.cameraPosition = view.inverted().map(QVector3D(0.0f, 0.0f, 0.0f));
  visual.viewMatrix = view;
  visual.projectionMatrix = proj;
  visual.guide = makeNukeStyleCameraGuidePrimitive();
  visual.aspect = aspect;
  visual.zoom = camera->zoom();

  visual.nearPlaneCorners.reserve(4);
  visual.farPlaneCorners.reserve(4);
  visual.nearPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, -1.0f, -1.0f, -1.0f));
  visual.nearPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, 1.0f, -1.0f, -1.0f));
  visual.nearPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, 1.0f, 1.0f, -1.0f));
  visual.nearPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, -1.0f, 1.0f, -1.0f));
  visual.farPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, -1.0f, -1.0f, 1.0f));
  visual.farPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, 1.0f, -1.0f, 1.0f));
  visual.farPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, 1.0f, 1.0f, 1.0f));
  visual.farPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, -1.0f, 1.0f, 1.0f));
  return visual;
}

void drawCameraFrustumOverlay(ArtifactIRenderer *renderer,
                              const CompositionRenderController::CameraFrustumVisual &visual,
                              bool activeCamera) {
  if (!renderer || !visual.valid || visual.nearPlaneCorners.size() < 4 ||
      visual.farPlaneCorners.size() < 4) {
    return;
  }

  const FloatColor outerColor = activeCamera
                                    ? FloatColor{0.35f, 0.95f, 0.58f, 0.92f}
                                    : FloatColor{0.36f, 0.72f, 0.98f, 0.88f};
  const FloatColor innerColor = activeCamera
                                    ? FloatColor{0.08f, 0.18f, 0.12f, 0.58f}
                                    : FloatColor{0.07f, 0.10f, 0.14f, 0.56f};
  const auto drawLine = [renderer](const QVector3D &a, const QVector3D &b,
                                   const FloatColor &color, float thickness) {
    renderer->drawGizmoLine(Detail::float3{a.x(), a.y(), a.z()},
                            Detail::float3{b.x(), b.y(), b.z()}, color,
                            thickness);
  };

  renderer->setUseExternalMatrices(true);
  renderer->set3DCameraMatrices(visual.viewMatrix, visual.projectionMatrix);
  for (int i = 0; i < 4; ++i) {
    const int next = (i + 1) % 4;
    drawLine(visual.nearPlaneCorners[i], visual.nearPlaneCorners[next], outerColor,
         1.2f);
    drawLine(visual.farPlaneCorners[i], visual.farPlaneCorners[next], innerColor,
         0.8f);
    drawLine(visual.nearPlaneCorners[i], visual.farPlaneCorners[i], outerColor,
         0.9f);
  }

  const QVector3D center =
      (visual.nearPlaneCorners[0] + visual.nearPlaneCorners[1] +
       visual.nearPlaneCorners[2] + visual.nearPlaneCorners[3]) /
      4.0f;
  const QVector3D camPos = visual.cameraPosition;
  drawLine(camPos, center, outerColor, 1.0f);
  renderer->reset3DCameraMatrices();
  renderer->setUseExternalMatrices(false);
}

QMatrix4x4 viewportOrientationViewMatrix(
    const ArtifactCore::ViewOrientationHotspot hotspot, const float cw,
    const float ch) {
  QMatrix4x4 view;
  const QQuaternion q =
      ArtifactCore::ViewOrientationNavigator::orientationForHotspot(hotspot);
  view.translate(cw * 0.5f, ch * 0.5f, 0.0f);
  view.rotate(q.conjugated());
  view.translate(-cw * 0.5f, -ch * 0.5f, 0.0f);
  return view;
}

QMatrix4x4 viewportOrientationProjectionMatrix(const float viewportW,
                                               const float viewportH) {
  QMatrix4x4 proj;
  const float w = std::max(1.0f, viewportW);
  const float h = std::max(1.0f, viewportH);
  proj.ortho(0.0f, w, h, 0.0f, -100000.0f, 100000.0f);
  return proj;
}

// Forward declaration
FramePosition currentFrameForComposition(const ArtifactCompositionPtr &comp);

QVector<MotionPathSample>
buildMotionPathSamples(const ArtifactAbstractLayerPtr &layer,
                       const ArtifactCompositionPtr &comp) {
  QVector<MotionPathSample> samples;
  if (!layer || !comp) {
    return samples;
  }

  const auto keyTimes = layer->transform3D().getAllKeyFrameTimes();
  if (keyTimes.empty()) {
    return samples;
  }

  samples.reserve(static_cast<int>(keyTimes.size()) + 1);
  const int fps =
      std::max(1, static_cast<int>(std::round(comp->frameRate().framerate())));

  for (const auto &time : keyTimes) {
    const auto snapshot = layer->transform3D().snapshotAt(time);
    samples.push_back({QPointF(snapshot.anchorCanvasPosition.x,
                               snapshot.anchorCanvasPosition.y),
                       MotionPathSampleKind::Keyframe, time.value()});
  }

  const FramePosition currentFrame = currentFrameForComposition(comp);
  const RationalTime currentTime(currentFrame.framePosition(), fps);
  const auto currentSnapshot = layer->transform3D().snapshotAt(currentTime);
  samples.push_back({QPointF(currentSnapshot.anchorCanvasPosition.x,
                             currentSnapshot.anchorCanvasPosition.y),
                     MotionPathSampleKind::Current,
                     currentFrame.framePosition()});

  return samples;
}

bool hitTestMotionPathSample(const QVector<MotionPathSample> &samples,
                             const QPointF &canvasPos, float threshold,
                             MotionPathSample &outSample) {
  const float thresholdSq = threshold * threshold;
  bool found = false;
  float bestDistSq = thresholdSq;
  for (const auto &sample : samples) {
    if (sample.kind != MotionPathSampleKind::Keyframe) {
      continue;
    }
    const QPointF delta = sample.position - canvasPos;
    const float distSq = static_cast<float>(QPointF::dotProduct(delta, delta));
    if (distSq <= bestDistSq) {
      bestDistSq = distSq;
      outSample = sample;
      found = true;
    }
  }
  return found;
}

FloatColor motionPathInterpolationColor(int interpolation, bool isCurrent) {
  const auto type =
      static_cast<ArtifactCore::InterpolationType>(interpolation);
  switch (type) {
  case ArtifactCore::InterpolationType::Constant:
    return isCurrent ? FloatColor{0.42f, 0.88f, 1.0f, 1.0f}
                     : FloatColor{0.25f, 0.72f, 0.92f, 0.95f};
  case ArtifactCore::InterpolationType::Linear:
    return isCurrent ? FloatColor{1.0f, 0.92f, 0.28f, 1.0f}
                     : FloatColor{1.0f, 0.78f, 0.22f, 0.95f};
  case ArtifactCore::InterpolationType::EaseIn:
  case ArtifactCore::InterpolationType::EaseOut:
  case ArtifactCore::InterpolationType::EaseInOut:
  case ArtifactCore::InterpolationType::BackIn:
  case ArtifactCore::InterpolationType::BackOut:
  case ArtifactCore::InterpolationType::BackInOut:
  case ArtifactCore::InterpolationType::Exponential:
  case ArtifactCore::InterpolationType::Bezier:
  case ArtifactCore::InterpolationType::Sine:
  case ArtifactCore::InterpolationType::Cubic:
  case ArtifactCore::InterpolationType::Quintic:
    return isCurrent ? FloatColor{1.0f, 0.58f, 0.30f, 1.0f}
                     : FloatColor{0.97f, 0.48f, 0.24f, 0.95f};
  default:
    return isCurrent ? FloatColor{0.82f, 0.70f, 1.0f, 1.0f}
                     : FloatColor{0.72f, 0.58f, 0.95f, 0.95f};
  }
}

LayerDragMode hitTestLayerDragMode(const ArtifactAbstractLayerPtr &layer,
                                   const QPointF &viewportPos,
                                   ArtifactIRenderer *renderer) {
  if (!layer || !renderer) {
    return LayerDragMode::None;
  }

  const QRectF bbox = layer->transformedBoundingBox();
  if (!bbox.isValid() || bbox.width() <= 0.0 || bbox.height() <= 0.0) {
    return LayerDragMode::None;
  }

  constexpr float kHandleHitSize = 16.0f;
  const auto containsHandle = [&](float x, float y) {
    const auto p = renderer->canvasToViewport({x, y});
    const QRectF rect(p.x - kHandleHitSize * 0.5f, p.y - kHandleHitSize * 0.5f,
                      kHandleHitSize, kHandleHitSize);
    return rect.contains(viewportPos);
  };

  if (containsHandle(static_cast<float>(bbox.left()),
                     static_cast<float>(bbox.top()))) {
    return LayerDragMode::ScaleTL;
  }
  if (containsHandle(static_cast<float>(bbox.right()),
                     static_cast<float>(bbox.top()))) {
    return LayerDragMode::ScaleTR;
  }
  if (containsHandle(static_cast<float>(bbox.left()),
                     static_cast<float>(bbox.bottom()))) {
    return LayerDragMode::ScaleBL;
  }
  if (containsHandle(static_cast<float>(bbox.right()),
                     static_cast<float>(bbox.bottom()))) {
    return LayerDragMode::ScaleBR;
  }

  return LayerDragMode::Move;
}

bool layerIntersectsCanvasRect(const ArtifactAbstractLayerPtr &layer,
                               const QRectF &rect,
                               const FramePosition &currentFrame) {
  if (!layer || !rect.isValid() || !isLayerEffectivelyVisible(layer) ||
      !layer->isActiveAt(currentFrame)) {
    return false;
  }

  const QRectF bbox = layer->transformedBoundingBox();
  if (!bbox.isValid()) {
    return false;
  }

  return bbox.intersects(rect);
}

ArtifactCompositionPtr
resolvePreferredComposition(ArtifactProjectService *service) {
  // ProjectService を最優先
  if (service) {
    if (auto comp = service->currentComposition().lock()) {
      return comp;
    }
  }

  // フォールバック: ActiveContextService
  if (auto *app = ArtifactApplicationManager::instance()) {
    if (auto *active = app->activeContextService()) {
      if (auto comp = active->activeComposition()) {
        return comp;
      }
    }
  }

  // フォールバック: PlaybackService
  if (auto *playback = ArtifactPlaybackService::instance()) {
    if (auto comp = playback->currentComposition()) {
      return comp;
    }
  }

  return ArtifactCompositionPtr();
}

FramePosition currentFrameForComposition(const ArtifactCompositionPtr &comp) {
  if (!comp) {
    return FramePosition(0);
  }
  FramePosition currentFrame = comp->framePosition();
  if (auto *playback = ArtifactPlaybackService::instance()) {
    const auto playbackComp = playback->currentComposition();
    if (!playbackComp || playbackComp->id() == comp->id()) {
      currentFrame = playback->currentFrame();
    }
  }
  return currentFrame;
}

ArtifactAbstractLayerPtr
hitTopmostLayerAtViewportPos(const ArtifactCompositionPtr &comp,
                             ArtifactIRenderer *renderer,
                             const QPointF &viewportPos) {
  if (!comp || !renderer) {
    return ArtifactAbstractLayerPtr();
  }

  const auto currentFrame = currentFrameForComposition(comp);
  const auto canvasPos =
      renderer->viewportToCanvas({static_cast<float>(viewportPos.x()),
                                  static_cast<float>(viewportPos.y())});
  const auto &layers = comp->allLayerRef();
  for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i) {
    const auto &layer = layers[static_cast<size_t>(i)];
    if (!isLayerEffectivelyVisible(layer) || !layer->isActiveAt(currentFrame)) {
      continue;
    }

    const QTransform globalTransform = layer->getGlobalTransform();
    bool invertible = false;
    const QTransform invTransform = globalTransform.inverted(&invertible);
    if (invertible) {
      const QPointF localPos =
          invTransform.map(QPointF(canvasPos.x, canvasPos.y));
      if (layer->localBounds().contains(localPos)) {
        return layer;
      }
      continue;
    }

    const QRectF bbox = layer->transformedBoundingBox();
    if (bbox.contains(canvasPos.x, canvasPos.y)) {
      return layer;
    }
  }

  return ArtifactAbstractLayerPtr();
}

/// Apply track matte to a layer surface by evaluating its MatteStack.
/// Modifies the surface's alpha channel in-place.
static void applyLayerMatteToSurface(
    ArtifactAbstractLayer *layer, QImage &surface,
    const std::function<QImage(const ArtifactCore::Id &)> &sourceResolver)
{
    auto mattes = layer->matteReferences();
    if (mattes.empty()) return;

    const int w = surface.width();
    const int h = surface.height();
    if (w <= 0 || h <= 0) return;

    // Build source alpha buffers from matte source layers
    std::vector<std::vector<float>> sources;
    for (const auto &ref : mattes) {
        if (!ref.enabled || ref.sourceLayerId.isNil()) continue;

        QImage srcImage = sourceResolver(ref.sourceLayerId);
        if (srcImage.isNull()) continue;

        // Resize to match target surface
        if (srcImage.size() != surface.size()) {
            srcImage = srcImage.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        srcImage = srcImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);

        const int srcW = srcImage.width();
        const int srcH = srcImage.height();

        std::vector<float> alpha(static_cast<size_t>(w) * h, 0.0f);
        const bool useLuma = (ref.type == MatteType::Luma || ref.type == MatteType::InverseLuma);

        for (int y = 0; y < h && y < srcH; ++y) {
            const QRgb *srcLine = reinterpret_cast<const QRgb *>(srcImage.constScanLine(y));
            for (int x = 0; x < w && x < srcW; ++x) {
                QRgb px = srcLine[x];
                if (useLuma) {
                    // ITU-R BT.601 luma
                    float luma = 0.299f * qRed(px) + 0.587f * qGreen(px) + 0.114f * qBlue(px);
                    alpha[static_cast<size_t>(y) * w + x] = luma / 255.0f;
                } else {
                    alpha[static_cast<size_t>(y) * w + x] = qAlpha(px) / 255.0f;
                }
            }
        }

        // Invert if needed
        if (ref.invert) {
            for (auto &v : alpha) v = 1.0f - v;
        }

        sources.push_back(std::move(alpha));
    }

    if (sources.empty()) return;

    // Use Core's evaluateMatteStack to combine sources
    MatteStack stack;
    for (const auto &ref : mattes) {
        if (ref.enabled && !ref.sourceLayerId.isNil()) {
            stack.addNode(ref.toCoreMatteNode());
        }
    }

    auto result = evaluateMatteStack(sources, stack, w, h);
    if (!result.isValid()) return;

    // Apply matte mask to premultiplied RGBA surface.
    surface = surface.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(surface.scanLine(y));
        for (int x = 0; x < w; ++x) {
            float maskAlpha = result.sampleAlpha(x, y);
            int currentAlpha = qAlpha(line[x]);
            int newAlpha = static_cast<int>(currentAlpha * maskAlpha + 0.5f);
            const int scaledRed = static_cast<int>(qRed(line[x]) * maskAlpha + 0.5f);
            const int scaledGreen =
                static_cast<int>(qGreen(line[x]) * maskAlpha + 0.5f);
            const int scaledBlue =
                static_cast<int>(qBlue(line[x]) * maskAlpha + 0.5f);
            line[x] = qRgba(std::clamp(scaledRed, 0, 255),
                            std::clamp(scaledGreen, 0, 255),
                            std::clamp(scaledBlue, 0, 255),
                            std::clamp(newAlpha, 0, 255));
        }
    }
}

void drawLayerForCompositionView(
    ArtifactAbstractLayer *layer, ArtifactIRenderer *renderer,
    float opacityOverride = -1.0f, QString *videoDebugOut = nullptr,
    QHash<QString, LayerSurfaceCacheEntry> *surfaceCache = nullptr,
    GPUTextureCacheManager *gpuTextureCacheManager = nullptr,
    int64_t cacheFrameNumber = std::numeric_limits<int64_t>::min(),
    bool useGpuPath = false, const DetailLevel lod = DetailLevel::High,
    const QMatrix4x4 *cameraView = nullptr,
    const QMatrix4x4 *cameraProj = nullptr,
    const std::function<QImage(const ArtifactCore::Id &)> *matteSourceResolver = nullptr,
    const std::vector<SceneLightEntry> *sceneLights = nullptr) {
  if (!layer || !renderer) {
    qCDebug(compositionViewLog)
        << "[CompositionView] drawLayerForCompositionView: invalid "
           "layer/renderer";
    return;
  }

  if (const auto parent = layer->parentLayer();
      parent && parent->isGroupLayer()) {
    return;
  }

  const QRectF localRect = layer->localBounds();
  if (!localRect.isValid() || localRect.width() <= 0.0 ||
      localRect.height() <= 0.0) {
    qCDebug(compositionViewLog)
        << "[CompositionView] skip layer: invalid local bounds"
        << "id=" << layer->id().toString() << "rect=" << localRect;
    return;
  }

  const QTransform globalTransform = layer->getGlobalTransform();
  const QMatrix4x4 globalTransform4x4 = layer->getGlobalTransform4x4();

  // Handle 3D layers separately
  if (layer->is3D()) {
    std::vector<ArtifactCore::Light> filteredLights;
    const auto *modelLayer = dynamic_cast<Artifact3DLayer *>(layer);
    if (modelLayer && sceneLights) {
      filteredLights = filterSceneLightsForLayer(modelLayer, *sceneLights);
      renderer->setSceneLights(filteredLights);
    }
    if (cameraView && cameraProj) {
      renderer->set3DCameraMatrices(*cameraView, *cameraProj);
    }
    layer->draw(renderer);
    if (cameraView && cameraProj) {
      renderer->reset3DCameraMatrices();
    }
    if (modelLayer && sceneLights) {
      std::vector<ArtifactCore::Light> restoreLights;
      restoreLights.reserve(sceneLights->size());
      for (const auto &entry : *sceneLights) {
        restoreLights.push_back(entry.light);
      }
      renderer->setSceneLights(restoreLights);
    }
    return;
  }

  auto applyRasterizerEffectsAndMasksToSurface =
      [&](ArtifactAbstractLayer *targetLayer, QImage &surface) {
        ArtifactCore::ImageF32x4_RGBA processed;
        if (buildRasterizedSurfaceBuffer(targetLayer, surface, &processed)) {
          if (!processed.isEmpty()) {
            surface = processed.toQImage();
          }
        }
      };

  auto hasRasterizerEffectsOrMasks = [](ArtifactAbstractLayer *targetLayer) {
    if (!targetLayer) {
      return false;
    }
    if (targetLayer->hasMasks()) {
      return true;
    }

    for (const auto &effect : targetLayer->getEffects()) {
      if (effect && effect->isEnabled() &&
          effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
        return true;
      }
    }
    return false;
  };

  auto applySurfaceAndDraw = [&](QImage surface, const QRectF &rect,
                                 bool allowSurfaceCache) {
    if (surface.isNull()) {
      return false;
    }

    // Apply track matte before drawing
    if (matteSourceResolver && layer->matteReferences().size() > 0) {
        applyLayerMatteToSurface(layer, surface, *matteSourceResolver);
    }

    const QString ownerId = layer->id().toString();
    const QString cacheSignature =
        buildLayerSurfaceCacheKey(layer, surface, cacheFrameNumber);
    LayerSurfaceCacheEntry *cacheEntry = nullptr;

    const bool useSurfaceCache =
        surfaceCache && layer->matteReferences().empty() &&
        !cacheSignature.isEmpty();

    if (useSurfaceCache) {
      auto cacheIt = surfaceCache->find(ownerId);
      if (cacheIt != surfaceCache->end() && cacheIt->ownerId == ownerId &&
          cacheIt->cacheSignature == cacheSignature &&
          !cacheIt->processedSurface.isNull()) {
        cacheEntry = &(*cacheIt);
        surface = cacheIt->processedSurface;
      } else {
        std::shared_ptr<ArtifactCore::ImageF32x4_RGBA> processedBuffer;
        if (allowSurfaceCache) {
          ArtifactCore::ImageF32x4_RGBA processed;
          if (buildRasterizedSurfaceBuffer(layer, surface, &processed)) {
            processedBuffer =
                std::make_shared<ArtifactCore::ImageF32x4_RGBA>(processed);
            if (!processed.isEmpty() &&
                (!gpuTextureCacheManager ||
                 !layerUsesGpuTextureCacheForCompositionView(layer))) {
              surface = processed.toQImage();
            }
          }
        }

        LayerSurfaceCacheEntry entry;
        entry.ownerId = ownerId;
        entry.cacheSignature = cacheSignature;
        entry.processedBuffer = processedBuffer;
        entry.processedSurface = surface;
        entry.frameNumber = cacheFrameNumber;
        if (gpuTextureCacheManager &&
            layerUsesGpuTextureCacheForCompositionView(layer)) {
          if (entry.processedBuffer) {
            entry.gpuTextureHandle = gpuTextureCacheManager->acquireOrCreate(
                ownerId, cacheSignature, *entry.processedBuffer);
          } else {
            entry.gpuTextureHandle = gpuTextureCacheManager->acquireOrCreate(
                ownerId, cacheSignature, surface);
          }
        }
        (*surfaceCache)[ownerId] = entry;
        cacheEntry = &(*surfaceCache)[ownerId];
      }
    } else if (allowSurfaceCache) {
      applyRasterizerEffectsAndMasksToSurface(layer, surface);
    }

    const float baseOpacity =
        (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
    drawWithClonerEffect(
        layer, globalTransform4x4,
        [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
          const float finalOpacity = baseOpacity * instanceWeight;

          if (gpuTextureCacheManager && cacheEntry &&
              layerUsesGpuTextureCacheForCompositionView(layer)) {
            if (!gpuTextureCacheManager->isValid(
                    cacheEntry->gpuTextureHandle)) {
              const QImage &uploadSurface =
                  cacheEntry->processedSurface.isNull()
                      ? surface
                      : cacheEntry->processedSurface;
              if (cacheEntry->processedBuffer) {
                cacheEntry->gpuTextureHandle =
                    gpuTextureCacheManager->acquireOrCreate(
                        layer->id().toString(), cacheSignature,
                        *cacheEntry->processedBuffer);
              } else {
                cacheEntry->gpuTextureHandle =
                    gpuTextureCacheManager->acquireOrCreate(
                        layer->id().toString(), cacheSignature, uploadSurface);
              }
            }
            const auto binding = gpuTextureCacheManager->bindingRecord(
                cacheEntry->gpuTextureHandle);
            if (binding.isValid()) {
              renderer->drawSpriteTransformed(
                  static_cast<float>(rect.x()), static_cast<float>(rect.y()),
                  static_cast<float>(rect.width()),
                  static_cast<float>(rect.height()), instanceTransform, binding.srv,
                  finalOpacity);
              return;
            }
          }

          renderer->drawSpriteTransformed(
              static_cast<float>(rect.x()), static_cast<float>(rect.y()),
              static_cast<float>(rect.width()),
              static_cast<float>(rect.height()), instanceTransform, surface,
              finalOpacity);
        });
    return true;
  };

  if (auto *solid2D = dynamic_cast<ArtifactSolid2DLayer *>(layer)) {
    const auto color = solid2D->color();
    if (hasRasterizerEffectsOrMasks(layer)) {
      const QSize surfaceSize(
          std::max(1, static_cast<int>(std::ceil(localRect.width()))),
          std::max(1, static_cast<int>(std::ceil(localRect.height()))));
      QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
      surface.fill(toQColor(color));
      applySurfaceAndDraw(surface, localRect, true);
    } else {
      const float baseOpacity =
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
      drawWithClonerEffect(
          layer, globalTransform4x4,
          [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
            renderer->drawSolidRectTransformed(
                static_cast<float>(localRect.x()),
                static_cast<float>(localRect.y()),
                static_cast<float>(localRect.width()),
                static_cast<float>(localRect.height()), instanceTransform,
                color, baseOpacity * instanceWeight);
          });
    }
    return;
  }

  if (auto *solidImage = dynamic_cast<ArtifactSolidImageLayer *>(layer)) {
    const auto color = solidImage->color();
    if (hasRasterizerEffectsOrMasks(layer)) {
      const QSize surfaceSize(
          std::max(1, static_cast<int>(std::ceil(localRect.width()))),
          std::max(1, static_cast<int>(std::ceil(localRect.height()))));
      QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
      surface.fill(toQColor(color));
      applySurfaceAndDraw(surface, localRect, true);
    } else {
      const float baseOpacity =
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
      drawWithClonerEffect(
          layer, globalTransform4x4,
          [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
            renderer->drawSolidRectTransformed(
                static_cast<float>(localRect.x()),
                static_cast<float>(localRect.y()),
                static_cast<float>(localRect.width()),
                static_cast<float>(localRect.height()), instanceTransform,
                color, baseOpacity * instanceWeight);
          });
    }
    return;
  }

  if (auto *imageLayer = dynamic_cast<ArtifactImageLayer *>(layer)) {
    if (!hasRasterizerEffectsOrMasks(layer) &&
        imageLayer->hasCurrentFrameBuffer()) {
      const ArtifactCore::ImageF32x4_RGBA &buffer =
          imageLayer->currentFrameBuffer();
      const float baseOpacity =
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
      drawWithClonerEffect(
          layer, globalTransform4x4,
          [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
            renderer->drawSpriteTransformed(
                static_cast<float>(localRect.x()),
                static_cast<float>(localRect.y()),
                static_cast<float>(localRect.width()),
                static_cast<float>(localRect.height()), instanceTransform,
                buffer, baseOpacity * instanceWeight);
          });
      return;
    }

    const QImage img = imageLayer->toQImage();
    if (!img.isNull()) {
      applySurfaceAndDraw(img, localRect, hasRasterizerEffectsOrMasks(layer));
      return;
    }
  }

  if (auto *svgLayer = dynamic_cast<ArtifactSvgLayer *>(layer)) {
    if (svgLayer->isLoaded()) {
      if (!hasRasterizerEffectsOrMasks(layer) &&
          svgLayer->hasCurrentFrameBuffer()) {
        const ArtifactCore::ImageF32x4_RGBA &buffer =
            svgLayer->currentFrameBuffer();
        const float baseOpacity =
            (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
        drawWithClonerEffect(
            layer, globalTransform4x4,
            [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
              renderer->drawSpriteTransformed(
                  static_cast<float>(localRect.x()),
                  static_cast<float>(localRect.y()),
                  static_cast<float>(localRect.width()),
                  static_cast<float>(localRect.height()), instanceTransform,
                  buffer, baseOpacity * instanceWeight);
            });
      } else {
        const QImage svgImage = svgLayer->toQImage();
        if (!svgImage.isNull()) {
          applySurfaceAndDraw(svgImage, localRect,
                              hasRasterizerEffectsOrMasks(layer));
        } else {
          svgLayer->draw(renderer);
        }
      }
      return;
    }
  }

  if (auto *videoLayer = dynamic_cast<ArtifactVideoLayer *>(layer)) {
    const bool hasRasterizer = hasRasterizerEffectsOrMasks(layer);
    const bool hasBuffer = videoLayer->hasCurrentFrameBuffer();
    const bool currentFrameReady =
        videoLayer->isFrameCached(layer->currentFrame());
    const bool loaded = videoLayer->isLoaded();
    const bool active =
        layer->isActiveAt(FramePosition(static_cast<int>(layer->currentFrame())));
    const FramePosition ip = layer->inPoint();
    const FramePosition op = layer->outPoint();
    if (!hasRasterizer && currentFrameReady) {
      const ArtifactCore::ImageF32x4_RGBA buffer =
          videoLayer->cachedFrameImageBuffer(layer->currentFrame());
      if (buffer.isEmpty()) {
        qCDebug(compositionViewLog)
            << "[VideoLayerT] cached frame marker without frame payload"
            << videoLayer->decodeState()
            << "timelineFrame=" << layer->currentFrame();
      } else {
        const float baseOpacity =
            (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
        if (videoDebugOut) {
          *videoDebugOut = QStringLiteral(
                               "[Video] branch=buffer loaded=%1 hasBuffer=%2 "
                               "rasterizer=%3 active=%4 frameReady=%5 "
                               "range=[%6,%7] curFrame=%8")
                               .arg(loaded)
                               .arg(hasBuffer)
                               .arg(hasRasterizer)
                               .arg(active)
                               .arg(currentFrameReady)
                               .arg(ip.framePosition())
                               .arg(op.framePosition())
                               .arg(layer->currentFrame());
          *videoDebugOut += QStringLiteral(" ") + videoLayer->decodeState();
        }
        videoLayer->markFrameRenderQueued(layer->currentFrame());
        drawWithClonerEffect(
            layer, globalTransform4x4,
            [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
              renderer->drawSpriteTransformed(
                  static_cast<float>(localRect.x()),
                  static_cast<float>(localRect.y()),
                  static_cast<float>(localRect.width()),
                  static_cast<float>(localRect.height()), instanceTransform,
                  buffer, baseOpacity * instanceWeight);
            });
        return;
      }
    }

    ArtifactCore::ImageF32x4_RGBA frameBuffer;
    QImage frame;
    bool usedSyncFallback = false;
    bool usedBufferFallback = false;
    QString reason;
    if (loaded) {
      frameBuffer = videoLayer->cachedFrameImageBuffer(layer->currentFrame());
      usedBufferFallback = !frameBuffer.isEmpty();
    } else {
      reason = QStringLiteral("notLoaded");
    }
    if (frameBuffer.isEmpty() && loaded) {
      // Rendering must never wait for a decoder. This starts or observes the
      // asynchronous request and retains the last good frame until the exact
      // frame reaches the RAM cache.
      frameBuffer = videoLayer->currentFrameImageBuffer();
      usedBufferFallback = !frameBuffer.isEmpty();
    }
    if (videoDebugOut) {
      if (reason.isEmpty()) {
        if (!loaded) {
          reason = QStringLiteral("notLoaded");
        } else if (usedSyncFallback) {
          reason = QStringLiteral("syncDecode");
        } else if (usedBufferFallback) {
          reason = currentFrameReady ? QStringLiteral("bufferFallback")
                                     : QStringLiteral("repeatLastGood");
        } else if (frameBuffer.isEmpty()) {
          reason = hasBuffer ? QStringLiteral("decodeNull")
                             : QStringLiteral("noBuffer");
        } else {
          reason = QStringLiteral("ok");
        }
      }
      *videoDebugOut = QStringLiteral(
                          "[Video] branch=decode loaded=%1 hasBuffer=%2 "
                          "rasterizer=%3 active=%4 frameReady=%5 "
                          "syncFallback=%6 bufferFallback=%7 reason=%8 "
                          "range=[%9,%10] curFrame=%11")
                          .arg(loaded)
                          .arg(hasBuffer)
                          .arg(hasRasterizer)
                          .arg(active)
                          .arg(currentFrameReady)
                          .arg(usedSyncFallback)
                          .arg(usedBufferFallback)
                          .arg(reason)
                          .arg(ip.framePosition())
                          .arg(op.framePosition())
                          .arg(layer->currentFrame());
      *videoDebugOut += QStringLiteral(" ") + videoLayer->decodeState();
    }
    if (!frameBuffer.isEmpty() && !hasRasterizer &&
        layer->matteReferences().empty()) {
      const float baseOpacity =
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
      videoLayer->markFrameRenderQueued(
          layer->currentFrame(), !currentFrameReady);
      drawWithClonerEffect(
          layer, globalTransform4x4,
          [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
            renderer->drawSpriteTransformed(
                static_cast<float>(localRect.x()),
                static_cast<float>(localRect.y()),
                static_cast<float>(localRect.width()),
                static_cast<float>(localRect.height()), instanceTransform,
                frameBuffer, baseOpacity * instanceWeight);
          });
      return;
    }
    if (!frameBuffer.isEmpty()) {
      frame = frameBuffer.toQImage();
    }
    if (!frame.isNull()) {
      videoLayer->markFrameRenderQueued(
          layer->currentFrame(), !currentFrameReady);
      applySurfaceAndDraw(frame, localRect, true);
      return;
    }

    qCDebug(compositionViewLog)
        << "[VideoLayerT] drawLayerForCompositionView: no drawable frame"
        << videoLayer->decodeState()
        << "timelineFrame=" << cacheFrameNumber
        << "usedSyncFallback=" << usedSyncFallback
        << "hasCurrentFrameBuffer=" << videoLayer->hasCurrentFrameBuffer();
  }

  if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(layer)) {
    if (!hasRasterizerEffectsOrMasks(layer)) {
      textLayer->draw(renderer);
      return;
    }
    if (textLayer->hasCurrentFrameBuffer()) {
      const ArtifactCore::ImageF32x4_RGBA &buffer =
          textLayer->currentFrameBuffer();
      const float baseOpacity =
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
      drawWithClonerEffect(
          layer, globalTransform4x4,
          [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
            renderer->drawSpriteTransformed(
                static_cast<float>(localRect.x()),
                static_cast<float>(localRect.y()),
                static_cast<float>(localRect.width()),
                static_cast<float>(localRect.height()), instanceTransform,
                buffer, baseOpacity * instanceWeight);
          });
      return;
    }
    const QImage textImage = textLayer->toQImage();
    if (!textImage.isNull()) {
      applySurfaceAndDraw(textImage, localRect, true);
    }
    return;
  }

  if (auto *compLayer = dynamic_cast<ArtifactCompositionLayer *>(layer)) {
    if (auto childComp = compLayer->sourceComposition()) {
      const QSize childSize = childComp->settings().compositionSize();
      QImage childImage =
          childComp->getThumbnail(childSize.width(), childSize.height());

      if (!childImage.isNull()) {
        applySurfaceAndDraw(childImage, localRect,
                            hasRasterizerEffectsOrMasks(layer));
      }
    }
    return;
  }

  if (layer->isParticleLayer()) {
    layer->draw(renderer);
    return;
  }

  // Fallback for layer types without a direct surface accessor.
  qCDebug(compositionViewLog) << "[CompositionView] fallback layer draw"
                              << "id=" << layer->id().toString()
                              << "type=" << layer->type_index().name();
  layer->draw(renderer);
}

// Draws checkerboard in Viewport Space so transparent regions of the
// composition reveal the pattern against the viewport background.
// This should be called before blitting the composition result to the
// visible framebuffer.
void drawViewportCheckerboardBackground(ArtifactIRenderer *renderer, float vw,
                                        float vh, float tileSize) {
  if (!renderer || vw <= 0.0f || vh <= 0.0f) {
    return;
  }

  // Save current state
  const float savedZoom = renderer->getZoom();
  float savedPanX = 0.0f;
  float savedPanY = 0.0f;
  renderer->getPan(savedPanX, savedPanY);

  renderer->setCanvasSize(vw, vh);
  renderer->setZoom(1.0f);
  renderer->setPan(0.0f, 0.0f);
  renderer->drawCheckerboard(0.0f, 0.0f, vw, vh, std::max(2.0f, tileSize),
                             {0.18f, 0.18f, 0.18f, 1.0f},
                             {0.28f, 0.28f, 0.28f, 1.0f});

  // Restore state
  renderer->setZoom(savedZoom);
  renderer->setPan(savedPanX, savedPanY);
}

// Draws checkerboard over the composition canvas in Composition Space.
// Must be called BEFORE layer drawing so transparent regions reveal the
// pattern.
void drawCompositionCheckerboard(ArtifactIRenderer *renderer,
                                 const ArtifactCompositionPtr &comp,
                                 float tileSize) {
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

  renderer->drawCheckerboard(0.0f, 0.0f, cw, ch, std::max(2.0f, tileSize),
                             {0.18f, 0.18f, 0.18f, 1.0f},
                             {0.28f, 0.28f, 0.28f, 1.0f});
}

// Draws the composition background in Composition Space (0,0)-(cw,ch).
// renderer must already be configured with setCanvasSize(cw,ch), zoom, and pan.
// The vertex shader handles the Composition→View→NDC transform automatically.
// MayaGradient is handled separately in viewport space so it can cover the
// full visible viewport instead of being clipped to the composition rect.
void drawCompositionBackgroundDirect(ArtifactIRenderer *renderer, float cw,
                                     float ch, const FloatColor &bgColor,
                                     CompositionBackgroundMode mode,
                                     float checkerboardTileSize,
                                     const QImage &mayaGradientSprite) {
  if (!renderer || cw <= 0.0f || ch <= 0.0f) {
    return;
  }
  if (mode == CompositionBackgroundMode::Solid) {
    if (bgColor.a() > 0.0f) {
      renderer->drawRectLocal(0.f, 0.f, cw, ch, bgColor, 1.0f);
    }
    return;
  }
  if (mode == CompositionBackgroundMode::Checkerboard) {
    // Checkerboard is the viewport background; the composition area itself
    // should stay filled with the composition bg color.
    if (bgColor.a() > 0.0f) {
      renderer->drawRectLocal(0.f, 0.f, cw, ch, bgColor, 1.0f);
    }
    return;
  }
  // MayaGradient only affects the viewport background; the composition area
  // itself is always rendered with the solid bgColor.
  if (mode == CompositionBackgroundMode::MayaGradient) {
    if (bgColor.a() > 0.0f) {
      renderer->drawRectLocal(0.f, 0.f, cw, ch, bgColor, 1.0f);
    }
    return;
  }
  if (bgColor.a() > 0.0f) {
    renderer->drawRectLocal(0.f, 0.f, cw, ch, bgColor, 1.0f);
  }
}

void drawViewportMayaGradientBackground(ArtifactIRenderer *renderer, float vw,
                                        float vh, const FloatColor &bgColor,
                                        const QImage &mayaGradientSprite) {
  if (!renderer || vw <= 0.0f || vh <= 0.0f) {
    return;
  }
  if (!mayaGradientSprite.isNull()) {
    renderer->drawSpriteTransformed(0.f, 0.f, vw, vh, QMatrix4x4{},
                                    mayaGradientSprite, 1.0f);
    return;
  }
  renderer->drawRectLocal(0.f, 0.f, vw, vh, bgColor, 1.0f);
}

// CompositionChangeDetector - 差分レンダリング用の変更検出器
class CompositionChangeDetector {
private:
  QSet<QString> changedLayers_;
  bool compositionSettingsChanged_ = false;
  mutable QMutex mutex_; // スレッドセーフ

public:
  // レイヤー変更をマーク
  void markLayerChanged(const QString &layerId) {
    QMutexLocker locker(&mutex_);
    changedLayers_.insert(layerId);
  }

  // Composition設定変更をマーク
  void markCompositionChanged() {
    QMutexLocker locker(&mutex_);
    compositionSettingsChanged_ = true;
  }

  // 全再描画が必要か判定
  bool needsFullRedraw() const {
    QMutexLocker locker(&mutex_);
    return compositionSettingsChanged_ || changedLayers_.size() > 2;
  }

  // 変更されたレイヤー一覧を取得
  QSet<QString> getChangedLayers() const {
    QMutexLocker locker(&mutex_);
    return changedLayers_;
  }

  // 変更状態をリセット
  void reset() {
    QMutexLocker locker(&mutex_);
    changedLayers_.clear();
    compositionSettingsChanged_ = false;
  }

  // デバッグ情報
  QString debugInfo() const {
    QMutexLocker locker(&mutex_);
    return QString("ChangedLayers: %1, CompositionChanged: %2")
        .arg(changedLayers_.size())
        .arg(compositionSettingsChanged_);
  }
};

class CompositionRenderController::Impl {
public:
  std::unique_ptr<ArtifactIRenderer> renderer_;
  std::unique_ptr<CompositionRenderer> compositionRenderer_;
  ArtifactPreviewCompositionPipeline previewPipeline_;
  std::unique_ptr<TransformGizmo> gizmo_;
  std::unique_ptr<TextGizmo> textGizmo_;
  std::unique_ptr<Artifact3DGizmo> gizmo3D_;
  std::unique_ptr<ArtifactPointTrackerGizmo> trackerGizmo_;
  ArtifactCore::MotionTracker* trackerMotionTracker_ = nullptr;
  std::unique_ptr<Artifact::OffscreenCompositionRenderer> trackerOffscreenRenderer_;
  std::unique_ptr<ArtifactCore::LayerBlendPipeline> blendPipeline_;
  static constexpr int kPreviewRenderPipelineSlotCount = 2;
  struct PreviewRenderPipelineSlot {
    enum class State {
      Free,
      Retirable,
      Ready,
      Submitted,
    };
    RenderPipeline pipeline;
    void* depthTargetView = nullptr;
    QSize depthTargetSize;
    quint64 lastSubmittedFrame = 0;
    State state = State::Free;
  };
  struct PreviewFrameRequest {
    qint64 framePos = 0;
    qint64 buildTargetFrame = -1;
    QString compositionId;
    int previewDownsample = 1;
    int effectiveDownsample = 1;
    bool pipelineEnabled = false;
    QString priorityReason;
    quint64 buildGeneration = 0;
  };
  std::array<PreviewRenderPipelineSlot, kPreviewRenderPipelineSlotCount>
      previewRenderPipelineSlots_;
  int activePreviewRenderPipelineSlot_ = 0;
  QString lastPreviewRenderPipelineSlotAcquireReason_ =
      QStringLiteral("initial");
  bool lastPreviewRenderPipelineAcquireHazard_ = false;
  quint64 previewRenderPipelineAcquireCount_ = 0;
  quint64 previewRenderPipelineFreeAcquireCount_ = 0;
  quint64 previewRenderPipelineReuseRetirableCount_ = 0;
  quint64 previewRenderPipelineReuseReadyCount_ = 0;
  quint64 previewRenderPipelineReuseSubmittedCount_ = 0;
  quint64 previewRenderPipelineConsecutiveReuseCount_ = 0;
  quint64 previewRenderPipelineMaxConsecutiveReuseCount_ = 0;
  bool initialized_ = false;
  bool running_ = false;
  float devicePixelRatio_ = 1.0f;
  bool renderScheduled_ = false;
  CompositionCompareMode compareMode_ = CompositionCompareMode::Off;
  bool referencePinned_ = false;
  int referenceFrame_ = 0;

  // Fixed-rate render tick (Phase 1: infrastructure only)
  std::unique_ptr<ArtifactCore::PreciseTicker> renderTickDriver_;
  std::atomic_bool renderTickPosted_{false};
  std::atomic_bool renderDirty_{false};
  static constexpr int kRenderTickIntervalMs = 16; // ~60fps
  QElapsedTimer startupTimer_;
  bool blendPipelineReady_ = false;
  bool blendPipelineInitAttempted_ = false;
  bool gpuBlendEnabled_ =
      !qEnvironmentVariableIsSet("ARTIFACT_COMPOSITION_DISABLE_GPU_BLEND");
  QString lastVideoDebug_;
  QString lastEmittedVideoDebug_;
  QString lastRenderPathSummary_;
  QString lastCompositionVisibilitySummary_;
  QString lastBlendMaskSummary_;
  qint64 lastSetupMs_ = 0;
  qint64 lastBasePassMs_ = 0;
  qint64 lastLayerPassMs_ = 0;
  qint64 lastOverlayMs_ = 0;
  qint64 lastFlushMs_ = 0;
  qint64 lastSubmit2DMs_ = 0;
  qint64 lastPresentMs_ = 0;
  QVector<QMetaObject::Connection> layerChangedConnections_;
  ArtifactCore::EventBus::Subscription compositionChangedSubscription_;

  // 変更検出器 (差分レンダリング用)
  CompositionChangeDetector changeDetector_;

  // LOD (Level of Detail)
  std::unique_ptr<LODManager> lodManager_;
  bool lodEnabled_ = true;

  LayerID selectedLayerId_;
  bool isDraggingLayer_ = false;
  bool gizmoDragActive_ = false;
  bool textGizmoDragActive_ = false;
  bool trackerGizmoDragActive_ = false;
  bool pendingMaskCreation_ = false;
  LayerID pendingMaskLayerId_;
  MaskPath pendingMaskPath_;
  std::chrono::steady_clock::time_point quickMaskPresetStartedAt_{};
  LayerID quickMaskPresetLayerId_;
  int quickMaskPresetMaskIndex_ = -1;
  int quickMaskPresetIndex_ = -1;
  bool rectangleToolDragging_ = false;
  RectangleToolMode rectangleToolMode_ = RectangleToolMode::None;
  QPointF rectangleToolStartCanvasPos_;
  QPointF rectangleToolCurrentCanvasPos_;
  ArtifactAbstractLayerWeak rectangleToolTargetLayer_;
  bool penToolPreviewVisible_ = false;
  bool penMaskPreviewValid_ = false;
  Detail::float2 penMaskPreviewCanvasPos_ = {0.0f, 0.0f};
  void clearPendingMaskCreation();
  void beginPendingMaskCreation(const ArtifactAbstractLayerPtr &layer,
                                const QPointF &localPos);
  bool finalizePendingMaskCreation(const ArtifactAbstractLayerPtr &layer);
  void clearRectangleToolSession();
  void beginRectangleToolSession(RectangleToolMode mode,
                                 const ArtifactAbstractLayerPtr &layer,
                                 const QPointF &canvasPos);

  // MayaGradient sprite cache — regenerated only when bgColor changes
  QImage cachedMayaGradientSprite_;
  FloatColor cachedMayaGradientBgColor_ = {-1.f, -1.f, -1.f, -1.f};
  FloatColor cachedMayaGradientWarmupBgColor_ = {-1.f, -1.f, -1.f, -1.f};
  QFuture<QImage> cachedMayaGradientWarmupFuture_;
  bool cachedMayaGradientWarmupQueued_ = false;
  QString lastMayaGradientDebugState_;

  // Solo layer presence cache — invalidated on any layer solo/visible change
  bool hasSoloLayerCache_ = false;
  bool soloLayerCacheDirty_ = true;

  LayerDragMode dragMode_ = LayerDragMode::None;
  TransformGizmo::Mode gizmoMode_ = TransformGizmo::Mode::All;
  QPointF dragStartCanvasPos_;
  QPointF dragStartLayerPos_;
  float dragStartScaleX_ = 1.0f;
  float dragStartScaleY_ = 1.0f;
  QRectF dragStartBoundingBox_;
  int64_t dragFrame_ = 0;
  QPointF dragAppliedDelta_;
  bool showGrid_ = false;
  // Composition canvas fill mode, separate from the renderer's viewport clear
  // color.
  CompositionBackgroundMode compositionBackgroundMode_ =
      CompositionBackgroundMode::Checkerboard;
  float checkerboardTileSize_ = 16.0f;
  Artifact::Grid::GridSettings gridSettings_{};
  bool showGuides_ = false;
  bool showSafeMargins_ = false;
  bool showMotionPathOverlay_ = false;
  bool showEffectHitboxOverlay_ = false;
  bool showDensityHeatmapOverlay_ = false;
  bool showAnchorCenterOverlay_ = false;
  bool showCameraFrustumOverlay_ = false;
  bool showFrameInfo_ = false; // Changed to false by default
  bool showGizmoOverlay_ = true;
  bool showCompositionRegionOverlay_ =
      false; // Temporarily disable the blue frame.
  std::array<bool, lineDebugKindCount()> lineDebugVisibility_ = {
      true,  // Grid
      true,  // Axis
      true,  // Bounds
      true,  // MaskPath
      true,  // MaskHandle
      true,  // SelectionRect
      false, // MotionPath
      false, // DebugProbe
  };
  ArtifactCore::ViewOrientationNavigator viewportOrientationNavigator_;
  bool viewportOrientationActive_ = false;
  int currentFrameForOverlay_ = 0;
  quint64 renderFrameCounter_ = 0;
  std::deque<double> recentFrameTimesMs_;
  double recentFrameTimeSumMs_ = 0.0;
  double lastFrameTimeMs_ = 0.0;
  double averageFrameTimeMs_ = 0.0;
  bool renderQueueActive_ =
      false; // When true, suppress cache invalidation during Render Queue
  int lastPipelineStateMask_ = -1;
  QSize lastDispatchWarningSize_;

  PreviewRenderPipelineSlot &acquirePreviewRenderPipelineSlot() {
    ++previewRenderPipelineAcquireCount_;
    lastPreviewRenderPipelineAcquireHazard_ = false;
    int selectedIndex = -1;
    for (int i = 0; i < kPreviewRenderPipelineSlotCount; ++i) {
      if (previewRenderPipelineSlots_[i].state ==
          PreviewRenderPipelineSlot::State::Free) {
        selectedIndex = i;
        lastPreviewRenderPipelineSlotAcquireReason_ =
            QStringLiteral("free-slot");
        ++previewRenderPipelineFreeAcquireCount_;
        previewRenderPipelineConsecutiveReuseCount_ = 0;
        break;
      }
    }

    if (selectedIndex < 0) {
      for (int i = 0; i < kPreviewRenderPipelineSlotCount; ++i) {
        const auto& slot = previewRenderPipelineSlots_[i];
        if (slot.state == PreviewRenderPipelineSlot::State::Retirable) {
          if (selectedIndex < 0 ||
              slot.lastSubmittedFrame <
                  previewRenderPipelineSlots_[selectedIndex].lastSubmittedFrame) {
            selectedIndex = i;
          }
        }
      }
    }

    if (selectedIndex < 0) {
      for (int i = 0; i < kPreviewRenderPipelineSlotCount; ++i) {
        const auto& slot = previewRenderPipelineSlots_[i];
        if (slot.state == PreviewRenderPipelineSlot::State::Ready) {
          if (selectedIndex < 0 ||
              slot.lastSubmittedFrame <
                  previewRenderPipelineSlots_[selectedIndex].lastSubmittedFrame) {
            selectedIndex = i;
          }
        }
      }
    }

    if (selectedIndex < 0) {
      quint64 oldestFrame = std::numeric_limits<quint64>::max();
      for (int i = 0; i < kPreviewRenderPipelineSlotCount; ++i) {
        const auto& slot = previewRenderPipelineSlots_[i];
        if (slot.lastSubmittedFrame < oldestFrame) {
          oldestFrame = slot.lastSubmittedFrame;
          selectedIndex = i;
        }
      }
    }

    if (selectedIndex < 0) {
      selectedIndex = 0;
    }

    if (lastPreviewRenderPipelineSlotAcquireReason_ !=
        QStringLiteral("free-slot")) {
      const auto reusedState =
          previewRenderPipelineSlots_[selectedIndex].state;
      lastPreviewRenderPipelineSlotAcquireReason_ =
          reusedState == PreviewRenderPipelineSlot::State::Retirable
              ? QStringLiteral("reuse-retirable-slot")
          : reusedState == PreviewRenderPipelineSlot::State::Ready
              ? QStringLiteral("reuse-ready-slot")
              : QStringLiteral("reuse-submitted-slot");
      if (reusedState == PreviewRenderPipelineSlot::State::Retirable) {
        ++previewRenderPipelineReuseRetirableCount_;
      } else if (reusedState == PreviewRenderPipelineSlot::State::Ready) {
        ++previewRenderPipelineReuseReadyCount_;
      } else {
        ++previewRenderPipelineReuseSubmittedCount_;
        lastPreviewRenderPipelineAcquireHazard_ = true;
      }
      ++previewRenderPipelineConsecutiveReuseCount_;
      previewRenderPipelineMaxConsecutiveReuseCount_ =
          std::max(previewRenderPipelineMaxConsecutiveReuseCount_,
                   previewRenderPipelineConsecutiveReuseCount_);
    }

    activePreviewRenderPipelineSlot_ = selectedIndex;
    auto &slot = previewRenderPipelineSlots_[activePreviewRenderPipelineSlot_];
    slot.lastSubmittedFrame = renderFrameCounter_;
    slot.state = PreviewRenderPipelineSlot::State::Submitted;
    return slot;
  }

  static QString previewRenderPipelineSlotStateText(
      const PreviewRenderPipelineSlot& slot) {
    switch (slot.state) {
    case PreviewRenderPipelineSlot::State::Free:
      return QStringLiteral("free");
    case PreviewRenderPipelineSlot::State::Retirable:
      return QStringLiteral("retirable");
    case PreviewRenderPipelineSlot::State::Ready:
      return QStringLiteral("ready");
    case PreviewRenderPipelineSlot::State::Submitted:
      return QStringLiteral("submitted");
    }
    return QStringLiteral("unknown");
  }

  static quint64 estimatePreviewRenderPipelineSlotBytes(
      const PreviewRenderPipelineSlot& slot) {
    const int width = std::max(
        static_cast<int>(slot.pipeline.width()),
        slot.depthTargetSize.width());
    const int height = std::max(
        static_cast<int>(slot.pipeline.height()),
        slot.depthTargetSize.height());
    if (width <= 0 || height <= 0) {
      return 0;
    }

    constexpr quint64 kRgba16fBytesPerPixel = 8;
    constexpr quint64 kRgba8BytesPerPixel = 4;
    constexpr quint64 kDepth32BytesPerPixel = 4;
    constexpr quint64 kPreviewPipelineColorTargetCount = 4;
    const quint64 pixelCount =
        static_cast<quint64>(width) * static_cast<quint64>(height);
    return pixelCount *
           ((kRgba16fBytesPerPixel * kPreviewPipelineColorTargetCount) +
            kRgba8BytesPerPixel + kDepth32BytesPerPixel);
  }

  QString previewRenderPipelineSlotsSummary() const {
    QStringList slotNotes;
    slotNotes.reserve(kPreviewRenderPipelineSlotCount);
    for (int i = 0; i < kPreviewRenderPipelineSlotCount; ++i) {
      const auto& slot = previewRenderPipelineSlots_[i];
      const QSize pipelineSize(slot.pipeline.width(), slot.pipeline.height());
      const QSize slotSize = pipelineSize.isValid() ? pipelineSize
                                                    : slot.depthTargetSize;
      slotNotes << QStringLiteral("#%1:%2:%3x%4:depth=%5:frame=%6")
                       .arg(i)
                       .arg(previewRenderPipelineSlotStateText(slot))
                       .arg(std::max(0, slotSize.width()))
                       .arg(std::max(0, slotSize.height()))
                       .arg(slot.depthTargetView ? 1 : 0)
                       .arg(slot.lastSubmittedFrame);
    }
    return slotNotes.join(QStringLiteral(","));
  }

  quint64 previewRenderPipelineEstimatedBytes() const {
    quint64 total = 0;
    for (const auto& slot : previewRenderPipelineSlots_) {
      total += estimatePreviewRenderPipelineSlotBytes(slot);
    }
    return total;
  }

  QString previewRenderPipelinePressureSummary() const {
    return QStringLiteral("acq=%1 free=%2 reuseRetirable=%3 reuseReady=%4 "
                          "reuseSubmitted=%5 reuseStreak=%6 reuseStreakMax=%7")
        .arg(previewRenderPipelineAcquireCount_)
        .arg(previewRenderPipelineFreeAcquireCount_)
        .arg(previewRenderPipelineReuseRetirableCount_)
        .arg(previewRenderPipelineReuseReadyCount_)
        .arg(previewRenderPipelineReuseSubmittedCount_)
        .arg(previewRenderPipelineConsecutiveReuseCount_)
        .arg(previewRenderPipelineMaxConsecutiveReuseCount_);
  }

  int previewRenderPipelineSlotCountByState(
      const PreviewRenderPipelineSlot::State state) const {
    int count = 0;
    for (const auto& slot : previewRenderPipelineSlots_) {
      if (slot.state == state) {
        ++count;
      }
    }
    return count;
  }

  QString previewRenderPipelineAcquirePolicySummary() const {
    const int freeCount = previewRenderPipelineSlotCountByState(
        PreviewRenderPipelineSlot::State::Free);
    const int retirableCount = previewRenderPipelineSlotCountByState(
        PreviewRenderPipelineSlot::State::Retirable);
    const int readyCount = previewRenderPipelineSlotCountByState(
        PreviewRenderPipelineSlot::State::Ready);
    const int submittedCount = previewRenderPipelineSlotCountByState(
        PreviewRenderPipelineSlot::State::Submitted);
    QString risk = QStringLiteral("safe");
    if (submittedCount == kPreviewRenderPipelineSlotCount) {
      risk = QStringLiteral("submitted-only");
    } else if (submittedCount > 0 && freeCount == 0 && retirableCount == 0) {
      risk = QStringLiteral("submitted-pressure");
    } else if (freeCount == 0 && retirableCount == 0) {
      risk = QStringLiteral("ready-pressure");
    } else if (freeCount == 0) {
      risk = QStringLiteral("reuse-preferred");
    }

    return QStringLiteral("risk=%1 free=%2 retirable=%3 ready=%4 submitted=%5")
        .arg(risk)
        .arg(freeCount)
        .arg(retirableCount)
        .arg(readyCount)
        .arg(submittedCount);
  }

  static QString multiFramePreviewFallbackReason(
      const bool gpuBlendEnabled,
      const bool blendPipelineReady,
      const bool hasGpuBlendJustification,
      const bool hasGpuBlendBlocker,
      const bool gpuBlendPathRequested,
      const bool renderPipelineReady,
      const bool hasDepthSlot,
      const bool acquireHazard,
      const bool transparentCompositionBackgroundRequested) {
    if (!gpuBlendEnabled) {
      return QStringLiteral("gpu-blend-disabled");
    }
    if (!blendPipelineReady) {
      return QStringLiteral("blend-pipeline-not-ready");
    }
    if (!hasGpuBlendJustification) {
      return QStringLiteral("no-multi-layer-blend-work");
    }
    if (hasGpuBlendBlocker) {
      return QStringLiteral("cpu-rasterizer-layer-present");
    }
    if (!gpuBlendPathRequested) {
      return QStringLiteral("gpu-path-not-requested");
    }
    if (!renderPipelineReady) {
      return QStringLiteral("render-pipeline-not-ready");
    }
    if (!hasDepthSlot) {
      return QStringLiteral("depth-slot-unavailable");
    }
    if (acquireHazard) {
      return QStringLiteral("submitted-slot-hazard");
    }
    if (transparentCompositionBackgroundRequested) {
      return QStringLiteral("transparent-background");
    }
    return QStringLiteral("eligible");
  }

  bool ensurePreviewRenderPipelineDepthSlot(
      PreviewRenderPipelineSlot &slot,
      const int width,
      const int height) {
    if (!renderer_ || width <= 0 || height <= 0) {
      return false;
    }
    const QSize targetSize(width, height);
    if (slot.depthTargetView && slot.depthTargetSize == targetSize) {
      return true;
    }
    if (slot.depthTargetView) {
      renderer_->destroyOffscreenTexture(slot.depthTargetView);
      slot.depthTargetView = nullptr;
      slot.depthTargetSize = QSize();
    }
    slot.depthTargetView =
        renderer_->createOffscreenDepthTexture(width, height);
    if (!slot.depthTargetView) {
      return false;
    }
    slot.depthTargetSize = targetSize;
    return true;
  }

  static void publishPreviewFrameRequestResult(
      const QPointer<ArtifactPlaybackService>& weakPlayback,
      const PreviewFrameRequest& request,
      const QImage& capturedFrame) {
    if (!weakPlayback || capturedFrame.isNull()) {
      return;
    }
    const QImage frameForPublish = capturedFrame;
    QMetaObject::invokeMethod(
        weakPlayback.data(),
        [weakPlayback, request, frameForPublish]() {
          if (!weakPlayback) {
            return;
          }
          const auto currentComposition = weakPlayback->currentComposition();
          if (!currentComposition ||
              currentComposition->id().toString() != request.compositionId) {
            return;
          }
          const auto currentSummary = weakPlayback->ramPreviewSummary();
          if (currentSummary.buildQueueGeneration != request.buildGeneration ||
              currentSummary.buildQueueNextFrame != request.buildTargetFrame ||
              request.buildTargetFrame != request.framePos ||
              !weakPlayback->isRamPreviewFramePendingBuild(request.framePos)) {
            return;
          }
          const QString renderPath = request.pipelineEnabled
              ? QStringLiteral("gpu-blend")
              : QStringLiteral("fallback");
          weakPlayback->storeCompositionPreviewFrameImage(
              request.framePos, frameForPublish, request.compositionId,
              request.previewDownsample, request.effectiveDownsample,
              renderPath, request.priorityReason, true);
        },
        Qt::QueuedConnection);
  }

  // Packed render key state for fast change detection.
  // Replaces the per-frame QByteArray string concatenation.
  struct RenderKeyState {
    CompositionID compId;
    quint64 baseSerial = 0;
    quint64 overlaySerial = 0;
    int64_t frame = 0;
    int32_t viewW = 0, viewH = 0, downsample = 0;
    float zoom = 0, panX = 0, panY = 0;
    float bgR = 0, bgG = 0, bgB = 0, bgA = 0;
    int32_t bgMode = 0;
    float checkerboardTileSize = 0.0f;
    float gridMajorInterval = 0.0f;
    int32_t gridSubdivisions = 0;
    uint8_t gridShowMajor = 0, gridShowMinor = 0, gridShowAxis = 0;
    float gridMajorR = 0.0f, gridMajorG = 0.0f, gridMajorB = 0.0f,
          gridMajorA = 0.0f;
    float gridMinorR = 0.0f, gridMinorG = 0.0f, gridMinorB = 0.0f,
          gridMinorA = 0.0f;
    float gridAxisR = 0.0f, gridAxisG = 0.0f, gridAxisB = 0.0f,
          gridAxisA = 0.0f;
    int32_t gizmoMode = -1, gizmoHover = -1, gizmoActive = -1;
    uint8_t gpuBlend = 0, showGrid = 0, showGuides = 0, showSafeMargins = 0,
            showAnchorCenter = 0, showCameraFrustum = 0,
            viewportInteracting = 0;
    LayerID selectedLayerId;

    bool operator==(const RenderKeyState &o) const {
      return compId == o.compId && baseSerial == o.baseSerial &&
             overlaySerial == o.overlaySerial && frame == o.frame &&
             viewW == o.viewW && viewH == o.viewH &&
             downsample == o.downsample && zoom == o.zoom && panX == o.panX &&
             panY == o.panY && bgR == o.bgR && bgG == o.bgG && bgB == o.bgB &&
             bgA == o.bgA && bgMode == o.bgMode && gizmoMode == o.gizmoMode &&
             checkerboardTileSize == o.checkerboardTileSize &&
             gridMajorInterval == o.gridMajorInterval &&
             gridSubdivisions == o.gridSubdivisions &&
             gridShowMajor == o.gridShowMajor &&
             gridShowMinor == o.gridShowMinor &&
             gridShowAxis == o.gridShowAxis &&
             gridMajorR == o.gridMajorR && gridMajorG == o.gridMajorG &&
             gridMajorB == o.gridMajorB && gridMajorA == o.gridMajorA &&
             gridMinorR == o.gridMinorR && gridMinorG == o.gridMinorG &&
             gridMinorB == o.gridMinorB && gridMinorA == o.gridMinorA &&
             gridAxisR == o.gridAxisR && gridAxisG == o.gridAxisG &&
             gridAxisB == o.gridAxisB && gridAxisA == o.gridAxisA &&
             gizmoHover == o.gizmoHover && gizmoActive == o.gizmoActive &&
             gpuBlend == o.gpuBlend && showGrid == o.showGrid &&
             showGuides == o.showGuides &&
             showSafeMargins == o.showSafeMargins &&
             showAnchorCenter == o.showAnchorCenter &&
             showCameraFrustum == o.showCameraFrustum &&
             viewportInteracting == o.viewportInteracting &&
             selectedLayerId == o.selectedLayerId;
    }
    bool operator!=(const RenderKeyState &o) const { return !(*this == o); }
  };
  RenderKeyState lastRenderKeyState_{};
  QString lastOverlayDebugSummary_;
  quint64 baseInvalidationSerial_ = 1;
  quint64 overlayInvalidationSerial_ = 1;
  RenderDamageTracker damageTracker_;

  // Motion path cache: avoids 300x getGlobalTransformAt() calls per frame.
  // Invalidated when layer, frame position, or overlay serial changes.
  struct MotionPathCacheEntry {
    LayerID layerId;
    int64_t framePos = INT64_MIN;
    quint64 overlaySerial = UINT64_MAX;
    struct Pt {
      int frame;
      float x, y;
      int interpolation = static_cast<int>(ArtifactCore::InterpolationType::Linear);
      float frameX = 0.0f;
      float frameY = 0.0f;
      float frameW = 0.0f;
      float frameH = 0.0f;
      bool hasFrameRect = false;
    };
    std::vector<Pt> pathPoints;
    std::vector<Pt> keyPoints;
    bool valid = false;
  };
  MotionPathCacheEntry motionPathCache_;
  bool isDraggingMotionPathKeyframe_ = false;
  ArtifactAbstractLayerWeak draggingMotionPathLayer_;
  int64_t draggingMotionPathFrame_ = 0;
  MotionPathPositionSnapshot draggingMotionPathBefore_;
  QPointF draggingMotionPathStartCanvasPos_;
  int hoveredMotionPathFrame_ = -1;
  bool dropGhostVisible_ = false;
  QRectF dropGhostRect_;
  QString dropGhostTitle_;
  QString dropGhostHint_;
  QString dropCandidateLabel_;
  bool infoOverlayVisible_ = false;
  QString infoOverlayTitle_;
  QString infoOverlayDetail_;
  bool commandPaletteVisible_ = false;
  QString commandPaletteQuery_;
  QStringList commandPaletteItems_;
  bool contextMenuVisible_ = false;
  QPointF contextMenuViewportPos_;
  QString contextMenuTitle_;
  QString contextMenuSubtitle_;
  QStringList contextMenuItems_;
  QVector<bool> contextMenuItemEnabled_;
  int contextMenuSelectedIndex_ = -1;
  bool pieMenuVisible_ = false;
  PieMenuModel pieMenuModel_;
  QPointF pieMenuViewportPos_;
  QPointF pieMenuMousePos_;
  int pieMenuSelectedIndex_ = -1;
  // Full-frame clear color used before composition content is drawn.
  FloatColor viewportClearColor_;
  QImage lastLayerRtPreview_;
  QImage lastAccumRtPreview_;
  QString lastLayerRtPixelStats_;
  QString lastAccumRtPixelStats_;
  Diligent::ITextureView* lastPresentedReadbackSRV_ = nullptr;
  FloatColor lastBgColorCache_ = {-1.f, -1.f, -1.f, -1.f};
  CompositionID lastBackgroundCompositionId_;
  QHash<QString, LayerSurfaceCacheEntry> surfaceCache_;
  std::unique_ptr<GPUTextureCacheManager> gpuTextureCacheManager_;

  // Render debounce timer: coalesces rapid LayerChangedEvent notifications
  // into a single renderOneFrame() call, preventing GPU saturation during drag
  static constexpr qint64 kRenderDebounceIntervalMs = 16; // ~60fps baseline
  
  // Render tick infrastructure
  bool renderInProgress_ = false;  // Guards against concurrent renderOneFrameImpl() calls
  QElapsedTimer gizmoDragRenderTimer_;  // Throttles render frequency during gizmo drag

  // Guide positions (composition-space pixels)
  QVector<float> guideVerticals_;   // X positions
  QVector<float> guideHorizontals_; // Y positions
  float lastCanvasWidth_ = 1920.0f;
  float lastCanvasHeight_ = 1080.0f;

  // Resolution scaling
  int previewDownsample_ = 1;
  int interactivePreviewDownsampleFloor_ = 4;
  float hostWidth_ = 0.0f;
  float hostHeight_ = 0.0f;
  QPointer<QWidget> hostWidget_;
  bool viewportInteracting_ = false;
  QElapsedTimer viewportInteractionElapsedTimer_;
  int viewportInteractionIdleMs_ = 120;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  QSize pendingResizeSize_;
  bool isRubberBandSelecting_ = false;
  bool dragGroupMove_ = false;
  QPointF rubberBandStartViewportPos_;
  QPointF rubberBandCurrentViewportPos_;
  SelectionMode selectionMode_ = SelectionMode::Replace;
  QVector<ArtifactAbstractLayerPtr> dragGroupLayers_;
  QHash<QString, QPointF> dragGroupStartPositions_;

  // Mask editing state
  int hoveredMaskIndex_ = -1;
  int hoveredPathIndex_ = -1;
  int hoveredVertexIndex_ = -1;
  int hoveredMaskHandleType_ = -1;
  int draggingMaskIndex_ = -1;
  int draggingPathIndex_ = -1;
  int draggingVertexIndex_ = -1;
  int draggingMaskHandleType_ = -1;
  bool isDraggingVertex_ = false;
  bool isDraggingMaskHandle_ = false;
  bool maskEditPending_ = false;
  bool maskEditDirty_ = false;
  ArtifactAbstractLayerWeak maskEditLayer_;
  std::vector<LayerMask> maskEditBefore_;
  std::chrono::steady_clock::time_point lastLayerMutationNotify_{};

  // Cyclic selection state
  QPointF lastHitPosition_;
  LayerID lastHitLayerId_;

  // ROI debug state
  bool debugMode_ = false; // ROI デバッグ表示フラグ

  void beginMaskEditTransaction(const ArtifactAbstractLayerPtr &layer) {
    if (!layer) {
      return;
    }
    maskEditPending_ = true;
    maskEditDirty_ = false;
    maskEditLayer_ = layer;
    maskEditBefore_.clear();
    maskEditBefore_.reserve(static_cast<size_t>(layer->maskCount()));
    for (int i = 0; i < layer->maskCount(); ++i) {
      maskEditBefore_.push_back(layer->mask(i));
    }
  }

  void markMaskEditDirty() {
    if (maskEditPending_) {
      maskEditDirty_ = true;
    }
  }

  void resetLayerMutationNotify() {
    lastLayerMutationNotify_ = {};
  }

  bool shouldPublishLayerMutation() {
    constexpr auto kLayerMutationNotifyInterval = std::chrono::milliseconds(16);
    const auto now = std::chrono::steady_clock::now();
    if (lastLayerMutationNotify_.time_since_epoch().count() == 0 ||
        now - lastLayerMutationNotify_ >= kLayerMutationNotifyInterval) {
      lastLayerMutationNotify_ = now;
      return true;
    }
    return false;
  }

  void publishLayerModified(const ArtifactAbstractLayerPtr& layer, bool force = false) {
    if (!layer) {
      return;
    }
    if (!force && !shouldPublishLayerMutation()) {
      return;
    }
    if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
  }

  void commitMaskEditTransaction() {
    if (!maskEditPending_) {
      return;
    }

    auto layer = maskEditLayer_.lock();
    maskEditPending_ = false;
    maskEditLayer_.reset();

    if (!layer || !maskEditDirty_) {
      maskEditBefore_.clear();
      maskEditDirty_ = false;
      return;
    }

    std::vector<LayerMask> afterMasks;
    afterMasks.reserve(static_cast<size_t>(layer->maskCount()));
    for (int i = 0; i < layer->maskCount(); ++i) {
      afterMasks.push_back(layer->mask(i));
    }

    if (auto *undo = UndoManager::instance()) {
      undo->push(std::make_unique<MaskEditCommand>(layer, maskEditBefore_,
                                                   std::move(afterMasks)));
    }

    maskEditBefore_.clear();
    maskEditDirty_ = false;
  }

  bool cyclePresetLayerMaskForLayer(const ArtifactAbstractLayerPtr &layer,
                                    bool reverse) {
    if (!layer) {
      return false;
    }

    const QRectF localBounds = layer->localBounds().normalized();
    if (!localBounds.isValid() || localBounds.width() < 1.0 ||
        localBounds.height() < 1.0) {
      return false;
    }

    const auto now = std::chrono::steady_clock::now();
    constexpr auto kCycleWindow = std::chrono::seconds(4);
    const bool isSameLayer =
        !quickMaskPresetLayerId_.isNil() && quickMaskPresetLayerId_ == layer->id();
    const bool isArmed = isSameLayer &&
                         quickMaskPresetStartedAt_.time_since_epoch().count() > 0 &&
                         now - quickMaskPresetStartedAt_ <= kCycleWindow;

    if (!isArmed) {
      quickMaskPresetLayerId_ = layer->id();
      quickMaskPresetStartedAt_ = now;
      quickMaskPresetIndex_ =
          reverse ? static_cast<int>(kQuickMaskPresetOrder.size()) - 1 : 0;
      quickMaskPresetMaskIndex_ = -1;
    } else if (reverse) {
      quickMaskPresetIndex_ =
          (quickMaskPresetIndex_ - 1 + static_cast<int>(kQuickMaskPresetOrder.size())) %
          static_cast<int>(kQuickMaskPresetOrder.size());
    } else {
      quickMaskPresetIndex_ =
          (quickMaskPresetIndex_ + 1) % static_cast<int>(kQuickMaskPresetOrder.size());
    }

    const QuickMaskPreset preset =
        kQuickMaskPresetOrder[static_cast<size_t>(quickMaskPresetIndex_)];
    const QRectF presetRect = quickMaskPresetRect(preset, localBounds);

    MaskPath path;
    for (const QPointF &corner : rectCorners(presetRect)) {
      MaskVertex vertex;
      vertex.position = corner;
      vertex.inTangent = QPointF(0.0, 0.0);
      vertex.outTangent = QPointF(0.0, 0.0);
      path.addVertex(vertex);
    }
    path.setClosed(true);

    LayerMask mask;
    mask.addMaskPath(path);

    beginMaskEditTransaction(layer);
    if (isArmed && quickMaskPresetMaskIndex_ >= 0 &&
        quickMaskPresetMaskIndex_ < layer->maskCount()) {
      layer->setMask(quickMaskPresetMaskIndex_, mask);
    } else {
      layer->addMask(mask);
      quickMaskPresetMaskIndex_ = layer->maskCount() - 1;
    }
    markMaskEditDirty();
    commitMaskEditTransaction();
    publishLayerModified(layer, true);
    return true;
  }

  void beginMotionPathDrag(const ArtifactAbstractLayerPtr &layer,
                           int64_t frame, const QPointF &canvasPos,
                           const MotionPathPositionSnapshot &before) {
    draggingMotionPathLayer_ = layer;
    draggingMotionPathFrame_ = frame;
    draggingMotionPathBefore_ = before;
    draggingMotionPathStartCanvasPos_ = canvasPos;
    isDraggingMotionPathKeyframe_ = true;
  }

  void clearMotionPathDragState() {
    isDraggingMotionPathKeyframe_ = false;
    draggingMotionPathLayer_.reset();
    draggingMotionPathFrame_ = 0;
    draggingMotionPathBefore_ = {};
    draggingMotionPathStartCanvasPos_ = {};
    hoveredMotionPathFrame_ = -1;
  }

  bool setHoveredMotionPathFrame(int frame) {
    if (hoveredMotionPathFrame_ == frame) {
      return false;
    }
    hoveredMotionPathFrame_ = frame;
    invalidateOverlayComposite();
    return true;
  }

  bool applyMotionPathDrag(const ArtifactAbstractLayerPtr &layer,
                           const QPointF &canvasPos) {
    auto draggingLayer = draggingMotionPathLayer_.lock();
    if (!layer || !renderer_ || !isDraggingMotionPathKeyframe_ ||
        !draggingLayer || draggingLayer->id() != layer->id()) {
      return false;
    }

    QPointF localPos = canvasPos;
    if (const auto parent = layer->parentLayer()) {
      const QTransform parentGlobal =
          parent->getGlobalTransformAt(draggingMotionPathFrame_);
      bool invertible = false;
      const QTransform invParent = parentGlobal.inverted(&invertible);
      if (!invertible) {
        return false;
      }
      localPos = invParent.map(canvasPos);
    }

    auto &t3d = layer->transform3D();
    const ArtifactCore::RationalTime time(draggingMotionPathFrame_, 24);
    t3d.setPositionKeyFrameValueAt(time, static_cast<float>(localPos.x()),
                                   static_cast<float>(localPos.y()));
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    if (auto *comp =
            static_cast<ArtifactAbstractComposition *>(layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
    invalidateOverlayComposite();
    return true;
  }

  void sync2DGizmosForLayer(const ArtifactAbstractLayerPtr &layer) {
    std::vector<ArtifactAbstractLayerPtr> selectedTargets;
    if (auto *app = ArtifactApplicationManager::instance()) {
      if (auto *selection = app->layerSelectionManager()) {
        for (const auto &candidate : selection->selectedLayers()) {
          if (candidate) {
            selectedTargets.push_back(candidate);
          }
        }
      }
    }
    if (selectedTargets.empty() && layer) {
      selectedTargets.push_back(layer);
    }
    const bool useTextGizmo = layerUsesTextGizmo(layer);
    if (textGizmo_) {
      textGizmo_->setLayer(useTextGizmo ? layer : nullptr);
    }
    if (gizmo_) {
      if (useTextGizmo) {
        gizmo_->setLayer(nullptr);
      } else if (selectedTargets.size() > 1) {
        gizmo_->setTargetLayers(std::move(selectedTargets));
      } else {
        gizmo_->setLayer(layer);
      }
    }
  }

  void
  syncSelectedLayerOverlayState(const ArtifactCompositionPtr &composition) {
    ArtifactAbstractLayerPtr layer;
    if (composition && !selectedLayerId_.isNil()) {
      layer = composition->layerById(selectedLayerId_);
    }

    sync2DGizmosForLayer(layer);

    if (gizmo3D_ && layer && layer->is3D()) {
      syncGizmo3DFromLayer(layer);
    }
  }

  void syncGizmo3DFromLayer(const ArtifactAbstractLayerPtr &layer) {
    if (!gizmo3D_ || !layer) {
      return;
    }

    if (layer->is3D()) {
      const auto &t3 = layer->transform3D();
      gizmo3D_->setDepthEnabled(true);
      gizmo3D_->setTransform(layer->position3D(), layer->rotation3D());
      gizmo3D_->setScale(QVector3D(t3.scaleX(), t3.scaleY(), 1.0f));
      return;
    }

    const QRectF localRect = layer->localBounds();
    const QTransform globalTransform = layer->getGlobalTransform();
    const QPointF center =
        localRect.isValid()
            ? globalTransform.map(localRect.center())
            : QPointF(globalTransform.dx(), globalTransform.dy());
    const float scaleX = std::max<float>(
        0.01f, static_cast<float>(
                   std::hypot(globalTransform.m11(), globalTransform.m12())));
    const float scaleY = std::max<float>(
        0.01f, static_cast<float>(
                   std::hypot(globalTransform.m21(), globalTransform.m22())));
    const float rotationZ =
        std::atan2(globalTransform.m12(), globalTransform.m11()) *
        (180.0f / 3.14159265358979323846f);

    gizmo3D_->setDepthEnabled(false);
    gizmo3D_->setTransform(QVector3D(static_cast<float>(center.x()),
                                     static_cast<float>(center.y()), 0.0f),
                           QVector3D(0.0f, 0.0f, rotationZ));
    gizmo3D_->setScale(QVector3D(scaleX, scaleY, 1.0f));
  }

  QRectF rubberBandCanvasRect() const {
    return viewportRectToCanvasRect(renderer_.get(),
                                    rubberBandStartViewportPos_,
                                    rubberBandCurrentViewportPos_);
  }

  void clearSelectionGestureState() {
    isDraggingLayer_ = false;
    isRubberBandSelecting_ = false;
    dragGroupMove_ = false;
    dragGroupLayers_.clear();
    dragGroupStartPositions_.clear();
  }

  void applyCompositionState(const ArtifactCompositionPtr &composition) {
    if (!renderer_ || !composition) {
      return;
    }

    // Ensure renderer is initialized before any rendering operations
    if (!renderer_->isInitialized() && hostWidget_) {
      renderer_->initialize(hostWidget_.data());
    }

    const auto size = composition->settings().compositionSize();
    const float cw = static_cast<float>(size.width() > 0 ? size.width() : 1920);
    const float ch =
        static_cast<float>(size.height() > 0 ? size.height() : 1080);
    lastCanvasWidth_ = cw;
    lastCanvasHeight_ = ch;
    if (compositionRenderer_) {
      compositionRenderer_->SetCompositionSize(cw, ch);
      compositionRenderer_->ApplyCompositionSpace();
      renderer_->setCanvasSize(cw, ch);
    } else {
      renderer_->setCanvasSize(cw, ch);
    }

    // NOTE: previewRenderPipelineSlots_ は renderOneFrameImpl() で
    // ビューポートの実サイズ (rcw, rch) を使って初期化される。ここで
    // コンポジションサイズ (cw, ch) で初期化すると、サイズが異なる場合に
    // 未初期化の D3D12 テクスチャが生成され、次フレームで再び不一致が起き、
    // ゴミデータが画面に出ることがある。サイズ管理は renderOneFrameImpl() に
    // 一元化する。
  }

  void bindCompositionChanged(CompositionRenderController *owner,
                              const ArtifactCompositionPtr &composition) {
    // Layer change notifications are now handled exclusively via
    // CompositionChangedEvent.
    compositionChangedSubscription_.disconnect();
    if (!owner || !composition) {
      return;
    }
    {
      const QString compositionId = composition->id().toString();
      compositionChangedSubscription_ =
          eventBus_.subscribe<CompositionChangedEvent>(
              [this, owner, composition, compositionId](const CompositionChangedEvent &event) {
                if (event.compositionId != compositionId) {
                  return;
                }
                applyCompositionState(composition);
                invalidateBaseComposite();
                invalidateOverlayComposite();
                owner->markRenderDirty();
              });
    }
  }

  void invalidateLayerSurfaceCache(const ArtifactAbstractLayerPtr &layer) {
    if (!layer) {
      return;
    }
    const QString ownerId = layer->id().toString();
    surfaceCache_.remove(ownerId);
    if (gpuTextureCacheManager_) {
      gpuTextureCacheManager_->invalidateOwner(ownerId);
    }
    LayerInvalidationRegion region;
    region.source = LayerInvalidationRegion::Source::Content;
    region.layerId = ownerId;
    damageTracker_.markDirty(ownerId, region);
  }

  void invalidateBaseComposite() {
    ++baseInvalidationSerial_;
    lastRenderKeyState_ = {};
    damageTracker_.clearAll();
  }

  void invalidateOverlayComposite() {
    ++overlayInvalidationSerial_;
    lastRenderKeyState_ = {};
  }

  RenderDamageTracker& damageTracker() { return damageTracker_; }

  void drawViewportGhostOverlay(CompositionRenderController *owner,
                                const ArtifactCompositionPtr &comp,
                                const ArtifactAbstractLayerPtr &selectedLayer,
                                const FramePosition &currentFrame);
  void drawCameraFrustumOverlay(ArtifactIRenderer *renderer,
                                const CompositionRenderController::CameraFrustumVisual &visual,
                                bool activeCamera);
  QRectF commandPaletteRect() const;
  QRectF contextMenuRect() const;
  QRectF pieMenuRect() const;
  QRectF viewportOverlayItemRect(int index) const;
  int viewportOverlayItemAt(const QPointF &viewportPos) const;
  int pieMenuItemAt(const QPointF &viewportPos) const;
  void drawPieMenuOverlay();
  void updateContextMenuOverlayMousePos(const QPointF &viewportPos);
  void drawViewportUiOverlay();

  // 変更検出器へのアクセス (デバッグ用)
  const CompositionChangeDetector &changeDetector() const {
    return changeDetector_;
  }

  CompositionChangeDetector &changeDetector() { return changeDetector_; }

  void renderOneFrameImpl(CompositionRenderController *owner);
};

CompositionRenderController::CompositionRenderController(QObject *parent)
    : QObject(parent), impl_(new Impl()) {
  impl_->viewportClearColor_ =
      FloatColor{QColor(28, 40, 56).redF(), QColor(28, 40, 56).greenF(),
                 QColor(28, 40, 56).blueF(), 1.0f};
  impl_->gizmo_ = std::make_unique<TransformGizmo>();
  impl_->textGizmo_ = std::make_unique<TextGizmo>();
  impl_->gizmo3D_ = std::make_unique<Artifact3DGizmo>(this);
  impl_->trackerGizmo_ = std::make_unique<ArtifactPointTrackerGizmo>();

  // Connect to project service to track layer selection
  if (auto *svc = ArtifactProjectService::instance()) {
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<LayerSelectionChangedEvent>(
            [this](const LayerSelectionChangedEvent &event) {
              auto comp = impl_->previewPipeline_.composition();
              if (comp && !event.compositionId.isEmpty() &&
                  comp->id().toString() != event.compositionId) {
                return;
              }
              // Guard: ignore spurious nil events if the selection manager
              // still has a valid current layer. This prevents property-edit
              // notifications from clearing the gizmo via the EventBus path.
              const LayerID incomingId(event.layerId);
              if (incomingId.isNil() && !impl_->selectedLayerId_.isNil()) {
                auto *app = ArtifactApplicationManager::instance();
                auto *sel = app ? app->layerSelectionManager() : nullptr;
                if (sel && sel->currentLayer()) {
                  return;
               }
              }
              if (impl_->selectedLayerId_ != incomingId) {
                impl_->clearPendingMaskCreation();
                impl_->selectedLayerId_ = incomingId;
                impl_->invalidateOverlayComposite();
              }
              impl_->syncSelectedLayerOverlayState(comp);
              markRenderDirty();
            }));

    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<LayerChangedEvent>(
            [this](const LayerChangedEvent &event) {
              auto comp = impl_->previewPipeline_.composition();
              if (!comp) {
                return;
              }
              // Accept events with empty compositionId (layer may not have
              // its composition pointer set yet, e.g. during creation) as
              // well as events matching the current composition.
              if (!event.compositionId.isEmpty() &&
                  comp->id().toString() != event.compositionId) {
                return;
              }
              if (impl_->renderQueueActive_) {
                return;
              }
              const auto layerId = LayerID(event.layerId);
              if (event.changeType == LayerChangedEvent::ChangeType::Created ||
                  event.changeType == LayerChangedEvent::ChangeType::Removed) {
                // Structural change: full cache clear + composition state sync
                if (event.changeType ==
                    LayerChangedEvent::ChangeType::Removed) {
                  if (layerId == impl_->selectedLayerId_) {
                    impl_->selectedLayerId_ = LayerID();
                  }
               }
                impl_->surfaceCache_.clear();
                if (impl_->gpuTextureCacheManager_) {
                  impl_->gpuTextureCacheManager_->clear();
               }
                impl_->applyCompositionState(comp);
              } else {
                // Property/transform modification: invalidate only this layer
                // ギズモドラッグ中は同じレイヤーのピクセルは変わらないので、
                // transform だけの更新では重いサーフェス再生成を避ける。
                if (auto layer = comp->layerById(layerId)) {
                  const bool skipCacheInvalidation =
                      impl_->gizmoDragActive_ &&
                      layerId == impl_->selectedLayerId_;
                  if (!skipCacheInvalidation) {
                    impl_->invalidateLayerSurfaceCache(layer);
                  }
                  impl_->changeDetector_.markLayerChanged(
                      layer->id().toString());
               }
              }
              impl_->invalidateBaseComposite();
              // ギズモドラッグ中はオーバーレイ同期コストを省く（ドラッグ終了時に一括同期）
              if (!impl_->gizmoDragActive_) {
                impl_->syncSelectedLayerOverlayState(
                    impl_->previewPipeline_.composition());
              }
              // Coalesce rapid property changes on the fixed render tick.
              markRenderDirty();
            }));

    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>(
            [this, svc](const CurrentCompositionChangedEvent &event) {
              ArtifactCompositionPtr comp;
              if (!event.compositionId.trimmed().isEmpty()) {
                const auto found =
                    svc->findComposition(CompositionID(event.compositionId));
                if (found.success && !found.ptr.expired()) {
                  comp = found.ptr.lock();
               }
              }
              if (!comp) {
                comp = resolvePreferredComposition(svc);
              }
              setComposition(comp);
            }));

    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<ProjectChangedEvent>(
            [this, svc](const ProjectChangedEvent &) {
              auto latest = resolvePreferredComposition(svc);
              auto current = impl_->previewPipeline_.composition();
              if (latest != current) {
                setComposition(latest);
              } else {
                impl_->invalidateBaseComposite();
              }
            }));

    // Handle resolution changes via internal event bus
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<PreviewQualityPresetChangedEvent>(
            [this](const PreviewQualityPresetChangedEvent &event) {
              setPreviewQualityPreset(
                  static_cast<PreviewQualityPreset>(event.preset));
            }));

    // Initial sync
    setPreviewQualityPreset(svc->previewQualityPreset());
  }

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ToolChangedEvent>(
          [this](const ToolChangedEvent &event) {
            switch (event.toolType) {
            case ToolType::Selection:
              setGizmoMode(TransformGizmo::Mode::All);
              break;
            case ToolType::Move:
              setGizmoMode(TransformGizmo::Mode::Translation);
              break;
            case ToolType::Rotation:
              setGizmoMode(TransformGizmo::Mode::Rotation);
              break;
            case ToolType::Scale:
              setGizmoMode(TransformGizmo::Mode::Scale);
              break;
            case ToolType::AnchorPoint:
              setGizmoMode(TransformGizmo::Mode::AnchorPoint);
              break;
            default:
              // For other tools, we might want to disable the gizmo or use a
              // basic mode
              setGizmoMode(TransformGizmo::Mode::None);
              break;
            }
          }));
}

CompositionRenderController::~CompositionRenderController() {
  destroy();
  delete impl_;
}

void CompositionRenderController::initialize(QWidget *hostWidget) {
  if (impl_->initialized_ || hostWidget == nullptr) {
    return;
  }

  impl_->hostWidget_ = hostWidget;
  impl_->startupTimer_.start();
  impl_->renderer_ = std::make_unique<ArtifactIRenderer>();
  impl_->renderer_->initialize(hostWidget);

  if (!impl_->renderer_->isInitialized()) {
    qWarning() << "[CompositionRenderController] renderer initialize failed for"
               << hostWidget << "size=" << hostWidget->size()
               << "DPR=" << hostWidget->devicePixelRatio();
    impl_->renderer_.reset();
    return;
  }
  impl_->compositionRenderer_ =
      std::make_unique<CompositionRenderer>(*impl_->renderer_);
  impl_->renderer_->setClearColor(impl_->viewportClearColor_);
  impl_->devicePixelRatio_ = static_cast<float>(hostWidget->devicePixelRatio());
  impl_->hostWidth_ =
      static_cast<float>(hostWidget->width()) * impl_->devicePixelRatio_;
  impl_->hostHeight_ =
      static_cast<float>(hostWidget->height()) * impl_->devicePixelRatio_;
  impl_->renderer_->setViewportSize(impl_->hostWidth_, impl_->hostHeight_);

  const auto comp = impl_->previewPipeline_.composition();
  if (comp) {
    impl_->applyCompositionState(comp);
    impl_->renderer_->fillToViewport();
  }

  // Precise render tick: clocked from ArtifactCore and marshalled back onto the
  // UI thread to keep swapchain / QWidget access thread-safe.
  impl_->renderTickDriver_ = std::make_unique<ArtifactCore::PreciseTicker>();
  impl_->renderTickDriver_->setInterval(std::chrono::milliseconds(
      compositionPreviewIntervalMs(impl_->previewPipeline_.composition())));
  impl_->renderTickDriver_->setCallback([this]() {
    if (!impl_ || impl_->renderTickPosted_.exchange(true, std::memory_order_acq_rel)) {
      return;
    }
    QMetaObject::invokeMethod(
        this,
        [this]() {
          if (!impl_) {
            return;
          }
          impl_->renderTickPosted_.store(false, std::memory_order_release);
          renderCrashTrace("tick-enter", impl_->renderFrameCounter_);
          if (impl_->viewportInteracting_ &&
              impl_->viewportInteractionElapsedTimer_.isValid() &&
              impl_->viewportInteractionElapsedTimer_.elapsed() >=
                  impl_->viewportInteractionIdleMs_) {
            renderCrashTrace("tick-finish-viewport-interaction",
                             impl_->renderFrameCounter_);
            finishViewportInteraction();
          }
          if (!impl_->initialized_ || !impl_->renderer_ || !impl_->running_) {
            renderCrashTrace("tick-skip-not-ready", impl_->renderFrameCounter_);
            return;
          }
          if (auto *host = impl_->hostWidget_.data()) {
            if (!host->isVisible()) {
              renderCrashTrace("tick-skip-host-hidden",
                               impl_->renderFrameCounter_);
              return;
            }
          }
          if (!impl_->renderDirty_.exchange(false, std::memory_order_acq_rel)) {
            if (!impl_->viewportInteracting_ && impl_->renderTickDriver_) {
              impl_->renderTickDriver_->stop();
            }
            return;
          }
          if (impl_->renderInProgress_) {
            impl_->renderDirty_.store(true, std::memory_order_release);
            renderCrashTrace("tick-skip-in-progress",
                             impl_->renderFrameCounter_);
            return;
          }
          struct RenderInProgressGuard {
            bool& flag;
            explicit RenderInProgressGuard(bool& f) : flag(f) { flag = true; }
            ~RenderInProgressGuard() { flag = false; }
          } renderGuard(impl_->renderInProgress_);
          renderCrashTrace("tick-render-begin", impl_->renderFrameCounter_);
          impl_->renderOneFrameImpl(this);
          renderCrashTrace("tick-render-end", impl_->renderFrameCounter_);
        },
        Qt::QueuedConnection);
  });
  impl_->renderTickDriver_->start();

  // PlaybackService のフレーム変更に合わせて再描画
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<FrameChangedEvent>(
          [this](const FrameChangedEvent &) {
            // 可視性チェック: 非表示（他タブの裏など）なら描画しない
            if (auto *owner = qobject_cast<QWidget *>(parent())) {
              if (!owner->isVisible())
                return;
            }
            // comp->goToFrame() は ArtifactPlaybackService::syncCurrentCompositionFrame()
            // が publishFrame より先に投入済みのため、ここで重複呼び出しは不要。
            const auto *playback = ArtifactPlaybackService::instance();
            if (!playback || !playback->isPlaying()) {
              impl_->invalidateOverlayComposite();
            }
            markRenderDirty();
          }));
  if (!impl_->gpuTextureCacheManager_) {
    impl_->gpuTextureCacheManager_ = std::make_unique<GPUTextureCacheManager>();
  }
  if (auto device = impl_->renderer_->device()) {
    impl_->gpuTextureCacheManager_->setDevice(device);
    impl_->gpuTextureCacheManager_->setBudgetBytes(512ull * 1024ull * 1024ull);
    impl_->gpuTextureCacheManager_->setMaxEntries(256);
  }

  impl_->initialized_ = true;

  // Schedule blend pipeline initialization off the hot render path.
  // blendPipeline_->initialize() compiles GPU shaders (slow); running it
  // inside renderOneFrameImpl would stall the render timer for hundreds of ms.
  if (impl_->gpuBlendEnabled_) {
    QTimer::singleShot(1500, this, [this]() {
      if (!impl_ || !impl_->initialized_)
        return;
      if (impl_->blendPipelineReady_ || impl_->blendPipelineInitAttempted_)
        return;
      impl_->blendPipelineInitAttempted_ = true;
      auto *renderer = impl_->renderer_.get();
      if (!renderer)
        return;
      auto device = renderer->device();
      if (!device)
        return;
      if (!impl_->blendPipeline_)
        impl_->blendPipeline_ = renderer->createLayerBlendPipeline();
      if (impl_->blendPipeline_) {
        QElapsedTimer t;
        t.start();
        impl_->blendPipelineReady_ = impl_->blendPipeline_->initialize();
        if (impl_->blendPipelineReady_) {
          qInfo() << "[CompositionView][Startup] blend pipeline lazy init ms="
                  << t.elapsed();
        } else {
          qWarning()
              << "[CompositionView] LayerBlendPipeline FAILED to initialize.";
        }
      }
    });
  }
}

void CompositionRenderController::destroy() {
  if (impl_->renderTickDriver_) {
    impl_->renderTickDriver_->stop();
  }
  stop();
  if (impl_->renderer_) {
    for (auto &slot : impl_->previewRenderPipelineSlots_) {
      if (slot.depthTargetView) {
        impl_->renderer_->destroyOffscreenTexture(slot.depthTargetView);
        slot.depthTargetView = nullptr;
        slot.depthTargetSize = QSize();
      }
    }
  }
  impl_->compositionRenderer_.reset();
  impl_->surfaceCache_.clear();
  if (impl_->gpuTextureCacheManager_) {
    impl_->gpuTextureCacheManager_->clearDevice();
    impl_->gpuTextureCacheManager_.reset();
  }
  impl_->blendPipeline_.reset();
  impl_->blendPipelineReady_ = false;
  impl_->blendPipelineInitAttempted_ = false;
  impl_->lastPresentedReadbackSRV_ = nullptr;
  if (impl_->renderer_) {
    impl_->renderer_->destroy();
    impl_->renderer_.reset();
  }
  impl_->invalidateBaseComposite();
  impl_->initialized_ = false;
}

bool CompositionRenderController::isInitialized() const {
  return impl_->initialized_;
}

void CompositionRenderController::start() {
  if (!impl_->initialized_ || impl_->running_) {
    return;
  }
  impl_->running_ = true;
  if (impl_->renderTickDriver_ && !impl_->renderTickDriver_->isRunning()) {
    impl_->renderTickDriver_->start();
  }
  impl_->invalidateBaseComposite();
  // Continuous timer removed for performance.
  // Rendering is now event-driven (frameChanged, propertyChanged, etc.)
  markRenderDirty();
}

void CompositionRenderController::stop() {
  if (impl_->renderTickDriver_) {
    impl_->renderTickDriver_->stop();
  }
  if (!impl_->running_) {
    return;
  }
  impl_->running_ = false;
  if (impl_->renderer_) {
    impl_->renderer_->flushAndWait();
  }
}
bool CompositionRenderController::isRunning() const { return impl_->running_; }

void CompositionRenderController::recreateSwapChain(QWidget *hostWidget) {
  if (!impl_->initialized_ || !impl_->renderer_ || hostWidget == nullptr) {
    return;
  }
  impl_->hostWidget_ = hostWidget;
  impl_->renderer_->flushAndWait();
  // If swapchain was never created (e.g., widget was 0×0 at init time),
  // use createSwapChain which handles fresh creation + shader/PSO setup.
  if (!impl_->renderer_->hasSwapChain()) {
    qDebug() << "[CompositionRenderController] recreateSwapChain: no swapchain "
                "— calling createSwapChain";
    impl_->renderer_->createSwapChain(hostWidget);
  } else {
    impl_->renderer_->recreateSwapChain(hostWidget);
  }
  impl_->invalidateBaseComposite();
}

void CompositionRenderController::setViewportSize(float w, float h) {
  if (!impl_->renderer_) {
    return;
  }
  // Refresh DPR whenever the viewport is resized (handles window-to-monitor
  // changes)
  if (impl_->hostWidget_) {
    impl_->devicePixelRatio_ =
        static_cast<float>(impl_->hostWidget_->devicePixelRatio());
  }
  // Callers pass logical pixels; convert to physical pixels for the renderer
  const float newHostWidth = w * impl_->devicePixelRatio_;
  const float newHostHeight = h * impl_->devicePixelRatio_;
  if (std::abs(newHostWidth - impl_->hostWidth_) < 0.5f &&
      std::abs(newHostHeight - impl_->hostHeight_) < 0.5f) {
    return;
  }
  impl_->hostWidth_ = newHostWidth;
  impl_->hostHeight_ = newHostHeight;
  impl_->renderer_->setViewportSize(impl_->hostWidth_, impl_->hostHeight_);
  impl_->invalidateBaseComposite();
}

void CompositionRenderController::setPreviewQualityPreset(
    PreviewQualityPreset preset) {
  int factor = 1;
  switch (preset) {
  case PreviewQualityPreset::Final:
    factor = 1;
    break;
  case PreviewQualityPreset::Preview:
    factor = 2;
    break;
  case PreviewQualityPreset::Draft:
    factor = 4;
    break;
  default:
    factor = 1;
    break;
  }

  if (impl_->previewDownsample_ != factor) {
    impl_->previewDownsample_ = factor;
    if (auto *playback = ArtifactPlaybackService::instance()) {
      playback->invalidateRamPreviewCache(
          QStringLiteral("preview-quality-changed"));
    }
    if (impl_->hostWidth_ > 0 && impl_->hostHeight_ > 0) {
      // hostWidth_/hostHeight_ are already physical pixels; call renderer
      // directly
      impl_->renderer_->setViewportSize(impl_->hostWidth_, impl_->hostHeight_);
    }
    impl_->invalidateBaseComposite();
    markRenderDirty();
  }
}

void CompositionRenderController::panBy(const QPointF &viewportDelta) {
  if (!impl_->renderer_) {
    return;
  }
  impl_->renderer_->panBy((float)viewportDelta.x() * impl_->devicePixelRatio_,
                          (float)viewportDelta.y() * impl_->devicePixelRatio_);
  // Phase 2: Use fixed-rate render tick instead of renderOneFrame().
  // No invalidateBaseComposite() needed — panX/panY in RenderKeyState
  // already detects the change.
  markRenderDirty();
}

void CompositionRenderController::setGizmoMode(
    const TransformGizmo::Mode mode) {
  if (!impl_) {
    return;
  }
  impl_->gizmoMode_ = mode;
  if (impl_->gizmo_) {
    impl_->gizmo_->setMode(mode);
  }
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

TransformGizmo::Mode CompositionRenderController::gizmoMode() const {
  return impl_ ? impl_->gizmoMode_ : TransformGizmo::Mode::All;
}

void CompositionRenderController::notifyViewportInteractionActivity() {
  const bool wasInteracting = impl_->viewportInteracting_;
  impl_->viewportInteracting_ = true;
  impl_->viewportInteractionElapsedTimer_.restart();
  if (impl_->running_ && impl_->renderTickDriver_ &&
      !impl_->renderTickDriver_->isRunning()) {
    impl_->renderTickDriver_->start();
  }
  if (!wasInteracting) {
    impl_->invalidateBaseComposite();
  }
}

void CompositionRenderController::finishViewportInteraction() {
  if (!impl_->viewportInteracting_) {
    return;
  }
  impl_->viewportInteracting_ = false;
  impl_->invalidateBaseComposite();
  markRenderDirty();
}

void CompositionRenderController::setComposition(
    ArtifactCompositionPtr composition) {
  qCDebug(compositionViewLog) << "[CompositionView] setComposition"
                              << "isNull=" << (composition == nullptr) << "id="
                              << (composition ? composition->id().toString()
                                              : QStringLiteral("<null>"));

  auto currentComposition = impl_->previewPipeline_.composition();
  const bool samePointer = (currentComposition == composition);
  const bool sameId = !samePointer && composition && currentComposition &&
                      composition->id() == currentComposition->id();

  if (samePointer || sameId) {
    if (auto *playback = ArtifactPlaybackService::instance()) {
      if (playback->currentComposition() != composition) {
        playback->setCurrentComposition(composition);
      }
    }
    if (composition && impl_->renderer_) {
      impl_->applyCompositionState(composition);
    }
    if (composition && !impl_->selectedLayerId_.isNil()) {
      impl_->sync2DGizmosForLayer(
          composition->layerById(impl_->selectedLayerId_));
    } else if (!composition) {
      impl_->sync2DGizmosForLayer(nullptr);
    }
    if (sameId) {
      // Pointer changed but same composition — re-bind changed signals
      impl_->previewPipeline_.setComposition(composition);
      impl_->bindCompositionChanged(this, composition);
    }
    if (impl_->renderTickDriver_) {
      impl_->renderTickDriver_->setInterval(std::chrono::milliseconds(
          compositionPreviewIntervalMs(impl_->previewPipeline_.composition())));
    }
    markRenderDirty();
    return;
  }

  for (auto &connection : impl_->layerChangedConnections_) {
    disconnect(connection);
  }
  impl_->layerChangedConnections_.clear();
  impl_->compositionChangedSubscription_.disconnect();
  impl_->surfaceCache_.clear();
  if (impl_->gpuTextureCacheManager_) {
    impl_->gpuTextureCacheManager_->clear();
  }
  impl_->invalidateBaseComposite();

  impl_->previewPipeline_.setComposition(composition);
  impl_->bindCompositionChanged(this, composition);

  if (auto *playback = ArtifactPlaybackService::instance()) {
    if (playback->currentComposition() != composition) {
      playback->setCurrentComposition(composition);
    }
  }

  if (composition && impl_->renderer_) {
    impl_->applyCompositionState(composition);
    impl_->renderer_->fillToViewport();

    impl_->syncSelectedLayerOverlayState(composition);
    if (impl_->renderTickDriver_) {
      impl_->renderTickDriver_->setInterval(
          std::chrono::milliseconds(compositionPreviewIntervalMs(composition)));
    }
    // コンポジションがセットされた瞬間に1フレーム描画
    markRenderDirty();
  } else if (!composition) {
    impl_->syncSelectedLayerOverlayState(composition);
    if (impl_->renderTickDriver_) {
      impl_->renderTickDriver_->setInterval(
          std::chrono::milliseconds(compositionPreviewIntervalMs(nullptr)));
    }
    markRenderDirty();
  }
}

ArtifactCompositionPtr CompositionRenderController::composition() const {
  return impl_->previewPipeline_.composition();
}

LayerID CompositionRenderController::selectedLayerId() const {
  return impl_->selectedLayerId_;
}

void CompositionRenderController::setCompareMode(CompositionCompareMode mode) {
  impl_->compareMode_ = mode;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

CompositionCompareMode CompositionRenderController::compareMode() const {
  return impl_->compareMode_;
}

void CompositionRenderController::setReferencePinned(bool pinned) {
  impl_->referencePinned_ = pinned;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isReferencePinned() const {
  return impl_->referencePinned_;
}

void CompositionRenderController::setReferenceFrame(int frame) {
  impl_->referenceFrame_ = frame;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

int CompositionRenderController::referenceFrame() const {
  return impl_->referenceFrame_;
}

void CompositionRenderController::setSelectedLayerId(const LayerID &id) {
  if (impl_->selectedLayerId_ == id) {
    return;
  }
  impl_->clearPendingMaskCreation();
  impl_->selectedLayerId_ = id;
  impl_->invalidateOverlayComposite();
  impl_->syncSelectedLayerOverlayState(impl_->previewPipeline_.composition());

  auto* sel = ArtifactLayerSelectionManager::instance();
  if (sel) {
    auto comp = impl_->previewPipeline_.composition();
    if (!id.isNil() && comp) {
      auto layer = comp->layerById(id);
      if (layer) {
        sel->selectLayer(layer);
      }
    } else {
      sel->clearSelection();
    }
  }

  markRenderDirty();
}

void CompositionRenderController::setClearColor(const FloatColor &color) {
  if (toQColor(impl_->viewportClearColor_) == toQColor(color)) {
    return;
  }
  impl_->viewportClearColor_ = color;
  if (impl_->renderer_) {
    impl_->renderer_->setClearColor(color);
  }
  impl_->invalidateBaseComposite();
  markRenderDirty();
}

FloatColor CompositionRenderController::clearColor() const {
  return impl_->viewportClearColor_;
}

void CompositionRenderController::setShowGrid(bool show) {
  if (impl_->showGrid_ == show) {
    return;
  }
  impl_->showGrid_ = show;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}
bool CompositionRenderController::isShowGrid() const {
  return impl_->showGrid_;
}
void CompositionRenderController::setLineDebugKindVisible(LineDebugKind kind,
                                                          bool visible) {
  if (!impl_) {
    return;
  }
  const size_t index = lineDebugKindIndex(kind);
  if (index >= impl_->lineDebugVisibility_.size()) {
    return;
  }
  if (impl_->lineDebugVisibility_[index] == visible) {
    return;
  }
  impl_->lineDebugVisibility_[index] = visible;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}
bool CompositionRenderController::isLineDebugKindVisible(
    LineDebugKind kind) const {
  if (!impl_) {
    return false;
  }
  return lineDebugKindVisible(impl_->lineDebugVisibility_, kind);
}
void CompositionRenderController::setShowCheckerboard(bool show) {
  const auto nextMode = show ? CompositionBackgroundMode::Checkerboard
                             : CompositionBackgroundMode::Solid;
  if (impl_->compositionBackgroundMode_ == nextMode) {
    return;
  }
  impl_->compositionBackgroundMode_ = nextMode;
  impl_->invalidateBaseComposite();
  markRenderDirty();
}
bool CompositionRenderController::isShowCheckerboard() const {
  return impl_->compositionBackgroundMode_ ==
         CompositionBackgroundMode::Checkerboard;
}
void CompositionRenderController::setCheckerboardSize(float size) {
  const float clamped = std::clamp(size, 2.0f, 128.0f);
  if (std::abs(impl_->checkerboardTileSize_ - clamped) <= 0.001f) {
    return;
  }
  impl_->checkerboardTileSize_ = clamped;
  impl_->invalidateBaseComposite();
  markRenderDirty();
}
float CompositionRenderController::checkerboardSize() const {
  return impl_->checkerboardTileSize_;
}
void CompositionRenderController::setGridSettings(
    const Artifact::Grid::GridSettings &settings) {
  impl_->gridSettings_ = settings;
  if (impl_->showGrid_) {
    markRenderDirty();
  }
}
Artifact::Grid::GridSettings CompositionRenderController::gridSettings() const {
  return impl_->gridSettings_;
}
void CompositionRenderController::setCompositionBackgroundMode(int mode) {
  const auto backgroundMode = static_cast<CompositionBackgroundMode>(mode);
  if (impl_->compositionBackgroundMode_ == backgroundMode) {
    return;
  }
  impl_->compositionBackgroundMode_ = backgroundMode;
  impl_->invalidateBaseComposite();
  markRenderDirty();
}
int CompositionRenderController::compositionBackgroundMode() const {
  return static_cast<int>(impl_->compositionBackgroundMode_);
}
void CompositionRenderController::setShowGuides(bool show) {
  if (impl_->showGuides_ == show) {
    return;
  }
  impl_->showGuides_ = show;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}
bool CompositionRenderController::isShowGuides() const {
  return impl_->showGuides_;
}
void CompositionRenderController::setShowSafeMargins(bool show) {
  if (impl_->showSafeMargins_ == show) {
    return;
  }
  impl_->showSafeMargins_ = show;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}
bool CompositionRenderController::isShowSafeMargins() const {
  return impl_->showSafeMargins_;
}

void CompositionRenderController::setShowAnchorCenterOverlay(bool show) {
  if (impl_->showAnchorCenterOverlay_ == show) {
    return;
  }
  impl_->showAnchorCenterOverlay_ = show;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isShowAnchorCenterOverlay() const {
  return impl_ ? impl_->showAnchorCenterOverlay_ : false;
}

void CompositionRenderController::setShowCameraFrustumOverlay(bool show) {
  if (impl_->showCameraFrustumOverlay_ == show) {
    return;
  }
  impl_->showCameraFrustumOverlay_ = show;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isShowCameraFrustumOverlay() const {
  return impl_ ? impl_->showCameraFrustumOverlay_ : false;
}

void CompositionRenderController::setShowMotionPathOverlay(bool show) {
  if (impl_->showMotionPathOverlay_ == show) {
    return;
  }
  impl_->showMotionPathOverlay_ = show;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isShowMotionPathOverlay() const {
  return impl_ ? impl_->showMotionPathOverlay_ : false;
}

void CompositionRenderController::setShowEffectHitboxOverlay(bool show) {
  if (impl_->showEffectHitboxOverlay_ == show) {
    return;
  }
  impl_->showEffectHitboxOverlay_ = show;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isShowEffectHitboxOverlay() const {
  return impl_ ? impl_->showEffectHitboxOverlay_ : false;
}

void CompositionRenderController::setShowDensityHeatmapOverlay(bool show) {
  if (impl_->showDensityHeatmapOverlay_ == show) {
    return;
  }
  impl_->showDensityHeatmapOverlay_ = show;
  markRenderDirty();
}

bool CompositionRenderController::isShowDensityHeatmapOverlay() const {
  return impl_ ? impl_->showDensityHeatmapOverlay_ : false;
}

bool CompositionRenderController::setSelectedLayerMotionPathKeyframeAtCurrentFrame() {
  if (!impl_) {
    return false;
  }

  const auto comp = impl_->previewPipeline_.composition();
  if (!comp || impl_->selectedLayerId_.isNil()) {
    return false;
  }

  const auto layer = comp->layerById(impl_->selectedLayerId_);
  if (!layer) {
    return false;
  }

  const FramePosition currentFrame = currentFrameForComposition(comp);
  const ArtifactCore::RationalTime time(currentFrame.framePosition(), 24);
  auto &t3d = layer->transform3D();

  MotionPathPositionSnapshot before;
  before.hasPositionKey = t3d.hasPositionKeyFrameAt(time);
  before.x = t3d.positionXAt(time);
  before.y = t3d.positionYAt(time);

  t3d.setPositionKeyFrameValueAt(time, before.x, before.y);

  MotionPathPositionSnapshot after;
  after.hasPositionKey = t3d.hasPositionKeyFrameAt(time);
  after.x = t3d.positionXAt(time);
  after.y = t3d.positionYAt(time);

  const bool changed = before.hasPositionKey != after.hasPositionKey ||
                       std::abs(before.x - after.x) > 0.0001f ||
                       std::abs(before.y - after.y) > 0.0001f;
  if (!changed) {
    return false;
  }

  if (auto *mgr = UndoManager::instance()) {
    mgr->push(std::make_unique<MotionPathUndoCommand>(
        layer, currentFrame.framePosition(), before, after));
  }
  layer->setDirty(LayerDirtyFlag::Transform);
  layer->changed();
  if (auto *compPtr = static_cast<ArtifactAbstractComposition *>(layer->composition())) {
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{compPtr->id().toString(), layer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }
  impl_->motionPathCache_.valid = false;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::removeSelectedLayerMotionPathKeyframeAtCurrentFrame() {
  if (!impl_) {
    return false;
  }

  const auto comp = impl_->previewPipeline_.composition();
  if (!comp || impl_->selectedLayerId_.isNil()) {
    return false;
  }

  const auto layer = comp->layerById(impl_->selectedLayerId_);
  if (!layer) {
    return false;
  }

  const FramePosition currentFrame = currentFrameForComposition(comp);
  const ArtifactCore::RationalTime time(currentFrame.framePosition(), 24);
  auto &t3d = layer->transform3D();
  if (!t3d.hasPositionKeyFrameAt(time)) {
    return false;
  }

  MotionPathPositionSnapshot before;
  before.hasPositionKey = true;
  before.x = t3d.positionXAt(time);
  before.y = t3d.positionYAt(time);

  t3d.removePositionKeyFrameAt(time);

  MotionPathPositionSnapshot after;
  after.hasPositionKey = false;
  after.x = t3d.positionXAt(time);
  after.y = t3d.positionYAt(time);

  if (auto *mgr = UndoManager::instance()) {
    mgr->push(std::make_unique<MotionPathUndoCommand>(
        layer, currentFrame.framePosition(), before, after));
  }
  layer->setDirty(LayerDirtyFlag::Transform);
  layer->changed();
  if (auto *compPtr = static_cast<ArtifactAbstractComposition *>(layer->composition())) {
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{compPtr->id().toString(), layer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }
  impl_->motionPathCache_.valid = false;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::setSelectedLayerMotionPathInterpolationAtCurrentFrame(
    int interpolationType) {
  if (!impl_) {
    return false;
  }

  const auto comp = impl_->previewPipeline_.composition();
  if (!comp || impl_->selectedLayerId_.isNil()) {
    return false;
  }

  const auto layer = comp->layerById(impl_->selectedLayerId_);
  if (!layer) {
    return false;
  }

  const FramePosition currentFrame = currentFrameForComposition(comp);
  const ArtifactCore::RationalTime time(currentFrame.framePosition(), 24);
  auto &t3d = layer->transform3D();
  if (!t3d.hasPositionKeyFrameAt(time)) {
    return false;
  }

  MotionPathInterpolationSnapshot before;
  before.hasPositionKey = true;
  before.xInterpolation =
      static_cast<int>(t3d.positionXKeyFrameInterpolationAt(time));
  before.yInterpolation =
      static_cast<int>(t3d.positionYKeyFrameInterpolationAt(time));

  const auto interp =
      static_cast<ArtifactCore::InterpolationType>(interpolationType);
  if (before.xInterpolation == interpolationType &&
      before.yInterpolation == interpolationType) {
    return false;
  }

  t3d.setPositionKeyFrameInterpolationAt(time, interp, interp);

  MotionPathInterpolationSnapshot after;
  after.hasPositionKey = true;
  after.xInterpolation =
      static_cast<int>(t3d.positionXKeyFrameInterpolationAt(time));
  after.yInterpolation =
      static_cast<int>(t3d.positionYKeyFrameInterpolationAt(time));

  if (auto *mgr = UndoManager::instance()) {
    mgr->push(std::make_unique<MotionPathInterpolationUndoCommand>(
        layer, currentFrame.framePosition(), before, after));
  }
  layer->setDirty(LayerDirtyFlag::Transform);
  layer->changed();
  if (auto *compPtr =
          static_cast<ArtifactAbstractComposition *>(layer->composition())) {
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{compPtr->id().toString(), layer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }
  impl_->motionPathCache_.valid = false;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

void CompositionRenderController::setDropGhostPreview(
    const QRectF &viewportRect, const QString &title, const QString &hint,
    const QString &label) {
  if (!impl_) {
    return;
  }
  if (impl_->dropGhostVisible_ && impl_->dropGhostRect_ == viewportRect &&
      impl_->dropGhostTitle_ == title && impl_->dropGhostHint_ == hint &&
      impl_->dropCandidateLabel_ == label) {
    return;
  }
  impl_->dropGhostVisible_ = true;
  impl_->dropGhostRect_ = viewportRect;
  impl_->dropGhostTitle_ = title;
  impl_->dropGhostHint_ = hint;
  impl_->dropCandidateLabel_ = label;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

void CompositionRenderController::clearDropGhostPreview() {
  if (!impl_ ||
      (!impl_->dropGhostVisible_ && impl_->dropGhostRect_.isNull() &&
       impl_->dropGhostTitle_.isEmpty() && impl_->dropGhostHint_.isEmpty() &&
       impl_->dropCandidateLabel_.isEmpty())) {
    return;
  }
  impl_->dropGhostVisible_ = false;
  impl_->dropGhostRect_ = QRectF();
  impl_->dropGhostTitle_.clear();
  impl_->dropGhostHint_.clear();
  impl_->dropCandidateLabel_.clear();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

void CompositionRenderController::setInfoOverlayText(const QString &title,
                                                     const QString &detail) {
  if (!impl_) {
    return;
  }
  const QString normalizedTitle = title.trimmed();
  const QString normalizedDetail = detail.trimmed();
  if (impl_->infoOverlayVisible_ &&
      impl_->infoOverlayTitle_ == normalizedTitle &&
      impl_->infoOverlayDetail_ == normalizedDetail) {
    return;
  }
  impl_->infoOverlayVisible_ = true;
  impl_->infoOverlayTitle_ = normalizedTitle;
  impl_->infoOverlayDetail_ = normalizedDetail;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

void CompositionRenderController::clearInfoOverlayText() {
  if (!impl_ ||
      (!impl_->infoOverlayVisible_ && impl_->infoOverlayTitle_.isEmpty() &&
       impl_->infoOverlayDetail_.isEmpty())) {
    return;
  }
  impl_->infoOverlayVisible_ = false;
  impl_->infoOverlayTitle_.clear();
  impl_->infoOverlayDetail_.clear();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

void CompositionRenderController::showCommandPaletteOverlay(
    const QString &query, const QStringList &items) {
  if (!impl_) {
    return;
  }
  impl_->commandPaletteVisible_ = true;
  impl_->commandPaletteQuery_ = query;
  impl_->commandPaletteItems_ = items;
  impl_->contextMenuVisible_ = false;
  impl_->contextMenuTitle_.clear();
  impl_->contextMenuSubtitle_.clear();
  impl_->contextMenuItems_.clear();
  impl_->contextMenuItemEnabled_.clear();
  impl_->contextMenuSelectedIndex_ = -1;
  impl_->pieMenuVisible_ = false;
  impl_->pieMenuModel_ = PieMenuModel{};
  impl_->pieMenuSelectedIndex_ = -1;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

void CompositionRenderController::showContextMenuOverlay(
    const QPointF &viewportPos, const QStringList &items,
    const QString &title, const QString &subtitle,
    const QVector<bool> &enabledStates) {
  if (!impl_) {
    return;
  }
  impl_->contextMenuVisible_ = true;
  impl_->contextMenuViewportPos_ = viewportPos;
  impl_->contextMenuTitle_ = title;
  impl_->contextMenuSubtitle_ = subtitle;
  impl_->contextMenuItems_ = items;
  impl_->contextMenuItemEnabled_ = enabledStates;
  if (impl_->contextMenuItemEnabled_.size() != impl_->contextMenuItems_.size()) {
    impl_->contextMenuItemEnabled_.resize(impl_->contextMenuItems_.size());
    impl_->contextMenuItemEnabled_.fill(true);
  }
  impl_->contextMenuSelectedIndex_ = impl_->viewportOverlayItemAt(viewportPos);
  impl_->commandPaletteVisible_ = false;
  impl_->commandPaletteItems_.clear();
  impl_->pieMenuVisible_ = false;
  impl_->pieMenuModel_ = PieMenuModel{};
  impl_->pieMenuSelectedIndex_ = -1;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

void CompositionRenderController::showPieMenuOverlay(
    const PieMenuModel &model, const QPointF &viewportPos) {
  if (!impl_) {
    return;
  }
  impl_->pieMenuVisible_ = true;
  impl_->pieMenuModel_ = model;
  impl_->pieMenuViewportPos_ = viewportPos;
  impl_->pieMenuMousePos_ = viewportPos;
  impl_->pieMenuSelectedIndex_ = impl_->pieMenuItemAt(viewportPos);
  impl_->commandPaletteVisible_ = false;
  impl_->commandPaletteItems_.clear();
  impl_->contextMenuVisible_ = false;
  impl_->contextMenuTitle_.clear();
  impl_->contextMenuSubtitle_.clear();
  impl_->contextMenuItems_.clear();
  impl_->contextMenuItemEnabled_.clear();
  impl_->contextMenuSelectedIndex_ = -1;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

void CompositionRenderController::hideViewportOverlay() {
  if (!impl_ ||
      (!impl_->commandPaletteVisible_ && !impl_->contextMenuVisible_ &&
       !impl_->pieMenuVisible_)) {
    return;
  }
  impl_->commandPaletteVisible_ = false;
  impl_->commandPaletteQuery_.clear();
  impl_->commandPaletteItems_.clear();
  impl_->contextMenuVisible_ = false;
  impl_->contextMenuTitle_.clear();
  impl_->contextMenuSubtitle_.clear();
  impl_->contextMenuItems_.clear();
  impl_->contextMenuItemEnabled_.clear();
  impl_->contextMenuSelectedIndex_ = -1;
  impl_->pieMenuVisible_ = false;
  impl_->pieMenuModel_ = PieMenuModel{};
  impl_->pieMenuSelectedIndex_ = -1;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isViewportOverlayVisible() const {
  return impl_ && (impl_->commandPaletteVisible_ || impl_->contextMenuVisible_ ||
                   impl_->pieMenuVisible_);
}

bool CompositionRenderController::isContextMenuOverlayVisible() const {
  return impl_ && impl_->contextMenuVisible_;
}

int CompositionRenderController::viewportOverlayItemAt(
    const QPointF &viewportPos) const {
  return impl_ ? impl_->viewportOverlayItemAt(viewportPos) : -1;
}

QString CompositionRenderController::confirmPieMenuOverlaySelection() {
  if (!impl_ || !impl_->pieMenuVisible_) {
    return QString();
  }
  const int index = impl_->pieMenuSelectedIndex_;
  QString commandId;
  if (index >= 0 &&
      index < static_cast<int>(impl_->pieMenuModel_.items.size())) {
    auto &item = impl_->pieMenuModel_.items[static_cast<size_t>(index)];
    if (item.enabled) {
      commandId = item.commandId;
      if (item.action) {
        item.action();
      }
    }
  }
  hideViewportOverlay();
  return commandId;
}

void CompositionRenderController::updatePieMenuOverlayMousePos(
    const QPointF &viewportPos) {
  if (!impl_ || !impl_->pieMenuVisible_) {
    return;
  }
  impl_->pieMenuMousePos_ = viewportPos;
  const int selected = impl_->pieMenuItemAt(viewportPos);
  if (selected != impl_->pieMenuSelectedIndex_) {
    impl_->pieMenuSelectedIndex_ = selected;
    impl_->invalidateOverlayComposite();
    markRenderDirty();
  }
}

void CompositionRenderController::updateContextMenuOverlayMousePos(
    const QPointF &viewportPos) {
  if (!impl_ || !impl_->contextMenuVisible_) {
    return;
  }
  const int selected = impl_->viewportOverlayItemAt(viewportPos);
  if (selected != impl_->contextMenuSelectedIndex_) {
    impl_->contextMenuSelectedIndex_ = selected;
    impl_->invalidateOverlayComposite();
    markRenderDirty();
  }
}

void CompositionRenderController::cancelPieMenuOverlay() {
  if (!impl_ || !impl_->pieMenuVisible_) {
    return;
  }
  hideViewportOverlay();
}

bool CompositionRenderController::isPieMenuOverlayVisible() const {
  return impl_ && impl_->pieMenuVisible_;
}

void CompositionRenderController::setGpuBlendEnabled(bool enabled) {
  if (impl_->gpuBlendEnabled_ == enabled) {
    return;
  }
  impl_->gpuBlendEnabled_ = enabled;
  if (auto *playback = ArtifactPlaybackService::instance()) {
    playback->invalidateRamPreviewCache(
        QStringLiteral("render-policy-changed"));
  }
  impl_->invalidateBaseComposite();
  qWarning() << "[CompositionView] GPU blend user toggle changed"
             << "enabled=" << impl_->gpuBlendEnabled_ << "envDisable="
             << qEnvironmentVariableIsSet(
                    "ARTIFACT_COMPOSITION_DISABLE_GPU_BLEND");
  markRenderDirty();
}

bool CompositionRenderController::isGpuBlendEnabled() const {
  return impl_->gpuBlendEnabled_;
}

void CompositionRenderController::resetView() {
  if (impl_->renderer_) {
    impl_->renderer_->resetView();
    impl_->invalidateBaseComposite();
    markRenderDirty();
  }
}

void CompositionRenderController::zoomInAt(const QPointF &viewportPos) {
  if (impl_->renderer_) {
    zoomAtFactor(viewportPos, 1.1f);
  }
}

void CompositionRenderController::zoomOutAt(const QPointF &viewportPos) {
  if (impl_->renderer_) {
    zoomAtFactor(viewportPos, 1.0f / 1.1f);
  }
}

void CompositionRenderController::zoomAtFactor(const QPointF &viewportPos,
                                               float factor) {
  if (!impl_->renderer_) {
    return;
  }
  notifyViewportInteractionActivity();
  const float currentZoom = impl_->renderer_->getZoom();
  const float newZoom = std::clamp(currentZoom * factor, 0.05f, 64.0f);
  impl_->renderer_->zoomAroundViewportPoint(
      {(float)viewportPos.x() * impl_->devicePixelRatio_,
       (float)viewportPos.y() * impl_->devicePixelRatio_},
      newZoom);
  impl_->invalidateBaseComposite();
  markRenderDirty();
}

void CompositionRenderController::zoomFit() {
  if (impl_->renderer_) {
    impl_->renderer_->fitToViewport(0.0f);
    impl_->invalidateBaseComposite();
    float panX, panY;
    impl_->renderer_->getPan(panX, panY);
    const float zoom = impl_->renderer_->getZoom();
    qDebug() << "[CompositionRenderController] zoomFit done:"
             << "zoom=" << zoom << "pan=(" << panX << "," << panY << ")"
             << "hostSize=" << impl_->hostWidth_ << "x" << impl_->hostHeight_;
    markRenderDirty();
  }
}

void CompositionRenderController::zoomFill() {
  if (impl_->renderer_) {
    impl_->renderer_->fillToViewport();
    impl_->invalidateBaseComposite();
    float panX, panY;
    impl_->renderer_->getPan(panX, panY);
    const float zoom = impl_->renderer_->getZoom();
    qDebug() << "[CompositionRenderController] zoomFill done:"
             << "zoom=" << zoom << "pan=(" << panX << "," << panY << ")"
             << "hostSize=" << impl_->hostWidth_ << "x" << impl_->hostHeight_;
    markRenderDirty();
  }
}

void CompositionRenderController::zoom100() {
  if (impl_->renderer_) {
    impl_->renderer_->setZoom(1.0f);
    // Center the canvas in the viewport at 100% zoom.
    // hostWidth_/hostHeight_ are physical pixels, while lastCanvasWidth_/Height_
    // are composition pixels. Convert the canvas size into the same physical
    // pixel space before calculating the pan offset.
    const float dpr = impl_->devicePixelRatio_ > 0.0f ? impl_->devicePixelRatio_ : 1.0f;
    const float canvasW = impl_->lastCanvasWidth_ * dpr;
    const float canvasH = impl_->lastCanvasHeight_ * dpr;
    const float panX = (impl_->hostWidth_ - canvasW) * 0.5f;
    const float panY = (impl_->hostHeight_ - canvasH) * 0.5f;
    impl_->renderer_->setPan(panX, panY);
    impl_->invalidateBaseComposite();
    markRenderDirty();
  }
}

ArtifactIRenderer *CompositionRenderController::renderer() const {
  return impl_->renderer_.get();
}

QImage CompositionRenderController::captureCurrentFrameImage() const {
  if (impl_->renderer_ && impl_->lastPresentedReadbackSRV_) {
    const QImage frame =
        impl_->renderer_->readbackTextureViewToImage(
            impl_->lastPresentedReadbackSRV_);
    if (!frame.isNull()) {
      return frame;
    }
  }

  if (auto *host = impl_->hostWidget_.data()) {
    const QImage grabbed = host->grab().toImage();
    if (!grabbed.isNull()) {
      return grabbed;
    }
  }

  if (impl_->renderer_) {
    return impl_->renderer_->readbackToImage();
  }

  return QImage();
}

ArtifactCore::FrameDebugSnapshot
CompositionRenderController::frameDebugSnapshot() const {
  ArtifactCore::TraceScopeRecord traceScope;
  traceScope.name = QStringLiteral("CompositionRenderController::frameDebugSnapshot");
  traceScope.domain = ArtifactCore::TraceDomain::Render;
  QElapsedTimer traceTimer;
  traceTimer.start();
  const auto finalizeTraceScope = [&]() {
    traceScope.endNs = traceTimer.nsecsElapsed();
    if (traceScope.endNs <= traceScope.startNs) {
      traceScope.endNs = traceScope.startNs + 1;
    }
    ArtifactCore::TraceRecorder::instance().recordScope(traceScope);
  };

  if (impl_->renderer_) {
    const int slotIndex = std::clamp(
        impl_->activePreviewRenderPipelineSlot_, 0,
        Impl::kPreviewRenderPipelineSlotCount - 1);
    auto &diagnosticPipeline =
        impl_->previewRenderPipelineSlots_[slotIndex].pipeline;
    if (diagnosticPipeline.ready()) {
      impl_->lastLayerRtPreview_ =
          impl_->renderer_->readbackTextureViewToImage(
              diagnosticPipeline.layerRTV());
      impl_->lastAccumRtPreview_ =
          impl_->renderer_->readbackTextureViewToImage(
              diagnosticPipeline.accumRTV());
      impl_->lastLayerRtPixelStats_ =
          previewPixelStats(impl_->lastLayerRtPreview_);
      impl_->lastAccumRtPixelStats_ =
          previewPixelStats(impl_->lastAccumRtPreview_);
    }
  }

  ArtifactCore::FrameDebugSnapshot snapshot;
  const auto comp = impl_->previewPipeline_.composition();
  const auto selectedLayerId = impl_->selectedLayerId_;
  auto *playback = ArtifactPlaybackService::instance();
  auto *queue = ArtifactRenderQueueService::instance();

  if (playback) {
    const auto playbackComp = playback->currentComposition();
    if (!playbackComp || !comp || playbackComp->id() == comp->id()) {
      snapshot.frame = playback->currentFrame();
      snapshot.playbackState = playbackStateToString(playback->state());
    } else {
      snapshot.frame = comp ? comp->framePosition() : FramePosition(0);
      snapshot.playbackState = QStringLiteral("stopped");
    }
  } else {
    snapshot.frame = comp ? comp->framePosition() : FramePosition(0);
    snapshot.playbackState = QStringLiteral("stopped");
  }
  snapshot.renderLastFrameMs = lastFrameTimeMs();
  snapshot.renderAverageFrameMs = averageFrameTimeMs();
  if (impl_->renderer_) {
    snapshot.renderGpuFrameMs = impl_->renderer_->lastFrameGpuTimeMs();
    snapshot.renderCost = impl_->renderer_->frameCostStats();
  }

  snapshot.timestampMs =
      static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count());

  if (comp) {
    snapshot.compositionName = comp->settings().compositionName().toQString();
  } else {
    snapshot.compositionName = QStringLiteral("<none>");
  }

  snapshot.selectedLayerName = QStringLiteral("<none>");
  const auto selectedLayer = (comp && !selectedLayerId.isNil())
                                 ? comp->layerById(selectedLayerId)
                                 : ArtifactAbstractLayerPtr{};
  if (comp) {
    const auto &layers = comp->allLayerRef();
    snapshot.totalLayerCount = static_cast<int>(layers.size());
    for (const auto &layer : layers) {
      if (!layer) {
        continue;
      }
      if (dynamic_cast<ArtifactTextLayer *>(layer.get())) {
        ++snapshot.textLayerCount;
      }
      if (isLayerEffectivelyVisible(layer) && layer->isActiveAt(snapshot.frame)) {
        ++snapshot.visibleLayerCount;
      }
    }
  }
  if (comp && !selectedLayerId.isNil()) {
    if (auto layer = selectedLayer) {
      snapshot.selectedLayerName = layer->layerName();
      snapshot.selectedLayerMaskCount = layer->maskCount();
      snapshot.selectedLayerEffectCount = layer->effectCount();
      snapshot.selectedLayerMatteCount = static_cast<int>(layer->matteReferences().size());
    }
  }

  snapshot.renderBackend = QStringLiteral("unknown");
  if (queue) {
    snapshot.renderBackend = renderBackendToString(queue->renderBackend());
    const int jobCount = queue->jobCount();
    for (int i = 0; i < jobCount; ++i) {
      const QString status = queue->jobStatusAt(i);
      if (status.contains(QStringLiteral("fail"), Qt::CaseInsensitive) ||
          status.contains(QStringLiteral("error"), Qt::CaseInsensitive)) {
        snapshot.failed = true;
        snapshot.failureReason = queue->jobErrorMessageAt(i);
        break;
      }
    }
  }

  snapshot.compareMode = ArtifactCore::FrameDebugCompareMode::Disabled;
  snapshot.compareTargetId = QString();

  if (playback) {
    ArtifactCore::FrameDebugResourceRecord playbackResource;
    playbackResource.label = QStringLiteral("Playback");
    playbackResource.type = QStringLiteral("timeline");
    playbackResource.relation = QStringLiteral("service");
    playbackResource.cacheHit = playback->droppedFrameCount() == 0;
    playbackResource.stale = false;
    playbackResource.note =
        QStringLiteral("state=%1 droppedFrames=%2")
            .arg(snapshot.playbackState)
            .arg(playback->droppedFrameCount());
    snapshot.resources.push_back(playbackResource);
  }

  if (!impl_->lastRenderPathSummary_.isEmpty()) {
    ArtifactCore::FrameDebugResourceRecord renderPathResource;
    renderPathResource.label = QStringLiteral("Render Path");
    renderPathResource.type = QStringLiteral("composition");
    renderPathResource.relation = QStringLiteral("path");
    renderPathResource.cacheHit = impl_->blendPipelineReady_;
    renderPathResource.stale = false;
    renderPathResource.note = impl_->lastRenderPathSummary_;
    snapshot.resources.push_back(renderPathResource);
  }

  if (!impl_->lastCompositionVisibilitySummary_.isEmpty()) {
    ArtifactCore::FrameDebugResourceRecord visibilityResource;
    visibilityResource.label = QStringLiteral("Composition Visibility");
    visibilityResource.type = QStringLiteral("composition");
    visibilityResource.relation = QStringLiteral("visibility");
    visibilityResource.cacheHit =
        impl_->lastCompositionVisibilitySummary_.contains(
            QStringLiteral("state=drawn"));
    visibilityResource.stale =
        impl_->lastCompositionVisibilitySummary_.contains(
            QStringLiteral("frameOutOfRange=1"));
    visibilityResource.note = impl_->lastCompositionVisibilitySummary_;
    snapshot.resources.push_back(visibilityResource);
  }

  if (!impl_->lastBlendMaskSummary_.isEmpty()) {
    ArtifactCore::FrameDebugResourceRecord blendMaskResource;
    blendMaskResource.label = QStringLiteral("Blend / Mask Contract");
    blendMaskResource.type = QStringLiteral("blendMask");
    blendMaskResource.relation = QStringLiteral("contract");
    blendMaskResource.cacheHit =
        impl_->lastBlendMaskSummary_.contains(QStringLiteral("failed=0")) &&
        impl_->lastBlendMaskSummary_.contains(QStringLiteral("directFallback=0"));
    blendMaskResource.stale =
        impl_->lastBlendMaskSummary_.contains(QStringLiteral("maskContract=pending"));
    blendMaskResource.texture.valid = true;
    blendMaskResource.texture.name = QStringLiteral("accum/temp/layer");
    blendMaskResource.texture.format =
        QStringLiteral("accum=temp=RGBA32F layer=RGBA8_sRGB");
    blendMaskResource.texture.width =
        std::max(1, static_cast<int>(std::lround(impl_->lastCanvasWidth_)));
    blendMaskResource.texture.height =
        std::max(1, static_cast<int>(std::lround(impl_->lastCanvasHeight_)));
    blendMaskResource.texture.mipLevel = 0;
    blendMaskResource.texture.mipLevels = 1;
    blendMaskResource.texture.sliceIndex = 0;
    blendMaskResource.texture.arrayLayers = 1;
    blendMaskResource.texture.sampleCount = 1;
    blendMaskResource.texture.srgb = false;
    blendMaskResource.note = impl_->lastBlendMaskSummary_;
    snapshot.resources.push_back(blendMaskResource);
  }

  if (!impl_->lastVideoDebug_.isEmpty()) {
    ArtifactCore::FrameDebugResourceRecord videoResource;
    videoResource.label = QStringLiteral("Video Decode");
    videoResource.type = QStringLiteral("video");
    videoResource.relation = QStringLiteral("decode");
    videoResource.cacheHit =
        impl_->lastVideoDebug_.contains(QStringLiteral("source=ram-cache")) ||
        impl_->lastVideoDebug_.contains(QStringLiteral("frameReady=1"));
    videoResource.stale =
        impl_->lastVideoDebug_.contains(QStringLiteral("repeatLastGood")) ||
        impl_->lastVideoDebug_.contains(QStringLiteral("decoding=true")) ||
        impl_->lastVideoDebug_.contains(QStringLiteral("stage=requested")) ||
        impl_->lastVideoDebug_.contains(QStringLiteral("stage=decoding"));
    videoResource.note = impl_->lastVideoDebug_;
    snapshot.resources.push_back(videoResource);
  }

  if (impl_->renderer_) {
    const QString particleDebug = impl_->renderer_->particleDebugState();
    if (!particleDebug.isEmpty() && particleDebug != QStringLiteral("<none>")) {
      ArtifactCore::FrameDebugResourceRecord particleResource;
      particleResource.label = QStringLiteral("Particle Draw");
      particleResource.type = QStringLiteral("particle");
      particleResource.relation = QStringLiteral("draw");
      particleResource.cacheHit = !particleDebug.contains(QStringLiteral("skipped="));
      particleResource.stale = particleDebug.contains(QStringLiteral("skipped="));
      particleResource.note = particleDebug;
      snapshot.resources.push_back(particleResource);
    }

    const QString glyphAtlasDebug = impl_->renderer_->glyphAtlasDebugState();
    if (!glyphAtlasDebug.isEmpty() && glyphAtlasDebug != QStringLiteral("<none>")) {
      ArtifactCore::FrameDebugResourceRecord glyphAtlasResource;
      glyphAtlasResource.label = QStringLiteral("Glyph Atlas");
      glyphAtlasResource.type = QStringLiteral("glyphAtlas");
      glyphAtlasResource.relation = QStringLiteral("atlas");
      glyphAtlasResource.cacheHit = !glyphAtlasDebug.contains(QStringLiteral("dirty=true"));
      glyphAtlasResource.stale = glyphAtlasDebug.contains(QStringLiteral("dirty=true"));
      glyphAtlasResource.note = glyphAtlasDebug;
      snapshot.resources.push_back(glyphAtlasResource);
    }
  }

  if (impl_->gpuTextureCacheManager_) {
    const auto stats = impl_->gpuTextureCacheManager_->stats();
    ArtifactCore::FrameDebugResourceRecord textureCacheResource;
    textureCacheResource.label = QStringLiteral("GPU Texture Cache");
    textureCacheResource.type = QStringLiteral("cache");
    textureCacheResource.relation = QStringLiteral("upload");
    textureCacheResource.cacheHit = stats.hitCount >= stats.missCount;
    textureCacheResource.stale = false;
    textureCacheResource.note =
        QStringLiteral("entries=%1 bytes=%2 hits=%3 misses=%4")
            .arg(stats.entryCount)
            .arg(static_cast<qulonglong>(stats.memoryBytes))
            .arg(static_cast<qulonglong>(stats.hitCount))
            .arg(static_cast<qulonglong>(stats.missCount));
    snapshot.resources.push_back(textureCacheResource);
  }

  if (comp) {
    auto makePass = [&](const QString &name, ArtifactCore::FrameDebugPassKind kind,
                        qint64 durationMs, const QString &note = QString()) {
      ArtifactCore::FrameDebugPassRecord pass;
      pass.name = name;
      pass.kind = kind;
      pass.status =
          snapshot.failed ? ArtifactCore::FrameDebugPassStatus::Failed
                          : ArtifactCore::FrameDebugPassStatus::Success;
      pass.durationUs = std::max<qint64>(0, durationMs) * 1000;
      pass.note = note;
      return pass;
    };
    auto addBinding = [](ArtifactCore::FrameDebugPassRecord &pass,
                         const QString &key, const QString &value,
                         const QString &stage = QString(),
                         const QString &note = QString()) {
      ArtifactCore::FrameDebugBindingRecord binding;
      binding.key = key;
      binding.value = value;
      binding.stage = stage;
      binding.note = note;
      pass.debugBindings.push_back(std::move(binding));
    };
    const QString backendName =
        snapshot.renderBackend.isEmpty() ? QStringLiteral("unknown")
                                         : snapshot.renderBackend;

    ArtifactCore::FrameDebugPassRecord setupPass =
        makePass(QStringLiteral("setup"), ArtifactCore::FrameDebugPassKind::Clear,
                 impl_->lastSetupMs_);
    ArtifactCore::FrameDebugPassRecord basePass =
        makePass(QStringLiteral("base"), ArtifactCore::FrameDebugPassKind::Clear,
                 impl_->lastBasePassMs_);
    ArtifactCore::FrameDebugPassRecord layerPass =
        makePass(QStringLiteral("layer"), ArtifactCore::FrameDebugPassKind::Draw,
                 impl_->lastLayerPassMs_, impl_->lastRenderPathSummary_);
    ArtifactCore::FrameDebugPassRecord overlayPass =
        makePass(QStringLiteral("overlay"),
                 ArtifactCore::FrameDebugPassKind::Composite,
                 impl_->lastOverlayMs_);
    ArtifactCore::FrameDebugPassRecord flushPass =
        makePass(QStringLiteral("flush"),
                 ArtifactCore::FrameDebugPassKind::Resolve,
                 impl_->lastFlushMs_);
    ArtifactCore::FrameDebugPassRecord presentPass =
        makePass(QStringLiteral("present"),
                 ArtifactCore::FrameDebugPassKind::Readback,
                 impl_->lastPresentMs_);
    setupPass.name = QStringLiteral("Viewport Setup");
    setupPass.backend = backendName;
    setupPass.shaderName = QStringLiteral("fixed-function/setup");
    setupPass.previewResourceLabel = QStringLiteral("viewport");
    addBinding(setupPass, QStringLiteral("hostWidth"),
               QString::number(std::max(1, static_cast<int>(std::lround(impl_->hostWidth_)))));
    addBinding(setupPass, QStringLiteral("hostHeight"),
               QString::number(std::max(1, static_cast<int>(std::lround(impl_->hostHeight_)))));
    addBinding(setupPass, QStringLiteral("canvasWidth"),
               QString::number(std::max(1, static_cast<int>(std::lround(impl_->lastCanvasWidth_)))));
    addBinding(setupPass, QStringLiteral("canvasHeight"),
               QString::number(std::max(1, static_cast<int>(std::lround(impl_->lastCanvasHeight_)))));

    basePass.name = QStringLiteral("Composition Background");
    basePass.backend = backendName;
    basePass.shaderName = QStringLiteral("checkerboard/grid/clear");
    basePass.previewResourceLabel = QStringLiteral("viewport");
    addBinding(basePass, QStringLiteral("viewportSize"),
               QStringLiteral("%1 x %2")
                   .arg(std::max(1, static_cast<int>(std::lround(impl_->hostWidth_))))
                   .arg(std::max(1, static_cast<int>(std::lround(impl_->hostHeight_)))),
               QStringLiteral("pixel"));
    addBinding(basePass, QStringLiteral("backgroundMode"),
               QStringLiteral("composition-editor"));
    addBinding(basePass, QStringLiteral("renderPathSummary"),
               impl_->lastRenderPathSummary_.isEmpty() ? QStringLiteral("<none>")
                                                       : impl_->lastRenderPathSummary_);

    layerPass.name = QStringLiteral("Layer Content");
    layerPass.backend = backendName;
    layerPass.shaderName = QStringLiteral("legacy-composition-draw");
    layerPass.previewResourceLabel =
        snapshot.selectedLayerName.isEmpty() ||
                snapshot.selectedLayerName == QStringLiteral("<none>")
            ? QStringLiteral("viewport")
            : snapshot.selectedLayerName;
    addBinding(layerPass, QStringLiteral("selectedLayer"),
               snapshot.selectedLayerName.isEmpty() ? QStringLiteral("<none>")
                                                    : snapshot.selectedLayerName);
    addBinding(layerPass, QStringLiteral("visibleLayerCount"),
               QString::number(snapshot.visibleLayerCount));
    addBinding(layerPass, QStringLiteral("selectedLayerEffects"),
               QString::number(snapshot.selectedLayerEffectCount));
    addBinding(layerPass, QStringLiteral("selectedLayerMasks"),
               QString::number(snapshot.selectedLayerMaskCount));

    overlayPass.name = QStringLiteral("Editor Overlay / Gizmo");
    overlayPass.backend = backendName;
    overlayPass.shaderName = QStringLiteral("overlay-helpers");
    overlayPass.previewResourceLabel = QStringLiteral("viewport");
    addBinding(overlayPass, QStringLiteral("selection"),
               snapshot.selectedLayerName.isEmpty() ? QStringLiteral("<none>")
                                                    : snapshot.selectedLayerName);
    addBinding(overlayPass, QStringLiteral("overlayStage"),
               QStringLiteral("gizmo/hud/helpers"));

    flushPass.name = QStringLiteral("Flush / Resolve");
    flushPass.backend = backendName;
    flushPass.shaderName = QStringLiteral("resolve");
    flushPass.previewResourceLabel = QStringLiteral("viewport");

    presentPass.name = QStringLiteral("Present / Readback");
    presentPass.backend = backendName;
    presentPass.shaderName = QStringLiteral("readback");
    presentPass.previewResourceLabel = QStringLiteral("viewport");
    addBinding(presentPass, QStringLiteral("readbackTarget"),
               QStringLiteral("viewport"));

    ArtifactCore::FrameDebugAttachmentRecord outputAttachment;
    outputAttachment.name = QStringLiteral("viewport");
    outputAttachment.role = QStringLiteral("output");
    outputAttachment.texture.valid = true;
    outputAttachment.texture.name = QStringLiteral("viewport");
    outputAttachment.texture.format = snapshot.renderBackend.isEmpty() ? QStringLiteral("viewport") : snapshot.renderBackend;
    outputAttachment.texture.width = std::max(1, static_cast<int>(std::lround(impl_->hostWidth_)));
    outputAttachment.texture.height = std::max(1, static_cast<int>(std::lround(impl_->hostHeight_)));
    outputAttachment.texture.mipLevel = 0;
    outputAttachment.texture.mipLevels = std::max(1, 1 + static_cast<int>(std::floor(std::log2(
        static_cast<double>(std::max(outputAttachment.texture.width, outputAttachment.texture.height))))));
    outputAttachment.texture.sliceIndex = 0;
    outputAttachment.texture.arrayLayers = 1;
    outputAttachment.texture.sampleCount = 1;
    outputAttachment.texture.srgb = false;
    outputAttachment.readOnly = true;
    presentPass.outputs.push_back(outputAttachment);

    snapshot.attachments.push_back(outputAttachment);

    if (impl_->renderer_) {
      ArtifactCore::FrameDebugResourceRecord layerTargetResource;
      layerTargetResource.label = QStringLiteral("Layer RT");
      layerTargetResource.type = QStringLiteral("rt");
      layerTargetResource.relation = QStringLiteral("intermediate");
      layerTargetResource.cacheHit = true;
      layerTargetResource.texture.valid = true;
      layerTargetResource.texture.name = QStringLiteral("Layer RT");
      layerTargetResource.texture.format = QStringLiteral("render-target");
      layerTargetResource.texture.width = std::max(1, static_cast<int>(std::lround(impl_->lastCanvasWidth_)));
      layerTargetResource.texture.height = std::max(1, static_cast<int>(std::lround(impl_->lastCanvasHeight_)));
      layerTargetResource.texture.mipLevel = 0;
      layerTargetResource.texture.mipLevels = 1;
      layerTargetResource.texture.sliceIndex = 0;
      layerTargetResource.texture.arrayLayers = 1;
      layerTargetResource.texture.sampleCount = 1;
      layerTargetResource.texture.srgb = false;
      layerTargetResource.note = QStringLiteral("previewSource=layer-render-target");
      snapshot.resources.push_back(layerTargetResource);

      ArtifactCore::FrameDebugResourceRecord accumResource;
      accumResource.label = QStringLiteral("Accum RT");
      accumResource.type = QStringLiteral("rt");
      accumResource.relation = QStringLiteral("intermediate");
      accumResource.cacheHit = true;
      accumResource.texture.valid = true;
      accumResource.texture.name = QStringLiteral("Accum RT");
      accumResource.texture.format = QStringLiteral("render-target");
      accumResource.texture.width = std::max(1, static_cast<int>(std::lround(impl_->lastCanvasWidth_)));
      accumResource.texture.height = std::max(1, static_cast<int>(std::lround(impl_->lastCanvasHeight_)));
      accumResource.texture.mipLevel = 0;
      accumResource.texture.mipLevels = 1;
      accumResource.texture.sliceIndex = 0;
      accumResource.texture.arrayLayers = 1;
      accumResource.texture.sampleCount = 1;
      accumResource.texture.srgb = false;
      accumResource.note = QStringLiteral("previewSource=accum-render-target");
      snapshot.resources.push_back(accumResource);
    }

    if (impl_->renderer_) {
      QImage viewportAfter = captureCurrentFrameImage();
      if (comp) {
        applyCompositionFinalEffectsToImage(comp.get(), viewportAfter);
      }
      const QImage viewportAlpha =
          impl_->renderer_->readbackChannelToImage(ArtifactIRenderer::ChannelType::Alpha);
      addSnapshotPreview(snapshot, QStringLiteral("viewport"),
                         QStringLiteral("Viewport Final"),
                         QStringLiteral("before=unavailable after=final-present alpha=readback"),
                         QImage(), viewportAfter, viewportAlpha);
      addSnapshotPreview(snapshot, QStringLiteral("Particle Draw"),
                         QStringLiteral("Particle Draw"),
                         QStringLiteral("previewSource=final-viewport before=unavailable"),
                         QImage(), viewportAfter, viewportAlpha);
      addSnapshotPreview(snapshot, QStringLiteral("Blend / Mask Contract"),
                         QStringLiteral("Blend / Mask Contract"),
                         QStringLiteral("previewSource=final-viewport before=unavailable"),
                         QImage(), viewportAfter, viewportAlpha);
      addSnapshotPreview(snapshot, QStringLiteral("Layer RT"),
                         QStringLiteral("Layer RT"),
                         QStringLiteral("previewSource=layer-render-target before=unavailable"),
                         QImage(), impl_->lastLayerRtPreview_, viewportAlpha);
      addSnapshotPreview(snapshot, QStringLiteral("Accum RT"),
                         QStringLiteral("Accum RT"),
                         QStringLiteral("previewSource=accum-render-target before=unavailable"),
                         QImage(), impl_->lastAccumRtPreview_, viewportAlpha);
      if (!snapshot.selectedLayerName.isEmpty() &&
          snapshot.selectedLayerName != QStringLiteral("<none>")) {
        addSnapshotPreview(snapshot, snapshot.selectedLayerName,
                           snapshot.selectedLayerName,
                           QStringLiteral("previewSource=final-viewport before=unavailable"),
                           QImage(), viewportAfter, viewportAlpha);
      }
    }

    if (!snapshot.selectedLayerName.isEmpty() &&
        snapshot.selectedLayerName != QStringLiteral("<none>")) {
      const auto selectedLayer = comp ? comp->layerById(selectedLayerId)
                                      : ArtifactAbstractLayerPtr{};
      ArtifactCore::FrameDebugResourceRecord selectedResource;
      selectedResource.label = snapshot.selectedLayerName;
      selectedResource.type = QStringLiteral("layer");
      selectedResource.relation = QStringLiteral("selected");
      selectedResource.cacheHit = true;
      selectedResource.texture.valid = true;
      selectedResource.texture.name = snapshot.selectedLayerName;
      selectedResource.texture.format = QStringLiteral("layer-proxy");
      selectedResource.texture.width = std::max(1, static_cast<int>(std::lround(impl_->lastCanvasWidth_)));
      selectedResource.texture.height = std::max(1, static_cast<int>(std::lround(impl_->lastCanvasHeight_)));
      selectedResource.texture.mipLevel = 0;
      selectedResource.texture.mipLevels = std::max(1, 1 + static_cast<int>(std::floor(std::log2(
          static_cast<double>(std::max(selectedResource.texture.width, selectedResource.texture.height))))));
      selectedResource.texture.sliceIndex = 0;
      selectedResource.texture.arrayLayers = 1;
      selectedResource.texture.sampleCount = 1;
      selectedResource.texture.srgb = false;
      if (selectedLayer) {
        auto *layer = selectedLayer.get();
        if (auto *videoLayer = dynamic_cast<ArtifactVideoLayer *>(layer)) {
          selectedResource.type = QStringLiteral("video");
          selectedResource.note = videoLayer->decodeState();
        } else if (layer->isParticleLayer()) {
          selectedResource.type = QStringLiteral("particle");
          selectedResource.note = QStringLiteral("particle layer");
        } else if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(layer)) {
          selectedResource.type = QStringLiteral("text");
          selectedResource.note = textLayer->debugState();
        } else {
          selectedResource.note = impl_->lastVideoDebug_;
        }

        if (layer->maskCount() > 0) {
          QStringList maskNotes;
          maskNotes.reserve(layer->maskCount());
          int enabledMaskCount = 0;
          int totalPathCount = 0;
          for (int m = 0; m < layer->maskCount(); ++m) {
            const LayerMask mask = layer->mask(m);
            const int pathCount = mask.maskPathCount();
            totalPathCount += pathCount;
            if (mask.isEnabled()) {
              ++enabledMaskCount;
            }
            maskNotes.push_back(
                QStringLiteral("#%1 paths=%2 enabled=%3")
                    .arg(m + 1)
                    .arg(pathCount)
                    .arg(mask.isEnabled() ? QStringLiteral("on")
                                          : QStringLiteral("off")));
          }

          ArtifactCore::FrameDebugResourceRecord maskResource;
          maskResource.label = QStringLiteral("Mask Inspector");
          maskResource.type = QStringLiteral("mask");
          maskResource.relation = QStringLiteral("selected");
          maskResource.cacheHit = enabledMaskCount > 0;
          maskResource.stale = false;
          maskResource.note = QStringLiteral("maskCount=%1 enabled=%2 paths=%3 | %4")
                                 .arg(layer->maskCount())
                                 .arg(enabledMaskCount)
                                 .arg(totalPathCount)
                                 .arg(maskNotes.join(QStringLiteral("; ")));
          snapshot.resources.push_back(maskResource);
        }
      } else {
        selectedResource.note = impl_->lastVideoDebug_;
      }
      if (impl_->renderer_) {
        QImage viewportAfter = captureCurrentFrameImage();
        if (comp) {
          applyCompositionFinalEffectsToImage(comp.get(), viewportAfter);
        }
        const QImage viewportAlpha =
            impl_->renderer_->readbackChannelToImage(ArtifactIRenderer::ChannelType::Alpha);
        addSnapshotPreview(snapshot, selectedResource.label, selectedResource.label,
                           QStringLiteral("resource-preview=selected-layer final-readback"),
                           QImage(), viewportAfter, viewportAlpha);
      }
      snapshot.resources.push_back(selectedResource);
    }

    snapshot.passes.push_back(setupPass);
    snapshot.passes.push_back(basePass);
    snapshot.passes.push_back(layerPass);
    snapshot.passes.push_back(overlayPass);
    snapshot.passes.push_back(flushPass);
    snapshot.passes.push_back(presentPass);
    if (impl_->renderer_) {
      const auto rendererPasses = impl_->renderer_->frameDebugPasses();
      snapshot.passes.insert(snapshot.passes.end(), rendererPasses.begin(),
                             rendererPasses.end());
    }
  }

  const double visualRaw = snapshot.visibleLayerCount * 0.08 +
                           snapshot.selectedLayerMaskCount * 0.16 +
                           snapshot.selectedLayerEffectCount * 0.12 +
                           snapshot.selectedLayerMatteCount * 0.10;
  const double informationRaw = snapshot.resources.size() * 0.08 +
                                snapshot.attachments.size() * 0.12 +
                                snapshot.passes.size() * 0.10 +
                                snapshot.textLayerCount * 0.05;
  const double luminanceRaw = snapshot.textLayerCount * 0.06 +
                              (selectedLayer ? 0.10 : 0.0) +
                              (snapshot.renderBackend == QStringLiteral("gpu") ? 0.04 : 0.0);
  const double motionRaw = clamp01(snapshot.renderLastFrameMs / 24.0) * 0.45 +
                           clamp01(snapshot.renderGpuFrameMs / 24.0) * 0.25 +
                           clamp01(static_cast<double>(snapshot.renderCost.drawCalls) / 5000.0) * 0.30;

  snapshot.visualDensityScore = clamp01(visualRaw);
  snapshot.informationDensityScore = clamp01(informationRaw);
  snapshot.luminanceDensityScore = clamp01(luminanceRaw);
  snapshot.motionDensityScore = clamp01(motionRaw);

  const std::array<std::pair<const char *, double>, 4> densityScores{{
      {"visual", snapshot.visualDensityScore},
      {"information", snapshot.informationDensityScore},
      {"luminance", snapshot.luminanceDensityScore},
      {"motion", snapshot.motionDensityScore},
  }};
  const auto dominantIt = std::max_element(
      densityScores.begin(), densityScores.end(),
      [](const auto &a, const auto &b) { return a.second < b.second; });
  const QString dominantAxis = QString::fromLatin1(dominantIt->first);
  const double dominantScore = dominantIt->second;
  snapshot.densityLabel = densityLevelFromScore(dominantScore);
  snapshot.densityWarning =
      densityWarningForDominantAxis(dominantAxis, dominantScore);
  snapshot.densityNextAction = densityNextActionForAxis(dominantAxis);

  ArtifactCore::TraceRecorder::instance().recordFrameDebugSnapshot(snapshot);
  finalizeTraceScope();
  return snapshot;
}

double CompositionRenderController::lastFrameTimeMs() const {
  return impl_ ? impl_->lastFrameTimeMs_ : 0.0;
}

double CompositionRenderController::averageFrameTimeMs() const {
  return impl_ ? impl_->averageFrameTimeMs_ : 0.0;
}

void CompositionRenderController::focusSelectedLayer() {
  if (!impl_->renderer_) {
    return;
  }

  auto comp = impl_->previewPipeline_.composition();
  if (!comp || impl_->selectedLayerId_.isNil()) {
    zoomFit();
    return;
  }

  const auto layer = comp->layerById(impl_->selectedLayerId_);
  if (!layer) {
    zoomFit();
    return;
  }

  const QRectF bounds = layer->transformedBoundingBox();
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    zoomFit();
    return;
  }

  const float viewW = impl_->hostWidth_ > 0.0f ? impl_->hostWidth_ : 1.0f;
  const float viewH = impl_->hostHeight_ > 0.0f ? impl_->hostHeight_ : 1.0f;
  const float margin = 48.0f;
  const float availableW = std::max(1.0f, viewW - margin * 2.0f);
  const float availableH = std::max(1.0f, viewH - margin * 2.0f);
  const float zoomX = availableW / static_cast<float>(bounds.width());
  const float zoomY = availableH / static_cast<float>(bounds.height());
  const float zoom = std::clamp(std::min(zoomX, zoomY), 0.02f, 64.0f);
  const QPointF center = bounds.center();

  impl_->renderer_->setZoom(zoom);
  impl_->renderer_->setPan(viewW * 0.5f - static_cast<float>(center.x()) * zoom,
                           viewH * 0.5f -
                               static_cast<float>(center.y()) * zoom);
  impl_->invalidateBaseComposite();
  markRenderDirty();
}

bool CompositionRenderController::createFullLayerMaskForLayer(
    const ArtifactAbstractLayerPtr &layer) {
  return cyclePresetLayerMaskForLayer(layer, false);
}

bool CompositionRenderController::cyclePresetLayerMaskForLayer(
    const ArtifactAbstractLayerPtr &layer, bool reverse) {
  if (!impl_ || !layer) {
    return false;
  }
  return impl_->cyclePresetLayerMaskForLayer(layer, reverse);
}

LayerID CompositionRenderController::layerAtViewportPos(
    const QPointF &viewportPos) const {
  auto comp = impl_->previewPipeline_.composition();
  // viewportPos is in logical pixels; convert to physical for hit testing
  const QPointF physPos = viewportPos * impl_->devicePixelRatio_;
  const auto layer =
      hitTopmostLayerAtViewportPos(comp, impl_->renderer_.get(), physPos);
  return layer ? layer->id() : LayerID::Nil();
}

Ray CompositionRenderController::createPickingRay(
    const QPointF &viewportPos) const {
  if (!impl_->renderer_)
    return {};

  QMatrix4x4 view = impl_->renderer_->getViewMatrix();
  QMatrix4x4 proj = impl_->renderer_->getProjectionMatrix();
  QRect viewport(0, 0, (int)impl_->hostWidth_, (int)impl_->hostHeight_);

  QVector3D nearPos = QVector3D(viewportPos.x(), viewportPos.y(), 0.0f)
                          .unproject(view, proj, viewport);
  QVector3D farPos = QVector3D(viewportPos.x(), viewportPos.y(), 1.0f)
                         .unproject(view, proj, viewport);

  return {nearPos, (farPos - nearPos).normalized()};
}

void CompositionRenderController::handleMousePress(QMouseEvent *event) {
  if (!event || !impl_->renderer_)
    return;

  qCDebug(compositionViewLog)
      << "[MousePress] ENTER pos:" << event->position()
      << "button:" << event->button() << "modifiers:" << event->modifiers()
      << "devicePixelRatio:" << impl_->devicePixelRatio_;

  // event->position() is in logical pixels; convert to physical for rendering
  // pipeline
  const QPointF viewportPos = event->position() * impl_->devicePixelRatio_;
  auto toolManager = ArtifactApplicationManager::instance()
                         ? ArtifactApplicationManager::instance()->toolManager()
                         : nullptr;
  auto activeTool = toolManager ? toolManager->activeTool() : ToolType::Selection;

  auto comp = impl_->previewPipeline_.composition();
  auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
                           ? comp->layerById(impl_->selectedLayerId_)
                           : ArtifactAbstractLayerPtr{};
  auto syncPrimarySelectionLayer = [this, comp](const ArtifactAbstractLayerPtr &primaryLayer) {
    const LayerID primaryId = primaryLayer ? primaryLayer->id() : LayerID::Nil();
    if (impl_->selectedLayerId_ != primaryId) {
      impl_->clearPendingMaskCreation();
      impl_->selectedLayerId_ = primaryId;
      impl_->invalidateOverlayComposite();
    }
    impl_->syncSelectedLayerOverlayState(comp);
    markRenderDirty();
  };

  
  // MotionSketch tool
  if (event->button() == Qt::LeftButton && activeTool == ToolType::MotionSketch && selectedLayer && impl_->renderer_) {
    const auto cPos = impl_->renderer_->viewportToCanvas(
        {(float)viewportPos.x(), (float)viewportPos.y()});
    auto* app = ArtifactApplicationManager::instance();
    if (app && app->motionSketchTool()) {
      app->motionSketchTool()->beginSketch(QPointF(cPos.x, cPos.y), selectedLayer);
    }
    markRenderDirty();
    event->accept();
    return;
  }

  // Puppet tool
  if (event->button() == Qt::LeftButton && activeTool == ToolType::Puppet && impl_->renderer_) {
    const auto cPos = impl_->renderer_->viewportToCanvas(
        {(float)viewportPos.x(), (float)viewportPos.y()});
    auto* app = ArtifactApplicationManager::instance();
    if (app && app->puppetTool()) {
      const QPointF canvasPt(cPos.x, cPos.y);
      QString hitId = app->puppetTool()->hitTestPin(canvasPt);
      if (!hitId.isEmpty()) {
        app->puppetTool()->setSelectedPinId(hitId);
      } else if (selectedLayer) {
        app->puppetTool()->addPin(selectedLayer->id(), canvasPt);
        if (app && app->puppetTool()) {
          app->puppetTool()->deformLayer(selectedLayer->id(), impl_->renderer_.get());
        }
      }
    }
    markRenderDirty();
    event->accept();
    return;
  }
if (event->button() == Qt::LeftButton && activeTool == ToolType::Rectangle) {
    auto *selectionManager = ArtifactApplicationManager::instance()
                                 ? ArtifactApplicationManager::instance()
                                       ->layerSelectionManager()
                                 : nullptr;
    ArtifactAbstractLayerPtr effectiveSelectedLayer =
        selectionManager ? selectionManager->currentLayer()
                         : ArtifactAbstractLayerPtr{};
    if (!effectiveSelectedLayer && !selectionManager && selectedLayer) {
      effectiveSelectedLayer = selectedLayer;
    }

    const auto canvasPos = impl_->renderer_->viewportToCanvas(
        {(float)viewportPos.x(), (float)viewportPos.y()});
    impl_->clearPendingMaskCreation();
    if (effectiveSelectedLayer) {
      impl_->beginRectangleToolSession(RectangleToolMode::Mask,
                                       effectiveSelectedLayer,
                                       QPointF(canvasPos.x, canvasPos.y));
      impl_->beginMaskEditTransaction(effectiveSelectedLayer);
    } else {
      impl_->beginRectangleToolSession(RectangleToolMode::Shape,
                                       ArtifactAbstractLayerPtr{},
                                       QPointF(canvasPos.x, canvasPos.y));
    }

    markRenderDirty();
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton && activeTool == ToolType::Pen &&
      selectedLayer && comp && impl_->renderer_) {
    const auto cPos = impl_->renderer_->viewportToCanvas(
        {(float)viewportPos.x(), (float)viewportPos.y()});
    const QTransform globalTransform = selectedLayer->getGlobalTransform();
    bool invertible = false;
    const QTransform invTransform = globalTransform.inverted(&invertible);

    if (invertible) {
      impl_->beginMaskEditTransaction(selectedLayer);
      const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));
      impl_->penMaskPreviewCanvasPos_ = {cPos.x, cPos.y};
      impl_->penMaskPreviewValid_ = true;

      if (impl_->pendingMaskCreation_ &&
          impl_->pendingMaskLayerId_ != selectedLayer->id()) {
        impl_->clearPendingMaskCreation();
      }

      const float handleThreshold = 14.0f / impl_->renderer_->getZoom();
      int handleMaskIndex = -1;
      int handlePathIndex = -1;
      int handleVertexIndex = -1;
      MaskHandleType handleType = MaskHandleType::None;
      if (hitTestMaskHandle(selectedLayer, QPointF(cPos.x, cPos.y), handleThreshold,
                            handleMaskIndex, handlePathIndex, handleVertexIndex,
                            handleType)) {
        impl_->resetLayerMutationNotify();
        impl_->isDraggingVertex_ = false;
        impl_->isDraggingMaskHandle_ = true;
        impl_->draggingMaskIndex_ = handleMaskIndex;
        impl_->draggingPathIndex_ = handlePathIndex;
        impl_->draggingVertexIndex_ = handleVertexIndex;
        impl_->draggingMaskHandleType_ = static_cast<int>(handleType);
        impl_->hoveredMaskIndex_ = handleMaskIndex;
        impl_->hoveredPathIndex_ = handlePathIndex;
        impl_->hoveredVertexIndex_ = handleVertexIndex;
        impl_->hoveredMaskHandleType_ = static_cast<int>(handleType);
        event->accept();
        return;
      }

      // 1. Hit test existing vertices for dragging or closing path
      const float hitThreshold =
          12.0f / impl_->renderer_->getZoom(); // widened for direct mask edits
      for (int m = 0; m < selectedLayer->maskCount(); ++m) {
        LayerMask mask = selectedLayer->mask(m);
        for (int p = 0; p < mask.maskPathCount(); ++p) {
          MaskPath path = mask.maskPath(p);
          for (int v = 0; v < path.vertexCount(); ++v) {
            MaskVertex vertex = path.vertex(v);
            if (QVector2D(vertex.position - localPos).length() <
                hitThreshold) {
              if (v == 0 && !path.isClosed() && path.vertexCount() > 2) {
                path.setClosed(true);
                mask.setMaskPath(p, path);
                mask.addMaskPath(MaskPath());
                selectedLayer->setMask(m, mask);
                impl_->markMaskEditDirty();
                qDebug() << "[PenTool] Closed path" << p;
                ArtifactCore::globalEventBus().publish(LayerChangedEvent{
                    comp->id().toString(), selectedLayer->id().toString(),
                    LayerChangedEvent::ChangeType::Modified});
                impl_->isDraggingVertex_ = false;
                impl_->draggingMaskIndex_ = m;
                impl_->draggingPathIndex_ = p;
                impl_->draggingVertexIndex_ = -1;
                impl_->hoveredMaskIndex_ = m;
                impl_->hoveredPathIndex_ = p;
                impl_->hoveredVertexIndex_ = -1;
                event->accept();
                return;
              }

              if (event->modifiers().testFlag(Qt::ControlModifier)) {
                impl_->isDraggingVertex_ = false;
                impl_->resetLayerMutationNotify();
                impl_->isDraggingMaskHandle_ = true;
                impl_->draggingMaskIndex_ = m;
                impl_->draggingPathIndex_ = p;
                impl_->draggingVertexIndex_ = v;
                impl_->draggingMaskHandleType_ =
                    static_cast<int>(MaskHandleType::OutTangent);
                impl_->hoveredMaskIndex_ = m;
                impl_->hoveredPathIndex_ = p;
                impl_->hoveredVertexIndex_ = v;
                impl_->hoveredMaskHandleType_ =
                    static_cast<int>(MaskHandleType::OutTangent);
                qDebug() << "[PenTool] Started dragging bezier handle" << v;
                event->accept();
                return;
              }

              impl_->isDraggingVertex_ = true;
              impl_->resetLayerMutationNotify();
              impl_->draggingMaskIndex_ = m;
              impl_->draggingPathIndex_ = p;
              impl_->draggingVertexIndex_ = v;
              qDebug() << "[PenTool] Started dragging vertex" << v;
              event->accept();
              return;
            }
          }
        }
      }

      if (impl_->pendingMaskCreation_ &&
          impl_->pendingMaskLayerId_ == selectedLayer->id() &&
          impl_->pendingMaskPath_.vertexCount() >= 3) {
        const float closeThreshold = 12.0f / impl_->renderer_->getZoom();
        const MaskVertex firstVertex = impl_->pendingMaskPath_.vertex(0);
        if (QVector2D(firstVertex.position - localPos).length() <
            closeThreshold) {
          if (impl_->finalizePendingMaskCreation(selectedLayer)) {
            impl_->markMaskEditDirty();
            qDebug() << "[PenTool] Finalized pending mask path"
                     << "layer:" << selectedLayer->id().toString();
            ArtifactCore::globalEventBus().publish(LayerChangedEvent{
                comp->id().toString(), selectedLayer->id().toString(),
                LayerChangedEvent::ChangeType::Modified});
          }
          event->accept();
          return;
        }
      }

      int segmentMaskIndex = -1;
      int segmentPathIndex = -1;
      int segmentIndex = -1;
      const float segmentThreshold = 12.0f / impl_->renderer_->getZoom();
      if (hitTestMaskSegment(selectedLayer, QPointF(cPos.x, cPos.y),
                             segmentThreshold, segmentMaskIndex,
                             segmentPathIndex, segmentIndex)) {
        if (insertVertexOnMaskSegment(selectedLayer, segmentMaskIndex,
                                      segmentPathIndex, segmentIndex,
                                      localPos)) {
          const int insertedVertexIndex = segmentIndex + 1;
          impl_->markMaskEditDirty();
          impl_->isDraggingVertex_ = true;
          impl_->resetLayerMutationNotify();
          impl_->draggingMaskIndex_ = segmentMaskIndex;
          impl_->draggingPathIndex_ = segmentPathIndex;
          impl_->draggingVertexIndex_ = insertedVertexIndex;
          impl_->hoveredMaskIndex_ = segmentMaskIndex;
          impl_->hoveredPathIndex_ = segmentPathIndex;
          impl_->hoveredVertexIndex_ = insertedVertexIndex;
          qDebug() << "[PenTool] Inserted mask vertex on segment"
                   << "mask:" << segmentMaskIndex
                   << "path:" << segmentPathIndex
                   << "vertex:" << insertedVertexIndex;
          ArtifactCore::globalEventBus().publish(LayerChangedEvent{
              comp->id().toString(), selectedLayer->id().toString(),
              LayerChangedEvent::ChangeType::Modified});
        }
        event->accept();
        return;
      }

      impl_->beginPendingMaskCreation(selectedLayer, localPos);
      impl_->isDraggingVertex_ = false;
      impl_->isDraggingMaskHandle_ = true;
      impl_->draggingMaskIndex_ = -1;
      impl_->draggingPathIndex_ = -1;
      impl_->draggingVertexIndex_ =
          impl_->pendingMaskPath_.vertexCount() - 1;
      impl_->draggingMaskHandleType_ =
          static_cast<int>(MaskHandleType::OutTangent);

      qDebug() << "[PenTool] Added pending mask vertex at local:" << localPos
               << "layer:" << selectedLayer->id().toString()
               << "pendingVertices:" << impl_->pendingMaskPath_.vertexCount();
      event->accept();
      return;
    }
  }

  if (event->button() == Qt::LeftButton && activeTool != ToolType::Pen &&
      impl_->showMotionPathOverlay_ && selectedLayer && comp &&
      impl_->renderer_) {
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
      if (setSelectedLayerMotionPathKeyframeAtCurrentFrame()) {
        event->accept();
        return;
      }
    }

    const auto cPos = impl_->renderer_->viewportToCanvas(
        {(float)viewportPos.x(), (float)viewportPos.y()});
    const auto motionPathSamples = buildMotionPathSamples(selectedLayer, comp);
    MotionPathSample hitSample;
    const float hitThreshold = std::max(8.0f, 12.0f / impl_->renderer_->getZoom());
    if (hitTestMotionPathSample(motionPathSamples, QPointF(cPos.x, cPos.y),
                                hitThreshold, hitSample)) {
      const auto &t3d = selectedLayer->transform3D();
      const ArtifactCore::RationalTime time(hitSample.framePosition, 24);
      if (event->modifiers().testFlag(Qt::AltModifier)) {
        MotionPathPositionSnapshot before;
        before.hasPositionKey = t3d.hasPositionKeyFrameAt(time);
        before.x = t3d.positionXAt(time);
        before.y = t3d.positionYAt(time);
        if (before.hasPositionKey) {
          auto &mutableT3d = selectedLayer->transform3D();
          mutableT3d.removePositionKeyFrameAt(time);
          MotionPathPositionSnapshot after;
          after.hasPositionKey = false;
          after.x = mutableT3d.positionXAt(time);
          after.y = mutableT3d.positionYAt(time);
          if (auto *mgr = UndoManager::instance()) {
            mgr->push(std::make_unique<MotionPathUndoCommand>(
                selectedLayer, hitSample.framePosition, before, after));
          }
          selectedLayer->setDirty(LayerDirtyFlag::Transform);
          selectedLayer->changed();
          if (auto *compPtr =
                  static_cast<ArtifactAbstractComposition *>(selectedLayer->composition())) {
            ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
                LayerChangedEvent{compPtr->id().toString(),
                                  selectedLayer->id().toString(),
                                  LayerChangedEvent::ChangeType::Modified});
          }
          impl_->motionPathCache_.valid = false;
          impl_->invalidateOverlayComposite();
          markRenderDirty();
          event->accept();
          return;
        }
      }
      MotionPathPositionSnapshot before;
      before.hasPositionKey = t3d.hasPositionKeyFrameAt(time);
      before.x = t3d.positionXAt(time);
      before.y = t3d.positionYAt(time);
      impl_->beginMotionPathDrag(selectedLayer, hitSample.framePosition,
                                 QPointF(cPos.x, cPos.y), before);
      impl_->motionPathCache_.valid = false;
      impl_->invalidateOverlayComposite();
      event->accept();
      return;
    }
  }

  // 3D Gizmo hit test (GIZ-2) — only for 3D layers
  if (selectedLayer && impl_->gizmo3D_ && selectedLayer->is3D() &&
      activeTool != ToolType::Pen) {
    impl_->gizmo3D_->setDepthEnabled(selectedLayer->is3D());
    Ray ray = createPickingRay(viewportPos);
    GizmoAxis axis =
        impl_->gizmo3D_->hitTest(ray, impl_->renderer_->getViewMatrix(),
                                 impl_->renderer_->getProjectionMatrix());
    if (axis != GizmoAxis::None) {
      impl_->gizmo3D_->beginDrag(axis, ray);
      impl_->gizmoDragActive_ = true;
      notifyViewportInteractionActivity();
      impl_->invalidateOverlayComposite();
      markRenderDirty();
      return;
    }
  }

  // Gizmo hit test first (2D)
  if (activeTool != ToolType::Pen) {
    ArtifactAbstractLayerPtr gizmoLayer;
    if (comp && !impl_->selectedLayerId_.isNil()) {
      gizmoLayer = comp->layerById(impl_->selectedLayerId_);
    }
    if (impl_->textGizmo_ && layerUsesTextGizmo(gizmoLayer)) {
      impl_->textGizmo_->handleMousePress(viewportPos, impl_->renderer_.get());
      if (impl_->textGizmo_->isDragging()) {
        impl_->textGizmoDragActive_ = true;
        notifyViewportInteractionActivity();
        impl_->gizmoDragRenderTimer_.restart();
        return;
      }
    } else if (impl_->gizmo_) {
      impl_->gizmo_->handleMousePress(viewportPos, impl_->renderer_.get());
      if (impl_->gizmo_->isDragging()) {
        impl_->gizmoDragActive_ = true;
        notifyViewportInteractionActivity();
        impl_->gizmoDragRenderTimer_.restart();
        return;
      }
    }
  }

  // Tracker gizmo hit test (TrackPoint tool)
  if (activeTool == ToolType::TrackPoint && impl_->trackerGizmo_) {
    if (!impl_->trackerMotionTracker_) {
      trackerInitialize();
    }
    impl_->trackerGizmo_->handleMousePress(viewportPos, impl_->renderer_.get());
    if (impl_->trackerGizmo_->isDragging()) {
      impl_->trackerGizmoDragActive_ = true;
      notifyViewportInteractionActivity();
      impl_->gizmoDragRenderTimer_.restart();
      return;
    }
  }

  if (event->button() == Qt::LeftButton) {
    if (comp && impl_->renderer_) {
      const auto cPos = impl_->renderer_->viewportToCanvas(
          {(float)viewportPos.x(), (float)viewportPos.y()});
      auto *selection =
          ArtifactApplicationManager::instance()
              ? ArtifactApplicationManager::instance()->layerSelectionManager()
              : nullptr;
      const auto currentFrame = currentFrameForComposition(comp);

      const auto &layers = comp->allLayerRef();

      ArtifactAbstractLayerPtr hitLayer = nullptr;
      QVector<ArtifactAbstractLayerPtr> hitLayers;
      const bool ignoreLocked = event->modifiers().testFlag(Qt::AltModifier);
      const bool backPick = event->modifiers().testFlag(Qt::ControlModifier);

      // Collect all layers at this position
      for (int i = (int)layers.size() - 1; i >= 0; --i) {
        auto &layer = layers[i];
        if (!isLayerEffectivelyVisible(layer))
          continue;
        if ((layer->isLocked() || layer->isSelectionLocked()) && !ignoreLocked)
          continue;
        if (!layer->isActiveAt(currentFrame))
          continue;

        const QTransform globalTransform = layer->getGlobalTransform();
        bool invertible = false;
        const QTransform invTransform = globalTransform.inverted(&invertible);

        bool isHit = false;
        if (invertible) {
          const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));
          if (layer->localBounds().contains(localPos)) {
            isHit = true;
          }
        } else {
          auto bbox = layer->transformedBoundingBox();
          if (bbox.contains(cPos.x, cPos.y)) {
            isHit = true;
          }
        }

        if (isHit) {
          hitLayers.push_back(layer);
        }
      }

      // Cyclic selection logic
      if (!hitLayers.isEmpty()) {
        if (backPick && hitLayers.size() > 1) {
          hitLayer = hitLayers[1];
        } else {
          const float posThreshold = 3.0f; // px
          const bool sameSpot =
              QVector2D(viewportPos - impl_->lastHitPosition_).length() <
              posThreshold;

          if (sameSpot && !impl_->lastHitLayerId_.isNil()) {
            int currentHitIdx = -1;
            for (int i = 0; i < hitLayers.size(); ++i) {
              if (hitLayers[i]->id() == impl_->lastHitLayerId_) {
                currentHitIdx = i;
                break;
              }
            }

            if (currentHitIdx != -1) {
              int nextIdx = (currentHitIdx + 1) % hitLayers.size();
              hitLayer = hitLayers[nextIdx];
            } else {
              hitLayer = hitLayers[0];
            }
          } else {
            hitLayer = hitLayers[0];
          }
        }

        impl_->lastHitPosition_ = viewportPos;
        impl_->lastHitLayerId_ = hitLayer->id();
      } else {
        impl_->lastHitLayerId_ = LayerID::Nil();
      }

      if (hitLayer) {
        const std::shared_ptr<ArtifactAbstractLayer> hitLayerRef = hitLayer;
        if (selection) {
          const bool ctrl = event->modifiers().testFlag(Qt::ControlModifier);
          const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
          if (ctrl) {
            if (selection->isSelected(hitLayerRef)) {
              selection->removeFromSelection(hitLayerRef);
            } else {
              selection->addToSelection(hitLayerRef);
            }
          } else if (shift) {
            selection->addToSelection(hitLayerRef);
          } else {
            if (auto *svc = ArtifactProjectService::instance()) {
              svc->selectLayer(hitLayer->id());
            } else {
              selection->selectLayer(hitLayerRef);
            }
            // If svc failed to update the selection manager (e.g., its
            // currentComposition doesn't match the render controller's
            // composition), it may have called clearSelection(). Fix this
            // directly so the deferred syncSelectionState() won't nullify the
            // gizmo on the next tick.
            if (selection && !selection->currentLayer()) {
              selection->selectLayer(hitLayerRef);
            }
          }
        }

        // Use hitLayer as the fallback: if the selection manager still has no
        // current layer (composition mismatch, etc.), don't override with null.
        ArtifactAbstractLayerPtr primaryLayer = hitLayer;
        if (selection) {
          if (auto current = selection->currentLayer()) {
            primaryLayer = current;
          }
        }
        syncPrimarySelectionLayer(primaryLayer);
        impl_->sync2DGizmosForLayer(primaryLayer);
        if (impl_->gizmo3D_ && primaryLayer) {
          impl_->syncGizmo3DFromLayer(primaryLayer);
        }

        if (activeTool == ToolType::Selection) {
          impl_->clearSelectionGestureState();
          impl_->isRubberBandSelecting_ = false;
          impl_->dragGroupMove_ = selection &&
                                  selection->selectedLayers().size() > 1 &&
                                  selection->isSelected(hitLayerRef);
          impl_->dragGroupLayers_.clear();
          impl_->dragGroupStartPositions_.clear();
          if (impl_->dragGroupMove_ && selection) {
            const auto selected = selection->selectedLayers();
            impl_->dragGroupLayers_.reserve(selected.size());
            for (const auto &layer : selected) {
              if (!layer) {
                continue;
              }
              const QString id = layer->id().toString();
              impl_->dragGroupLayers_.push_back(layer);
              impl_->dragGroupStartPositions_.insert(
                  id, QPointF(layer->transform3D().positionX(),
                              layer->transform3D().positionY()));
            }
            impl_->dragMode_ = LayerDragMode::Move;
          } else {
            impl_->dragMode_ = hitTestLayerDragMode(hitLayer, event->position(),
                                                    impl_->renderer_.get());
            if (impl_->dragMode_ == LayerDragMode::None) {
              impl_->dragMode_ = LayerDragMode::Move;
            }
          }

          impl_->dragStartCanvasPos_ = QPointF(cPos.x, cPos.y);
          impl_->dragStartLayerPos_ =
              QPointF(hitLayer->transform3D().positionX(),
                      hitLayer->transform3D().positionY());
          impl_->dragStartScaleX_ = hitLayer->transform3D().scaleX();
          impl_->dragStartScaleY_ = hitLayer->transform3D().scaleY();
          impl_->dragStartBoundingBox_ = hitLayer->transformedBoundingBox();
          impl_->dragFrame_ = comp->framePosition().framePosition();
          impl_->dragAppliedDelta_ = QPointF(0.0, 0.0);
          impl_->isDraggingLayer_ = true;
        } else {
          impl_->clearSelectionGestureState();
          impl_->isDraggingLayer_ = false;
          impl_->dragMode_ = LayerDragMode::None;
        }
      } else {
        const bool selectionTool = activeTool == ToolType::Selection;
        if (selectionTool) {
          impl_->clearSelectionGestureState();
          impl_->isDraggingLayer_ = false;
          impl_->dragMode_ = LayerDragMode::None;
          impl_->isRubberBandSelecting_ = true;
          impl_->rubberBandStartViewportPos_ = viewportPos;
          impl_->rubberBandCurrentViewportPos_ = viewportPos;
          impl_->selectionMode_ =
              selectionModeFromModifiers(event->modifiers());
        } else {
          if (!(event->modifiers() & Qt::ShiftModifier)) {
            if (selection) {
              selection->clearSelection();
            }
            setSelectedLayerId(LayerID::Nil());
            impl_->sync2DGizmosForLayer(nullptr);
          }
          impl_->clearSelectionGestureState();
          impl_->isDraggingLayer_ = false;
          impl_->dragMode_ = LayerDragMode::None;
        }
      }
    }
  }
}

void CompositionRenderController::handleMouseMove(
    const QPointF &viewportPosLogical) {
  qCDebug(compositionViewLog)
      << "[MouseMove] ENTER logicalPos:" << viewportPosLogical
      << "devicePixelRatio:" << impl_->devicePixelRatio_;

  // Convert logical pixels (from Qt event) to physical pixels for the rendering
  // pipeline
  const QPointF viewportPos = viewportPosLogical * impl_->devicePixelRatio_;
  auto toolManager = ArtifactApplicationManager::instance()->toolManager();
  auto activeTool =
      toolManager ? toolManager->activeTool() : ToolType::Selection;
  bool needsRender = false;

  if (impl_->isRubberBandSelecting_) {
    impl_->rubberBandCurrentViewportPos_ = viewportPos;
    markRenderDirty();
    return;
  }

  // Finish MotionSketch on mouse release
  {
    auto* app = ArtifactApplicationManager::instance();
    if (app && app->motionSketchTool() && app->motionSketchTool()->isSketching()) {
      app->motionSketchTool()->finishSketch();
      markRenderDirty();
    }
  }
  if (impl_->isDraggingMotionPathKeyframe_) {
    auto comp = impl_->previewPipeline_.composition();
    if (comp && impl_->renderer_) {
      auto selectedLayer = comp->layerById(impl_->selectedLayerId_);
      if (selectedLayer) {
        const auto canvasPos = impl_->renderer_->viewportToCanvas(
            {(float)viewportPos.x(), (float)viewportPos.y()});
        if (impl_->applyMotionPathDrag(
                selectedLayer, QPointF(canvasPos.x, canvasPos.y))) {
          impl_->motionPathCache_.valid = false;
          markRenderDirty();
          return;
        }
      }
    }
  }

  if (impl_->showMotionPathOverlay_ && !impl_->isDraggingMotionPathKeyframe_) {
    auto comp = impl_->previewPipeline_.composition();
    if (comp && impl_->renderer_) {
      auto selectedLayer = comp->layerById(impl_->selectedLayerId_);
      if (selectedLayer) {
        const auto cPos = impl_->renderer_->viewportToCanvas(
            {(float)viewportPos.x(), (float)viewportPos.y()});
        const auto motionPathSamples = buildMotionPathSamples(selectedLayer, comp);
        MotionPathSample hoverSample;
        const float hoverThreshold =
            std::max(8.0f, 12.0f / impl_->renderer_->getZoom());
        bool hoverChanged = false;
        if (hitTestMotionPathSample(motionPathSamples, QPointF(cPos.x, cPos.y),
                                    hoverThreshold, hoverSample)) {
          hoverChanged = impl_->setHoveredMotionPathFrame(
              static_cast<int>(hoverSample.framePosition));
        } else {
          hoverChanged = impl_->setHoveredMotionPathFrame(-1);
        }
        if (hoverChanged) {
          needsRender = true;
        }
      } else {
        if (impl_->setHoveredMotionPathFrame(-1)) {
          needsRender = true;
        }
      }
    }
  } else if (impl_->hoveredMotionPathFrame_ != -1) {
    if (impl_->setHoveredMotionPathFrame(-1)) {
      needsRender = true;
    }
  }

  
  // MotionSketch tool: capture samples during drag
  if (activeTool == ToolType::MotionSketch && impl_->renderer_) {
    auto* app = ArtifactApplicationManager::instance();
    if (app && app->motionSketchTool() && app->motionSketchTool()->isSketching()) {
      const auto cPos = impl_->renderer_->viewportToCanvas(
          {(float)viewportPos.x(), (float)viewportPos.y()});
      app->motionSketchTool()->addSample(QPointF(cPos.x, cPos.y));
      markRenderDirty();
      return;
    }
  }

  // Puppet tool: drag pin
  if (activeTool == ToolType::Puppet && impl_->renderer_) {
    auto* app = ArtifactApplicationManager::instance();
    if (app && app->puppetTool()) {
      QString selId = app->puppetTool()->selectedPinId();
      if (!selId.isEmpty()) {
        const auto cPos = impl_->renderer_->viewportToCanvas(
            {(float)viewportPos.x(), (float)viewportPos.y()});
        app->puppetTool()->movePin(selId, QPointF(cPos.x, cPos.y));
        auto comp = impl_->previewPipeline_.composition();
        if (comp && !impl_->selectedLayerId_.isNil()) {
          app->puppetTool()->deformLayer(impl_->selectedLayerId_, impl_->renderer_.get());
        }
        markRenderDirty();
        return;
      }
    }
  }
if (activeTool == ToolType::Pen && impl_->isDraggingVertex_) {
    auto comp = impl_->previewPipeline_.composition();
    if (comp && impl_->renderer_) {
      auto selectedLayer = comp->layerById(impl_->selectedLayerId_);
      if (selectedLayer) {
        const auto cPos = impl_->renderer_->viewportToCanvas(
            {(float)viewportPos.x(), (float)viewportPos.y()});
        const QTransform globalTransform = selectedLayer->getGlobalTransform();
        bool invertible = false;
        const QTransform invTransform = globalTransform.inverted(&invertible);

        if (invertible) {
          const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));
          LayerMask mask = selectedLayer->mask(impl_->draggingMaskIndex_);
          MaskPath path = mask.maskPath(impl_->draggingPathIndex_);
          MaskVertex vertex = path.vertex(impl_->draggingVertexIndex_);

          vertex.position = localPos;
          path.setVertex(impl_->draggingVertexIndex_, vertex);
          mask.setMaskPath(impl_->draggingPathIndex_, path);
          selectedLayer->setMask(impl_->draggingMaskIndex_, mask);
          impl_->markMaskEditDirty();
          impl_->publishLayerModified(selectedLayer);
          return;
        }
      }
    }
  }

  if (activeTool == ToolType::Pen && impl_->isDraggingMaskHandle_) {
    auto comp = impl_->previewPipeline_.composition();
    if (comp && impl_->renderer_) {
      auto selectedLayer = comp->layerById(impl_->selectedLayerId_);
      if (selectedLayer) {
        const auto cPos = impl_->renderer_->viewportToCanvas(
            {(float)viewportPos.x(), (float)viewportPos.y()});
        const QTransform globalTransform = selectedLayer->getGlobalTransform();
        bool invertible = false;
        const QTransform invTransform = globalTransform.inverted(&invertible);

        if (invertible) {
          const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));
          const auto handleType =
              static_cast<MaskHandleType>(impl_->draggingMaskHandleType_);
          const bool breakTangents =
              QGuiApplication::keyboardModifiers().testFlag(Qt::AltModifier);

          if (impl_->draggingMaskIndex_ == -1 &&
              impl_->pendingMaskCreation_ &&
              impl_->draggingVertexIndex_ >= 0 &&
              impl_->draggingVertexIndex_ <
                  impl_->pendingMaskPath_.vertexCount()) {
            MaskVertex vertex =
                impl_->pendingMaskPath_.vertex(impl_->draggingVertexIndex_);
            setMaskVertexHandle(vertex, handleType, localPos - vertex.position,
                                breakTangents);
            impl_->pendingMaskPath_.setVertex(impl_->draggingVertexIndex_,
                                              vertex);
            impl_->penMaskPreviewCanvasPos_ = {cPos.x, cPos.y};
            impl_->penMaskPreviewValid_ = true;
            impl_->invalidateOverlayComposite();
            markRenderDirty();
            return;
          }

          if (impl_->draggingMaskIndex_ >= 0 &&
              impl_->draggingPathIndex_ >= 0 &&
              impl_->draggingVertexIndex_ >= 0) {
            LayerMask mask = selectedLayer->mask(impl_->draggingMaskIndex_);
            MaskPath path = mask.maskPath(impl_->draggingPathIndex_);
            MaskVertex vertex = path.vertex(impl_->draggingVertexIndex_);
            setMaskVertexHandle(vertex, handleType, localPos - vertex.position,
                                breakTangents);
            path.setVertex(impl_->draggingVertexIndex_, vertex);
            mask.setMaskPath(impl_->draggingPathIndex_, path);
            selectedLayer->setMask(impl_->draggingMaskIndex_, mask);
            impl_->markMaskEditDirty();
            impl_->publishLayerModified(selectedLayer);
            return;
          }
        }
      }
    }
  }

  if (impl_->rectangleToolDragging_) {
    const auto cPos = impl_->renderer_->viewportToCanvas(
        {(float)viewportPos.x(), (float)viewportPos.y()});
    impl_->rectangleToolCurrentCanvasPos_ = QPointF(cPos.x, cPos.y);
    markRenderDirty();
    return;
  }

  // Hover detection for Pen tool
  if (activeTool == ToolType::Pen) {
    impl_->penToolPreviewVisible_ = true;
    const int prevHoveredMaskIndex = impl_->hoveredMaskIndex_;
    const int prevHoveredPathIndex = impl_->hoveredPathIndex_;
    const int prevHoveredVertexIndex = impl_->hoveredVertexIndex_;
    const int prevHoveredMaskHandleType = impl_->hoveredMaskHandleType_;
    impl_->hoveredMaskIndex_ = -1;
    impl_->hoveredPathIndex_ = -1;
    impl_->hoveredVertexIndex_ = -1;
    impl_->hoveredMaskHandleType_ = -1;

    auto comp = impl_->previewPipeline_.composition();
    if (comp && impl_->renderer_) {
      auto selectedLayer = comp->layerById(impl_->selectedLayerId_);
      if (selectedLayer) {
        const auto cPos = impl_->renderer_->viewportToCanvas(
            {(float)viewportPos.x(), (float)viewportPos.y()});
        const QTransform globalTransform = selectedLayer->getGlobalTransform();
        bool invertible = false;
        const QTransform invTransform = globalTransform.inverted(&invertible);

        if (invertible) {
          const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));
          const float hitThreshold = 12.0f / impl_->renderer_->getZoom();
          impl_->penMaskPreviewCanvasPos_ = {cPos.x, cPos.y};
          impl_->penMaskPreviewValid_ = true;

          int handleMaskIndex = -1;
          int handlePathIndex = -1;
          int handleVertexIndex = -1;
          MaskHandleType handleType = MaskHandleType::None;
          if (hitTestMaskHandle(selectedLayer, QPointF(cPos.x, cPos.y),
                                hitThreshold, handleMaskIndex, handlePathIndex,
                                handleVertexIndex, handleType)) {
            impl_->hoveredMaskIndex_ = handleMaskIndex;
            impl_->hoveredPathIndex_ = handlePathIndex;
            impl_->hoveredVertexIndex_ = handleVertexIndex;
            impl_->hoveredMaskHandleType_ = static_cast<int>(handleType);
          } else {
            for (int m = 0; m < selectedLayer->maskCount(); ++m) {
              LayerMask mask = selectedLayer->mask(m);
              for (int p = 0; p < mask.maskPathCount(); ++p) {
                MaskPath path = mask.maskPath(p);
                for (int v = 0; v < path.vertexCount(); ++v) {
                  MaskVertex vertex = path.vertex(v);
                  if (QVector2D(vertex.position - localPos).length() <
                      hitThreshold) {
                    impl_->hoveredMaskIndex_ = m;
                    impl_->hoveredPathIndex_ = p;
                    impl_->hoveredVertexIndex_ = v;
                    break;
                  }
               }
                if (impl_->hoveredVertexIndex_ != -1)
                  break;
              }
              if (impl_->hoveredVertexIndex_ != -1)
                break;
            }
          }
        }
      }
    }
    if (prevHoveredMaskIndex != impl_->hoveredMaskIndex_ ||
        prevHoveredPathIndex != impl_->hoveredPathIndex_ ||
        prevHoveredVertexIndex != impl_->hoveredVertexIndex_ ||
        prevHoveredMaskHandleType != impl_->hoveredMaskHandleType_) {
      impl_->invalidateOverlayComposite();
      needsRender = true;
    }
  } else {
    impl_->penToolPreviewVisible_ = false;
    impl_->penMaskPreviewValid_ = false;
  }

  // 3D Gizmo interaction (GIZ-2, GIZ-3) — only for 3D layers
  if (impl_->gizmo3D_) {
    auto comp3D = impl_->previewPipeline_.composition();
    auto sel3DLayer = (!impl_->selectedLayerId_.isNil() && comp3D)
                          ? comp3D->layerById(impl_->selectedLayerId_)
                          : ArtifactAbstractLayerPtr{};
    if (sel3DLayer && sel3DLayer->is3D()) {
      Ray ray = createPickingRay(viewportPos);
      if (impl_->gizmo3D_->isDragging()) {
        notifyViewportInteractionActivity();
        impl_->gizmo3D_->updateDrag(ray);

        // Update layer transform from gizmo
        auto comp = impl_->previewPipeline_.composition();
        if (comp && !impl_->selectedLayerId_.isNil()) {
          if (auto layer = comp->layerById(impl_->selectedLayerId_)) {
            if (impl_->gizmo3D_->mode() == GizmoMode::Scale) {
              auto &t3 = layer->transform3D();
              const ArtifactCore::RationalTime time(layer->currentFrame(), 30);
              const QVector3D scale = impl_->gizmo3D_->scale();
              t3.setScale(time, scale.x(), scale.y());
            } else {
              const QVector3D currentPos = layer->position3D();
              const QVector3D gizmoPos = impl_->gizmo3D_->position();
              if (impl_->gizmo3D_->depthEnabled()) {
                layer->setPosition3D(gizmoPos);
              } else {
                layer->setPosition3D(
                    QVector3D(gizmoPos.x(), gizmoPos.y(), currentPos.z()));
              }
              layer->setRotation3D(impl_->gizmo3D_->rotation());
            }
            ArtifactCore::globalEventBus().publish(
                LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                                  LayerChangedEvent::ChangeType::Modified});
          }
        }
        return;
      } else {
        // Hover highlighting
        const auto prevHoverAxis = impl_->gizmo3D_->hoverAxis();
        impl_->gizmo3D_->hitTest(ray, impl_->renderer_->getViewMatrix(),
                                 impl_->renderer_->getProjectionMatrix());
        if (prevHoverAxis != impl_->gizmo3D_->hoverAxis()) {
          impl_->invalidateOverlayComposite();
          needsRender = true;
        }
      }
    }
  }

  if (impl_->textGizmo_ && impl_->textGizmo_->isDragging()) {
    impl_->textGizmo_->handleMouseMove(viewportPos, impl_->renderer_.get());
    notifyViewportInteractionActivity();
    impl_->invalidateBaseComposite();
    markRenderDirty();
    return;
  }

  if (impl_->gizmo_) {
    impl_->gizmo_->handleMouseMove(viewportPos, impl_->renderer_.get());
    if (impl_->gizmo_->isDragging()) {
      notifyViewportInteractionActivity();
      impl_->invalidateBaseComposite();
      markRenderDirty();
      return;
    }
  }

  if (impl_->trackerGizmo_ && impl_->trackerGizmo_->isDragging()) {
    impl_->trackerGizmo_->handleMouseMove(viewportPos, impl_->renderer_.get());
    notifyViewportInteractionActivity();
    impl_->invalidateBaseComposite();
    markRenderDirty();
    return;
  }

  if (needsRender) {
    markRenderDirty();
  }
}

void CompositionRenderController::handleMouseRelease() {
  qCDebug(compositionViewLog) << "[MouseRelease] ENTER";

  // Finish MotionSketch on mouse release
  {
    auto* app = ArtifactApplicationManager::instance();
    if (app && app->motionSketchTool() && app->motionSketchTool()->isSketching()) {
      app->motionSketchTool()->finishSketch();
      markRenderDirty();
    }
  }
  if (impl_->isDraggingMotionPathKeyframe_) {
    auto layer = impl_->draggingMotionPathLayer_.lock();
    if (layer) {
      const ArtifactCore::RationalTime time(impl_->draggingMotionPathFrame_, 24);
      const auto &t3d = layer->transform3D();
      MotionPathPositionSnapshot after;
      after.hasPositionKey = t3d.hasPositionKeyFrameAt(time);
      after.x = t3d.positionXAt(time);
      after.y = t3d.positionYAt(time);

      const bool changed = impl_->draggingMotionPathBefore_.hasPositionKey != after.hasPositionKey ||
                           std::abs(impl_->draggingMotionPathBefore_.x - after.x) > 0.0001f ||
                           std::abs(impl_->draggingMotionPathBefore_.y - after.y) > 0.0001f;
      if (changed) {
        if (auto *mgr = UndoManager::instance()) {
          mgr->push(std::make_unique<MotionPathUndoCommand>(
              layer, impl_->draggingMotionPathFrame_,
              impl_->draggingMotionPathBefore_, after));
        }
        if (auto *comp = static_cast<ArtifactAbstractComposition *>(layer->composition())) {
          ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
              LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                                LayerChangedEvent::ChangeType::Modified});
        }
      }
    }
    impl_->clearMotionPathDragState();
    impl_->motionPathCache_.valid = false;
    impl_->invalidateOverlayComposite();
    markRenderDirty();
  }

  impl_->isDraggingLayer_ = false;
  auto comp = impl_->previewPipeline_.composition();

  auto syncPrimarySelectionLayer = [this, comp](const ArtifactAbstractLayerPtr &primaryLayer) {
    const LayerID primaryId = primaryLayer ? primaryLayer->id() : LayerID::Nil();
    if (impl_->selectedLayerId_ != primaryId) {
      impl_->clearPendingMaskCreation();
      impl_->selectedLayerId_ = primaryId;
      impl_->invalidateOverlayComposite();
    }
    impl_->syncSelectedLayerOverlayState(comp);
    markRenderDirty();
  };

  if (impl_->isRubberBandSelecting_) {
    auto *selection =
        ArtifactApplicationManager::instance()
            ? ArtifactApplicationManager::instance()->layerSelectionManager()
            : nullptr;
    if (comp && selection && impl_->renderer_) {
      const QRectF rect = impl_->rubberBandCanvasRect().normalized();
      const auto currentFrame = currentFrameForComposition(comp);
      const auto &layers = comp->allLayerRef();
      QVector<ArtifactAbstractLayerPtr> hits;
      hits.reserve(layers.size());
      for (const auto &layer : layers) {
        if (!layerIntersectsCanvasRect(layer, rect, currentFrame)) {
          continue;
        }
        hits.push_back(layer);
      }

      if (impl_->selectionMode_ == SelectionMode::Replace) {
        selection->clearSelection();
      }

      for (const auto &layer : hits) {
        if (!layer) {
          continue;
        }
        const std::shared_ptr<ArtifactAbstractLayer> layerRef = layer;
        if (impl_->selectionMode_ == SelectionMode::Toggle) {
          if (selection->isSelected(layerRef)) {
            selection->removeFromSelection(layerRef);
          } else {
            selection->addToSelection(layerRef);
          }
        } else {
          selection->addToSelection(layerRef);
        }
      }

      if (impl_->selectionMode_ == SelectionMode::Replace && hits.isEmpty()) {
        selection->clearSelection();
      }

      const auto primaryLayer = selection->currentLayer();
      syncPrimarySelectionLayer(primaryLayer);
      impl_->sync2DGizmosForLayer(primaryLayer);
      if (impl_->gizmo3D_ && primaryLayer) {
        impl_->syncGizmo3DFromLayer(primaryLayer);
      }
    }
    impl_->clearSelectionGestureState();
    markRenderDirty();
    return;
  }

  if (impl_->rectangleToolDragging_) {
    auto comp = impl_->previewPipeline_.composition();
    auto *selectionManager = ArtifactApplicationManager::instance()
                                 ? ArtifactApplicationManager::instance()
                                       ->layerSelectionManager()
                                 : nullptr;
    const QRectF rect = dragRectFromPoints(impl_->rectangleToolStartCanvasPos_,
                                           impl_->rectangleToolCurrentCanvasPos_);
    const bool meaningfulRect = isMeaningfulDragRect(rect);

    if (impl_->rectangleToolMode_ == RectangleToolMode::Mask) {
      auto layer = impl_->rectangleToolTargetLayer_.lock();
      if (layer && meaningfulRect) {
        const QTransform globalTransform = layer->getGlobalTransform();
        bool invertible = false;
        const QTransform invTransform = globalTransform.inverted(&invertible);
        const auto corners = rectCorners(rect);

        MaskPath path;
        for (const auto &corner : corners) {
          const QPointF localCorner =
              invertible ? invTransform.map(corner) : corner;
          MaskVertex vertex;
          vertex.position = localCorner;
          vertex.inTangent = QPointF(0.0, 0.0);
          vertex.outTangent = QPointF(0.0, 0.0);
          path.addVertex(vertex);
        }
        path.setClosed(true);

        LayerMask mask;
        mask.addMaskPath(path);
        layer->addMask(mask);
        impl_->markMaskEditDirty();
        impl_->publishLayerModified(layer, true);
      }
      impl_->commitMaskEditTransaction();
    } else if (impl_->rectangleToolMode_ == RectangleToolMode::Shape &&
               meaningfulRect && comp) {
      const QString layerName =
          uniqueLayerNameForCurrentComposition(QStringLiteral("Rectangle"));
      if (auto *service = ArtifactProjectService::instance()) {
        ArtifactLayerInitParams params(layerName, LayerType::Shape);
        service->addLayerToCurrentComposition(params, true);

        ArtifactAbstractLayerPtr createdLayer =
            selectionManager ? selectionManager->currentLayer()
                             : ArtifactAbstractLayerPtr{};
        if (!createdLayer && !comp->allLayer().isEmpty()) {
          createdLayer = comp->allLayer().back();
        }

        if (createdLayer) {
          const QRectF normalizedRect = rect.normalized();
          const QVector3D currentPos = createdLayer->position3D();
          createdLayer->setPosition3D(QVector3D(
              static_cast<float>(normalizedRect.left()),
              static_cast<float>(normalizedRect.top()), currentPos.z()));
          if (auto shapeLayer =
                  std::dynamic_pointer_cast<ArtifactShapeLayer>(createdLayer)) {
            shapeLayer->setSize(
                std::max(1, static_cast<int>(std::lround(normalizedRect.width()))),
                std::max(1, static_cast<int>(std::lround(normalizedRect.height()))));
          }
          setSelectedLayerId(createdLayer->id());
          impl_->sync2DGizmosForLayer(createdLayer);
          if (impl_->gizmo3D_) {
            impl_->syncGizmo3DFromLayer(createdLayer);
          }
        }
      }
    }

    impl_->clearRectangleToolSession();
    markRenderDirty();
    return;
  }

  const bool publishFinalMaskEditChange = impl_->maskEditDirty_;
  const auto maskEditLayer = impl_->maskEditLayer_.lock();
  impl_->isDraggingVertex_ = false;
  impl_->isDraggingMaskHandle_ = false;
  impl_->draggingMaskIndex_ = -1;
  impl_->draggingPathIndex_ = -1;
  impl_->draggingVertexIndex_ = -1;
  impl_->draggingMaskHandleType_ = -1;
  impl_->commitMaskEditTransaction();
  if (publishFinalMaskEditChange) {
    impl_->publishLayerModified(maskEditLayer, true);
  }
  impl_->resetLayerMutationNotify();

  if (impl_->gizmo3D_) {
    const bool wasDragging = impl_->gizmoDragActive_;
    impl_->gizmoDragActive_ = false;
    impl_->gizmo3D_->endDrag();
    impl_->invalidateOverlayComposite();
    if (wasDragging) {
      finishViewportInteraction();
    }
    markRenderDirty();
  }

  if (impl_->textGizmo_) {
    const bool wasTextDragging = impl_->textGizmoDragActive_;
    impl_->textGizmoDragActive_ = false;
    impl_->textGizmo_->handleMouseRelease();
    impl_->invalidateOverlayComposite();
    if (wasTextDragging) {
      finishViewportInteraction();
    }
    markRenderDirty();
  }

  if (impl_->gizmo_) {
    const bool wasDragging = impl_->gizmoDragActive_;
    impl_->gizmoDragActive_ = false;
    impl_->gizmo_->handleMouseRelease();
    impl_->invalidateOverlayComposite();
    if (wasDragging) {
      finishViewportInteraction();
    }
    markRenderDirty();
  }

  if (impl_->trackerGizmo_) {
    const bool wasTrackerDragging = impl_->trackerGizmoDragActive_;
    impl_->trackerGizmoDragActive_ = false;
    impl_->trackerGizmo_->handleMouseRelease();
    impl_->invalidateOverlayComposite();
    if (wasTrackerDragging) {
      finishViewportInteraction();
    }
    markRenderDirty();
  }
}

bool CompositionRenderController::hasPendingMaskEdit() const {
  return impl_ && impl_->maskEditPending_;
}

TransformGizmo *CompositionRenderController::gizmo() const {
  return impl_->gizmo_.get();
}

Artifact3DGizmo *CompositionRenderController::gizmo3D() const {
  return impl_->gizmo3D_.get();
}

ArtifactPointTrackerGizmo *CompositionRenderController::trackerGizmo() const {
  return impl_->trackerGizmo_.get();
}

void CompositionRenderController::trackerInitialize() {
  auto* tm = ArtifactCore::TrackerManager::instance().createTracker(
      QStringLiteral("Point Tracker"));
  impl_->trackerMotionTracker_ = tm;
  if (impl_->trackerGizmo_) {
    impl_->trackerGizmo_->setTracker(tm);
  }
}

static void ensureOffscreenRenderer(
    std::unique_ptr<Artifact::OffscreenCompositionRenderer>& renderer,
    ArtifactIRenderer* mainRenderer, int width, int height)
{
  if (!renderer && mainRenderer) {
    auto device = mainRenderer->device();
    if (device) {
      renderer = std::make_unique<Artifact::OffscreenCompositionRenderer>(
          device, static_cast<unsigned int>(width), static_cast<unsigned int>(height));
    }
  }
}

void CompositionRenderController::trackerTrackForward() {
  if (!impl_->trackerMotionTracker_ || !impl_->trackerGizmo_) return;
  auto* tracker = impl_->trackerMotionTracker_;
  auto* gizmo = impl_->trackerGizmo_.get();
  auto* renderer = impl_->renderer_.get();
  if (!renderer) return;

  auto comp = impl_->previewPipeline_.composition();
  if (!comp) return;

  const float fps = comp->frameRate().framerate();
  if (fps <= 0.0f) return;

  const QSize compSize = comp->settings().compositionSize();
  const int compW = compSize.width();
  const int compH = compSize.height();
  if (compW <= 0 || compH <= 0) return;

  ensureOffscreenRenderer(impl_->trackerOffscreenRenderer_, renderer, compW, compH);
  if (!impl_->trackerOffscreenRenderer_) return;

  tracker->clearTrackingData();
  const auto& st = gizmo->state();
  tracker->addTrackPoint({st.innerCenter.x(), st.innerCenter.y()});

  const int frameCount = 30;
  const double frameStep = 1.0 / fps;

  for (int i = 0; i <= frameCount; ++i) {
    const double timeSec = i * frameStep;
    ArtifactCore::FramePosition pos(static_cast<int64_t>(i));
    QImage frame = impl_->trackerOffscreenRenderer_->renderToQImage(pos, comp.get());
    if (!frame.isNull()) {
      tracker->setFrame(timeSec, frame);
    }
  }

  tracker->trackForward(0.0, frameCount * frameStep);

  gizmo->setTracker(tracker);
  markRenderDirty();
}

void CompositionRenderController::trackerTrackBackward() {
  if (!impl_->trackerMotionTracker_ || !impl_->trackerGizmo_) return;
  auto* tracker = impl_->trackerMotionTracker_;
  auto* gizmo = impl_->trackerGizmo_.get();
  auto* renderer = impl_->renderer_.get();
  if (!renderer) return;

  auto comp = impl_->previewPipeline_.composition();
  if (!comp) return;

  const float fps = comp->frameRate().framerate();
  if (fps <= 0.0f) return;

  const QSize compSize = comp->settings().compositionSize();
  const int compW = compSize.width();
  const int compH = compSize.height();
  if (compW <= 0 || compH <= 0) return;

  ensureOffscreenRenderer(impl_->trackerOffscreenRenderer_, renderer, compW, compH);
  if (!impl_->trackerOffscreenRenderer_) return;

  tracker->clearTrackingData();
  const auto& st = gizmo->state();
  tracker->addTrackPoint({st.innerCenter.x(), st.innerCenter.y()});

  const int frameCount = 30;
  const double frameStep = 1.0 / fps;

  for (int i = frameCount; i >= 0; --i) {
    const double timeSec = i * frameStep;
    ArtifactCore::FramePosition pos(static_cast<int64_t>(i));
    QImage frame = impl_->trackerOffscreenRenderer_->renderToQImage(pos, comp.get());
    if (!frame.isNull()) {
      tracker->setFrame(timeSec, frame);
    }
  }

  tracker->trackBackward(frameCount * frameStep, 0.0);

  gizmo->setTracker(tracker);
  markRenderDirty();
}

void CompositionRenderController::trackerTrackAll() {
  if (!impl_->trackerMotionTracker_ || !impl_->trackerGizmo_) return;
  auto* tracker = impl_->trackerMotionTracker_;
  auto* gizmo = impl_->trackerGizmo_.get();
  auto* renderer = impl_->renderer_.get();
  if (!renderer) return;

  auto comp = impl_->previewPipeline_.composition();
  if (!comp) return;

  const int64_t totalFrames = comp->frameRange().duration();
  const float fps = comp->frameRate().framerate();
  if (totalFrames <= 0 || fps <= 0.0f) return;

  const QSize compSize = comp->settings().compositionSize();
  const int compW = compSize.width();
  const int compH = compSize.height();
  if (compW <= 0 || compH <= 0) return;

  ensureOffscreenRenderer(impl_->trackerOffscreenRenderer_, renderer, compW, compH);
  if (!impl_->trackerOffscreenRenderer_) return;

  tracker->clearTrackingData();
  const auto& st = gizmo->state();
  tracker->addTrackPoint({st.innerCenter.x(), st.innerCenter.y()});

  for (int64_t i = 0; i < totalFrames; ++i) {
    const double timeSec = static_cast<double>(i) / fps;
    ArtifactCore::FramePosition pos(i);
    QImage frame = impl_->trackerOffscreenRenderer_->renderToQImage(pos, comp.get());
    if (!frame.isNull()) {
      tracker->setFrame(timeSec, frame);
    }
  }

  tracker->trackAll();

  gizmo->setTracker(tracker);
  markRenderDirty();
}

void CompositionRenderController::trackerApplyToPosition() {
  if (!impl_->trackerMotionTracker_ || !impl_->trackerGizmo_) return;

  auto comp = impl_->previewPipeline_.composition();
  if (!comp) return;

  ArtifactAbstractLayerPtr targetLayer;
  if (!impl_->selectedLayerId_.isNil()) {
    targetLayer = comp->layerById(impl_->selectedLayerId_);
  }

  Artifact::ArtifactPointTrackerTool::ApplyOptions opts;
  opts.createNullLayer = true;
  opts.applyToSelectedLayer = (targetLayer != nullptr);

  Artifact::ArtifactPointTrackerTool::applyTrackingResult(
      comp.get(), *impl_->trackerMotionTracker_, opts, targetLayer);

  markRenderDirty();
}

void CompositionRenderController::trackerApplyToAnchor() {
  if (!impl_->trackerMotionTracker_ || !impl_->trackerGizmo_) return;

  auto comp = impl_->previewPipeline_.composition();
  if (!comp) return;

  ArtifactAbstractLayerPtr targetLayer;
  if (!impl_->selectedLayerId_.isNil()) {
    targetLayer = comp->layerById(impl_->selectedLayerId_);
  }

  Artifact::ArtifactPointTrackerTool::ApplyOptions opts;
  opts.createNullLayer = false;
  opts.applyToSelectedLayer = (targetLayer != nullptr);
  opts.writeAnchor = true;

  Artifact::ArtifactPointTrackerTool::applyTrackingResult(
      comp.get(), *impl_->trackerMotionTracker_, opts, targetLayer);

  markRenderDirty();
}

void CompositionRenderController::trackerDelete() {
  if (impl_->trackerMotionTracker_) {
    ArtifactCore::TrackerManager::instance().removeTracker(
        impl_->trackerMotionTracker_->id());
    impl_->trackerMotionTracker_ = nullptr;
  }
  if (impl_->trackerGizmo_) {
    impl_->trackerGizmo_->setTracker(nullptr);
  }
  markRenderDirty();
}

CompositionRenderController::CameraFrustumVisual
CompositionRenderController::cameraFrustumVisual() const {
  return buildCameraFrustumVisual(impl_->previewPipeline_.composition(),
                                  impl_->selectedLayerId_);
}

void CompositionRenderController::setViewportOrientation(
    ArtifactCore::ViewOrientationHotspot hotspot) {
  if (!impl_) {
    return;
  }
  if (impl_->viewportOrientationActive_ &&
      impl_->viewportOrientationNavigator_.activeHotspot() == hotspot) {
    return;
  }
  impl_->viewportOrientationNavigator_.snapTo(hotspot, true);
  impl_->viewportOrientationActive_ = true;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

ArtifactCore::ViewOrientationHotspot
CompositionRenderController::viewportOrientation() const {
  if (!impl_) {
    return ArtifactCore::ViewOrientationHotspot::Front;
  }
  return impl_->viewportOrientationNavigator_.activeHotspot();
}

// ROI Debug
void CompositionRenderController::setDebugMode(bool enabled) {
  impl_->debugMode_ = enabled;
}

bool CompositionRenderController::isDebugMode() const {
  return impl_->debugMode_;
}

Qt::CursorShape CompositionRenderController::cursorShapeForViewportPos(
    const QPointF &viewportPos) const {
  const auto *app = ArtifactApplicationManager::instance();
  const auto *toolManager = app ? app->toolManager() : nullptr;
  const ToolType activeTool =
      toolManager != nullptr ? toolManager->activeTool() : ToolType::Selection;

  if (activeTool == ToolType::Pen) {
    if (impl_->isDraggingMaskHandle_ || impl_->isDraggingVertex_) {
      return Qt::ClosedHandCursor;
    }
    if (impl_->hoveredMaskHandleType_ !=
        static_cast<int>(MaskHandleType::None)) {
      return Qt::SizeAllCursor;
    }
    if (impl_->hoveredVertexIndex_ >= 0) {
      return Qt::OpenHandCursor;
    }
    return Qt::CrossCursor;
  }

  if (activeTool == ToolType::Rectangle) {
    return Qt::CrossCursor;
  }

  if (activeTool == ToolType::MotionSketch) {
    return Qt::CrossCursor;
  }

  if (activeTool == ToolType::Puppet) {
    return Qt::PointingHandCursor;
  }

  if (activeTool == ToolType::TrackPoint) {
    if (impl_->renderer_ && impl_->trackerGizmo_) {
      const QPointF physPos = viewportPos * impl_->devicePixelRatio_;
      return impl_->trackerGizmo_->cursorShapeForViewportPos(
          physPos, impl_->renderer_.get());
    }
    return Qt::CrossCursor;
  }

  if (!impl_->renderer_) {
    return Qt::ArrowCursor;
  }

  auto comp = impl_->previewPipeline_.composition();
  ArtifactAbstractLayerPtr selectedLayer;
  if (comp && !impl_->selectedLayerId_.isNil()) {
    selectedLayer = comp->layerById(impl_->selectedLayerId_);
  }

  // viewportPos is in logical pixels; convert to physical for gizmo hit testing
  const QPointF physPos = viewportPos * impl_->devicePixelRatio_;
  if (impl_->textGizmo_ && layerUsesTextGizmo(selectedLayer)) {
    return impl_->textGizmo_->cursorShapeForViewportPos(physPos,
                                                        impl_->renderer_.get());
  }
  if (!impl_->gizmo_) {
    return Qt::ArrowCursor;
  }
  return impl_->gizmo_->cursorShapeForViewportPos(physPos,
                                                  impl_->renderer_.get());
}

void CompositionRenderController::renderOneFrame() {
  if (!impl_->initialized_ || !impl_->renderer_) {
    return;
  }
  if (!impl_->running_) {
    return;
  }
  if (auto *host = impl_->hostWidget_.data()) {
    if (!host->isVisible()) {
      return;
    }
  }
  // Re-entrancy guard: renderOneFrameImpl must not be called recursively.
  if (impl_->renderScheduled_) {
    return;
  }
  impl_->renderScheduled_ = true;
  impl_->renderOneFrameImpl(this);
  impl_->renderScheduled_ = false;
}

void CompositionRenderController::markRenderDirty() {
  impl_->renderDirty_.store(true, std::memory_order_release);
  if (impl_->running_ && impl_->renderTickDriver_ &&
      !impl_->renderTickDriver_->isRunning()) {
    impl_->renderTickDriver_->start();
  }
}

void CompositionRenderController::Impl::clearPendingMaskCreation() {
  pendingMaskCreation_ = false;
  pendingMaskLayerId_ = LayerID();
  pendingMaskPath_.clearVertices();
  pendingMaskPath_.setClosed(false);
  penToolPreviewVisible_ = false;
  penMaskPreviewValid_ = false;
}

void CompositionRenderController::Impl::clearRectangleToolSession() {
  rectangleToolDragging_ = false;
  rectangleToolMode_ = RectangleToolMode::None;
  rectangleToolStartCanvasPos_ = {};
  rectangleToolCurrentCanvasPos_ = {};
  rectangleToolTargetLayer_.reset();
}

void CompositionRenderController::Impl::beginRectangleToolSession(
    RectangleToolMode mode, const ArtifactAbstractLayerPtr &layer,
    const QPointF &canvasPos) {
  rectangleToolDragging_ = mode != RectangleToolMode::None;
  rectangleToolMode_ = mode;
  rectangleToolStartCanvasPos_ = canvasPos;
  rectangleToolCurrentCanvasPos_ = canvasPos;
  rectangleToolTargetLayer_ = layer;
  if (!rectangleToolDragging_) {
    rectangleToolTargetLayer_.reset();
  }
}

void CompositionRenderController::Impl::beginPendingMaskCreation(
    const ArtifactAbstractLayerPtr &layer, const QPointF &localPos) {
  if (!layer) {
    return;
  }

  if (!pendingMaskCreation_ || pendingMaskLayerId_ != layer->id()) {
    clearPendingMaskCreation();
    pendingMaskCreation_ = true;
    pendingMaskLayerId_ = layer->id();
  }

  MaskVertex vertex;
  vertex.position = localPos;
  vertex.inTangent = QPointF(0, 0);
  vertex.outTangent = QPointF(0, 0);
  pendingMaskPath_.addVertex(vertex);
  penMaskPreviewValid_ = true;
}

bool CompositionRenderController::Impl::finalizePendingMaskCreation(
    const ArtifactAbstractLayerPtr &layer) {
  if (!layer || !pendingMaskCreation_ || pendingMaskLayerId_ != layer->id()) {
    return false;
  }

  if (pendingMaskPath_.vertexCount() < 3) {
    return false;
  }

  MaskPath path = pendingMaskPath_;
  path.setClosed(true);

  beginMaskEditTransaction(layer);

  LayerMask newMask;
  newMask.addMaskPath(path);
  layer->addMask(newMask);
  markMaskEditDirty();

  clearPendingMaskCreation();
  return true;
}

void CompositionRenderController::Impl::renderOneFrameImpl(
    CompositionRenderController *owner) {
  renderCrashTrace("render-enter", renderFrameCounter_);
  if (!owner || !initialized_ || !renderer_ || !running_) {
    renderCrashTrace("render-skip-not-ready", renderFrameCounter_);
    return;
  }
  // Swapchain may not exist yet (deferred from 0×0 init).
  // Skip rendering — the first resize will create the swapchain and
  // trigger a new frame via the debounce timer.
  if (!renderer_->hasSwapChain()) {
    renderCrashTrace("render-skip-no-swapchain", renderFrameCounter_);
    return;
  }

  // 変更検出器のデバッグログ (カテゴリ制御)
  static int renderCount = 0;
  if (renderCount++ % 60 == 0) { // 2秒に1回
    qCDebug(compositionViewLog)
        << "[CompositionChangeDetector]" << changeDetector_.debugInfo();
  }
  if (auto *host = hostWidget_.data()) {
    if (!host->isVisible()) {
      renderCrashTrace("render-skip-host-hidden", renderFrameCounter_);
      return;
    }
  }

  struct RenderCostCaptureGuard {
    ArtifactIRenderer* renderer = nullptr;
    quint64 frame = 0;
    RenderCostCaptureGuard(ArtifactIRenderer* r, quint64 f) : renderer(r), frame(f) {
      if (renderer) {
        renderer->beginFrameCostCapture();
        renderer->beginFrameGpuProfiling();
      }
    }
    ~RenderCostCaptureGuard() {
      if (renderer) {
        renderCrashTrace("render-cost-guard-gpu-end-begin", frame);
        renderer->endFrameGpuProfiling();
        renderCrashTrace("render-cost-guard-gpu-end-end", frame);
        renderCrashTrace("render-cost-guard-cost-end-begin", frame);
        renderer->endFrameCostCapture();
        renderCrashTrace("render-cost-guard-cost-end-end", frame);
      }
    }
  };

  // 強制的なサイズ同期:
  // ホストウィジェットの物理サイズとスワップチェーンを一致させる
  if (auto *host = hostWidget_.data()) {
    const float curW = static_cast<float>(host->width()) * devicePixelRatio_;
    const float curH = static_cast<float>(host->height()) * devicePixelRatio_;
    if (std::abs(curW - hostWidth_) > 0.5f ||
        std::abs(curH - hostHeight_) > 0.5f) {
      qCDebug(compositionViewLog)
          << "[CompositionView] Widget size changed, scheduling swapchain "
             "update:"
          << curW << "x" << curH;
      const QSize pending(host->width(), host->height());
      pendingResizeSize_ = QSize();
      if (owner && renderer_) {
        renderCrashTrace("render-resize-begin", renderFrameCounter_,
                         QStringLiteral("size=%1x%2").arg(pending.width()).arg(pending.height()));
        owner->setViewportSize(static_cast<float>(pending.width()),
                               static_cast<float>(pending.height()));
        owner->recreateSwapChain(host);
        owner->markRenderDirty();
        renderCrashTrace("render-resize-end", renderFrameCounter_);
      }
      return;
    }
  }

  QElapsedTimer frameTimer;
  frameTimer.start();
  qint64 phaseNs = 0;
  auto markPhaseMs = [&frameTimer, &phaseNs]() -> qint64 {
    const qint64 nowNs = frameTimer.nsecsElapsed();
    const qint64 phaseMs = (nowNs - phaseNs) / 1000000;
    phaseNs = nowNs;
    return phaseMs;
  };

  struct TraceScopeGuard {
    ArtifactCore::TraceScopeRecord scope;
    QElapsedTimer timer;
    TraceScopeGuard() {
      scope.name = QStringLiteral("CompositionRenderController::renderOneFrameImpl");
      scope.domain = ArtifactCore::TraceDomain::Render;
      scope.startNs = 0;
      timer.start();
    }
    ~TraceScopeGuard() {
      scope.endNs = timer.nsecsElapsed();
      if (scope.endNs <= scope.startNs) {
        scope.endNs = scope.startNs + 1;
      }
      ArtifactCore::TraceRecorder::instance().recordScope(scope);
    }
  } traceGuard;

  renderer_->setSceneLights(std::vector<ArtifactCore::Light>{});
  renderCrashTrace("render-after-scene-lights", renderFrameCounter_);

  auto comp = previewPipeline_.composition();
  if (auto *service = ArtifactProjectService::instance()) {
    const auto preferred = resolvePreferredComposition(service);
    if (preferred && preferred != comp) {
      comp = preferred;
      previewPipeline_.setComposition(comp);
      qCDebug(compositionViewLog)
          << "[CompositionView] renderOneFrame resynced preferred composition"
          << "id=" << comp->id().toString()
          << "layers=" << comp->allLayerRef().size();
    }
  }
  if (!comp) {
    renderCrashTrace("render-no-comp-present-begin", renderFrameCounter_);
    renderer_->setOverrideRTV(nullptr);
    renderer_->setClearColor(viewportClearColor_);
    renderer_->clearRenderTarget(viewportClearColor_);
    renderer_->present();
    renderCrashTrace("render-no-comp-present-end", renderFrameCounter_);
    return;
  }

  if (auto *service = ArtifactProjectService::instance()) {
    const auto projectDiagnostics = service->currentProjectDiagnostics();
    const bool hasBlockingErrors = std::any_of(
        projectDiagnostics.begin(), projectDiagnostics.end(),
        [](const auto& diagnostic) { return diagnostic.isError(); });
    if (hasBlockingErrors) {
      const QString blockedSummary =
          QStringLiteral("preflight=blocked issues=%1")
              .arg(static_cast<int>(projectDiagnostics.size()));
      if (blockedSummary != lastRenderPathSummary_) {
        qWarning() << "[CompositionView] render preflight blocked by project health errors"
                   << "issues=" << projectDiagnostics.size();
      }
      lastRenderPathSummary_ = blockedSummary;
      renderCrashTrace("render-preflight-blocked-present-begin", renderFrameCounter_);
      renderer_->setOverrideRTV(nullptr);
      renderer_->setClearColor(viewportClearColor_);
      renderer_->clearRenderTarget(viewportClearColor_);
      renderer_->present();
      renderCrashTrace("render-preflight-blocked-present-end", renderFrameCounter_);
      return;
    }
  }

  auto renderCostGuard =
      std::make_unique<RenderCostCaptureGuard>(renderer_.get(), renderFrameCounter_);
  renderCrashTrace("render-cost-begin", renderFrameCounter_);

  const QSize compSize = comp->settings().compositionSize();
  const float cw =
      static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);
  const float ch =
      static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);
  const FloatColor bgColor = comp->backgroundColor();
  lastCanvasWidth_ = cw;
  lastCanvasHeight_ = ch;

  // --- Profiler frame begin ---
  {
    bool isPlayback = false;
    if (auto *pb = ArtifactPlaybackService::instance())
      isPlayback = pb->isPlaying();
    ArtifactCore::Profiler::instance().beginFrame(
        static_cast<std::int64_t>(renderFrameCounter_), compSize.width(),
        compSize.height(), isPlayback);
  }

  // Update MayaGradient sprite cache once; it is viewport background art and
  // should not be tinted by the composition fill color.
  if (!floatColorEquals(cachedMayaGradientBgColor_, bgColor)) {
    cachedMayaGradientBgColor_ = bgColor;
    cachedMayaGradientSprite_ = QImage();
  }
  if (compositionBackgroundMode_ == CompositionBackgroundMode::MayaGradient &&
      cachedMayaGradientSprite_.isNull() && !cachedMayaGradientWarmupQueued_ &&
      !cachedMayaGradientWarmupFuture_.isRunning()) {
    cachedMayaGradientWarmupQueued_ = true;
    cachedMayaGradientWarmupBgColor_ = bgColor;
    cachedMayaGradientWarmupFuture_ =
        QtConcurrent::run(&sharedBackgroundThreadPool(), [bgColor]() {
          return makeMayaGradientSprite(bgColor);
        });
  }
  if (cachedMayaGradientWarmupQueued_ &&
      cachedMayaGradientWarmupFuture_.isFinished()) {
    QImage warmed = cachedMayaGradientWarmupFuture_.result();
    cachedMayaGradientWarmupQueued_ = false;
    if (floatColorEquals(cachedMayaGradientWarmupBgColor_,
                         cachedMayaGradientBgColor_)) {
      cachedMayaGradientSprite_ = std::move(warmed);
    }
  }
  if (compositionBackgroundMode_ == CompositionBackgroundMode::MayaGradient) {
    const QString mayaDebugState =
        QStringLiteral(
            "mode=MayaGradient spriteNull=%1 spriteSize=%2x%3 bg=%4,%5,%6,%7")
            .arg(cachedMayaGradientSprite_.isNull() ? 1 : 0)
            .arg(cachedMayaGradientSprite_.width())
            .arg(cachedMayaGradientSprite_.height())
            .arg(bgColor.r(), 0, 'f', 3)
            .arg(bgColor.g(), 0, 'f', 3)
            .arg(bgColor.b(), 0, 'f', 3)
            .arg(bgColor.a(), 0, 'f', 3);
    if (mayaDebugState != lastMayaGradientDebugState_) {
      lastMayaGradientDebugState_ = mayaDebugState;
      qCDebug(compositionViewLog)
          << "[CompositionView][Background]" << mayaDebugState;
    }
  } else if (!lastMayaGradientDebugState_.isEmpty()) {
    lastMayaGradientDebugState_.clear();
  }

  // GPU path should represent the currently visible viewport, not only the
  // composition rect. Otherwise layers that extend outside the comp get clipped
  // at the intermediate RT stage.
  const float viewportW = hostWidth_ > 0.0f ? hostWidth_ : cw;
  const float viewportH = hostHeight_ > 0.0f ? hostHeight_ : ch;
  const int effectivePreviewDownsample =
      viewportInteracting_
          ? std::max(previewDownsample_, interactivePreviewDownsampleFloor_)
          : (gpuBlendEnabled_ ? std::max(previewDownsample_, 2)
                              : previewDownsample_);
  const float previewRcw = std::max(
      1.0f, viewportW / static_cast<float>(effectivePreviewDownsample));
  const float previewRch = std::max(
      1.0f, viewportH / static_cast<float>(effectivePreviewDownsample));
  // GPU blend intermediates mirror the editor viewport. This keeps non-Normal
  // blend modes consistent with the direct editor path when layers extend
  // outside the composition rect.
  const float rcw = gpuBlendEnabled_ ? std::max(1.0f, viewportW) : previewRcw;
  const float rch = gpuBlendEnabled_ ? std::max(1.0f, viewportH) : previewRch;

  if (compositionRenderer_) {
    compositionRenderer_->SetCompositionSize(cw, ch);
    // Note: ApplyCompositionSpace sets renderer canvas size to FULL size.
    // We override it below if pipeline is enabled.
    compositionRenderer_->ApplyCompositionSpace();
  } else {
    renderer_->setCanvasSize(cw, ch);
  }

  const auto &layers = comp->allLayerRef();
  FramePosition currentFrame = comp->framePosition();
  if (auto *playback = ArtifactPlaybackService::instance()) {
    const auto playbackComp = playback->currentComposition();
    if (!playbackComp || playbackComp->id() == comp->id()) {
      currentFrame = playback->currentFrame();
    }
  }

  const int sceneLightFps = std::max(
      1, static_cast<int>(std::llround(comp->frameRate().framerate())));
  const RationalTime sceneLightTime(currentFrame.framePosition(), sceneLightFps);
  std::vector<SceneLightEntry> sceneLights;
  sceneLights.reserve(layers.size());
  for (const auto &layer : layers) {
    if (!layer || !isLayerEffectivelyVisible(layer) ||
        !layer->isActiveAt(currentFrame)) {
      continue;
    }

    auto *lightLayer = dynamic_cast<ArtifactLightLayer *>(layer.get());
    if (!lightLayer) {
      continue;
    }
    sceneLights.push_back(
        SceneLightEntry{makeSceneLightFromLayer(lightLayer, sceneLightTime), lightLayer});
  }
  std::vector<ArtifactCore::Light> rendererSceneLights;
  rendererSceneLights.reserve(sceneLights.size());
  for (const auto &entry : sceneLights) {
    rendererSceneLights.push_back(entry.light);
  }
  renderer_->setSceneLights(rendererSceneLights);

  // Build matte source resolver: find layer by ID, render to QImage
  auto matteResolverLambda = [&layers](const ArtifactCore::Id &layerId) -> QImage {
      for (const auto &l : layers) {
          if (l && l->id() == layerId) {
              if (auto *imgLayer = dynamic_cast<ArtifactImageLayer *>(l.get())) {
                  return imgLayer->toQImage();
              }
              if (auto *videoLayer = dynamic_cast<ArtifactVideoLayer *>(l.get())) {
                  return videoLayer->currentFrameToQImage();
              }
              if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(l.get())) {
                  return textLayer->toQImage();
              }
              if (auto *svgLayer = dynamic_cast<ArtifactSvgLayer *>(l.get())) {
                  return svgLayer->toQImage();
              }
          }
      }
      return {};
  };
  std::function<QImage(const ArtifactCore::Id &)> matteResolver = matteResolverLambda;

  // Find active camera layer for 3D rendering
  ArtifactCameraLayer *activeCamera = nullptr;
  for (const auto &l : layers) {
    auto layerCopy = l;
    if (auto cam = dynamic_cast<ArtifactCameraLayer *>(layerCopy.get())) {
      if (isLayerEffectivelyVisible(layerCopy) && cam->isActiveAt(currentFrame)) {
        activeCamera = cam;
        break; // Use first visible camera
      }
    }
  }

  // Compute camera matrices if we have a visible camera
  bool has3DCamera = false;
  QMatrix4x4 cameraViewMatrix;
  QMatrix4x4 cameraProjMatrix;
  if (activeCamera) {
    const QSize compSize = comp->settings().compositionSize();
    const float cw =
        static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);
    const float ch =
        static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);
    const float aspect = std::max(0.001f, cw / std::max(0.001f, ch));

    cameraViewMatrix = activeCamera->viewMatrix();
    cameraProjMatrix = activeCamera->projectionMatrix(aspect);
    if (activeCamera->stereoMode() != StereoMode::Mono) {
      const ArtifactCore::StereoCamera stereoCamera =
          ArtifactCore::StereoCamera::fromHmd(cameraViewMatrix.inverted(),
                                              activeCamera->ipd(),
                                              activeCamera->nearClipPlane(),
                                              activeCamera->farClipPlane());
      cameraViewMatrix = stereoCamera.leftEyeView;
    }
    has3DCamera = true;
  }
  int64_t effectiveEndFrame = 0;
  for (const auto &l : layers) {
    if (l) {
      effectiveEndFrame =
          std::max(effectiveEndFrame, l->outPoint().framePosition());
    }
  }
  const int64_t framePos = currentFrame.framePosition();
  const bool frameOutOfRange =
      (framePos < 0 ||
       (effectiveEndFrame > 0 && framePos >= effectiveEndFrame));
  ArtifactCore::ImageF32x4_RGBA ramPreviewFrameImage;
  bool useRamPreviewFallback = false;
  QString ramPreviewFallbackReason = QStringLiteral("no-playback-service");
  bool playbackSameComposition = false;
  ArtifactRamPreviewFrameCacheState playbackPreviewState;
  bool playbackPreviewStateValid = false;
  bool playbackPreviewPendingBuild = false;
  bool playbackAllowsRamFallbackWhilePlaying = false;
  bool playbackPlaying = false;
  if (auto *playback = ArtifactPlaybackService::instance()) {
    const auto playbackComp = playback->currentComposition();
    const bool sameComposition =
        !playbackComp || playbackComp->id() == comp->id();
    playbackSameComposition = sameComposition;
    if (!sameComposition) {
      ramPreviewFallbackReason = QStringLiteral("composition-mismatch");
    } else {
      playbackPreviewState = playback->ramPreviewFrameState(framePos);
      playbackPreviewStateValid = true;
      playbackPreviewPendingBuild =
          playback->isRamPreviewFramePendingBuild(framePos);
      playbackAllowsRamFallbackWhilePlaying =
          playback->ramPreviewPlaybackFallbackWhilePlaying();
      playbackPlaying = playback->isPlaying();
      if (frameOutOfRange) {
        ramPreviewFallbackReason = QStringLiteral("frame-out-of-range");
      } else if (!playbackPreviewState.ready) {
        ramPreviewFallbackReason =
            ramPreviewNotReadyReason(playbackPreviewState);
      } else if (!playbackPreviewState.imageAvailable) {
        ramPreviewFallbackReason = QStringLiteral("ready-missing-image");
      } else if (!playback->tryGetRamPreviewFrameImage(framePos, ramPreviewFrameImage)) {
        ramPreviewFallbackReason = QStringLiteral("ready-missing-image");
      } else if (viewportInteracting_) {
        ramPreviewFallbackReason = QStringLiteral("viewport-interacting");
      } else if (playbackPlaying && !playbackAllowsRamFallbackWhilePlaying) {
        ramPreviewFallbackReason = QStringLiteral("playing-policy-disabled");
      } else {
        useRamPreviewFallback = true;
        ramPreviewFallbackReason =
            playbackPlaying ? QStringLiteral("ready-playing")
                            : QStringLiteral("ready");
      }
    }
  }
  float panX = 0.0f;
  float panY = 0.0f;
  renderer_->getPan(panX, panY);
  const float zoom = renderer_->getZoom();
  const bool viewportTransformChangedSinceLastFrame =
      std::abs(lastRenderKeyState_.zoom - zoom) > 0.0001f ||
      std::abs(lastRenderKeyState_.panX - panX) > 0.5f ||
      std::abs(lastRenderKeyState_.panY - panY) > 0.5f;
  if (useRamPreviewFallback && viewportTransformChangedSinceLastFrame) {
    useRamPreviewFallback = false;
    ramPreviewFallbackReason = QStringLiteral("viewport-transform-changing");
  }
  const QRectF visibleCanvasRect = viewportRectToCanvasRect(
      renderer_.get(), QPointF(0.0f, 0.0f), QPointF(viewportW, viewportH));
  const float roiPad = std::max(48.0f, 64.0f / std::max(0.001f, zoom));
  const QRectF roiRect =
      visibleCanvasRect.adjusted(-roiPad, -roiPad, roiPad, roiPad);
  // --- Change detection via packed struct instead of string concatenation ---
  const auto currentBgColor = comp->backgroundColor();
  const bool bgChanged = currentBgColor.r() != lastBgColorCache_.r() ||
                         currentBgColor.g() != lastBgColorCache_.g() ||
                         currentBgColor.b() != lastBgColorCache_.b() ||
                         currentBgColor.a() != lastBgColorCache_.a();
  if (bgChanged || lastBackgroundCompositionId_ != comp->id()) {
    lastBgColorCache_ = currentBgColor;
    lastBackgroundCompositionId_ = comp->id();
    qCDebug(compositionViewLog) << "[CompositionView][Background]"
                                << "compositionId=" << comp->id().toString();
  }
  const auto backgroundMode = compositionBackgroundMode_;
  const FloatColor layerBgColor = currentBgColor;
  const Impl::RenderKeyState currentKey{
      comp->id(),
      baseInvalidationSerial_,
      overlayInvalidationSerial_,
      framePos,
      static_cast<int32_t>(viewportW),
      static_cast<int32_t>(viewportH),
      effectivePreviewDownsample,
      zoom,
      panX,
      panY,
      currentBgColor.r(),
      currentBgColor.g(),
      currentBgColor.b(),
      currentBgColor.a(),
      static_cast<int32_t>(backgroundMode),
      checkerboardTileSize_,
      gridSettings_.majorInterval,
      gridSettings_.subdivisions,
      static_cast<uint8_t>(gridSettings_.showMajor ? 1 : 0),
      static_cast<uint8_t>(gridSettings_.showMinor ? 1 : 0),
      static_cast<uint8_t>(gridSettings_.showAxis ? 1 : 0),
      gridSettings_.majorColor.r(),
      gridSettings_.majorColor.g(),
      gridSettings_.majorColor.b(),
      gridSettings_.majorColor.a(),
      gridSettings_.minorColor.r(),
      gridSettings_.minorColor.g(),
      gridSettings_.minorColor.b(),
      gridSettings_.minorColor.a(),
      gridSettings_.axisColor.r(),
      gridSettings_.axisColor.g(),
      gridSettings_.axisColor.b(),
      gridSettings_.axisColor.a(),
      gizmo3D_ ? static_cast<int32_t>(gizmo3D_->mode()) : -1,
      gizmo3D_ ? static_cast<int32_t>(gizmo3D_->hoverAxis()) : -1,
      gizmo3D_ ? static_cast<int32_t>(gizmo3D_->activeAxis()) : -1,
      static_cast<uint8_t>(gpuBlendEnabled_ ? 1 : 0),
      static_cast<uint8_t>(showGrid_ ? 1 : 0),
      static_cast<uint8_t>(showGuides_ ? 1 : 0),
      static_cast<uint8_t>(showSafeMargins_ ? 1 : 0),
      static_cast<uint8_t>(showAnchorCenterOverlay_ ? 1 : 0),
      static_cast<uint8_t>(showCameraFrustumOverlay_ ? 1 : 0),
      static_cast<uint8_t>(viewportInteracting_ ? 1 : 0),
      selectedLayerId_};
  const bool forceContinuousRedraw =
      viewportInteracting_ || isRubberBandSelecting_ || dropGhostVisible_ ||
      (gizmo_ && gizmo_->isDragging()) ||
      (textGizmo_ && textGizmo_->isDragging());
  if (!forceContinuousRedraw && currentKey == lastRenderKeyState_) {
    return;
  }
  lastRenderKeyState_ = currentKey;
  renderer_->setClearColor(viewportClearColor_);
  renderer_->clear();

  {

    const bool hasGpuBlendJustification =
        std::any_of(layers.begin(), layers.end(),
                    [&](const ArtifactAbstractLayerPtr &layer) {
                      if (!layer || !isLayerEffectivelyVisible(layer) ||
                          !layer->isActiveAt(currentFrame)) {
                        return false;
                      }
                      return layer->layerBlendType() !=
                                 ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL &&
                             !layerHasCpuRasterizerWork(layer.get());
                    });
    const bool hasGpuBlendBlocker =
        std::any_of(layers.begin(), layers.end(),
                    [&](const ArtifactAbstractLayerPtr &layer) {
                      return layer && isLayerEffectivelyVisible(layer) &&
                             layer->isActiveAt(currentFrame) &&
                             layerHasCpuRasterizerWork(layer.get());
                    });
    const bool gpuBlendRequested = gpuBlendEnabled_ && blendPipelineReady_;
    const bool gpuBlendPathRequested =
        gpuBlendRequested && hasGpuBlendJustification && !hasGpuBlendBlocker;

    auto &previewRenderSlot = acquirePreviewRenderPipelineSlot();
    auto &renderPipeline = previewRenderSlot.pipeline;

    // Avoid paying render-pipeline setup cost when GPU blending is disabled.
    if (gpuBlendPathRequested) {
      if (auto device = renderer_->device()) {
        renderPipeline.initialize(device, static_cast<Uint32>(rcw),
                                  static_cast<Uint32>(rch),
                                  RenderConfig::LinearColorFormat);
        if (!ensurePreviewRenderPipelineDepthSlot(
                previewRenderSlot,
                static_cast<int>(std::ceil(rcw)),
                static_cast<int>(std::ceil(rch)))) {
          qWarning() << "[CompositionView] failed to allocate preview depth slot"
                     << "slot=" << activePreviewRenderPipelineSlot_
                     << "size="
                     << QSize(static_cast<int>(std::ceil(rcw)),
                              static_cast<int>(std::ceil(rch)));
        }
      }
    } else if (gpuBlendRequested && lastPipelineStateMask_ != -1) {
      qCDebug(compositionViewLog)
          << "[CompositionView] GPU blend path not used for this frame"
          << "layers=" << layers.size()
          << "frameOutOfRange=" << frameOutOfRange;
    }

    const bool transparentCompositionBackgroundRequested =
        currentBgColor.a() < 0.999f;
    const bool previewRenderSlotAcquireHazard =
        lastPreviewRenderPipelineAcquireHazard_;
    const bool pipelineEnabled =
        gpuBlendPathRequested && renderPipeline.ready() &&
        previewRenderSlot.depthTargetView != nullptr &&
        !previewRenderSlotAcquireHazard &&
        !transparentCompositionBackgroundRequested;
    if (!pipelineEnabled) {
      lastLayerRtPixelStats_.clear();
      lastAccumRtPixelStats_.clear();
    }
    previewRenderSlot.state = pipelineEnabled
                                  ? PreviewRenderPipelineSlot::State::Ready
                                  : PreviewRenderPipelineSlot::State::Free;
    const QString multiFrameReason = multiFramePreviewFallbackReason(
        gpuBlendEnabled_, blendPipelineReady_, hasGpuBlendJustification,
        hasGpuBlendBlocker, gpuBlendPathRequested, renderPipeline.ready(),
        previewRenderSlot.depthTargetView != nullptr,
        previewRenderSlotAcquireHazard,
        transparentCompositionBackgroundRequested);
    const int pipelineStateMask = (gpuBlendEnabled_ ? 0x1 : 0x0) |
                                  (renderPipeline.ready() ? 0x2 : 0x0) |
                                  (blendPipelineReady_ ? 0x4 : 0x0);
    renderCrashTrace("render-pipeline-state", renderFrameCounter_,
                     QStringLiteral("enabled=%1 requested=%2 blendReady=%3 renderReady=%4 layers=%5")
                         .arg(pipelineEnabled ? 1 : 0)
                         .arg(gpuBlendPathRequested ? 1 : 0)
                         .arg(blendPipelineReady_ ? 1 : 0)
                         .arg(renderPipeline.ready() ? 1 : 0)
                         .arg(layers.size()));
    if (transparentCompositionBackgroundRequested && gpuBlendPathRequested &&
        renderPipeline.ready()) {
      qCDebug(compositionViewLog)
          << "[CompositionView] transparent composition background forces fallback path"
          << "alpha=" << currentBgColor.a();
    } else if (previewRenderSlotAcquireHazard && gpuBlendPathRequested &&
               renderPipeline.ready()) {
      qCDebug(compositionViewLog)
          << "[CompositionView] submitted slot hazard forces fallback path"
          << "slot=" << activePreviewRenderPipelineSlot_
          << "acquire=" << lastPreviewRenderPipelineSlotAcquireReason_;
    }
    if (pipelineStateMask != lastPipelineStateMask_) {
      lastPipelineStateMask_ = pipelineStateMask;
      if (!pipelineEnabled) {
        renderCrashTrace("render-gpu-blend-disabled-log-begin", renderFrameCounter_);
        qWarning() << "[CompositionView] GPU blend path disabled"
                   << "gpuBlendEnabled=" << gpuBlendEnabled_
                   << "renderPipelineReady=" << renderPipeline.ready()
                   << "slot=" << activePreviewRenderPipelineSlot_
                   << "blendPipelineReady=" << blendPipelineReady_ << "size="
                   << QSize(static_cast<int>(cw), static_cast<int>(ch));
        renderCrashTrace("render-gpu-blend-disabled-log-end", renderFrameCounter_);
      } else {
        renderCrashTrace("render-gpu-blend-enabled-log-begin", renderFrameCounter_);
        qDebug() << "[CompositionView] GPU blend path enabled"
                 << "slot=" << activePreviewRenderPipelineSlot_
                 << "size="
                 << QSize(static_cast<int>(cw), static_cast<int>(ch));
        renderCrashTrace("render-gpu-blend-enabled-log-end", renderFrameCounter_);
      }
    }
    renderCrashTrace("render-after-pipeline-state-log", renderFrameCounter_);
    if (pipelineEnabled) {
      const QSize pipelineSize(static_cast<int>(renderPipeline.width()),
                               static_cast<int>(renderPipeline.height()));
      // Compute shaders now have explicit bounds guards.
      if (((pipelineSize.width() & 7) != 0 ||
           (pipelineSize.height() & 7) != 0) &&
          pipelineSize != lastDispatchWarningSize_) {
        lastDispatchWarningSize_ = pipelineSize;
        qCDebug(compositionViewLog) << "[CompositionView] GPU blend path uses "
                                       "non-8-aligned render size: "
                                    << pipelineSize;
      }
    }

    int drawnLayerCount = 0;
    int surfaceUploadLayerCount = 0;
    int cpuRasterLayerCount = 0;
    int maskedLayerCount = 0;
    int totalMaskCount = 0;
    int nonNormalBlendLayerCount = 0;
    int blendDispatchCount = 0;
    int blendFailureCount = 0;
    int blendRetryNormalCount = 0;
    int directBlendFallbackCount = 0;
    int layerToFloatConvertCount = 0;
    int skipInvisibleCount = 0;
    int skipSoloCount = 0;
    int skipInactiveCount = 0;
    int skipRoiCount = 0;
    int skipLodCount = 0;
    int opacityZeroCount = 0;
    int compositedLayerCount = 0;
    QStringList blendMaskLayerNotes;
    const float targetViewportW = hostWidth_;
    const float targetViewportH = hostHeight_;
    const float legacyDownsampleViewportW =
        hostWidth_ > 0.0f
            ? hostWidth_ / static_cast<float>(effectivePreviewDownsample)
            : 0.0f;
    const float legacyDownsampleViewportH =
        hostHeight_ > 0.0f
            ? hostHeight_ / static_cast<float>(effectivePreviewDownsample)
            : 0.0f;
    qint64 setupMs = markPhaseMs();
    qint64 basePassMs = 0;
    qint64 layerPassMs = 0;
    qint64 overlayMs = 0;
    qint64 flushMs = 0;
    qint64 presentMs = 0;

    ArtifactCore::ProfileScope _profSetup(
        "RenderFrame", ArtifactCore::ProfileCategory::Render);

    // hasSoloLayer: dirty-flag キャッシュで毎フレームの O(N) スキャンを回避
    if (soloLayerCacheDirty_) {
      hasSoloLayerCache_ = std::any_of(
          layers.begin(), layers.end(), [](const ArtifactAbstractLayerPtr &l) {
            return l && l->isVisible() && l->isSolo();
          });
      soloLayerCacheDirty_ = false;
    }
    const bool hasSoloLayer = hasSoloLayerCache_;
    const QStringList selectedIds = selectedLayerIdList();
    const bool hasSelection = !selectedIds.isEmpty();

    if (compositionViewLog().isDebugEnabled()) {
      const ArtifactAbstractLayerPtr overlaySelectedLayer =
          (!selectedLayerId_.isNil() && comp)
              ? comp->layerById(selectedLayerId_)
              : ArtifactAbstractLayerPtr{};
      const int overlayMaskCount =
          overlaySelectedLayer ? overlaySelectedLayer->maskCount() : 0;
      const int overlayActiveHandle =
          gizmo_ ? static_cast<int>(gizmo_->activeHandle()) : -1;
      const QString overlaySummary =
          QStringLiteral("frame=%1 selCount=%2 selectedLayer=%3 gizmo=%4 "
                         "gizmoMode=%5 gizmoDragging=%6 activeHandle=%7 "
                         "motionPath=%8 anchorCenter=%9 masks=%10 region=%11")
              .arg(currentFrame.framePosition())
              .arg(selectedIds.size())
              .arg(selectedLayerId_.isNil() ? QStringLiteral("<none>")
                                            : selectedLayerId_.toString())
              .arg(showGizmoOverlay_ ? 1 : 0)
              .arg(static_cast<int>(gizmoMode_))
              .arg(gizmo_ && gizmo_->isDragging() ? 1 : 0)
              .arg(overlayActiveHandle)
              .arg(showMotionPathOverlay_ ? 1 : 0)
              .arg(showAnchorCenterOverlay_ ? 1 : 0)
              .arg(overlayMaskCount)
              .arg(showCompositionRegionOverlay_ ? 1 : 0);
      if (overlaySummary != lastOverlayDebugSummary_ ||
          (renderFrameCounter_ % 120u) == 0u) {
        lastOverlayDebugSummary_ = overlaySummary;
        qCDebug(compositionViewLog)
            << "[CompositionView][OverlayState]" << overlaySummary;
      }
    }

    // バックバッファ全体をクリア (外側ゴミ表示修正)
    renderCrashTrace("render-clear-begin", renderFrameCounter_);
    renderer_->clearRenderTarget(viewportClearColor_);
    renderCrashTrace("render-clear-end", renderFrameCounter_);

    Diligent::ITextureView* ramPreviewReadbackSRV = nullptr;
    lastPresentedReadbackSRV_ = nullptr;
    if (useRamPreviewFallback) {
      renderCrashTrace("render-branch-ram-preview", renderFrameCounter_);
      if (backgroundMode == CompositionBackgroundMode::MayaGradient) {
        drawViewportMayaGradientBackground(renderer_.get(), viewportW,
                                           viewportH, layerBgColor,
                                           cachedMayaGradientSprite_);
      } else if (backgroundMode == CompositionBackgroundMode::Checkerboard) {
        drawViewportCheckerboardBackground(renderer_.get(), viewportW,
                                           viewportH, checkerboardTileSize_);
      }
      renderer_->setUseExternalMatrices(false);
      renderer_->resetGizmoCameraMatrices();
      renderer_->reset3DCameraMatrices();
      renderer_->setCanvasSize(cw, ch);
      renderer_->setZoom(zoom);
      renderer_->setPan(panX, panY);
      drawCompositionBackgroundDirect(renderer_.get(), cw, ch, layerBgColor,
                                      backgroundMode, checkerboardTileSize_,
                                      cachedMayaGradientSprite_);
      QMatrix4x4 identity;
      renderer_->drawSpriteTransformed(0.0f, 0.0f, cw, ch, identity,
                                       ramPreviewFrameImage, 1.0f);
      basePassMs = markPhaseMs();
      layerPassMs = 0;
    } else if (pipelineEnabled) {
    renderCrashTrace("render-branch-gpu-pipeline", renderFrameCounter_);
    // ============================================================
    // GPU パイプライン: レイヤー 0 枚でも frameOutOfRange でも常に描画
    // ============================================================
      ArtifactCore::ProfileScope _profBase(
          "BasePass", ArtifactCore::ProfileCategory::Composite);
      auto accumSRV = renderPipeline.accumSRV();
      auto tempUAV = renderPipeline.tempUAV();
      auto layerRTV = renderPipeline.layerRTV();
      auto layerSRV = renderPipeline.layerSRV();
      auto layerFloatSRV = renderPipeline.layerFloatSRV();
      auto layerFloatUAV = renderPipeline.layerFloatUAV();

      // ==== オフスクリーン描画前の状態保存 ====
      const float origZoom = renderer_->getZoom();
      const FloatColor origClearColor = renderer_->getClearColor();
      float origPanX, origPanY;
      renderer_->getPan(origPanX, origPanY);
      const float origViewW = hostWidth_;
      const float origViewH = hostHeight_;
      auto* previewDepthDSV = static_cast<Diligent::ITextureView*>(
          previewRenderSlot.depthTargetView);
      renderer_->setOverrideDSV(previewDepthDSV);

      // -- 1: 背景を offscreen 座標系で準備 --
      // 背景矩形は一度 layerRTV に rasterize してから compute blend で
      // accum にシードする。これで accum/temp は Vulkan 互換の
      // linear storage image に保てる。
      renderer_->setViewportSize(rcw, rch);
      renderer_->setCanvasSize(cw, ch);
      renderer_->setZoom(origZoom);
      renderer_->setPan(origPanX, origPanY);
      renderer_->setViewportRect(rcw, rch);

      const FloatColor layerBgColor = comp->backgroundColor();
      if (compositionViewLog().isDebugEnabled()) {
        qCDebug(compositionViewLog)
            << "[CompositionView] background pass (gpu)"
            << "compSize=" << QSize(static_cast<int>(cw), static_cast<int>(ch))
            << "rtSize=" << QSize(static_cast<int>(rcw), static_cast<int>(rch))
            << "viewport="
            << QSize(static_cast<int>(origViewW), static_cast<int>(origViewH))
            << "zoom=" << origZoom << "pan=(" << origPanX << "," << origPanY << ")"
            << "bg="
            << QColor::fromRgbF(layerBgColor.r(), layerBgColor.g(),
                                layerBgColor.b(), layerBgColor.a())
            << "bgMode=" << static_cast<int>(backgroundMode)
            << "compositionSpaceApplied=" << true;
      }
      renderer_->setOverrideRTV(renderPipeline.accumRTV());
        renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
        renderer_->clear();
        renderer_->setClearColor(viewportClearColor_);
        renderer_->setOverrideRTV(nullptr);

      // Keep offscreen drawing in the same viewport transform as the editor.
      renderer_->setCanvasSize(cw, ch);
      renderer_->setZoom(origZoom);
      renderer_->setPan(origPanX, origPanY);

      // Seed accum with the composition background through the same
      // layerRTV -> LayerFloat -> blend path used by visible layers. This keeps
      // GPU blend frames consistent with fallback/RAM preview frames.
      renderer_->setOverrideRTV(layerRTV);
      renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
      renderer_->clear();
      drawCompositionBackgroundDirect(renderer_.get(), cw, ch, layerBgColor,
                                      backgroundMode, checkerboardTileSize_,
                                      cachedMayaGradientSprite_);
      renderer_->flush();
      renderer_->setOverrideRTV(nullptr);
      renderer_->unbindColorTargetsForCompute();

      if (layerBgColor.a() > 0.0f) {
        ++layerToFloatConvertCount;
        const bool convertedBackgroundToFloat = renderer_->convertLayerToFloat(
            blendPipeline_.get(), layerSRV, layerFloatUAV,
            static_cast<Diligent::Uint32>(renderPipeline.width()),
            static_cast<Diligent::Uint32>(renderPipeline.height()));
        Diligent::ITextureView *backgroundBlendSrc =
            convertedBackgroundToFloat ? layerFloatSRV : layerSRV;
        if (!convertedBackgroundToFloat) {
          qWarning() << "[CompositionView] background-to-float conversion failed; "
                        "falling back to legacy background SRV";
        }
        ++blendDispatchCount;
        if (renderer_->blendLayers(blendPipeline_.get(), backgroundBlendSrc,
                                   accumSRV, tempUAV,
                                   ArtifactCore::BlendMode::Normal, 1.0f)) {
          renderPipeline.swapAccumAndTemp();
          accumSRV = renderPipeline.accumSRV();
          tempUAV = renderPipeline.tempUAV();
        } else {
          ++blendFailureCount;
          qWarning() << "[CompositionView] background seed blend failed";
        }
      }

      basePassMs = markPhaseMs();

      // Already in composition-space offscreen coordinates; layer loop uses it
      // as-is.

      // -- 2: レイヤーブレンド（frameOutOfRange ならスキップ）--
      ArtifactCore::ProfileScope _profLayer(
          "LayerPass", ArtifactCore::ProfileCategory::Composite);
      if (!frameOutOfRange) {
        const DetailLevel lod = detailLevelFromZoom(origZoom);
        renderer_->setDetailLevel(static_cast<LODManager::DetailLevel>(
            lod)); // Pass LOD to renderer/effects
        for (const auto &layer : layers) {
          if (!isLayerEffectivelyVisible(layer)) {
            ++skipInvisibleCount;
            continue;
          }
          if (hasSoloLayer && !layer->isSolo()) {
            ++skipSoloCount;
            continue;
          }
          if (!layer->isActiveAt(currentFrame)) {
            ++skipInactiveCount;
            continue;
          }
          const QRectF layerBounds = layer->transformedBoundingBox();
          if (layerBounds.isValid() &&
              layerBounds.intersected(roiRect).isEmpty()) {
            ++skipRoiCount;
            continue;
          }

          // --- Feature 3: Layer Drawing Skip (LOD-based) ---
          // Skip rendering layers that are too small to be visible on screen.
          if (layerBounds.isValid()) {
            const float screenW =
                std::abs(static_cast<float>(layerBounds.width()) * origZoom);
            const float screenH =
                std::abs(static_cast<float>(layerBounds.height()) * origZoom);

            if (lod == DetailLevel::Low) {
              if (screenW < 8.0f || screenH < 8.0f) {
                ++skipLodCount;
                continue;
              }
            } else if (lod == DetailLevel::Medium) {
              if (screenW < 2.0f || screenH < 2.0f) {
                ++skipLodCount;
                continue;
              }
            }
          }
          // ------------------------------------------------

          ++drawnLayerCount;
          if (layerUsesSurfaceUploadForCompositionView(layer.get())) {
            ++surfaceUploadLayerCount;
          }
          if (layerHasCpuRasterizerWork(layer.get())) {
            ++cpuRasterLayerCount;
          }

          if (layerNeedsFrameSyncForCompositionView(layer.get())) {
            layer->goToFrame(currentFrame.framePosition());
          }
          const auto blendMode =
              ArtifactCore::toBlendMode(layer->layerBlendType());
          const float opacity = layer->opacity();
          if (opacity <= 0.0f) {
            ++opacityZeroCount;
            qCDebug(compositionViewLog)
                << "[LayerSkip] opacity <= 0"
                << "layer=" << layer->id().toString()
                << "layerName=" << layer->layerName()
                << "opacity=" << opacity
                << "rawOpacity=" << layer->opacity()
                << "hasSelection=" << hasSelection;
            continue;
          }
          ++compositedLayerCount;
          const int layerMaskCount = layer->maskCount();
          if (layerMaskCount > 0) {
            ++maskedLayerCount;
            totalMaskCount += layerMaskCount;
          }
          if (blendMode != ArtifactCore::BlendMode::Normal) {
            ++nonNormalBlendLayerCount;
          }

          // -- Adjustment Layer or Normal Layer? --
          renderer_->setOverrideRTV(layerRTV);
          if (layer->isAdjustmentLayer()) {
            renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
            renderer_->clear();
            // -- Adjustment Layer: Capture the background --
            // We draw the current composition result (accumSRV) into our layer
            // buffer. This makes the 'background' available as a source for
            // this layer's effects.
            const float savedZoom = renderer_->getZoom();
            float savedPanX = 0.0f;
            float savedPanY = 0.0f;
            renderer_->getPan(savedPanX, savedPanY);
            renderer_->setCanvasSize(rcw, rch);
            renderer_->setZoom(1.0f);
            renderer_->setPan(0.0f, 0.0f);
            renderer_->drawSprite(0.0f, 0.0f, rcw, rch, accumSRV, 1.0f);
            renderer_->setCanvasSize(cw, ch);
            renderer_->setZoom(savedZoom);
            renderer_->setPan(savedPanX, savedPanY);
          } else {
            renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
            renderer_->clear();
          }

          QString *dbgOut = &lastVideoDebug_;
          drawLayerForCompositionView(
              layer.get(), renderer_.get(), 1.0f, dbgOut, &surfaceCache_,
              gpuTextureCacheManager_.get(), currentFrame.framePosition(), true,
              lod, has3DCamera ? &cameraViewMatrix : nullptr,
              has3DCamera ? &cameraProjMatrix : nullptr, &matteResolver,
              &sceneLights);
          // Keep the command-buffer architecture, but make the graphics ->
          // compute boundary explicit. The blend pipeline samples layerSRV,
          // so pending draw packets must be submitted before dispatch.
          renderer_->flush();
          renderer_->setOverrideRTV(nullptr);

          // CS 実行前に RTV を解除
          renderer_->unbindColorTargetsForCompute();

          ++blendDispatchCount;
          bool convertedLayerToFloat = renderer_->convertLayerToFloat(
              blendPipeline_.get(), layerSRV, layerFloatUAV,
              static_cast<Diligent::Uint32>(renderPipeline.width()),
              static_cast<Diligent::Uint32>(renderPipeline.height()));
          Diligent::ITextureView *blendSrcSRV =
              convertedLayerToFloat ? layerFloatSRV : layerSRV;
          if (convertedLayerToFloat) {
            ++layerToFloatConvertCount;
          }
          if (!convertedLayerToFloat) {
            qWarning() << "[CompositionView] layer-to-float conversion failed; "
                          "falling back to legacy layer SRV"
                       << "layer=" << layer->id().toString()
                       << "layerName=" << layer->layerName()
                       << "mode=" << static_cast<int>(blendMode);
          }
          bool blendOk = renderer_->blendLayers(
              blendPipeline_.get(), blendSrcSRV, accumSRV, tempUAV, blendMode,
              opacity);
          if (!blendOk && blendMode != ArtifactCore::BlendMode::Normal) {
            ++blendRetryNormalCount;
            qWarning() << "[CompositionView] blend failed; retrying with Normal"
                       << "layer=" << layer->id().toString()
                       << "layerName=" << layer->layerName()
                       << "mode=" << static_cast<int>(blendMode)
                       << "opacity=" << opacity;
            ++blendDispatchCount;
            blendOk = renderer_->blendLayers(
                blendPipeline_.get(), blendSrcSRV, accumSRV, tempUAV,
                ArtifactCore::BlendMode::Normal, opacity);
          }
          if (!blendOk) {
            ++blendFailureCount;
            ++directBlendFallbackCount;
            qWarning() << "[CompositionView] blend failed; falling back to direct sprite"
                       << "layer=" << layer->id().toString()
                       << "layerName=" << layer->layerName()
                       << "mode=" << static_cast<int>(blendMode)
                       << "opacity=" << opacity;
            renderer_->setOverrideRTV(renderPipeline.accumRTV());
            renderer_->drawSprite(0.0f, 0.0f, cw, ch, layerSRV, opacity);
            renderer_->flush();
            renderer_->setOverrideRTV(nullptr);
            continue;
          }
          if (blendMaskLayerNotes.size() < 4 &&
              (blendMode != ArtifactCore::BlendMode::Normal ||
               layerMaskCount > 0)) {
            blendMaskLayerNotes
                << QStringLiteral("%1:blend=%2 opacity=%3 masks=%4")
                       .arg(layer->layerName(),
                            ArtifactCore::BlendModeUtils::toString(blendMode),
                            QString::number(opacity, 'f', 3))
                       .arg(layerMaskCount);
          }
          renderPipeline.swapAccumAndTemp();
          accumSRV = renderPipeline.accumSRV();
          tempUAV = renderPipeline.tempUAV();
        }
      }

      // ==== オフスクリーン描画後: ホスト viewport に戻す ====
      renderer_->setViewportRect(origViewW, origViewH);

      // -- 3: main FB に背景を描画してから accum を blit --
      // MayaGradient と Checkerboard はビューポート全体を覆う背景として main FB
      // に描く。 accum を SRC_ALPHA でブリットすると、composition 外は alpha=0
      // のため main FB
      // の背景（グラディエント/チェッカーボード/ビューポートクリアカラー）が
      // 透けて見える。composition 内は bgColor（不透明）で遮蔽される。
      renderer_->setUseExternalMatrices(false);
      renderer_->resetGizmoCameraMatrices();
      renderer_->reset3DCameraMatrices();
      const bool transparentCompositionBackground = layerBgColor.a() < 0.999f;
      if (backgroundMode == CompositionBackgroundMode::MayaGradient) {
        drawViewportMayaGradientBackground(renderer_.get(), origViewW,
                                           origViewH, bgColor,
                                           cachedMayaGradientSprite_);
      } else if (backgroundMode == CompositionBackgroundMode::Checkerboard ||
                 transparentCompositionBackground) {
        drawViewportCheckerboardBackground(renderer_.get(), origViewW,
                                           origViewH,
                                           checkerboardTileSize_);
      }
      renderer_->setCanvasSize(cw, ch);
      renderer_->setZoom(origZoom);
      renderer_->setPan(origPanX, origPanY);
      drawCompositionBackgroundDirect(renderer_.get(), cw, ch, layerBgColor,
                                      backgroundMode, checkerboardTileSize_,
                                      cachedMayaGradientSprite_);

      // -- 4: オフスクリーン RT を viewport-space のまま描画。
      // blend pipeline output is premultiplied, while the sprite present path
      // expects straight alpha. Convert once before the final blit.
      Diligent::ITextureView* finalPresentSRV = accumSRV;
      ++layerToFloatConvertCount;
      const bool convertedAccumToStraight = renderer_->convertLayerToFloat(
          blendPipeline_.get(), accumSRV, layerFloatUAV,
          static_cast<Diligent::Uint32>(renderPipeline.width()),
          static_cast<Diligent::Uint32>(renderPipeline.height()));
      if (convertedAccumToStraight) {
        finalPresentSRV = layerFloatSRV;
      } else {
        qWarning() << "[CompositionView] accum-to-straight conversion failed; "
                      "presenting premultiplied accum directly";
      }
      ramPreviewReadbackSRV = finalPresentSRV;
      lastPresentedReadbackSRV_ = finalPresentSRV;
      renderer_->setCanvasSize(origViewW, origViewH);
      renderer_->setZoom(1.0f);
      renderer_->setPan(0.0f, 0.0f);
      renderer_->drawSprite(0.0f, 0.0f, rcw, rch, finalPresentSRV,
                            1.0f);
      // コンポジションのキャンバス座標系に戻す
      if (compositionRenderer_) {
        compositionRenderer_->SetCompositionSize(cw, ch);
        compositionRenderer_->ApplyCompositionSpace();
      } else {
        renderer_->setCanvasSize(cw, ch);
      }
      renderer_->setZoom(origZoom);
      renderer_->setPan(origPanX, origPanY);
      renderer_->setClearColor(
          origClearColor); // Bug A fix: GPUパス前に保存したクリアカラーを復元
      renderer_->setOverrideDSV(nullptr);
      layerPassMs = markPhaseMs();
    } else {
      // === Fallback path (GPU パイプラインなし) ===
      renderCrashTrace("render-branch-fallback", renderFrameCounter_);
      renderer_->setViewportRect(viewportW, viewportH);
      const float prevZoom = renderer_->getZoom();
      float prevPanX = 0.0f;
      float prevPanY = 0.0f;
      renderer_->getPan(prevPanX, prevPanY);
      renderer_->setUseExternalMatrices(false);
      renderer_->resetGizmoCameraMatrices();
      renderer_->reset3DCameraMatrices();
      const bool transparentCompositionBackground = layerBgColor.a() < 0.999f;
      if (backgroundMode == CompositionBackgroundMode::MayaGradient) {
        renderer_->setCanvasSize(viewportW, viewportH);
        renderer_->setZoom(1.0f);
        renderer_->setPan(0.0f, 0.0f);
        drawViewportMayaGradientBackground(renderer_.get(), viewportW,
                                           viewportH, layerBgColor,
                                           cachedMayaGradientSprite_);
        renderer_->setCanvasSize(cw, ch);
        renderer_->setZoom(prevZoom);
        renderer_->setPan(prevPanX, prevPanY);
      }
      renderer_->setCanvasSize(cw, ch); // キャンバスを Composition Space に設定
      if (backgroundMode == CompositionBackgroundMode::Checkerboard ||
          transparentCompositionBackground) {
        renderer_->setCanvasSize(viewportW, viewportH);
        renderer_->setZoom(1.0f);
        renderer_->setPan(0.0f, 0.0f);
        drawViewportCheckerboardBackground(renderer_.get(), viewportW,
                                           viewportH,
                                           checkerboardTileSize_);
        renderer_->setCanvasSize(cw, ch);
        renderer_->setZoom(prevZoom);
        renderer_->setPan(prevPanX, prevPanY);
      }
      // Composition Space で直接 fill する（viewport-space 変換不要）
      drawCompositionBackgroundDirect(renderer_.get(), cw, ch, layerBgColor,
                                      backgroundMode, checkerboardTileSize_,
                                      cachedMayaGradientSprite_);
      lastPresentedReadbackSRV_ = nullptr;
      renderer_->setUseExternalMatrices(false);
      renderer_->resetGizmoCameraMatrices();
      renderer_->reset3DCameraMatrices();
      renderer_->setCanvasSize(cw, ch);
      renderer_->setZoom(prevZoom);
      renderer_->setPan(prevPanX, prevPanY);
      const bool showGridDebug =
          owner->isLineDebugKindVisible(LineDebugKind::Grid);
      if (showGrid_ && showGridDebug) {
        renderer_->drawGrid(0, 0, cw, ch,
                            std::max(1.0f, gridSettings_.majorInterval),
                            gridSettings_.majorStyle.thickness,
                            gridSettings_.majorColor);
        const FloatColor thirdsColor{0.32f, 0.86f, 1.0f, 0.52f};
        const FloatColor thirdsShadow{0.0f, 0.0f, 0.0f, 0.26f};
        const float thirdX1 = cw / 3.0f;
        const float thirdX2 = cw * 2.0f / 3.0f;
        const float thirdY1 = ch / 3.0f;
        const float thirdY2 = ch * 2.0f / 3.0f;
        drawTaggedSolidLine(renderer_.get(), {thirdX1, 0.0f}, {thirdX1, ch},
                            thirdsShadow, 2.0f, showGridDebug);
        drawTaggedSolidLine(renderer_.get(), {thirdX2, 0.0f}, {thirdX2, ch},
                            thirdsShadow, 2.0f, showGridDebug);
        drawTaggedSolidLine(renderer_.get(), {0.0f, thirdY1}, {cw, thirdY1},
                            thirdsShadow, 2.0f, showGridDebug);
        drawTaggedSolidLine(renderer_.get(), {0.0f, thirdY2}, {cw, thirdY2},
                            thirdsShadow, 2.0f, showGridDebug);
        drawTaggedSolidLine(renderer_.get(), {thirdX1, 0.0f}, {thirdX1, ch},
                            thirdsColor, 1.0f, showGridDebug);
        drawTaggedSolidLine(renderer_.get(), {thirdX2, 0.0f}, {thirdX2, ch},
                            thirdsColor, 1.0f, showGridDebug);
        drawTaggedSolidLine(renderer_.get(), {0.0f, thirdY1}, {cw, thirdY1},
                            thirdsColor, 1.0f, showGridDebug);
        drawTaggedSolidLine(renderer_.get(), {0.0f, thirdY2}, {cw, thirdY2},
                            thirdsColor, 1.0f, showGridDebug);
      }

      if (compositionViewLog().isDebugEnabled()) {
        const auto tl = renderer_->canvasToViewport({0.0f, 0.0f});
        const auto br = renderer_->canvasToViewport({cw, ch});
        const float rectX = std::min(tl.x, br.x);
        const float rectY = std::min(tl.y, br.y);
        const float rectW = std::fabs(br.x - tl.x);
        const float rectH = std::fabs(br.y - tl.y);

        qCDebug(compositionViewLog)
            << "[CompositionView] fallback comp rect in viewport"
            << QRectF(rectX, rectY, rectW, rectH);

        const float prevZoom = renderer_->getZoom();
        float prevPanX = 0.0f;
        float prevPanY = 0.0f;
        renderer_->getPan(prevPanX, prevPanY);

        renderer_->setCanvasSize(viewportW, viewportH);
        renderer_->setZoom(1.0f);
        renderer_->setPan(0.0f, 0.0f);

        renderer_->drawRectLocal(12.0f, 12.0f, 72.0f, 72.0f,
                                 {1.0f, 0.0f, 1.0f, 0.9f}, 1.0f);
        renderer_->drawRectOutline(rectX, rectY, rectW, rectH,
                                   {1.0f, 0.2f, 0.2f, 1.0f});

        renderer_->setCanvasSize(cw, ch);
        renderer_->setZoom(prevZoom);
        renderer_->setPan(prevPanX, prevPanY);
      }
      basePassMs = markPhaseMs();

      if (!frameOutOfRange) {
        const DetailLevel lod = detailLevelFromZoom(renderer_->getZoom());
        renderer_->setDetailLevel(static_cast<LODManager::DetailLevel>(
            lod)); // Pass LOD to renderer/effects

        for (const auto &layer : layers) {
          if (!isLayerEffectivelyVisible(layer)) {
            ++skipInvisibleCount;
            continue;
          }
          if (hasSoloLayer && !layer->isSolo()) {
            ++skipSoloCount;
            continue;
          }
          if (!layer->isActiveAt(currentFrame)) {
            ++skipInactiveCount;
            continue;
          }

          // === 段階 2: ROI 計算 ===
          const QRectF layerBounds = layer->transformedBoundingBox();
          const QRectF intersected = layerBounds.intersected(roiRect);

          // --- Feature 3: Layer Drawing Skip (LOD-based) ---
          // Skip rendering layers that are too small to be visible on screen.
          if (layerBounds.isValid()) {
            const auto tl = renderer_->canvasToViewport(
                {(float)layerBounds.left(), (float)layerBounds.top()});
            const auto br = renderer_->canvasToViewport(
                {(float)layerBounds.right(), (float)layerBounds.bottom()});
            const float screenW = std::abs(br.x - tl.x);
            const float screenH = std::abs(br.y - tl.y);

            if (lod == DetailLevel::Low) {
              if (screenW < 8.0f || screenH < 8.0f) {
                ++skipLodCount;
                continue;
              }
            } else if (lod == DetailLevel::Medium) {
              if (screenW < 2.0f || screenH < 2.0f) {
                ++skipLodCount;
                continue;
              }
            }
          }
          // ------------------------------------------------

          // === 段階 3: 空 ROI スキップ ===
          if (intersected.isEmpty()) {
            ++skipRoiCount;
            continue; // 画面外レイヤーをスキップ
          }

          ++drawnLayerCount;
          if (layerUsesSurfaceUploadForCompositionView(layer.get())) {
            ++surfaceUploadLayerCount;
          }
          if (layerHasCpuRasterizerWork(layer.get())) {
            ++cpuRasterLayerCount;
          }
          if (layerNeedsFrameSyncForCompositionView(layer.get())) {
            layer->goToFrame(currentFrame.framePosition());
          }
          const auto blendMode =
              ArtifactCore::toBlendMode(layer->layerBlendType());
          const float opacity = layer->opacity();
          if (opacity <= 0.0f) {
            ++opacityZeroCount;
            continue;
          }
          ++compositedLayerCount;
          const int layerMaskCount = layer->maskCount();
          if (layerMaskCount > 0) {
            ++maskedLayerCount;
            totalMaskCount += layerMaskCount;
          }
          if (blendMode != ArtifactCore::BlendMode::Normal) {
            ++nonNormalBlendLayerCount;
          }
          if (blendMaskLayerNotes.size() < 4 &&
              (blendMode != ArtifactCore::BlendMode::Normal ||
               layerMaskCount > 0)) {
            blendMaskLayerNotes
                << QStringLiteral("%1:blend=%2 opacity=%3 masks=%4")
                       .arg(layer->layerName(),
                            ArtifactCore::BlendModeUtils::toString(blendMode),
                            QString::number(opacity, 'f', 3))
                       .arg(layerMaskCount);
          }
          QString *dbgOut =
              QLoggingCategory::defaultCategory()->isDebugEnabled()
                  ? &lastVideoDebug_
                  : nullptr;
          drawLayerForCompositionView(
              layer.get(), renderer_.get(), opacity, dbgOut, &surfaceCache_,
              gpuTextureCacheManager_.get(), currentFrame.framePosition(),
              false, lod, has3DCamera ? &cameraViewMatrix : nullptr,
              has3DCamera ? &cameraProjMatrix : nullptr, &matteResolver,
              &sceneLights);

          // === 段階 7: ROI デバッグ表示 ===
          if (debugMode_) {
            // ROI を赤い枠で表示
            const bool showDebugProbe =
                owner->isLineDebugKindVisible(LineDebugKind::DebugProbe);
            drawTaggedRectOutline(renderer_.get(), intersected,
                                  FloatColor{1.0f, 0.0f, 0.0f, 1.0f},
                                  showDebugProbe);
          }
        }
      }
      layerPassMs = markPhaseMs();
    }

    // Temporarily disable motion path overlay while debugging stray
    // frame-like rectangles in the viewport.
    // if (renderer_ && showMotionPathOverlay_ && comp &&
    //     !selectedLayerId_.isNil()) {
    //   ArtifactCore::ProfileScope _profMotion1(
    //       "MotionPath1", ArtifactCore::ProfileCategory::Render);
    //   if (auto selectedLayer = comp->layerById(selectedLayerId_)) {
    //     const auto motionPath = buildMotionPathSamples(selectedLayer, comp);
    //     QVector<MotionPathSample> keyframes;
    //     keyframes.reserve(motionPath.size());
    //     const MotionPathSample *currentSample = nullptr;
    //     for (const auto &sample : motionPath) {
    //       if (sample.kind == MotionPathSampleKind::Current) {
    //         currentSample = &sample;
    //       } else {
    //         keyframes.push_back(sample);
    //       }
    //     }
    //
    //     auto samePoint = [](const QPointF &a, const QPointF &b) {
    //       return qFuzzyCompare(a.x(), b.x()) && qFuzzyCompare(a.y(), b.y());
    //     };
    //
    //     const bool currentMatchesKeyframe =
    //         currentSample &&
    //         std::any_of(keyframes.begin(), keyframes.end(),
    //                     [&](const MotionPathSample &sample) {
    //                       return samePoint(sample.position,
    //                                        currentSample->position);
    //                     });
    //
    //     const bool hasMotion = keyframes.size() >= 2 ||
    //                            (currentSample != nullptr &&
    //                             !keyframes.empty() && !currentMatchesKeyframe);
    //
    //     if (hasMotion) {
    //       const FloatColor pathColor{0.95f, 0.65f, 0.22f, 0.85f};
    //       const FloatColor keyColor{1.0f, 0.92f, 0.28f, 1.0f};
    //       const FloatColor currentColor{0.28f, 0.9f, 1.0f, 1.0f};
    //       QPointF prev = keyframes[0].position;
    //       for (int i = 1; i < keyframes.size(); ++i) {
    //         const QPointF cur = keyframes[i].position;
    //         renderer_->drawSolidLine(
    //             {static_cast<float>(prev.x()), static_cast<float>(prev.y())},
    //             {static_cast<float>(cur.x()), static_cast<float>(cur.y())},
    //             pathColor, 1.2f);
    //         prev = cur;
    //       }
    //       for (const auto &sample : keyframes) {
    //         renderer_->drawPoint(static_cast<float>(sample.position.x()),
    //                              static_cast<float>(sample.position.y()), 6.0f,
    //                              keyColor);
    //       }
    //       if (currentSample && !currentMatchesKeyframe) {
    //         renderer_->drawPoint(
    //             static_cast<float>(currentSample->position.x()),
    //             static_cast<float>(currentSample->position.y()), 4.0f,
    //             currentColor);
    //       }
    //     }
    //   }
    // }

    if (showGizmoOverlay_ && gizmo_) {
      ArtifactCore::ProfileScope _profGizmo(
          "GizmoMask", ArtifactCore::ProfileCategory::Render);
      auto selectedLayer = (!selectedLayerId_.isNil() && comp)
                               ? comp->layerById(selectedLayerId_)
                               : ArtifactAbstractLayerPtr{};
      if (selectedLayer && isLayerEffectivelyVisible(selectedLayer)) {
        gizmo_->setMode(gizmoMode_);
        if (!selectedLayer->is3D()) {
          ArtifactCore::ProfileScope _profG2D(
              "Gizmo2D", ArtifactCore::ProfileCategory::Render);
          sync2DGizmosForLayer(selectedLayer);
          if (layerUsesTextGizmo(selectedLayer) && textGizmo_) {
            ArtifactCore::ProfileScope _profG2DDrawCall(
                "Gizmo2DDrawCall", ArtifactCore::ProfileCategory::Render);
            textGizmo_->draw(renderer_.get());
          } else if (gizmo_) {
            ArtifactCore::ProfileScope _profG2DDrawCall(
                "Gizmo2DDrawCall", ArtifactCore::ProfileCategory::Render);
            const bool showDuringDrag =
                ArtifactCore::ArtifactAppSettings::instance()
                    ? ArtifactCore::ArtifactAppSettings::instance()
                          ->compositionShowGizmoDuringDrag()
                    : false;
            const bool suppressMoveDragVisual =
                gizmo_->isDragging() &&
                gizmo_->activeHandle() == TransformGizmo::HandleType::Move &&
                !showDuringDrag;
            if (!suppressMoveDragVisual) {
              gizmo_->draw(renderer_.get());
            }
          }
        } else {
          sync2DGizmosForLayer(nullptr);
        }

        if (gizmo3D_ && selectedLayer->is3D()) {
          ArtifactCore::ProfileScope _profG3D(
              "Gizmo3D", ArtifactCore::ProfileCategory::Render);
          syncGizmo3DFromLayer(selectedLayer);
          const float viewportW =
              hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
          const float viewportH =
              hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
          if (viewportW > 0.0f && viewportH > 0.0f) {
            float panX = 0.0f;
            float panY = 0.0f;
            float zoom = 1.0f;
            renderer_->getPan(panX, panY);
            zoom = std::max(0.001f, renderer_->getZoom());

            QMatrix4x4 view;
            view.translate(panX, panY, 0.0f);
            view.scale(zoom, zoom, 1.0f);

            QMatrix4x4 proj;
            proj.ortho(0.0f, viewportW, viewportH, 0.0f, -1000.0f, 1000.0f);

            {
              ArtifactCore::ProfileScope _profG3DDraw(
                  "Gizmo3DDraw", ArtifactCore::ProfileCategory::Render);
              gizmo3D_->draw(renderer_.get(), view, proj);
            }
            {
              ArtifactCore::ProfileScope _profG3DFlush(
                  "Gizmo3DFlush", ArtifactCore::ProfileCategory::Render);
              renderer_->flushGizmo3D();
            }
          } else {
            {
              ArtifactCore::ProfileScope _profG3DDraw(
                  "Gizmo3DDraw", ArtifactCore::ProfileCategory::Render);
              gizmo3D_->draw(renderer_.get(), renderer_->getViewMatrix(),
                             renderer_->getProjectionMatrix());
            }
            {
              ArtifactCore::ProfileScope _profG3DFlush(
                  "Gizmo3DFlush", ArtifactCore::ProfileCategory::Render);
              renderer_->flushGizmo3D();
            }
          }
        }

        // Tracker Gizmo overlay (independent of selection)
        {
          const auto* tm = ArtifactApplicationManager::instance()
                               ? ArtifactApplicationManager::instance()->toolManager()
                               : nullptr;
          if (trackerGizmo_ && tm && tm->activeTool() == ToolType::TrackPoint) {
            ArtifactCore::ProfileScope _profTracker(
                "TrackerGizmo", ArtifactCore::ProfileCategory::Render);
            trackerGizmo_->draw(renderer_.get());
          }
        }

        // Mask Overlay Drawing
        const int maskCount = selectedLayer->maskCount();
        if (maskCount > 0 && renderer_ &&
            selectedLayer->isActiveAt(currentFrame)) {
          ArtifactCore::ProfileScope _profMask(
              "MaskDraw", ArtifactCore::ProfileCategory::Render);
          const QTransform globalTransform =
              selectedLayer->getGlobalTransform();
          const FloatColor maskPointShadowColor = {0.0f, 0.0f, 0.0f, 0.42f};
          const FloatColor maskPointColor = {0.97f, 0.99f, 1.0f, 1.0f};
          const FloatColor hoverColor = {1.0f, 0.76f, 0.28f, 1.0f};
          const FloatColor dragColor = {1.0f, 0.40f, 0.24f, 1.0f};
          const FloatColor handleStrokeColor = {0.82f, 0.92f, 1.0f, 0.88f};
          const FloatColor handlePointColor = {0.70f, 0.90f, 1.0f, 0.95f};
          const FloatColor handleHoverColor = {1.0f, 0.78f, 0.32f, 1.0f};
          const FloatColor handleDragColor = {1.0f, 0.44f, 0.24f, 1.0f};
          const FloatColor activeMaskStrokeColor = {1.0f, 0.86f, 0.42f, 0.60f};
          const FloatColor activeMaskPointColor = {1.0f, 0.90f, 0.54f, 1.0f};
          const FloatColor activeMaskHandleColor = {1.0f, 0.82f, 0.40f, 1.0f};
          constexpr float maskStrokeWidth = 5.2f;
          constexpr float handleStrokeWidth = 3.0f;
          const bool showMaskPath =
              owner->isLineDebugKindVisible(LineDebugKind::MaskPath);
          const bool showMaskHandle =
              owner->isLineDebugKindVisible(LineDebugKind::MaskHandle);

          for (int m = 0; m < maskCount; ++m) {
            LayerMask mask = selectedLayer->mask(m);
            if (!mask.isEnabled())
              continue;
            const bool isActiveMask =
                (isDraggingMaskHandle_ && draggingMaskIndex_ == m) ||
                (isDraggingVertex_ && draggingMaskIndex_ == m) ||
                (hoveredMaskIndex_ == m);

            for (int p = 0; p < mask.maskPathCount(); ++p) {
              MaskPath path = mask.maskPath(p);
              const int vertexCount = path.vertexCount();
              if (vertexCount == 0)
                continue;

              struct VertexMarker {
                Detail::float2 pos;
                FloatColor color;
                float radius;
              };
              std::vector<VertexMarker> markers;
              markers.reserve(static_cast<size_t>(vertexCount));

              Detail::float2 lastCanvasPos;
              {
                ArtifactCore::ProfileScope _profMaskLines(
                    "MaskDrawLines", ArtifactCore::ProfileCategory::Render);
                for (int v = 0; v < vertexCount; ++v) {
                  MaskVertex vertex = path.vertex(v);
                  QPointF canvasPos = globalTransform.map(vertex.position);
                  Detail::float2 currentCanvasPos = {(float)canvasPos.x(),
                                                     (float)canvasPos.y()};

                  const QPointF inHandlePos = globalTransform.map(vertex.position + vertex.inTangent);
                  const QPointF outHandlePos = globalTransform.map(vertex.position + vertex.outTangent);
                  const Detail::float2 inHandleCanvas = {(float)inHandlePos.x(), (float)inHandlePos.y()};
                  const Detail::float2 outHandleCanvas = {(float)outHandlePos.x(), (float)outHandlePos.y()};

                  if (showMaskHandle && vertex.inTangent != QPointF(0, 0)) {
                    const FloatColor strokeColor =
                        isActiveMask ? activeMaskStrokeColor : handleStrokeColor;
                    renderer_->drawThickLineLocal(currentCanvasPos, inHandleCanvas,
                                                  handleStrokeWidth, strokeColor);
                    FloatColor handleColor = handlePointColor;
                    if (isActiveMask) {
                      handleColor = activeMaskHandleColor;
                    }
                    if (isDraggingMaskHandle_ && draggingMaskIndex_ == m && draggingPathIndex_ == p &&
                        draggingVertexIndex_ == v && draggingMaskHandleType_ == static_cast<int>(MaskHandleType::InTangent)) {
                      handleColor = handleDragColor;
                    } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p && hoveredVertexIndex_ == v &&
                               hoveredMaskHandleType_ == static_cast<int>(MaskHandleType::InTangent)) {
                      handleColor = handleHoverColor;
                    }
                    drawMaskSquareMarker(renderer_.get(), inHandleCanvas, 7.0f,
                                         handleColor, &maskPointShadowColor,
                                         4.0f);
                  }
                  if (showMaskHandle && vertex.outTangent != QPointF(0, 0)) {
                    const FloatColor strokeColor =
                        isActiveMask ? activeMaskStrokeColor : handleStrokeColor;
                    renderer_->drawThickLineLocal(currentCanvasPos, outHandleCanvas,
                                                  handleStrokeWidth, strokeColor);
                    FloatColor handleColor = handlePointColor;
                    if (isActiveMask) {
                      handleColor = activeMaskHandleColor;
                    }
                    if (isDraggingMaskHandle_ && draggingMaskIndex_ == m && draggingPathIndex_ == p &&
                        draggingVertexIndex_ == v && draggingMaskHandleType_ == static_cast<int>(MaskHandleType::OutTangent)) {
                      handleColor = handleDragColor;
                    } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p && hoveredVertexIndex_ == v &&
                               hoveredMaskHandleType_ == static_cast<int>(MaskHandleType::OutTangent)) {
                      handleColor = handleHoverColor;
                    }
                    drawMaskSquareMarker(renderer_.get(), outHandleCanvas, 7.0f,
                                         handleColor, &maskPointShadowColor,
                                         4.0f);
                  }

                  if (showMaskPath && v > 0) {
                    const MaskVertex previousVertex = path.vertex(v - 1);
                    const QVector<QPointF> samples =
                        maskSegmentPolyline(previousVertex, vertex, 18);
                    for (int i = 1; i < samples.size(); ++i) {
                      const QPointF a = globalTransform.map(samples[i - 1]);
                      const QPointF b = globalTransform.map(samples[i]);
                      if (isActiveMask) {
                        renderer_->drawThickLineLocal(
                            {static_cast<float>(a.x()), static_cast<float>(a.y())},
                            {static_cast<float>(b.x()), static_cast<float>(b.y())},
                            maskStrokeWidth + 2.0f, activeMaskStrokeColor);
                      }
                      renderer_->drawThickLineLocal(
                          {static_cast<float>(a.x()),
                           static_cast<float>(a.y())},
                          {static_cast<float>(b.x()),
                           static_cast<float>(b.y())},
                          maskStrokeWidth, handleStrokeColor);
                    }
                  }

                  FloatColor currentColor = maskPointColor;
                  float currentPointRadius = 14.0f;

                  if (isDraggingVertex_ && draggingMaskIndex_ == m &&
                      draggingPathIndex_ == p && draggingVertexIndex_ == v) {
                    currentColor = dragColor;
                    currentPointRadius = 17.0f;
                  } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p &&
                             hoveredVertexIndex_ == v) {
                    currentColor = hoverColor;
                    currentPointRadius = 17.0f;
                  } else if (isActiveMask) {
                    currentColor = activeMaskPointColor;
                    currentPointRadius = 16.0f;
                  }

                  markers.push_back(
                      {currentCanvasPos, currentColor, currentPointRadius});
                  lastCanvasPos = currentCanvasPos;
               }
              }

              if (showMaskPath && path.isClosed() && vertexCount > 1) {
                MaskVertex firstVertex = path.vertex(0);
                MaskVertex lastVertex = path.vertex(vertexCount - 1);
                const QVector<QPointF> samples =
                    maskSegmentPolyline(lastVertex, firstVertex, 18);
                for (int i = 1; i < samples.size(); ++i) {
                  const QPointF a = globalTransform.map(samples[i - 1]);
                  const QPointF b = globalTransform.map(samples[i]);
                  if (isActiveMask) {
                    renderer_->drawThickLineLocal(
                        {static_cast<float>(a.x()), static_cast<float>(a.y())},
                        {static_cast<float>(b.x()), static_cast<float>(b.y())},
                        maskStrokeWidth + 2.0f, activeMaskStrokeColor);
                  }
                  renderer_->drawThickLineLocal(
                      {static_cast<float>(a.x()), static_cast<float>(a.y())},
                      {static_cast<float>(b.x()), static_cast<float>(b.y())},
                      maskStrokeWidth, handleStrokeColor);
                }
              }

              {
                ArtifactCore::ProfileScope _profMaskPoints(
                    "MaskDrawPoints", ArtifactCore::ProfileCategory::Render);
                if (showMaskHandle) {
                  for (const auto &marker : markers) {
                    drawMaskSquareMarker(renderer_.get(), marker.pos,
                                          std::max(5.5f, marker.radius * 0.42f),
                                          marker.color, &maskPointShadowColor,
                                          3.0f);
                  }
               }
              }
              }
            }
          }

        }

        if (pendingMaskCreation_ &&
            pendingMaskLayerId_ == selectedLayer->id() &&
            pendingMaskPath_.vertexCount() > 0) {
          const MaskPath &path = pendingMaskPath_;
          const int vertexCount = path.vertexCount();
          const FloatColor pendingLineColor = {0.82f, 0.92f, 1.0f, 0.88f};
          const FloatColor pendingPointShadowColor = {0.0f, 0.0f, 0.0f, 0.36f};
          const FloatColor pendingPointColor = {0.84f, 0.98f, 1.0f, 0.88f};
          const QTransform globalTransform = selectedLayer->getGlobalTransform();
          constexpr float pendingStrokeWidth = 5.8f;

          Detail::float2 lastCanvasPos;
          for (int v = 0; v < vertexCount; ++v) {
            const MaskVertex vertex = path.vertex(v);
            const QPointF canvasPos = globalTransform.map(vertex.position);
            const Detail::float2 currentCanvasPos = {
                static_cast<float>(canvasPos.x()),
                static_cast<float>(canvasPos.y())};
            if (v > 0) {
              const MaskVertex previousVertex = path.vertex(v - 1);
              const QVector<QPointF> samples =
                  maskSegmentPolyline(previousVertex, vertex, 18);
              for (int i = 1; i < samples.size(); ++i) {
                const QPointF a = globalTransform.map(samples[i - 1]);
                const QPointF b = globalTransform.map(samples[i]);
                renderer_->drawThickLineLocal(
                    {static_cast<float>(a.x()), static_cast<float>(a.y())},
                    {static_cast<float>(b.x()), static_cast<float>(b.y())},
                    pendingStrokeWidth, pendingLineColor);
              }
            }
            drawMaskSquareMarker(renderer_.get(), currentCanvasPos, 7.5f,
                                 pendingPointColor, &pendingPointShadowColor,
                                 4.0f);
            lastCanvasPos = currentCanvasPos;
          }
          if (penToolPreviewVisible_ && penMaskPreviewValid_) {
            const FloatColor previewLineShadowColor = {0.0f, 0.0f, 0.0f, 0.20f};
            const FloatColor previewLineColor = {0.50f, 0.95f, 1.0f, 0.58f};
            renderer_->drawThickLineLocal(lastCanvasPos,
                                          penMaskPreviewCanvasPos_, 4.0f,
                                          previewLineShadowColor);
            renderer_->drawThickLineLocal(lastCanvasPos,
                                          penMaskPreviewCanvasPos_, 2.0f,
                                          previewLineColor);
          }
        }

        if (penToolPreviewVisible_ && penMaskPreviewValid_) {
          const bool closingSoon =
              pendingMaskCreation_ &&
              pendingMaskLayerId_ == selectedLayer->id() &&
              pendingMaskPath_.vertexCount() >= 3;
          const FloatColor previewShadowColor = {0.0f, 0.0f, 0.0f, 0.30f};
          const FloatColor previewColor = closingSoon
                                              ? FloatColor{1.0f, 0.88f, 0.46f, 0.95f}
                                              : FloatColor{0.82f, 0.97f, 1.0f, 0.92f};
          drawMaskSquareMarker(renderer_.get(), penMaskPreviewCanvasPos_, 6.0f,
                               previewColor, &previewShadowColor, 4.0f);
        }
      }

    // M-CE Phase 2: ピクセルグリッド自動表示 (ズーム 800% 以上でフェードイン)
    if (renderer_ && renderer_->getZoom() >= 8.0f) {
      const float zoom = renderer_->getZoom();
      const float alpha = std::clamp((zoom - 8.0f) / 8.0f, 0.0f, 0.5f) * 0.4f;
      if (alpha > 0.0f && comp) {
        const auto compSize = comp->settings().compositionSize();
        const float compW =
            static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);
        const float compH = static_cast<float>(
            compSize.height() > 0 ? compSize.height() : 1080);
        renderer_->drawGrid(0.0f, 0.0f, compW, compH, 1.0f, 1.0f / zoom,
                            {0.6f, 0.6f, 0.6f, alpha});
      }
    }

    if (renderer_) {
      // Reset to composition space with the CURRENT viewport zoom/pan so that
      // overlays (bounding box, motion path) are drawn at the correct position.
      // Using hardcoded zoom=1/pan=0 here placed the bounding box at wrong
      // screen coordinates (appeared as an orange L-shape at top-left corner).
      renderer_->setUseExternalMatrices(false);
      renderer_->resetGizmoCameraMatrices();
      renderer_->reset3DCameraMatrices();
      renderer_->setCanvasSize(cw, ch);
      // zoom and pan are already restored to origZoom/origPan by the render
      // path above — do NOT override them here.
    }

    if (renderer_ && !selectedIds.isEmpty()) {
      // Reuse the layer list already fetched above; avoids a second allLayer()
      // call.
      const auto &layersForOverlay = layers;

      // M-UI-6 Composition Motion Path Overlay
      // Guard: only run when the overlay is enabled — the 300-iteration
      // getGlobalTransformAt() loop is the main >1000ms bottleneck.
      if (comp && showMotionPathOverlay_) {
        ArtifactCore::ProfileScope _profMotion(
            "MotionPath", ArtifactCore::ProfileCategory::Render);
        const float zoom = renderer_->getZoom();
        const float invZoom = zoom > 0.0001f ? 1.0f / zoom : 1.0f;
        for (const auto &layer : layersForOverlay) {
          if (!layer || !isLayerSelected(selectedIds, layer))
            continue;

          const auto &t3d = layer->transform3D();
          const size_t posKeyCount = t3d.getPositionKeyFrameCount();
          if (posKeyCount == 0) {
            motionPathCache_.valid = false;
            continue;
          }

          auto posTimes = t3d.getPositionKeyFrameTimes();
          if (posTimes.empty()) {
            motionPathCache_.valid = false;
            continue;
          }

          int minFrame = static_cast<int>(posTimes.front().value());
          int maxFrame = static_cast<int>(posTimes.back().value());
          const int currentFrameNum = currentFrame.framePosition();
          const bool hasPathSegment = posTimes.size() >= 2;

          // Limit drawing range for performance (±300 frames around playhead).
          minFrame = std::max(minFrame, currentFrameNum - 300);
          maxFrame = std::min(maxFrame, currentFrameNum + 300);

          if (minFrame >= maxFrame)
            continue;

          const int64_t rate = posTimes.front().scale();
          const FloatColor pathColor{0.9f, 0.4f, 0.8f, 0.9f};
          const float lineThickness = std::max(1.0f, 1.5f * invZoom);
          const float dotRadius = std::max(1.5f, 2.5f * invZoom);

          // --- Motion path cache -------------------------------------------
          // Key: (layerId, currentFrameNum, overlaySerial).
          // On a cache miss re-sample all path points; on a hit skip all
          // getGlobalTransformAt() calls (the hot loop that caused >1000ms).
          const bool cacheHit =
              motionPathCache_.valid &&
              motionPathCache_.layerId == layer->id() &&
              motionPathCache_.framePos ==
                  static_cast<int64_t>(currentFrameNum) &&
              motionPathCache_.overlaySerial == overlayInvalidationSerial_;

          if (!cacheHit) {
            motionPathCache_.valid = false;
            motionPathCache_.layerId = layer->id();
            motionPathCache_.framePos = static_cast<int64_t>(currentFrameNum);
            motionPathCache_.overlaySerial = overlayInvalidationSerial_;
            motionPathCache_.pathPoints.clear();
            motionPathCache_.keyPoints.clear();

            if (hasPathSegment) {
              for (int f = minFrame; f <= maxFrame; f += 2) {
                if (f > maxFrame)
                  f = maxFrame;
                ArtifactCore::RationalTime t(f, rate);
                QTransform gTrans = layer->getGlobalTransformAt(f);
                float ax = t3d.anchorXAt(t);
                float ay = t3d.anchorYAt(t);
                QPointF wPos = gTrans.map(QPointF(ax, ay));
                motionPathCache_.pathPoints.push_back(
                    {f, (float)wPos.x(), (float)wPos.y()});
              }
            }

            for (const auto &kfTime : posTimes) {
              int f = static_cast<int>(kfTime.value());
              if (f < minFrame || f > maxFrame)
                continue;
              QTransform gTrans = layer->getGlobalTransformAt(f);
              float ax = t3d.anchorXAt(kfTime);
              float ay = t3d.anchorYAt(kfTime);
              QPointF wPos = gTrans.map(QPointF(ax, ay));
              const int interp = static_cast<int>(
                  t3d.positionXKeyFrameInterpolationAt(kfTime));
              MotionPathCacheEntry::Pt pt;
              pt.frame = f;
              pt.x = static_cast<float>(wPos.x());
              pt.y = static_cast<float>(wPos.y());
              pt.interpolation = interp;
              const QRectF localBounds = layer->localBounds();
              if (localBounds.isValid() && localBounds.width() > 0.0 &&
                  localBounds.height() > 0.0) {
                const QPointF tl = gTrans.map(localBounds.topLeft());
                const QPointF tr = gTrans.map(localBounds.topRight());
                const QPointF br = gTrans.map(localBounds.bottomRight());
                const QPointF bl = gTrans.map(localBounds.bottomLeft());
                const float minX =
                    static_cast<float>(std::min(std::min(tl.x(), tr.x()),
                                                std::min(br.x(), bl.x())));
                const float minY =
                    static_cast<float>(std::min(std::min(tl.y(), tr.y()),
                                                std::min(br.y(), bl.y())));
                const float maxX =
                    static_cast<float>(std::max(std::max(tl.x(), tr.x()),
                                                std::max(br.x(), bl.x())));
                const float maxY =
                    static_cast<float>(std::max(std::max(tl.y(), tr.y()),
                                                std::max(br.y(), bl.y())));
                pt.frameX = minX;
                pt.frameY = minY;
                pt.frameW = std::max(0.0f, maxX - minX);
                pt.frameH = std::max(0.0f, maxY - minY);
                pt.hasFrameRect = true;
              }
              motionPathCache_.keyPoints.push_back(pt);
            }
            motionPathCache_.valid = true;
          }

          // Render from cache (no getGlobalTransformAt() calls on a hit).
          if (!motionPathCache_.pathPoints.empty()) {
            Detail::float2 lastPos;
            bool hasLastPos = false;
            for (const auto &pt : motionPathCache_.pathPoints) {
              Detail::float2 currentPos(pt.x, pt.y);
              if (hasLastPos) {
                drawTaggedSolidLine(renderer_.get(),
                                    {lastPos.x, lastPos.y},
                                    {currentPos.x, currentPos.y}, pathColor,
                                    lineThickness, true);
              }
              renderer_->drawPoint(pt.x, pt.y, dotRadius * 0.6f,
                                   {0.8f, 0.8f, 0.8f, 0.7f});
              lastPos = currentPos;
              hasLastPos = true;
            }
          }
          for (const auto &pt : motionPathCache_.keyPoints) {
            const bool isCurrent = pt.frame == currentFrameNum;
            const bool isHovered = pt.frame == hoveredMotionPathFrame_;
            const FloatColor keyShadow{0.0f, 0.0f, 0.0f, 0.45f};
            const FloatColor keyColor =
                motionPathInterpolationColor(pt.interpolation, isCurrent);
            if (pt.hasFrameRect) {
              const FloatColor frameShadow{0.0f, 0.0f, 0.0f,
                                           isCurrent ? 0.30f : 0.18f};
              const FloatColor frameColor =
                  isCurrent ? FloatColor{0.98f, 0.88f, 0.35f, 0.95f}
                            : FloatColor{0.78f, 0.82f, 0.90f, 0.62f};
              const float dashThickness =
                  isCurrent ? std::max(1.5f, 2.2f * invZoom)
                            : std::max(1.0f, 1.6f * invZoom);
              const float dashLen = std::max(6.0f, 10.0f * invZoom);
              const float gapLen = std::max(4.0f, 7.0f * invZoom);
              renderer_->drawDashedRectOutline(
                  pt.frameX, pt.frameY, pt.frameW, pt.frameH, frameShadow,
                  dashThickness * 1.8f, dashLen, gapLen);
              renderer_->drawDashedRectOutline(
                  pt.frameX, pt.frameY, pt.frameW, pt.frameH, frameColor,
                  dashThickness, dashLen, gapLen);
            }
            const float outerRadius =
                isHovered ? dotRadius * 2.4f : dotRadius * 1.8f;
            const float innerRadius =
                isHovered ? dotRadius * 1.35f : dotRadius * 1.15f;
            const FloatColor ringColor = isHovered
                                              ? FloatColor{1.0f, 1.0f, 1.0f, 0.95f}
                                              : keyShadow;
            renderer_->drawPoint(pt.x, pt.y, outerRadius, ringColor);
            renderer_->drawPoint(pt.x, pt.y, innerRadius, keyColor);
          }
        }
      }
      {
        ArtifactCore::ProfileScope _profBBox(
            "BoundingBox", ArtifactCore::ProfileCategory::Render);
        if (showGizmoOverlay_ && showGuides_) {
          const FloatColor primaryColor{1.0f, 0.72f, 0.22f, 1.0f};
          const FloatColor secondaryColor{0.28f, 0.74f, 1.0f, 0.85f};
          for (const auto &layer : layersForOverlay) {
            if (!isLayerEffectivelyVisible(layer) ||
                !layer->isActiveAt(currentFrame)) {
              continue;
            }
            if (dynamic_cast<ArtifactVideoLayer *>(layer.get())) {
              continue;
            }
            if (!isLayerSelected(selectedIds, layer)) {
              continue;
            }

            const QRectF localBounds = layer->localBounds();
            if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
                localBounds.height() <= 0.0) {
              continue;
            }

            const bool primary =
                !selectedLayerId_.isNil() && layer->id() == selectedLayerId_;
            const QTransform globalTransform = layer->getGlobalTransform();
            const QPointF tl = globalTransform.map(localBounds.topLeft());
            const QPointF tr = globalTransform.map(localBounds.topRight());
            const QPointF br = globalTransform.map(localBounds.bottomRight());
            const QPointF bl = globalTransform.map(localBounds.bottomLeft());
            const FloatColor color = primary ? primaryColor : secondaryColor;
            const float thickness = primary ? 1.9f : 1.4f;
            const bool showBounds =
                owner->isLineDebugKindVisible(LineDebugKind::Bounds);
            drawTaggedSolidLine(renderer_.get(), tl, tr, color, thickness,
                                showBounds);
            drawTaggedSolidLine(renderer_.get(), tr, br, color, thickness,
                                showBounds);
            drawTaggedSolidLine(renderer_.get(), br, bl, color, thickness,
                                showBounds);
            drawTaggedSolidLine(renderer_.get(), bl, tl, color, thickness,
                                showBounds);
          }
        }
      } // BoundingBox scope
    }

    if (renderer_ && isRubberBandSelecting_) {
      const QRectF rubberBandRect = rubberBandCanvasRect().normalized();
      if (rubberBandRect.isValid() && rubberBandRect.width() > 0.0f &&
          rubberBandRect.height() > 0.0f) {
        renderer_->drawSolidRect(static_cast<float>(rubberBandRect.left()),
                                 static_cast<float>(rubberBandRect.top()),
                                 static_cast<float>(rubberBandRect.width()),
                                 static_cast<float>(rubberBandRect.height()),
                                 {0.25f, 0.55f, 1.0f, 0.14f}, 1.0f);
        const bool showSelectionRect =
            owner->isLineDebugKindVisible(LineDebugKind::SelectionRect);
        drawTaggedRectOutline(renderer_.get(), rubberBandRect,
                              {0.25f, 0.70f, 1.0f, 0.95f},
                              showSelectionRect);
      }
    }

    if (renderer_ && rectangleToolDragging_) {
      const QRectF rect =
          dragRectFromPoints(rectangleToolStartCanvasPos_,
                             rectangleToolCurrentCanvasPos_)
              .normalized();
      if (rect.isValid() && rect.width() > 0.0f && rect.height() > 0.0f) {
        const FloatColor fillColor =
            rectangleToolMode_ == RectangleToolMode::Mask
                ? FloatColor{0.28f, 0.88f, 1.0f, 0.12f}
                : FloatColor{1.0f, 0.78f, 0.24f, 0.12f};
        const FloatColor outlineColor =
            rectangleToolMode_ == RectangleToolMode::Mask
                ? FloatColor{0.28f, 0.88f, 1.0f, 0.95f}
                : FloatColor{1.0f, 0.78f, 0.24f, 0.95f};
        renderer_->drawSolidRect(static_cast<float>(rect.left()),
                                 static_cast<float>(rect.top()),
                                 static_cast<float>(rect.width()),
                                 static_cast<float>(rect.height()), fillColor,
                                 1.0f);
        drawRectOutline(renderer_.get(), rect, outlineColor, 1.6f);
      }
    }

    if (showFrameInfo_ && renderer_) {
      const float infoW = 60.0f;
      const float infoH = 14.0f;
      const float infoX = 4.0f;
      const float infoY = lastCanvasHeight_ - infoH - 4.0f;
      renderer_->drawSolidRect(infoX, infoY, infoW, infoH,
                               {0.0f, 0.0f, 0.0f, 0.6f}, 0.8f);
      const int frame = currentFrame.framePosition();
      const float barRatio =
          (frame > 0) ? std::min(1.0f, static_cast<float>(frame) / 1000.0f)
                      : 0.0f;
      const float barW = infoW * barRatio;
      if (barW > 1.0f) {
        renderer_->drawSolidRect(infoX, infoY, barW, infoH,
                                 {0.2f, 0.6f, 1.0f, 0.5f}, 0.6f);
      }
    }

    if (showSafeMargins_) {
      // Safe area is a screen-space 2D guide. Keep it out of any 3D camera
      // path.
      const float actionSafeW = cw * 0.9f;
      const float actionSafeH = ch * 0.9f;
      const float titleSafeW = cw * 0.8f;
      const float titleSafeH = ch * 0.8f;
      const FloatColor outlineColor = {0.0f, 0.0f, 0.0f, 0.72f};
      const FloatColor innerColor = {0.95f, 0.97f, 1.0f, 0.94f};
      const auto snap = [](float value) { return std::round(value) + 0.5f; };

      const float actionX = snap((cw - actionSafeW) * 0.5f);
      const float actionY = snap((ch - actionSafeH) * 0.5f);
      const float actionX2 = snap(actionX + actionSafeW);
      const float actionY2 = snap(actionY + actionSafeH);
      const float titleX = snap((cw - titleSafeW) * 0.5f);
      const float titleY = snap((ch - titleSafeH) * 0.5f);
      const float titleX2 = snap(titleX + titleSafeW);
      const float titleY2 = snap(titleY + titleSafeH);
      const auto drawSafeRect = [&](float x1, float y1, float x2, float y2) {
        if (!renderer_) {
          return;
        }
        const float w = x2 - x1;
        const float h = y2 - y1;
        if (w <= 0.0f || h <= 0.0f) {
          return;
        }

        renderer_->drawRectOutline(x1, y1, w, h, outlineColor);
        if (w > 4.0f && h > 4.0f) {
          renderer_->drawRectOutline(x1 + 1.0f, y1 + 1.0f, w - 2.0f, h - 2.0f,
                                     innerColor);
        }
      };

      drawSafeRect(actionX, actionY, actionX2, actionY2);
      drawSafeRect(titleX, titleY, titleX2, titleY2);

      const float crossSize = std::max(20.0f, std::min(cw, ch) * 0.05f);
      renderer_->drawCrosshair(snap(cw * 0.5f), snap(ch * 0.5f), crossSize,
                               innerColor);
    }

    guideVerticals_.clear();
    guideHorizontals_.clear();
    if (showGuides_ && comp && !selectedLayerId_.isNil()) {
      const auto guideLayer = comp->layerById(selectedLayerId_);
      if (guideLayer) {
        const QRectF bounds = guideLayer->transformedBoundingBox();
        if (bounds.isValid() && bounds.width() > 0.0 && bounds.height() > 0.0) {
          const float left = static_cast<float>(bounds.left());
          const float right = static_cast<float>(bounds.right());
          const float top = static_cast<float>(bounds.top());
          const float bottom = static_cast<float>(bounds.bottom());
          const float centerX = static_cast<float>(bounds.center().x());
          const float centerY = static_cast<float>(bounds.center().y());
          guideVerticals_.push_back(left);
          guideVerticals_.push_back(centerX);
          guideVerticals_.push_back(right);
          guideHorizontals_.push_back(top);
          guideHorizontals_.push_back(centerY);
          guideHorizontals_.push_back(bottom);
        }
      }
    }

    if (showGuides_) {
      const FloatColor guideColor = {0.2f, 0.8f, 1.0f, 0.7f};
      for (float x : guideVerticals_) {
        if (x >= 0 && x <= cw) {
          renderer_->drawSolidLine({x, 0}, {x, ch}, guideColor, 1.0f);
        }
      }
      for (float y : guideHorizontals_) {
        if (y >= 0 && y <= ch) {
          renderer_->drawSolidLine({0, y}, {cw, y}, guideColor, 1.0f);
        }
      }
      if (guideVerticals_.isEmpty() && guideHorizontals_.isEmpty()) {
        renderer_->drawSolidLine({cw * 0.5f, 0}, {cw * 0.5f, ch}, guideColor,
                                 1.0f);
        renderer_->drawSolidLine({0, ch * 0.5f}, {cw, ch * 0.5f}, guideColor,
                                 1.0f);
      }
    }
    {
      ArtifactCore::ProfileScope _profOverlay(
          "Overlay", ArtifactCore::ProfileCategory::UI);
      const ArtifactAbstractLayerPtr selectedLayer =
          (!selectedLayerId_.isNil() && comp)
              ? comp->layerById(selectedLayerId_)
              : ArtifactAbstractLayerPtr{};
      renderer_->setUseExternalMatrices(false);
      renderer_->resetGizmoCameraMatrices();
      renderer_->reset3DCameraMatrices();
      renderer_->setCanvasSize(cw, ch);
      if (showCompositionRegionOverlay_) {
        ::Artifact::drawCompositionRegionOverlay(renderer_.get(), comp);
      }
      if (showCameraFrustumOverlay_ && activeCamera) {
        const auto cameraOverlayVisual =
            buildCameraFrustumVisual(activeCamera, comp);
        ::Artifact::drawCameraFrustumOverlay(
            renderer_.get(), cameraOverlayVisual,
            activeCamera->id() == selectedLayerId_);
      }
      if (showDensityHeatmapOverlay_) {
        drawVisualDensityOverlay(renderer_.get(), comp, selectedLayer,
                                 currentFrame);
      }
      if (selectedLayer) {
        if (!showGizmoOverlay_) {
          ::Artifact::drawSelectionOverlay(renderer_.get(), selectedLayer);
        }
        if (selectedLayer->isCloneLayer()) {
          ::Artifact::drawClonerFrameOverlay(renderer_.get(), selectedLayer);
        }
        const bool selectedLayerIsActiveCamera =
            activeCamera && activeCamera->id() == selectedLayer->id();
        ::Artifact::drawCameraSelectionOverlay(
            renderer_.get(), selectedLayer, selectedLayerIsActiveCamera);
        if (showEffectHitboxOverlay_) {
          drawEffectHitboxOverlay(renderer_.get(), comp, selectedLayer);
        }
      }
      const bool anchorOverlayToolActive =
          gizmoMode_ == TransformGizmo::Mode::AnchorPoint ||
          (gizmo_ && gizmo_->isDragging() &&
           gizmo_->activeHandle() == TransformGizmo::HandleType::Anchor);
      if (showAnchorCenterOverlay_ && selectedLayer && anchorOverlayToolActive) {
        ::Artifact::drawAnchorCenterOverlay(renderer_.get(), selectedLayer);
      }
      drawViewportGhostOverlay(owner, comp, selectedLayer, currentFrame);
      drawViewportUiOverlay();
    }
    overlayMs = markPhaseMs();

    flushMs = 0;
  renderCrashTrace("render-cost-end-begin", renderFrameCounter_);
  renderCostGuard.reset();
  renderCrashTrace("render-cost-end", renderFrameCounter_);

    if (!lastVideoDebug_.isEmpty() &&
        lastVideoDebug_ != lastEmittedVideoDebug_) {
      lastEmittedVideoDebug_ = lastVideoDebug_;
      qDebug() << lastVideoDebug_;
      Q_EMIT owner->videoDebugMessage(lastVideoDebug_);
    }

    // renderer_->flushAndWait(); // 毎フレーム同期を削除し、性能改善を試む
    {
      ArtifactCore::ProfileScope _profPresent(
          "Present", ArtifactCore::ProfileCategory::Render);
      renderCrashTrace("render-present-begin", renderFrameCounter_);
      renderer_->present();
      const double presentedGpuFrameMs = renderer_->lastFrameGpuTimeMs();
      const QString presentedStatus = renderer_->lastPresentStatus();
      QString presentedVideoDebug;
      for (const auto& layer : layers) {
        if (auto* videoLayer =
                dynamic_cast<ArtifactVideoLayer*>(layer.get())) {
          if (useRamPreviewFallback &&
              isLayerEffectivelyVisible(layer) &&
              layer->isActiveAt(currentFrame)) {
            videoLayer->markFrameCompositionCacheReady(
                layer->currentFrame(), playbackPreviewState.onDisk);
            videoLayer->markFrameRenderQueued(layer->currentFrame());
          }
          videoLayer->markFramePresented(
              layer->currentFrame(), presentedGpuFrameMs, presentedStatus);
          if (presentedVideoDebug.isEmpty() &&
              isLayerEffectivelyVisible(layer) &&
              layer->isActiveAt(currentFrame)) {
            presentedVideoDebug =
                QStringLiteral("[Video] phase=present compositionCache=%1 "
                               "compositionCacheReason=%2 ")
                    .arg(useRamPreviewFallback
                             ? (playbackPreviewState.onDisk
                                    ? QStringLiteral("disk-hydrated-ram")
                                    : QStringLiteral("ram"))
                             : QStringLiteral("live"))
                    .arg(ramPreviewFallbackReason) +
                videoLayer->decodeState();
          }
        }
      }
      if (!presentedVideoDebug.isEmpty() &&
          presentedVideoDebug != lastEmittedVideoDebug_) {
        lastVideoDebug_ = presentedVideoDebug;
        lastEmittedVideoDebug_ = presentedVideoDebug;
        qDebug() << presentedVideoDebug;
        Q_EMIT owner->videoDebugMessage(presentedVideoDebug);
      }
      if (pipelineEnabled) {
        previewRenderSlot.state = PreviewRenderPipelineSlot::State::Retirable;
      }
      renderCrashTrace("render-present-end", renderFrameCounter_);
    }

    if (auto *playback = ArtifactPlaybackService::instance()) {
      const auto previewSummary = playback->ramPreviewSummary();
      const bool asyncRamPreviewReadbackEnabled =
          qEnvironmentVariableIsSet("ARTIFACT_ENABLE_RAM_PREVIEW_ASYNC_READBACK");
      const bool shouldCaptureRamPreview =
          asyncRamPreviewReadbackEnabled &&
          playback->isRamPreviewEnabled() && playbackPreviewStateValid &&
          playbackSameComposition && !playback->isPlaying() &&
          !viewportInteracting_ && !frameOutOfRange &&
          !playbackPreviewState.ready && playbackPreviewPendingBuild;
      if (shouldCaptureRamPreview) {
        renderCrashTrace("render-async-readback-begin", renderFrameCounter_);
        // Asynchronous readback: GPU copy + fence wait runs on a worker thread,
        // avoiding a full GPU pipeline stall on the render thread.
        const auto weakPlayback = QPointer<ArtifactPlaybackService>(playback);
        const PreviewFrameRequest previewRequest{
            framePos,
            previewSummary.buildQueueNextFrame,
            comp->id().toString(),
            previewDownsample_,
            effectivePreviewDownsample,
            pipelineEnabled,
            previewSummary.currentPriorityReason,
            previewSummary.buildQueueGeneration};
        const bool previewRequestMatchesRenderedFrame =
            previewRequest.buildTargetFrame == previewRequest.framePos;
        if (!previewRequestMatchesRenderedFrame) {
          renderCrashTrace("render-async-readback-skip-frame-mismatch",
                           renderFrameCounter_);
        } else if (previewRequest.pipelineEnabled && ramPreviewReadbackSRV) {
          renderer_->readbackTextureViewToImageAsync(
              ramPreviewReadbackSRV,
              [weakPlayback, previewRequest](const QImage& capturedFrame) {
                publishPreviewFrameRequestResult(weakPlayback, previewRequest,
                                                 capturedFrame);
              });
        } else {
          renderer_->readbackToImageAsync(
              [weakPlayback, previewRequest](const QImage& capturedFrame) {
                publishPreviewFrameRequestResult(weakPlayback, previewRequest,
                                                 capturedFrame);
              });
        }
        renderCrashTrace("render-async-readback-end", renderFrameCounter_);
      }
    }
    presentMs = markPhaseMs();
    lastSubmit2DMs_ = latestTimerMs("Submit2D");

    ++renderFrameCounter_;
    const qint64 frameMs = frameTimer.elapsed();
    lastFrameTimeMs_ = static_cast<double>(frameMs);
    recentFrameTimesMs_.push_back(lastFrameTimeMs_);
    recentFrameTimeSumMs_ += lastFrameTimeMs_;
    constexpr std::size_t kRecentFrameTimeHistory = 120;
    while (recentFrameTimesMs_.size() > kRecentFrameTimeHistory) {
      recentFrameTimeSumMs_ -= recentFrameTimesMs_.front();
      recentFrameTimesMs_.pop_front();
    }
    if (!recentFrameTimesMs_.empty()) {
      averageFrameTimeMs_ =
          recentFrameTimeSumMs_ /
          static_cast<double>(recentFrameTimesMs_.size());
    } else {
      averageFrameTimeMs_ = 0.0;
    }
    lastSetupMs_ = setupMs;
    lastBasePassMs_ = basePassMs;
    lastLayerPassMs_ = layerPassMs;
    lastOverlayMs_ = overlayMs;
    lastFlushMs_ = flushMs;
    lastSubmit2DMs_ = std::max<qint64>(lastSubmit2DMs_, 0);
    lastPresentMs_ = presentMs;
    if (renderFrameCounter_ <= 5 ||
        renderer_->lastPresentStatus() != QStringLiteral("ok")) {
      quintptr hostWinId = 0;
      QSize hostSize;
      qreal hostDpr = 1.0;
      bool hostVisible = false;
      if (auto *host = hostWidget_.data()) {
        hostWinId = static_cast<quintptr>(host->winId());
        hostSize = host->size();
        hostDpr = host->devicePixelRatio();
        hostVisible = host->isVisible();
      }
      qInfo() << "[CompositionView][PresentProbe]"
              << "frame=" << renderFrameCounter_
              << "status=" << renderer_->lastPresentStatus()
              << "attempts=" << renderer_->presentAttemptCount()
              << "ok=" << renderer_->presentSuccessCount()
              << "fail=" << renderer_->presentFailureCount()
              << "skip=" << renderer_->presentSkippedCount()
              << "hostVisible=" << hostVisible
              << "hostWinId=" << hostWinId
              << "hostSize=" << hostSize
              << "hostDpr=" << hostDpr
              << "hasSwapChain=" << renderer_->hasSwapChain();
    }
    const auto textureCacheStats = gpuTextureCacheManager_
                                       ? gpuTextureCacheManager_->stats()
                                       : GPUTextureCacheStats{};
    const QString mfrpSlotsSummary = previewRenderPipelineSlotsSummary();
    const QString mfrpPressureSummary = previewRenderPipelinePressureSummary();
    const QString mfrpPolicySummary = previewRenderPipelineAcquirePolicySummary();
    const qulonglong mfrpEstimatedBytes =
        static_cast<qulonglong>(previewRenderPipelineEstimatedBytes());
    lastRenderPathSummary_ =
        QStringLiteral(
            "path=%1 gpuBlendEnabled=%2 gpuBlendReady=%3 layersTotal=%4 "
             "layersDrawn=%5 surfaceUploadLayers=%6 cpuRasterLayers=%7 "
             "frameOutOfRange=%8 previewDownsample=%9 effectiveDownsample=%10 "
             "viewportInteracting=%11 cacheEntries=%12 cacheBytes=%13 "
             "presentStatus=%14 presentOk=%15 presentFail=%16 presentSkip=%17 "
             "ramPreviewFallback=%18 ramPreviewFallbackReason=%19 "
             "mfrpEligible=%20 mfrpReason=%21 mfrpSlot=%22 mfrpSlotState=%23 "
             "mfrpDepthReady=%24 mfrpLastSubmitFrame=%25 "
             "mfrpSlotAcquire=%26 mfrpEstimatedBytes=%27 mfrpSlots={%28} "
             "mfrpPressure={%29} mfrpPolicy={%30} rayTracing={%31}")
            .arg(pipelineEnabled ? QStringLiteral("gpu-blend")
                                 : QStringLiteral("fallback"))
            .arg(gpuBlendEnabled_)
            .arg(blendPipelineReady_)
            .arg(layers.size())
            .arg(drawnLayerCount)
            .arg(surfaceUploadLayerCount)
            .arg(cpuRasterLayerCount)
            .arg(frameOutOfRange)
            .arg(previewDownsample_)
            .arg(effectivePreviewDownsample)
            .arg(viewportInteracting_)
            .arg(textureCacheStats.entryCount)
            .arg(static_cast<qulonglong>(textureCacheStats.memoryBytes))
            .arg(renderer_->lastPresentStatus())
            .arg(static_cast<qulonglong>(renderer_->presentSuccessCount()))
            .arg(static_cast<qulonglong>(renderer_->presentFailureCount()))
            .arg(static_cast<qulonglong>(renderer_->presentSkippedCount()))
            .arg(useRamPreviewFallback ? 1 : 0)
            .arg(ramPreviewFallbackReason)
            .arg(multiFrameReason == QStringLiteral("eligible") ? 1 : 0)
            .arg(multiFrameReason)
            .arg(activePreviewRenderPipelineSlot_)
            .arg(previewRenderPipelineSlotStateText(previewRenderSlot))
            .arg(previewRenderSlot.depthTargetView ? 1 : 0)
            .arg(previewRenderSlot.lastSubmittedFrame)
            .arg(lastPreviewRenderPipelineSlotAcquireReason_)
            .arg(mfrpEstimatedBytes)
            .arg(mfrpSlotsSummary)
            .arg(mfrpPressureSummary)
            .arg(mfrpPolicySummary)
            .arg(renderer_->rayTracingDebugState());
    const QString visibilityState =
        frameOutOfRange
            ? QStringLiteral("frameOutOfRange")
            : (compositedLayerCount > 0
                   ? QStringLiteral("drawn")
                   : (layers.isEmpty() ? QStringLiteral("noLayers")
                                       : QStringLiteral("allSkipped")));
    lastCompositionVisibilitySummary_ =
        QStringLiteral(
            "phase=composition-visibility-v1 state=%1 path=%2 layersTotal=%3 "
            "drawn=%4 composited=%5 skippedInvisible=%6 skippedSolo=%7 "
            "skippedInactive=%8 skippedRoi=%9 skippedLod=%10 opacityZero=%11 "
            "frameOutOfRange=%12 gpuBlendBlocker=%13 "
            "forceContinuousRedraw=%14 viewportInteracting=%15 "
            "firstLayerSeed=%16 notes=%17")
            .arg(visibilityState,
                 pipelineEnabled ? QStringLiteral("gpu-blend")
                                 : QStringLiteral("fallback"))
            .arg(layers.size())
            .arg(drawnLayerCount)
            .arg(compositedLayerCount)
            .arg(skipInvisibleCount)
            .arg(skipSoloCount)
            .arg(skipInactiveCount)
            .arg(skipRoiCount)
            .arg(skipLodCount)
            .arg(opacityZeroCount)
            .arg(frameOutOfRange ? 1 : 0)
            .arg(hasGpuBlendBlocker ? 1 : 0)
            .arg(forceContinuousRedraw ? 1 : 0)
            .arg(viewportInteracting_ ? 1 : 0)
            .arg(layerToFloatConvertCount)
            .arg(hasGpuBlendBlocker
                     ? QStringLiteral("gpuBlendBlockedByCpuRasterizerWork")
                     : QStringLiteral("none"));
    lastBlendMaskSummary_ =
        QStringLiteral(
            "phase=blend-mask-smoke-v1 path=%1 blendContract=straight-src_over-premul-accum "
            "maskContract=%2 pipelineFormat=RGBA32F layerFormat=RGBA8_sRGB layerComputeFormat=RGBA32F "
            "layersDrawn=%3 nonNormal=%4 maskedLayers=%5 masks=%6 "
            "dispatch=%7 retryNormal=%8 failed=%9 directFallback=%10 "
            "layerToFloatConvert=%11 layerPixels={%12} accumPixels={%13} "
            "notes=%14")
            .arg(pipelineEnabled ? QStringLiteral("gpu-blend")
                                 : QStringLiteral("fallback"))
            .arg(totalMaskCount > 0 ? QStringLiteral("pending")
                                    : QStringLiteral("none"))
            .arg(drawnLayerCount)
            .arg(nonNormalBlendLayerCount)
            .arg(maskedLayerCount)
            .arg(totalMaskCount)
            .arg(blendDispatchCount)
            .arg(blendRetryNormalCount)
            .arg(blendFailureCount)
            .arg(directBlendFallbackCount)
            .arg(layerToFloatConvertCount)
            .arg(lastLayerRtPixelStats_.isEmpty()
                     ? QStringLiteral("state=unavailable")
                     : lastLayerRtPixelStats_)
            .arg(lastAccumRtPixelStats_.isEmpty()
                     ? QStringLiteral("state=unavailable")
                     : lastAccumRtPixelStats_)
            .arg(blendMaskLayerNotes.isEmpty()
                     ? QStringLiteral("none")
                     : blendMaskLayerNotes.join(QStringLiteral("; ")));
    if (compositionViewLog().isDebugEnabled()) {
      if (frameMs >= 16) {
        qCDebug(compositionViewLog)
            << "[CompositionView][Perf][Slow]"
            << "frameMs=" << frameMs << "pipelineEnabled=" << pipelineEnabled
            << "layersTotal=" << layers.size()
            << "layersDrawn=" << drawnLayerCount
            << "surfaceUploadLayers=" << surfaceUploadLayerCount
            << "cpuRasterLayers=" << cpuRasterLayerCount
            << "frameOutOfRange=" << frameOutOfRange
            << "previewDownsample=" << previewDownsample_
            << "effectivePreviewDownsample=" << effectivePreviewDownsample
            << "viewportInteracting=" << viewportInteracting_
            << "setupMs=" << setupMs << "basePassMs=" << basePassMs
            << "layerPassMs=" << layerPassMs << "overlayMs=" << overlayMs
            << "flushMs=" << flushMs << "submit2DMs=" << lastSubmit2DMs_
            << "presentMs=" << presentMs;
      } else if ((renderFrameCounter_ % 120u) == 0u) {
        qCDebug(compositionViewLog)
            << "[CompositionView][Perf]"
            << "frameMs=" << frameMs << "pipelineEnabled=" << pipelineEnabled
            << "layersTotal=" << layers.size()
            << "layersDrawn=" << drawnLayerCount << "setupMs=" << setupMs
            << "basePassMs=" << basePassMs << "layerPassMs=" << layerPassMs
            << "overlayMs=" << overlayMs << "flushMs=" << flushMs
            << "submit2DMs=" << lastSubmit2DMs_ << "presentMs=" << presentMs;
      }
    }
  } // _profSetup ("RenderFrame") destructs here — BEFORE endFrame() so its
    // duration is correctly recorded (popScope requires inFrame == true).
  ArtifactCore::Profiler::instance().endFrame();
} // end renderOneFrameImpl

QRectF CompositionRenderController::Impl::commandPaletteRect() const {
  const float overlayWf = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
  const float overlayHf = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
  const float panelW = std::min(560.0f, std::max(320.0f, overlayWf - 48.0f));
  const int visibleRows =
      std::min(8, static_cast<int>(commandPaletteItems_.size()));
  const float panelH = 54.0f + static_cast<float>(visibleRows) * 30.0f + 12.0f;
  const float x = std::max(12.0f, (overlayWf - panelW) * 0.5f);
  const float y = std::max(12.0f, std::min(96.0f, overlayHf * 0.16f));
  return QRectF(x, y, panelW, std::min(panelH, std::max(96.0f, overlayHf - y - 12.0f)));
}

QRectF CompositionRenderController::Impl::contextMenuRect() const {
  const float overlayWf = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
  const float overlayHf = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
  QFont font = QApplication::font();
  font.setPointSizeF(std::max(9.0, static_cast<double>(font.pointSizeF())));
  QFont titleFont = font;
  titleFont.setBold(true);
  titleFont.setPointSizeF(titleFont.pointSizeF() + 1.0);
  const QFontMetrics titleFm(titleFont);
  const QFontMetrics fm(font);
  int textW = 140;
  for (const QString &item : contextMenuItems_) {
    textW = std::max(textW, fm.horizontalAdvance(item));
  }
  if (!contextMenuTitle_.trimmed().isEmpty()) {
    textW = std::max(textW, titleFm.horizontalAdvance(contextMenuTitle_));
  }
  if (!contextMenuSubtitle_.trimmed().isEmpty()) {
    textW = std::max(textW, fm.horizontalAdvance(contextMenuSubtitle_));
  }
  const float panelW = static_cast<float>(std::min(360, textW + 48));
  const bool hasTitle = !contextMenuTitle_.trimmed().isEmpty();
  const bool hasSubtitle = !contextMenuSubtitle_.trimmed().isEmpty();
  const float headerH = hasTitle ? (hasSubtitle ? 54.0f : 36.0f) : 0.0f;
  const float panelH = 12.0f + headerH +
                       static_cast<float>(contextMenuItems_.size()) * 28.0f;
  float x = static_cast<float>(contextMenuViewportPos_.x());
  float y = static_cast<float>(contextMenuViewportPos_.y());
  if (x + panelW > overlayWf - 8.0f) {
    x = overlayWf - panelW - 8.0f;
  }
  if (y + panelH > overlayHf - 8.0f) {
    y = overlayHf - panelH - 8.0f;
  }
  return QRectF(std::max(8.0f, x), std::max(8.0f, y), panelW, panelH);
}

QRectF CompositionRenderController::Impl::pieMenuRect() const {
  const float overlayWf = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
  const float overlayHf = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
  const float diameter = std::min(overlayWf, overlayHf) * 0.42f;
  const float size = std::clamp(diameter, 180.0f, 420.0f);
  const float minX = 12.0f;
  const float minY = 12.0f;
  const float maxX = std::max(minX, overlayWf - size - 12.0f);
  const float maxY = std::max(minY, overlayHf - size - 12.0f);
  const float x = std::min(std::max(static_cast<float>(pieMenuViewportPos_.x()) - size * 0.5f, minX), maxX);
  const float y = std::min(std::max(static_cast<float>(pieMenuViewportPos_.y()) - size * 0.5f, minY), maxY);
  return QRectF(x, y, size, size);
}

QRectF CompositionRenderController::Impl::viewportOverlayItemRect(int index) const {
  if (index < 0) {
    return {};
  }
  if (commandPaletteVisible_) {
    if (index >= static_cast<int>(commandPaletteItems_.size()) || index >= 8) {
      return {};
    }
    const QRectF panel = commandPaletteRect();
    return QRectF(panel.left() + 10.0, panel.top() + 54.0 + index * 30.0,
                  panel.width() - 20.0, 28.0);
  }
  if (contextMenuVisible_) {
    if (index >= static_cast<int>(contextMenuItems_.size())) {
      return {};
    }
    const bool hasTitle = !contextMenuTitle_.trimmed().isEmpty();
    const bool hasSubtitle = !contextMenuSubtitle_.trimmed().isEmpty();
    const float headerH = hasTitle ? (hasSubtitle ? 54.0f : 36.0f) : 0.0f;
    const QRectF panel = contextMenuRect();
    return QRectF(panel.left() + 6.0, panel.top() + 6.0 + headerH + index * 28.0,
                  panel.width() - 12.0, 26.0);
  }
  return {};
}

int CompositionRenderController::Impl::pieMenuItemAt(const QPointF &viewportPos) const {
  if (!pieMenuVisible_ || pieMenuModel_.items.empty()) {
    return -1;
  }
  const QRectF rect = pieMenuRect();
  const QPointF center = rect.center();
  const QPointF delta = viewportPos - center;
  const double dist = std::hypot(delta.x(), delta.y());
  const double innerRadius = rect.width() * 0.19;
  const double outerRadius = rect.width() * 0.48;
  if (dist < innerRadius || dist > outerRadius) {
    return -1;
  }
  const double angle = std::atan2(-delta.y(), delta.x()) * 180.0 / M_PI;
  double normalized = angle < 0.0 ? angle + 360.0 : angle;
  const int count = static_cast<int>(pieMenuModel_.items.size());
  if (count <= 0) {
    return -1;
  }
  const double sectorSize = 360.0 / static_cast<double>(count);
  double shifted = normalized - (90.0 - sectorSize * 0.5);
  while (shifted < 0.0) {
    shifted += 360.0;
  }
  while (shifted >= 360.0) {
    shifted -= 360.0;
  }
  const int index = static_cast<int>(shifted / sectorSize);
  if (index < 0 || index >= count) {
    return -1;
  }
  return index;
}

int CompositionRenderController::Impl::viewportOverlayItemAt(
    const QPointF &viewportPos) const {
  const int count = commandPaletteVisible_
                        ? std::min(8, static_cast<int>(commandPaletteItems_.size()))
                        : (contextMenuVisible_
                               ? static_cast<int>(contextMenuItems_.size())
                               : 0);
  for (int i = 0; i < count; ++i) {
    if (viewportOverlayItemRect(i).contains(viewportPos)) {
      return i;
    }
  }
  return -1;
}

void CompositionRenderController::Impl::drawPieMenuOverlay() {
  if (!renderer_ || !pieMenuVisible_ || pieMenuModel_.items.empty()) {
    return;
  }

  const float overlayWf = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
  const float overlayHf = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
  if (overlayWf <= 0.0f || overlayHf <= 0.0f) {
    return;
  }
  const QRectF rect = pieMenuRect();
  ::Artifact::drawViewportPieMenuOverlay(renderer_.get(), overlayWf, overlayHf,
                                         rect, pieMenuModel_,
                                         pieMenuSelectedIndex_);
}

void CompositionRenderController::Impl::drawViewportUiOverlay() {
  if (!renderer_ || (!commandPaletteVisible_ && !contextMenuVisible_ &&
                     !pieMenuVisible_)) {
    return;
  }
  const float overlayWf = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
  const float overlayHf = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
  if (overlayWf <= 0.0f || overlayHf <= 0.0f) {
    return;
  }

  const float prevZoom = renderer_->getZoom();
  float prevPanX = 0.0f;
  float prevPanY = 0.0f;
  renderer_->getPan(prevPanX, prevPanY);
  renderer_->setUseExternalMatrices(false);
  renderer_->setCanvasSize(overlayWf, overlayHf);
  renderer_->setZoom(1.0f);
  renderer_->setPan(0.0f, 0.0f);

  if (commandPaletteVisible_) {
    const QString queryText = commandPaletteQuery_.trimmed().isEmpty()
                                  ? QStringLiteral("Type to filter commands")
                                  : commandPaletteQuery_.trimmed();
    ::Artifact::drawViewportCommandPaletteOverlay(
        renderer_.get(), overlayWf, overlayHf, commandPaletteRect(), queryText,
        commandPaletteItems_);
  }

  if (contextMenuVisible_) {
    ::Artifact::drawViewportContextMenuOverlay(
        renderer_.get(), overlayWf, overlayHf, contextMenuRect(),
        contextMenuTitle_, contextMenuSubtitle_, contextMenuItems_,
        contextMenuItemEnabled_, contextMenuSelectedIndex_);
  }

  if (pieMenuVisible_) {
    drawPieMenuOverlay();
  }

  renderer_->setZoom(prevZoom);
  renderer_->setPan(prevPanX, prevPanY);
  if (lastCanvasWidth_ > 0.0f && lastCanvasHeight_ > 0.0f) {
    renderer_->setCanvasSize(lastCanvasWidth_, lastCanvasHeight_);
  }
}

void CompositionRenderController::Impl::drawViewportGhostOverlay(
    CompositionRenderController *owner, const ArtifactCompositionPtr &comp,
    const ArtifactAbstractLayerPtr &selectedLayer,
    const FramePosition &currentFrame) {
  Q_UNUSED(owner);
  Q_UNUSED(currentFrame);
  if (!renderer_) {
    return;
  }

  const bool dropActive = dropGhostVisible_ && !dropGhostRect_.isNull();
  if (!dropActive) {
    return;
  }

  if (compositionViewLog().isDebugEnabled()) {
    qCDebug(compositionViewLog)
        << "[CompositionView][ViewportGhost]"
        << "dropActive=" << dropActive << "rect=" << dropGhostRect_
        << "title=" << dropGhostTitle_;
  }

  const float overlayWf = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
  const float overlayHf = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
  if (overlayWf <= 0.0f || overlayHf <= 0.0f) {
    return;
  }

  const int overlayW = std::max(1, static_cast<int>(std::ceil(overlayWf)));
  const int overlayH = std::max(1, static_cast<int>(std::ceil(overlayHf)));
  const QSize compSize = comp ? comp->settings().compositionSize() : QSize();
  const QSize restoreCanvasSize(compSize.width() > 0 ? compSize.width() : 1920,
                                compSize.height() > 0 ? compSize.height() : 1080);
  const bool snapHintActive = gizmo_ && gizmo_->isDragging() && selectedLayer;
  const bool infoActive = infoOverlayVisible_ &&
                          (!infoOverlayTitle_.trimmed().isEmpty() ||
                           !infoOverlayDetail_.trimmed().isEmpty());
  const QString ghostTitle = dropGhostTitle_.isEmpty()
                                 ? QStringLiteral("Drop to add layer")
                                 : dropGhostTitle_;
  const QString ghostHint = dropGhostHint_.isEmpty()
                                ? QStringLiteral("Release to place")
                                : dropGhostHint_;
  const QRectF ghostRect = dropGhostRect_.normalized();
  if (dropActive) {
    ::Artifact::drawViewportDropGhostOverlay(renderer_.get(), comp, overlayWf,
                                             overlayHf, ghostRect, ghostTitle,
                                             ghostHint, dropCandidateLabel_);
    if (!infoActive && !snapHintActive) {
      return;
    }
  }

  if (infoActive) {
    const QString title = infoOverlayTitle_.trimmed().isEmpty()
                              ? QStringLiteral("Info")
                              : infoOverlayTitle_.trimmed();
    const QString detail = infoOverlayDetail_.trimmed();
    ::Artifact::drawViewportInfoOverlay(renderer_.get(), overlayW, overlayH,
                                        title, detail, &restoreCanvasSize);
  }

  {
    auto *appManager = ArtifactApplicationManager::instance();
    auto *toolManager =
        appManager != nullptr ? appManager->toolManager() : nullptr;
    auto *selectionManager = appManager != nullptr
                                 ? appManager->layerSelectionManager()
                                 : nullptr;
    const ToolType activeTool = toolManager != nullptr
                                    ? toolManager->activeTool()
                                    : ToolType::Selection;
    const QString toolLabel = toolTypeToOverlayLabel(activeTool);
    const float zoom = renderer_ != nullptr ? renderer_->getZoom() : 1.0f;
    QStringList statusParts;
    statusParts << toolLabel
                << QStringLiteral("%1%")
                       .arg(zoom * 100.0f, 0, 'f', zoom < 10.0f ? 1 : 0);
    if (selectionManager != nullptr) {
      const int selectedCount =
          static_cast<int>(selectionManager->selectedLayers().size());
      if (selectedCount > 0) {
        statusParts << QStringLiteral("%1 selected").arg(selectedCount);
      }
    }
    const QString statusText = statusParts.join(QStringLiteral("  •  "));
    ::Artifact::drawViewportStatusChipOverlay(renderer_.get(), overlayW,
                                              overlayH, statusText,
                                              &restoreCanvasSize);
  }

  if (selectedLayer && !infoActive) {
    ::Artifact::drawSelectionSummaryOverlay(renderer_.get(), selectedLayer,
                                            overlayW, overlayH);
  }

  if (snapHintActive) {
    const bool snapBypassed =
        QGuiApplication::keyboardModifiers().testFlag(Qt::AltModifier);
    QString snapTitle =
        snapBypassed ? QStringLiteral("Snap Off") : QStringLiteral("Snap On");
    const QString snapDetail =
        snapBypassed ? QStringLiteral("Hold Alt to enable free move")
                     : QStringLiteral("Hold Alt to bypass snapping");
    int verticalCount = 0;
    int horizontalCount = 0;
    if (!snapBypassed && gizmo_) {
      for (const auto &line : gizmo_->activeSnapLines()) {
        if (line.isVertical) {
          ++verticalCount;
        } else {
          ++horizontalCount;
        }
      }
    }
    ::Artifact::drawViewportSnapHintOverlay(renderer_.get(), overlayW, overlayH,
                                            snapBypassed, snapTitle, snapDetail,
                                            verticalCount, horizontalCount,
                                            &restoreCanvasSize);
  }
}

void CompositionRenderController::setRenderQueueActive(bool active) {
  if (impl_) {
    impl_->renderQueueActive_ = active;
  }
}

bool CompositionRenderController::isRenderQueueActive() const {
  return impl_ ? impl_->renderQueueActive_ : false;
}

} // namespace Artifact
