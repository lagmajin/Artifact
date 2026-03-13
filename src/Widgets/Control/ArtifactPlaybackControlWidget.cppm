module;
#include <wobjectimpl.h>
#include <QHBoxLayout>
#include <QWidget>
#include <QPushButton>
#include <QToolButton>
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

import Artifact.Service.ActiveContext;
import Artifact.Service.Playback;

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

namespace Artifact
{
 using namespace ArtifactCore;

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

};
