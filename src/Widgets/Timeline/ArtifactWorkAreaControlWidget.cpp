module;
#include <QWidget>
#include <QPainter>
#include <QLinearGradient>
#include <QMouseEvent>
#include <algorithm>

#include <wobjectimpl.h>
module Artifact.Widget.WorkAreaControlWidget;

import Artifact.Event.Types;
import Event.Bus;
import Widgets.Utils.CSS;

namespace Artifact
{
	W_OBJECT_IMPL(WorkAreaControl)

 namespace
 {
  struct TimelineTheme
  {
   QColor background;
   QColor surface;
   QColor border;
   QColor accent;
   QColor workArea;
  };

  TimelineTheme timelineTheme()
  {
   const auto& theme = ArtifactCore::currentDCCTheme();
   return {
    QColor(theme.backgroundColor),
    QColor(theme.secondaryBackgroundColor),
    QColor(theme.borderColor),
    QColor(theme.accentColor),
    QColor(64, 142, 232),
   };
  }
 }
	
 class WorkAreaControl::Impl
 {
public:
  bool draggingLeft{ false };
  bool draggingRight{ false };
 bool draggingRange{ false };
 float dragGrabRatio{ 0.0f };
  bool hoveringLeft{ false };
  bool hoveringRight{ false };
  bool hoveringRange{ false };
  ArtifactCore::EventBus* eventBus_ = nullptr;
 };

 

 WorkAreaControl::WorkAreaControl(QWidget* parent /*= nullptr*/):QWidget(parent),impl_(new Impl())
 {
  setMouseTracking(true);
  setMinimumHeight(26);
 }

 WorkAreaControl::~WorkAreaControl()
 {
  delete impl_;
 }

 void WorkAreaControl::setStart(float s) {
  if (start != s) {
    start = s;
    const auto event = TimelineWorkAreaStartChangedEvent{s};
    if (impl_ && impl_->eventBus_) impl_->eventBus_->post<TimelineWorkAreaStartChangedEvent>(event);
    else ArtifactCore::globalEventBus().post<TimelineWorkAreaStartChangedEvent>(event);
    update();
  }
 }

 void WorkAreaControl::setEnd(float e) {
  if (end != e) {
    end = e;
    const auto event = TimelineWorkAreaEndChangedEvent{e};
    if (impl_ && impl_->eventBus_) impl_->eventBus_->post<TimelineWorkAreaEndChangedEvent>(event);
    else ArtifactCore::globalEventBus().post<TimelineWorkAreaEndChangedEvent>(event);
    update();
  }
 }

 void WorkAreaControl::setEventBus(ArtifactCore::EventBus* eventBus) {
  if (impl_) {
    impl_->eventBus_ = eventBus;
  }
 }

 void WorkAreaControl::paintEvent(QPaintEvent*)
 {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const TimelineTheme theme = timelineTheme();

  // Background
  p.fillRect(rect(), theme.background);

  // Constrain usable width to keep handles fully inside
  const int handleHalfW = 6;
  const int handleW = handleHalfW * 2;
  int usableWidth = width() - handleW;

  int x1 = handleHalfW + static_cast<int>(start * usableWidth);
  int x2 = handleHalfW + static_cast<int>(end * usableWidth);

  // Darken outside range
  p.fillRect(0, 0, x1, height(), QColor(0, 0, 0, 100));
  p.fillRect(x2, 0, width() - x2, height(), QColor(0, 0, 0, 100));

  // Range strip
  QRect rangeRect(x1, 0, x2 - x1, height());
  QLinearGradient grad(rangeRect.topLeft(), rangeRect.bottomLeft());
  grad.setColorAt(0, theme.workArea.lighter(122));
  grad.setColorAt(1, theme.workArea.darker(126));
  p.fillRect(rangeRect, grad);

  // Bottom border for work area
  p.setPen(QPen(theme.workArea.lighter(116), 2));
  p.drawLine(x1, height() - 1, x2, height() - 1);

  // Handles (Blue AE style) - highlight on hover
  const int handleTopInset = 1;
  const int handleHeight = std::max(1, height() - 2);
  
  // Left handle
  if (impl_->hoveringLeft || impl_->draggingLeft) {
    p.setBrush(theme.workArea.lighter(138));
  } else {
    p.setBrush(theme.workArea);
  }
  p.setPen(QPen(theme.border.lighter(120), 1));
  p.drawRoundedRect(QRectF(x1 - handleHalfW, handleTopInset, handleW, handleHeight), 2, 2);
  
  // Right handle
  if (impl_->hoveringRight || impl_->draggingRight) {
    p.setBrush(theme.workArea.lighter(138));
  } else {
    p.setBrush(theme.workArea);
  }
  p.drawRoundedRect(QRectF(x2 - handleHalfW, handleTopInset, handleW, handleHeight), 2, 2);
 }

