module;

#include <QCursor>
#include <QGuiApplication>
#include <QSize>
#include <QString>
#include <QTransform>
#include <QVariant>

#include <algorithm>
#include <cmath>

module Artifact.Widgets.ContentGizmo;

import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Solid2D;
import Artifact.Layers.SolidImage;
import Artifact.Layer.SourceCrop;
import Artifact.Layer.InitParams;
import Artifact.Render.IRenderer;
import Artifact.Widgets.CompositionGizmoUndoCommands;
import Color.Float;
import Memory.SharedPtr;
import Undo.UndoManager;

namespace Artifact {

namespace {

FloatColor cropColor() { return FloatColor{1.0f, 0.72f, 0.2f, 1.0f}; }
FloatColor cropDimColor() { return FloatColor{1.0f, 0.72f, 0.2f, 0.35f}; }
FloatColor frameColor() { return FloatColor{0.6f, 0.6f, 0.65f, 0.5f}; }
FloatColor sizeColor() { return FloatColor{0.3f, 0.85f, 1.0f, 1.0f}; }
FloatColor gradientColor() { return FloatColor{0.9f, 0.4f, 0.9f, 1.0f}; }
FloatColor handleOutline() { return FloatColor{0.1f, 0.1f, 0.1f, 1.0f}; }

QPointF toCanvas(ArtifactIRenderer* renderer, const QPointF& viewportPos) {
  const auto p = renderer->viewportToCanvas(
      Detail::float2{static_cast<float>(viewportPos.x()),
                     static_cast<float>(viewportPos.y())});
  return QPointF(p.x, p.y);
}

QPointF layerToCanvas(const ArtifactAbstractLayerPtr& layer, const QPointF& local) {
  return layer->getGlobalTransform().map(local);
}

QPointF canvasDeltaToLocal(const ArtifactAbstractLayerPtr& layer,
                           const QPointF& canvas0, const QPointF& canvas1) {
  bool ok = false;
  const QTransform inverse = layer->getGlobalTransform().inverted(&ok);
  if (!ok) {
    return QPointF();
  }
  return inverse.map(canvas1) - inverse.map(canvas0);
}

void drawPolyQuad(ArtifactIRenderer* renderer,
                  const QPointF& a, const QPointF& b,
                  const QPointF& c, const QPointF& d,
                  const FloatColor& color, float thickness) {
  const auto f = [](const QPointF& p) {
    return Detail::float2{static_cast<float>(p.x()), static_cast<float>(p.y())};
  };
  renderer->drawSolidLine(f(a), f(b), color, thickness);
  renderer->drawSolidLine(f(b), f(c), color, thickness);
  renderer->drawSolidLine(f(c), f(d), color, thickness);
  renderer->drawSolidLine(f(d), f(a), color, thickness);
}

void drawSquareHandle(ArtifactIRenderer* renderer, const QPointF& center,
                      float size, const FloatColor& fill) {
  const float half = size * 0.5f;
  renderer->drawSolidRect(static_cast<float>(center.x() - half + 1.0f),
                          static_cast<float>(center.y() - half + 1.0f), size,
                          size, FloatColor{0.0f, 0.0f, 0.0f, 0.4f}, 1.0f);
  renderer->drawSolidRect(static_cast<float>(center.x() - half),
                          static_cast<float>(center.y() - half), size, size,
                          fill, 1.0f);
}

double normalizeAngle360(double degrees) {
  double v = std::fmod(degrees, 360.0);
  if (v < 0.0) v += 360.0;
  return v;
}

}  // namespace

ContentGizmo::ContentGizmo() = default;
ContentGizmo::~ContentGizmo() = default;

void ContentGizmo::setLayer(ArtifactAbstractLayerPtr layer) {
  if (isDragging_) {
    cancelInteraction();
  }
  layer_ = std::move(layer);
  targetKind_ = TargetKind::None;
  if (!layer_) {
    return;
  }
  if (ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer_)) {
    targetKind_ = TargetKind::ImageCrop;
  } else if (ArtifactCore::dynamicPointerCast<ArtifactSolid2DLayer>(layer_) ||
             ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(layer_)) {
    targetKind_ = TargetKind::Solid;
  }
}

bool ContentGizmo::resolveTarget() const {
  return layer_ && targetKind_ != TargetKind::None && layer_->isVisible() &&
         !layer_->isLocked() && !layer_->isSelectionLocked();
}

float ContentGizmo::viewportTolerance(ArtifactIRenderer* renderer) const {
  const float zoom = renderer ? renderer->getZoom() : 1.0f;
  if (!std::isfinite(zoom) || zoom <= 0.0f) {
    return 10.0f;
  }
  return 10.0f / zoom;
}

