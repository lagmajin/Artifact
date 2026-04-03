module;
#include <wobjectimpl.h>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
#include <QIcon>
#include <QFileInfo>
#include <QDebug>
#include <QKeySequence>
#include <QStringList>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>

module Artifact.Widgets.PlaybackControlWidget;

import Utils;
import Icon.SvgToIcon;
import Frame.Range;
import Artifact.Composition.InOutPoints;
import Widgets.Utils.CSS;
import Artifact.Application.Manager;
import Artifact.Service.Playback;
import Artifact.Service.ActiveContext;
import Artifact.Composition.PlaybackController;

namespace {
void applyThemeTextPalette(QWidget* widget, const QColor& color, int shade = 100)
{
    if (!widget) {
        return;
    }
    QPalette pal = widget->palette();
    pal.setColor(QPalette::WindowText, color.darker(shade));
    pal.setColor(QPalette::Text, color.darker(shade));
    widget->setPalette(pal);
}

QIcon loadIconWithFallback(const QString& fileName)
{
  const QString resourcePath = ArtifactCore::resolveIconResourcePath(fileName);
  {
    const QIcon icon = ArtifactCore::svgToQIcon(resourcePath, QSize(24, 24));
    if (!icon.isNull()) {
      return icon;
    }
  }

  const QString filePath = ArtifactCore::resolveIconPath(fileName);
  {
    const QIcon fileIcon = ArtifactCore::svgToQIcon(filePath, QSize(24, 24));
    if (!fileIcon.isNull()) {
      return fileIcon;
    }
  }

  qWarning().noquote() << "[ArtifactPlaybackControlWidget] icon load failed:"
                       << "resource=" << resourcePath
                       << "exists=" << QFileInfo::exists(resourcePath)
                       << "file=" << filePath
                       << "exists=" << QFileInfo::exists(filePath);
  return QIcon();
}

QIcon loadIconWithFallback(const QStringList& fileNames)
{
  for (const auto& fileName : fileNames) {
    const QIcon icon = loadIconWithFallback(fileName);
    if (!icon.isNull()) {
      return icon;
    }
  }
  return QIcon();
}

QString formatFrameCount(qint64 frame)
{
    return QStringLiteral("F%1").arg(frame);
}

QString formatTimecode(qint64 frame, float fps)
{
    const int safeFps = std::max(1, static_cast<int>(std::lround(std::max(0.001f, fps))));
    const qint64 totalSeconds = frame / safeFps;
    const int ff = static_cast<int>(frame % safeFps);
    const int ss = static_cast<int>(totalSeconds % 60);
    const int mm = static_cast<int>((totalSeconds / 60) % 60);
    const int hh = static_cast<int>(totalSeconds / 3600);
    return QStringLiteral("%1:%2:%3:%4")
        .arg(hh, 2, 10, QChar('0'))
        .arg(mm, 2, 10, QChar('0'))
        .arg(ss, 2, 10, QChar('0'))
        .arg(ff, 2, 10, QChar('0'));
}

QString formatSpeedLabel(float speed)
{
    return QStringLiteral("x%1").arg(speed >= 0.0f ? speed : -speed, 0, 'f', speed >= 1.0f ? 1 : 2);
}
}

namespace Artifact
{
using namespace ArtifactCore;

using PlaybackState = ::Artifact::PlaybackState;

// ============================================================================
// ArtifactPlaybackControlWidget::Impl
// ============================================================================

class ArtifactPlaybackControlWidget::Impl {
public:
    ArtifactPlaybackControlWidget* owner_;
    
    // Buttons
    QToolButton* playButton_ = nullptr;
    QToolButton* pauseButton_ = nullptr;
    QToolButton* stopButton_ = nullptr;
    QToolButton* stepBackwardButton_ = nullptr;
    QToolButton* stepForwardButton_ = nullptr;
    QToolButton* seekStartButton_ = nullptr;
    QToolButton* seekEndButton_ = nullptr;
    QToolButton* seekPreviousButton_ = nullptr;
    QToolButton* seekNextButton_ = nullptr;
    QToolButton* loopButton_ = nullptr;
    QToolButton* inButton_ = nullptr;
    QToolButton* outButton_ = nullptr;
    QToolButton* clearInOutButton_ = nullptr;
    QToolButton* speedQuarterButton_ = nullptr;
    QToolButton* speedHalfButton_ = nullptr;
    QToolButton* speedOneButton_ = nullptr;
    QSlider* scrubSlider_ = nullptr;
    QLabel* currentTimeLabel_ = nullptr;
    QLabel* rangeLabel_ = nullptr;
    
