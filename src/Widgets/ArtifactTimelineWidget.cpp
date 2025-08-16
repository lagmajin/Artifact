module;

#include <QWidget>
#include <QLabel>
#include <wobjectimpl.h>
#include <QBoxLayout>
module Artifact.Widgets.Timeline;

import Widgets.Utils.CSS;

import Artifact.Layers.Hierarchy.Model;



namespace Artifact {

 using namespace ArtifactCore;


 W_OBJECT_IMPL(ArtifactTimeCodeWidget)
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


	class ArtifactTimelineWidget::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactTimelineWidget::Impl::Impl()
 {

 }

 ArtifactTimelineWidget::Impl::~Impl()
 {

 }

 ArtifactTimelineWidget::ArtifactTimelineWidget(QWidget* parent/*=nullptr*/):QWidget(parent)
 {
  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);

  setStyleSheet(style);

  auto layerTreeView = new ArtifactLayerHierarchyView();

  QHBoxLayout* layout = new QHBoxLayout();
  layout->addWidget(layerTreeView);
  

  setLayout(layout);

 }
 ArtifactTimelineWidget::~ArtifactTimelineWidget()
 {

 }
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





 void ArtifactTimelineWidget::keyPressEvent(QKeyEvent* event)
 {



  //throw std::logic_error("The method or operation is not implemented.");
 }

 void ArtifactTimelineWidget::keyReleaseEvent(QKeyEvent* event)
 {

 }

 W_OBJECT_IMPL(TimelineTrackView)

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