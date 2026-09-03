module;

#include <QPointF>

#include <vector>

export module Artifact.Widgets.LayerEditor.MaskDragController;

import Artifact.Layer.Abstract;
import Artifact.Mask.Path;
import Artifact.Widgets.LayerEditor.Geometry;

export namespace Artifact {

class LayerEditorMaskDragController {
public:
 bool dragHandle(const ArtifactAbstractLayerPtr& layer,
                 const QPointF& canvasPosition,
                 int maskIndex, int pathIndex, int vertexIndex,
                 MaskHandleType handleType) const;

 bool dragVertex(const ArtifactAbstractLayerPtr& layer,
                 const QPointF& canvasPosition,
                 int maskIndex, int pathIndex, int vertexIndex,
                 bool proportionalDragActive,
                 const std::vector<MaskVertex>& proportionalBefore,
                 const QPointF& proportionalOrigin,
                 float proportionalRadius) const;
};

}
