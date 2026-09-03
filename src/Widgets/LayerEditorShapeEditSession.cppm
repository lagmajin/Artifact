module;

#include <QPointF>

#include <utility>
#include <vector>

module Artifact.Widgets.LayerEditor.ShapeEditSession;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
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

void restorePolygon(const ArtifactCore::SharedPtr<ArtifactShapeLayer>& shape,
                    const std::vector<QPointF>& points, bool closed)
{
 if (points.size() >= 3) shape->setCustomPolygonPoints(points, closed);
 else shape->clearCustomPolygonPoints();
 shape->changed();
}

void restorePath(const ArtifactCore::SharedPtr<ArtifactShapeLayer>& shape,
                 const std::vector<CustomPathVertex>& vertices, bool closed)
{
 if (vertices.size() >= 3) shape->setCustomPathVertices(vertices, closed);
 else shape->clearCustomPath();
 shape->changed();
}

}

void LayerEditorShapeEditSession::beginPolygon(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer || (polygonPending_ && polygonLayer_.lock() == layer)) return;
 auto shape = shapeLayer(layer);
 if (!shape) return;
 polygonPending_ = true;
 polygonDirty_ = false;
 polygonLayer_ = layer;
 polygonBefore_ = shape->customPolygonPoints();
 polygonBeforeClosed_ = shape->customPolygonClosed();
}

void LayerEditorShapeEditSession::markPolygonDirty()
{
 if (polygonPending_) polygonDirty_ = true;
}

void LayerEditorShapeEditSession::commitPolygon()
{
 if (!polygonPending_) return;
 auto layer = polygonLayer_.lock();
 polygonPending_ = false;
 polygonLayer_.reset();
 if (layer && polygonDirty_) {
  if (auto shape = shapeLayer(layer)) {
   const auto after = shape->customPolygonPoints();
   const bool afterClosed = shape->customPolygonClosed();
   if (auto* undo = UndoManager::instance();
       undo && !undo->push(makeShapeEditCommand(
           layer, polygonBefore_, after, polygonBeforeClosed_, afterClosed))) {
    restorePolygon(shape, polygonBefore_, polygonBeforeClosed_);
   }
  }
 }
 polygonBefore_.clear();
 polygonDirty_ = false;
}

void LayerEditorShapeEditSession::cancelPolygon()
{
 auto layer = polygonLayer_.lock();
 if (layer && polygonPending_ && polygonDirty_) {
  if (auto shape = shapeLayer(layer))
   restorePolygon(shape, polygonBefore_, polygonBeforeClosed_);
 }
 polygonPending_ = false;
 polygonDirty_ = false;
 polygonLayer_.reset();
 polygonBefore_.clear();
}

bool LayerEditorShapeEditSession::polygonPending() const noexcept
{
 return polygonPending_;
}

void LayerEditorShapeEditSession::beginPath(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer || (pathPending_ && pathLayer_.lock() == layer)) return;
 auto shape = shapeLayer(layer);
 if (!shape) return;
 pathPending_ = true;
 pathDirty_ = false;
 pathLayer_ = layer;
 pathBefore_ = shape->customPathVertices();
 pathBeforeClosed_ = shape->customPathClosed();
}

void LayerEditorShapeEditSession::markPathDirty()
{
 if (pathPending_) pathDirty_ = true;
}

void LayerEditorShapeEditSession::commitPath()
{
 if (!pathPending_) return;
 auto layer = pathLayer_.lock();
 pathPending_ = false;
 pathLayer_.reset();
 if (layer && pathDirty_) {
  if (auto shape = shapeLayer(layer)) {
   const auto after = shape->customPathVertices();
   const bool afterClosed = shape->customPathClosed();
   if (auto* undo = UndoManager::instance();
       undo && !undo->push(makePathVertexEditCommand(
           layer, pathBefore_, after, pathBeforeClosed_, afterClosed))) {
    restorePath(shape, pathBefore_, pathBeforeClosed_);
   }
  }
 }
 pathBefore_.clear();
 pathDirty_ = false;
}

void LayerEditorShapeEditSession::cancelPath()
{
 auto layer = pathLayer_.lock();
 if (layer && pathPending_ && pathDirty_) {
  if (auto shape = shapeLayer(layer))
   restorePath(shape, pathBefore_, pathBeforeClosed_);
 }
 pathPending_ = false;
 pathDirty_ = false;
 pathLayer_.reset();
 pathBefore_.clear();
}

bool LayerEditorShapeEditSession::pathPending() const noexcept
{
 return pathPending_;
}

}
