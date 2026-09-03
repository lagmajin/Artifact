module;

#include <QPointF>
#include <QSize>
#include <QString>

#include <functional>

export module Artifact.Widgets.LayerEditor.ViewportChromeInteractionController;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.ViewportChrome;
import Tool;

export namespace Artifact {

enum class LayerEditorChromeCursor {
 Unchanged, Pointing, Arrow, Cross, Unset
};

struct LayerEditorViewportChromeInteractionState {
 QPointF viewportPosition;
 QSize physicalViewportSize;
 QPointF physicalViewportCenter;
 qreal devicePixelRatio = 1.0;
 LayerEditorSurfaceMode surfaceMode = LayerEditorSurfaceMode::Edit;
 EditMode editMode = EditMode::View;
 bool hasLayerIdentity = false;
 ArtifactAbstractLayerPtr layer;
 int* hoveredControl = nullptr;
 float* zoomLevel = nullptr;
};

struct LayerEditorViewportChromeCallbacks {
 std::function<void(LayerEditorSurfaceMode)> setSurfaceMode;
 std::function<void(EditMode)> setEditMode;
 std::function<void(DisplayMode)> setDisplayMode;
 std::function<bool(int)> toggleLayerState;
};

struct LayerEditorViewportChromePressResult {
 bool consumed = false;
 bool requestRender = false;
};

struct LayerEditorViewportChromeHoverResult {
 bool overChrome = false;
 bool requestRender = false;
 bool updateToolTip = false;
 QString toolTip;
 LayerEditorChromeCursor cursor = LayerEditorChromeCursor::Unchanged;
};

class LayerEditorViewportChromeInteractionController {
public:
 LayerEditorViewportChromePressResult press(
     const LayerEditorViewportChromeInteractionState& state,
     const LayerEditorViewportChromeCallbacks& callbacks,
     ArtifactIRenderer& renderer) const;
 LayerEditorViewportChromeHoverResult hover(
     const LayerEditorViewportChromeInteractionState& state) const;
};

}
