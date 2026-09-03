module;

#include <QPointF>

#include <functional>

export module Artifact.Widgets.LayerEditor.ViewKeyInputController;

import Artifact.Render.IRenderer;
import Tool;
import Artifact.Widgets.LayerEditor.ViewportChrome;

export namespace Artifact {

struct LayerEditorViewKeyInputState {
 int key = 0;
 bool altModifier = false;
 bool autoRepeat = false;
 bool hasTargetLayer = false;
 QPointF viewportCenter;
 float* zoomLevel = nullptr;
};

struct LayerEditorViewKeyInputCallbacks {
 std::function<void(int)> toggleLayerState;
 std::function<void(LayerEditorSurfaceMode)> setSurfaceMode;
 std::function<void(DisplayMode)> setDisplayMode;
};

struct LayerEditorViewKeyInputResult {
 bool consumed = false;
 bool requestRender = false;
};

class LayerEditorViewKeyInputController {
public:
 LayerEditorViewKeyInputResult handle(
     const LayerEditorViewKeyInputState& state,
     const LayerEditorViewKeyInputCallbacks& callbacks,
     ArtifactIRenderer& renderer) const;
};

}
