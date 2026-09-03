module;

#include <QPointF>

export module Artifact.Widgets.LayerEditor.MaskHoverController;

import Artifact.Layer.Abstract;
import Artifact.Widgets.LayerEditor.Geometry;

export namespace Artifact {

struct LayerEditorMaskHoverState {
 int maskIndex = -1;
 int pathIndex = -1;
 int vertexIndex = -1;
 MaskHandleType handleType = MaskHandleType::None;
};

class LayerEditorMaskHoverController {
public:
 bool update(const ArtifactAbstractLayerPtr& layer,
             const QPointF& canvasPosition, float zoom);
 bool hitVertex(const ArtifactAbstractLayerPtr& layer,
                const QPointF& canvasPosition, float zoom,
                int& maskIndex, int& pathIndex, int& vertexIndex) const;
 void set(const LayerEditorMaskHoverState& state) noexcept;
 void clear() noexcept;
 bool deleteHoveredVertex(const ArtifactAbstractLayerPtr& layer);

 const LayerEditorMaskHoverState& state() const noexcept;
 bool hasVertex() const noexcept;

private:
 LayerEditorMaskHoverState state_;
};

}
