module;
#include <QLabel>
#include <QBoxLayout>
#include <QVBoxLayout>
#include <QString>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QPushButton>
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
  frameNumberLabel_->setText("(0 f)");
 }

 ArtifactTimeCodeWidget::ArtifactTimeCodeWidget(QWidget* parent /*= nullptr*/) : QWidget(parent), impl_(new Impl())
 {
  auto layout = new QVBoxLayout();
  layout->setSpacing(0);
  layout->setContentsMargins(8, 2, 8, 2);
  layout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  
  impl_->timecodeLabel_->setObjectName("timeLabel");
  impl_->frameNumberLabel_->setObjectName("frameLabel");
  impl_->timecodeLabel_->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
  impl_->frameNumberLabel_->setAlignment(Qt::AlignLeft | Qt::AlignTop);

  layout->addWidget(impl_->timecodeLabel_);
  layout->addWidget(impl_->frameNumberLabel_);

  setLayout(layout);
  setFixedHeight(34);

  setAttribute(Qt::WA_StyledBackground, true);

  setStyleSheet(
   "ArtifactTimeCodeWidget {"
   "  background-color: #2D2D2D;"
   "}"
   "QLabel#timeLabel {"
   "  font-family: 'Consolas', 'Courier New', monospace;"
   "  font-size: 12px;"
   "  font-weight: bold;"
   "  color: #5EA7EE;"
   "}"
   "QLabel#frameLabel {"
   "  font-family: 'Consolas', 'Courier New', monospace;"
   "  font-size: 9px;"
   "  color: #9A9A9A;"
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
        .arg(h, 1, 10)
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(ff, 2, 10, QChar('0'));

    if (impl_->timecodeLabel_) impl_->timecodeLabel_->setText(tc);
    if (impl_->frameNumberLabel_) impl_->frameNumberLabel_->setText(QString("(%1 f)").arg(totalFrames));

    // Example use of RationalTime API (kept for future extension)
    Q_UNUSED(rt);
 }

 W_OBJECT_IMPL(ArtifactTimelineSearchBarWidget)

 class ArtifactTimelineSearchBarWidget::Impl {
 public:
  Impl();
  QLineEdit* searchLineEdit_ = nullptr;
 };

 ArtifactTimelineSearchBarWidget::Impl::Impl()
 {
  searchLineEdit_ = new QLineEdit();
  searchLineEdit_->setPlaceholderText("検索");
 }

 ArtifactTimelineSearchBarWidget::ArtifactTimelineSearchBarWidget(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  auto layout = new QHBoxLayout(this);
  layout->setSpacing(8);
  layout->setContentsMargins(4, 0, 4, 0);

  impl_->searchLineEdit_->setObjectName("timelineSearchBox");
  
  // Set a clear button equivalent
  impl_->searchLineEdit_->setClearButtonEnabled(true);
  setFixedHeight(28);

  layout->addWidget(impl_->searchLineEdit_);

  setAttribute(Qt::WA_StyledBackground, true);

  setStyleSheet(
   "ArtifactTimelineSearchBarWidget {"
   "  background-color: #2D2D2D;"
   "}"
   "QLineEdit#timelineSearchBox {"
   "  background-color: #1E1E1E;"
   "  color: #CCCCCC;"
   "  border: 1px solid #3E3E3E;"
   "  border-radius: 10px;"
   "  padding: 0px 8px;"
   "  font-size: 9px;"
   "}"
   "QLineEdit#timelineSearchBox:focus {"
   "  border: 1px solid #007ACC;"
   "}"
  );

  QObject::connect(impl_->searchLineEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
   searchTextChanged(text);
  });
 }

 ArtifactTimelineSearchBarWidget::~ArtifactTimelineSearchBarWidget()
 {
  delete impl_;
 }

};
