module;
#include <QPointF>
#include <memory>

export module Artifact.Widgets.TransformGizmo;

import Artifact.Render.IRenderer;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Utils.Id;
import Color.Float;

export namespace Artifact {

 class TransformGizmo {
 public:
  enum class HandleType {
   None,
   Move,
   Scale_TL, Scale_TR, Scale_BL, Scale_BR,
   Scale_T, Scale_B, Scale_L, Scale_R,
   Rotate
  };

  TransformGizmo();
  ~TransformGizmo();

  void setLayer(ArtifactAbstractLayerPtr layer);
  void draw(ArtifactIRenderer* renderer);

  bool handleMousePress(const QPointF& viewportPos, ArtifactIRenderer* renderer);
  bool handleMouseMove(const QPointF& viewportPos, ArtifactIRenderer* renderer);
  void handleMouseRelease();

  bool isDragging() const { return isDragging_; }

 private:
  HandleType hitTest(const QPointF& viewportPos, ArtifactIRenderer* renderer);
  
  ArtifactAbstractLayerPtr layer_;
  HandleType activeHandle_ = HandleType::None;
  bool isDragging_ = false;
  QPointF lastCanvasMousePos_;
  
  static constexpr float HANDLE_SIZE = 8.0f;
  static constexpr float ROTATE_HANDLE_DISTANCE = 30.0f;
 };

}
