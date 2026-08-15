module;
#include <utility>
#include <memory>
#include <string>
#include <functional>

#include <wobjectdefs.h>
#include <QWidget>
#include <QElapsedTimer>
#include <QComboBox>
export module Artifact.Widgets.AudioMixer;


import Audio.Mixer;
import Audio.Bus;
import Audio.Analyze;
import Audio.Effect.Spectrum;
import Memory.SharedPtr;
import Artifact.Widgets.SpectrumAnalyzer;

class QLabel;

export namespace Artifact {

/**
 * @brief エフェクトスロットのウィジェット
 */
class AudioEffectSlotWidget : public QWidget {
    W_OBJECT(AudioEffectSlotWidget)
public:
    AudioEffectSlotWidget(ArtifactCore::SharedPtr<ArtifactCore::AudioBus> bus, int slotIndex, QWidget* parent = nullptr);
    virtual ~AudioEffectSlotWidget();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    ArtifactCore::SharedPtr<ArtifactCore::AudioBus> bus_;
    int slotIndex_;
};

/**
 * @brief 個別のオーディオ・チャンネルストリップ・ウィジェット
 * フェーダー、メーター、パン、Mute/Solo を備えます。
 */
class AudioChannelStripWidget : public QWidget {
	W_OBJECT(AudioChannelStripWidget)
public:
    AudioChannelStripWidget(ArtifactCore::SharedPtr<ArtifactCore::AudioBus> bus,
                            ArtifactCore::AudioMixer* mixer = nullptr,
                            std::function<void()> onChanged = {},
                            QWidget* parent = nullptr);
    virtual ~AudioChannelStripWidget();

    void updateMeters(); // 定期的に呼び出してレベル表示を更新

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    ArtifactCore::SharedPtr<ArtifactCore::AudioBus> bus_;
    ArtifactCore::AudioMixer* mixer_ = nullptr;
    std::function<void()> onChanged_;
    QComboBox* routeCombo_ = nullptr;
    SpectrumAnalyzerWidget* analyzerWidget_;
    std::unique_ptr<ArtifactCore::AudioAnalyzer> analyzer_;
    std::unique_ptr<ArtifactCore::AudioSpectrum> loudnessAnalyzer_;
    QLabel* loudnessLabel_ = nullptr;
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
    QWidget* addBusButton_ = nullptr;
    QTimer* meterTimer_ = nullptr;
};

} // namespace Artifact
