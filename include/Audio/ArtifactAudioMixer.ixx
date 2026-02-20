module;

#include <QString>
#include <QVector>
#include <QObject>

export module Artifact.Audio.Mixer;

import std;
import Artifact.Layer.Abstract;

export namespace Artifact
{

// オーディオミキサー用のチャンネルストリップ
class AudioMixerChannelStrip : public QObject
{
    Q_OBJECT
public:
    using LayerID = uint64_t;

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

Q_SIGNALS:
    void volumeChanged(float volume);
    void panChanged(float pan);
    void muteChanged(bool muted);
    void soloChanged(bool solo);
    void levelChanged(float left, float right);

private:
    class Impl;
    Impl* impl_;
};

// マスターチャンネル
class AudioMixerMasterBus : public QObject
{
    Q_OBJECT
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

Q_SIGNALS:
    void volumeChanged(float volume);
    void muteChanged(bool muted);
    void levelChanged(float left, float right);

private:
    class Impl;
    Impl* impl_;
};

// オーディオミキサー本体
class AudioMixer : public QObject
{
    Q_OBJECT
public:
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

Q_SIGNALS:
    void channelStripAdded(LayerID layerId);
    void channelStripRemoved(LayerID layerId);

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
