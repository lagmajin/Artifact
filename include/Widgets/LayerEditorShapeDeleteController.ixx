module;

export module Artifact.Widgets.LayerEditor.ShapeDeleteController;

import Artifact.Layer.Abstract;
import Artifact.Widgets.LayerEditor.ShapeEditSession;
import Artifact.Widgets.LayerEditor.ShapeHoverController;

export namespace Artifact {

struct LayerEditorShapeDeleteResult {
 bool handled = false;
 bool startPolygonDrag = false;
 int polygonVertexIndex = -1;
};

class LayerEditorShapeDeleteController {
public:
 LayerEditorShapeDeleteResult handle(
     const ArtifactAbstractLayerPtr& layer,
     LayerEditorShapeHoverController& hoverController,
     LayerEditorShapeEditSession& editSession) const;
};

}