QRectF ContentGizmo::cropCanvasRect(ArtifactIRenderer* renderer) const {
  QRectF out;
  if (!resolveTarget() || targetKind_ != TargetKind::ImageCrop || !renderer) {
    return out;
  }
  const auto imageLayer =
      ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer_);
  if (!imageLayer) {
    return out;
  }
  const auto sourceSize = imageLayer->sourceSize();
  if (sourceSize.width <= 0 || sourceSize.height <= 0) {
    return out;
  }
  const QSizeF srcSize(static_cast<qreal>(sourceSize.width),
                       static_cast<qreal>(sourceSize.height));
  const SourceCrop crop = imageLayer->sourceCrop();
  QRectF effective = crop.effectiveCropRect(srcSize);
  if (!effective.isValid() || effective.width() <= 0.0 ||
      effective.height() <= 0.0) {
    effective = QRectF(QPointF(0.0, 0.0), srcSize);
  }
  const QRectF output = layer_->localBounds();
  if (!output.isValid() || output.width() <= 0.0 || output.height() <= 0.0) {
    return out;
  }
  QRectF drawRect = output;
  if (crop.preserveAspect()) {
    const qreal fitted =
        std::min(output.width() / effective.width(),
                 output.height() / effective.height());
    if (!std::isfinite(fitted) || fitted <= 0.0) {
      return out;
    }
    const QSizeF fittedSize(effective.width() * fitted,
                            effective.height() * fitted);
    drawRect = QRectF(output.center() -
                          QPointF(fittedSize.width() * 0.5,
                                  fittedSize.height() * 0.5),
                      fittedSize);
  }
  const QPointF topLeft(
      drawRect.left() +
          (effective.left() - 0.0) / srcSize.width() * 0.0,
      drawRect.top());
  // Effective rect -> output frame linear map. Exact when crop pan/zoom/
  // rotation are at defaults (then effective == raw).
  const qreal scaleX = drawRect.width() / effective.width();
  const qreal scaleY = drawRect.height() / effective.height();
  const auto mapPoint = [&](const QPointF& src) {
    return QPointF(drawRect.left() + (src.x() - effective.left()) * scaleX,
                   drawRect.top() + (src.y() - effective.top()) * scaleY);
  };
  const QPointF canvasTopLeft = layerToCanvas(layer_, mapPoint(effective.topLeft()));
  const QPointF canvasTopRight =
      layerToCanvas(layer_, mapPoint(effective.topRight()));
  const QPointF canvasBottomRight =
      layerToCanvas(layer_, mapPoint(effective.bottomRight()));
  const QPointF canvasBottomLeft =
      layerToCanvas(layer_, mapPoint(effective.bottomLeft()));
  const qreal left = std::min({canvasTopLeft.x(), canvasTopRight.x(),
                               canvasBottomRight.x(), canvasBottomLeft.x()});
  const qreal right = std::max({canvasTopLeft.x(), canvasTopRight.x(),
                                canvasBottomRight.x(), canvasBottomLeft.x()});
  const qreal top = std::min({canvasTopLeft.y(), canvasTopRight.y(),
                              canvasBottomRight.y(), canvasBottomLeft.y()});
  const qreal bottom = std::max({canvasTopLeft.y(), canvasTopRight.y(),
                                 canvasBottomRight.y(), canvasBottomLeft.y()});
  out = QRectF(QPointF(left, top), QPointF(right, bottom));
  return out;
}

QRectF ContentGizmo::solidCanvasRect(ArtifactIRenderer* renderer) const {
  QRectF out;
  if (!resolveTarget() || targetKind_ != TargetKind::Solid || !renderer) {
    return out;
  }
  const auto sourceSize = layer_->sourceSize();
  if (sourceSize.width <= 0 || sourceSize.height <= 0) {
    return out;
  }
  const QRectF local(0.0, 0.0, static_cast<qreal>(sourceSize.width),
                     static_cast<qreal>(sourceSize.height));
  const QPointF canvasTopLeft = layerToCanvas(layer_, local.topLeft());
  const QPointF canvasTopRight = layerToCanvas(layer_, local.topRight());
  const QPointF canvasBottomRight = layerToCanvas(layer_, local.bottomRight());
  const QPointF canvasBottomLeft = layerToCanvas(layer_, local.bottomLeft());
  const qreal left = std::min({canvasTopLeft.x(), canvasTopRight.x(),
                               canvasBottomRight.x(), canvasBottomLeft.x()});
  const qreal right = std::max({canvasTopLeft.x(), canvasTopRight.x(),
                                canvasBottomRight.x(), canvasBottomLeft.x()});
  const qreal top = std::min({canvasTopLeft.y(), canvasTopRight.y(),
                              canvasBottomRight.y(), canvasBottomLeft.y()});
  const qreal bottom = std::max({canvasTopLeft.y(), canvasTopRight.y(),
                                 canvasBottomRight.y(), canvasBottomLeft.y()});
  out = QRectF(QPointF(left, top), QPointF(right, bottom));
  return out;
}

