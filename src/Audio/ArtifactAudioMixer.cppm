module;
#include <utility>

#include <QDebug>
#include <QHash>
#include <QObject>
#include <QSignalBlocker>
#include <QString>
#include <QStringList>
#include <QVector>
#include <wobjectimpl.h>

module Artifact.Audio.Mixer;

import Artifact.Audio.Mixer;
import Artifact.Composition.Abstract;
import Artifact.Layer.Audio;
import Artifact.Layer.Video;
import std;

namespace Artifact
{
    using namespace ArtifactCore;

namespace
{
float readLayerVolume(const ArtifactAbstractLayerPtr& layer)
{
    if (!layer) {
        return 1.0f;
    }
    if (auto audioLayer = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
        return audioLayer->volume();
    }
    if (auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
        return static_cast<float>(videoLayer->audioVolume());
    }
    return 1.0f;
}

bool readLayerMuted(const ArtifactAbstractLayerPtr& layer)
{
    if (!layer) {
        return false;
    }
    if (auto audioLayer = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
        return audioLayer->isMuted();
    }
    if (auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
        return videoLayer->isAudioMuted();
    }
    return false;
}

void applyLayerVolume(const ArtifactAbstractLayerPtr& layer, const float volume)
{
    if (!layer) {
        return;
    }
    if (auto audioLayer = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
        audioLayer->setVolume(volume);
        return;
    }
    if (auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
        videoLayer->setAudioVolume(volume);
        layer->changed();
    }
}

void applyLayerMuted(const ArtifactAbstractLayerPtr& layer, const bool muted)
{
    if (!layer) {
        return;
    }
    if (auto audioLayer = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
        if (audioLayer->isMuted() != muted) {
            audioLayer->mute();
        }
        return;
    }
    if (auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
        videoLayer->setAudioMuted(muted);
        layer->changed();
    }
}

void applyLayerSolo(const ArtifactAbstractLayerPtr& layer, const bool solo)
{
    if (!layer || layer->isSolo() == solo) {
        return;
    }
    layer->setSolo(solo);
    layer->changed();
}

bool supportsMixerLayer(const ArtifactAbstractLayerPtr& layer)
{
    return static_cast<bool>(std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) ||
        static_cast<bool>(std::dynamic_pointer_cast<ArtifactVideoLayer>(layer));
}

float volumeToMeterDb(const float volume, const bool muted)
{
    if (muted || volume <= 0.0001f) {
        return -60.0f;
    }
    return std::clamp(20.0f * std::log10(volume), -60.0f, 0.0f);
}
}

    W_OBJECT_IMPL(AudioMixerChannelStrip)
    W_OBJECT_IMPL(AudioMixerMasterBus)
    W_OBJECT_IMPL(AudioMixer)

class AudioMixerChannelStrip::Impl
{
public:
    LayerID layerId_;
    QString layerName_;

    float volume_ = 1.0f;
    float pan_ = 0.0f;
    bool muted_ = false;
    bool solo_ = false;
    bool stereoLinked_ = true;

