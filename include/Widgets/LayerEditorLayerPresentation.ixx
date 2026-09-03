module;

#include <QString>

class QWidget;

export module Artifact.Widgets.LayerEditor.LayerPresentation;

import Artifact.Layer.Abstract;

export namespace Artifact {

QString layerEditorLayerTypeLabel(const ArtifactAbstractLayerPtr& layer);
QString layerEditorLayerNameLabel(const ArtifactAbstractLayerPtr& layer);
void publishLayerEditorReadout(
    QWidget* widget,
    const ArtifactAbstractLayerPtr& layer,
    bool isActive);

}