QPointF ContentGizmo::gradientCenterCanvas(ArtifactIRenderer* renderer) const {
  if (!resolveTarget() || targetKind_ != TargetKind::Solid || !renderer) {
    return QPointF();
  }
  const auto sourceSize = layer_->sourceSize();
  if (sourceSize.width <= 0 || sourceSize.height <= 0) {
    return QPointF();
  }
  double cx = 0.5;
  double cy = 0.5;
  if (const auto solid =
          ArtifactCore::dynamicPointerCast<ArtifactSolid2DLayer>(layer_)) {
    cx = solid->gradientCenterX();
    cy = solid->gradientCenterY();
  } else if (const auto image =
                 ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(
                     layer_)) {
    cx = image->gradientCenterX();
    cy = image->gradientCenterY();
  } else {
    return QPointF();
  }
  if (!std::isfinite(cx) || !std::isfinite(cy)) {
    return QPointF();
  }
  const QPointF local(static_cast<qreal>(sourceSize.width) * cx,
                      static_cast<qreal>(sourceSize.height) * cy);
  return layerToCanvas(layer_, local);
}

QPointF ContentGizmo::gradientAngleCanvas(ArtifactIRenderer* renderer) const {
  const QPointF center = gradientCenterCanvas(renderer);
  if (!resolveTarget() || targetKind_ != TargetKind::Solid || !renderer) {
    return QPointF();
  }
  const auto sourceSize = layer_->sourceSize();
  double angle = 90.0;
  double scale = 1.0;
  double offset = 0.0;
  bool reverse = false;
  if (const auto solid =
          ArtifactCore::dynamicPointerCast<ArtifactSolid2DLayer>(layer_)) {
    angle = solid->gradientAngleDegrees();
    scale = solid->gradientScale();
    offset = solid->gradientOffset();
    reverse = solid->gradientReverse();
  } else if (const auto image =
                 ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(
                     layer_)) {
    angle = image->gradientAngleDegrees();
    scale = image->gradientScale();
    offset = image->gradientOffset();
    reverse = image->gradientReverse();
  } else {
    return QPointF();
  }
  if (!std::isfinite(angle) || !std::isfinite(scale) || !std::isfinite(offset)) {
    return QPointF();
  }
  const double radians = angle * 3.14159265358979323846 / 180.0;
  const double dx = std::cos(radians);
  const double dy = -std::sin(radians);
  const double halfSpan =
      std::max(1.0, std::hypot(static_cast<double>(sourceSize.width),
                               static_cast<double>(sourceSize.height))) *
      0.5 * std::clamp(scale, 0.01, 1000000.0);
  const double direction = reverse ? -1.0 : 1.0;
  // Center knob is in canvas space; rebuild the axis in layer-local space.
  bool ok = false;
  const QTransform inverse = layer_->getGlobalTransform().inverted(&ok);
  if (!ok) {
    return QPointF();
  }
  const QPointF centerLocal = inverse.map(center);
  const QPointF endLocal(
      centerLocal.x() + dx * halfSpan * direction + dx * halfSpan * offset,
      centerLocal.y() + dy * halfSpan * direction + dy * halfSpan * offset);
  return layerToCanvas(layer_, endLocal);
}

void ContentGizmo::draw(ArtifactIRenderer* renderer) {
  if (!resolveTarget() || !renderer) {
    return;
  }
  const float zoom = renderer->getZoom();
  const float invZoom =
      (std::isfinite(zoom) && zoom > 0.0f) ? 1.0f / zoom : 1.0f;
  const float thickness = std::max(1.0f, 1.2f * invZoom);
  const float handleSize = std::clamp(10.0f * invZoom, 6.0f, 18.0f);
  if (targetKind_ == TargetKind::ImageCrop) {
    const QRectF rect = cropCanvasRect(renderer);
    if (!rect.isValid()) {
      return;
    }
    const QPointF tl = rect.topLeft();
    const QPointF tr = rect.topRight();
    const QPointF br = rect.bottomRight();
    const QPointF bl = rect.bottomLeft();
    const QPointF topMid((tl.x() + tr.x()) * 0.5, (tl.y() + tr.y()) * 0.5);
    const QPointF bottomMid((bl.x() + br.x()) * 0.5, (bl.y() + br.y()) * 0.5);
    const QPointF leftMid((tl.x() + bl.x()) * 0.5, (tl.y() + bl.y()) * 0.5);
    const QPointF rightMid((tr.x() + br.x()) * 0.5, (tr.y() + br.y()) * 0.5);
    drawPolyQuad(renderer, tl, tr, br, bl, cropColor(), thickness);
    drawSquareHandle(renderer, tl, handleSize, cropColor());
    drawSquareHandle(renderer, tr, handleSize, cropColor());
    drawSquareHandle(renderer, br, handleSize, cropColor());
    drawSquareHandle(renderer, bl, handleSize, cropColor());
    drawSquareHandle(renderer, topMid, handleSize, cropColor());
    drawSquareHandle(renderer, bottomMid, handleSize, cropColor());
    drawSquareHandle(renderer, leftMid, handleSize, cropColor());
    drawSquareHandle(renderer, rightMid, handleSize, cropColor());
    return;
  }
  if (targetKind_ == TargetKind::Solid) {
    const QRectF rect = solidCanvasRect(renderer);
    if (!rect.isValid()) {
      return;
    }
    drawPolyQuad(renderer, rect.topLeft(), rect.topRight(),
                 rect.bottomRight(), rect.bottomLeft(), sizeColor(),
                 thickness);
    drawSquareHandle(renderer, rect.topLeft(), handleSize, sizeColor());
    drawSquareHandle(renderer, rect.topRight(), handleSize, sizeColor());
    drawSquareHandle(renderer, rect.bottomRight(), handleSize, sizeColor());
    drawSquareHandle(renderer, rect.bottomLeft(), handleSize, sizeColor());
    const QPointF centerKnob = gradientCenterCanvas(renderer);
    const QPointF angleKnob = gradientAngleCanvas(renderer);
    if (!centerKnob.isNull() && !angleKnob.isNull()) {
      const auto f = [](const QPointF& p) {
        return Detail::float2{static_cast<float>(p.x()),
                              static_cast<float>(p.y())};
      };
      renderer->drawSolidLine(f(centerKnob), f(angleKnob), gradientColor(),
                              thickness);
      drawSquareHandle(renderer, centerKnob, handleSize, gradientColor());
      drawSquareHandle(renderer, angleKnob, handleSize, gradientColor());
    }
  }
}

