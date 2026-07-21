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
#include <QString>
#include <QUuid>

#include <QTimer>

#include <QTransform>

#include <QVector3D>

#include <QVector4D>

#include <QVector>
#include <Layer/ArtifactSolidGradientUtil.hpp>

#include <deque>

#include <QtConcurrent>

#include <algorithm>

#include <array>

#include <chrono>

#include <cmath>

#include <cstddef>

#include <cstdint>

#include <cstring>

#include <functional>

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

import Settings.Accessibility;

import Artifact.Render.ROI;

import Artifact.Render.Context;
import Analyze.SmartPalette;
import Color.Harmonizer;

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

import Graphics.Shader.Compute.MaskCutout;

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

QString colorHexLabel(const QColor& color) {
  return color.name(QColor::HexRgb).toUpper();
}

QString colorRgbLabel(const QColor& color) {
  return QStringLiteral("RGB %1,%2,%3")
      .arg(color.red())
      .arg(color.green())
      .arg(color.blue());
}

QString colorHslLabel(const QColor& color) {
  const int hue = color.hslHue() >= 0 ? color.hslHue() : 0;
  return QStringLiteral("HSL %1,%2,%3")
      .arg(hue)
      .arg(color.hslSaturation())
      .arg(color.lightness());
}

QColor qColorFromFloatColor(const FloatColor& color) {
  return QColor::fromRgbF(std::clamp(color.r(), 0.0f, 1.0f),
                          std::clamp(color.g(), 0.0f, 1.0f),
                          std::clamp(color.b(), 0.0f, 1.0f),
                          std::clamp(color.a(), 0.0f, 1.0f));
}

QVector<FloatColor> buildReferenceHarmonyPalette(const FloatColor& baseColor) {
  QVector<FloatColor> palette;
  palette.push_back(baseColor);
  const QList<FloatColor> analogous = ColorHarmonizer::getAnalogous(baseColor);
  for (const auto& color : analogous) {
    palette.push_back(color);
  }
  palette.push_back(ColorHarmonizer::getComplementary(baseColor));
  const QList<FloatColor> triadic = ColorHarmonizer::getTriadic(baseColor);
  if (!triadic.isEmpty()) {
    palette.push_back(triadic.front());
  }
  return palette;
}

Q_LOGGING_CATEGORY(compositionViewLog, "artifact.compositionview")

bool layerHasRasterizerEffectsOrMasks(ArtifactAbstractLayer* targetLayer) {
  if (!targetLayer) {
    return false;
  }
  if (targetLayer->hasMasks()) {
    return true;
  }
  for (const auto& effect : targetLayer->getEffects()) {
    if (effect && effect->isEnabled() &&
        effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
      return true;
    }
  }
  return false;
}

bool solid3DCardColor(ArtifactAbstractLayer* layer, FloatColor& color) {
  if (!layer || !layer->is3D() || layer->isAdjustmentLayer() ||
      layer->layerBlendType() !=
          ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL ||
      layerHasRasterizerEffectsOrMasks(layer) || layer->hasModifiers() ||
      !layer->matteReferences().empty()) {
    return false;
  }

  if (auto* solid2D = dynamic_cast<ArtifactSolid2DLayer*>(layer)) {
    if (solid2D->fillType() != ArtifactSolidFillType::Solid) {
      return false;
    }
    color = solid2D->color();
    return true;
  }
  if (auto* solidImage = dynamic_cast<ArtifactSolidImageLayer*>(layer)) {
    if (solidImage->fillType() != ArtifactSolidFillType::Solid) {
      return false;
    }
    color = solidImage->color();
    return true;
  }
  return false;
}

bool isOpaqueSolid3DCard(ArtifactAbstractLayer* layer) {
  FloatColor color;
  return solid3DCardColor(layer, color) && color.a() >= 0.9999f;
}

struct DirectShape3DCard {
  std::vector<Detail::float2> fillPoints;
  std::vector<Detail::float2> strokePoints;
  FloatColor fillColor;
  FloatColor strokeColor;
  float strokeWidth = 0.0f;
  bool strokeClosed = false;
};

bool directShape3DCard(ArtifactAbstractLayer* layer,
                       DirectShape3DCard& card) {
  if (!layer || !layer->is3D() || layer->isAdjustmentLayer() ||
      layer->layerBlendType() !=
          ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL ||
      layerHasRasterizerEffectsOrMasks(layer) || layer->hasModifiers() ||
      !layer->matteReferences().empty()) {
    return false;
  }
  auto* shapeLayer = dynamic_cast<ArtifactShapeLayer*>(layer);
  if (!shapeLayer) {
    return false;
  }
  const auto appendPoints = [](const std::vector<QPointF>& source,
                               std::vector<Detail::float2>& destination) {
    destination.clear();
    destination.reserve(source.size());
    for (const auto& point : source) {
      destination.push_back({static_cast<float>(point.x()),
                             static_cast<float>(point.y())});
    }
  };
  appendPoints(shapeLayer->direct3DCardFillPoints(), card.fillPoints);
  appendPoints(shapeLayer->direct3DCardStrokePoints(), card.strokePoints);
  card.fillColor = shapeLayer->direct3DCardFillColor();
  card.strokeColor = shapeLayer->direct3DCardStrokeColor();
  card.strokeWidth = shapeLayer->strokeWidth();
  card.strokeClosed = shapeLayer->direct3DCardStrokeClosed();
  return card.fillPoints.size() >= 3 || card.strokePoints.size() >= 2;
}

std::vector<Detail::float2> makeDirectShapeStrokeQuad(
    const Detail::float2& start, const Detail::float2& end, float width) {
  const float dx = end.x - start.x;
  const float dy = end.y - start.y;
  const float length = std::sqrt(dx * dx + dy * dy);
  if (length <= 0.0001f || width <= 0.0f) {
    return {};
  }
  const float halfWidth = width * 0.5f;
  const float nx = -dy / length * halfWidth;
  const float ny = dx / length * halfWidth;
  return {{start.x + nx, start.y + ny}, {end.x + nx, end.y + ny},
          {end.x - nx, end.y - ny}, {start.x - nx, start.y - ny}};
}

const ArtifactCore::ImageF32x4_RGBA* textured3DCardBuffer(
    ArtifactAbstractLayer* layer, QString* sourceKind = nullptr) {
  if (!layer || !layer->is3D() || layer->isAdjustmentLayer() ||
      layer->layerBlendType() !=
          ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL ||
      layerHasRasterizerEffectsOrMasks(layer) || layer->hasModifiers() ||
      !layer->matteReferences().empty()) {
    return nullptr;
  }
  if (auto* imageLayer = dynamic_cast<ArtifactImageLayer*>(layer);
      imageLayer && imageLayer->hasCurrentFrameBuffer()) {
    if (sourceKind) *sourceKind = QStringLiteral("image");
    return &imageLayer->currentFrameBuffer();
  }
  if (auto* svgLayer = dynamic_cast<ArtifactSvgLayer*>(layer);
      svgLayer && svgLayer->isLoaded() && svgLayer->hasCurrentFrameBuffer()) {
    if (sourceKind) *sourceKind = QStringLiteral("svg");
    return &svgLayer->currentFrameBuffer();
  }
  if (auto* textLayer = dynamic_cast<ArtifactTextLayer*>(layer);
      textLayer && textLayer->hasCurrentFrameBuffer()) {
    if (sourceKind) *sourceKind = QStringLiteral("text");
    return &textLayer->currentFrameBuffer();
  }
  return nullptr;
}

ArtifactVideoLayer* readyVideo3DCardLayer(ArtifactAbstractLayer* layer,
                                          int64_t timelineFrame) {
  if (!layer || !layer->is3D() || layer->isAdjustmentLayer() ||
      layer->layerBlendType() !=
          ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL ||
      layerHasRasterizerEffectsOrMasks(layer) || layer->hasModifiers() ||
      !layer->matteReferences().empty()) {
    return nullptr;
  }
  auto* videoLayer = dynamic_cast<ArtifactVideoLayer*>(layer);
  return videoLayer && videoLayer->isLoaded() &&
                 videoLayer->isFrameCached(timelineFrame)
             ? videoLayer
             : nullptr;
}


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



  ctx.effectStrength = layer->effectEnvelope()

      .sample(ctx.layerFrame).effectStrength;



  ctx.sampler = nullptr;

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



bool continuousRenderDiagnosticsEnabled()

