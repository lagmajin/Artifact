module;
#include <wobjectimpl.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

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
import Widgets.Utils.CSS;
import Artifact.Application.Manager;
import Artifact.Service.Playback;
<<<<<<< HEAD
import Artifact.Service.ActiveContext;
=======

namespace {
QIcon loadIconWithFallback(const QString& fileName)
{
  const QString resourcePath = ArtifactCore::resolveIconResourcePath(fileName);
  QIcon icon(resourcePath);
  if (!icon.isNull()) {
    return icon;
  }
  return QIcon(ArtifactCore::resolveIconPath(fileName));
}
}
>>>>>>> aed72ea

namespace Artifact
{
using namespace ArtifactCore;

<<<<<<< HEAD
// ============================================================================
// ArtifactPlaybackControlWidget::Impl
// ============================================================================
=======
 class ArtifactPlaybackControlWidget::Impl {
 private:

 public:
  Impl();
  ~Impl() = default;
  QToolButton* playButton_ = nullptr;
  QToolButton* pauseButton_ = nullptr;
  QToolButton* stopButton_ = nullptr;
  QToolButton* backForward_ = nullptr;
  QToolButton* stepForwardButton_ = nullptr;
  QToolButton* stepBackwardButton_ = nullptr;
  QToolButton* seekStartButton_ = nullptr;
  QToolButton* seekEndButton_ = nullptr;
  
 bool isPlaying_ = false;
 bool isLooping_ = false;
  float playbackSpeed_ = 1.0f;
  
  void setIconSize(const QSize& size);
  void syncFromService();
  void refreshButtonStates();

  void handlePlayButtonClicked();
  void handleStopButtonClicked();
  void handlePauseButtonClicked();
  void handleStepForwardClicked();
  void handleStepBackwardClicked();
  void handleSeekStartClicked();
  void handleSeekEndClicked();
 };

 ArtifactPlaybackControlWidget::Impl::Impl()
 {
  playButton_ = new QToolButton();
  playButton_->setIcon(loadIconWithFallback("PlayArrow.png"));
  playButton_->setIconSize(QSize(32, 32));
  
  pauseButton_ = new QToolButton();
  pauseButton_->setIcon(loadIconWithFallback("Png/pause.png"));
  pauseButton_->setIconSize(QSize(32, 32));
  
  stopButton_ = new QToolButton();
  stopButton_->setIcon(loadIconWithFallback("Png/stop.png"));
  stopButton_->setIconSize(QSize(32, 32));
  
  stepBackwardButton_ = new QToolButton();
  stepBackwardButton_->setIcon(loadIconWithFallback("Png/step_backward.png"));
  stepBackwardButton_->setIconSize(QSize(24, 24));
  
  stepForwardButton_ = new QToolButton();
  stepForwardButton_->setIcon(loadIconWithFallback("Png/step_forward.png"));
  stepForwardButton_->setIconSize(QSize(24, 24));
  
  seekStartButton_ = new QToolButton();
  seekStartButton_->setIcon(loadIconWithFallback("Png/seek_start.png"));
  seekStartButton_->setIconSize(QSize(24, 24));
  
  seekEndButton_ = new QToolButton();
  seekEndButton_->setIcon(loadIconWithFallback("Png/seek_end.png"));
  seekEndButton_->setIconSize(QSize(24, 24));
  
  backForward_ = new QToolButton();
 }

 void ArtifactPlaybackControlWidget::Impl::setIconSize(const QSize& size)
 {
  playButton_->setIconSize(size);
  pauseButton_->setIconSize(size);
  stopButton_->setIconSize(size);
  stepBackwardButton_->setIconSize(size);
  stepForwardButton_->setIconSize(size);
  seekStartButton_->setIconSize(size);
  seekEndButton_->setIconSize(size);
 }

 void ArtifactPlaybackControlWidget::Impl::syncFromService()
 {
  if (auto* service = ArtifactPlaybackService::instance()) {
   isPlaying_ = service->isPlaying();
   isLooping_ = service->isLooping();
   playbackSpeed_ = service->playbackSpeed();
  }
  refreshButtonStates();
 }

