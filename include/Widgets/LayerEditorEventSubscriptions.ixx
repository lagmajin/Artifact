module;

#include <functional>
#include <vector>

export module Artifact.Widgets.LayerEditor.EventSubscriptions;

import Event.Bus;
import Artifact.Event.Types;

export namespace Artifact {

struct LayerEditorEventCallbacks {
 std::function<void(const LayerSelectionChangedEvent&)> layerSelectionChanged;
 std::function<void(const LayerChangedEvent&)> layerChanged;
 std::function<void(const FrameChangedEvent&)> frameChanged;
 std::function<void(const PlaybackStateChangedEvent&)> playbackStateChanged;
 std::function<void(const ProjectChangedEvent&)> projectChanged;
 std::function<void(const CurrentCompositionChangedEvent&)> currentCompositionChanged;
};

std::vector<ArtifactCore::EventBus::Subscription>
subscribeLayerEditorEvents(ArtifactCore::EventBus& eventBus,
                           LayerEditorEventCallbacks callbacks);

}
