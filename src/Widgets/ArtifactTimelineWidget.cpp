module;

#include <QWidget>
#include <QLabel>
#include <wobjectimpl.h>
#include <QBoxLayout>
#include <QSplitter>
#include <QStandardItem>
module Artifact.Widgets.Timeline;


import std;
import Widgets.Utils.CSS;

import Artifact.Layers.Hierarchy.Model;
import Artifact.Widget.WorkAreaControlWidget;


import ArtifactTimelineIconModel;
import Artifact.Timeline.Label;
import Artifact.Timeline.RulerWidget;
import Artifact.Timeline.ScaleWidget;
import Artifact.Timeline.TimeCodeWidget;
import Panel.DraggableSplitter;



namespace Artifact {

 using namespace ArtifactCore;
 using namespace ArtifactWidgets;



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

  setWindowFlags(Qt::FramelessWindowHint);

  setWindowTitle("TimelineWidget");

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




  auto* rightPanelLayout = new QVBoxLayout();
  rightPanelLayout->setSpacing(0);
  rightPanelLayout->setContentsMargins(0, 0, 0, 0);
  auto timeRulerWidget = new ArtifactTimelineRulerWidget();
  auto timeScaleWidget = new TimelineScaleWidget();
  auto workAreaWidget = new WorkAreaControl();
  auto timelineTrackView = new TimelineTrackView();


  auto rightPanel = new QWidget();

  rightPanelLayout->addWidget(timeRulerWidget);
  rightPanelLayout->addWidget(timeScaleWidget);
  rightPanelLayout->addWidget(workAreaWidget);
  rightPanelLayout->addWidget(timelineTrackView);
  rightPanel->setLayout(rightPanelLayout);



  // 全体のタイムラインスプリッター
  auto mainSplitter = new QSplitter(Qt::Horizontal);
  mainSplitter->setStyleSheet(R"(
    QSplitter::handle {
        background: #555555;
    }
)");
  mainSplitter->setHandleWidth(10);
  mainSplitter->addWidget(leftPanel);
  mainSplitter->addWidget(rightPanel);
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
 private:

 public:
  Impl();
  ~Impl();
 };

 TimelineTrackView::Impl::Impl()
 {

 }

 TimelineTrackView::Impl::~Impl()
 {

 }

 TimelineTrackView::TimelineTrackView(QWidget* parent /*= nullptr*/) :QGraphicsView(parent)
 {
  setScene(new TimelineScene());
  setRenderHint(QPainter::Antialiasing);

  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
 }

 TimelineTrackView::~TimelineTrackView()
 {

 }

 void TimelineTrackView::setZoomLevel(double pixelsPerFrame)
 {

 }

 void TimelineTrackView::drawBackground(QPainter* painter, const QRectF& rect)
 {
  painter->save();

  // 背景
  painter->fillRect(viewport()->rect(), QColor(30, 30, 30));

  // 線の色と幅
  QPen pen(QColor(80, 80, 80));
  pen.setWidth(1);
  painter->setPen(pen);

  int spacing = 20; // px単位
  int h = viewport()->height();
  int w = viewport()->width();

  // 横線
  for (int y = 0; y <= h; y += spacing) {
   painter->drawLine(0, y, w, y);
  }

  painter->restore();
 }

 void TimelineTrackView::drawForeground(QPainter* painter, const QRectF& rect)
 {
  //painter->fillRect()
 }


 void TimelineScene::drawBackground(QPainter* painter, const QRectF& rect)
 {
  //painter->fillRect(rect, Qt::darkGray);
 }

 TimelineScene::TimelineScene(QWidget* parent/*=nullptr*/) :QGraphicsScene(parent)
 {
  setSceneRect(0, 0, 1000, 800);
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
  //setHeaderHidden(true); // ヘッダー非表示も可
  setColumnWidth(0, 16); // アイコン列幅
  setColumnWidth(1, 16);
  setColumnWidth(2, 16);
  setColumnWidth(3, 16);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  int iconW = 24;
  header()->setIconSize(QSize(iconW, iconW));

  auto model = new ArtifactTimelineIconModel();

  setModel(model);
  header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);


  header()->resizeSection(0, 20);
  header()->setStretchLastSection(false);
 }

 ArtifactTimelineIconView::~ArtifactTimelineIconView()
 {

 }



};