ContentGizmo::HandleType ContentGizmo::hitTest(
    const QPointF& viewportPos, ArtifactIRenderer* renderer) const {
  if (!resolveTarget() || !renderer) {
    return HandleType::None;
  }
  const QPointF canvas = toCanvas(renderer, viewportPos);
  const float tolerance = viewportTolerance(renderer);
  const auto nearHandle = [&](const QPointF& handlePos) {
    const double dx = canvas.x() - handlePos.x();
    const double dy = canvas.y() - handlePos.y();
    return std::hypot(dx, dy) <= tolerance;
  };
  if (targetKind_ == TargetKind::ImageCrop) {
    const QRectF rect = cropCanvasRect(renderer);
    if (!rect.isValid()) {
      return HandleType::None;
    }
    const QPointF tl = rect.topLeft();
    const QPointF tr = rect.topRight();
    const QPointF br = rect.bottomRight();
    const QPointF bl = rect.bottomLeft();
    if (nearHandle(tl)) return HandleType::CropTopLeft;
    if (nearHandle(tr)) return HandleType::CropTopRight;
    if (nearHandle(br)) return HandleType::CropBottomRight;
    if (nearHandle(bl)) return HandleType::CropBottomLeft;
    const QPointF topMid((tl.x() + tr.x()) * 0.5, (tl.y() + tr.y()) * 0.5);
    const QPointF bottomMid((bl.x() + br.x()) * 0.5, (bl.y() + br.y()) * 0.5);
    const QPointF leftMid((tl.x() + bl.x()) * 0.5, (tl.y() + bl.y()) * 0.5);
    const QPointF rightMid((tr.x() + br.x()) * 0.5, (tr.y() + br.y()) * 0.5);
    if (nearHandle(topMid)) return HandleType::CropTop;
    if (nearHandle(bottomMid)) return HandleType::CropBottom;
    if (nearHandle(leftMid)) return HandleType::CropLeft;
    if (nearHandle(rightMid)) return HandleType::CropRight;
    return HandleType::None;
  }
  if (targetKind_ == TargetKind::Solid) {
    const QPointF centerKnob = gradientCenterCanvas(renderer);
    const QPointF angleKnob = gradientAngleCanvas(renderer);
    if (!centerKnob.isNull() && nearHandle(centerKnob)) {
      return HandleType::GradientCenter;
    }
    if (!angleKnob.isNull() && nearHandle(angleKnob)) {
      return HandleType::GradientAngle;
    }
    const QRectF rect = solidCanvasRect(renderer);
    if (!rect.isValid()) {
      return HandleType::None;
    }
    if (nearHandle(rect.topLeft())) return HandleType::SizeTopLeft;
    if (nearHandle(rect.topRight())) return HandleType::SizeTopRight;
    if (nearHandle(rect.bottomRight())) return HandleType::SizeBottomRight;
    if (nearHandle(rect.bottomLeft())) return HandleType::SizeBottomLeft;
    return HandleType::None;
  }
  return HandleType::None;
}

