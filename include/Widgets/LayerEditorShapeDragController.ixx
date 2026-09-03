module;

#include <QPointF>

#include <vector>

export module Artifact.Widgets.LayerEditor.ShapeDragController;

import Artifact.Layer.Shape;

export namespace Artifact {

class LayerEditorShapeDragController {
public:
 bool dragPolygonVertex(
     ArtifactShapeLayer& shape, const QPointF& localPosition,
     int vertexIndex, const std::vector<int>& selectedIndices,
     const std::vector<QPointF>& dragBefore,
     bool proportionalEditingEnabled, bool proportionalDragActive,
     const std::vector<QPointF>& proportionalBefore,
     const QPointF& proportionalOrigin, float proportionalRadius) const;

 bool dragPathVertex(
     ArtifactShapeLayer& shape, const QPointF& localPosition,
     int vertexIndex, const std::vector<int>& selectedIndices,
     const std::vector<CustomPathVertex>& dragBefore,
     bool proportionalEditingEnabled, bool proportionalDragActive,
     const std::vector<CustomPathVertex>& proportionalBefore,
     const QPointF& proportionalOrigin, float proportionalRadius) const;

 bool dragPathTangent(ArtifactShapeLayer& shape,
                      const QPointF& localPosition,
                      int vertexIndex, int tangentType,
                      bool independentTangent) const;
};

}
