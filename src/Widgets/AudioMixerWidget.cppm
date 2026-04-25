module;
#include <utility>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QPainter>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QMouseEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <cmath>
#include <wobjectimpl.h>

module Artifact.Widgets.AudioMixer;

import Audio.Bus;
import Audio.Mixer;
import Artifact.VST.Effect;
import Artifact.VST.Host;

namespace Artifact {
W_OBJECT_IMPL(AudioEffectSlotWidget)
W_OBJECT_IMPL(AudioChannelStripWidget)
W_OBJECT_IMPL(AudioMixerWidget)

// ============================================================================
// AudioEffectSlotWidget
// ============================================================================

AudioEffectSlotWidget::AudioEffectSlotWidget(std::shared_ptr<ArtifactCore::AudioBus> bus, int slotIndex, QWidget* parent)
    : QWidget(parent), bus_(bus), slotIndex_(slotIndex) {
    
    setFixedHeight(24);
    setCursor(Qt::PointingHandCursor);
}

AudioEffectSlotWidget::~AudioEffectSlotWidget() = default;

void AudioEffectSlotWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    bool hasEffect = (bus_ && slotIndex_ < bus_->getEffectCount());
    auto effect = hasEffect ? bus_->getEffect(slotIndex_) : nullptr;

    // Background
    painter.fillRect(rect(), QColor(30, 30, 30));
    painter.setPen(QPen(QColor(60, 60, 60), 1));
    painter.drawRect(rect().adjusted(0,0,-1,-1));

    // Text
    painter.setPen(hasEffect ? QColor(0, 200, 255) : QColor(100, 100, 100));
    QString text = hasEffect ? QString::fromStdString(effect->getName()) : "--- Empty ---";
    
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);
    painter.drawText(rect().adjusted(5, 0, -5, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
}

void AudioEffectSlotWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QMenu menu(this);
        
        bool hasEffect = (bus_ && slotIndex_ < bus_->getEffectCount());

        if (!hasEffect) {
            menu.addAction("Insert VST Effect...", [this]() {
                // ここではデバッグ用に簡易的な VSTEffect を追加するロジック
                auto vst = std::make_shared<Artifact::VSTEffect>();
                // 本来はファイルダイアログ等で path を選択
                bus_->addEffect(vst);
                update();
            });
        } else {
            auto effect = bus_->getEffect(slotIndex_);
            menu.addAction(effect->isBypassed() ? "Enable" : "Bypass", [this, effect]() {
                effect->setBypass(!effect->isBypassed());
                update();
            });
            menu.addAction("Remove", [this]() {
                bus_->removeEffect(slotIndex_);
                update();
            });
            if (auto vst = std::dynamic_pointer_cast<Artifact::VSTEffect>(effect)) {
                menu.addAction("Open Editor", [this, vst]() {
                    vst->openEditor(nullptr); // 実装に応じて親ウィンドウを渡す
                });
            }
        }
        
        menu.exec(mapToGlobal(event->pos()));
    }
}

// ============================================================================
// AudioChannelStripWidget
// ============================================================================