    // State
    bool isPlaying_ = false;
    bool isPaused_ = false;
    bool isStopped_ = true;
    bool isLooping_ = false;
    float playbackSpeed_ = 1.0f;
    ArtifactInOutPoints* inOutPoints_ = nullptr;
    Impl(ArtifactPlaybackControlWidget* owner)
        : owner_(owner)
    {}
    
    ~Impl() = default;
    
    void setupUI()
    {
        auto* mainLayout = new QVBoxLayout(owner_);
        mainLayout->setSpacing(6);
        mainLayout->setContentsMargins(10, 8, 10, 8);
        
        auto* transportRow = new QHBoxLayout();
        transportRow->setSpacing(6);
        
        seekStartButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/colored/E3E3E3/seek_start.svg")
        }, "先頭へ (Home)", Qt::Key_Home);
        
        stepBackwardButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/neutral/skip_previous.svg")
        }, "1フレーム戻る (←)", Qt::Key_Left);

        playButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/colored/E3E3E3/play_arrow.svg")
        }, "再生/一時停止 (Space)", Qt::Key_Space);
        playButton_->setFixedSize(44, 36); // Play is special
        playButton_->setIconSize(QSize(28, 28));

        stopButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/colored/E3E3E3/stop.svg"),
            QStringLiteral("Material/stop.svg")
        }, "停止", 0);

        stepForwardButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/neutral/skip_next.svg")
        }, "1フレーム進む (→)", Qt::Key_Right);

        seekEndButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/colored/E3E3E3/seek_end.svg")
        }, "末尾へ (End)", Qt::Key_End);
        
        inButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/neutral/push_pin.svg")
        }, "In 点設定 (I)", Qt::Key_I);
        
        outButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/neutral/remove_circle.svg")
        }, "Out 点設定 (O)", Qt::Key_O);
        clearInOutButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/neutral/clear.svg"),
            QStringLiteral("Material/clear.svg")
        }, "In/Out クリア", 0);

        loopButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/colored/E3E3E3/loop.svg")
        }, "ループ再生 (L)", Qt::Key_L);
        loopButton_->setCheckable(true);

        transportRow->addWidget(seekStartButton_);
        transportRow->addWidget(stepBackwardButton_);
        transportRow->addWidget(playButton_);
        transportRow->addWidget(stopButton_);
        transportRow->addWidget(stepForwardButton_);
        transportRow->addWidget(seekEndButton_);
        transportRow->addSpacing(8);
        transportRow->addWidget(inButton_);
        transportRow->addWidget(outButton_);
        transportRow->addWidget(clearInOutButton_);
        transportRow->addWidget(loopButton_);

        auto* metaColumn = new QVBoxLayout();
        metaColumn->setSpacing(2);
        currentTimeLabel_ = createLabel(QStringLiteral("F0 00:00:00:00 / 00:00:00:00"),
                                        QStringLiteral("現在フレーム / 総尺"));
        {
            QFont font = currentTimeLabel_->font();
            font.setPointSize(13);
            font.setWeight(QFont::DemiBold);
            currentTimeLabel_->setFont(font);
            applyThemeTextPalette(currentTimeLabel_, QColor(ArtifactCore::currentDCCTheme().textColor));
        }
        rangeLabel_ = createLabel(QStringLiteral("In --:--:--:--   Out --:--:--:--"),
                                  QStringLiteral("In / Out 範囲"));
        {
            QFont font = rangeLabel_->font();
            font.setPointSize(11);
            rangeLabel_->setFont(font);
            applyThemeTextPalette(rangeLabel_, QColor(ArtifactCore::currentDCCTheme().textColor), 125);
        }
        metaColumn->addWidget(currentTimeLabel_);
        metaColumn->addWidget(rangeLabel_);
        transportRow->addLayout(metaColumn);
        transportRow->addStretch();

        auto* speedLayout = new QHBoxLayout();
        speedLayout->setSpacing(4);
        speedQuarterButton_ = createTextToolButton(QStringLiteral("x0.25"), "再生速度 0.25x", true);
        speedHalfButton_ = createTextToolButton(QStringLiteral("x0.5"), "再生速度 0.5x", true);
        speedOneButton_ = createTextToolButton(QStringLiteral("x1.0"), "再生速度 1.0x", true);
        speedLayout->addWidget(speedQuarterButton_);
        speedLayout->addWidget(speedHalfButton_);
        speedLayout->addWidget(speedOneButton_);
        transportRow->addLayout(speedLayout);

        mainLayout->addLayout(transportRow);

        scrubSlider_ = new QSlider(Qt::Horizontal, owner_);
        scrubSlider_->setRange(0, 300);
        scrubSlider_->setTracking(true);
        scrubSlider_->setMinimumHeight(18);
        scrubSlider_->setToolTip(QStringLiteral("Current frame scrubber"));
        mainLayout->addWidget(scrubSlider_);

        connectSignals();
    }
    
    QToolButton* createToolButton(const QStringList& iconNames, const QString& tooltip, int shortcut)
    {
        auto* button = new QToolButton();
        button->setIcon(loadIconWithFallback(iconNames));
        button->setIconSize(QSize(20, 20));
        button->setToolTip(tooltip);
        button->setAutoRaise(true);
        button->setFixedSize(32, 32);
        applyThemeTextPalette(button, QColor(ArtifactCore::currentDCCTheme().textColor));
        
        if (shortcut != 0) {
            button->setShortcut(QKeySequence(shortcut));
        }
        
        return button;
    }

    QToolButton* createTextToolButton(const QString& text, const QString& tooltip, bool checkable)
    {
        auto* button = createToolButton(QStringList{}, tooltip, 0);
        button->setText(text);
        button->setCheckable(checkable);
        button->setFixedSize(48, 28);
        button->setIcon(QIcon());
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        QFont font = button->font();
        font.setPointSize(11);
        font.setWeight(QFont::DemiBold);
        button->setFont(font);
        applyThemeTextPalette(button, QColor(ArtifactCore::currentDCCTheme().textColor));
        return button;
    }

    QLabel* createLabel(const QString& text, const QString& tooltip)
    {
        auto* label = new QLabel(text, owner_);
        label->setToolTip(tooltip);
        label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        QFont font = label->font();
        font.setPointSize(12);
        font.setWeight(QFont::DemiBold);
        label->setFont(font);
        applyThemeTextPalette(label, QColor(ArtifactCore::currentDCCTheme().textColor));
        return label;
    }

    void updateSpeedPresetButtons(float speed)
    {
        const auto setChecked = [](QToolButton* button, bool checked) {
            if (!button) return;
            button->blockSignals(true);
            button->setChecked(checked);
            button->blockSignals(false);
        };
        setChecked(speedQuarterButton_, std::abs(speed - 0.25f) < 0.001f);
        setChecked(speedHalfButton_, std::abs(speed - 0.5f) < 0.001f);
        setChecked(speedOneButton_, std::abs(speed - 1.0f) < 0.001f);
        playbackSpeed_ = speed;
        if (currentTimeLabel_) {
            currentTimeLabel_->setToolTip(QStringLiteral("Playback speed: %1").arg(formatSpeedLabel(speed)));
        }
    }

    void updateFrameWidgets()
    {
        if (!owner_) {
            return;
        }
        const auto* service = ArtifactPlaybackService::instance();
        const FrameRate fpsRate = service ? service->frameRate() : FrameRate(30.0f);
        const float fps = std::max(1.0f, fpsRate.framerate());
        const FrameRange range = service ? service->frameRange() : FrameRange(FramePosition(0), FramePosition(300));
        const FramePosition current = service ? service->currentFrame() : FramePosition(0);

        const qint64 startFrame = std::min(range.start(), range.end());
        const qint64 endFrame = std::max(range.start(), range.end());
        const qint64 clampedCurrent = std::clamp(current.framePosition(), startFrame, endFrame);

        if (scrubSlider_) {
            QSignalBlocker blocker(scrubSlider_);
            scrubSlider_->setRange(static_cast<int>(startFrame), static_cast<int>(std::max(startFrame, endFrame)));
            scrubSlider_->setValue(static_cast<int>(clampedCurrent));
        }

        if (currentTimeLabel_) {
            currentTimeLabel_->setText(QStringLiteral("%1  %2 / %3")
                                           .arg(formatFrameCount(clampedCurrent))
                                           .arg(formatTimecode(clampedCurrent, fps))
                                           .arg(formatTimecode(range.duration(), fps)));
        }

        if (rangeLabel_) {
            QString inText = QStringLiteral("In --:--:--:--");
            QString outText = QStringLiteral("Out --:--:--:--");
            if (inOutPoints_) {
                if (const auto inPoint = inOutPoints_->inPoint()) {
                    inText = QStringLiteral("In %1").arg(formatTimecode(inPoint->framePosition(), fps));
                }
                if (const auto outPoint = inOutPoints_->outPoint()) {
                    outText = QStringLiteral("Out %1").arg(formatTimecode(outPoint->framePosition(), fps));
                }
            }
            rangeLabel_->setText(QStringLiteral("%1   %2").arg(inText, outText));
        }
    }

    void attachInOutPoints(ArtifactInOutPoints* points)
    {
        if (inOutPoints_ == points) {
            return;
        }
        if (inOutPoints_) {
            QObject::disconnect(inOutPoints_, nullptr, owner_, nullptr);
        }
        inOutPoints_ = points;
        if (!inOutPoints_) {
            updateFrameWidgets();
            return;
        }

        QObject::connect(inOutPoints_, &ArtifactInOutPoints::inPointChanged, owner_,
                         [this](std::optional<FramePosition>) { updateFrameWidgets(); });
        QObject::connect(inOutPoints_, &ArtifactInOutPoints::outPointChanged, owner_,
                         [this](std::optional<FramePosition>) { updateFrameWidgets(); });
        QObject::connect(inOutPoints_, &ArtifactInOutPoints::pointsCleared, owner_,
                         [this]() { updateFrameWidgets(); });
        updateFrameWidgets();
    }
    
    void connectSignals()
    {
        // 再生制御
        QObject::connect(playButton_, &QToolButton::clicked, owner_, [this]() {
            handlePlayButtonClicked();
        });

        QObject::connect(stopButton_, &QToolButton::clicked, owner_, [this]() {
            handleStopButtonClicked();
        });
        
        // シーク操作
        QObject::connect(seekStartButton_, &QToolButton::clicked, owner_, [this]() {
            handleSeekStartClicked();
        });
        
        QObject::connect(seekEndButton_, &QToolButton::clicked, owner_, [this]() {
            handleSeekEndClicked();
        });
        
        // フレーム操作
        QObject::connect(stepBackwardButton_, &QToolButton::clicked, owner_, [this]() {
            handleStepBackwardClicked();
        });
        
        QObject::connect(stepForwardButton_, &QToolButton::clicked, owner_, [this]() {
            handleStepForwardClicked();
        });
        
        // オプション
        QObject::connect(loopButton_, &QToolButton::toggled, owner_, [this](bool checked) {
            handleLoopToggled(checked);
        });
        
        QObject::connect(inButton_, &QToolButton::clicked, owner_, [this]() {
            handleInButtonClicked();
        });
        
        QObject::connect(outButton_, &QToolButton::clicked, owner_, [this]() {
            handleOutButtonClicked();
        });

        QObject::connect(clearInOutButton_, &QToolButton::clicked, owner_, [this]() {
            handleClearInOutClicked();
        });

        QObject::connect(speedQuarterButton_, &QToolButton::clicked, owner_, [this]() {
            handleSpeedPresetClicked(0.25f);
        });
        QObject::connect(speedHalfButton_, &QToolButton::clicked, owner_, [this]() {
            handleSpeedPresetClicked(0.5f);
        });
        QObject::connect(speedOneButton_, &QToolButton::clicked, owner_, [this]() {
            handleSpeedPresetClicked(1.0f);
        });

        QObject::connect(scrubSlider_, &QSlider::valueChanged, owner_, [this](int value) {
            if (auto* service = ArtifactPlaybackService::instance()) {
                service->goToFrame(FramePosition(value));
            }
            updateFrameWidgets();
        });
        
        // サービスからの状態更新を監視
        if (auto* service = ArtifactPlaybackService::instance()) {
            QObject::connect(service, &ArtifactPlaybackService::playbackStateChanged,
                owner_, [this](::Artifact::PlaybackState state) {
                    this->updatePlaybackState(state);
                });

            QObject::connect(service, &ArtifactPlaybackService::frameChanged,
                owner_, [this](const FramePosition&) {
                    updateFrameWidgets();
                });

            QObject::connect(service, &ArtifactPlaybackService::frameRangeChanged,
                owner_, [this](const FrameRange&) {
                    updateFrameWidgets();
                });

            QObject::connect(service, &ArtifactPlaybackService::playbackSpeedChanged,
                owner_, [this](float speed) {
                    updateSpeedPresetButtons(speed);
                });
            
            QObject::connect(service, &ArtifactPlaybackService::loopingChanged,
                owner_, [this](bool loop) {
                    isLooping_ = loop;
                    if (loopButton_) {
                        loopButton_->blockSignals(true);
                        loopButton_->setChecked(loop);
                        loopButton_->blockSignals(false);
                    }
                });

            QObject::connect(service, &ArtifactPlaybackService::currentCompositionChanged,
                owner_, [this](ArtifactCompositionPtr) {
                    syncFromService();
                });
        }
    }
    
    void handlePlayButtonClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            if (service->isPlaying()) {
                service->pause();
            } else {
                service->play();
            }
        }
        Q_EMIT owner_->playRequested();
    }
    
    void handlePauseButtonClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->pause();
        }
        Q_EMIT owner_->pauseRequested();
    }
    
    void handleStopButtonClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->stop();
        }
        Q_EMIT owner_->stopRequested();
    }
    
    void handleSeekStartClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->goToStartFrame();
        }
        Q_EMIT owner_->seekStartRequested();
    }
    
    void handleSeekEndClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->goToEndFrame();
        }
        Q_EMIT owner_->seekEndRequested();
    }
    
    void handleSeekPreviousClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->goToPreviousMarker();
        }
        Q_EMIT owner_->seekPreviousRequested();
    }
    
    void handleSeekNextClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->goToNextMarker();
        }
        Q_EMIT owner_->seekNextRequested();
    }
    
    void handleStepBackwardClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->goToPreviousFrame();
        }
        Q_EMIT owner_->stepBackwardRequested();
    }
    
    void handleStepForwardClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->goToNextFrame();
        }
        Q_EMIT owner_->stepForwardRequested();
    }
    
    void handleLoopToggled(bool enabled)
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->setLooping(enabled);
        }
        isLooping_ = enabled;
        Q_EMIT owner_->loopToggled(enabled);
    }
    
    void handleInButtonClicked()
    {
        Q_EMIT owner_->inPointSet();
    }
    
    void handleOutButtonClicked()
    {
        Q_EMIT owner_->outPointSet();
    }
    
    void handleClearInOutClicked()
    {
        Q_EMIT owner_->inoutCleared();
    }

    void handleSpeedPresetClicked(float speed)
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->setPlaybackSpeed(speed);
        }
        updateSpeedPresetButtons(speed);
        Q_EMIT owner_->playbackSpeedChanged(speed);
    }
    
    void updatePlaybackState(::Artifact::PlaybackState state)
    {
        isPlaying_ = (state == ::Artifact::PlaybackState::Playing);
        isPaused_ = (state == ::Artifact::PlaybackState::Paused);
        isStopped_ = (state == ::Artifact::PlaybackState::Stopped);
        
        // ボタンの状態と外観を更新
        if (playButton_) {
            playButton_->setChecked(isPlaying_);
            playButton_->setIcon(loadIconWithFallback(isPlaying_
                ? QStringList{QStringLiteral("MaterialVS/colored/E3E3E3/pause.svg"),
                              QStringLiteral("Material/pause.svg")}
                : QStringList{QStringLiteral("MaterialVS/colored/E3E3E3/play_arrow.svg"),
                              QStringLiteral("Material/play_arrow.svg")}));
            playButton_->setToolTip(isPlaying_ ? QStringLiteral("一時停止 (Space)") : QStringLiteral("再生 (Space)"));
        }
        if (pauseButton_) {
            pauseButton_->setEnabled(isPlaying_);
        }
        if (stopButton_) {
            stopButton_->setEnabled(isPlaying_ || isPaused_);
        }
        updateFrameWidgets();
    }
    
    void syncFromService()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            updatePlaybackState(service->state());
            isLooping_ = service->isLooping();
            playbackSpeed_ = service->playbackSpeed();
            updateSpeedPresetButtons(playbackSpeed_);
            attachInOutPoints(service->inOutPoints());
            updateFrameWidgets();
            if (loopButton_) {
                loopButton_->setChecked(isLooping_);
            }
            if (scrubSlider_) {
                QSignalBlocker blocker(scrubSlider_);
                const FrameRange range = service->frameRange();
                scrubSlider_->setRange(static_cast<int>(std::min(range.start(), range.end())),
                                       static_cast<int>(std::max(range.start(), range.end())));
                scrubSlider_->setValue(static_cast<int>(service->currentFrame().framePosition()));
            }
        }
    }
};

