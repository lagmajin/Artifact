module;
#include <utility>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <RefCntAutoPtr.hpp>
#include <wobjectimpl.h>
#include <QTimer>
#include <QDebug>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QKeyEvent>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QFontMetrics>
#include <QAction>
#include <QApplication>
#include <QImage>
#include <QLinearGradient>
#include <QLineF>
#include <QMenu>
#include <QPainter>
#include <QCursor>
#include <QHash>
#include <QPixmap>
#include <QPointer>
#include <QStandardPaths>
#include <QTransform>
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>

module Artifact.Widgets.RenderLayerWidgetv2;
import Graphics;
import Graphics.Shader.Set;
import Graphics.Shader.Compile.Task;
import Graphics.Shader.Compute.HLSL.Blend;
import Layer.Blend;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Service.Application;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Service.ActiveContext;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Undo.UndoManager;
import Event.Bus;
import Artifact.Event.Types;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Effect.Abstract;
import Shape.Operator;
import Property.Abstract;

import Artifact.Render.IRenderer;
import Artifact.Render.CompositionRenderer;
import Artifact.Preview.Pipeline;
import Artifact.Layer.Image;
import Artifact.Widgets.TransformGizmo;
import Utils.Path;

namespace Artifact {

 using namespace ArtifactCore;

W_OBJECT_IMPL(ArtifactLayerEditorWidgetV2)

namespace {
Q_LOGGING_CATEGORY(layerViewPerfLog, "artifact.layerviewperf")

enum class LayerBackgroundMode {
  Alpha,
  Solid,
  MayaGradient
};

enum class LayerSurfaceMode {
  Edit,
  Inspect,
  Impact
};

const FloatColor kImpactParentColor{0.30f, 0.86f, 0.58f, 0.88f};
const FloatColor kImpactChildColor{0.24f, 0.66f, 1.0f, 0.82f};
const FloatColor kImpactMatteColor{0.78f, 0.42f, 1.0f, 0.90f};
const FloatColor kImpactDependentColor{1.0f, 0.58f, 0.16f, 0.88f};

QString editModeLabel(EditMode mode)
{
  switch (mode) {
  case EditMode::Transform:
    return QStringLiteral("Move");
  case EditMode::Mask:
    return QStringLiteral("Mask");
  case EditMode::Paint:
    return QStringLiteral("Paint");
  case EditMode::Shape:
    return QStringLiteral("Shape");
  case EditMode::View:
  default:
    return QStringLiteral("View");
  }
}

QString displayModeLabel(DisplayMode mode)
{
  switch (mode) {
  case DisplayMode::Mask:
    return QStringLiteral("Mask");
  case DisplayMode::Alpha:
    return QStringLiteral("Alpha");
  case DisplayMode::Wireframe:
    return QStringLiteral("Wireframe");
  case DisplayMode::Color:
  default:
    return QStringLiteral("Final");
  }
}

QString layerTypeLabel(const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) return QStringLiteral("—");

  QString type;
  if (std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) {
    type = QStringLiteral("Shape");
  } else if (std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
    type = QStringLiteral("Image");
  } else {
    type = layer->className().toQString().trimmed();
    if (type.startsWith(QStringLiteral("Artifact"))) {
      type.remove(0, 8);
    }
    if (type.endsWith(QStringLiteral("Layer"))) {
      type.chop(5);
    }
    if (type.isEmpty()) {
      type = QStringLiteral("Layer");
    }
    QString spaced;
    spaced.reserve(type.size() + 4);
    for (qsizetype i = 0; i < type.size(); ++i) {
      if (i > 0 && type.at(i).isUpper() && type.at(i - 1).isLower()) {
        spaced.append(QStringLiteral(" "));
      }
      spaced.append(type.at(i));
    }
    type = spaced;
  }

  const QString dimension = layer->is3D()
      ? QStringLiteral("3D") : QStringLiteral("2D");
  if (!type.contains(dimension, Qt::CaseInsensitive)) {
    type += QStringLiteral(" %1").arg(dimension);
  }
  return type;
}

QString layerNameLabel(const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) return QStringLiteral("No layer selected");
  const QString name = layer->layerName().trimmed();
  return name.isEmpty() ? QStringLiteral("Untitled Layer") : name;
}

bool layerEditModeAvailable(const ArtifactAbstractLayerPtr& layer,
                            EditMode mode)
{
  if (mode == EditMode::View) return true;
  if (!layer || !layer->isVisible() || layer->isLocked()) return false;
  if (mode == EditMode::Shape || mode == EditMode::Paint) {
    return static_cast<bool>(std::dynamic_pointer_cast<ArtifactShapeLayer>(layer));
  }
  if (mode == EditMode::Mask) return layer->maskCount() > 0;
  return true;
}

FramePosition currentLayerViewFrame()
{
  if (auto* playback = ArtifactPlaybackService::instance()) {
    return playback->currentFrame();
  }
  if (auto* project = ArtifactProjectService::instance()) {
    if (auto composition = project->currentComposition().lock()) {
      return composition->framePosition();
    }
  }
  return FramePosition{};
}

QRectF viewportSurfaceModeRect(float viewportWidth, float viewportHeight)
{
  if (viewportWidth < 220.0f || viewportHeight < 112.0f) return {};
  const float width = viewportWidth >= 980.0f
      ? 430.0f
      : viewportWidth >= 720.0f
          ? 284.0f : std::max(1.0f, viewportWidth - 16.0f);
  const float x = viewportWidth >= 720.0f
      ? (viewportWidth - width) * 0.5f : 8.0f;
  return QRectF(x, 64.0f, width, 38.0f);
}

QRectF viewportSurfaceModeItemsRect(float viewportWidth, float viewportHeight)
{
  const QRectF panel = viewportSurfaceModeRect(viewportWidth, viewportHeight);
  if (panel.isEmpty() || panel.width() <= 284.0) return panel;
  return QRectF(panel.x(), panel.y(), 284.0, panel.height());
}

QRectF viewportSurfaceSoloRect(float viewportWidth, float viewportHeight)
{
  const QRectF panel = viewportSurfaceModeRect(viewportWidth, viewportHeight);
  if (panel.isEmpty() || panel.width() <= 284.0) return {};
  return QRectF(panel.right() - 78.0, panel.y(), 78.0, panel.height());
}

QRectF viewportEditToolRect(float viewportWidth, float viewportHeight)
{
  return viewportWidth >= 1000.0f && viewportHeight >= 300.0f
      ? QRectF(16.0f, 64.0f, 64.0f, 152.0f) : QRectF{};
}

QRectF viewportDisplayModeRect(float viewportWidth, float viewportHeight)
{
  return viewportWidth >= 860.0f && viewportHeight >= 160.0f
      ? QRectF(viewportWidth - 308.0f, 112.0f, 292.0f, 38.0f) : QRectF{};
}

QRectF viewportOrientationRect(float viewportWidth, float viewportHeight)
{
  return viewportWidth >= 860.0f && viewportHeight >= 112.0f
      ? QRectF(viewportWidth - 102.0f, 16.0f, 86.0f, 86.0f)
      : QRectF{};
}

QRectF viewportZoomRect(float viewportWidth, float viewportHeight)
{
  if (viewportWidth < 190.0f || viewportHeight < 64.0f) return {};
  const float width = viewportWidth >= 720.0f
      ? 216.0f : std::min(174.0f, viewportWidth - 16.0f);
  return QRectF((viewportWidth - width) * 0.5f, 16.0f, width, 38.0f);
}

float viewportZoomStop(float panelWidth, int stop)
{
  switch (stop) {
  case 1: return panelWidth * 0.24f;
  case 2: return panelWidth * 0.61f;
  case 3: return panelWidth * 0.805f;
  case 4: return panelWidth;
  default: return 0.0f;
  }
}

int viewportZoomControlIndex(float panelWidth, float relativeX)
{
  if (relativeX < viewportZoomStop(panelWidth, 1)) return 0;
  if (relativeX < viewportZoomStop(panelWidth, 2)) return 1;
  if (relativeX < viewportZoomStop(panelWidth, 3)) return 2;
  return 3;
}

QRectF viewportStateCardRect(float viewportWidth, float viewportHeight)
{
  if (viewportWidth < 980.0f || viewportHeight < 220.0f) return {};
  const float width = std::clamp(viewportWidth * 0.22f, 220.0f, 310.0f);
  return QRectF((viewportWidth - width) * 0.5f,
                std::max(68.0f, viewportHeight - 52.0f),
                width, 36.0f);
}

QString viewportChromeToolTip(int control)
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
  case 42: return QStringLiteral("Toggle layer solo (Alt+S)");
  case 43: return QStringLiteral("Toggle layer solo (Alt+S)");
  default: return {};
  }
}

QString shapeOperatorTypeName(const ArtifactCore::ShapeOperatorType type)
{
  switch (type) {
  case ArtifactCore::ShapeOperatorType::TrimPaths:
    return QStringLiteral("Trim Paths");
  case ArtifactCore::ShapeOperatorType::Repeater:
    return QStringLiteral("Repeater");
  case ArtifactCore::ShapeOperatorType::MergePaths:
    return QStringLiteral("Merge Paths");
  case ArtifactCore::ShapeOperatorType::OffsetPaths:
    return QStringLiteral("Offset Paths");
  case ArtifactCore::ShapeOperatorType::PuckerBloat:
    return QStringLiteral("Pucker & Bloat");
  case ArtifactCore::ShapeOperatorType::RoundedCorners:
    return QStringLiteral("Rounded Corners");
  case ArtifactCore::ShapeOperatorType::WigglePaths:
    return QStringLiteral("Wiggle Paths");
  case ArtifactCore::ShapeOperatorType::ZigZag:
    return QStringLiteral("Zig Zag");
  case ArtifactCore::ShapeOperatorType::Twist:
    return QStringLiteral("Twist");
  case ArtifactCore::ShapeOperatorType::HandDrawnWobble:
    return QStringLiteral("Hand Drawn Wobble");
  default:
    return QStringLiteral("Unknown Operator");
  }
}

const QCursor& hudCursor(const QString& iconName,
                         const Qt::CursorShape fallbackShape,
                         const int hotX = 12,
                         const int hotY = 12)
{
  static QHash<QString, QCursor> cache;
  const QString key = iconName + QStringLiteral("|%1|%2|%3")
                                   .arg(static_cast<int>(fallbackShape))
                                   .arg(hotX)
                                   .arg(hotY);
  auto it = cache.constFind(key);
  if (it != cache.constEnd()) {
    return it.value();
  }

  QPixmap pixmap(ArtifactCore::resolveIconPath(QStringLiteral("Studio/%1").arg(iconName)));
  if (!pixmap.isNull()) {
    pixmap = pixmap.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    it = cache.insert(key, QCursor(pixmap, hotX, hotY));
    return it.value();
  }

  it = cache.insert(key, QCursor(fallbackShape));
  return it.value();
}

const QCursor& hudCursorForTransformHandle(TransformGizmo::HandleType handle,
                                           bool dragging)
{
  switch (handle) {
  case TransformGizmo::HandleType::Move:
  case TransformGizmo::HandleType::Scale_Center:
    return hudCursor(QStringLiteral("hud_cursor_move.svg"),
                     dragging ? Qt::ClosedHandCursor : Qt::SizeAllCursor);
  case TransformGizmo::HandleType::Scale_L:
  case TransformGizmo::HandleType::Scale_R:
    return hudCursor(QStringLiteral("hud_cursor_scale_horizontal.svg"),
                     Qt::SizeHorCursor);
  case TransformGizmo::HandleType::Scale_T:
  case TransformGizmo::HandleType::Scale_B:
    return hudCursor(QStringLiteral("hud_cursor_scale_vertical.svg"),
                     Qt::SizeVerCursor);
  case TransformGizmo::HandleType::Scale_TL:
  case TransformGizmo::HandleType::Scale_TR:
  case TransformGizmo::HandleType::Scale_BL:
  case TransformGizmo::HandleType::Scale_BR:
    return hudCursor(QStringLiteral("hud_cursor_scale_uniform.svg"),
                     Qt::SizeFDiagCursor);
  case TransformGizmo::HandleType::Rotate:
    return hudCursor(QStringLiteral("hud_cursor_rotate_corner.svg"),
                     Qt::CrossCursor);
  case TransformGizmo::HandleType::Anchor:
    return hudCursor(QStringLiteral("hud_cursor_anchor.svg"),
                     Qt::CrossCursor);
  default:
    return hudCursor(QStringLiteral("hud_cursor_select.svg"),
                     Qt::ArrowCursor, 2, 2);
  }
}

bool isMaskEditingMode(EditMode mode)
{
  return mode == EditMode::Mask || mode == EditMode::Paint;
}

bool isShapeEditingMode(EditMode mode)
{
  return mode == EditMode::Paint || mode == EditMode::Shape;
}

void publishModeReadout(QWidget *widget, EditMode editMode, DisplayMode displayMode)
{
  if (!widget) {
    return;
  }

  const QString editLabel = editModeLabel(editMode);
  const QString displayLabel = displayModeLabel(displayMode);
  QString surfaceLabel = widget->property("artifactSurfaceMode").toString();
  if (surfaceLabel.isEmpty()) {
    surfaceLabel = QStringLiteral("Edit");
  }
  widget->setProperty("artifactEditMode", editLabel);
  widget->setProperty("artifactDisplayMode", displayLabel);
  widget->setProperty("artifactModeSummary",
                      QStringLiteral("%1 / %2").arg(editLabel, displayLabel));
  widget->setProperty(
      "artifactViewSummary",
      QStringLiteral("%1 | %2 / %3")
          .arg(surfaceLabel, editLabel, displayLabel));
  widget->setAccessibleDescription(
      QStringLiteral("%1 surface, %2 mode, %3 display")
          .arg(surfaceLabel, editLabel, displayLabel));
}

void publishLayerReadout(QWidget* widget,
                         const ArtifactAbstractLayerPtr& layer)
{
  if (!widget) return;
  const QString name = layerNameLabel(layer);
  widget->setProperty("artifactLayerName", name);
  widget->setProperty("artifactLayerType", layerTypeLabel(layer));
  widget->setProperty("artifactLayerVisible", layer && layer->isVisible());
  widget->setProperty("artifactLayerLocked", layer && layer->isLocked());
  widget->setProperty("artifactLayerSolo", layer && layer->isSolo());
  widget->setProperty(
      "artifactLayerActive",
      layer && layer->isActiveAt(currentLayerViewFrame()));
  widget->setProperty(
      "artifactLayerCacheState",
      !layer || !layer->usesLayerCache()
          ? QStringLiteral("Off")
          : layer->isDirty() ? QStringLiteral("Dirty")
                             : QStringLiteral("Ready"));
  widget->setAccessibleName(layer
      ? QStringLiteral("Layer Solo View — %1").arg(name)
      : QStringLiteral("Layer Solo View"));
}

QImage makeMayaGradientSprite(const QSize& size, const FloatColor& bgColor)
{
  const int w = std::max(1, size.width());
  const int h = std::max(1, size.height());
  QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);

  QPainter painter(&image);
  QLinearGradient grad(0.0, 0.0, 0.0, static_cast<qreal>(h));
  grad.setColorAt(0.00, QColor::fromRgbF(0.26f, 0.32f, 0.38f, 1.0f));
  grad.setColorAt(0.07, QColor::fromRgbF(0.24f, 0.30f, 0.36f, 1.0f));
  grad.setColorAt(0.14, QColor::fromRgbF(0.21f, 0.27f, 0.33f, 1.0f));
  grad.setColorAt(0.22, QColor::fromRgbF(0.19f, 0.25f, 0.30f, 1.0f));
  grad.setColorAt(0.30, QColor::fromRgbF(0.17f, 0.22f, 0.27f, 1.0f));
  grad.setColorAt(0.41, QColor::fromRgbF(0.15f, 0.20f, 0.25f, 1.0f));
  grad.setColorAt(0.52, QColor::fromRgbF(0.13f, 0.17f, 0.22f, 1.0f));
  grad.setColorAt(0.64, QColor::fromRgbF(0.12f, 0.15f, 0.20f, 1.0f));
  grad.setColorAt(0.76, QColor::fromRgbF(0.10f, 0.13f, 0.17f, 1.0f));
  grad.setColorAt(0.88, QColor::fromRgbF(0.09f, 0.12f, 0.15f, 1.0f));
  grad.setColorAt(1.00, QColor::fromRgbF(0.08f, 0.10f, 0.13f, 1.0f));
  painter.fillRect(image.rect(), grad);

  QLinearGradient glow(0.0, 0.0, static_cast<qreal>(w), static_cast<qreal>(h));
  QColor tint = QColor::fromRgbF(bgColor.r(), bgColor.g(), bgColor.b(), 1.0f);
  tint.setAlpha(72);
  glow.setColorAt(0.0, tint.lighter(112));
  QColor tintDark = tint.darker(140);
  tintDark.setAlpha(28);
  glow.setColorAt(1.0, tintDark);
  painter.fillRect(image.rect(), glow);
  return image;
}

QSize physicalViewportSize(const QWidget* widget)
{
 if (!widget) {
  return {};
 }
 const qreal dpr = widget->devicePixelRatio();
 return QSize(std::max(1, static_cast<int>(std::lround(widget->width() * dpr))),
              std::max(1, static_cast<int>(std::lround(widget->height() * dpr))));
}

class ShapeEditCommand final : public Artifact::UndoCommand {public:
 ShapeEditCommand(Artifact::ArtifactAbstractLayerPtr layer,
                  std::vector<QPointF> beforePoints,
                  std::vector<QPointF> afterPoints)
     : layer_(layer),
       beforePoints_(std::move(beforePoints)),
       afterPoints_(std::move(afterPoints)) {}

 void undo() override {
  applySnapshot(beforePoints_);
 }

 void redo() override {
  applySnapshot(afterPoints_);
 }

 QString label() const override {
  return QStringLiteral("Edit Polygon");
 }

private:
 void applySnapshot(const std::vector<QPointF>& points) {
  auto layer = layer_.lock();
  if (!layer) {
   return;
  }
  auto shape = std::dynamic_pointer_cast<Artifact::ArtifactShapeLayer>(layer);
  if (!shape) {
   return;
  }
  if (points.size() >= 3) {
   shape->setCustomPolygonPoints(points, true);
  } else {
   shape->clearCustomPolygonPoints();
  }
  if (auto* mgr = Artifact::UndoManager::instance()) {
   mgr->notifyAnythingChanged();
  }
 }

 Artifact::ArtifactAbstractLayerWeak layer_;
 std::vector<QPointF> beforePoints_;
 std::vector<QPointF> afterPoints_;
};

class CornerRadiusEditCommand final : public Artifact::UndoCommand {
public:
 CornerRadiusEditCommand(Artifact::ArtifactAbstractLayerPtr layer, float before, float after)
     : layer_(std::move(layer)), before_(before), after_(after) {}
 void undo() override { apply(before_); }
 void redo() override { apply(after_); }
 QString label() const override { return QStringLiteral("Edit Corner Radius"); }
private:
 void apply(float r) {
  auto layer = layer_.lock();
  if (!layer) return;
  if (auto shape = std::dynamic_pointer_cast<Artifact::ArtifactShapeLayer>(layer))
   shape->setCornerRadius(r);
  if (auto* mgr = Artifact::UndoManager::instance()) mgr->notifyAnythingChanged();
 }
 Artifact::ArtifactAbstractLayerWeak layer_;
 float before_, after_;
};

class StarInnerRadiusEditCommand final : public Artifact::UndoCommand {
public:
 StarInnerRadiusEditCommand(Artifact::ArtifactAbstractLayerPtr layer, float before, float after)
     : layer_(std::move(layer)), before_(before), after_(after) {}
 void undo() override { apply(before_); }
 void redo() override { apply(after_); }
 QString label() const override { return QStringLiteral("Edit Star Inner Radius"); }
private:
 void apply(float r) {
  auto layer = layer_.lock();
  if (!layer) return;
  if (auto shape = std::dynamic_pointer_cast<Artifact::ArtifactShapeLayer>(layer))
   shape->setStarInnerRadius(r);
  if (auto* mgr = Artifact::UndoManager::instance()) mgr->notifyAnythingChanged();
 }
 Artifact::ArtifactAbstractLayerWeak layer_;
 float before_, after_;
};

class PathVertexEditCommand final : public Artifact::UndoCommand {
public:
 PathVertexEditCommand(Artifact::ArtifactAbstractLayerPtr layer,
                       std::vector<Artifact::CustomPathVertex> before,
                       std::vector<Artifact::CustomPathVertex> after,
                       bool beforeClosed, bool afterClosed)
     : layer_(std::move(layer)),
       before_(std::move(before)), after_(std::move(after)),
       beforeClosed_(beforeClosed), afterClosed_(afterClosed) {}
 void undo() override { apply(before_, beforeClosed_); }
 void redo() override { apply(after_, afterClosed_); }
 QString label() const override { return QStringLiteral("Edit Path Vertices"); }
private:
 void apply(const std::vector<Artifact::CustomPathVertex>& verts, bool closed) {
  auto layer = layer_.lock();
  if (!layer) return;
  auto shape = std::dynamic_pointer_cast<Artifact::ArtifactShapeLayer>(layer);
  if (!shape) return;
  if (verts.size() >= 3)
   shape->setCustomPathVertices(verts, closed);
  else
   shape->clearCustomPath();
  if (auto* mgr = Artifact::UndoManager::instance()) mgr->notifyAnythingChanged();
 }
 Artifact::ArtifactAbstractLayerWeak layer_;
 std::vector<Artifact::CustomPathVertex> before_, after_;
 bool beforeClosed_, afterClosed_;
};

class ShapeConversionCommand final : public Artifact::UndoCommand {
public:
 ShapeConversionCommand(Artifact::ArtifactAbstractLayerPtr layer,
                        std::vector<QPointF> beforePolygon,
                        bool beforePolygonClosed,
                        std::vector<Artifact::CustomPathVertex> beforePath,
                        bool beforePathClosed,
                        std::vector<QPointF> afterPolygon,
                        bool afterPolygonClosed,
                        std::vector<Artifact::CustomPathVertex> afterPath,
                        bool afterPathClosed)
     : layer_(std::move(layer)),
       beforePolygon_(std::move(beforePolygon)),
       beforePolygonClosed_(beforePolygonClosed),
       beforePath_(std::move(beforePath)),
       beforePathClosed_(beforePathClosed),
       afterPolygon_(std::move(afterPolygon)),
       afterPolygonClosed_(afterPolygonClosed),
       afterPath_(std::move(afterPath)),
       afterPathClosed_(afterPathClosed) {}

 void undo() override { apply(beforePolygon_, beforePolygonClosed_, beforePath_, beforePathClosed_); }
 void redo() override { apply(afterPolygon_, afterPolygonClosed_, afterPath_, afterPathClosed_); }
 QString label() const override { return QStringLiteral("Convert Shape Structure"); }

private:
 void apply(const std::vector<QPointF>& polygon, bool polygonClosed,
            const std::vector<Artifact::CustomPathVertex>& path, bool pathClosed) {
  auto layer = layer_.lock();
  if (!layer) {
   return;
  }
  auto shape = std::dynamic_pointer_cast<Artifact::ArtifactShapeLayer>(layer);
  if (!shape) {
   return;
  }
  if (path.size() >= 3) {
   shape->setCustomPathVertices(path, pathClosed);
  } else {
   shape->clearCustomPath();
  }
  if (polygon.size() >= 3) {
   shape->setCustomPolygonPoints(polygon, polygonClosed);
  } else {
   shape->clearCustomPolygonPoints();
  }
  if (auto* mgr = Artifact::UndoManager::instance()) {
   mgr->notifyAnythingChanged();
  }
 }

 Artifact::ArtifactAbstractLayerWeak layer_;
 std::vector<QPointF> beforePolygon_;
 bool beforePolygonClosed_;
 std::vector<Artifact::CustomPathVertex> beforePath_;
 bool beforePathClosed_;
 std::vector<QPointF> afterPolygon_;
 bool afterPolygonClosed_;
 std::vector<Artifact::CustomPathVertex> afterPath_;
 bool afterPathClosed_;
};

enum class MaskHandleType { None, InTangent, OutTangent };

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

constexpr float kMinProportionalEditRadius = 8.0f;
constexpr float kMaxProportionalEditRadius = 4096.0f;

float proportionalEditWeight(const qreal distance, const qreal radius)
{
 if (radius <= 0.0) {
  return distance <= 0.0 ? 1.0f : 0.0f;
 }
 if (distance >= radius) {
  return 0.0f;
 }
 const qreal t = std::clamp(1.0 - (distance / radius), 0.0, 1.0);
 return static_cast<float>(t * t * (3.0 - 2.0 * t));
}

std::vector<QPointF> buildShapeEditSeedPoints(const ArtifactShapeLayer& shape)
{
 const float w = static_cast<float>(std::max(1, shape.shapeWidth()));
 const float h = static_cast<float>(std::max(1, shape.shapeHeight()));
 const float cx = w * 0.5f;
 const float cy = h * 0.5f;

 switch (shape.shapeType()) {
 case ShapeType::Rect:
  return {QPointF(0.0, 0.0), QPointF(w, 0.0), QPointF(w, h), QPointF(0.0, h)};
 case ShapeType::Square: {
  const float side = std::min(w, h);
  const float left = (w - side) * 0.5f;
  const float top = (h - side) * 0.5f;
  return {QPointF(left, top), QPointF(left + side, top),
          QPointF(left + side, top + side), QPointF(left, top + side)};
 }
 case ShapeType::Triangle:
  return {QPointF(cx, 0.0f), QPointF(w, h), QPointF(0.0f, h)};
 case ShapeType::Ellipse: {
  const int segments = 16;
  std::vector<QPointF> points;
  points.reserve(static_cast<size_t>(segments));
  for (int i = 0; i < segments; ++i) {
   const float angle = static_cast<float>(i) * 2.0f * static_cast<float>(M_PI) /
                       static_cast<float>(segments);
   points.push_back(QPointF(cx + std::cos(angle) * cx,
                            cy + std::sin(angle) * cy));
  }
  return points;
 }
 case ShapeType::Star: {
  const int pts = std::max(3, shape.starPoints());
  const float outerR = std::min(cx, cy);
  const float innerR = outerR * std::clamp(shape.starInnerRadius(), 0.0f, 1.0f);
  std::vector<QPointF> points;
  points.reserve(static_cast<size_t>(pts * 2));
  for (int i = 0; i < pts * 2; ++i) {
   const float angle = static_cast<float>(i) * static_cast<float>(M_PI) /
                       static_cast<float>(pts) - static_cast<float>(M_PI) * 0.5f;
   const float r = (i % 2 == 0) ? outerR : innerR;
   points.push_back(QPointF(cx + r * std::cos(angle),
                            cy + r * std::sin(angle)));
  }
  return points;
 }
 case ShapeType::Polygon: {
  const int sides = std::max(3, shape.polygonSides());
  const float r = std::min(cx, cy);
  std::vector<QPointF> points;
  points.reserve(static_cast<size_t>(sides));
  for (int i = 0; i < sides; ++i) {
   const float angle = static_cast<float>(i) * 2.0f * static_cast<float>(M_PI) /
                       static_cast<float>(sides) - static_cast<float>(M_PI) * 0.5f;
   points.push_back(QPointF(cx + r * std::cos(angle),
                            cy + r * std::sin(angle)));
  }
  return points;
 }
 case ShapeType::Line:
  break;
 }
 return {};
}

bool hitTestMaskHandle(const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos,
                       float threshold, int& maskIndex, int& pathIndex, int& vertexIndex,
                       MaskHandleType& handleType)
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
        for (MaskHandleType candidate : {MaskHandleType::InTangent, MaskHandleType::OutTangent}) {
          if ((candidate == MaskHandleType::InTangent && vertex.inTangent == QPointF(0, 0)) ||
              (candidate == MaskHandleType::OutTangent && vertex.outTangent == QPointF(0, 0))) {
            continue;
          }
          const QPointF handlePos = maskHandlePosition(path, v, candidate);
          const QPointF delta = handlePos - localPos;
          if (QPointF::dotProduct(delta, delta) <= thresholdSq) {
            maskIndex = m;
            pathIndex = p;
            vertexIndex = v;
            handleType = candidate;
            return true;
          }
        }
      }
    }
  }
  return false;
}

FloatColor brightenColor(const FloatColor& color, const float factor)
{
  return {
      std::clamp(color.r() * factor, 0.0f, 1.0f),
      std::clamp(color.g() * factor, 0.0f, 1.0f),
      std::clamp(color.b() * factor, 0.0f, 1.0f),
      color.a()};
}

FloatColor withAlpha(const FloatColor& color, const float alpha)
{
  return {color.r(), color.g(), color.b(), std::clamp(alpha, 0.0f, 1.0f)};
}

FloatColor lerpColor(const FloatColor& a, const FloatColor& b, const float t)
{
  const float u = std::clamp(t, 0.0f, 1.0f);
  return {
      a.r() + (b.r() - a.r()) * u,
      a.g() + (b.g() - a.g()) * u,
      a.b() + (b.b() - a.b()) * u,
      a.a() + (b.a() - a.a()) * u};
}

