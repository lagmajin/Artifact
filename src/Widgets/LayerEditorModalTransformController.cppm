module;

#include <QPointF>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

module Artifact.Widgets.LayerEditor.ModalTransformController;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Widgets.LayerEditor.Geometry;

namespace Artifact {
namespace {

double snapped(double value, double interval)
{
 return interval > 0.0 ? std::round(value / interval) * interval : value;
}

}

bool LayerEditorModalTransformController::begin(
    LayerEditorModalTransformMode mode, const ArtifactAbstractLayerPtr& layer,
    const QPointF& viewportPosition,
    const std::vector<int>& polygonSelection,
    const std::vector<int>& pathSelection)
{
 if (!layer || mode == LayerEditorModalTransformMode::None) return false;
 auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
     ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
 if (!shape) return false;

 const bool path = shape->hasCustomPath();
 if (mode == LayerEditorModalTransformMode::Inset && path) return false;
 const auto& selection = path ? pathSelection : polygonSelection;
 if (mode != LayerEditorModalTransformMode::Inset && selection.empty()) return false;
 if (mode == LayerEditorModalTransformMode::Inset &&
     shape->customPolygonPoints().size() < 3) return false;

 mode_ = mode;
 axis_ = LayerEditorModalTransformAxis::Free;
 target_ = path ? LayerEditorModalTransformTarget::Path
                : LayerEditorModalTransformTarget::Polygon;
 layer_ = layer;
 lastViewportPosition_ = viewportPosition;
 accumulatedDelta_ = QPointF();
 accumulatedScalar_ = 0.0;
 selection_ = selection;
 polygonBefore_.clear();
 pathBefore_.clear();
 if (path) pathBefore_ = shape->customPathVertices();
 else polygonBefore_ = shape->customPolygonPoints();
 return true;
}

LayerEditorModalTransformTarget LayerEditorModalTransformController::update(
    const QPointF& viewportPosition, double zoom, bool precision, bool snap)
{
 if (!active()) return LayerEditorModalTransformTarget::None;
 auto layer = layer_.lock();
 auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
     ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
 if (!shape) {
  finish();
  return LayerEditorModalTransformTarget::None;
 }

 const QPointF viewportDelta = viewportPosition - lastViewportPosition_;
 lastViewportPosition_ = viewportPosition;
 const double precisionScale = precision ? 0.1 : 1.0;
 const double safeZoom = std::max(0.001, zoom);
 const QPointF unconstrainedCanvasDelta =
     viewportDelta * (precisionScale / safeZoom);
 QPointF canvasDelta = unconstrainedCanvasDelta;
 if (axis_ == LayerEditorModalTransformAxis::X) canvasDelta.setY(0.0);
 if (axis_ == LayerEditorModalTransformAxis::Y) canvasDelta.setX(0.0);

 if (mode_ == LayerEditorModalTransformMode::Grab) {
  accumulatedDelta_ += canvasDelta;
  QPointF applied = accumulatedDelta_;
  if (snap) applied = QPointF(snapped(applied.x(), 10.0),
                              snapped(applied.y(), 10.0));
  if (target_ == LayerEditorModalTransformTarget::Path)
   shape->setCustomPathVertices(
       translateSelectedPath(pathBefore_, selection_, applied),
       shape->customPathClosed());
  else
   shape->setCustomPolygonPoints(
       translateSelectedPolygon(polygonBefore_, selection_, applied),
       shape->customPolygonClosed());
 } else if (mode_ == LayerEditorModalTransformMode::Rotate) {
  accumulatedScalar_ += viewportDelta.x() * 0.01 * precisionScale;
  const double radians = snap
      ? snapped(accumulatedScalar_, std::numbers::pi / 12.0)
      : accumulatedScalar_;
  if (target_ == LayerEditorModalTransformTarget::Path)
   shape->setCustomPathVertices(
       rotateSelectedPath(pathBefore_, selection_, radians),
       shape->customPathClosed());
  else
   shape->setCustomPolygonPoints(
       rotateSelectedPolygon(polygonBefore_, selection_, radians),
       shape->customPolygonClosed());
 } else if (mode_ == LayerEditorModalTransformMode::Scale) {
  accumulatedScalar_ += (viewportDelta.x() - viewportDelta.y()) *
                        0.005 * precisionScale;
  double factor = std::clamp(1.0 + accumulatedScalar_, 0.01, 100.0);
  if (snap) factor = std::max(0.01, snapped(factor, 0.1));
  QPointF factors(factor, factor);
  if (axis_ == LayerEditorModalTransformAxis::X) factors.setY(1.0);
  if (axis_ == LayerEditorModalTransformAxis::Y) factors.setX(1.0);
  if (target_ == LayerEditorModalTransformTarget::Path)
   shape->setCustomPathVertices(
       scaleSelectedPathAxes(pathBefore_, selection_, factors),
       shape->customPathClosed());
  else
   shape->setCustomPolygonPoints(
       scaleSelectedPolygonAxes(polygonBefore_, selection_, factors),
       shape->customPolygonClosed());
 } else if (mode_ == LayerEditorModalTransformMode::Inset) {
  accumulatedScalar_ += unconstrainedCanvasDelta.x();
  double factor = std::clamp(1.0 - accumulatedScalar_ * 0.01, 0.01, 100.0);
  if (snap) factor = std::max(0.01, snapped(factor, 0.1));
  shape->setCustomPolygonPoints(insetPolygon(polygonBefore_, factor),
                                shape->customPolygonClosed());
 }
 return target_;
}

void LayerEditorModalTransformController::toggleAxis(
    LayerEditorModalTransformAxis axis)
{
 if (!active() || axis == LayerEditorModalTransformAxis::Free) return;
 axis_ = axis_ == axis ? LayerEditorModalTransformAxis::Free : axis;
}

void LayerEditorModalTransformController::finish()
{
 mode_ = LayerEditorModalTransformMode::None;
 axis_ = LayerEditorModalTransformAxis::Free;
 target_ = LayerEditorModalTransformTarget::None;
 layer_.reset();
 lastViewportPosition_ = QPointF();
 accumulatedDelta_ = QPointF();
 accumulatedScalar_ = 0.0;
 selection_.clear();
 polygonBefore_.clear();
 pathBefore_.clear();
}

bool LayerEditorModalTransformController::active() const noexcept
{
 return mode_ != LayerEditorModalTransformMode::None;
}

bool LayerEditorModalTransformController::editsPath() const noexcept
{
 return target_ == LayerEditorModalTransformTarget::Path;
}

LayerEditorModalTransformMode LayerEditorModalTransformController::mode() const noexcept
{
 return mode_;
}

LayerEditorModalTransformAxis LayerEditorModalTransformController::axis() const noexcept
{
 return axis_;
}

}