 void ArtifactPlaybackControlWidget::Impl::refreshButtonStates()
 {
  if (playButton_) {
   playButton_->setEnabled(!isPlaying_);
  }
  if (pauseButton_) {
   pauseButton_->setEnabled(isPlaying_);
  }
  if (stopButton_) {
   stopButton_->setEnabled(isPlaying_);
  }
 }

 void ArtifactPlaybackControlWidget::Impl::handlePlayButtonClicked()
 {
  if (auto* service = ArtifactPlaybackService::instance()) {
   service->play();
  }
  syncFromService();
 }

 void ArtifactPlaybackControlWidget::Impl::handleStopButtonClicked()
 {
  if (auto* service = ArtifactPlaybackService::instance()) {
   service->stop();
  }
  syncFromService();
 }

 void ArtifactPlaybackControlWidget::Impl::handlePauseButtonClicked()
 {
  if (auto* service = ArtifactPlaybackService::instance()) {
   service->pause();
  }
  syncFromService();
 }

 void ArtifactPlaybackControlWidget::Impl::handleStepForwardClicked()
 {
  if (auto* service = ArtifactPlaybackService::instance()) {
   service->goToNextFrame();
  }
  syncFromService();
 }

 void ArtifactPlaybackControlWidget::Impl::handleStepBackwardClicked()
 {
  if (auto* service = ArtifactPlaybackService::instance()) {
   service->goToPreviousFrame();
  }
  syncFromService();
 }

 void ArtifactPlaybackControlWidget::Impl::handleSeekStartClicked()
 {
  if (auto* service = ArtifactPlaybackService::instance()) {
   service->goToStartFrame();
  }
  syncFromService();
 }

 void ArtifactPlaybackControlWidget::Impl::handleSeekEndClicked()
 {
  if (auto* service = ArtifactPlaybackService::instance()) {
   service->goToEndFrame();
  }
  syncFromService();
 }

 W_OBJECT_IMPL(ArtifactPlaybackControlWidget)

  ArtifactPlaybackControlWidget::ArtifactPlaybackControlWidget(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  setObjectName("PlaybackControlWidget");
  setWindowTitle("PlaybackControlWidget");

  auto layout = new QHBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  layout->addWidget(impl_->seekStartButton_);
  layout->addWidget(impl_->stepBackwardButton_);
  layout->addWidget(impl_->playButton_);
  layout->addWidget(impl_->pauseButton_);
  layout->addWidget(impl_->stopButton_);
  layout->addWidget(impl_->stepForwardButton_);
  layout->addWidget(impl_->seekEndButton_);

  setLayout(layout);

  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);

  setStyleSheet(style);
  
  // ボタンの接続
  connect(impl_->playButton_, &QToolButton::clicked,
   this, &ArtifactPlaybackControlWidget::play);

  connect(impl_->pauseButton_, &QToolButton::clicked,
   this, [this]() {
    impl_->handlePauseButtonClicked();
    Q_EMIT pauseButtonClicked();
   });

  connect(impl_->stopButton_, &QToolButton::clicked,
   this, &ArtifactPlaybackControlWidget::stop);

  connect(impl_->stepForwardButton_, &QToolButton::clicked,
   this, &ArtifactPlaybackControlWidget::stepForward);

  connect(impl_->stepBackwardButton_, &QToolButton::clicked,
   this, &ArtifactPlaybackControlWidget::stepBackward);

  connect(impl_->seekStartButton_, &QToolButton::clicked,
   this, &ArtifactPlaybackControlWidget::seekStart);

  connect(impl_->seekEndButton_, &QToolButton::clicked,
   this, &ArtifactPlaybackControlWidget::seekEnd);

  if (auto* playbackService = ArtifactPlaybackService::instance()) {
   connect(playbackService, &ArtifactPlaybackService::playbackStateChanged, this,
    [this](PlaybackState) {
     impl_->syncFromService();
    });
   connect(playbackService, &ArtifactPlaybackService::playbackSpeedChanged, this,
    [this](float speed) {
     impl_->playbackSpeed_ = speed;
     impl_->refreshButtonStates();
    });
   connect(playbackService, &ArtifactPlaybackService::loopingChanged, this,
    [this](bool loop) {
     impl_->isLooping_ = loop;
     impl_->refreshButtonStates();
    });
  }

