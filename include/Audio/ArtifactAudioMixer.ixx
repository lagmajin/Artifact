module;

#include <QString>
#include <QStringList>
#include <QVector>
#include <QObject>
#include <wobjectdefs.h>
export module Artifact.Audio.Mixer;

import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Utils.Id;
import std;

export namespace Artifact
{
    using namespace ArtifactCore;

// オーディオミキサー用のチャンネルストリップ
class AudioMixerChannelStrip : public QObject
{
    W_OBJECT(AudioMixerChannelStrip)
public:
    using LayerID = ArtifactCore::LayerID;

    explicit AudioMixerChannelStrip(QObject* parent = nullptr);
    ~AudioMixerChannelStrip();

    // レイヤー関連
    void setLayerId(LayerID id);
    LayerID layerId() const;

    void setLayerName(const QString& name);
    QString layerName() const;

    // オーディオパラメータ
    void setVolume(float volume);  // 0.0 - 2.0 (1.0 = 0dB)
    float volume() const;

    void setPan(float pan);  // -1.0 (L) to 1.0 (R)
    float pan() const;

    void setMuted(bool muted);
    bool isMuted() const;

    void setSolo(bool solo);
    bool isSolo() const;

    void setStereoLinked(bool linked);
    bool isStereoLinked() const;

    // エフェクトチェーン
    void addEffect(const QString& effectId);
    void removeEffect(int index);
    void moveEffect(int from, int to);
    QStringList effectChain() const;

    // メータリング用
    float leftLevel() const;   // -60dB to 0dB
    float rightLevel() const;
    float peakLeft() const;
    float peakRight() const;
    void updateLevels(float left, float right);

    void resetPeak();


    void volumeChanged(float volume) W_SIGNAL(volumeChanged, volume);
    void panChanged(float pan) W_SIGNAL(panChanged, pan);
    void muteChanged(bool muted) W_SIGNAL(muteChanged, muted);
    void soloChanged(bool solo) W_SIGNAL(soloChanged, solo);
    void levelChanged(float left, float right) W_SIGNAL(levelChanged, left, right);

private:
    class Impl;
    Impl* impl_;
};

// マスターチャンネル
class AudioMixerMasterBus : public QObject
{
    W_OBJECT(AudioMixerMasterBus)
public:
    explicit AudioMixerMasterBus(QObject* parent = nullptr);
    ~AudioMixerMasterBus();

    void setVolume(float volume);
    float volume() const;

    void setMuted(bool muted);
    bool isMuted() const;

    float leftLevel() const;
    float rightLevel() const;
    void updateLevels(float left, float right);


    void volumeChanged(float volume) W_SIGNAL(volumeChanged, volume);
    void muteChanged(bool muted) W_SIGNAL(muteChanged, muted);
    void levelChanged(float left, float right) W_SIGNAL(levelChanged, left, right);

private:
    class Impl;
    Impl* impl_;
};

// オーディオミキサー本体
class AudioMixer : public QObject
{
    W_OBJECT(AudioMixer)
public:
    using LayerID = AudioMixerChannelStrip::LayerID;

    explicit AudioMixer(QObject* parent = nullptr);
    ~AudioMixer();

    // チャンネルストリップ管理
    AudioMixerChannelStrip* addChannelStrip(LayerID layerId);
    void removeChannelStrip(LayerID layerId);
    AudioMixerChannelStrip* getChannelStrip(LayerID layerId);
    QVector<AudioMixerChannelStrip*> allChannelStrips() const;
    int channelCount() const;

    // マスターバス
    AudioMixerMasterBus* masterBus();
    const AudioMixerMasterBus* masterBus() const;

    void clearChannelStrips();
    void syncFromComposition(ArtifactCompositionPtr composition);
    ArtifactCompositionPtr composition() const;

    // Solo管理（複数のsolo状態管理）
    bool hasAnySolo() const;
    void updateSoloStates();

    // ミキサー全体のコントロール
    void setAllMuted(bool muted);
    void resetAllPeaks();

    // オーディオエンジンのサンプルレート
    void setSampleRate(int rate);
    int sampleRate() const;

    void setBufferSize(int size);
    int bufferSize() const;


    void channelStripAdded(LayerID layerId) W_SIGNAL(channelStripAdded, layerId);
    void channelStripRemoved(LayerID layerId) W_SIGNAL(channelStripRemoved, layerId);

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
