module;
#include <QLabel>
#include <QBoxLayout>
#include <QString>
#include <wobjectimpl.h>

module Artifact.Timeline.TimeCodeWidget;

import Time.Rational;

namespace Artifact
{
 W_OBJECT_IMPL(ArtifactTimeCodeWidget)
 class ArtifactTimeCodeWidget::Impl {
 public:
  Impl();
  QLabel* timecodeLabel_ = nullptr;
  QLabel* frameNumberLabel_ = nullptr;
 };

 ArtifactTimeCodeWidget::Impl::Impl()
 {
  timecodeLabel_ = new QLabel();
  timecodeLabel_->setText("00:00:00:00");
  frameNumberLabel_ = new QLabel();
 }

 ArtifactTimeCodeWidget::ArtifactTimeCodeWidget(QWidget* parent /*= nullptr*/) : QWidget(parent), impl_(new Impl())
 {
  auto vLayout = new QVBoxLayout();
  vLayout->setSpacing(0);
  vLayout->setContentsMargins(0, 0, 0, 0);
  vLayout->setAlignment(Qt::AlignTop);
  impl_->timecodeLabel_->setObjectName("timeLabel");
  impl_->frameNumberLabel_->setObjectName("frameLabel");

  vLayout->addWidget(impl_->timecodeLabel_);
  vLayout->addWidget(impl_->frameNumberLabel_);

  auto layout = new QHBoxLayout();
  layout->setSpacing(0);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addLayout(vLayout);

  setLayout(layout);

  setAttribute(Qt::WA_StyledBackground, true);

  setStyleSheet(
   "ArtifactTimeCodeWidget {"
   "  background-color: #1C1C1C;"
   "}"
   "QLabel#timeLabel {"
   "  font-size: 38px;"
   "  color: #E8E8E8;"
   "}"
   "QLabel#frameLabel {"
   "  font-size: 30px;"
   "  color: #8A8A8A;"
   "}"
  );
 }

 ArtifactTimeCodeWidget::~ArtifactTimeCodeWidget()
 {
  delete impl_;
 }

 void ArtifactTimeCodeWidget::updateTimeCode(int frame)
 {
    // Default FPS for display ? could be made configurable later
    const int fps = 30;

    // Use RationalTime to represent the frame/time (value = frame count, scale = fps)
    ArtifactCore::RationalTime rt(static_cast<int64_t>(frame), static_cast<int64_t>(fps));

    // Compute hours/minutes/seconds/frames
    int totalFrames = frame;
    int ff = totalFrames % fps;
    int totalSeconds = totalFrames / fps;
    int s = totalSeconds % 60;
    int m = (totalSeconds / 60) % 60;
    int h = totalSeconds / 3600;

    QString tc = QString("%1:%2:%3:%4")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(ff, 2, 10, QChar('0'));

    if (impl_->timecodeLabel_) impl_->timecodeLabel_->setText(tc);
    if (impl_->frameNumberLabel_) impl_->frameNumberLabel_->setText(QString::number(totalFrames));

    // Example use of RationalTime API (kept for future extension)
    Q_UNUSED(rt);
 }

};