{

  static const bool enabled =

      qEnvironmentVariableIsSet("ARTIFACT_ENABLE_CONTINUOUS_RENDER_DIAGNOSTICS") &&

      qEnvironmentVariable("ARTIFACT_ENABLE_CONTINUOUS_RENDER_DIAGNOSTICS") !=

          QStringLiteral("0");

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





QImage makeSkyboxGradientSprite()

{  constexpr int kWidth = 4;

  constexpr int kHeight = 4096;

  QImage image(kWidth, kHeight, QImage::Format_RGBA64);

  image.fill(Qt::transparent);



  QPainter painter(&image);

  QLinearGradient gradient(0.0, 0.0, 0.0, static_cast<qreal>(kHeight));

  gradient.setColorAt(0.00, QColor::fromRgbF(0.12f, 0.20f, 0.45f, 1.0f));

  gradient.setColorAt(0.20, QColor::fromRgbF(0.22f, 0.35f, 0.60f, 1.0f));

  gradient.setColorAt(0.40, QColor::fromRgbF(0.40f, 0.55f, 0.75f, 1.0f));

  gradient.setColorAt(0.60, QColor::fromRgbF(0.60f, 0.72f, 0.85f, 1.0f));

  gradient.setColorAt(0.80, QColor::fromRgbF(0.80f, 0.85f, 0.92f, 1.0f));

  gradient.setColorAt(1.00, QColor::fromRgbF(0.95f, 0.95f, 0.97f, 1.0f));

  painter.fillRect(image.rect(), gradient);

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



enum class TransformFieldDragMode { None, Center, Radius, SecondaryRadius };



bool fieldTargetsSelectedLayers(const CompositionTransformField &field,
                               const QSet<LayerID> &selectedLayerIds)
{
  if (selectedLayerIds.isEmpty()) {
    return false;
  }
  for (const auto &targetLayerId : field.targetLayerIds) {
    if (selectedLayerIds.contains(targetLayerId)) {
      return true;
    }
  }
  return false;
}



QPointF transformFieldDisplayCenter(const ArtifactCompositionPtr &comp,
                                   const CompositionTransformField &field)
{
  QPointF displayCenter = field.center;
  if (!comp || field.coordinateParentLayerId.isNil()) {
    return displayCenter;
  }
  if (const auto parentLayer = comp->layerById(field.coordinateParentLayerId)) {
    displayCenter = parentLayer->getGlobalTransform().map(displayCenter);
  }
  return displayCenter;
}



QPointF transformFieldDisplayRadiusPoint(const ArtifactCompositionPtr &comp,
                                         const CompositionTransformField &field)
{
  const qreal angleRadians =
      field.rotationDegrees * (std::acos(-1.0) / 180.0);
  const QPointF radiusDirection = field.shape == QStringLiteral("linear")
                                      ? QPointF(std::cos(angleRadians),
                                                std::sin(angleRadians))
                                      : QPointF(1.0, 0.0);
  const QPointF radiusPoint = field.center + radiusDirection * field.radius;
  if (!comp || field.coordinateParentLayerId.isNil()) {
    return radiusPoint;
  }
  if (const auto parentLayer = comp->layerById(field.coordinateParentLayerId)) {
    return parentLayer->getGlobalTransform().map(radiusPoint);
  }
  return radiusPoint;
}

QPointF transformFieldDisplaySecondaryRadiusPoint(
    const ArtifactCompositionPtr &comp, const CompositionTransformField &field)
{
  const QPointF radiusPoint =
      field.center + QPointF(0.0, field.secondaryRadius);
  if (!comp || field.coordinateParentLayerId.isNil()) {
    return radiusPoint;
  }
  if (const auto parentLayer = comp->layerById(field.coordinateParentLayerId)) {
    return parentLayer->getGlobalTransform().map(radiusPoint);
  }
  return radiusPoint;
}

std::array<QPointF, 4> transformFieldDisplayBoxCorners(
    const ArtifactCompositionPtr &comp, const CompositionTransformField &field)
{
  std::array<QPointF, 4> corners = {
      field.center + QPointF(-field.radius, -field.secondaryRadius),
      field.center + QPointF(field.radius, -field.secondaryRadius),
      field.center + QPointF(field.radius, field.secondaryRadius),
      field.center + QPointF(-field.radius, field.secondaryRadius),
  };
  if (!comp || field.coordinateParentLayerId.isNil()) {
    return corners;
  }
  if (const auto parentLayer = comp->layerById(field.coordinateParentLayerId)) {
    const QTransform transform = parentLayer->getGlobalTransform();
    for (auto &corner : corners) {
      corner = transform.map(corner);
    }
  }
  return corners;
}



bool transformFieldLocalPointFromCanvas(const ArtifactCompositionPtr &comp,
                                        const CompositionTransformField &field,
                                        const QPointF &canvasPoint,
                                        QPointF &outLocalPoint)
{
  outLocalPoint = canvasPoint;
  if (!comp || field.coordinateParentLayerId.isNil()) {
    return true;
  }
  const auto parentLayer = comp->layerById(field.coordinateParentLayerId);
  if (!parentLayer) {
    return true;
  }
  bool invertible = false;
  const QTransform invTransform = parentLayer->getGlobalTransform().inverted(&invertible);
  if (!invertible) {
    return false;
  }
  outLocalPoint = invTransform.map(canvasPoint);
  return true;
}



bool hitTestTransformFieldHandle(
    const ArtifactCompositionPtr &comp, const QPointF &canvasPos,
    const QSet<LayerID> &selectedLayerIds, qreal hitThreshold,
    QString &outFieldId, TransformFieldDragMode &outMode)
{
  outFieldId.clear();
  outMode = TransformFieldDragMode::None;
  if (!comp) {
    return false;
  }

  qreal bestDistance = std::numeric_limits<qreal>::max();

  for (const auto &field : comp->transformFields()) {
    if (!field.enabled || !fieldTargetsSelectedLayers(field, selectedLayerIds)) {
      continue;
    }

    const QPointF displayCenter = transformFieldDisplayCenter(comp, field);
    const QPointF displayRadiusPoint = transformFieldDisplayRadiusPoint(comp, field);
    const QPointF displaySecondaryRadiusPoint =
        transformFieldDisplaySecondaryRadiusPoint(comp, field);

    const qreal centerDistance = std::hypot(canvasPos.x() - displayCenter.x(),
                                            canvasPos.y() - displayCenter.y());
    if (centerDistance <= hitThreshold && centerDistance < bestDistance) {
      bestDistance = centerDistance;
      outFieldId = field.fieldId;
      outMode = TransformFieldDragMode::Center;
    }

    const qreal radiusDistance = std::hypot(
        canvasPos.x() - displayRadiusPoint.x(),
        canvasPos.y() - displayRadiusPoint.y());
    if (radiusDistance <= hitThreshold && radiusDistance < bestDistance) {
      bestDistance = radiusDistance;
      outFieldId = field.fieldId;
      outMode = TransformFieldDragMode::Radius;
    }

    if (field.shape == QStringLiteral("box")) {
      const qreal secondaryRadiusDistance = std::hypot(
          canvasPos.x() - displaySecondaryRadiusPoint.x(),
          canvasPos.y() - displaySecondaryRadiusPoint.y());
      if (secondaryRadiusDistance <= hitThreshold &&
          secondaryRadiusDistance < bestDistance) {
        bestDistance = secondaryRadiusDistance;
        outFieldId = field.fieldId;
        outMode = TransformFieldDragMode::SecondaryRadius;
      }
    }
  }

  return !outFieldId.isEmpty() && outMode != TransformFieldDragMode::None;
}



void drawTransformFieldBadge(ArtifactIRenderer *renderer,
                             const CompositionTransformField &field,
                             const QPointF &displayCenter,
                             const QPointF &displayRadiusPoint,
                             bool isHoveredField, bool isDraggingField,
                             bool isActiveField, float inverseZoom)
{
  if (!renderer || !isActiveField) {
    return;
  }

  const QString badgeTitle = field.displayName.trimmed().isEmpty()
                                 ? QStringLiteral("Live Field")
                                 : field.displayName.trimmed();
  const QString blendMode = field.blendMode.trimmed().toLower();
  const QString blendLabel =
      blendMode == QStringLiteral("additive")
          ? QStringLiteral("Additive")
          : blendMode == QStringLiteral("multiply")
                ? QStringLiteral("Multiply")
                : blendMode == QStringLiteral("screen")
                      ? QStringLiteral("Screen")
                      : QStringLiteral("Normal");
  const QString shapeLabel = field.shape == QStringLiteral("box")
                                 ? QStringLiteral("BOX")
                                 : field.shape == QStringLiteral("linear")
                                       ? QStringLiteral("LINEAR")
                                       : QStringLiteral("RADIAL");
  const QString badgeSubtitle = QStringLiteral("%1 / %2 / %3 / %4")
                                   .arg(QStringLiteral("ACTIVE"), shapeLabel, blendLabel,
                                        field.enabled ? QStringLiteral("enabled")
                                                      : QStringLiteral("disabled"));
  const QString badgeDetails = QStringLiteral("%1 / %2 / scale %3 / time %4s")
                                   .arg(QStringLiteral("strength %1")
                                            .arg(QString::number(field.strength, 'f', 1)),
                                         field.invert ? QStringLiteral("invert on")
                                                     : QStringLiteral("invert off"),
                                         QString::number(field.edgeScale, 'f', 2),
                                         QString::number(field.timeOffsetSeconds, 'f', 2));

  const QPointF anchor =
      QPointF((displayCenter.x() + displayRadiusPoint.x()) * 0.5,
              (displayCenter.y() + displayRadiusPoint.y()) * 0.5);
  const double accessibilityFontScale =
      std::max(1.0f, Accessibility::fontScale());
  const float panelW =
      std::clamp(160.0f + static_cast<float>(badgeTitle.size()) * 4.5f, 160.0f,
                 320.0f);
  const float panelH = 54.0f * static_cast<float>(accessibilityFontScale);
  const float panelX = static_cast<float>(anchor.x()) + 12.0f;
  const float panelY = static_cast<float>(anchor.y()) - 10.0f - panelH;

  const FloatColor panelFill =
      isDraggingField
          ? FloatColor{0.16f, 0.10f, 0.05f, 0.88f}
          : isHoveredField
                ? FloatColor{0.10f, 0.12f, 0.08f, 0.86f}
                : FloatColor{0.05f, 0.12f, 0.08f, 0.82f};
  const FloatColor panelBorder =
      isDraggingField
          ? FloatColor{1.0f, 0.55f, 0.18f, 0.96f}
          : isHoveredField ? FloatColor{1.0f, 0.83f, 0.32f, 0.94f}
                           : FloatColor{0.38f, 0.95f, 0.63f, 0.94f};
  const float contrastScale = Accessibility::contrastScale();

  renderer->drawOverlayPanel(panelX, panelY, panelW, panelH, panelFill,
                             panelBorder);

  QFont titleFont = QApplication::font();
  titleFont.setPointSizeF(
      std::max(9.0, static_cast<double>(titleFont.pointSizeF()) *
                          accessibilityFontScale));
  titleFont.setWeight(QFont::DemiBold);
  QFont detailFont = QApplication::font();
  detailFont.setPointSizeF(
      std::max(7.5, (static_cast<double>(detailFont.pointSizeF()) - 0.5) *
                        accessibilityFontScale));

  renderer->drawText(
      QRectF(panelX + 10.0f, panelY + 6.0f, panelW - 20.0f, 16.0f),
      badgeTitle, titleFont, FloatColor{0.98f, 0.99f, 1.0f, 1.0f},
      Qt::AlignLeft | Qt::AlignVCenter);
  renderer->drawText(
      QRectF(panelX + 10.0f, panelY + 22.0f, panelW - 20.0f, 14.0f),
      badgeSubtitle, detailFont, FloatColor{0.88f, 0.94f, 0.91f, 1.0f},
      Qt::AlignLeft | Qt::AlignVCenter);
  renderer->drawText(
      QRectF(panelX + 10.0f, panelY + 36.0f, panelW - 20.0f, 12.0f),
      badgeDetails, detailFont, FloatColor{0.80f, 0.88f, 0.84f, 1.0f},
      Qt::AlignLeft | Qt::AlignVCenter);

  const float tagSize = std::max(8.0f, 10.0f * inverseZoom);
  renderer->drawCircle(static_cast<float>(displayCenter.x()),
                       static_cast<float>(displayCenter.y()),
                       tagSize * 1.4f, panelBorder, 1.4f * contrastScale, true);
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

  // Keep the legacy QImage boundary in the same internal layout as the
  // float-image path. qImageToCvMat() returns BGRA for ARGB32 on Windows and
  // setFromCVMat() preserves CV_32FC4 channel order. The GPU upload boundary
  // owns the single BGRA -> RGBA conversion; converting here as well swaps
  // red and blue again for effect-processed surfaces.
  if (mat.type() != CV_32FC4) {

    mat.convertTo(mat, CV_32FC4, 1.0 / 255.0);

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



  const QMatrix4x4 globalMat = layer->getGlobalTransform4x4();

  const QVector3D position = globalMat.map(QVector3D(0.0f, 0.0f, 0.0f));

  QVector3D direction = globalMat.mapVector(QVector3D(0.0f, 0.0f, 1.0f));

  if (direction.lengthSquared() <= 0.000001f) {

    direction = QVector3D(0.0f, 0.0f, 1.0f);

  } else {

    direction.normalize();

  }



  light.setPosition(ArtifactCore::float3<float>{position.x(), position.y(), position.z()});

  light.setDirection(

      ArtifactCore::float3<float>{direction.x(), direction.y(), direction.z()});



  if (layer->lightType() == LightType::Point) {

    light.setRange(std::max(10.0f, layer->shadowRadius() * 10.0f));

  }

  if (layer->lightType() == LightType::Spot) {

    light.setRange(layer->coneLength());

    const float outerHalfAngle = layer->coneAngle() * 0.5f;

    const float innerHalfAngle =

        std::max(0.0f, layer->coneAngle() - layer->coneFeather()) * 0.5f;

    light.setCutoff(innerHalfAngle, outerHalfAngle);

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

                                  const QImage &surface, int64_t frameNumber,

                                  quint64 surfaceGeneration) {

  if (!layer) {

    return QString();

  }



  QString key = layer->id().toString();

  key +=

      QStringLiteral("|size=%1x%2").arg(surface.width()).arg(surface.height());

  key += QStringLiteral("|generation=%1").arg(surfaceGeneration);

  bool hasAnimatedEffectProperty = false;
  for (const auto &effect : layer->getEffects()) {
    if (!effect || !effect->isEnabled()) {
      continue;
    }
    for (const auto &property : effect->editableProperties()) {
      if (property && (!property->getKeyFrames().empty() ||
                       property->hasExpression() ||
                       property->hasEnvelopes())) {
        hasAnimatedEffectProperty = true;
        break;
      }
    }
    if (hasAnimatedEffectProperty) {
      break;
    }
  }
  if (hasAnimatedEffectProperty) {
    key += QStringLiteral("|effectFrame=%1").arg(frameNumber);
  }



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

enum class MotionPathTangentHandle { None, In, Out };

struct MotionPathTangentSnapshot {
  bool present = false;
  ArtifactCore::PositionSpatialTangents tangents;
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

class MotionPathTangentUndoCommand final : public UndoCommand {
public:
  MotionPathTangentUndoCommand(ArtifactAbstractLayerPtr layer, int64_t frame,
                               MotionPathTangentSnapshot before,
                               MotionPathTangentSnapshot after)
      : layer_(layer), frame_(frame), before_(before), after_(after) {}

  void undo() override { apply(before_); }
  void redo() override { apply(after_); }
  QString label() const override { return QStringLiteral("Edit Motion Path Tangent"); }

private:
  void apply(const MotionPathTangentSnapshot &snapshot) {
    auto layer = layer_.lock();
    if (!layer) return;
    const ArtifactCore::RationalTime time(frame_, 24);
    auto &t3d = layer->transform3D();
    if (snapshot.present) {
      t3d.setPositionKeyFrameSpatialTangentsAt(time, snapshot.tangents);
    } else {
      t3d.removePositionKeyFrameSpatialTangentsAt(time);
    }
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    if (auto *comp = static_cast<ArtifactAbstractComposition *>(layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
    if (auto *mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
  }

  ArtifactAbstractLayerWeak layer_;
  int64_t frame_ = 0;
  MotionPathTangentSnapshot before_;
  MotionPathTangentSnapshot after_;
};



class TransformFieldUndoCommand final : public UndoCommand {

public:

  TransformFieldUndoCommand(ArtifactCompositionWeakPtr composition,
                            CompositionTransformField before,
                            CompositionTransformField after)
      : composition_(std::move(composition)),
        before_(std::move(before)),
        after_(std::move(after))
  {
  }

  void undo() override { apply(before_); }

  void redo() override { apply(after_); }

  QString label() const override
  {
    return QStringLiteral("Edit Live Field");
  }

private:
  void apply(const CompositionTransformField &field)
  {
    if (const auto composition = composition_.lock()) {
      composition->addTransformField(field);
      composition->changed();
      if (auto *mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
      }
    }
  }

  ArtifactCompositionWeakPtr composition_;
  CompositionTransformField before_;
  CompositionTransformField after_;
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

    const QQuaternion& orientation, const QPointF& target,

    const float distance) {

  const QQuaternion cameraOrientation = orientation.conjugated();
  const QVector3D targetPosition(
      static_cast<float>(target.x()), static_cast<float>(target.y()), 0.0f);

  const QVector3D eye =

      targetPosition +
      cameraOrientation.rotatedVector(QVector3D(0.0f, 0.0f, 1.0f)) *

                   std::max(1.0f, distance);

  QVector3D up =
      cameraOrientation.rotatedVector(QVector3D(0.0f, 1.0f, 0.0f));

  if (up.lengthSquared() < 1.0e-6f) {

    up = QVector3D(0.0f, 1.0f, 0.0f);

  }

  QMatrix4x4 view;

  view.lookAt(eye, targetPosition, up.normalized());

  return view;

}



QMatrix4x4 viewportOrientationProjectionMatrix(const float viewportW,

                                               const float viewportH,

                                               const float fovDegrees) {

  QMatrix4x4 proj;

  const float w = std::max(1.0f, viewportW);

  const float h = std::max(1.0f, viewportH);

  const float aspect = w / h;

  proj.perspective(std::clamp(fovDegrees, 8.0f, 120.0f), aspect, 1.0f,

                   100000.0f);
  proj(1, 1) = -proj(1, 1);

  return proj;

}



// Forward declaration

FramePosition currentFrameForComposition(const ArtifactCompositionPtr &comp);

std::vector<RationalTime> motionPathPositionKeyTimes(
    const ArtifactAbstractLayerPtr &layer, const int fps) {
  std::vector<int64_t> frames;
  if (!layer) {
    return {};
  }

  const auto collectPropertyFrames =
      [&frames, fps](const auto &property) {
        if (!property || !property->isAnimatable()) {
          return;
        }
        for (const auto &keyframe : property->getKeyFrames()) {
          frames.push_back(keyframe.time.rescaledTo(fps));
        }
      };
  collectPropertyFrames(
      layer->getProperty(QStringLiteral("transform.position.x")));
  collectPropertyFrames(
      layer->getProperty(QStringLiteral("transform.position.y")));

  if (frames.empty()) {
    for (const auto &time : layer->transform3D().getPositionKeyFrameTimes()) {
      frames.push_back(time.rescaledTo(fps));
    }
  }

  std::sort(frames.begin(), frames.end());
  frames.erase(std::unique(frames.begin(), frames.end()), frames.end());

  std::vector<RationalTime> times;
  times.reserve(frames.size());
  for (const int64_t frame : frames) {
    times.emplace_back(frame, fps);
  }
  return times;
}

int motionPathAdaptiveSampleStep(int minFrame, int maxFrame, float zoom);

int motionPathPositionInterpolation(
    const ArtifactAbstractLayerPtr &layer, const RationalTime &time) {
  if (layer) {
    const auto property =
        layer->getProperty(QStringLiteral("transform.position.x"));
    if (property && property->isAnimatable()) {
      for (const auto &keyframe : property->getKeyFrames()) {
        if (keyframe.time == time ||
            keyframe.time.rescaledTo(time.scale()) == time.value()) {
          return static_cast<int>(keyframe.interpolation);
        }
      }
    }
    return static_cast<int>(
        layer->transform3D().positionXKeyFrameInterpolationAt(time));
  }
  return static_cast<int>(ArtifactCore::InterpolationType::Linear);
}



QVector<MotionPathSample>

buildMotionPathSamples(const ArtifactAbstractLayerPtr &layer,

                       const ArtifactCompositionPtr &comp) {

  QVector<MotionPathSample> samples;

  if (!layer || !comp) {

    return samples;

  }



  const int fps =
      std::max(1, static_cast<int>(std::round(comp->frameRate().framerate())));
  const auto keyTimes = motionPathPositionKeyTimes(layer, fps);

  if (keyTimes.empty()) {

    return samples;

  }



  samples.reserve(static_cast<int>(keyTimes.size()) + 1);

  for (const auto &time : keyTimes) {
    const int64_t frame = time.rescaledTo(fps);
    const auto &transform = layer->transform3D();
    const QTransform globalTransform = layer->getGlobalTransformAt(frame);
    const QPointF anchor = globalTransform.map(
        QPointF(transform.anchorXAt(time), transform.anchorYAt(time)));
    samples.push_back(
        {anchor, MotionPathSampleKind::Keyframe, frame});

  }



  const FramePosition currentFrame = currentFrameForComposition(comp);

  const RationalTime currentTime(currentFrame.framePosition(), fps);

  const auto &transform = layer->transform3D();
  const QTransform currentGlobalTransform =
      layer->getGlobalTransformAt(currentFrame.framePosition());
  const QPointF currentAnchor = currentGlobalTransform.map(
      QPointF(transform.anchorXAt(currentTime),
              transform.anchorYAt(currentTime)));
  samples.push_back({currentAnchor, MotionPathSampleKind::Current,
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
    const QMatrix4x4 *previousCameraView = nullptr,
    const QMatrix4x4 *previousCameraProj = nullptr,
    const std::function<QImage(const ArtifactCore::Id &)> *matteSourceResolver = nullptr,

    const std::vector<SceneLightEntry> *sceneLights = nullptr,

    bool interactiveDraft = false, bool applyViewportCameraTo2D = false,
    quint64 surfaceGeneration = 1,
    const std::function<Diligent::ITextureView*(ArtifactCompositionLayer*,
                                                int64_t, const QSize&)>*
        precompGpuResolver = nullptr) {

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


  FloatColor cardColor;

  if (solid3DCardColor(layer, cardColor)) {

    const float cardOpacity =

        opacityOverride >= 0.0f ? opacityOverride : layer->opacity();

    const bool writeDepth = cardOpacity * cardColor.a() >= 0.9999f;

    if (cameraView && cameraProj) {

      renderer->set3DCameraMatrices(*cameraView, *cameraProj);

    }

    renderer->draw3DCard(localRect, globalTransform4x4, cardColor,

                         cardOpacity, writeDepth);

    if (cameraView && cameraProj) {

      renderer->reset3DCameraMatrices();

    }

    return;

  }

  DirectShape3DCard shapeCard;

  if (directShape3DCard(layer, shapeCard)) {

    const float cardOpacity =

        opacityOverride >= 0.0f ? opacityOverride : layer->opacity();

    const bool fillWriteDepth =

        cardOpacity * shapeCard.fillColor.a() >= 0.9999f;

    const bool strokeWriteDepth =

        cardOpacity * shapeCard.strokeColor.a() >= 0.9999f;

    if (cameraView && cameraProj) {

      renderer->set3DCameraMatrices(*cameraView, *cameraProj);

    }

    if (shapeCard.fillPoints.size() >= 3) {

      renderer->draw3DShape(shapeCard.fillPoints, globalTransform4x4,

                            shapeCard.fillColor, cardOpacity,

                            fillWriteDepth);

    }

    if (shapeCard.strokePoints.size() >= 2 &&

        shapeCard.strokeWidth > 0.0f) {

      const size_t strokeSegmentCount =

          shapeCard.strokeClosed ? shapeCard.strokePoints.size()

                                 : shapeCard.strokePoints.size() - 1;

      for (size_t segment = 0; segment < strokeSegmentCount; ++segment) {

        const auto& start = shapeCard.strokePoints[segment];

        const auto& end = shapeCard.strokePoints[

            (segment + 1) % shapeCard.strokePoints.size()];

        const auto quad = makeDirectShapeStrokeQuad(

            start, end, shapeCard.strokeWidth);

        if (quad.size() == 4) {

          renderer->draw3DShape(quad, globalTransform4x4,

                                shapeCard.strokeColor, cardOpacity,

                                strokeWriteDepth);

        }

      }

    }

    if (cameraView && cameraProj) {

      renderer->reset3DCameraMatrices();

    }

    return;

  }

  QString texturedCardKind;

  if (const auto* cardBuffer =

          textured3DCardBuffer(layer, &texturedCardKind);

      cardBuffer && gpuTextureCacheManager) {

    QString textureOwner = layer->id().toString();

    QString textureKey = QStringLiteral("3d-card:%1:g%2:f%3")

                             .arg(texturedCardKind)

                             .arg(surfaceGeneration)

                             .arg(cacheFrameNumber);

    if (auto* imageCard = dynamic_cast<ArtifactImageLayer*>(layer);

        imageCard && imageCard->canShareSourceGpuTexture() &&

        !imageCard->sourceAssetId().isNull() &&

        imageCard->sourceVersion() > 0) {

      textureOwner = QStringLiteral("asset:%1").arg(

          imageCard->sourceAssetId().toString(QUuid::WithoutBraces));

      textureKey = QStringLiteral("image-f32:v%1|3d-card")

                       .arg(imageCard->sourceVersion());

    }

    const auto handle = gpuTextureCacheManager->acquireOrCreate(

        textureOwner, textureKey, *cardBuffer);

    const auto binding = gpuTextureCacheManager->bindingRecord(handle);

    if (binding.isValid()) {

      const float cardOpacity =

          opacityOverride >= 0.0f ? opacityOverride : layer->opacity();

      if (cameraView && cameraProj) {

        renderer->set3DCameraMatrices(*cameraView, *cameraProj);

      }

      renderer->draw3DTexturedCard(localRect, globalTransform4x4,

                                   binding.srv, cardOpacity);

      if (cameraView && cameraProj) {

        renderer->reset3DCameraMatrices();

      }

      return;

    }

  }

  if (auto* videoCard = readyVideo3DCardLayer(layer, cacheFrameNumber);

      videoCard && gpuTextureCacheManager) {

    const auto frameBuffer =

        videoCard->cachedFrameImageBuffer(cacheFrameNumber);

    if (!frameBuffer.isEmpty()) {

      const QString textureOwner = layer->id().toString();

      const QString textureKey = QStringLiteral("3d-card:video:s%1:g%2")

                                     .arg(videoCard->currentSourceFrameValue())

                                     .arg(surfaceGeneration);

      const auto handle = gpuTextureCacheManager->acquireOrCreate(

          textureOwner, textureKey, frameBuffer);

      const auto binding = gpuTextureCacheManager->bindingRecord(handle);

      if (binding.isValid()) {

        const float cardOpacity =

            opacityOverride >= 0.0f ? opacityOverride : layer->opacity();

        videoCard->markFrameRenderQueued(cacheFrameNumber);

        if (cameraView && cameraProj) {

          renderer->set3DCameraMatrices(*cameraView, *cameraProj);

        }

        renderer->draw3DTexturedCard(localRect, globalTransform4x4,

                                     binding.srv, cardOpacity);

        if (cameraView && cameraProj) {

          renderer->reset3DCameraMatrices();

        }

        return;

      }

    }

  }



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
      if (previousCameraView && previousCameraProj) {
        renderer->setPrevious3DCameraMatrices(*previousCameraView,
                                              *previousCameraProj);
      }
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

  struct External2DMatrixScope {
    ArtifactIRenderer *renderer = nullptr;
    bool active = false;

    ~External2DMatrixScope() {
      if (renderer && active) {
        renderer->setUseExternalMatrices(false);
      }
    }
  } external2DMatrixScope{
      renderer, applyViewportCameraTo2D && cameraView && cameraProj};

  if (external2DMatrixScope.active) {
    renderer->setViewMatrix(*cameraView);
    renderer->setProjectionMatrix(*cameraProj);
    renderer->setUseExternalMatrices(true);
  }

  auto applySurfaceAndDraw = [&](QImage surface, const QRectF &rect,

                                 bool allowSurfaceCache) {

    if (surface.isNull()) {

      return false;

    }



    QHash<ArtifactCore::Id, QImage> resolvedMatteSources;

    QString matteSourceSignature;

    if (matteSourceResolver && layer->matteReferences().size() > 0) {

      for (const auto &ref : layer->matteReferences()) {

        if (!ref.enabled || ref.sourceLayerId.isNil()) {

          continue;

        }

        QImage source = (*matteSourceResolver)(ref.sourceLayerId);

        if (source.isNull()) {

          matteSourceSignature +=

              QStringLiteral("|matte=%1:null").arg(ref.sourceLayerId.toString());

          continue;

        }

        matteSourceSignature +=

            QStringLiteral("|matte=%1:%2:%3x%4")

                .arg(ref.sourceLayerId.toString())

                .arg(static_cast<qulonglong>(source.cacheKey()))

                .arg(source.width())

                .arg(source.height());

        resolvedMatteSources.insert(ref.sourceLayerId, std::move(source));

      }

    }



    const QString ownerId = layer->id().toString();

    QString cacheSignature =

        buildLayerSurfaceCacheKey(layer, surface, cacheFrameNumber,

                                  surfaceGeneration);

    cacheSignature +=

        QStringLiteral("|interactiveDraft=%1").arg(interactiveDraft ? 1 : 0);

    cacheSignature += matteSourceSignature;

    QString gpuOwnerId = ownerId;
    QString gpuCacheSignature = cacheSignature;
    if (!allowSurfaceCache && matteSourceSignature.isEmpty()) {
      if (auto* imageLayer = dynamic_cast<ArtifactImageLayer*>(layer);
          imageLayer && imageLayer->canShareSourceGpuTexture()) {
        const auto version = imageLayer->sourceVersion();
        if (version > 0) {
          gpuOwnerId = QStringLiteral("asset:%1").arg(
              imageLayer->sourceAssetId().toString(QUuid::WithoutBraces));
          gpuCacheSignature = QStringLiteral("image-f32:v%1").arg(version);
        }
      }
    }

    auto applyResolvedMattes = [&](QImage& targetSurface) {
      if (targetSurface.isNull() || resolvedMatteSources.isEmpty()) {
        return;
      }

      const std::function<QImage(const ArtifactCore::Id&)> cachedResolver =
          [&resolvedMatteSources](const ArtifactCore::Id& id) {
            return resolvedMatteSources.value(id);
          };

      applyLayerMatteToSurface(layer, targetSurface, cachedResolver);
    };

    const QString cacheSlotKey =

        ownerId + (interactiveDraft ? QStringLiteral("|draft")

                                    : QStringLiteral("|full"));

    LayerSurfaceCacheEntry *cacheEntry = nullptr;
    std::shared_ptr<ArtifactCore::ImageF32x4_RGBA> directProcessedBuffer;



    const bool useSurfaceCache = surfaceCache && !cacheSignature.isEmpty();



    if (useSurfaceCache) {

      auto cacheIt = surfaceCache->find(cacheSlotKey);

      if (cacheIt != surfaceCache->end() && cacheIt->ownerId == ownerId &&

          cacheIt->cacheSignature == cacheSignature &&

          (!cacheIt->processedSurface.isNull() || cacheIt->processedBuffer)) {

        cacheEntry = &(*cacheIt);
        directProcessedBuffer = cacheEntry->processedBuffer;

        if (!cacheIt->processedSurface.isNull()) {

          surface = cacheIt->processedSurface;

        }

      } else {

        std::shared_ptr<ArtifactCore::ImageF32x4_RGBA> processedBuffer;

        if (allowSurfaceCache) {

          ArtifactCore::ImageF32x4_RGBA processed;

          // Downsampling already controls preview cost. Skipping the rasterizer
          // here makes Blur and Gaussian Blur disappear in Draft/interactive views.
          if (buildRasterizedSurfaceBuffer(layer, surface, &processed)) {

            processedBuffer =

                std::make_shared<ArtifactCore::ImageF32x4_RGBA>(processed);
            directProcessedBuffer = processedBuffer;

          }

        }

        if (processedBuffer && !resolvedMatteSources.isEmpty()) {
          // Preserve the rasterizer output. Replacing it from the original
          // surface discarded Blur / Gaussian Blur / Glow on GPU-cached layers.
          QImage processedSurface = processedBuffer->toQImage();
          applyResolvedMattes(processedSurface);
          ArtifactCore::ImageF32x4_RGBA mattedProcessed;
          mattedProcessed.setFromCVMat(
              ArtifactCore::CvUtils::qImageToCvMat(processedSurface, true));
          processedBuffer =
              std::make_shared<ArtifactCore::ImageF32x4_RGBA>(mattedProcessed);
          directProcessedBuffer = processedBuffer;
          surface = std::move(processedSurface);
        } else {
          applyResolvedMattes(surface);
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

                gpuOwnerId, gpuCacheSignature, *entry.processedBuffer);

          } else {

            entry.gpuTextureHandle = gpuTextureCacheManager->acquireOrCreate(

                gpuOwnerId, gpuCacheSignature, surface);

          }

          if (entry.processedBuffer) {
            // Keep the processed buffer authoritative even when the first GPU
            // upload fails; the draw fallback will materialize this result.
            entry.processedSurface = QImage();
          }

        }

        (*surfaceCache)[cacheSlotKey] = entry;

        cacheEntry = &(*surfaceCache)[cacheSlotKey];
        directProcessedBuffer = cacheEntry->processedBuffer;

      }

    } else if (allowSurfaceCache) {

      ArtifactCore::ImageF32x4_RGBA processed;

      if (buildRasterizedSurfaceBuffer(layer, surface, &processed) &&

          !processed.isEmpty()) {
        directProcessedBuffer =
            std::make_shared<ArtifactCore::ImageF32x4_RGBA>(processed);

      }

      if (directProcessedBuffer && !resolvedMatteSources.isEmpty()) {
        // Matte resolution is still a QImage-only compatibility boundary.
        // Re-enter the float path immediately after applying it.
        QImage processedSurface = directProcessedBuffer->toQImage();
        applyResolvedMattes(processedSurface);
        ArtifactCore::ImageF32x4_RGBA mattedProcessed;
        mattedProcessed.setFromCVMat(
            ArtifactCore::CvUtils::qImageToCvMat(processedSurface, true));
        directProcessedBuffer =
            std::make_shared<ArtifactCore::ImageF32x4_RGBA>(mattedProcessed);
        surface = std::move(processedSurface);
      } else if (!directProcessedBuffer) {
        applyResolvedMattes(surface);
      }

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

                        gpuOwnerId, gpuCacheSignature,

                        *cacheEntry->processedBuffer);

              } else {

                cacheEntry->gpuTextureHandle =

                    gpuTextureCacheManager->acquireOrCreate(

                        gpuOwnerId, gpuCacheSignature, uploadSurface);

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

            directProcessedBuffer = cacheEntry->processedBuffer;

          }



          if (directProcessedBuffer && !directProcessedBuffer->isEmpty()) {
            renderer->drawSpriteTransformed(
                static_cast<float>(rect.x()), static_cast<float>(rect.y()),
                static_cast<float>(rect.width()),
                static_cast<float>(rect.height()), instanceTransform,
                *directProcessedBuffer, finalOpacity);
          } else {
            renderer->drawSpriteTransformed(
                static_cast<float>(rect.x()), static_cast<float>(rect.y()),
                static_cast<float>(rect.width()),
                static_cast<float>(rect.height()), instanceTransform, surface,
                finalOpacity);
          }

        });

    return true;

  };



  if (auto *solid2D = dynamic_cast<ArtifactSolid2DLayer *>(layer)) {

    const auto color = solid2D->color();

    if (layerHasRasterizerEffectsOrMasks(layer)) {
      const QSize surfaceSize(

          std::max(1, static_cast<int>(std::ceil(localRect.width()))),

          std::max(1, static_cast<int>(std::ceil(localRect.height()))));

      QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
      if (solid2D->fillType() != ArtifactSolidFillType::Solid) {
        surface = ArtifactSolidGradientUtil::makeSolidGradientImage(
            surfaceSize,
            QColor::fromRgbF(solid2D->gradientStartColor().r(), solid2D->gradientStartColor().g(),
                             solid2D->gradientStartColor().b(), solid2D->gradientStartColor().a()),
            QColor::fromRgbF(solid2D->gradientEndColor().r(), solid2D->gradientEndColor().g(),
                             solid2D->gradientEndColor().b(), solid2D->gradientEndColor().a()),
            static_cast<int>(solid2D->fillType()), solid2D->gradientAngleDegrees(),
            solid2D->gradientReverse(), solid2D->gradientCenterX(), solid2D->gradientCenterY(),
            solid2D->gradientScale(), solid2D->gradientOffset());
      } else {
        surface.fill(toQColor(color));
      }

      applySurfaceAndDraw(surface, localRect, true);

    } else {

      const float baseOpacity =

          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());

      if (solid2D->fillType() != ArtifactSolidFillType::Solid) {
        drawWithClonerEffect(
            layer, globalTransform4x4,
            [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
              renderer->drawGradientRectTransformed(
                  static_cast<float>(localRect.x()), static_cast<float>(localRect.y()),
                  static_cast<float>(localRect.width()), static_cast<float>(localRect.height()),
                  instanceTransform, solid2D->gradientStartColor(), solid2D->gradientEndColor(),
                  static_cast<int>(solid2D->fillType()), solid2D->gradientAngleDegrees(),
                  solid2D->gradientReverse(), solid2D->gradientCenterX(), solid2D->gradientCenterY(),
                  solid2D->gradientScale(), solid2D->gradientOffset(), baseOpacity * instanceWeight);
            });
        return;
      }

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

    if (layerHasRasterizerEffectsOrMasks(layer)) {
      const QSize surfaceSize(

          std::max(1, static_cast<int>(std::ceil(localRect.width()))),

          std::max(1, static_cast<int>(std::ceil(localRect.height()))));

      QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
      if (solidImage->fillType() != ArtifactSolidFillType::Solid) {
        surface = solidImage->toQImage();
      } else {
        surface.fill(toQColor(color));
      }

      applySurfaceAndDraw(surface, localRect, true);

    } else {

      const float baseOpacity =

          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());

      if (solidImage->fillType() != ArtifactSolidFillType::Solid) {
        drawWithClonerEffect(
            layer, globalTransform4x4,
            [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
              renderer->drawGradientRectTransformed(
                  static_cast<float>(localRect.x()), static_cast<float>(localRect.y()),
                  static_cast<float>(localRect.width()), static_cast<float>(localRect.height()),
                  instanceTransform, solidImage->gradientStartColor(), solidImage->gradientEndColor(),
                  static_cast<int>(solidImage->fillType()), solidImage->gradientAngleDegrees(),
                  solidImage->gradientReverse(), solidImage->gradientCenterX(), solidImage->gradientCenterY(),
                  solidImage->gradientScale(), solidImage->gradientOffset(), baseOpacity * instanceWeight);
            });
        return;
      }

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

    if (!layerHasRasterizerEffectsOrMasks(layer) &&
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

      applySurfaceAndDraw(img, localRect, layerHasRasterizerEffectsOrMasks(layer));
      return;

    }

  }



  if (auto *svgLayer = dynamic_cast<ArtifactSvgLayer *>(layer)) {

    if (svgLayer->isLoaded()) {

      if (!layerHasRasterizerEffectsOrMasks(layer) &&
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

                              layerHasRasterizerEffectsOrMasks(layer));
        } else {

          svgLayer->draw(renderer);

        }

      }

      return;

    }

  }



  if (auto *videoLayer = dynamic_cast<ArtifactVideoLayer *>(layer)) {

    const bool hasRasterizer = layerHasRasterizerEffectsOrMasks(layer);
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

    if (!layerHasRasterizerEffectsOrMasks(layer)) {
      textLayer->draw(renderer);

      return;

    }

    if (textLayer->hasCurrentFrameBuffer()) {

      ArtifactCore::ImageF32x4_RGBA buffer =

          textLayer->currentFrameBuffer().DeepCopy();

      applyRasterizerEffectsAndMasksToSurface(layer, buffer, lod);

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

      // Master Properties are instance overrides. Apply them only while this
      // precomp instance is sampled, then restore the shared child composition
      // so sibling instances cannot leak values into one another.
      const int64_t childFrame =
          cacheFrameNumber == std::numeric_limits<int64_t>::min()
              ? childComp->framePosition().framePosition()
              : static_cast<int64_t>(std::llround(
                    layer->getSourceFrameAtCompFrame(cacheFrameNumber)));
      if (layer->is3D() && precompGpuResolver) {
        if (auto* childSRV = (*precompGpuResolver)(compLayer, childFrame,
                                                   childSize)) {
          renderer->draw3DTexturedCard(localRect, layer->getGlobalTransform4x4(),
                                       childSRV,
                                       opacityOverride >= 0.0f
                                           ? opacityOverride
                                           : layer->opacity());
          return;
        }
      }
      const FramePosition childRestoreFrame = childComp->framePosition();
      if (childRestoreFrame.framePosition() != childFrame) {
        childComp->goToFrame(childFrame);
      }
      // Resolve authored animation/expression state first. The instance scope
      // is then the final value layer for this sample, including recursively
      // exposed Master Properties on nested precomp layers.
      const bool overrideScopeActive =
          compLayer->beginExposedPropertyOverrideScope();
      QImage childImage = childComp->getThumbnailAtFrame(
          childFrame, childSize.width(), childSize.height());
      if (overrideScopeActive) {
        compLayer->endExposedPropertyOverrideScope();
      }
      if (childRestoreFrame.framePosition() != childFrame) {
        childComp->goToFrame(childRestoreFrame.framePosition());
      }



      if (!childImage.isNull()) {

        applySurfaceAndDraw(childImage, localRect,

                            layerHasRasterizerEffectsOrMasks(layer));
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

                                     const QImage &mayaGradientSprite,
                                     const QMatrix4x4 *viewportView = nullptr,
                                     const QMatrix4x4 *viewportProjection =
                                         nullptr) {

  if (!renderer || cw <= 0.0f || ch <= 0.0f) {

    return;

  }

  const auto drawCanvasFill =
      [&](const FloatColor &color) {
        if (viewportView && viewportProjection) {
          QMatrix4x4 identity;
          renderer->setViewMatrix(*viewportView);
          renderer->setProjectionMatrix(*viewportProjection);
          renderer->setUseExternalMatrices(true);
          renderer->drawSolidRectTransformed(0.0f, 0.0f, cw, ch, identity,
                                             color, 1.0f);
          renderer->setUseExternalMatrices(false);
          return;
        }
        renderer->drawRectLocal(0.f, 0.f, cw, ch, color, 1.0f);
      };

  if (mode == CompositionBackgroundMode::Solid) {

    if (bgColor.a() > 0.0f) {

      drawCanvasFill(bgColor);

    }

    return;

  }

  if (mode == CompositionBackgroundMode::Checkerboard) {

    // Checkerboard is the viewport background; the composition area itself

    // should stay filled with the composition bg color.

    if (bgColor.a() > 0.0f) {

      drawCanvasFill(bgColor);

    }

    return;

  }

  // MayaGradient only affects the viewport background; the composition area

  // itself is always rendered with the solid bgColor.

  if (mode == CompositionBackgroundMode::MayaGradient) {

    if (bgColor.a() > 0.0f) {

      drawCanvasFill(bgColor);

    }

    return;

  }

  // Skybox renders a sky-like gradient covering the composition area

  if (mode == CompositionBackgroundMode::Skybox)

  {

    QImage skyGradient = makeSkyboxGradientSprite();

    if (!skyGradient.isNull())

    {

      renderer->drawSpriteTransformed(0.f, 0.f, cw, ch, QMatrix4x4{}, skyGradient, 1.0f);

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

  struct PrecompGpuOutputEntry {
    void* colorTargetView = nullptr;
    void* depthTargetView = nullptr;
    Diligent::ITextureView* colorShaderResourceView = nullptr;
    QSize size;
    qint64 frame = std::numeric_limits<qint64>::min();
    quint64 generation = 0;
    bool ready = false;
  };

  QHash<QString, PrecompGpuOutputEntry> precompGpuOutputs_;

  void clearPrecompGpuOutputs() {
    if (renderer_) {
      for (auto& entry : precompGpuOutputs_) {
        renderer_->destroyOffscreenTexture(entry.colorTargetView);
        renderer_->destroyOffscreenTexture(entry.depthTargetView);
      }
    }
    precompGpuOutputs_.clear();
  }

  PrecompGpuOutputEntry* preparePrecompGpuOutput(
      const QString& compositionId, qint64 frame, const QSize& size,
      quint64 generation) {
    if (!renderer_ || compositionId.isEmpty() || size.width() <= 0 ||
        size.height() <= 0) {
      return nullptr;
    }
    auto& entry = precompGpuOutputs_[compositionId];
    if (entry.size != size || !entry.colorTargetView ||
        !entry.depthTargetView || !entry.colorShaderResourceView) {
      renderer_->destroyOffscreenTexture(entry.colorTargetView);
      renderer_->destroyOffscreenTexture(entry.depthTargetView);
      entry = {};
      entry.colorTargetView =
          renderer_->createOffscreenTexture(size.width(), size.height());
      entry.depthTargetView =
          renderer_->createOffscreenDepthTexture(size.width(), size.height());
      entry.colorShaderResourceView =
          renderer_->offscreenTextureShaderResourceView(entry.colorTargetView);
      entry.size = size;
    }
    entry.frame = frame;
    entry.generation = generation;
    entry.ready = false;
    return entry.colorTargetView && entry.depthTargetView &&
                   entry.colorShaderResourceView
               ? &entry
               : nullptr;
  }

  Diligent::ITextureView* renderPrecomp2DGpuOutput(
      ArtifactCompositionLayer* precompLayer, qint64 childFrame,
      const QSize& childSize, float restoreViewportW, float restoreViewportH,
      float restoreCanvasW, float restoreCanvasH) {
    if (!precompLayer || !renderer_ || !gpuTextureCacheManager_) return nullptr;
    const auto child = precompLayer->sourceComposition();
    if (!child || childSize.isEmpty()) return nullptr;
    const auto layers = child->allLayerRef();
    for (const auto& layer : layers) {
      if (!layer || layer->is3D() || layer->isAdjustmentLayer() ||
          layer->maskCount() != 0 || layer->layerBlendType() !=
              ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL ||
          layerHasRasterizerEffectsOrMasks(layer.get()) ||
          dynamic_cast<ArtifactCompositionLayer*>(layer.get()) != nullptr) {
        return nullptr;
      }
    }
    auto* output = preparePrecompGpuOutput(
        child->id().toString(), childFrame, childSize, renderFrameCounter_);
    if (!output) return nullptr;
    const auto savedFrame = child->framePosition();
    const float savedZoom = renderer_->getZoom();
    float savedPanX = 0.0f, savedPanY = 0.0f;
    renderer_->getPan(savedPanX, savedPanY);
    child->goToFrame(childFrame);
    const bool scopeActive = precompLayer->beginExposedPropertyOverrideScope();
    renderer_->pushRenderTarget(output->colorTargetView, output->depthTargetView);
    renderer_->setViewportRect(childSize.width(), childSize.height());
    renderer_->setCanvasSize(childSize.width(), childSize.height());
    renderer_->setPan(0.0f, 0.0f);
    renderer_->setZoom(1.0f);
    renderer_->clearRenderTarget(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
    renderer_->clearDepthRenderTarget(output->depthTargetView);
    for (const auto& layer : layers) {
      if (layer && child->shouldEvaluateLayer(layer->id()) &&
          layer->isActiveAt(FramePosition(childFrame)) && layer->isVisible() &&
          layer->shouldIncludeInFinalRender()) {
        drawLayerForCompositionView(layer.get(), renderer_.get(), 1.0f,
                                    &lastVideoDebug_, &surfaceCache_,
                                    gpuTextureCacheManager_.get(), childFrame,
                                    true, DetailLevel::High,
                                    static_cast<const QMatrix4x4*>(nullptr));
      }
    }
    renderer_->flush();
    renderer_->popRenderTarget();
    renderer_->setViewportRect(restoreViewportW, restoreViewportH);
    renderer_->setCanvasSize(restoreCanvasW, restoreCanvasH);
    renderer_->setPan(savedPanX, savedPanY);
    renderer_->setZoom(savedZoom);
    if (scopeActive) precompLayer->endExposedPropertyOverrideScope();
    child->goToFrame(savedFrame.framePosition());
    output->ready = true;
    return output->colorShaderResourceView;
  }

  std::unique_ptr<CompositionRenderer> compositionRenderer_;

  ArtifactPreviewCompositionPipeline previewPipeline_;

  std::unique_ptr<TransformGizmo> gizmo_;

  std::unique_ptr<TextGizmo> textGizmo_;

  std::unique_ptr<Artifact3DGizmo> gizmo3D_;

  std::unique_ptr<ArtifactPointTrackerGizmo> trackerGizmo_;

  ArtifactCore::MotionTracker* trackerMotionTracker_ = nullptr;

  std::unique_ptr<Artifact::OffscreenCompositionRenderer> trackerOffscreenRenderer_;

  std::unique_ptr<ArtifactCore::LayerBlendPipeline> blendPipeline_;

  std::unique_ptr<ArtifactCore::MaskCutoutPipeline> maskCutoutPipeline_;

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

  enum class FrameRenderPassKind {

    Setup,

    Base,

    Surface,

    Mask,

    Composite,

    Post,

    Overlay,

    Flush,

    Present,

  };

  struct FrameRenderPass {

    FrameRenderPassKind kind = FrameRenderPassKind::Setup;

    QString name;

    QString note;

  };

  struct RenderPassResources {

    RenderPipeline* pipeline = nullptr;

    Diligent::ITextureView* layerRTV = nullptr;

    Diligent::ITextureView* layerSRV = nullptr;

    Diligent::ITextureView* layerFloatSRV = nullptr;

    Diligent::ITextureView* layerFloatUAV = nullptr;

    Diligent::ITextureView* accumSRV = nullptr;

    Diligent::ITextureView* tempUAV = nullptr;

  };

  struct RenderPassContext {

    ArtifactIRenderer* renderer = nullptr;

    quint64 frame = 0;

  };

  class RenderPass {

  public:

    virtual ~RenderPass() = default;

    virtual bool setup(RenderPassResources& resources) = 0;

    virtual bool execute(RenderPassContext& context,

                         RenderPassResources& resources) = 0;

    virtual FrameRenderPassKind kind() const = 0;

    virtual QString name() const = 0;

  };

  class FunctionalRenderPass final : public RenderPass {

  public:

    using Setup = std::function<bool(RenderPassResources&)>;

    using Execute =

        std::function<bool(RenderPassContext&, RenderPassResources&)>;



    FunctionalRenderPass(FrameRenderPassKind passKind, QString passName,

                         Setup setup, Execute execute)

        : kind_(passKind), name_(std::move(passName)),

          setup_(std::move(setup)), execute_(std::move(execute)) {}



    bool setup(RenderPassResources& resources) override {

      return !setup_ || setup_(resources);

    }

    bool execute(RenderPassContext& context,

                 RenderPassResources& resources) override {

      return execute_ && execute_(context, resources);

    }

    FrameRenderPassKind kind() const override { return kind_; }

    QString name() const override { return name_; }



  private:

    FrameRenderPassKind kind_ = FrameRenderPassKind::Setup;

    QString name_;

    Setup setup_;

    Execute execute_;

  };

  struct RenderPassExecutor {

    static bool run(RenderPass& pass, RenderPassContext& context,

                    RenderPassResources& resources) {

      if (!context.renderer || !pass.setup(resources)) {

        return false;

      }

      return pass.execute(context, resources);

    }



    template <std::size_t PassCount>

    static bool runAll(const std::array<RenderPass*, PassCount>& passes,

                       RenderPassContext& context,

                       RenderPassResources& resources) {

      for (RenderPass* pass : passes) {

        if (!pass || !run(*pass, context, resources)) {

          return false;

        }

      }

      return true;

    }

  };

  struct GpuBasePassState {

    float origZoom = 1.0f;

    FloatColor origClearColor{};

    float origPanX = 0.0f;

    float origPanY = 0.0f;

    float origViewW = 0.0f;

    float origViewH = 0.0f;

  };

  struct GpuLayerBlendResult {

    bool blended = false;

    bool directFallbackUsed = false;

    bool convertedLayerToFloat = false;

  };

  struct PresentStageResult {

    double presentedGpuFrameMs = 0.0;

    QString presentedStatus;

    QString presentedVideoDebug;

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
  QString compareRestoreStateId_;
  bool stateCompareSessionActive_ = false;

  bool referencePinned_ = false;

  int referenceFrame_ = 0;



  // Fixed-rate render tick (Phase 1: infrastructure only)

  std::unique_ptr<ArtifactCore::PreciseTicker> renderTickDriver_;

  std::atomic_bool renderTickPosted_{false};

  std::atomic_bool renderDirty_{false};

  static constexpr int kRenderTickIntervalMs = 16; // ~60fps

  QElapsedTimer startupTimer_;

  bool blendPipelineReady_ = false;

  bool blendPipelineInitScheduled_ = false;

  bool blendPipelineInitInProgress_ = false;

  int blendPipelineInitAttemptCount_ = 0;

  quint64 blendPipelineInitGeneration_ = 0;

  static constexpr int kBlendPipelineMaxInitAttempts = 3;

  QString blendPipelineInitState_ = QStringLiteral("not-scheduled");

  bool gpuBlendEnabled_ =

      !qEnvironmentVariableIsSet("ARTIFACT_COMPOSITION_DISABLE_GPU_BLEND");

  void scheduleBlendPipelineInitialization(
      CompositionRenderController* owner, int delayMs, const QString& reason) {
    if (!owner || !initialized_ || !gpuBlendEnabled_ || blendPipelineReady_ ||
        blendPipelineInitScheduled_ || blendPipelineInitInProgress_ ||
        blendPipelineInitAttemptCount_ >= kBlendPipelineMaxInitAttempts) {
      return;
    }

    blendPipelineInitScheduled_ = true;
    blendPipelineInitState_ = QStringLiteral("scheduled:%1").arg(reason);
    QTimer::singleShot(std::max(0, delayMs), owner,
                       [this, owner, reason,
                        generation = blendPipelineInitGeneration_]() {
      if (generation != blendPipelineInitGeneration_) {
        return;
      }
      blendPipelineInitScheduled_ = false;
      if (!initialized_ || !gpuBlendEnabled_ || blendPipelineReady_) {
        return;
      }

      auto* renderer = renderer_.get();
      if (!renderer || !renderer->device() || !renderer->immediateContext()) {
        blendPipelineInitState_ = QStringLiteral("waiting-for-device");
        scheduleBlendPipelineInitialization(
            owner, 250, QStringLiteral("device-not-ready"));
        return;
      }

      blendPipelineInitInProgress_ = true;
      ++blendPipelineInitAttemptCount_;
      blendPipelineInitState_ = QStringLiteral("initializing:%1").arg(reason);

      if (!blendPipeline_) {
        blendPipeline_ = renderer->createLayerBlendPipeline();
      }

      QElapsedTimer timer;
      timer.start();
      blendPipelineReady_ = blendPipeline_ && blendPipeline_->initialize() &&
                            blendPipeline_->ready();
      blendPipelineInitInProgress_ = false;

      if (blendPipelineReady_) {
        blendPipelineInitState_ = QStringLiteral("ready");
        qInfo() << "[CompositionView][Startup] blend pipeline init"
                << "attempt=" << blendPipelineInitAttemptCount_
                << "reason=" << reason << "ms=" << timer.elapsed();
        invalidateBaseComposite();
        owner->markRenderDirty();
        return;
      }

      blendPipeline_.reset();
      blendPipelineInitState_ = QStringLiteral("failed:%1/%2")
                                    .arg(blendPipelineInitAttemptCount_)
                                    .arg(kBlendPipelineMaxInitAttempts);
      qWarning() << "[CompositionView] LayerBlendPipeline initialization failed"
                 << "attempt=" << blendPipelineInitAttemptCount_
                 << "reason=" << reason;

      if (blendPipelineInitAttemptCount_ < kBlendPipelineMaxInitAttempts) {
        scheduleBlendPipelineInitialization(
            owner, 500 * blendPipelineInitAttemptCount_,
            QStringLiteral("retry-after-failure"));
      }
    });
  }

  QString lastVideoDebug_;

  QString lastEmittedVideoDebug_;

  QString lastRenderPathSummary_;

  QString lastCompositionVisibilitySummary_;

  QString lastBlendMaskSummary_;

  QString lastFrameRenderPassPlanSummary_;

  qint64 lastSetupMs_ = 0;

  qint64 lastBasePassMs_ = 0;

  qint64 lastSurfacePassMs_ = 0;

  qint64 lastMaskPassMs_ = 0;

  qint64 lastCompositePassMs_ = 0;

  qint64 lastPostPassMs_ = 0;

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



  std::vector<FrameRenderPass> buildFrameRenderPassPlan(

      bool useRamPreviewFallback, bool pipelineEnabled,

      bool hasComposition) const {

    std::vector<FrameRenderPass> plan;

    plan.reserve(8);

    plan.push_back(

        {FrameRenderPassKind::Setup, QStringLiteral("setup"),

         useRamPreviewFallback ? QStringLiteral("ram-preview fallback")

                               : QStringLiteral("frame bootstrap")});

    plan.push_back(

        {FrameRenderPassKind::Base, QStringLiteral("background"),

         pipelineEnabled ? QStringLiteral("seed background and intermediates")

                         : QStringLiteral("viewport background")});

    if (pipelineEnabled) {

      plan.push_back(

          {FrameRenderPassKind::Surface, QStringLiteral("surface"),

           QStringLiteral("layer surface capture")});

      plan.push_back(

          {FrameRenderPassKind::Mask, QStringLiteral("mask"),

           QStringLiteral("mask and track matte resolve")});

      plan.push_back(

          {FrameRenderPassKind::Composite, QStringLiteral("composite"),

           QStringLiteral("blend into accum target")});

      plan.push_back(

          {FrameRenderPassKind::Post, QStringLiteral("resolve"),

           QStringLiteral("resolve accumulation for viewport")});

    } else {

      plan.push_back(

          {FrameRenderPassKind::Composite,

           useRamPreviewFallback ? QStringLiteral("ram-preview-composite")

                                 : QStringLiteral("direct-composite"),

           useRamPreviewFallback ? QStringLiteral("cached frame composite")

                                 : QStringLiteral("direct layer composite")});

    }

    plan.push_back(

        {FrameRenderPassKind::Overlay, QStringLiteral("overlay"),

         hasComposition ? QStringLiteral("controller overlays")

                        : QStringLiteral("no composition")});

    plan.push_back(

        {FrameRenderPassKind::Flush, QStringLiteral("flush"),

         QStringLiteral("submit queued draw packets")});

    plan.push_back(

        {FrameRenderPassKind::Present, QStringLiteral("present"),

         QStringLiteral("swapchain present")});

    return plan;

  }



  QString summarizeFrameRenderPassPlan(

      const std::vector<FrameRenderPass>& plan) const {

    QStringList items;

    items.reserve(static_cast<int>(plan.size()));

    for (const auto& pass : plan) {

      items.push_back(pass.name);

    }

    return items.join(QStringLiteral(" -> "));

  }



  GpuBasePassState beginGpuBasePass(

      RenderPipeline& renderPipeline, PreviewRenderPipelineSlot& previewRenderSlot,

      float rcw, float rch, float cw, float ch, FloatColor layerBgColor,

      CompositionBackgroundMode backgroundMode) {

    GpuBasePassState state;

    state.origZoom = renderer_->getZoom();

    state.origClearColor = renderer_->getClearColor();

    renderer_->getPan(state.origPanX, state.origPanY);

    state.origViewW = hostWidth_;

    state.origViewH = hostHeight_;



    auto* previewDepthDSV =

        static_cast<Diligent::ITextureView*>(previewRenderSlot.depthTargetView);

    renderer_->setOverrideDSV(previewDepthDSV);



    renderer_->setViewportSize(rcw, rch);

    renderer_->setCanvasSize(cw, ch);

    const float previewScale =

        std::min(rcw / std::max(1.0f, state.origViewW),

                 rch / std::max(1.0f, state.origViewH));

    renderer_->setZoom(state.origZoom * previewScale);

    renderer_->setPan(state.origPanX * previewScale,

                      state.origPanY * previewScale);

    renderer_->setViewportRect(rcw, rch);



    if (compositionViewLog().isDebugEnabled()) {

      qCDebug(compositionViewLog)

          << "[CompositionView] background pass (gpu)"

          << "compSize=" << QSize(static_cast<int>(cw), static_cast<int>(ch))

          << "rtSize=" << QSize(static_cast<int>(rcw), static_cast<int>(rch))

          << "viewport="

          << QSize(static_cast<int>(state.origViewW),

                   static_cast<int>(state.origViewH))

          << "zoom=" << state.origZoom << "pan=(" << state.origPanX << ","

          << state.origPanY << ")"

          << "bg="

          << QColor::fromRgbF(layerBgColor.r(), layerBgColor.g(),

                              layerBgColor.b(), layerBgColor.a())

          << "bgMode=" << static_cast<int>(backgroundMode)

          << "compositionSpaceApplied=" << true;

    }



    renderer_->pushRenderTarget(renderPipeline.accumRTV(), previewRenderSlot.depthTargetView);

    if (previewRenderSlot.depthTargetView) {

      renderer_->clearDepthRenderTarget(previewRenderSlot.depthTargetView);

    }

    renderer_->clearRenderTarget(renderPipeline.accumRTV(),

                                 FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

    renderer_->popRenderTarget();



    return state;

  }



  void seedGpuBasePassBackground(

      RenderPipeline& renderPipeline, Diligent::ITextureView* layerRTV,

      Diligent::ITextureView* layerSRV, Diligent::ITextureView* layerFloatSRV,

      Diligent::ITextureView* layerFloatUAV, Diligent::ITextureView*& accumSRV,

      Diligent::ITextureView*& tempUAV, int& layerToFloatConvertCount,

      int& blendDispatchCount, int& blendFailureCount, float cw, float ch,

      FloatColor layerBgColor, CompositionBackgroundMode backgroundMode) {

    renderer_->pushRenderTarget(layerRTV);

    renderer_->clearRenderTarget(layerRTV, FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

    drawCompositionBackgroundDirect(renderer_.get(), cw, ch, layerBgColor,

                                    backgroundMode, checkerboardTileSize_,

                                    cachedMayaGradientSprite_,
                                    viewportOrientationMatricesValid_
                                        ? &viewportOrientationViewForOverlay_
                                        : nullptr,
                                    viewportOrientationMatricesValid_
                                        ? &viewportOrientationProjectionForOverlay_
                                        : nullptr);

    renderer_->flush();

    renderer_->popRenderTarget();

    renderer_->unbindColorTargetsForCompute();



    if (layerBgColor.a() <= 0.0f) {

      return;

    }



    ++layerToFloatConvertCount;

    const bool convertedBackgroundToFloat = renderer_->convertLayerToFloat(

        blendPipeline_.get(), layerSRV, layerFloatUAV,

        static_cast<Diligent::Uint32>(renderPipeline.width()),

        static_cast<Diligent::Uint32>(renderPipeline.height()));

    Diligent::ITextureView* backgroundBlendSrc =

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



  void drawGpuLayerToIntermediate(

      ArtifactAbstractLayer* layer, Diligent::ITextureView* layerRTV,

      Diligent::ITextureView* accumSRV, float rcw, float rch, float cw,

      float ch, DetailLevel lod, const FramePosition& currentFrame,

      const std::function<QImage(const ArtifactCore::Id&)>& matteResolver,

      const std::vector<SceneLightEntry>& sceneLights, bool has3DCamera,

      const QMatrix4x4& cameraViewMatrix, const QMatrix4x4& cameraProjMatrix,

      bool preserveSceneDepth) {

    renderer_->setOverrideRTV(layerRTV);

    renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

    if (layer->isAdjustmentLayer()) {

      renderer_->clear();

      const float savedZoom = renderer_->getZoom();

      float savedPanX = 0.0f;

      float savedPanY = 0.0f;

      renderer_->setCanvasSize(rcw, rch);

      renderer_->setZoom(1.0f);

      renderer_->setPan(0.0f, 0.0f);

      renderer_->drawSprite(0.0f, 0.0f, rcw, rch, accumSRV, 1.0f);

      renderer_->setCanvasSize(cw, ch);

      renderer_->setZoom(savedZoom);

      renderer_->setPan(savedPanX, savedPanY);

      // Interactive/draft mode: skip effect processing for adjustment layers

      // to avoid GPU->CPU readback. Full-quality render will apply effects.

      if (viewportInteracting_ || previewDownsample_ >= interactivePreviewDownsampleFloor_) {

        renderer_->flush();

        renderer_->setOverrideRTV(nullptr);

        renderer_->unbindColorTargetsForCompute();

        return;

      }

    } else if (preserveSceneDepth) {

      // A contiguous 3D scene bin keeps the shared depth attachment alive while
      // each layer continues through the existing per-layer color compositor.
      // Clearing only color lets later geometry depth-test against earlier
      // geometry without carrying the previous layer color into layerRTV.
      renderer_->clearRenderTarget(
          layerRTV, FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

    } else {

      renderer_->clear();

    }



    QString* dbgOut = &lastVideoDebug_;

    const std::function<Diligent::ITextureView*(ArtifactCompositionLayer*,
                                                int64_t, const QSize&)>
        precompGpuResolver =
            [this, rcw, rch, cw, ch](ArtifactCompositionLayer* precomp,
                                     int64_t childFrame,
                                     const QSize& childSize) {
              return renderPrecomp2DGpuOutput(precomp, childFrame, childSize,
                                               rcw, rch, cw, ch);
            };

    drawLayerForCompositionView(

        layer, renderer_.get(), 1.0f, dbgOut, &surfaceCache_,

        gpuTextureCacheManager_.get(), currentFrame.framePosition(), true, lod,

        has3DCamera ? &cameraViewMatrix : nullptr,

        has3DCamera ? &cameraProjMatrix : nullptr,
        nullptr, nullptr, &matteResolver, &sceneLights,
        viewportInteracting_ ||

            previewDownsample_ >= interactivePreviewDownsampleFloor_,

        viewportOrientationActive_, surfaceGeneration(layer),
        &precompGpuResolver);

    renderer_->flush();

    renderer_->setOverrideRTV(nullptr);

    renderer_->unbindColorTargetsForCompute();

  }



  void drawGpuLayerEmissionToTarget(

      ArtifactAbstractLayer* layer, Diligent::ITextureView* emissionRTV) {

    if (!layer || !emissionRTV || !layer->is3D()) {

      return;

    }

    renderer_->setOverrideRTV(emissionRTV);

    renderer_->setMeshEmissionOnlyPass(true);

    layer->draw(renderer_.get());

    renderer_->flush();

    renderer_->setMeshEmissionOnlyPass(false);

    renderer_->setOverrideRTV(nullptr);

  }



  void drawGpuLayerNormalToTarget(

      ArtifactAbstractLayer* layer, Diligent::ITextureView* normalRTV) {

    if (!layer || !normalRTV || !layer->is3D()) {

      return;

    }

    renderer_->setOverrideRTV(normalRTV);

    renderer_->setMeshNormalOnlyPass(true);

    layer->draw(renderer_.get());

    renderer_->flush();

    renderer_->setMeshNormalOnlyPass(false);

    renderer_->setOverrideRTV(nullptr);

  }



  void drawGpuLayerVelocityToTarget(

      ArtifactAbstractLayer* layer, Diligent::ITextureView* velocityRTV) {

    if (!layer || !velocityRTV || !layer->is3D()) {

      return;

    }

    renderer_->setOverrideRTV(velocityRTV);

    renderer_->setMeshVelocityOnlyPass(true);

    layer->draw(renderer_.get());

    renderer_->flush();

    renderer_->setMeshVelocityOnlyPass(false);

    renderer_->setOverrideRTV(nullptr);

  }



  void drawGpuLayerIdToTarget(ArtifactAbstractLayer* layer,

                              Diligent::ITextureView* idRTV,

                              ArtifactIRenderer::ChannelType channel,

                              float encodedId) {

    if (!layer || !idRTV || !layer->is3D()) {

      return;

    }

    renderer_->setOverrideRTV(idRTV);

    renderer_->setMeshIdPass(channel, encodedId);

    layer->draw(renderer_.get());

    renderer_->flush();

    renderer_->setMeshIdPass(ArtifactIRenderer::ChannelType::Custom, 0.0f);

    renderer_->setOverrideRTV(nullptr);

  }



  void drawGpuLayerAlbedoToTarget(ArtifactAbstractLayer* layer,

                                  Diligent::ITextureView* albedoRTV) {

    if (!layer || !albedoRTV || !layer->is3D()) {

      return;

    }

    renderer_->setOverrideRTV(albedoRTV);

    renderer_->setMeshAlbedoOnlyPass(true);

    layer->draw(renderer_.get());

    renderer_->flush();

    renderer_->setMeshAlbedoOnlyPass(false);

    renderer_->setOverrideRTV(nullptr);

  }



  Diligent::ITextureView* prepareGpuLayerForBlend(

      ArtifactAbstractLayer* layer, RenderPipeline& renderPipeline,

      Diligent::ITextureView* layerSRV, Diligent::ITextureView* layerFloatSRV,

      Diligent::ITextureView* layerFloatUAV, Diligent::ITextureView* tempUAV,

      const QHash<ArtifactCore::Id, QImage>& matteSourceImages,

      int& layerToFloatConvertCount, bool& convertedLayerToFloat) {

    convertedLayerToFloat = renderer_->convertLayerToFloat(

        blendPipeline_.get(), layerSRV, layerFloatUAV,

        static_cast<Diligent::Uint32>(renderPipeline.width()),

        static_cast<Diligent::Uint32>(renderPipeline.height()));

    if (!convertedLayerToFloat) {

      qCritical() << "[CompositionView] layer-to-float conversion failed; "

                     "rejecting incompatible blend input"

                 << "layer=" << layer->id().toString()

                 << "layerName=" << layer->layerName();

      return nullptr;

    }



    ++layerToFloatConvertCount;

    const auto mattes = layer->matteReferences();

    if (mattes.empty()) {

      return layerFloatSRV;

    }



    auto devCtx = renderer_->immediateContext();

    bool allMatted = true;

    for (const auto& matteRef : mattes) {

      if (!matteRef.enabled || matteRef.sourceLayerId.isNil()) {

        continue;

      }

      const auto matteIt = matteSourceImages.constFind(matteRef.sourceLayerId);

      if (matteIt == matteSourceImages.constEnd() || matteIt.value().isNull()) {

        allMatted = false;

        continue;

      }

      const QImage& matteSrc = matteIt.value();

      const bool uploaded = renderPipeline.updateMatteSourceFromData(

          devCtx.RawPtr(), matteSrc.constBits(),

          static_cast<Diligent::Uint32>(matteSrc.width()),

          static_cast<Diligent::Uint32>(matteSrc.height()),

          static_cast<Diligent::Uint32>(matteSrc.bytesPerLine()));

      if (!uploaded) {

        allMatted = false;

        continue;

      }



      Diligent::Uint32 shaderMode = 0;

      switch (matteRef.type) {

      case MatteType::Alpha:

        shaderMode = matteRef.invert ? 2u : 0u;

        break;

      case MatteType::Luma:

        shaderMode = matteRef.invert ? 3u : 1u;

        break;

      case MatteType::InverseAlpha:

        shaderMode = matteRef.invert ? 0u : 2u;

        break;

      case MatteType::InverseLuma:

        shaderMode = matteRef.invert ? 1u : 3u;

        break;

      }



      ArtifactCore::MatteTrackParams params;

      params.matteCount = 1;

      params.matteMode0 = shaderMode;

      params.stackMode = 0;

      params.lumaMode = 0;

      params.opacity = 1.0f;

      if (!renderer_->applyTrackMatte(

              blendPipeline_.get(), layerFloatSRV,

              renderPipeline.matteSourceSRV(), nullptr, nullptr, tempUAV,

              params, renderPipeline.width(), renderPipeline.height())) {

        allMatted = false;

        continue;

      }



      Diligent::CopyTextureAttribs copyAttrs = {};

      copyAttrs.pSrcTexture = tempUAV->GetTexture();

      copyAttrs.SrcTextureTransitionMode =

          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

      copyAttrs.pDstTexture = layerFloatUAV->GetTexture();

      copyAttrs.DstTextureTransitionMode =

          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

      devCtx->CopyTexture(copyAttrs);

    }

    if (!allMatted) {

      qCritical() << "[CompositionView] GPU track matte failed; layer blend skipped for"

                 << layer->layerName();

      return nullptr;

    }

    return layerFloatSRV;

  }



  GpuLayerBlendResult blendGpuLayerIntoAccum(

      ArtifactAbstractLayer* layer, RenderPipeline& renderPipeline,

      Diligent::ITextureView* layerSRV,

      Diligent::ITextureView* preparedBlendSRV,

      Diligent::ITextureView*& accumSRV, Diligent::ITextureView*& tempUAV,

      ArtifactCore::BlendMode blendMode, float opacity, float cw, float ch,

      int& blendDispatchCount, int& blendRetryNormalCount,

      int& blendFailureCount, int& directBlendFallbackCount,

      bool convertedLayerToFloat) {

    GpuLayerBlendResult result;

    result.convertedLayerToFloat = convertedLayerToFloat;

    (void)renderPipeline;
    (void)layerSRV;
    (void)cw;
    (void)ch;
    (void)blendRetryNormalCount;
    (void)directBlendFallbackCount;

    if (!convertedLayerToFloat || !preparedBlendSRV) {

      ++blendFailureCount;

      qCritical() << "[CompositionView] blend input does not satisfy the "
                     "canonical float/straight-alpha contract"
                  << "layer=" << layer->id().toString()
                  << "layerName=" << layer->layerName()
                  << "mode=" << static_cast<int>(blendMode);

      return result;

    }

    ++blendDispatchCount;

    result.blended = renderer_->blendLayers(blendPipeline_.get(),

                                            preparedBlendSRV,

                                            accumSRV, tempUAV, blendMode,

                                            opacity);

    if (!result.blended) {

      ++blendFailureCount;

      qCritical() << "[CompositionView] requested blend failed; accumulator "
                     "left unchanged"

                 << "layer=" << layer->id().toString()

                 << "layerName=" << layer->layerName()

                 << "mode=" << static_cast<int>(blendMode)

                 << "opacity=" << opacity;

      return result;

    }



    renderPipeline.swapAccumAndTemp();

    accumSRV = renderPipeline.accumSRV();

    tempUAV = renderPipeline.tempUAV();

    return result;

  }



  Diligent::ITextureView* finalizeGpuRenderToViewport(

      RenderPipeline& renderPipeline, Diligent::ITextureView* accumSRV,

      Diligent::ITextureView* layerFloatSRV,

      Diligent::ITextureView* layerFloatUAV, float rcw, float rch, float cw,

      float ch, float origViewW, float origViewH, float origZoom,

      float origPanX, float origPanY, FloatColor bgColor,

      FloatColor layerBgColor, FloatColor origClearColor,

      CompositionBackgroundMode backgroundMode,

      int& layerToFloatConvertCount) {

    renderer_->setViewportRect(origViewW, origViewH);

    renderer_->setUseExternalMatrices(false);

    renderer_->resetGizmoCameraMatrices();

    renderer_->reset3DCameraMatrices();



    const bool transparentCompositionBackground = layerBgColor.a() < 0.999f;

    if (backgroundMode == CompositionBackgroundMode::MayaGradient) {

      drawViewportMayaGradientBackground(renderer_.get(), origViewW, origViewH,

                                         bgColor, cachedMayaGradientSprite_);

    } else if (backgroundMode == CompositionBackgroundMode::Checkerboard ||

               transparentCompositionBackground) {

      drawViewportCheckerboardBackground(renderer_.get(), origViewW, origViewH,

                                         checkerboardTileSize_);

    }



    renderer_->setCanvasSize(cw, ch);

    renderer_->setZoom(origZoom);

    renderer_->setPan(origPanX, origPanY);

    drawCompositionBackgroundDirect(renderer_.get(), cw, ch, layerBgColor,

                                    backgroundMode, checkerboardTileSize_,

                                    cachedMayaGradientSprite_,
                                    viewportOrientationMatricesValid_
                                        ? &viewportOrientationViewForOverlay_
                                        : nullptr,
                                    viewportOrientationMatricesValid_
                                        ? &viewportOrientationProjectionForOverlay_
                                        : nullptr);



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



    lastPresentedReadbackSRV_ = finalPresentSRV;

    Diligent::ITextureView* channelComponentSource = nullptr;
    Diligent::Uint32 channelComponent = 0;
    switch (viewportChannelDisplayMode_) {
    case ViewportChannelDisplayMode::Red:
      channelComponentSource = finalPresentSRV;
      channelComponent = 0;
      break;
    case ViewportChannelDisplayMode::Green:
      channelComponentSource = finalPresentSRV;
      channelComponent = 1;
      break;
    case ViewportChannelDisplayMode::Blue:
      channelComponentSource = finalPresentSRV;
      channelComponent = 2;
      break;
    case ViewportChannelDisplayMode::Alpha:
      channelComponentSource = finalPresentSRV;
      channelComponent = 3;
      break;
    case ViewportChannelDisplayMode::AlbedoR:
      channelComponentSource = renderPipeline.albedoSRV();
      channelComponent = 0;
      break;
    case ViewportChannelDisplayMode::AlbedoG:
      channelComponentSource = renderPipeline.albedoSRV();
      channelComponent = 1;
      break;
    case ViewportChannelDisplayMode::AlbedoB:
      channelComponentSource = renderPipeline.albedoSRV();
      channelComponent = 2;
      break;
    case ViewportChannelDisplayMode::NormalX:
      channelComponentSource = renderPipeline.normalSRV();
      channelComponent = 0;
      break;
    case ViewportChannelDisplayMode::NormalY:
      channelComponentSource = renderPipeline.normalSRV();
      channelComponent = 1;
      break;
    case ViewportChannelDisplayMode::NormalZ:
      channelComponentSource = renderPipeline.normalSRV();
      channelComponent = 2;
      break;
    case ViewportChannelDisplayMode::VelocityX:
      channelComponentSource = renderPipeline.velocitySRV();
      channelComponent = 0;
      break;
    case ViewportChannelDisplayMode::VelocityY:
      channelComponentSource = renderPipeline.velocitySRV();
      channelComponent = 1;
      break;
    default:
      break;
    }
    if (channelComponentSource && blendPipeline_) {
      auto context = renderer_->immediateContext();
      if (context && blendPipeline_->displayComponent(
                         context.RawPtr(), channelComponentSource,
                         renderPipeline.tempUAV(), channelComponent,
                         renderPipeline.width(), renderPipeline.height())) {
        viewportChannelDisplaySRV_ = renderPipeline.tempSRV();
      }
    }

    renderer_->setCanvasSize(origViewW, origViewH);

    renderer_->setZoom(1.0f);

    renderer_->setPan(0.0f, 0.0f);

    renderer_->drawSprite(0.0f, 0.0f, origViewW, origViewH, finalPresentSRV,

                          1.0f);



    if (compositionRenderer_) {

      compositionRenderer_->SetCompositionSize(cw, ch);

      compositionRenderer_->ApplyCompositionSpace();

    } else {

      renderer_->setCanvasSize(cw, ch);

    }

    renderer_->setZoom(origZoom);

    renderer_->setPan(origPanX, origPanY);

    renderer_->setClearColor(origClearColor);

    renderer_->setOverrideDSV(nullptr);

    return finalPresentSRV;

  }



  PresentStageResult presentAndUpdateVideoLayers(

      const std::vector<ArtifactAbstractLayerPtr>& layers,

      const FramePosition& currentFrame, bool useRamPreviewFallback,

      const ArtifactRamPreviewFrameCacheState& playbackPreviewState,

      const QString& ramPreviewFallbackReason) {

    PresentStageResult result;

    renderer_->present();

    result.presentedGpuFrameMs = renderer_->lastFrameGpuTimeMs();

    result.presentedStatus = renderer_->lastPresentStatus();



    for (const auto& layer : layers) {

      if (auto* videoLayer = dynamic_cast<ArtifactVideoLayer*>(layer.get())) {

        if (useRamPreviewFallback && isLayerEffectivelyVisible(layer) &&

            layer->isActiveAt(currentFrame)) {

          videoLayer->markFrameCompositionCacheReady(

              layer->currentFrame(), playbackPreviewState.onDisk);

          videoLayer->markFrameRenderQueued(layer->currentFrame());

        }

        videoLayer->markFramePresented(layer->currentFrame(),

                                       result.presentedGpuFrameMs,

                                       result.presentedStatus);

        if (result.presentedVideoDebug.isEmpty() &&

            isLayerEffectivelyVisible(layer) &&

            layer->isActiveAt(currentFrame)) {

          result.presentedVideoDebug =

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

    return result;

  }



  LayerID selectedLayerId_;

  bool isDraggingLayer_ = false;

  bool gizmoDragActive_ = false;

  bool designReorderActive_ = false;

  LayerID designReorderLayerId_;

  LayerID designReorderParentId_;

  int designReorderTargetIndex_ = -1;

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

  void renderMotionPathOverlayForLayer(
      const ArtifactAbstractLayerPtr &layer, const ArtifactCompositionPtr &comp,
      int currentFrameNum, float zoom, float invZoom,
      const LayerID &selectedLayerId, const QVector<LayerID> &selectedIds,
      bool hasSelectedIds, quint64 overlayInvalidationSerial,
      bool useSelectedIdFilter);



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

  ViewportChannelDisplayMode viewportChannelDisplayMode_ =
      ViewportChannelDisplayMode::Color;
  Diligent::ITextureView* viewportChannelDisplaySRV_ = nullptr;

  bool showReferenceOverlay_ = false;

  bool showColorSamplerOverlay_ = false;

  bool showAutoColorPaletteOverlay_ = false;

  bool showAnchorCenterOverlay_ = false;

  bool showCameraFrustumOverlay_ = false;

  QImage referenceOverlayImage_;

  bool colorSamplerHasSample_ = false;

  QColor colorSamplerColor_;

  QPoint colorSamplerImagePixel_;

  QPointF colorSamplerCanvasPos_;

  QVector<FloatColor> referenceDominantPalette_;

  QVector<FloatColor> referenceHarmonyPalette_;

  bool showFrameInfo_ = false; // Changed to false by default

  bool showGizmoOverlay_ = true;

  bool showXRayOverlay_ = false;

  bool showIsolationOverlay_ = false;

  bool showCompositionRegionOverlay_ =

      false; // Temporarily disable the blue frame.

  // Onion Skin

  bool showOnionSkin_ = false;

  int onionSkinFrameCount_ = 2;

  int onionSkinOpacity_ = 30;

  QVector<QImage> onionSkinFrames_;

  bool onionSkinCapturePending_ = false;

  QMutex onionSkinMutex_;

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
  bool viewportOrientationMatricesValid_ = false;
  QMatrix4x4 viewportOrientationViewForOverlay_;
  QMatrix4x4 viewportOrientationProjectionForOverlay_;
  // Keep the 3D gizmo on the exact camera used for 3D layer rendering.
  // This is intentionally separate from the 2D composition pan/zoom matrices.
  bool gizmo3DCameraMatricesValid_ = false;
  QMatrix4x4 gizmo3DViewMatrix_;
  QMatrix4x4 gizmo3DProjectionMatrix_;

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

    const quint64 pixelCount =

        static_cast<quint64>(width) * static_cast<quint64>(height);

    constexpr quint64 kPreviewPipelineColorTargetCount = 4;

    const quint64 optionalEmissionBytes = slot.pipeline.hasEmissionTarget()

                                              ? pixelCount * kRgba16fBytesPerPixel

                                              : 0;

    return pixelCount *

           ((kRgba16fBytesPerPixel * kPreviewPipelineColorTargetCount) +

            kRgba8BytesPerPixel + kDepth32BytesPerPixel) +

           optionalEmissionBytes;

  }



  QString previewRenderPipelineSlotsSummary() const {

    QStringList slotNotes;

    slotNotes.reserve(kPreviewRenderPipelineSlotCount);

    for (int i = 0; i < kPreviewRenderPipelineSlotCount; ++i) {

      const auto& slot = previewRenderPipelineSlots_[i];

      const QSize pipelineSize(slot.pipeline.width(), slot.pipeline.height());

      const QSize slotSize = pipelineSize.isValid() ? pipelineSize

                                                    : slot.depthTargetSize;

      slotNotes << QStringLiteral("#%1:%2:%3x%4:depth=%5:emission=%6:frame=%7")

                       .arg(i)

                       .arg(previewRenderPipelineSlotStateText(slot))

                       .arg(std::max(0, slotSize.width()))

                       .arg(std::max(0, slotSize.height()))

                       .arg(slot.depthTargetView ? 1 : 0)

                       .arg(slot.pipeline.hasEmissionTarget() ? 1 : 0)

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

      const bool hasCpuRasterizerUpload,

      const bool gpuBlendPathRequested,

      const bool renderPipelineReady,

      const bool hasDepthSlot,

      const bool acquireHazard) {

    (void)hasCpuRasterizerUpload;

    if (!gpuBlendEnabled) {

      return QStringLiteral("gpu-blend-disabled");

    }

    if (!blendPipelineReady) {

      return QStringLiteral("blend-pipeline-not-ready");

    }

    if (!hasGpuBlendJustification) {

      return QStringLiteral("no-multi-layer-blend-work");

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

    uint8_t gpuBlend = 0, gpuBlendReady = 0, showGrid = 0, showGuides = 0,
            showSafeMargins = 0,

            showAnchorCenter = 0, showCameraFrustum = 0, showXRay = 0,
            showIsolation = 0,

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

             gpuBlend == o.gpuBlend && gpuBlendReady == o.gpuBlendReady &&
             showGrid == o.showGrid &&

             showGuides == o.showGuides &&

             showSafeMargins == o.showSafeMargins &&

             showAnchorCenter == o.showAnchorCenter &&

             showCameraFrustum == o.showCameraFrustum &&

             showXRay == o.showXRay &&

             viewportInteracting == o.viewportInteracting &&

             showIsolation == o.showIsolation &&
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
      float inTangentX = 0.0f;
      float inTangentY = 0.0f;
      float outTangentX = 0.0f;
      float outTangentY = 0.0f;
      bool hasSpatialTangents = false;

    };

    std::vector<Pt> pathPoints;

    // Equal-time samples used only for the velocity-reading dots. These stay
    // independent of the adaptive path tessellation so the dot spacing keeps
    // representing motion speed even where curvature adds line samples.
    std::vector<Pt> timeDotPoints;

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

  bool isDraggingMotionPathTangent_ = false;
  ArtifactAbstractLayerWeak draggingMotionPathTangentLayer_;
  int64_t draggingMotionPathTangentFrame_ = 0;
  MotionPathTangentHandle draggingMotionPathTangentHandle_ =
      MotionPathTangentHandle::None;
  MotionPathTangentSnapshot draggingMotionPathTangentBefore_;

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

  bool workCursorVisible_ = false;

  QPointF workCursorCanvasPos_;

  WorkCursorState workCursorState_;

  QString workCursorLabel_;

  struct ViewState {
    QPointF pan;
    float zoom = 1.0f;
    QQuaternion orientation;
    bool viewportOrientationActive = false;
  };

  std::vector<ViewState> viewUndoStack_;
  std::vector<ViewState> viewRedoStack_;
  bool restoringViewState_ = false;

  ViewState captureViewState() const {
    ViewState state;
    if (renderer_) {
      float panX = 0.0f;
      float panY = 0.0f;
      renderer_->getPan(panX, panY);
      state.pan = QPointF(panX, panY);
      state.zoom = renderer_->getZoom();
    }
    state.orientation = viewportOrientationNavigator_.currentOrientation();
    state.viewportOrientationActive = viewportOrientationActive_;
    return state;
  }

  void applyViewState(const ViewState &state) {
    if (!renderer_) {
      return;
    }
    restoringViewState_ = true;
    renderer_->setPan(static_cast<float>(state.pan.x()),
                      static_cast<float>(state.pan.y()));
    renderer_->setZoom(state.zoom);
    viewportOrientationNavigator_.setCurrentOrientation(state.orientation);
    viewportOrientationActive_ = state.viewportOrientationActive;
    restoringViewState_ = false;
    invalidateOverlayComposite();
    renderDirty_.store(true, std::memory_order_release);
    if (running_ && renderTickDriver_ && !renderTickDriver_->isRunning()) {
      renderTickDriver_->start();
    }
  }

  void pushViewHistory() {
    if (!renderer_ || restoringViewState_) {
      return;
    }
    viewUndoStack_.push_back(captureViewState());
    if (viewUndoStack_.size() > 32) {
      viewUndoStack_.erase(viewUndoStack_.begin());
    }
    viewRedoStack_.clear();
  }

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

  QHash<QString, quint64> surfaceGenerations_;

  std::unique_ptr<GPUTextureCacheManager> gpuTextureCacheManager_;

  QElapsedTimer projectPreflightTimer_;

  CompositionID projectPreflightCompositionId_;

  bool cachedProjectHasBlockingErrors_ = false;

  int cachedProjectDiagnosticCount_ = 0;



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



  // Live field editing state

  bool isDraggingTransformField_ = false;

  QString draggingTransformFieldId_;

  CompositionTransformField draggingTransformFieldBefore_;

  TransformFieldDragMode draggingTransformFieldMode_ =
      TransformFieldDragMode::None;

  QString hoveredTransformFieldId_;

  TransformFieldDragMode hoveredTransformFieldMode_ =
      TransformFieldDragMode::None;



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

  bool hitTestMotionPathTangent(const QPointF &canvasPos, float threshold,
                                int64_t &outFrame,
                                MotionPathTangentHandle &outHandle) const {
    const float thresholdSq = threshold * threshold;
    float bestDistanceSq = thresholdSq;
    bool found = false;
    for (const auto &point : motionPathCache_.keyPoints) {
      if (!point.hasSpatialTangents) continue;
      const auto testHandle = [&](float x, float y,
                                  MotionPathTangentHandle handle) {
        const QPointF delta = QPointF(x, y) - canvasPos;
        const float distanceSq =
            static_cast<float>(QPointF::dotProduct(delta, delta));
        if (distanceSq <= bestDistanceSq) {
          bestDistanceSq = distanceSq;
          outFrame = point.frame;
          outHandle = handle;
          found = true;
        }
      };
      testHandle(point.inTangentX, point.inTangentY,
                 MotionPathTangentHandle::In);
      testHandle(point.outTangentX, point.outTangentY,
                 MotionPathTangentHandle::Out);
    }
    return found;
  }

  void beginMotionPathTangentDrag(const ArtifactAbstractLayerPtr &layer,
                                  int64_t frame,
                                  MotionPathTangentHandle handle) {
    draggingMotionPathTangentLayer_ = layer;
    draggingMotionPathTangentFrame_ = frame;
    draggingMotionPathTangentHandle_ = handle;
    draggingMotionPathTangentBefore_ = {};
    if (layer) {
      const ArtifactCore::RationalTime time(frame, 24);
      draggingMotionPathTangentBefore_.present =
          layer->transform3D().positionKeyFrameSpatialTangentsAt(
              time, draggingMotionPathTangentBefore_.tangents);
    }
    isDraggingMotionPathTangent_ = true;
  }

  void clearMotionPathTangentDragState() {
    isDraggingMotionPathTangent_ = false;
    draggingMotionPathTangentLayer_.reset();
    draggingMotionPathTangentFrame_ = 0;
    draggingMotionPathTangentHandle_ = MotionPathTangentHandle::None;
    draggingMotionPathTangentBefore_ = {};
  }

  bool applyMotionPathTangentDrag(const QPointF &canvasPos) {
    auto layer = draggingMotionPathTangentLayer_.lock();
    if (!layer || !isDraggingMotionPathTangent_ ||
        draggingMotionPathTangentHandle_ == MotionPathTangentHandle::None) {
      return false;
    }

    QPointF localHandlePos = canvasPos;
    if (const auto parent = layer->parentLayer()) {
      const QTransform parentGlobal =
          parent->getGlobalTransformAt(draggingMotionPathTangentFrame_);
      bool invertible = false;
      const QTransform inverseParent = parentGlobal.inverted(&invertible);
      if (!invertible) return false;
      localHandlePos = inverseParent.map(canvasPos);
    }

    auto &t3d = layer->transform3D();
    const ArtifactCore::RationalTime time(draggingMotionPathTangentFrame_, 24);
    const QPointF keyPosition(t3d.positionXAt(time), t3d.positionYAt(time));
    const QPointF delta = localHandlePos - keyPosition;
    ArtifactCore::PositionSpatialTangents tangents;
    if (!t3d.positionKeyFrameSpatialTangentsAt(time, tangents)) {
      tangents.linked = true;
    }
    if (draggingMotionPathTangentHandle_ == MotionPathTangentHandle::In) {
      tangents.inTangent = {static_cast<float>(delta.x()),
                            static_cast<float>(delta.y())};
      if (tangents.linked) {
        tangents.outTangent = {-tangents.inTangent.x, -tangents.inTangent.y};
      }
    } else {
      tangents.outTangent = {static_cast<float>(delta.x()),
                             static_cast<float>(delta.y())};
      if (tangents.linked) {
        tangents.inTangent = {-tangents.outTangent.x, -tangents.outTangent.y};
      }
    }
    if (!t3d.setPositionKeyFrameSpatialTangentsAt(time, tangents)) return false;
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    if (auto *comp = static_cast<ArtifactAbstractComposition *>(layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
    motionPathCache_.valid = false;
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



    if (gizmo3D_ && layer &&
        (layer->is3D() || viewportOrientationActive_)) {

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

    clearTransformFieldDragState();

    clearTransformFieldHoverState();

  }



  void clearTransformFieldDragState()
  {
    isDraggingTransformField_ = false;
    draggingTransformFieldId_.clear();
    draggingTransformFieldBefore_ = {};
    draggingTransformFieldMode_ = TransformFieldDragMode::None;
  }



  void clearTransformFieldHoverState()
  {
    hoveredTransformFieldId_.clear();
    hoveredTransformFieldMode_ = TransformFieldDragMode::None;
  }



  bool beginTransformFieldDrag(const ArtifactCompositionPtr &comp,
                               const QString &fieldId,
                               TransformFieldDragMode mode)
  {
    if (!comp || fieldId.isEmpty() || mode == TransformFieldDragMode::None) {
      return false;
    }

    for (const auto &field : comp->transformFields()) {
      if (field.fieldId != fieldId) {
        continue;
      }
      draggingTransformFieldBefore_ = field;
      draggingTransformFieldId_ = fieldId;
      draggingTransformFieldMode_ = mode;
      hoveredTransformFieldId_ = fieldId;
      hoveredTransformFieldMode_ = mode;
      if (comp->activeTransformFieldId() != fieldId) {
        comp->setActiveTransformFieldId(fieldId);
      }
      isDraggingTransformField_ = true;
      return true;
    }

    return false;
  }



  bool applyTransformFieldDrag(const ArtifactCompositionPtr &comp,
                               const QPointF &canvasPos)
  {
    if (!isDraggingTransformField_ || draggingTransformFieldId_.isEmpty() ||
        !comp) {
      return false;
    }

    for (auto field : comp->transformFields()) {
      if (field.fieldId != draggingTransformFieldId_) {
        continue;
      }

      QPointF localPoint;
      if (!transformFieldLocalPointFromCanvas(comp, field, canvasPos,
                                              localPoint)) {
        return false;
      }

      if (draggingTransformFieldMode_ == TransformFieldDragMode::Center) {
        field.center = localPoint;
      } else if (draggingTransformFieldMode_ == TransformFieldDragMode::Radius) {
        field.radius = field.shape == QStringLiteral("box")
                           ? std::max<qreal>(
                                 0.01, std::abs(localPoint.x() - field.center.x()))
                           : std::max<qreal>(
                                 0.01, std::hypot(localPoint.x() - field.center.x(),
                                                  localPoint.y() - field.center.y()));
        if (field.shape == QStringLiteral("linear")) {
          field.rotationDegrees = std::atan2(
                                      localPoint.y() - field.center.y(),
                                      localPoint.x() - field.center.x()) *
                                  (180.0 / std::acos(-1.0));
        }
      } else if (draggingTransformFieldMode_ ==
                 TransformFieldDragMode::SecondaryRadius) {
        if (field.shape != QStringLiteral("box")) {
          return false;
        }
        field.secondaryRadius = std::max<qreal>(
            0.01, std::abs(localPoint.y() - field.center.y()));
      } else {
        return false;
      }

      comp->addTransformField(field);
      comp->changed();
      invalidateOverlayComposite();
      return true;
    }

    return false;
  }



  bool updateTransformFieldHover(const ArtifactCompositionPtr &comp,
                                 const QPointF &canvasPos,
                                 const QSet<LayerID> &selectedLayerIds,
                                 qreal hitThreshold)
  {
    QString fieldId;
    TransformFieldDragMode mode = TransformFieldDragMode::None;
    if (!hitTestTransformFieldHandle(comp, canvasPos, selectedLayerIds,
                                     hitThreshold, fieldId, mode)) {
      const bool changed = !hoveredTransformFieldId_.isEmpty() ||
                           hoveredTransformFieldMode_ != TransformFieldDragMode::None;
      clearTransformFieldHoverState();
      return changed;
    }

    if (hoveredTransformFieldId_ == fieldId &&
        hoveredTransformFieldMode_ == mode) {
      return false;
    }
    hoveredTransformFieldId_ = std::move(fieldId);
    hoveredTransformFieldMode_ = mode;
    return true;
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

                projectPreflightTimer_.invalidate();

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

    surfaceGenerations_[ownerId] =

        surfaceGenerations_.value(ownerId, 1) + 1;

    surfaceCache_.remove(ownerId + QStringLiteral("|full"));

    surfaceCache_.remove(ownerId + QStringLiteral("|draft"));

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



  quint64 surfaceGeneration(const ArtifactAbstractLayer *layer) const {

    return layer ? surfaceGenerations_.value(layer->id().toString(), 1) : 1;

  }



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

  void drawViewportOverlayPass(CompositionRenderController *owner,

                               const ArtifactCompositionPtr &comp,

                               ArtifactCameraLayer *activeCamera,

                               const std::vector<ArtifactAbstractLayerPtr> &layers,

                               const QStringList &selectedIds,

                               const FramePosition &currentFrame, float cw,

                               float ch, bool has3DCamera,

                               const QMatrix4x4 &cameraViewMatrix,

                               const QMatrix4x4 &cameraProjMatrix);

  void drawViewportCanvasOverlay(float cw, float ch);

  void drawReferenceOverlayImage(float canvasWidth, float canvasHeight);
  void drawViewportChannelOverlayImage(float canvasWidth, float canvasHeight);
  void syncViewportChannelReadbackConfiguration();
  QImage composeViewportChannelOverlayImage() const;
  void drawOnionSkinOverlay(float canvasWidth, float canvasHeight);
  void queueOnionSkinCapture(CompositionRenderController *owner);
  void appendOnionSkinFrame(QImage frame);

  bool updateColorSamplerOverlay(CompositionRenderController *owner,
                                 const QPointF &viewportPos);

  void rebuildReferencePaletteOverlay();

  void drawColorSamplerOverlay(int overlayW, int overlayH);

  void drawAutoColorPaletteOverlay(int overlayW, int overlayH);

  void drawViewportGuideOverlay(const ArtifactCompositionPtr &comp, float cw,

                                float ch);

  void drawViewportInteractionOverlay(CompositionRenderController *owner,

                                      const FramePosition &currentFrame,

                                      float cw, float ch);

  void drawSelectionEditingOverlay(

      CompositionRenderController *owner,

      const ArtifactCompositionPtr &comp,

      const std::vector<ArtifactAbstractLayerPtr> &layers,

      const QStringList &selectedIds,

      const FramePosition &currentFrame, float cw, float ch,

      bool has3DCamera, const QMatrix4x4 &cameraViewMatrix,

      const QMatrix4x4 &cameraProjMatrix);

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

                impl_->surfaceGenerations_.clear();

                if (impl_->gpuTextureCacheManager_) {

                  impl_->gpuTextureCacheManager_->clear();

               }

                impl_->applyCompositionState(comp);

              } else {

                // Property/transform modification: invalidate only this layer

                // ギズモドラッグ中は同じレイヤーのピクセルは変わらないので、

                // transform だけの更新では重いサーフェス再生成を避ける。

                if (auto layer = comp->layerById(layerId)) {

                  const bool directDragTarget =

                      impl_->isDraggingLayer_ &&

                      (layerId == impl_->selectedLayerId_ ||

                       std::any_of(

                           impl_->dragGroupLayers_.cbegin(),

                           impl_->dragGroupLayers_.cend(),

                           [&layerId](const ArtifactAbstractLayerPtr &candidate) {

                             return candidate && candidate->id() == layerId;

                           }));

                  const bool skipCacheInvalidation =

                      directDragTarget ||

                      (impl_->gizmoDragActive_ &&

                       layerId == impl_->selectedLayerId_);

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

          const bool animateResizeGhost =
              impl_->gizmo_ && impl_->gizmo_->isDragging() &&
              impl_->gizmo_->activeHandle() >=
                  TransformGizmo::HandleType::Scale_TL &&
              impl_->gizmo_->activeHandle() <=
                  TransformGizmo::HandleType::Scale_Center;
          if (animateResizeGhost) {
            impl_->renderDirty_.store(true, std::memory_order_release);
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
    impl_->scheduleBlendPipelineInitialization(
        this, 1500, QStringLiteral("startup"));
  }

  // Mask cutout is an independent capability. Its initialization must not
  // decide whether standard Add/Multiply composition is available.
  QTimer::singleShot(1500, this, [this]() {
    if (!impl_ || !impl_->initialized_ || !impl_->renderer_ ||
        !impl_->renderer_->device() || !impl_->renderer_->immediateContext()) {
      return;
    }
    if (!impl_->maskCutoutPipeline_) {
      auto ctx = std::make_unique<ArtifactCore::GpuContext>(
          impl_->renderer_->device(), impl_->renderer_->immediateContext());
      impl_->maskCutoutPipeline_ =
          std::make_unique<ArtifactCore::MaskCutoutPipeline>(*ctx);
      ctx.release();
      if (impl_->maskCutoutPipeline_->initialize()) {
        qInfo() << "[CompositionView][Startup] mask cutout pipeline init OK";
      } else {
        qWarning() << "[CompositionView] MaskCutoutPipeline FAILED to init.";
      }
    }
  });

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

  impl_->clearPrecompGpuOutputs();

  impl_->surfaceCache_.clear();

  impl_->surfaceGenerations_.clear();

  if (impl_->gpuTextureCacheManager_) {

    impl_->gpuTextureCacheManager_->clearDevice();

    impl_->gpuTextureCacheManager_.reset();

  }

  impl_->blendPipeline_.reset();

  impl_->blendPipelineReady_ = false;

  impl_->blendPipelineInitScheduled_ = false;

  impl_->blendPipelineInitInProgress_ = false;

  impl_->blendPipelineInitAttemptCount_ = 0;

  ++impl_->blendPipelineInitGeneration_;

  impl_->blendPipelineInitState_ = QStringLiteral("not-scheduled");

  impl_->maskCutoutPipeline_.reset();

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

  const float savedZoom = impl_->renderer_->getZoom();
  float savedPanX = 0.0f;
  float savedPanY = 0.0f;
  impl_->renderer_->getPan(savedPanX, savedPanY);

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

  impl_->renderer_->setZoom(savedZoom);
  impl_->renderer_->setPan(savedPanX, savedPanY);

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

  if (!impl_->restoringViewState_) {
    impl_->pushViewHistory();
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

  if (impl_->stateCompareSessionActive_ && currentComposition) {
    currentComposition->setActiveStateVariantId(
        impl_->compareRestoreStateId_);
  }
  impl_->stateCompareSessionActive_ = false;
  impl_->compareRestoreStateId_.clear();
  impl_->compareMode_ = CompositionCompareMode::Off;



  for (auto &connection : impl_->layerChangedConnections_) {

    disconnect(connection);

  }

  impl_->layerChangedConnections_.clear();

  impl_->compositionChangedSubscription_.disconnect();

  impl_->surfaceCache_.clear();

  impl_->clearPrecompGpuOutputs();

  impl_->surfaceGenerations_.clear();

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
  const auto composition = impl_->previewPipeline_.composition();
  if (composition) {
    if (mode == CompositionCompareMode::Off) {
      if (impl_->stateCompareSessionActive_) {
        composition->setActiveStateVariantId(
            impl_->compareRestoreStateId_);
      }
      impl_->stateCompareSessionActive_ = false;
      impl_->compareRestoreStateId_.clear();
    } else if (mode == CompositionCompareMode::A ||
               mode == CompositionCompareMode::B) {
      const QString targetStateId = mode == CompositionCompareMode::A
          ? composition->stateComparisonAId()
          : composition->stateComparisonBId();
      const bool hasConfiguredPair =
          !composition->stateComparisonAId().isEmpty() ||
          !composition->stateComparisonBId().isEmpty();
      if (hasConfiguredPair) {
        if (!impl_->stateCompareSessionActive_) {
          impl_->compareRestoreStateId_ =
              composition->activeStateVariantId();
          impl_->stateCompareSessionActive_ = true;
        }
        if (!composition->setActiveStateVariantId(targetStateId)) {
          return;
        }
      }
    }
  }

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

    impl_->invalidateOverlayComposite();

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

void CompositionRenderController::setShowGizmoOverlay(bool show) {

  if (!impl_ || impl_->showGizmoOverlay_ == show) return;
  impl_->showGizmoOverlay_ = show;
  impl_->invalidateOverlayComposite();
  markRenderDirty();

}

bool CompositionRenderController::isShowGizmoOverlay() const {

  return impl_ ? impl_->showGizmoOverlay_ : true;

}

void CompositionRenderController::setShowXRayOverlay(bool show) {

  if (!impl_ || impl_->showXRayOverlay_ == show) return;
  impl_->showXRayOverlay_ = show;
  markRenderDirty();

}

bool CompositionRenderController::isShowXRayOverlay() const {

  return impl_ ? impl_->showXRayOverlay_ : false;

}

void CompositionRenderController::setShowIsolationOverlay(bool show) {

  if (!impl_ || impl_->showIsolationOverlay_ == show) return;
  impl_->showIsolationOverlay_ = show;
  markRenderDirty();

}

bool CompositionRenderController::isShowIsolationOverlay() const {

  return impl_ ? impl_->showIsolationOverlay_ : false;

}

void CompositionRenderController::setShowOnionSkin(bool show) {

  if (impl_->showOnionSkin_ == show) return;
  impl_->showOnionSkin_ = show;
  if (!show) {
    QMutexLocker locker(&impl_->onionSkinMutex_);
    impl_->onionSkinFrames_.clear();
    impl_->onionSkinCapturePending_ = false;
  }
  impl_->invalidateOverlayComposite();
  markRenderDirty();

}

bool CompositionRenderController::isShowOnionSkin() const {

  return impl_->showOnionSkin_;

}

void CompositionRenderController::setOnionSkinFrameCount(int count) {

  impl_->onionSkinFrameCount_ = std::clamp(count, 1, 5);
  {
    QMutexLocker locker(&impl_->onionSkinMutex_);
    while (impl_->onionSkinFrames_.size() > impl_->onionSkinFrameCount_) {
      impl_->onionSkinFrames_.removeLast();
    }
  }
  markRenderDirty();

}

int CompositionRenderController::onionSkinFrameCount() const {

  return impl_->onionSkinFrameCount_;

}

void CompositionRenderController::setOnionSkinOpacity(int percent) {

  impl_->onionSkinOpacity_ = std::clamp(percent, 5, 80);
  markRenderDirty();

}

int CompositionRenderController::onionSkinOpacity() const {

  return impl_->onionSkinOpacity_;

}void CompositionRenderController::setShowMotionPathOverlay(bool show) {

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

void CompositionRenderController::setReferenceOverlayImage(const QImage& image) {

  if (!impl_) {

    return;

  }

  impl_->referenceOverlayImage_ =
      image.isNull() ? QImage() : image.convertToFormat(QImage::Format_RGBA8888);

  if (!impl_->referenceOverlayImage_.isNull()) {

    impl_->showReferenceOverlay_ = true;

  }

  impl_->rebuildReferencePaletteOverlay();

  markRenderDirty();

}

void CompositionRenderController::clearReferenceOverlayImage() {

  if (!impl_) {

    return;

  }

  impl_->referenceOverlayImage_ = QImage();

  impl_->showReferenceOverlay_ = false;

  impl_->referenceDominantPalette_.clear();

  impl_->referenceHarmonyPalette_.clear();

  markRenderDirty();

}

bool CompositionRenderController::hasReferenceOverlayImage() const {

  return impl_ && !impl_->referenceOverlayImage_.isNull();

}

void CompositionRenderController::setShowReferenceOverlay(bool show) {

  if (!impl_) {

    return;

  }

  const bool nextShow = show && !impl_->referenceOverlayImage_.isNull();

  if (impl_->showReferenceOverlay_ == nextShow) {

    return;

  }

  impl_->showReferenceOverlay_ = nextShow;

  markRenderDirty();

}

bool CompositionRenderController::isShowReferenceOverlay() const {

  return impl_ && impl_->showReferenceOverlay_ &&
         !impl_->referenceOverlayImage_.isNull();

}

void CompositionRenderController::setShowColorSamplerOverlay(bool show) {

  if (!impl_ || impl_->showColorSamplerOverlay_ == show) {

    return;

  }

  impl_->showColorSamplerOverlay_ = show;

  if (!show) {

    impl_->colorSamplerHasSample_ = false;

  }

  markRenderDirty();

}

bool CompositionRenderController::isShowColorSamplerOverlay() const {

  return impl_ ? impl_->showColorSamplerOverlay_ : false;

}

void CompositionRenderController::setShowAutoColorPaletteOverlay(bool show) {

  if (!impl_) {

    return;

  }

  const bool nextShow = show && !impl_->referenceDominantPalette_.isEmpty();

  if (impl_->showAutoColorPaletteOverlay_ == nextShow) {

    return;

  }

  impl_->showAutoColorPaletteOverlay_ = nextShow;

  markRenderDirty();

}

bool CompositionRenderController::isShowAutoColorPaletteOverlay() const {

  return impl_ ? impl_->showAutoColorPaletteOverlay_ : false;

}

void CompositionRenderController::setViewportChannelDisplayMode(
    ViewportChannelDisplayMode mode) {

  if (!impl_ || impl_->viewportChannelDisplayMode_ == mode) {

    return;

  }

  impl_->viewportChannelDisplayMode_ = mode;
  impl_->syncViewportChannelReadbackConfiguration();

  markRenderDirty();

}

ViewportChannelDisplayMode
CompositionRenderController::viewportChannelDisplayMode() const {

  return impl_ ? impl_->viewportChannelDisplayMode_
               : ViewportChannelDisplayMode::Color;

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

bool CompositionRenderController::placeWorkCursorAtViewportPos(

    const QPointF &viewportPos) {

  if (!impl_ || !impl_->renderer_) {

    return false;

  }

  const auto canvasPos = impl_->renderer_->viewportToCanvas(

      {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});

  if (impl_->viewportOrientationActive_) {
    setWorkCursorWorldPosition(canvasPos.x, canvasPos.y, 0.0f);
  } else {
    setWorkCursorCanvasPosition(QPointF(canvasPos.x, canvasPos.y));
  }

  return true;

}

void CompositionRenderController::setWorkCursorCanvasPosition(

    const QPointF &canvasPos) {

  if (!impl_) {

    return;

  }

  const QPointF normalized =

      QPointF(std::isfinite(canvasPos.x()) ? canvasPos.x() : 0.0,

              std::isfinite(canvasPos.y()) ? canvasPos.y() : 0.0);

  if (impl_->workCursorVisible_ &&

      impl_->workCursorCanvasPos_ == normalized &&
      !impl_->workCursorState_.spatial) {

    return;

  }

  impl_->workCursorCanvasPos_ = normalized;

  impl_->workCursorState_.x = static_cast<float>(normalized.x());
  impl_->workCursorState_.y = static_cast<float>(normalized.y());
  impl_->workCursorState_.z = 0.0f;
  impl_->workCursorState_.spatial = false;

  impl_->workCursorVisible_ = true;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}

QPointF CompositionRenderController::workCursorCanvasPosition() const {

  return impl_ ? impl_->workCursorCanvasPos_ : QPointF();

}

void CompositionRenderController::setWorkCursorWorldPosition(
    const float x, const float y, const float z) {
  if (!impl_) {
    return;
  }
  const auto finiteOrZero = [](const float value) {
    return std::isfinite(value) ? value : 0.0f;
  };
  WorkCursorState next = impl_->workCursorState_;
  next.x = finiteOrZero(x);
  next.y = finiteOrZero(y);
  next.z = finiteOrZero(z);
  next.spatial = true;
  if (impl_->workCursorVisible_ &&
      next.x == impl_->workCursorState_.x &&
      next.y == impl_->workCursorState_.y &&
      next.z == impl_->workCursorState_.z &&
      impl_->workCursorState_.spatial) {
    return;
  }
  impl_->workCursorState_ = next;
  impl_->workCursorCanvasPos_ = QPointF(next.x, next.y);
  impl_->workCursorVisible_ = true;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

WorkCursorState CompositionRenderController::workCursorState() const {
  return impl_ ? impl_->workCursorState_ : WorkCursorState{};
}

bool CompositionRenderController::moveWorkCursorToSelection() {
  if (!impl_) {
    return false;
  }
  auto *selection = ArtifactLayerSelectionManager::instance();
  if (!selection) {
    return false;
  }
  const auto selected = selection->selectedLayers();
  if (selected.isEmpty()) {
    return false;
  }
  QVector3D center{0.0f, 0.0f, 0.0f};
  int count = 0;
  bool spatial = false;
  for (const auto &layer : selected) {
    if (!layer) {
      continue;
    }
    const auto point = layer->getGlobalTransform4x4().map(QVector3D(0.0f, 0.0f, 0.0f));
    center += point;
    spatial = spatial || layer->is3D();
    ++count;
  }
  if (count <= 0) {
    return false;
  }
  const float invCount = 1.0f / static_cast<float>(count);
  if (spatial) {
    setWorkCursorWorldPosition(center.x() * invCount, center.y() * invCount,
                               center.z() * invCount);
  } else {
    setWorkCursorCanvasPosition(
        QPointF(center.x() * invCount, center.y() * invCount));
  }
  return true;
}

void CompositionRenderController::moveWorkCursorToWorldOrigin() {
  setWorkCursorWorldPosition(0.0f, 0.0f, 0.0f);
}

bool isDesignWorkspace(const QObject *controller) {
  if (!controller) {
    return false;
  }
  const QObject *workspaceOwner = controller->parent();
  return workspaceOwner &&
         workspaceOwner->property("artifactWorkspaceMode").toString() ==
             QStringLiteral("Design");
}

bool isAbsoluteLayoutLayer(const ArtifactAbstractLayerPtr &layer) {
  const auto property = layer
                            ? layer->getProperty(
                                  QStringLiteral("component.layout.mode"))
                            : nullptr;
  return property && property->getValue().toInt() == 2;
}

void CompositionRenderController::Impl::renderMotionPathOverlayForLayer(
    const ArtifactAbstractLayerPtr &layer, const ArtifactCompositionPtr &comp,
    int currentFrameNum, float zoom, float invZoom,
    const LayerID &selectedLayerId, const QVector<LayerID> &selectedIds,
    bool hasSelectedIds, quint64 overlayInvalidationSerial,
    bool useSelectedIdFilter) {
  if (!layer || !comp || !renderer_) {
    return;
  }

  if (hasSelectedIds) {
    if (!selectedIds.contains(layer->id())) {
      return;
    }
  } else if (useSelectedIdFilter && (selectedLayerId.isNil() ||
                                     layer->id() != selectedLayerId)) {
    return;
  }

  const auto &t3d = layer->transform3D();
  const int motionPathFps =
      std::max(1, static_cast<int>(std::round(comp->frameRate().framerate())));
  const auto posTimes = motionPathPositionKeyTimes(layer, motionPathFps);
  if (posTimes.empty()) {
    motionPathCache_.valid = false;
    return;
  }

  const int keyMinFrame = static_cast<int>(posTimes.front().value());
  const int keyMaxFrame = static_cast<int>(posTimes.back().value());
  const int minFrame = keyMinFrame;
  const int maxFrame = keyMaxFrame;
  if (minFrame > maxFrame) {
    return;
  }

  const int64_t rate = posTimes.front().scale();

  // Camera layers need a world-space path. The standard path overlay below is
  // intentionally 2D so it can offer handle editing for regular layers; using
  // it for a camera makes depth movement appear flattened into the canvas.
  if (dynamic_cast<ArtifactCameraLayer *>(layer.get())) {
    using namespace Artifact::Detail;
    const auto cameraPositionAt = [&t3d, rate](const int frame) {
      const ArtifactCore::RationalTime time(frame, rate);
      return QVector3D(static_cast<float>(t3d.positionXAt(time)),
                       static_cast<float>(t3d.positionYAt(time)),
                       static_cast<float>(t3d.positionZAt(time)));
    };
    const float lineThickness = std::max(1.0f, 1.6f * invZoom);
    const float markerSize = std::max(3.0f, 7.0f * invZoom);
    const int sampleStep = motionPathAdaptiveSampleStep(minFrame, maxFrame, zoom);
    const FloatColor pastPathColor{1.0f, 0.60f, 0.16f, 0.95f};
    const FloatColor futurePathColor{0.38f, 0.82f, 1.0f, 0.92f};

    QVector3D previous = cameraPositionAt(minFrame);
    int previousFrame = minFrame;
    for (int frame = minFrame + sampleStep;; frame += sampleStep) {
      const int nextFrame = std::min(frame, maxFrame);
      const QVector3D current = cameraPositionAt(nextFrame);
      const FloatColor color = (previousFrame + nextFrame) <= currentFrameNum * 2
          ? pastPathColor
          : futurePathColor;
      renderer_->drawGizmoLine(float3{previous.x(), previous.y(), previous.z()},
                               float3{current.x(), current.y(), current.z()},
                               color, lineThickness);
      if (nextFrame == maxFrame) {
        break;
      }
      previous = current;
      previousFrame = nextFrame;
    }

    for (const auto &keyTime : posTimes) {
      const int frame = static_cast<int>(keyTime.value());
      const QVector3D keyPosition = cameraPositionAt(frame);
      const bool isCurrent = frame == currentFrameNum;
      renderer_->drawGizmoCube(
          float3{keyPosition.x(), keyPosition.y(), keyPosition.z()},
          isCurrent ? markerSize * 0.72f : markerSize * 0.45f,
          isCurrent ? FloatColor{1.0f, 0.92f, 0.30f, 1.0f}
                    : FloatColor{0.95f, 0.72f, 0.30f, 0.90f});
    }

    if (currentFrameNum >= minFrame && currentFrameNum <= maxFrame) {
      const QVector3D current = cameraPositionAt(currentFrameNum);
      renderer_->drawGizmoRing(float3{current.x(), current.y(), current.z()},
                               float3{0.0f, 1.0f, 0.0f}, markerSize * 1.25f,
                               FloatColor{1.0f, 0.96f, 0.38f, 0.95f}, 1.4f);
    }
    return;
  }

  const FloatColor pastPathColor{0.96f, 0.46f, 0.72f, 0.96f};
  const FloatColor futurePathColor{0.42f, 0.76f, 1.0f, 0.92f};
  const FloatColor pathShadowColor{0.0f, 0.0f, 0.0f, 0.52f};
  const float lineThickness = std::max(1.0f, 1.5f * invZoom);
  const float dotRadius = std::max(1.5f, 2.5f * invZoom);
  const bool hasPathSegment = posTimes.size() >= 2;
  const bool cacheHit =
      motionPathCache_.valid &&
      motionPathCache_.layerId == layer->id() &&
      motionPathCache_.framePos == static_cast<int64_t>(currentFrameNum) &&
      motionPathCache_.overlaySerial == overlayInvalidationSerial;

  if (!cacheHit) {
    motionPathCache_.valid = false;
    motionPathCache_.layerId = layer->id();
    motionPathCache_.framePos = static_cast<int64_t>(currentFrameNum);
    motionPathCache_.overlaySerial = overlayInvalidationSerial;
    motionPathCache_.pathPoints.clear();
    motionPathCache_.timeDotPoints.clear();
    motionPathCache_.keyPoints.clear();

    if (hasPathSegment) {
      const int sampleStep =
          motionPathAdaptiveSampleStep(minFrame, maxFrame, zoom);
      const float curvatureTolerance = std::max(0.75f, 1.5f * invZoom);
      constexpr size_t kMaxCurveSamples = 2048;
      const auto evaluatePoint = [&](int frame) {
        const ArtifactCore::RationalTime t(frame, rate);
        const QTransform transform = layer->getGlobalTransformAt(frame);
        const QPointF position = transform.map(
            QPointF(t3d.anchorXAt(t), t3d.anchorYAt(t)));
        return MotionPathCacheEntry::Pt{frame, static_cast<float>(position.x()),
                                        static_cast<float>(position.y())};
      };
      const auto appendAdaptiveSegment =
          [&](auto &&self, const MotionPathCacheEntry::Pt &start,
              const MotionPathCacheEntry::Pt &end, int depth) -> void {
        if (motionPathCache_.pathPoints.size() >= kMaxCurveSamples) {
          // Preserve the segment endpoint without growing the cache. The final
          // retained segment becomes a coarse fallback once the safety cap is
          // reached.
          motionPathCache_.pathPoints.back() = end;
          return;
        }
        const int frameSpan = end.frame - start.frame;
        if (frameSpan <= 1 || depth >= 7) {
          motionPathCache_.pathPoints.push_back(end);
          return;
        }

        const int midpointFrame = start.frame + frameSpan / 2;
        const auto midpoint = evaluatePoint(midpointFrame);
        const float chordMidX = (start.x + end.x) * 0.5f;
        const float chordMidY = (start.y + end.y) * 0.5f;
        const float deviation = std::hypot(midpoint.x - chordMidX,
                                           midpoint.y - chordMidY);
        if (deviation <= curvatureTolerance) {
          motionPathCache_.pathPoints.push_back(end);
          return;
        }
        self(self, start, midpoint, depth + 1);
        self(self, midpoint, end, depth + 1);
      };

      auto previous = evaluatePoint(minFrame);
      motionPathCache_.pathPoints.push_back(previous);
      for (int frame = minFrame + sampleStep;; frame += sampleStep) {
        const int nextFrame = std::min(frame, maxFrame);
        const auto next = evaluatePoint(nextFrame);
        appendAdaptiveSegment(appendAdaptiveSegment, previous, next, 0);
        if (nextFrame == maxFrame) {
          break;
        }
        previous = next;
      }

      constexpr int kTargetVelocityDots = 72;
      const int64_t frameSpan = static_cast<int64_t>(maxFrame) - minFrame;
      const int timeDotStep = static_cast<int>(std::max<int64_t>(
          1, (frameSpan + kTargetVelocityDots - 1) / kTargetVelocityDots));
      for (int frame = minFrame;; frame += timeDotStep) {
        motionPathCache_.timeDotPoints.push_back(evaluatePoint(frame));
        if (frame >= maxFrame - timeDotStep) {
          if (frame != maxFrame) {
            motionPathCache_.timeDotPoints.push_back(evaluatePoint(maxFrame));
          }
          break;
        }
      }
    }

    for (const auto &kfTime : posTimes) {
      const int f = static_cast<int>(kfTime.value());
      if (f < keyMinFrame || f > keyMaxFrame) {
        continue;
      }
      const QTransform gTrans = layer->getGlobalTransformAt(f);
      const QPointF wPos =
          gTrans.map(QPointF(t3d.anchorXAt(kfTime), t3d.anchorYAt(kfTime)));
      MotionPathCacheEntry::Pt pt;
      pt.frame = f;
      pt.x = static_cast<float>(wPos.x());
      pt.y = static_cast<float>(wPos.y());
      pt.interpolation = motionPathPositionInterpolation(layer, kfTime);
      ArtifactCore::PositionSpatialTangents tangents;
      if (t3d.positionKeyFrameSpatialTangentsAt(kfTime, tangents)) {
        QTransform tangentTransform;
        if (const auto parent = layer->parentLayer()) {
          tangentTransform = parent->getGlobalTransformAt(f);
        }
        const QPointF tangentOrigin = tangentTransform.map(QPointF(0.0, 0.0));
        const auto mapTangent = [&](const auto &tangent) {
          const QPointF transformed = tangentTransform.map(
              QPointF(tangent.x, tangent.y));
          return wPos + (transformed - tangentOrigin);
        };
        const QPointF inHandle = mapTangent(tangents.inTangent);
        const QPointF outHandle = mapTangent(tangents.outTangent);
        pt.inTangentX = static_cast<float>(inHandle.x());
        pt.inTangentY = static_cast<float>(inHandle.y());
        pt.outTangentX = static_cast<float>(outHandle.x());
        pt.outTangentY = static_cast<float>(outHandle.y());
        pt.hasSpatialTangents = true;
      }
      motionPathCache_.keyPoints.push_back(pt);
    }
    motionPathCache_.valid = true;
  }

  if (viewportOrientationMatricesValid_) {
    renderer_->setViewMatrix(viewportOrientationViewForOverlay_);
    renderer_->setProjectionMatrix(viewportOrientationProjectionForOverlay_);
    renderer_->setUseExternalMatrices(true);
  }

  if (!motionPathCache_.pathPoints.empty()) {
    Detail::float2 lastPos;
    int lastFrame = 0;
    bool hasLastPos = false;
    for (const auto &pt : motionPathCache_.pathPoints) {
      Detail::float2 currentPos(pt.x, pt.y);
      if (hasLastPos) {
        const bool isPastSegment =
            (lastFrame + pt.frame) <= currentFrameNum * 2;
        const FloatColor segmentColor =
            isPastSegment ? pastPathColor : futurePathColor;
        drawTaggedSolidLine(renderer_.get(), {lastPos.x, lastPos.y},
                            {currentPos.x, currentPos.y}, pathShadowColor,
                            lineThickness + std::max(1.5f, 2.0f * invZoom),
                            true);
        drawTaggedSolidLine(renderer_.get(), {lastPos.x, lastPos.y},
                            {currentPos.x, currentPos.y}, segmentColor,
                            lineThickness, true);
      }
      lastPos = currentPos;
      lastFrame = pt.frame;
      hasLastPos = true;
    }
    for (const auto &dot : motionPathCache_.timeDotPoints) {
      const FloatColor dotColor =
          dot.frame <= currentFrameNum
              ? FloatColor{1.0f, 0.72f, 0.86f, 0.86f}
              : FloatColor{0.68f, 0.88f, 1.0f, 0.82f};
      renderer_->drawPoint(dot.x, dot.y, dotRadius, pathShadowColor);
      renderer_->drawPoint(dot.x, dot.y, dotRadius * 0.53f, dotColor);
    }
    if (currentFrameNum >= minFrame && currentFrameNum <= maxFrame) {
      const ArtifactCore::RationalTime currentTime(
          static_cast<int64_t>(currentFrameNum), rate);
      const QTransform currentTransform =
          layer->getGlobalTransformAt(currentFrameNum);
      const QPointF currentPosition = currentTransform.map(
          QPointF(t3d.anchorXAt(currentTime), t3d.anchorYAt(currentTime)));
      renderer_->drawPoint(static_cast<float>(currentPosition.x()),
                                 static_cast<float>(currentPosition.y()),
                                 dotRadius * 2.0f,
                                 {0.12f, 0.10f, 0.02f, 0.85f});
      renderer_->drawPoint(static_cast<float>(currentPosition.x()),
                                 static_cast<float>(currentPosition.y()),
                                 dotRadius * 1.25f,
                                 {0.98f, 0.88f, 0.35f, 1.0f});
    }
  }
  for (const auto &pt : motionPathCache_.keyPoints) {
    const bool isCurrent = pt.frame == currentFrameNum;
    const bool isHovered = pt.frame == hoveredMotionPathFrame_;
    const FloatColor keyColor =
        motionPathInterpolationColor(pt.interpolation, isCurrent);
    if (pt.hasSpatialTangents) {
      const FloatColor tangentLine{0.50f, 0.88f, 1.0f,
                                   isCurrent ? 0.92f : 0.58f};
      const FloatColor tangentHandle{0.76f, 0.95f, 1.0f,
                                     isCurrent ? 1.0f : 0.84f};
      const float handleRadius = isCurrent ? dotRadius * 1.15f : dotRadius;
      drawTaggedSolidLine(renderer_.get(), {pt.x, pt.y},
                          {pt.inTangentX, pt.inTangentY}, tangentLine,
                          std::max(1.0f, invZoom), true);
      drawTaggedSolidLine(renderer_.get(), {pt.x, pt.y},
                          {pt.outTangentX, pt.outTangentY}, tangentLine,
                          std::max(1.0f, invZoom), true);
      renderer_->drawPoint(pt.inTangentX, pt.inTangentY, handleRadius,
                           tangentHandle);
      renderer_->drawPoint(pt.outTangentX, pt.outTangentY, handleRadius,
                           tangentHandle);
    }
    const float outerRadius =
        isHovered ? dotRadius * 3.4f : dotRadius * 2.8f;
    const float innerRadius =
        isHovered ? dotRadius * 2.0f : dotRadius * 1.65f;
    const FloatColor ringColor =
        isHovered ? FloatColor{1.0f, 1.0f, 1.0f, 0.95f}
                  : FloatColor{0.96f, 0.96f, 1.0f, 0.92f};
    renderer_->drawPoint(pt.x, pt.y, outerRadius, ringColor);
    renderer_->drawPoint(pt.x, pt.y, innerRadius, keyColor);
  }
  if (viewportOrientationMatricesValid_) {
    renderer_->setUseExternalMatrices(false);
  }
}

int motionPathAdaptiveSampleStep(int minFrame, int maxFrame, float zoom) {
  const int64_t span = std::max<int64_t>(0, static_cast<int64_t>(maxFrame) -
                                             static_cast<int64_t>(minFrame));
  // Keep long work areas bounded while retaining one-frame precision for
  // short paths. Keyframes are added separately, so this only controls the
  // interpolated path samples between them.
  const int64_t kTargetSamples = static_cast<int64_t>(std::clamp<long long>(
      std::lround(240.0 * std::sqrt(std::max(1.0f, zoom))), 240LL, 960LL));
  return static_cast<int>(std::max<int64_t>(1, (span + kTargetSamples - 1) /
                                                kTargetSamples));
}

void CompositionRenderController::setWorkCursorLabel(const QString &label) {

  if (!impl_) {

    return;

  }

  const QString normalized = label.trimmed();

  if (impl_->workCursorLabel_ == normalized) {

    return;

  }

  impl_->workCursorLabel_ = normalized;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}

QString CompositionRenderController::workCursorLabel() const {

  return impl_ ? impl_->workCursorLabel_ : QString();

}

void CompositionRenderController::setWorkCursorVisible(bool visible) {

  if (!impl_ || impl_->workCursorVisible_ == visible) {

    return;

  }

  impl_->workCursorVisible_ = visible;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}

bool CompositionRenderController::isWorkCursorVisible() const {

  return impl_ && impl_->workCursorVisible_;

}

void CompositionRenderController::clearWorkCursor() {

  if (!impl_ || !impl_->workCursorVisible_) {

    return;

  }

  impl_->workCursorVisible_ = false;

  impl_->workCursorLabel_.clear();

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

  if (enabled && !impl_->blendPipelineReady_) {
    impl_->blendPipelineInitAttemptCount_ = 0;
    impl_->blendPipelineInitState_ = QStringLiteral("re-enabled");
    impl_->scheduleBlendPipelineInitialization(
        this, 0, QStringLiteral("user-enabled"));
  } else if (!enabled) {
    ++impl_->blendPipelineInitGeneration_;
    impl_->blendPipelineInitScheduled_ = false;
    impl_->blendPipelineInitState_ = QStringLiteral("disabled");
  }

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

QPointF CompositionRenderController::viewportPan() const {

  if (!impl_ || !impl_->renderer_) {

    return QPointF();

  }

  float panX = 0.0f;
  float panY = 0.0f;
  impl_->renderer_->getPan(panX, panY);
  return QPointF(panX, panY);

}

float CompositionRenderController::viewportZoom() const {

  return impl_ && impl_->renderer_ ? impl_->renderer_->getZoom() : 1.0f;

}

bool CompositionRenderController::canUndoView() const {

  return impl_ && !impl_->viewUndoStack_.empty();

}

bool CompositionRenderController::canRedoView() const {

  return impl_ && !impl_->viewRedoStack_.empty();

}

void CompositionRenderController::pushViewHistory() {

  if (!impl_) {

    return;

  }

  impl_->pushViewHistory();

}

void CompositionRenderController::undoView() {

  if (!impl_ || impl_->viewUndoStack_.empty()) {

    return;

  }

  impl_->viewRedoStack_.push_back(impl_->captureViewState());
  const auto state = impl_->viewUndoStack_.back();
  impl_->viewUndoStack_.pop_back();
  impl_->applyViewState(state);

}

void CompositionRenderController::redoView() {

  if (!impl_ || impl_->viewRedoStack_.empty()) {

    return;

  }

  impl_->viewUndoStack_.push_back(impl_->captureViewState());
  const auto state = impl_->viewRedoStack_.back();
  impl_->viewRedoStack_.pop_back();
  impl_->applyViewState(state);

}



void CompositionRenderController::resetView() {

  if (impl_->renderer_) {

    impl_->pushViewHistory();
    impl_->renderer_->resetView();

    impl_->invalidateBaseComposite();

    markRenderDirty();

  }

}



void CompositionRenderController::zoomInAt(const QPointF &viewportPos) {

  if (impl_->renderer_) {

    impl_->pushViewHistory();
    zoomAtFactor(viewportPos, 1.1f);

  }

}



void CompositionRenderController::zoomOutAt(const QPointF &viewportPos) {

  if (impl_->renderer_) {

    impl_->pushViewHistory();
    zoomAtFactor(viewportPos, 1.0f / 1.1f);

  }

}



void CompositionRenderController::zoomAtFactor(const QPointF &viewportPos,

                                               float factor) {

  if (!impl_->renderer_) {

    return;

  }

  notifyViewportInteractionActivity();

  if (impl_->restoringViewState_) {
    return;
  }

  impl_->pushViewHistory();

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

    impl_->pushViewHistory();
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

    impl_->pushViewHistory();
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

    impl_->pushViewHistory();
    impl_->renderer_->setZoom(1.0f);

    // Center the canvas in the viewport at 100% zoom.

    // hostWidth_/hostHeight_ are physical pixels. lastCanvasWidth_/Height_ are
    // already the renderer's canvas size in the same physical render space, so
    // do not apply DPR again here.

    const float canvasW = impl_->lastCanvasWidth_;

    const float canvasH = impl_->lastCanvasHeight_;

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

    ArtifactCore::FrameDebugPassRecord surfacePass =

        makePass(QStringLiteral("surface"), ArtifactCore::FrameDebugPassKind::Draw,

                 impl_->lastSurfacePassMs_, impl_->lastRenderPathSummary_);

    ArtifactCore::FrameDebugPassRecord maskPass =

        makePass(QStringLiteral("mask"),

                 ArtifactCore::FrameDebugPassKind::Composite,

                 impl_->lastMaskPassMs_, impl_->lastBlendMaskSummary_);

    ArtifactCore::FrameDebugPassRecord compositePass =

        makePass(QStringLiteral("composite"),

                 ArtifactCore::FrameDebugPassKind::Composite,

                 impl_->lastCompositePassMs_, impl_->lastBlendMaskSummary_);

    ArtifactCore::FrameDebugPassRecord postPass =

        makePass(QStringLiteral("post"),

                 ArtifactCore::FrameDebugPassKind::Resolve,

                 impl_->lastPostPassMs_);

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



    surfacePass.name = QStringLiteral("Layer Surface Pass");

    surfacePass.backend = backendName;

    surfacePass.shaderName = QStringLiteral("layer-surface-capture");

    surfacePass.previewResourceLabel =

        snapshot.selectedLayerName.isEmpty() ||

                snapshot.selectedLayerName == QStringLiteral("<none>")

            ? QStringLiteral("viewport")

            : snapshot.selectedLayerName;

    addBinding(surfacePass, QStringLiteral("selectedLayer"),

               snapshot.selectedLayerName.isEmpty() ? QStringLiteral("<none>")

                                                    : snapshot.selectedLayerName);

    addBinding(surfacePass, QStringLiteral("visibleLayerCount"),

               QString::number(snapshot.visibleLayerCount));

    addBinding(surfacePass, QStringLiteral("selectedLayerEffects"),

               QString::number(snapshot.selectedLayerEffectCount));

    addBinding(surfacePass, QStringLiteral("selectedLayerMasks"),

               QString::number(snapshot.selectedLayerMaskCount));



    maskPass.name = QStringLiteral("Mask / Matte Pass");

    maskPass.backend = backendName;

    maskPass.shaderName = QStringLiteral("mask-matte-resolve");

    maskPass.previewResourceLabel = QStringLiteral("Layer RT");

    addBinding(maskPass, QStringLiteral("selectedLayerMasks"),

               QString::number(snapshot.selectedLayerMaskCount));

    addBinding(maskPass, QStringLiteral("selectedLayerMattes"),

               QString::number(snapshot.selectedLayerMatteCount));

    addBinding(maskPass, QStringLiteral("blendMaskSummary"),

               impl_->lastBlendMaskSummary_.isEmpty()

                   ? QStringLiteral("<none>")

                   : impl_->lastBlendMaskSummary_);



    compositePass.name = QStringLiteral("Composite Pass");

    compositePass.backend = backendName;

    compositePass.shaderName = QStringLiteral("blend-into-accum");

    compositePass.previewResourceLabel = QStringLiteral("Accum RT");

    addBinding(compositePass, QStringLiteral("renderPathSummary"),

               impl_->lastRenderPathSummary_.isEmpty()

                   ? QStringLiteral("<none>")

                   : impl_->lastRenderPathSummary_);



    postPass.name = QStringLiteral("Post Pass");

    postPass.backend = backendName;

    postPass.shaderName = QStringLiteral("composition-post");

    postPass.previewResourceLabel = QStringLiteral("viewport");

    addBinding(postPass, QStringLiteral("status"),

               comp ? QStringLiteral("final-effects stage")

                    : QStringLiteral("no composition"));



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

    snapshot.passes.push_back(surfacePass);

    snapshot.passes.push_back(maskPass);

    snapshot.passes.push_back(compositePass);

    snapshot.passes.push_back(postPass);

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



  const QMatrix4x4 view = impl_->gizmo3DCameraMatricesValid_
                              ? impl_->gizmo3DViewMatrix_
                              : impl_->renderer_->getViewMatrix();

  const QMatrix4x4 proj = impl_->gizmo3DCameraMatricesValid_
                              ? impl_->gizmo3DProjectionMatrix_
                              : impl_->renderer_->getProjectionMatrix();

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

  if (event->button() == Qt::LeftButton && activeTool == ToolType::Selection &&
      event->modifiers().testFlag(Qt::AltModifier) && comp && impl_->renderer_) {
    QSet<LayerID> selectedLayerIds;
    if (auto *selection = ArtifactApplicationManager::instance()
                              ? ArtifactApplicationManager::instance()->layerSelectionManager()
                              : nullptr) {
      for (const auto &layer : selection->selectedLayers()) {
        if (layer) {
          selectedLayerIds.insert(layer->id());
        }
      }
    }
    if (selectedLayerIds.isEmpty() && !impl_->selectedLayerId_.isNil()) {
      selectedLayerIds.insert(impl_->selectedLayerId_);
    }

    if (!selectedLayerIds.isEmpty()) {
      const auto cPos = impl_->renderer_->viewportToCanvas(
          {(float)viewportPos.x(), (float)viewportPos.y()});
      const float hitThreshold =
          14.0f * Accessibility::targetScale() /
          impl_->renderer_->getZoom();
      QString fieldId;
      TransformFieldDragMode dragMode = TransformFieldDragMode::None;
      if (hitTestTransformFieldHandle(comp, QPointF(cPos.x, cPos.y),
                                      selectedLayerIds, hitThreshold, fieldId,
                                      dragMode) &&
          impl_->beginTransformFieldDrag(comp, fieldId, dragMode)) {
        notifyViewportInteractionActivity();
        impl_->applyTransformFieldDrag(comp, QPointF(cPos.x, cPos.y));
        markRenderDirty();
        event->accept();
        return;
      }
    }
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

          12.0f * Accessibility::targetScale() /
          impl_->renderer_->getZoom(); // widened for direct mask edits

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

    const float hitThreshold = std::max(
        8.0f * Accessibility::targetScale(),
        12.0f * Accessibility::targetScale() /
            impl_->renderer_->getZoom());

    int64_t tangentFrame = 0;
    MotionPathTangentHandle tangentHandle = MotionPathTangentHandle::None;
    if (impl_->hitTestMotionPathTangent(QPointF(cPos.x, cPos.y),
                                        hitThreshold, tangentFrame,
                                        tangentHandle)) {
      impl_->beginMotionPathTangentDrag(selectedLayer, tangentFrame,
                                        tangentHandle);
      setInfoOverlayText(QStringLiteral("Motion Path"),
                         QStringLiteral("Drag tangent to shape the path"));
      event->accept();
      return;
    }

    if (hitTestMotionPathSample(motionPathSamples, QPointF(cPos.x, cPos.y),

                                hitThreshold, hitSample)) {

      const auto &t3d = selectedLayer->transform3D();

      const ArtifactCore::RationalTime time(hitSample.framePosition, 24);

      if (event->modifiers().testFlag(Qt::ControlModifier)) {
        impl_->beginMotionPathTangentDrag(selectedLayer,
                                          hitSample.framePosition,
                                          MotionPathTangentHandle::Out);
        setInfoOverlayText(QStringLiteral("Motion Path"),
                           QStringLiteral("Drag to create linked tangents"));
        event->accept();
        return;
      }

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

    const QMatrix4x4& gizmoView = impl_->gizmo3DCameraMatricesValid_
                                      ? impl_->gizmo3DViewMatrix_
                                      : impl_->renderer_->getViewMatrix();
    const QMatrix4x4& gizmoProj = impl_->gizmo3DCameraMatricesValid_
                                      ? impl_->gizmo3DProjectionMatrix_
                                      : impl_->renderer_->getProjectionMatrix();
    GizmoAxis axis = impl_->gizmo3D_->hitTest(ray, gizmoView, gizmoProj);

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

        if (isDesignWorkspace(this) && !isAbsoluteLayoutLayer(gizmoLayer) &&
            impl_->gizmo_->activeHandle() == TransformGizmo::HandleType::Move) {

          impl_->gizmo_->handleMouseRelease();
          impl_->gizmoDragActive_ = false;
          impl_->designReorderActive_ = false;
          impl_->designReorderTargetIndex_ = -1;
          if (gizmoLayer && comp && !gizmoLayer->parentLayerId().isNil()) {
            const auto layoutParent = comp->layerById(gizmoLayer->parentLayerId());
            const auto enabledProperty = layoutParent
                                             ? layoutParent->getProperty(
                                                   QStringLiteral("component.layout.enabled"))
                                             : nullptr;
            if (enabledProperty && enabledProperty->getValue().toBool()) {
              const auto layers = comp->allLayer();
              for (int index = 0; index < layers.size(); ++index) {
                if (layers.at(index) && layers.at(index)->id() == gizmoLayer->id()) {
                  impl_->designReorderTargetIndex_ = index;
                  break;
                }
              }
              impl_->designReorderActive_ = impl_->designReorderTargetIndex_ >= 0;
              impl_->designReorderLayerId_ = gizmoLayer->id();
              impl_->designReorderParentId_ = gizmoLayer->parentLayerId();
            }
          }
          setInfoOverlayText(
              QStringLiteral("Design"),
              impl_->designReorderActive_
                  ? QStringLiteral("Drag across siblings to reorder")
                  : QStringLiteral("Position is layout-owned; enable Auto Layout on the parent"));
          impl_->invalidateOverlayComposite();
          markRenderDirty();
          return;

        }

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

  bool needsRender = impl_->updateColorSamplerOverlay(this, viewportPos);

  if (impl_->designReorderActive_) {
    const auto comp = impl_->previewPipeline_.composition();
    const auto parent = comp ? comp->layerById(impl_->designReorderParentId_)
                             : ArtifactAbstractLayerPtr{};
    const auto directionProperty = parent
                                       ? parent->getProperty(
                                             QStringLiteral("component.layout.stackDirection"))
                                       : nullptr;
    const bool vertical = directionProperty &&
                          directionProperty->getValue().toInt() != 0;
    const auto canvas = impl_->renderer_->viewportToCanvas(
        {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
    const qreal pointerAxis = vertical ? static_cast<qreal>(canvas.y)
                                       : static_cast<qreal>(canvas.x);
    if (comp) {
      const auto layers = comp->allLayer();
      qreal closestDistance = std::numeric_limits<qreal>::max();
      int closestIndex = impl_->designReorderTargetIndex_;
      for (int index = 0; index < layers.size(); ++index) {
        const auto &sibling = layers.at(index);
        if (!sibling || sibling->parentLayerId() != impl_->designReorderParentId_) {
          continue;
        }
        if (isAbsoluteLayoutLayer(sibling)) {
          continue;
        }
        const QRectF bounds = sibling->getGlobalTransform().mapRect(
            sibling->localBounds());
        const qreal siblingAxis = vertical ? bounds.center().y()
                                           : bounds.center().x();
        const qreal distance = std::abs(pointerAxis - siblingAxis);
        if (distance < closestDistance) {
          closestDistance = distance;
          closestIndex = index;
        }
      }
      if (closestIndex != impl_->designReorderTargetIndex_) {
        impl_->designReorderTargetIndex_ = closestIndex;
        setInfoOverlayText(QStringLiteral("Design"),
                           QStringLiteral("Reorder target %1")
                               .arg(closestIndex + 1));
        impl_->invalidateOverlayComposite();
        markRenderDirty();
      }
    }
    return;
  }



  if (impl_->isRubberBandSelecting_) {

    impl_->rubberBandCurrentViewportPos_ = viewportPos;

    if (needsRender || impl_->isRubberBandSelecting_) {

      markRenderDirty();

    }

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

  if (impl_->isDraggingMotionPathTangent_) {
    if (impl_->renderer_) {
      const auto canvasPos = impl_->renderer_->viewportToCanvas(
          {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
      if (impl_->applyMotionPathTangentDrag(QPointF(canvasPos.x, canvasPos.y))) {
        markRenderDirty();
        return;
      }
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

  if (activeTool == ToolType::Selection &&
      QApplication::keyboardModifiers().testFlag(Qt::AltModifier) &&
      impl_->renderer_ &&
      !impl_->isDraggingTransformField_) {
    auto comp = impl_->previewPipeline_.composition();
    QSet<LayerID> selectedLayerIds;
    if (auto *selection = ArtifactApplicationManager::instance()
                              ? ArtifactApplicationManager::instance()->layerSelectionManager()
                              : nullptr) {
      for (const auto &layer : selection->selectedLayers()) {
        if (layer) {
          selectedLayerIds.insert(layer->id());
        }
      }
    }
    if (selectedLayerIds.isEmpty() && !impl_->selectedLayerId_.isNil()) {
      selectedLayerIds.insert(impl_->selectedLayerId_);
    }
    if (!selectedLayerIds.isEmpty() && comp) {
      const auto cPos = impl_->renderer_->viewportToCanvas(
          {(float)viewportPos.x(), (float)viewportPos.y()});
      const float hitThreshold =
          14.0f * Accessibility::targetScale() /
          impl_->renderer_->getZoom();
      if (impl_->updateTransformFieldHover(comp, QPointF(cPos.x, cPos.y),
                                          selectedLayerIds, hitThreshold)) {
        needsRender = true;
      }
    } else if (!impl_->hoveredTransformFieldId_.isEmpty() ||
               impl_->hoveredTransformFieldMode_ != TransformFieldDragMode::None) {
      impl_->clearTransformFieldHoverState();
      needsRender = true;
    }
  } else if (!impl_->hoveredTransformFieldId_.isEmpty() ||
             impl_->hoveredTransformFieldMode_ != TransformFieldDragMode::None) {
    impl_->clearTransformFieldHoverState();
    needsRender = true;
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

  if (impl_->isDraggingTransformField_ && impl_->renderer_) {
    auto comp = impl_->previewPipeline_.composition();
    if (comp) {
      const auto cPos = impl_->renderer_->viewportToCanvas(
          {(float)viewportPos.x(), (float)viewportPos.y()});
      if (impl_->applyTransformFieldDrag(comp, QPointF(cPos.x, cPos.y))) {
        notifyViewportInteractionActivity();
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

          const float hitThreshold =
              12.0f * Accessibility::targetScale() /
              impl_->renderer_->getZoom();

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

        const QMatrix4x4& gizmoView = impl_->gizmo3DCameraMatricesValid_
                                          ? impl_->gizmo3DViewMatrix_
                                          : impl_->renderer_->getViewMatrix();
        const QMatrix4x4& gizmoProj = impl_->gizmo3DCameraMatricesValid_
                                          ? impl_->gizmo3DProjectionMatrix_
                                          : impl_->renderer_->getProjectionMatrix();
        impl_->gizmo3D_->hitTest(ray, gizmoView, gizmoProj);

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

  if (impl_->designReorderActive_) {
    const LayerID layerId = impl_->designReorderLayerId_;
    const int targetIndex = impl_->designReorderTargetIndex_;
    impl_->designReorderActive_ = false;
    impl_->designReorderLayerId_ = LayerID::Nil();
    impl_->designReorderParentId_ = LayerID::Nil();
    impl_->designReorderTargetIndex_ = -1;
    bool reordered = false;
    if (targetIndex >= 0) {
      if (auto *service = ArtifactProjectService::instance()) {
        reordered = service->moveLayerInCurrentComposition(layerId, targetIndex);
      }
    }
    setInfoOverlayText(QStringLiteral("Design"),
                       reordered ? QStringLiteral("Auto Layout order updated")
                                 : QStringLiteral("Auto Layout order unchanged"));
    impl_->invalidateOverlayComposite();
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

  if (impl_->isDraggingMotionPathTangent_) {
    auto layer = impl_->draggingMotionPathTangentLayer_.lock();
    if (layer) {
      const ArtifactCore::RationalTime time(
          impl_->draggingMotionPathTangentFrame_, 24);
      MotionPathTangentSnapshot after;
      after.present = layer->transform3D().positionKeyFrameSpatialTangentsAt(
          time, after.tangents);
      const auto &before = impl_->draggingMotionPathTangentBefore_;
      const bool changed = before.present != after.present ||
          std::abs(before.tangents.inTangent.x - after.tangents.inTangent.x) > 0.0001f ||
          std::abs(before.tangents.inTangent.y - after.tangents.inTangent.y) > 0.0001f ||
          std::abs(before.tangents.outTangent.x - after.tangents.outTangent.x) > 0.0001f ||
          std::abs(before.tangents.outTangent.y - after.tangents.outTangent.y) > 0.0001f;
      if (changed) {
        if (auto *mgr = UndoManager::instance()) {
          mgr->push(std::make_unique<MotionPathTangentUndoCommand>(
              layer, impl_->draggingMotionPathTangentFrame_, before, after));
        }
      }
    }
    impl_->clearMotionPathTangentDragState();
    impl_->motionPathCache_.valid = false;
    impl_->invalidateOverlayComposite();
    markRenderDirty();
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

  if (impl_->isDraggingTransformField_) {
    auto comp = impl_->previewPipeline_.composition();
    if (comp && !impl_->draggingTransformFieldId_.isEmpty()) {
      for (const auto &field : comp->transformFields()) {
        if (field.fieldId != impl_->draggingTransformFieldId_) {
          continue;
        }
        const bool changed =
            field.center != impl_->draggingTransformFieldBefore_.center ||
            std::abs(field.radius - impl_->draggingTransformFieldBefore_.radius) >
                0.0001 ||
            std::abs(field.secondaryRadius -
                     impl_->draggingTransformFieldBefore_.secondaryRadius) >
                0.0001 ||
            std::abs(field.rotationDegrees -
                     impl_->draggingTransformFieldBefore_.rotationDegrees) >
                0.0001 ||
            std::abs(field.timeOffsetSeconds -
                     impl_->draggingTransformFieldBefore_.timeOffsetSeconds) >
                0.0001 ||
            std::abs(field.strength - impl_->draggingTransformFieldBefore_.strength) >
                0.0001 ||
            std::abs(field.expansion -
                     impl_->draggingTransformFieldBefore_.expansion) >
                0.0001 ||
            std::abs(field.edgeScale -
                     impl_->draggingTransformFieldBefore_.edgeScale) >
                0.0001 ||
            field.blendMode != impl_->draggingTransformFieldBefore_.blendMode ||
            field.invert != impl_->draggingTransformFieldBefore_.invert ||
            field.enabled != impl_->draggingTransformFieldBefore_.enabled ||
            field.coordinateParentLayerId !=
                impl_->draggingTransformFieldBefore_.coordinateParentLayerId ||
            field.targetLayerIds !=
                impl_->draggingTransformFieldBefore_.targetLayerIds ||
            field.shape != impl_->draggingTransformFieldBefore_.shape ||
            field.displayName != impl_->draggingTransformFieldBefore_.displayName;
        if (changed) {
          if (auto *mgr = UndoManager::instance()) {
            mgr->push(std::make_unique<TransformFieldUndoCommand>(
                ArtifactCompositionWeakPtr(comp),
                impl_->draggingTransformFieldBefore_, field));
          }
        }
        break;
      }
    }

    impl_->clearTransformFieldDragState();
    impl_->invalidateOverlayComposite();
    finishViewportInteraction();
    markRenderDirty();
    return;
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

void CompositionRenderController::clearViewportOrientation() {
  if (!impl_ || !impl_->viewportOrientationActive_) {
    return;
  }
  impl_->viewportOrientationActive_ = false;
  impl_->viewportOrientationMatricesValid_ = false;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
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

  impl_->pushViewHistory();
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

QQuaternion CompositionRenderController::viewportOrientationQuaternion() const {

  if (!impl_) {

    return QQuaternion();

  }

  return impl_->viewportOrientationNavigator_.currentOrientation();

}

void CompositionRenderController::setViewportOrientationQuaternion(

    const QQuaternion &orientation) {

  if (!impl_) {

    return;

  }

  const QQuaternion normalized = orientation.normalized();
  const QQuaternion current =
      impl_->viewportOrientationNavigator_.currentOrientation();
  if (std::abs(QQuaternion::dotProduct(current, normalized)) > 0.999999f) {

    return;

  }

  // Continuous view-cube/orbit updates must not append one history entry per
  // mouse move. Discrete hotspot snaps still record history in
  // setViewportOrientation().
  impl_->viewportOrientationNavigator_.setCurrentOrientation(normalized);

  impl_->viewportOrientationActive_ = true;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

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

  if (activeTool == ToolType::Selection &&
      QApplication::keyboardModifiers().testFlag(Qt::AltModifier)) {
    auto comp = impl_->previewPipeline_.composition();
    if (comp) {
      QSet<LayerID> selectedLayerIds;
      if (auto* selection = ArtifactApplicationManager::instance()
                                ? ArtifactApplicationManager::instance()->layerSelectionManager()
                                : nullptr) {
        for (const auto& layer : selection->selectedLayers()) {
          if (layer) {
            selectedLayerIds.insert(layer->id());
          }
        }
      }
      if (selectedLayerIds.isEmpty() && !impl_->selectedLayerId_.isNil()) {
        selectedLayerIds.insert(impl_->selectedLayerId_);
      }
      if (!selectedLayerIds.isEmpty()) {
        const auto canvasPos = impl_->renderer_->viewportToCanvas(
            {(float)viewportPos.x(), (float)viewportPos.y()});
        QString fieldId;
        TransformFieldDragMode mode = TransformFieldDragMode::None;
        if (hitTestTransformFieldHandle(
                comp, QPointF(canvasPos.x, canvasPos.y), selectedLayerIds,
                14.0f * Accessibility::targetScale() /
                    std::max(0.001f, impl_->renderer_->getZoom()),
                fieldId, mode)) {
          if (mode == TransformFieldDragMode::Center) {
            return impl_->isDraggingTransformField_ &&
                           impl_->draggingTransformFieldMode_ ==
                               TransformFieldDragMode::Center
                       ? Qt::ClosedHandCursor
                       : Qt::SizeAllCursor;
          }
          if (mode == TransformFieldDragMode::Radius) {
            return impl_->isDraggingTransformField_ &&
                           impl_->draggingTransformFieldMode_ ==
                               TransformFieldDragMode::Radius
                       ? Qt::ClosedHandCursor
                       : Qt::SizeHorCursor;
          }
          if (mode == TransformFieldDragMode::SecondaryRadius) {
            return impl_->isDraggingTransformField_ &&
                           impl_->draggingTransformFieldMode_ ==
                               TransformFieldDragMode::SecondaryRadius
                       ? Qt::ClosedHandCursor
                       : Qt::SizeVerCursor;
          }
        }
      }
    }
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

    bool enabled = false;

    RenderCostCaptureGuard(ArtifactIRenderer* r, quint64 f, bool capture)

        : renderer(r), frame(f), enabled(capture) {

      if (renderer && enabled) {

        renderer->beginFrameCostCapture();

        renderer->beginFrameGpuProfiling();

      }

    }

    ~RenderCostCaptureGuard() {

      if (renderer && enabled) {

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

  const bool captureRenderDiagnostics =

      continuousRenderDiagnosticsEnabled() ||

      (renderFrameCounter_ % 30u) == 0u;

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

    bool enabled = false;

    explicit TraceScopeGuard(bool capture) : enabled(capture) {

      scope.name = QStringLiteral("CompositionRenderController::renderOneFrameImpl");

      scope.domain = ArtifactCore::TraceDomain::Render;

      scope.startNs = 0;

      timer.start();

    }

    ~TraceScopeGuard() {

      if (!enabled) {

        return;

      }

      scope.endNs = timer.nsecsElapsed();

      if (scope.endNs <= scope.startNs) {

        scope.endNs = scope.startNs + 1;

      }

      ArtifactCore::TraceRecorder::instance().recordScope(scope);

    }

  } traceGuard(captureRenderDiagnostics);



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

    constexpr qint64 kProjectPreflightRefreshMs = 1000;

    const bool compositionChanged =

        projectPreflightCompositionId_ != comp->id();

    if (compositionChanged || !projectPreflightTimer_.isValid() ||

        projectPreflightTimer_.elapsed() >= kProjectPreflightRefreshMs) {

      const auto projectDiagnostics = service->currentProjectDiagnostics();

      cachedProjectHasBlockingErrors_ = std::any_of(

          projectDiagnostics.begin(), projectDiagnostics.end(),

          [](const auto &diagnostic) { return diagnostic.isError(); });

      cachedProjectDiagnosticCount_ =

          static_cast<int>(projectDiagnostics.size());

      projectPreflightCompositionId_ = comp->id();

      projectPreflightTimer_.restart();

    }

    if (cachedProjectHasBlockingErrors_) {

      const QString blockedSummary =

          QStringLiteral("preflight=blocked issues=%1")

              .arg(cachedProjectDiagnosticCount_);

      if (blockedSummary != lastRenderPathSummary_) {

        qWarning() << "[CompositionView] render preflight blocked by project health errors"

                   << "issues=" << cachedProjectDiagnosticCount_;

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

      std::make_unique<RenderCostCaptureGuard>(

          renderer_.get(), renderFrameCounter_, captureRenderDiagnostics);

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

          : 1;

  const bool draftRendering =

      viewportInteracting_ &&

      effectivePreviewDownsample > 1;

  const int previewRenderWidth = std::max(

      1, static_cast<int>(std::ceil(

             viewportW / static_cast<float>(effectivePreviewDownsample))));

  const int previewRenderHeight = std::max(

      1, static_cast<int>(std::ceil(

             viewportH / static_cast<float>(effectivePreviewDownsample))));

  // Keep the settled GPU blend path at the same physical resolution as the

  // direct Normal path. Downsampling is an interaction-only optimization;

  // otherwise Add/Multiply look permanently softer than Normal.

  const float rcw = static_cast<float>(previewRenderWidth);

  const float rch = static_cast<float>(previewRenderHeight);



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

  // Crowd and clone collision are owned by a composition-wide fixed-step
  // session. The layer-local code remains only as a fallback for isolated
  // layer previews that do not have a composition owner.
  comp->evaluateLayerComponentSimulation(currentFrame, viewportInteracting_);



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



  // Resolve each matte source at most once per rendered frame. Multiple matte

  // consumers then share the same implicitly-shared QImage until the frame is

  // complete.

  QHash<ArtifactCore::Id, QImage> matteSourceFrameCache;

  auto matteResolverLambda =

      [&layers, &matteSourceFrameCache](const ArtifactCore::Id &layerId)

      -> QImage {

      const auto cached = matteSourceFrameCache.constFind(layerId);

      if (cached != matteSourceFrameCache.constEnd()) {

          return cached.value();

      }

      QImage resolved;

      for (const auto &l : layers) {

          if (l && l->id() == layerId) {

              if (auto *imgLayer = dynamic_cast<ArtifactImageLayer *>(l.get())) {

                  resolved = imgLayer->toQImage();

                  break;

              }

              if (auto *videoLayer = dynamic_cast<ArtifactVideoLayer *>(l.get())) {

                  resolved = videoLayer->currentFrameToQImage();

                  break;

              }

              if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(l.get())) {

                  resolved = textLayer->toQImage();

                  break;

              }

              if (auto *svgLayer = dynamic_cast<ArtifactSvgLayer *>(l.get())) {

                  resolved = svgLayer->toQImage();

                  break;

              }

          }

      }

      matteSourceFrameCache.insert(layerId, resolved);

      return resolved;

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
  QMatrix4x4 previousCameraViewMatrix;
  QMatrix4x4 previousCameraProjMatrix;
  if (activeCamera) {

    const QSize compSize = comp->settings().compositionSize();

    const float cw =

        static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);

    const float ch =

        static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);

    const float aspect = std::max(0.001f, cw / std::max(0.001f, ch));

    const double currentShakeTime = currentFrame.toSeconds(comp->frameRate().framerate());
    const double frameDelta =
        comp->frameRate().framerate() > 0.0
            ? 1.0 / comp->frameRate().framerate()
            : 0.0;
    activeCamera->advanceShake(currentShakeTime, frameDelta);



    cameraViewMatrix = activeCamera->viewMatrix();

    cameraProjMatrix = activeCamera->projectionMatrix(aspect);
    previousCameraViewMatrix = cameraViewMatrix;
    previousCameraProjMatrix = cameraProjMatrix;
    const int64_t currentFrameNumber = currentFrame.framePosition();
    if (currentFrameNumber > 0) {
      comp->goToFrame(currentFrameNumber - 1);
      activeCamera->advanceShake(currentShakeTime - frameDelta, 0.0);
      previousCameraViewMatrix = activeCamera->viewMatrix();
      previousCameraProjMatrix = activeCamera->projectionMatrix(aspect);
      comp->goToFrame(currentFrameNumber);
      activeCamera->advanceShake(currentShakeTime, 0.0);
    }
    if (activeCamera->stereoMode() != StereoMode::Mono) {

      const ArtifactCore::StereoCamera stereoCamera =

          ArtifactCore::StereoCamera::fromHmd(cameraViewMatrix.inverted(),

                                              activeCamera->ipd(),

                                              activeCamera->nearClipPlane(),

                                              activeCamera->farClipPlane());

      cameraViewMatrix = stereoCamera.leftEyeView;
      const ArtifactCore::StereoCamera previousStereoCamera =
          ArtifactCore::StereoCamera::fromHmd(previousCameraViewMatrix.inverted(),
                                              activeCamera->ipd(),
                                              activeCamera->nearClipPlane(),
                                              activeCamera->farClipPlane());
      previousCameraViewMatrix = previousStereoCamera.leftEyeView;
    }

    has3DCamera = true;

  }

  // A newly created 3D layer is centered in the composition, but a normal
  // 2D composition has no camera layer yet. Keep that first 3D object visible
  // until an explicit camera or viewport orientation takes ownership.
  if (!has3DCamera) {
    const QSize compSize = comp->settings().compositionSize();
    const float fallbackWidth =
        static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);
    const float fallbackHeight =
        static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);
    constexpr float fallbackFovDegrees = 45.0f;
    constexpr float degreesToRadians =
        3.14159265358979323846f / 180.0f;
    const float fallbackDistance =
        (fallbackHeight * 0.5f) /
        std::tan(fallbackFovDegrees * 0.5f * degreesToRadians);
    const QVector3D fallbackTarget(fallbackWidth * 0.5f,
                                   fallbackHeight * 0.5f, 0.0f);
    cameraViewMatrix.lookAt(
        fallbackTarget + QVector3D(0.0f, 0.0f, fallbackDistance),
        fallbackTarget, QVector3D(0.0f, 1.0f, 0.0f));
    cameraProjMatrix.perspective(fallbackFovDegrees,
                                 fallbackWidth / fallbackHeight, 1.0f,
                                 100000.0f);
    cameraProjMatrix(1, 1) = -cameraProjMatrix(1, 1);
    previousCameraViewMatrix = cameraViewMatrix;
    previousCameraProjMatrix = cameraProjMatrix;
    has3DCamera = true;
  }

  viewportOrientationMatricesValid_ = false;
  if (viewportOrientationActive_) {

    const float orientationViewportW = std::max(1.0f, hostWidth_);
    const float orientationViewportH = std::max(1.0f, hostHeight_);
    const float orientationZoom = std::max(0.001f, renderer_->getZoom());
    float orientationPanX = 0.0f;
    float orientationPanY = 0.0f;
    renderer_->getPan(orientationPanX, orientationPanY);

    constexpr float orientationFovDegrees = 45.0f;
    constexpr float degreesToRadians =
        3.14159265358979323846f / 180.0f;
    const float orientationDistance =
        (orientationViewportH * 0.5f) /
        (orientationZoom *
         std::tan(orientationFovDegrees * 0.5f * degreesToRadians));
    const QPointF orientationTarget(
        (orientationViewportW * 0.5f - orientationPanX) / orientationZoom,
        (orientationViewportH * 0.5f - orientationPanY) / orientationZoom);

    const QQuaternion orientation =
        viewportOrientationNavigator_.currentOrientation();

    cameraViewMatrix = viewportOrientationViewMatrix(
        orientation, orientationTarget, orientationDistance);

    cameraProjMatrix = viewportOrientationProjectionMatrix(
        orientationViewportW, orientationViewportH, orientationFovDegrees);

    previousCameraViewMatrix = cameraViewMatrix;

    previousCameraProjMatrix = cameraProjMatrix;

    has3DCamera = true;
    viewportOrientationViewForOverlay_ = cameraViewMatrix;
    viewportOrientationProjectionForOverlay_ = cameraProjMatrix;
    viewportOrientationMatricesValid_ = true;

  }

  gizmo3DCameraMatricesValid_ = has3DCamera;
  if (gizmo3DCameraMatricesValid_) {
    gizmo3DViewMatrix_ = cameraViewMatrix;
    gizmo3DProjectionMatrix_ = cameraProjMatrix;
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

      static_cast<uint8_t>(blendPipelineReady_ ? 1 : 0),

      static_cast<uint8_t>(showGrid_ ? 1 : 0),

      static_cast<uint8_t>(showGuides_ ? 1 : 0),

      static_cast<uint8_t>(showSafeMargins_ ? 1 : 0),

      static_cast<uint8_t>(showAnchorCenterOverlay_ ? 1 : 0),

      static_cast<uint8_t>(showCameraFrustumOverlay_ ? 1 : 0),

      static_cast<uint8_t>(showXRayOverlay_ ? 1 : 0),

      static_cast<uint8_t>(showIsolationOverlay_ ? 1 : 0),

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

                                  ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL;

                    });

    const bool hasCpuRasterizerUpload =

        std::any_of(layers.begin(), layers.end(),

                    [&](const ArtifactAbstractLayerPtr &layer) {

                      return layer && isLayerEffectivelyVisible(layer) &&

                             layer->isActiveAt(currentFrame) &&

                             layerHasCpuRasterizerWork(layer.get());

                    });

    const auto globalIlluminationState =
        renderer_->globalIlluminationState();
    const auto globalIlluminationSettings =
        renderer_->globalIlluminationSettings();
    const bool screenSpaceGlobalIlluminationRequested =
        globalIlluminationState.selectedMode ==
            GlobalIlluminationMode::SSGI ||
        globalIlluminationState.selectedMode ==
            GlobalIlluminationMode::Hybrid;
    const bool viewportMultiChannelRequested =
        renderer_->isMultiChannelEnabled();

    if (gpuBlendEnabled_ &&
        (hasGpuBlendJustification ||
         screenSpaceGlobalIlluminationRequested ||
         viewportMultiChannelRequested) &&
        !blendPipelineReady_) {
      scheduleBlendPipelineInitialization(
          owner, 0,
          screenSpaceGlobalIlluminationRequested
              ? QStringLiteral("screen-space-gi-requested")
              : QStringLiteral("non-normal-layer-visible"));
    }

    const bool gpuBlendRequested = gpuBlendEnabled_ && blendPipelineReady_;

    const bool gpuBlendPathRequested =

        gpuBlendRequested &&
        (hasGpuBlendJustification ||
         screenSpaceGlobalIlluminationRequested ||
         viewportMultiChannelRequested);



    auto &previewRenderSlot = acquirePreviewRenderPipelineSlot();

    auto &renderPipeline = previewRenderSlot.pipeline;

    const bool emissionChannelRequested =

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::Emission);

    const bool objectIdChannelRequested =

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::ObjectId);

    const bool materialIdChannelRequested =

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::MaterialId);

    const bool albedoChannelRequested =

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoR) ||

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoG) ||

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoB);

    const bool normalChannelRequested =

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::NormalX) ||

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::NormalY) ||

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::NormalZ);

    const bool velocityChannelRequested =

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::VelocityX) ||

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::VelocityY);

    const bool auxiliary3DChannelRequested =

        emissionChannelRequested || objectIdChannelRequested ||

        materialIdChannelRequested || albedoChannelRequested ||

        normalChannelRequested || velocityChannelRequested ||

        screenSpaceGlobalIlluminationRequested;



    // Avoid paying render-pipeline setup cost when GPU blending is disabled.

    if (gpuBlendPathRequested) {

      if (auto device = renderer_->device()) {

        const bool initializedWithF16 = renderPipeline.initialize(

            device,

            static_cast<Uint32>(previewRenderWidth),

            static_cast<Uint32>(previewRenderHeight),

            RenderConfig::PipelineFormatF16,

            auxiliary3DChannelRequested);

        if (!initializedWithF16) {
          qWarning() << "[CompositionView] RGBA16_FLOAT pipeline unavailable;"
                        " falling back to RGBA32_FLOAT";
          renderPipeline.initialize(

              device,

              static_cast<Uint32>(previewRenderWidth),

              static_cast<Uint32>(previewRenderHeight),

              RenderConfig::PipelineFormatF32,

              auxiliary3DChannelRequested);
        }

        if (!ensurePreviewRenderPipelineDepthSlot(

                previewRenderSlot,

                previewRenderWidth,

                previewRenderHeight)) {

          qWarning() << "[CompositionView] failed to allocate preview depth slot"

                     << "slot=" << activePreviewRenderPipelineSlot_

                     << "size="

                     << QSize(previewRenderWidth, previewRenderHeight);

        }

      }

    } else if (gpuBlendRequested && lastPipelineStateMask_ != -1) {

      qCDebug(compositionViewLog)

          << "[CompositionView] GPU blend path not used for this frame"

          << "layers=" << layers.size()

          << "frameOutOfRange=" << frameOutOfRange;

    }



    const bool previewRenderSlotAcquireHazard =

        lastPreviewRenderPipelineAcquireHazard_;

    const bool pipelineEnabled =

        gpuBlendPathRequested && renderPipeline.ready() &&

        previewRenderSlot.depthTargetView != nullptr &&

        !previewRenderSlotAcquireHazard;

    if (!pipelineEnabled) {

      lastLayerRtPixelStats_.clear();

      lastAccumRtPixelStats_.clear();

    }



    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::NormalX,

        (gpuBlendPathRequested && renderPipeline.hasNormalTarget())

            ? renderPipeline.normalSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::NormalY,

        (gpuBlendPathRequested && renderPipeline.hasNormalTarget())

            ? renderPipeline.normalSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::NormalZ,

        (gpuBlendPathRequested && renderPipeline.hasNormalTarget())

            ? renderPipeline.normalSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::VelocityX,

        (gpuBlendPathRequested && renderPipeline.hasVelocityTarget())

            ? renderPipeline.velocitySRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::VelocityY,

        (gpuBlendPathRequested && renderPipeline.hasVelocityTarget())

            ? renderPipeline.velocitySRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::Emission,

        (gpuBlendPathRequested && renderPipeline.hasEmissionTarget())

            ? renderPipeline.emissionSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::ObjectId,

        (gpuBlendPathRequested && renderPipeline.hasObjectIdTarget())

            ? renderPipeline.objectIdSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::MaterialId,

        (gpuBlendPathRequested && renderPipeline.hasMaterialIdTarget())

            ? renderPipeline.materialIdSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::AlbedoR,

        (gpuBlendPathRequested && renderPipeline.hasAlbedoTarget())

            ? renderPipeline.albedoSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::AlbedoG,

        (gpuBlendPathRequested && renderPipeline.hasAlbedoTarget())

            ? renderPipeline.albedoSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::AlbedoB,

        (gpuBlendPathRequested && renderPipeline.hasAlbedoTarget())

            ? renderPipeline.albedoSRV()

            : nullptr);

    viewportChannelDisplaySRV_ = nullptr;
    if (pipelineEnabled) {
      switch (viewportChannelDisplayMode_) {
      case ViewportChannelDisplayMode::Emission:
        viewportChannelDisplaySRV_ = renderPipeline.emissionSRV();
        break;
      case ViewportChannelDisplayMode::ObjectId:
        viewportChannelDisplaySRV_ = renderPipeline.objectIdSRV();
        break;
      case ViewportChannelDisplayMode::MaterialId:
        viewportChannelDisplaySRV_ = renderPipeline.materialIdSRV();
        break;
      case ViewportChannelDisplayMode::Albedo:
        viewportChannelDisplaySRV_ = renderPipeline.albedoSRV();
        break;
      case ViewportChannelDisplayMode::Normal:
        viewportChannelDisplaySRV_ = renderPipeline.normalSRV();
        break;
      case ViewportChannelDisplayMode::Velocity:
        viewportChannelDisplaySRV_ = renderPipeline.velocitySRV();
        break;
      default:
        break;
      }
    }

    previewRenderSlot.state = pipelineEnabled

                                  ? PreviewRenderPipelineSlot::State::Ready

                                  : PreviewRenderPipelineSlot::State::Free;

    const QString multiFrameReason = multiFramePreviewFallbackReason(

        gpuBlendEnabled_, blendPipelineReady_, hasGpuBlendJustification,

        hasCpuRasterizerUpload, gpuBlendPathRequested, renderPipeline.ready(),

        previewRenderSlot.depthTargetView != nullptr,

        previewRenderSlotAcquireHazard);

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

    if (previewRenderSlotAcquireHazard && gpuBlendPathRequested &&

               renderPipeline.ready()) {

      qCDebug(compositionViewLog)

          << "[CompositionView] submitted slot hazard forces fallback path"

          << "slot=" << activePreviewRenderPipelineSlot_

          << "acquire=" << lastPreviewRenderPipelineSlotAcquireReason_;

    }

    const auto framePassPlan =

        buildFrameRenderPassPlan(useRamPreviewFallback, pipelineEnabled,

                                 static_cast<bool>(comp));

    const QString framePassPlanSummary =

        summarizeFrameRenderPassPlan(framePassPlan);

    if (framePassPlanSummary != lastFrameRenderPassPlanSummary_) {

      lastFrameRenderPassPlanSummary_ = framePassPlanSummary;

      qCDebug(compositionViewLog)

          << "[CompositionView][FramePassPlan]" << framePassPlanSummary

          << "mode="

          << (useRamPreviewFallback ? QStringLiteral("ram-preview")

                                    : (pipelineEnabled ? QStringLiteral("gpu")

                                                       : QStringLiteral("direct")));

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

    qint64 surfacePassMs = 0;

    qint64 maskPassMs = 0;

    qint64 compositePassMs = 0;

    qint64 postPassMs = 0;

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

    RenderPassContext setupContext{renderer_.get(), renderFrameCounter_};

    RenderPassResources setupResources;

    FunctionalRenderPass setupPass(

        FrameRenderPassKind::Setup, QStringLiteral("Viewport Setup / Clear"),

        [](RenderPassResources&) { return true; },

        [&](RenderPassContext& context, RenderPassResources&) {

          context.renderer->clearRenderTarget(viewportClearColor_);

          return true;

        });

    RenderPassExecutor::run(setupPass, setupContext, setupResources);

    renderCrashTrace("render-clear-end", renderFrameCounter_);



    Diligent::ITextureView* ramPreviewReadbackSRV = nullptr;

    lastPresentedReadbackSRV_ = nullptr;

    if (useRamPreviewFallback) {

      renderCrashTrace("render-branch-ram-preview", renderFrameCounter_);

      RenderPassContext ramContext{renderer_.get(), renderFrameCounter_};

      RenderPassResources ramResources;

      FunctionalRenderPass ramBasePass(

          FrameRenderPassKind::Base, QStringLiteral("RAM Preview Base"),

          [](RenderPassResources&) { return true; },

          [&](RenderPassContext& context, RenderPassResources&) {

            if (backgroundMode == CompositionBackgroundMode::MayaGradient) {

              drawViewportMayaGradientBackground(

                  context.renderer, viewportW, viewportH, layerBgColor,

                  cachedMayaGradientSprite_);

            } else if (backgroundMode ==

                       CompositionBackgroundMode::Checkerboard) {

              drawViewportCheckerboardBackground(

                  context.renderer, viewportW, viewportH,

                  checkerboardTileSize_);

            }

            context.renderer->setUseExternalMatrices(false);

            context.renderer->resetGizmoCameraMatrices();

            context.renderer->reset3DCameraMatrices();

            context.renderer->setCanvasSize(cw, ch);

            context.renderer->setZoom(zoom);

            context.renderer->setPan(panX, panY);

            drawCompositionBackgroundDirect(

                context.renderer, cw, ch, layerBgColor, backgroundMode,

                checkerboardTileSize_, cachedMayaGradientSprite_,
                viewportOrientationMatricesValid_
                    ? &viewportOrientationViewForOverlay_
                    : nullptr,
                viewportOrientationMatricesValid_
                    ? &viewportOrientationProjectionForOverlay_
                    : nullptr);

            basePassMs = markPhaseMs();

            return true;

          });

      FunctionalRenderPass ramCompositePass(

          FrameRenderPassKind::Composite,

          QStringLiteral("RAM Preview Composite"),

          [](RenderPassResources&) { return true; },

          [&](RenderPassContext& context, RenderPassResources&) {

            QMatrix4x4 identity;

            context.renderer->drawSpriteTransformed(

                0.0f, 0.0f, cw, ch, identity, ramPreviewFrameImage, 1.0f);

            surfacePassMs = markPhaseMs();

            return true;

          });

      const std::array<RenderPass*, 2> ramPasses{

          &ramBasePass, &ramCompositePass};

      RenderPassExecutor::runAll(ramPasses, ramContext, ramResources);

      maskPassMs = surfacePassMs;

      compositePassMs = surfacePassMs;

      postPassMs = compositePassMs;

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

      auto emissionRTV = renderPipeline.emissionRTV();

      auto normalRTV = renderPipeline.normalRTV();

      auto velocityRTV = renderPipeline.velocityRTV();

      auto objectIdRTV = renderPipeline.objectIdRTV();

      auto materialIdRTV = renderPipeline.materialIdRTV();

      auto albedoRTV = renderPipeline.albedoRTV();



      // Pre-render matte source layers for GPU track matte

      QHash<ArtifactCore::Id, QImage> matteSourceImages;

      {

        const int matteTW = static_cast<int>(std::ceil(rcw));

        const int matteTH = static_cast<int>(std::ceil(rch));

        for (const auto &layerForMatte : layers) {

          if (!layerForMatte || !isLayerEffectivelyVisible(layerForMatte) ||

              !layerForMatte->isActiveAt(currentFrame))

            continue;

          auto mattes = layerForMatte->matteReferences();

          if (mattes.empty()) continue;

          for (const auto &matteRef : mattes) {

            if (!matteRef.enabled || matteRef.sourceLayerId.isNil()) continue;

            if (matteSourceImages.contains(matteRef.sourceLayerId)) continue;

            QImage matteImg = matteResolver(matteRef.sourceLayerId);

            if (matteImg.isNull()) continue;

            if (matteImg.size() != QSize(matteTW, matteTH)) {

              matteImg = matteImg.scaled(matteTW, matteTH,

                  Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

            }

            matteSourceImages.insert(matteRef.sourceLayerId,

                matteImg.convertToFormat(QImage::Format_RGBA8888));

          }

        }

      }



      const FloatColor layerBgColor = comp->backgroundColor();

      GpuBasePassState gpuBasePassState;

      RenderPassContext baseContext{renderer_.get(), renderFrameCounter_};

      RenderPassResources baseResources{

          &renderPipeline, layerRTV, layerSRV, layerFloatSRV,

          layerFloatUAV, accumSRV, tempUAV};

      FunctionalRenderPass basePass(

          FrameRenderPassKind::Base, QStringLiteral("Clear / Base"),

          [](RenderPassResources& resources) {

            return resources.pipeline && resources.layerRTV &&

                   resources.layerSRV && resources.accumSRV &&

                   resources.tempUAV;

          },

          [&](RenderPassContext&, RenderPassResources& resources) {

            gpuBasePassState = beginGpuBasePass(

                *resources.pipeline, previewRenderSlot, rcw, rch, cw, ch,

                layerBgColor, backgroundMode);

            seedGpuBasePassBackground(

                *resources.pipeline, resources.layerRTV, resources.layerSRV,

                resources.layerFloatSRV, resources.layerFloatUAV,

                resources.accumSRV, resources.tempUAV,

                layerToFloatConvertCount, blendDispatchCount,

                blendFailureCount, cw, ch, layerBgColor, backgroundMode);

            accumSRV = resources.accumSRV;

            tempUAV = resources.tempUAV;

            return true;

          });

      RenderPassExecutor::run(basePass, baseContext, baseResources);

      const float origZoom = gpuBasePassState.origZoom;

      const FloatColor origClearColor = gpuBasePassState.origClearColor;

      const float origPanX = gpuBasePassState.origPanX;

      const float origPanY = gpuBasePassState.origPanY;

      const float origViewW = gpuBasePassState.origViewW;

      const float origViewH = gpuBasePassState.origViewH;



      if (emissionRTV) {

        renderer_->clearRenderTarget(emissionRTV,

                                     FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

      }

      if (normalRTV) {

        renderer_->clearRenderTarget(normalRTV,

                                     FloatColor{0.5f, 0.5f, 1.0f, 1.0f});

      }

      if (velocityRTV) {

        renderer_->clearRenderTarget(velocityRTV,

                                     FloatColor{0.5f, 0.5f, 0.5f, 1.0f});

      }

      if (objectIdRTV) {

        renderer_->clearRenderTarget(objectIdRTV,

                                     FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

      }

      if (materialIdRTV) {

        renderer_->clearRenderTarget(materialIdRTV,

                                     FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

      }

      if (albedoRTV) {

        renderer_->clearRenderTarget(albedoRTV,

                                     FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

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

        bool shared3DSceneDepthOpen = false;

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

          const bool isolateSkipThisLayer =
              showIsolationOverlay_ && !selectedLayerId_.isNil() &&
              layer->id() != selectedLayerId_;
          if (isolateSkipThisLayer) {
            continue;
          }

          const bool xRayDimThisLayer =
              showXRayOverlay_ && !selectedLayerId_.isNil() &&
              layer->id() != selectedLayerId_;
          const float opacity =
              layer->opacity() * (xRayDimThisLayer ? 0.35f : 1.0f);

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

          // Model3D, solid/vector cards and alpha-split still/video cards share
          // one depth attachment while retaining the layer blend pipeline.
          const auto* sceneModelLayer =
              dynamic_cast<Artifact3DLayer*>(layer.get());
          const bool opaqueSolidCard = isOpaqueSolid3DCard(layer.get());
          DirectShape3DCard shapeCard;
          const bool opaqueShapeCard =
              directShape3DCard(layer.get(), shapeCard) &&
              (shapeCard.fillPoints.empty() ||
               shapeCard.fillColor.a() >= 0.9999f) &&
              (shapeCard.strokePoints.empty() ||
               shapeCard.strokeColor.a() >= 0.9999f);
          const bool texturedCard =
              gpuTextureCacheManager_ &&
              textured3DCardBuffer(layer.get()) != nullptr;
          const bool videoCard =
              gpuTextureCacheManager_ &&
              readyVideo3DCardLayer(layer.get(), currentFrame.framePosition()) !=
                  nullptr;
          const bool shared3DSceneDepthMember =
              ((sceneModelLayer != nullptr &&
                !sceneModelLayer->hasTransparentMaterial()) ||
               opaqueSolidCard || opaqueShapeCard || texturedCard ||
               videoCard) &&
              !layer->isAdjustmentLayer() &&
              blendMode == ArtifactCore::BlendMode::Normal &&
              opacity >= 0.9999f && layerMaskCount == 0 &&
              layer->matteReferences().empty();
          const bool preserveSceneDepth =
              shared3DSceneDepthMember && shared3DSceneDepthOpen;
          if (!shared3DSceneDepthMember) {
            shared3DSceneDepthOpen = false;
          }



          RenderPassContext passContext{renderer_.get(), renderFrameCounter_};

          RenderPassResources passResources{

              &renderPipeline, layerRTV, layerSRV, layerFloatSRV,

              layerFloatUAV, accumSRV, tempUAV};

          FunctionalRenderPass layerRasterPass(

              FrameRenderPassKind::Surface, QStringLiteral("Layer Raster"),

              [](RenderPassResources& resources) {

                return resources.pipeline && resources.layerRTV &&

                       resources.layerSRV;

              },

              [&](RenderPassContext&, RenderPassResources& resources) {

                drawGpuLayerToIntermediate(

                    layer.get(), resources.layerRTV, resources.accumSRV, rcw,

                    rch, cw, ch, lod, currentFrame, matteResolver, sceneLights,

                    has3DCamera, cameraViewMatrix, cameraProjMatrix,

                    preserveSceneDepth);

                if ((!draftRendering || emissionChannelRequested) && emissionRTV) {

                  drawGpuLayerEmissionToTarget(layer.get(), emissionRTV);

                }

                if ((!draftRendering || normalChannelRequested ||
                     screenSpaceGlobalIlluminationRequested) && normalRTV) {

                  drawGpuLayerNormalToTarget(layer.get(), normalRTV);

                }

                if ((!draftRendering || velocityChannelRequested) && velocityRTV) {

                  drawGpuLayerVelocityToTarget(layer.get(), velocityRTV);

                }

                if ((!draftRendering || objectIdChannelRequested) && objectIdRTV) {

                  const quint32 objectHash =

                      qHash(layer->id().toString(), 0x51a7u) & 0x00ffffffu;

                  drawGpuLayerIdToTarget(

                      layer.get(), objectIdRTV,

                      ArtifactIRenderer::ChannelType::ObjectId,

                      static_cast<float>(objectHash) / 16777215.0f);

                }

                if ((!draftRendering || materialIdChannelRequested) && materialIdRTV) {

                  QString materialKey =

                      layer->layerName() + QStringLiteral("|material");

                  if (auto* modelLayer =

                          dynamic_cast<Artifact3DLayer*>(layer.get())) {

                    materialKey = modelLayer->materialSignature();

                  }

                  const quint32 materialHash =

                      qHash(materialKey, 0x7f31u) & 0x00ffffffu;

                  drawGpuLayerIdToTarget(

                      layer.get(), materialIdRTV,

                      ArtifactIRenderer::ChannelType::MaterialId,

                      static_cast<float>(materialHash) / 16777215.0f);

                }

                if ((!draftRendering || albedoChannelRequested ||
                     screenSpaceGlobalIlluminationRequested) && albedoRTV) {

                  drawGpuLayerAlbedoToTarget(layer.get(), albedoRTV);

                }

                surfacePassMs = markPhaseMs();

                return true;

              });

          Diligent::ITextureView* preparedBlendSRV = layerSRV;

          bool convertedLayerToFloat = false;

          FunctionalRenderPass maskPass(

              FrameRenderPassKind::Mask, QStringLiteral("Mask / Track Matte"),

              [](RenderPassResources& resources) {

                return resources.pipeline && resources.layerSRV &&

                       resources.layerFloatSRV && resources.layerFloatUAV &&

                       resources.tempUAV;

              },

              [&](RenderPassContext&, RenderPassResources& resources) {

                preparedBlendSRV = prepareGpuLayerForBlend(

                    layer.get(), *resources.pipeline, resources.layerSRV,

                    resources.layerFloatSRV, resources.layerFloatUAV,

                    resources.tempUAV, matteSourceImages,

                    layerToFloatConvertCount, convertedLayerToFloat);

                maskPassMs = markPhaseMs();

                return preparedBlendSRV != nullptr;

              });

          GpuLayerBlendResult blendResult;

          FunctionalRenderPass blendPass(

              FrameRenderPassKind::Composite, QStringLiteral("Blend"),

              [](RenderPassResources& resources) {

                return resources.pipeline && resources.layerSRV &&

                       resources.accumSRV && resources.tempUAV;

              },

              [&](RenderPassContext&, RenderPassResources& resources) {

                blendResult = blendGpuLayerIntoAccum(

                    layer.get(), *resources.pipeline, resources.layerSRV,

                    preparedBlendSRV, resources.accumSRV, resources.tempUAV,

                    blendMode, opacity, cw, ch, blendDispatchCount,

                    blendRetryNormalCount, blendFailureCount,

                    directBlendFallbackCount, convertedLayerToFloat);

                accumSRV = resources.accumSRV;

                tempUAV = resources.tempUAV;

                compositePassMs = markPhaseMs();

                return blendResult.blended || blendResult.directFallbackUsed;

              });

          const std::array<RenderPass*, 3> layerPasses{

              &layerRasterPass, &maskPass, &blendPass};

          if (!RenderPassExecutor::runAll(layerPasses, passContext,

                                          passResources)) {

            shared3DSceneDepthOpen = false;

            continue;

          }

          if (!blendResult.blended) {

            shared3DSceneDepthOpen = false;

            continue;

          }

          shared3DSceneDepthOpen = shared3DSceneDepthMember;

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

        }

      }

      if (screenSpaceGlobalIlluminationRequested) {
        renderer_->unbindColorTargetsForCompute();
        auto* depthSRV = renderer_->offscreenTextureShaderResourceView(
            previewRenderSlot.depthTargetView);
        const auto giInputs =
            renderPipeline.globalIlluminationInputs(depthSRV);
        auto context = renderer_->immediateContext();
        const bool dispatched = context &&
            renderPipeline.dispatchScreenSpaceGlobalIllumination(
                context.RawPtr(), giInputs,
                globalIlluminationSettings.ssgiResolutionScale,
                globalIlluminationSettings.ssgiRaySteps,
                1.0f, 0.01f,
                globalIlluminationSettings.temporalAccumulation,
                globalIlluminationSettings.denoise);
        if (!dispatched && compositionViewLog().isDebugEnabled()) {
          qCDebug(compositionViewLog)
              << "[CompositionView] SSGI dispatch skipped"
              << renderer_->globalIlluminationDebugState()
              << "inputs=" << giInputs.validForScreenSpace();
        }
      }



      RenderPassContext resolveContext{renderer_.get(), renderFrameCounter_};

      RenderPassResources resolveResources{

          &renderPipeline, layerRTV, layerSRV, layerFloatSRV,

          layerFloatUAV, accumSRV, tempUAV};

      FunctionalRenderPass resolvePass(

          FrameRenderPassKind::Post, QStringLiteral("Resolve"),

          [](RenderPassResources& resources) {

            return resources.pipeline && resources.accumSRV &&

                   resources.layerFloatSRV && resources.layerFloatUAV;

          },

          [&](RenderPassContext&, RenderPassResources& resources) {

            ramPreviewReadbackSRV = finalizeGpuRenderToViewport(

                *resources.pipeline, resources.accumSRV,

                resources.layerFloatSRV, resources.layerFloatUAV, rcw, rch,

                cw, ch, origViewW, origViewH, origZoom, origPanX, origPanY,

                bgColor, layerBgColor, origClearColor, backgroundMode,

                layerToFloatConvertCount);

            return ramPreviewReadbackSRV != nullptr;

          });

      RenderPassExecutor::run(resolvePass, resolveContext, resolveResources);

      postPassMs = markPhaseMs();

    } else {

      // === Fallback path (GPU パイプラインなし) ===

      renderCrashTrace("render-branch-fallback", renderFrameCounter_);

      RenderPassContext fallbackContext{renderer_.get(), renderFrameCounter_};

      RenderPassResources fallbackResources;

      FunctionalRenderPass fallbackBasePass(

          FrameRenderPassKind::Base, QStringLiteral("Direct Base"),

          [](RenderPassResources&) { return true; },

          [&](RenderPassContext& context, RenderPassResources&) {

      context.renderer->setViewportRect(viewportW, viewportH);

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

                                      cachedMayaGradientSprite_,
                                      viewportOrientationMatricesValid_
                                          ? &viewportOrientationViewForOverlay_
                                          : nullptr,
                                      viewportOrientationMatricesValid_
                                          ? &viewportOrientationProjectionForOverlay_
                                          : nullptr);

      lastPresentedReadbackSRV_ = nullptr;

      renderer_->setUseExternalMatrices(false);

      renderer_->resetGizmoCameraMatrices();

      renderer_->reset3DCameraMatrices();

      renderer_->setCanvasSize(cw, ch);

      renderer_->setZoom(prevZoom);

      renderer_->setPan(prevPanX, prevPanY);

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

      return true;

          });

      RenderPassExecutor::run(fallbackBasePass, fallbackContext,

                              fallbackResources);

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

          const bool isolateSkipThisLayer =
              showIsolationOverlay_ && !selectedLayerId_.isNil() &&
              layer->id() != selectedLayerId_;
          if (isolateSkipThisLayer) {
            continue;
          }

          const bool xRayDimThisLayer =
              showXRayOverlay_ && !selectedLayerId_.isNil() &&
              layer->id() != selectedLayerId_;
          const float opacity =
              layer->opacity() * (xRayDimThisLayer ? 0.35f : 1.0f);

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

          RenderPassContext directContext{renderer_.get(),

                                          renderFrameCounter_};

          RenderPassResources directResources;

          FunctionalRenderPass directCompositePass(

              FrameRenderPassKind::Composite,

              QStringLiteral("Direct Layer Composite"),

              [](RenderPassResources&) { return true; },

              [&](RenderPassContext& context, RenderPassResources&) {

                drawLayerForCompositionView(

                    layer.get(), context.renderer, opacity, dbgOut,

                    &surfaceCache_, gpuTextureCacheManager_.get(),

                    currentFrame.framePosition(), false, lod,

                    has3DCamera ? &cameraViewMatrix : nullptr,

                    has3DCamera ? &cameraProjMatrix : nullptr,
                    has3DCamera ? &previousCameraViewMatrix : nullptr,
                    has3DCamera ? &previousCameraProjMatrix : nullptr, &matteResolver,
                    &sceneLights, draftRendering, viewportOrientationActive_,

                    surfaceGeneration(layer.get()));

                return true;

              });

          RenderPassExecutor::run(directCompositePass, directContext,

                                  directResources);

          surfacePassMs = markPhaseMs();

          maskPassMs = surfacePassMs;

          compositePassMs = surfacePassMs;



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

      postPassMs = markPhaseMs();

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



    const std::vector<ArtifactAbstractLayerPtr> layerVector(layers.cbegin(),

                                                            layers.cend());



    drawSelectionEditingOverlay(owner, comp, layerVector, selectedIds,

                                currentFrame, cw, ch, has3DCamera,

                                cameraViewMatrix, cameraProjMatrix);



    /* if (showGizmoOverlay_ && gizmo_) {

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

          if (!gizmo3DCameraMatricesValid_ && viewportW > 0.0f && viewportH > 0.0f) {

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

            gizmo3D_->draw(
                renderer_.get(),
                gizmo3DCameraMatricesValid_ ? gizmo3DViewMatrix_
                                             : renderer_->getViewMatrix(),
                gizmo3DCameraMatricesValid_ ? gizmo3DProjectionMatrix_
                                             : renderer_->getProjectionMatrix());

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



    if (renderer_ &&
        (!selectedIds.isEmpty() || !selectedLayerId_.isNil())) {

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

          if (!layer)

            continue;

          if (!selectedIds.isEmpty()) {

            if (!isLayerSelected(selectedIds, layer))

              continue;

          } else if (selectedLayerId_.isNil() ||

                     layer->id() != selectedLayerId_) {

            continue;

          }



          const auto &t3d = layer->transform3D();

          const int motionPathFps = std::max(
              1, static_cast<int>(std::round(comp->frameRate().framerate())));
          auto posTimes =
              motionPathPositionKeyTimes(layer, motionPathFps);

          if (posTimes.empty()) {

            motionPathCache_.valid = false;

            continue;

          }



          const int keyMinFrame = static_cast<int>(posTimes.front().value());

          const int keyMaxFrame = static_cast<int>(posTimes.back().value());

          int minFrame = keyMinFrame;

          int maxFrame = keyMaxFrame;

          const int currentFrameNum = currentFrame.framePosition();

          const bool hasPathSegment = posTimes.size() >= 2;



          if (minFrame > maxFrame)

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



            if (hasPathSegment && minFrame <= maxFrame) {

              const int sampleStep =
                  motionPathAdaptiveSampleStep(minFrame, maxFrame, zoom);
              for (int f = minFrame;; f += sampleStep) {
                if (f > maxFrame) f = maxFrame;

                ArtifactCore::RationalTime t(f, rate);

                QTransform gTrans = layer->getGlobalTransformAt(f);

                float ax = t3d.anchorXAt(t);

                float ay = t3d.anchorYAt(t);

                QPointF wPos = gTrans.map(QPointF(ax, ay));

                motionPathCache_.pathPoints.push_back(

                    {f, (float)wPos.x(), (float)wPos.y()});

                if (f == maxFrame) break;

              }

            }



            for (const auto &kfTime : posTimes) {

              int f = static_cast<int>(kfTime.value());

              if (f < keyMinFrame || f > keyMaxFrame)

                continue;

              QTransform gTrans = layer->getGlobalTransformAt(f);

              float ax = t3d.anchorXAt(kfTime);

              float ay = t3d.anchorYAt(kfTime);

              QPointF wPos = gTrans.map(QPointF(ax, ay));

              const int interp =
                  motionPathPositionInterpolation(layer, kfTime);

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
            float previousDistance = 0.0f;

            for (const auto &pt : motionPathCache_.pathPoints) {

              Detail::float2 currentPos(pt.x, pt.y);

              if (hasLastPos) {
                previousDistance = std::hypot(currentPos.x - lastPos.x,
                                              currentPos.y - lastPos.y);

                drawTaggedSolidLine(renderer_.get(),

                                    {lastPos.x, lastPos.y},

                                    {currentPos.x, currentPos.y}, pathColor,

                                    lineThickness, true);

              }

              const float speedRadius = std::clamp(
                  dotRadius * (0.45f + previousDistance * 0.015f),
                  dotRadius * 0.45f, dotRadius * 1.25f);
              const FloatColor dotColor =
                  pt.frame < currentFrameNum
                      ? FloatColor{0.98f, 0.48f, 0.72f, 0.78f}
                      : FloatColor{0.42f, 0.72f, 1.0f, 0.78f};
              renderer_->drawPoint(pt.x, pt.y, speedRadius,
                                   dotColor);

              lastPos = currentPos;

              hasLastPos = true;

            }

            // Evaluate the current marker directly instead of snapping it to
            // the nearest adaptive sample.
            if (currentFrameNum >= minFrame && currentFrameNum <= maxFrame) {
              const ArtifactCore::RationalTime currentTime(
                  static_cast<int64_t>(currentFrameNum), rate);
              const QTransform currentTransform =
                  layer->getGlobalTransformAt(currentFrameNum);
              const QPointF currentPosition = currentTransform.map(
                  QPointF(t3d.anchorXAt(currentTime),
                          t3d.anchorYAt(currentTime)));
              renderer_->drawPoint(static_cast<float>(currentPosition.x()),
                                   static_cast<float>(currentPosition.y()),
                                   dotRadius * 2.0f,
                                   {0.12f, 0.10f, 0.02f, 0.85f});
              renderer_->drawPoint(static_cast<float>(currentPosition.x()),
                                   static_cast<float>(currentPosition.y()),
                                   dotRadius * 1.25f,
                                   {0.98f, 0.88f, 0.35f, 1.0f});
            }

          }

          for (const auto &pt : motionPathCache_.keyPoints) {

            const bool isCurrent = pt.frame == currentFrameNum;

            const bool isHovered = pt.frame == hoveredMotionPathFrame_;

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

                isHovered ? dotRadius * 3.4f : dotRadius * 2.8f;

            const float innerRadius =

                isHovered ? dotRadius * 2.0f : dotRadius * 1.65f;

            const FloatColor ringColor = isHovered

                                              ? FloatColor{1.0f, 1.0f, 1.0f, 0.95f}

                                              : FloatColor{0.96f, 0.96f, 1.0f, 0.92f};

            renderer_->drawPoint(pt.x, pt.y, outerRadius, ringColor);

            renderer_->drawPoint(pt.x, pt.y, innerRadius, keyColor);

          }

        }

      }

      {

        ArtifactCore::ProfileScope _profBBox(

            "BoundingBox", ArtifactCore::ProfileCategory::Render);

        // Selection frames are layer controls, not composition guides. Keep
        // them visible whenever gizmos are enabled even if guides are hidden.
        if (showGizmoOverlay_) {

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

    } */



    const std::vector<ArtifactAbstractLayerPtr> overlayLayers(layers.cbegin(),

                                                              layers.cend());

    RenderPassContext overlayContext{renderer_.get(), renderFrameCounter_};

    RenderPassResources overlayResources;

    FunctionalRenderPass overlayPass(

        FrameRenderPassKind::Overlay, QStringLiteral("Overlay"),

        [](RenderPassResources&) { return true; },

        [&](RenderPassContext&, RenderPassResources&) {

          drawViewportCanvasOverlay(cw, ch);

          drawViewportInteractionOverlay(owner, currentFrame, cw, ch);

          drawViewportGuideOverlay(comp, cw, ch);

          drawViewportOverlayPass(owner, comp, activeCamera, overlayLayers,

                                  selectedIds, currentFrame, cw, ch,

                                  has3DCamera, cameraViewMatrix,

                                  cameraProjMatrix);

          return true;

        });

    RenderPassExecutor::run(overlayPass, overlayContext, overlayResources);

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

      PresentStageResult presentResult;

      RenderPassContext presentContext{renderer_.get(), renderFrameCounter_};

      RenderPassResources presentResources;

      FunctionalRenderPass presentPass(

          FrameRenderPassKind::Present, QStringLiteral("Present"),

          [](RenderPassResources&) { return true; },

          [&](RenderPassContext&, RenderPassResources&) {

            presentResult = presentAndUpdateVideoLayers(

                layerVector, currentFrame, useRamPreviewFallback,

                playbackPreviewState, ramPreviewFallbackReason);

            return !presentResult.presentedStatus.contains(

                QStringLiteral("fail"), Qt::CaseInsensitive);

          });

      RenderPassExecutor::run(presentPass, presentContext, presentResources);

      if (!presentResult.presentedVideoDebug.isEmpty() &&

          presentResult.presentedVideoDebug != lastEmittedVideoDebug_) {

        lastVideoDebug_ = presentResult.presentedVideoDebug;

        lastEmittedVideoDebug_ = presentResult.presentedVideoDebug;

        qDebug() << presentResult.presentedVideoDebug;

        Q_EMIT owner->videoDebugMessage(presentResult.presentedVideoDebug);

      }

      if (pipelineEnabled) {

        previewRenderSlot.state = PreviewRenderPipelineSlot::State::Retirable;

      }

      if (showOnionSkin_) {
        queueOnionSkinCapture(owner);
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

    lastSurfacePassMs_ = std::max<qint64>(0, surfacePassMs - basePassMs);

    lastMaskPassMs_ = std::max<qint64>(0, maskPassMs - surfacePassMs);

    lastCompositePassMs_ = std::max<qint64>(0, compositePassMs - maskPassMs);

    lastPostPassMs_ = std::max<qint64>(0, postPassMs - compositePassMs);

    lastOverlayMs_ = overlayMs;

    lastFlushMs_ = flushMs;

    lastSubmit2DMs_ = std::max<qint64>(lastSubmit2DMs_, 0);

    lastPresentMs_ = presentMs;

    const qint64 layerPassMs = std::max<qint64>(0, postPassMs - basePassMs);

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

    lastRenderPathSummary_ +=
        QStringLiteral(" gpuBlendInitState=%1 gpuBlendInitAttempts=%2 transparentOutput=%3")
            .arg(blendPipelineInitState_)
            .arg(blendPipelineInitAttemptCount_)
            .arg(currentBgColor.a() < 0.999f ? 1 : 0);

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

            "frameOutOfRange=%12 cpuRasterizerUpload=%13 "

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

            .arg(hasCpuRasterizerUpload ? 1 : 0)

            .arg(forceContinuousRedraw ? 1 : 0)

            .arg(viewportInteracting_ ? 1 : 0)

            .arg(layerToFloatConvertCount)

            .arg(hasCpuRasterizerUpload

                     ? QStringLiteral("gpuBlendUsesCpuRasterizerUpload")

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



void CompositionRenderController::Impl::drawViewportOverlayPass(

    CompositionRenderController *owner, const ArtifactCompositionPtr &comp,

    ArtifactCameraLayer *activeCamera,

    const std::vector<ArtifactAbstractLayerPtr> &layers,

    const QStringList &selectedIds, const FramePosition &currentFrame,

    float cw, float ch, bool has3DCamera,

    const QMatrix4x4 &cameraViewMatrix,

    const QMatrix4x4 &cameraProjMatrix) {

  ArtifactCore::ProfileScope _profOverlay("Overlay",

                                          ArtifactCore::ProfileCategory::UI);

  const ArtifactAbstractLayerPtr selectedLayer =

      (!selectedLayerId_.isNil() && comp) ? comp->layerById(selectedLayerId_)

                                          : ArtifactAbstractLayerPtr{};

  renderer_->setUseExternalMatrices(false);

  renderer_->resetGizmoCameraMatrices();

  renderer_->reset3DCameraMatrices();

  renderer_->setCanvasSize(cw, ch);

  drawViewportChannelOverlayImage(cw, ch);

  drawReferenceOverlayImage(cw, ch);

  drawOnionSkinOverlay(cw, ch);

  if (showCompositionRegionOverlay_ &&
      !viewportOrientationMatricesValid_) {
    ::Artifact::drawCompositionRegionOverlay(renderer_.get(), comp);
  }
  if (viewportOrientationMatricesValid_) {
    const float borderThickness =
        std::max(1.0f, 2.0f / std::max(0.001f, renderer_->getZoom()));
    const FloatColor borderColor{0.42f, 0.82f, 1.0f, 0.92f};
    QMatrix4x4 identity;
    renderer_->setViewMatrix(viewportOrientationViewForOverlay_);
    renderer_->setProjectionMatrix(
        viewportOrientationProjectionForOverlay_);
    renderer_->setUseExternalMatrices(true);
    renderer_->drawSolidRectTransformed(
        0.0f, 0.0f, cw, borderThickness, identity, borderColor, 1.0f);
    renderer_->drawSolidRectTransformed(
        0.0f, std::max(0.0f, ch - borderThickness), cw, borderThickness,
        identity, borderColor, 1.0f);
    renderer_->drawSolidRectTransformed(
        0.0f, 0.0f, borderThickness, ch, identity, borderColor, 1.0f);
    renderer_->drawSolidRectTransformed(
        std::max(0.0f, cw - borderThickness), 0.0f, borderThickness, ch,
        identity, borderColor, 1.0f);
    renderer_->setUseExternalMatrices(false);
  }
  if (comp) {
    const QString activeTransformFieldId = comp->activeTransformFieldId();
    QSet<LayerID> selectedLayerIds;
    if (auto* selection = ArtifactLayerSelectionManager::instance()) {
      for (const auto& layer : selection->selectedLayers()) {
        if (layer) {
          selectedLayerIds.insert(layer->id());
        }
      }
    }
    for (const auto& field : comp->transformFields()) {
      const bool isActiveField = field.fieldId == activeTransformFieldId;
      if (!field.enabled && !isActiveField) {
        continue;
      }
      const bool isDisabledField = !field.enabled;
      bool targetsSelection = false;
      for (const auto& targetLayerId : field.targetLayerIds) {
        if (selectedLayerIds.contains(targetLayerId)) {
          targetsSelection = true;
          break;
        }
      }
      if (!targetsSelection && !isActiveField) {
        continue;
      }

      const QPointF displayCenter = transformFieldDisplayCenter(comp, field);
      const QPointF displayRadiusPoint =
          transformFieldDisplayRadiusPoint(comp, field);
      const QPointF displaySecondaryRadiusPoint =
          transformFieldDisplaySecondaryRadiusPoint(comp, field);
      auto displayBoxCorners = transformFieldDisplayBoxCorners(comp, field);
      const float displayRadius = static_cast<float>(
          std::hypot(displayRadiusPoint.x() - displayCenter.x(),
                     displayRadiusPoint.y() - displayCenter.y()));
      if (displayRadius <= 0.001f) {
        continue;
      }
      const float inverseZoom =
          1.0f / std::max(0.001f, renderer_->getZoom());
      const bool isDraggingField =
          draggingTransformFieldId_ == field.fieldId && isDraggingTransformField_;
      const bool isHoveredField =
          hoveredTransformFieldId_ == field.fieldId &&
          hoveredTransformFieldMode_ != TransformFieldDragMode::None;
      const FloatColor fieldColor = isDraggingField
                                        ? FloatColor{1.0f, 0.55f, 0.18f, 0.96f}
                                        : isHoveredField
                                              ? FloatColor{1.0f, 0.83f, 0.32f, 0.96f}
                                              : isDisabledField
                                                    ? FloatColor{0.58f, 0.58f, 0.58f, 0.70f}
                                                    : isActiveField
                                                    ? FloatColor{0.38f, 0.95f, 0.63f, 0.95f}
                                                    : FloatColor{0.22f, 0.78f, 1.0f, 0.92f};
      const float strokeWidth = isActiveField ? 2.2f : 1.5f;
      const FloatColor guideColor =
          isDraggingField
              ? FloatColor{1.0f, 0.66f, 0.32f, 0.55f}
              : isHoveredField
                    ? FloatColor{1.0f, 0.86f, 0.44f, 0.42f}
                    : isActiveField
                          ? FloatColor{0.38f, 0.95f, 0.63f, 0.32f}
                          : FloatColor{0.22f, 0.78f, 1.0f, 0.22f};
      renderer_->drawSolidLine(
          {static_cast<float>(displayCenter.x()),
           static_cast<float>(displayCenter.y())},
          {static_cast<float>(displayRadiusPoint.x()),
           static_cast<float>(displayRadiusPoint.y())},
          guideColor, std::max(1.0f, 1.2f * inverseZoom));
      if (field.shape == QStringLiteral("box")) {
        renderer_->drawSolidLine(
            {static_cast<float>(displayCenter.x()),
             static_cast<float>(displayCenter.y())},
            {static_cast<float>(displaySecondaryRadiusPoint.x()),
             static_cast<float>(displaySecondaryRadiusPoint.y())},
            guideColor, std::max(1.0f, 1.2f * inverseZoom));
      }
      const float centerHandleRadius =
          isDraggingField ? std::max(5.5f, 8.0f * inverseZoom)
                          : isHoveredField ? std::max(4.5f, 6.5f * inverseZoom)
                                           : std::max(3.5f, 5.0f * inverseZoom);
      const float radiusHandleRadius =
          isDraggingField ? std::max(4.8f, 7.0f * inverseZoom)
                          : isHoveredField ? std::max(4.0f, 5.8f * inverseZoom)
                                           : std::max(3.0f, 4.8f * inverseZoom);
      if (field.shape == QStringLiteral("box")) {
        for (size_t cornerIndex = 0; cornerIndex < displayBoxCorners.size();
             ++cornerIndex) {
          const QPointF &from = displayBoxCorners[cornerIndex];
          const QPointF &to =
              displayBoxCorners[(cornerIndex + 1) % displayBoxCorners.size()];
          renderer_->drawSolidLine(
              {static_cast<float>(from.x()), static_cast<float>(from.y())},
              {static_cast<float>(to.x()), static_cast<float>(to.y())},
              fieldColor, strokeWidth);
        }
      } else if (field.shape == QStringLiteral("linear")) {
        const QPointF direction = displayRadiusPoint - displayCenter;
        const qreal directionLength =
            std::max<qreal>(0.0001, std::hypot(direction.x(), direction.y()));
        const QPointF unitDirection = direction / directionLength;
        const QPointF perpendicular(-unitDirection.y(), unitDirection.x());
        const QPointF negativeEdge = displayCenter - direction;
        const qreal crossbarLength = std::max<qreal>(
            12.0 * inverseZoom,
            std::hypot(displaySecondaryRadiusPoint.x() - displayCenter.x(),
                       displaySecondaryRadiusPoint.y() - displayCenter.y()));
        renderer_->drawSolidLine(
            {static_cast<float>(negativeEdge.x()),
             static_cast<float>(negativeEdge.y())},
            {static_cast<float>(displayRadiusPoint.x()),
             static_cast<float>(displayRadiusPoint.y())},
            fieldColor, strokeWidth);
        for (const QPointF edge : {negativeEdge, displayRadiusPoint}) {
          const QPointF from = edge - perpendicular * crossbarLength;
          const QPointF to = edge + perpendicular * crossbarLength;
          renderer_->drawSolidLine(
              {static_cast<float>(from.x()), static_cast<float>(from.y())},
              {static_cast<float>(to.x()), static_cast<float>(to.y())},
              fieldColor, strokeWidth);
        }
      } else {
        renderer_->drawCircle(
            static_cast<float>(displayCenter.x()),
            static_cast<float>(displayCenter.y()), displayRadius, fieldColor,
            strokeWidth, false);
      }
      renderer_->drawCircle(
          static_cast<float>(displayCenter.x()),
          static_cast<float>(displayCenter.y()),
          centerHandleRadius, fieldColor, 1.0f, true);
      renderer_->drawCircle(
          static_cast<float>(displayRadiusPoint.x()),
          static_cast<float>(displayRadiusPoint.y()),
          radiusHandleRadius, fieldColor, 1.0f, true);
      if (field.shape == QStringLiteral("box")) {
        renderer_->drawCircle(
            static_cast<float>(displaySecondaryRadiusPoint.x()),
            static_cast<float>(displaySecondaryRadiusPoint.y()),
            radiusHandleRadius, fieldColor, 1.0f, true);
      }
      drawTransformFieldBadge(renderer_.get(), field, displayCenter,
                              displayRadiusPoint, isHoveredField,
                              isDraggingField, isActiveField, inverseZoom);
    }
  }
  if (showCameraFrustumOverlay_ && activeCamera) {
    const auto cameraOverlayVisual = buildCameraFrustumVisual(activeCamera, comp);

    ::Artifact::drawCameraFrustumOverlay(renderer_.get(), cameraOverlayVisual,

                                         activeCamera->id() == selectedLayerId_);

  }

  if (showDensityHeatmapOverlay_) {

    drawVisualDensityOverlay(renderer_.get(), comp, selectedLayer, currentFrame);

  }

  if (selectedLayer) {

    if (!showGizmoOverlay_) {
      const QMatrix4x4* overlayViewMatrix =
          viewportOrientationMatricesValid_ ? &viewportOrientationViewForOverlay_
                                            : nullptr;
      const QMatrix4x4* overlayProjMatrix =
          viewportOrientationMatricesValid_
              ? &viewportOrientationProjectionForOverlay_
              : nullptr;

      ::Artifact::drawSelectionOverlay(
          renderer_.get(), selectedLayer, overlayViewMatrix,
          overlayProjMatrix);

    }

    if (selectedLayer->isCloneLayer()) {

      ::Artifact::drawClonerFrameOverlay(renderer_.get(), selectedLayer);

    }

    const bool selectedLayerIsActiveCamera =

        activeCamera && activeCamera->id() == selectedLayer->id();

    ::Artifact::drawCameraSelectionOverlay(renderer_.get(), selectedLayer,

                                           selectedLayerIsActiveCamera);

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

  if (workCursorVisible_ && !workCursorState_.spatial) {

    ::Artifact::drawViewportWorkCursorOverlay(renderer_.get(),
                                              workCursorCanvasPos_,
                                              workCursorLabel_);

  }

  drawSelectionEditingOverlay(owner, comp, layers, selectedIds, currentFrame,

                              cw, ch, has3DCamera, cameraViewMatrix,

                              cameraProjMatrix);

  drawViewportGhostOverlay(owner, comp, selectedLayer, currentFrame);

  drawViewportUiOverlay();

}

void CompositionRenderController::Impl::drawReferenceOverlayImage(
    float canvasWidth, float canvasHeight) {

  if (!renderer_ || !showReferenceOverlay_ || referenceOverlayImage_.isNull()) {

    return;

  }

  const float imageWidth = static_cast<float>(referenceOverlayImage_.width());

  const float imageHeight = static_cast<float>(referenceOverlayImage_.height());

  if (imageWidth <= 0.0f || imageHeight <= 0.0f || canvasWidth <= 0.0f ||
      canvasHeight <= 0.0f) {

    return;

  }

  const float scale =
      std::min(canvasWidth / imageWidth, canvasHeight / imageHeight);

  const float drawWidth = std::max(1.0f, imageWidth * scale);

  const float drawHeight = std::max(1.0f, imageHeight * scale);

  const float drawX = (canvasWidth - drawWidth) * 0.5f;

  const float drawY = (canvasHeight - drawHeight) * 0.5f;

  renderer_->drawSprite(drawX, drawY, drawWidth, drawHeight,
                        referenceOverlayImage_, 0.38f);

}

void CompositionRenderController::Impl::drawViewportChannelOverlayImage(
    float canvasWidth, float canvasHeight) {

  if (!renderer_ ||
      viewportChannelDisplayMode_ == ViewportChannelDisplayMode::Color) {

    return;

  }

  if (viewportChannelDisplaySRV_) {
    renderer_->drawSprite(0.0f, 0.0f, canvasWidth, canvasHeight,
                          viewportChannelDisplaySRV_, 1.0f);
    return;
  }

  const QImage channelImage = composeViewportChannelOverlayImage();
  if (channelImage.isNull()) {

    return;

  }

  const float imageWidth = static_cast<float>(channelImage.width());
  const float imageHeight = static_cast<float>(channelImage.height());
  if (imageWidth <= 0.0f || imageHeight <= 0.0f || canvasWidth <= 0.0f ||
      canvasHeight <= 0.0f) {

    return;

  }

  const float scale = std::min(canvasWidth / imageWidth, canvasHeight / imageHeight);
  const float drawWidth = std::max(1.0f, imageWidth * scale);
  const float drawHeight = std::max(1.0f, imageHeight * scale);
  const float drawX = (canvasWidth - drawWidth) * 0.5f;
  const float drawY = (canvasHeight - drawHeight) * 0.5f;

  renderer_->drawSprite(drawX, drawY, drawWidth, drawHeight, channelImage, 1.0f);

}

QImage CompositionRenderController::Impl::composeViewportChannelOverlayImage() const {

  if (!renderer_ ||
      viewportChannelDisplayMode_ == ViewportChannelDisplayMode::Color) {
    return {};
  }

  const auto composeRgb = [](const QImage &rImage, const QImage &gImage,
                             const QImage &bImage, int fallbackBlue = 0) {
    if (rImage.isNull() || gImage.isNull() || bImage.isNull() ||
        rImage.size() != gImage.size() || rImage.size() != bImage.size()) {
      return QImage{};
    }
    QImage out(rImage.size(), QImage::Format_RGBA8888);
    for (int y = 0; y < out.height(); ++y) {
      const auto *rSrc = rImage.constScanLine(y);
      const auto *gSrc = gImage.constScanLine(y);
      const auto *bSrc = bImage.constScanLine(y);
      auto *dst = out.scanLine(y);
      for (int x = 0; x < out.width(); ++x) {
        dst[x * 4 + 0] = rSrc[x];
        dst[x * 4 + 1] = gSrc[x];
        dst[x * 4 + 2] = bSrc[x];
        dst[x * 4 + 3] = 255;
      }
    }

    Q_UNUSED(fallbackBlue);
    return out;
  };

  const auto composeRgbAlpha = [](const QImage &rgbImage, const QImage &alphaImage) {
    if (rgbImage.isNull() || alphaImage.isNull() ||
        rgbImage.size() != alphaImage.size()) {
      return QImage{};
    }
    const QImage rgba = (rgbImage.format() == QImage::Format_RGBA8888)
                            ? rgbImage
                            : rgbImage.convertToFormat(QImage::Format_RGBA8888);
    const QImage alpha = (alphaImage.format() == QImage::Format_Grayscale8)
                             ? alphaImage
                             : alphaImage.convertToFormat(QImage::Format_Grayscale8);
    QImage out(rgbImage.width() * 2, rgbImage.height(), QImage::Format_RGBA8888);
    for (int y = 0; y < out.height(); ++y) {
      const auto *rgbSrc = rgba.constScanLine(y);
      const auto *alphaSrc = alpha.constScanLine(y);
      auto *dst = out.scanLine(y);
      for (int x = 0; x < rgbImage.width(); ++x) {
        const int dstX = x * 4;
        dst[dstX + 0] = rgbSrc[x * 4 + 0];
        dst[dstX + 1] = rgbSrc[x * 4 + 1];
        dst[dstX + 2] = rgbSrc[x * 4 + 2];
        dst[dstX + 3] = rgbSrc[x * 4 + 3];
      }
      for (int x = 0; x < alpha.width(); ++x) {
        const int v = alphaSrc[x];
        const int dstX = (x + rgbImage.width()) * 4;
        dst[dstX + 0] = static_cast<uchar>(v);
        dst[dstX + 1] = static_cast<uchar>(v);
        dst[dstX + 2] = static_cast<uchar>(v);
        dst[dstX + 3] = 255;
      }
    }
    return out;
  };

  const auto composeVelocity = [](const QImage &xImage, const QImage &yImage) {
    if (xImage.isNull() || yImage.isNull() || xImage.size() != yImage.size()) {
      return QImage{};
    }
    QImage out(xImage.size(), QImage::Format_RGBA8888);
    for (int y = 0; y < out.height(); ++y) {
      const auto *xSrc = xImage.constScanLine(y);
      const auto *ySrc = yImage.constScanLine(y);
      auto *dst = out.scanLine(y);
      for (int x = 0; x < out.width(); ++x) {
        dst[x * 4 + 0] = xSrc[x];
        dst[x * 4 + 1] = ySrc[x];
        dst[x * 4 + 2] = 127;
        dst[x * 4 + 3] = 255;
      }
    }
    return out;
  };

  const auto pseudoColorGray = [](const QImage &grayImage, bool invert = false) {
    if (grayImage.isNull()) {
      return QImage{};
    }
    const QImage gray = (grayImage.format() == QImage::Format_Grayscale8)
                            ? grayImage
                            : grayImage.convertToFormat(QImage::Format_Grayscale8);
    QImage out(gray.size(), QImage::Format_RGBA8888);
    for (int y = 0; y < out.height(); ++y) {
      const auto *src = gray.constScanLine(y);
      auto *dst = out.scanLine(y);
      for (int x = 0; x < out.width(); ++x) {
        const int v = invert ? (255 - src[x]) : src[x];
        const int r = std::clamp(64 + v * 3 / 2, 0, 255);
        const int g = std::clamp((255 - std::abs(v - 128) * 2), 0, 255);
        const int b = std::clamp(255 - v / 2, 0, 255);
        dst[x * 4 + 0] = static_cast<uchar>(r);
        dst[x * 4 + 1] = static_cast<uchar>(g);
        dst[x * 4 + 2] = static_cast<uchar>(b);
        dst[x * 4 + 3] = 255;
      }
    }
    return out;
  };

  auto readChannel = [this](ArtifactIRenderer::ChannelType channel) {
    return renderer_->readbackChannelToImage(channel);
  };

  switch (viewportChannelDisplayMode_) {
  case ViewportChannelDisplayMode::Alpha:
    return readChannel(ArtifactIRenderer::ChannelType::Alpha);
  case ViewportChannelDisplayMode::ColorAlpha:
    return composeRgbAlpha(
        renderer_->readbackToImage(),
        readChannel(ArtifactIRenderer::ChannelType::Alpha));
  case ViewportChannelDisplayMode::Red:
    return readChannel(ArtifactIRenderer::ChannelType::Red);
  case ViewportChannelDisplayMode::Green:
    return readChannel(ArtifactIRenderer::ChannelType::Green);
  case ViewportChannelDisplayMode::Blue:
    return readChannel(ArtifactIRenderer::ChannelType::Blue);
  case ViewportChannelDisplayMode::Depth:
    return pseudoColorGray(
        readChannel(ArtifactIRenderer::ChannelType::Depth), true);
  case ViewportChannelDisplayMode::Emission:
    return readChannel(ArtifactIRenderer::ChannelType::Emission);
  case ViewportChannelDisplayMode::ObjectId:
    return pseudoColorGray(
        readChannel(ArtifactIRenderer::ChannelType::ObjectId));
  case ViewportChannelDisplayMode::MaterialId:
    return pseudoColorGray(
        readChannel(ArtifactIRenderer::ChannelType::MaterialId), true);
  case ViewportChannelDisplayMode::Albedo:
    return composeRgb(
        readChannel(ArtifactIRenderer::ChannelType::AlbedoR),
        readChannel(ArtifactIRenderer::ChannelType::AlbedoG),
        readChannel(ArtifactIRenderer::ChannelType::AlbedoB));
  case ViewportChannelDisplayMode::AlbedoR:
    return readChannel(ArtifactIRenderer::ChannelType::AlbedoR);
  case ViewportChannelDisplayMode::AlbedoG:
    return readChannel(ArtifactIRenderer::ChannelType::AlbedoG);
  case ViewportChannelDisplayMode::AlbedoB:
    return readChannel(ArtifactIRenderer::ChannelType::AlbedoB);
  case ViewportChannelDisplayMode::Normal:
    return composeRgb(
        readChannel(ArtifactIRenderer::ChannelType::NormalX),
        readChannel(ArtifactIRenderer::ChannelType::NormalY),
        readChannel(ArtifactIRenderer::ChannelType::NormalZ));
  case ViewportChannelDisplayMode::NormalX:
    return readChannel(ArtifactIRenderer::ChannelType::NormalX);
  case ViewportChannelDisplayMode::NormalY:
    return readChannel(ArtifactIRenderer::ChannelType::NormalY);
  case ViewportChannelDisplayMode::NormalZ:
    return readChannel(ArtifactIRenderer::ChannelType::NormalZ);
  case ViewportChannelDisplayMode::Velocity:
    return composeVelocity(
        readChannel(ArtifactIRenderer::ChannelType::VelocityX),
        readChannel(ArtifactIRenderer::ChannelType::VelocityY));
  case ViewportChannelDisplayMode::VelocityX:
    return readChannel(ArtifactIRenderer::ChannelType::VelocityX);
  case ViewportChannelDisplayMode::VelocityY:
    return readChannel(ArtifactIRenderer::ChannelType::VelocityY);
  case ViewportChannelDisplayMode::Color:
    return {};
  }

  return {};

}

void CompositionRenderController::Impl::drawOnionSkinOverlay(
    float canvasWidth, float canvasHeight) {

  if (!renderer_ || !showOnionSkin_ || canvasWidth <= 0.0f ||
      canvasHeight <= 0.0f) {

    return;

  }

  QVector<QImage> frames;
  {
    QMutexLocker locker(&onionSkinMutex_);
    frames = onionSkinFrames_;
  }

  if (frames.isEmpty()) {
    return;
  }

  const int frameCount = std::clamp(onionSkinFrameCount_, 1, 5);
  const int drawCount = std::min(frameCount, static_cast<int>(frames.size()));
  const float baseOpacity = std::clamp(onionSkinOpacity_ / 100.0f, 0.05f, 0.80f);

  for (int i = 0; i < drawCount; ++i) {
    const int index = frames.size() - 1 - i;
    const QImage &frame = frames.at(index);
    if (frame.isNull()) {
      continue;
    }
    const float ageFactor = 1.0f - (static_cast<float>(i) /
                                    std::max(1.0f, static_cast<float>(drawCount)));
    const float opacity = std::clamp(baseOpacity * (0.45f + 0.55f * ageFactor),
                                     0.05f, 0.85f);
    renderer_->drawSprite(0.0f, 0.0f, canvasWidth, canvasHeight, frame, opacity);
  }
}

void CompositionRenderController::Impl::appendOnionSkinFrame(QImage frame) {

  if (frame.isNull()) {
    return;
  }

  QMutexLocker locker(&onionSkinMutex_);
  onionSkinFrames_.prepend(std::move(frame));
  while (onionSkinFrames_.size() > 5) {
    onionSkinFrames_.removeLast();
  }
}

void CompositionRenderController::Impl::queueOnionSkinCapture(
    CompositionRenderController *owner) {

  if (!renderer_ || !showOnionSkin_ || !owner) {
    return;
  }

  {
    QMutexLocker locker(&onionSkinMutex_);
    if (onionSkinCapturePending_) {
      return;
    }
    onionSkinCapturePending_ = true;
  }

  renderer_->readbackToImageAsync(
      [this, owner](const QImage &capturedFrame) {
        if (!owner) {
          QMutexLocker locker(&onionSkinMutex_);
          onionSkinCapturePending_ = false;
          return;
        }
        const QImage capturedCopy = capturedFrame.copy();
        QMetaObject::invokeMethod(
            owner,
            [this, capturedCopy]() mutable {
              QMutexLocker locker(&onionSkinMutex_);
              onionSkinCapturePending_ = false;
              if (!showOnionSkin_ || capturedCopy.isNull()) {
                return;
              }
              onionSkinFrames_.prepend(capturedCopy);
              while (onionSkinFrames_.size() > 5) {
                onionSkinFrames_.removeLast();
              }
            },
            Qt::QueuedConnection);
      });
}

void CompositionRenderController::Impl::syncViewportChannelReadbackConfiguration() {

  if (!renderer_) {
    return;
  }

  const bool needsAuxChannel =
      viewportChannelDisplayMode_ != ViewportChannelDisplayMode::Color &&
      viewportChannelDisplayMode_ != ViewportChannelDisplayMode::Alpha &&
      viewportChannelDisplayMode_ != ViewportChannelDisplayMode::ColorAlpha &&
      viewportChannelDisplayMode_ != ViewportChannelDisplayMode::Red &&
      viewportChannelDisplayMode_ != ViewportChannelDisplayMode::Green &&
      viewportChannelDisplayMode_ != ViewportChannelDisplayMode::Blue;

  if (needsAuxChannel) {
    renderer_->setMultiChannelEnabled(true);
  }

  switch (viewportChannelDisplayMode_) {
  case ViewportChannelDisplayMode::Depth:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::Depth, true);
    break;
  case ViewportChannelDisplayMode::Emission:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::Emission, true);
    break;
  case ViewportChannelDisplayMode::ObjectId:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::ObjectId, true);
    break;
  case ViewportChannelDisplayMode::MaterialId:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::MaterialId, true);
    break;
  case ViewportChannelDisplayMode::Albedo:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoR, true);
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoG, true);
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoB, true);
    break;
  case ViewportChannelDisplayMode::AlbedoR:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoR, true);
    break;
  case ViewportChannelDisplayMode::AlbedoG:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoG, true);
    break;
  case ViewportChannelDisplayMode::AlbedoB:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoB, true);
    break;
  case ViewportChannelDisplayMode::Normal:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalX, true);
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalY, true);
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalZ, true);
    break;
  case ViewportChannelDisplayMode::NormalX:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalX, true);
    break;
  case ViewportChannelDisplayMode::NormalY:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalY, true);
    break;
  case ViewportChannelDisplayMode::NormalZ:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalZ, true);
    break;
  case ViewportChannelDisplayMode::Velocity:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::VelocityX, true);
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::VelocityY, true);
    break;
  case ViewportChannelDisplayMode::VelocityX:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::VelocityX, true);
    break;
  case ViewportChannelDisplayMode::VelocityY:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::VelocityY, true);
    break;
  case ViewportChannelDisplayMode::Color:
  case ViewportChannelDisplayMode::Alpha:
  case ViewportChannelDisplayMode::Red:
  case ViewportChannelDisplayMode::Green:
  case ViewportChannelDisplayMode::Blue:
    break;
  }

}

bool CompositionRenderController::Impl::updateColorSamplerOverlay(
    CompositionRenderController *owner, const QPointF &viewportPos) {

  if (!showColorSamplerOverlay_ || !renderer_ || !owner) {

    return false;

  }

  const QImage frame = owner->captureCurrentFrameImage();

  if (frame.isNull()) {

    const bool hadSample = colorSamplerHasSample_;

    colorSamplerHasSample_ = false;

    return hadSample;

  }

  const int sampleX = std::clamp(static_cast<int>(std::lround(viewportPos.x())), 0,
                                 std::max(0, frame.width() - 1));

  const int sampleY = std::clamp(static_cast<int>(std::lround(viewportPos.y())), 0,
                                 std::max(0, frame.height() - 1));

  const QColor sampledColor = frame.pixelColor(sampleX, sampleY);

  const auto canvasPos =
      renderer_->viewportToCanvas({static_cast<float>(viewportPos.x()),
                                   static_cast<float>(viewportPos.y())});

  const bool changed =
      !colorSamplerHasSample_ || colorSamplerColor_ != sampledColor ||
      colorSamplerImagePixel_ != QPoint(sampleX, sampleY) ||
      std::abs(colorSamplerCanvasPos_.x() - canvasPos.x) > 0.5 ||
      std::abs(colorSamplerCanvasPos_.y() - canvasPos.y) > 0.5;

  colorSamplerHasSample_ = true;
  colorSamplerColor_ = sampledColor;
  colorSamplerImagePixel_ = QPoint(sampleX, sampleY);
  colorSamplerCanvasPos_ = QPointF(canvasPos.x, canvasPos.y);

  return changed;

}

void CompositionRenderController::Impl::rebuildReferencePaletteOverlay() {

  referenceDominantPalette_.clear();

  referenceHarmonyPalette_.clear();

  if (referenceOverlayImage_.isNull()) {

    showAutoColorPaletteOverlay_ = false;

    return;

  }

  const auto extracted = SmartPaletteAnalyzer::extractPalette(referenceOverlayImage_, 5);

  for (const auto& color : extracted) {

    referenceDominantPalette_.push_back(color);

  }

  if (!referenceDominantPalette_.isEmpty()) {

    referenceHarmonyPalette_ =
        buildReferenceHarmonyPalette(referenceDominantPalette_.front());

  }

  if (showAutoColorPaletteOverlay_ && referenceDominantPalette_.isEmpty()) {

    showAutoColorPaletteOverlay_ = false;

  }

}

void CompositionRenderController::Impl::drawColorSamplerOverlay(int overlayW,
                                                                int overlayH) {

  if (!renderer_ || !showColorSamplerOverlay_ || !colorSamplerHasSample_) {

    return;

  }

  QFont font = QApplication::font();
  font.setPointSizeF(std::max(9.0, static_cast<double>(font.pointSizeF())));
  const QFontMetrics fm(font);

  const QString title =
      QStringLiteral("Sampler %1").arg(colorHexLabel(colorSamplerColor_));

  const QString detail =
      QStringLiteral("%1  •  %2  •  XY %3,%4  •  PX %5,%6")
          .arg(colorRgbLabel(colorSamplerColor_))
          .arg(colorHslLabel(colorSamplerColor_))
          .arg(QString::number(colorSamplerCanvasPos_.x(), 'f', 1))
          .arg(QString::number(colorSamplerCanvasPos_.y(), 'f', 1))
          .arg(colorSamplerImagePixel_.x())
          .arg(colorSamplerImagePixel_.y());

  const int panelW = std::min(overlayW - 24,
                              std::max(260, fm.horizontalAdvance(detail) + 24));
  const int panelH = fm.height() * 2 + 18;
  const int panelX = 12;
  const int panelY = std::max(52, overlayH - panelH - 12);

  renderer_->drawRoundedPanel(static_cast<float>(panelX), static_cast<float>(panelY),
                              static_cast<float>(panelW), static_cast<float>(panelH),
                              8.0f,
                              FloatColor{0.03f, 0.04f, 0.06f, 0.86f},
                              FloatColor{0.82f, 0.86f, 0.90f, 0.92f},
                              1.0f, 1.0f);
  renderer_->drawSolidRect(static_cast<float>(panelX + 10),
                           static_cast<float>(panelY + 10), 24.0f, 24.0f,
                           FloatColor{colorSamplerColor_.redF(),
                                      colorSamplerColor_.greenF(),
                                      colorSamplerColor_.blueF(), 1.0f},
                           1.0f);
  renderer_->drawText(QRectF(panelX + 42.0f, panelY + 7.0f, panelW - 52.0f, fm.height() + 4.0f),
                      title, font, FloatColor{0.94f, 0.97f, 1.0f, 1.0f},
                      Qt::AlignLeft | Qt::AlignTop);
  renderer_->drawText(QRectF(panelX + 42.0f, panelY + 10.0f + fm.height(),
                             panelW - 52.0f, fm.height() + 4.0f),
                      fm.elidedText(detail, Qt::ElideRight, panelW - 52),
                      font, FloatColor{0.72f, 0.77f, 0.82f, 1.0f},
                      Qt::AlignLeft | Qt::AlignTop);

}

void CompositionRenderController::Impl::drawAutoColorPaletteOverlay(int overlayW,
                                                                     int overlayH) {

  if (!renderer_ || !showAutoColorPaletteOverlay_ ||
      referenceDominantPalette_.isEmpty()) {

    return;

  }

  QFont font = QApplication::font();
  font.setPointSizeF(std::max(8.5, static_cast<double>(font.pointSizeF() - 0.5)));
  const QFontMetrics fm(font);
  const int swatchSize = 24;
  const int rowGap = 10;
  const int labelW = 96;
  const int dominantCount = referenceDominantPalette_.size();
  const int harmonyCount = referenceHarmonyPalette_.size();
  const int maxCount = std::max(dominantCount, harmonyCount);
  const int panelW = std::min(overlayW - 24, labelW + maxCount * (swatchSize + 6) + 20);
  const int panelH = 74;
  const int panelX = std::max(12, overlayW - panelW - 12);
  const int panelY = std::max(52, overlayH - panelH - 12);

  renderer_->drawRoundedPanel(static_cast<float>(panelX), static_cast<float>(panelY),
                              static_cast<float>(panelW), static_cast<float>(panelH),
                              8.0f,
                              FloatColor{0.03f, 0.04f, 0.06f, 0.88f},
                              FloatColor{0.35f, 0.62f, 0.92f, 0.94f},
                              1.0f, 1.0f);

  const auto drawPaletteRow = [&](const QString& label,
                                  const QVector<FloatColor>& colors,
                                  int rowIndex) {
    const float rowY = static_cast<float>(panelY + 10 + rowIndex * (swatchSize + rowGap));
    renderer_->drawText(QRectF(panelX + 10.0f, rowY + 2.0f, labelW - 10.0f, fm.height() + 2.0f),
                        label, font, FloatColor{0.90f, 0.94f, 0.97f, 1.0f},
                        Qt::AlignLeft | Qt::AlignTop);
    for (int i = 0; i < colors.size(); ++i) {
      const float swatchX = static_cast<float>(panelX + labelW + i * (swatchSize + 6));
      const QColor swatchColor = qColorFromFloatColor(colors.at(i));
      renderer_->drawSolidRect(
          swatchX, rowY, static_cast<float>(swatchSize), static_cast<float>(swatchSize),
          FloatColor{swatchColor.redF(), swatchColor.greenF(), swatchColor.blueF(), 1.0f},
          1.0f);
    }
  };

  drawPaletteRow(QStringLiteral("Dominant"), referenceDominantPalette_, 0);
  drawPaletteRow(QStringLiteral("Harmony"), referenceHarmonyPalette_, 1);

}



void CompositionRenderController::Impl::drawViewportGuideOverlay(

    const ArtifactCompositionPtr &comp, float cw, float ch) {

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



  if (!showGuides_) {

    return;

  }



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



void CompositionRenderController::Impl::drawViewportCanvasOverlay(float cw,

                                                                  float ch) {

  if (!renderer_ || cw <= 0.0f || ch <= 0.0f) {

    return;

  }



  const float zoom = std::max(renderer_->getZoom(), 0.001f);

  if (showGrid_) {

    const float majorSpacing = std::max(1.0f, gridSettings_.majorInterval);

    const int subdivisions = std::max(1, gridSettings_.subdivisions);

    const float minorSpacing = majorSpacing / static_cast<float>(subdivisions);



    if (gridSettings_.showMinor && subdivisions > 1 &&

        minorSpacing * zoom >= 4.0f) {

      renderer_->drawGrid(0.0f, 0.0f, cw, ch, minorSpacing,

                          gridSettings_.minorStyle.thickness,

                          gridSettings_.minorColor);

    }

    if (gridSettings_.showMajor) {

      renderer_->drawGrid(0.0f, 0.0f, cw, ch, majorSpacing,

                          gridSettings_.majorStyle.thickness,

                          gridSettings_.majorColor);

    }

    if (gridSettings_.showAxis) {

      const auto origin = renderer_->canvasToViewport({0.0f, 0.0f});

      const auto bottomRight = renderer_->canvasToViewport({cw, ch});

      const float axisThickness =

          std::max(1.0f, gridSettings_.axisStyle.thickness);

      renderer_->drawThickLineLocal(

          {origin.x, origin.y}, {bottomRight.x, origin.y}, axisThickness,

          gridSettings_.axisColor);

      renderer_->drawThickLineLocal(

          {origin.x, origin.y}, {origin.x, bottomRight.y}, axisThickness,

          gridSettings_.axisColor);

    }

  }



  if (!showSafeMargins_) {

    return;

  }



  const auto canvasTopLeft = renderer_->canvasToViewport({0.0f, 0.0f});

  const auto canvasBottomRight = renderer_->canvasToViewport({cw, ch});

  const float screenW = std::abs(canvasBottomRight.x - canvasTopLeft.x);

  const float screenH = std::abs(canvasBottomRight.y - canvasTopLeft.y);

  if (screenW <= 0.0f || screenH <= 0.0f) {

    return;

  }



  const FloatColor outlineColor = {0.0f, 0.0f, 0.0f, 0.72f};

  const FloatColor innerColor = {0.95f, 0.97f, 1.0f, 0.94f};

  const auto snapScreen = [](float value) {

    return std::round(value) + 0.5f;

  };

  const auto drawSafeRect = [&](float ratio) {

    const float insetX = screenW * (1.0f - ratio) * 0.5f;

    const float insetY = screenH * (1.0f - ratio) * 0.5f;

    const float x = snapScreen(std::min(canvasTopLeft.x, canvasBottomRight.x) +

                               insetX);

    const float y = snapScreen(std::min(canvasTopLeft.y, canvasBottomRight.y) +

                               insetY);

    const float w = std::max(0.0f, screenW - insetX * 2.0f);

    const float h = std::max(0.0f, screenH - insetY * 2.0f);

    if (w <= 2.0f || h <= 2.0f) {

      return;

    }

    renderer_->drawRectOutlineLocal(x, y, w, h, outlineColor);

    renderer_->drawRectOutlineLocal(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f,

                                    innerColor);

  };



  drawSafeRect(0.9f);

  drawSafeRect(0.8f);



  const float centerX =

      snapScreen((canvasTopLeft.x + canvasBottomRight.x) * 0.5f);

  const float centerY =

      snapScreen((canvasTopLeft.y + canvasBottomRight.y) * 0.5f);

  const float crossSize =

      std::clamp(std::min(screenW, screenH) * 0.05f, 12.0f, 72.0f);

  renderer_->drawCrosshair(centerX, centerY, crossSize, innerColor);

}



void CompositionRenderController::Impl::drawViewportInteractionOverlay(

    CompositionRenderController *owner, const FramePosition &currentFrame,

    float cw, float ch) {

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

                            {0.25f, 0.70f, 1.0f, 0.95f}, showSelectionRect);

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



}



void CompositionRenderController::Impl::drawSelectionEditingOverlay(

    CompositionRenderController *owner, const ArtifactCompositionPtr &comp,

    const std::vector<ArtifactAbstractLayerPtr> &layers,

    const QStringList &selectedIds, const FramePosition &currentFrame, float cw,

    float ch, bool has3DCamera, const QMatrix4x4 &cameraViewMatrix,

    const QMatrix4x4 &cameraProjMatrix) {

  if (renderer_) {

    renderer_->setUseExternalMatrices(false);

    renderer_->resetGizmoCameraMatrices();

    renderer_->reset3DCameraMatrices();

    renderer_->setCanvasSize(cw, ch);

  }



  if (showGizmoOverlay_ && gizmo_) {

    ArtifactCore::ProfileScope _profGizmo("GizmoMask",

                                          ArtifactCore::ProfileCategory::Render);

    auto selectedLayer = (!selectedLayerId_.isNil() && comp)

                             ? comp->layerById(selectedLayerId_)

                             : ArtifactAbstractLayerPtr{};

    if (selectedLayer && isLayerEffectivelyVisible(selectedLayer)) {

      gizmo_->setMode(gizmoMode_);

      if (!selectedLayer->is3D() && !viewportOrientationActive_) {

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



      if (gizmo3D_ &&
          (selectedLayer->is3D() || viewportOrientationActive_)) {

        ArtifactCore::ProfileScope _profG3D(

            "Gizmo3D", ArtifactCore::ProfileCategory::Render);

        syncGizmo3DFromLayer(selectedLayer);

        const float viewportW = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;

        const float viewportH =

            hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;

        if (!gizmo3DCameraMatricesValid_ && viewportW > 0.0f && viewportH > 0.0f) {

          float panX = 0.0f;

          float panY = 0.0f;

          float zoom = 1.0f;

          renderer_->getPan(panX, panY);

          zoom = std::max(0.001f, renderer_->getZoom());



          QMatrix4x4 view;
          QMatrix4x4 proj;
          if (viewportOrientationMatricesValid_) {
            view = viewportOrientationViewForOverlay_;
            proj = viewportOrientationProjectionForOverlay_;
          } else if (has3DCamera) {
            // Use the same scene camera as the selected 3D layer. A 2D
            // pan/zoom fallback makes the axes detach after Z transforms.
            view = cameraViewMatrix;
            proj = cameraProjMatrix;
          } else {
            view.translate(panX, panY, 0.0f);
            view.scale(zoom, zoom, 1.0f);
            proj.ortho(0.0f, viewportW, viewportH, 0.0f, -1000.0f,
                       1000.0f);
          }



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

            gizmo3D_->draw(
                renderer_.get(),
                gizmo3DCameraMatricesValid_ ? gizmo3DViewMatrix_
                                             : renderer_->getViewMatrix(),
                gizmo3DCameraMatricesValid_ ? gizmo3DProjectionMatrix_
                                             : renderer_->getProjectionMatrix());

          }

          {

            ArtifactCore::ProfileScope _profG3DFlush(

                "Gizmo3DFlush", ArtifactCore::ProfileCategory::Render);

            renderer_->flushGizmo3D();

          }

        }

      }



      {

        const auto *tm = ArtifactApplicationManager::instance()

                             ? ArtifactApplicationManager::instance()->toolManager()

                             : nullptr;

        if (trackerGizmo_ && tm && tm->activeTool() == ToolType::TrackPoint) {

          ArtifactCore::ProfileScope _profTracker(

              "TrackerGizmo", ArtifactCore::ProfileCategory::Render);

          trackerGizmo_->draw(renderer_.get());

        }

      }

    }

  }



  if (renderer_ && workCursorVisible_ && workCursorState_.spatial) {
    const Detail::float3 center{workCursorState_.x, workCursorState_.y,
                                workCursorState_.z};
    const float cursorSize = 18.0f;
    const FloatColor shadow{0.01f, 0.02f, 0.03f, 0.88f};
    const FloatColor accent{1.0f, 0.34f, 0.12f, 1.0f};
    const FloatColor white{0.96f, 0.98f, 1.0f, 0.96f};
    const auto line = [this, &shadow, &white](const Detail::float3 &a,
                                              const Detail::float3 &b) {
      renderer_->drawGizmoLine(a, b, shadow, 3.0f);
      renderer_->drawGizmoLine(a, b, white, 1.4f);
    };
    const QMatrix4x4 &cursorView = viewportOrientationMatricesValid_
        ? viewportOrientationViewForOverlay_
        : renderer_->getViewMatrix();
    const QMatrix4x4 &cursorProjection = viewportOrientationMatricesValid_
        ? viewportOrientationProjectionForOverlay_
        : renderer_->getProjectionMatrix();
    renderer_->set3DCameraMatrices(cursorView, cursorProjection);
    renderer_->drawGizmoRing(center, {1.0f, 0.0f, 0.0f}, cursorSize,
                             accent, 1.5f);
    renderer_->drawGizmoRing(center, {0.0f, 1.0f, 0.0f}, cursorSize,
                             accent, 1.5f);
    renderer_->drawGizmoRing(center, {0.0f, 0.0f, 1.0f}, cursorSize,
                             accent, 1.5f);
    line({center.x - cursorSize * 1.35f, center.y, center.z},
         {center.x + cursorSize * 1.35f, center.y, center.z});
    line({center.x, center.y - cursorSize * 1.35f, center.z},
         {center.x, center.y + cursorSize * 1.35f, center.z});
    line({center.x, center.y, center.z - cursorSize * 1.35f},
         {center.x, center.y, center.z + cursorSize * 1.35f});
    renderer_->flushGizmo3D();
    renderer_->reset3DCameraMatrices();
  }

  if (renderer_ && renderer_->getZoom() >= 8.0f) {

    const float zoom = renderer_->getZoom();

    const float alpha = std::clamp((zoom - 8.0f) / 8.0f, 0.0f, 0.5f) * 0.4f;

    if (alpha > 0.0f && comp) {

      const auto compSize = comp->settings().compositionSize();

      const float compW =

          static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);

      const float compH =

          static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);

      renderer_->drawGrid(0.0f, 0.0f, compW, compH, 1.0f, 1.0f / zoom,

                          {0.6f, 0.6f, 0.6f, alpha});

    }

  }



  if (renderer_ &&
      (!selectedIds.isEmpty() || !selectedLayerId_.isNil())) {

    const auto &layersForOverlay = layers;

    if (comp && showMotionPathOverlay_) {

      ArtifactCore::ProfileScope _profMotion("MotionPath",

                                             ArtifactCore::ProfileCategory::Render);

      QVector<LayerID> selectedLayerIds;
      selectedLayerIds.reserve(selectedIds.size());
      for (const auto &selectedId : selectedIds) {
        selectedLayerIds.emplace_back(selectedId);
      }

      const float zoom = renderer_->getZoom();

      const float invZoom = zoom > 0.0001f ? 1.0f / zoom : 1.0f;

      for (const auto &layer : layersForOverlay) {

        if (!layer) {
          continue;
        }

        renderMotionPathOverlayForLayer(
            layer, comp, currentFrame.framePosition(), zoom, invZoom,
            selectedLayerId_, selectedLayerIds, !selectedLayerIds.isEmpty(),
            overlayInvalidationSerial_, true);

      }

    }



    {

      ArtifactCore::ProfileScope _profBBox(

          "BoundingBox", ArtifactCore::ProfileCategory::Render);

      // The projected 3D frame belongs to the transform gizmo. Guide
      // visibility must not suppress the selected layer's controls.
      if (showGizmoOverlay_) {

        const FloatColor secondaryColor{0.28f, 0.74f, 1.0f, 0.85f};

        for (const auto &layer : layers) {

          if (!isLayerEffectivelyVisible(layer) ||

              !layer->isActiveAt(currentFrame) ||

              dynamic_cast<ArtifactVideoLayer *>(layer.get()) ||

              !isLayerSelected(selectedIds, layer)) {

            continue;

          }

          const QRectF localBounds = layer->localBounds();

          if (!localBounds.isValid() || localBounds.width() <= 0.0 ||

              localBounds.height() <= 0.0) {

            continue;

          }

          const bool primary =

              !selectedLayerId_.isNil() && layer->id() == selectedLayerId_;

          // The normal 2D transform gizmo already owns the primary frame in
          // canvas view, so avoid drawing it twice. A 3D layer always needs
          // this camera-projected frame alongside its axis gizmo.
          if (primary && !layer->is3D() &&
              !viewportOrientationMatricesValid_) {
            continue;
          }

          const QMatrix4x4 frameView = viewportOrientationMatricesValid_
              ? viewportOrientationViewForOverlay_
              : has3DCamera ? cameraViewMatrix
                            : renderer_->getViewMatrix();
          const QMatrix4x4 frameProjection = viewportOrientationMatricesValid_
              ? viewportOrientationProjectionForOverlay_
              : has3DCamera ? cameraProjMatrix
                            : renderer_->getProjectionMatrix();
          const FloatColor frameColor = primary
              ? FloatColor{0.24f, 0.68f, 1.0f, 0.98f}
              : secondaryColor;
          const float frameThickness = primary ? 2.2f : 1.6f;
          ::Artifact::drawSelectionFrameOverlay(
              renderer_.get(), layer, frameColor, frameThickness, &frameView,
              &frameProjection);

        }

      }

    }

  }

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

    if (workCursorVisible_) {

      statusParts << QStringLiteral("Cursor");

    }

    const QString statusText = statusParts.join(QStringLiteral("  •  "));

    ::Artifact::drawViewportStatusChipOverlay(renderer_.get(), overlayW,

                                              overlayH, statusText,

                                              &restoreCanvasSize);

  }

  drawColorSamplerOverlay(overlayW, overlayH);

  drawAutoColorPaletteOverlay(overlayW, overlayH);



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
