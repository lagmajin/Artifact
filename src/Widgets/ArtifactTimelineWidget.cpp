module;

#include <QWidget>
#include <QLabel>
#include <wobjectimpl.h>
#include <QBoxLayout>
module ArtifactTimelineWidget;







namespace Artifact {

 class ArtifactTimeCodeWidget::Impl{
 private:
  QLabel* timecodeLabel=nullptr;
  QLabel* frameNumberLabel=nullptr;
 public:
  Impl();
 };

 ArtifactTimeCodeWidget::Impl::Impl()
 {
  timecodeLabel = new QLabel();
  frameNumberLabel = new QLabel();

 }

 ArtifactTimeCodeWidget::ArtifactTimeCodeWidget(QWidget* parent /*= nullptr*/):QWidget(parent),impl_(new Impl())
 {
  
  QVBoxLayout* layout = new QVBoxLayout();



  setLayout(layout);
 }

 ArtifactTimeCodeWidget::~ArtifactTimeCodeWidget()
 {

 }

 void ArtifactTimeCodeWidget::updateTimeCode(int frame)
 {

 }

 W_OBJECT_IMPL(ArtifactTimelineWidget)

 void ArtifactTimelineWidget::paintEvent(QPaintEvent* event)
 {

 }

 void ArtifactTimelineWidget::mousePressEvent(QMouseEvent* event)
 {

 }

 void ArtifactTimelineWidget::mouseMoveEvent(QMouseEvent* event)
 {

 }

 void ArtifactTimelineWidget::wheelEvent(QWheelEvent* event)
 {

 }



 ArtifactTimelineWidget::ArtifactTimelineWidget(QWidget* parent/*=nullptr*/)
 {

 }
 ArtifactTimelineWidget::~ArtifactTimelineWidget()
 {

 }

 void ArtifactTimelineWidget::keyPressEvent(QKeyEvent* event)
 {



  //throw std::logic_error("The method or operation is not implemented.");
 }

 void ArtifactTimelineWidget::keyReleaseEvent(QKeyEvent* event)
 {

 }



 TimelineTrackView::TimelineTrackView(QWidget* parent /*= nullptr*/)
 {

 }

 TimelineTrackView::~TimelineTrackView()
 {

 }

 void TimelineTrackView::setZoomLevel(double pixelsPerFrame)
 {

 }

};