Qt::CursorShape ContentGizmo::cursorShapeForViewportPos(
    const QPointF& viewportPos, ArtifactIRenderer* renderer) const {
  switch (hitTest(viewportPos, renderer)) {
    case HandleType::CropLeft:
    case HandleType::CropRight:
      return Qt::SizeHorCursor;
    case HandleType::CropTop:
    case HandleType::CropBottom:
      return Qt::SizeVerCursor;
    case HandleType::CropTopLeft:
    case HandleType::CropBottomRight:
    case HandleType::SizeTopLeft:
    case HandleType::SizeBottomRight:
      return Qt::SizeFDiagCursor;
    case HandleType::CropTopRight:
    case HandleType::CropBottomLeft:
    case HandleType::SizeTopRight:
    case HandleType::SizeBottomLeft:
      return Qt::SizeBDiagCursor;
    case HandleType::GradientCenter:
    case HandleType::GradientAngle:
      return Qt::PointingHandCursor;
    case HandleType::None:
      break;
  }
  return Qt::ArrowCursor;
}

bool ContentGizmo::handleMousePress(const QPointF& viewportPos,
                                    ArtifactIRenderer* renderer) {
  if (isDragging_ || !resolveTarget() || !renderer) {
    return false;
  }
  const HandleType hit = hitTest(viewportPos, renderer);
  if (hit == HandleType::None) {
    return false;
  }
  activeHandle_ = hit;
  isDragging_ = true;
  dragStartCanvasPos_ = toCanvas(renderer, viewportPos);
  dragLastCanvasPos_ = dragStartCanvasPos_;
  dragAspectLock_ = false;
  dragCenterAnchor_ = false;
  if (targetKind_ == TargetKind::ImageCrop) {
    const auto imageLayer =
        ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer_);
    if (!imageLayer) {
      isDragging_ = false;
      activeHandle_ = HandleType::None;
      return false;
    }
    dragCropWasEnabled_ = imageLayer->sourceCropEnabled();
    if (!dragCropWasEnabled_) {
      imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.enabled"),
                                         true);
    }
    const SourceCrop enabled = imageLayer->sourceCrop();
    dragCropStart_ = enabled.cropRect();
    const auto sourceSize = imageLayer->sourceSize();
    dragCropSourceSize_ = QSizeF(static_cast<qreal>(sourceSize.width),
                                 static_cast<qreal>(sourceSize.height));
    if (!dragCropStart_.isValid() || dragCropStart_.width() <= 0.0 ||
        dragCropStart_.height() <= 0.0) {
      dragCropStart_ = QRectF(QPointF(0.0, 0.0), dragCropSourceSize_);
    }
    return true;
  }
  if (targetKind_ == TargetKind::Solid) {
    const auto sourceSize = layer_->sourceSize();
    dragSizeBefore_ = QSize(sourceSize.width, sourceSize.height);
    dragSizeStart_ = dragSizeBefore_;
    dragGradientChanged_ = false;
    if (const auto solid =
            ArtifactCore::dynamicPointerCast<ArtifactSolid2DLayer>(layer_)) {
      dragGradientCenterX_ = solid->gradientCenterX();
      dragGradientCenterY_ = solid->gradientCenterY();
      dragGradientAngle_ = solid->gradientAngleDegrees();
    } else if (const auto image =
                   ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(
                       layer_)) {
      dragGradientCenterX_ = image->gradientCenterX();
      dragGradientCenterY_ = image->gradientCenterY();
      dragGradientAngle_ = image->gradientAngleDegrees();
    }
    dragGradientStartCenterX_ = dragGradientCenterX_;
    dragGradientStartCenterY_ = dragGradientCenterY_;
    dragGradientStartAngle_ = dragGradientAngle_;
    return true;
  }
  isDragging_ = false;
  activeHandle_ = HandleType::None;
  return false;
}

