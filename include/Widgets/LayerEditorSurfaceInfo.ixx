module;

#include <QString>
#include <vector>

export module Artifact.Widgets.LayerEditor.SurfaceInfo;

import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Frame.Position;
import Tool;
import Utils.Id;

export namespace Artifact {

struct LayerEditorSurfaceInfo {
 QString title;
 QString body;
 std::vector<LayerID> parentLayerIds;
 std::vector<LayerID> childLayerIds;
 std::vector<LayerID> matteLayerIds;
 std::vector<LayerID> dependentLayerIds;
};

LayerEditorSurfaceInfo buildLayerEditorSurfaceInfo(
    const ArtifactAbstractLayerPtr& layer,
    const ArtifactCompositionPtr& composition,
    const FramePosition& currentFrame, DisplayMode displayMode,
    bool inspectMode);

}