FloatColor maskModeColor(MaskMode mode)
{
  switch (mode) {
  case MaskMode::Subtract:
    return {0.96f, 0.50f, 0.30f, 1.0f};
  case MaskMode::Intersect:
    return {0.30f, 0.82f, 0.52f, 1.0f};
  case MaskMode::Difference:
    return {0.82f, 0.50f, 0.90f, 1.0f};
  case MaskMode::Add:
  default:
    return {0.18f, 0.72f, 0.90f, 1.0f};
  }
}

void drawMaskSolidHandle(ArtifactIRenderer* renderer,
                         const Detail::float2& center,
                         const float size,
                         const FloatColor& fill,
                         const bool active)
{
  if (!renderer) {
    return;
  }

  const float half = size * 0.5f;
  const float glowPad = active ? 2.6f : 1.4f;
  const float innerInset = std::clamp(size * 0.18f, 1.0f, 2.6f);
  renderer->drawSolidRect(center.x - half - glowPad,
                          center.y - half - glowPad,
                          size + glowPad * 2.0f,
                          size + glowPad * 2.0f,
                          withAlpha(fill, active ? 0.26f : 0.12f), 1.0f);
  renderer->drawSolidRect(center.x - half + 1.2f,
                          center.y - half + 1.2f,
                          size, size,
                          {0.0f, 0.0f, 0.0f, active ? 0.42f : 0.30f}, 1.0f);
  renderer->drawSolidRect(center.x - half,
                          center.y - half,
                          size, size,
                          brightenColor(fill, active ? 1.14f : 1.04f), 1.0f);
  renderer->drawSolidRect(center.x - half + innerInset,
                          center.y - half + innerInset,
                          std::max(1.0f, size - innerInset * 2.0f),
                          std::max(1.0f, size - innerInset * 2.0f),
                          brightenColor(fill, active ? 1.28f : 1.14f), 1.0f);
  renderer->drawRectOutline(center.x - half,
                            center.y - half,
                            size, size,
                            active ? FloatColor{1.0f, 1.0f, 1.0f, 1.0f}
                                   : FloatColor{0.10f, 0.12f, 0.14f, 0.98f});
}

} // namespace

 class ArtifactLayerEditorWidgetV2::Impl {
 private:
 public:
  Impl();
  ~Impl();
  void initialize(QWidget* window);
  void initializeSwapChain(QWidget* window);
  void destroy();
  std::unique_ptr<ArtifactIRenderer> renderer_;
  std::unique_ptr<CompositionRenderer> compositionRenderer_;
  bool initialized_ = false;
  bool isPanning_=false;
  QPointF lastMousePos_;
  float zoomLevel_ = 1.0f;
  QPointer<QWidget> widget_;
  //bool isPanning_ = false;
  bool isPlay_ = false;
  std::atomic_bool running_{ false };
  QTimer* renderTimer_ = nullptr;
  std::mutex resizeMutex_;
  quint64 renderTickCount_ = 0;
  quint64 renderExecutedCount_ = 0;
  std::atomic_bool renderRequestPending_{ false };
  bool renderRequestScheduled_ = false;
  bool renderInProgress_ = false;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  
  
 bool released = true;
  bool m_initialized;
  std::unique_ptr<TransformGizmo> transformGizmo_;
 RefCntAutoPtr<ITexture> m_layerRT;
 RefCntAutoPtr<IFence> m_layer_fence;
  LayerBackgroundMode backgroundMode_ = LayerBackgroundMode::Alpha;
  LayerSurfaceMode surfaceMode_ = LayerSurfaceMode::Edit;
  EditMode editModeBeforeSurface_ = EditMode::View;
  EditMode editMode_ = EditMode::View;
  DisplayMode displayMode_ = DisplayMode::Color;
  DisplayMode displayModeBeforeMask_ = DisplayMode::Color;
  int hoveredChromeControl_ = -1;
  bool surfaceInfoDirty_ = true;
  LayerID surfaceInfoLayerId_{};
  LayerSurfaceMode surfaceInfoMode_ = LayerSurfaceMode::Edit;
  QString surfaceInfoTitle_;
  QString surfaceInfoBody_;
  std::vector<LayerID> impactParentLayerIds_;
  std::vector<LayerID> impactChildLayerIds_;
  std::vector<LayerID> impactMatteLayerIds_;
  std::vector<LayerID> impactDependentLayerIds_;
  QImage cachedMayaGradientSprite_;
  QSize cachedMayaGradientSize_;
  LayerID targetLayerId_{};
  FloatColor targetLayerTint_{ 1.0f, 0.5f, 0.5f, 1.0f };
  FloatColor clearColor_{ 0.10f, 0.10f, 0.10f, 1.0f };
  bool isDraggingMaskVertex_ = false;
  int draggingMaskIndex_ = -1;
  int draggingPathIndex_ = -1;
  int draggingVertexIndex_ = -1;
  int draggingMaskHandleType_ = -1;
  int hoveredMaskIndex_ = -1;
  int hoveredPathIndex_ = -1;
  int hoveredVertexIndex_ = -1;
  int hoveredMaskHandleType_ = -1;
  bool maskEditPending_ = false;
  bool maskEditDirty_ = false;
  ArtifactAbstractLayerWeak maskEditLayer_;
  std::vector<LayerMask> maskEditBefore_;
  bool proportionalEditingEnabled_ = false;
  float proportionalEditRadius_ = 96.0f;
  bool proportionalDragActive_ = false;
  QPointF proportionalDragOrigin_;
  std::vector<MaskVertex> proportionalMaskVerticesBefore_;
  std::vector<QPointF> proportionalShapePointsBefore_;
  std::vector<CustomPathVertex> proportionalPathVerticesBefore_;
  bool isDraggingMaskHandle_ = false;
  bool isDraggingShapeVertex_ = false;
  int draggingShapeVertexIndex_ = -1;
   int hoveredShapeVertexIndex_ = -1;
   int hoveredShapeSegmentIndex_ = -1;
   std::vector<int> selectedShapeVertexIndices_;
   std::vector<QPointF> selectedShapeDragBefore_;
  QPointF shapeContextMenuCanvasPos_;
  bool shapeEditPending_ = false;
  bool shapeEditDirty_ = false;
  ArtifactAbstractLayerWeak shapeEditLayer_;
  std::vector<QPointF> shapeEditBefore_;
  
  void defaultHandleKeyPressEvent(QKeyEvent* event);
  void setSurfaceMode(LayerSurfaceMode nextMode);
  bool toggleLayerState(int stateIndex);
  bool isSolidLayerForPreview(const ArtifactAbstractLayerPtr& layer);
  bool tryGetSolidPreviewColor(const ArtifactAbstractLayerPtr& layer, FloatColor& outColor);
  void defaultHandleKeyReleaseEvent(QKeyEvent* event);
  void recreateSwapChain(QWidget* window);
  void recreateSwapChainInternal(QWidget* window);
  ArtifactAbstractLayerPtr targetLayer() const;
  void beginMaskEditTransaction(const ArtifactAbstractLayerPtr& layer);
  void markMaskEditDirty();
  void commitMaskEditTransaction();
  void resetProportionalDragState();
  void beginMaskProportionalDragSnapshot(const MaskPath& path, int vertexIndex);
  void beginShapeProportionalDragSnapshot(const std::vector<QPointF>& points, int vertexIndex);
  void beginPathProportionalDragSnapshot(const std::vector<CustomPathVertex>& verts, int vertexIndex);
  bool applyMaskProportionalDrag(const ArtifactAbstractLayerPtr& layer, const QPointF& localPos);
  bool applyShapeProportionalDrag(const ArtifactAbstractLayerPtr& layer,
                                  ArtifactShapeLayer& shape,
                                  const QPointF& localPos);
  bool applyPathProportionalDrag(ArtifactShapeLayer& shape, const QPointF& localPos);
  QString proportionalEditingStatusText() const;
  bool hitTestMaskVertex(const ArtifactAbstractLayerPtr& layer,
                         const QPointF& canvasPos,
                         int& maskIndex,
                         int& pathIndex,
                         int& vertexIndex) const;
  void updateMaskHover(const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos);
  void drawMaskOverlay(const ArtifactAbstractLayerPtr& layer);
  bool deleteHoveredMaskVertex(const ArtifactAbstractLayerPtr& layer);

  void beginShapeEditTransaction(const ArtifactAbstractLayerPtr& layer);
  void markShapeEditDirty();
  void commitShapeEditTransaction();
  bool hitTestShapeVertex(const ArtifactAbstractLayerPtr& layer,
                          const QPointF& canvasPos,
                          int& vertexIndex) const;
  bool hitTestShapeSegment(const ArtifactAbstractLayerPtr& layer,
                           const QPointF& canvasPos,
                           int& insertIndex) const;
  void updateShapeHover(const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos);
  void drawShapeOverlay(const ArtifactAbstractLayerPtr& layer);
  void drawTransformOverlay(const ArtifactAbstractLayerPtr& layer);
  bool insertPointOnHoveredShapeSegment(const ArtifactAbstractLayerPtr& layer,
                                        const QPointF& canvasPos);
  bool deleteHoveredShapeVertex(const ArtifactAbstractLayerPtr& layer);
  bool splitHoveredShapeSegment(const ArtifactAbstractLayerPtr& layer);
  void syncTransformGizmo(const ArtifactAbstractLayerPtr& layer);

  // Phase 1: Parametric shape handles
  bool isDraggingCornerRadius_ = false;
  float cornerRadiusDragStart_ = 0.0f;
  float cornerRadiusDragAnchorX_ = 0.0f;
  float cornerRadiusDragMaxCr_ = 0.0f;
  bool hoveredCornerRadius_ = false;
  bool isDraggingStarInnerRadius_ = false;
  float starInnerRadiusDragStart_ = 0.0f;
  QPointF starInnerRadiusDragCenter_;
  float starInnerRadiusDragOuterR_ = 0.0f;
  bool hoveredStarInnerRadius_ = false;
  ArtifactAbstractLayerWeak paramHandleEditLayer_;
  float cornerRadiusBefore_ = 0.0f;
  float starInnerRadiusBefore_ = 0.0f;

  // Phase 5: Bezier path editing
  bool isDraggingPathVertex_ = false;
  bool isDraggingPathTangent_ = false;
  int draggingPathVertexIndex_ = -1;
  int draggingPathTangentType_ = 0; // 0=in, 1=out
  int hoveredPathVertexIndex_ = -1;
  int hoveredPathTangentIndex_ = -1;
   int hoveredPathTangentType_ = 0;
   std::vector<int> selectedPathVertexIndices_;
   std::vector<CustomPathVertex> selectedPathDragBefore_;
  bool pathEditPending_ = false;
  bool pathEditDirty_ = false;
  ArtifactAbstractLayerWeak pathEditLayer_;
  std::vector<CustomPathVertex> pathEditBefore_;
  bool pathEditBeforeClosed_ = true;

  QPointF cornerRadiusHandleCanvasPos(const ArtifactShapeLayer& shape) const;
  QPointF starInnerRadiusHandleCanvasPos(const ArtifactShapeLayer& shape) const;
  bool hitTestCornerRadiusHandle(const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos) const;
  bool hitTestStarInnerRadiusHandle(const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos) const;
  void drawShapeParamHandles(const ArtifactAbstractLayerPtr& layer);
  void drawTransformHUD(const ArtifactAbstractLayerPtr& layer);
  void drawSurfaceOverlay(const ArtifactAbstractLayerPtr& layer);
  void drawViewportChrome(const ArtifactAbstractLayerPtr& layer);
  void refreshSurfaceInfo(const ArtifactAbstractLayerPtr& layer);
  bool handleViewportChromePress(const QPointF& viewportPos);
  bool updateViewportChromeHover(const QPointF& viewportPos);
  void drawCustomPathOverlay(const ArtifactAbstractLayerPtr& layer);
  void beginPathEditTransaction(const ArtifactAbstractLayerPtr& layer);
  void markPathEditDirty();
  void commitPathEditTransaction();
  bool hitTestCustomPathVertex(const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos, int& vertexIndex) const;
  bool hitTestCustomPathTangent(const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos, int& vertexIndex, int& tangentType) const;
  
  void startRenderLoop();
  void stopRenderLoop();
  void requestRender();
  void renderOneFrame();
  void refreshBackgroundCache();
};

ArtifactLayerEditorWidgetV2::Impl::Impl()
{
 transformGizmo_ = std::make_unique<TransformGizmo>();

}

 ArtifactLayerEditorWidgetV2::Impl::~Impl()
 {

 }

 void ArtifactLayerEditorWidgetV2::Impl::initialize(QWidget* window)
 {
  widget_ = window;
  renderer_ = std::make_unique<ArtifactIRenderer>();
  renderer_->initialize(window);

  if (!renderer_ || !renderer_->isInitialized()) {
   qWarning() << "[ArtifactLayerEditorWidgetV2] renderer initialize failed for"
              << window << "size=" << (window ? window->size() : QSize())
              << "DPR=" << (window ? window->devicePixelRatio() : 0.0);
   compositionRenderer_.reset();
   renderer_.reset();
   return;
  }

  compositionRenderer_ = std::make_unique<CompositionRenderer>(*renderer_);
  initialized_ = true;
 }

 void ArtifactLayerEditorWidgetV2::Impl::initializeSwapChain(QWidget* window)
 {
  if (!renderer_) {
   return;
  }
  renderer_->recreateSwapChain(window);
  // Set the actual widget size so ViewportTransformer doesn't stay at the
  // default {1920, 1080} until the first resizeEvent fires.
  if (window && window->width() > 0 && window->height() > 0) {
   const QSize viewportSize = physicalViewportSize(window);
   renderer_->setViewportSize(static_cast<float>(viewportSize.width()),
                              static_cast<float>(viewportSize.height()));
  }
 }

void ArtifactLayerEditorWidgetV2::Impl::destroy()
{
  // Stop EventBus callbacks before invalidating the render receiver.  Hidden
  // layer views can receive FrameChangedEvent before their first showEvent.
  eventBusSubscriptions_.clear();
  renderRequestPending_.store(false, std::memory_order_release);
  renderRequestScheduled_ = false;
  stopRenderLoop();
  if (transformGizmo_) {
   transformGizmo_->setLayer(nullptr);
  }
  if (renderer_) {
   renderer_->destroy();
   renderer_.reset();
  }
  compositionRenderer_.reset();
  initialized_ = false;
  widget_.clear();
 }

void ArtifactLayerEditorWidgetV2::Impl::setSurfaceMode(
    LayerSurfaceMode nextMode)
{
 if (!widget_ || surfaceMode_ == nextMode) return;

 if (surfaceMode_ == LayerSurfaceMode::Edit) {
  editModeBeforeSurface_ = editMode_;
 }
 surfaceMode_ = nextMode;
 const QString surfaceName = nextMode == LayerSurfaceMode::Edit
     ? QStringLiteral("Edit")
     : nextMode == LayerSurfaceMode::Inspect
         ? QStringLiteral("Inspect")
         : QStringLiteral("Impact");
 widget_->setProperty("artifactSurfaceMode", surfaceName);
 surfaceInfoDirty_ = true;

 auto* editor = static_cast<ArtifactLayerEditorWidgetV2*>(widget_.data());
 editor->setEditMode(nextMode == LayerSurfaceMode::Edit
                         ? editModeBeforeSurface_
                         : EditMode::View);
 if (transformGizmo_ && nextMode != LayerSurfaceMode::Edit) {
  transformGizmo_->setLayer(nullptr);
 }
 requestRender();
}

bool ArtifactLayerEditorWidgetV2::Impl::toggleLayerState(int stateIndex)
{
 auto layer = targetLayer();
 if (!layer || stateIndex < 0 || stateIndex > 2) return false;

 if (auto* undo = UndoManager::instance()) {
  if (stateIndex == 0) {
   undo->push(std::make_unique<SetLayerVisibilityCommand>(
       layer, !layer->isVisible()));
  } else if (stateIndex == 1) {
   undo->push(std::make_unique<SetLayerLockCommand>(
       layer, !layer->isLocked()));
  } else {
   undo->push(std::make_unique<SetLayerSoloCommand>(
       layer, !layer->isSolo()));
  }
 }

 publishLayerReadout(widget_, layer);
 surfaceInfoDirty_ = true;
 if (!layer->isVisible() || layer->isLocked()) {
  hoveredCornerRadius_ = false;
  hoveredStarInnerRadius_ = false;
  hoveredShapeVertexIndex_ = -1;
  hoveredShapeSegmentIndex_ = -1;
  hoveredPathVertexIndex_ = -1;
  hoveredPathTangentIndex_ = -1;
  hoveredMaskIndex_ = -1;
  hoveredPathIndex_ = -1;
  hoveredVertexIndex_ = -1;
  hoveredMaskHandleType_ = -1;
 }
 syncTransformGizmo(layer);
 requestRender();
 return true;
}

 void ArtifactLayerEditorWidgetV2::Impl::defaultHandleKeyPressEvent(QKeyEvent* event)
 {
  if (!event || !renderer_ || !widget_) {
   return;
  }

  const QPointF center(widget_->width() * 0.5, widget_->height() * 0.5);
  if (event->modifiers().testFlag(Qt::AltModifier)) {
   int stateIndex = -1;
   if (event->key() == Qt::Key_V) stateIndex = 0;
   if (event->key() == Qt::Key_L) stateIndex = 1;
   if (event->key() == Qt::Key_S) stateIndex = 2;
   if (stateIndex >= 0 && targetLayer()) {
    if (!event->isAutoRepeat()) {
     toggleLayerState(stateIndex);
    }
    event->accept();
    return;
   }

   if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_3) {
    const LayerSurfaceMode modes[] = {
        LayerSurfaceMode::Edit,
        LayerSurfaceMode::Inspect,
        LayerSurfaceMode::Impact};
    setSurfaceMode(modes[event->key() - Qt::Key_1]);
    event->accept();
    return;
   }

   DisplayMode nextDisplayMode;
   bool hasDisplayMode = true;
   switch (event->key()) {
   case Qt::Key_C: nextDisplayMode = DisplayMode::Color; break;
   case Qt::Key_A: nextDisplayMode = DisplayMode::Alpha; break;
   case Qt::Key_M: nextDisplayMode = DisplayMode::Mask; break;
   case Qt::Key_W: nextDisplayMode = DisplayMode::Wireframe; break;
   default: hasDisplayMode = false; break;
   }
   if (hasDisplayMode) {
    static_cast<ArtifactLayerEditorWidgetV2*>(widget_.data())
        ->setDisplayMode(nextDisplayMode);
    if (auto* app = Artifact::ApplicationService::instance()) {
     if (auto* toolService = app->toolService()) {
      toolService->setDisplayMode(nextDisplayMode);
     }
    }
    event->accept();
    return;
   }
  }
 switch (event->key()) {
  case Qt::Key_F:
   renderer_->fitToViewport();
   zoomLevel_ = renderer_->getZoom();
   requestRender();
   event->accept();
   return;
  case Qt::Key_R:
   renderer_->resetView();
   zoomLevel_ = 1.0f;
   requestRender();
   event->accept();
   return;
  case Qt::Key_1:
   zoomLevel_ = 1.0f;
   renderer_->zoomAroundViewportPoint({ static_cast<float>(center.x()), static_cast<float>(center.y()) }, zoomLevel_);
   requestRender();
   event->accept();
   return;
  case Qt::Key_Plus:
  case Qt::Key_Equal:
   zoomLevel_ = std::clamp(zoomLevel_ * 1.1f, 0.05f, 32.0f);
   renderer_->zoomAroundViewportPoint({ static_cast<float>(center.x()), static_cast<float>(center.y()) }, zoomLevel_);
   requestRender();
   event->accept();
   return;
  case Qt::Key_Minus:
  case Qt::Key_Underscore:
   zoomLevel_ = std::clamp(zoomLevel_ / 1.1f, 0.05f, 32.0f);
   renderer_->zoomAroundViewportPoint({ static_cast<float>(center.x()), static_cast<float>(center.y()) }, zoomLevel_);
   requestRender();
   event->accept();
   return;
  case Qt::Key_Left:
   renderer_->panBy(24.0f, 0.0f);
   requestRender();
   event->accept();
   return;
  case Qt::Key_Right:
   renderer_->panBy(-24.0f, 0.0f);
   requestRender();
   event->accept();
   return;
  case Qt::Key_Up:
   renderer_->panBy(0.0f, 24.0f);
   requestRender();
   event->accept();
   return;
  case Qt::Key_Down:
   renderer_->panBy(0.0f, -24.0f);
   requestRender();
   event->accept();
   return;
  default:
   break;
  }

 }

 void ArtifactLayerEditorWidgetV2::Impl::defaultHandleKeyReleaseEvent(QKeyEvent* event)
 {
  Q_UNUSED(event);
 }

 bool ArtifactLayerEditorWidgetV2::Impl::isSolidLayerForPreview(const ArtifactAbstractLayerPtr& layer)
 {
  if (!layer) {
   return false;
  }
  const auto groups = layer->getLayerPropertyGroups();
  for (const auto& group : groups) {
   if (group.name().compare(QStringLiteral("Solid"), Qt::CaseInsensitive) == 0) {
    return true;
   }
  }
  return false;
 }

 bool ArtifactLayerEditorWidgetV2::Impl::tryGetSolidPreviewColor(const ArtifactAbstractLayerPtr& layer, FloatColor& outColor)
 {
  if (!layer) {
   return false;
  }
  const auto groups = layer->getLayerPropertyGroups();
  for (const auto& group : groups) {
   if (group.name().compare(QStringLiteral("Solid"), Qt::CaseInsensitive) != 0) {
    continue;
   }
   for (const auto& property : group.allProperties()) {
    if (!property) {
     continue;
    }
    if (property->getType() != ArtifactCore::PropertyType::Color) {
     continue;
    }
    const QColor color = property->getColorValue();
    if (!color.isValid()) {
     continue;
    }
    outColor = FloatColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    return true;
   }
  }
  return false;
 }

 void ArtifactLayerEditorWidgetV2::Impl::recreateSwapChainInternal(QWidget* window)
 {

 }

 ArtifactAbstractLayerPtr ArtifactLayerEditorWidgetV2::Impl::targetLayer() const
 {
  if (targetLayerId_.isNil()) {
   return {};
  }
  if (auto* service = ArtifactProjectService::instance()) {
   if (auto composition = service->currentComposition().lock()) {
    return composition->layerById(targetLayerId_);
   }
  }
  return {};
 }

 void ArtifactLayerEditorWidgetV2::Impl::beginMaskEditTransaction(const ArtifactAbstractLayerPtr& layer)
 {
  if (!layer) {
   return;
  }
  if (maskEditPending_ && maskEditLayer_.lock() == layer) {
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

 void ArtifactLayerEditorWidgetV2::Impl::markMaskEditDirty()
 {
  if (maskEditPending_) {
   maskEditDirty_ = true;
  }
 }

void ArtifactLayerEditorWidgetV2::Impl::commitMaskEditTransaction()
{
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
  if (auto* undo = UndoManager::instance()) {
   undo->push(std::make_unique<MaskEditCommand>(layer, maskEditBefore_, std::move(afterMasks)));
  }
 maskEditBefore_.clear();
 maskEditDirty_ = false;
}

void ArtifactLayerEditorWidgetV2::Impl::resetProportionalDragState()
{
 proportionalDragActive_ = false;
 proportionalDragOrigin_ = {};
 proportionalMaskVerticesBefore_.clear();
 proportionalShapePointsBefore_.clear();
 proportionalPathVerticesBefore_.clear();
}

void ArtifactLayerEditorWidgetV2::Impl::beginMaskProportionalDragSnapshot(const MaskPath& path,
                                                                          const int vertexIndex)
{
 resetProportionalDragState();
 if (!proportionalEditingEnabled_ || vertexIndex < 0 || vertexIndex >= path.vertexCount()) {
  return;
 }
 proportionalMaskVerticesBefore_.reserve(static_cast<size_t>(path.vertexCount()));
 for (int i = 0; i < path.vertexCount(); ++i) {
  proportionalMaskVerticesBefore_.push_back(path.vertex(i));
 }
 proportionalDragOrigin_ = proportionalMaskVerticesBefore_[static_cast<size_t>(vertexIndex)].position;
 proportionalDragActive_ = true;
}

void ArtifactLayerEditorWidgetV2::Impl::beginShapeProportionalDragSnapshot(const std::vector<QPointF>& points,
                                                                           const int vertexIndex)
{
 resetProportionalDragState();
 if (!proportionalEditingEnabled_ || vertexIndex < 0 ||
     vertexIndex >= static_cast<int>(points.size())) {
  return;
 }
 proportionalShapePointsBefore_ = points;
 proportionalDragOrigin_ = points[static_cast<size_t>(vertexIndex)];
 proportionalDragActive_ = true;
}

void ArtifactLayerEditorWidgetV2::Impl::beginPathProportionalDragSnapshot(
    const std::vector<CustomPathVertex>& verts,
    const int vertexIndex)
{
 resetProportionalDragState();
 if (!proportionalEditingEnabled_ || vertexIndex < 0 ||
     vertexIndex >= static_cast<int>(verts.size())) {
  return;
 }
 proportionalPathVerticesBefore_ = verts;
 proportionalDragOrigin_ = verts[static_cast<size_t>(vertexIndex)].pos;
 proportionalDragActive_ = true;
}

bool ArtifactLayerEditorWidgetV2::Impl::applyMaskProportionalDrag(const ArtifactAbstractLayerPtr& layer,
                                                                  const QPointF& localPos)
{
 if (!layer || !proportionalDragActive_ ||
     draggingMaskIndex_ < 0 || draggingPathIndex_ < 0 || draggingVertexIndex_ < 0) {
  return false;
 }
 LayerMask mask = layer->mask(draggingMaskIndex_);
 MaskPath path = mask.maskPath(draggingPathIndex_);
 if (proportionalMaskVerticesBefore_.size() != static_cast<size_t>(path.vertexCount()) ||
     draggingVertexIndex_ >= path.vertexCount()) {
  return false;
 }
 const QPointF delta = localPos - proportionalDragOrigin_;
 for (int i = 0; i < path.vertexCount(); ++i) {
  const MaskVertex& before = proportionalMaskVerticesBefore_[static_cast<size_t>(i)];
  const float weight = proportionalEditWeight(
      QLineF(before.position, proportionalDragOrigin_).length(),
      proportionalEditRadius_);
  if (weight <= 0.0f) {
   continue;
  }
  MaskVertex updated = before;
  updated.position += delta * weight;
  path.setVertex(i, updated);
 }
 mask.setMaskPath(draggingPathIndex_, path);
 layer->setMask(draggingMaskIndex_, mask);
 markMaskEditDirty();
 return true;
}

bool ArtifactLayerEditorWidgetV2::Impl::applyShapeProportionalDrag(const ArtifactAbstractLayerPtr& layer,
                                                                   ArtifactShapeLayer& shape,
                                                                   const QPointF& localPos)
{
 if (!layer || !proportionalDragActive_ || draggingShapeVertexIndex_ < 0 ||
     proportionalShapePointsBefore_.empty() ||
     draggingShapeVertexIndex_ >= static_cast<int>(proportionalShapePointsBefore_.size())) {
  return false;
 }
 auto points = proportionalShapePointsBefore_;
 const QPointF delta = localPos - proportionalDragOrigin_;
 for (size_t i = 0; i < points.size(); ++i) {
  const QPointF& before = proportionalShapePointsBefore_[i];
  const float weight = proportionalEditWeight(
      QLineF(before, proportionalDragOrigin_).length(),
      proportionalEditRadius_);
  if (weight <= 0.0f) {
   continue;
  }
  const QPointF moved = before + delta * weight;
  points[i] = QPointF(
      std::clamp(moved.x(), 0.0, static_cast<double>(shape.shapeWidth())),
      std::clamp(moved.y(), 0.0, static_cast<double>(shape.shapeHeight())));
 }
 shape.setCustomPolygonPoints(points, shape.customPolygonClosed());
 markShapeEditDirty();
 return true;
}

bool ArtifactLayerEditorWidgetV2::Impl::applyPathProportionalDrag(ArtifactShapeLayer& shape,
                                                                  const QPointF& localPos)
{
 if (!proportionalDragActive_ || draggingPathVertexIndex_ < 0 ||
     proportionalPathVerticesBefore_.empty() ||
     draggingPathVertexIndex_ >= static_cast<int>(proportionalPathVerticesBefore_.size())) {
  return false;
 }
 auto verts = proportionalPathVerticesBefore_;
 const QPointF delta = localPos - proportionalDragOrigin_;
 for (size_t i = 0; i < verts.size(); ++i) {
  const CustomPathVertex& before = proportionalPathVerticesBefore_[i];
  const float weight = proportionalEditWeight(
      QLineF(before.pos, proportionalDragOrigin_).length(),
      proportionalEditRadius_);
  if (weight <= 0.0f) {
   continue;
  }
  verts[i].pos = before.pos + delta * weight;
 }
 shape.setCustomPathVertices(verts, shape.customPathClosed());
 markPathEditDirty();
 return true;
}

QString ArtifactLayerEditorWidgetV2::Impl::proportionalEditingStatusText() const
{
 const QString state = proportionalEditingEnabled_ ? QStringLiteral("prop on") : QStringLiteral("prop off");
 return QStringLiteral("%1 %2 / O / [ ]")
     .arg(state)
     .arg(static_cast<int>(std::lround(proportionalEditRadius_)));
}

void ArtifactLayerEditorWidgetV2::Impl::beginShapeEditTransaction(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer) {
  return;
 }
 if (shapeEditPending_ && shapeEditLayer_.lock() == layer) {
  return;
 }
 shapeEditPending_ = true;
 shapeEditDirty_ = false;
 shapeEditLayer_ = layer;
 shapeEditBefore_.clear();
 if (auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) {
  shapeEditBefore_ = shape->customPolygonPoints();
 }
}

void ArtifactLayerEditorWidgetV2::Impl::markShapeEditDirty()
{
 if (shapeEditPending_) {
  shapeEditDirty_ = true;
 }
}

void ArtifactLayerEditorWidgetV2::Impl::commitShapeEditTransaction()
{
 if (!shapeEditPending_) {
  return;
 }
 auto layer = shapeEditLayer_.lock();
 shapeEditPending_ = false;
 shapeEditLayer_.reset();
 if (!layer || !shapeEditDirty_) {
  shapeEditBefore_.clear();
  shapeEditDirty_ = false;
  return;
 }
 std::vector<QPointF> afterPoints;
 if (auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) {
  afterPoints = shape->customPolygonPoints();
 }
 if (auto* undo = UndoManager::instance()) {
  undo->push(std::make_unique<ShapeEditCommand>(layer, shapeEditBefore_, std::move(afterPoints)));
 }
 shapeEditBefore_.clear();
 shapeEditDirty_ = false;
}

bool ArtifactLayerEditorWidgetV2::Impl::hitTestShapeVertex(const ArtifactAbstractLayerPtr& layer,
                                                           const QPointF& canvasPos,
                                                           int& vertexIndex) const
{
 if (!renderer_ || !layer) {
  return false;
 }
 const auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape || !shape->hasCustomPolygon()) {
  return false;
 }
 const auto points = shape->customPolygonPoints();
 if (points.size() < 3) {
  return false;
 }
 const QTransform globalTransform = layer->getGlobalTransform();
 const float zoom = std::max(0.1f, renderer_->getZoom());
 bool invertible = false;
 const QTransform invTransform = globalTransform.inverted(&invertible);
 if (!invertible) {
  return false;
 }
 const QPointF localPos = invTransform.map(canvasPos);
 const float threshold = 8.0f / std::max(0.1f, renderer_->getZoom());
 for (int i = 0; i < static_cast<int>(points.size()); ++i) {
  if (std::hypot(points[static_cast<size_t>(i)].x() - localPos.x(),
                 points[static_cast<size_t>(i)].y() - localPos.y()) <= threshold) {
   vertexIndex = i;
   return true;
  }
 }
 return false;
}

