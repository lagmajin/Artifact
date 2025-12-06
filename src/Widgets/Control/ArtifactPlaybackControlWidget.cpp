module;
#include <wobjectimpl.h>
#include <QHBoxLayout>
#include <QWidget>
#include <QPushButton>
#include <QToolButton>
module Artifact.Widgets.PlaybackControlWidget;
import Utils;
import Widgets.Utils.CSS;

namespace Artifact
{
 using namespace ArtifactCore;
	
 class ArtifactPlaybackControlWidget::Impl {
 private:

 public:
  Impl();
  ~Impl()=default;
  QToolButton* playButton_ = nullptr;
  QToolButton* stopButton_ = nullptr;
  QToolButton* backForward_ = nullptr;
 	
 };

 ArtifactPlaybackControlWidget::Impl::Impl()
 {
  playButton_ = new QToolButton();
  playButton_->setIcon(QIcon(getIconPath() + "/PlayArrow.png"));
  stopButton_ = new QToolButton();
  backForward_ = new QToolButton();

 }
	
 W_OBJECT_IMPL(ArtifactPlaybackControlWidget)

 ArtifactPlaybackControlWidget::ArtifactPlaybackControlWidget(QWidget* parent /*= nullptr*/):QWidget(parent),impl_(new Impl())
 {
  auto layout = new QHBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  layout->addWidget(impl_->backForward_);
  layout->addWidget(impl_->playButton_);
  layout->addWidget(impl_->stopButton_);

  setLayout(layout);
 	
  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);

  setStyleSheet(style);
  connect(impl_->playButton_, &QToolButton::clicked,
   this, &ArtifactPlaybackControlWidget::play);
 	
  connect(impl_->stopButton_, &QToolButton::clicked,
   this, &ArtifactPlaybackControlWidget::stop);
 }

 ArtifactPlaybackControlWidget::~ArtifactPlaybackControlWidget()
 {
  delete impl_;
 }

 void ArtifactPlaybackControlWidget::play()
 {
  qDebug() << "Played";
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