// ============================================================================
// ArtifactPlaybackControlWidget Implementation
// ============================================================================

W_OBJECT_IMPL(ArtifactPlaybackControlWidget)

ArtifactPlaybackControlWidget::ArtifactPlaybackControlWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this))
{
    setWindowTitle("Playback Control");
    setMinimumHeight(82);
    setAutoFillBackground(false);
    
    impl_->setupUI();
    
    impl_->syncFromService();
}

ArtifactPlaybackControlWidget::~ArtifactPlaybackControlWidget()
{
    delete impl_;
}

void ArtifactPlaybackControlWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    const auto& theme = ArtifactCore::currentDCCTheme();
    painter.fillRect(rect(), QColor(theme.backgroundColor));
}

void ArtifactPlaybackControlWidget::play()
{
    impl_->handlePlayButtonClicked();
}

void ArtifactPlaybackControlWidget::pause()
{
    impl_->handlePauseButtonClicked();
}

void ArtifactPlaybackControlWidget::stop()
{
    impl_->handleStopButtonClicked();
}

void ArtifactPlaybackControlWidget::togglePlayPause()
{
    if (impl_->isPlaying_ || impl_->isPaused_) {
        pause();
    } else {
        play();
    }
}

void ArtifactPlaybackControlWidget::seekStart()
{
    impl_->handleSeekStartClicked();
}

