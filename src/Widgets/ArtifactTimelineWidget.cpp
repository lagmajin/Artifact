module;

#include <QWidget>
#include <QLabel>
#include <wobjectimpl.h>
#include <QBoxLayout>
#include <QSplitter>
#include <QStandardItem>
module Artifact.Widgets.Timeline;

import Widgets.Utils.CSS;

import Artifact.Layers.Hierarchy.Model;

import Panel.DraggableSplitter;

namespace Artifact {

 using namespace ArtifactCore;
 using namespace ArtifactWidgets;

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
  auto timecodeLabel = impl_->timecodeLabel_ = new QLabel();
  timecodeLabel->setText("Time");
  auto frameNumberLabel = impl_->frameNumberLabel_ = new QLabel();
  frameNumberLabel->setText("fps");

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


 W_OBJECT_IMPL(ArtifactTimelineWidget)


  class ArtifactTimelineWidget::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
  TimelineLabel* timelineLabel_ = nullptr;

 };

 ArtifactTimelineWidget::Impl::Impl()
 {

 }

 ArtifactTimelineWidget::Impl::~Impl()
 {

 }

 ArtifactTimelineWidget::ArtifactTimelineWidget(QWidget* parent/*=nullptr*/) :QWidget(parent)
 {
  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);

  setStyleSheet(style);

  auto iconView = new ArtifactTimelineIconView();
  iconView->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  iconView->setFixedWidth(80);

  auto layerTreeView = new ArtifactLayerHierarchyView();
  layerTreeView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  auto leftSplitter = new DraggableSplitter(Qt::Horizontal);
  leftSplitter->addWidget(iconView);
  leftSplitter->addWidget(layerTreeView);
  leftSplitter->setStretchFactor(0, 0); // アイコン列は固定
  leftSplitter->setStretchFactor(1, 1); // 名前列は伸縮可能

  auto leftHeader = new ArtifactTimeCodeWidget(); // タイムコード+検索バー
  auto leftLayout = new QVBoxLayout();
  leftLayout->setSpacing(0);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->addWidget(leftHeader);
  leftLayout->addWidget(leftSplitter);

  auto leftPanel = new QWidget();
  leftPanel->setLayout(leftLayout);





  // 全体のタイムラインスプリッター
  auto mainSplitter = new QSplitter(Qt::Horizontal);
  mainSplitter->addWidget(leftPanel);
 	//mainSplitter->addWidget(leftSplitter);
  
  mainSplitter->setStretchFactor(0, 1);
  mainSplitter->setStretchFactor(1, 3);

  auto label = new TimelineLabel();


  auto layout = new QVBoxLayout();
  layout->addWidget(mainSplitter);
  layout->addWidget(label);
  layout->setSpacing(0);
  layout->setContentsMargins(0, 0, 0, 0);

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

  class TimelineTrackView::Impl
 {
 public:

 };

 TimelineTrackView::TimelineTrackView(QWidget* parent /*= nullptr*/) :QGraphicsView(parent)
 {
  setScene(new TimelineScene());

 }

 TimelineTrackView::~TimelineTrackView()
 {

 }

 void TimelineTrackView::setZoomLevel(double pixelsPerFrame)
 {

 }

 void TimelineTrackView::drawBackground(QPainter* painter, const QRectF& rect)
 {
  //painter->fillRect(rect, Qt::white);

  QGraphicsView::drawBackground(painter, rect);
 }

 void TimelineTrackView::drawForeground(QPainter* painter, const QRectF& rect)
 {
  //painter->fillRect()
 }


 void TimelineScene::drawBackground(QPainter* painter, const QRectF& rect)
 {
  painter->fillRect(rect, Qt::white);
 }

 class ArtifactTimelineIconView::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactTimelineIconView::Impl::Impl()
 {

 }

 ArtifactTimelineIconView::Impl::~Impl()
 {

 }

 ArtifactTimelineIconView::ArtifactTimelineIconView(QWidget* parent /*= nullptr*/) :QTreeView(parent)
 {
  setHeaderHidden(true); // ヘッダー非表示も可
  setColumnWidth(0, 20); // アイコン列幅
  setColumnWidth(1, 20);
  setColumnWidth(2, 20);
  setColumnWidth(3, 20);
  setSelectionBehavior(QAbstractItemView::SelectRows);




 }

 ArtifactTimelineIconView::~ArtifactTimelineIconView()
 {

 }

 class TimelineLabel::Impl
 {
 private:

 public:
  QLabel* frameRenderingLabel = nullptr;
 };

 TimelineLabel::TimelineLabel(QWidget* parent /*= nullptr*/) :QWidget(parent)
 {


 	auto layout = new QHBoxLayout();


  setLayout(layout);

  setFixedHeight(28);
 }

 TimelineLabel::~TimelineLabel()
 {

 }

};