AudioChannelStripWidget::AudioChannelStripWidget(std::shared_ptr<ArtifactCore::AudioBus> bus, QWidget* parent)
    : QWidget(parent), bus_(bus) {
    
    analyzer_ = std::make_unique<ArtifactCore::AudioAnalyzer>(1024);
    clipTimer_.start();
    
    setFixedWidth(80);
    setMinimumHeight(350);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 10);
    layout->setSpacing(8);

    // 0. Spectrum Analyzer at Top
    analyzerWidget_ = new SpectrumAnalyzerWidget(this);
    layout->addWidget(analyzerWidget_);

    // 0.5. FX Slots (FX Rack)
    QVBoxLayout* fxLayout = new QVBoxLayout();
    fxLayout->setSpacing(2);
    for (int i = 0; i < 4; ++i) {
        fxLayout->addWidget(new AudioEffectSlotWidget(bus, i, this));
    }
    layout->addLayout(fxLayout);

    // 1. Bus Name Label
    QString busName = QString::fromStdString(bus->getName());
    QLabel* nameLabel = new QLabel(busName, this);
    nameLabel->setAlignment(Qt::AlignCenter);
    QFont nameFont = nameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(11);
    nameLabel->setFont(nameFont);
    QPalette namePalette = nameLabel->palette();
    namePalette.setColor(QPalette::WindowText, QColor(224, 224, 224));
    nameLabel->setPalette(namePalette);
    layout->addWidget(nameLabel);

    // 2. Pan Pot (簡易的に水平スライダー)
    QSlider* panSlider = new QSlider(Qt::Horizontal, this);
    panSlider->setRange(-100, 100);
    panSlider->setValue(static_cast<int>(bus->getPan() * 100));
    connect(panSlider, &QSlider::valueChanged, this, [this](int val) {
        bus_->setPan(val / 100.0f);
    });
    layout->addWidget(panSlider);

    // 3. Fader & Meter Area (水平に並べる)
    QHBoxLayout* faderArea = new QHBoxLayout();
    
    // Volume Fader
    QSlider* fader = new QSlider(Qt::Vertical, this);
    fader->setRange(-600, 120); // -60dB to +12dB (0.1dB step)
    fader->setValue(static_cast<int>(bus->getVolume() * 10));
    connect(fader, &QSlider::valueChanged, this, [this](int val) {
        bus_->setVolume(val / 10.0f);
    });
    faderArea->addWidget(fader);
    
    layout->addLayout(faderArea);

    // 4. Mute / Solo Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* muteBtn = new QPushButton("M", this);
    muteBtn->setCheckable(true);
    muteBtn->setFixedSize(28, 28);
    muteBtn->setFlat(true);
    connect(muteBtn, &QPushButton::toggled, this, [this](bool checked) {
        bus_->setMute(checked);
    });

    QPushButton* soloBtn = new QPushButton("S", this);
    soloBtn->setCheckable(true);
    soloBtn->setFixedSize(28, 28);
    soloBtn->setFlat(true);
    connect(soloBtn, &QPushButton::toggled, this, [this](bool checked) {
        bus_->setSolo(checked);
    });

    btnLayout->addWidget(muteBtn);
    btnLayout->addWidget(soloBtn);
    layout->addLayout(btnLayout);

    // レンダリングスタイル
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    QPalette stripPalette = palette();
    stripPalette.setColor(QPalette::Window, QColor(26, 27, 30));
    setPalette(stripPalette);
}

AudioChannelStripWidget::~AudioChannelStripWidget() = default;

void AudioChannelStripWidget::updateMeters() {
    // 実際に解析を行う
    if (analyzer_ && bus_) {
        auto result = analyzer_->analyze(bus_->getOutputBuffer());
        analyzerWidget_->setSpectrum(result.spectrum);
    }

    if (bus_) {
        const float peak = std::max(bus_->getPeakLevel(0), bus_->getPeakLevel(1));
        clipPeak_ = peak;
        if (peak >= 1.0f) {
            clipLatchedUntilMs_ = clipTimer_.elapsed() + 1200;
        }
    }
    
    update(); // 強制再描画
}

void AudioChannelStripWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // レベルメーターの描画 (Volume Faderの横に配置したい)
    // 描画座標を計算
    int meterX = width() - 15;
    int meterY = 60;
    int meterH = height() - 120;
    int meterW = 6;

    // BG
    painter.fillRect(meterX, meterY, meterW, meterH, QColor(30,30,30));

    // Get levels (L/R 平均または最大)
    float peak = std::max(bus_->getPeakLevel(0), bus_->getPeakLevel(1));
    float rms = std::max(bus_->getRMSLevel(0), bus_->getRMSLevel(1));
    const bool clipped = clipPeak_ >= 1.0f || clipTimer_.elapsed() < clipLatchedUntilMs_;

    // Convert to 0-1 range for display (Logarithmic)
    auto toDbfs = [](float val) {
        if (val <= 0.0001f) return -60.0f;
        return 20.0f * std::log10(val);
    };

    float peakDbfs = toDbfs(peak);
    float peakNorm = std::clamp((peakDbfs + 60.0f) / 60.0f, 0.0f, 1.0f);
    
    // グラデーションで塗りつぶし
    QLinearGradient grad(0, meterY + meterH, 0, meterY);
    grad.setColorAt(0.0, QColor(0, 200, 0));  // Green
    grad.setColorAt(0.7, QColor(200, 200, 0)); // Yellow
    grad.setColorAt(1.0, QColor(255, 0, 0));    // Red

    int fillH = static_cast<int>(peakNorm * meterH);
    painter.fillRect(meterX, meterY + meterH - fillH, meterW, fillH, grad);

    if (clipped) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(232, 54, 54));
        painter.drawRoundedRect(QRect(meterX - 34, meterY - 24, 42, 18), 6, 6);
        painter.setPen(QColor(255, 245, 245));
        QFont clipFont = painter.font();
        clipFont.setBold(true);
        clipFont.setPixelSize(9);
        painter.setFont(clipFont);
        painter.drawText(QRect(meterX - 34, meterY - 24, 42, 18), Qt::AlignCenter, QStringLiteral("CLIP"));
    }

    // 目盛り (dBFS scale)
    static const struct { float db; bool showLabel; } kMarks[] = {
        {  0.0f, true  },
        { -3.0f, false },
        { -6.0f, true  },
        {-12.0f, true  },
        {-18.0f, false },
        {-24.0f, true  },
        {-36.0f, false },
        {-48.0f, true  },
    };
    {
        QFont scaleFont;
        scaleFont.setFamily(QStringLiteral("Consolas"));
        scaleFont.setPixelSize(8);
        painter.setFont(scaleFont);

        for (const auto& mark : kMarks) {
            const float norm = (mark.db + 60.0f) / 60.0f;
            const int tickY = meterY + meterH - static_cast<int>(norm * meterH);

            // メーターバーを横断するティックライン (半透明白)
            painter.setPen(QPen(QColor(255, 255, 255, 65), 1));
            painter.drawLine(meterX, tickY, meterX + meterW, tickY);

            if (mark.showLabel) {
                const QString label = (mark.db == 0.0f)
                    ? QStringLiteral("0")
                    : QString::number(static_cast<int>(mark.db));
                painter.setPen(QColor(120, 140, 158));
                painter.drawText(QRect(meterX - 22, tickY - 4, 20, 9),
                                 Qt::AlignRight | Qt::AlignVCenter, label);
            }
        }
    }

    // 5. Gain Reduction Meter (Orange bar, top-down)
    float gr = bus_->getGainReduction(); // 0.0 ~ 1.0 (1.0 = no reduction)
    if (gr < 0.99f) {
        int grX = meterX - 8;
        int grFillH = static_cast<int>((1.0f - gr) * meterH);
        painter.fillRect(grX, meterY, 4, grFillH, QColor(255, 128, 0));
    }
}

// ============================================================================
// AudioMixerWidget
// ============================================================================

AudioMixerWidget::AudioMixerWidget(ArtifactCore::AudioMixer* mixer, QWidget* parent)
    : QWidget(parent), mixer_(mixer) {
    
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignLeft);

    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    QPalette mixerPalette = palette();
    mixerPalette.setColor(QPalette::Window, QColor(15, 16, 18));
    setPalette(mixerPalette);

    // メーター定期更新タイマー（表示中のみ稼働）
    meterTimer_ = new QTimer(this);
    connect(meterTimer_, &QTimer::timeout, this, &AudioMixerWidget::updateAllMeters);
    meterTimer_->start(33); // 30fps

    refreshBuses();
}

AudioMixerWidget::~AudioMixerWidget() = default;

void AudioMixerWidget::refreshBuses() {
    // 既存のストリップを削除
    for (auto* strip : strips_) {
        strip->deleteLater();
    }
    strips_.clear();

    // マスターバスを一番左に
    auto master = mixer_->getMasterBus();
    if (master) {
        auto* strip = new AudioChannelStripWidget(master, this);
        strip->setFixedWidth(100); // Masterは少し太めに
        layout()->addWidget(strip);
        strips_.push_back(strip);
    }

    // 他のバスをリストアップ (本来はMixerから取得)
    // 今回は簡易的にMaster以外の管理は省略
}

void AudioMixerWidget::updateAllMeters() {
    for (auto* strip : strips_) {
        strip->updateMeters();
    }
}

void AudioMixerWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (meterTimer_ && !meterTimer_->isActive()) {
        meterTimer_->start(33);
    }
}

void AudioMixerWidget::hideEvent(QHideEvent* event)
{
    if (meterTimer_) {
        meterTimer_->stop();
    }
    QWidget::hideEvent(event);
}

} // namespace Artifact
