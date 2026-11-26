module;

#include <DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>

#define NOMINMAX

#define QT_NO_KEYWORDS



#include <QApplication>
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>

#include <QByteArray>

#include <QColor>
#include <QDebug>

#include <QElapsedTimer>

#include <QFont>
#include <QFontDatabase>

#include <QFontMetrics>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>

#include <QFuture>

#include <QHash>

#include <QImage>

#include <QList>

#include <QLoggingCategory>

#include <QLineF>

#include <QMatrix4x4>

#include <QMetaObject>

#include <QMutex>

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <QPointer>
#include <QPointF>

#include <QRect>

#include <QRectF>
#include <QInputDialog>

#include <QSizeF>

#include <QSet>
#include <QSettings>

#include <QStringList>
#include <QString>
#include <QTextDocument>
#include <QUuid>

#include <QTimer>

#include <QTransform>
#include <QWidget>
#include <QScreen>
#include <QtGlobal>

#include <QVector3D>
#include <QVector2D>

#include <QVector4D>

#include <QVector>
#include <deque>
#include <cstdio>


#include <algorithm>

#include <atomic>

#include <array>

#include <chrono>

#include <cmath>

#include <cstddef>

#include <cstdint>

#include <functional>

#include <limits>

#include <memory>

#include <string_view>



#include <utility>

#include <vector>





module Artifact.Widgets.CompositionRenderController;

import Memory.SharedPtr;
import Core.ArtifactArray;
import Core.ArtifactTuple;
import Artifact.Layer.Construction;
import Artifact.Widgets.CompositionConstructionUndoCommand;

import Artifact.Render.IRenderer;
import Graphics.RayTracingManager;
import Graphics.SurfaceColorContract;
import Material.Material;
import Artifact.Widgets.ViewportOverlay;

import Artifact.Grid.System;
import ArtifactCore.Rig2D;

import Artifact.Render.GPUTextureCacheManager;

import Artifact.Render.CompositionViewDrawing;
import Artifact.Render.CompositionMatteSurface;
import Artifact.Render.CompositionRenderer;

import Artifact.Render.Queue.Service;

import Artifact.Render.Config;

import Settings.Accessibility;

import Artifact.Render.ROI;

import Artifact.Render.Context;
import Analyze.SmartPalette;
import Color.Harmonizer;

import Artifact.Widgets.CompositionRenderOverlay;
import Artifact.Layer.Audio;
import Audio.Effect.Spectrum;

import Artifact.Preview.Pipeline;

import Core.Camera;

import Frame.Debug;

import Graphics.RenderGraph;

import Core.Diagnostics.Trace;

import Artifact.Composition.Abstract;

import Artifact.Layer.Abstract;
import Artifact.Layer.Particle;
import Artifact.Layer.MpmSand2D;
import Artifact.Layer.EnvironmentMap;
import Artifact.Layers.Abstract._2D;

import Artifact.Layer.Clone;

import Artifact.Layer.CloneEffectSupport;

import Artifact.Layer.Camera;

import Artifact.Layer.Light;

import Core.Light;

import Artifact.Effect.Abstract;

import Artifact.Effect.Context;
import Artifact.Effect.FrameSampler;


import Artifact.Layer.Image;

import Artifact.Layer.Svg;

import Artifact.Layer.Video;

import Video.VideoFrame;

import Artifact.Layer.Solid2D;

import Artifact.Layers.SolidImage;

import Artifact.Layer.Text;

import Artifact.Layer.Composition;
import Artifact.Layer.ParametricComposition;

import Artifact.Layer.Shape;

import Layer.Matte;

import Artifact.Layers.Model3D;

import Artifact.Render.Offscreen;

import Image.ImageF32x4_RGBA;
import Image.Cryptomatte.Pixel;

import FloatRGBA;

import Frame.Position;

import Artifact.Application.Manager;

import Artifact.Layers.Selection.Manager;

import Artifact.Service.ActiveContext;

import Artifact.Widgets.TransformGizmo;

import Artifact.Widgets.TextGizmo;

import Artifact.Widgets.ContentGizmo;

import Artifact.Widgets.Gizmo3D;

import Artifact.Widgets.ViewportMath;

import Artifact.Widgets.PointTrackerGizmo;

import Artifact.Tool.PointTracker;

import Tracking.MotionTracker;

import Artifact.Render.OffscreenComposition;
import Artifact.Render.PointwiseEffectFusion;
import Artifact.Widgets.CompositionHostFrameSnapshot;

import ExposureEffect;
import HueAndSaturation;
import LevelsEffect;
import BrightnessEffect;
import InvertEffect;
import GrayscaleEffect;
import Artifact.Effect.WhiteBalance;

import Artifact.Widgets.PieMenu;

import UI.View.Orientation.Navigator;

import Geometry.CameraGuide;

import Math.Interpolate;

import Artifact.Tool.Manager;

import Artifact.Tool.MotionSketchTool;

import Artifact.Tool.PuppetTool;

import Artifact.Tool.Brush;

import Artifact.Layer.Paint;

import Artifact.Mask.LayerMask;

import Artifact.Mask.Path;

import Artifact.Project.Health;

import Utils.Id;

import Time.Rational;

import Artifact.Render.Pipeline;
import :ChannelPresentation;
import :Impl;
import :GizmoEditing;
import :LayerDrawing;
import :MaskEditing;
import :RasterEffects;
import :ViewportGeometry;
import Artifact.Render.MotionBlurPass;
import Artifact.Render.DepthOfFieldPass;
import Artifact.Render.FinalPostProcess;
import Artifact.Color.OCIOManager;
import Artifact.Widgets.ViewportColorPipeline;

import Graphics.LayerBlendPipeline;
import Container.NamedVector;

import Graphics.GPUcomputeContext;

import Graphics.Shader.Compute.MaskCutout;

import Widgets.Utils.CSS;




import Artifact.Service.Project;
import Artifact.Project.Items;
import Artifact.Layer.InitParams;

import Artifact.Service.Playback; // 追加

import Application.AppSettings;

import Playback.State;

import Thread.Helper;

import Thread.PreciseTicker;

import Event.Bus;

import Artifact.Event.Types;

import Undo.UndoManager;
import Utils.String.UniString;
import Configuration.LayeredConfigStore;
import Configuration.ConfigLayer;

import Frame.Rate;
import Math.Vec;

import Color.Float;

import Image;

import ArtifactCore.ImageProcessing.OpenCV.RotoBrushEngine;

import ArtifactCore.Utils.PerformanceProfiler;
import Artifact.Render.CompositionChangeDetector;
import Artifact.Render.CompositionPassState;
import Artifact.Render.CompositionRenderPassPlan;
import Artifact.Render.CompositionRenderPassContext;
import Artifact.Render.CompositionPassExecutor;
import Artifact.Widgets.CompositionMotionPathCommands;
import Artifact.Widgets.CompositionTrackerSessionUndoCommands;
import Artifact.Widgets.CompositionOnionSkinHistory;
import Artifact.Widgets.CompositionRigUndoCommands;
import Artifact.Widgets.CompositionTextPuppetUndoCommands;
import Artifact.Widgets.CompositionLayerUndoCommands;
import Artifact.Widgets.CompositionEditUndoCommands;
import Artifact.Widgets.CompositionGizmoUndoCommands;
import Artifact.Widgets.LayerEditor.Geometry;



