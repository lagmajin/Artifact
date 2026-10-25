module;
#include <chrono>
#include <cstdio>
#include <QByteArray>
#include <QString>

module Artifact.Layer.Video;
import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Event.Bus;

namespace Artifact {
void publishVideoLayerModified(ArtifactVideoLayer* layer, bool frameReady)
{
    if (!layer) {
        return;
    }

    if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {
        static const bool profileNotifications =
            qEnvironmentVariableIntValue("ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP") != 0;
        static int notificationSamples = 0;
        const bool profileThisNotification = profileNotifications && notificationSamples++ < 64;
        const auto notificationStart = profileThisNotification
            ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                              frameReady ? LayerChangedEvent::ChangeType::FrameReady
                                         : LayerChangedEvent::ChangeType::Modified});
        if (profileThisNotification) {
            std::fprintf(stderr, "[VideoPreviewNotification] elapsedMs=%.3f\n",
                         std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - notificationStart).count());
        }
    }
}

}
