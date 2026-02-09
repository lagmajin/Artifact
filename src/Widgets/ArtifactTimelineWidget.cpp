module;

#include <QWidget>
#include <QLabel>
#include <wobjectimpl.h>
#include <QBoxLayout>
#include <QSplitter>
#include <QStandardItem>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <cmath>
#include <algorithm>
module Artifact.Widgets.Timeline;


import std;
import Widgets.Utils.CSS;

import Artifact.Layers.Hierarchy.Model;
import Artifact.Widget.WorkAreaControlWidget;

import ArtifactTimelineIconModel;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Widgets.SeekBar;
import Artifact.Widgets.Timeline.Label;
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
  ArtifactTimelineBottomLabel* timelineLabel_ = nullptr;
 ArtifactLayerTimelinePanelWrapper* layerTimelinePanel_ = nullptr;

 };

 ArtifactTimelineWidget::Impl::Impl()
 {

 }

 ArtifactTimelineWidget::Impl::~Impl()
 {

 }

 ArtifactTimelineWidget::ArtifactTimelineWidget(QWidget* parent/*=nullptr*/) :QWidget(parent), impl_(new Impl())
 {

  setWindowFlags(Qt::FramelessWindowHint);

  setWindowTitle("TimelineWidget");

  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);

  setStyleSheet(style);

  //auto iconView = new ArtifactTimelineIconView();
  //iconView->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  //iconView->setFixedWidth(80);

  auto layerTreeView = new ArtifactLayerTimelinePanelWrapper();
  layerTreeView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  auto leftSplitter = new DraggableSplitter(Qt::Horizontal);
  //leftSplitter->addWidget(iconView);
  leftSplitter->addWidget(layerTreeView);
  leftSplitter->setStretchFactor(0, 0); // アイコン列は固定
  leftSplitter->setStretchFactor(1, 1); // 名前列は伸縮可能

  auto leftHeader = new ArtifactTimeCodeWidget(); // タイムコード+検索バー
  auto leftLayout = new QVBoxLayout();
  leftLayout->setSpacing(0);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  //leftLayout->addWidget(leftHeader);
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
  //auto layerTimelinePanel = new ArtifactLayerTimelinePanelWrapper();
  //layerTimelinePanel->setMinimumWidth(220);
  //layerTimelinePanel->setMaximumWidth(320);

  auto* trackSplitter = new DraggableSplitter(Qt::Horizontal);
  //trackSplitter->addWidget(layerTimelinePanel);
  trackSplitter->addWidget(timelineTrackView);
  trackSplitter->setStretchFactor(0, 0);
  trackSplitter->setStretchFactor(1, 1);
  trackSplitter->setHandleWidth(6);


  auto rightPanel = new QWidget();

  rightPanelLayout->addWidget(timeRulerWidget);
  rightPanelLayout->addWidget(timeScaleWidget);
  rightPanelLayout->addWidget(workAreaWidget);
  rightPanelLayout->addWidget(trackSplitter);
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

  auto label = new ArtifactTimelineBottomLabel();


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
  double position_ = 0.0;
  double duration_ = 1.0;
  double zoomLevel_ = 1.0;
  double minZoomLevel_ = 0.1;
  double maxZoomLevel_ = 10.0;
  bool isPanning_ = false;
  QPoint lastPanPoint_;
 };

 TimelineTrackView::Impl::Impl()
 {

 }

 TimelineTrackView::Impl::~Impl()
 {

 }

