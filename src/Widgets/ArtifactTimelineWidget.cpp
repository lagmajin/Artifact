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
#include <QGraphicsRectItem>
#include <QBrush>
#include <QResizeEvent>
#include <QEvent>
#include <QPainter>
#include <QMenu>
#include <cmath>
#include <algorithm>
#include <vector>
#include <qtmetamacros.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Widgets.Timeline;





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
import Artifact.Widgets.Timeline.GlobalSwitches;
import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Frame.Position;



namespace Artifact {

using namespace ArtifactCore;
using namespace ArtifactWidgets;

 namespace {
  constexpr double kTimelineRowHeight = 28.0;
  constexpr int kTimelineTopRowHeight = 16;   // aligns with right ruler row
  constexpr int kTimelineHeaderRowHeight = 34; // fits timecode + frame labels without compression
  constexpr int kDefaultTimelineFrames = 300;
  std::shared_ptr<ArtifactAbstractComposition> safeCompositionLookup(const CompositionID& id)
  {
    if (id.isNil()) return nullptr;
    auto* service = ArtifactProjectService::instance();
    if (!service) return nullptr;
    auto result = service->findComposition(id);
    if (!result.success) return nullptr;
    return result.ptr.lock();
  }

  class TimelinePlayheadOverlay final : public QWidget
  {
  public:
   explicit TimelinePlayheadOverlay(QWidget* parent = nullptr) : QWidget(parent)
   {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
   }

   void setPlayheadX(const int x)
   {
    if (playheadX_ == x) {
     return;
    }
    playheadX_ = x;
    update();
   }

  protected:
   void paintEvent(QPaintEvent* event) override
   {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(QColor(255, 80, 60, 220), 1));
    painter.drawLine(playheadX_, 0, playheadX_, height());
   }

  private:
   int playheadX_ = 0;
  };

  class PlayheadSyncFilter final : public QObject
  {
  public:
   PlayheadSyncFilter(QWidget* host, TimelineTrackView* trackView, TimelinePlayheadOverlay* overlay, QObject* parent = nullptr)
    : QObject(parent), host_(host), trackView_(trackView), overlay_(overlay)
   {
   }

   void sync()
   {
    if (!host_ || !trackView_ || !overlay_ || !trackView_->viewport()) {
     return;
    }
    overlay_->setGeometry(host_->rect());
    overlay_->raise();

    const QPoint viewportPos = trackView_->mapFromScene(QPointF(trackView_->position(), 0.0));
    const int xInHost = trackView_->viewport()->mapTo(host_, viewportPos).x();
    overlay_->setPlayheadX(xInHost);
   }

  protected:
   bool eventFilter(QObject* watched, QEvent* event) override
   {
    Q_UNUSED(watched);
    switch (event->type()) {
    case QEvent::Resize:
    case QEvent::LayoutRequest:
    case QEvent::Paint:
    case QEvent::Show:
    case QEvent::Move:
     sync();
     break;
    default:
     break;
    }
    return QObject::eventFilter(watched, event);
   }

  private:
   QWidget* host_ = nullptr;
   TimelineTrackView* trackView_ = nullptr;
   TimelinePlayheadOverlay* overlay_ = nullptr;
  };
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
    TimelineTrackView* trackView_ = nullptr;  // Right-side timeline view
    ArtifactSeekBar* seekBar_ = nullptr;
    TimelinePlayheadOverlay* playheadOverlay_ = nullptr;
    PlayheadSyncFilter* playheadSync_ = nullptr;
    WorkAreaControl* workArea_ = nullptr;
    ArtifactTimelineRulerWidget* ruler_ = nullptr;
    CompositionID compositionId_;
    bool shyActive_ = false;
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
  leftSplitter->setStretchFactor(0, 0); // ACR͌Œ
  leftSplitter->setStretchFactor(1, 1); // O͐Lk\

  auto leftHeader = new ArtifactTimeCodeWidget(); // ^CR[h
  auto searchBar = new ArtifactTimelineSearchBarWidget(); // o[
  auto globalSwitches = new ArtifactTimelineGlobalSwitches(); // AE{^Q
  searchBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  searchBar->setFixedWidth(260);

