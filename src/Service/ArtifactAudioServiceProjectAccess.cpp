module;

#include <QSet>
#include <QString>

module Artifact.Service.Audio;

import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Audio.Mixer;
import Audio.Bus;
import Memory.SharedPtr;
import Utils.Id;

namespace Artifact {

bool compositionLayerAudioMuted(const ArtifactAbstractLayerPtr& layer,
                                bool* hasAudioMute);

bool syncCurrentProjectCompositionAudio(float masterVolume, bool masterMuted)
{
    auto* projectService = ArtifactProjectService::instance();
    const auto composition = projectService
        ? projectService->currentComposition().lock()
        : ArtifactCompositionPtr{};
    if (!composition) {
        return false;
    }

    composition->ensureAudioMixer();
    const auto mixer = composition->getAudioMixer();
    if (!mixer) {
        return false;
    }
    QSet<QString> activeLayerBuses;
    for (const auto& layer : composition->allLayer()) {
        if (!layer || !layer->hasAudio()) {
            continue;
        }
        const auto rawName = ArtifactCore::AudioMixer::layerBusName(layer->id());
        const QString name = QString::fromUtf8(
            rawName.data(), static_cast<int>(rawName.length()));
        activeLayerBuses.insert(name);
        auto bus = mixer->ensureLayerBus(layer->id());
        if (!bus) {
            continue;
        }
        bool hasAudioMute = false;
        const bool muted = compositionLayerAudioMuted(layer, &hasAudioMute);
        if (hasAudioMute) {
            bus->setMute(muted);
        }
        bus->setSolo(layer->isSolo());
    }
    for (const auto& rawName : mixer->busNames()) {
        const QString name = QString::fromUtf8(
            rawName.data(), static_cast<int>(rawName.length()));
        if (mixer->busKind(mixer->findBusByName(name)) !=
                ArtifactCore::AudioBusKind::Layer ||
            activeLayerBuses.contains(name)) {
            continue;
        }
        if (const auto staleBus = mixer->findBusByName(name)) {
            mixer->removeBus(staleBus);
        }
    }
    if (auto* playback = ArtifactPlaybackService::instance()) {
        playback->setAudioMasterVolume(masterVolume);
        playback->setAudioMasterMuted(masterMuted);
    }
    return true;
}

} // namespace Artifact
