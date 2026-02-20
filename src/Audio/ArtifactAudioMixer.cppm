module;

#include <QString>
#include <QVector>
#include <QObject>
#include <QHash>
#include <QDebug>

module Artifact.Audio.Mixer;

import std;

namespace Artifact
{

// ==================== AudioMixerChannelStrip::Impl ====================

class AudioMixerChannelStrip::Impl
{
public:
    LayerID layerId_ = 0;
    QString layerName_;

    float volume_ = 1.0f;      // 1.0 = 0dB
    float pan_ = 0.0f;         // -1.0 to 1.0
    bool muted_ = false;
    bool solo_ = false;
    bool stereoLinked_ = true;

    QStringList effectChain_;

    // メータリング
    float leftLevel_ = -60.0f;   // dB
    float rightLevel_ = -60.0f;
    float peakLeft_ = -60.0f;
    float peakRight_ = -60.0f;
};

// ==================== AudioMixerChannelStrip ====================

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
    impl_->volume_ = std::max(0.0f, std::min(2.0f, volume));
    Q_EMIT volumeChanged(impl_->volume_);
}

float AudioMixerChannelStrip::volume() const
{
    return impl_->volume_;
}

void AudioMixerChannelStrip::setPan(float pan)
{
    impl_->pan_ = std::max(-1.0f, std::min(1.0f, pan));
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

    // ピーク更新
    if (left > impl_->peakLeft_) impl_->peakLeft_ = left;
    if (right > impl_->peakRight_) impl_->peakRight_ = right;

    Q_EMIT levelChanged(left, right);
}

void AudioMixerChannelStrip::resetPeak()
{
    impl_->peakLeft_ = -60.0f;
    impl_->peakRight_ = -60.0f;
}

// ==================== AudioMixerMasterBus::Impl ====================

class AudioMixerMasterBus::Impl
{
public:
    float volume_ = 1.0f;
    bool muted_ = false;
    float leftLevel_ = -60.0f;
    float rightLevel_ = -60.0f;
};

// ==================== AudioMixerMasterBus ====================

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
    impl_->volume_ = std::max(0.0f, std::min(2.0f, volume));
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

// ==================== AudioMixer::Impl ====================

class AudioMixer::Impl
{
public:
    QHash<LayerID, std::unique_ptr<AudioMixerChannelStrip>> channelStrips_;
    std::unique_ptr<AudioMixerMasterBus> masterBus_;

    int sampleRate_ = 44100;
    int bufferSize_ = 512;
};

// ==================== AudioMixer ====================

AudioMixer::AudioMixer(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
    impl_->masterBus_ = std::make_unique<AudioMixerMasterBus>(this);
}

AudioMixer::~AudioMixer()
{
    delete impl_;
}

AudioMixerChannelStrip* AudioMixer::addChannelStrip(LayerID layerId)
{
    if (impl_->channelStrips_.contains(layerId)) {
        return impl_->channelStrips_[layerId].get();
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
    if (impl_->channelStrips_.contains(layerId)) {
        impl_->channelStrips_.remove(layerId);
        Q_EMIT channelStripRemoved(layerId);
    }
}

AudioMixerChannelStrip* AudioMixer::getChannelStrip(LayerID layerId)
{
    auto it = impl_->channelStrips_.find(layerId);
    return (it != impl_->channelStrips_.end()) ? it->get() : nullptr;
}

QVector<AudioMixerChannelStrip*> AudioMixer::allChannelStrips() const
{
    QVector<AudioMixerChannelStrip*> result;
    for (auto& [id, strip] : impl_->channelStrips_) {
        result.append(strip.get());
    }
    return result;
}

int AudioMixer::channelCount() const
{
    return impl_->channelStrips_.size();
}

AudioMixerMasterBus* AudioMixer::masterBus()
{
    return impl_->masterBus_.get();
}

const AudioMixerMasterBus* AudioMixer::masterBus() const
{
    return impl_->masterBus_.get();
}

bool AudioMixer::hasAnySolo() const
{
    for (const auto& [id, strip] : impl_->channelStrips_) {
        if (strip->isSolo()) return true;
    }
    return false;
}

void AudioMixer::updateSoloStates()
{
    bool anySolo = hasAnySolo();
    for (auto& [id, strip] : impl_->channelStrips_) {
        if (anySolo) {
            // Solo状態がある場合、solo以外のチャンネルをミュート
            strip->setMuted(!strip->isSolo());
        } else {
            // Soloがない場合、通常のミュート状態にリセット
            // （実際の実装では元の状态を保存が必要）
        }
    }
}

void AudioMixer::setAllMuted(bool muted)
{
    for (auto& [id, strip] : impl_->channelStrips_) {
        strip->setMuted(muted);
    }
    impl_->masterBus_->setMuted(muted);
}

void AudioMixer::resetAllPeaks()
{
    for (auto& [id, strip] : impl_->channelStrips_) {
        strip->resetPeak();
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