bool ContentGizmo::handleMouseMove(const QPointF& viewportPos,
                                   ArtifactIRenderer* renderer) {
  if (!isDragging_ || !resolveTarget() || !renderer) {
    return false;
  }
  const QPointF canvas = toCanvas(renderer, viewportPos);
  const Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
  dragAspectLock_ = modifiers.testFlag(Qt::ShiftModifier);
  dragCenterAnchor_ = modifiers.testFlag(Qt::AltModifier);
  if (targetKind_ == TargetKind::ImageCrop) {
    const auto imageLayer =
        ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer_);
    if (!imageLayer) {
      return false;
    }
    if (dragCropSourceSize_.width() <= 0.0 ||
        dragCropSourceSize_.height() <= 0.0) {
      return false;
    }
    // Canvas delta -> layer-local delta -> source-pixel delta. The output
    // frame maps linearly onto the raw crop rect (exact for default
    // pan/zoom/rotation; approximate otherwise).
    const QPointF localDelta =
        canvasDeltaToLocal(layer_, dragLastCanvasPos_, canvas);
    const QRectF output = layer_->localBounds();
    if (!output.isValid() || output.width() <= 0.0 ||
        output.height() <= 0.0) {
      return false;
    }
    QRectF current = imageLayer->sourceCrop().cropRect();
    if (!current.isValid() || current.width() <= 0.0 ||
        current.height() <= 0.0) {
      current = QRectF(QPointF(0.0, 0.0), dragCropSourceSize_);
    }
    const double scaleX = current.width() / output.width();
    const double scaleY = current.height() / output.height();
    if (!std::isfinite(scaleX) || !std::isfinite(scaleY)) {
      return false;
    }
    const QPointF sourceDelta(localDelta.x() * scaleX,
                              localDelta.y() * scaleY);
    QRectF next = current;
    const bool lockAspect =
        dragAspectLock_ || imageLayer->sourceCrop().preserveAspect();
    const double startRatio = (dragCropStart_.height() > 0.0)
                                  ? dragCropStart_.width() /
                                        dragCropStart_.height()
                                  : 1.0;
    const auto applyLeft = [&](double dx) {
      next.setLeft(next.left() + dx);
      if (dragCenterAnchor_) {
        next.setRight(next.right() - dx);
      }
    };
    const auto applyRight = [&](double dx) {
      next.setRight(next.right() + dx);
      if (dragCenterAnchor_) {
        next.setLeft(next.left() - dx);
      }
    };
    const auto applyTop = [&](double dy) {
      next.setTop(next.top() + dy);
      if (dragCenterAnchor_) {
        next.setBottom(next.bottom() - dy);
      }
    };
    const auto applyBottom = [&](double dy) {
      next.setBottom(next.bottom() + dy);
      if (dragCenterAnchor_) {
        next.setTop(next.top() - dy);
      }
    };
    switch (activeHandle_) {
      case HandleType::CropLeft: applyLeft(sourceDelta.x()); break;
      case HandleType::CropRight: applyRight(sourceDelta.x()); break;
      case HandleType::CropTop: applyTop(sourceDelta.y()); break;
      case HandleType::CropBottom: applyBottom(sourceDelta.y()); break;
      case HandleType::CropTopLeft:
        applyLeft(sourceDelta.x());
        applyTop(sourceDelta.y());
        break;
      case HandleType::CropTopRight:
        applyRight(sourceDelta.x());
        applyTop(sourceDelta.y());
        break;
      case HandleType::CropBottomLeft:
        applyLeft(sourceDelta.x());
        applyBottom(sourceDelta.y());
        break;
      case HandleType::CropBottomRight:
        applyRight(sourceDelta.x());
        applyBottom(sourceDelta.y());
        break;
      default:
        return false;
    }
    next = next.normalized();
    if (next.width() < 1.0 || next.height() < 1.0) {
      return false;
    }
    if (lockAspect && std::isfinite(startRatio) && startRatio > 0.0) {
      // Keep the drag-start ratio anchored at the fixed opposite edge.
      const double fixedRight = (activeHandle_ == HandleType::CropLeft ||
                                 activeHandle_ == HandleType::CropTopLeft ||
                                 activeHandle_ == HandleType::CropBottomLeft)
                                    ? current.right()
                                    : next.right();
      const double fixedLeft = (activeHandle_ == HandleType::CropRight ||
                                activeHandle_ == HandleType::CropTopRight ||
                                activeHandle_ == HandleType::CropBottomRight)
                                   ? current.left()
                                   : next.left();
      const double fixedBottom = (activeHandle_ == HandleType::CropTop ||
                                  activeHandle_ == HandleType::CropTopLeft ||
                                  activeHandle_ == HandleType::CropTopRight)
                                     ? current.bottom()
                                     : next.bottom();
      const double fixedTop = (activeHandle_ == HandleType::CropBottom ||
                               activeHandle_ == HandleType::CropBottomLeft ||
                               activeHandle_ == HandleType::CropBottomRight)
                                    ? current.top()
                                    : next.top();
      if (activeHandle_ == HandleType::CropLeft ||
          activeHandle_ == HandleType::CropRight) {
        next.setTop(fixedBottom - next.width() / startRatio);
      } else if (activeHandle_ == HandleType::CropTop ||
                 activeHandle_ == HandleType::CropBottom) {
        next.setLeft(fixedRight - next.height() * startRatio);
        Q_UNUSED(fixedLeft);
        Q_UNUSED(fixedTop);
      } else {
        // Corners: fit inside the dragged box preserving ratio.
        if (next.width() / next.height() > startRatio) {
          const double w = next.height() * startRatio;
          if (activeHandle_ == HandleType::CropTopLeft ||
              activeHandle_ == HandleType::CropBottomLeft) {
            next.setLeft(next.right() - w);
          } else {
            next.setRight(next.left() + w);
          }
        } else {
          const double h = next.width() / startRatio;
          if (activeHandle_ == HandleType::CropTopLeft ||
              activeHandle_ == HandleType::CropTopRight) {
            next.setTop(next.bottom() - h);
          } else {
            next.setBottom(next.top() + h);
          }
        }
      }
      next = next.normalized();
      if (next.width() < 1.0 || next.height() < 1.0) {
        return false;
      }
    }
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropX"),
                                       next.x());
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropY"),
                                       next.y());
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropWidth"),
                                       next.width());
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropHeight"),
                                       next.height());
    dragLastCanvasPos_ = canvas;
    return true;
  }
  if (targetKind_ == TargetKind::Solid) {
    bool invertible = false;
    const QTransform inverse = layer_->getGlobalTransform().inverted(&invertible);
    if (!invertible) {
      return false;
    }
    const QPointF local = inverse.map(canvas);
    if (activeHandle_ == HandleType::GradientCenter ||
        activeHandle_ == HandleType::GradientAngle) {
      const auto sourceSize = layer_->sourceSize();
      if (sourceSize.width <= 0 || sourceSize.height <= 0) {
        return false;
      }
      if (activeHandle_ == HandleType::GradientCenter) {
        dragGradientCenterX_ = std::clamp(
            local.x() / static_cast<double>(sourceSize.width), 0.0, 1.0);
        dragGradientCenterY_ = std::clamp(
            local.y() / static_cast<double>(sourceSize.height), 0.0, 1.0);
      } else {
        const double dx =
            local.x() - dragGradientCenterX_ * sourceSize.width;
        const double dy =
            local.y() - dragGradientCenterY_ * sourceSize.height;
        if (std::hypot(dx, dy) < 1.0) {
          return false;
        }
        dragGradientAngle_ = normalizeAngle360(
            std::atan2(-dy, dx) * 180.0 / 3.14159265358979323846);
      }
      dragGradientChanged_ = true;
      const auto writeGradient = [&](const QString& path, double value) {
        if (const auto solid =
                ArtifactCore::dynamicPointerCast<ArtifactSolid2DLayer>(layer_)) {
          solid->setLayerPropertyValue(path, value);
        } else if (const auto image =
                       ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(
                           layer_)) {
          image->setLayerPropertyValue(path, value);
        }
      };
      writeGradient(QStringLiteral("solid.gradientCenterX"),
                    dragGradientCenterX_);
      writeGradient(QStringLiteral("solid.gradientCenterY"),
                    dragGradientCenterY_);
      writeGradient(QStringLiteral("solid.gradientAngleDegrees"),
                    dragGradientAngle_);
      dragLastCanvasPos_ = canvas;
      return true;
    }
    const bool isSizeHandle =
        activeHandle_ == HandleType::SizeTopLeft ||
        activeHandle_ == HandleType::SizeTopRight ||
        activeHandle_ == HandleType::SizeBottomLeft ||
        activeHandle_ == HandleType::SizeBottomRight;
    if (!isSizeHandle) {
      return false;
    }
    int newWidth = static_cast<int>(std::round(local.x()));
    int newHeight = static_cast<int>(std::round(local.y()));
    if (dragAspectLock_ && dragSizeStart_.width() > 0 &&
        dragSizeStart_.height() > 0) {
      const double kx =
          static_cast<double>(newWidth) / dragSizeStart_.width();
      const double ky =
          static_cast<double>(newHeight) / dragSizeStart_.height();
      const double k = std::max(kx, ky);
      if (!std::isfinite(k) || k <= 0.0) {
        return false;
      }
      newWidth = static_cast<int>(
          std::round(dragSizeStart_.width() * k));
      newHeight = static_cast<int>(
          std::round(dragSizeStart_.height() * k));
    }
    newWidth = std::clamp(newWidth, 1, 16384);
    newHeight = std::clamp(newHeight, 1, 16384);
    const auto sourceSize = layer_->sourceSize();
    if (newWidth == sourceSize.width && newHeight == sourceSize.height) {
      dragLastCanvasPos_ = canvas;
      return true;
    }
    if (const auto solid =
            ArtifactCore::dynamicPointerCast<ArtifactSolid2DLayer>(layer_)) {
      solid->setSize(newWidth, newHeight);
    } else if (const auto image =
                   ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(
                       layer_)) {
      image->setSize(newWidth, newHeight);
    } else {
      return false;
    }
    layer_->setDirty(LayerDirtyFlag::Source);
    layer_->changed();
    dragLastCanvasPos_ = canvas;
    return true;
  }
  return false;
}

