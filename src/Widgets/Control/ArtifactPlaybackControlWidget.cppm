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
  //QToolButton* backForward_ = nullptr;
  void setIconSize(const QSize& size);

  void handlePlayButtonClicked();
  void handleStopButtonClicked();
  //void handleSeekStartButtonClicked();
 };

 ArtifactPlaybackControlWidget::Impl::Impl()
 {
  playButton_ = new QToolButton();
  playButton_->setIcon(QIcon(getIconPath() + "/PlayArrow.png"));
  playButton_->setIconSize(QSize(32, 32));
  stopButton_ = new QToolButton();
  stopButton_->setIcon(QIcon(getIconPath() + "/Png/stop.png"));
  stopButton_->setIconSize(QSize(32, 32));
  backForward_ = new QToolButton();

  pauseButton_ = new QToolButton();
  pauseButton_->setIcon(QIcon(getIconPath() + "/Png/pause.png"));
  pauseButton_->setIconSize(QSize(32, 32));
 }

 void ArtifactPlaybackControlWidget::Impl::setIconSize(const QSize& size)
 {

 }

 void ArtifactPlaybackControlWidget::Impl::handlePlayButtonClicked()
 {

 }

 void ArtifactPlaybackControlWidget::Impl::handleStopButtonClicked()
 {

 }

 W_OBJECT_IMPL(ArtifactPlaybackControlWidget)

  ArtifactPlaybackControlWidget::ArtifactPlaybackControlWidget(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  setObjectName("PlaybackControlWidget");
  setWindowTitle("PlaybackControlWidget");

  auto layout = new QHBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  layout->addWidget(impl_->backForward_);
  layout->addWidget(impl_->playButton_);
  layout->addWidget(impl_->pauseButton_);
  layout->addWidget(impl_->stopButton_);

  setLayout(layout);

  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);

  setStyleSheet(style);
  connect(impl_->playButton_, &QToolButton::clicked,
   this, &ArtifactPlaybackControlWidget::play);

  connect(impl_->stopButton_, &QToolButton::clicked,
   this, &ArtifactPlaybackControlWidget::stop);

  setMinimumSize(0, 32);
 }

 ArtifactPlaybackControlWidget::~ArtifactPlaybackControlWidget()
 {
  delete impl_;
 }

 void ArtifactPlaybackControlWidget::play()
 {
  qDebug() << "Played";

  ArtifactApplicationManager::instance()->activeContextService()->sendPlayToActiveContext();
 }

 void ArtifactPlaybackControlWidget::stop()
 {

 }

 void ArtifactPlaybackControlWidget::seekStart()
 {

 }

 void ArtifactPlaybackControlWidget::seekEnd()
 {

 }

};