module;

#include <QWidget>

export module Artifact.Widgets.CompositionTextEditor;

import Artifact.Layer.Abstract;
import Artifact.Widgets.CompositionRenderController;

export namespace Artifact {

bool editTextLayerInline(QWidget *parent, const ArtifactAbstractLayerPtr &layer,
                         CompositionRenderController *controller);

}
