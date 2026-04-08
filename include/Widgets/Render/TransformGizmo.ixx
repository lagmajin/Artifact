module;
#include <utility>
#include <memory>
#include <vector>
#include <QPointF>
#include <QRectF>
#include <QCursor>
export module Artifact.Widgets.TransformGizmo;


import Artifact.Render.IRenderer;
import Artifact.Composition.Abstract;
export import Artifact.Layer.Abstract;
import Utils.Id;
import Color.Float;

export namespace Artifact {

 struct SnapLine {
  bool isVertical = false;
  float position = 0.0f;
 };

 export class TransformGizmo {
 public:
  enum class Mode {
   All,
   Move,
   Rotate,
   Scale
  };

  enum class HandleType {
   None,
   Move,
   Scale_TL, Scale_TR, Scale_BL, Scale_BR,
   Scale_T, Scale_B, Scale_L, Scale_R,
   Scale_Center,
   Rotate,
   Anchor
  };

  TransformGizmo();
  ~TransformGizmo();

  void setMode(Mode mode);
  Mode mode() const;

  void setLayer(ArtifactAbstractLayerPtr layer);
  void draw(ArtifactIRenderer* renderer);
  Qt::CursorShape cursorShapeForViewportPos(const QPointF& viewportPos, ArtifactIRenderer* renderer) const;
  HandleType handleAtViewportPos(const QPointF& viewportPos, ArtifactIRenderer* renderer) const;

  bool handleMousePress(const QPointF& viewportPos, ArtifactIRenderer* renderer);
  bool handleMouseMove(const QPointF& viewportPos, ArtifactIRenderer* renderer);
  void handleMouseRelease();

  bool isDragging() const { return isDragging_; }
  HandleType activeHandle() const { return activeHandle_; }

 private:
  HandleType hitTest(const QPointF& viewportPos, ArtifactIRenderer* renderer) const;
  bool allowsHandle(HandleType handle) const;
  
  ArtifactAbstractLayerPtr layer_;
  Mode mode_ = Mode::All;
  HandleType activeHandle_ = HandleType::None;
  bool isDragging_ = false;
  std::vector<SnapLine> activeSnapLines_;
  QPointF dragStartCanvasPos_;
  QPointF dragStartLayerPos_;
  float dragStartScaleX_ = 1.0f;
  float dragStartScaleY_ = 1.0f;
  float dragStartRotation_ = 0.0f;
  int64_t dragStartFrame_ = 0;
  bool dragStartHasPositionKey_ = false;
  bool dragStartHasRotationKey_ = false;
  bool dragStartHasScaleKey_ = false;
  QTransform dragStartGlobalTransform_;
  QRectF dragStartBoundingBox_;
  QRectF dragStartLocalBounds_;
  QPointF dragStartAnchor_;
  float dragStartAnchorZ_ = 0.0f;
  QPointF lastCanvasMousePos_;
  
  static constexpr float HANDLE_SIZE = 10.0f;
  static constexpr float ROTATE_HANDLE_DISTANCE = 32.0f;
  static constexpr float ROTATE_HANDLE_RADIUS = 6.0f;
  static constexpr float ANCHOR_HANDLE_SIZE = 8.0f;
 };

}