bool ArtifactLayerEditorWidgetV2::Impl::hitTestShapeSegment(const ArtifactAbstractLayerPtr& layer,
                                                           const QPointF& canvasPos,
                                                           int& insertIndex) const
{
 if (!renderer_ || !layer) {
  return false;
 }
 const auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape || !shape->hasCustomPolygon()) {
  return false;
 }
 const auto points = shape->customPolygonPoints();
 if (points.size() < 2) {
  return false;
 }
 const QTransform globalTransform = layer->getGlobalTransform();
 bool invertible = false;
 const QTransform invTransform = globalTransform.inverted(&invertible);
 if (!invertible) {
  return false;
 }
 const QPointF localPos = invTransform.map(canvasPos);
 const float threshold = 10.0f / std::max(0.1f, renderer_->getZoom());
 const auto distanceToSegment = [](const QPointF& p, const QPointF& a, const QPointF& b) -> double {
  const QPointF ab = b - a;
  const double abLenSq = ab.x() * ab.x() + ab.y() * ab.y();
  if (abLenSq <= std::numeric_limits<double>::epsilon()) {
   return std::hypot(p.x() - a.x(), p.y() - a.y());
  }
  const QPointF ap = p - a;
  const double t = std::clamp((ap.x() * ab.x() + ap.y() * ab.y()) / abLenSq, 0.0, 1.0);
  const QPointF proj = a + ab * t;
  return std::hypot(p.x() - proj.x(), p.y() - proj.y());
 };

 const bool closed = shape->customPolygonClosed();
 const int segmentCount = closed ? static_cast<int>(points.size())
                                 : static_cast<int>(points.size()) - 1;
 double bestDistance = std::numeric_limits<double>::max();
 int bestIndex = -1;
 for (int i = 0; i < segmentCount; ++i) {
  const int next = (i + 1) % static_cast<int>(points.size());
  const double distance = distanceToSegment(localPos,
                                            points[static_cast<size_t>(i)],
                                            points[static_cast<size_t>(next)]);
  if (distance < bestDistance) {
   bestDistance = distance;
   bestIndex = i;
  }
 }
 if (bestIndex >= 0 && bestDistance <= threshold) {
  insertIndex = bestIndex + 1;
  return true;
 }
 return false;
}

void ArtifactLayerEditorWidgetV2::Impl::updateShapeHover(const ArtifactAbstractLayerPtr& layer,
                                                         const QPointF& canvasPos)
{
 hoveredShapeVertexIndex_ = -1;
 hoveredShapeSegmentIndex_ = -1;
 int vertexIndex = -1;
 if (hitTestShapeVertex(layer, canvasPos, vertexIndex)) {
  hoveredShapeVertexIndex_ = vertexIndex;
  return;
 }
 int insertIndex = -1;
 if (hitTestShapeSegment(layer, canvasPos, insertIndex)) {
  hoveredShapeSegmentIndex_ = std::max(0, insertIndex - 1);
 }
}

void ArtifactLayerEditorWidgetV2::Impl::drawShapeOverlay(const ArtifactAbstractLayerPtr& layer)
{
 if (!renderer_ || !layer) {
  return;
 }
 const auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape) return;
 if (shape->hasCustomPath()) {
  drawCustomPathOverlay(layer);
  return;
 }
 if (!shape->hasCustomPolygon()) {
  return;
 }
 const auto points = shape->customPolygonPoints();
 if (points.size() < 3) {
  return;
 }

 const QTransform globalTransform = layer->getGlobalTransform();
  const FloatColor outlineShadow = {0.0f, 0.0f, 0.0f, 0.30f};
  const FloatColor outlineColor = {0.98f, 0.72f, 0.28f, 0.96f};
  const FloatColor segmentHoverColor = {0.42f, 0.86f, 1.0f, 0.90f};
  const FloatColor pointShadowColor = {0.0f, 0.0f, 0.0f, 0.42f};
  const FloatColor pointColor = {0.98f, 0.99f, 1.0f, 1.0f};
   const FloatColor hoverColor = {1.0f, 0.76f, 0.28f, 1.0f};
   const FloatColor selectedColor = {0.28f, 0.78f, 1.0f, 1.0f};
  const FloatColor dragColor = {1.0f, 0.40f, 0.24f, 1.0f};

 Detail::float2 firstCanvasPos{};
 Detail::float2 lastCanvasPos{};
 for (int i = 0; i < static_cast<int>(points.size()); ++i) {
   const QPointF canvasPos = globalTransform.map(points[static_cast<size_t>(i)]);
  const Detail::float2 currentCanvasPos{static_cast<float>(canvasPos.x()), static_cast<float>(canvasPos.y())};
   if (i > 0) {
    renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos, 6.0f, outlineShadow);
    renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos, 3.5f, outlineColor);
  } else {
   firstCanvasPos = currentCanvasPos;
  }
  lastCanvasPos = currentCanvasPos;
 }
 if (shape->customPolygonClosed()) {
  renderer_->drawThickLineLocal(lastCanvasPos, firstCanvasPos, 6.0f, outlineShadow);
  renderer_->drawThickLineLocal(lastCanvasPos, firstCanvasPos, 3.5f, outlineColor);
 }

  if (hoveredShapeSegmentIndex_ >= 0 && hoveredShapeSegmentIndex_ < static_cast<int>(points.size())) {
   const bool closed = shape->customPolygonClosed();
   const int nextIndex = closed
                             ? (hoveredShapeSegmentIndex_ + 1) % static_cast<int>(points.size())
                             : hoveredShapeSegmentIndex_ + 1;
   if (nextIndex >= 0 && nextIndex < static_cast<int>(points.size())) {
    const QPointF a = globalTransform.map(points[static_cast<size_t>(hoveredShapeSegmentIndex_)]);
    const QPointF b = globalTransform.map(points[static_cast<size_t>(nextIndex)]);
    const Detail::float2 segA{static_cast<float>(a.x()), static_cast<float>(a.y())};
    const Detail::float2 segB{static_cast<float>(b.x()), static_cast<float>(b.y())};
    renderer_->drawThickLineLocal(segA, segB, 5.5f, segmentHoverColor);
    const QPointF mid = (a + b) * 0.5;
    renderer_->drawCircle(static_cast<float>(mid.x()), static_cast<float>(mid.y()),
                          12.0f, segmentHoverColor, 1.0f, true);
   }
  }

 for (int i = 0; i < static_cast<int>(points.size()); ++i) {
   const QPointF canvasPos = globalTransform.map(points[static_cast<size_t>(i)]);
   FloatColor currentColor = pointColor;
   float currentRadius = 16.0f;
   if (std::find(selectedShapeVertexIndices_.begin(), selectedShapeVertexIndices_.end(), i) !=
       selectedShapeVertexIndices_.end()) {
    currentColor = selectedColor;
    currentRadius = 18.0f;
   }
   if (isDraggingShapeVertex_ && draggingShapeVertexIndex_ == i) {
   currentColor = dragColor;
   currentRadius = 20.0f;
  } else if (hoveredShapeVertexIndex_ == i) {
   currentColor = hoverColor;
   currentRadius = 18.0f;
  }
  renderer_->drawCircle(static_cast<float>(canvasPos.x()),
                        static_cast<float>(canvasPos.y()),
                        currentRadius + 4.0f,
                        pointShadowColor, 1.0f, true);
 renderer_->drawCircle(static_cast<float>(canvasPos.x()),
                        static_cast<float>(canvasPos.y()),
                        currentRadius,
                        currentColor, 1.0f, true);
 }

 if (hoveredShapeVertexIndex_ >= 0 || hoveredShapeSegmentIndex_ >= 0) {
  const bool hoverVertex = hoveredShapeVertexIndex_ >= 0;
  const bool draggingVertex = isDraggingShapeVertex_;
  QFont hudFont = QApplication::font();
  hudFont.setPointSizeF(std::max(9.0, static_cast<double>(hudFont.pointSizeF())));
  const QString headline = hoverVertex
                               ? QStringLiteral("vertex %1%2")
                                     .arg(hoveredShapeVertexIndex_ + 1)
                                     .arg(draggingVertex ? QStringLiteral(" selected") : QString())
                               : QStringLiteral("segment %1").arg(hoveredShapeSegmentIndex_ + 1);
  const QString detail = hoverVertex
                             ? (draggingVertex ? QStringLiteral("dragging / delete / convert")
                                               : QStringLiteral("drag / delete / convert"))
                             : QStringLiteral("insert / split / convert");
  const QPointF hudAnchor = globalTransform.map(points.front()) + QPointF(14.0, -30.0);
  const float zoom = std::max(0.1f, renderer_->getZoom());
  const QRectF hudRect(hudAnchor, QSizeF(228.0f / zoom, 48.0f / zoom));
  renderer_->drawOverlayPanel(hudRect.x(), hudRect.y(), hudRect.width(), hudRect.height(),
                              FloatColor{0.06f, 0.09f, 0.13f, 0.88f},
                              FloatColor{0.42f, 0.72f, 0.98f, 0.90f});
  renderer_->drawText(hudRect.adjusted(8.0, 5.0, -8.0, -4.0),
                      QStringLiteral("Shape %1\n%2\n%3").arg(headline, detail, proportionalEditingStatusText()),
                      hudFont, FloatColor{0.95f, 0.97f, 1.0f, 1.0f},
                      Qt::AlignLeft | Qt::AlignVCenter);
 }
}

void ArtifactLayerEditorWidgetV2::Impl::drawTransformOverlay(const ArtifactAbstractLayerPtr& layer)
{
 if (!renderer_ || !transformGizmo_ || !layer) {
  return;
 }
 if (displayMode_ == DisplayMode::Mask || isMaskEditingMode(editMode_)) {
  return;
 }
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
 if (selectedTargets.size() > 1) {
  transformGizmo_->setTargetLayers(std::move(selectedTargets));
 } else {
  transformGizmo_->setLayer(layer);
 }
 transformGizmo_->setMode(TransformGizmo::Mode::All);
 transformGizmo_->draw(renderer_.get());
}

void ArtifactLayerEditorWidgetV2::Impl::syncTransformGizmo(const ArtifactAbstractLayerPtr& layer)
{
 if (!transformGizmo_) {
  return;
 }
 if (surfaceMode_ != LayerSurfaceMode::Edit ||
     !layer || !layer->isVisible() || layer->isLocked() ||
     displayMode_ == DisplayMode::Mask ||
     isMaskEditingMode(editMode_)) {
  transformGizmo_->setLayer(nullptr);
  return;
 }
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
 if (selectedTargets.size() > 1) {
  transformGizmo_->setTargetLayers(std::move(selectedTargets));
 } else {
  transformGizmo_->setLayer(layer);
 }
 transformGizmo_->setMode(TransformGizmo::Mode::All);
}

// ============================================================
// Phase 1: Parametric shape handles
// ============================================================

static constexpr float kParamHandleRadius = 6.0f;

QPointF ArtifactLayerEditorWidgetV2::Impl::cornerRadiusHandleCanvasPos(const ArtifactShapeLayer& shape) const
{
 // Top-right corner, inset by cornerRadius along top edge.
 // Square shapes are centered inside the layer bounds, so derive the local rect first.
 const float cr = shape.cornerRadius();
 const float w = static_cast<float>(shape.shapeWidth());
 if (shape.shapeType() == Artifact::ShapeType::Square) {
  const float h = static_cast<float>(shape.shapeHeight());
  const float side = std::min(w, h);
  const float left = (w - side) * 0.5f;
  const float top = (h - side) * 0.5f;
  return QPointF(left + side - cr, top);
 }
 return QPointF(w - cr, 0.0f);
}

QPointF ArtifactLayerEditorWidgetV2::Impl::starInnerRadiusHandleCanvasPos(const ArtifactShapeLayer& shape) const
{
 const float outerR = std::min(shape.shapeWidth(), shape.shapeHeight()) * 0.5f;
 const float innerR = outerR * shape.starInnerRadius();
 const float cx = shape.shapeWidth() * 0.5f;
 const float cy = shape.shapeHeight() * 0.5f;
 // Handle is on top axis from center, at innerR distance
 return QPointF(cx, cy - innerR);
}

bool ArtifactLayerEditorWidgetV2::Impl::hitTestCornerRadiusHandle(const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos) const
{
if (!layer) return false;
auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
if (!shape || (shape->shapeType() != Artifact::ShapeType::Rect &&
               shape->shapeType() != Artifact::ShapeType::Square)) return false;
 const QPointF localHandle = cornerRadiusHandleCanvasPos(*shape);
 const QTransform globalTransform = layer->getGlobalTransform();
 const QPointF worldHandle = globalTransform.map(localHandle);
 if (!renderer_) return false;
 const Detail::float2 vHandle = renderer_->canvasToViewport({static_cast<float>(worldHandle.x()), static_cast<float>(worldHandle.y())});
 const Detail::float2 vPos = renderer_->canvasToViewport({static_cast<float>(canvasPos.x()), static_cast<float>(canvasPos.y())});
 const float dx = vHandle.x - vPos.x;
 const float dy = vHandle.y - vPos.y;
 return std::sqrt(dx * dx + dy * dy) <= kParamHandleRadius * 1.5f;
}

bool ArtifactLayerEditorWidgetV2::Impl::hitTestStarInnerRadiusHandle(const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos) const
{
 if (!layer) return false;
 auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape || shape->shapeType() != Artifact::ShapeType::Star) return false;
 const QPointF localHandle = starInnerRadiusHandleCanvasPos(*shape);
 const QTransform globalTransform = layer->getGlobalTransform();
 const QPointF worldHandle = globalTransform.map(localHandle);
 if (!renderer_) return false;
 const Detail::float2 vHandle = renderer_->canvasToViewport({static_cast<float>(worldHandle.x()), static_cast<float>(worldHandle.y())});
 const Detail::float2 vPos = renderer_->canvasToViewport({static_cast<float>(canvasPos.x()), static_cast<float>(canvasPos.y())});
 const float dx = vHandle.x - vPos.x;
 const float dy = vHandle.y - vPos.y;
 return std::sqrt(dx * dx + dy * dy) <= kParamHandleRadius * 1.5f;
}

void ArtifactLayerEditorWidgetV2::Impl::drawShapeParamHandles(const ArtifactAbstractLayerPtr& layer)
{
if (!renderer_ || !layer) return;
auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
if (!shape) return;
const QTransform globalTransform = layer->getGlobalTransform();
const float zoom = renderer_->getZoom();
const float handleR = kParamHandleRadius / (zoom > 0.001f ? zoom : 1.0f);
if (shape->shapeType() == Artifact::ShapeType::Rect ||
    shape->shapeType() == Artifact::ShapeType::Square) {
  const QPointF localHandle = cornerRadiusHandleCanvasPos(*shape);
  const QPointF worldHandle = globalTransform.map(localHandle);
  const FloatColor col = hoveredCornerRadius_ ? FloatColor{1,0.6f,0,1} : FloatColor{0,0.7f,1,1};
  renderer_->drawCircle(
      static_cast<float>(worldHandle.x()), static_cast<float>(worldHandle.y()),
      handleR, col, 1.0f, true);
  renderer_->drawCircle(
      static_cast<float>(worldHandle.x()), static_cast<float>(worldHandle.y()),
      handleR, FloatColor{1,1,1,0.7f}, 1.0f, false);
 } else if (shape->shapeType() == Artifact::ShapeType::Star) {
  const QPointF localHandle = starInnerRadiusHandleCanvasPos(*shape);
  const QPointF worldHandle = globalTransform.map(localHandle);
  const FloatColor col = hoveredStarInnerRadius_ ? FloatColor{1,0.6f,0,1} : FloatColor{0,0.7f,1,1};
  renderer_->drawCircle(
      static_cast<float>(worldHandle.x()), static_cast<float>(worldHandle.y()),
      handleR, col, 1.0f, true);
  renderer_->drawCircle(
      static_cast<float>(worldHandle.x()), static_cast<float>(worldHandle.y()),
      handleR, FloatColor{1,1,1,0.7f}, 1.0f, false);
 }
}

// ============================================================
// Phase 4: viewport-fixed XYWH selection HUD
// ============================================================

