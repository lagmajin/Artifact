module;

#include <QPointF>

#include <vector>

export module Artifact.Widgets.LayerEditor.ShapeEditSession;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;

export namespace Artifact {

class LayerEditorShapeEditSession {
public:
 void beginPolygon(const ArtifactAbstractLayerPtr& layer);
 void markPolygonDirty();
 void commitPolygon();
 void cancelPolygon();
 bool polygonPending() const noexcept;

 void beginPath(const ArtifactAbstractLayerPtr& layer);
 void markPathDirty();
 void commitPath();
 void cancelPath();
 bool pathPending() const noexcept;

private:
 bool polygonPending_ = false;
 bool polygonDirty_ = false;
 ArtifactAbstractLayerWeak polygonLayer_;
 std::vector<QPointF> polygonBefore_;
 bool polygonBeforeClosed_ = true;

 bool pathPending_ = false;
 bool pathDirty_ = false;
 ArtifactAbstractLayerWeak pathLayer_;
 std::vector<CustomPathVertex> pathBefore_;
 bool pathBeforeClosed_ = true;
};

}