    QStringList effectChain_;
    float leftLevel_ = -60.0f;
    float rightLevel_ = -60.0f;
    float peakLeft_ = -60.0f;
    float peakRight_ = -60.0f;
};

AudioMixerChannelStrip::AudioMixerChannelStrip(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

AudioMixerChannelStrip::~AudioMixerChannelStrip()
{
    delete impl_;
}

void AudioMixerChannelStrip::setLayerId(LayerID id)
{
    impl_->layerId_ = id;
}

AudioMixerChannelStrip::LayerID AudioMixerChannelStrip::layerId() const
{
    return impl_->layerId_;
}

void AudioMixerChannelStrip::setLayerName(const QString& name)
{
    impl_->layerName_ = name;
}

QString AudioMixerChannelStrip::layerName() const
{
    return impl_->layerName_;
}

void AudioMixerChannelStrip::setVolume(float volume)
{
    const float clamped = std::clamp(volume, 0.0f, 2.0f);
    if (std::abs(impl_->volume_ - clamped) <= 0.0001f) {
        return;
    }
    impl_->volume_ = clamped;
    Q_EMIT volumeChanged(impl_->volume_);
}

float AudioMixerChannelStrip::volume() const
{
    return impl_->volume_;
}

void AudioMixerChannelStrip::setPan(float pan)
{
    impl_->pan_ = std::clamp(pan, -1.0f, 1.0f);
    Q_EMIT panChanged(impl_->pan_);
}

float AudioMixerChannelStrip::pan() const
{
    return impl_->pan_;
}

void AudioMixerChannelStrip::setMuted(bool muted)
{
    impl_->muted_ = muted;
    Q_EMIT muteChanged(muted);
}

bool AudioMixerChannelStrip::isMuted() const
{
    return impl_->muted_;
}

void AudioMixerChannelStrip::setSolo(bool solo)
{
    impl_->solo_ = solo;
    Q_EMIT soloChanged(solo);
}

bool AudioMixerChannelStrip::isSolo() const
{
    return impl_->solo_;
}

void AudioMixerChannelStrip::setStereoLinked(bool linked)
{
    impl_->stereoLinked_ = linked;
}

bool AudioMixerChannelStrip::isStereoLinked() const
{
    return impl_->stereoLinked_;
}

void AudioMixerChannelStrip::addEffect(const QString& effectId)
{
    impl_->effectChain_.append(effectId);
}

void AudioMixerChannelStrip::removeEffect(int index)
{
    if (index >= 0 && index < impl_->effectChain_.size()) {
        impl_->effectChain_.removeAt(index);
    }
}

void AudioMixerChannelStrip::moveEffect(int from, int to)
{
    if (from >= 0 && from < impl_->effectChain_.size() &&
        to >= 0 && to < impl_->effectChain_.size()) {
        impl_->effectChain_.move(from, to);
    }
}

QStringList AudioMixerChannelStrip::effectChain() const
{
    return impl_->effectChain_;
}

float AudioMixerChannelStrip::leftLevel() const
{
    return impl_->leftLevel_;
}

float AudioMixerChannelStrip::rightLevel() const
{
    return impl_->rightLevel_;
}

float AudioMixerChannelStrip::peakLeft() const
{
    return impl_->peakLeft_;
}

float AudioMixerChannelStrip::peakRight() const
{
    return impl_->peakRight_;
}

void AudioMixerChannelStrip::updateLevels(float left, float right)
{
    impl_->leftLevel_ = left;
    impl_->rightLevel_ = right;
    impl_->peakLeft_ = std::max(impl_->peakLeft_, left);
    impl_->peakRight_ = std::max(impl_->peakRight_, right);
    Q_EMIT levelChanged(left, right);
}

void AudioMixerChannelStrip::resetPeak()
{
    impl_->peakLeft_ = -60.0f;
    impl_->peakRight_ = -60.0f;
}

class AudioMixerMasterBus::Impl
{
public:
    float volume_ = 1.0f;
    bool muted_ = false;
    float leftLevel_ = -60.0f;
    float rightLevel_ = -60.0f;
};

AudioMixerMasterBus::AudioMixerMasterBus(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

AudioMixerMasterBus::~AudioMixerMasterBus()
{
    delete impl_;
}

void AudioMixerMasterBus::setVolume(float volume)
{
    const float clamped = std::clamp(volume, 0.0f, 2.0f);
    if (std::abs(impl_->volume_ - clamped) <= 0.0001f) {
        return;
    }
    impl_->volume_ = clamped;
    Q_EMIT volumeChanged(impl_->volume_);
}

float AudioMixerMasterBus::volume() const
{
    return impl_->volume_;
}

void AudioMixerMasterBus::setMuted(bool muted)
{
    impl_->muted_ = muted;
    Q_EMIT muteChanged(muted);
}

bool AudioMixerMasterBus::isMuted() const
{
    return impl_->muted_;
}

float AudioMixerMasterBus::leftLevel() const
{
    return impl_->leftLevel_;
}

float AudioMixerMasterBus::rightLevel() const
{
    return impl_->rightLevel_;
}

void AudioMixerMasterBus::updateLevels(float left, float right)
{
    impl_->leftLevel_ = left;
    impl_->rightLevel_ = right;
    Q_EMIT levelChanged(left, right);
}

class AudioMixer::Impl
{
public:
    std::map<LayerID, std::unique_ptr<AudioMixerChannelStrip>> channelStrips_;
    std::unique_ptr<AudioMixerMasterBus> masterBus_;
    ArtifactCompositionPtr composition_;
    int sampleRate_ = 44100;
    int bufferSize_ = 512;

    void refreshDerivedLevels() const
    {
        float masterLeft = -60.0f;
        float masterRight = -60.0f;

        for (const auto& pair : channelStrips_) {
            auto* strip = pair.second.get();
            if (!strip) {
                continue;
            }

            const float level = volumeToMeterDb(strip->volume(), strip->isMuted());
            strip->updateLevels(level, level);
            masterLeft = std::max(masterLeft, level);
            masterRight = std::max(masterRight, level);
        }

        if (!masterBus_) {
            return;
        }

        if (masterBus_->isMuted() || channelStrips_.empty()) {
            masterBus_->updateLevels(-60.0f, -60.0f);
            return;
        }

        const float masterGain = volumeToMeterDb(masterBus_->volume(), false);
        masterBus_->updateLevels(
            std::clamp(masterLeft + masterGain, -60.0f, 0.0f),
            std::clamp(masterRight + masterGain, -60.0f, 0.0f));
    }

    void refreshPlaybackLevels(float leftRms, float rightRms) const
    {
        // Distribute real-time levels to channel strips proportionally
        for (const auto& pair : channelStrips_) {
            auto* strip = pair.second.get();
            if (!strip) continue;
            if (strip->isMuted()) {
                strip->updateLevels(-60.0f, -60.0f);
                continue;
            }
            const float gain = volumeToMeterDb(strip->volume(), false);
            strip->updateLevels(
                std::clamp(leftRms + gain, -60.0f, 0.0f),
                std::clamp(rightRms + gain, -60.0f, 0.0f));
        }

        if (!masterBus_) {
            return;
        }
        if (masterBus_->isMuted()) {
            masterBus_->updateLevels(-60.0f, -60.0f);
            return;
        }
        masterBus_->updateLevels(leftRms, rightRms);
    }
};

AudioMixer::AudioMixer(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
    impl_->masterBus_ = std::make_unique<AudioMixerMasterBus>(this);
    QObject::connect(impl_->masterBus_.get(), &AudioMixerMasterBus::volumeChanged, this,
        [this](const float) {
            impl_->refreshDerivedLevels();
        });
    QObject::connect(impl_->masterBus_.get(), &AudioMixerMasterBus::muteChanged, this,
        [this](const bool) {
            impl_->refreshDerivedLevels();
        });
}

AudioMixer::~AudioMixer()
{
    delete impl_;
}

AudioMixerChannelStrip* AudioMixer::addChannelStrip(LayerID layerId)
{
    auto it = impl_->channelStrips_.find(layerId);
    if (it != impl_->channelStrips_.end()) {
        return it->second.get();
    }

    auto strip = std::make_unique<AudioMixerChannelStrip>(this);
    strip->setLayerId(layerId);
    AudioMixerChannelStrip* ptr = strip.get();
    impl_->channelStrips_[layerId] = std::move(strip);
    Q_EMIT channelStripAdded(layerId);
    return ptr;
}

void AudioMixer::removeChannelStrip(LayerID layerId)
{
    if (impl_->channelStrips_.erase(layerId) > 0) {
        Q_EMIT channelStripRemoved(layerId);
    }
}

AudioMixerChannelStrip* AudioMixer::getChannelStrip(LayerID layerId)
{
    auto it = impl_->channelStrips_.find(layerId);
    return it != impl_->channelStrips_.end() ? it->second.get() : nullptr;
}

QVector<AudioMixerChannelStrip*> AudioMixer::allChannelStrips() const
{
    QVector<AudioMixerChannelStrip*> result;
    for (const auto& pair : impl_->channelStrips_) {
        result.append(pair.second.get());
    }
    return result;
}

int AudioMixer::channelCount() const
{
    return static_cast<int>(impl_->channelStrips_.size());
}

AudioMixerMasterBus* AudioMixer::masterBus()
{
    return impl_->masterBus_.get();
}

const AudioMixerMasterBus* AudioMixer::masterBus() const
{
    return impl_->masterBus_.get();
}

void AudioMixer::updatePlaybackLevels(float leftRms, float rightRms)
{
    impl_->refreshPlaybackLevels(leftRms, rightRms);
}

void AudioMixer::clearChannelStrips()
{
    std::vector<LayerID> removedIds;
    removedIds.reserve(impl_->channelStrips_.size());
    for (const auto& pair : impl_->channelStrips_) {
        removedIds.push_back(pair.first);
    }
    impl_->channelStrips_.clear();
    for (const auto& id : removedIds) {
        Q_EMIT channelStripRemoved(id);
    }
}

void AudioMixer::syncFromComposition(ArtifactCompositionPtr composition)
{
    impl_->composition_ = composition;
    clearChannelStrips();

    if (!composition) {
        impl_->refreshDerivedLevels();
        return;
    }

    for (const auto& layer : composition->allLayer()) {
        if (!layer || !supportsMixerLayer(layer)) {
            continue;
        }

        auto* strip = addChannelStrip(layer->id());
        {
            const QSignalBlocker blocker(strip);
            const QString layerName = layer->layerName().trimmed().isEmpty()
                ? QStringLiteral("Audio Layer")
                : layer->layerName();
            strip->setLayerName(layerName);
            strip->setStereoLinked(true);
            strip->setPan(0.0f);
            strip->setVolume(readLayerVolume(layer));
            strip->setMuted(readLayerMuted(layer));
            strip->setSolo(layer->isSolo());
        }

        QObject::connect(strip, &AudioMixerChannelStrip::volumeChanged, this,
            [this, layer](const float volume) {
                applyLayerVolume(layer, volume);
                impl_->refreshDerivedLevels();
            });
        QObject::connect(strip, &AudioMixerChannelStrip::muteChanged, this,
            [this, layer](const bool muted) {
                applyLayerMuted(layer, muted);
                impl_->refreshDerivedLevels();
            });
        QObject::connect(strip, &AudioMixerChannelStrip::soloChanged, this,
            [this, layer](const bool solo) {
                applyLayerSolo(layer, solo);
                impl_->refreshDerivedLevels();
            });
    }

    impl_->refreshDerivedLevels();
}

ArtifactCompositionPtr AudioMixer::composition() const
{
    return impl_->composition_;
}

bool AudioMixer::hasAnySolo() const
{
    for (const auto& pair : impl_->channelStrips_) {
        if (pair.second->isSolo()) {
            return true;
        }
    }
    return false;
}

void AudioMixer::updateSoloStates()
{
    const bool anySolo = hasAnySolo();
    for (const auto& pair : impl_->channelStrips_) {
        if (anySolo) {
            pair.second->setMuted(!pair.second->isSolo());
        }
    }
}

void AudioMixer::setAllMuted(bool muted)
{
    for (const auto& pair : impl_->channelStrips_) {
        pair.second->setMuted(muted);
    }
    impl_->masterBus_->setMuted(muted);
    impl_->refreshDerivedLevels();
}

void AudioMixer::resetAllPeaks()
{
    for (const auto& pair : impl_->channelStrips_) {
        pair.second->resetPeak();
    }
}

void AudioMixer::setSampleRate(int rate)
{
    impl_->sampleRate_ = rate;
}

int AudioMixer::sampleRate() const
{
    return impl_->sampleRate_;
}

void AudioMixer::setBufferSize(int size)
{
    impl_->bufferSize_ = size;
}

int AudioMixer::bufferSize() const
{
    return impl_->bufferSize_;
}

} // namespace Artifact