namespace Artifact {

using namespace CompositionRenderControllerDetail;

namespace CompositionRenderControllerDetail {

static QString colorHexLabel(const QColor& color) {
  return color.name(QColor::HexRgb).toUpper();
}

static QString colorRgbLabel(const QColor& color) {
  return QStringLiteral("RGB %1,%2,%3")
      .arg(color.red())
      .arg(color.green())
      .arg(color.blue());
}

static QString colorHslLabel(const QColor& color) {
  const int hue = color.hslHue() >= 0 ? color.hslHue() : 0;
  return QStringLiteral("HSL %1,%2,%3")
      .arg(hue)
      .arg(color.hslSaturation())
      .arg(color.lightness());
}

static QColor qColorFromFloatColor(const FloatColor& color) {
  return QColor::fromRgbF(std::clamp(color.r(), 0.0f, 1.0f),
                          std::clamp(color.g(), 0.0f, 1.0f),
                          std::clamp(color.b(), 0.0f, 1.0f),
                          std::clamp(color.a(), 0.0f, 1.0f));
}

static QVector<FloatColor> buildReferenceHarmonyPalette(const FloatColor& baseColor) {
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

static void drawClonerFrameOverlay(ArtifactIRenderer* renderer,

                            const ArtifactAbstractLayerPtr& layer)

{

  if (!renderer || !layer) {

    return;

  }



  const auto cloneLayer = ArtifactCore::dynamicPointerCast<ArtifactCloneLayer>(layer);

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

static void drawEffectHitboxOverlay(ArtifactIRenderer *renderer,

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

      const FloatColor effectiveMaskColor = mask.isLocked()

          ? FloatColor{0.72f, 0.72f, 0.76f, 0.82f}

          : mask.color();
      drawRectOutline(renderer, maskBounds, effectiveMaskColor, 1.4f);

    }

  }



  const auto matteRefs = selectedLayer->matteReferences();

  for (const auto &matteRef : matteRefs) {

    if (!matteRef.enabled || matteRef.sourceLayerId.isNil() ||
        matteRef.sourceLayerId == selectedLayer->id()) {

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

static bool layerHasOnionSkinAnimation(const ArtifactAbstractLayerPtr &layer) {
  if (!layer) {
    return false;
  }

  const auto &transform = layer->transform3D();
  if (transform.getPositionKeyFrameCount() > 1 ||
      transform.getRotationKeyFrameCount() > 1 ||
      transform.getScaleKeyFrameCount() > 1 || layer->hasSourceTimeMapping()) {
    return true;
  }

  for (const auto &group : layer->getLayerPropertyGroups()) {
    for (const auto &property : group.allProperties()) {
      if (property && property->keyFrameCount() > 1) {
        return true;
      }
    }
  }

  if (const auto *textLayer = dynamic_cast<const ArtifactTextLayer *>(layer.get());
      textLayer && textLayer->hasSourceTextKeyframes()) {
    return true;
  }

  // These layer types can change from frame to frame without property
  // keyframes, so their temporal content is also valid onion-skin input.
  return dynamic_cast<const ArtifactVideoLayer *>(layer.get()) ||
         dynamic_cast<const ArtifactCompositionLayer *>(layer.get()) ||
         layer->isParticleLayer();
}

static bool compositionHasOnionSkinAnimation(const ArtifactCompositionPtr &composition,
                                      const FramePosition &currentFrame) {
  if (!composition) {
    return false;
  }
  const auto &layers = composition->allLayerRef();
  return std::any_of(
      layers.cbegin(), layers.cend(),
      [&currentFrame](const ArtifactAbstractLayerPtr &layer) {
        return isLayerEffectivelyVisible(layer) &&
               layer->isActiveAt(currentFrame) &&
               layerHasOnionSkinAnimation(layer);
      });
}

} // namespace CompositionRenderControllerDetail

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

  float x = contextMenuViewportPos_.x;

  float y = contextMenuViewportPos_.y;

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

  const float x = std::min(std::max(pieMenuViewportPos_.x - size * 0.5f, minX), maxX);

  const float y = std::min(std::max(pieMenuViewportPos_.y - size * 0.5f, minY), maxY);

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



int CompositionRenderController::Impl::pieMenuItemAt(
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPos) const {

  if (!pieMenuVisible_ || pieMenuModel_.items.empty()) {

    return -1;

  }

  const QRectF rect = pieMenuRect();

  const QPointF center = rect.center();

  const QPointF delta(viewportPos.x - center.x(), viewportPos.y - center.y());

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
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPos) const {

  const int count = commandPaletteVisible_

                        ? std::min(8, static_cast<int>(commandPaletteItems_.size()))

                        : (contextMenuVisible_

                               ? static_cast<int>(contextMenuItems_.size())

                               : 0);

  for (int i = 0; i < count; ++i) {

    if (viewportOverlayItemRect(i).contains(viewportPos.x, viewportPos.y)) {

      return i;

    }

  }

  return -1;

}



void drawParticle2DControlOverlay(ArtifactIRenderer* renderer,
                                  ArtifactParticleLayer* layer,
                                  float zoom)
{
  if (!renderer || !layer || layer->is3D()) return;
  const QTransform transform = layer->getGlobalTransform();
  const QPointF emitter = transform.map(QPointF(layer->emitterPosition().x(),
                                                 layer->emitterPosition().y()));
  const float inverseZoom = 1.0f / std::max(0.001f, zoom);
  const FloatColor emitterColor{0.20f, 0.92f, 1.0f, 0.95f};
  const float handleRadius = std::max(4.0f, 7.0f * inverseZoom);
  renderer->drawCircle(static_cast<float>(emitter.x()),
                       static_cast<float>(emitter.y()), handleRadius,
                       emitterColor, 1.5f, true);
  renderer->drawSolidLine(
      {static_cast<float>(emitter.x() - 10.0f * inverseZoom),
       static_cast<float>(emitter.y())},
      {static_cast<float>(emitter.x() + 10.0f * inverseZoom),
       static_cast<float>(emitter.y())},
      emitterColor, std::max(1.0f, inverseZoom));
  renderer->drawSolidLine(
      {static_cast<float>(emitter.x()),
       static_cast<float>(emitter.y() - 10.0f * inverseZoom)},
      {static_cast<float>(emitter.x()),
       static_cast<float>(emitter.y() + 10.0f * inverseZoom)},
      emitterColor, std::max(1.0f, inverseZoom));
  const QVector3D direction = layer->emitterDirection().normalized();
  if (direction.lengthSquared() > 0.0001f) {
    const QPointF directionPoint = transform.map(
        QPointF(layer->emitterPosition().x() + direction.x() * 72.0f,
                layer->emitterPosition().y() + direction.y() * 72.0f));
    renderer->drawSolidLine(
        {static_cast<float>(emitter.x()), static_cast<float>(emitter.y())},
        {static_cast<float>(directionPoint.x()), static_cast<float>(directionPoint.y())},
        FloatColor{0.25f, 1.0f, 0.66f, 0.78f},
        std::max(1.0f, 1.4f * inverseZoom));
    renderer->drawCircle(static_cast<float>(directionPoint.x()),
                         static_cast<float>(directionPoint.y()),
                         std::max(3.0f, 5.0f * inverseZoom),
                         FloatColor{0.25f, 1.0f, 0.66f, 0.92f}, 1.0f, true);
  }

  const auto* particle = layer->particleSystem();
  const auto* emitterObject = particle && !particle->emitters().empty()
      ? particle->emitters().front().get() : nullptr;
  if (!emitterObject) return;
  const auto& effectors = emitterObject->effectors();
  for (int i = 0; i < static_cast<int>(effectors.size()); ++i) {
    const auto& effector = effectors[static_cast<size_t>(i)];
    if (!effector || !effector->enabled) continue;
    const QPointF point = transform.map(QPointF(effector->position.x(),
                                                 effector->position.y()));
    const bool selected = i == layer->selectedEffectorIndex();
    const FloatColor color = selected
        ? FloatColor{1.0f, 0.72f, 0.22f, 0.98f}
        : FloatColor{0.95f, 0.45f, 0.22f, 0.86f};
    renderer->drawCircle(static_cast<float>(point.x()),
                         static_cast<float>(point.y()),
                         std::max(3.0f, (selected ? 6.0f : 4.5f) * inverseZoom),
                         color, 1.2f, true);
    QVector3D effectorDirection;
    float directionStrength = 0.0f;
    if (const auto* force = dynamic_cast<const ForceEffector*>(effector.get())) {
      effectorDirection = force->force;
      directionStrength = force->force.length();
    } else if (const auto* wind = dynamic_cast<const WindEffector*>(effector.get())) {
      effectorDirection = wind->windDirection;
      directionStrength = wind->windStrength;
    }
    if (effectorDirection.lengthSquared() > 0.0001f) {
      const QVector3D direction = effectorDirection.normalized();
      const float length = std::clamp(28.0f + directionStrength * 0.12f,
                                      28.0f, 96.0f);
      const QPointF arrowPoint = transform.map(
          QPointF(effector->position.x() + direction.x() * length,
                  effector->position.y() + direction.y() * length));
      renderer->drawSolidLine(
          {static_cast<float>(point.x()), static_cast<float>(point.y())},
          {static_cast<float>(arrowPoint.x()), static_cast<float>(arrowPoint.y())},
          color, std::max(1.0f, 1.3f * inverseZoom));
      renderer->drawCircle(static_cast<float>(arrowPoint.x()),
                           static_cast<float>(arrowPoint.y()),
                           std::max(2.5f, 4.0f * inverseZoom),
                           color, 1.0f, true);
    }
    float influenceRadius = 0.0f;
    if (const auto* vortex = dynamic_cast<const VortexEffector*>(effector.get())) {
      influenceRadius = vortex->radius;
    } else if (const auto* attractor = dynamic_cast<const AttractorEffector*>(effector.get())) {
      influenceRadius = attractor->radius;
    } else if (const auto* repeller = dynamic_cast<const RepellerEffector*>(effector.get())) {
      influenceRadius = repeller->radius;
    }
    if (std::isfinite(influenceRadius) && influenceRadius > 0.0f) {
      const QPointF radiusPoint = transform.map(
          QPointF(effector->position.x() + influenceRadius,
                  effector->position.y()));
      const float radiusHandleRadius = std::max(
          3.0f, (selected ? 6.0f : 4.0f) * inverseZoom);
      renderer->drawCircle(static_cast<float>(point.x()),
                           static_cast<float>(point.y()),
                           influenceRadius * std::max(0.001f,
                                                      static_cast<float>(layer->getGlobalTransform().m11())),
                           FloatColor{color.r(), color.g(), color.b(), selected ? 0.34f : 0.18f},
                           std::max(1.0f, static_cast<float>(inverseZoom)), false);
      renderer->drawSolidLine(
          {static_cast<float>(point.x()), static_cast<float>(point.y())},
          {static_cast<float>(radiusPoint.x()), static_cast<float>(radiusPoint.y())},
          FloatColor{color.r(), color.g(), color.b(), selected ? 0.55f : 0.30f},
          std::max(1.0f, inverseZoom));
      renderer->drawCircle(static_cast<float>(radiusPoint.x()),
                           static_cast<float>(radiusPoint.y()),
                           radiusHandleRadius, color, 1.2f, true);
    }
    renderer->drawSolidLine(
        {static_cast<float>(emitter.x()), static_cast<float>(emitter.y())},
        {static_cast<float>(point.x()), static_cast<float>(point.y())},
        FloatColor{color.r(), color.g(), color.b(), 0.28f},
        std::max(1.0f, inverseZoom));
  }
}

// P0-3d: re-render only the IRR rectangle at the requested quality. The
// scissor is already active for the frame so subsequent drawSprite() /
// clear() calls naturally stay within the region. The function
// intentionally does not own a full re-render path yet; it triggers a
// composition re-submit via the existing damage-tracking path so the
// renderer fills the IRR with fresh contents at the new quality. A
// future P0-3d.1 milestone will replace this with a true partial-render
// pipeline driven by the ProgressiveRenderer callback.
void CompositionRenderController::Impl::renderPartialRegion(
    CompositionRenderController *owner, RenderQuality quality,
    const RenderROI &roi) {
  if (!owner || !renderer_) {
    return;
  }
  // RenderQuality is recorded for diagnostics. A future progressive
  // upgrade will translate this into a downsampling factor passed to
  // the pipeline.
  lastPartialRenderQuality_ = quality;
  ++partialRenderCount_;
  // The IRR scissor applied earlier already restricts writes to the
  // rectangle. We trigger a fresh damage pass so the cache layer below
  // the pipeline re-emits the contents inside the rectangle.
  if (!roi.isEmpty()) {
    damageTracker_.markFullRedraw();
    invalidateBaseComposite();
    owner->markRenderDirty();
  }
  (void)owner;
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

    const HostFrameSnapshot &hostSnapshot, const ArtifactCompositionPtr &comp,

    ArtifactCameraLayer *activeCamera,

    const QList<ArtifactAbstractLayerPtr> &layers,

    const QStringList &selectedIds, const FramePosition &currentFrame,

    float cw, float ch, bool has3DCamera,

    const QMatrix4x4 &cameraViewMatrix,

    const QMatrix4x4 &cameraProjMatrix) {

  ArtifactCore::ProfileScope _profOverlay("Overlay",

                                          ArtifactCore::ProfileCategory::UI);

  const ArtifactAbstractLayerPtr selectedLayer =

      (!selectedLayerId_.isNil() && comp) ? comp->layerById(selectedLayerId_)

                                          : ArtifactAbstractLayerPtr{};

  if (showGizmoOverlay_ && renderer_) {
    const QMatrix4x4 view = viewportOrientationMatricesValid_
        ? viewportOrientationViewForOverlay_
        : has3DCamera ? cameraViewMatrix : renderer_->getViewMatrix();
    const QMatrix4x4 projection = viewportOrientationMatricesValid_
        ? viewportOrientationProjectionForOverlay_
        : has3DCamera ? cameraProjMatrix : renderer_->getProjectionMatrix();
    renderer_->set3DCameraMatrices(view, projection);
    for (const auto& layer : layers) {
      auto* light = layer ? dynamic_cast<ArtifactLightLayer*>(layer.get()) : nullptr;
      if (!light || !isLayerEffectivelyVisible(layer) ||
          !layer->isActiveAt(currentFrame) ||
          !passesLayerRenderFilter(layerRenderFilter_, selectedIds, selectedLayerId_, layer) ||
          !passesLayerCategoryMask(viewportLayerCategoryMask_, layer,
                                  CompositionViewportLayerCategory::Light3D)) continue;
      light->draw(renderer_.get());
    }
    renderer_->flushGizmo3D();
    renderer_->reset3DCameraMatrices();
  }

  // P1-4 / S3: Bounding Box shading mode draws the existing 3D bounds
  // overlay for every visible 3D layer. Viewport-only presentation: layer
  // render modes, serialization and Render Queue output stay untouched.
  if (viewportShadingMode_ == CompositionViewportShadingMode::BoundingBox &&
      renderer_) {
    const QMatrix4x4 boundsView = viewportOrientationMatricesValid_
        ? viewportOrientationViewForOverlay_
        : has3DCamera ? cameraViewMatrix : renderer_->getViewMatrix();
    const QMatrix4x4 boundsProjection = viewportOrientationMatricesValid_
        ? viewportOrientationProjectionForOverlay_
        : has3DCamera ? cameraProjMatrix : renderer_->getProjectionMatrix();
    for (const auto &layer : layers) {
      if (!layer || !isLayerEffectivelyVisible(layer) ||
          !layer->isActiveAt(currentFrame) ||
          !passesLayerRenderFilter(layerRenderFilter_, selectedIds,
                                   selectedLayerId_, layer) ||
          !passesLayerCategoryMask(viewportLayerCategoryMask_, layer,
                                   CompositionViewportLayerCategory::Model3D)) {
        continue;
      }
      if (!dynamic_cast<Artifact3DLayer *>(layer.get())) {
        continue;
      }
      ::Artifact::draw3DSelectionBoundsOverlay(
          renderer_.get(), layer, &boundsView, &boundsProjection);
    }
  }
  if (showAudioWaveformOverlay_ || showAudioSpectrumOverlay_) {
    if (auto *audioLayer = selectedLayer
                                ? dynamic_cast<ArtifactAudioLayer *>(selectedLayer.get())
                                : nullptr) {
      const QString sourcePath = audioLayer->sourcePath();
      const QFileInfo sourceInfo(sourcePath);
      const qint64 fileSize = sourceInfo.exists() ? sourceInfo.size() : -1;
      const qint64 modifiedMs = sourceInfo.exists()
                                    ? sourceInfo.lastModified().toMSecsSinceEpoch()
                                    : -1;
      if (showAudioWaveformOverlay_) {
        if (sourcePath != audioWaveformCacheSourcePath_ ||
            fileSize != audioWaveformCacheFileSize_ ||
            modifiedMs != audioWaveformCacheModifiedMs_) {
          const auto waveform = audioLayer->buildWaveformData(256);
          audioWaveformCachePeaks_.assign(waveform.peaks.cbegin(), waveform.peaks.cend());
          audioWaveformCacheRms_.assign(waveform.rms.cbegin(), waveform.rms.cend());
          audioWaveformCacheSourcePath_ = sourcePath;
          audioWaveformCacheFileSize_ = fileSize;
          audioWaveformCacheModifiedMs_ = modifiedMs;
        }
        drawAudioWaveformOverlay(renderer_.get(), audioWaveformCachePeaks_,
                                 audioWaveformCacheRms_, cw, ch);
      }
      if (showAudioSpectrumOverlay_ &&
          (sourcePath != audioSpectrumCacheSourcePath_ ||
           fileSize != audioSpectrumCacheFileSize_ ||
           modifiedMs != audioSpectrumCacheModifiedMs_ ||
           currentFrame.framePosition() != audioSpectrumCacheFrame_)) {
        ArtifactCore::AudioSegment segment;
        if (audioLayer->getAudio(segment, currentFrame, 1024, audioLayer->sampleRate())) {
          ArtifactCore::AudioSpectrum analyzer;
          analyzer.setBins(48);
          analyzer.process(segment);
          audioSpectrumCache_ = analyzer.getSpectrum();
        } else {
          audioSpectrumCache_.clear();
        }
        audioSpectrumCacheSourcePath_ = sourcePath;
        audioSpectrumCacheFileSize_ = fileSize;
        audioSpectrumCacheModifiedMs_ = modifiedMs;
        audioSpectrumCacheFrame_ = currentFrame.framePosition();
      }
      if (showAudioSpectrumOverlay_) {
        drawAudioSpectrumOverlay(renderer_.get(), audioSpectrumCache_, cw, ch);
      }
    }
  } else {
    audioWaveformCacheSourcePath_.clear();
    audioWaveformCachePeaks_.clear();
    audioWaveformCacheRms_.clear();
    audioWaveformCacheFileSize_ = -1;
    audioWaveformCacheModifiedMs_ = -1;
    audioSpectrumCacheSourcePath_.clear();
    audioSpectrumCacheFileSize_ = -1;
    audioSpectrumCacheModifiedMs_ = -1;
    audioSpectrumCacheFrame_ = -1;
    audioSpectrumCache_.clear();
  }

  renderer_->setUseExternalMatrices(false);

  renderer_->resetGizmoCameraMatrices();

  renderer_->reset3DCameraMatrices();

  renderer_->setCanvasSize(cw, ch);

  drawViewportChannelOverlayImage(cw, ch);

  drawReferenceOverlayImage(cw, ch);

  drawOnionSkinOverlay(comp, currentFrame, cw, ch);

  if (auto* particleLayer = selectedLayer
          ? dynamic_cast<ArtifactParticleLayer*>(selectedLayer.get())
          : nullptr) {
    drawParticle2DControlOverlay(renderer_.get(), particleLayer,
                                  renderer_->getZoom());
  }

  if (viewportOrientationMatricesValid_) {
    const float borderThickness =
        std::max(1.0f, 2.0f / std::max(0.001f, renderer_->getZoom()));
    const FloatColor borderColor{0.22f, 0.24f, 0.27f, 0.65f};
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
  if (showCameraFrustumOverlay_ && has3DCamera) {
    const auto cameraOverlayVisual = activeCamera
        ? buildCameraFrustumVisual(activeCamera, comp)
        : buildDefaultCameraFrustumVisual(cameraViewMatrix, cameraProjMatrix);

    ::Artifact::drawCameraFrustumOverlay(
        renderer_.get(), cameraOverlayVisual, cameraViewMatrix,
        cameraProjMatrix,
        activeCamera && activeCamera->id() == selectedLayerId_);

    // Two-node camera POI handle for the selected camera layer.
    if (selectedLayer && selectedLayer->is3D()) {
      ::Artifact::drawCameraPoiOverlay(renderer_.get(), selectedLayer,
                                       cameraViewMatrix, cameraProjMatrix);
    }

    if (activeCamera) {
      const QString cameraHud =
          QStringLiteral("Camera  Near %1  Far %2  Focus %3%4%5")
              .arg(activeCamera->nearClipPlane().value, 0, 'f', 2)
              .arg(activeCamera->farClipPlane().value, 0, 'f', 2)
              .arg(activeCamera->focusDistance().value, 0, 'f', 2)
              .arg(activeCamera->depthOfField() ? QStringLiteral("  DOF")
                                                : QString())
              .arg(activeCamera->motionBlur() ? QStringLiteral("  MB")
                                              : QString());
      renderer_->drawText(
          QRectF(12.0, 12.0, 420.0, 20.0), cameraHud,
          fixedWidthFont(9),
          FloatColor{0.82f, 0.95f, 1.0f, 0.92f}, Qt::AlignLeft | Qt::AlignTop);
    }

  }

  if (showDensityHeatmapOverlay_) {

    drawVisualDensityOverlay(renderer_.get(), comp, selectedLayer, currentFrame);

  }

  if (selectedLayer) {

    if (auto *selectedModel = dynamic_cast<Artifact3DLayer *>(selectedLayer.get());
        selectedModel) {
      const QMatrix4x4 selectionView = viewportOrientationMatricesValid_
          ? viewportOrientationViewForOverlay_
          : has3DCamera ? cameraViewMatrix : renderer_->getViewMatrix();
      const QMatrix4x4 selectionProjection = viewportOrientationMatricesValid_
          ? viewportOrientationProjectionForOverlay_
          : has3DCamera ? cameraProjMatrix : renderer_->getProjectionMatrix();
      ::Artifact::draw3DSelectionWireframeOverlay(
          renderer_.get(), selectedLayer, &selectionView, &selectionProjection);
    }

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

      // F1 (Phase D-1): shape vertex/tangent/segment emphasis on top of
      // the selection overlay. DTO only; no layer mutation here.
      if (dynamic_cast<ArtifactShapeLayer *>(selectedLayer.get())) {
        ::Artifact::ShapeVertexOverlayState shapeState;
        shapeState.hoveredVertex = hoveredShapePathVertex_;
        shapeState.hoveredTangent = hoveredShapePathTangent_;
        shapeState.draggingVertex = isDraggingShapePathVertex_
                                        ? draggingShapePathVertexIndex_
                                        : -1;
        shapeState.draggingTangent = isDraggingShapePathVertex_
                                         ? draggingShapePathTangent_
                                         : 0;
        shapeState.hoveredSegment = hoveredShapePathSegment_;
        shapeState.showSegmentInsert =
            hoveredShapePathSegment_ >= 0 &&
            hoveredShapePathVertex_ < 0 &&
            hostSnapshot.shiftModifierActive;
        shapeState.selectedVertices = selectedShapePathVertices_;
        shapeState.hoveredParam = hoveredShapeParam_;
        shapeState.draggingParam = shapeParamDragMode_;
        shapeState.hoveredOp = hoveredShapeOp_;
        shapeState.hoveredOpField = hoveredShapeOpField_;
        shapeState.draggingOp = shapeOpDragOp_;
        shapeState.draggingOpField = shapeOpDragField_;
        ::Artifact::drawShapeVertexOverlay(renderer_.get(), selectedLayer,
                                           shapeState);
      }

      // Keep the active layer's gizmo path above, while also showing the
      // selection overlay for every selected 3D layer. This makes the Phase 2
      // wireframe feedback useful for multi-selection without changing gizmo
      // ownership or introducing another event path.
      if (auto *selectionManager = ArtifactLayerSelectionManager::instance()) {
        for (const auto &additionalLayer : selectionManager->selectedLayersInOrder()) {
          if (!additionalLayer || additionalLayer->id() == selectedLayer->id()) {
            continue;
          }
          ::Artifact::drawSelectionOverlay(
              renderer_.get(), additionalLayer, overlayViewMatrix,
              overlayProjMatrix);
        }
      }

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

      gizmoMode_ == TransformGizmo::Mode::Rotate ||

      (gizmo_ && gizmo_->isDragging() &&

       gizmo_->activeHandle() == TransformGizmo::HandleType::Anchor);

  if (showAnchorCenterOverlay_ && selectedLayer && anchorOverlayToolActive) {

    ::Artifact::drawAnchorCenterOverlay(renderer_.get(), selectedLayer,
                                        &cameraViewMatrix, &cameraProjMatrix);

    const ArtifactCore::RationalTime anchorTime(currentFrame.framePosition(),
                                                30);
    const auto &transform3D = selectedLayer->transform3D();
    const QPointF anchorValue(transform3D.anchorXAt(anchorTime),
                              transform3D.anchorYAt(anchorTime));
    const QFont anchorFont = fixedWidthFont(9);
    const bool anchorIs3D = selectedLayer->is3D();
    const float anchorPanelWidth = anchorIs3D ? 232.0f : 178.0f;
    const float anchorPanelHeight = 38.0f;
    QString anchorLabel = QStringLiteral("Anchor  X %1  Y %2")
                              .arg(anchorValue.x(), 0, 'f', 1)
                              .arg(anchorValue.y(), 0, 'f', 1);
    if (anchorIs3D) {
      anchorLabel += QStringLiteral("  Z %1")
                         .arg(selectedLayer->transform3D().anchorZAt(anchorTime),
                              0, 'f', 1);
    }
    renderer_->drawRoundedPanel(
        12.0f, 12.0f, anchorPanelWidth, anchorPanelHeight, 6.0f,
        FloatColor{0.03f, 0.04f, 0.06f, 0.88f},
        FloatColor{0.68f, 0.86f, 1.0f, 0.95f}, 1.0f, 1.0f);
    renderer_->drawText(
        QRectF(21.0f, 18.0f, anchorPanelWidth - 18.0f, 22.0f),
        anchorLabel,
        anchorFont, FloatColor{0.88f, 0.96f, 1.0f, 1.0f},
        Qt::AlignLeft | Qt::AlignVCenter);

  }

  if (workCursorVisible_ && !workCursorState_.spatial) {

    ::Artifact::drawViewportWorkCursorOverlay(renderer_.get(),
                                              ArtifactCore::Coordinates::toQPointF(
                                                  workCursorState_.canvasPosition),
                                              workCursorLabel_);

  }

  drawSelectionEditingOverlay(hostSnapshot, comp, layers, selectedIds, currentFrame,

                              cw, ch, has3DCamera, cameraViewMatrix,

                              cameraProjMatrix);

  drawViewportGhostOverlay(hostSnapshot, comp, selectedLayer, currentFrame);

  drawViewportUiOverlay();

  drawViewportMagnifierOverlay(cw, ch);

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
  Artifact::drawViewportChannelOverlayImage(
      renderer_.get(), viewportChannelDisplayMode_,
      viewportChannelDisplaySRV_.RawPtr(), presentationLayout_, hostWidth_,
      hostHeight_, canvasWidth, canvasHeight);
}

void CompositionRenderController::Impl::drawOnionSkinOverlay(
    const ArtifactCompositionPtr &composition,
    const FramePosition &currentFrame, float canvasWidth, float canvasHeight) {

  if (!renderer_ || !showOnionSkin_ || canvasWidth <= 0.0f ||
      canvasHeight <= 0.0f) {

    return;

  }

  if (!composition) {
    resetOnionSkinHistory();
    return;
  }

  const bool hasAnimatedContent =
      compositionHasOnionSkinAnimation(composition, currentFrame);
  onionSkinHistory_.draw(
      renderer_.get(), true, hasAnimatedContent, composition->id().toString(),
      currentFrame.framePosition(), canvasWidth, canvasHeight,
      onionSkinFrameCount_, onionSkinOpacity_);
}

bool CompositionRenderController::Impl::hasCalculatedOnionSkinSource(
    const ArtifactCompositionPtr &composition) const {
  if (!composition || selectedLayerId_.isNil()) {
    return false;
  }

  const auto selectedLayer = composition->layerById(selectedLayerId_);
  return selectedLayer &&
         dynamic_cast<ArtifactPaintLayer *>(selectedLayer.get()) != nullptr;
}

void CompositionRenderController::Impl::queueOnionSkinCapture(
    CompositionRenderController *owner,
    const ArtifactCompositionPtr &composition,
    const FramePosition &currentFrame,
    float canvasWidth, float canvasHeight) {

  if (!renderer_ || !showOnionSkin_ || !owner || !composition) {
    return;
  }

  if (hasCalculatedOnionSkinSource(composition)) {
    const auto paintLayer = composition->layerById(selectedLayerId_);
    ::Artifact::drawPaintLayerOnionSkinOverlay(
        renderer_.get(), paintLayer, composition, canvasWidth, canvasHeight,
        onionSkinFrameCount_, onionSkinOpacity_);
    return;
  }

  const bool hasAnimatedContent =
      compositionHasOnionSkinAnimation(composition, currentFrame);
  onionSkinHistory_.queueCapture(
      owner, renderer_.get(), true, hasAnimatedContent,
      composition->id().toString(), currentFrame.framePosition());
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
    colorSamplerLayerId_ = LayerID::Nil();

    return hadSample;

  }

  const int sampleX = std::clamp(static_cast<int>(std::lround(viewportPos.x())), 0,
                                 std::max(0, frame.width() - 1));

  const int sampleY = std::clamp(static_cast<int>(std::lround(viewportPos.y())), 0,
                                 std::max(0, frame.height() - 1));

  const QColor sampledColor = frame.pixelColor(sampleX, sampleY);

  const LayerID sampledLayerId = owner->layerAtViewportPos(
      ArtifactCore::Coordinates::screenLogicalPointFromQPointF(viewportPos));

  const auto canvasPos =
      renderer_->viewportToCanvas({static_cast<float>(viewportPos.x()),
                                   static_cast<float>(viewportPos.y())});

  const bool changed =
      !colorSamplerHasSample_ || colorSamplerColor_ != sampledColor ||
      colorSamplerImagePixel_ != QPoint(sampleX, sampleY) ||
      colorSamplerLayerId_ != sampledLayerId ||
      std::abs(colorSamplerCanvasPos_.x - canvasPos.x) > 0.5 ||
      std::abs(colorSamplerCanvasPos_.y - canvasPos.y) > 0.5;

  colorSamplerHasSample_ = true;
  colorSamplerColor_ = sampledColor;
  colorSamplerImagePixel_ = QPoint(sampleX, sampleY);
  colorSamplerLayerId_ = sampledLayerId;
  colorSamplerCanvasPos_ = {canvasPos.x, canvasPos.y};

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

  const QString layerLabel = colorSamplerLayerId_.isNil()
                                 ? QStringLiteral("Layer —")
                                 : QStringLiteral("Layer %1")
                                       .arg(colorSamplerLayerId_.toString());
  const QString detail =
      QStringLiteral("%1  •  %2  •  %3  •  XY %4,%5  •  PX %6,%7")
          .arg(colorRgbLabel(colorSamplerColor_))
          .arg(colorHslLabel(colorSamplerColor_))
          .arg(layerLabel)
          .arg(QString::number(colorSamplerCanvasPos_.x, 'f', 1))
          .arg(QString::number(colorSamplerCanvasPos_.y, 'f', 1))
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

bool CompositionRenderController::Impl::magnifierLoupeRect(QRectF &outRect) const {
  if (hostWidth_ <= 0.0f || hostHeight_ <= 0.0f) return false;
  const float dpr = devicePixelRatio_ > 0.0f ? devicePixelRatio_ : 1.0f;
  const float side = 180.0f * dpr;
  const float margin = 16.0f * dpr;
  const float maxX = std::max(0.0f, hostWidth_ - side);
  const float maxY = std::max(0.0f, hostHeight_ - side);
  float lx = std::max(0.0f, maxX - margin);
  float ly = std::max(0.0f, maxY - margin);
  if (magnifierFollowCursor_) {
    lx = std::clamp(magnifierCursorViewportPos_.x + margin,
                    0.0f, maxX);
    ly = std::clamp(magnifierCursorViewportPos_.y + margin,
                    0.0f, maxY);
  }
  outRect = QRectF(static_cast<double>(lx), static_cast<double>(ly),
                   static_cast<double>(side), static_cast<double>(side));
  return true;
}

void CompositionRenderController::Impl::drawViewportMagnifierOverlay(float cw,
                                                                     float ch) {
  if (!renderer_ || !magnifierEnabled_ || !lastPresentedReadbackSRV_ ||
      hostWidth_ <= 0.0f || hostHeight_ <= 0.0f) {
    return;
  }

  QRectF loupe;
  if (!magnifierLoupeRect(loupe)) return;

  const float side = static_cast<float>(loupe.width());
  const float lx = static_cast<float>(loupe.x());
  const float ly = static_cast<float>(loupe.y());
  const float dpr = devicePixelRatio_ > 0.0f ? devicePixelRatio_ : 1.0f;
  const float scale = static_cast<float>(std::clamp(magnifierScale_, 2, 8));

  float cursorX = magnifierCursorViewportPos_.x;
  float cursorY = magnifierCursorViewportPos_.y;
  if (cursorX <= 0.0f && cursorY <= 0.0f) {
    cursorX = hostWidth_ * 0.5f;
    cursorY = hostHeight_ * 0.5f;
  }

  // Canvas coordinates inside the scissored viewport are loupe-relative, so
  // scale the whole composited texture and shift the sampled cursor point to
  // the loupe centre (mirrors the quad-pane blit in finalizeGpuRenderToViewport).
  const float offsetX = side * 0.5f - cursorX * scale;
  const float offsetY = side * 0.5f - cursorY * scale;

  const auto theme = ArtifactCore::currentDCCTheme();
  const QColor borderQc(theme.focusRingColor);
  const QColor accentQc(theme.accentColor);
  const QColor textQc(theme.textColor);
  const QColor panelQc(theme.backgroundColor);
  auto toFloat = [](const QColor &c, float alpha) {
    return FloatColor{static_cast<float>(c.redF()),
                      static_cast<float>(c.greenF()),
                      static_cast<float>(c.blueF()), alpha};
  };
  const FloatColor borderColor = toFloat(borderQc, 1.0f);
  const FloatColor crosshairColor = toFloat(accentQc, 0.95f);
  const FloatColor textColor = toFloat(textQc, 1.0f);
  const FloatColor panelColor = toFloat(panelQc, 0.72f);

  const float savedZoom = renderer_->getZoom();
  float savedPanX = 0.0f;
  float savedPanY = 0.0f;
  renderer_->getPan(savedPanX, savedPanY);

  // Hardware viewport/scissor is immediate state while sprite commands are
  // buffered; drain before switching so the loupe is not clipped unexpectedly.
  renderer_->flush();

  renderer_->setViewportRect(lx, ly, side, side, hostWidth_, hostHeight_);
  renderer_->setCanvasSize(side, side);
  renderer_->setPan(0.0f, 0.0f);
  renderer_->setZoom(1.0f);

  renderer_->drawSprite(offsetX, offsetY, hostWidth_ * scale, hostHeight_ * scale,
                        lastPresentedReadbackSRV_.RawPtr(), 1.0f);

  const float bw = std::max(1.0f, 1.5f * dpr);
  renderer_->drawSolidRect(0.0f, 0.0f, side, bw, borderColor, 1.0f);
  renderer_->drawSolidRect(0.0f, side - bw, side, bw, borderColor, 1.0f);
  renderer_->drawSolidRect(0.0f, 0.0f, bw, side, borderColor, 1.0f);
  renderer_->drawSolidRect(side - bw, 0.0f, bw, side, borderColor, 1.0f);

  const float cross = 6.0f * dpr;
  const float center = side * 0.5f;
  renderer_->drawSolidRect(center - cross, center - 0.5f, cross * 2.0f, 1.0f,
                           crosshairColor, 0.95f);
  renderer_->drawSolidRect(center - 0.5f, center - cross, 1.0f, cross * 2.0f,
                           crosshairColor, 0.95f);

  if (magnifierLabelScale_ != magnifierScale_) {
    magnifierLabelScale_ = magnifierScale_;
    magnifierLabelFont_ = QApplication::font();
    magnifierLabelFont_.setPointSizeF(
        std::max(9.0, static_cast<double>(magnifierLabelFont_.pointSizeF())));
    magnifierLabelText_ = QStringLiteral("%1x").arg(magnifierScale_);
    const QFontMetrics fm(magnifierLabelFont_);
    magnifierLabelWidth_ = fm.horizontalAdvance(magnifierLabelText_);
    magnifierLabelHeight_ = fm.height();
  }
  const float labelH = static_cast<float>(magnifierLabelHeight_) + 6.0f;
  const float labelW = static_cast<float>(magnifierLabelWidth_) + 12.0f;
  renderer_->drawSolidRect(0.0f, side - labelH, labelW, labelH, panelColor, 1.0f);
  renderer_->drawText(QRectF(6.0f, side - labelH, side - 12.0f, labelH),
                      magnifierLabelText_, magnifierLabelFont_, textColor,
                      Qt::AlignLeft | Qt::AlignVCenter);

  renderer_->flush();

  renderer_->setViewportRect(hostWidth_, hostHeight_);
  renderer_->setCanvasSize(cw, ch);
  renderer_->setPan(savedPanX, savedPanY);
  renderer_->setZoom(savedZoom);
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

  if (comp && !selectedLayerId_.isNil()) {
    const auto child = comp->layerById(selectedLayerId_);
    const auto parent = child ? child->parentLayer() : nullptr;
    if (child && parent && renderer_) {
      const QPointF childCanvas = child->getGlobalTransform().map(QPointF(0.0, 0.0));
      const QPointF parentCanvas = parent->getGlobalTransform().map(QPointF(0.0, 0.0));
      const auto childViewport = renderer_->canvasToViewport(
          {static_cast<float>(childCanvas.x()), static_cast<float>(childCanvas.y())});
      const auto parentViewport = renderer_->canvasToViewport(
          {static_cast<float>(parentCanvas.x()), static_cast<float>(parentCanvas.y())});
      const FloatColor relationshipColor{0.35f, 0.85f, 1.0f, 0.82f};
      renderer_->drawDashedLineLocal(
          {childViewport.x, childViewport.y},
          {parentViewport.x, parentViewport.y}, 1.5f, 8.0f, 5.0f,
          relationshipColor);
      renderer_->drawCircle(childViewport.x, childViewport.y, 3.5f,
                            relationshipColor, 1.0f, false);
      renderer_->drawCircle(parentViewport.x, parentViewport.y, 4.5f,
                            relationshipColor, 1.0f, false);
    }
  }

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



  ViewportOverlay::drawGuides(renderer_.get(), guideVerticals_,
                              guideHorizontals_, showGuides_, cw, ch);

}



}

// ── Rig2D Visualisation ──────────────────────────────────────

namespace Artifact {
namespace {

SelectionFrameHandle selectionFrameHandleFor(
    TransformGizmo::HandleType handle) {
  switch (handle) {
  case TransformGizmo::HandleType::Scale_TL:
    return SelectionFrameHandle::TopLeft;
  case TransformGizmo::HandleType::Scale_TR:
    return SelectionFrameHandle::TopRight;
  case TransformGizmo::HandleType::Scale_BL:
    return SelectionFrameHandle::BottomLeft;
  case TransformGizmo::HandleType::Scale_BR:
    return SelectionFrameHandle::BottomRight;
  case TransformGizmo::HandleType::Scale_T:
    return SelectionFrameHandle::Top;
  case TransformGizmo::HandleType::Scale_B:
    return SelectionFrameHandle::Bottom;
  case TransformGizmo::HandleType::Scale_L:
    return SelectionFrameHandle::Left;
  case TransformGizmo::HandleType::Scale_R:
    return SelectionFrameHandle::Right;
  default:
    return SelectionFrameHandle::None;
  }
}

void drawRigBone(Artifact::ArtifactIRenderer* renderer,
                 ArtifactCore::Bone2D* bone, const QTransform& globalTx,
                 const ArtifactCore::FloatColor& color, float thickness, float zoom) {
    if (!renderer || !bone) return;
    Q_UNUSED(zoom);
    const QMatrix4x4 &boneMatrix = bone->globalMatrix();
    const QVector3D origin3 = boneMatrix * QVector3D(0.0f, 0.0f, 0.0f);
    const QVector3D tip3 = boneMatrix * QVector3D(0.0f, -bone->length(), 0.0f);
    const QPointF bonePos = globalTx.map(QPointF(origin3.x(), origin3.y()));
    const QPointF tip = globalTx.map(QPointF(tip3.x(), tip3.y()));
    renderer->drawSolidLine(
        {(float)bonePos.x(), (float)bonePos.y()},
        {(float)tip.x(), (float)tip.y()},
        color, thickness);
    renderer->drawCircle((float)bonePos.x(), (float)bonePos.y(), 4.0f, color, 0.0f, true);
    for (auto* child : bone->children()) {
        if (child) drawRigBone(renderer, child, globalTx, color, thickness, zoom);
    }
}

void drawRigControl(Artifact::ArtifactIRenderer* renderer,
                    ArtifactCore::RigControl2D* ctrl,
                    const QTransform& globalTx) {
    if (!renderer || !ctrl || !ctrl->enabled()) return;
    QPointF cPos;
    if (ctrl->kind() == ArtifactCore::RigControlKind::Point) {
        QVector2D pt = ctrl->value().value<QVector2D>();
        cPos = globalTx.map(QPointF(pt.x(), pt.y()));
    } else {
        cPos = globalTx.map(QPointF(0, 0));
    }
    float size = 8.0f;
    ArtifactCore::FloatColor color = ArtifactCore::FloatColor(1.0f, 0.3f, 0.3f, 1.0f);
    switch (ctrl->kind()) {
    case ArtifactCore::RigControlKind::Slider: color = ArtifactCore::FloatColor(0.3f, 0.8f, 1.0f, 1.0f); break;
    case ArtifactCore::RigControlKind::Point:  color = ArtifactCore::FloatColor(0.3f, 1.0f, 0.4f, 1.0f); break;
    case ArtifactCore::RigControlKind::Angle:  color = ArtifactCore::FloatColor(1.0f, 0.7f, 0.2f, 1.0f); break;
    default: break;
    }
    renderer->drawCircle((float)cPos.x(), (float)cPos.y(), size, color, 0.0f, true);
    renderer->drawCircle((float)cPos.x(), (float)cPos.y(), size + 2.0f,
                          ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.5f), 1.0f, false);
}

void drawRigSkinWireframe(Artifact::ArtifactIRenderer* renderer,
                           ArtifactCore::SkinMesh* mesh,
                           const QTransform& globalTx,
                           const ArtifactCore::FloatColor& color) {
    if (!renderer || !mesh) return;
    const auto& tris = mesh->triangles();
    const auto& verts = mesh->vertices();
    for (size_t i = 0; i + 2 < tris.size(); i += 3) {
        for (int e = 0; e < 3; ++e) {
            uint32_t a = tris[i + e], b = tris[i + (e + 1) % 3];
            if (a >= verts.size() || b >= verts.size()) continue;
            QPointF pa = globalTx.map(QPointF(verts[a].position.x(), verts[a].position.y()));
            QPointF pb = globalTx.map(QPointF(verts[b].position.x(), verts[b].position.y()));
            renderer->drawSolidLine(
                {(float)pa.x(), (float)pa.y()}, {(float)pb.x(), (float)pb.y()},
                color, 0.7f);
        }
    }
}

} // namespace


void CompositionRenderController::Impl::drawThreeDimensionalGroundGrid() {

  // The composition grid is a 2D, composition-space aid in the Front view.
  // Only spatial viewport orientations use the projected ground grid.
  if (!renderer_ || !showGrid_ || !gizmo3DCameraMatricesValid_ ||
      isFrontOrthographicViewport()) {
    return;
  }

  const QRect viewport(0, 0,
                       std::max(1, static_cast<int>(hostWidth_)),
                       std::max(1, static_cast<int>(hostHeight_)));
  const QVector3D origin = ViewportMath::projectToTopDown(
      QVector3D(0.0f, 0.0f, 0.0f), gizmo3DViewMatrix_,
      gizmo3DProjectionMatrix_, viewport);
  const QVector3D unitX = ViewportMath::projectToTopDown(
      QVector3D(1.0f, 0.0f, 0.0f), gizmo3DViewMatrix_,
      gizmo3DProjectionMatrix_, viewport);
  const QVector3D unitY = ViewportMath::projectToTopDown(
      QVector3D(0.0f, 1.0f, 0.0f), gizmo3DViewMatrix_,
      gizmo3DProjectionMatrix_, viewport);
  const float pixels = std::max(
      std::hypot(unitX.x() - origin.x(), unitX.y() - origin.y()),
      std::hypot(unitY.x() - origin.x(), unitY.y() - origin.y()));
  const float pixelsPerWorldUnit =
      std::isfinite(pixels) && pixels > 0.0001f
          ? std::clamp(pixels, 0.0001f, 10000.0f)
          : 1.0f;
  const auto tickStep = ViewportTickCalculator::compute(
      pixelsPerWorldUnit, gridAutoStepTargetViewportInterval_,
      QStringLiteral("units"));
  const float majorSpacing = gridAutoStepEnabled_
      ? std::max(1.0f, tickStep.interval)
      : std::max(1.0f, gridSettings_.majorInterval);

  Artifact::Grid::GroundGridSettings groundSettings;
  groundSettings.majorInterval = std::max(0.0001f, majorSpacing);
  groundSettings.subdivisions = std::max(1, gridSettings_.subdivisions);

  bool invertible = false;
  const QMatrix4x4 inverseView = gizmo3DViewMatrix_.inverted(&invertible);
  const QVector3D cameraPosition = invertible
      ? inverseView.map(QVector3D(0.0f, 0.0f, 0.0f))
      : QVector3D();
  const float cameraDistance = std::max(groundSettings.majorInterval,
                                        cameraPosition.length());
  groundSettings.extent = std::max(
      groundSettings.majorInterval * 20.0f,
      std::min(cameraDistance * 1.5f,
               groundSettings.majorInterval * 80.0f));
  groundSettings.fadeStart = groundSettings.extent * 0.60f;
  groundSettings.fadeEnd = groundSettings.extent;
  groundSettings.majorColor = gridSettings_.majorColor;
  groundSettings.minorColor = gridSettings_.minorColor;

  renderer_->setGizmoCameraMatrices(gizmo3DViewMatrix_,
                                    gizmo3DProjectionMatrix_);
  const auto groundLines =
      Artifact::Grid::GridSystem::computeGroundGridLines(groundSettings);
  for (const auto &line : groundLines) {
    if (line.color.a() <= 0.0f ||
        (line.isMajor && !gridSettings_.showMajor) ||
        (!line.isMajor && !gridSettings_.showMinor)) {
      continue;
    }
    renderer_->draw3DLine(
        {line.start.x(), line.start.y(), line.start.z()},
        {line.end.x(), line.end.y(), line.end.z()}, line.color,
        line.thickness);
  }
  if (gridSettings_.showAxis) {
    const float axisExtent = groundSettings.extent;
    renderer_->draw3DLine(
        {-axisExtent, 0.0f, 0.0f}, {axisExtent, 0.0f, 0.0f},
        gridSettings_.axisColor, gridSettings_.axisStyle.thickness);
    renderer_->draw3DLine(
        {0.0f, -axisExtent, 0.0f}, {0.0f, axisExtent, 0.0f},
        gridSettings_.axisColor, gridSettings_.axisStyle.thickness);
  }
  renderer_->flushGizmo3D();
  renderer_->resetGizmoCameraMatrices();
}

void CompositionRenderController::Impl::drawViewportCanvasOverlay(
                                                                  const HostFrameSnapshot &hostSnapshot,
                                                                  float cw,

                                                                  float ch) {

  if (!renderer_ || cw <= 0.0f || ch <= 0.0f) {

    return;

  }

  const bool threeDimensionalViewport = !isFrontOrthographicViewport();

  const auto projectedWorldPixelsPerUnit = [&]() {
    if (!gizmo3DCameraMatricesValid_) {
      return 1.0f;
    }
    const QRect viewport(0, 0,
                         std::max(1, static_cast<int>(hostWidth_)),
                         std::max(1, static_cast<int>(hostHeight_)));
    const QVector3D origin = ViewportMath::projectToTopDown(
        QVector3D(0.0f, 0.0f, 0.0f), gizmo3DViewMatrix_,
        gizmo3DProjectionMatrix_, viewport);
    const QVector3D unitX = ViewportMath::projectToTopDown(
        QVector3D(1.0f, 0.0f, 0.0f), gizmo3DViewMatrix_,
        gizmo3DProjectionMatrix_, viewport);
    const QVector3D unitY = ViewportMath::projectToTopDown(
        QVector3D(0.0f, 1.0f, 0.0f), gizmo3DViewMatrix_,
        gizmo3DProjectionMatrix_, viewport);
    const float xPixels = std::hypot(unitX.x() - origin.x(),
                                     unitX.y() - origin.y());
    const float yPixels = std::hypot(unitY.x() - origin.x(),
                                     unitY.y() - origin.y());
    const float pixels = std::max(xPixels, yPixels);
    return std::isfinite(pixels) && pixels > 0.0001f
               ? std::clamp(pixels, 0.0001f, 10000.0f)
               : 1.0f;
  };

  if (auto *app = ArtifactApplicationManager::instance()) {
    if (app->puppetTool() && !selectedLayerId_.isNil()) {
      app->puppetTool()->renderOverlay(renderer_.get(), selectedLayerId_);
    }
  }

  // Rig tools opt into the overlay automatically; the public flag also lets
  // a future rig panel keep the skeleton visible without owning a tool.
  const bool rigToolActive =
      hostSnapshot.activeToolKind == CompositionHostToolKind::RigSelect ||
      hostSnapshot.activeToolKind == CompositionHostToolKind::RigWeight;
  if ((showRigOverlay_ || rigToolActive) && !selectedLayerId_.isNil()) {
    if (const auto comp = previewPipeline_.composition()) {
      if (const auto layer = comp->layerById(selectedLayerId_)) {
        if (auto *rigLayer = dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())) {
          auto &rig = rigLayer->rig2D();
          const QTransform layerTx = rigLayer->getGlobalTransform();
          const FloatColor boneColor(0.25f, 0.82f, 1.0f, 0.95f);
          const FloatColor meshColor(0.35f, 1.0f, 0.45f, 0.55f);
          const float rigZoom = std::max(renderer_->getZoom(), 0.001f);
          const bool showRigBones = lineDebugKindVisible(
              lineDebugVisibility_, LineDebugKind::RigBone);
          const bool showRigControls = lineDebugKindVisible(
              lineDebugVisibility_, LineDebugKind::RigControl);
          const bool showRigSkin = lineDebugKindVisible(
              lineDebugVisibility_, LineDebugKind::RigSkin);
          if (showRigBones && rig.rootBone()) {
            drawRigBone(renderer_.get(), rig.rootBone(), layerTx,
                        boneColor, std::max(1.0f, 2.0f / rigZoom), rigZoom);
          }
          if (showRigControls) {
            for (auto *control : rig.controls()) {
              drawRigControl(renderer_.get(), control, layerTx);
            }
          }
          if (showRigBones && !selectedRigBoneId_.isNil()) {
            if (auto *bone = rig.findBone(selectedRigBoneId_)) {
              const auto position = layerTx.map(QPointF(
                  bone->resolvedTransform().position.x(),
                  bone->resolvedTransform().position.y()));
              renderer_->drawCircle(static_cast<float>(position.x()),
                                    static_cast<float>(position.y()), 7.0f,
                                    FloatColor(1.0f, 0.9f, 0.2f, 1.0f),
                                    2.0f, false);
            }
          }
          if (showRigControls && !selectedRigControlId_.isNil()) {
            if (auto *control = rig.findControl(selectedRigControlId_)) {
              QPointF localPosition;
              if (control->kind() == RigControlKind::Point) {
                const QVector2D value = control->value().value<QVector2D>();
                localPosition = QPointF(value.x(), value.y());
              }
              const auto position = layerTx.map(localPosition);
              renderer_->drawCircle(static_cast<float>(position.x()),
                                    static_cast<float>(position.y()), 11.0f,
                                    FloatColor(1.0f, 0.9f, 0.2f, 1.0f),
                                    2.0f, false);
            }
          }
          if (rigToolActive || showRigOverlay_) {
            QString selectionText = QStringLiteral("none");
            if (!selectedRigBoneId_.isNil()) {
              if (auto *bone = rig.findBone(selectedRigBoneId_)) {
                selectionText = QStringLiteral("bone: %1  R:%2°")
                    .arg(bone->name())
                    .arg(QString::number(bone->localTransform().rotation, 'f', 1));
              }
            } else if (!selectedRigControlId_.isNil()) {
              if (auto *control = rig.findControl(selectedRigControlId_)) {
                selectionText = QStringLiteral("control: %1").arg(control->name());
                if (control->kind() == RigControlKind::Point) {
                  const QVector2D point = control->value().value<QVector2D>();
                  selectionText += QStringLiteral("  X:%1 Y:%2")
                      .arg(QString::number(point.x(), 'f', 1))
                      .arg(QString::number(point.y(), 'f', 1));
                } else {
                  selectionText += QStringLiteral("  V:%1")
                      .arg(QString::number(control->value().toDouble(), 'f', 2));
                }
              }
            }
            const QString modeText =
                hostSnapshot.activeToolKind == CompositionHostToolKind::RigWeight
                ? QStringLiteral("WEIGHT") : QStringLiteral("SELECT");
            const QString poseState = rigPoseClipboardValid_
                ? QStringLiteral("  POSE:READY") : QString();
            const QFont rigHudFont(QStringLiteral("sans-serif"), 9);
            renderer_->drawSolidRect(10.0f, ch - 34.0f, 250.0f, 24.0f,
                                     FloatColor(0.02f, 0.04f, 0.08f, 0.82f),
                                     0.9f);
            renderer_->drawText(
                QRectF(18.0f, ch - 32.0f, 234.0f, 20.0f),
                QStringLiteral("RIG %1  %2%3")
                    .arg(modeText, selectionText, poseState),
                rigHudFont, FloatColor(0.78f, 0.92f, 1.0f, 0.96f),
                Qt::AlignLeft | Qt::AlignVCenter);
          }
          if (rigToolActive || showRigOverlay_) {
            const float panelWidth = 224.0f;
            const float panelX = std::max(8.0f, cw - panelWidth - 10.0f);
            const float panelY = 10.0f;
            const float rowHeight = 17.0f;
            const int maxRows = std::max(1, static_cast<int>((ch - 48.0f) /
                                                               rowHeight));
            renderer_->drawSolidRect(panelX, panelY, panelWidth,
                                     std::min(ch - 20.0f, 28.0f +
                                              rowHeight * maxRows),
                                     FloatColor(0.02f, 0.04f, 0.08f, 0.84f),
                                     0.9f);
            const QFont hierarchyFont(QStringLiteral("sans-serif"), 8);
            renderer_->drawText(
                QRectF(panelX + 8.0f, panelY + 4.0f, panelWidth - 16.0f, 16.0f),
                QStringLiteral("RIG HIERARCHY"), hierarchyFont,
                FloatColor(0.82f, 0.94f, 1.0f, 0.98f),
                Qt::AlignLeft | Qt::AlignVCenter);
            int row = 0;
            bool hierarchyTruncated = false;
            const std::function<void(Bone2D *, int)> drawBoneRow =
                [&](Bone2D *bone, int depth) {
                  if (!bone) return;
                  if (row >= maxRows) {
                    hierarchyTruncated = true;
                    return;
                  }
                  const float y = panelY + 23.0f + rowHeight * row++;
                  const bool selected = bone->id() == selectedRigBoneId_;
                  if (selected) {
                    renderer_->drawSolidRect(panelX + 3.0f, y - 1.0f,
                                             panelWidth - 6.0f, rowHeight,
                                             FloatColor(0.18f, 0.34f, 0.5f, 0.9f),
                                             0.0f);
                  }
                  renderer_->drawText(
                      QRectF(panelX + 8.0f + depth * 11.0f, y,
                             panelWidth - 16.0f - depth * 11.0f, rowHeight),
                      QStringLiteral("%1 %2")
                          .arg(depth > 0 ? QStringLiteral("↳")
                                         : QStringLiteral("●"))
                          .arg(bone->name()),
                      hierarchyFont,
                      selected ? FloatColor(1.0f, 0.9f, 0.25f, 1.0f)
                               : FloatColor(0.72f, 0.84f, 0.92f, 0.95f),
                      Qt::AlignLeft | Qt::AlignVCenter);
                  for (auto *child : bone->children()) {
                    drawBoneRow(child, depth + 1);
                  }
                };
            drawBoneRow(rig.rootBone(), 0);
            for (auto *control : rig.controls()) {
              if (!control) continue;
              if (row >= maxRows) {
                hierarchyTruncated = true;
                break;
              }
              const float y = panelY + 23.0f + rowHeight * row++;
              const bool selected = control->id() == selectedRigControlId_;
              if (selected) {
                renderer_->drawSolidRect(panelX + 3.0f, y - 1.0f,
                                         panelWidth - 6.0f, rowHeight,
                                         FloatColor(0.36f, 0.28f, 0.12f, 0.9f),
                                         0.0f);
              }
              renderer_->drawText(
                  QRectF(panelX + 19.0f, y, panelWidth - 27.0f, rowHeight),
                  QStringLiteral("◇ %1").arg(control->name()), hierarchyFont,
                  selected ? FloatColor(1.0f, 0.9f, 0.25f, 1.0f)
                           : FloatColor(0.72f, 0.84f, 0.92f, 0.95f),
                  Qt::AlignLeft | Qt::AlignVCenter);
            }
            if (hierarchyTruncated && row > 0) {
              const float y = panelY + 23.0f + rowHeight * (row - 1);
              renderer_->drawText(
                  QRectF(panelX + panelWidth - 82.0f, y,
                         74.0f, rowHeight), QStringLiteral("… more"),
                  hierarchyFont, FloatColor(1.0f, 0.72f, 0.28f, 0.98f),
                  Qt::AlignRight | Qt::AlignVCenter);
            }
          }
          if (showRigSkin && rig.skinMesh()) {
            drawRigSkinWireframe(renderer_.get(), rig.skinMesh(), layerTx,
                                 meshColor);
            if (hostSnapshot.activeToolKind == CompositionHostToolKind::RigWeight) {
              const auto &bones = rig.bones();
              int selectedBoneIndex = 0;
              if (!selectedRigBoneId_.isNil()) {
                for (int i = 0; i < bones.size(); ++i) {
                  if (bones[i] && bones[i]->id() == selectedRigBoneId_) {
                    selectedBoneIndex = i;
                    break;
                  }
                }
              }
              std::vector<QVector2D> deformed;
              rig.skinMesh()->deform(&rig, deformed);
              const auto &vertices = rig.skinMesh()->vertices();
              for (size_t i = 0; i < vertices.size() && i < deformed.size(); ++i) {
                float weight = 0.0f;
                for (int slot = 0; slot < 4; ++slot) {
                  if (vertices[i].boneIndices[slot] == selectedBoneIndex) {
                    weight += vertices[i].weights[slot];
                  }
                }
                weight = std::clamp(weight, 0.0f, 1.0f);
                const FloatColor weightColor = weight < 0.5f
                    ? FloatColor(0.05f, weight * 2.0f, 1.0f - weight * 2.0f, 0.88f)
                    : FloatColor((weight - 0.5f) * 2.0f, 1.0f,
                                 0.05f, 0.88f);
                const auto position = layerTx.map(QPointF(
                    deformed[i].x(), deformed[i].y()));
                renderer_->drawCircle(static_cast<float>(position.x()),
                                      static_cast<float>(position.y()),
                                      std::clamp(4.0f / rigZoom, 2.0f, 7.0f),
                                      weightColor, 0.0f, true);
              }
            }
          }
          if (hostSnapshot.activeToolKind == CompositionHostToolKind::RigWeight) {
            const QPointF brushPosition = layerTx.map(
                ArtifactCore::Coordinates::toQPointF(
                    rigWeightLastLocalPoint_));
            const float brushRadius = rigWeightRadius_ * rigZoom;
              const bool subtract = hostSnapshot.controlModifierActive;
            const FloatColor brushColor = subtract
                ? FloatColor(1.0f, 0.28f, 0.28f, 0.85f)
                : FloatColor(0.25f, 0.85f, 1.0f, 0.85f);
            renderer_->drawCircle(static_cast<float>(brushPosition.x()),
                                  static_cast<float>(brushPosition.y()),
                                  std::max(2.0f, brushRadius), brushColor,
                                  1.5f, false);
            renderer_->drawCircle(static_cast<float>(brushPosition.x()),
                                  static_cast<float>(brushPosition.y()), 2.0f,
                                  brushColor, 0.0f, true);
            const QFont brushHudFont(QStringLiteral("sans-serif"), 8);
            renderer_->drawSolidRect(10.0f, ch - 62.0f, 250.0f, 22.0f,
                                     FloatColor(0.02f, 0.04f, 0.08f, 0.78f),
                                     0.8f);
            renderer_->drawText(
                QRectF(18.0f, ch - 60.0f, 234.0f, 18.0f),
                QStringLiteral("WEIGHT %1  R:%2  O:%3  F:%4")
                    .arg(subtract ? QStringLiteral("SUB") : QStringLiteral("ADD"))
                    .arg(QString::number(rigWeightRadius_, 'f', 0))
                    .arg(QString::number(rigWeightOpacity_, 'f', 2))
                    .arg(QString::number(rigWeightFlow_, 'f', 2)),
                brushHudFont, FloatColor(0.78f, 0.92f, 1.0f, 0.96f),
                Qt::AlignLeft | Qt::AlignVCenter);
          }
        }
      }
    }
  }



  const float zoom = std::max(renderer_->getZoom(), 0.001f);

  if (auto *app = ArtifactApplicationManager::instance()) {
    if (auto *motionSketch = app->motionSketchTool();
        motionSketch && motionSketch->isSketching()) {
      if (!motionSketch->showBackground()) {
        renderer_->drawSolidRect(0.0f, 0.0f, cw, ch,
                                 {0.01f, 0.02f, 0.04f, 0.52f}, 1.0f);
      }
      if (motionSketch->showWireframe() && !selectedLayerId_.isNil()) {
        if (const auto comp = previewPipeline_.composition()) {
          if (const auto layer = comp->layerById(selectedLayerId_)) {
            const QRectF wireframe = layer->transformedBoundingBox().normalized();
            if (wireframe.isValid() && wireframe.width() > 0.0 &&
                wireframe.height() > 0.0) {
              drawRectOutline(renderer_.get(), wireframe,
                              {1.0f, 0.72f, 0.20f, 0.8f}, 1.0f);
            }
          }
        }
      }
      const auto &samples = motionSketch->sampledPoints();
      if (samples.size() >= 2) {
        std::vector<Detail::float2> sketchPath;
        sketchPath.reserve(samples.size());
        for (const auto &sample : samples) {
          sketchPath.emplace_back(static_cast<float>(sample.x()),
                                  static_cast<float>(sample.y()));
        }
        renderer_->drawPolyline(sketchPath,
                                {0.98f, 0.70f, 0.20f, 0.9f},
                                1.8f / zoom);
        const auto &last = samples.back();
        const auto &previous = samples[samples.size() - 2];
        const float velocity = static_cast<float>(
            std::hypot(last.x() - previous.x(), last.y() - previous.y()));
        float acceleration = 0.0f;
        if (samples.size() >= 3) {
          const auto &beforePrevious = samples[samples.size() - 3];
          const float previousVelocity = static_cast<float>(std::hypot(
              previous.x() - beforePrevious.x(),
              previous.y() - beforePrevious.y()));
          acceleration = velocity - previousVelocity;
        }
        const QFont sketchFont(QStringLiteral("sans-serif"), 9);
        renderer_->drawSolidRect(8.0f, 8.0f, 404.0f, 18.0f,
                                 {0.0f, 0.0f, 0.0f, 0.68f}, 0.85f);
        const QString pressureLabel =
            motionSketch->pressureAffectsOpacity()
                ? QString::number(motionSketch->pressure(), 'f', 2)
                : QStringLiteral("OFF");
        renderer_->drawText(
            QRectF(12.0f, 8.0f, 396.0f, 18.0f),
            QStringLiteral("Sketch %1  v %2  a %3  smooth %4  %5fps  P:%6  WF:%7  BG:%8")
                .arg(QString::number(samples.size()))
                .arg(QString::number(velocity, 'f', 1))
                .arg(QString::number(acceleration, 'f', 1))
                .arg(QString::number(motionSketch->smoothing(), 'f', 2))
                .arg(QString::number(motionSketch->sampleRate(), 'f', 0))
                .arg(pressureLabel)
                .arg(motionSketch->showWireframe() ? QStringLiteral("ON")
                                                    : QStringLiteral("OFF"))
                .arg(motionSketch->showBackground() ? QStringLiteral("ON")
                                                     : QStringLiteral("OFF")),
            sketchFont, {1.0f, 0.9f, 0.55f, 0.95f},
            Qt::AlignLeft | Qt::AlignVCenter);
      }
    }
  }

  if (textToolDragging_) {
    const QRectF textRect =
        QRectF(QPointF(textToolStartCanvas_.x, textToolStartCanvas_.y),
               QPointF(textToolCurrentCanvas_.x,
                       textToolCurrentCanvas_.y)).normalized();
    renderer_->drawRectOutlineLocal(
        static_cast<float>(textRect.left()), static_cast<float>(textRect.top()),
        static_cast<float>(textRect.width()),
        static_cast<float>(textRect.height()),
        FloatColor{0.40f, 0.82f, 1.0f, 0.95f});
  }

  if (showGrid_ && !threeDimensionalViewport) {

    const auto niceGridInterval = [](float raw) {
      if (!(raw > 0.0f) || !std::isfinite(raw)) return 1.0f;
      const float exponent = std::floor(std::log10(raw));
      const float scale = std::pow(10.0f, exponent);
      const float normalized = raw / scale;
      const float nice = normalized <= 1.0f ? 1.0f
          : normalized <= 2.0f ? 2.0f
          : normalized <= 5.0f ? 5.0f : 10.0f;
      return std::max(1.0f, nice * scale);
    };
    const float targetViewportInterval =
        gridAutoStepTargetViewportInterval_;
    const float autoSpacing = niceGridInterval(
        targetViewportInterval / std::max(0.001f, zoom));
    const float majorSpacing = gridAutoStepEnabled_
        ? autoSpacing : std::max(1.0f, gridSettings_.majorInterval);

    const int subdivisions = std::max(1, gridSettings_.subdivisions);

    // Visible viewport rectangle in canvas coordinates. The grid spans the
    // whole viewport (not just the composition frame) while staying anchored
    // to the composition origin, matching AE's composition-space grid.
    float gridPanX = 0.0f;
    float gridPanY = 0.0f;
    renderer_->getPan(gridPanX, gridPanY);
    const float gridOriginX = -gridPanX / zoom;
    const float gridOriginY = -gridPanY / zoom;
    const float gridExtentW = std::max(1.0f, hostWidth_) / zoom;
    const float gridExtentH = std::max(1.0f, hostHeight_) / zoom;

    const float minorSpacing = majorSpacing / static_cast<float>(subdivisions);
    const float gridFade = std::clamp(0.25f + zoom * 0.75f, 0.25f, 1.0f);
    const auto fadedGridColor = [gridFade](const FloatColor& color) {
      return FloatColor{color.r(), color.g(), color.b(), color.a() * gridFade};
    };
    const FloatColor fadedMajorColor = fadedGridColor(gridSettings_.majorColor);
    const FloatColor fadedMinorColor = fadedGridColor(gridSettings_.minorColor);
    const FloatColor fadedAxisColor = fadedGridColor(gridSettings_.axisColor);

    if (gridPolarMode_) {
      const float centerX = cw * 0.5f;
      const float centerY = ch * 0.5f;
      const float maxRadius = std::hypot(centerX, centerY);
      const int maxRings = 64;
      int ringCount = 0;
      for (float radius = majorSpacing;
           radius <= maxRadius && ringCount < maxRings;
           radius += majorSpacing, ++ringCount) {
        renderer_->drawCircle(centerX, centerY, radius,
                              fadedMajorColor,
                              gridSettings_.majorStyle.thickness, false);
      }
      if (gridSettings_.showMinor && subdivisions > 1 &&
          minorSpacing * zoom >= 4.0f) {
        int minorRingCount = 0;
        for (float radius = minorSpacing;
             radius < maxRadius && minorRingCount < maxRings * subdivisions;
             radius += minorSpacing, ++minorRingCount) {
          const float majorRemainder = std::fmod(radius, majorSpacing);
          if (majorRemainder < minorSpacing * 0.25f ||
              majorSpacing - majorRemainder < minorSpacing * 0.25f) {
            continue;
          }
          renderer_->drawCircle(centerX, centerY, radius,
                                fadedMinorColor,
                                gridSettings_.minorStyle.thickness, false);
        }
      }
      const int radialCount = 24;
      for (int i = 0; i < radialCount; ++i) {
        const float angle = static_cast<float>(i) * 6.2831853071795864769f /
                            static_cast<float>(radialCount);
            renderer_->drawThickLineLocal(
            {centerX, centerY},
            {centerX + std::cos(angle) * maxRadius,
             centerY + std::sin(angle) * maxRadius},
            gridSettings_.minorStyle.thickness,
            fadedMinorColor);
      }
      if (gridSettings_.showNumbers && majorSpacing * zoom >= 24.0f) {
        const QFont polarLabelFont(QStringLiteral("Segoe UI"), 8);
        const FloatColor polarLabelColor{0.78f, 0.82f, 0.90f,
                                         0.82f * gridFade};
        int labelCount = 0;
        for (float radius = majorSpacing;
             radius <= maxRadius && labelCount < 32;
             radius += majorSpacing, ++labelCount) {
          const auto labelPoint = renderer_->canvasToViewport(
              {centerX + radius, centerY});
          renderer_->drawText(
              QRectF(labelPoint.x + 4.0f, labelPoint.y - 16.0f,
                     72.0f, 14.0f),
              QStringLiteral("r %1").arg(QString::number(
                  radius, 'f', majorSpacing < 1.0f ? 2 : 0)),
              polarLabelFont, polarLabelColor,
              Qt::AlignLeft | Qt::AlignVCenter);
        }
      }
      renderer_->drawCircle(centerX, centerY, 3.0f,
                            gridSettings_.axisColor, 1.0f, true);
    }

    if (gridIsometricMode_) {
      const float centerX = cw * 0.5f;
      const float centerY = ch * 0.5f;
      const float extent = std::hypot(cw, ch);
      const float lineSpacing = std::max(1.0f, majorSpacing);
      const auto drawParallelFamily = [&](float dx, float dy, float spacing,
                                          const FloatColor& color,
                                          float thickness) {
        const float length = std::hypot(dx, dy);
        dx /= length;
        dy /= length;
        const float nx = -dy;
        const float ny = dx;
        const int lineLimit = static_cast<int>(extent / spacing) + 2;
        for (int i = -lineLimit; i <= lineLimit; ++i) {
          const float offset = static_cast<float>(i) * spacing;
          renderer_->drawThickLineLocal(
              {centerX - dx * extent + nx * offset,
               centerY - dy * extent + ny * offset},
              {centerX + dx * extent + nx * offset,
               centerY + dy * extent + ny * offset},
              thickness, color);
        }
      };
      drawParallelFamily(1.0f, 0.57735026919f, lineSpacing,
                         fadedMajorColor,
                         gridSettings_.majorStyle.thickness);
      drawParallelFamily(-1.0f, 0.57735026919f, lineSpacing,
                         fadedMajorColor,
                         gridSettings_.majorStyle.thickness);
      drawParallelFamily(0.0f, 1.0f, lineSpacing,
                         fadedMajorColor,
                         gridSettings_.majorStyle.thickness);
      if (gridSettings_.showMinor && subdivisions > 1 &&
          minorSpacing * zoom >= 4.0f) {
        drawParallelFamily(1.0f, 0.57735026919f, minorSpacing,
                           fadedMinorColor,
                           gridSettings_.minorStyle.thickness);
        drawParallelFamily(-1.0f, 0.57735026919f, minorSpacing,
                           fadedMinorColor,
                           gridSettings_.minorStyle.thickness);
        drawParallelFamily(0.0f, 1.0f, minorSpacing,
                           fadedMinorColor,
                           gridSettings_.minorStyle.thickness);
      }
      if (gridSettings_.showNumbers && lineSpacing * zoom >= 24.0f) {
        const QFont isoLabelFont(QStringLiteral("Segoe UI"), 8);
        const FloatColor isoLabelColor{0.78f, 0.82f, 0.90f,
                                       0.82f * gridFade};
        const auto labelPoint = renderer_->canvasToViewport(
            {centerX + lineSpacing * 2.0f, centerY});
        renderer_->drawText(
            QRectF(labelPoint.x + 4.0f, labelPoint.y - 16.0f, 104.0f, 14.0f),
            QStringLiteral("iso step %1").arg(QString::number(
                lineSpacing, 'f', lineSpacing < 1.0f ? 2 : 0)),
            isoLabelFont, isoLabelColor,
            Qt::AlignLeft | Qt::AlignVCenter);
      }
    }



    if (!gridPolarMode_ && !gridIsometricMode_ &&
        gridSettings_.showMinor && subdivisions > 1 &&

        minorSpacing * zoom >= 4.0f) {

      renderer_->drawGrid(gridOriginX, gridOriginY, gridExtentW, gridExtentH,
                          minorSpacing,

                          gridSettings_.minorStyle.thickness / zoom,

                          fadedMinorColor);

    }

    if (!gridPolarMode_ && !gridIsometricMode_ &&
        gridSettings_.showMajor) {

      renderer_->drawGrid(gridOriginX, gridOriginY, gridExtentW, gridExtentH,
                          majorSpacing,

                          gridSettings_.majorStyle.thickness / zoom,

                          fadedMajorColor);

    }

    if (!gridPolarMode_ && !gridIsometricMode_ &&
        gridSettings_.showAxis) {

      const float axisThickness =

          std::max(1.0f, gridSettings_.axisStyle.thickness) / zoom;

      // Local lines receive canvas coordinates; viewport pan/zoom is applied
      // by the renderer. Match the grid extent without transforming twice.

      renderer_->drawThickLineLocal(

          {0.0f, gridOriginY}, {0.0f, gridOriginY + gridExtentH},
          axisThickness, fadedAxisColor);

      renderer_->drawThickLineLocal(

          {gridOriginX, 0.0f}, {gridOriginX + gridExtentW, 0.0f},
          axisThickness, fadedAxisColor);

    }

    if (!gridPolarMode_ && !gridIsometricMode_ &&
        gridSettings_.showNumbers && majorSpacing * zoom >= 24.0f) {
      const QFont gridLabelFont(QStringLiteral("Segoe UI"), 8);
      const FloatColor gridLabelColor = {0.78f, 0.82f, 0.90f,
                                         0.82f * gridFade};
      const int maxLabels = 48;
      int labelCount = 0;
      const float labelStartX =
          std::floor(gridOriginX / majorSpacing) * majorSpacing;
      for (float x = labelStartX;
           x <= gridOriginX + gridExtentW && labelCount < maxLabels;
           x += majorSpacing, ++labelCount) {
        const auto point = renderer_->canvasToViewport({x, 0.0f});
        renderer_->drawText(
            QRectF(point.x + 3.0f, point.y + 3.0f, 64.0f, 14.0f),
            QString::number(x, 'f', majorSpacing < 1.0f ? 2 : 0),
            gridLabelFont, gridLabelColor,
            Qt::AlignLeft | Qt::AlignVCenter);
      }
      labelCount = 0;
      const float labelStartY =
          std::floor(gridOriginY / majorSpacing) * majorSpacing;
      for (float y = labelStartY;
           y <= gridOriginY + gridExtentH && labelCount < maxLabels;
           y += majorSpacing, ++labelCount) {
        const auto point = renderer_->canvasToViewport({0.0f, y});
        renderer_->drawText(
            QRectF(point.x + 3.0f, point.y + 3.0f, 64.0f, 14.0f),
            QString::number(y, 'f', majorSpacing < 1.0f ? 2 : 0),
            gridLabelFont, gridLabelColor,
            Qt::AlignLeft | Qt::AlignVCenter);
      }
    }

  }

  if (showViewportRuler_ && !threeDimensionalViewport) {
    const float safeZoom = std::max(0.0001f, zoom);
    float panX = 0.0f;
    float panY = 0.0f;
    renderer_->getPan(panX, panY);
    const QPointF viewportOrigin(-panX / safeZoom, -panY / safeZoom);
    const QSizeF viewportSize(
        std::max(1.0f, hostWidth_) / safeZoom,
        std::max(1.0f, hostHeight_) / safeZoom);
    const bool rulerCacheDirty =
        std::abs(viewportRulerCacheZoom_ - safeZoom) > 0.0001f ||
        std::abs(viewportRulerCachePanX_ - panX) > 0.01f ||
        std::abs(viewportRulerCachePanY_ - panY) > 0.01f ||
        std::abs(viewportRulerCacheWidth_ - hostWidth_) > 0.5f ||
        std::abs(viewportRulerCacheHeight_ - hostHeight_) > 0.5f ||
        std::abs(viewportRulerCacheCanvasWidth_ - cw) > 0.5f ||
        std::abs(viewportRulerCacheCanvasHeight_ - ch) > 0.5f;
    if (rulerCacheDirty) {
      viewportRulerHorizontalTicks_ = ViewportRulerData::generateTicks(
          safeZoom, viewportOrigin, viewportSize, QSizeF(cw, ch), true,
          100.0f, QStringLiteral("px"));
      viewportRulerVerticalTicks_ = ViewportRulerData::generateTicks(
          safeZoom, viewportOrigin, viewportSize, QSizeF(cw, ch), false,
          100.0f, QStringLiteral("px"));
      viewportRulerCacheZoom_ = safeZoom;
      viewportRulerCachePanX_ = panX;
      viewportRulerCachePanY_ = panY;
      viewportRulerCacheWidth_ = hostWidth_;
      viewportRulerCacheHeight_ = hostHeight_;
      viewportRulerCacheCanvasWidth_ = cw;
      viewportRulerCacheCanvasHeight_ = ch;
    }
    const FloatColor tickColor = {0.65f, 0.70f, 0.78f, 0.72f};
    const FloatColor labelColor = {0.86f, 0.89f, 0.95f, 0.9f};
    const QFont rulerFont(QStringLiteral("Segoe UI"), 11);
    // The ruler belongs to the viewport edge, not to the transformed canvas
    // edge. This keeps it visible while the canvas is panned or zoomed out.
    const float left = 12.0f;
    const float right = std::max(1.0f, hostWidth_ - 12.0f);
    const float top = 12.0f;
    const float bottom = std::max(1.0f, hostHeight_ - 12.0f);
    for (const auto& tick : viewportRulerHorizontalTicks_) {
      const auto point = renderer_->canvasToViewport({tick.canvasPos, ch});
      const float length = tick.level == ViewportRulerTickLevel::Major
                               ? 20.0f
                               : tick.level == ViewportRulerTickLevel::Minor ? 12.0f
                                                                             : 6.0f;
      renderer_->drawSolidLine({point.x, top}, {point.x, top + length},
                               tickColor, tick.level == ViewportRulerTickLevel::Major ? 2.25f : 1.5f);
      if (!tick.label.isEmpty()) {
        renderer_->drawText(QRectF(point.x + 4.0f, top + 22.0f, 100.0f, 20.0f),
                            tick.label, rulerFont, labelColor,
                            Qt::AlignLeft | Qt::AlignVCenter);
      }
    }
    for (const auto& tick : viewportRulerVerticalTicks_) {
      const auto point = renderer_->canvasToViewport({0.0f, tick.canvasPos});
      const float length = tick.level == ViewportRulerTickLevel::Major
                               ? 20.0f
                               : tick.level == ViewportRulerTickLevel::Minor ? 12.0f
                                                                             : 6.0f;
      renderer_->drawSolidLine({right, point.y}, {right - length, point.y},
                               tickColor, tick.level == ViewportRulerTickLevel::Major ? 2.25f : 1.5f);
      if (!tick.label.isEmpty()) {
        renderer_->drawText(QRectF(right - 116.0f, point.y + 2.0f, 104.0f, 20.0f),
                            tick.label, rulerFont, labelColor,
                            Qt::AlignLeft | Qt::AlignVCenter);
      }
    }
    const auto scaleBar = ViewportScaleBarDataFactory::generate(
        safeZoom, QSizeF(std::max(1.0f, right - left), std::max(1.0f, bottom - top)),
        140.0f);
    const float scaleBarX = right - scaleBar.barWidthPx - 96.0f;
    renderer_->drawSolidLine({scaleBarX, bottom},
                             {scaleBarX + scaleBar.barWidthPx, bottom},
                             tickColor, 4.0f);
    renderer_->drawText(QRectF(scaleBarX,
                               bottom - 33.0f, 116.0f, 20.0f),
                        scaleBar.label, rulerFont, labelColor,
                        Qt::AlignLeft | Qt::AlignVCenter);
  }

  if (showViewportRuler_ && threeDimensionalViewport) {
    const float pixelsPerWorldUnit = projectedWorldPixelsPerUnit();
    const float right = std::max(1.0f, hostWidth_ - 20.0f);
    const float bottom = std::max(1.0f, hostHeight_ - 20.0f);
    const auto scaleBar = ViewportScaleBarDataFactory::generate(
        pixelsPerWorldUnit,
        QSizeF(std::max(1.0f, hostWidth_), std::max(1.0f, hostHeight_)),
        140.0f, QStringLiteral("units"));
    const float scaleBarX = right - scaleBar.barWidthPx;
    const FloatColor worldTickColor{0.68f, 0.76f, 0.88f, 0.88f};
    const FloatColor worldLabelColor{0.90f, 0.93f, 0.98f, 0.96f};
    const QFont worldScaleFont(QStringLiteral("Segoe UI"), 11);
    renderer_->drawSolidLine({scaleBarX, bottom}, {right, bottom},
                             worldTickColor, 4.0f);
    renderer_->drawSolidLine({scaleBarX, bottom - 7.0f},
                             {scaleBarX, bottom + 7.0f},
                             worldTickColor, 2.0f);
    renderer_->drawSolidLine({right, bottom - 7.0f},
                             {right, bottom + 7.0f},
                             worldTickColor, 2.0f);
    renderer_->drawText(
        QRectF(scaleBarX, bottom - 33.0f,
               std::max(140.0f, scaleBar.barWidthPx), 20.0f),
        scaleBar.label, worldScaleFont, worldLabelColor,
        Qt::AlignLeft | Qt::AlignVCenter);
  }



  ViewportOverlay::drawSafeAreaAndOrigin(
      renderer_.get(), cw, ch, showSafeMargins_, showOriginOverlay_);
  // P1-14: display-only aspect mask, applied in canvas space so pan / zoom /
  // canvas rotation keep the mask aligned with the composition.
  {
    const float aspectMaskRatio = [this]() -> float {
      switch (viewportAspectMask_) {
      case CompositionViewportAspectMask::Square:
        return 1.0f;
      case CompositionViewportAspectMask::FourByThree:
        return 4.0f / 3.0f;
      case CompositionViewportAspectMask::SixteenByNine:
        return 16.0f / 9.0f;
      case CompositionViewportAspectMask::Cinemascope:
        return 2.39f;
      case CompositionViewportAspectMask::Off:
      default:
        return 0.0f;
      }
    }();
    ViewportOverlay::drawAspectMask(renderer_.get(), cw, ch, aspectMaskRatio);
  }
  ViewportOverlay::drawPixelGrid(renderer_.get(), cw, ch, zoom,
                                 showPixelGrid_);
  ViewportOverlay::drawCompositionBoundary(renderer_.get(), cw, ch,
                                           showOutsideComposition_);

}



void CompositionRenderController::Impl::drawViewportInteractionOverlay(

    const HostFrameSnapshot &hostSnapshot, const FramePosition &currentFrame,

    float cw, float ch) {

  if (renderer_ && fitGuideVisible_ && fitGuideCompRect_.isValid() &&
      fitGuideResultRect_.isValid()) {
    const auto drawRectOutline = [this](const QRectF &rect,
                                         const FloatColor &color,
                                         float width) {
      const float l = static_cast<float>(rect.left());
      const float t = static_cast<float>(rect.top());
      const float r = static_cast<float>(rect.right());
      const float b = static_cast<float>(rect.bottom());
      renderer_->drawSolidLine({l, t}, {r, t}, color, width);
      renderer_->drawSolidLine({r, t}, {r, b}, color, width);
      renderer_->drawSolidLine({r, b}, {l, b}, color, width);
      renderer_->drawSolidLine({l, b}, {l, t}, color, width);
    };
    const float lineWidth = 1.25f / std::max(0.001f, renderer_->getZoom());
    drawRectOutline(fitGuideCompRect_, {0.70f, 0.78f, 0.90f, 0.72f}, lineWidth);
    drawRectOutline(fitGuideResultRect_, {0.25f, 0.90f, 0.98f, 0.95f}, lineWidth);
  }

  if (renderer_ && maskSnapPreviewValid_ && pendingMaskCreation_) {
    const float markerSize = std::clamp(
        12.0f / std::max(0.001f, renderer_->getZoom()), 6.0f, 20.0f);
    renderer_->drawCrosshair(
        maskSnapPreviewCanvasPos_.x, maskSnapPreviewCanvasPos_.y, markerSize,
        FloatColor{0.30f, 0.92f, 1.0f, 0.95f});
  }

  if (renderer_ && isShapeVertexMarqueeSelecting_) {
    const QRectF rect = shapeVertexMarqueeCanvasRect().normalized();
    if (rect.isValid() && rect.width() > 0.0f && rect.height() > 0.0f) {
      renderer_->drawSolidRect(
          static_cast<float>(rect.left()), static_cast<float>(rect.top()),
          static_cast<float>(rect.width()), static_cast<float>(rect.height()),
          {0.20f, 0.92f, 0.72f, 0.12f}, 1.0f);
      drawTaggedRectOutline(renderer_.get(), rect,
                            {0.20f, 0.95f, 0.78f, 0.95f}, false);
    }
  }

  if (renderer_ && isRubberBandSelecting_) {

    if (isLassoSelecting_ && lassoViewportPoints_.size() >= 2) {
      std::vector<Detail::float2> outline;
      outline.reserve(lassoViewportPoints_.size() + 1);
      for (const auto &point : lassoViewportPoints_) {
        const auto canvas = renderer_->viewportToCanvas(
            {point.x, point.y});
        outline.emplace_back(canvas.x, canvas.y);
      }
      const auto first = outline.front();
      outline.push_back(first);
      renderer_->drawPolyline(
          outline, {0.35f, 0.78f, 1.0f, 0.95f},
          1.6f / std::max(0.001f, renderer_->getZoom()));
      return;
    }

    const QRectF rubberBandRect = rubberBandCanvasRect().normalized();

    if (rubberBandRect.isValid() && rubberBandRect.width() > 0.0f &&

        rubberBandRect.height() > 0.0f) {

      renderer_->drawSolidRect(static_cast<float>(rubberBandRect.left()),

                               static_cast<float>(rubberBandRect.top()),

                               static_cast<float>(rubberBandRect.width()),

                               static_cast<float>(rubberBandRect.height()),

                               {0.25f, 0.55f, 1.0f, 0.14f}, 1.0f);

      const bool showSelectionRect = hostSnapshot.showSelectionRectLines;

      drawTaggedRectOutline(renderer_.get(), rubberBandRect,

                            {0.25f, 0.70f, 1.0f, 0.95f}, showSelectionRect);

    }

  }



  if (renderer_ && brushCursorVisible_) {
    const auto cursorViewport = renderer_->canvasToViewport(
        {brushCursorCanvasPos_.x, brushCursorCanvasPos_.y});
    const auto *brushTool = ArtifactApplicationManager::instance()
                                ? ArtifactApplicationManager::instance()->brushTool()
                                : nullptr;
    const float pressure = brushTool ? std::clamp(brushTool->pressure(), 0.0f,
                                                  1.0f)
                                     : 1.0f;
    const float pressureSize =
        brushTool && brushTool->pressureAffectsSize()
            ? std::max(0.05f, pressure)
            : 1.0f;
    const float effectiveFlow =
        brushTool
            ? brushTool->flow() *
                  (brushTool->pressureAffectsFlow() ? pressure : 1.0f)
            : 1.0f;
    const float radius = std::max(2.0f,
                                  (brushTool ? brushTool->radius() : 10.0f) *
                                      pressureSize *
                                      std::max(0.001f, renderer_->getZoom()));
    const float previewAlpha =
        brushTool && brushTool->pressureAffectsOpacity()
            ? std::clamp(0.18f + pressure * 0.72f, 0.18f, 0.90f)
            : 0.90f;
    const float effectivePreviewAlpha =
        std::clamp(previewAlpha * (0.35f + effectiveFlow * 0.65f), 0.08f,
                   0.90f);
    const FloatColor cursorColor =
        brushTool && brushTool->eraserMode()
            ? FloatColor{1.0f, 0.55f, 0.28f, effectivePreviewAlpha}
            : (brushTool
                   ? FloatColor{brushTool->color().r(), brushTool->color().g(),
                                brushTool->color().b(), effectivePreviewAlpha}
                   : FloatColor{0.35f, 0.85f, 1.0f, 0.9f});
    const float tiltMagnitude = brushTool
                                    ? std::min(60.0f, std::hypot(
                                          brushTool->tiltX(), brushTool->tiltY()))
                                    : 0.0f;
    const float tiltAngle =
        (brushTool && brushTool->tiltAffectsAngle() && tiltMagnitude > 0.01f)
            ? std::atan2(brushTool->tiltY(), brushTool->tiltX())
            : 0.0f;
    const float roundness =
        (brushTool ? brushTool->roundness() : 1.0f) *
        (brushTool && brushTool->tiltAffectsRoundness()
             ? std::max(0.25f, 1.0f - (tiltMagnitude / 60.0f) * 0.5f)
             : 1.0f);
    const float angle =
        ((brushTool ? brushTool->angle() : 0.0f) *
             0.017453292519943295f +
         tiltAngle);
    const float cosAngle = std::cos(angle);
    const float sinAngle = std::sin(angle);
    std::vector<Detail::float2> outline;
    outline.reserve(33);
    for (int i = 0; i <= 32; ++i) {
      const float theta = 6.283185307179586f * static_cast<float>(i) / 32.0f;
      const float localX = std::cos(theta) * radius;
      const float localY = std::sin(theta) * radius * roundness;
      const QPointF viewportPoint(
          cursorViewport.x + localX * cosAngle - localY * sinAngle,
          cursorViewport.y + localX * sinAngle + localY * cosAngle);
      const auto canvasPoint = renderer_->viewportToCanvas(
          {static_cast<float>(viewportPoint.x()),
           static_cast<float>(viewportPoint.y())});
      outline.emplace_back(canvasPoint.x, canvasPoint.y);
    }
    renderer_->drawPolyline(outline, cursorColor, 1.4f /
                                                   std::max(0.001f, renderer_->getZoom()));
    if (brushTool && brushTool->scatter() > 0.001f) {
      const FloatColor scatterColor{
          cursorColor.r(), cursorColor.g(), cursorColor.b(), 0.34f};
      renderer_->drawCircle(
          brushCursorCanvasPos_.x,
          brushCursorCanvasPos_.y,
          brushTool->radius() * (1.0f + brushTool->scatter()), scatterColor,
          0.9f / std::max(0.001f, renderer_->getZoom()), false);
    }

    if (brushTool) {
      const QFont brushHudFont(QStringLiteral("sans-serif"), 8);
      const QString eraserMode =
          brushTool->eraserMode()
              ? (brushTool->eraserModeKind() == 1
                     ? QStringLiteral(" Layer")
                     : (brushTool->eraserModeKind() == 2
                            ? QStringLiteral(" Last")
                            : QStringLiteral(" Paint")))
              : QString();
      const QString brushHud =
          QStringLiteral("%1px F%2 O%3 H%4 R%5 A%6 S%7%8")
              .arg(QString::number(brushTool->radius(), 'f', 1))
              .arg(QString::number(brushTool->flow() * 100.0f, 'f', 0))
              .arg(QString::number(brushTool->opacity() * 100.0f, 'f', 0))
              .arg(QString::number(brushTool->hardness() * 100.0f, 'f', 0))
              .arg(QString::number(brushTool->roundness() * 100.0f, 'f', 0))
              .arg(QString::number(brushTool->angle(), 'f', 0))
              .arg(QString::number(brushTool->spacing() * 100.0f, 'f', 0))
              .arg(eraserMode);
      const QString dynamicsHud =
          QStringLiteral("P%1 EffF%2 FJ%3  J%4/%5/%6/%7  Sc%8  T%9,%10")
              .arg(QString::number(pressure * 100.0f, 'f', 0))
              .arg(QString::number(effectiveFlow * 100.0f, 'f', 0))
              .arg(QString::number(brushTool->flowJitter() * 100.0f, 'f', 0))
              .arg(QString::number(brushTool->sizeJitter() * 100.0f, 'f', 0))
              .arg(QString::number(brushTool->opacityJitter() * 100.0f, 'f', 0))
              .arg(QString::number(brushTool->angleJitter() * 100.0f, 'f', 0))
              .arg(QString::number(brushTool->roundnessJitter() * 100.0f, 'f', 0))
              .arg(QString::number(brushTool->scatter() * 100.0f, 'f', 0))
              .arg(QString::number(brushTool->tiltX(), 'f', 0))
              .arg(QString::number(brushTool->tiltY(), 'f', 0));
      const float hudX = brushCursorCanvasPos_.x +
                         10.0f / std::max(0.001f, renderer_->getZoom());
      const float hudY = brushCursorCanvasPos_.y -
                         22.0f / std::max(0.001f, renderer_->getZoom());
      const float hudWidth = eraserMode.isEmpty() ? 184.0f : 224.0f;
      renderer_->drawSolidRect(hudX, hudY, hudWidth, 28.0f,
                               {0.0f, 0.0f, 0.0f, 0.64f}, 0.8f);
      renderer_->drawText(QRectF(hudX + 3.0f, hudY,
                                 hudWidth - 6.0f, 14.0f),
                          brushHud, brushHudFont,
                          {0.9f, 0.96f, 1.0f, 0.92f},
                          Qt::AlignLeft | Qt::AlignVCenter);
      renderer_->drawText(QRectF(hudX + 3.0f, hudY + 13.0f,
                                 hudWidth - 6.0f, 14.0f),
                          dynamicsHud, brushHudFont,
                          {0.76f, 0.88f, 0.96f, 0.90f},
                          Qt::AlignLeft | Qt::AlignVCenter);
    }

    if (brushTool && brushTool->isDragging() &&
        brushTool->currentStrokePoints().size() >= 2) {
      std::vector<Detail::float2> strokePreview;
      strokePreview.reserve(brushTool->currentStrokePoints().size());
      for (const QPointF &point : brushTool->currentStrokePoints()) {
        strokePreview.emplace_back(static_cast<float>(point.x()),
                                   static_cast<float>(point.y()));
      }
      const FloatColor previewColor =
          brushTool->eraserMode()
              ? FloatColor{1.0f, 0.45f, 0.20f,
                           std::clamp(
                               (brushTool->pressureAffectsOpacity()
                                    ? (0.16f + pressure * 0.54f)
                                    : 0.60f) *
                                   (0.35f + effectiveFlow * 0.65f),
                               0.06f, 0.70f)}
              : FloatColor{brushTool->color().r(), brushTool->color().g(),
                           brushTool->color().b(),
                           std::clamp(
                               (brushTool->pressureAffectsOpacity()
                                    ? (0.16f + pressure * 0.54f)
                                    : 0.60f) *
                                   (0.35f + effectiveFlow * 0.65f),
                               0.06f, 0.70f)};
      renderer_->drawPolyline(
          strokePreview, previewColor,
          std::max(1.0f, brushTool->radius() * 0.18f));
    }
  }

  if (renderer_ && cloneStampSourceSet_) {
    if (hostSnapshot.activeToolKind == CompositionHostToolKind::Clone) {
      const float sourceCrossSize =
          std::clamp(12.0f / std::max(0.001f, renderer_->getZoom()), 6.0f, 28.0f);
      renderer_->drawCrosshair(
          cloneStampSourceCanvas_.x,
          cloneStampSourceCanvas_.y, sourceCrossSize,
          {0.32f, 0.92f, 1.0f, 0.95f});
      const QFont sourceFont(QStringLiteral("sans-serif"), 8);
      renderer_->drawText(
          QRectF(cloneStampSourceCanvas_.x + sourceCrossSize + 4.0f,
                 cloneStampSourceCanvas_.y - 10.0f, 52.0f, 16.0f),
          QStringLiteral("Source"), sourceFont, {0.45f, 0.95f, 1.0f, 0.95f},
          Qt::AlignLeft | Qt::AlignVCenter);
    }
  }

  if (renderer_ && rectangleToolDragging_) {

    const QRectF rect =

        dragRectFromPoints(rectangleToolStartCanvasPos_,

                           rectangleToolCurrentCanvasPos_,
                           rectangleToolFromCenter_)

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

      const bool shapePreview =
          rectangleToolMode_ == RectangleToolMode::Shape ||
          rectangleToolMode_ == RectangleToolMode::EllipseShape ||
          rectangleToolMode_ == RectangleToolMode::StarShape ||
          rectangleToolMode_ == RectangleToolMode::PolygonShape ||
          rectangleToolMode_ == RectangleToolMode::TriangleShape;
      if (shapePreview && rectangleToolRoundness_ > 0.0f) {
        renderer_->drawRoundedPanel(
            static_cast<float>(rect.left()), static_cast<float>(rect.top()),
            static_cast<float>(rect.width()), static_cast<float>(rect.height()),
            rectangleToolRoundness_, fillColor, outlineColor, 1.0f, 1.6f);
      } else {
        renderer_->drawSolidRect(static_cast<float>(rect.left()),
                                 static_cast<float>(rect.top()),
                                 static_cast<float>(rect.width()),
                                 static_cast<float>(rect.height()), fillColor,
                                 1.0f);
        drawRectOutline(renderer_.get(), rect, outlineColor, 1.6f);
      }
      QString shapeTypeName = QStringLiteral("Rectangle");
      if (rectangleToolMode_ == RectangleToolMode::Mask) {
        shapeTypeName = QStringLiteral("Mask");
      } else if (rectangleToolMode_ == RectangleToolMode::EllipseMask) {
        shapeTypeName = QStringLiteral("Ellipse Mask");
      } else {
        switch (rectangleToolShapeType_) {
        case ShapeType::Rect: shapeTypeName = QStringLiteral("Rectangle"); break;
        case ShapeType::Ellipse: shapeTypeName = QStringLiteral("Ellipse"); break;
        case ShapeType::Star: shapeTypeName = QStringLiteral("Star"); break;
        case ShapeType::Polygon: shapeTypeName = QStringLiteral("Polygon"); break;
        case ShapeType::Line: shapeTypeName = QStringLiteral("Line"); break;
        case ShapeType::Triangle: shapeTypeName = QStringLiteral("Triangle"); break;
        case ShapeType::Square: shapeTypeName = QStringLiteral("Square"); break;
        case ShapeType::Arrow: shapeTypeName = QStringLiteral("Arrow"); break;
        case ShapeType::Heart: shapeTypeName = QStringLiteral("Heart"); break;
        case ShapeType::Diamond: shapeTypeName = QStringLiteral("Diamond"); break;
        case ShapeType::Gear: shapeTypeName = QStringLiteral("Gear"); break;
        case ShapeType::Cross: shapeTypeName = QStringLiteral("Cross"); break;
        }
      }

      const QString sizeLabel = (rectangleToolRoundness_ > 0.0f && rectangleToolMode_ == RectangleToolMode::Shape)
          ? QStringLiteral("%1 x %2  R %3")
                .arg(QString::number(rect.width(), 'f', 1),
                     QString::number(rect.height(), 'f', 1),
                     QString::number(rectangleToolRoundness_, 'f', 1))
          : QStringLiteral("%1  %2 x %3")
                .arg(shapeTypeName,
                     QString::number(rect.width(), 'f', 1),
                     QString::number(rect.height(), 'f', 1));
      const QFont sizeFont(QStringLiteral("sans-serif"), 9);
      const float labelX = static_cast<float>(rect.left());
      const float labelY = static_cast<float>(rect.top()) - 18.0f;
      renderer_->drawSolidRect(labelX, labelY, 124.0f, 16.0f,
                               {0.0f, 0.0f, 0.0f, 0.72f}, 0.9f);
      renderer_->drawText(QRectF(labelX + 4.0f, labelY, 116.0f, 16.0f),
                          sizeLabel, sizeFont,
                          {1.0f, 1.0f, 1.0f, 0.95f},
                          Qt::AlignLeft | Qt::AlignVCenter);

    }

  }

  if (renderer_ && gizmo3D_ && gizmoDragActive_ &&
      (projectedFrameMove_ ||
       projectedFrameHandle_ != TransformGizmo::HandleType::None ||
       gizmo3D_->isDragging())) {
    const auto comp = previewPipeline_.composition();
    const auto layer = comp && !selectedLayerId_.isNil()
                           ? comp->layerById(selectedLayerId_)
                           : ArtifactAbstractLayerPtr{};
    if (layer) {
      const QRectF bounds = layer->localBounds();
      const auto scale = gizmo3D_->scale();
      const auto position = gizmo3D_->position();
      const auto rotation = gizmo3D_->rotation();
      const auto positionDelta = position - gizmoUndoWorldPivotBefore_;
      const ArtifactCore::Units::EulerDegrees3 rotationDelta{
          rotation.x - gizmoUndoBefore_.rotation.x,
          rotation.y - gizmoUndoBefore_.rotation.y,
          rotation.z - gizmoUndoBefore_.rotation.z};
      const auto safeRatio = [](float value, float before) {
        return ArtifactCore::Units::ScaleFactor{
            value / (std::abs(before) > 0.0001f ? before : 0.0001f)};
      };
      const ArtifactCore::Units::Scale3 scaleRatio{
          safeRatio(scale.x, gizmoUndoBefore_.scale.x).value,
          safeRatio(scale.y, gizmoUndoBefore_.scale.y).value,
          safeRatio(scale.z, gizmoUndoBefore_.scale.z).value};
      const QString gizmoModeLabel =
          gizmo3D_->activeOperation() == GizmoOperation::Rotate
              ? QStringLiteral("ROTATE")
              : (gizmo3D_->activeOperation() == GizmoOperation::Scale
                     ? QStringLiteral("SCALE")
                     : (gizmo3D_->activeOperation() == GizmoOperation::Translate
                            ? QStringLiteral("MOVE")
                            : (gizmo3D_->mode() == GizmoMode::Rotate
                                   ? QStringLiteral("ROTATE")
                                   : (gizmo3D_->mode() == GizmoMode::Scale
                                          ? QStringLiteral("SCALE")
                                          : QStringLiteral("MOVE")))));
      const QString baseGizmoAxisLabel =
          gizmo3D_->activeAxis() == GizmoAxis::X
              ? QStringLiteral("X")
              : (gizmo3D_->activeAxis() == GizmoAxis::Y
                     ? QStringLiteral("Y")
                     : (gizmo3D_->activeAxis() == GizmoAxis::Z
                            ? QStringLiteral("Z")
                            : (gizmo3D_->activeAxis() == GizmoAxis::XY
                                   ? QStringLiteral("XY")
                                   : (gizmo3D_->activeAxis() == GizmoAxis::YZ
                                          ? QStringLiteral("YZ")
                                          : (gizmo3D_->activeAxis() == GizmoAxis::XZ
                                                 ? QStringLiteral("XZ")
                                                 : (gizmo3D_->activeAxis() == GizmoAxis::Screen
                                                        ? QStringLiteral("SCREEN")
                                                        : QString()))))));
      const QString gizmoAxisLabel = gizmo3D_->isBoundingBoxDragging()
          ? (gizmo3D_->activeAxis() == GizmoAxis::Screen
                 ? QStringLiteral("BB CORNER")
                 : (gizmo3D_->activeAxis() == GizmoAxis::XY
                        ? QStringLiteral("BB EDGE XY")
                        : (gizmo3D_->activeAxis() == GizmoAxis::YZ
                               ? QStringLiteral("BB EDGE YZ")
                               : (gizmo3D_->activeAxis() == GizmoAxis::XZ
                                      ? QStringLiteral("BB EDGE XZ")
                                      : QStringLiteral("BB FACE ") +
                                            baseGizmoAxisLabel))))
          : baseGizmoAxisLabel;
      QString detail =
          QStringLiteral("W %1  H %2\nX %3  Y %4  Z %5\n"
                         "dX %6  dY %7  dZ %8\n"
                         "S %9/%10  x %11/%12  RZ %13  dR %14  %15")
              .arg(QString::number(bounds.width() * std::abs(scale.x), 'f', 1))
              .arg(QString::number(bounds.height() * std::abs(scale.y), 'f', 1))
              .arg(QString::number(position.x, 'f', 1))
              .arg(QString::number(position.y, 'f', 1))
              .arg(QString::number(position.z, 'f', 1))
              .arg(QString::number(positionDelta.x, 'f', 1))
              .arg(QString::number(positionDelta.y, 'f', 1))
              .arg(QString::number(positionDelta.z, 'f', 1))
              .arg(QString::number(scale.x, 'f', 2))
              .arg(QString::number(scale.y, 'f', 2))
              .arg(QString::number(scaleRatio.x, 'f', 2))
              .arg(QString::number(scaleRatio.y, 'f', 2))
              .arg(QString::number(rotation.z.value, 'f', 1))
              .arg(QString::number(rotationDelta.z.value, 'f', 1))
              .arg(projectedFrameMove_
                       ? QStringLiteral("MOVE")
                       : (projectedFrameHandle_ != TransformGizmo::HandleType::None
                              ? QStringLiteral("RESIZE")
                              : (gizmoAxisLabel.isEmpty()
                                     ? gizmoModeLabel
                                     : gizmoModeLabel + QLatin1Char(' ') + gizmoAxisLabel)));
      if (const auto *textLayer =
              dynamic_cast<const ArtifactTextLayer *>(layer.get())) {
        const float textBoxWidth = textLayer->maxWidth();
        const float textBoxHeight = textLayer->boxHeight();
        if (textBoxWidth > 0.0f && textBoxHeight > 0.0f) {
          detail += QStringLiteral("\nTextBox %1 x %2")
                        .arg(QString::number(textBoxWidth, 'f', 1),
                             QString::number(textBoxHeight, 'f', 1));
        }
      }
      const QFont hudFont(QStringLiteral("sans-serif"), 9);
      const QFontMetrics hudMetrics(hudFont);
      const int hudLines = std::max(
          1, static_cast<int>(detail.count(QLatin1Char('\n'))) + 1);
      const float hudHeight = 12.0f + static_cast<float>(hudLines) * 16.0f;
      int longestLineWidth = 0;
      for (const QString &line : detail.split(QLatin1Char('\n'))) {
        longestLineWidth = std::max(longestLineWidth,
                                    hudMetrics.horizontalAdvance(line));
      }
      const float panelWidth = std::clamp(
          static_cast<float>(longestLineWidth + 24), 238.0f, 360.0f);
      const float viewportWidth = std::max(1.0f, hostWidth_);
      const float viewportHeight = std::max(1.0f, hostHeight_);
      float panelX = 8.0f;
      float panelY = 8.0f;
      if (projectedFrameLastPointerValid_) {
        panelX = std::clamp(
            projectedFrameLastPointer_.x + 18.0f,
            8.0f, std::max(8.0f, viewportWidth - panelWidth - 8.0f));
        panelY = std::clamp(
            projectedFrameLastPointer_.y + 18.0f,
            8.0f, std::max(8.0f, viewportHeight - hudHeight - 8.0f));
      } else if (gizmo3DCameraMatricesValid_) {
        const QVector3D worldCenter = layer->getGlobalTransform4x4().map(
            QVector3D(static_cast<float>(bounds.center().x()),
                      static_cast<float>(bounds.center().y()), 0.0f));
        const QVector3D projectedCenter = ViewportMath::projectToTopDown(
            worldCenter, gizmo3DViewMatrix_, gizmo3DProjectionMatrix_,
            QRect(0, 0, static_cast<int>(viewportWidth),
                  static_cast<int>(viewportHeight)));
        if (std::isfinite(projectedCenter.x()) &&
            std::isfinite(projectedCenter.y())) {
          panelX = std::clamp(projectedCenter.x() + 16.0f, 8.0f,
                              std::max(8.0f, viewportWidth - panelWidth - 8.0f));
          panelY = std::clamp(projectedCenter.y() + 16.0f, 8.0f,
                              std::max(8.0f, viewportHeight - hudHeight - 8.0f));
        }
      }
      const QRectF hudRect(panelX, panelY, panelWidth, hudHeight);
      renderer_->drawSolidRect(static_cast<float>(hudRect.left()),
                               static_cast<float>(hudRect.top()),
                               static_cast<float>(hudRect.width()),
                               static_cast<float>(hudRect.height()),
                               {0.01f, 0.02f, 0.04f, 0.82f}, 0.9f);
      renderer_->drawText(hudRect.adjusted(8.0, 3.0, -6.0, -3.0), detail,
                          hudFont, {0.78f, 0.92f, 1.0f, 0.96f},
                          Qt::AlignLeft | Qt::AlignVCenter);
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

    const HostFrameSnapshot &hostSnapshot, const ArtifactCompositionPtr &comp,

    const QList<ArtifactAbstractLayerPtr> &layers,

    const QStringList &selectedIds, const FramePosition &currentFrame, float cw,

    float ch, bool has3DCamera, const QMatrix4x4 &cameraViewMatrix,

    const QMatrix4x4 &cameraProjMatrix) {

  if (renderer_) {

    renderer_->setUseExternalMatrices(false);

    renderer_->resetGizmoCameraMatrices();

    renderer_->reset3DCameraMatrices();

    renderer_->setCanvasSize(cw, ch);

  }

  // With no active 3D camera, the renderer's generic view/projection matrices
  // are not a canvas-space camera.  Use the same orthographic matrices as the
  // 3D axis gizmo so a newly added plane gets its projected frame immediately.
  QMatrix4x4 fallbackFrameView;
  QMatrix4x4 fallbackFrameProjection;
  const auto frameLayer = comp && !selectedLayerId_.isNil()
      ? comp->layerById(selectedLayerId_) : ArtifactAbstractLayerPtr{};
  // Ordinary 2D cards are composited with canvas pan/zoom, not the scene's
  // perspective camera. View-cube mode explicitly projects both together.
  if (!viewportOrientationMatricesValid_ && frameLayer &&
      !frameLayer->is3D() && layerUsesProjectedFrameGizmo(frameLayer)) {
    has3DCamera = false;
    gizmo3DCameraMatricesValid_ = false;
  }
  if (renderer_ && !viewportOrientationMatricesValid_ && !has3DCamera) {
    float panX = 0.0f;
    float panY = 0.0f;
    renderer_->getPan(panX, panY);
    const float zoom = std::max(0.001f, renderer_->getZoom());
    const float viewportW = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
    const float viewportH = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
    // Use the renderer's actual canvas mapping, including viewport centering
    // and rotation, instead of reconstructing it from pan/zoom alone.
    const auto origin = renderer_->canvasToViewport({0.0f, 0.0f});
    const auto xUnit = renderer_->canvasToViewport({1.0f, 0.0f});
    const auto yUnit = renderer_->canvasToViewport({0.0f, 1.0f});
    fallbackFrameView(0, 0) = xUnit.x - origin.x;
    fallbackFrameView(1, 0) = xUnit.y - origin.y;
    fallbackFrameView(0, 1) = yUnit.x - origin.x;
    fallbackFrameView(1, 1) = yUnit.y - origin.y;
    fallbackFrameView(0, 3) = origin.x;
    fallbackFrameView(1, 3) = origin.y;
    fallbackFrameProjection.ortho(0.0f, std::max(1.0f, viewportW),
                                  std::max(1.0f, viewportH), 0.0f,
                                  -1000.0f, 1000.0f);
  }



  // The projected-frame/3D gizmo has its own lifetime.  Do not make its
  // first draw depend on the legacy 2D gizmo having been initialized yet.
  if (showGizmoOverlay_ && (gizmo_ || gizmo3D_)) {

    ArtifactCore::ProfileScope _profGizmo("GizmoMask",

                                          ArtifactCore::ProfileCategory::Render);

    auto selectedLayer = (!selectedLayerId_.isNil() && comp)

                             ? comp->layerById(selectedLayerId_)

                             : ArtifactAbstractLayerPtr{};

    if (selectedLayer && isLayerEffectivelyVisible(selectedLayer)) {

      if (gizmo_) {
        gizmo_->setMode(gizmoMode_);
      }

      // Text remains a screen-space editing surface and is manipulated by the
      // 2D TextGizmo regardless of viewport orientation. Artifact3DGizmo's
      // unified frame gizmo is reserved for non-text layers (its hit test
      // already excludes text layers).
      const auto overlayOwner = gizmoOwnerForLayer(selectedLayer);
      const bool use2DTransformGizmo =
          overlayOwner == GizmoOwner::Text || overlayOwner == GizmoOwner::Content ||
          overlayOwner == GizmoOwner::Transform2D;
      if (use2DTransformGizmo) {

        ArtifactCore::ProfileScope _profG2D(

            "Gizmo2D", ArtifactCore::ProfileCategory::Render);

        sync2DGizmosForLayer(selectedLayer);

        if (overlayOwner == GizmoOwner::Content && contentGizmo_) {

          ArtifactCore::ProfileScope _profG2DDrawCall(

              "Gizmo2DDrawCall", ArtifactCore::ProfileCategory::Render);

          contentGizmo_->draw(renderer_.get());

        } else if (overlayOwner == GizmoOwner::Text && textGizmo_) {

          ArtifactCore::ProfileScope _profG2DDrawCall(

              "Gizmo2DDrawCall", ArtifactCore::ProfileCategory::Render);

          textGizmo_->draw(renderer_.get());

          if (textEditSessionActive_ &&
              selectedLayer->id() == textEditSessionLayerId_) {
            if (auto textSessionLayer = ArtifactCore::dynamicPointerCast<
                    ArtifactTextLayer>(selectedLayer)) {
              drawTextSessionOverlay(
                  *renderer_, textSessionLayer, textEditSessionEditText_,
                  textEditCursorPos_, textEditSelectionAnchor_,
                  textEditPreeditText_, textEditCaretVisible_);
            }
          }

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



      // Non-text layers use the 3D manipulator as their unified frame gizmo.
      // Text layers are handled by the 2D TextGizmo above, so skip the 3D
      // gizmo to avoid a second, non-interactive frame overlapping it.
      const bool showProjected3DGizmo =
          overlayOwner == GizmoOwner::Transform3D;
      if (gizmo3D_ && showProjected3DGizmo) {

        ArtifactCore::ProfileScope _profG3D(

            "Gizmo3D", ArtifactCore::ProfileCategory::Render);

        const QMatrix4x4& viewForGizmoSpace = gizmo3DCameraMatricesValid_
            ? gizmo3DViewMatrix_
            : (has3DCamera ? cameraViewMatrix : fallbackFrameView);
        bool viewBasisInvertible = false;
        const QMatrix4x4 inverseGizmoView =
            viewForGizmoSpace.inverted(&viewBasisInvertible);
        if (viewBasisInvertible) {
          gizmo3D_->setViewBasis(
              ArtifactCore::Coordinates::worldVectorFromQVector3D(
                  inverseGizmoView.mapVector(QVector3D(1.0f, 0.0f, 0.0f))),
              ArtifactCore::Coordinates::worldVectorFromQVector3D(
                  inverseGizmoView.mapVector(QVector3D(0.0f, -1.0f, 0.0f))),
              ArtifactCore::Coordinates::worldVectorFromQVector3D(
                  inverseGizmoView.mapVector(QVector3D(0.0f, 0.0f, 1.0f))));
        }

        syncGizmo3DFromLayer(selectedLayer);

        const bool maskToolActive =
            hostSnapshot.activeToolKind == CompositionHostToolKind::Pen ||
            (hostSnapshot.activeToolKind == CompositionHostToolKind::Rectangle &&
             rectangleToolMode_ == RectangleToolMode::Mask);
        gizmo3D_->setDepthEnabled(selectedLayer->is3D() && !maskToolActive);

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
            view = fallbackFrameView;
            proj = fallbackFrameProjection;
          }

          // The projected-plane press and drag paths must use the exact
          // fallback pair used to draw this gizmo.  Reading the renderer's
          // mutable generic matrices here makes the handle ray depend on the
          // last unrelated draw call.
          gizmo3DViewMatrix_ = view;
          gizmo3DProjectionMatrix_ = proj;
          gizmo3DCameraMatricesValid_ = true;



          {

            ArtifactCore::ProfileScope _profG3DDraw(

                "Gizmo3DDraw", ArtifactCore::ProfileCategory::Render);

            gizmo3D_->draw(renderer_.get(), view, proj, viewportW, viewportH);

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
                                             : renderer_->getProjectionMatrix(), viewportW, viewportH);

          }

          {

            ArtifactCore::ProfileScope _profG3DFlush(

                "Gizmo3DFlush", ArtifactCore::ProfileCategory::Render);

            renderer_->flushGizmo3D();

          }

        }

      }



      {

        if (trackerGizmo_ && hostSnapshot.trackerToolActive) {

          ArtifactCore::ProfileScope _profTracker(

              "TrackerGizmo", ArtifactCore::ProfileCategory::Render);

          const double trackerFps =
              std::max(1.0, static_cast<double>(comp->frameRate().framerate()));
          trackerGizmo_->setCurrentFrame(
              static_cast<double>(currentFrame.framePosition()) / trackerFps);

          trackerGizmo_->draw(renderer_.get());

        }

      }

    }

  }



  if (renderer_ && workCursorVisible_ && workCursorState_.spatial) {
    const Detail::float3 center{workCursorState_.worldPosition.x,
                                workCursorState_.worldPosition.y,
                                workCursorState_.worldPosition.z};
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



  projectedFrameSizeBadgesVisible_ = false;
  projectedFrameWidthBadgeRect_ = {};
  projectedFrameHeightBadgeRect_ = {};

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

      // The next pointer event uses the single snapshot for tangent
      // hit-testing. The draw loop may have ended on a different selected
      // layer, so restore the active layer's snapshot explicitly.
      if (!selectedLayerId_.isNil()) {
        const QString selectedCacheKey =
            comp->id().toString() + QLatin1Char(':') +
            selectedLayerId_.toString();
        const auto selectedCache =
            motionPathCacheByLayer_.constFind(selectedCacheKey);
        if (selectedCache != motionPathCacheByLayer_.cend()) {
          motionPathCache_ = selectedCache.value();
        } else {
          motionPathCache_.valid = false;
        }
      }

      // Historical Plane frames are a transform-only motion aid, not an
      // onion-skin of rendered pixels. Draw them in viewport pixel space so
      // their dotted outline follows the same camera projection as the frame.
      const QMatrix4x4 &motionFrameView = viewportOrientationMatricesValid_
          ? viewportOrientationViewForOverlay_
          : gizmo3DCameraMatricesValid_ ? gizmo3DViewMatrix_
                                        : fallbackFrameView;
      const QMatrix4x4 &motionFrameProjection =
          viewportOrientationMatricesValid_
              ? viewportOrientationProjectionForOverlay_
              : gizmo3DCameraMatricesValid_ ? gizmo3DProjectionMatrix_
                                            : fallbackFrameProjection;
      const QRect motionFrameViewport(
          0, 0, std::max(1, static_cast<int>(hostWidth_)),
          std::max(1, static_cast<int>(hostHeight_)));
      const int motionFrameFps = std::max(
          1, static_cast<int>(std::round(comp->frameRate().framerate())));
      RendererPanZoomScope motionFrameViewScope(renderer_.get());
      renderer_->setUseExternalMatrices(false);
      renderer_->setCanvasSize(std::max(1.0f, hostWidth_),
                               std::max(1.0f, hostHeight_));
      renderer_->setZoom(1.0f);
      renderer_->setPan(0.0f, 0.0f);
      for (const auto &layer : layersForOverlay) {
        if (!layer ||
            (!selectedLayerIds.isEmpty()
                 ? !selectedLayerIds.contains(layer->id())
                 : selectedLayerId_.isNil() ||
                       layer->id() != selectedLayerId_)) {
          continue;
        }
        drawPastFixedPlaneMotionFrames(
            renderer_.get(), layer, currentFrame.framePosition(), motionFrameFps,
            motionFrameView, motionFrameProjection, motionFrameViewport);
      }
    }



    {

      ArtifactCore::ProfileScope _profBBox(

          "BoundingBox", ArtifactCore::ProfileCategory::Render);

      // The projected 3D frame belongs to the transform gizmo. Guide
      // visibility must not suppress the selected layer's controls.
      if (showGizmoOverlay_) {

        const FloatColor secondaryColor{0.28f, 0.74f, 1.0f, 0.85f};

        QVector<ArtifactAbstractLayerPtr> combinedSelectionLayers;
        bool combinedSelectionUsesProjectedFrame =
            viewportOrientationMatricesValid_;
        for (const auto &candidate : layers) {
          if (candidate && isLayerEffectivelyVisible(candidate) &&
              candidate->isActiveAt(currentFrame) &&
              !dynamic_cast<ArtifactVideoLayer *>(candidate.get()) &&
              // A volume is edited with the 3D axis gizmo and its mesh
              // outline; a projected 2D frame would misrepresent its depth.
              (!candidate->is3D() || layerUsesProjectedFrameGizmo(candidate)) &&
              isLayerSelected(selectedIds, candidate)) {
            combinedSelectionLayers.push_back(candidate);
            combinedSelectionUsesProjectedFrame =
                combinedSelectionUsesProjectedFrame ||
                layerUsesProjectedFrameGizmo(candidate);
          }
        }
        const bool drawCombinedProjectedFrame =
            combinedSelectionLayers.size() > 1 &&
            combinedSelectionUsesProjectedFrame;

        for (const auto &layer : layers) {

          if (!isLayerEffectivelyVisible(layer) ||

              !layer->isActiveAt(currentFrame) ||

              dynamic_cast<ArtifactVideoLayer *>(layer.get()) ||

              (!isLayerSelected(selectedIds, layer) &&
               (selectedLayerId_.isNil() ||
                layer->id() != selectedLayerId_))) {

            continue;

          }

          const QRectF localBounds = layer->localBounds();

          if (!localBounds.isValid() || localBounds.width() <= 0.0 ||

              localBounds.height() <= 0.0) {

            continue;

          }

          // Only planes have an unambiguous 2D frame. Keep volume objects on
          // the 3D transform-gizmo/mesh-outline path even in oriented views.
          if (layer->is3D() && !layerUsesProjectedFrameGizmo(layer)) {
            continue;
          }

          if (drawCombinedProjectedFrame) {
            continue;
          }

          const bool primary =

              !selectedLayerId_.isNil() && layer->id() == selectedLayerId_;

          if (primary && !layerUsesProjectedFrameGizmo(layer) &&
              !viewportOrientationMatricesValid_) {
            continue;
          }

          const QMatrix4x4 &frameView = viewportOrientationMatricesValid_
              ? viewportOrientationViewForOverlay_
              : has3DCamera ? cameraViewMatrix
                            : fallbackFrameView;
          const QMatrix4x4 &frameProjection = viewportOrientationMatricesValid_
              ? viewportOrientationProjectionForOverlay_
              : has3DCamera ? cameraProjMatrix
                            : fallbackFrameProjection;
          const bool projectedFrame = layerUsesProjectedFrameGizmo(layer) ||
              viewportOrientationMatricesValid_;
          const FloatColor frameColor = primary
              ? FloatColor{0.24f, 0.68f, 1.0f, 0.98f}
              : secondaryColor;
          const float frameThickness = primary ? 2.2f : 1.6f;
          const bool showProjectedScaleHandles = !projectedFrame ||
              gizmoMode_ == TransformGizmo::Mode::All ||
              gizmoMode_ == TransformGizmo::Mode::Scale;
          // Projected frames have no rotation hit target; use the axis ring.
          const bool showProjectedRotationHandle = !projectedFrame;
          float projectedHandleSize = 0.0f;
          if (projectedFrame) {
            const QRect frameViewport(
                0, 0, std::max(1, static_cast<int>(hostWidth_)),
                std::max(1, static_cast<int>(hostHeight_)));
            const QPointF localCenter = localBounds.center();
            const QMatrix4x4 world = layer->getGlobalTransform4x4();
            const QVector3D centerProjected = ViewportMath::projectToTopDown(
                world.map(QVector3D(static_cast<float>(localCenter.x()),
                                    static_cast<float>(localCenter.y()), 0.0f)),
                frameView, frameProjection, frameViewport);
            const QVector3D xProjected = ViewportMath::projectToTopDown(
                world.map(QVector3D(static_cast<float>(localCenter.x() + 1.0),
                                    static_cast<float>(localCenter.y()), 0.0f)),
                frameView, frameProjection, frameViewport);
            const QVector3D yProjected = ViewportMath::projectToTopDown(
                world.map(QVector3D(static_cast<float>(localCenter.x()),
                                    static_cast<float>(localCenter.y() + 1.0),
                                    0.0f)),
                frameView, frameProjection, frameViewport);
            const float pixelsPerUnit = 0.5f * static_cast<float>(
                QLineF(QPointF(centerProjected.x(), centerProjected.y()),
                       QPointF(xProjected.x(), xProjected.y())).length() +
                QLineF(QPointF(centerProjected.x(), centerProjected.y()),
                       QPointF(yProjected.x(), yProjected.y())).length());
            if (std::isfinite(pixelsPerUnit) && pixelsPerUnit > 0.0001f) {
              // This value is expressed in layer-local units and is transformed
              // with the frame below. Do not cap it to an arbitrary local size:
              // that makes the final on-screen handle shrink again once a layer
              // becomes small enough to hit the cap.
              projectedHandleSize = 18.0f / pixelsPerUnit;
            }
          }
          SelectionFrameHandleFeedback handleFeedback;
          if (primary && projectedFrame && !drawCombinedProjectedFrame) {
            handleFeedback.hovered = selectionFrameHandleFor(
                projectedFrameHoverHandle_);
            if (gizmoDragActive_) {
              handleFeedback.active = selectionFrameHandleFor(
                  projectedFrameHandle_);
              handleFeedback.dragging =
                  handleFeedback.active != SelectionFrameHandle::None;
            }
          }
          ::Artifact::drawSelectionFrameOverlay(
              renderer_.get(), layer, frameColor, frameThickness, &frameView,
              &frameProjection, showProjectedScaleHandles,
              showProjectedRotationHandle, projectedHandleSize,
              handleFeedback);

        }

        if (drawCombinedProjectedFrame) {
          const QMatrix4x4 &frameView = viewportOrientationMatricesValid_
              ? viewportOrientationViewForOverlay_
              : has3DCamera ? cameraViewMatrix
                            : fallbackFrameView;
          const QMatrix4x4 &frameProjection = viewportOrientationMatricesValid_
              ? viewportOrientationProjectionForOverlay_
              : has3DCamera ? cameraProjMatrix
                            : fallbackFrameProjection;
          const QRect frameViewport(
              0, 0, std::max(1, static_cast<int>(hostWidth_)),
              std::max(1, static_cast<int>(hostHeight_)));
          const ScreenPhysicalBounds2 combinedPhysicalBounds =
              projectedSelectionFrameBounds(combinedSelectionLayers,
                                            frameView, frameProjection,
                                            frameViewport);
          const QRectF combinedBounds =
              qRectFromScreenPhysicalBounds(combinedPhysicalBounds);
          if (isValidScreenPhysicalBounds(combinedPhysicalBounds)) {
            const float previousZoom = renderer_->getZoom();
            float previousPanX = 0.0f;
            float previousPanY = 0.0f;
            renderer_->getPan(previousPanX, previousPanY);
            renderer_->setUseExternalMatrices(false);
            renderer_->setCanvasSize(std::max(1.0f, hostWidth_),
                                     std::max(1.0f, hostHeight_));
            renderer_->setZoom(1.0f);
            renderer_->setPan(0.0f, 0.0f);
            const QRectF visibleFrameArea(
                0.0, 0.0, std::max(1.0f, hostWidth_) - 1.0f,
                std::max(1.0f, hostHeight_) - 1.0f);
            const FloatColor individualFrameColor{0.24f, 0.64f, 0.92f,
                                                   0.62f};
            for (const auto &selectedLayer : combinedSelectionLayers) {
              std::array<ArtifactCore::Coordinates::ScreenPhysicalPoint2, 4>
                  projectedPoints;
              int visibleCorners = 0;
              if (!projectedLayerFrameCorners(
                      selectedLayer, frameView, frameProjection,
                      frameViewport, projectedPoints, &visibleCorners)) {
                continue;
              }
              const FloatColor frameColor{
                  individualFrameColor.r(), individualFrameColor.g(),
                  individualFrameColor.b(), individualFrameColor.a() *
                      (visibleCorners < 4 ? 0.48f : 1.0f)};
              for (size_t index = 0; index < projectedPoints.size(); ++index) {
                QPointF start = qPointFromScreenPhysicalPoint(
                    projectedPoints[index]);
                QPointF end = qPointFromScreenPhysicalPoint(
                    projectedPoints[(index + 1) % projectedPoints.size()]);
                if (!clipProjectedFrameEdge(visibleFrameArea, start, end)) {
                  continue;
                }
                renderer_->drawSolidLine(
                    {static_cast<float>(start.x()),
                     static_cast<float>(start.y())},
                    {static_cast<float>(end.x()),
                     static_cast<float>(end.y())},
                    frameColor, 1.15f);
              }
            }
            const FloatColor combinedColor{0.20f, 0.72f, 1.0f, 1.0f};
            const std::array<QPointF, 4> combinedCorners{
                combinedBounds.topLeft(), combinedBounds.topRight(),
                combinedBounds.bottomRight(), combinedBounds.bottomLeft()};
            for (size_t index = 0; index < combinedCorners.size(); ++index) {
              QPointF start = combinedCorners[index];
              QPointF end =
                  combinedCorners[(index + 1) % combinedCorners.size()];
              if (!clipProjectedFrameEdge(visibleFrameArea, start, end)) {
                continue;
              }
              renderer_->drawSolidLine(
                  {static_cast<float>(start.x()),
                   static_cast<float>(start.y())},
                  {static_cast<float>(end.x()), static_cast<float>(end.y())},
                  combinedColor, 2.2f);
            }
            const bool showScaleHandles =
                gizmoMode_ == TransformGizmo::Mode::All ||
                gizmoMode_ == TransformGizmo::Mode::Scale;
            if (showScaleHandles) {
              const std::array<std::pair<QPointF, SelectionFrameHandle>, 8>
                  handles{{
                      {combinedBounds.topLeft(), SelectionFrameHandle::TopLeft},
                      {combinedBounds.topRight(), SelectionFrameHandle::TopRight},
                      {combinedBounds.bottomLeft(), SelectionFrameHandle::BottomLeft},
                      {combinedBounds.bottomRight(), SelectionFrameHandle::BottomRight},
                      {QPointF(combinedBounds.center().x(), combinedBounds.top()),
                       SelectionFrameHandle::Top},
                      {QPointF(combinedBounds.center().x(), combinedBounds.bottom()),
                       SelectionFrameHandle::Bottom},
                      {QPointF(combinedBounds.left(), combinedBounds.center().y()),
                       SelectionFrameHandle::Left},
                      {QPointF(combinedBounds.right(), combinedBounds.center().y()),
                       SelectionFrameHandle::Right}}};
              const SelectionFrameHandle hovered =
                  selectionFrameHandleFor(projectedFrameHoverHandle_);
              const SelectionFrameHandle active = gizmoDragActive_
                  ? selectionFrameHandleFor(projectedFrameHandle_)
                  : SelectionFrameHandle::None;
              for (const auto &[handle, handleType] : handles) {
                if (!visibleFrameArea.contains(handle)) continue;
                const bool isActive = active == handleType;
                const bool isHovered = !gizmoDragActive_ &&
                                       hovered == handleType;
                const float outerSize = isActive ? 14.0f
                    : isHovered ? 12.0f : 10.0f;
                const FloatColor fill = isActive
                    ? FloatColor{1.0f, 0.46f, 0.14f, 1.0f}
                    : isHovered ? FloatColor{0.40f, 0.80f, 1.0f, 1.0f}
                                : combinedColor;
                renderer_->drawSolidRect(
                    static_cast<float>(handle.x() - outerSize * 0.5f),
                    static_cast<float>(handle.y() - outerSize * 0.5f),
                    outerSize, outerSize,
                    FloatColor{0.04f, 0.08f, 0.12f, 1.0f}, 1.0f);
                renderer_->drawSolidRect(
                    static_cast<float>(handle.x() - 3.0),
                    static_cast<float>(handle.y() - 3.0), 6.0f, 6.0f,
                    fill, 1.0f);
              }
            }
            renderer_->setZoom(previousZoom);
            renderer_->setPan(previousPanX, previousPanY);
            if (lastCanvasWidth_ > 0.0f && lastCanvasHeight_ > 0.0f) {
              renderer_->setCanvasSize(lastCanvasWidth_, lastCanvasHeight_);
            }
          }
        }

      }

    }

  }

  // Show dimensions only while resizing, away from the central handles.
  const auto *frameBadgeSelection = ArtifactLayerSelectionManager::instance();
  const bool singleFrameBadgeSelection =
      !frameBadgeSelection || frameBadgeSelection->selectedLayers().size() <= 1;
  if (renderer_ && comp && !selectedLayerId_.isNil() &&
      selectedIds.size() <= 1 && singleFrameBadgeSelection && gizmoDragActive_ &&
      isScaleHandle(projectedFrameHandle_)) {
    const auto layer = comp->layerById(selectedLayerId_);
    if (layer && layerUsesProjectedFrameGizmo(layer)) {
      const QRectF bounds = layer->localBounds();
      const QRect viewport(0, 0, std::max(1, static_cast<int>(hostWidth_)),
                           std::max(1, static_cast<int>(hostHeight_)));
      const QMatrix4x4 &view = viewportOrientationMatricesValid_
          ? viewportOrientationViewForOverlay_
          : gizmo3DCameraMatricesValid_ ? gizmo3DViewMatrix_
                                         : fallbackFrameView;
      const QMatrix4x4 &projection = viewportOrientationMatricesValid_
          ? viewportOrientationProjectionForOverlay_
          : gizmo3DCameraMatricesValid_ ? gizmo3DProjectionMatrix_
                                         : fallbackFrameProjection;
      const QVector3D worldCenter = layer->getGlobalTransform4x4().map(
          QVector3D(static_cast<float>(bounds.center().x()),
                    static_cast<float>(bounds.center().y()), 0.0f));
      const QVector3D center = ViewportMath::projectToTopDown(
          worldCenter, view, projection, viewport);
      if (bounds.isValid() && std::isfinite(center.x()) &&
          std::isfinite(center.y()) && center.z() >= 0.0f &&
          center.z() <= 1.0f) {
        const auto time = gizmoTransformTime(layer, layer->currentFrame());
        const auto &transform = layer->transform3D();
        const QString widthText = QStringLiteral("W %1")
            .arg(QString::number(bounds.width() *
                                 std::abs(transform.scaleXAt(time)), 'f', 1));
        const QString heightText = QStringLiteral("H %1")
            .arg(QString::number(bounds.height() *
                                 std::abs(transform.scaleYAt(time)), 'f', 1));
        const QFont badgeFont(QStringLiteral("sans-serif"), 9);
        const QFontMetrics metrics(badgeFont);
        const float width = std::max(
            66.0f, static_cast<float>(metrics.horizontalAdvance(widthText) + 18));
        const float height = 20.0f;
        const float panelX = std::clamp(projectedFrameLastPointer_.x + 24.0f, 6.0f,
            std::max(6.0f, static_cast<float>(viewport.width()) - width - 6.0f));
        const float panelY = std::clamp(projectedFrameLastPointer_.y + 24.0f, 6.0f,
            std::max(6.0f, static_cast<float>(viewport.height()) -
                              height * 2.0f - 10.0f));
        projectedFrameWidthBadgeRect_ = QRectF(panelX, panelY, width, height);
        projectedFrameHeightBadgeRect_ =
            QRectF(panelX, panelY + height + 3.0f, width, height);
        const float previousZoom = renderer_->getZoom();
        float previousPanX = 0.0f;
        float previousPanY = 0.0f;
        renderer_->getPan(previousPanX, previousPanY);
        renderer_->setUseExternalMatrices(false);
        renderer_->setCanvasSize(std::max(1.0f, hostWidth_),
                                 std::max(1.0f, hostHeight_));
        renderer_->setZoom(1.0f);
        renderer_->setPan(0.0f, 0.0f);
        const auto drawBadge = [&](const QRectF &rect, const QString &text) {
          renderer_->drawSolidRect(static_cast<float>(rect.left()),
                                   static_cast<float>(rect.top()),
                                   static_cast<float>(rect.width()),
                                   static_cast<float>(rect.height()),
                                   {0.03f, 0.08f, 0.13f, 0.94f}, 0.9f);
          renderer_->drawText(rect.adjusted(7.0, 1.0, -5.0, -1.0), text,
                              badgeFont, {0.82f, 0.94f, 1.0f, 1.0f},
                              Qt::AlignLeft | Qt::AlignVCenter);
        };
        drawBadge(projectedFrameWidthBadgeRect_, widthText);
        drawBadge(projectedFrameHeightBadgeRect_, heightText);
        renderer_->setZoom(previousZoom);
        renderer_->setPan(previousPanX, previousPanY);
        if (lastCanvasWidth_ > 0.0f && lastCanvasHeight_ > 0.0f) {
          renderer_->setCanvasSize(lastCanvasWidth_, lastCanvasHeight_);
        }
        projectedFrameSizeBadgesVisible_ = true;
      }
    }
  }

  if (showGizmoOverlay_ && renderer_ && comp) {
    const auto construction = ArtifactCore::dynamicPointerCast<ArtifactConstructionLayer>(
        comp->layerById(selectedLayerId_));
    if (construction && construction->isVisible() && !construction->isLocked()) {
      const auto world = construction->getGlobalTransform4x4();
      const auto view = constructionDragLayer_ ? constructionDragView_
          : gizmo3DCameraMatricesValid_ ? gizmo3DViewMatrix_ : renderer_->getViewMatrix();
      const auto projection = constructionDragLayer_ ? constructionDragProjection_
          : gizmo3DCameraMatricesValid_ ? gizmo3DProjectionMatrix_ : renderer_->getProjectionMatrix();
      const QRect viewport(0, 0, int(hostWidth_), int(hostHeight_));
      const float zoom = renderer_->getZoom();
      float panX = 0, panY = 0;
      renderer_->getPan(panX, panY);
      renderer_->setUseExternalMatrices(false);
      renderer_->setCanvasSize(std::max(1.0f, hostWidth_), std::max(1.0f, hostHeight_));
      renderer_->setZoom(1); renderer_->setPan(0, 0);
      const float radius = 4.0f * devicePixelRatio_;
      for (const auto &handle : constructionHandles(*construction)) {
        const auto p = ViewportMath::projectToTopDown(world.map(QVector3D(float(handle.point.x()), float(handle.point.y()), 0)), view, projection, viewport);
        if (!std::isfinite(p.x()) || !std::isfinite(p.y()) || !std::isfinite(p.z()) || p.z() < 0 || p.z() > 1) continue;
        const bool active = constructionDragLayer_ && constructionDragHandle_.index == handle.index;
        const FloatColor color = active ? FloatColor{1, 0.72f, 0.2f, 1} : FloatColor{0.3f, 0.85f, 1, 1};
        renderer_->drawSolidRect(p.x() - radius - 1, p.y() - radius - 1,
            radius * 2 + 2, radius * 2 + 2, FloatColor{0.08f, 0.08f, 0.08f, 1}, 1);
        renderer_->drawSolidRect(p.x() - radius, p.y() - radius, radius * 2, radius * 2, color, 1);
      }
      renderer_->setZoom(zoom); renderer_->setPan(panX, panY);
      renderer_->setCanvasSize(cw, ch);
    }
  }

  if (gizmo3D_ && gizmo3D_->isDragging() &&
      (projectedFrameSnapVerticalValid_ ||
       projectedFrameSnapHorizontalValid_)) {
    const float previousZoom = renderer_->getZoom();
    float previousPanX = 0.0f;
    float previousPanY = 0.0f;
    renderer_->getPan(previousPanX, previousPanY);
    renderer_->setUseExternalMatrices(false);
    renderer_->setCanvasSize(std::max(1.0f, hostWidth_),
                             std::max(1.0f, hostHeight_));
    renderer_->setZoom(1.0f);
    renderer_->setPan(0.0f, 0.0f);
    const FloatColor snapColor{0.26f, 0.82f, 1.0f, 0.90f};
    if (projectedFrameSnapVerticalValid_) {
      renderer_->drawSolidRect(projectedFrameSnapVertical_ - 0.75f, 0.0f,
                               1.5f, std::max(1.0f, hostHeight_),
                               snapColor, 1.0f);
    }
    if (projectedFrameSnapHorizontalValid_) {
      renderer_->drawSolidRect(0.0f,
                               projectedFrameSnapHorizontal_ - 0.75f,
                               std::max(1.0f, hostWidth_), 1.5f,
                               snapColor, 1.0f);
    }
    renderer_->setZoom(previousZoom);
    renderer_->setPan(previousPanX, previousPanY);
    if (lastCanvasWidth_ > 0.0f && lastCanvasHeight_ > 0.0f) {
      renderer_->setCanvasSize(lastCanvasWidth_, lastCanvasHeight_);
    }
  }

  // SPEC 3.5 / 13.1 / 13.2: while a projected-frame resize handle is dragged,
  // relate the driven handle to the point that stays fixed (the diagonally
  // opposite corner, or the opposite edge midpoint). The guide uses the same
  // frame projection as the hit test, so an oblique camera keeps it on the
  // layer plane. The combined selection frame keeps its envelope-only
  // presentation and is not guided per layer.
  if (renderer_ && gizmoDragActive_ && !gizmoGroupTransformActive_ &&
      projectedFrameScalePointerBasisValid_ &&
      isScaleHandle(projectedFrameHandle_)) {
    const auto guideComp = previewPipeline_.composition();
    const auto guideLayer = guideComp && !selectedLayerId_.isNil()
                                ? guideComp->layerById(selectedLayerId_)
                                : ArtifactAbstractLayerPtr{};
    if (guideLayer && (layerUsesProjectedFrameGizmo(guideLayer) ||
                       viewportOrientationMatricesValid_)) {
      const QMatrix4x4 &guideView = viewportOrientationMatricesValid_
          ? viewportOrientationViewForOverlay_
          : gizmo3DCameraMatricesValid_ ? gizmo3DViewMatrix_
                                        : renderer_->getViewMatrix();
      const QMatrix4x4 &guideProjection = viewportOrientationMatricesValid_
          ? viewportOrientationProjectionForOverlay_
          : gizmo3DCameraMatricesValid_ ? gizmo3DProjectionMatrix_
                                        : renderer_->getProjectionMatrix();
      const QRect guideViewport(
          0, 0, std::max(1, static_cast<int>(hostWidth_)),
          std::max(1, static_cast<int>(hostHeight_)));
      std::array<ArtifactCore::Coordinates::ScreenPhysicalPoint2, 4>
          guideCorners;
      ArtifactCore::Coordinates::ScreenPhysicalPoint2 movingPoint{};
      ArtifactCore::Coordinates::ScreenPhysicalPoint2 fixedPoint{};
      if (projectedLayerFrameCorners(guideLayer, guideView, guideProjection,
                                     guideViewport, guideCorners) &&
          projectedFrameGuidePoints(guideCorners, projectedFrameHandle_,
                                    movingPoint, fixedPoint)) {
        const float previousZoom = renderer_->getZoom();
        float previousPanX = 0.0f;
        float previousPanY = 0.0f;
        renderer_->getPan(previousPanX, previousPanY);
        renderer_->setUseExternalMatrices(false);
        renderer_->setCanvasSize(std::max(1.0f, hostWidth_),
                                 std::max(1.0f, hostHeight_));
        renderer_->setZoom(1.0f);
        renderer_->setPan(0.0f, 0.0f);
        const QRectF visibleGuideArea(
            0.0, 0.0, std::max(1.0f, hostWidth_) - 1.0f,
            std::max(1.0f, hostHeight_) - 1.0f);
        const FloatColor guideColor{0.30f, 0.88f, 1.0f, 0.55f};
        const FloatColor fixedMarkColor{0.30f, 0.88f, 1.0f, 0.92f};
        const FloatColor startMarkColor{0.86f, 0.92f, 1.0f, 0.45f};
        QPointF guideStart = qPointFromScreenPhysicalPoint(movingPoint);
        QPointF guideEnd = qPointFromScreenPhysicalPoint(fixedPoint);
        if (clipProjectedFrameEdge(visibleGuideArea, guideStart, guideEnd)) {
          renderer_->drawSolidLine(
              {static_cast<float>(guideStart.x()),
               static_cast<float>(guideStart.y())},
              {static_cast<float>(guideEnd.x()),
               static_cast<float>(guideEnd.y())},
              guideColor, 1.2f);
        }
        if (visibleGuideArea.contains(
                qPointFromScreenPhysicalPoint(fixedPoint))) {
          renderer_->drawSolidRect(fixedPoint.x - 4.0f,
                                   fixedPoint.y - 4.0f,
                                   8.0f, 8.0f,
                                   {0.03f, 0.08f, 0.13f, 0.92f}, 1.0f);
          renderer_->drawSolidRect(fixedPoint.x - 2.5f,
                                   fixedPoint.y - 2.5f,
                                   5.0f, 5.0f, fixedMarkColor, 1.0f);
        }
        if (projectedFrameScaleStartHandlePointValid_ &&
            visibleGuideArea.contains(qPointFromScreenPhysicalPoint(
                projectedFrameScaleStartHandlePoint_))) {
          renderer_->drawSolidRect(
              projectedFrameScaleStartHandlePoint_.x - 2.0f,
              projectedFrameScaleStartHandlePoint_.y - 2.0f,
              4.0f, 4.0f, startMarkColor, 1.0f);
        }
        renderer_->setZoom(previousZoom);
        renderer_->setPan(previousPanX, previousPanY);
        if (lastCanvasWidth_ > 0.0f && lastCanvasHeight_ > 0.0f) {
          renderer_->setCanvasSize(lastCanvasWidth_, lastCanvasHeight_);
        }
      }
    }
  }

  // P0-1 Box zoom marquee. Drawn in canvas space using the current
  // pan/zoom so the rectangle tracks the cursor even mid-drag.
  if (isBoxZooming_) {
    const QPointF boxZoomStart =
        ArtifactCore::Coordinates::toQPointF(boxZoomStartViewportPos_);
    const QPointF boxZoomCurrent =
        ArtifactCore::Coordinates::toQPointF(boxZoomCurrentViewportPos_);
    const QRectF zoomRect = dragRectFromPoints(
        boxZoomStart, boxZoomCurrent);
    const QRectF canvasRect = viewportRectToCanvasRect(
        renderer_.get(), boxZoomStart, boxZoomCurrent).normalized();
    if (!canvasRect.isEmpty()) {
      const FloatColor boxFill{0.25f, 0.55f, 1.0f, 0.10f};
      const FloatColor boxOutline{0.30f, 0.70f, 1.0f, 0.95f};
      renderer_->drawSolidRect(static_cast<float>(canvasRect.left()),
                               static_cast<float>(canvasRect.top()),
                               static_cast<float>(canvasRect.width()),
                               static_cast<float>(canvasRect.height()),
                               boxFill, 1.0f);
      drawTaggedRectOutline(renderer_.get(), canvasRect, boxOutline, true);
      // Avoid unused-variable warnings when the viewport rectangle is the
      // primary draw source (kept symmetric with the rubber-band overlay).
      (void)zoomRect;
    }
  }

  // P0-2 Tumble pivot marker. Spatial orientations only; front orthographic
  // does not tumble so the marker is intentionally suppressed.
  if (tumblePivotOverrideEnabled_ && usesSpatialViewportOrientation() &&
      renderer_) {
    const float invZoom =
        1.0f / std::max(0.001f, renderer_->getZoom());
    const float pivotRadius = std::max(5.0f, 7.0f * invZoom);
    const FloatColor pivotColor{1.0f, 0.92f, 0.30f, 0.95f};
    renderer_->drawSolidLine(
        {tumblePivotCanvasPos_.x - pivotRadius,
         tumblePivotCanvasPos_.y},
        {tumblePivotCanvasPos_.x + pivotRadius,
         tumblePivotCanvasPos_.y},
        pivotColor, std::max(1.0f, invZoom));
    renderer_->drawSolidLine(
        {tumblePivotCanvasPos_.x,
         tumblePivotCanvasPos_.y - pivotRadius},
        {tumblePivotCanvasPos_.x,
         tumblePivotCanvasPos_.y + pivotRadius},
        pivotColor, std::max(1.0f, invZoom));
  }

  // P0-3a Interactive Render Region outline + handles. Drawn in canvas
  // space; the rectangle itself is owned by the controller and not yet
  // wired into the renderer's ROI pipeline (P0-3b).
  if (interactiveRenderRegionActive_ && renderer_) {
    const FloatColor fillColor{0.95f, 0.55f, 0.10f, 0.06f};
    const FloatColor outlineColor{1.0f, 0.65f, 0.15f, 0.95f};
    const QRectF &rect = interactiveRenderRegionRect_;
    renderer_->drawSolidRect(
        static_cast<float>(rect.left()), static_cast<float>(rect.top()),
        static_cast<float>(rect.width()), static_cast<float>(rect.height()),
        fillColor, 1.0f);
    drawTaggedRectOutline(renderer_.get(), rect, outlineColor, true);
    // Eight resize handles + a move hitbox indicator (center cross).
    const float invZoom = 1.0f / std::max(0.001f, renderer_->getZoom());
    const float handleSize = std::max(6.0f, 10.0f * invZoom);
    const FloatColor handleColor{1.0f, 0.80f, 0.30f, 0.95f};
    auto drawHandle = [&](float cx, float cy) {
      renderer_->drawSolidRect(cx - handleSize * 0.5f,
                               cy - handleSize * 0.5f, handleSize,
                               handleSize, handleColor, 1.0f);
    };
    const float left = static_cast<float>(rect.left());
    const float right = static_cast<float>(rect.right());
    const float top = static_cast<float>(rect.top());
    const float bottom = static_cast<float>(rect.bottom());
    const float cx = static_cast<float>(rect.center().x());
    const float cy = static_cast<float>(rect.center().y());
    drawHandle(left, top);
    drawHandle(cx, top);
    drawHandle(right, top);
    drawHandle(right, cy);
    drawHandle(right, bottom);
    drawHandle(cx, bottom);
    drawHandle(left, bottom);
    drawHandle(left, cy);
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

    const HostFrameSnapshot &hostSnapshot, const ArtifactCompositionPtr &comp,

    const ArtifactAbstractLayerPtr &selectedLayer,

    const FramePosition &currentFrame) {

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

    const QString &toolLabel = hostSnapshot.activeToolLabel;

    const float zoom = renderer_ != nullptr ? renderer_->getZoom() : 1.0f;

    QStringList statusParts;

    statusParts << toolLabel

                << QStringLiteral("%1%")

                       .arg(zoom * 100.0f, 0, 'f', zoom < 10.0f ? 1 : 0);

    if (hostSnapshot.selectedLayerCount > 0) {

      const int selectedCount = hostSnapshot.selectedLayerCount;

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

    const bool snapBypassed = hostSnapshot.altModifierActive;

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

namespace CompositionRenderControllerDetail {

QFont fixedWidthFont(const int pointSize) {
  QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  font.setPointSize(pointSize);
  return font;
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

QString lineDebugVisibilitySettingKey(LineDebugKind kind) {
  switch (kind) {
  case LineDebugKind::RigBone:
    return QStringLiteral("Viewport/LineDebug/RigBone");
  case LineDebugKind::RigControl:
    return QStringLiteral("Viewport/LineDebug/RigControl");
  case LineDebugKind::RigSkin:
    return QStringLiteral("Viewport/LineDebug/RigSkin");
  default:
    return {};
  }
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



} // namespace CompositionRenderControllerDetail

namespace CompositionRenderControllerDetail {





} // namespace CompositionRenderControllerDetail

void CompositionRenderController::setShowAudioWaveformOverlay(bool show) {
  if (!impl_ || impl_->showAudioWaveformOverlay_ == show) return;
  impl_->showAudioWaveformOverlay_ = show;
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("Viewport/AudioWaveformOverlay"), show);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isShowAudioWaveformOverlay() const {
  return impl_ ? impl_->showAudioWaveformOverlay_ : false;
}

void CompositionRenderController::setShowAudioSpectrumOverlay(bool show) {
  if (!impl_ || impl_->showAudioSpectrumOverlay_ == show) return;
  impl_->showAudioSpectrumOverlay_ = show;
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("Viewport/AudioSpectrumOverlay"), show);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isShowAudioSpectrumOverlay() const {
  return impl_ ? impl_->showAudioSpectrumOverlay_ : false;
}

void CompositionRenderController::setOnionSkinFrameCount(int count) {

  impl_->onionSkinFrameCount_ = std::clamp(count, 1, 5);
  impl_->onionSkinHistory_.trim(impl_->onionSkinFrameCount_);
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

  if (!show && impl_->historicalScaleDragging_) cancelGizmoInteraction();

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

impl_->invalidateOverlayComposite();

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

impl_->invalidateOverlayComposite();

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

impl_->invalidateOverlayComposite();

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

impl_->invalidateOverlayComposite();

markRenderDirty();

}

bool CompositionRenderController::isShowAutoColorPaletteOverlay() const {

  return impl_ ? impl_->showAutoColorPaletteOverlay_ : false;

}

void CompositionRenderController::setDropGhostPreview(

    ScreenPhysicalBounds2 viewportBounds, const QString &title, const QString &hint,

    const QString &label) {

  if (!impl_) {

    return;

  }

  const QRectF viewportRect = toQRectF(viewportBounds);

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
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos,
    const QStringList &items,

    const QString &title, const QString &subtitle,

    const QVector<bool> &enabledStates) {

  if (!impl_) {

    return;

  }

  impl_->contextMenuVisible_ = true;

  const auto physicalPos = ArtifactCore::Coordinates::toScreenPhysical(
      viewportPos, impl_->devicePixelRatio_);
  impl_->contextMenuViewportPos_ = physicalPos;

  impl_->contextMenuTitle_ = title;

  impl_->contextMenuSubtitle_ = subtitle;

  impl_->contextMenuItems_ = items;

  impl_->contextMenuItemEnabled_ = enabledStates;

  if (impl_->contextMenuItemEnabled_.size() != impl_->contextMenuItems_.size()) {

    impl_->contextMenuItemEnabled_.resize(impl_->contextMenuItems_.size());

    impl_->contextMenuItemEnabled_.fill(true);

  }

  impl_->contextMenuSelectedIndex_ = impl_->viewportOverlayItemAt(physicalPos);

  impl_->commandPaletteVisible_ = false;

  impl_->commandPaletteItems_.clear();

  impl_->pieMenuVisible_ = false;

  impl_->pieMenuModel_ = PieMenuModel{};

  impl_->pieMenuSelectedIndex_ = -1;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}



void CompositionRenderController::showPieMenuOverlay(
    const PieMenuModel &model,
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {

  if (!impl_) {

    return;

  }

  impl_->pieMenuVisible_ = true;

  impl_->pieMenuModel_ = model;

  const auto physicalPos = ArtifactCore::Coordinates::toScreenPhysical(
      viewportPos, impl_->devicePixelRatio_);
  impl_->pieMenuViewportPos_ = physicalPos;
  impl_->pieMenuSelectedIndex_ = impl_->pieMenuItemAt(physicalPos);

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
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) const {
  if (!impl_) {
    return -1;
  }
  return impl_->viewportOverlayItemAt(
      ArtifactCore::Coordinates::toScreenPhysical(viewportPos,
                                                   impl_->devicePixelRatio_));

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
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {

  if (!impl_ || !impl_->pieMenuVisible_) {

    return;

  }

  const auto physicalPos = ArtifactCore::Coordinates::toScreenPhysical(
      viewportPos, impl_->devicePixelRatio_);
  const int selected = impl_->pieMenuItemAt(physicalPos);

  if (selected != impl_->pieMenuSelectedIndex_) {

    impl_->pieMenuSelectedIndex_ = selected;

    impl_->invalidateOverlayComposite();

    markRenderDirty();

  }

}



void CompositionRenderController::updateContextMenuOverlayMousePos(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {

  if (!impl_ || !impl_->contextMenuVisible_) {

    return;

  }

  const auto physicalPos = ArtifactCore::Coordinates::toScreenPhysical(
      viewportPos, impl_->devicePixelRatio_);
  const int selected = impl_->viewportOverlayItemAt(physicalPos);

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

} // namespace Artifact