  auto searchBarLayout = new QHBoxLayout();
  searchBarLayout->setSpacing(8);
  searchBarLayout->setContentsMargins(0, 0, 8, 0);
  searchBarLayout->addWidget(leftHeader);
  searchBarLayout->addWidget(searchBar);
  searchBarLayout->addWidget(globalSwitches);
  searchBarLayout->addStretch(1);
  searchBarLayout->setStretch(0, 0);
  searchBarLayout->setStretch(1, 0);
  searchBarLayout->setStretch(2, 0);
  searchBarLayout->setStretch(3, 1);
 
   QObject::connect(globalSwitches, &ArtifactTimelineGlobalSwitches::shyChanged, 
                    this, &ArtifactTimelineWidget::onShyChanged);
  
  auto headerWidget = new QWidget();
  headerWidget->setLayout(searchBarLayout);
  headerWidget->setFixedHeight(kTimelineHeaderRowHeight);

  auto leftTopSpacer = new QWidget();
  leftTopSpacer->setFixedHeight(kTimelineTopRowHeight);
  leftTopSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  leftTopSpacer->setStyleSheet("background-color: #2D2D30; border-bottom: 1px solid #1a1a1a;");

  auto leftLayout = new QVBoxLayout();
  leftLayout->setSpacing(0);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->addWidget(leftTopSpacer);
  leftLayout->addWidget(headerWidget);
  leftLayout->addWidget(leftSplitter);

  auto leftPanel = new QWidget();
  leftPanel->setLayout(leftLayout);




  auto* rightPanelLayout = new QVBoxLayout();
  rightPanelLayout->setSpacing(0);
  rightPanelLayout->setContentsMargins(0, 0, 0, 0);
    auto timeRulerWidget = impl_->ruler_ = new ArtifactTimelineRulerWidget();
    auto timeScaleWidget = new TimelineScaleWidget();
    auto workAreaWidget = impl_->workArea_ = new WorkAreaControl();
    auto seekBar = impl_->seekBar_ = new ArtifactSeekBar();
    auto timelineTrackView = impl_->trackView_ = new TimelineTrackView();
    timelineTrackView->setDuration(kDefaultTimelineFrames);
    timeRulerWidget->setFixedHeight(kTimelineTopRowHeight);
    timeRulerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    seekBar->setFixedHeight(kTimelineHeaderRowHeight);
    seekBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    seekBar->setTotalFrames(kDefaultTimelineFrames);
    seekBar->setCurrentFrame(FramePosition(0));
 
    // Sync Ruler and WorkArea
    // Sync disabled
    
    
    
 
    // Zoom/Scale Integration
    auto updateZoom = [this]() {
      if (!impl_->trackView_ || !impl_->workArea_) return;
      double duration = impl_->trackView_->duration();
      float s = impl_->ruler_->startValue();
      float e = impl_->ruler_->endValue();
      double range = std::max(0.01f, e - s);
      
      int viewW = impl_->trackView_->viewport()->width();
      if (viewW > 0) {
        double newZoom = viewW / (duration * range);
        impl_->trackView_->setZoomLevel(newZoom);
        
        if (auto* hBar = impl_->trackView_->horizontalScrollBar()) {
          hBar->setValue(static_cast<int>(s * duration * newZoom));
        }
      }
    };

    QObject::connect(timeRulerWidget, &ArtifactTimelineRulerWidget::startChanged, this, updateZoom);
    QObject::connect(timeRulerWidget, &ArtifactTimelineRulerWidget::endChanged, this, updateZoom);
    QObject::connect(seekBar, &ArtifactSeekBar::frameChanged, this, [timelineTrackView](const auto& frame) {
      timelineTrackView->setPosition(static_cast<double>(frame.framePosition()));
    });
    QObject::connect(timelineTrackView, &TimelineTrackView::seekPositionChanged, this, [timelineTrackView, seekBar](double ratio) {
      const double duration = timelineTrackView->duration();
      const int frame = static_cast<int>(std::round(std::clamp(ratio, 0.0, 1.0) * duration));
      seekBar->setCurrentFrame(FramePosition(frame));
    });

