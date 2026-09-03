module;

#include <vector>

export module Artifact.Widgets.LayerEditor.ShapeInputController;

import Artifact.Layer.Abstract;
import Artifact.Widgets.LayerEditor.ShapeEditSession;

export namespace Artifact {

enum class LayerEditorShapeKeyAction { Extrude, ToggleClosed, ToggleSelectAll };
enum class LayerEditorShapeKeyResult {
 Ignored,
 SelectionChanged,
 GeometryChanged,
 ExtrudedPolygon,
 ExtrudedPath
};

class LayerEditorShapeInputController {
public:
 LayerEditorShapeKeyResult handle(
     LayerEditorShapeKeyAction action,
     const ArtifactAbstractLayerPtr& layer,
     std::vector<int>& polygonSelection,
     std::vector<int>& pathSelection,
     LayerEditorShapeEditSession& editSession) const;
};

}
