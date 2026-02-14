module;
#include <wobjectimpl.h>
#include <QHBoxLayout>
#include <QWidget>
#include <QPushButton>
#include <QToolButton>
module Artifact.Widgets.PlaybackControlWidget;

import std;
import Utils;
import Widgets.Utils.CSS;
import Artifact.Application.Manager;

import Artifact.Service.ActiveContext;


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
  playButton_->setIcon(QIcon(getIconPath() + "/PlayArrow.png"));
  playButton_->setIconSize(QSize(32, 32));
  
  pauseButton_ = new QToolButton();
  pauseButton_->setIcon(QIcon(getIconPath() + "/Png/pause.png"));
  pauseButton_->setIconSize(QSize(32, 32));
  
  stopButton_ = new QToolButton();
  stopButton_->setIcon(QIcon(getIconPath() + "/Png/stop.png"));
  stopButton_->setIconSize(QSize(32, 32));
  
  stepBackwardButton_ = new QToolButton();
  stepBackwardButton_->setIcon(QIcon(getIconPath() + "/Png/step_backward.png"));
  stepBackwardButton_->setIconSize(QSize(24, 24));
  
  stepForwardButton_ = new QToolButton();
  stepForwardButton_->setIcon(QIcon(getIconPath() + "/Png/step_forward.png"));
  stepForwardButton_->setIconSize(QSize(24, 24));
  
  seekStartButton_ = new QToolButton();
  seekStartButton_->setIcon(QIcon(getIconPath() + "/Png/seek_start.png"));
  seekStartButton_->setIconSize(QSize(24, 24));
  
  seekEndButton_ = new QToolButton();
  seekEndButton_->setIcon(QIcon(getIconPath() + "/Png/seek_end.png"));
  seekEndButton_->setIconSize(QSize(24, 24));
  
  backForward_ = new QToolButton();
 }

 void ArtifactPlaybackControlWidget::Impl::setIconSize(const QSize& size)
 {

 }

 void ArtifactPlaybackControlWidget::Impl::handlePlayButtonClicked()
 {
  isPlaying_ = !isPlaying_;
  // TODO: 再生サービスに通知
 }

 void ArtifactPlaybackControlWidget::Impl::handleStopButtonClicked()
 {
  isPlaying_ = false;
  // TODO: 再生サービスに通知
 }

 void ArtifactPlaybackControlWidget::Impl::handlePauseButtonClicked()
 {
  isPlaying_ = false;
  // TODO: 再生サービスに通知
 }

 void ArtifactPlaybackControlWidget::Impl::handleStepForwardClicked()
 {
  // TODO: 再生サービスに通知
 }

 void ArtifactPlaybackControlWidget::Impl::handleStepBackwardClicked()
 {
  // TODO: 再生サービスに通知
 }

 void ArtifactPlaybackControlWidget::Impl::handleSeekStartClicked()
 {
  // TODO: 再生サービスに通知
 }

 void ArtifactPlaybackControlWidget::Impl::handleSeekEndClicked()
 {
  // TODO: 再生サービスに通知
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
   this, &ArtifactPlaybackControlWidget::pauseButtonClicked);

  connect(impl_->stopButton_, &QToolButton::clicked,
   this, &ArtifactPlaybackControlWidget::stop);

  connect(impl_->stepForwardButton_, &QToolButton::clicked,
   this, &ArtifactPlaybackControlWidget::stepForwardClicked);

  connect(impl_->stepBackwardButton_, &QToolButton::clicked,
   this, &ArtifactPlaybackControlWidget::stepBackwardClicked);

  connect(impl_->seekStartButton_, &QToolButton::clicked,
   this, &ArtifactPlaybackControlWidget::seekStartClicked);

  connect(impl_->seekEndButton_, &QToolButton::clicked,
   this, &ArtifactPlaybackControlWidget::seekEndClicked);

  setMinimumSize(0, 32);
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
  Q_EMIT loopToggled(enabled);
 }

 void ArtifactPlaybackControlWidget::setPreviewRange(int start, int end)
 {
  // TODO: プレビューレンジ設定
 }

 void ArtifactPlaybackControlWidget::setPlaybackSpeed(float speed)
 {
  impl_->playbackSpeed_ = qBound(0.1f, speed, 10.0f);
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