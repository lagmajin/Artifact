module;

#include <QPointF>

export module Artifact.Widgets.LayerEditor.ShapeParameterController;

import Artifact.Layer.Abstract;

export namespace Artifact {

enum class LayerEditorShapeParameterHandle { None, CornerRadius, StarInnerRadius };

class LayerEditorShapeParameterController {
public:
 LayerEditorShapeParameterHandle handleAt(
     const ArtifactAbstractLayerPtr& layer,
     const QPointF& canvasPosition, float zoom) const;
 bool begin(const ArtifactAbstractLayerPtr& layer,
            const QPointF& canvasPosition,
            const QPointF& viewportPosition, float zoom);
 bool update(const QPointF& canvasPosition,
             const QPointF& viewportPosition);
 bool commit();
 void cancel();
 void clearHover() noexcept;
 bool updateHover(const ArtifactAbstractLayerPtr& layer,
                  const QPointF& canvasPosition, float zoom);

 bool active() const noexcept;
 LayerEditorShapeParameterHandle activeHandle() const noexcept;
 bool cornerRadiusHovered() const noexcept;
 bool starInnerRadiusHovered() const noexcept;

private:
 LayerEditorShapeParameterHandle activeHandle_ = LayerEditorShapeParameterHandle::None;
 LayerEditorShapeParameterHandle hoveredHandle_ = LayerEditorShapeParameterHandle::None;
 ArtifactAbstractLayerWeak layer_;
 float before_ = 0.0f;
 float start_ = 0.0f;
 float anchorViewportX_ = 0.0f;
 float maxCornerRadius_ = 0.0f;
};

}
