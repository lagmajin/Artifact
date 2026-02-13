module;

#include <QWidget>
#include <QLabel>
#include <wobjectimpl.h>
#include <wobjectdefs.h>
#include <QBoxLayout>
#include <QSplitter>
#include <QStandardItem>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QResizeEvent>
#include <cmath>
#include <algorithm>
#include <vector>
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
import Artifact.Timeline.Objects;



namespace Artifact {

using namespace ArtifactCore;
using namespace ArtifactWidgets;

// ===== TimelineScene Implementation =====
class TimelineScene::Impl {
public:
 std::vector<double> trackHeights_;
 std::vector<ClipItem*> clips_;
 std::vector<ClipItem*> selectedClips_;
 TimelineScene* parent_;
  
 Impl(TimelineScene* parent) : parent_(parent) {}
 ~Impl() {
  for (auto clip : clips_) {
   parent_->removeItem(clip);
   delete clip;
  }
  clips_.clear();
 }
  
 double getTotalTrackHeight() const {
  double total = 0.0;
  for (double height : trackHeights_) {
   total += height;
  }
  return total;
 }
};

TimelineScene::TimelineScene(QWidget* parent) : QGraphicsScene(parent), impl_(new Impl(this))
{
 setSceneRect(0, 0, 2000, 800);
 impl_->trackHeights_.push_back(20.0);
}

TimelineScene::~TimelineScene()
{
 delete impl_;
}

void TimelineScene::drawBackground(QPainter* painter, const QRectF& rect)
{
 painter->fillRect(rect, QColor(30, 30, 30));
}

int TimelineScene::addTrack(double height)
{
 int trackIndex = static_cast<int>(impl_->trackHeights_.size());
 impl_->trackHeights_.push_back(height);
 double newHeight = impl_->getTotalTrackHeight();
 QRectF sr = sceneRect();
 sr.setHeight(newHeight);
 setSceneRect(sr);
 return trackIndex;
}

void TimelineScene::removeTrack(int trackIndex)
{
 if (trackIndex < 0 || trackIndex >= static_cast<int>(impl_->trackHeights_.size())) {
  return;
 }
  
 auto it = impl_->clips_.begin();
 while (it != impl_->clips_.end()) {
  bool isInTrack = false;
  double yPos = (*it)->pos().y();
  double trackY = 0.0;
  for (int i = 0; i < trackIndex && i < static_cast<int>(impl_->trackHeights_.size()); ++i) {
   trackY += impl_->trackHeights_[i];
  }
  if (yPos >= trackY && yPos < trackY + impl_->trackHeights_[trackIndex]) {
   isInTrack = true;
  }
   
  if (isInTrack) {
   removeItem(*it);
   delete *it;
   it = impl_->clips_.erase(it);
  } else {
   ++it;
  }
 }
  
 impl_->trackHeights_.erase(impl_->trackHeights_.begin() + trackIndex);
 double newHeight = impl_->getTotalTrackHeight();
 QRectF sr = sceneRect();
 sr.setHeight(newHeight);
 setSceneRect(sr);
}

int TimelineScene::trackCount() const
{
 return static_cast<int>(impl_->trackHeights_.size());
}

double TimelineScene::trackHeight(int trackIndex) const
{
 if (trackIndex < 0 || trackIndex >= static_cast<int>(impl_->trackHeights_.size())) {
  return 0.0;
 }
 return impl_->trackHeights_[trackIndex];
}

void TimelineScene::setTrackHeight(int trackIndex, double height)
{
 if (trackIndex < 0 || trackIndex >= static_cast<int>(impl_->trackHeights_.size())) {
  return;
 }
 impl_->trackHeights_[trackIndex] = std::max(10.0, height);
 double newHeight = impl_->getTotalTrackHeight();
 QRectF sr = sceneRect();
 sr.setHeight(newHeight);
 setSceneRect(sr);
 update();
}

double TimelineScene::getTrackYPosition(int trackIndex) const
{
 if (trackIndex < 0 || trackIndex >= static_cast<int>(impl_->trackHeights_.size())) {
  return 0.0;
 }
 double yPos = 0.0;
 for (int i = 0; i < trackIndex; ++i) {
  yPos += impl_->trackHeights_[i];
 }
 return yPos;
}

ClipItem* TimelineScene::addClip(int trackIndex, double start, double duration)
{
 if (trackIndex < 0 || trackIndex >= static_cast<int>(impl_->trackHeights_.size())) {
  return nullptr;
 }
  
 double yPos = getTrackYPosition(trackIndex);
 double height = impl_->trackHeights_[trackIndex];
  
 auto clip = new ClipItem(start, duration, height);
 clip->setPos(start, yPos);
 addItem(clip);
 impl_->clips_.push_back(clip);
 return clip;
}

void TimelineScene::removeClip(ClipItem* clip)
{
 auto it = std::find(impl_->clips_.begin(), impl_->clips_.end(), clip);
 if (it != impl_->clips_.end()) {
  impl_->selectedClips_.erase(std::remove(impl_->selectedClips_.begin(), impl_->selectedClips_.end(), clip),
                               impl_->selectedClips_.end());
  removeItem(clip);
  delete clip;
  impl_->clips_.erase(it);
 }
}

const std::vector<ClipItem*>& TimelineScene::getClips() const
{
 return impl_->clips_;
}

int TimelineScene::getTrackAtPosition(double yPos) const
{
 double currentY = 0.0;
 for (int i = 0; i < static_cast<int>(impl_->trackHeights_.size()); ++i) {
  if (yPos >= currentY && yPos < currentY + impl_->trackHeights_[i]) {
   return i;
  }
  currentY += impl_->trackHeights_[i];
 }
 return -1;
}

void TimelineScene::clearSelection()
{
 impl_->selectedClips_.clear();
}

const std::vector<ClipItem*>& TimelineScene::getSelectedClips() const
{
 return impl_->selectedClips_;
}

// ===== ArtifactTimelineWidget Implementation =====

W_OBJECT_IMPL(ArtifactTimelineWidget)