void ArtifactLayerEditorWidgetV2::Impl::drawTransformHUD(const ArtifactAbstractLayerPtr& layer)
{
 if (!renderer_ || !widget_ || !layer) return;
 QRectF canvasBB = layer->transformedBoundingBox();
 if (transformGizmo_ && transformGizmo_->isDragging()) {
  const QRectF draggingBounds = transformGizmo_->currentCanvasBoundingRect();
  if (draggingBounds.isValid() && !draggingBounds.isEmpty()) {
   canvasBB = draggingBounds;
  }
 }
 if (!canvasBB.isValid() || canvasBB.isEmpty()) return;
 const Detail::float2 bottomRightVP = renderer_->canvasToViewport(
     {static_cast<float>(canvasBB.right()),
      static_cast<float>(canvasBB.bottom())});
 if (!std::isfinite(bottomRightVP.x) || !std::isfinite(bottomRightVP.y)) {
  return;
 }
 const float x = static_cast<float>(canvasBB.x());
 const float y = static_cast<float>(canvasBB.y());
 const float w = static_cast<float>(canvasBB.width());
 const float h = static_cast<float>(canvasBB.height());
 const QString text = QStringLiteral("X %1    Y %2\nW %3    H %4")
     .arg(QString::number(x, 'f', 0))
     .arg(QString::number(y, 'f', 0))
     .arg(QString::number(w, 'f', 0))
     .arg(QString::number(h, 'f', 0));

 const QSize viewportSize = physicalViewportSize(widget_);
 const float viewportW = static_cast<float>(std::max(1, viewportSize.width()));
 const float viewportH = static_cast<float>(std::max(1, viewportSize.height()));
 constexpr float hudW = 132.0f;
 constexpr float hudH = 54.0f;
 constexpr float gap = 8.0f;
 if (viewportW < hudW + gap * 2.0f ||
     viewportH < hudH + gap * 2.0f) {
  return;
 }
 float hudX = bottomRightVP.x + gap;
 float hudY = bottomRightVP.y + gap;
 if (hudX + hudW > viewportW - gap) {
  hudX = bottomRightVP.x - hudW - gap;
 }
 if (hudY + hudH > viewportH - gap) {
  hudY = bottomRightVP.y - hudH - gap;
 }
 hudX = std::clamp(hudX, gap, std::max(gap, viewportW - hudW - gap));
 hudY = std::clamp(hudY, gap, std::max(gap, viewportH - hudH - gap));

 const QRectF reservedChrome[] = {
     viewportZoomRect(viewportW, viewportH),
     viewportSurfaceModeRect(viewportW, viewportH),
     viewportEditToolRect(viewportW, viewportH),
     viewportDisplayModeRect(viewportW, viewportH),
     viewportOrientationRect(viewportW, viewportH),
     viewportH >= 220.0f
         ? QRectF(0.0f, std::max(0.0f, viewportH - 60.0f),
                  viewportW, 60.0f)
         : QRectF{},
 };
 for (int pass = 0; pass < 2; ++pass) {
  for (const QRectF& reserved : reservedChrome) {
   if (reserved.isEmpty() ||
       !QRectF(hudX, hudY, hudW, hudH).intersects(reserved)) {
    continue;
   }
   const float below = static_cast<float>(reserved.bottom()) + gap;
   const float above = static_cast<float>(reserved.top()) - hudH - gap;
   if (below + hudH <= viewportH - 60.0f - gap) {
    hudY = below;
   } else if (above >= gap) {
    hudY = above;
   }
  }
 }
 hudY = std::clamp(hudY, gap, viewportH - hudH - gap);

 const float savedZoom = renderer_->getZoom();
 float savedPanX = 0.0f;
 float savedPanY = 0.0f;
 renderer_->getPan(savedPanX, savedPanY);
 renderer_->setCanvasSize(viewportW, viewportH);
 renderer_->setZoom(1.0f);
 renderer_->setPan(0.0f, 0.0f);
 renderer_->setUseExternalMatrices(false);
 renderer_->drawRoundedPanel(
     hudX + 2.0f, hudY + 3.0f, hudW, hudH, 6.0f,
     FloatColor{0.0f, 0.0f, 0.0f, 0.30f},
     FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
 renderer_->drawRoundedPanel(
     hudX, hudY, hudW, hudH, 6.0f,
     FloatColor{0.045f, 0.055f, 0.067f, 0.90f},
     FloatColor{0.18f, 0.55f, 0.94f, 0.96f});
 QFont hudFont = QApplication::font();
 hudFont.setPixelSize(12);
 renderer_->drawText(QRectF(hudX + 10.0f, hudY + 5.0f,
                            hudW - 20.0f, hudH - 10.0f),
                     text, hudFont, FloatColor{0.93f, 0.95f, 0.98f, 1.0f},
                     Qt::AlignLeft | Qt::AlignVCenter, 1.0f);
 renderer_->setZoom(savedZoom);
 renderer_->setPan(savedPanX, savedPanY);
 if (auto* service = ArtifactProjectService::instance()) {
  if (auto composition = service->currentComposition().lock()) {
   const QSize compositionSize = composition->settings().compositionSize();
   if (compositionSize.width() > 0 && compositionSize.height() > 0) {
    renderer_->setCanvasSize(static_cast<float>(compositionSize.width()),
                             static_cast<float>(compositionSize.height()));
   }
  }
 }
}

void ArtifactLayerEditorWidgetV2::Impl::drawSurfaceOverlay(
    const ArtifactAbstractLayerPtr& layer)
{
 if (!renderer_ || !layer || surfaceMode_ == LayerSurfaceMode::Edit) return;

 const QRectF bounds = layer->transformedBoundingBox();
 if (!bounds.isValid() || bounds.isEmpty()) return;

 const FloatColor accent = surfaceMode_ == LayerSurfaceMode::Inspect
     ? FloatColor{0.24f, 0.66f, 1.0f, 0.96f}
     : FloatColor{1.0f, 0.58f, 0.16f, 0.96f};
 renderer_->setUseExternalMatrices(false);
 renderer_->drawDashedRectOutline(
     static_cast<float>(bounds.x()), static_cast<float>(bounds.y()),
     static_cast<float>(bounds.width()), static_cast<float>(bounds.height()),
     accent, 1.5f, 10.0f, 5.0f);
 const float zoom = std::max(0.1f, renderer_->getZoom());
 const QPointF center = bounds.center();
 const auto& transform = layer->transform3D();
 const QPointF pivot = layer->getGlobalTransform().map(
     QPointF(transform.anchorX(), transform.anchorY()));
 renderer_->drawCrosshair(static_cast<float>(pivot.x()),
                          static_cast<float>(pivot.y()),
                          9.0f / zoom,
                          FloatColor{0.02f, 0.03f, 0.04f, 0.92f});
 renderer_->drawCrosshair(static_cast<float>(pivot.x()),
                          static_cast<float>(pivot.y()),
                          7.0f / zoom, accent);
 renderer_->drawCircle(static_cast<float>(pivot.x()),
                       static_cast<float>(pivot.y()),
                       4.0f / zoom, accent, 1.0f, false);

 if (surfaceMode_ != LayerSurfaceMode::Impact) return;
 refreshSurfaceInfo(layer);
 auto* service = ArtifactProjectService::instance();
 if (!service) return;
 auto composition = service->currentComposition().lock();
 if (!composition) return;

 int linkCount = 0;
 constexpr int maxImpactLinks = 24;
 std::vector<LayerID> drawnImpactLayerIds;
 drawnImpactLayerIds.reserve(maxImpactLinks);
 const auto drawImpactLink = [&](const ArtifactAbstractLayerPtr& other,
                                 const FloatColor& color,
                                 bool incoming) {
  if (!other || other->id() == layer->id() || linkCount >= maxImpactLinks) {
   return;
  }
  if (std::find(drawnImpactLayerIds.begin(), drawnImpactLayerIds.end(),
                other->id()) != drawnImpactLayerIds.end()) {
   return;
  }
  const QRectF otherBounds = other->transformedBoundingBox();
  if (!otherBounds.isValid() || otherBounds.isEmpty()) return;
  const QPointF otherCenter = otherBounds.center();
  const auto boundaryPoint = [](const QRectF& rect,
                                const QPointF& toward) {
   const QPointF rectCenter = rect.center();
   const QPointF delta = toward - rectCenter;
   constexpr qreal epsilon = 0.001;
   if (std::abs(delta.x()) <= epsilon &&
       std::abs(delta.y()) <= epsilon) {
    return rectCenter;
   }
   qreal scaleX = std::numeric_limits<qreal>::max();
   qreal scaleY = std::numeric_limits<qreal>::max();
   if (std::abs(delta.x()) > epsilon) {
    scaleX = (rect.width() * 0.5) / std::abs(delta.x());
   }
   if (std::abs(delta.y()) > epsilon) {
    scaleY = (rect.height() * 0.5) / std::abs(delta.y());
   }
   return rectCenter + delta * std::min(scaleX, scaleY);
  };
  const QPointF targetEdge = boundaryPoint(bounds, otherCenter);
  const QPointF otherEdge = boundaryPoint(otherBounds, center);
  renderer_->drawDashedRectOutline(
      static_cast<float>(otherBounds.x()),
      static_cast<float>(otherBounds.y()),
      static_cast<float>(otherBounds.width()),
      static_cast<float>(otherBounds.height()),
      color, 1.0f / zoom, 8.0f / zoom, 4.0f / zoom);
  renderer_->drawSolidLine(
      {static_cast<float>(targetEdge.x()), static_cast<float>(targetEdge.y())},
      {static_cast<float>(otherEdge.x()),
       static_cast<float>(otherEdge.y())},
      color, 1.25f / zoom);
  const QPointF arrowFrom = incoming ? otherEdge : targetEdge;
  const QPointF arrowTip = incoming ? targetEdge : otherEdge;
  const QPointF arrowDelta = arrowTip - arrowFrom;
  const qreal arrowDistance = std::hypot(arrowDelta.x(), arrowDelta.y());
  if (arrowDistance > 0.001) {
   const QPointF direction = arrowDelta / arrowDistance;
   const QPointF perpendicular(-direction.y(), direction.x());
   const QPointF arrowBase = arrowTip - direction * (8.0 / zoom);
   const QPointF arrowWing = perpendicular * (3.5 / zoom);
   const QPointF wingA = arrowBase + arrowWing;
   const QPointF wingB = arrowBase - arrowWing;
   renderer_->drawSolidLine(
       {static_cast<float>(arrowTip.x()), static_cast<float>(arrowTip.y())},
       {static_cast<float>(wingA.x()), static_cast<float>(wingA.y())},
       color, 1.25f / zoom);
   renderer_->drawSolidLine(
       {static_cast<float>(arrowTip.x()), static_cast<float>(arrowTip.y())},
       {static_cast<float>(wingB.x()), static_cast<float>(wingB.y())},
       color, 1.25f / zoom);
  }
  renderer_->drawCircle(static_cast<float>(otherCenter.x()),
                        static_cast<float>(otherCenter.y()),
                        4.0f / zoom, color, 1.0f, true);
  drawnImpactLayerIds.push_back(other->id());
  ++linkCount;
 };

 for (const auto& id : impactParentLayerIds_) {
  drawImpactLink(composition->layerById(id), kImpactParentColor, true);
 }
 for (const auto& id : impactChildLayerIds_) {
  drawImpactLink(composition->layerById(id), kImpactChildColor, false);
 }
 for (const auto& id : impactMatteLayerIds_) {
  drawImpactLink(composition->layerById(id), kImpactMatteColor, true);
 }
 for (const auto& id : impactDependentLayerIds_) {
  drawImpactLink(composition->layerById(id), kImpactDependentColor, false);
 }
 renderer_->drawDashedRectOutline(
     static_cast<float>(bounds.x()), static_cast<float>(bounds.y()),
     static_cast<float>(bounds.width()), static_cast<float>(bounds.height()),
     accent, 1.5f, 10.0f, 5.0f);
 renderer_->drawCrosshair(static_cast<float>(pivot.x()),
                          static_cast<float>(pivot.y()),
                          7.0f / zoom, accent);
 renderer_->drawCircle(static_cast<float>(pivot.x()),
                       static_cast<float>(pivot.y()),
                       4.0f / zoom, accent, 1.0f, false);
}

void ArtifactLayerEditorWidgetV2::Impl::refreshSurfaceInfo(
    const ArtifactAbstractLayerPtr& layer)
{
 const LayerID layerId = layer ? layer->id() : LayerID{};
 if (!surfaceInfoDirty_ && surfaceInfoLayerId_ == layerId &&
     surfaceInfoMode_ == surfaceMode_) {
  return;
 }

 surfaceInfoDirty_ = false;
 surfaceInfoLayerId_ = layerId;
 surfaceInfoMode_ = surfaceMode_;
 surfaceInfoTitle_ = surfaceMode_ == LayerSurfaceMode::Inspect
     ? QStringLiteral("Inspect") : QStringLiteral("Impact");
 surfaceInfoBody_ = QStringLiteral("No layer selected");
 impactParentLayerIds_.clear();
 impactChildLayerIds_.clear();
 impactMatteLayerIds_.clear();
 impactDependentLayerIds_.clear();
 if (!layer) return;

 surfaceInfoTitle_ += QStringLiteral(" · %1").arg(layerNameLabel(layer));
 if (surfaceMode_ == LayerSurfaceMode::Inspect) {
  const FramePosition currentFrame = currentLayerViewFrame();
  surfaceInfoTitle_ += QStringLiteral(" · Frame %1")
      .arg(currentFrame.framePosition());
  const auto source = layer->sourceSize();
  const QRectF bounds = layer->transformedBoundingBox();
  const auto& transform = layer->transform3D();
  int enabledMatteCount = 0;
  for (const auto& ref : layer->matteReferences()) {
   if (ref.enabled && !ref.sourceLayerId.isNil()) {
    ++enabledMatteCount;
   }
  }
  QString effectSummary = QStringLiteral("None");
  const auto effects = layer->getEffects();
  if (!effects.empty()) {
   effectSummary.clear();
   constexpr int maxNamedEffects = 2;
   const int namedEffectCount = std::min(
       static_cast<int>(effects.size()), maxNamedEffects);
   for (int i = 0; i < namedEffectCount; ++i) {
    if (i > 0) effectSummary += QStringLiteral(" › ");
    const auto& effect = effects[static_cast<size_t>(i)];
    const QString name = effect
        ? effect->displayName().toQString().trimmed() : QString{};
    effectSummary += name.isEmpty() ? QStringLiteral("Unnamed")
        : name.size() > 18 ? name.left(17) + QStringLiteral("…") : name;
   }
   if (effects.size() > static_cast<size_t>(namedEffectCount)) {
    const int remainingEffectCount = static_cast<int>(effects.size()) -
        namedEffectCount;
    effectSummary += QStringLiteral("  +%1")
        .arg(remainingEffectCount);
   }
  }
  QString maskSummary = QStringLiteral("None");
  if (layer->maskCount() > 0) {
   const LayerMask firstMask = layer->mask(0);
   QString pathName = QStringLiteral("Mask 1");
   QString modeName = QStringLiteral("Empty");
   QString opacityText = QStringLiteral("—");
   QString invertedText;
   if (firstMask.maskPathCount() > 0) {
    const MaskPath path = firstMask.maskPath(0);
    const QString candidateName = path.name().toQString().trimmed();
    if (!candidateName.isEmpty()) {
     pathName = candidateName.size() > 16
         ? candidateName.left(15) + QStringLiteral("…")
         : candidateName;
    }
    switch (path.mode()) {
    case MaskMode::Subtract: modeName = QStringLiteral("Subtract"); break;
    case MaskMode::Intersect: modeName = QStringLiteral("Intersect"); break;
    case MaskMode::Difference: modeName = QStringLiteral("Difference"); break;
    case MaskMode::Add:
    default: modeName = QStringLiteral("Add"); break;
    }
    opacityText = QStringLiteral("%1%")
        .arg(std::clamp(path.opacity() * 100.0f, 0.0f, 100.0f), 0, 'f', 0);
    invertedText = path.isInverted() ? QStringLiteral(" · Inverted")
                                     : QString{};
   }
   const QString remainingMasks = layer->maskCount() > 1
       ? QStringLiteral(" · +%1").arg(layer->maskCount() - 1)
       : QString{};
   maskSummary = QStringLiteral("%1 · %2 · %3%4%5%6")
       .arg(pathName)
       .arg(modeName)
       .arg(opacityText)
       .arg(invertedText)
       .arg(firstMask.isEnabled() ? QString{}
                                  : QStringLiteral(" · Disabled"))
       .arg(remainingMasks);
  }
  const QString stateText = QStringLiteral("%1 · %2 · %3 · %4")
      .arg(layer->isVisible() ? QStringLiteral("Visible")
                              : QStringLiteral("Hidden"),
           layer->isLocked() ? QStringLiteral("Locked")
                             : QStringLiteral("Unlocked"),
           layer->isSolo() ? QStringLiteral("Solo")
                           : QStringLiteral("Solo off"),
           layer->isActiveAt(currentFrame) ? QStringLiteral("Active")
                                           : QStringLiteral("Out of range"));
  const QString cacheText = !layer->usesLayerCache()
      ? QStringLiteral("Off")
      : layer->isDirty() ? QStringLiteral("Dirty")
                         : QStringLiteral("Ready");
  surfaceInfoBody_ = QStringLiteral(
      "%1  ·  Stage Final  ·  Source %2 × %3\n"
      "Bounds X %4 Y %5 W %6 H %7  ·  Pivot %8, %9\n"
      "%10\n"
      "Opacity %11%  ·  %12  ·  Matte %13  ·  Cache %14\n"
      "Mask %15\n"
      "FX %16: %17")
      .arg(layerTypeLabel(layer))
      .arg(source.width)
      .arg(source.height)
      .arg(bounds.x(), 0, 'f', 0)
      .arg(bounds.y(), 0, 'f', 0)
      .arg(bounds.width(), 0, 'f', 0)
      .arg(bounds.height(), 0, 'f', 0)
      .arg(transform.anchorX(), 0, 'f', 0)
      .arg(transform.anchorY(), 0, 'f', 0)
      .arg(stateText)
      .arg(std::clamp(layer->opacity() * 100.0f, 0.0f, 100.0f), 0, 'f', 0)
      .arg(ArtifactCore::BlendModeUtils::toString(
          ArtifactCore::toBlendMode(layer->layerBlendType())))
      .arg(enabledMatteCount)
      .arg(cacheText)
      .arg(maskSummary)
      .arg(layer->effectCount())
      .arg(effectSummary);
  return;
 }

 int childCount = 0;
 int dependentCount = 0;
 int matteInputCount = 0;
 QString parentName = QStringLiteral("None");
 QString childNames;
 QString matteNames;
 QString dependentNames;
 const auto appendRelationshipName = [](QString& summary,
                                        const ArtifactAbstractLayerPtr& item,
                                        int visibleIndex) {
  if (!item || visibleIndex >= 1) return;
  QString name = layerNameLabel(item);
  if (name.size() > 16) name = name.left(15) + QStringLiteral("…");
  if (!summary.isEmpty()) summary += QStringLiteral(", ");
  summary += name;
 };
 if (const auto parent = layer->parentLayer()) {
  parentName = layerNameLabel(parent);
  if (parentName.size() > 16) {
   parentName = parentName.left(15) + QStringLiteral("…");
  }
  impactParentLayerIds_.push_back(parent->id());
 }
 for (const auto& ref : layer->matteReferences()) {
  if (ref.enabled && !ref.sourceLayerId.isNil()) {
   ++matteInputCount;
   impactMatteLayerIds_.push_back(ref.sourceLayerId);
  }
 }
 if (auto* service = ArtifactProjectService::instance()) {
  if (auto composition = service->currentComposition().lock()) {
   const auto children = composition->childLayersOf(layer->id());
   childCount = static_cast<int>(children.size());
   int namedChildCount = 0;
   for (const auto& child : children) {
    if (child) {
     impactChildLayerIds_.push_back(child->id());
     appendRelationshipName(childNames, child, namedChildCount++);
    }
   }
   int namedMatteCount = 0;
   for (const auto& matteId : impactMatteLayerIds_) {
    appendRelationshipName(
        matteNames, composition->layerById(matteId), namedMatteCount++);
   }
   int namedDependentCount = 0;
   for (const auto& candidate : composition->allLayerRef()) {
    if (!candidate || candidate->id() == layer->id()) continue;
    for (const auto& ref : candidate->matteReferences()) {
     if (ref.enabled && ref.sourceLayerId == layer->id()) {
      ++dependentCount;
      impactDependentLayerIds_.push_back(candidate->id());
      appendRelationshipName(
          dependentNames, candidate, namedDependentCount++);
      break;
     }
    }
   }
  }
 }
 const auto relationshipSummary = [](int count, const QString& names) {
  if (count <= 0 || names.isEmpty()) return QString::number(count);
  const QString overflow = count > 1
      ? QStringLiteral(", +%1").arg(count - 1) : QString{};
  return QStringLiteral("%1 (%2%3)").arg(count).arg(names, overflow);
 };
 surfaceInfoBody_ = QStringLiteral(
      "Parent %1  ·  Children %2\n"
      "Matte inputs %3  ·  Used by %4\n"
      "Effects %5  ·  Modifiers %6  ·  Masks %7")
      .arg(parentName)
      .arg(relationshipSummary(childCount, childNames))
      .arg(relationshipSummary(matteInputCount, matteNames))
      .arg(relationshipSummary(dependentCount, dependentNames))
      .arg(layer->effectCount())
      .arg(layer->modifierCount())
      .arg(layer->maskCount());
}

void ArtifactLayerEditorWidgetV2::Impl::drawViewportChrome(
    const ArtifactAbstractLayerPtr& layer)
{
 if (!renderer_ || !widget_) return;

 const QSize viewportSize = physicalViewportSize(widget_);
 const float viewportW = static_cast<float>(std::max(1, viewportSize.width()));
 const float viewportH = static_cast<float>(std::max(1, viewportSize.height()));
 const float currentZoom = renderer_->getZoom();
 float currentPanX = 0.0f;
 float currentPanY = 0.0f;
 renderer_->getPan(currentPanX, currentPanY);

 renderer_->setCanvasSize(viewportW, viewportH);
 renderer_->setZoom(1.0f);
 renderer_->setPan(0.0f, 0.0f);
 renderer_->setUseExternalMatrices(false);

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
  renderer_->drawRoundedPanel(
      x + 2.0f, y + 3.0f, width, height, radius,
      FloatColor{0.0f, 0.0f, 0.0f, 0.30f},
      FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
  renderer_->drawRoundedPanel(x, y, width, height, radius, fill, stroke);
 };

 QFont uiFont = QApplication::font();
 uiFont.setPixelSize(12);
 QFont compactFont = uiFont;
 compactFont.setPixelSize(11);

 const QRectF surfacePanel = viewportSurfaceModeRect(viewportW, viewportH);
 if (!surfacePanel.isEmpty()) {
  const QRectF surfaceItems = viewportSurfaceModeItemsRect(viewportW, viewportH);
  const float surfacePanelX = static_cast<float>(surfacePanel.x());
  const float surfacePanelY = static_cast<float>(surfacePanel.y());
  const float surfacePanelW = static_cast<float>(surfacePanel.width());
  const float surfacePanelH = static_cast<float>(surfacePanel.height());
  const float surfaceItemsX = static_cast<float>(surfaceItems.x());
  const float surfaceItemsW = static_cast<float>(surfaceItems.width());
  constexpr float surfaceInset = 4.0f;
  const float surfaceItemW = (surfaceItemsW - surfaceInset * 2.0f) / 3.0f;
  struct SurfaceItem {
   LayerSurfaceMode mode;
   const char* label;
  };
  const SurfaceItem surfaces[] = {
      {LayerSurfaceMode::Edit, "Edit"},
      {LayerSurfaceMode::Inspect, "Inspect"},
      {LayerSurfaceMode::Impact, "Impact"},
  };
  drawChromePanel(surfacePanelX, surfacePanelY,
                  surfacePanelW, surfacePanelH,
                  6.0f, panelFill, panelStroke);
  for (int i = 0; i < 3; ++i) {
   const float itemX = surfacePanelX + surfaceInset +
       surfaceItemW * static_cast<float>(i);
   const bool selected = surfaceMode_ == surfaces[i].mode;
   const bool hovered = hoveredChromeControl_ == 30 + i;
   if (hovered) {
    const FloatColor accent = surfaces[i].mode == LayerSurfaceMode::Impact
        ? amberAccent : blueAccent;
    renderer_->drawRoundedPanel(
        itemX, surfacePanelY + surfaceInset,
        surfaceItemW - 2.0f, surfacePanelH - surfaceInset * 2.0f,
        4.0f,
        FloatColor{0.18f, 0.21f, 0.25f, 0.94f},
        selected ? accent : panelStroke);
   }
   const FloatColor itemAccent = surfaces[i].mode == LayerSurfaceMode::Impact
       ? amberAccent : blueAccent;
   renderer_->drawText(
       QRectF(itemX + 3.0f, surfacePanelY + 3.0f,
              surfaceItemW - 8.0f, surfacePanelH - 6.0f),
       QString::fromLatin1(surfaces[i].label), compactFont,
       selected ? itemAccent : mutedText, Qt::AlignCenter);
   if (i < 2) {
    const float separatorX = itemX + surfaceItemW - 1.0f;
    renderer_->drawSolidLine(
        {separatorX, surfacePanelY + 10.0f},
        {separatorX, surfacePanelY + surfacePanelH - 10.0f},
        FloatColor{0.34f, 0.37f, 0.41f, 0.72f}, 1.0f);
   }
  }
  if (surfacePanelW > surfaceItemsW) {
   const float dividerX = surfaceItemsX + surfaceItemsW + 2.0f;
   renderer_->drawSolidLine(
       {dividerX, surfacePanelY + 9.0f},
       {dividerX, surfacePanelY + surfacePanelH - 9.0f},
       FloatColor{0.30f, 0.33f, 0.37f, 0.68f}, 1.0f);
   const bool soloActive = layer && layer->isSolo();
   const QRectF soloRect = viewportSurfaceSoloRect(viewportW, viewportH);
   if (hoveredChromeControl_ == 43 && layer && !soloRect.isEmpty()) {
    renderer_->drawRoundedPanel(
        static_cast<float>(soloRect.x() + 3.0),
        static_cast<float>(soloRect.y() + 4.0),
        static_cast<float>(soloRect.width() - 6.0),
        static_cast<float>(soloRect.height() - 8.0),
        4.0f,
        FloatColor{0.18f, 0.21f, 0.25f, 0.94f},
        soloActive ? amberAccent : panelStroke);
   }
   renderer_->drawText(
       QRectF(dividerX + 10.0f, surfacePanelY + 3.0f,
              48.0f, surfacePanelH - 6.0f),
       QStringLiteral("Final"), compactFont, textColor,
       Qt::AlignLeft | Qt::AlignVCenter);
   renderer_->drawCircle(dividerX + 57.0f,
                         surfacePanelY + surfacePanelH * 0.5f,
                         3.0f,
                         FloatColor{0.10f, 0.88f, 0.48f, 1.0f},
                         1.0f, true);
   renderer_->drawText(
       QRectF(dividerX + 73.0f, surfacePanelY + 3.0f,
              40.0f, surfacePanelH - 6.0f),
       QStringLiteral("Solo"), compactFont,
       soloActive ? amberAccent : mutedText,
       Qt::AlignLeft | Qt::AlignVCenter);
   renderer_->drawCircle(dividerX + 116.0f,
                         surfacePanelY + surfacePanelH * 0.5f,
                         3.0f,
                         soloActive
                             ? amberAccent
                             : FloatColor{0.46f, 0.49f, 0.54f, 1.0f},
                         1.0f, true);
  }

  if (surfaceMode_ == LayerSurfaceMode::Edit) {
   const QRectF toolPanel = viewportEditToolRect(viewportW, viewportH);
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
   const bool selected = editMode_ == tools[i].mode;
   const bool hovered = hoveredChromeControl_ == i;
   const bool enabled = layerEditModeAvailable(layer, tools[i].mode);
   if (selected || (enabled && hovered)) {
    renderer_->drawRoundedPanel(toolPanelX + toolInset, itemY,
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
   renderer_->drawText(
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
       : surfaceMode_ == LayerSurfaceMode::Inspect ? 146.0f : 112.0f;
   const FloatColor infoAccent = surfaceMode_ == LayerSurfaceMode::Inspect
       ? blueAccent : amberAccent;
   drawChromePanel(infoPanelX, infoPanelY,
                   infoPanelW, infoPanelH,
                   7.0f, panelFill, infoAccent);
   refreshSurfaceInfo(layer);
   QFont titleFont = uiFont;
   titleFont.setBold(true);
   const QFontMetrics titleMetrics(titleFont);
   const QString visibleTitle = titleMetrics.elidedText(
       surfaceInfoTitle_, Qt::ElideMiddle,
       static_cast<int>(infoPanelW - 24.0f));
   renderer_->drawText(
       QRectF(infoPanelX + 12.0f, infoPanelY + 7.0f,
              infoPanelW - 24.0f, 22.0f),
       visibleTitle, titleFont, infoAccent,
       Qt::AlignLeft | Qt::AlignVCenter);
   const QString visibleBody = compactInfo
       ? QFontMetrics(compactFont).elidedText(
             surfaceInfoBody_.section(QStringLiteral("\n"), 0, 0),
             Qt::ElideRight,
             std::max(1, static_cast<int>(infoPanelW - 24.0f)))
       : surfaceInfoBody_;
   renderer_->drawText(
       QRectF(infoPanelX + 12.0f, infoPanelY + 31.0f,
              infoPanelW - 24.0f,
              !compactInfo && surfaceMode_ == LayerSurfaceMode::Impact
                  ? infoPanelH - 59.0f : infoPanelH - 38.0f),
       visibleBody, compactFont, textColor,
       Qt::AlignLeft | Qt::AlignTop);
   if (!compactInfo && surfaceMode_ == LayerSurfaceMode::Impact) {
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
     renderer_->drawCircle(legendX + 4.0f, legendY + 8.0f,
                           3.0f, item.color, 1.0f, true);
     renderer_->drawText(
         QRectF(legendX + 11.0f, legendY,
                item.width - 11.0f, 16.0f),
         QString::fromLatin1(item.label), compactFont, mutedText,
         Qt::AlignLeft | Qt::AlignVCenter);
     legendX += item.width;
    }
   }
  }
 }

 const QRectF zoomPanel = viewportZoomRect(viewportW, viewportH);
 if (!zoomPanel.isEmpty()) {
 const float zoomPanelW = static_cast<float>(zoomPanel.width());
 const float zoomPanelH = static_cast<float>(zoomPanel.height());
 const float zoomPanelX = static_cast<float>(zoomPanel.x());
 const float zoomPanelY = static_cast<float>(zoomPanel.y());
 drawChromePanel(zoomPanelX, zoomPanelY, zoomPanelW, zoomPanelH,
                 6.0f, panelFill, panelStroke);
 if (hoveredChromeControl_ >= 20 && hoveredChromeControl_ <= 23) {
  const int zoomIndex = hoveredChromeControl_ - 20;
  const float segmentX = viewportZoomStop(zoomPanelW, zoomIndex);
  const float segmentW = viewportZoomStop(zoomPanelW, zoomIndex + 1) -
      segmentX;
  renderer_->drawRoundedPanel(zoomPanelX + segmentX + 2.0f,
                              zoomPanelY + 4.0f,
                              segmentW - 4.0f,
                              zoomPanelH - 8.0f,
                              4.0f,
                              FloatColor{0.18f, 0.21f, 0.25f, 0.94f},
                              panelStroke);
 }
 for (int i = 1; i < 4; ++i) {
  const float separatorX = viewportZoomStop(zoomPanelW, i);
  renderer_->drawSolidLine(
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
  const float segmentX = viewportZoomStop(zoomPanelW, i);
  const float segmentW = viewportZoomStop(zoomPanelW, i + 1) - segmentX;
  renderer_->drawText(
      QRectF(zoomPanelX + segmentX, zoomPanelY + 3.0f,
             segmentW, zoomPanelH - 6.0f),
      zoomLabels[i], i == 3 ? compactFont : uiFont,
      i == 1 ? textColor : mutedText, Qt::AlignCenter);
 }
 }

 const QRectF modePanel = viewportDisplayModeRect(viewportW, viewportH);
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
   const bool selected = displayMode_ == displayItems[i].mode;
   const bool hovered = hoveredChromeControl_ == 10 + i;
   if (selected || hovered) {
    renderer_->drawRoundedPanel(itemX, modePanelY + modeInset,
                                modeItemW - 2.0f, modePanelH - modeInset * 2.0f,
                                4.0f,
                                selected
                                    ? selectedFill
                                    : FloatColor{0.18f, 0.21f, 0.25f, 0.94f},
                                selected ? blueAccent : panelStroke);
   }
   renderer_->drawText(
       QRectF(itemX + 2.0f, modePanelY + 3.0f,
              modeItemW - 6.0f, modePanelH - 6.0f),
       QString::fromLatin1(displayItems[i].label), compactFont,
       selected ? textColor : mutedText, Qt::AlignCenter);
   if (i == 0 && selected) {
    renderer_->drawCircle(itemX + modeItemW - 10.0f,
                          modePanelY + modePanelH * 0.5f,
                          3.0f,
                          FloatColor{0.10f, 0.88f, 0.48f, 1.0f},
                          1.0f, true);
    }
  }
  for (int i = 1; i < 4; ++i) {
   const float separatorX = modePanelX + modeInset +
       modeItemW * static_cast<float>(i) - 1.0f;
   renderer_->drawSolidLine(
       {separatorX, modePanelY + 9.0f},
       {separatorX, modePanelY + modePanelH - 9.0f},
       FloatColor{0.30f, 0.33f, 0.37f, 0.68f}, 1.0f);
  }
 }

 const QRectF cubeRect = viewportOrientationRect(viewportW, viewportH);
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
   renderer_->drawSolidLine(
       {cubeCenterX, cubeY + 18.0f},
       {cubeCenterX, cubeY + 8.0f}, axisY, 1.5f);
   renderer_->drawSolidLine(
       {cubeX + cubeSize - 12.0f, cubeCenterY},
       {cubeX + cubeSize - 4.0f, cubeCenterY}, axisX, 1.5f);
   renderer_->drawSolidLine(
       {cubeCenterX, cubeY + 54.0f},
       {cubeCenterX, cubeY + 62.0f}, axisZ, 1.5f);
   QFont axisFont = compactFont;
   axisFont.setPixelSize(10);
   axisFont.setBold(true);
   renderer_->drawText(QRectF(cubeCenterX - 7.0f, cubeY,
                              14.0f, 13.0f),
                       QStringLiteral("Y"), axisFont, axisY,
                       Qt::AlignCenter);
   renderer_->drawText(QRectF(cubeX + cubeSize - 15.0f,
                              cubeCenterY - 7.0f, 14.0f, 14.0f),
                       QStringLiteral("X"), axisFont, axisX,
                       Qt::AlignCenter);
   renderer_->drawText(QRectF(cubeCenterX - 7.0f, cubeY + 56.0f,
                              14.0f, 13.0f),
                       QStringLiteral("Z"), axisFont, axisZ,
                       Qt::AlignCenter);
   renderer_->drawRoundedPanel(cubeX + 12.0f, cubeY + 18.0f,
                               cubeSize - 24.0f, 36.0f,
                               4.0f,
                               selectedFill,
                               blueAccent);
   renderer_->drawText(QRectF(cubeX + 12.0f, cubeY + 18.0f,
                              cubeSize - 24.0f, 36.0f),
                       layerIs3D ? QStringLiteral("Layer")
                                 : QStringLiteral("Front"),
                       compactFont, textColor, Qt::AlignCenter);
   renderer_->drawText(QRectF(cubeX + 8.0f, cubeY + 66.0f,
                              cubeSize - 16.0f, 18.0f),
                       layerIs3D ? QStringLiteral("3D")
                                 : QStringLiteral("2D"),
                       compactFont,
                       mutedText, Qt::AlignCenter);
 }

 QString canvasStateTitle;
 QString canvasStateDetail;
 if (!layer) {
  canvasStateTitle = QStringLiteral("No layer selected");
  canvasStateDetail = QStringLiteral("Select a layer to open it in Layer Solo View");
 } else {
  const FramePosition currentFrame = currentLayerViewFrame();
  if (!layer->isVisible()) {
   canvasStateTitle = QStringLiteral("Layer is hidden");
   canvasStateDetail = QStringLiteral("Enable Visible in the layer state controls");
  } else if (!layer->isActiveAt(currentFrame)) {
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
  const float contentTop = surfaceMode_ == LayerSurfaceMode::Edit
      ? 132.0f
      : surfaceMode_ == LayerSurfaceMode::Inspect ? 320.0f : 288.0f;
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
   renderer_->drawText(
       QRectF(statePanelX + 14.0f, statePanelY + 7.0f,
              statePanelW - 28.0f, 23.0f),
       QFontMetrics(stateTitleFont).elidedText(
           canvasStateTitle, Qt::ElideRight,
           std::max(1, static_cast<int>(statePanelW - 28.0f))),
       stateTitleFont, textColor,
       Qt::AlignCenter);
   renderer_->drawText(
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
 const QRectF stateCard = viewportStateCardRect(viewportW, viewportH);
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
  layerName = layerNameLabel(layer);
  layerType = layerTypeLabel(layer);
  solo = layer->isSolo();
  active = layer->isActiveAt(currentLayerViewFrame());
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
 renderer_->drawText(
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
   const bool hovered = hoveredChromeControl_ == 40 + i;
   if (stateActive[i] || hovered) {
    const FloatColor accent = i == 0 ? blueAccent : amberAccent;
    renderer_->drawRoundedPanel(
        itemX + 3.0f, bottomY + 4.0f,
        stateItemW - 6.0f, cardH - 8.0f, 4.0f,
        stateActive[i] ? FloatColor{0.10f, 0.25f, 0.38f, 0.92f}
                       : FloatColor{0.18f, 0.21f, 0.25f, 0.94f},
        stateActive[i] ? accent : panelStroke);
   }
   renderer_->drawText(
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
  renderer_->drawText(
      QRectF(rightX + 12.0f, bottomY + 3.0f,
             rightW - 30.0f, cardH - 6.0f),
      compactMetrics.elidedText(detailText, Qt::ElideRight,
                                static_cast<int>(rightW - 30.0f)),
      compactFont, textColor, Qt::AlignLeft | Qt::AlignVCenter);
  renderer_->drawCircle(rightX + rightW - 14.0f, bottomY + cardH * 0.5f,
                        4.0f,
                        cacheEnabled && !cacheDirty
                            ? FloatColor{0.10f, 0.88f, 0.48f, 1.0f}
                            : cacheEnabled
                                ? amberAccent
                                : FloatColor{0.46f, 0.49f, 0.54f, 1.0f},
                        1.0f, true);
 }
 }

 renderer_->setZoom(currentZoom);
 renderer_->setPan(currentPanX, currentPanY);
 if (auto* service = ArtifactProjectService::instance()) {
  if (auto composition = service->currentComposition().lock()) {
   const QSize compSize = composition->settings().compositionSize();
   if (compSize.width() > 0 && compSize.height() > 0) {
    renderer_->setCanvasSize(static_cast<float>(compSize.width()),
                             static_cast<float>(compSize.height()));
   }
  }
 }
}

bool ArtifactLayerEditorWidgetV2::Impl::handleViewportChromePress(
    const QPointF& viewportPos)
{
 if (!renderer_ || !widget_) return false;

 const qreal dpr = widget_->devicePixelRatioF();
 const QPointF pos = viewportPos * dpr;
 const QSize viewportSize = physicalViewportSize(widget_);
 const float viewportW = static_cast<float>(viewportSize.width());
 const float viewportH = static_cast<float>(viewportSize.height());

 const QRectF surfaceRect = viewportSurfaceModeRect(viewportW, viewportH);
 const QRectF surfaceItemsRect = viewportSurfaceModeItemsRect(viewportW, viewportH);
 if (!surfaceRect.isEmpty()) {
  const QRectF surfaceSoloRect = viewportSurfaceSoloRect(viewportW, viewportH);
  if (surfaceSoloRect.contains(pos)) {
   toggleLayerState(2);
   return true;
  }
  if (surfaceItemsRect.contains(pos)) {
   const float surfaceItemW = static_cast<float>(
       (surfaceItemsRect.width() - 8.0) / 3.0);
   const int index = std::clamp(
       static_cast<int>((pos.x() - surfaceItemsRect.left() - 4.0) / surfaceItemW),
       0, 2);
   const LayerSurfaceMode modes[] = {
       LayerSurfaceMode::Edit,
       LayerSurfaceMode::Inspect,
       LayerSurfaceMode::Impact};
   setSurfaceMode(modes[index]);
   return true;
  }

  const QRectF toolRect = viewportEditToolRect(viewportW, viewportH);
  if (!toolRect.isEmpty() && surfaceMode_ == LayerSurfaceMode::Edit &&
      toolRect.contains(pos)) {
   const int index = std::clamp(
       static_cast<int>((pos.y() - toolRect.top() - 4.0) / 36.0), 0, 3);
   const EditMode modes[] = {
       EditMode::View, EditMode::Transform, EditMode::Shape, EditMode::Mask};
   const auto layer = targetLayer();
   if (!layerEditModeAvailable(layer, modes[index])) {
    return true;
   }
   static_cast<ArtifactLayerEditorWidgetV2*>(widget_.data())->setEditMode(
       modes[index]);
   return true;
  }
 }

 const QRectF modeRect = viewportDisplayModeRect(viewportW, viewportH);
 if (!modeRect.isEmpty()) {
  if (modeRect.contains(pos)) {
   const int index = std::clamp(
       static_cast<int>((pos.x() - modeRect.left() - 4.0) / 71.0), 0, 3);
   const DisplayMode modes[] = {
       DisplayMode::Color, DisplayMode::Alpha,
       DisplayMode::Mask, DisplayMode::Wireframe};
   static_cast<ArtifactLayerEditorWidgetV2*>(widget_.data())->setDisplayMode(
       modes[index]);
   if (auto* app = Artifact::ApplicationService::instance()) {
    if (auto* toolService = app->toolService()) {
     toolService->setDisplayMode(modes[index]);
    }
   }
   return true;
  }
 }

 const QRectF stateRect = viewportStateCardRect(viewportW, viewportH);
 if (!stateRect.isEmpty() && stateRect.contains(pos)) {
  if (!targetLayer()) return false;
  const int index = std::clamp(
      static_cast<int>((pos.x() - stateRect.left()) /
                       (stateRect.width() / 3.0)), 0, 2);
  toggleLayerState(index);
  return true;
 }

 const QRectF zoomRect = viewportZoomRect(viewportW, viewportH);
 if (!zoomRect.contains(pos)) return false;

 const float relativeX = static_cast<float>(pos.x() - zoomRect.left());
 const int zoomControl = viewportZoomControlIndex(
     static_cast<float>(zoomRect.width()), relativeX);
 const QPointF center(widget_->width() * 0.5 * dpr,
                      widget_->height() * 0.5 * dpr);
 if (zoomControl == 0) {
  zoomLevel_ = std::clamp(renderer_->getZoom() / 1.1f, 0.05f, 32.0f);
  renderer_->zoomAroundViewportPoint(
      {static_cast<float>(center.x()), static_cast<float>(center.y())},
      zoomLevel_);
 } else if (zoomControl == 1) {
  zoomLevel_ = 1.0f;
  renderer_->zoomAroundViewportPoint(
      {static_cast<float>(center.x()), static_cast<float>(center.y())},
      zoomLevel_);
 } else if (zoomControl == 2) {
  zoomLevel_ = std::clamp(renderer_->getZoom() * 1.1f, 0.05f, 32.0f);
  renderer_->zoomAroundViewportPoint(
      {static_cast<float>(center.x()), static_cast<float>(center.y())},
      zoomLevel_);
 } else {
  renderer_->fitToViewport();
  zoomLevel_ = renderer_->getZoom();
 }
 requestRender();
 return true;
}

bool ArtifactLayerEditorWidgetV2::Impl::updateViewportChromeHover(
    const QPointF& viewportPos)
{
 if (!widget_) return false;

 const qreal dpr = widget_->devicePixelRatioF();
 const QPointF pos = viewportPos * dpr;
 const QSize viewportSize = physicalViewportSize(widget_);
 const float viewportW = static_cast<float>(viewportSize.width());
 const float viewportH = static_cast<float>(viewportSize.height());
 int nextControl = -1;
 bool nextControlEnabled = true;

 const QRectF surfaceRect = viewportSurfaceModeRect(viewportW, viewportH);
 const QRectF surfaceItemsRect = viewportSurfaceModeItemsRect(viewportW, viewportH);
 if (!surfaceRect.isEmpty()) {
  const QRectF surfaceSoloRect = viewportSurfaceSoloRect(viewportW, viewportH);
  if (surfaceSoloRect.contains(pos) && !targetLayerId_.isNil()) {
   nextControl = 43;
  } else if (surfaceItemsRect.contains(pos)) {
   const float surfaceItemW = static_cast<float>(
       (surfaceItemsRect.width() - 8.0) / 3.0);
   nextControl = 30 + std::clamp(
       static_cast<int>((pos.x() - surfaceItemsRect.left() - 4.0) / surfaceItemW),
       0, 2);
  }
  const QRectF toolRect = viewportEditToolRect(viewportW, viewportH);
  if (nextControl < 0 && !toolRect.isEmpty() &&
      surfaceMode_ == LayerSurfaceMode::Edit &&
      toolRect.contains(pos)) {
   nextControl = std::clamp(
       static_cast<int>((pos.y() - toolRect.top() - 4.0) / 36.0), 0, 3);
  }
 }

 const QRectF modeRect = viewportDisplayModeRect(viewportW, viewportH);
 if (nextControl < 0 && !modeRect.isEmpty()) {
  if (modeRect.contains(pos)) {
   nextControl = 10 + std::clamp(
       static_cast<int>((pos.x() - modeRect.left() - 4.0) / 71.0), 0, 3);
  }
 }

 if (nextControl < 0) {
  const QRectF stateRect = viewportStateCardRect(viewportW, viewportH);
  if (!targetLayerId_.isNil() && !stateRect.isEmpty() &&
      stateRect.contains(pos)) {
   nextControl = 40 + std::clamp(
       static_cast<int>((pos.x() - stateRect.left()) /
                        (stateRect.width() / 3.0)), 0, 2);
  }
 }

 if (nextControl < 0) {
  const QRectF zoomRect = viewportZoomRect(viewportW, viewportH);
  if (zoomRect.contains(pos)) {
   const float relativeX = static_cast<float>(pos.x() - zoomRect.left());
   nextControl = 20 + viewportZoomControlIndex(
       static_cast<float>(zoomRect.width()), relativeX);
  }
 }

 if (nextControl >= 0 && nextControl <= 3) {
  const EditMode modes[] = {
      EditMode::View, EditMode::Transform, EditMode::Shape, EditMode::Mask};
  nextControlEnabled = layerEditModeAvailable(
      targetLayer(), modes[nextControl]);
 }

 const int previousControl = hoveredChromeControl_;
 if (previousControl != nextControl) {
  hoveredChromeControl_ = nextControl;
  QString toolTip = viewportChromeToolTip(nextControl);
  if (nextControl >= 0 && nextControl <= 3) {
   if (!nextControlEnabled) {
    const auto layer = targetLayer();
    toolTip = !layer
        ? QStringLiteral("Select a layer to edit")
        : !layer->isVisible() || layer->isLocked()
            ? QStringLiteral("Unlock and show the layer to edit it")
            : nextControl == 2
                ? QStringLiteral("Shape editing is unavailable for this layer")
                : QStringLiteral("Add a mask before entering mask edit mode");
   }
  }
  widget_->setToolTip(toolTip);
  requestRender();
 }
 if (nextControl >= 0) {
  widget_->setCursor(nextControlEnabled
                         ? Qt::PointingHandCursor : Qt::ArrowCursor);
  return true;
 }
 if (previousControl >= 0) {
  const auto layer = targetLayer();
  if (isMaskEditingMode(editMode_) && layer && layer->isVisible() &&
      !layer->isLocked()) {
   widget_->setCursor(Qt::CrossCursor);
  } else {
   widget_->unsetCursor();
  }
 }
 return false;
}

// ============================================================
// Phase 5: Bezier path overlay + transactions
// ============================================================

void ArtifactLayerEditorWidgetV2::Impl::beginPathEditTransaction(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer) return;
 auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape) return;
 pathEditLayer_ = layer;
 pathEditBefore_ = shape->customPathVertices();
 pathEditBeforeClosed_ = shape->customPathClosed();
 pathEditPending_ = true;
 pathEditDirty_ = false;
}

void ArtifactLayerEditorWidgetV2::Impl::markPathEditDirty()
{
 pathEditDirty_ = true;
}

void ArtifactLayerEditorWidgetV2::Impl::commitPathEditTransaction()
{
 pathEditPending_ = false;
 if (!pathEditDirty_) return;
 auto layer = pathEditLayer_.lock();
 if (!layer) return;
 auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape) return;
 const auto afterVerts = shape->customPathVertices();
 const bool afterClosed = shape->customPathClosed();
 if (auto* undo = UndoManager::instance()) {
  undo->push(std::make_unique<PathVertexEditCommand>(
      layer, pathEditBefore_, afterVerts, pathEditBeforeClosed_, afterClosed));
 }
 pathEditBefore_.clear();
 pathEditDirty_ = false;
}

bool ArtifactLayerEditorWidgetV2::Impl::hitTestCustomPathVertex(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos, int& vertexIndex) const
{
 if (!layer || !renderer_) return false;
 auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape || !shape->hasCustomPath()) return false;
 const QTransform gt = layer->getGlobalTransform();
 const float zoom = renderer_->getZoom();
 const float snapR = 8.0f / (zoom > 0.001f ? zoom : 1.0f);
 const auto& verts = shape->customPathVertices();
 for (int i = 0; i < static_cast<int>(verts.size()); ++i) {
  const QPointF world = gt.map(verts[i].pos);
  const float dx = static_cast<float>(world.x() - canvasPos.x());
  const float dy = static_cast<float>(world.y() - canvasPos.y());
  if (std::sqrt(dx * dx + dy * dy) <= snapR) {
   vertexIndex = i;
   return true;
  }
 }
 return false;
}

