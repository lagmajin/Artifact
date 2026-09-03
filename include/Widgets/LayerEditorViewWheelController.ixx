module;

#include <QPoint>

export module Artifact.Widgets.LayerEditor.ViewWheelController;

export namespace Artifact {

enum class LayerEditorViewWheelAction {
 None,
 PanHorizontal,
 Zoom
};

struct LayerEditorViewWheelInput {
 QPoint angleDelta;
 QPoint pixelDelta;
 bool shiftModifier = false;
};

struct LayerEditorViewWheelResult {
 LayerEditorViewWheelAction action = LayerEditorViewWheelAction::None;
 float value = 0.0f;
};

class LayerEditorViewWheelController {
public:
 LayerEditorViewWheelResult handle(const LayerEditorViewWheelInput& input) const;
};

}
