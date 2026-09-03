module;

#include <QSize>
#include <QString>

export module Artifact.Widgets.LayerEditor.ViewportChromeRenderer;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.ViewportChrome;
import Tool;

export namespace Artifact {

struct LayerEditorViewportChromeState {
 QSize viewportSize;
 QSize restoreCanvasSize;
 LayerEditorSurfaceMode surfaceMode = LayerEditorSurfaceMode::Edit;
 EditMode editMode = EditMode::View;
 DisplayMode displayMode = DisplayMode::Color;
 int hoveredControl = -1;
 QString surfaceInfoTitle;
 QString surfaceInfoBody;
 QString layerName;
 QString layerType;
 bool layerActive = false;
 bool viewToolEnabled = true;
 bool transformToolEnabled = false;
 bool shapeToolEnabled = false;
 bool maskToolEnabled = false;
};

void drawLayerEditorViewportChrome(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    const LayerEditorViewportChromeState& state);

}
