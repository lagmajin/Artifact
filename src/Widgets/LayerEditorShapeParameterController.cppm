module;

#include <QLineF>
#include <QPointF>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <utility>

module Artifact.Widgets.LayerEditor.ShapeParameterController;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Widgets.LayerEditor.Geometry;
import Artifact.Widgets.LayerEditor.ShapeCommands;
import Undo.UndoManager;

namespace Artifact {
namespace {

ArtifactCore::SharedPtr<ArtifactShapeLayer> shapeLayer(
    const ArtifactAbstractLayerPtr& layer)
{
 return ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
     ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
}

}

LayerEditorShapeParameterHandle LayerEditorShapeParameterController::handleAt(
    const ArtifactAbstractLayerPtr& layer,
    const QPointF& canvasPosition, float zoom) const
{
 auto shape = shapeLayer(layer);
 if (!shape) return LayerEditorShapeParameterHandle::None;
 QPointF localHandle;
 LayerEditorShapeParameterHandle handle = LayerEditorShapeParameterHandle::None;
 if (shape->shapeType() == ShapeType::Rect || shape->shapeType() == ShapeType::Square) {
  localHandle = shapeCornerRadiusHandlePosition(*shape);
  handle = LayerEditorShapeParameterHandle::CornerRadius;
 } else if (shape->shapeType() == ShapeType::Star) {
  localHandle = shapeStarInnerRadiusHandlePosition(*shape);
  handle = LayerEditorShapeParameterHandle::StarInnerRadius;
 } else {
  return LayerEditorShapeParameterHandle::None;
 }
 const QPointF canvasHandle = layer->getGlobalTransform().map(localHandle);
 const qreal viewportDistance = QLineF(canvasHandle, canvasPosition).length() *
                                std::max(0.1f, zoom);
 return viewportDistance <= 9.0 ? handle : LayerEditorShapeParameterHandle::None;
}

bool LayerEditorShapeParameterController::begin(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPosition,
    const QPointF& viewportPosition, float zoom)
{
 const auto handle = handleAt(layer, canvasPosition, zoom);
 auto shape = shapeLayer(layer);
 if (!shape || handle == LayerEditorShapeParameterHandle::None) return false;
 activeHandle_ = handle;
 hoveredHandle_ = handle;
 layer_ = layer;
 anchorViewportX_ = static_cast<float>(viewportPosition.x());
 if (handle == LayerEditorShapeParameterHandle::CornerRadius) {
  before_ = start_ = shape->cornerRadius();
  maxCornerRadius_ = std::min(shape->shapeWidth(), shape->shapeHeight()) * 0.5f;
 } else {
  before_ = start_ = shape->starInnerRadius();
  maxCornerRadius_ = 0.0f;
 }
 return true;
}

bool LayerEditorShapeParameterController::update(
    const QPointF& canvasPosition, const QPointF& viewportPosition)
{
 auto layer = layer_.lock();
 auto shape = shapeLayer(layer);
 if (!shape || !active()) return false;
 if (activeHandle_ == LayerEditorShapeParameterHandle::CornerRadius) {
  const float delta = static_cast<float>(viewportPosition.x()) - anchorViewportX_;
  shape->setCornerRadius(std::clamp(start_ + delta * 0.5f,
                                    0.0f, maxCornerRadius_));
  return true;
 }
 bool invertible = false;
 const QTransform inverse = layer->getGlobalTransform().inverted(&invertible);
 if (!invertible) return false;
 const QPointF local = inverse.map(canvasPosition);
 const float outerRadius =
     std::min(shape->shapeWidth(), shape->shapeHeight()) * 0.5f;
 const QPointF center(shape->shapeWidth() * 0.5, shape->shapeHeight() * 0.5);
 const float distance = static_cast<float>(QLineF(center, local).length());
 shape->setStarInnerRadius(outerRadius > 0.001f
      ? std::clamp(distance / outerRadius, 0.05f, 0.99f) : start_);
 return true;
}

bool LayerEditorShapeParameterController::commit()
{
 if (!active()) return false;
 auto layer = layer_.lock();
 auto shape = shapeLayer(layer);
 const auto handle = activeHandle_;
 activeHandle_ = LayerEditorShapeParameterHandle::None;
 layer_.reset();
 if (!shape) return true;
 const float after = handle == LayerEditorShapeParameterHandle::CornerRadius
     ? shape->cornerRadius() : shape->starInnerRadius();
 if (std::abs(after - before_) <= 0.001f) return true;
 auto command = handle == LayerEditorShapeParameterHandle::CornerRadius
     ? makeCornerRadiusEditCommand(layer, before_, after)
     : makeStarInnerRadiusEditCommand(layer, before_, after);
 if (auto* undo = UndoManager::instance(); undo && !undo->push(std::move(command))) {
  if (handle == LayerEditorShapeParameterHandle::CornerRadius)
   shape->setCornerRadius(before_);
  else
   shape->setStarInnerRadius(before_);
  shape->changed();
 }
 return true;
}

void LayerEditorShapeParameterController::cancel()
{
 auto shape = shapeLayer(layer_.lock());
 if (shape && active()) {
  if (activeHandle_ == LayerEditorShapeParameterHandle::CornerRadius)
   shape->setCornerRadius(before_);
  else
   shape->setStarInnerRadius(before_);
  shape->changed();
 }
 activeHandle_ = LayerEditorShapeParameterHandle::None;
 layer_.reset();
}

void LayerEditorShapeParameterController::clearHover() noexcept
{
 hoveredHandle_ = LayerEditorShapeParameterHandle::None;
}

bool LayerEditorShapeParameterController::updateHover(
    const ArtifactAbstractLayerPtr& layer,
    const QPointF& canvasPosition, float zoom)
{
 const auto before = hoveredHandle_;
 hoveredHandle_ = handleAt(layer, canvasPosition, zoom);
 return before != hoveredHandle_;
}

bool LayerEditorShapeParameterController::active() const noexcept
{
 return activeHandle_ != LayerEditorShapeParameterHandle::None;
}

LayerEditorShapeParameterHandle
LayerEditorShapeParameterController::activeHandle() const noexcept
{
 return activeHandle_;
}

bool LayerEditorShapeParameterController::cornerRadiusHovered() const noexcept
{
 return hoveredHandle_ == LayerEditorShapeParameterHandle::CornerRadius;
}

bool LayerEditorShapeParameterController::starInnerRadiusHovered() const noexcept
{
 return hoveredHandle_ == LayerEditorShapeParameterHandle::StarInnerRadius;
}

}