  class ArtifactTimelineWidget::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
  ArtifactTimelineBottomLabel* timelineLabel_ = nullptr;
  ArtifactLayerTimelinePanelWrapper* layerTimelinePanel_ = nullptr;
  CompositionID compositionId_;

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
  impl_->layerTimelinePanel_ = layerTreeView;

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

 void ArtifactTimelineWidget::setComposition(const CompositionID& id)
 {
  impl_->compositionId_ = id;
  if (impl_->layerTimelinePanel_) {
   impl_->layerTimelinePanel_->setComposition(id);
  }
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

 // ===== TimelineTrackView Implementation =====

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
  TimelineScene* scene_ = nullptr;
  ClipItem* draggedClip_ = nullptr;
  ClipItem* lastSelectedClip_ = nullptr;
 };

 TimelineTrackView::Impl::Impl()
 {

 }

 TimelineTrackView::Impl::~Impl()
 {

 }

TimelineTrackView::TimelineTrackView(QWidget* parent /*= nullptr*/) :QGraphicsView(parent),impl_(new Impl())
{
 auto scene = new TimelineScene();
 impl_->scene_ = scene;
 setScene(scene);
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
  if (impl_->scene_) {
   QRectF rect = impl_->scene_->sceneRect();
   rect.setWidth(std::max(rect.width(), impl_->duration_));
   impl_->scene_->setSceneRect(rect);
  }
  viewport()->update();
 }

 double TimelineTrackView::zoomLevel() const
 {
  return impl_->zoomLevel_;
 }

 TimelineScene* TimelineTrackView::timelineScene() const
 {
  return impl_->scene_;
 }

 int TimelineTrackView::addTrack(double height)
 {
  if (impl_->scene_) {
   return impl_->scene_->addTrack(height);
  }
  return -1;
 }

 void TimelineTrackView::removeTrack(int trackIndex)
 {
  if (impl_->scene_) {
   impl_->scene_->removeTrack(trackIndex);
   viewport()->update();
  }
 }

 ClipItem* TimelineTrackView::addClip(int trackIndex, double start, double duration)
 {
  if (impl_->scene_) {
   return impl_->scene_->addClip(trackIndex, start, duration);
  }
  return nullptr;
 }

 void TimelineTrackView::removeClip(ClipItem* clip)
 {
  if (impl_->scene_ && clip) {
   impl_->scene_->removeClip(clip);
   viewport()->update();
  }
 }

 void TimelineTrackView::clearSelection()
 {
  if (impl_->scene_) {
   impl_->scene_->clearSelection();
   impl_->lastSelectedClip_ = nullptr;
   viewport()->update();
  }
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
    
    // Check if clicking on a clip
    auto items = scene()->items(scenePos, Qt::IntersectsItemShape);
    ClipItem* clickedClip = nullptr;
    for (auto item : items) {
     if (auto clip = dynamic_cast<ClipItem*>(item)) {
      clickedClip = clip;
      break;
     }
    }
    
    if (clickedClip) {
     // Clip selected/deselected
     bool isSelected = clickedClip->isSelected();
     if (!isSelected && !(event->modifiers() & Qt::ShiftModifier)) {
      clearSelection();
     }
     clickedClip->setSelected(!isSelected);
     
     if (clickedClip->isSelected()) {
      impl_->lastSelectedClip_ = clickedClip;
      emit clipSelected(clickedClip);
     } else {
      emit clipDeselected(clickedClip);
     }
    } else {
     // Click on timeline for seeking
     setPosition(scenePos.x());
     double ratio = impl_->duration_ > 0.0 ? impl_->position_ / impl_->duration_ : 0.0;
     emit seekPositionChanged(ratio);
     
     if (!(event->modifiers() & Qt::ShiftModifier)) {
      clearSelection();
     }
    }
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

  void TimelineTrackView::resizeEvent(QResizeEvent* event)
  {
   QGraphicsView::resizeEvent(event);
   if (impl_->scene_) {
    QRectF rect = impl_->scene_->sceneRect();
    rect.setHeight(std::max(rect.height(), static_cast<double>(event->size().height())));
    rect.setWidth(std::max(impl_->duration_, static_cast<double>(event->size().width())));
    impl_->scene_->setSceneRect(rect);
   }
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