void ArtifactPlaybackControlWidget::seekEnd()
{
    impl_->handleSeekEndClicked();
}

void ArtifactPlaybackControlWidget::seekPrevious()
{
    impl_->handleSeekPreviousClicked();
}

void ArtifactPlaybackControlWidget::seekNext()
{
    impl_->handleSeekNextClicked();
}

void ArtifactPlaybackControlWidget::stepForward()
{
    impl_->handleStepForwardClicked();
}

void ArtifactPlaybackControlWidget::stepBackward()
{
    impl_->handleStepBackwardClicked();
}

void ArtifactPlaybackControlWidget::setLoopEnabled(bool enabled)
{
    if (impl_->loopButton_) {
        impl_->loopButton_->setChecked(enabled);
    }
    impl_->handleLoopToggled(enabled);
}

bool ArtifactPlaybackControlWidget::isLoopEnabled() const
{
    return impl_->isLooping_;
}

void ArtifactPlaybackControlWidget::setPlaybackSpeed(float speed)
{
    impl_->playbackSpeed_ = speed;
    if (auto* service = ArtifactPlaybackService::instance()) {
        service->setPlaybackSpeed(speed);
    }
    Q_EMIT playbackSpeedChanged(speed);
}

float ArtifactPlaybackControlWidget::playbackSpeed() const
{
    return impl_->playbackSpeed_;
}

