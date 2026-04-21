module;
#include <QMouseEvent>
#include <QLinearGradient>
#include <QPainterPath>
#include <QPainter>
#include <QWidget>
#include <wobjectimpl.h>

module Artifact.Timeline.NavigatorWidget;

import std;
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

 int ArtifactTimelineNavigatorWidget::totalFrames() const
 {
  return impl_ ? impl_->totalFrames_ : 0;
 }

 void ArtifactTimelineNavigatorWidget::setStart(float s)
 {
  if (start != s) {
   start = s;
   startChanged(s);
   update();
  }
 }

 void ArtifactTimelineNavigatorWidget::setEnd(float e)
 {
  if (end != e) {
   end = e;
   endChanged(e);
   update();
  }
 }

 void ArtifactTimelineNavigatorWidget::setCurrentFrame(double frame)
 {
  const double sanitized = std::max(0.0, frame);
  if (std::abs(currentFrame_ - sanitized) > 0.0001) {
   currentFrame_ = sanitized;
   update();
  }
 }

 void ArtifactTimelineNavigatorWidget::setTotalFrames(const int totalFrames)
 {
  const int sanitized = std::max(1, totalFrames);
  if (impl_ && impl_->totalFrames_ != sanitized) {
   impl_->totalFrames_ = sanitized;
   update();
  }
 }

 void ArtifactTimelineNavigatorWidget::paintEvent(QPaintEvent*)
 {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const TimelineTheme theme = timelineTheme();
  const QColor playheadColor(255, 106, 71);

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

  if (impl_->currentFrame_ >= 0.0 && impl_->totalFrames_ > 1) {
   const double ratio = std::clamp(impl_->currentFrame_ /
                                       std::max(1.0, static_cast<double>(impl_->totalFrames_ - 1)),
                                   0.0, 1.0);
   const int currentX = kHandleHalfW + static_cast<int>(std::lround(ratio * usableWidth));
   const int clampedCurrentX = std::clamp(currentX, trackRect.left(), trackRect.right());
   const qreal headTop = 1.0;
   const qreal headHeight = std::min<qreal>(10.0, static_cast<qreal>(trackRect.height() - 3));
   const qreal headWidth = 12.0;
   const qreal stemTop = headTop + headHeight + 1.0;
   const qreal stemBottom = static_cast<qreal>(outer.bottom()) - 1.0;
   QPainterPath headPath;
   headPath.moveTo(clampedCurrentX, headTop + headHeight);
   headPath.lineTo(clampedCurrentX - headWidth * 0.5, headTop);
   headPath.lineTo(clampedCurrentX + headWidth * 0.5, headTop);
   headPath.closeSubpath();
   p.setPen(QPen(QColor(18, 18, 18, 150), 1));
   p.setBrush(playheadColor);
   p.drawPath(headPath);
   p.setPen(QPen(playheadColor, 2, Qt::SolidLine, Qt::FlatCap));
   p.drawLine(QPointF(clampedCurrentX, stemTop), QPointF(clampedCurrentX, stemBottom));
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
 }

 void ArtifactTimelineNavigatorWidget::mouseReleaseEvent(QMouseEvent* ev)
 {
  Q_UNUSED(ev);
  impl_->draggingLeft = impl_->draggingRight = impl_->draggingRange = false;
 }
}
