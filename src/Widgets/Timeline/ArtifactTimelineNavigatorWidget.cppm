module;
#include <QColor>
#include <QLinearGradient>
#include <QPaintEvent>
#include <QPainterPath>
#include <QPen>
#include <QRect>
#include <QRectF>
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>
#include <wobjectimpl.h>
#include "TimelinePlayheadDraw.hpp"

module Artifact.Timeline.NavigatorWidget;

import std;
import Event.Bus;
import Artifact.Event.Types;
import Widgets.Utils.CSS;

namespace Artifact
{
 W_OBJECT_IMPL(ArtifactTimelineNavigatorWidget)

 namespace
 {
  constexpr int kHandleHalfW = 6;
  constexpr int kHandleW = kHandleHalfW * 2;

  struct TimelineTheme
  {
   QColor background;
   QColor surface;
   QColor border;
   QColor accent;
   QColor text;
  };

  TimelineTheme timelineTheme()
  {
   const auto& theme = ArtifactCore::currentDCCTheme();
   return {
    QColor(theme.backgroundColor),
    QColor(theme.secondaryBackgroundColor),
    QColor(theme.borderColor),
    QColor(theme.accentColor),
    QColor(theme.textColor),
   };
  }
 }

 class ArtifactTimelineNavigatorWidget::Impl
 {
 public:
  Impl();
  ~Impl();
  int totalFrames_ = 300;
  bool draggingLeft{ false };
  bool draggingRight{ false };
  bool draggingRange{ false };
  float dragGrabRatio{ 0.0f };
 };

 ArtifactTimelineNavigatorWidget::Impl::Impl()
 {
 }

 ArtifactTimelineNavigatorWidget::Impl::~Impl()
 {
 }

 ArtifactTimelineNavigatorWidget::ArtifactTimelineNavigatorWidget(QWidget* parent /*= nullptr*/)
  : QWidget(parent), impl_(new Impl())
 {
  setMinimumHeight(16);
  setMouseTracking(true);
 }

 ArtifactTimelineNavigatorWidget::~ArtifactTimelineNavigatorWidget()
 {
  delete impl_;
 }

 void ArtifactTimelineNavigatorWidget::rangeChanged()
 {
  ArtifactCore::globalEventBus().publish<TimelineNavigatorRangeChangedEvent>(
      TimelineNavigatorRangeChangedEvent{start, end});
 }

 int ArtifactTimelineNavigatorWidget::totalFrames() const
 {
  return impl_ ? impl_->totalFrames_ : 0;
 }

 void ArtifactTimelineNavigatorWidget::setStart(float s)
 {
  if (start != s) {
   start = s;
   update();
  }
 }

 void ArtifactTimelineNavigatorWidget::setEnd(float e)
 {
  if (end != e) {
   end = e;
   update();
  }
 }

 void ArtifactTimelineNavigatorWidget::setCurrentFrame(double frame)
 {
  const double finiteFrame = std::isfinite(frame) ? frame : 0.0;
  const double sanitized = std::clamp(
      finiteFrame, 0.0,
      static_cast<double>(std::max(0, impl_->totalFrames_ - 1)));
  if (std::abs(currentFrame_ - sanitized) > 0.0001) {
   currentFrame_ = sanitized;
   update();
  }
 }

 void ArtifactTimelineNavigatorWidget::setTotalFrames(const int totalFrames)
 {
  const int sanitized = std::max(1, totalFrames);
  if (!impl_) {
   return;
  }
  const double maxFrame = static_cast<double>(std::max(0, sanitized - 1));
  const double clampedCurrentFrame =
      std::clamp(currentFrame_, 0.0, maxFrame);
  const bool totalChanged = impl_->totalFrames_ != sanitized;
  const bool frameChanged =
      std::abs(currentFrame_ - clampedCurrentFrame) > 0.0001;
  if (totalChanged || frameChanged) {
   impl_->totalFrames_ = sanitized;
   currentFrame_ = clampedCurrentFrame;
   update();
  }
 }