bool ArtifactPlaybackControlWidget::isPlaying() const
{
    return impl_->isPlaying_;
}

bool ArtifactPlaybackControlWidget::isPaused() const
{
    return impl_->isPaused_;
}

bool ArtifactPlaybackControlWidget::isStopped() const
{
    return impl_->isStopped_;
}

void ArtifactPlaybackControlWidget::setInPoint()
{
    impl_->handleInButtonClicked();
}

void ArtifactPlaybackControlWidget::setOutPoint()
{
    impl_->handleOutButtonClicked();
}

void ArtifactPlaybackControlWidget::clearInOutPoints()
{
    impl_->handleClearInOutClicked();
}

// ============================================================================
// ArtifactPlaybackInfoWidget::Impl
// ============================================================================

class ArtifactPlaybackInfoWidget::Impl {
public:
    ArtifactPlaybackInfoWidget* owner_;
    
    QLabel* frameLabel_ = nullptr;
    QLabel* speedLabel_ = nullptr;
    QLabel* droppedLabel_ = nullptr;
    
    int64_t currentFrame_ = 0;
    int64_t totalFrames_ = 300;
    float speed_ = 1.0f;
    int64_t droppedFrames_ = 0;
    
    Impl(ArtifactPlaybackInfoWidget* owner)
        : owner_(owner)
    {}
    
