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
  auto timecodeLabel = impl_->timecodeLabel_ = new QLabel();
  timecodeLabel->setText("00:00:00:00");
  auto frameNumberLabel = impl_->frameNumberLabel_ = new QLabel();
  frameNumberLabel->setText("fps");

  QFont monoFont("Consolas");
  monoFont.setPixelSize(24);
  timecodeLabel->setFont(monoFont);
  frameNumberLabel->setFont(monoFont);

  vLayout->addWidget(timecodeLabel);
  vLayout->addWidget(frameNumberLabel);


  auto layout = new QHBoxLayout();
  layout->addLayout(vLayout);

  setLayout(layout);
 }



 ArtifactTimeCodeWidget::~ArtifactTimeCodeWidget()
 {

 }

 void ArtifactTimeCodeWidget::updateTimeCode(int frame)
 {

 }


};