 void ArtifactTimelineNavigatorWidget::paintEvent(QPaintEvent*)
 {
  QPainter p(this);
  TimelinePlayheadDraw::enableTimelinePainterHints(p);
  const TimelineTheme theme = timelineTheme();
  const QColor playheadColor = TimelinePlayheadDraw::playheadColor();

  const QRect outer = rect();
  p.fillRect(outer, theme.background);

  p.setPen(theme.border);
  p.drawLine(outer.topLeft(), outer.topRight());
  p.setPen(theme.background.darker(170));
  p.drawLine(outer.bottomLeft(), outer.bottomRight());

  const QRect trackRect = outer.adjusted(kHandleHalfW, 4, -kHandleHalfW, -4);
  if (!trackRect.isValid() || trackRect.width() <= 0) {
   return;
  }

  p.setPen(Qt::NoPen);
  p.setBrush(theme.surface);
  p.drawRoundedRect(trackRect, 3, 3);

  const int usableWidth = std::max(1, width() - kHandleW);
  const int x1 = kHandleHalfW + static_cast<int>(start * usableWidth);
  const int x2 = kHandleHalfW + static_cast<int>(end * usableWidth);
  const int clampedX1 = std::clamp(x1, trackRect.left(), trackRect.right());
  const int clampedX2 = std::clamp(x2, trackRect.left(), trackRect.right());

  p.fillRect(QRect(trackRect.left(), trackRect.top(), std::max(0, clampedX1 - trackRect.left()), trackRect.height()),
             QColor(0, 0, 0, 70));
  p.fillRect(QRect(clampedX2, trackRect.top(), std::max(0, trackRect.right() - clampedX2), trackRect.height()),
             QColor(0, 0, 0, 70));

  p.setPen(QPen(theme.border.lighter(105), 1));
  const int segmentCount = std::clamp(trackRect.width() / 72, 4, 12);
  for (int i = 1; i < segmentCount; ++i) {
   const int x = trackRect.left() + static_cast<int>(std::lround((static_cast<double>(i) / segmentCount) * trackRect.width()));
   p.drawLine(x, trackRect.top() + 2, x, trackRect.bottom() - 2);
  }

  if (impl_->totalFrames_ > 1 && trackRect.width() > 24) {
   const int approxMajorCount = std::clamp(trackRect.width() / 96, 4, 10);
   const int majorStepFrames = std::max(1, (impl_->totalFrames_ - 1) / approxMajorCount);
   const int minorStepFrames = std::max(1, majorStepFrames / 4);

   p.setPen(QPen(theme.border.lighter(140), 1));
   for (int f = 0; f < impl_->totalFrames_; f += minorStepFrames) {
    const double ratio = static_cast<double>(f) / std::max(1, impl_->totalFrames_ - 1);
    const int x = trackRect.left() + static_cast<int>(std::lround(ratio * trackRect.width()));
    if (x < trackRect.left() || x > trackRect.right()) {
     continue;
    }
    const bool major = (f % majorStepFrames) == 0;
    const int tickTop = major ? trackRect.top() + 1 : trackRect.top() + 4;
    const int tickBottom = major ? trackRect.bottom() - 1 : trackRect.bottom() - 4;
    p.drawLine(x, tickTop, x, tickBottom);
   }
  }

  const QRect rangeRect(clampedX1, trackRect.top(), std::max(1, clampedX2 - clampedX1), trackRect.height());
  QLinearGradient grad(rangeRect.topLeft(), rangeRect.bottomLeft());
  grad.setColorAt(0.0, playheadColor.lighter(110));
  grad.setColorAt(1.0, playheadColor.darker(135));
  p.setPen(QPen(playheadColor.lighter(120), 1));
  p.setBrush(grad);
  p.drawRoundedRect(rangeRect.adjusted(0, 0, -1, 0), 3, 3);

  const QRectF leftHandleRect(clampedX1 - kHandleHalfW, 2, kHandleW, height() - 4);
  const QRectF rightHandleRect(clampedX2 - kHandleHalfW, 2, kHandleW, height() - 4);
  p.setBrush(theme.surface.lighter(130));
  p.setPen(QPen(theme.border.darker(135), 1));
  p.drawRoundedRect(leftHandleRect, 2, 2);
  p.drawRoundedRect(rightHandleRect, 2, 2);

  if (currentFrame_ >= 0.0 && impl_->totalFrames_ > 1) {
   // Keep the navigator playhead on the same visible-range mapping used by
   // the ruler and TimelinePlayheadOverlay. Mapping against the full duration
   // makes the two indicators diverge as soon as the navigator is zoomed.
   const double totalFrames = static_cast<double>(impl_->totalFrames_);
   const double visibleStart = static_cast<double>(start) * totalFrames;
   const double visibleDuration = std::max(
       0.01, static_cast<double>(end - start) * totalFrames);
   const double visibleRatio = (currentFrame_ - visibleStart) / visibleDuration;
   if (visibleRatio < 0.0 || visibleRatio > 1.0) {
    return;
   }
   const int currentX = static_cast<int>(
       std::lround(visibleRatio * static_cast<double>(width())));
   const int clampedCurrentX =
       std::clamp(currentX, 0, std::max(0, width() - 1));
   const qreal stemBottom = static_cast<qreal>(outer.bottom()) - 1.0;
   TimelinePlayheadDraw::drawPlayhead(
       p, static_cast<qreal>(clampedCurrentX), 0.0, stemBottom, false, 1.0,
       std::min<qreal>(10.0, static_cast<qreal>(trackRect.height() - 3)), 12.0);
  }
 }