    void setupUI()
    {
        auto* layout = new QHBoxLayout(owner_);
        layout->setSpacing(16);
        layout->setContentsMargins(8, 4, 8, 4);
        
        // フレーム表示
        frameLabel_ = createLabel("0 / 300", "現在のフレーム / 総フレーム数");
        layout->addWidget(frameLabel_);
        layout->addSpacing(24);
        
        // 速度表示
        speedLabel_ = createLabel("1.00x", "再生速度");
        layout->addWidget(speedLabel_);
        
        // ドロップフレーム表示
        droppedLabel_ = createLabel("Dropped: 0", "ドロップフレーム数");
        applyThemeTextPalette(droppedLabel_, QColor(ArtifactCore::currentDCCTheme().textColor), 90);
        layout->addWidget(droppedLabel_);
        
        layout->addStretch();
    }
    
    QLabel* createLabel(const QString& text, const QString& tooltip)
    {
        auto* label = new QLabel(text);
        label->setToolTip(tooltip);
        label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        QFont font = label->font();
        font.setPointSize(12);
        label->setFont(font);
        applyThemeTextPalette(label, QColor(ArtifactCore::currentDCCTheme().textColor));
        return label;
    }
};

// ============================================================================
// ArtifactPlaybackInfoWidget Implementation
// ============================================================================