    impl_->trackView_ = timelineTrackView;  // Store reference for layer creation
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
  rightPanelLayout->addWidget(seekBar);
  rightPanelLayout->addWidget(timeScaleWidget);
  rightPanelLayout->addWidget(workAreaWidget);
  rightPanelLayout->addWidget(trackSplitter);
  rightPanel->setLayout(rightPanelLayout);

  auto* playheadOverlay = new TimelinePlayheadOverlay(rightPanel);
  auto* playheadSync = new PlayheadSyncFilter(rightPanel, timelineTrackView, playheadOverlay, rightPanel);
  rightPanel->installEventFilter(playheadSync);
  if (timelineTrackView->viewport()) {
   timelineTrackView->viewport()->installEventFilter(playheadSync);
  }
  if (auto* hBar = timelineTrackView->horizontalScrollBar()) {
   QObject::connect(hBar, &QScrollBar::valueChanged, this, [playheadSync](int) { playheadSync->sync(); });
  }
  QObject::connect(seekBar, &ArtifactSeekBar::frameChanged, this, [playheadSync](const auto&) { playheadSync->sync(); });
  impl_->playheadOverlay_ = playheadOverlay;
  impl_->playheadSync_ = playheadSync;
  playheadSync->sync();



  // Ŝ̃^CCXvb^[
  auto mainSplitter = new QSplitter(Qt::Horizontal);
  mainSplitter->setStyleSheet(R"(
    QSplitter::handle {
        background: #555555;
    }
)");
  mainSplitter->setHandleWidth(7);
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

  // Sync scrollbars
  if (auto* leftScroll = layerTreeView->verticalScrollBar()) {
   if (auto* rightScroll = timelineTrackView->verticalScrollBar()) {
    QObject::connect(leftScroll, &QScrollBar::valueChanged, rightScroll, &QScrollBar::setValue);
    QObject::connect(rightScroll, &QScrollBar::valueChanged, leftScroll, &QScrollBar::setValue);
   }
  }

  // Connect Layer Signals
  if (auto* svc = ArtifactProjectService::instance()) {
   QObject::connect(svc, &ArtifactProjectService::layerCreated, this, &ArtifactTimelineWidget::onLayerCreated);
   QObject::connect(svc, &ArtifactProjectService::layerRemoved, this, &ArtifactTimelineWidget::onLayerRemoved);
  }

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

