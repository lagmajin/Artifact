module;

module Artifact.Service.Audio;

import Artifact.Layer.Abstract;
import Artifact.Layer.Audio;
import Artifact.Layer.Video;
import Memory.SharedPtr;

namespace Artifact {

bool setVideoLayerAudioVolume(const ArtifactAbstractLayerPtr& layer,
                              float volume)
{
    const auto videoLayer = ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(
        layer);
    if (!videoLayer) {
        return false;
    }
    videoLayer->setAudioVolume(volume);
    layer->changed();
    return true;
}

bool setVideoLayerAudioPan(const ArtifactAbstractLayerPtr& layer, float pan)
{
    const auto videoLayer = ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(
        layer);
    if (!videoLayer) {
        return false;
    }
    videoLayer->setAudioPan(pan);
    layer->changed();
    return true;
}

bool setVideoLayerAudioMute(const ArtifactAbstractLayerPtr& layer, bool muted)
{
    const auto videoLayer = ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(
        layer);
    if (!videoLayer) {
        return false;
    }
    videoLayer->setAudioMuted(muted);
    layer->changed();
    return true;
}

bool compositionLayerAudioMuted(const ArtifactAbstractLayerPtr& layer,
                                bool* hasAudioMute)
{
    const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(
        layer);
    const auto videoLayer = ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(
        layer);
    if (hasAudioMute) {
        *hasAudioMute = static_cast<bool>(audioLayer) ||
                        static_cast<bool>(videoLayer);
    }
    return audioLayer ? audioLayer->isMuted()
                      : videoLayer ? videoLayer->isAudioMuted() : false;
}

bool videoLayerAudioMuted(const ArtifactAbstractLayerPtr& layer,
                          bool* isVideoLayer)
{
    const auto videoLayer = ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(
        layer);
    if (isVideoLayer) {
        *isVideoLayer = static_cast<bool>(videoLayer);
    }
    return videoLayer ? videoLayer->isAudioMuted() : false;
}

} // namespace Artifact
