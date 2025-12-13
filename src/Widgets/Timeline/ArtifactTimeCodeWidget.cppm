module;
#include <QLabel>
#include <QBoxLayout>
#include <wobjectimpl.h>

module Artifact.Timeline.TimeCodeWidget;


namespace Artifact
{
 W_OBJECT_IMPL(ArtifactTimeCodeWidget)
  class ArtifactTimeCodeWidget::Impl {
  private:

  public:
   Impl();
   QLabel* timecodeLabel_ = nullptr;
   QLabel* frameNumberLabel_ = nullptr;
 };

 ArtifactTimeCodeWidget::Impl::Impl()
 {
  //timecodeLabel_ = new QLabel();
  //timecodeLabel_->setText("Time:");
  frameNumberLabel_ = new QLabel();

 }

 ArtifactTimeCodeWidget::ArtifactTimeCodeWidget(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  auto vLayout = new QVBoxLayout();
  vLayout->setSpacing(0);
  vLayout->setContentsMargins(0, 0, 0, 0);
  vLayout->setAlignment(Qt::AlignTop);
  auto timecodeLabel = impl_->timecodeLabel_ = new QLabel();
  timecodeLabel->setObjectName("timeLabel");
  timecodeLabel->setText("00:00:00:00");
  auto frameNumberLabel = impl_->frameNumberLabel_ = new QLabel();
  frameNumberLabel->setObjectName("frameLabel");
  frameNumberLabel->setText("fps");
 	

  vLayout->addWidget(timecodeLabel);
  vLayout->addWidget(frameNumberLabel);


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

 }

 void ArtifactTimeCodeWidget::updateTimeCode(int frame)
 {

 }


};