 void WorkAreaControl::mouseMoveEvent(QMouseEvent* ev)
 {
  const int handleHalfW = 6;
  const int handleW = handleHalfW * 2;
  const int usableWidth = std::max(1, width() - handleW);

  // Update hover state
  int x1 = handleHalfW + static_cast<int>(start * usableWidth);
  int x2 = handleHalfW + static_cast<int>(end * usableWidth);
  
  bool newHoverLeft = QRect(x1 - handleHalfW, 0, handleW, height()).contains(ev->pos());
  bool newHoverRight = QRect(x2 - handleHalfW, 0, handleW, height()).contains(ev->pos());
  bool newHoverRange = QRect(x1 + handleHalfW, 0, std::max(0, x2 - x1 - handleW), height()).contains(ev->pos());
  
  bool hoverChanged = (impl_->hoveringLeft != newHoverLeft) || 
                      (impl_->hoveringRight != newHoverRight) || 
                      (impl_->hoveringRange != newHoverRange);
  
  impl_->hoveringLeft = newHoverLeft;
  impl_->hoveringRight = newHoverRight;
  impl_->hoveringRange = newHoverRange;
  
  // Update cursor
  if (impl_->hoveringLeft || impl_->hoveringRight) {
    setCursor(Qt::SizeHorCursor);
  } else if (impl_->hoveringRange) {
    setCursor(Qt::SizeAllCursor);
  } else {
    unsetCursor();
  }
  
  if (hoverChanged) {
    update();
  }

  if (!(ev->buttons() & Qt::LeftButton)) {
   impl_->draggingLeft = impl_->draggingRight = impl_->draggingRange = false;
   return;
  }

  if (impl_->draggingLeft) {
   float newStart = (float(ev->pos().x()) - handleHalfW) / float(usableWidth);
   setStart(qBound(0.0f, newStart, end - 0.01f));
  }
  else if (impl_->draggingRight) {
   float newEnd = (float(ev->pos().x()) - handleHalfW) / float(usableWidth);
   setEnd(qBound(start + 0.01f, newEnd, 1.0f));
  } else if (impl_->draggingRange) {
   const float range = std::max(0.01f, end - start);
   float left = (float(ev->pos().x()) - handleHalfW) / float(usableWidth) - impl_->dragGrabRatio;
   left = qBound(0.0f, left, 1.0f - range);
   setStart(left);
   setEnd(left + range);
  }
 }

 void WorkAreaControl::mouseReleaseEvent(QMouseEvent*)
 {
  impl_->draggingLeft = impl_->draggingRight = impl_->draggingRange = false;
  // Reset hover state on release
  impl_->hoveringLeft = impl_->hoveringRight = impl_->hoveringRange = false;
  unsetCursor();
  update();
 }

 void WorkAreaControl::mousePressEvent(QMouseEvent* ev)
 {
  if (ev->button() != Qt::LeftButton) return;
  const int handleHalfW = 6;
  const int handleW = handleHalfW * 2;
  const int usableWidth = std::max(1, width() - handleW);

  int x1 = handleHalfW + static_cast<int>(start * usableWidth);
  int x2 = handleHalfW + static_cast<int>(end * usableWidth);

  if (QRect(x1 - handleHalfW, 0, handleW, height()).contains(ev->pos())) impl_->draggingLeft = true;
  else if (QRect(x2 - handleHalfW, 0, handleW, height()).contains(ev->pos())) impl_->draggingRight = true;
  else if (QRect(x1 + handleHalfW, 0, std::max(0, x2 - x1 - handleW), height()).contains(ev->pos())) {
   impl_->draggingRange = true;
   const float normalizedX = (float(ev->pos().x()) - handleHalfW) / float(usableWidth);
   impl_->dragGrabRatio = normalizedX - start;
  }
 }

 void WorkAreaControl::leaveEvent(QEvent* event)
 {
  impl_->hoveringLeft = impl_->hoveringRight = impl_->hoveringRange = false;
  unsetCursor();
  update();
  QWidget::leaveEvent(event);
 }

};
