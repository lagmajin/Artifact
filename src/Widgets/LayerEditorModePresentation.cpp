module;

#include <QWidget>

module Artifact.Widgets.LayerEditor.ModePresentation;

import Artifact.Widgets.LayerEditor.ViewportChrome;
import Tool;

namespace Artifact {

void publishLayerEditorModeReadout(
    QWidget* widget,
    const EditMode editMode,
    const DisplayMode displayMode)
{
 if (!widget) return;
 const QString editLabel = layerEditorEditModeLabel(editMode);
 const QString displayLabel = layerEditorDisplayModeLabel(displayMode);
 QString surfaceLabel = widget->property("artifactSurfaceMode").toString();
 if (surfaceLabel.isEmpty()) surfaceLabel = QStringLiteral("Edit");
 widget->setProperty("artifactEditMode", editLabel);
 widget->setProperty("artifactDisplayMode", displayLabel);
 widget->setProperty(
     "artifactModeSummary", QStringLiteral("%1 / %2").arg(editLabel, displayLabel));
 widget->setProperty(
     "artifactViewSummary",
     QStringLiteral("%1 | %2 / %3").arg(surfaceLabel, editLabel, displayLabel));
 widget->setAccessibleDescription(
     QStringLiteral("%1 surface, %2 mode, %3 display")
         .arg(surfaceLabel, editLabel, displayLabel));
}

}