 void ArtifactTimelineNavigatorWidget::mousePressEvent(QMouseEvent* ev)
 {
  if (ev->button() != Qt::LeftButton) {
   return;
  }

  const int usableWidth = std::max(1, width() - kHandleW);
  const int x1 = kHandleHalfW + static_cast<int>(start * usableWidth);
  const int x2 = kHandleHalfW + static_cast<int>(end * usableWidth);

  if (QRect(x1 - kHandleHalfW, 0, kHandleW, height()).contains(ev->pos())) {
   impl_->draggingLeft = true;
  } else if (QRect(x2 - kHandleHalfW, 0, kHandleW, height()).contains(ev->pos())) {
   impl_->draggingRight = true;
  } else if (QRect(x1 + kHandleHalfW, 0, std::max(0, x2 - x1 - kHandleW), height()).contains(ev->pos())) {
   impl_->draggingRange = true;
   const float normalizedX = (float(ev->pos().x()) - kHandleHalfW) / float(usableWidth);
   impl_->dragGrabRatio = normalizedX - start;
  }
 }

 void ArtifactTimelineNavigatorWidget::mouseMoveEvent(QMouseEvent* ev)
 {
  const int usableWidth = std::max(1, width() - kHandleW);

  if (!(ev->buttons() & Qt::LeftButton)) {
   impl_->draggingLeft = impl_->draggingRight = impl_->draggingRange = false;
   return;
  }

  const float oldStart = start;
  const float oldEnd = end;

  if (impl_->draggingLeft) {
   float newStart = (float(ev->pos().x()) - kHandleHalfW) / float(usableWidth);
   setStart(qBound(0.0f, newStart, end - 0.01f));
  } else if (impl_->draggingRight) {
   float newEnd = (float(ev->pos().x()) - kHandleHalfW) / float(usableWidth);
   setEnd(qBound(start + 0.01f, newEnd, 1.0f));
  } else if (impl_->draggingRange) {
   const float range = std::max(0.01f, end - start);
   float left = (float(ev->pos().x()) - kHandleHalfW) / float(usableWidth) - impl_->dragGrabRatio;
   left = qBound(0.0f, left, 1.0f - range);
   setStart(left);
   setEnd(left + range);
  }
  if (start != oldStart || end != oldEnd) {
   rangeChanged();
  }
 }

 void ArtifactTimelineNavigatorWidget::mouseReleaseEvent(QMouseEvent* ev)
 {
  Q_UNUSED(ev);
  impl_->draggingLeft = impl_->draggingRight = impl_->draggingRange = false;
 }
}