void ContentGizmo::handleMouseRelease() {
  if (!isDragging_) {
    return;
  }
  if (targetKind_ == TargetKind::ImageCrop) {
    if (const auto imageLayer =
            ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer_)) {
      const SourceCrop after = imageLayer->sourceCrop();
      SourceCrop before = after;
      before.setEnabled(dragCropWasEnabled_);
      before.setCropRect(dragCropStart_.isValid() ? dragCropStart_
                                                  : after.cropRect());
      const bool changed = before.enabled() != after.enabled() ||
                           before.cropRect() != after.cropRect();
      if (changed) {
        auto* manager = UndoManager::instance();
        if (manager && !manager->push(std::make_unique<SourceCropRectUndoCommand>(
                                  imageLayer, before, after))) {
          // Roll back to the press-time state without an undo entry.
          imageLayer->setLayerPropertyValue(
              QStringLiteral("sourceCrop.enabled"), before.enabled());
          const QRectF rect = before.cropRect();
          imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropX"),
                                             rect.x());
          imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropY"),
                                             rect.y());
          imageLayer->setLayerPropertyValue(
              QStringLiteral("sourceCrop.cropWidth"), rect.width());
          imageLayer->setLayerPropertyValue(
              QStringLiteral("sourceCrop.cropHeight"), rect.height());
        }
      }
    }
  } else if (targetKind_ == TargetKind::Solid) {
    const auto sourceSize = layer_->sourceSize();
    const QSize afterSize(sourceSize.width, sourceSize.height);
    if (afterSize != dragSizeBefore_) {
      auto* manager = UndoManager::instance();
      if (manager && !manager->push(std::make_unique<SolidSizeUndoCommand>(
                                layer_, dragSizeBefore_, afterSize))) {
        if (const auto solid =
                ArtifactCore::dynamicPointerCast<ArtifactSolid2DLayer>(layer_)) {
          solid->setSize(dragSizeBefore_.width(), dragSizeBefore_.height());
        } else if (const auto image =
                       ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(
                           layer_)) {
          image->setSize(dragSizeBefore_.width(), dragSizeBefore_.height());
        }
        layer_->setDirty(LayerDirtyFlag::Source);
        layer_->changed();
      }
    }
    if (dragGradientChanged_) {
      const bool changed =
          std::abs(dragGradientCenterX_ - dragGradientStartCenterX_) > 1e-9 ||
          std::abs(dragGradientCenterY_ - dragGradientStartCenterY_) > 1e-9 ||
          std::abs(normalizeAngle360(dragGradientAngle_ -
                                     dragGradientStartAngle_)) > 1e-9;
      if (changed) {
        SolidGradientUndoCommand::AxisState beforeState;
        beforeState.centerX = dragGradientStartCenterX_;
        beforeState.centerY = dragGradientStartCenterY_;
        beforeState.angleDegrees = dragGradientStartAngle_;
        SolidGradientUndoCommand::AxisState afterState;
        afterState.centerX = dragGradientCenterX_;
        afterState.centerY = dragGradientCenterY_;
        afterState.angleDegrees = dragGradientAngle_;
        auto* manager = UndoManager::instance();
        if (manager && !manager->push(std::make_unique<SolidGradientUndoCommand>(
                                  layer_, beforeState, afterState))) {
          const auto restore = [&](const QString& path, double value) {
            if (const auto solid =
                    ArtifactCore::dynamicPointerCast<ArtifactSolid2DLayer>(
                        layer_)) {
              solid->setLayerPropertyValue(path, value);
            } else if (const auto image =
                           ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(
                               layer_)) {
              image->setLayerPropertyValue(path, value);
            }
          };
          restore(QStringLiteral("solid.gradientCenterX"),
                  beforeState.centerX);
          restore(QStringLiteral("solid.gradientCenterY"),
                  beforeState.centerY);
          restore(QStringLiteral("solid.gradientAngleDegrees"),
                  beforeState.angleDegrees);
        }
      }
    }
  }
  isDragging_ = false;
  activeHandle_ = HandleType::None;
  dragGradientChanged_ = false;
}