   if (impl_->trackView_) {
    impl_->trackView_->clearTracks();
    if (auto svc = ArtifactProjectService::instance()) {
     auto res = svc->findComposition(id);
     if (res.success && !res.ptr.expired()) {
      auto comp = res.ptr.lock();
      int totalFrames = static_cast<int>(std::round(comp->frameRange().duration()));
      if (totalFrames < 2) {
       totalFrames = kDefaultTimelineFrames;
      }
      impl_->trackView_->setDuration(static_cast<double>(totalFrames));
      if (impl_->seekBar_) {
       impl_->seekBar_->setTotalFrames(std::max(1, totalFrames));
       impl_->seekBar_->setCurrentFrame(FramePosition(0));
      }
      if (impl_->playheadSync_) {
       impl_->playheadSync_->sync();
      }
       
      auto layers = comp->allLayer();
      for (const auto& l : layers) {
       if (l) onLayerCreated(id, l->id());
      }
        }
       }
      }
     }

     void ArtifactTimelineWidget::onLayerCreated(const CompositionID& compId, const LayerID& lid)
  {
   if (compId != impl_->compositionId_) return;
   if (!impl_->trackView_) return;

   qDebug() << "[ArtifactTimelineWidget::onLayerCreated] Layer created:" << lid.toString();

   int trackIndex = impl_->trackView_->addTrack(kTimelineRowHeight);
   double startFrame = 0.0;
   double duration = 100.0;
   ClipItem* clip = impl_->trackView_->addClip(trackIndex, startFrame, duration);
   refreshTracks();
  }

  void ArtifactTimelineWidget::onLayerRemoved(const CompositionID& compId, const LayerID& lid)
  {
   if (compId != impl_->compositionId_) return;
   qDebug() << "[ArtifactTimelineWidget::onLayerRemoved] Layer removed:" << lid.toString();
  }

   void ArtifactTimelineWidget::onShyChanged(bool active)
   {
       impl_->shyActive_ = active;
       if (impl_->layerTimelinePanel_) {
           refreshTracks();
       }
   }

  void ArtifactTimelineWidget::refreshTracks()
  {
      if (!impl_->trackView_) return;
      impl_->trackView_->clearTracks();
      
      auto comp = safeCompositionLookup(impl_->compositionId_);
      if (!comp) return;

       auto allLayers = comp->allLayer();
       
       for (auto& l : allLayers) {
           if (l) {
               if (impl_->shyActive_ && l->isShy()) continue;
               
               int trackIndex = impl_->trackView_->addTrack(kTimelineRowHeight);
               impl_->trackView_->addClip(trackIndex, 0, 300);
           }
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
 setFocusPolicy(Qt::StrongFocus);

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

 void TimelineTrackView::clearTracks()
 {
  if (impl_->scene_) {
   impl_->scene_->clearTracks();
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

  const double rowSpacing = kTimelineRowHeight;
  int startRow = std::max(0, static_cast<int>(std::floor(rect.top() / rowSpacing)));
  int endRow = static_cast<int>(std::ceil(rect.bottom() / rowSpacing));

  for (int i = startRow; i <= endRow; ++i) {
   double sceneY = i * rowSpacing;
   QPointF vpYTop = mapFromScene(QPointF(0, sceneY));
   QPointF vpYBottom = mapFromScene(QPointF(0, sceneY + rowSpacing));
   double h = vpYBottom.y() - vpYTop.y();
   
   if (i % 2 == 0) {
    painter->fillRect(QRectF(0, vpYTop.y(), viewport()->width(), h), QColor(42, 42, 42));
   } else {
    painter->fillRect(QRectF(0, vpYTop.y(), viewport()->width(), h), QColor(45, 45, 45));
   }
  }

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

  //const double rowSpacing = 28.0;
  double rowStart = std::floor(rect.top() / rowSpacing) * rowSpacing;
  for (double y = rowStart; y <= rect.bottom(); y += rowSpacing) {
   painter->drawLine(QLineF(rect.left(), y, rect.right(), y));
  }
  painter->restore();
 }

 void TimelineTrackView::drawForeground(QPainter* painter, const QRectF& rect)
 {
  Q_UNUSED(rect);
  painter->save();
  painter->setWorldTransform(QTransform());
  QRectF viewRect = viewport()->rect();
  const bool hasTracks = impl_->scene_ && impl_->scene_->trackCount() > 0;
  if (!hasTracks) {
   QRectF emptyRect = viewRect;
   QFont emptyTitle = painter->font();
   emptyTitle.setPointSize(11);
   emptyTitle.setBold(true);
   painter->setFont(emptyTitle);
   painter->setPen(QColor(205, 205, 205));
   painter->drawText(emptyRect, Qt::AlignCenter, "No layers");

   QFont emptyHint = painter->font();
   emptyHint.setPointSize(9);
   emptyHint.setBold(false);
   painter->setFont(emptyHint);
   painter->setPen(QColor(145, 145, 145));
   painter->drawText(emptyRect.adjusted(0, 24, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
     "Add a layer to see timeline clips");
  }

  if (hasTracks) {
   QRectF barRect(0, viewRect.height() - 30, viewRect.width(), 30);
   painter->fillRect(barRect, QColor(28, 28, 28, 220));
   painter->setPen(QColor(150, 150, 150));
   painter->drawText(barRect.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
    "Ctrl + Wheel: Zoom");
  }
  painter->restore();
 }

 QSize TimelineTrackView::minimumSizeHint() const
 {
  return QSize(600, 600);
 }

  void TimelineTrackView::mousePressEvent(QMouseEvent* event)
  {
   setFocus(Qt::MouseFocusReason);
   if (event->button() == Qt::MiddleButton) {
    impl_->isPanning_ = true;
    impl_->lastPanPoint_ = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
   }

 if (event->button() == Qt::LeftButton) {
    // Scrub hotspot at the very top of the right clip scene:
    // clicking this band always seeks, even when clips exist below.
    constexpr int kTopSeekHotZonePx = 8;
    if (event->pos().y() <= kTopSeekHotZonePx) {
      const QPointF topScenePos = mapToScene(event->pos());
      setPosition(topScenePos.x());
      const double ratio = impl_->duration_ > 0.0 ? impl_->position_ / impl_->duration_ : 0.0;
      Q_EMIT seekPositionChanged(ratio);
      event->accept();
      return;
    }

    const QPointF scenePos = mapToScene(event->pos());
    
    // Check if clicking on a clip/resize handle.
    auto items = scene()->items(scenePos, Qt::IntersectsItemShape);
    ResizeHandle* clickedHandle = nullptr;
    ClipItem* clickedClip = nullptr;
    for (auto item : items) {
     if (auto* handle = dynamic_cast<ResizeHandle*>(item)) {
      clickedHandle = handle;
      if (auto* parentClip = dynamic_cast<ClipItem*>(handle->parentItem())) {
       clickedClip = parentClip;
      }
      break;
     }
     if (auto* clip = dynamic_cast<ClipItem*>(item)) {
      clickedClip = clip;
      break;
     }
    }
    
    if (clickedHandle) {
     // Do not seek when operating resize handles; let QGraphicsItem drag logic own the input.
    } else if (clickedClip) {
     // Clip selected/deselected
     bool isSelected = clickedClip->isSelected();
     if (!isSelected && !(event->modifiers() & Qt::ShiftModifier)) {
      clearSelection();
     }
     clickedClip->setSelected(!isSelected);
     
     if (clickedClip->isSelected()) {
      impl_->lastSelectedClip_ = clickedClip;
      Q_EMIT clipSelected(clickedClip);
     } else {
      Q_EMIT clipDeselected(clickedClip);
     }
    } else {
     // Click on timeline for seeking
     setPosition(scenePos.x());
     double ratio = impl_->duration_ > 0.0 ? impl_->position_ / impl_->duration_ : 0.0;
     Q_EMIT seekPositionChanged(ratio);
     
     if (!(event->modifiers() & Qt::ShiftModifier)) {
      clearSelection();
      }
   }
  }

  if (event->button() == Qt::RightButton) {
   const QPointF scenePos = mapToScene(event->pos());
   auto items = scene()->items(scenePos, Qt::IntersectsItemShape);
   ClipItem* clickedClip = nullptr;
   for (auto* item : items) {
    if (auto* clip = dynamic_cast<ClipItem*>(item)) {
     clickedClip = clip;
     break;
    }
   }

   QMenu menu(this);
   QAction* deleteClipAction = nullptr;
   QAction* duplicateClipAction = nullptr;
   QAction* splitClipAction = nullptr;
   QAction* trimInAtPlayheadAction = nullptr;
   QAction* trimOutAtPlayheadAction = nullptr;
   QAction* nudgeLeftAction = nullptr;
   QAction* nudgeRightAction = nullptr;
   QAction* moveTrackUpAction = nullptr;
   QAction* moveTrackDownAction = nullptr;
   QAction* deleteSelectedAction = nullptr;
   QAction* selectTrackClipsAction = nullptr;
   QAction* clearSelectionAction = nullptr;
   QAction* addTrackAction = nullptr;
   QAction* removeTrackAction = nullptr;

   if (clickedClip && impl_->scene_) {
    deleteClipAction = menu.addAction("Delete Clip");
    duplicateClipAction = menu.addAction("Duplicate Clip");
    splitClipAction = menu.addAction("Split At Playhead");
    trimInAtPlayheadAction = menu.addAction("Trim In to Playhead");
    trimOutAtPlayheadAction = menu.addAction("Trim Out to Playhead");
    menu.addSeparator();
    nudgeLeftAction = menu.addAction("Nudge Left 1f");
    nudgeRightAction = menu.addAction("Nudge Right 1f");
    moveTrackUpAction = menu.addAction("Move Clip to Track Up");
    moveTrackDownAction = menu.addAction("Move Clip to Track Down");
    menu.addSeparator();
   }

   if (impl_->scene_) {
    deleteSelectedAction = menu.addAction("Delete Selected Clips");
    selectTrackClipsAction = menu.addAction("Select All Clips in This Track");
    clearSelectionAction = menu.addAction("Clear Selection");
    menu.addSeparator();
   }

   if (impl_->scene_) {
    QMenu* editModeMenu = menu.addMenu("Edit Mode");
    QAction* rippleAction = editModeMenu->addAction("Ripple Edit");
    rippleAction->setCheckable(true);
    rippleAction->setChecked(impl_->scene_->rippleEditEnabled());

    QMenu* snapMenu = menu.addMenu("Snap Strength");
    QAction* low = snapMenu->addAction("Low");
    QAction* medium = snapMenu->addAction("Medium");
    QAction* high = snapMenu->addAction("High");
    low->setCheckable(true);
    medium->setCheckable(true);
    high->setCheckable(true);

    const auto strength = impl_->scene_->snapStrength();
    low->setChecked(strength == TimelineScene::SnapStrength::Low);
    medium->setChecked(strength == TimelineScene::SnapStrength::Medium);
    high->setChecked(strength == TimelineScene::SnapStrength::High);

    menu.addSeparator();
    addTrackAction = menu.addAction("Add Track");
    removeTrackAction = menu.addAction("Remove This Track");
    const int cursorTrack = impl_->scene_->getTrackAtPosition(scenePos.y());
    removeTrackAction->setEnabled(cursorTrack >= 0 && impl_->scene_->trackCount() > 1);

    QAction* chosen = menu.exec(event->globalPos());
    if (!chosen) {
     event->accept();
     return;
    }

    if (chosen == deleteClipAction && clickedClip) {
     removeClip(clickedClip);
    } else if (chosen == duplicateClipAction && clickedClip) {
     const int trackIndex = impl_->scene_->getTrackAtPosition(clickedClip->pos().y() + 1.0);
     if (trackIndex >= 0) {
      const double start = clickedClip->pos().x();
      const double duration = clickedClip->getDuration();
      addClip(trackIndex, start + duration + 5.0, duration);
     }
    } else if (chosen == splitClipAction && clickedClip) {
     const double playhead = impl_->position_;
     const double start = clickedClip->pos().x();
     const double end = start + clickedClip->getDuration();
     if (playhead > start + 1.0 && playhead < end - 1.0) {
      const int trackIndex = impl_->scene_->getTrackAtPosition(clickedClip->pos().y() + 1.0);
      if (trackIndex >= 0) {
       const double leftDuration = playhead - start;
       const double rightDuration = end - playhead;
       clickedClip->setDuration(leftDuration);
       addClip(trackIndex, playhead, rightDuration);
      }
     }
    } else if (chosen == trimInAtPlayheadAction && clickedClip) {
     const double playhead = impl_->position_;
     const double start = clickedClip->getStart();
     const double end = start + clickedClip->getDuration();
     if (playhead > start + 1.0 && playhead < end - 1.0) {
      clickedClip->setStartDuration(playhead, end - playhead);
     }
    } else if (chosen == trimOutAtPlayheadAction && clickedClip) {
     const double playhead = impl_->position_;
     const double start = clickedClip->getStart();
     const double end = start + clickedClip->getDuration();
     if (playhead > start + 1.0 && playhead < end - 1.0) {
      clickedClip->setDuration(playhead - start);
     }
    } else if (chosen == nudgeLeftAction && clickedClip) {
     clickedClip->setStart(std::max(0.0, clickedClip->getStart() - 1.0));
    } else if (chosen == nudgeRightAction && clickedClip) {
     clickedClip->setStart(clickedClip->getStart() + 1.0);
    } else if (chosen == moveTrackUpAction && clickedClip) {
     const int trackIndex = impl_->scene_->getTrackAtPosition(clickedClip->pos().y() + 1.0);
     if (trackIndex > 0) {
      const double d = clickedClip->getDuration();
      const double s = clickedClip->getStart();
      removeClip(clickedClip);
      addClip(trackIndex - 1, s, d);
     }
    } else if (chosen == moveTrackDownAction && clickedClip) {
     const int trackIndex = impl_->scene_->getTrackAtPosition(clickedClip->pos().y() + 1.0);
     if (trackIndex >= 0 && trackIndex + 1 < impl_->scene_->trackCount()) {
      const double d = clickedClip->getDuration();
      const double s = clickedClip->getStart();
      removeClip(clickedClip);
      addClip(trackIndex + 1, s, d);
     }
    } else if (chosen == deleteSelectedAction) {
     auto selected = impl_->scene_->getSelectedClips();
     std::vector<ClipItem*> snapshot(selected.begin(), selected.end());
     for (auto* clip : snapshot) {
      removeClip(clip);
     }
    } else if (chosen == selectTrackClipsAction) {
     const int trackIndex = impl_->scene_->getTrackAtPosition(scenePos.y());
     if (trackIndex >= 0) {
      impl_->scene_->clearSelection();
      for (auto* clip : impl_->scene_->getClips()) {
       if (!clip) continue;
       const int clipTrack = impl_->scene_->getTrackAtPosition(clip->pos().y() + 1.0);
       if (clipTrack == trackIndex) {
        clip->setSelected(true);
       }
      }
     }
    } else if (chosen == clearSelectionAction) {
     clearSelection();
    } else if (chosen == rippleAction) {
     impl_->scene_->setRippleEditEnabled(rippleAction->isChecked());
    } else if (chosen == low) {
     impl_->scene_->setSnapStrength(TimelineScene::SnapStrength::Low);
    } else if (chosen == medium) {
     impl_->scene_->setSnapStrength(TimelineScene::SnapStrength::Medium);
    } else if (chosen == high) {
     impl_->scene_->setSnapStrength(TimelineScene::SnapStrength::High);
    } else if (chosen == addTrackAction) {
     addTrack(kTimelineRowHeight);
    } else if (chosen == removeTrackAction) {
     const int trackIndex = impl_->scene_->getTrackAtPosition(scenePos.y());
     if (trackIndex >= 0 && impl_->scene_->trackCount() > 1) {
      impl_->scene_->removeTrack(trackIndex);
     }
    }

    event->accept();
    return;
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

  void TimelineTrackView::keyPressEvent(QKeyEvent* event)
  {
   int frameDelta = 0;
   if (event->key() == Qt::Key_Left) {
    frameDelta = -1;
   } else if (event->key() == Qt::Key_Right) {
    frameDelta = 1;
   }

   if (frameDelta == 0) {
    QGraphicsView::keyPressEvent(event);
    return;
   }

   if (!impl_->scene_) {
    event->accept();
    return;
   }

   const auto& selected = impl_->scene_->getSelectedClips();
   if (selected.empty()) {
    setPosition(impl_->position_ + static_cast<double>(frameDelta));
    const double ratio = impl_->duration_ > 0.0 ? impl_->position_ / impl_->duration_ : 0.0;
    Q_EMIT seekPositionChanged(ratio);
    event->accept();
    return;
   }

   double minStart = 0.0;
   bool hasMin = false;
   for (auto* clip : selected) {
    if (!clip) {
     continue;
    }
    const double start = clip->getStart();
    if (!hasMin || start < minStart) {
     minStart = start;
     hasMin = true;
    }
   }

   if (!hasMin) {
    event->accept();
    return;
   }

   double appliedDelta = static_cast<double>(frameDelta);
   if (frameDelta < 0 && minStart + appliedDelta < 0.0) {
    // Keep multi-selection aligned and never move past frame 0.
    appliedDelta = -std::floor(minStart);
   }

   if (std::abs(appliedDelta) < 1e-6) {
    event->accept();
    return;
   }

   for (auto* clip : selected) {
    if (!clip) {
     continue;
    }
    clip->setStart(std::max(0.0, clip->getStart() + appliedDelta));
   }
   viewport()->update();
   event->accept();
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
  //setHeaderHidden(true); // optional compact header mode
  setColumnWidth(0, 16); // ACR
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
