module;

class QWidget;

export module Artifact.Widgets.LayerEditor.ModePresentation;

import Tool;

export namespace Artifact {

void publishLayerEditorModeReadout(
    QWidget* widget,
    EditMode editMode,
    DisplayMode displayMode);

}