bool ContentGizmo::cancelInteraction() {
  if (!isDragging_) {
    return false;
  }
  if (targetKind_ == TargetKind::ImageCrop) {
    if (const auto imageLayer =
            ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer_)) {
      imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.enabled"),
                                         dragCropWasEnabled_);
      imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropX"),
                                         dragCropStart_.x());
      imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropY"),
                                         dragCropStart_.y());
      imageLayer->setLayerPropertyValue(
          QStringLiteral("sourceCrop.cropWidth"), dragCropStart_.width());
      imageLayer->setLayerPropertyValue(
          QStringLiteral("sourceCrop.cropHeight"), dragCropStart_.height());
    }
  } else if (targetKind_ == TargetKind::Solid) {
    if (const auto solid =
            ArtifactCore::dynamicPointerCast<ArtifactSolid2DLayer>(layer_)) {
      solid->setSize(dragSizeBefore_.width(), dragSizeBefore_.height());
    } else if (const auto image =
                   ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(
                       layer_)) {
      image->setSize(dragSizeBefore_.width(), dragSizeBefore_.height());
    }
    layer_->setDirty(LayerDirtyFlag::Source);
    layer_->changed();
  }
  isDragging_ = false;
  activeHandle_ = HandleType::None;
  dragGradientChanged_ = false;
  return true;
}

}  // namespace Artifact
