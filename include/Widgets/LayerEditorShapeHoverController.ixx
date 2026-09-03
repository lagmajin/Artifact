module;

#include <QPointF>

export module Artifact.Widgets.LayerEditor.ShapeHoverController;

import Artifact.Layer.Abstract;

export namespace Artifact {

struct LayerEditorShapeHoverState {
 int polygonVertex = -1;
 int polygonSegment = -1;
 int pathVertex = -1;
 int pathTangentVertex = -1;
 int pathTangentType = 0;
};

class LayerEditorShapeHoverController {
public:
 bool hitPolygonVertex(const ArtifactAbstractLayerPtr& layer,
                       const QPointF& canvasPosition, float zoom,
                       int& vertexIndex) const;
 bool hitPolygonSegment(const ArtifactAbstractLayerPtr& layer,
                        const QPointF& canvasPosition, float zoom,
                        int& insertIndex) const;
 bool hitPathVertex(const ArtifactAbstractLayerPtr& layer,
                    const QPointF& canvasPosition, float zoom,
                    int& vertexIndex) const;
 bool hitPathTangent(const ArtifactAbstractLayerPtr& layer,
                     const QPointF& canvasPosition, float zoom,
                     int& vertexIndex, int& tangentType) const;

 bool updatePolygon(const ArtifactAbstractLayerPtr& layer,
                    const QPointF& canvasPosition, float zoom);
 bool updatePath(const ArtifactAbstractLayerPtr& layer,
                 const QPointF& canvasPosition, float zoom);
 void setPolygon(int vertexIndex, int segmentIndex) noexcept;
 void setPath(int vertexIndex, int tangentVertex, int tangentType) noexcept;
 void clear() noexcept;

 const LayerEditorShapeHoverState& state() const noexcept;

private:
 LayerEditorShapeHoverState state_;
};

}