bool ArtifactLayerEditorWidgetV2::Impl::hitTestCustomPathTangent(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos, int& vertexIndex, int& tangentType) const
{
 if (!layer || !renderer_) return false;
 auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape || !shape->hasCustomPath()) return false;
 const QTransform gt = layer->getGlobalTransform();
 const float zoom = renderer_->getZoom();
 const float snapR = 7.0f / (zoom > 0.001f ? zoom : 1.0f);
 const auto& verts = shape->customPathVertices();
 for (int i = 0; i < static_cast<int>(verts.size()); ++i) {
  // outTangent handle
  if (verts[i].outTangent != QPointF(0, 0)) {
   const QPointF out = gt.map(verts[i].pos + verts[i].outTangent);
   float dx = static_cast<float>(out.x() - canvasPos.x());
   float dy = static_cast<float>(out.y() - canvasPos.y());
   if (std::sqrt(dx * dx + dy * dy) <= snapR) {
    vertexIndex = i; tangentType = 1; return true;
   }
  }
  // inTangent handle
  if (verts[i].inTangent != QPointF(0, 0)) {
   const QPointF in = gt.map(verts[i].pos + verts[i].inTangent);
   float dx = static_cast<float>(in.x() - canvasPos.x());
   float dy = static_cast<float>(in.y() - canvasPos.y());
   if (std::sqrt(dx * dx + dy * dy) <= snapR) {
    vertexIndex = i; tangentType = 0; return true;
   }
  }
 }
 return false;
}

void ArtifactLayerEditorWidgetV2::Impl::drawCustomPathOverlay(const ArtifactAbstractLayerPtr& layer)
{
 if (!renderer_ || !layer) return;
 auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape || !shape->hasCustomPath()) return;
 const QTransform gt = layer->getGlobalTransform();
 const float zoom = renderer_->getZoom();
 const float vR = 5.0f / (zoom > 0.001f ? zoom : 1.0f);
 const float tR = 4.0f / (zoom > 0.001f ? zoom : 1.0f);
 const auto verts = shape->customPathVertices();
 const bool closed = shape->customPathClosed();
 const int n = static_cast<int>(verts.size());
 // Draw path segments
 for (int i = 0; i < n; ++i) {
  const int j = (i + 1) % n;
  if (!closed && j == 0) break;
  const QPointF p0 = gt.map(verts[i].pos);
  const QPointF p1 = gt.map(verts[j].pos);
  renderer_->drawThickLineLocal(
      {static_cast<float>(p0.x()), static_cast<float>(p0.y())},
      {static_cast<float>(p1.x()), static_cast<float>(p1.y())},
      2.0f, FloatColor{0.4f,0.7f,1,0.5f});
 }
 // Draw tangent lines and handles
 for (int i = 0; i < n; ++i) {
  const QPointF vp = gt.map(verts[i].pos);
  if (verts[i].outTangent != QPointF(0, 0)) {
   const QPointF tp = gt.map(verts[i].pos + verts[i].outTangent);
   renderer_->drawThickLineLocal(
       {static_cast<float>(vp.x()), static_cast<float>(vp.y())},
       {static_cast<float>(tp.x()), static_cast<float>(tp.y())},
       1.0f, FloatColor{1,1,1,0.3f});
   const bool hov = hoveredPathTangentIndex_ == i && hoveredPathTangentType_ == 1;
   renderer_->drawCircle(
       static_cast<float>(tp.x()), static_cast<float>(tp.y()),
       tR, hov ? FloatColor{1,0.7f,0,1} : FloatColor{0.8f,0.5f,1,0.9f}, 1.0f, true);
  }
  if (verts[i].inTangent != QPointF(0, 0)) {
   const QPointF tp = gt.map(verts[i].pos + verts[i].inTangent);
   renderer_->drawThickLineLocal(
       {static_cast<float>(vp.x()), static_cast<float>(vp.y())},
       {static_cast<float>(tp.x()), static_cast<float>(tp.y())},
       1.0f, FloatColor{1,1,1,0.3f});
   const bool hov = hoveredPathTangentIndex_ == i && hoveredPathTangentType_ == 0;
   renderer_->drawCircle(
       static_cast<float>(tp.x()), static_cast<float>(tp.y()),
       tR, hov ? FloatColor{1,0.7f,0,1} : FloatColor{0.8f,0.5f,1,0.9f}, 1.0f, true);
  }
  // Vertex handle
   const bool hov = hoveredPathVertexIndex_ == i;
   const bool selected = std::find(selectedPathVertexIndices_.begin(),
                                   selectedPathVertexIndices_.end(), i) !=
                         selectedPathVertexIndices_.end();
   renderer_->drawCircle(
       static_cast<float>(vp.x()), static_cast<float>(vp.y()),
       selected ? vR * 1.35f : vR,
       hov ? FloatColor{1,0.5f,0,1}
           : (selected ? FloatColor{0.25f,0.85f,1,1} : FloatColor{0.2f,0.8f,1,1}),
       1.0f, true);
  renderer_->drawCircle(
      static_cast<float>(vp.x()), static_cast<float>(vp.y()),
      vR, FloatColor{1,1,1,0.8f}, 1.0f, false);
 }

 if (hoveredPathVertexIndex_ >= 0 || hoveredPathTangentIndex_ >= 0) {
  QFont hudFont = QApplication::font();
  hudFont.setPointSizeF(std::max(9.0, static_cast<double>(hudFont.pointSizeF())));
  const bool hoverTangent = hoveredPathTangentIndex_ >= 0;
  const bool draggingVertex = isDraggingPathVertex_;
  const bool draggingTangent = isDraggingPathTangent_;
  const QString headline = hoverTangent
                               ? QStringLiteral("tangent %1%2")
                                     .arg(hoveredPathTangentIndex_ + 1)
                                     .arg(draggingTangent ? QStringLiteral(" selected") : QString())
                               : QStringLiteral("vertex %1%2")
                                     .arg(hoveredPathVertexIndex_ + 1)
                                     .arg(draggingVertex ? QStringLiteral(" selected") : QString());
  const QString detail = hoverTangent
                             ? (draggingTangent ? QStringLiteral("dragging / rebalance / smooth")
                                                : QStringLiteral("drag / rebalance / smooth"))
                             : QStringLiteral("drag / delete / toggle smooth");
  const QPointF hudAnchor = gt.map(verts.front().pos) + QPointF(14.0, -30.0);
  const QRectF hudRect(hudAnchor, QSizeF(236.0 / zoom, 48.0 / zoom));
  renderer_->drawOverlayPanel(hudRect.x(), hudRect.y(), hudRect.width(), hudRect.height(),
                              FloatColor{0.06f, 0.09f, 0.13f, 0.88f},
                              FloatColor{0.42f, 0.72f, 0.98f, 0.90f});
  renderer_->drawText(hudRect.adjusted(8.0, 5.0, -8.0, -4.0),
                      QStringLiteral("Path %1\n%2\n%3").arg(headline, detail, proportionalEditingStatusText()),
                      hudFont, FloatColor{0.95f, 0.97f, 1.0f, 1.0f},
                      Qt::AlignLeft | Qt::AlignVCenter);
 }
}

bool ArtifactLayerEditorWidgetV2::Impl::deleteHoveredShapeVertex(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer || hoveredShapeVertexIndex_ < 0) {
  return false;
 }
 auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape || !shape->hasCustomPolygon()) {
  return false;
 }
 auto points = shape->customPolygonPoints();
 if (hoveredShapeVertexIndex_ < 0 || hoveredShapeVertexIndex_ >= static_cast<int>(points.size())) {
  return false;
 }
 const bool closed = shape->customPolygonClosed();
 beginShapeEditTransaction(layer);
 points.erase(points.begin() + hoveredShapeVertexIndex_);
 if (points.size() >= 3) {
  shape->setCustomPolygonPoints(points, closed);
 } else {
  shape->clearCustomPolygonPoints();
 }
 markShapeEditDirty();
 hoveredShapeVertexIndex_ = -1;
 return true;
}

bool ArtifactLayerEditorWidgetV2::Impl::insertPointOnHoveredShapeSegment(
    const ArtifactAbstractLayerPtr& layer,
    const QPointF& canvasPos)
{
 if (!layer || hoveredShapeSegmentIndex_ < 0) {
  return false;
 }
 auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape || !shape->hasCustomPolygon()) {
  return false;
 }
 std::vector<QPointF> points = shape->customPolygonPoints();
 if (points.size() < 2) {
  return false;
 }
 const bool closed = shape->customPolygonClosed();
 const int segmentCount = closed ? static_cast<int>(points.size())
                                 : static_cast<int>(points.size()) - 1;
 if (segmentCount <= 0) {
  return false;
 }
 const int segIndex = std::clamp(hoveredShapeSegmentIndex_, 0, segmentCount - 1);
 const QTransform globalTransform = layer->getGlobalTransform();
 bool invertible = false;
 const QTransform invTransform = globalTransform.inverted(&invertible);
 if (!invertible) {
  return false;
 }
 const QPointF rawLocalPos = invTransform.map(canvasPos);
 const QPointF localPos(
     std::clamp(rawLocalPos.x(), 0.0, static_cast<double>(shape->shapeWidth())),
     std::clamp(rawLocalPos.y(), 0.0, static_cast<double>(shape->shapeHeight())));
 beginShapeEditTransaction(layer);
 const int clampedInsertIndex = std::clamp(segIndex + 1, 0, static_cast<int>(points.size()));
 points.insert(points.begin() + clampedInsertIndex, localPos);
 shape->setCustomPolygonPoints(points, closed);
 markShapeEditDirty();
 hoveredShapeVertexIndex_ = clampedInsertIndex;
 hoveredShapeSegmentIndex_ = clampedInsertIndex - 1;
 isDraggingShapeVertex_ = true;
 draggingShapeVertexIndex_ = clampedInsertIndex;
 return true;
}

bool ArtifactLayerEditorWidgetV2::Impl::splitHoveredShapeSegment(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer || hoveredShapeSegmentIndex_ < 0) {
  return false;
 }
 auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape || !shape->hasCustomPolygon()) {
  return false;
 }
 std::vector<QPointF> points = shape->customPolygonPoints();
 if (points.size() < 2) {
  return false;
 }
 const int segmentCount = shape->customPolygonClosed()
                              ? static_cast<int>(points.size())
                              : static_cast<int>(points.size()) - 1;
 if (segmentCount <= 0) {
  return false;
 }
 const int segIndex = std::clamp(hoveredShapeSegmentIndex_, 0, segmentCount - 1);
 const int nextIndex = shape->customPolygonClosed()
                           ? (segIndex + 1) % static_cast<int>(points.size())
                           : segIndex + 1;
 if (nextIndex < 0 || nextIndex >= static_cast<int>(points.size())) {
  return false;
 }
 const QPointF insertPoint = (points[static_cast<size_t>(segIndex)] +
                              points[static_cast<size_t>(nextIndex)]) * 0.5;
 const bool closed = shape->customPolygonClosed();
 beginShapeEditTransaction(layer);
 const int clampedInsertIndex = std::clamp(segIndex + 1, 0, static_cast<int>(points.size()));
 points.insert(points.begin() + clampedInsertIndex, insertPoint);
 shape->setCustomPolygonPoints(points, closed);
 markShapeEditDirty();
 hoveredShapeVertexIndex_ = clampedInsertIndex;
 hoveredShapeSegmentIndex_ = clampedInsertIndex - 1;
 isDraggingShapeVertex_ = true;
 draggingShapeVertexIndex_ = clampedInsertIndex;
 return true;
}

bool ArtifactLayerEditorWidgetV2::Impl::hitTestMaskVertex(const ArtifactAbstractLayerPtr& layer,
                                                           const QPointF& canvasPos,
                                                           int& maskIndex,
                                                           int& pathIndex,
                                                           int& vertexIndex) const
 {
  if (!renderer_ || !layer) {
   return false;
  }
  const QTransform globalTransform = layer->getGlobalTransform();
  bool invertible = false;
  const QTransform invTransform = globalTransform.inverted(&invertible);
  if (!invertible) {
   return false;
  }
  const QPointF localPos = invTransform.map(canvasPos);
  const float threshold = 8.0f / std::max(0.1f, renderer_->getZoom());
  for (int m = 0; m < layer->maskCount(); ++m) {
   const LayerMask mask = layer->mask(m);
   for (int p = 0; p < mask.maskPathCount(); ++p) {
    const MaskPath path = mask.maskPath(p);
    for (int v = 0; v < path.vertexCount(); ++v) {
     const MaskVertex vertex = path.vertex(v);
     if (std::hypot(vertex.position.x() - localPos.x(), vertex.position.y() - localPos.y()) <= threshold) {
      maskIndex = m;
      pathIndex = p;
      vertexIndex = v;
      return true;
     }
    }
   }
  }
  return false;
 }

 void ArtifactLayerEditorWidgetV2::Impl::updateMaskHover(const ArtifactAbstractLayerPtr& layer,
                                                         const QPointF& canvasPos)
 {
 hoveredMaskIndex_ = -1;
 hoveredPathIndex_ = -1;
 hoveredVertexIndex_ = -1;
 hoveredMaskHandleType_ = -1;
 int maskIndex = -1;
 int pathIndex = -1;
 int vertexIndex = -1;
 MaskHandleType handleType = MaskHandleType::None;
 const float handleThreshold = 10.0f / std::max(0.1f, renderer_->getZoom());
 if (hitTestMaskHandle(layer, canvasPos, handleThreshold,
                       maskIndex, pathIndex, vertexIndex, handleType)) {
  hoveredMaskIndex_ = maskIndex;
  hoveredPathIndex_ = pathIndex;
  hoveredVertexIndex_ = vertexIndex;
  hoveredMaskHandleType_ = static_cast<int>(handleType);
 } else if (hitTestMaskVertex(layer, canvasPos, maskIndex, pathIndex, vertexIndex)) {
  hoveredMaskIndex_ = maskIndex;
  hoveredPathIndex_ = pathIndex;
  hoveredVertexIndex_ = vertexIndex;
 }
}

 void ArtifactLayerEditorWidgetV2::Impl::drawMaskOverlay(const ArtifactAbstractLayerPtr& layer)
 {
  if (!renderer_ || !layer || layer->maskCount() <= 0) {
   return;
  }

 const QTransform globalTransform = layer->getGlobalTransform();
  const FloatColor maskPointShadowColor = {0.0f, 0.0f, 0.0f, 0.42f};
  const FloatColor hoverColor = {0.98f, 0.72f, 0.24f, 1.0f};
  const FloatColor dragColor = {0.98f, 0.42f, 0.18f, 1.0f};

  for (int m = 0; m < layer->maskCount(); ++m) {
   LayerMask mask = layer->mask(m);
   if (!mask.isEnabled()) {
    continue;
   }
   for (int p = 0; p < mask.maskPathCount(); ++p) {
    MaskPath path = mask.maskPath(p);
    const int vertexCount = path.vertexCount();
    if (vertexCount == 0) {
     continue;
    }
    const FloatColor modeColor = maskModeColor(path.mode());
    const FloatColor maskLineShadowColor = {0.0f, 0.0f, 0.0f, 0.36f};
    const FloatColor maskLineColor = brightenColor(modeColor, 1.04f);
    const FloatColor maskLineHighlightColor = brightenColor(modeColor, 1.32f);
    const FloatColor handlePointColor =
        withAlpha(lerpColor(modeColor, FloatColor{0.86f, 0.92f, 0.98f, 1.0f}, 0.52f), 0.98f);
    const FloatColor handleHoverColor = lerpColor(modeColor, hoverColor, 0.72f);
    const FloatColor handleDragColor = lerpColor(modeColor, dragColor, 0.82f);
    const FloatColor vertexBaseColor =
        withAlpha(lerpColor(modeColor, FloatColor{0.88f, 0.94f, 0.98f, 1.0f}, 0.46f), 1.0f);
    const FloatColor tangentInColor = withAlpha(brightenColor(modeColor, 1.16f), 0.92f);
    const FloatColor tangentOutColor =
        withAlpha(lerpColor(modeColor, FloatColor{0.92f, 0.84f, 0.58f, 1.0f}, 0.26f), 0.92f);

    Detail::float2 lastCanvasPos{};
    for (int v = 0; v < vertexCount; ++v) {
     MaskVertex vertex = path.vertex(v);
     QPointF canvasPos = globalTransform.map(vertex.position);
     Detail::float2 currentCanvasPos = {(float)canvasPos.x(), (float)canvasPos.y()};
     const QPointF inHandlePos = globalTransform.map(vertex.position + vertex.inTangent);
     const QPointF outHandlePos = globalTransform.map(vertex.position + vertex.outTangent);
     const Detail::float2 inHandleCanvas = {(float)inHandlePos.x(), (float)inHandlePos.y()};
     const Detail::float2 outHandleCanvas = {(float)outHandlePos.x(), (float)outHandlePos.y()};

     if (vertex.inTangent != QPointF(0, 0)) {
      const bool isDraggingIn = isDraggingMaskHandle_ && draggingMaskIndex_ == m &&
                                draggingPathIndex_ == p && draggingVertexIndex_ == v &&
                                draggingMaskHandleType_ == static_cast<int>(MaskHandleType::InTangent);
      const bool isHoveringIn = hoveredMaskIndex_ == m && hoveredPathIndex_ == p &&
                                hoveredVertexIndex_ == v &&
                                hoveredMaskHandleType_ == static_cast<int>(MaskHandleType::InTangent);
      FloatColor tangentColor = tangentInColor;
      FloatColor handleColor = tangentInColor;
      float stemThickness = 2.2f;
      float stemShadowThickness = 5.8f;
      if (isDraggingMaskHandle_ && draggingMaskIndex_ == m && draggingPathIndex_ == p &&
          draggingVertexIndex_ == v && draggingMaskHandleType_ == static_cast<int>(MaskHandleType::InTangent)) {
       tangentColor = handleDragColor;
       handleColor = handleDragColor;
       stemThickness = 3.2f;
       stemShadowThickness = 7.8f;
      } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p && hoveredVertexIndex_ == v &&
                 hoveredMaskHandleType_ == static_cast<int>(MaskHandleType::InTangent)) {
       tangentColor = handleHoverColor;
       handleColor = handleHoverColor;
       stemThickness = 2.8f;
       stemShadowThickness = 7.0f;
      }
      renderer_->drawThickLineLocal(currentCanvasPos, inHandleCanvas, stemShadowThickness, maskLineShadowColor);
      renderer_->drawThickLineLocal(currentCanvasPos, inHandleCanvas, stemThickness, tangentColor);
      renderer_->drawThickLineLocal(currentCanvasPos, inHandleCanvas,
                                    std::max(1.0f, stemThickness * 0.42f),
                                    brightenColor(tangentColor, 1.20f));
      drawMaskSolidHandle(renderer_.get(), inHandleCanvas, isDraggingIn || isHoveringIn ? 11.0f : 8.2f,
                          handleColor, isDraggingIn || isHoveringIn);
     }
     if (vertex.outTangent != QPointF(0, 0)) {
      const bool isDraggingOut = isDraggingMaskHandle_ && draggingMaskIndex_ == m &&
                                 draggingPathIndex_ == p && draggingVertexIndex_ == v &&
                                 draggingMaskHandleType_ == static_cast<int>(MaskHandleType::OutTangent);
      const bool isHoveringOut = hoveredMaskIndex_ == m && hoveredPathIndex_ == p &&
                                 hoveredVertexIndex_ == v &&
                                 hoveredMaskHandleType_ == static_cast<int>(MaskHandleType::OutTangent);
      FloatColor tangentColor = tangentOutColor;
      FloatColor handleColor = handlePointColor;
      float stemThickness = 2.2f;
      float stemShadowThickness = 5.8f;
      if (isDraggingMaskHandle_ && draggingMaskIndex_ == m && draggingPathIndex_ == p &&
          draggingVertexIndex_ == v && draggingMaskHandleType_ == static_cast<int>(MaskHandleType::OutTangent)) {
       handleColor = handleDragColor;
       tangentColor = handleDragColor;
       stemThickness = 3.2f;
       stemShadowThickness = 7.8f;
      } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p && hoveredVertexIndex_ == v &&
                 hoveredMaskHandleType_ == static_cast<int>(MaskHandleType::OutTangent)) {
       handleColor = handleHoverColor;
       tangentColor = handleHoverColor;
       stemThickness = 2.8f;
       stemShadowThickness = 7.0f;
      }
      renderer_->drawThickLineLocal(currentCanvasPos, outHandleCanvas, stemShadowThickness, maskLineShadowColor);
      renderer_->drawThickLineLocal(currentCanvasPos, outHandleCanvas, stemThickness, tangentColor);
      renderer_->drawThickLineLocal(currentCanvasPos, outHandleCanvas,
                                    std::max(1.0f, stemThickness * 0.42f),
                                    brightenColor(tangentColor, 1.18f));
      drawMaskSolidHandle(renderer_.get(), outHandleCanvas, isDraggingOut || isHoveringOut ? 11.0f : 8.2f,
                          handleColor, isDraggingOut || isHoveringOut);
     }
     if (v > 0) {
      const bool segmentActive =
          (isDraggingMaskVertex_ && draggingMaskIndex_ == m && draggingPathIndex_ == p &&
           (draggingVertexIndex_ == v || draggingVertexIndex_ == (v - 1))) ||
          (hoveredMaskIndex_ == m && hoveredPathIndex_ == p &&
           (hoveredVertexIndex_ == v || hoveredVertexIndex_ == (v - 1)));
      const FloatColor currentLineBase = segmentActive
          ? lerpColor(maskLineColor, hoverColor, isDraggingMaskVertex_ ? 0.72f : 0.58f)
          : maskLineColor;
      renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos,
                                    segmentActive ? 8.8f : 7.0f, maskLineShadowColor);
      renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos,
                                    segmentActive ? 5.2f : 4.1f, currentLineBase);
      renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos,
                                    segmentActive ? 2.2f : 1.5f,
                                    brightenColor(segmentActive ? currentLineBase : maskLineHighlightColor,
                                                  segmentActive ? 1.16f : 1.0f));
     }
     lastCanvasPos = currentCanvasPos;
    }

    if (path.isClosed() && vertexCount > 1) {
     MaskVertex firstVertex = path.vertex(0);
     QPointF firstCanvasPos = globalTransform.map(firstVertex.position);
     renderer_->drawThickLineLocal(lastCanvasPos,
                                   {(float)firstCanvasPos.x(), (float)firstCanvasPos.y()},
                                   7.4f, maskLineShadowColor);
     renderer_->drawThickLineLocal(lastCanvasPos,
                                   {(float)firstCanvasPos.x(), (float)firstCanvasPos.y()},
                                   4.3f, maskLineColor);
     renderer_->drawThickLineLocal(lastCanvasPos,
                                   {(float)firstCanvasPos.x(), (float)firstCanvasPos.y()},
                                   1.6f, maskLineHighlightColor);
    }

    for (int v = 0; v < vertexCount; ++v) {
     MaskVertex vertex = path.vertex(v);
     QPointF canvasPos = globalTransform.map(vertex.position);
     FloatColor currentColor = vertexBaseColor;
     float currentRadius = 12.0f;
     bool currentActive = false;
     if (isDraggingMaskVertex_ && draggingMaskIndex_ == m && draggingPathIndex_ == p && draggingVertexIndex_ == v) {
      currentColor = dragColor;
      currentRadius = 15.0f;
      currentActive = true;
     } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p && hoveredVertexIndex_ == v) {
      currentColor = hoverColor;
      currentRadius = 14.0f;
      currentActive = true;
     }
     const Detail::float2 currentCanvasPos = {(float)canvasPos.x(), (float)canvasPos.y()};
     renderer_->drawSolidRect(currentCanvasPos.x - (currentRadius + 3.2f) * 0.5f,
                              currentCanvasPos.y - (currentRadius + 3.2f) * 0.5f,
                              currentRadius + 3.2f, currentRadius + 3.2f,
                              maskPointShadowColor, 1.0f);
     drawMaskSolidHandle(renderer_.get(), currentCanvasPos, currentRadius, currentColor, currentActive);
    }
   }
  }
 }

 bool ArtifactLayerEditorWidgetV2::Impl::deleteHoveredMaskVertex(const ArtifactAbstractLayerPtr& layer)
 {
  if (!layer || hoveredMaskIndex_ < 0 || hoveredPathIndex_ < 0 || hoveredVertexIndex_ < 0) {
   return false;
  }

  LayerMask mask = layer->mask(hoveredMaskIndex_);
  if (hoveredPathIndex_ >= mask.maskPathCount()) {
   return false;
  }

  MaskPath path = mask.maskPath(hoveredPathIndex_);
  if (hoveredVertexIndex_ >= path.vertexCount()) {
   return false;
  }

  path.removeVertex(hoveredVertexIndex_);
  if (path.vertexCount() <= 0) {
   mask.removeMaskPath(hoveredPathIndex_);
   if (mask.maskPathCount() <= 0) {
    layer->removeMask(hoveredMaskIndex_);
   } else {
    layer->setMask(hoveredMaskIndex_, mask);
   }
  } else {
   mask.setMaskPath(hoveredPathIndex_, path);
   layer->setMask(hoveredMaskIndex_, mask);
  }

  markMaskEditDirty();
  hoveredVertexIndex_ = -1;
  hoveredPathIndex_ = -1;
  hoveredMaskIndex_ = -1;
  return true;
 }