TimelineTrackView::TimelineTrackView(QWidget* parent /*= nullptr*/) :QGraphicsView(parent),impl_(new Impl())
{
 setScene(new TimelineScene());
 setRenderHint(QPainter::Antialiasing);

 setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
 setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
 setResizeAnchor(QGraphicsView::AnchorViewCenter);
 setDragMode(QGraphicsView::NoDrag);

 setZoomLevel(impl_->zoomLevel_);
}

 TimelineTrackView::~TimelineTrackView()
 {
  delete impl_;
 }

 double TimelineTrackView::position() const
 {
  return impl_->position_;
 }

 void TimelineTrackView::setPosition(double position)
 {
  impl_->position_ = qBound(0.0, position, impl_->duration_);
  viewport()->update();
 }

 double TimelineTrackView::duration() const
 {
  return impl_->duration_;
 }

 void TimelineTrackView::setDuration(double duration)
 {
  impl_->duration_ = std::max(0.0, duration);
  if (auto scene = this->scene()) {
   QRectF rect = scene->sceneRect();
   rect.setWidth(std::max(rect.width(), impl_->duration_));
   scene->setSceneRect(rect);
  }
  viewport()->update();
 }

 double TimelineTrackView::zoomLevel() const
 {
  return impl_->zoomLevel_;
 }

 namespace {
 double gridSpacingForZoom(double zoomLevel)
 {
  if (zoomLevel < 0.15) {
   return 400.0;
  }
  if (zoomLevel < 0.3) {
   return 200.0;
  }
  if (zoomLevel < 0.6) {
   return 100.0;
  }
  if (zoomLevel < 1.0) {
   return 50.0;
  }
  if (zoomLevel < 2.0) {
   return 25.0;
  }
  if (zoomLevel < 4.0) {
   return 10.0;
  }
  return 5.0;
 }
 }

 void TimelineTrackView::setZoomLevel(double pixelsPerFrame)
 {
  double clamped = std::clamp(pixelsPerFrame, impl_->minZoomLevel_, impl_->maxZoomLevel_);
  if (std::abs(clamped - impl_->zoomLevel_) < 1e-5) {
   return;
  }

  const QPointF center = mapToScene(viewport()->rect().center());
  impl_->zoomLevel_ = clamped;
  QTransform transform;
  transform.scale(impl_->zoomLevel_, 1.0);
  setTransform(transform);
  centerOn(center);
  viewport()->update();
 }

 void TimelineTrackView::drawBackground(QPainter* painter, const QRectF& rect)
 {
  painter->save();
  painter->setWorldTransform(QTransform());
  painter->fillRect(viewport()->rect(), QColor(30, 30, 30));
  painter->restore();

  painter->save();
  QPen verticalPen(QColor(70, 70, 70));
  verticalPen.setWidth(1);
  painter->setPen(verticalPen);

  double spacing = gridSpacingForZoom(impl_->zoomLevel_);
  double left = std::max(0.0, rect.left());
  double right = std::max(left, rect.right());
  double rangeEnd = impl_->duration_ > 0.0 ? std::min(impl_->duration_, right) : right;
  double startLine = std::floor(left / spacing) * spacing;
  double endLine = std::ceil(rangeEnd / spacing) * spacing;

  for (double x = startLine; x <= endLine; x += spacing) {
   if (x < 0.0) {
    continue;
   }
   painter->drawLine(QLineF(x, rect.top(), x, rect.bottom()));
  }
  painter->restore();

  painter->save();
  QPen horizontalPen(QColor(60, 60, 60));
  horizontalPen.setWidth(1);
  painter->setPen(horizontalPen);

  const double rowSpacing = 20.0;
  double rowStart = std::floor(rect.top() / rowSpacing) * rowSpacing;
  for (double y = rowStart; y <= rect.bottom(); y += rowSpacing) {
   painter->drawLine(QLineF(rect.left(), y, rect.right(), y));
  }
  painter->restore();
 }

 void TimelineTrackView::drawForeground(QPainter* painter, const QRectF& rect)
 {
  painter->save();
  QPen headPen(QColor(255, 80, 60));
  headPen.setWidth(2);
  painter->setPen(headPen);
  painter->drawLine(QLineF(impl_->position_, rect.top(), impl_->position_, rect.bottom()));
  painter->restore();

  painter->save();
  painter->setWorldTransform(QTransform());
  QRectF viewRect = viewport()->rect();
  QRectF barRect(0, viewRect.height() - 30, viewRect.width(), 30);
  painter->fillRect(barRect, QColor(28, 28, 28, 220));
  painter->restore();
 }

 QSize TimelineTrackView::minimumSizeHint() const
 {
  return QSize(600, 600);
 }

 void TimelineTrackView::mousePressEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::MiddleButton) {
   impl_->isPanning_ = true;
   impl_->lastPanPoint_ = event->pos();
   setCursor(Qt::ClosedHandCursor);
   event->accept();
   return;
  }

  if (event->button() == Qt::LeftButton) {
   const QPointF scenePos = mapToScene(event->pos());
   setPosition(scenePos.x());
   double ratio = impl_->duration_ > 0.0 ? impl_->position_ / impl_->duration_ : 0.0;
   emit seekPositionChanged(ratio);
  }

  QGraphicsView::mousePressEvent(event);
 }

 void TimelineTrackView::mouseMoveEvent(QMouseEvent* event)
 {
  if (impl_->isPanning_) {
   QPoint delta = event->pos() - impl_->lastPanPoint_;
   impl_->lastPanPoint_ = event->pos();
   horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
   verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
   event->accept();
   return;
  }

  QGraphicsView::mouseMoveEvent(event);
 }

 void TimelineTrackView::mouseReleaseEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::MiddleButton && impl_->isPanning_) {
   impl_->isPanning_ = false;
   setCursor(Qt::ArrowCursor);
   event->accept();
   return;
  }

  QGraphicsView::mouseReleaseEvent(event);
 }

 void TimelineTrackView::wheelEvent(QWheelEvent* event)
 {
  if (event->modifiers() & Qt::ControlModifier) {
   double delta = event->angleDelta().y() / 120.0;
   double factor = std::pow(1.15, delta);
   setZoomLevel(impl_->zoomLevel_ * factor);
   event->accept();
   return;
  }

  auto* hBar = horizontalScrollBar();
  if (hBar) {
   hBar->setValue(hBar->value() - event->angleDelta().y());
  } else {
   QGraphicsView::wheelEvent(event);
  }
  event->accept();
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