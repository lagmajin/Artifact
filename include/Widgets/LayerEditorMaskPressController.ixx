module;

#include <QPointF>

#include <vector>

export module Artifact.Widgets.LayerEditor.MaskPressController;

import Artifact.Layer.Abstract;
import Artifact.Mask.Path;
import Artifact.Widgets.LayerEditor.MaskEditSession;
import Artifact.Widgets.LayerEditor.Geometry;

export namespace Artifact {

enum class LayerEditorMaskPressKind {
 Consumed, DragHandle, DragVertex, GeometryChanged
};

struct LayerEditorMaskPressResult {
 LayerEditorMaskPressKind kind = LayerEditorMaskPressKind::Consumed;
 int maskIndex = -1;
 int pathIndex = -1;
 int vertexIndex = -1;
 MaskHandleType handleType = MaskHandleType::None;
 std::vector<MaskVertex> proportionalBefore;
 QPointF proportionalOrigin;
};

class LayerEditorMaskPressController {
public:
 LayerEditorMaskPressResult handle(
     const ArtifactAbstractLayerPtr& layer,
     const QPointF& canvasPosition, float zoom,
     bool proportionalEditingEnabled,
     LayerEditorMaskEditSession& editSession) const;
 bool closeOpenPathOnDoubleClick(
     const ArtifactAbstractLayerPtr& layer,
     const QPointF& canvasPosition, float zoom,
     LayerEditorMaskEditSession& editSession) const;
};

}
