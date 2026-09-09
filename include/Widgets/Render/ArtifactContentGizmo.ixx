module;

#include <QCursor>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QTransform>

export module Artifact.Widgets.ContentGizmo;

import Artifact.Render.IRenderer;
import Artifact.Layer.Abstract;

namespace Artifact {

// Viewport content handles for image layers (source crop rect) and solid
// layers (size corners + gradient axis knobs). AE requires dialogs or
// effects for the same edits; this gizmo edits them in place.
// Mapping is exact for unrotated layers with default crop pan/zoom/rotation;
// otherwise the overlay follows the rendered frame and numeric editing
// remains the fallback.
export class ContentGizmo {
 public:
  enum class HandleType {
    None,
    CropLeft,
    CropRight,
    CropTop,
    CropBottom,
    CropTopLeft,
    CropTopRight,
    CropBottomLeft,
    CropBottomRight,
    SizeTopLeft,
    SizeTopRight,
    SizeBottomLeft,
    SizeBottomRight,
    GradientCenter,
    GradientAngle
  };

  enum class TargetKind { None, ImageCrop, Solid };

  ContentGizmo();
  ~ContentGizmo();

  void setLayer(ArtifactAbstractLayerPtr layer);
  void draw(ArtifactIRenderer* renderer);

  HandleType hitTest(const QPointF& viewportPos,
                     ArtifactIRenderer* renderer) const;
  Qt::CursorShape cursorShapeForViewportPos(
      const QPointF& viewportPos, ArtifactIRenderer* renderer) const;

  bool handleMousePress(const QPointF& viewportPos,
                        ArtifactIRenderer* renderer);
  bool handleMouseMove(const QPointF& viewportPos,
                       ArtifactIRenderer* renderer);
  void handleMouseRelease();
  bool cancelInteraction();

  bool isDragging() const { return isDragging_; }
  HandleType activeHandle() const { return activeHandle_; }
  TargetKind targetKind() const { return targetKind_; }

 private:
  bool resolveTarget() const;
  QRectF cropCanvasRect(ArtifactIRenderer* renderer) const;
  QRectF solidCanvasRect(ArtifactIRenderer* renderer) const;
  QPointF gradientCenterCanvas(ArtifactIRenderer* renderer) const;
  QPointF gradientAngleCanvas(ArtifactIRenderer* renderer) const;
  float viewportTolerance(ArtifactIRenderer* renderer) const;

  ArtifactAbstractLayerPtr layer_;
  bool isDragging_ = false;
  HandleType activeHandle_ = HandleType::None;
  TargetKind targetKind_ = TargetKind::None;

  QPointF dragStartCanvasPos_;
  QPointF dragLastCanvasPos_;
  // Crop drag state (source pixels).
  bool dragCropWasEnabled_ = false;
  QRectF dragCropStart_;
  QSizeF dragCropSourceSize_;
  // Solid drag state.
  QSize dragSizeBefore_;
  QSize dragSizeStart_;
  double dragGradientCenterX_ = 0.5;
  double dragGradientCenterY_ = 0.5;
  double dragGradientAngle_ = 90.0;
  double dragGradientStartCenterX_ = 0.5;
  double dragGradientStartCenterY_ = 0.5;
  double dragGradientStartAngle_ = 90.0;
  bool dragGradientChanged_ = false;
  bool dragAspectLock_ = false;
  bool dragCenterAnchor_ = false;
};

}  // namespace Artifact