W_OBJECT_IMPL(ArtifactPlaybackInfoWidget)

ArtifactPlaybackInfoWidget::ArtifactPlaybackInfoWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this))
{
    setMinimumHeight(32);
    impl_->setupUI();
    setAutoFillBackground(false);
}

ArtifactPlaybackInfoWidget::~ArtifactPlaybackInfoWidget()
{
    delete impl_;
}

void ArtifactPlaybackInfoWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(45, 45, 48));
}

void ArtifactPlaybackInfoWidget::setCurrentFrame(int64_t frame)
{
    impl_->currentFrame_ = frame;
    
    if (impl_->frameLabel_) {
        impl_->frameLabel_->setText(QString("%1 / %2").arg(frame).arg(impl_->totalFrames_));
    }
    
    Q_EMIT frameChanged(frame);
}

void ArtifactPlaybackInfoWidget::setTotalFrames(int64_t frames)
{
    impl_->totalFrames_ = frames;
    setCurrentFrame(impl_->currentFrame_);
}

void ArtifactPlaybackInfoWidget::setFrameRate(float fps)
{
    Q_UNUSED(fps);
}

void ArtifactPlaybackInfoWidget::setPlaybackSpeed(float speed)
{
    impl_->speed_ = speed;
    if (impl_->speedLabel_) {
        QString speedText = speed >= 0 ? QString("%1x").arg(speed, 0, 'f', 2)
                                        : QString("%1x (REV)").arg(std::abs(speed), 0, 'f', 2);
        impl_->speedLabel_->setText(speedText);
    }
}

void ArtifactPlaybackInfoWidget::setDroppedFrames(int64_t count)
{
    impl_->droppedFrames_ = count;
    if (impl_->droppedLabel_) {
        impl_->droppedLabel_->setText(QString("Dropped: %1").arg(count));
    }
}

void ArtifactPlaybackInfoWidget::setEditable(bool editable)
{
    // 将来的にはスピンボックスなどで直接入力可能に
    Q_UNUSED(editable);
}

