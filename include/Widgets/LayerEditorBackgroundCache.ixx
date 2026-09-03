module;

#include <QImage>
#include <QSize>

export module Artifact.Widgets.LayerEditor.BackgroundCache;

import Color.Float;

export namespace Artifact {

QImage makeLayerEditorMayaGradientSprite(
    const QSize& size,
    const ArtifactCore::FloatColor& backgroundColor);

}