void ArtifactLayerEditorWidgetV2::Impl::startRenderLoop()
{
 if (running_)
  return;
 running_ = true;
 if (renderTimer_ && !renderTimer_->isActive()) {
  renderTimer_->start();
 }
 requestRender();
}

 void ArtifactLayerEditorWidgetV2::Impl::stopRenderLoop()
 {
  running_ = false;        // ループを抜ける
  if (renderTimer_) {
   renderTimer_->stop();
  }

 if (renderer_) {
  renderer_->flushAndWait();
 }
}

void ArtifactLayerEditorWidgetV2::Impl::requestRender()
{
 constexpr int kInteractiveRenderCoalesceMs = 16;
 if (!widget_) {
  return;
 }
 renderRequestPending_.store(true, std::memory_order_release);
 if (renderRequestScheduled_) {
  return;
 }
 renderRequestScheduled_ = true;
 // Property sliders and effect controls can emit several mutations inside one
 // display interval. Keep only the newest state and render at most once per
 // interval instead of blocking the UI thread for every intermediate value.
 QTimer::singleShot(kInteractiveRenderCoalesceMs, widget_, [this]() {
  renderRequestScheduled_ = false;
  if (!renderRequestPending_.exchange(false, std::memory_order_acq_rel)) {
   return;
  }
  if (!initialized_ || !renderer_ || !widget_ || !widget_->isVisible() ||
      widget_->width() <= 0 || widget_->height() <= 0) {
   return;
  }
  if (renderInProgress_) {
   renderRequestPending_.store(true, std::memory_order_release);
   requestRender();
   return;
  }
  std::lock_guard<std::mutex> lock(resizeMutex_);
  if (renderInProgress_) {
   renderRequestPending_.store(true, std::memory_order_release);
   requestRender();
   return;
  }
  renderInProgress_ = true;
  QElapsedTimer frameTimer;
  frameTimer.start();
  renderOneFrame();
  renderInProgress_ = false;
  ++renderExecutedCount_;
  const qint64 elapsedMs = frameTimer.elapsed();
  if (elapsedMs >= 8 || (renderExecutedCount_ % 120ull) == 1ull) {
   qCDebug(layerViewPerfLog) << "[LayerView][Frame]"
                             << "ms=" << elapsedMs
                             << "executed=" << renderExecutedCount_
                             << "targetLayerNil=" << targetLayerId_.isNil()
                             << "visible=" << widget_->isVisible()
                             << "size=" << widget_->size();
  }
  if (renderRequestPending_.load(std::memory_order_acquire)) {
   requestRender();
  }
 });
}

void ArtifactLayerEditorWidgetV2::Impl::renderOneFrame()
{
 if (!initialized_ || !renderer_)
  return;
 renderer_->clear();
 const QSize viewportSize = physicalViewportSize(widget_);
 const float viewportW = static_cast<float>(std::max(1, viewportSize.width()));
 const float viewportH = static_cast<float>(std::max(1, viewportSize.height()));
 const float prevZoom = renderer_->getZoom();
 float prevPanX = 0.0f;
 float prevPanY = 0.0f;
 renderer_->getPan(prevPanX, prevPanY);
 renderer_->setViewportSize(viewportW, viewportH);
 renderer_->setCanvasSize(viewportW, viewportH);
 renderer_->setZoom(1.0f);
 renderer_->setPan(0.0f, 0.0f);
 renderer_->setUseExternalMatrices(false);
 renderer_->resetGizmoCameraMatrices();
 renderer_->reset3DCameraMatrices();
 if (backgroundMode_ == LayerBackgroundMode::Alpha) {
  renderer_->drawCheckerboard(0.0f, 0.0f, viewportW, viewportH, 56.0f,
                              FloatColor(0.33f, 0.34f, 0.35f, 1.0f),
                              FloatColor(0.26f, 0.27f, 0.28f, 1.0f));
 } else if (backgroundMode_ == LayerBackgroundMode::MayaGradient) {
  refreshBackgroundCache();
  if (!cachedMayaGradientSprite_.isNull()) {
   renderer_->drawSprite(0.0f, 0.0f, viewportW, viewportH,
                         cachedMayaGradientSprite_, 1.0f);
  }
 } else {
  renderer_->drawRectLocal(0.0f, 0.0f, viewportW, viewportH, clearColor_);
 }
 if (compositionRenderer_) {
  if (auto* service = ArtifactProjectService::instance()) {
   if (auto composition = service->currentComposition().lock()) {
    const auto compSize = composition->settings().compositionSize();
    compositionRenderer_->SetCompositionSize(static_cast<float>(compSize.width()), static_cast<float>(compSize.height()));
   }
  }
  compositionRenderer_->ApplyCompositionSpace();
 }
 renderer_->setZoom(prevZoom);
 renderer_->setPan(prevPanX, prevPanY);
 ArtifactAbstractLayerPtr displayedLayer;
  if (!targetLayerId_.isNil()) {
   if (auto* service = ArtifactProjectService::instance()) {
    if (auto composition = service->currentComposition().lock()) {
     // コンポジションサイズを設定
     const auto compSize = composition->settings().compositionSize();
     if (compSize.width() > 0 && compSize.height() > 0) {
      renderer_->setCanvasSize(static_cast<float>(compSize.width()), static_cast<float>(compSize.height()));
     }

     if (auto layer = composition->layerById(targetLayerId_)) {
      displayedLayer = layer;
      const FramePosition currentFrame = currentLayerViewFrame();
      layer->goToFrame(currentFrame.framePosition());
      const auto source = layer->sourceSize();
      if (source.width > 0 && source.height > 0) {
       // レイヤーサイズも設定（コンポジションサイズを上書きしないためコメントアウト）
       // renderer_->setCanvasSize(static_cast<float>(source.width), static_cast<float>(source.height));
      }
      
      const bool isVisible = layer->isVisible();
      const bool isActive = layer->isActiveAt(currentFrame);
      if (!isVisible || !isActive || layer->opacity() <= 0.0f) {
      } else {
       layer->draw(renderer_.get());
       if (surfaceMode_ == LayerSurfaceMode::Inspect &&
           displayMode_ == DisplayMode::Mask) {
        drawMaskOverlay(layer);
        drawSurfaceOverlay(layer);
       } else if (surfaceMode_ != LayerSurfaceMode::Edit) {
        drawSurfaceOverlay(layer);
       } else if (!layer->isLocked()) {
        if (displayMode_ == DisplayMode::Mask || editMode_ == EditMode::Mask) {
         drawMaskOverlay(layer);
        } else if (isShapeEditingMode(editMode_)) {
         drawShapeOverlay(layer);
        } else {
         drawTransformOverlay(layer);
         drawShapeParamHandles(layer);
         drawTransformHUD(layer);
        }
       }
      }
     }
    }
   }
  }
 if (!targetLayerId_.isNil()) {
  if (auto layer = targetLayer()) {
   syncTransformGizmo(layer);
  } else if (transformGizmo_) {
   transformGizmo_->setLayer(nullptr);
  }
 }
 drawViewportChrome(displayedLayer);
 renderer_->flush();
 renderer_->present();
}

void ArtifactLayerEditorWidgetV2::Impl::refreshBackgroundCache()
{
 if (backgroundMode_ != LayerBackgroundMode::MayaGradient) {
  cachedMayaGradientSprite_ = QImage();
  cachedMayaGradientSize_ = QSize();
  return;
 }
 const QSize viewportSize = physicalViewportSize(widget_);
 if (viewportSize.isEmpty() || cachedMayaGradientSize_ == viewportSize) {
  return;
 }
 cachedMayaGradientSize_ = viewportSize;
 cachedMayaGradientSprite_ = makeMayaGradientSprite(viewportSize, clearColor_);
}

 static LayerID currentSelectedLayerId()
 {
  if (auto *app = ArtifactApplicationManager::instance()) {
   if (auto *selectionManager = app->layerSelectionManager()) {
    if (auto current = selectionManager->currentLayer()) {
     return current->id();
    }
   }
  }
  return LayerID();
 }

void ArtifactLayerEditorWidgetV2::Impl::recreateSwapChain(QWidget* window)
 {
  if (!initialized_ || !renderer_) {
   return;
  }
  if (!window || window->width() <= 0 || window->height() <= 0) {
   return;
  }
  std::lock_guard<std::mutex> lock(resizeMutex_);
  renderer_->recreateSwapChain(window);
  const QSize viewportSize = physicalViewportSize(window);
  renderer_->setViewportSize(static_cast<float>(viewportSize.width()), static_cast<float>(viewportSize.height()));
 }