// ============================================================================
// ArtifactPlaybackSpeedWidget::Impl
// ============================================================================

class ArtifactPlaybackSpeedWidget::Impl {
public:
    ArtifactPlaybackSpeedWidget* owner_;
    
    QSlider* speedSlider_ = nullptr;
    QDoubleSpinBox* speedSpin_ = nullptr;
    QComboBox* presetCombo_ = nullptr;
    
    float currentSpeed_ = 1.0f;
    
    Impl(ArtifactPlaybackSpeedWidget* owner)
        : owner_(owner)
    {}
    
    void setupUI()
    {
        auto* layout = new QHBoxLayout(owner_);
        layout->setSpacing(8);
        layout->setContentsMargins(8, 4, 8, 4);
        
        // プリセットコンボボックス
        presetCombo_ = new QComboBox();
        presetCombo_->addItem("0.25x", 0.25);
        presetCombo_->addItem("0.5x", 0.5);
        presetCombo_->addItem("1.0x", 1.0);
        presetCombo_->addItem("2.0x", 2.0);
        presetCombo_->addItem("-1.0x (REV)", -1.0);
        presetCombo_->setCurrentIndex(2);  // 1.0x
        presetCombo_->setFixedWidth(80);
        layout->addWidget(new QLabel("Speed:"));
        layout->addWidget(presetCombo_);
        
        // スライダー
        speedSlider_ = new QSlider(Qt::Horizontal);
        speedSlider_->setRange(-20, 20);  // -2.0x to 2.0x
        speedSlider_->setValue(10);       // 1.0x
        speedSlider_->setFixedWidth(150);
        layout->addWidget(speedSlider_);
        
        // スピンボックス
        speedSpin_ = new QDoubleSpinBox();
        speedSpin_->setRange(-2.0, 2.0);
        speedSpin_->setSingleStep(0.25);
        speedSpin_->setValue(1.0);
        speedSpin_->setSuffix("x");
        speedSpin_->setFixedWidth(70);
        layout->addWidget(speedSpin_);
        
        // シグナル接続
        QObject::connect(presetCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            owner_, [this](int index) {
                float speed = presetCombo_->itemData(index).toFloat();
                setPlaybackSpeed(speed);
            });
        
        QObject::connect(speedSlider_, &QSlider::valueChanged,
            owner_, [this](int value) {
                float speed = value / 10.0f;
                if (speedSpin_) {
                    speedSpin_->blockSignals(true);
                    speedSpin_->setValue(speed);
                    speedSpin_->blockSignals(false);
                }
                setPlaybackSpeed(speed);
            });
        
        QObject::connect(speedSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            owner_, [this](double value) {
                if (speedSlider_) {
                    speedSlider_->blockSignals(true);
                    speedSlider_->setValue(static_cast<int>(value * 10));
                    speedSlider_->blockSignals(false);
                }
                setPlaybackSpeed(static_cast<float>(value));
            });
    }
    
    void setPlaybackSpeed(float speed)
    {
        currentSpeed_ = speed;
        Q_EMIT owner_->speedChanged(speed);
    }
};

// ============================================================================
// ArtifactPlaybackSpeedWidget Implementation
// ============================================================================

W_OBJECT_IMPL(ArtifactPlaybackSpeedWidget)

ArtifactPlaybackSpeedWidget::ArtifactPlaybackSpeedWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this))
{
    setMinimumHeight(32);
    impl_->setupUI();
    setAutoFillBackground(false);
}

ArtifactPlaybackSpeedWidget::~ArtifactPlaybackSpeedWidget()
{
    delete impl_;
}

void ArtifactPlaybackSpeedWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(45, 45, 48));
}

float ArtifactPlaybackSpeedWidget::playbackSpeed() const
{
    return impl_->currentSpeed_;
}

void ArtifactPlaybackSpeedWidget::setPlaybackSpeed(float speed)
{
    impl_->setPlaybackSpeed(speed);
}

void ArtifactPlaybackSpeedWidget::setSpeedPreset(float speed)
{
    if (impl_->presetCombo_) {
        int index = impl_->presetCombo_->findData(speed);
        if (index >= 0) {
            impl_->presetCombo_->setCurrentIndex(index);
        }
    }
    setPlaybackSpeed(speed);
}

} // namespace Artifact