  setMinimumSize(0, 32);
  impl_->syncFromService();
 }

 ArtifactPlaybackControlWidget::~ArtifactPlaybackControlWidget()
 {
  delete impl_;
 }

 void ArtifactPlaybackControlWidget::play()
 {
  impl_->handlePlayButtonClicked();
  Q_EMIT playButtonClicked();
 }

 void ArtifactPlaybackControlWidget::stop()
 {
  impl_->handleStopButtonClicked();
  Q_EMIT stopButtonClicked();
 }

 void ArtifactPlaybackControlWidget::seekStart()
 {
  impl_->handleSeekStartClicked();
  Q_EMIT seekStartClicked();
 }

 void ArtifactPlaybackControlWidget::seekEnd()
 {
  impl_->handleSeekEndClicked();
  Q_EMIT seekEndClicked();
 }

 void ArtifactPlaybackControlWidget::stepForward()
 {
  impl_->handleStepForwardClicked();
  Q_EMIT stepForwardClicked();
 }

 void ArtifactPlaybackControlWidget::stepBackward()
 {
  impl_->handleStepBackwardClicked();
  Q_EMIT stepBackwardClicked();
 }

 void ArtifactPlaybackControlWidget::setLoopEnabled(bool enabled)
 {
  impl_->isLooping_ = enabled;
  if (auto* service = ArtifactPlaybackService::instance()) {
   service->setLooping(enabled);
  }
  impl_->refreshButtonStates();
  Q_EMIT loopToggled(enabled);
 }

 void ArtifactPlaybackControlWidget::setPreviewRange(int start, int end)
 {
  // TODO: プレビューレンジ設定
 }

 void ArtifactPlaybackControlWidget::setPlaybackSpeed(float speed)
 {
  impl_->playbackSpeed_ = qBound(0.1f, speed, 10.0f);
  if (auto* service = ArtifactPlaybackService::instance()) {
   service->setPlaybackSpeed(impl_->playbackSpeed_);
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

 bool ArtifactPlaybackControlWidget::isLooping() const
 {
  return impl_->isLooping_;
 }
>>>>>>> aed72ea

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
    
    // State
    bool isPlaying_ = false;
    bool isPaused_ = false;
    bool isStopped_ = true;
    bool isLooping_ = false;
    float playbackSpeed_ = 1.0f;
    
    // Icons
    QString iconPath_;
    
    Impl(ArtifactPlaybackControlWidget* owner)
        : owner_(owner)
    {
        iconPath_ = getIconPath();
    }
    
    ~Impl() = default;
    
    void setupUI()
    {
        auto* mainLayout = new QHBoxLayout(owner_);
        mainLayout->setSpacing(4);
        mainLayout->setContentsMargins(8, 4, 8, 4);
        
        // 再生コントロールグループ
        auto* playLayout = new QHBoxLayout();
        playLayout->setSpacing(2);
        
        // 再生/停止/一時停止
        playButton_ = createToolButton("PlayArrow.png", "再生 (Space)", Qt::Key_Space);
        pauseButton_ = createToolButton("pause.png", "一時停止", Qt::NoShortcut);
        stopButton_ = createToolButton("stop.png", "停止", Qt::NoShortcut);
        
        playLayout->addWidget(playButton_);
        playLayout->addWidget(pauseButton_);
        playLayout->addWidget(stopButton_);
        
        // シークコントロール
        auto* seekLayout = new QHBoxLayout();
        seekLayout->setSpacing(2);
        
        seekStartButton_ = createToolButton("seek_start.png", "先頭へ (Home)", Qt::Key_Home);
        seekPreviousButton_ = createToolButton("step_backward.png", "前へ (PageUp)", Qt::Key_PageUp);
        seekNextButton_ = createToolButton("step_forward.png", "次へ (PageDown)", Qt::Key_PageDown);
        seekEndButton_ = createToolButton("seek_end.png", "末尾へ (End)", Qt::Key_End);
        
        seekLayout->addWidget(seekStartButton_);
        seekLayout->addWidget(seekPreviousButton_);
        seekLayout->addWidget(seekNextButton_);
        seekLayout->addWidget(seekEndButton_);
        
        // フレーム操作
        auto* stepLayout = new QHBoxLayout();
        stepLayout->setSpacing(2);
        
        stepBackwardButton_ = createToolButton("step_backward.png", "1 フレーム戻る (←)", Qt::Key_Left);
        stepForwardButton_ = createToolButton("step_forward.png", "1 フレーム進む (→)", Qt::Key_Right);
        
        stepLayout->addWidget(stepBackwardButton_);
        stepLayout->addWidget(stepForwardButton_);
        
        // ループ・In/Out
        auto* optionLayout = new QHBoxLayout();
        optionLayout->setSpacing(2);
        
        loopButton_ = createToolButton("loop.png", "ループ再生 (L)", Qt::Key_L);
        loopButton_->setCheckable(true);
        loopButton_->setChecked(false);
        
        inButton_ = createToolButton("in_point.png", "In 点設定 (I)", Qt::Key_I);
        outButton_ = createToolButton("out_point.png", "Out 点設定 (O)", Qt::Key_O);
        clearInOutButton_ = createToolButton("clear.png", "In/Out クリア", Qt::NoShortcut);
        
        optionLayout->addWidget(loopButton_);
        optionLayout->addWidget(inButton_);
        optionLayout->addWidget(outButton_);
        optionLayout->addWidget(clearInOutButton_);
        
        // メインレイアウトに追加
        mainLayout->addLayout(playLayout);
        mainLayout->addSpacing(12);
        mainLayout->addLayout(seekLayout);
        mainLayout->addSpacing(12);
        mainLayout->addLayout(stepLayout);
        mainLayout->addSpacing(12);
        mainLayout->addLayout(optionLayout);
        mainLayout->addStretch();
        
        // シグナル接続
        connectSignals();
    }
    
    QToolButton* createToolButton(const QString& iconName, const QString& tooltip, QKeySequence::StandardKey shortcut)
    {
        auto* button = new QToolButton();
        button->setIcon(QIcon(iconPath_ + "/" + iconName));
        button->setIconSize(QSize(24, 24));
        button->setToolTip(tooltip);
        button->setAutoRaise(true);
        button->setFixedSize(36, 36);
        
        if (shortcut != Qt::NoShortcut) {
            button->setShortcut(shortcut);
        }
        
        return button;
    }
    
    void connectSignals()
    {
        // 再生制御
        QObject::connect(playButton_, &QToolButton::clicked, owner_, [this]() {
            handlePlayButtonClicked();
        });
        
        QObject::connect(pauseButton_, &QToolButton::clicked, owner_, [this]() {
            handlePauseButtonClicked();
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
        
        QObject::connect(seekPreviousButton_, &QToolButton::clicked, owner_, [this]() {
            handleSeekPreviousClicked();
        });
        
        QObject::connect(seekNextButton_, &QToolButton::clicked, owner_, [this]() {
            handleSeekNextClicked();
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
        
        // サービスからの状態更新を監視
        if (auto* service = ArtifactPlaybackService::instance()) {
            QObject::connect(service, &ArtifactPlaybackService::playbackStateChanged,
                owner_, [this](PlaybackState state) {
                    updatePlaybackState(state);
                });
            
            QObject::connect(service, &ArtifactPlaybackService::loopingChanged,
                owner_, [this](bool loop) {
                    isLooping_ = loop;
                    if (loopButton_) {
                        loopButton_->blockSignals(true);
                        loopButton_->setChecked(loop);
                        loopButton_->blockSignals(false);
                    }
                    Q_EMIT owner_->loopToggled(loop);
                });
            
            QObject::connect(service, &ArtifactPlaybackService::playbackSpeedChanged,
                owner_, [this](float speed) {
                    playbackSpeed_ = speed;
                    Q_EMIT owner_->playbackSpeedChanged(speed);
                });
        }
    }
    
    void handlePlayButtonClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->play();
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
    
    void updatePlaybackState(PlaybackState state)
    {
        isPlaying_ = (state == PlaybackState::Playing);
        isPaused_ = (state == PlaybackState::Paused);
        isStopped_ = (state == PlaybackState::Stopped);
        
        // ボタンの状態を更新
        if (playButton_) {
            playButton_->setEnabled(isStopped_ || isPaused_);
        }
        if (pauseButton_) {
            pauseButton_->setEnabled(isPlaying_);
        }
        if (stopButton_) {
            stopButton_->setEnabled(isPlaying_ || isPaused_);
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
    setMinimumHeight(48);
    
    impl_->setupUI();
    
    // スタイルシート適用
    setStyleSheet(R"(
        QToolButton {
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 4px;
        }
        QToolButton:hover {
            background-color: rgba(255, 255, 255, 0.1);
            border: 1px solid rgba(255, 255, 255, 0.2);
        }
        QToolButton:pressed {
            background-color: rgba(255, 255, 255, 0.2);
        }
        QToolButton:disabled {
            opacity: 0.5;
        }
        QToolButton:checked {
            background-color: rgba(100, 150, 255, 0.3);
            border: 1px solid rgba(100, 150, 255, 0.5);
        }
    )");
}

ArtifactPlaybackControlWidget::~ArtifactPlaybackControlWidget()
{
    delete impl_;
}

void ArtifactPlaybackControlWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(45, 45, 48));
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
    QLabel* timecodeLabel_ = nullptr;
    QLabel* fpsLabel_ = nullptr;
    QLabel* speedLabel_ = nullptr;
    QLabel* droppedLabel_ = nullptr;
    
    int64_t currentFrame_ = 0;
    int64_t totalFrames_ = 300;
    float fps_ = 30.0f;
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
        
        // タイムコード表示
        timecodeLabel_ = createLabel("00:00:00:00", "タイムコード (HH:MM:SS:FF)");
        timecodeLabel_->setFont(QFont("Consolas", 11, QFont::Bold));
        layout->addWidget(timecodeLabel_);
        
        layout->addSpacing(24);
        
        // FPS 表示
        fpsLabel_ = createLabel("30.00 fps", "フレームレート");
        layout->addWidget(fpsLabel_);
        
        // 速度表示
        speedLabel_ = createLabel("1.00x", "再生速度");
        layout->addWidget(speedLabel_);
        
        // ドロップフレーム表示
        droppedLabel_ = createLabel("Dropped: 0", "ドロップフレーム数");
        droppedLabel_->setStyleSheet("color: #ff6b6b;");
        layout->addWidget(droppedLabel_);
        
        layout->addStretch();
    }
    
    QLabel* createLabel(const QString& text, const QString& tooltip)
    {
        auto* label = new QLabel(text);
        label->setToolTip(tooltip);
        label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        label->setStyleSheet("color: #e0e0e0; font-size: 12px;");
        return label;
    }
    
    void updateTimecode()
    {
        int64_t totalSeconds = currentFrame_ / static_cast<int64_t>(fps_);
        int64_t frame = currentFrame_ % static_cast<int64_t>(fps_);
        int64_t hours = totalSeconds / 3600;
        int64_t minutes = (totalSeconds % 3600) / 60;
        int64_t seconds = totalSeconds % 60;
        
        QString tc = QString("%1:%2:%3:%4")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(frame, 2, 10, QChar('0'));
        
        if (timecodeLabel_) {
            timecodeLabel_->setText(tc);
        }
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
    
    setStyleSheet(R"(
        ArtifactPlaybackInfoWidget {
            background-color: #2d2d30;
            border-top: 1px solid #3e3e42;
        }
    )");
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
    impl_->updateTimecode();
    
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
    impl_->fps_ = fps;
    if (impl_->fpsLabel_) {
        impl_->fpsLabel_->setText(QString("%1 fps").arg(fps, 0, 'f', 2));
    }
    impl_->updateTimecode();
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
    
    setStyleSheet(R"(
        ArtifactPlaybackSpeedWidget {
            background-color: #2d2d30;
            border-top: 1px solid #3e3e42;
        }
        QSlider::groove:horizontal {
            background: #3e3e42;
            height: 4px;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #6496ff;
            width: 12px;
            margin: -4px 0;
            border-radius: 6px;
        }
        QSlider::handle:horizontal:hover {
            background: #80b0ff;
        }
    )");
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
    impl_->currentSpeed_ = speed;
    Q_EMIT speedChanged(speed);
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
