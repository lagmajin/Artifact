module;

#include <QCursor>
#include <QString>

export module Artifact.Widgets.LayerEditor.HudCursor;

import Artifact.Widgets.TransformGizmo;

export namespace Artifact {

const QCursor& layerEditorHudCursor(
    const QString& iconName,
    Qt::CursorShape fallbackShape,
    int hotX = 12,
    int hotY = 12);
const QCursor& layerEditorHudCursorForTransformHandle(
    TransformGizmo::HandleType handle,
    bool dragging);

}
