module;
#include <utility>
#include <memory>
#include <vector>
#include <chrono>
#include <QPointF>
#include <QRectF>
#include <QCursor>
#include <QTransform>
#include <QString>
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

 struct SnapLabel {
  bool isVertical = false;
  QPointF position;
  QString text;
 };

 export class TransformGizmo {
 public:
  enum class Mode {
   All,
   Move,
   Rotate,
   Scale,
   // Backwards-compatible aliases used across the codebase
   Translation = Move,
   Rotation = Rotate,
   AnchorPoint,
   None
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
  const std::vector<SnapLine>& activeSnapLines() const { return activeSnapLines_; }
  const std::vector<SnapLabel>& activeSnapLabels() const { return activeSnapLabels_; }
  QRectF currentCanvasBoundingRect() const;

 private:
  HandleType hitTest(const QPointF& viewportPos, ArtifactIRenderer* renderer) const;
  bool allowsHandle(HandleType handle) const;
  
  ArtifactAbstractLayerPtr layer_;
  Mode mode_ = Mode::All;
  HandleType activeHandle_ = HandleType::None;
  bool isDragging_ = false;
  std::vector<SnapLine> activeSnapLines_;
  std::vector<SnapLabel> activeSnapLabels_;
  // ドラッグ開始時に一度だけ計算するスナップラインキャッシュ
  std::vector<float> cachedSnapVLines_;
  std::vector<float> cachedSnapHLines_;
  std::vector<float> cachedSpacingVLines_;
  std::vector<float> cachedSpacingHLines_;
  QPointF dragStartCanvasPos_;
  QPointF dragStartLocalMousePos_;
  QPointF dragStartLayerPos_;
  float dragStartScaleX_ = 1.0f;
  float dragStartScaleY_ = 1.0f;
  float dragStartRotation_ = 0.0f;
  int64_t dragStartFrame_ = 0;
  bool dragStartHasPositionKey_ = false;
  bool dragStartHasRotationKey_ = false;
  bool dragStartHasScaleKey_ = false;
  bool dragStartHasTextBoxState_ = false;
  QTransform dragStartGlobalTransform_;
  QRectF dragStartBoundingBox_;
  QRectF dragStartLocalBounds_;
  QPointF dragStartAnchor_;
  float dragStartAnchorZ_ = 0.0f;
  float dragStartTextBoxWidth_ = 0.0f;
  float dragStartTextBoxHeight_ = 0.0f;
  float dragStartPointerAngle_ = 0.0f;
  float dragAccumulatedRotationDelta_ = 0.0f;
  QPointF lastCanvasMousePos_;
  std::chrono::steady_clock::time_point lastDragMutationNotify_{};
  bool resizeBadgeVisible_ = false;
  std::vector<QString> resizeBadgeLines_;
  QPointF resizeBadgeAnchor_;
  QRectF resizeBadgeBox_;
  
  static constexpr float HANDLE_SIZE = 10.0f;
  // Reduced rotation gizmo size ~30% to improve usability
  static constexpr float ROTATE_HANDLE_DISTANCE = 22.0f; // was 32.0f
  static constexpr float ROTATE_HANDLE_RADIUS = 4.0f;   // was 6.0f
  static constexpr float ANCHOR_HANDLE_SIZE = 8.0f;

  // 1. ジオメトリキャッシュ: 毎フレームの計算を避けるためのキャッシュ
  struct GeometryCache {
   Detail::float2 tl, tr, bl, br;
   QPointF center, top, bottom, left, right;
  };

  void updateGeometryCache(const QTransform& globalTransform, const QRectF& localRect, float zoom);

  GeometryCache cachedPoints_;
  float cachedZoom_ = -1.0f;
  QTransform cachedLayerTransform_;
  QRectF cachedLocalRect_;
  bool geometryCacheValid_ = false;
  bool isSelected_ = true;  // 外部から設定される選択状態
 };

}
