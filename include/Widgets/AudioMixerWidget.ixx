module;
#include <QWidget>
#include <memory>
#include <string>

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
    Q_OBJECT
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
    Q_OBJECT
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
};

/**
 * @brief デスク全体を俯瞰するオーディオミキサー・ウィジェット
 * 各バス（チャンネルストリップ）を横に並べて管理します。
 */
class AudioMixerWidget : public QWidget {
    Q_OBJECT
public:
    AudioMixerWidget(ArtifactCore::AudioMixer* mixer, QWidget* parent = nullptr);
    virtual ~AudioMixerWidget();

    void refreshBuses(); // バスの増減を反映
    void updateAllMeters();

private:
    ArtifactCore::AudioMixer* mixer_;
    std::vector<AudioChannelStripWidget*> strips_;
};

} // namespace Artifact
