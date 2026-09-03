module;

#include <QPointF>

#include <vector>

export module Artifact.Widgets.LayerEditor.ModalTransformController;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;

export namespace Artifact {

enum class LayerEditorModalTransformMode { None, Grab, Rotate, Scale, Inset };
enum class LayerEditorModalTransformAxis { Free, X, Y };
enum class LayerEditorModalTransformTarget { None, Polygon, Path };

class LayerEditorModalTransformController {
public:
 bool begin(LayerEditorModalTransformMode mode,
            const ArtifactAbstractLayerPtr& layer,
            const QPointF& viewportPosition,
            const std::vector<int>& polygonSelection,
            const std::vector<int>& pathSelection);
 LayerEditorModalTransformTarget update(
     const QPointF& viewportPosition, double zoom,
     bool precision, bool snap);
 void toggleAxis(LayerEditorModalTransformAxis axis);
 void finish();

 bool active() const noexcept;
 bool editsPath() const noexcept;
 LayerEditorModalTransformMode mode() const noexcept;
 LayerEditorModalTransformAxis axis() const noexcept;

private:
 LayerEditorModalTransformMode mode_ = LayerEditorModalTransformMode::None;
 LayerEditorModalTransformAxis axis_ = LayerEditorModalTransformAxis::Free;
 LayerEditorModalTransformTarget target_ = LayerEditorModalTransformTarget::None;
 ArtifactAbstractLayerWeak layer_;
 QPointF lastViewportPosition_;
 QPointF accumulatedDelta_;
 double accumulatedScalar_ = 0.0;
 std::vector<int> selection_;
 std::vector<QPointF> polygonBefore_;
 std::vector<CustomPathVertex> pathBefore_;
};

}
