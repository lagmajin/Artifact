module;
#include <utility>
#include <memory>
#include <string>

#include <wobjectdefs.h>
#include <QWidget>
#include <QElapsedTimer>
export module Artifact.Widgets.AudioMixer;


import Audio.Mixer;
import Audio.Bus;
import Audio.Analyze;
import Artifact.Widgets.SpectrumAnalyzer;

export namespace Artifact {

/**
 * @brief エフェクトスロットのウィジェット
 */
class AudioEffectSlotWidget : public QWidget {
    W_OBJECT(AudioEffectSlotWidget)
public:
    AudioEffectSlotWidget(std::shared_ptr<ArtifactCore::AudioBus> bus, int slotIndex, QWidget* parent = nullptr);
    virtual ~AudioEffectSlotWidget();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    std::shared_ptr<ArtifactCore::AudioBus> bus_;
    int slotIndex_;
};

/**
 * @brief 個別のオーディオ・チャンネルストリップ・ウィジェット
 * フェーダー、メーター、パン、Mute/Solo を備えます。
 */
class AudioChannelStripWidget : public QWidget {
	W_OBJECT(AudioChannelStripWidget)
public:
    AudioChannelStripWidget(std::shared_ptr<ArtifactCore::AudioBus> bus, QWidget* parent = nullptr);
    virtual ~AudioChannelStripWidget();

    void updateMeters(); // 定期的に呼び出してレベル表示を更新

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::shared_ptr<ArtifactCore::AudioBus> bus_;
    SpectrumAnalyzerWidget* analyzerWidget_;
    std::unique_ptr<ArtifactCore::AudioAnalyzer> analyzer_;
    QElapsedTimer clipTimer_;
    qint64 clipLatchedUntilMs_ = 0;
    float clipPeak_ = 0.0f;
};

/**
 * @brief デスク全体を俯瞰するオーディオミキサー・ウィジェット
 * 各バス（チャンネルストリップ）を横に並べて管理します。
 */
class AudioMixerWidget : public QWidget {
    W_OBJECT(AudioMixerWidget)
public:
    AudioMixerWidget(ArtifactCore::AudioMixer* mixer, QWidget* parent = nullptr);
    virtual ~AudioMixerWidget();

    void refreshBuses(); // バスの増減を反映
    void updateAllMeters();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    ArtifactCore::AudioMixer* mixer_;
    std::vector<AudioChannelStripWidget*> strips_;
    QTimer* meterTimer_ = nullptr;
};

} // namespace Artifact