ArtifactLayerEditorWidgetV2::ArtifactLayerEditorWidgetV2(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  // requestRender() uses this object as the queued-call receiver.  Assign it at
  // construction time rather than waiting for renderer initialization.
  impl_->widget_ = this;
  setMinimumSize(1, 1);

  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);

  setWindowTitle(QStringLiteral("Layer Solo View"));
  setProperty("artifactSurfaceMode", QStringLiteral("Edit"));
  publishModeReadout(this, impl_->editMode_, impl_->displayMode_);
  publishLayerReadout(this, ArtifactAbstractLayerPtr{});

  impl_->renderTimer_ = new QTimer(this);
  impl_->renderTimer_->setInterval(16);
  QObject::connect(impl_->renderTimer_, &QTimer::timeout, this, [this]() {
   ++impl_->renderTickCount_;
   if ((impl_->renderTickCount_ % 120ull) == 1ull) {
    qCDebug(layerViewPerfLog) << "[LayerView][Timer]"
                              << "ticks=" << impl_->renderTickCount_
                              << "executed=" << impl_->renderExecutedCount_
                              << "visible=" << isVisible()
                              << "hidden=" << isHidden()
                              << "windowVisible=" << (window() ? window()->isVisible() : false)
                              << "size=" << size()
                              << "running=" << impl_->running_.load(std::memory_order_acquire);
   }
  if (!impl_ || !impl_->initialized_ || !impl_->renderer_ || !impl_->running_.load(std::memory_order_acquire)) {
   return;
  }
  if (!isVisible() || width() <= 0 || height() <= 0) {
   return;
  }
  if (impl_->renderInProgress_) {
   return;
  }
  std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
  if (impl_->renderInProgress_) {
   return;
  }
  impl_->renderInProgress_ = true;
  QElapsedTimer frameTimer;
  frameTimer.start();
  impl_->renderOneFrame();
  impl_->renderInProgress_ = false;
  ++impl_->renderExecutedCount_;
  const qint64 elapsedMs = frameTimer.elapsed();
  if (elapsedMs >= 8 || (impl_->renderExecutedCount_ % 120ull) == 1ull) {
   qCDebug(layerViewPerfLog) << "[LayerView][Frame]"
                             << "ms=" << elapsedMs
                             << "executed=" << impl_->renderExecutedCount_
                             << "targetLayerNil=" << impl_->targetLayerId_.isNil()
                             << "visible=" << isVisible()
                             << "size=" << size();
  }
 });

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerSelectionChangedEvent>(
          [this](const LayerSelectionChangedEvent& event) {
            setTargetLayer(LayerID(event.layerId));
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerChangedEvent>(
          [this](const LayerChangedEvent& event) {
            if (impl_->surfaceMode_ == LayerSurfaceMode::Impact ||
                impl_->targetLayerId_.toString() == event.layerId) {
              impl_->surfaceInfoDirty_ = true;
            }
            if (event.changeType == LayerChangedEvent::ChangeType::Removed &&
                impl_->targetLayerId_.toString() == event.layerId) {
              clearTargetLayer();
              return;
            }
            if (impl_->targetLayerId_.isNil()) {
              return;
            }
            auto targetLayer = impl_->targetLayer();
            if (!targetLayer) {
              return;
            }
            if (impl_->targetLayerId_.toString() == event.layerId) {
              publishLayerReadout(this, targetLayer);
            }
            if (impl_->surfaceMode_ == LayerSurfaceMode::Impact ||
                impl_->targetLayerId_.toString() == event.layerId ||
                targetLayer->parentLayerId().toString() == event.layerId) {
              impl_->requestRender();
            }
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<FrameChangedEvent>(
          [this](const FrameChangedEvent& event) {
            if (impl_->targetLayerId_.isNil()) {
              return;
            }
            if (impl_->surfaceMode_ == LayerSurfaceMode::Inspect) {
              impl_->surfaceInfoDirty_ = true;
            }
            if (impl_->isPlay_ && impl_->running_.load(std::memory_order_acquire)) {
              return;
            }
            if (auto* service = ArtifactProjectService::instance()) {
              if (auto composition = service->currentComposition().lock()) {
                if (composition->id().toString() == event.compositionId) {
                  impl_->requestRender();
                }
              }
            }
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<PlaybackStateChangedEvent>(
          [this](const PlaybackStateChangedEvent& event) {
            impl_->isPlay_ = (event.state == ArtifactCore::PlaybackState::Playing);
            if (impl_->isPlay_) {
              if (isVisible()) {
                impl_->startRenderLoop();
              }
              return;
            }
            impl_->stopRenderLoop();
            impl_->requestRender();
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectChangedEvent>(
          [this](const ProjectChangedEvent&) {
            impl_->surfaceInfoDirty_ = true;
            const auto targetId = impl_->targetLayerId_;
            if (targetId.isNil()) {
              return;
            }
            if (auto* currentService = ArtifactProjectService::instance()) {
              if (auto composition = currentService->currentComposition().lock()) {
                if (composition->containsLayerById(targetId)) {
                  setTargetLayer(targetId);
                  return;
                }
              }
            }
            clearTargetLayer();
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>(
          [this](const CurrentCompositionChangedEvent&) {
            impl_->surfaceInfoDirty_ = true;
            const LayerID selectedId = currentSelectedLayerId();
            if (!selectedId.isNil()) {
              setTargetLayer(selectedId);
              return;
            }
            if (!impl_->targetLayerId_.isNil()) {
              if (auto* service = ArtifactProjectService::instance()) {
                if (auto composition = service->currentComposition().lock()) {
                  if (composition->containsLayerById(impl_->targetLayerId_)) {
                    setTargetLayer(impl_->targetLayerId_);
                    return;
                  }
                }
              }
            }
            clearTargetLayer();
          }));
 }

 void ArtifactLayerEditorWidgetV2::clearTargetLayer()
 {
  std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
  if (impl_->shapeEditPending_) {
   impl_->commitShapeEditTransaction();
  }
  if (impl_->pathEditPending_) {
   impl_->commitPathEditTransaction();
  }
 impl_->targetLayerId_ = LayerID();
 publishLayerReadout(this, ArtifactAbstractLayerPtr{});
 impl_->surfaceInfoDirty_ = true;
  impl_->isDraggingShapeVertex_ = false;
  impl_->draggingShapeVertexIndex_ = -1;
  impl_->hoveredShapeVertexIndex_ = -1;
 impl_->isDraggingCornerRadius_ = false;
 impl_->isDraggingStarInnerRadius_ = false;
 impl_->hoveredCornerRadius_ = false;
 impl_->hoveredStarInnerRadius_ = false;
 impl_->resetProportionalDragState();
  impl_->isDraggingPathVertex_ = false;
  impl_->isDraggingPathTangent_ = false;
    impl_->draggingPathVertexIndex_ = -1;
     impl_->selectedPathDragBefore_.clear();
  impl_->hoveredPathVertexIndex_ = -1;
  impl_->hoveredPathTangentIndex_ = -1;
 if (impl_->renderer_) {
  impl_->renderer_->clear();
  impl_->renderer_->flush();
  impl_->renderer_->present();
 }
 if (impl_->transformGizmo_) {
  impl_->transformGizmo_->setLayer(nullptr);
 }
}

 ArtifactLayerEditorWidgetV2::~ArtifactLayerEditorWidgetV2()
 {
  impl_->destroy();
  delete impl_;
  impl_ = nullptr;
 }

 void ArtifactLayerEditorWidgetV2::keyPressEvent(QKeyEvent* event)
 {
 if (impl_->editMode_ == EditMode::Mask && impl_->renderer_ && event) {
   if (event->key() == Qt::Key_O) {
    impl_->proportionalEditingEnabled_ = !impl_->proportionalEditingEnabled_;
    impl_->requestRender();
    event->accept();
    return;
   }
   if (event->key() == Qt::Key_BracketLeft || event->key() == Qt::Key_BracketRight) {
    const float scale = event->key() == Qt::Key_BracketLeft ? 0.85f : 1.15f;
    impl_->proportionalEditRadius_ = std::clamp(
        impl_->proportionalEditRadius_ * scale,
        kMinProportionalEditRadius,
        kMaxProportionalEditRadius);
    impl_->requestRender();
    event->accept();
    return;
   }
   if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    auto layer = impl_->targetLayer();
    if (layer && layer->isVisible() && !layer->isLocked()) {
     impl_->beginMaskEditTransaction(layer);
    }
    if (layer && layer->isVisible() && !layer->isLocked() &&
        impl_->deleteHoveredMaskVertex(layer)) {
     impl_->commitMaskEditTransaction();
     impl_->requestRender();
     event->accept();
     return;
    }
    impl_->commitMaskEditTransaction();
   }
  }
  if (impl_->displayMode_ != DisplayMode::Mask &&
      isShapeEditingMode(impl_->editMode_) && impl_->renderer_ && event) {
   if (event->key() == Qt::Key_O) {
    impl_->proportionalEditingEnabled_ = !impl_->proportionalEditingEnabled_;
    impl_->requestRender();
    event->accept();
    return;
   }
   if (event->key() == Qt::Key_BracketLeft || event->key() == Qt::Key_BracketRight) {
    const float scale = event->key() == Qt::Key_BracketLeft ? 0.85f : 1.15f;
    impl_->proportionalEditRadius_ = std::clamp(
        impl_->proportionalEditRadius_ * scale,
        kMinProportionalEditRadius,
        kMaxProportionalEditRadius);
    impl_->requestRender();
    event->accept();
    return;
   }
   if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    auto layer = impl_->targetLayer();
    if (layer && layer->isVisible() && !layer->isLocked()) {
     impl_->beginShapeEditTransaction(layer);
    }
    bool handled = false;
    if (layer && layer->isVisible() && !layer->isLocked() &&
        impl_->deleteHoveredShapeVertex(layer)) {
     handled = true;
    } else if (layer && layer->isVisible() && !layer->isLocked() &&
               impl_->splitHoveredShapeSegment(layer)) {
     handled = true;
    }
    if (handled) {
     impl_->commitShapeEditTransaction();
     impl_->requestRender();
     event->accept();
     return;
    }
    impl_->commitShapeEditTransaction();
   }
  }
  impl_->defaultHandleKeyPressEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::keyReleaseEvent(QKeyEvent* event)
 {
  impl_->defaultHandleKeyReleaseEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::mousePressEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::LeftButton &&
      impl_->handleViewportChromePress(event->position())) {
   event->accept();
   return;
  }

  if (event->button() == Qt::MiddleButton ||
   (event->button() == Qt::RightButton && event->modifiers() & Qt::AltModifier))
  {
   impl_->updateViewportChromeHover(QPointF(-1.0, -1.0));
   impl_->isPanning_ = true;
   impl_->lastMousePos_ = event->position(); // 前回位置を保存
   setCursor(hudCursor(QStringLiteral("hud_cursor_pan.svg"),
                       Qt::ClosedHandCursor));
   event->accept();
   return;
  }

  if (event->button() == Qt::LeftButton) {
   if (const auto layer = impl_->targetLayer();
       layer && (!layer->isVisible() || layer->isLocked())) {
    event->accept();
    return;
   }
  }

  if (impl_->surfaceMode_ == LayerSurfaceMode::Edit &&
      impl_->transformGizmo_ && impl_->renderer_ &&
      impl_->displayMode_ != DisplayMode::Mask &&
      impl_->editMode_ != EditMode::Mask &&
      !isShapeEditingMode(impl_->editMode_) &&
      event->button() == Qt::LeftButton) {
   // Phase 1: parametric shape handle hit test (takes priority over gizmo)
   auto layer = impl_->targetLayer();
   if (layer && layer->isVisible() && !layer->isLocked()) {
    const Detail::float2 cp = impl_->renderer_->viewportToCanvas(
        {(float)event->position().x(), (float)event->position().y()});
    const QPointF canvasPoint(cp.x, cp.y);
    if (impl_->hitTestCornerRadiusHandle(layer, canvasPoint)) {
     auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
     impl_->isDraggingCornerRadius_ = true;
     impl_->cornerRadiusDragStart_ = shape->cornerRadius();
     impl_->cornerRadiusBefore_ = shape->cornerRadius();
     impl_->cornerRadiusDragAnchorX_ = static_cast<float>(event->position().x());
     impl_->cornerRadiusDragMaxCr_ = std::min(shape->shapeWidth(), shape->shapeHeight()) * 0.5f;
     impl_->paramHandleEditLayer_ = layer;
     setCursor(hudCursor(QStringLiteral("hud_cursor_scale_horizontal.svg"),
                         Qt::SizeHorCursor));
     event->accept();
     return;
    }
    if (impl_->hitTestStarInnerRadiusHandle(layer, canvasPoint)) {
     auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
     impl_->isDraggingStarInnerRadius_ = true;
     impl_->starInnerRadiusDragStart_ = shape->starInnerRadius();
     impl_->starInnerRadiusBefore_ = shape->starInnerRadius();
     impl_->paramHandleEditLayer_ = layer;
     setCursor(hudCursor(QStringLiteral("hud_cursor_scale_vertical.svg"),
                         Qt::SizeVerCursor));
     event->accept();
     return;
    }
   }
   if (layer && impl_->transformGizmo_->handleMousePress(
                      {event->position().x(), event->position().y()},
                      impl_->renderer_.get())) {
    setCursor(hudCursorForTransformHandle(impl_->transformGizmo_->activeHandle(),
                                          true));
    impl_->requestRender();
    event->accept();
    return;
   }
  }

 if (impl_->displayMode_ != DisplayMode::Mask &&
     isShapeEditingMode(impl_->editMode_) &&
     event->button() == Qt::LeftButton && impl_->renderer_) {
   auto layer = impl_->targetLayer();
   auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
   if (shape && shape->shapeType() != ShapeType::Line) {
    const Detail::float2 canvasPos = impl_->renderer_->viewportToCanvas(
        {(float)event->position().x(), (float)event->position().y()});
    const QPointF canvasPoint(static_cast<qreal>(canvasPos.x),
                              static_cast<qreal>(canvasPos.y));

    // Phase 5: bezier path editing takes priority when path exists
    if (shape->hasCustomPath()) {
     int vi = -1, ti = -1, tt = 0;
     if (impl_->hitTestCustomPathTangent(layer, canvasPoint, ti, tt)) {
      impl_->beginPathEditTransaction(layer);
      impl_->isDraggingPathTangent_ = true;
      impl_->draggingPathVertexIndex_ = ti;
      impl_->draggingPathTangentType_ = tt;
      setCursor(hudCursor(QStringLiteral("hud_cursor_move.svg"),
                          Qt::ClosedHandCursor));
      event->accept();
      return;
     }
      if (impl_->hitTestCustomPathVertex(layer, canvasPoint, vi)) {
      const bool extendSelection = (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) != 0;
      auto selectedIt = std::find(impl_->selectedPathVertexIndices_.begin(),
                                  impl_->selectedPathVertexIndices_.end(), vi);
      if (extendSelection) {
       if (selectedIt == impl_->selectedPathVertexIndices_.end())
        impl_->selectedPathVertexIndices_.push_back(vi);
       else
        impl_->selectedPathVertexIndices_.erase(selectedIt);
      } else if (selectedIt == impl_->selectedPathVertexIndices_.end()) {
       impl_->selectedPathVertexIndices_ = {vi};
      }
      if (impl_->selectedPathVertexIndices_.empty()) {
       impl_->requestRender();
       event->accept();
       return;
      }
      impl_->beginPathEditTransaction(layer);
      impl_->isDraggingPathVertex_ = true;
      impl_->draggingPathVertexIndex_ = vi;
      impl_->selectedPathDragBefore_ = shape->customPathVertices();
      impl_->beginPathProportionalDragSnapshot(shape->customPathVertices(), vi);
      setCursor(hudCursor(QStringLiteral("hud_cursor_move.svg"),
                          Qt::ClosedHandCursor));
      event->accept();
      return;
     }
     // Click on empty area → add new bezier vertex
     impl_->selectedPathVertexIndices_.clear();
     impl_->beginPathEditTransaction(layer);
     const QTransform gt = layer->getGlobalTransform();
     bool invertible = false;
     const QTransform inv = gt.inverted(&invertible);
     if (invertible) {
      auto verts = shape->customPathVertices();
      CustomPathVertex nv;
      nv.pos = inv.map(canvasPoint);
      verts.push_back(nv);
      shape->setCustomPathVertices(verts, shape->customPathClosed());
      impl_->markPathEditDirty();
      impl_->hoveredPathVertexIndex_ = static_cast<int>(verts.size()) - 1;
      impl_->requestRender();
      event->accept();
      return;
     }
    }

    int vertexIndex = -1;
     if (impl_->hitTestShapeVertex(layer, canvasPoint, vertexIndex)) {
      const bool extendSelection = (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) != 0;
      auto selectedIt = std::find(impl_->selectedShapeVertexIndices_.begin(),
                                  impl_->selectedShapeVertexIndices_.end(), vertexIndex);
      if (extendSelection) {
       if (selectedIt == impl_->selectedShapeVertexIndices_.end())
        impl_->selectedShapeVertexIndices_.push_back(vertexIndex);
       else
        impl_->selectedShapeVertexIndices_.erase(selectedIt);
      } else if (selectedIt == impl_->selectedShapeVertexIndices_.end()) {
       impl_->selectedShapeVertexIndices_ = {vertexIndex};
      }
      if (impl_->selectedShapeVertexIndices_.empty()) {
       impl_->requestRender();
       event->accept();
       return;
      }
      impl_->beginShapeEditTransaction(layer);
      impl_->isDraggingShapeVertex_ = true;
      impl_->draggingShapeVertexIndex_ = vertexIndex;
      impl_->selectedShapeDragBefore_ = shape->customPolygonPoints();
     impl_->beginShapeProportionalDragSnapshot(shape->customPolygonPoints(), vertexIndex);
     setCursor(hudCursor(QStringLiteral("hud_cursor_move.svg"),
                         Qt::ClosedHandCursor));
     event->accept();
     return;
    }

    int insertIndex = -1;
    if (impl_->hitTestShapeSegment(layer, canvasPoint, insertIndex)) {
      impl_->hoveredShapeSegmentIndex_ = std::max(0, insertIndex - 1);
      if (impl_->insertPointOnHoveredShapeSegment(layer, canvasPoint)) {
       setCursor(hudCursor(QStringLiteral("hud_cursor_move.svg"),
                           Qt::ClosedHandCursor));
       impl_->requestRender();
       event->accept();
       return;
      }
    }

     impl_->beginShapeEditTransaction(layer);
     impl_->selectedShapeVertexIndices_.clear();
    std::vector<QPointF> points = shape->customPolygonPoints();
    const QTransform globalTransform = layer->getGlobalTransform();
    bool invertible = false;
    const QTransform invTransform = globalTransform.inverted(&invertible);
    if (invertible) {
     const QPointF rawLocalPos = invTransform.map(canvasPoint);
     const QPointF localPos(
         std::clamp(rawLocalPos.x(), 0.0, static_cast<double>(shape->shapeWidth())),
         std::clamp(rawLocalPos.y(), 0.0, static_cast<double>(shape->shapeHeight())));
     if (points.size() < 3) {
      points.clear();
      const int sides = std::max(3, shape->polygonSides());
      const double width = std::max(1, shape->shapeWidth());
      const double height = std::max(1, shape->shapeHeight());
      const QPointF center(width * 0.5, height * 0.5);
      const double radius = std::min(width, height) * 0.45;
      for (int i = 0; i < sides; ++i) {
       const double angle = static_cast<double>(i) * 2.0 * M_PI / static_cast<double>(sides) - M_PI * 0.5;
       points.push_back(center + QPointF(std::cos(angle) * radius, std::sin(angle) * radius));
      }
     } else {
     points.push_back(localPos);
     }
     shape->setCustomPolygonPoints(points, shape->customPolygonClosed());
     impl_->markShapeEditDirty();
     impl_->hoveredShapeVertexIndex_ = static_cast<int>(points.size()) - 1;
     impl_->hoveredShapeSegmentIndex_ = static_cast<int>(points.size()) - 2;
     impl_->requestRender();
     event->accept();
     return;
    }
   }
  }

    if (impl_->editMode_ == EditMode::Mask && event->button() == Qt::LeftButton && impl_->renderer_) {
   auto layer = impl_->targetLayer();
   if (layer && layer->isVisible() && !layer->isLocked()) {
     const Detail::float2 canvasPos = impl_->renderer_->viewportToCanvas(
         {(float)event->position().x(), (float)event->position().y()});
     const QPointF canvasPoint(static_cast<qreal>(canvasPos.x),
                               static_cast<qreal>(canvasPos.y));

    int handleMaskIndex = -1;
    int handlePathIndex = -1;
    int handleVertexIndex = -1;
    MaskHandleType handleType = MaskHandleType::None;
    const float handleThreshold = 10.0f / std::max(0.1f, impl_->renderer_->getZoom());
    if (hitTestMaskHandle(layer, canvasPoint, handleThreshold,
                          handleMaskIndex, handlePathIndex, handleVertexIndex,
                          handleType)) {
      impl_->beginMaskEditTransaction(layer);
      impl_->isDraggingMaskVertex_ = false;
      impl_->isDraggingMaskHandle_ = true;
      impl_->draggingMaskIndex_ = handleMaskIndex;
      impl_->draggingPathIndex_ = handlePathIndex;
      impl_->draggingVertexIndex_ = handleVertexIndex;
      impl_->draggingMaskHandleType_ = static_cast<int>(handleType);
      impl_->hoveredMaskIndex_ = handleMaskIndex;
      impl_->hoveredPathIndex_ = handlePathIndex;
      impl_->hoveredVertexIndex_ = handleVertexIndex;
      impl_->hoveredMaskHandleType_ = static_cast<int>(handleType);
      setCursor(hudCursor(QStringLiteral("hud_cursor_move.svg"),
                          Qt::ClosedHandCursor));
      event->accept();
      return;
    }

    int maskIndex = -1;
    int pathIndex = -1;
    int vertexIndex = -1;
     if (impl_->hitTestMaskVertex(layer, canvasPoint, maskIndex, pathIndex, vertexIndex)) {
      LayerMask mask = layer->mask(maskIndex);
      MaskPath path = mask.maskPath(pathIndex);
      if (vertexIndex == 0 && !path.isClosed() && path.vertexCount() > 2) {
       impl_->beginMaskEditTransaction(layer);
       path.setClosed(true);
      mask.setMaskPath(pathIndex, path);
      layer->setMask(maskIndex, mask);
      impl_->markMaskEditDirty();
      impl_->requestRender();
      event->accept();
      return;
     }
      impl_->beginMaskEditTransaction(layer);
      impl_->isDraggingMaskVertex_ = true;
      impl_->draggingMaskIndex_ = maskIndex;
      impl_->draggingPathIndex_ = pathIndex;
      impl_->draggingVertexIndex_ = vertexIndex;
      impl_->beginMaskProportionalDragSnapshot(path, vertexIndex);
     setCursor(hudCursor(QStringLiteral("hud_cursor_move.svg"),
                         Qt::ClosedHandCursor));
     event->accept();
     return;
    }

    // 空クリックでは新規マスクを自動生成しない。
    // 既存の頂点・ハンドル編集だけを行い、作成は別の明示操作に寄せる。
    event->accept();
    return;
   }
  }

  QWidget::mousePressEvent(event);
 }

void ArtifactLayerEditorWidgetV2::mouseReleaseEvent(QMouseEvent* event)
 {
 if (impl_->isPanning_ &&
     (event->button() == Qt::MiddleButton ||
      event->button() == Qt::RightButton)) {
   impl_->isPanning_ = false;
   if (!impl_->updateViewportChromeHover(event->position())) {
    const auto layer = impl_->targetLayer();
    if (isMaskEditingMode(impl_->editMode_) && layer &&
        layer->isVisible() && !layer->isLocked()) {
     setCursor(Qt::CrossCursor);
    } else {
     unsetCursor();
    }
   }
   event->accept();
   return;
  }

  if (impl_->editMode_ == EditMode::Mask && event->button() == Qt::LeftButton) {
   if (impl_->isDraggingMaskHandle_) {
    impl_->isDraggingMaskHandle_ = false;
    impl_->draggingMaskIndex_ = -1;
    impl_->draggingPathIndex_ = -1;
    impl_->draggingVertexIndex_ = -1;
    impl_->resetProportionalDragState();
    impl_->draggingMaskHandleType_ = -1;
    impl_->commitMaskEditTransaction();
    unsetCursor();
    event->accept();
    return;
   }
   if (impl_->isDraggingMaskVertex_) {
    impl_->isDraggingMaskVertex_ = false;
    impl_->draggingMaskIndex_ = -1;
    impl_->draggingPathIndex_ = -1;
    impl_->draggingVertexIndex_ = -1;
    impl_->resetProportionalDragState();
    impl_->commitMaskEditTransaction();
    unsetCursor();
    event->accept();
    return;
   }
  }
  // Phase 1: parametric handle release → push undo
  if (event->button() == Qt::LeftButton) {
   if (impl_->isDraggingCornerRadius_) {
    impl_->isDraggingCornerRadius_ = false;
    unsetCursor();
    auto layer = impl_->paramHandleEditLayer_.lock();
    auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
    if (shape) {
     const float after = shape->cornerRadius();
     if (std::abs(after - impl_->cornerRadiusBefore_) > 0.001f) {
      if (auto* undo = UndoManager::instance()) {
       undo->push(std::make_unique<CornerRadiusEditCommand>(layer, impl_->cornerRadiusBefore_, after));
      }
     }
    }
    impl_->paramHandleEditLayer_.reset();
    event->accept();
    return;
   }
   if (impl_->isDraggingStarInnerRadius_) {
    impl_->isDraggingStarInnerRadius_ = false;
    unsetCursor();
    auto layer = impl_->paramHandleEditLayer_.lock();
    auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
    if (shape) {
     const float after = shape->starInnerRadius();
     if (std::abs(after - impl_->starInnerRadiusBefore_) > 0.001f) {
      if (auto* undo = UndoManager::instance()) {
       undo->push(std::make_unique<StarInnerRadiusEditCommand>(layer, impl_->starInnerRadiusBefore_, after));
      }
     }
    }
    impl_->paramHandleEditLayer_.reset();
    event->accept();
    return;
   }
  }
  if (isShapeEditingMode(impl_->editMode_) && event->button() == Qt::LeftButton) {
   // Phase 5: path vertex/tangent release
   if (impl_->isDraggingPathVertex_ || impl_->isDraggingPathTangent_) {
   impl_->isDraggingPathVertex_ = false;
   impl_->isDraggingPathTangent_ = false;
   impl_->draggingPathVertexIndex_ = -1;
    impl_->resetProportionalDragState();
    impl_->commitPathEditTransaction();
    unsetCursor();
    event->accept();
    return;
   }
   if (impl_->pathEditPending_) {
    impl_->commitPathEditTransaction();
    event->accept();
    return;
   }
   if (impl_->isDraggingShapeVertex_) {
   impl_->isDraggingShapeVertex_ = false;
    impl_->draggingShapeVertexIndex_ = -1;
     impl_->selectedShapeDragBefore_.clear();
    impl_->resetProportionalDragState();
    impl_->commitShapeEditTransaction();
    unsetCursor();
    event->accept();
    return;
   }
   if (impl_->shapeEditPending_) {
    impl_->commitShapeEditTransaction();
    event->accept();
    return;
   }
  }
  if (impl_->transformGizmo_ && event->button() == Qt::LeftButton && impl_->transformGizmo_->isDragging()) {
   impl_->transformGizmo_->handleMouseRelease();
   impl_->requestRender();
   unsetCursor();
   event->accept();
   return;
  }
  QWidget::mouseReleaseEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::mouseDoubleClickEvent(QMouseEvent* event)
 {
  if (impl_->editMode_ == EditMode::Mask && event->button() == Qt::LeftButton && impl_->renderer_) {
   auto layer = impl_->targetLayer();
   if (layer && layer->isVisible() && !layer->isLocked()) {
     const Detail::float2 canvasPos = impl_->renderer_->viewportToCanvas(
         {(float)event->position().x(), (float)event->position().y()});
     const QPointF canvasPoint(static_cast<qreal>(canvasPos.x),
                               static_cast<qreal>(canvasPos.y));
    int maskIndex = -1;
    int pathIndex = -1;
    int vertexIndex = -1;
    if (impl_->hitTestMaskVertex(layer, canvasPoint, maskIndex, pathIndex, vertexIndex)) {
     LayerMask mask = layer->mask(maskIndex);
     MaskPath path = mask.maskPath(pathIndex);
     if (vertexIndex == 0 && !path.isClosed() && path.vertexCount() > 2) {
      impl_->beginMaskEditTransaction(layer);
      path.setClosed(true);
      mask.setMaskPath(pathIndex, path);
      layer->setMask(maskIndex, mask);
      impl_->markMaskEditDirty();
      impl_->requestRender();
      event->accept();
      return;
     }
    }
   }
  }
  QWidget::mouseDoubleClickEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::mouseMoveEvent(QMouseEvent* event)
 {
  if (impl_->isPanning_) {
   const QPointF currentPos = event->position();
   const QPointF delta = currentPos - impl_->lastMousePos_;
   impl_->lastMousePos_ = currentPos;
   panBy(delta);
   event->accept();
   return;
  }

  if (event->buttons() == Qt::NoButton &&
      impl_->updateViewportChromeHover(event->position())) {
   event->accept();
   return;
  }

  // Phase 1: parametric shape handle drag
  if ((impl_->isDraggingCornerRadius_ || impl_->isDraggingStarInnerRadius_) && impl_->renderer_) {
   auto layer = impl_->paramHandleEditLayer_.lock();
   auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
   if (shape) {
    if (impl_->isDraggingCornerRadius_) {
     const float dx = static_cast<float>(event->position().x()) - impl_->cornerRadiusDragAnchorX_;
     const float newCr = std::clamp(impl_->cornerRadiusDragStart_ + dx * 0.5f, 0.0f, impl_->cornerRadiusDragMaxCr_);
     shape->setCornerRadius(newCr);
     impl_->requestRender();
     event->accept();
     return;
    }
    if (impl_->isDraggingStarInnerRadius_) {
     const Detail::float2 cp = impl_->renderer_->viewportToCanvas(
         {(float)event->position().x(), (float)event->position().y()});
     const QTransform gt = layer->getGlobalTransform();
     bool inv = false;
     const QTransform invGt = gt.inverted(&inv);
     if (inv) {
      const QPointF localPos = invGt.map(QPointF(cp.x, cp.y));
      const float outerR = std::min(shape->shapeWidth(), shape->shapeHeight()) * 0.5f;
      const QPointF center(shape->shapeWidth() * 0.5, shape->shapeHeight() * 0.5);
       const float dist = static_cast<float>(QLineF(center, localPos).length());
      shape->setStarInnerRadius(outerR > 0.001f ? std::clamp(dist / outerR, 0.05f, 0.99f) : impl_->starInnerRadiusDragStart_);
     }
     impl_->requestRender();
     event->accept();
     return;
    }
   }
  }
  // Phase 1: hover update in View mode
  if (impl_->surfaceMode_ == LayerSurfaceMode::Edit &&
      impl_->displayMode_ != DisplayMode::Mask &&
      impl_->editMode_ != EditMode::Mask &&
      !isShapeEditingMode(impl_->editMode_) && impl_->renderer_) {
   auto layer = impl_->targetLayer();
   if (layer && layer->isVisible() && !layer->isLocked()) {
    const Detail::float2 cp = impl_->renderer_->viewportToCanvas(
        {(float)event->position().x(), (float)event->position().y()});
    const QPointF canvasPoint(cp.x, cp.y);
    const bool prevCr = impl_->hoveredCornerRadius_;
    const bool prevStar = impl_->hoveredStarInnerRadius_;
    impl_->hoveredCornerRadius_ = impl_->hitTestCornerRadiusHandle(layer, canvasPoint);
    impl_->hoveredStarInnerRadius_ = impl_->hitTestStarInnerRadiusHandle(layer, canvasPoint);
    if (prevCr != impl_->hoveredCornerRadius_ || prevStar != impl_->hoveredStarInnerRadius_) {
     impl_->requestRender();
    }
   }
  }

  if (impl_->surfaceMode_ == LayerSurfaceMode::Edit &&
      impl_->transformGizmo_ && impl_->renderer_ &&
      impl_->displayMode_ != DisplayMode::Mask &&
      impl_->editMode_ != EditMode::Mask &&
      !isShapeEditingMode(impl_->editMode_)) {
   auto layer = impl_->targetLayer();
   if (layer && layer->isVisible() && !layer->isLocked()) {
    if (impl_->transformGizmo_->isDragging()) {
     if (impl_->transformGizmo_->handleMouseMove(
             {event->position().x(), event->position().y()}, impl_->renderer_.get())) {
      impl_->requestRender();
      setCursor(hudCursorForTransformHandle(impl_->transformGizmo_->activeHandle(),
                                            true));
      event->accept();
      return;
     }
    } else {
     const auto handle = impl_->transformGizmo_->handleAtViewportPos(
         {event->position().x(), event->position().y()}, impl_->renderer_.get());
     setCursor(hudCursorForTransformHandle(handle, false));
    }
   } else {
    unsetCursor();
   }
  }

  if (impl_->editMode_ == EditMode::Mask && impl_->renderer_) {
   auto layer = impl_->targetLayer();
   if (layer && layer->isVisible() && !layer->isLocked()) {
     const Detail::float2 canvasPos = impl_->renderer_->viewportToCanvas(
         {(float)event->position().x(), (float)event->position().y()});
     const QPointF canvasPoint(static_cast<qreal>(canvasPos.x),
                               static_cast<qreal>(canvasPos.y));
    if (impl_->isDraggingMaskHandle_) {
     const QTransform globalTransform = layer->getGlobalTransform();
     bool invertible = false;
     const QTransform invTransform = globalTransform.inverted(&invertible);
     if (invertible) {
      LayerMask mask = layer->mask(impl_->draggingMaskIndex_);
      MaskPath path = mask.maskPath(impl_->draggingPathIndex_);
      MaskVertex vertex = path.vertex(impl_->draggingVertexIndex_);
      const QPointF localPos = invTransform.map(canvasPoint);
      const QPointF delta = localPos - vertex.position;
      if (impl_->draggingMaskHandleType_ == static_cast<int>(MaskHandleType::InTangent)) {
       vertex.inTangent = delta;
      } else if (impl_->draggingMaskHandleType_ == static_cast<int>(MaskHandleType::OutTangent)) {
       vertex.outTangent = delta;
      }
      path.setVertex(impl_->draggingVertexIndex_, vertex);
      mask.setMaskPath(impl_->draggingPathIndex_, path);
      layer->setMask(impl_->draggingMaskIndex_, mask);
      impl_->markMaskEditDirty();
      impl_->requestRender();
      event->accept();
      return;
     }
    }
    if (impl_->isDraggingMaskVertex_) {
     const QTransform globalTransform = layer->getGlobalTransform();
     bool invertible = false;
     const QTransform invTransform = globalTransform.inverted(&invertible);
     if (invertible) {
      LayerMask mask = layer->mask(impl_->draggingMaskIndex_);
      MaskPath path = mask.maskPath(impl_->draggingPathIndex_);
      MaskVertex vertex = path.vertex(impl_->draggingVertexIndex_);
      const QPointF localPos = invTransform.map(canvasPoint);
      if (!impl_->applyMaskProportionalDrag(layer, localPos)) {
       vertex.position = localPos;
       path.setVertex(impl_->draggingVertexIndex_, vertex);
       mask.setMaskPath(impl_->draggingPathIndex_, path);
       layer->setMask(impl_->draggingMaskIndex_, mask);
       impl_->markMaskEditDirty();
      }
      impl_->requestRender();
      event->accept();
      return;
     }
    } else {
     const int beforeMask = impl_->hoveredMaskIndex_;
     const int beforePath = impl_->hoveredPathIndex_;
     const int beforeVertex = impl_->hoveredVertexIndex_;
     const int beforeHandleType = impl_->hoveredMaskHandleType_;
      impl_->updateMaskHover(layer, canvasPoint);
     if (beforeMask != impl_->hoveredMaskIndex_ ||
         beforePath != impl_->hoveredPathIndex_ ||
         beforeVertex != impl_->hoveredVertexIndex_ ||
         beforeHandleType != impl_->hoveredMaskHandleType_) {
      impl_->requestRender();
     }
     if (impl_->hoveredVertexIndex_ >= 0) {
      setCursor(Qt::CrossCursor);
     } else {
      unsetCursor();
     }
    }
   }
  }
  if (impl_->displayMode_ != DisplayMode::Mask &&
      isShapeEditingMode(impl_->editMode_) && impl_->renderer_) {
   auto layer = impl_->targetLayer();
   auto shape = layer && layer->isVisible() && !layer->isLocked()
       ? std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)
       : std::shared_ptr<ArtifactShapeLayer>{};
   if (shape) {
    const Detail::float2 canvasPos = impl_->renderer_->viewportToCanvas(
        {(float)event->position().x(), (float)event->position().y()});
    const QPointF canvasPoint(static_cast<qreal>(canvasPos.x),
                              static_cast<qreal>(canvasPos.y));
    // Phase 5: bezier path drag
    if (shape->hasCustomPath()) {
     if (impl_->isDraggingPathVertex_ && impl_->draggingPathVertexIndex_ >= 0) {
      const QTransform gt = layer->getGlobalTransform();
      bool invertible = false;
      const QTransform inv = gt.inverted(&invertible);
     if (invertible) {
       auto verts = shape->customPathVertices();
        const int idx = impl_->draggingPathVertexIndex_;
        if (idx < static_cast<int>(verts.size())) {
        const QPointF localPos = inv.map(canvasPoint);
        if (!impl_->proportionalEditingEnabled_ &&
            impl_->selectedPathVertexIndices_.size() > 1 &&
            impl_->selectedPathDragBefore_.size() == verts.size()) {
         const QPointF delta = localPos - impl_->selectedPathDragBefore_[static_cast<size_t>(idx)].pos;
         for (const int selectedIndex : impl_->selectedPathVertexIndices_) {
          if (selectedIndex >= 0 && selectedIndex < static_cast<int>(verts.size())) {
           verts[static_cast<size_t>(selectedIndex)].pos =
               impl_->selectedPathDragBefore_[static_cast<size_t>(selectedIndex)].pos + delta;
          }
         }
         shape->setCustomPathVertices(verts, shape->customPathClosed());
         impl_->markPathEditDirty();
        } else if (!impl_->applyPathProportionalDrag(*shape, localPos)) {
        verts[idx].pos = localPos;
        shape->setCustomPathVertices(verts, shape->customPathClosed());
        impl_->markPathEditDirty();
       }
        impl_->requestRender();
        event->accept();
        return;
       }
      }
     }
     if (impl_->isDraggingPathTangent_ && impl_->draggingPathVertexIndex_ >= 0) {
      const QTransform gt = layer->getGlobalTransform();
      bool invertible = false;
      const QTransform inv = gt.inverted(&invertible);
      if (invertible) {
       auto verts = shape->customPathVertices();
       const int idx = impl_->draggingPathVertexIndex_;
       if (idx < static_cast<int>(verts.size())) {
        const QPointF localPos = inv.map(canvasPoint);
        const QPointF delta = localPos - verts[idx].pos;
        const bool independentTangent = (event->modifiers() & Qt::AltModifier) != 0;
        if (impl_->draggingPathTangentType_ == 1) {
         verts[idx].outTangent = delta;
         if (verts[idx].smooth && !independentTangent) {
          verts[idx].inTangent = -delta;
         }
        } else {
         verts[idx].inTangent = delta;
         if (verts[idx].smooth && !independentTangent) {
          verts[idx].outTangent = -delta;
         }
        }
        shape->setCustomPathVertices(verts, shape->customPathClosed());
        impl_->markPathEditDirty();
        impl_->requestRender();
        event->accept();
        return;
       }
      }
     }
     // Hover update for path vertices/tangents
     const int prevVi = impl_->hoveredPathVertexIndex_;
     const int prevTi = impl_->hoveredPathTangentIndex_;
     int vi = -1, ti = -1, tt = 0;
     if (impl_->hitTestCustomPathTangent(layer, canvasPoint, ti, tt)) {
      impl_->hoveredPathVertexIndex_ = -1;
      impl_->hoveredPathTangentIndex_ = ti;
      impl_->hoveredPathTangentType_ = tt;
     } else if (impl_->hitTestCustomPathVertex(layer, canvasPoint, vi)) {
      impl_->hoveredPathVertexIndex_ = vi;
      impl_->hoveredPathTangentIndex_ = -1;
     } else {
      impl_->hoveredPathVertexIndex_ = -1;
      impl_->hoveredPathTangentIndex_ = -1;
     }
     if (prevVi != impl_->hoveredPathVertexIndex_ || prevTi != impl_->hoveredPathTangentIndex_) {
      impl_->requestRender();
     }
     return;
    }
    if (shape->hasCustomPolygon()) {
     const QTransform globalTransform = layer->getGlobalTransform();
     bool invertible = false;
     const QTransform invTransform = globalTransform.inverted(&invertible);
     if (invertible) {
      std::vector<QPointF> points = shape->customPolygonPoints();
      if (impl_->draggingShapeVertexIndex_ >= 0 &&
          impl_->draggingShapeVertexIndex_ < static_cast<int>(points.size())) {
        const QPointF rawLocalPos = invTransform.map(canvasPoint);
        impl_->beginShapeEditTransaction(layer);
        if (!impl_->proportionalEditingEnabled_ &&
            impl_->selectedShapeVertexIndices_.size() > 1 &&
            impl_->selectedShapeDragBefore_.size() == points.size()) {
         const int anchorIndex = impl_->draggingShapeVertexIndex_;
         const QPointF delta = rawLocalPos -
                               impl_->selectedShapeDragBefore_[static_cast<size_t>(anchorIndex)];
         for (const int selectedIndex : impl_->selectedShapeVertexIndices_) {
          if (selectedIndex >= 0 && selectedIndex < static_cast<int>(points.size())) {
           const QPointF moved = impl_->selectedShapeDragBefore_[static_cast<size_t>(selectedIndex)] + delta;
           points[static_cast<size_t>(selectedIndex)] = QPointF(
               std::clamp(moved.x(), 0.0, static_cast<double>(shape->shapeWidth())),
               std::clamp(moved.y(), 0.0, static_cast<double>(shape->shapeHeight())));
          }
         }
         shape->setCustomPolygonPoints(points, shape->customPolygonClosed());
         impl_->markShapeEditDirty();
        } else if (!impl_->applyShapeProportionalDrag(layer, *shape, rawLocalPos)) {
        points[static_cast<size_t>(impl_->draggingShapeVertexIndex_)] = QPointF(
            std::clamp(rawLocalPos.x(), 0.0, static_cast<double>(shape->shapeWidth())),
            std::clamp(rawLocalPos.y(), 0.0, static_cast<double>(shape->shapeHeight())));
        shape->setCustomPolygonPoints(points, shape->customPolygonClosed());
        impl_->markShapeEditDirty();
       }
       impl_->requestRender();
       event->accept();
       return;
      }
     }
    } else {
     const int beforeVertex = impl_->hoveredShapeVertexIndex_;
     const int beforeSegment = impl_->hoveredShapeSegmentIndex_;
     impl_->updateShapeHover(layer, canvasPoint);
     if (beforeVertex != impl_->hoveredShapeVertexIndex_ ||
         beforeSegment != impl_->hoveredShapeSegmentIndex_) {
      impl_->requestRender();
     }
     if (impl_->hoveredShapeVertexIndex_ >= 0 ||
         impl_->hoveredShapeSegmentIndex_ >= 0) {
      setCursor(Qt::CrossCursor);
     } else {
      unsetCursor();
     }
    }
   } // if (shape)
 } // if (editMode_ == Paint)
 QWidget::mouseMoveEvent(event);
}


 void ArtifactLayerEditorWidgetV2::wheelEvent(QWheelEvent* event)
 {
  if (!impl_->renderer_) {
   QWidget::wheelEvent(event);
   return;
  }

  const float steps = static_cast<float>(event->angleDelta().y()) / 120.0f;
  if (std::abs(steps) <= std::numeric_limits<float>::epsilon()) {
   event->ignore();
   return;
  }

  const float currentZoom = impl_->renderer_->getZoom();
  const float zoomFactor = std::pow(1.1f, steps);
  impl_->zoomLevel_ = std::clamp(currentZoom * zoomFactor, 0.05f, 32.0f);
  zoomAroundPoint(event->position(), impl_->zoomLevel_);
  event->accept();
 }

void ArtifactLayerEditorWidgetV2::resizeEvent(QResizeEvent* event)
{
 QWidget::resizeEvent(event);
 if (event->size().width() <= 0 || event->size().height() <= 0) {
  return;
 }
 if (impl_->hoveredChromeControl_ >= 0) {
  impl_->hoveredChromeControl_ = -1;
  setToolTip(QString{});
  const auto layer = impl_->targetLayer();
  if (isMaskEditingMode(impl_->editMode_) && layer &&
      layer->isVisible() && !layer->isLocked()) {
   setCursor(Qt::CrossCursor);
  } else {
   unsetCursor();
  }
 }
 impl_->recreateSwapChain(this);
 if (impl_->initialized_ && impl_->renderer_) {
  impl_->requestRender();
 }
}

void ArtifactLayerEditorWidgetV2::paintEvent(QPaintEvent* event)
 {

 }

void ArtifactLayerEditorWidgetV2::contextMenuEvent(QContextMenuEvent* event)
{
 if (!impl_) {
  QWidget::contextMenuEvent(event);
  return;
 }
 QMenu menu(this);
 QAction* clearShapeOperatorsAct = nullptr;
 QAction* addTrimPathsAct = nullptr;
 QAction* addRepeaterAct = nullptr;
 QAction* addMergePathsAct = nullptr;
 QAction* addOffsetPathsAct = nullptr;
 QAction* addPuckerBloatAct = nullptr;
 QAction* addRoundedCornersAct = nullptr;
 QAction* addWigglePathsAct = nullptr;
 QAction* addZigZagAct = nullptr;
 QAction* addTwistAct = nullptr;
 QAction* insertPointAct = nullptr;
 QAction* splitSegmentAct = nullptr;
 QAction* deletePointAct = nullptr;
 QAction* toggleClosedAct = nullptr;
 QAction* convertToPathAct = nullptr;
 QAction* convertToPolygonAct = nullptr;
 QAction* pathDeletePointAct = nullptr;
 QAction* pathToggleSmoothAct = nullptr;
 QAction* pathToggleClosedAct = nullptr;
 std::shared_ptr<ArtifactShapeLayer> shapeLayer;
 if (impl_->displayMode_ != DisplayMode::Mask &&
     isShapeEditingMode(impl_->editMode_) && impl_->renderer_) {
  auto layer = impl_->targetLayer();
  if (layer && layer->isVisible() && !layer->isLocked()) {
   shapeLayer = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
  }
  if (shapeLayer) {
   QMenu* shapeOpsMenu = menu.addMenu(QStringLiteral("Add Operator"));
   addTrimPathsAct = shapeOpsMenu->addAction(QStringLiteral("Trim Paths"));
   addRepeaterAct = shapeOpsMenu->addAction(QStringLiteral("Repeater"));
   addMergePathsAct = shapeOpsMenu->addAction(QStringLiteral("Merge Paths"));
   addOffsetPathsAct = shapeOpsMenu->addAction(QStringLiteral("Offset Paths"));
   addPuckerBloatAct = shapeOpsMenu->addAction(QStringLiteral("Pucker & Bloat"));
   addRoundedCornersAct = shapeOpsMenu->addAction(QStringLiteral("Rounded Corners"));
   addWigglePathsAct = shapeOpsMenu->addAction(QStringLiteral("Wiggle Paths"));
   addZigZagAct = shapeOpsMenu->addAction(QStringLiteral("ZigZag"));
   addTwistAct = shapeOpsMenu->addAction(QStringLiteral("Twist"));
   clearShapeOperatorsAct = menu.addAction(QStringLiteral("Clear Shape Operators"));
   clearShapeOperatorsAct->setEnabled(shapeLayer->shapeOperatorCount() > 0);
   if (shapeLayer->shapeOperatorCount() > 0) {
    QMenu* opManageMenu = menu.addMenu(QStringLiteral("Manage Operators"));
    for (int oi = 0; oi < shapeLayer->shapeOperatorCount(); ++oi) {
     const auto ot = shapeLayer->shapeOperatorTypeAt(oi);
     const QString opName = QString::number(oi + 1) + QStringLiteral(". ") +
                            shapeOperatorTypeName(ot);
     QMenu* opItemMenu = opManageMenu->addMenu(opName);
     QAction* removeOpAct = opItemMenu->addAction(QStringLiteral("Remove"));
     removeOpAct->setData(oi);
     if (oi > 0) {
      QAction* moveUpAct = opItemMenu->addAction(QStringLiteral("Move Up"));
      moveUpAct->setData(oi);
     }
     if (oi < shapeLayer->shapeOperatorCount() - 1) {
      QAction* moveDownAct = opItemMenu->addAction(QStringLiteral("Move Down"));
      moveDownAct->setData(oi);
     }
    }
   }
    if (shapeLayer->hasCustomPolygon()) {
    const Detail::float2 canvasPos = impl_->renderer_->viewportToCanvas(
        {(float)event->pos().x(), (float)event->pos().y()});
    const QPointF canvasPoint(static_cast<qreal>(canvasPos.x),
                              static_cast<qreal>(canvasPos.y));
    impl_->shapeContextMenuCanvasPos_ = canvasPoint;
    impl_->updateShapeHover(layer, canvasPoint);

    insertPointAct = menu.addAction(QStringLiteral("Insert Point"));
    splitSegmentAct = menu.addAction(QStringLiteral("Split Segment"));
    deletePointAct = menu.addAction(QStringLiteral("Delete Point"));
    toggleClosedAct = menu.addAction(shapeLayer->customPolygonClosed()
                                         ? QStringLiteral("Open Polygon")
                                         : QStringLiteral("Close Polygon"));
    insertPointAct->setEnabled(impl_->hoveredShapeSegmentIndex_ >= 0);
    splitSegmentAct->setEnabled(impl_->hoveredShapeSegmentIndex_ >= 0);
    deletePointAct->setEnabled(impl_->hoveredShapeVertexIndex_ >= 0);
    toggleClosedAct->setEnabled(shapeLayer->customPolygonClosed() || shapeLayer->customPolygonPoints().size() >= 3);
   }
    convertToPathAct = menu.addAction(QStringLiteral("Convert to Editable Path"));
   }
   if (shapeLayer->hasCustomPath()) {
    convertToPolygonAct = menu.addAction(QStringLiteral("Convert to Polygon"));
    pathDeletePointAct = menu.addAction(QStringLiteral("Delete Point"));
    {
     const auto verts = shapeLayer->customPathVertices();
     const int vi = impl_->hoveredPathVertexIndex_;
     const bool validHover = vi >= 0 && static_cast<size_t>(vi) < verts.size();
     pathDeletePointAct->setEnabled(validHover);
     pathToggleSmoothAct = menu.addAction(validHover && verts[static_cast<size_t>(vi)].smooth
                                              ? QStringLiteral("Make Corner")
                                              : QStringLiteral("Make Smooth"));
     pathToggleSmoothAct->setEnabled(validHover);
    }
    pathToggleClosedAct = menu.addAction(shapeLayer->customPathClosed()
                                             ? QStringLiteral("Open Path")
                                             : QStringLiteral("Close Path"));
   }
   if (!menu.actions().isEmpty()) {
    QAction* shapeChosen = menu.exec(event->globalPos());
    if (shapeChosen) {
     bool handled = false;
     if (shapeChosen == clearShapeOperatorsAct) {
      shapeLayer->clearShapeOperators();
      impl_->requestRender();
      event->accept();
      return;
     }
     if (shapeChosen == addTrimPathsAct) {
      shapeLayer->addShapeOperator(ArtifactCore::ShapeOperatorType::TrimPaths);
      handled = true;
     } else if (shapeChosen == addRepeaterAct) {
      shapeLayer->addShapeOperator(ArtifactCore::ShapeOperatorType::Repeater);
      handled = true;
     } else if (shapeChosen == addMergePathsAct) {
      shapeLayer->addShapeOperator(ArtifactCore::ShapeOperatorType::MergePaths);
      handled = true;
     } else if (shapeChosen == addOffsetPathsAct) {
      shapeLayer->addShapeOperator(ArtifactCore::ShapeOperatorType::OffsetPaths);
      handled = true;
     } else if (shapeChosen == addPuckerBloatAct) {
      shapeLayer->addShapeOperator(ArtifactCore::ShapeOperatorType::PuckerBloat);
      handled = true;
     } else if (shapeChosen == addRoundedCornersAct) {
      shapeLayer->addShapeOperator(ArtifactCore::ShapeOperatorType::RoundedCorners);
      handled = true;
     } else if (shapeChosen == addWigglePathsAct) {
      shapeLayer->addShapeOperator(ArtifactCore::ShapeOperatorType::WigglePaths);
      handled = true;
     } else if (shapeChosen == addZigZagAct) {
      shapeLayer->addShapeOperator(ArtifactCore::ShapeOperatorType::ZigZag);
      handled = true;
     } else if (shapeChosen == addTwistAct) {
      shapeLayer->addShapeOperator(ArtifactCore::ShapeOperatorType::Twist);
      handled = true;
     } else if (shapeChosen->text() == QStringLiteral("Remove")) {
      const int oi = shapeChosen->data().toInt();
      if (oi >= 0 && oi < shapeLayer->shapeOperatorCount()) {
       shapeLayer->removeShapeOperatorAt(oi);
       handled = true;
      }
     } else if (shapeChosen->text() == QStringLiteral("Move Up")) {
      const int oi = shapeChosen->data().toInt();
      if (oi > 0 && oi < shapeLayer->shapeOperatorCount()) {
       shapeLayer->moveShapeOperator(oi, oi - 1);
       handled = true;
      }
     } else if (shapeChosen->text() == QStringLiteral("Move Down")) {
      const int oi = shapeChosen->data().toInt();
      if (oi >= 0 && oi < shapeLayer->shapeOperatorCount() - 1) {
       shapeLayer->moveShapeOperator(oi, oi + 1);
       handled = true;
      }
     } else if (shapeLayer->hasCustomPolygon()) {
      if (shapeChosen == deletePointAct) {
       impl_->beginShapeEditTransaction(layer);
       handled = impl_->deleteHoveredShapeVertex(layer);
      } else if (shapeChosen == insertPointAct) {
       impl_->beginShapeEditTransaction(layer);
       handled = impl_->insertPointOnHoveredShapeSegment(layer, impl_->shapeContextMenuCanvasPos_);
      } else if (shapeChosen == splitSegmentAct) {
       impl_->beginShapeEditTransaction(layer);
       handled = impl_->splitHoveredShapeSegment(layer);
      } else if (shapeChosen == toggleClosedAct) {
       impl_->beginShapeEditTransaction(layer);
       const bool closed = shapeLayer->customPolygonClosed();
       const auto points = shapeLayer->customPolygonPoints();
       if (closed || points.size() >= 3) {
        shapeLayer->setCustomPolygonPoints(points, !closed);
        impl_->markShapeEditDirty();
        handled = true;
       }
      } else if (shapeChosen == convertToPathAct) {
       const auto pts = shapeLayer->customPolygonPoints();
       const bool closed = shapeLayer->customPolygonClosed();
       const auto beforePath = shapeLayer->customPathVertices();
       const bool beforePathClosed = shapeLayer->customPathClosed();
       std::vector<CustomPathVertex> verts;
       verts.reserve(pts.size());
       for (const auto &p : pts)
        verts.push_back({p, QPointF(0, 0), QPointF(0, 0), false});
       const auto afterPath = verts;
       shapeLayer->setCustomPathVertices(verts, closed);
       shapeLayer->clearCustomPolygonPoints();
       impl_->hoveredPathVertexIndex_ = verts.empty() ? -1 : 0;
       impl_->hoveredPathTangentIndex_ = -1;
       impl_->hoveredPathTangentType_ = 0;
       if (auto* undo = UndoManager::instance()) {
        undo->push(std::make_unique<ShapeConversionCommand>(
            layer,
            pts, closed,
            beforePath, beforePathClosed,
            std::vector<QPointF>{}, false,
            afterPath, closed));
       }
       handled = true;
      }
      if (handled) {
       impl_->commitPathEditTransaction();
       impl_->requestRender();
       event->accept();
       return;
      }
      impl_->commitPathEditTransaction();
     } else if (shapeLayer->hasCustomPath()) {
      if (shapeChosen == pathDeletePointAct) {
       impl_->beginPathEditTransaction(layer);
       auto verts = shapeLayer->customPathVertices();
       const int vi = impl_->hoveredPathVertexIndex_;
       if (vi >= 0 && vi < static_cast<int>(verts.size())) {
        verts.erase(verts.begin() + vi);
        if (verts.size() >= 3) {
         shapeLayer->setCustomPathVertices(verts, shapeLayer->customPathClosed());
        } else {
         shapeLayer->clearCustomPath();
        }
        impl_->markPathEditDirty();
        handled = true;
       }
      } else if (shapeChosen == pathToggleSmoothAct) {
       impl_->beginPathEditTransaction(layer);
       auto verts = shapeLayer->customPathVertices();
       const int vi = impl_->hoveredPathVertexIndex_;
       if (vi >= 0 && vi < static_cast<int>(verts.size())) {
        verts[static_cast<size_t>(vi)].smooth = !verts[static_cast<size_t>(vi)].smooth;
        shapeLayer->setCustomPathVertices(verts, shapeLayer->customPathClosed());
        impl_->markPathEditDirty();
        handled = true;
       }
      } else if (shapeChosen == pathToggleClosedAct) {
       impl_->beginPathEditTransaction(layer);
       const bool closed = shapeLayer->customPathClosed();
       const auto verts = shapeLayer->customPathVertices();
       if (closed || verts.size() >= 3) {
        shapeLayer->setCustomPathVertices(verts, !closed);
        impl_->markPathEditDirty();
        handled = true;
       }
      } else if (shapeChosen == convertToPolygonAct) {
       const auto verts = shapeLayer->customPathVertices();
       const bool closed = shapeLayer->customPathClosed();
       const auto beforePolygon = shapeLayer->customPolygonPoints();
       const bool beforePolygonClosed = shapeLayer->customPolygonClosed();
       std::vector<QPointF> pts;
       pts.reserve(verts.size());
       for (const auto &v : verts)
        pts.push_back(v.pos);
       const auto afterPolygon = pts;
       shapeLayer->setCustomPolygonPoints(pts, closed);
       shapeLayer->clearCustomPath();
       impl_->hoveredShapeVertexIndex_ = pts.empty() ? -1 : 0;
       impl_->hoveredShapeSegmentIndex_ = pts.size() >= 2 ? 0 : -1;
       if (auto* undo = UndoManager::instance()) {
        undo->push(std::make_unique<ShapeConversionCommand>(
            layer,
            beforePolygon, beforePolygonClosed,
            verts, closed,
            afterPolygon, closed,
            std::vector<Artifact::CustomPathVertex>{}, false));
       }
       handled = true;
      }
      if (handled) {
       impl_->commitShapeEditTransaction();
       impl_->requestRender();
       event->accept();
       return;
      }
     }
     if (handled) {
      impl_->requestRender();
      event->accept();
      return;
     }
    }
   }
  }
  QMenu bgMenu(this);
 QAction* alphaAct = bgMenu.addAction(QStringLiteral("Alpha"));
 QAction* solidAct = bgMenu.addAction(QStringLiteral("Solid"));
 QAction* mayaAct = bgMenu.addAction(QStringLiteral("Maya Gradient"));
 alphaAct->setCheckable(true);
 solidAct->setCheckable(true);
 mayaAct->setCheckable(true);
 switch (impl_->backgroundMode_) {
  case LayerBackgroundMode::Alpha:
   alphaAct->setChecked(true);
   break;
  case LayerBackgroundMode::Solid:
   solidAct->setChecked(true);
   break;
  case LayerBackgroundMode::MayaGradient:
   mayaAct->setChecked(true);
   break;
 }

 QAction* chosen = bgMenu.exec(event->globalPos());
 if (!chosen) {
  event->accept();
  return;
 }
 if (chosen == alphaAct) {
  impl_->backgroundMode_ = LayerBackgroundMode::Alpha;
 } else if (chosen == solidAct) {
  impl_->backgroundMode_ = LayerBackgroundMode::Solid;
 } else if (chosen == mayaAct) {
  impl_->backgroundMode_ = LayerBackgroundMode::MayaGradient;
 } else {
  event->accept();
  return;
 }

 impl_->refreshBackgroundCache();
 if (impl_->initialized_ && impl_->renderer_) {
  impl_->requestRender();
 }
 event->accept();
}

void ArtifactLayerEditorWidgetV2::showEvent(QShowEvent* event)
 {
 QWidget::showEvent(event);
 if (auto *app = Artifact::ApplicationService::instance()) {
  if (auto *toolService = app->toolService()) {
   setDisplayMode(toolService->displayMode());
  }
 }
  qCDebug(layerViewPerfLog) << "[LayerView][Show]"
                            << "initialized=" << impl_->initialized_
                            << "visible=" << isVisible()
                            << "size=" << size();
 if (!impl_->initialized_) {
  impl_->initialize(this);
  if (impl_->initialized_) {
   impl_->initializeSwapChain(this);
   impl_->renderer_->fitToViewport();
   impl_->zoomLevel_ = impl_->renderer_->getZoom();
  }
 }
 if (impl_->initialized_) {
  if (!impl_->targetLayerId_.isNil()) {
   setTargetLayer(impl_->targetLayerId_);
  } else {
    const LayerID selectedId = currentSelectedLayerId();
    if (!selectedId.isNil()) {
     setTargetLayer(selectedId);
    }
   }
   if (impl_->isPlay_) {
    impl_->startRenderLoop();
   } else {
    impl_->requestRender();
   }
  }
 }

 void ArtifactLayerEditorWidgetV2::hideEvent(QHideEvent* event)
 {
  qCDebug(layerViewPerfLog) << "[LayerView][Hide]"
                            << "initialized=" << impl_->initialized_
                            << "visible=" << isVisible()
                            << "size=" << size();
 if (impl_->initialized_) {
   impl_->stopRenderLoop();
  }
  impl_->updateViewportChromeHover(QPointF(-1.0, -1.0));
  QWidget::hideEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::closeEvent(QCloseEvent* event)
 {
  impl_->destroy();
 QWidget::closeEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::focusInEvent(QFocusEvent* event)
 {
  QWidget::focusInEvent(event);
  if (auto *app = Artifact::ApplicationService::instance()) {
   if (auto *toolService = app->toolService()) {
    setDisplayMode(toolService->displayMode());
   }
  }
 }

 void ArtifactLayerEditorWidgetV2::focusOutEvent(QFocusEvent* event)
 {
  impl_->updateViewportChromeHover(QPointF(-1.0, -1.0));
  QWidget::focusOutEvent(event);
 }

void ArtifactLayerEditorWidgetV2::setClearColor(const FloatColor& color)
{
  std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
  impl_->clearColor_ = color;
  if (impl_->initialized_ && impl_->renderer_) {
   impl_->requestRender();
  }
}

void ArtifactLayerEditorWidgetV2::setTargetLayer(const LayerID& id)
{
 std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
 if (impl_->shapeEditPending_) {
  impl_->commitShapeEditTransaction();
 }
 impl_->targetLayerId_ = id;
 publishLayerReadout(this, impl_->targetLayer());
 impl_->surfaceInfoDirty_ = true;
 impl_->isDraggingShapeVertex_ = false;
 impl_->draggingShapeVertexIndex_ = -1;
 impl_->hoveredShapeVertexIndex_ = -1;
 impl_->selectedShapeVertexIndices_.clear();
 impl_->selectedShapeDragBefore_.clear();
 impl_->selectedPathVertexIndices_.clear();
 impl_->selectedPathDragBefore_.clear();
 const uint seed = qHash(id.toString());
 const auto channel = [seed](int shift) -> float {
  const int value = static_cast<int>((seed >> shift) & 0xFFu);
  return 0.25f + (static_cast<float>(value) / 255.0f) * 0.65f;
 };
 impl_->targetLayerTint_ = FloatColor(channel(0), channel(8), channel(16), 1.0f);
 if (impl_->renderer_) {
  if (auto* service = ArtifactProjectService::instance()) {
   if (auto composition = service->currentComposition().lock()) {
    // コンポジションサイズを設定
    const auto compSize = composition->settings().compositionSize();
    if (compSize.width() > 0 && compSize.height() > 0) {
     impl_->renderer_->setCanvasSize(static_cast<float>(compSize.width()), static_cast<float>(compSize.height()));
    }
    
    if (auto layer = composition->layerById(id)) {
     const auto source = layer->sourceSize();
     if (source.width > 0 && source.height > 0) {
      // レイヤーサイズは使用しない（コンポジションサイズを優先）
      // impl_->renderer_->setCanvasSize(static_cast<float>(source.width), static_cast<float>(source.height));
     }
     if (layer->isVisible() && !layer->isLocked() &&
         isShapeEditingMode(impl_->editMode_)) {
      if (auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) {
       if (!shape->hasCustomPolygon()) {
        const std::vector<QPointF> points = buildShapeEditSeedPoints(*shape);
        if (points.size() >= 3) {
         shape->setCustomPolygonPoints(points, true);
        }
        }
       }
      }
      impl_->syncTransformGizmo(layer);
      impl_->renderer_->fitToViewport();
      impl_->zoomLevel_ = impl_->renderer_->getZoom();
      impl_->requestRender();
      return;
     }
   }
  }
  if (impl_->transformGizmo_) {
   impl_->transformGizmo_->setLayer(nullptr);
  }
  impl_->renderer_->resetView();
  impl_->requestRender();
 }
}

 void ArtifactLayerEditorWidgetV2::resetView()
 {
 impl_->zoomLevel_ = 1.0f;
  if (impl_->renderer_) {
   impl_->renderer_->resetView();
   impl_->requestRender();
  }
}
 
 void ArtifactLayerEditorWidgetV2::fitToViewport()
  {
   if (impl_->renderer_) {
    impl_->renderer_->fitToViewport();
    impl_->zoomLevel_ = impl_->renderer_->getZoom();
    impl_->requestRender();
   }
  }
 
void ArtifactLayerEditorWidgetV2::panBy(const QPointF& delta)
{
  if (impl_->renderer_) {
   impl_->renderer_->panBy((float)delta.x(), (float)delta.y());
   impl_->requestRender();
  }
}

void ArtifactLayerEditorWidgetV2::zoomAroundPoint(const QPointF& viewportPos, float newZoom)
{
  if (impl_->renderer_) {
      impl_->renderer_->zoomAroundViewportPoint({(float)viewportPos.x(), (float)viewportPos.y()}, newZoom);
      impl_->requestRender();
  }
}

 void ArtifactLayerEditorWidgetV2::setEditMode(EditMode mode)
 {
  if (impl_->surfaceMode_ != LayerSurfaceMode::Edit && mode != EditMode::View) {
   impl_->editModeBeforeSurface_ = mode;
   mode = EditMode::View;
  }
  const bool wasMaskMode = isMaskEditingMode(impl_->editMode_);
  const bool isMaskMode = isMaskEditingMode(mode);
 impl_->editMode_ = mode;
 if (isMaskMode) {
   if (!wasMaskMode && impl_->displayMode_ != DisplayMode::Mask) {
    impl_->displayModeBeforeMask_ = impl_->displayMode_;
   }
   impl_->displayMode_ = DisplayMode::Mask;
   const auto layer = impl_->targetLayer();
   if (layer && layer->isVisible() && !layer->isLocked()) {
    setCursor(Qt::CrossCursor);
   } else {
    unsetCursor();
   }
  } else {
   unsetCursor();
   impl_->isDraggingMaskVertex_ = false;
   impl_->draggingMaskIndex_ = -1;
   impl_->draggingPathIndex_ = -1;
   impl_->draggingVertexIndex_ = -1;
   impl_->isDraggingMaskHandle_ = false;
   impl_->draggingMaskHandleType_ = -1;
   impl_->isDraggingShapeVertex_ = false;
   impl_->draggingShapeVertexIndex_ = -1;
    impl_->hoveredShapeVertexIndex_ = -1;
    impl_->selectedShapeVertexIndices_.clear();
    impl_->selectedShapeDragBefore_.clear();
    impl_->selectedPathVertexIndices_.clear();
    impl_->selectedPathDragBefore_.clear();
   impl_->hoveredMaskHandleType_ = -1;
   if (impl_->shapeEditPending_) {
    impl_->commitShapeEditTransaction();
   }
   if (wasMaskMode) {
    impl_->displayMode_ = impl_->displayModeBeforeMask_;
   }
  }
  if (impl_->transformGizmo_) {
   if (isMaskMode) {
    impl_->transformGizmo_->setLayer(nullptr);
   } else {
    impl_->syncTransformGizmo(impl_->targetLayer());
   }
  }
  if (isShapeEditingMode(mode) && !impl_->targetLayerId_.isNil()) {
   if (auto* service = ArtifactProjectService::instance()) {
    if (auto composition = service->currentComposition().lock()) {
     if (auto layer = composition->layerById(impl_->targetLayerId_);
         layer && layer->isVisible() && !layer->isLocked()) {
      if (auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) {
       if (!shape->hasCustomPolygon()) {
        const std::vector<QPointF> points = buildShapeEditSeedPoints(*shape);
        if (points.size() >= 3) {
         shape->setCustomPolygonPoints(points, true);
        }
       }
      }
     }
    }
   }
  }
  publishModeReadout(impl_->widget_, impl_->editMode_, impl_->displayMode_);
  if (impl_->initialized_ && impl_->renderer_) {
   impl_->requestRender();
  }
 }

 void ArtifactLayerEditorWidgetV2::setDisplayMode(DisplayMode mode)
 {
  if (isMaskEditingMode(impl_->editMode_)) {
    if (mode != DisplayMode::Mask) {
     impl_->displayModeBeforeMask_ = mode;
    }
    impl_->displayMode_ = DisplayMode::Mask;
  } else {
    impl_->displayMode_ = mode;
    if (mode != DisplayMode::Mask) {
     impl_->displayModeBeforeMask_ = mode;
    }
  }
  if (impl_->transformGizmo_) {
   impl_->syncTransformGizmo(impl_->targetLayer());
  }
  publishModeReadout(impl_->widget_, impl_->editMode_, impl_->displayMode_);
  if (impl_->initialized_ && impl_->renderer_) {
   impl_->requestRender();
  }
 }

void ArtifactLayerEditorWidgetV2::setPan(const QPointF& offset)
{
 if (impl_->renderer_) {
  impl_->renderer_->setPan((float)offset.x(), (float)offset.y());
  impl_->requestRender();
 }
}

 float ArtifactLayerEditorWidgetV2::zoom() const
 {
  return impl_->zoomLevel_;
 }

void ArtifactLayerEditorWidgetV2::setTargetLayer(LayerID& id)
{
 setTargetLayer(static_cast<const LayerID&>(id));
}

 QImage ArtifactLayerEditorWidgetV2::grabScreenShot()
 {
  return grab().toImage();
 }

 void ArtifactLayerEditorWidgetV2::play()
 {
  if (!impl_->initialized_) {
   return;
  }
  impl_->isPlay_ = true;
  impl_->startRenderLoop();
 }

 void ArtifactLayerEditorWidgetV2::stop()
 {
  impl_->isPlay_ = false;
  impl_->stopRenderLoop();
 }

 void ArtifactLayerEditorWidgetV2::takeScreenShot()
 {
  const QImage image = grabScreenShot();
  if (image.isNull()) {
   return;
  }

  QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
  if (defaultDir.isEmpty()) {
   defaultDir = QDir::homePath();
  }
  const QString defaultPath = QDir(defaultDir).filePath(
   QStringLiteral("artifact-layer-view-%1.png").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"))));
  const QString filePath = QFileDialog::getSaveFileName(
   this,
   QStringLiteral("Save Layer View Snapshot"),
   defaultPath,
   QStringLiteral("PNG Image (*.png)"));
  if (filePath.isEmpty()) {
   return;
  }
  image.save(filePath);
 }

};
