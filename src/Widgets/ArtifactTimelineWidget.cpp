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
#include <QSignalBlocker>
#include <QGraphicsRectItem>
#include <QBrush>
#include <QResizeEvent>
#include <QEvent>
#include <QPainter>
#include <QMenu>
#include <QTimer>
#include <qtmetamacros.h>
module Artifact.Widgets.Timeline;

import std;

import Widgets.Utils.CSS;

import Artifact.Layers.Hierarchy.Model;
import Artifact.Widget.WorkAreaControlWidget;

import ArtifactTimelineIconModel;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Timeline.ScrubBar;
import Artifact.Widgets.Timeline.Label;
import Artifact.Timeline.NavigatorWidget;
import Artifact.Timeline.TimeCodeWidget;
import Panel.DraggableSplitter;
import Artifact.Timeline.Objects;
import Artifact.Widgets.Timeline.GlobalSwitches;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Application.Manager;
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
 constexpr int kTimelineWorkAreaRowHeight = 26;
 constexpr int kDefaultTimelineFrames = 300;
 constexpr int kTopSeekHotZonePx = 28; // allow seeking from top band of clip scene
 constexpr double kTimelineScrollPaddingFrames = 120.0;
 inline double timelineFrameMax(const double duration)
 {
  return std::max(0.0, duration - 1.0);
 }
 inline double timelineViewportPaddingFrames(const QGraphicsView* view, const double zoomLevel)
 {
  if (!view || !view->viewport()) {
   return kTimelineScrollPaddingFrames;
  }

  const int viewportWidth = view->viewport()->width();
  if (viewportWidth <= 0 || zoomLevel <= 1e-5) {
   return kTimelineScrollPaddingFrames;
  }

  return std::max(kTimelineScrollPaddingFrames, static_cast<double>(viewportWidth) / zoomLevel);
 }
 inline double timelineSceneWidth(const double duration, const QGraphicsView* view = nullptr, const double zoomLevel = 1.0)
 {
  return std::max(1.0, duration + timelineViewportPaddingFrames(view, zoomLevel));
 }
 inline int wheelScrollDelta(const QWheelEvent* event, const bool horizontal)
 {
  if (!event) {
   return 0;
  }

  const QPoint pixelDelta = event->pixelDelta();
  if (!pixelDelta.isNull()) {
   return horizontal ? pixelDelta.x() : pixelDelta.y();
  }

  const QPoint angleDelta = event->angleDelta();
  return horizontal ? angleDelta.x() : angleDelta.y();
 }
 inline int layerInsertionIndexForTrackDrop(const QVector<LayerID>& trackLayerIds, const LayerID& draggedLayerId, const int trackIndex)
 {
  int targetLayerIndex = 0;
  const int trackCount = static_cast<int>(trackLayerIds.size());
  const int upperBound = std::clamp<int>(trackIndex, 0, trackCount);
  for (int i = 0; i < upperBound; ++i) {
   const auto& candidate = trackLayerIds[i];
   if (candidate.isNil() || candidate == draggedLayerId) {
    continue;
   }
   ++targetLayerIndex;
  }
  return targetLayerIndex;
 }
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

   void setPlayheadLine(const QRect& activeRect, const int x)
   {
    if (playheadX_ == x && activeRect_ == activeRect) {
     return;
    }
    activeRect_ = activeRect;
    playheadX_ = x;
    update();
   }

  protected:
   void paintEvent(QPaintEvent* event) override
   {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setClipRect(activeRect_);
    painter.setPen(QPen(QColor(255, 80, 60, 220), 1));
    painter.drawLine(playheadX_, activeRect_.top(), playheadX_, activeRect_.bottom());
   }

  private:
   QRect activeRect_;
   int playheadX_ = 0;
  };

  class PlayheadSyncFilter final : public QObject
  {
  public:
   PlayheadSyncFilter(QWidget* regionWidget, TimelineTrackView* trackView, TimelinePlayheadOverlay* overlay, QObject* parent = nullptr)
    : QObject(parent), regionWidget_(regionWidget), trackView_(trackView), overlay_(overlay)
   {
   }

   void sync()
   {
    if (!regionWidget_ || !trackView_ || !overlay_ || !trackView_->viewport()) {
     return;
    }

    if (!regionWidget_->isVisible() || !trackView_->isVisible() || !trackView_->viewport()->isVisible()) {
     overlay_->hide();
     return;
    }

    ensureOverlayHost();
    if (!overlayHost_) {
     overlay_->hide();
     return;
    }

    overlay_->setGeometry(overlayHost_->rect());
    const QRect activeRect(regionWidget_->mapTo(overlayHost_, QPoint(0, 0)), regionWidget_->size());
    const QPoint viewportPos = trackView_->mapFromScene(QPointF(trackView_->position(), 0.0));
    const int xInOverlay = trackView_->viewport()->mapTo(overlayHost_, viewportPos).x();
    overlay_->setPlayheadLine(activeRect, xInOverlay);
    overlay_->show();
    overlay_->raise();
   }

  protected:
   bool eventFilter(QObject* watched, QEvent* event) override
   {
    Q_UNUSED(watched);
    switch (event->type()) {
    case QEvent::Hide:
    case QEvent::ParentChange:
    case QEvent::Resize:
    case QEvent::LayoutRequest:
    case QEvent::Show:
    case QEvent::Move:
    case QEvent::WindowStateChange:
     sync();
     break;
    default:
     break;
    }
    return QObject::eventFilter(watched, event);
   }

  private:
   void ensureOverlayHost()
   {
    QWidget* desiredHost = regionWidget_;
    if (overlayHost_ == desiredHost) {
     return;
    }

    if (overlayHost_) {
     if (overlayHost_ != regionWidget_) {
      overlayHost_->removeEventFilter(this);
     }
    }
    overlayHost_ = desiredHost;
    if (!overlayHost_) {
     return;
    }

    overlay_->setParent(overlayHost_);
    if (overlayHost_ != regionWidget_) {
     overlayHost_->installEventFilter(this);
    }
   }

   QWidget* regionWidget_ = nullptr;
   TimelineTrackView* trackView_ = nullptr;
   TimelinePlayheadOverlay* overlay_ = nullptr;
   QWidget* overlayHost_ = nullptr;
  };

  class HeaderSeekFilter final : public QObject
  {
  public:
   HeaderSeekFilter(TimelineTrackView* trackView, ArtifactTimelineScrubBar* scrubBar, QObject* parent = nullptr)
    : QObject(parent), trackView_(trackView), scrubBar_(scrubBar)
   {
   }

  protected:
   bool eventFilter(QObject* watched, QEvent* event) override
   {
    if (!trackView_ || !scrubBar_) {
     return QObject::eventFilter(watched, event);
    }
    if (event->type() != QEvent::MouseButtonPress &&
        event->type() != QEvent::MouseMove &&
        event->type() != QEvent::MouseButtonRelease) {
      return QObject::eventFilter(watched, event);
    }

    auto* mouseEvent = dynamic_cast<QMouseEvent*>(event);
    auto* sourceWidget = qobject_cast<QWidget*>(watched);
    if (!mouseEvent || !sourceWidget) {
     return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress && mouseEvent->button() != Qt::LeftButton) {
     reservedClickCandidate_ = false;
     reservedClickSource_ = nullptr;
     return QObject::eventFilter(watched, event);
    }
    if (event->type() == QEvent::MouseMove && !(mouseEvent->buttons() & Qt::LeftButton)) {
     seeking_ = false;
     seekSource_ = nullptr;
     reservedClickCandidate_ = false;
     reservedClickSource_ = nullptr;
     return QObject::eventFilter(watched, event);
    }

    if (!trackView_->viewport()) {
      return QObject::eventFilter(watched, event);
    }

    const double frameMax = std::max(1.0, timelineFrameMax(trackView_->duration()));
    const auto seekFromHeaderWidget = [&](QWidget* widget, const QPoint& pos) -> double {
      const QRect viewportRect = trackView_->viewport()->rect();
      if (viewportRect.isEmpty()) {
       return 0.0;
      }

      const QPoint globalPos = widget->mapToGlobal(pos);
      int viewportX = trackView_->viewport()->mapFromGlobal(globalPos).x();
      viewportX = std::clamp(viewportX, viewportRect.left(), viewportRect.right());
      const int viewportY = std::clamp(kTopSeekHotZonePx / 2, viewportRect.top(), viewportRect.bottom());
      const QPointF scenePos = trackView_->mapToScene(QPoint(viewportX, viewportY));
      return std::clamp(scenePos.x(), 0.0, frameMax);
    };

    if (event->type() == QEvent::MouseButtonRelease) {
     const bool reservedClick = reservedClickCandidate_ && reservedClickSource_ == sourceWidget &&
      mouseEvent->button() == Qt::LeftButton;
     const int dragDistance = reservedClick
      ? (sourceWidget->mapToGlobal(mouseEvent->pos()) - reservedPressGlobalPos_).manhattanLength()
      : 0;

     if (mouseEvent->button() == Qt::LeftButton) {
      seeking_ = false;
      seekSource_ = nullptr;
     }

     reservedClickCandidate_ = false;
     reservedClickSource_ = nullptr;

     if (reservedClick && dragDistance <= kReservedClickDragThresholdPx) {
      const double clamped = seekFromHeaderWidget(sourceWidget, mouseEvent->pos());
      const int frame = static_cast<int>(std::round(clamped));
      trackView_->setPosition(clamped);
      scrubBar_->setCurrentFrame(FramePosition(frame));
      event->accept();
      return true;
     }

     return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseMove && reservedClickCandidate_ && reservedClickSource_ == sourceWidget) {
     const int dragDistance = (sourceWidget->mapToGlobal(mouseEvent->pos()) - reservedPressGlobalPos_).manhattanLength();
     if (dragDistance > kReservedClickDragThresholdPx) {
      reservedClickCandidate_ = false;
      reservedClickSource_ = nullptr;
     }
     return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseMove && (!seeking_ || seekSource_ != sourceWidget)) {
     return QObject::eventFilter(watched, event);
    }
    double clamped = 0.0;

    if (sourceWidget == trackView_->viewport()) {
     const QRect viewportRect = trackView_->viewport()->rect();
     if (viewportRect.isEmpty()) {
      return QObject::eventFilter(watched, event);
     }
     const QPoint viewportPos = mouseEvent->pos();
     if (!viewportRect.contains(viewportPos) || viewportPos.y() > kTopSeekHotZonePx) {
      return QObject::eventFilter(watched, event);
     }

     const QPointF scenePos = trackView_->mapToScene(viewportPos);
     if (trackView_->scene()) {
      // Do not steal input from clip/resize handle interactions.
      const auto items = trackView_->scene()->items(scenePos, Qt::IntersectsItemShape);
      for (auto* item : items) {
       if (dynamic_cast<ResizeHandle*>(item) || dynamic_cast<ClipItem*>(item)) {
        return QObject::eventFilter(watched, event);
       }
      }
     }

     clamped = std::clamp(scenePos.x(), 0.0, frameMax);
    } else {
     if (event->type() == QEvent::MouseButtonPress && isReservedRangeInteraction(sourceWidget, mouseEvent->pos())) {
      seeking_ = false;
      seekSource_ = nullptr;
      reservedClickCandidate_ = true;
      reservedClickSource_ = sourceWidget;
      reservedPressGlobalPos_ = sourceWidget->mapToGlobal(mouseEvent->pos());
      return QObject::eventFilter(watched, event);
     }
     reservedClickCandidate_ = false;
     reservedClickSource_ = nullptr;
     clamped = seekFromHeaderWidget(sourceWidget, mouseEvent->pos());
    }

    seeking_ = true;
    seekSource_ = sourceWidget;
    const int frame = static_cast<int>(std::round(clamped));

    trackView_->setPosition(clamped);
    scrubBar_->setCurrentFrame(FramePosition(frame));
    event->accept();
    return true;
   }

  private:
   static bool isReservedRangeInteraction(QWidget* widget, const QPoint& pos)
   {
    if (auto* navigator = dynamic_cast<ArtifactTimelineNavigatorWidget*>(widget)) {
     return isHandleInteraction(pos, navigator->width(), navigator->height(), navigator->startValue(), navigator->endValue());
    }
    if (auto* workArea = dynamic_cast<WorkAreaControl*>(widget)) {
     return isHandleInteraction(pos, workArea->width(), workArea->height(), workArea->startValue(), workArea->endValue());
    }
    return false;
   }

   static bool isHandleInteraction(const QPoint& pos, const int width, const int height, const float start, const float end)
   {
    const int handleHalfW = 6;
    const int handleW = handleHalfW * 2;
    const int usableWidth = std::max(1, width - handleW);
    const int x1 = handleHalfW + static_cast<int>(start * usableWidth);
    const int x2 = handleHalfW + static_cast<int>(end * usableWidth);

    const QRect leftHandleRect(x1 - handleHalfW, 0, handleW, height);
    const QRect rightHandleRect(x2 - handleHalfW, 0, handleW, height);
    if (leftHandleRect.contains(pos) || rightHandleRect.contains(pos)) {
     return true;
    }

    // When the range spans nearly the whole widget, reserving the whole bar creates
    // an oversized dead zone for playhead seeking. In that case only the handles
    // stay reserved.
    if (start <= 0.001f && end >= 0.999f) {
     return false;
    }

    const QRect rangeRect(x1 + handleHalfW, 0, std::max(0, x2 - x1 - handleW), height);
    return rangeRect.contains(pos);
   }

   TimelineTrackView* trackView_ = nullptr;
   ArtifactTimelineScrubBar* scrubBar_ = nullptr;
   bool seeking_ = false;
   QWidget* seekSource_ = nullptr;
   bool reservedClickCandidate_ = false;
   QWidget* reservedClickSource_ = nullptr;
   QPoint reservedPressGlobalPos_;
   static constexpr int kReservedClickDragThresholdPx = 4;
  };

  class HeaderScrollFilter final : public QObject
  {
  public:
   HeaderScrollFilter(TimelineTrackView* trackView, QObject* parent = nullptr)
    : QObject(parent), trackView_(trackView)
   {
   }

  protected:
   bool eventFilter(QObject* watched, QEvent* event) override
   {
    if (!trackView_) {
     return QObject::eventFilter(watched, event);
    }

    auto* widget = qobject_cast<QWidget*>(watched);
    auto* hBar = trackView_->horizontalScrollBar();
    auto* vBar = trackView_->verticalScrollBar();
    if (!widget || (!hBar && !vBar)) {
      return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::Wheel:
    {
     auto* wheelEvent = static_cast<QWheelEvent*>(event);
     if (wheelEvent->modifiers() & Qt::ControlModifier) {
      return QObject::eventFilter(watched, event);
     }

     const bool wantsHorizontal = (wheelEvent->modifiers() & Qt::ShiftModifier);

     if (wantsHorizontal && hBar) {
      int delta = wheelScrollDelta(wheelEvent, true);
      if (delta == 0) {
       delta = wheelScrollDelta(wheelEvent, false);
      }
      if (delta != 0) {
       hBar->setValue(hBar->value() - delta);
       event->accept();
       return true;
      }
     }

     if (vBar && vBar->maximum() > vBar->minimum()) {
      int delta = wheelScrollDelta(wheelEvent, false);
      if (delta == 0) {
       delta = wheelScrollDelta(wheelEvent, true);
      }
      if (delta != 0) {
       vBar->setValue(vBar->value() - delta);
       event->accept();
       return true;
      }
     }

     return QObject::eventFilter(watched, event);
    }
    case QEvent::MouseButtonPress:
    {
     auto* mouseEvent = static_cast<QMouseEvent*>(event);
     if (mouseEvent->button() != Qt::MiddleButton) {
      return QObject::eventFilter(watched, event);
     }
     panning_ = true;
     lastGlobalPos_ = mouseEvent->globalPosition().toPoint();
     widget->setCursor(Qt::ClosedHandCursor);
     event->accept();
     return true;
    }
    case QEvent::MouseMove:
    {
     auto* mouseEvent = static_cast<QMouseEvent*>(event);
     if (!panning_ || !(mouseEvent->buttons() & Qt::MiddleButton)) {
      return QObject::eventFilter(watched, event);
     }
     const QPoint currentGlobalPos = mouseEvent->globalPosition().toPoint();
     const QPoint delta = currentGlobalPos - lastGlobalPos_;
     lastGlobalPos_ = currentGlobalPos;
     if (hBar) {
      hBar->setValue(hBar->value() - delta.x());
     }
     if (vBar) {
      vBar->setValue(vBar->value() - delta.y());
     }
     event->accept();
     return true;
    }
    case QEvent::MouseButtonRelease:
    {
     auto* mouseEvent = static_cast<QMouseEvent*>(event);
     if (mouseEvent->button() != Qt::MiddleButton) {
      return QObject::eventFilter(watched, event);
     }
     panning_ = false;
     widget->unsetCursor();
     event->accept();
     return true;
    }
    default:
     break;
    }

    return QObject::eventFilter(watched, event);
   }

  private:
   TimelineTrackView* trackView_ = nullptr;
   bool panning_ = false;
   QPoint lastGlobalPos_;
  };

  class PanelVerticalScrollFilter final : public QObject
  {
  public:
   explicit PanelVerticalScrollFilter(QScrollBar* verticalBar, QObject* parent = nullptr)
    : QObject(parent), verticalBar_(verticalBar)
   {
   }

  protected:
   bool eventFilter(QObject* watched, QEvent* event) override
   {
    Q_UNUSED(watched);
    if (!verticalBar_ || event->type() != QEvent::Wheel) {
     return QObject::eventFilter(watched, event);
    }

    auto* wheelEvent = static_cast<QWheelEvent*>(event);
    if ((wheelEvent->modifiers() & Qt::ControlModifier) || (wheelEvent->modifiers() & Qt::ShiftModifier)) {
     return QObject::eventFilter(watched, event);
    }
    if (verticalBar_->maximum() <= verticalBar_->minimum()) {
     return QObject::eventFilter(watched, event);
    }

    int delta = wheelScrollDelta(wheelEvent, false);
    if (delta == 0) {
     delta = wheelScrollDelta(wheelEvent, true);
    }
    if (delta == 0) {
     return QObject::eventFilter(watched, event);
    }

    verticalBar_->setValue(verticalBar_->value() - delta);
    wheelEvent->accept();
    return true;
   }

  private:
   QScrollBar* verticalBar_ = nullptr;
  };

  class LeftHeaderPriorityFilter final : public QObject
  {
  public:
   LeftHeaderPriorityFilter(QWidget* host, QWidget* timecode, QWidget* searchBar, QWidget* switches, QObject* parent = nullptr)
    : QObject(parent), host_(host), timecode_(timecode), searchBar_(searchBar), switches_(switches)
   {
    if (searchBar_) {
     searchPreferredWidth_ = std::max(searchBar_->width(), searchBar_->sizeHint().width());
     searchMinimumWidth_ = std::max(96, searchBar_->minimumSizeHint().width());
    }
   }

   void sync()
   {
    if (!host_ || !timecode_ || !searchBar_ || !switches_) {
     return;
    }

    auto* layout = qobject_cast<QHBoxLayout*>(host_->layout());
    const int spacing = layout ? layout->spacing() : 0;
    const QMargins margins = layout ? layout->contentsMargins() : QMargins();
    const int availableWidth = std::max(0, host_->width() - margins.left() - margins.right());

    const int timecodeWidth = std::max(timecode_->minimumSizeHint().width(), timecode_->sizeHint().width());
    const int switchesWidth = std::max(switches_->minimumSizeHint().width(), switches_->sizeHint().width());
    const int requiredForSearch = timecodeWidth + spacing + searchMinimumWidth_;
    const int requiredForSwitches = requiredForSearch + spacing + switchesWidth;

    const bool showSearch = availableWidth >= requiredForSearch;
    const bool showSwitches = availableWidth >= requiredForSwitches;

    timecode_->setVisible(true);
    searchBar_->setVisible(showSearch);
    switches_->setVisible(showSwitches);

    if (!showSearch) {
     return;
    }

    const int reservedForSwitches = showSwitches ? spacing + switchesWidth : 0;
    const int maxSearchWidth = std::max(searchMinimumWidth_,
     availableWidth - timecodeWidth - spacing - reservedForSwitches);
    searchBar_->setFixedWidth(std::clamp(maxSearchWidth, searchMinimumWidth_, searchPreferredWidth_));
   }

  protected:
   bool eventFilter(QObject* watched, QEvent* event) override
   {
    Q_UNUSED(watched);
    switch (event->type()) {
    case QEvent::Resize:
    case QEvent::LayoutRequest:
    case QEvent::Show:
     sync();
     break;
    default:
     break;
    }
    return QObject::eventFilter(watched, event);
   }

  private:
   QWidget* host_ = nullptr;
   QWidget* timecode_ = nullptr;
   QWidget* searchBar_ = nullptr;
   QWidget* switches_ = nullptr;
   int searchPreferredWidth_ = 190;
   int searchMinimumWidth_ = 96;
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
    ArtifactTimelineScrubBar* scrubBar_ = nullptr;
    TimelinePlayheadOverlay* playheadOverlay_ = nullptr;
    PlayheadSyncFilter* playheadSync_ = nullptr;
    WorkAreaControl* workArea_ = nullptr;
    ArtifactTimelineNavigatorWidget* navigator_ = nullptr;
    CompositionID compositionId_;
    bool shyActive_ = false;
    QVector<LayerID> trackLayerIds_;
    bool syncingLayerSelection_ = false;
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
  leftHeader->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  searchBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  searchBar->setMinimumWidth(96);
  searchBar->setFixedWidth(190);
  globalSwitches->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  globalSwitches->setFixedWidth(globalSwitches->sizeHint().width());

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
   QObject::connect(layerTreeView, &ArtifactLayerTimelinePanelWrapper::visibleRowsChanged,
                    this, [this]() {
                     refreshTracks();
                    });
  
  auto headerWidget = new QWidget();
  headerWidget->setLayout(searchBarLayout);
  headerWidget->setFixedHeight(kTimelineHeaderRowHeight);

  auto* leftHeaderPriorityFilter = new LeftHeaderPriorityFilter(headerWidget, leftHeader, searchBar, globalSwitches, headerWidget);
  headerWidget->installEventFilter(leftHeaderPriorityFilter);
  leftHeaderPriorityFilter->sync();

  auto leftTopSpacer = new QWidget();
  leftTopSpacer->setFixedHeight(kTimelineTopRowHeight);
  leftTopSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  leftTopSpacer->setStyleSheet("background-color: #2D2D30; border-bottom: 1px solid #1a1a1a;");

  auto leftSubHeaderSpacer = new QWidget();
  leftSubHeaderSpacer->setFixedHeight(0);
  leftSubHeaderSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  leftSubHeaderSpacer->setStyleSheet("background-color: #2D2D30; border-bottom: 1px solid #1a1a1a;");

  auto leftLayout = new QVBoxLayout();
  leftLayout->setSpacing(0);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->addWidget(leftTopSpacer);
  leftLayout->addWidget(headerWidget);
  leftLayout->addWidget(leftSubHeaderSpacer);
  leftLayout->addWidget(leftSplitter);

  auto leftPanel = new QWidget();
  leftPanel->setLayout(leftLayout);




  auto* rightPanelLayout = new QVBoxLayout();
  rightPanelLayout->setSpacing(0);
  rightPanelLayout->setContentsMargins(0, 0, 0, 0);
    auto timeNavigatorWidget = impl_->navigator_ = new ArtifactTimelineNavigatorWidget();
    auto workAreaWidget = impl_->workArea_ = new WorkAreaControl();
    auto scrubBar = impl_->scrubBar_ = new ArtifactTimelineScrubBar();
    auto timelineTrackView = impl_->trackView_ = new TimelineTrackView();
    timelineTrackView->setDuration(kDefaultTimelineFrames);
    timeNavigatorWidget->setTotalFrames(kDefaultTimelineFrames);
    timeNavigatorWidget->setFixedHeight(kTimelineTopRowHeight);
    timeNavigatorWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    scrubBar->setFixedHeight(kTimelineHeaderRowHeight);
    scrubBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    workAreaWidget->setFixedHeight(kTimelineWorkAreaRowHeight);
    workAreaWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    scrubBar->setTotalFrames(kDefaultTimelineFrames);
    scrubBar->setCurrentFrame(FramePosition(0));
    scrubBar->setVisible(true);
    scrubBar->update();
 
    auto updateZoom = [this]() {
      if (!impl_->trackView_ || !impl_->workArea_) return;
      double duration = impl_->trackView_->duration();
      float s = impl_->navigator_->startValue();
      float e = impl_->navigator_->endValue();
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

    auto syncWorkAreaFromComposition = [this, workAreaWidget]() {
      if (!workAreaWidget) {
        return;
      }
      const auto comp = safeCompositionLookup(impl_->compositionId_);
      const double totalFrames = comp ? std::max(1.0, static_cast<double>(comp->frameRange().duration()))
                                      : static_cast<double>(kDefaultTimelineFrames);
      const FrameRange workArea = comp ? comp->workAreaRange() : FrameRange(0, static_cast<int64_t>(totalFrames));
      const double startNorm = std::clamp(static_cast<double>(workArea.start()) / totalFrames, 0.0, 1.0);
      const double endNorm = std::clamp(static_cast<double>(workArea.end()) / totalFrames, startNorm, 1.0);
      QSignalBlocker blocker(workAreaWidget);
      workAreaWidget->setStart(static_cast<float>(startNorm));
      workAreaWidget->setEnd(static_cast<float>(endNorm));
    };

    syncWorkAreaFromComposition();

    QObject::connect(timeNavigatorWidget, &ArtifactTimelineNavigatorWidget::startChanged, this, updateZoom);
    QObject::connect(timeNavigatorWidget, &ArtifactTimelineNavigatorWidget::endChanged, this, updateZoom);
    QObject::connect(workAreaWidget, &WorkAreaControl::startChanged, this, [this, workAreaWidget](float) {
      const auto comp = safeCompositionLookup(impl_->compositionId_);
      if (!comp) {
       return;
      }
      const int64_t totalFrames = std::max<int64_t>(1, comp->frameRange().duration());
      const int64_t startFrame = std::clamp<int64_t>(static_cast<int64_t>(std::llround(workAreaWidget->startValue() * totalFrames)), 0, totalFrames);
      const int64_t endFrame = std::clamp<int64_t>(static_cast<int64_t>(std::llround(workAreaWidget->endValue() * totalFrames)), startFrame, totalFrames);
      comp->setWorkAreaRange(FrameRange(startFrame, std::max<int64_t>(startFrame + 1, endFrame)));
    });
    QObject::connect(workAreaWidget, &WorkAreaControl::endChanged, this, [this, workAreaWidget](float) {
      const auto comp = safeCompositionLookup(impl_->compositionId_);
      if (!comp) {
       return;
      }
      const int64_t totalFrames = std::max<int64_t>(1, comp->frameRange().duration());
      const int64_t startFrame = std::clamp<int64_t>(static_cast<int64_t>(std::llround(workAreaWidget->startValue() * totalFrames)), 0, totalFrames);
      const int64_t endFrame = std::clamp<int64_t>(static_cast<int64_t>(std::llround(workAreaWidget->endValue() * totalFrames)), startFrame, totalFrames);
      comp->setWorkAreaRange(FrameRange(startFrame, std::max<int64_t>(startFrame + 1, endFrame)));
    });
    QObject::connect(scrubBar, &ArtifactTimelineScrubBar::frameChanged, this, [timelineTrackView](const auto& frame) {
      timelineTrackView->setPosition(static_cast<double>(frame.framePosition()));
      if (auto* app = ArtifactApplicationManager::instance()) {
       if (auto* ctx = app->activeContextService()) {
        ctx->seekToFrame(frame.framePosition());
       }
      }
    });
    QObject::connect(timelineTrackView, &TimelineTrackView::seekPositionChanged, this, [timelineTrackView, scrubBar](double ratio) {
      const double frameMax = std::max(1.0, static_cast<double>(scrubBar->totalFrames() - 1));
      const int frame = static_cast<int>(std::round(std::clamp(ratio, 0.0, 1.0) * frameMax));
      scrubBar->setCurrentFrame(FramePosition(frame));
    });
    QObject::connect(timelineTrackView, &TimelineTrackView::seekPositionChanged, this, [this](double) {
      if (impl_->playheadSync_) {
       impl_->playheadSync_->sync();
      }
    });
    QObject::connect(timelineTrackView, &TimelineTrackView::clipSelected, this, [this](ClipItem* clip) {
      if (!clip || impl_->syncingLayerSelection_) {
       return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
       svc->selectLayer(clip->layerId());
      }
    });
    QObject::connect(timelineTrackView, &TimelineTrackView::clipDeselected, this, [this](ClipItem*) {
      if (impl_->syncingLayerSelection_) {
       return;
      }
      if (!impl_->trackView_ || !impl_->trackView_->timelineScene() ||
          !impl_->trackView_->timelineScene()->getSelectedClips().empty()) {
       return;
      }
      if (auto* svc = ArtifactProjectService::instance()) {
       svc->selectLayer(LayerID());
      }
    });
    QObject::connect(timelineTrackView, &TimelineTrackView::layerClipEdited, this,
     [this](const LayerID& layerId, const int trackIndex, const double start, const double duration) {
      if (layerId.isNil() || trackIndex < 0) {
       return;
      }

      if (auto* svc = ArtifactProjectService::instance()) {
       auto result = svc->findComposition(impl_->compositionId_);
       if (result.success) {
       if (auto comp = result.ptr.lock()) {
         if (auto layer = comp->layerById(layerId)) {
         const int64_t oldInPoint = layer->inPoint().framePosition();
          const int64_t oldOutPoint = layer->outPoint().framePosition();
          const int64_t inPoint = std::max<int64_t>(0, static_cast<int64_t>(std::llround(start)));
          const int64_t outPoint = std::max<int64_t>(inPoint + 1, static_cast<int64_t>(std::llround(start + duration)));
          layer->setInPoint(FramePosition(inPoint));
          layer->setOutPoint(FramePosition(outPoint));
          if (inPoint != oldInPoint) {
           const int64_t delta = inPoint - oldInPoint;
           layer->setStartTime(FramePosition(layer->startTime().framePosition() + delta));
          }
         }
        }
       }
       svc->projectChanged();
      }

      const int oldTrackIndex = impl_->trackLayerIds_.indexOf(layerId);
      if (oldTrackIndex < 0 || oldTrackIndex == trackIndex) {
       return;
      }

      const int targetLayerIndex =
       layerInsertionIndexForTrackDrop(impl_->trackLayerIds_, layerId, trackIndex);

      if (auto* svc = ArtifactProjectService::instance()) {
       svc->moveLayerInCurrentComposition(layerId, targetLayerIndex);
       svc->selectLayer(layerId);
      }
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
  trackSplitter->setHandleWidth(5);


  auto rightPanel = new QWidget();
  rightPanelLayout->addWidget(timeNavigatorWidget);
  rightPanelLayout->addWidget(scrubBar);
  rightPanelLayout->addWidget(workAreaWidget);
  rightPanelLayout->addWidget(trackSplitter);
  rightPanel->setLayout(rightPanelLayout);

  auto* playheadOverlay = new TimelinePlayheadOverlay(rightPanel);

  auto* playheadSync = new PlayheadSyncFilter(rightPanel, timelineTrackView, playheadOverlay, rightPanel);
  rightPanel->installEventFilter(playheadSync);
  timelineTrackView->installEventFilter(playheadSync);
  if (timelineTrackView->viewport()) {
   timelineTrackView->viewport()->installEventFilter(playheadSync);
  }
  impl_->playheadOverlay_ = playheadOverlay;
  impl_->playheadSync_ = playheadSync;

  auto* headerSeekFilter = new HeaderSeekFilter(timelineTrackView, scrubBar, rightPanel);
  timeNavigatorWidget->installEventFilter(headerSeekFilter);
  workAreaWidget->installEventFilter(headerSeekFilter);
  if (timelineTrackView->viewport()) {
   timelineTrackView->viewport()->installEventFilter(headerSeekFilter);
  }

  auto* headerScrollFilter = new HeaderScrollFilter(timelineTrackView, rightPanel);
  rightPanel->installEventFilter(headerScrollFilter);
  timeNavigatorWidget->installEventFilter(headerScrollFilter);
  scrubBar->installEventFilter(headerScrollFilter);
  workAreaWidget->installEventFilter(headerScrollFilter);

  if (auto* hBar = timelineTrackView->horizontalScrollBar()) {
   QObject::connect(hBar, &QScrollBar::valueChanged, this, [this](int) {
    if (impl_->playheadSync_) {
     impl_->playheadSync_->sync();
    }
   });
  }
  QObject::connect(scrubBar, &ArtifactTimelineScrubBar::frameChanged, this, [this](const auto&) {
   if (impl_->playheadSync_) {
    impl_->playheadSync_->sync();
   }
  });
  if (auto* playbackService = ArtifactPlaybackService::instance()) {
   QObject::connect(playbackService, &ArtifactPlaybackService::frameChanged, this,
    [this, timelineTrackView, scrubBar, playbackService](const FramePosition& frame) {
     const auto currentComp = playbackService->currentComposition();
     if (!currentComp || currentComp->id() != impl_->compositionId_) {
      return;
     }

     timelineTrackView->setPosition(static_cast<double>(frame.framePosition()));
     scrubBar->setCurrentFrame(frame);
     if (impl_->playheadSync_) {
      impl_->playheadSync_->sync();
     }
    });
  }
  QTimer::singleShot(0, this, [updateZoom]() {
   updateZoom();
  });
  playheadSync->sync();



  // Ŝ̃^CCXvb^[
  auto mainSplitter = new QSplitter(Qt::Horizontal);
  mainSplitter->setStyleSheet(R"(
    QSplitter::handle {
        background: #555555;
    }
)");
  mainSplitter->setHandleWidth(6);
  mainSplitter->addWidget(leftPanel);
  mainSplitter->addWidget(rightPanel);
  mainSplitter->setChildrenCollapsible(false);
  leftPanel->setMinimumWidth(280);
  rightPanel->setMinimumWidth(480);
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
   auto* leftPanelWheelFilter = new PanelVerticalScrollFilter(leftScroll, leftPanel);
   leftPanel->installEventFilter(leftPanelWheelFilter);
   headerWidget->installEventFilter(leftPanelWheelFilter);
   leftTopSpacer->installEventFilter(leftPanelWheelFilter);
   leftSubHeaderSpacer->installEventFilter(leftPanelWheelFilter);

   if (auto* rightScroll = timelineTrackView->verticalScrollBar()) {
    auto syncVerticalScroll = [](QScrollBar* source, QScrollBar* target, const int value) {
     if (!source || !target) {
      return;
     }

     const int sourceMax = source->maximum();
     const int targetMax = target->maximum();
     const int mappedValue = (sourceMax <= 0 || targetMax <= 0)
      ? std::clamp(value, target->minimum(), target->maximum())
      : static_cast<int>(std::round((static_cast<double>(value) / static_cast<double>(sourceMax)) *
                                    static_cast<double>(targetMax)));

     const QSignalBlocker blocker(target);
     target->setValue(std::clamp(mappedValue, target->minimum(), target->maximum()));
    };

    QObject::connect(leftScroll, &QScrollBar::valueChanged, this, [leftScroll, rightScroll, syncVerticalScroll](int value) {
     syncVerticalScroll(leftScroll, rightScroll, value);
    });
    QObject::connect(rightScroll, &QScrollBar::valueChanged, this, [leftScroll, rightScroll, syncVerticalScroll](int value) {
     syncVerticalScroll(rightScroll, leftScroll, value);
    });
   }
  }

  // Connect Layer Signals
  if (auto* svc = ArtifactProjectService::instance()) {
   QObject::connect(svc, &ArtifactProjectService::layerCreated, this, &ArtifactTimelineWidget::onLayerCreated);
   QObject::connect(svc, &ArtifactProjectService::layerRemoved, this, &ArtifactTimelineWidget::onLayerRemoved);
   QObject::connect(svc, &ArtifactProjectService::layerSelected, this, [this](const LayerID& layerId) {
    if (!impl_->trackView_) {
     return;
    }
    impl_->syncingLayerSelection_ = true;
    impl_->trackView_->selectClipForLayer(layerId);
    impl_->syncingLayerSelection_ = false;
   });
   QObject::connect(svc, &ArtifactProjectService::projectChanged, this, [this]() {
    refreshTracks();
   });
  }

 }

 ArtifactTimelineWidget::~ArtifactTimelineWidget()
 {
  if (impl_ && impl_->playheadOverlay_) {
   delete impl_->playheadOverlay_;
   impl_->playheadOverlay_ = nullptr;
  }
  delete impl_;
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
     svc->changeCurrentComposition(id);
     auto res = svc->findComposition(id);
     if (res.success && !res.ptr.expired()) {
      auto comp = res.ptr.lock();
       if (auto* app = ArtifactApplicationManager::instance()) {
        if (auto* ctx = app->activeContextService()) {
         ctx->setActiveComposition(comp);
        }
        if (auto* selectionManager = app->layerSelectionManager()) {
         selectionManager->setActiveComposition(comp);
        }
       }
      int totalFrames = static_cast<int>(std::round(comp->frameRange().duration()));
      if (totalFrames < 2) {
        totalFrames = kDefaultTimelineFrames;
      }
      if (auto* playbackService = ArtifactPlaybackService::instance()) {
       playbackService->setCurrentComposition(comp);
      }
      impl_->trackView_->setDuration(static_cast<double>(totalFrames));
      if (impl_->workArea_) {
       const FrameRange workArea = comp->workAreaRange();
       const double startNorm = std::clamp(static_cast<double>(workArea.start()) / static_cast<double>(totalFrames), 0.0, 1.0);
       const double endNorm = std::clamp(static_cast<double>(workArea.end()) / static_cast<double>(totalFrames), startNorm, 1.0);
       const QSignalBlocker blocker(impl_->workArea_);
       impl_->workArea_->setStart(static_cast<float>(startNorm));
       impl_->workArea_->setEnd(static_cast<float>(endNorm));
      }
       if (impl_->scrubBar_) {
        impl_->scrubBar_->setTotalFrames(std::max(1, totalFrames));
        impl_->scrubBar_->setCurrentFrame(FramePosition(0));
       }
       if (impl_->navigator_) {
        impl_->navigator_->setTotalFrames(std::max(1, totalFrames));
       }
        impl_->trackView_->setPosition(0.0);
       if (auto* app = ArtifactApplicationManager::instance()) {
        if (auto* ctx = app->activeContextService()) {
         ctx->seekToFrame(0);
       }
      }
      if (impl_->playheadSync_) {
       impl_->playheadSync_->sync();
      }
      QTimer::singleShot(0, this, [this]() {
       if (!impl_ || !impl_->trackView_ || !impl_->navigator_ || !impl_->trackView_->viewport()) {
        return;
       }
       const double duration = impl_->trackView_->duration();
       const double range = std::max(0.01, static_cast<double>(impl_->navigator_->endValue() - impl_->navigator_->startValue()));
       const int viewW = impl_->trackView_->viewport()->width();
       if (viewW <= 0) {
        return;
       }
       const double newZoom = viewW / (duration * range);
       impl_->trackView_->setZoomLevel(newZoom);
       if (auto* hBar = impl_->trackView_->horizontalScrollBar()) {
        hBar->setValue(static_cast<int>(impl_->navigator_->startValue() * duration * newZoom));
       }
       if (impl_->playheadSync_) {
        impl_->playheadSync_->sync();
       }
      });
       
      refreshTracks();
        }
       }
      }
     }

     void ArtifactTimelineWidget::onLayerCreated(const CompositionID& compId, const LayerID& lid)
  {
   if (compId != impl_->compositionId_) return;
   if (!impl_->trackView_) return;

   qDebug() << "[ArtifactTimelineWidget::onLayerCreated] Layer created:" << lid.toString();
   refreshTracks();
  }

  void ArtifactTimelineWidget::onLayerRemoved(const CompositionID& compId, const LayerID& lid)
  {
   if (compId != impl_->compositionId_) return;
   qDebug() << "[ArtifactTimelineWidget::onLayerRemoved] Layer removed:" << lid.toString();
   refreshTracks();
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

      QVector<LayerID> visibleRows;
      if (impl_->layerTimelinePanel_) {
       visibleRows = impl_->layerTimelinePanel_->visibleTimelineRows();
      }

      if (!impl_->layerTimelinePanel_) {
       auto comp = safeCompositionLookup(impl_->compositionId_);
       if (!comp) return;

       for (auto& layer : comp->allLayer()) {
        if (!layer) continue;
        if (impl_->shyActive_ && layer->isShy()) continue;
        visibleRows.push_back(layer->id());
       }
      }

      impl_->trackLayerIds_ = visibleRows;
      std::unordered_set<std::string> clipRowsByLayerId;
      for (const auto& rowLayerId : visibleRows) {
        const int trackIndex = impl_->trackView_->addTrack(kTimelineRowHeight);
        if (!rowLayerId.isNil()) {
         const std::string layerKey = rowLayerId.toString().toStdString();
         if (!clipRowsByLayerId.insert(layerKey).second) {
          continue;
         }
         const auto result = safeCompositionLookup(impl_->compositionId_);
         const auto layer = result ? result->layerById(rowLayerId) : nullptr;
         const double clipStart = layer ? static_cast<double>(layer->inPoint().framePosition()) : 0.0;
         const double clipDuration = layer
          ? std::max(1.0, static_cast<double>(layer->outPoint().framePosition() - layer->inPoint().framePosition()))
          : 300.0;
         if (auto* clip = impl_->trackView_->addClip(trackIndex, clipStart, clipDuration)) {
          clip->setLayerId(rowLayerId);
         }
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
 setFrameShape(QFrame::NoFrame);
 setAlignment(Qt::AlignLeft | Qt::AlignTop);

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
  impl_->position_ = qBound(0.0, position, timelineFrameMax(impl_->duration_));
  viewport()->update();
 }

 double TimelineTrackView::duration() const
 {
  return impl_->duration_;
 }

void TimelineTrackView::setDuration(double duration)
{
 impl_->duration_ = std::max(1.0, duration);
  if (impl_->scene_) {
   QRectF rect = impl_->scene_->sceneRect();
   rect.setWidth(timelineSceneWidth(impl_->duration_, this, impl_->zoomLevel_));
   impl_->scene_->setSceneRect(rect);
  }
  viewport()->update();
}

double TimelineTrackView::visibleStartFrame() const
{
 if (!viewport() || !impl_->scene_) {
  return 0.0;
 }

 const QRect viewportRect = viewport()->rect();
 if (viewportRect.isEmpty()) {
  return 0.0;
 }

 const QRectF sceneBounds = impl_->scene_->sceneRect();
 return std::clamp(mapToScene(viewportRect.topLeft()).x(), sceneBounds.left(), sceneBounds.right());
}

double TimelineTrackView::visibleEndFrame() const
{
 if (!viewport() || !impl_->scene_) {
  return timelineFrameMax(impl_->duration_);
 }

 const QRect viewportRect = viewport()->rect();
 if (viewportRect.isEmpty()) {
  return timelineFrameMax(impl_->duration_);
 }

 const QRectF sceneBounds = impl_->scene_->sceneRect();
 const double visibleStart = visibleStartFrame();
 const double visibleEnd = std::clamp(mapToScene(viewportRect.topRight()).x(), sceneBounds.left(), sceneBounds.right());
 return std::max(visibleStart + 1.0, visibleEnd);
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
   if (auto* clip = impl_->scene_->addClip(trackIndex, start, duration)) {
    QObject::connect(clip, &ClipItem::geometryEdited, this, [this, clip](ClipItem*, const double startFrame, const double clipDuration) {
     if (!impl_->scene_ || !clip) {
      return;
     }
     const int trackIndex = impl_->scene_->getTrackAtPosition(clip->pos().y() + 1.0);
     Q_EMIT layerClipEdited(clip->layerId(), trackIndex, startFrame, clipDuration);
    });
    QObject::connect(clip, &ClipItem::dragEnded, this, [this, clip](ClipItem*, const double, const double sceneY) {
     if (!impl_->scene_ || !clip) {
      return;
     }
     const int dropTrackIndex = impl_->scene_->getTrackAtPosition(sceneY);
     Q_EMIT layerClipEdited(clip->layerId(), dropTrackIndex, clip->getStart(), clip->getDuration());
    });
    return clip;
   }
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

 void TimelineTrackView::selectClipForLayer(const LayerID& layerId)
 {
  if (!impl_->scene_) {
   return;
  }

  impl_->scene_->clearSelection();
  impl_->lastSelectedClip_ = nullptr;

  if (layerId.isNil()) {
   viewport()->update();
   return;
  }

  for (auto* clip : impl_->scene_->getClips()) {
   if (!clip) {
    continue;
   }
   if (clip->layerId() == layerId) {
    clip->setSelected(true);
    impl_->lastSelectedClip_ = clip;
    ensureVisible(clip);
    break;
   }
  }
  viewport()->update();
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
  if (impl_->scene_) {
   QRectF rect = impl_->scene_->sceneRect();
   rect.setWidth(timelineSceneWidth(impl_->duration_, this, impl_->zoomLevel_));
   if (viewport()) {
    rect.setHeight(std::max(rect.height(), static_cast<double>(viewport()->height())));
   }
   impl_->scene_->setSceneRect(rect);
  }
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
   const QPoint center = viewRect.center().toPoint();

   QFont emptyTitle = painter->font();
   emptyTitle.setPointSize(11);
   emptyTitle.setBold(true);
   painter->setFont(emptyTitle);
   painter->setPen(QColor(205, 205, 205));
   const QFontMetrics titleMetrics(emptyTitle);
   const int titleY = center.y() - 8;
   painter->drawText(QRect(0, titleY - titleMetrics.height(), viewRect.width(), titleMetrics.height() * 2),
    Qt::AlignHCenter | Qt::AlignVCenter, "No layers");

   QFont emptyHint = painter->font();
   emptyHint.setPointSize(9);
   emptyHint.setBold(false);
   painter->setFont(emptyHint);
   painter->setPen(QColor(145, 145, 145));
   const QFontMetrics hintMetrics(emptyHint);
   const int hintY = center.y() + 14;
   painter->drawText(QRect(0, hintY - hintMetrics.height(), viewRect.width(), hintMetrics.height() * 2),
    Qt::AlignHCenter | Qt::AlignVCenter, "Add a layer to see timeline clips");
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
     // Do not seek when operating resize handles; let QGraphicsItem input own this event.
     QGraphicsView::mousePressEvent(event);
     if (clickedClip) {
      impl_->lastSelectedClip_ = clickedClip;
      Q_EMIT clipSelected(clickedClip);
     }
     return;
    } else if (clickedClip) {
     // Let scene/view default selection logic run, then publish selection signal.
     QGraphicsView::mousePressEvent(event);
     if (clickedClip->isSelected()) {
      impl_->lastSelectedClip_ = clickedClip;
      Q_EMIT clipSelected(clickedClip);
     } else {
      Q_EMIT clipDeselected(clickedClip);
     }
     return;
    } else {
     // Scrub hotspot at the very top of the right clip scene.
     // Only seek from this zone when no clip/handle is under cursor.
     if (event->pos().y() <= kTopSeekHotZonePx) {
      setPosition(scenePos.x());
      const double frameMax = std::max(1.0, timelineFrameMax(impl_->duration_));
      const double ratio = impl_->position_ / frameMax;
      Q_EMIT seekPositionChanged(ratio);
      event->accept();
      return;
     }

     // Click on timeline for seeking
     setPosition(scenePos.x());
     const double frameMax = std::max(1.0, timelineFrameMax(impl_->duration_));
     double ratio = impl_->position_ / frameMax;
     Q_EMIT seekPositionChanged(ratio);
     
      if (!(event->modifiers() & Qt::ShiftModifier)) {
       clearSelection();
       Q_EMIT clipDeselected(impl_->lastSelectedClip_);
       impl_->lastSelectedClip_ = nullptr;
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
    const double frameMax = std::max(1.0, timelineFrameMax(impl_->duration_));
    const double ratio = impl_->position_ / frameMax;
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

   auto* vBar = verticalScrollBar();
   auto* hBar = horizontalScrollBar();
   const bool wantsHorizontal = (event->modifiers() & Qt::ShiftModifier);

   if (wantsHorizontal && hBar) {
    int delta = wheelScrollDelta(event, true);
    if (delta == 0) {
     delta = wheelScrollDelta(event, false);
    }
    if (delta != 0) {
     hBar->setValue(hBar->value() - delta);
     event->accept();
     return;
    }
   }

   if (vBar && vBar->maximum() > vBar->minimum()) {
    int delta = wheelScrollDelta(event, false);
    if (delta == 0) {
     delta = wheelScrollDelta(event, true);
    }
    if (delta != 0) {
     vBar->setValue(vBar->value() - delta);
     event->accept();
     return;
    }
   }

   QGraphicsView::wheelEvent(event);
  }

  void TimelineTrackView::resizeEvent(QResizeEvent* event)
  {
   QGraphicsView::resizeEvent(event);
  if (impl_->scene_) {
    QRectF rect = impl_->scene_->sceneRect();
    rect.setHeight(std::max(rect.height(), static_cast<double>(event->size().height())));
    rect.setWidth(timelineSceneWidth(impl_->duration_, this, impl_->zoomLevel_));
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
