module;

#include <utility>
#include <vector>

module Artifact.Widgets.LayerEditor.EventSubscriptions;

import Event.Bus;
import Artifact.Event.Types;

namespace Artifact {

std::vector<ArtifactCore::EventBus::Subscription>
subscribeLayerEditorEvents(ArtifactCore::EventBus& eventBus,
                           LayerEditorEventCallbacks callbacks)
{
 std::vector<ArtifactCore::EventBus::Subscription> subscriptions;
 subscriptions.reserve(6);
 subscriptions.push_back(eventBus.subscribe<LayerSelectionChangedEvent>(
     [callback = std::move(callbacks.layerSelectionChanged)](const auto& event) {
       if (callback) callback(event);
     }));
 subscriptions.push_back(eventBus.subscribe<LayerChangedEvent>(
     [callback = std::move(callbacks.layerChanged)](const auto& event) {
       if (callback) callback(event);
     }));
 subscriptions.push_back(eventBus.subscribe<FrameChangedEvent>(
     [callback = std::move(callbacks.frameChanged)](const auto& event) {
       if (callback) callback(event);
     }));
 subscriptions.push_back(eventBus.subscribe<PlaybackStateChangedEvent>(
     [callback = std::move(callbacks.playbackStateChanged)](const auto& event) {
       if (callback) callback(event);
     }));
 subscriptions.push_back(eventBus.subscribe<ProjectChangedEvent>(
     [callback = std::move(callbacks.projectChanged)](const auto& event) {
       if (callback) callback(event);
     }));
 subscriptions.push_back(eventBus.subscribe<CurrentCompositionChangedEvent>(
     [callback = std::move(callbacks.currentCompositionChanged)](const auto& event) {
       if (callback) callback(event);
     }));
 return subscriptions;
}

}
