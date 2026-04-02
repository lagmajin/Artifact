module;
#include <QMouseEvent>
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

  struct TimelineTheme {
   QColor background;
   QColor track;
   QColor trackBorder;
   QColor grid;
   QColor mutedText;
   QColor accent;
   QColor handle;
  };

  TimelineTheme timelineTheme()
  {
   const auto& theme = ArtifactCore::currentDCCTheme();
   TimelineTheme colors;
   colors.background = QColor(theme.backgroundColor);
   colors.track = QColor(theme.secondaryBackgroundColor);
   colors.trackBorder = QColor(theme.borderColor);
   colors.grid = colors.trackBorder.darker(120);
   colors.mutedText = QColor(theme.textColor).darker(120);
   colors.accent = QColor(theme.accentColor);
   colors.handle = QColor(theme.textColor);
   return colors;
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
  const auto colors = timelineTheme();

  const QRect outer = rect();
  p.fillRect(outer, colors.background);

  p.setPen(colors.trackBorder);
  p.drawLine(outer.topLeft(), outer.topRight());
  p.setPen(colors.background.darker(165));
  p.drawLine(outer.bottomLeft(), outer.bottomRight());

  const QRect trackRect = outer.adjusted(kHandleHalfW, 4, -kHandleHalfW, -4);
  if (!trackRect.isValid() || trackRect.width() <= 0) {
   return;
  }

  p.setPen(Qt::NoPen);
  p.setBrush(colors.track);
  p.drawRoundedRect(trackRect, 3, 3);

  const int usableWidth = std::max(1, width() - kHandleW);
  const int x1 = kHandleHalfW + static_cast<int>(start * usableWidth);
  const int x2 = kHandleHalfW + static_cast<int>(end * usableWidth);
  const int clampedX1 = std::clamp(x1, trackRect.left(), trackRect.right());
  const int clampedX2 = std::clamp(x2, trackRect.left(), trackRect.right());

  QColor dimmed = colors.background.darker(145);
  dimmed.setAlpha(76);
  p.fillRect(QRect(trackRect.left(), trackRect.top(), std::max(0, clampedX1 - trackRect.left()), trackRect.height()),
             dimmed);
  p.fillRect(QRect(clampedX2, trackRect.top(), std::max(0, trackRect.right() - clampedX2), trackRect.height()),
             dimmed);

  p.setPen(QPen(colors.grid, 1));
  const int segmentCount = std::clamp(trackRect.width() / 72, 4, 12);
  for (int i = 1; i < segmentCount; ++i) {
   const int x = trackRect.left() + static_cast<int>(std::lround((static_cast<double>(i) / segmentCount) * trackRect.width()));
   p.drawLine(x, trackRect.top() + 2, x, trackRect.bottom() - 2);
  }

  if (impl_->totalFrames_ > 1 && trackRect.width() > 24) {
   const int approxMajorCount = std::clamp(trackRect.width() / 96, 4, 10);
   const int majorStepFrames = std::max(1, (impl_->totalFrames_ - 1) / approxMajorCount);
   const int minorStepFrames = std::max(1, majorStepFrames / 4);

   p.setPen(QPen(colors.mutedText, 1));
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
  grad.setColorAt(0.0, colors.accent.lighter(112));
  grad.setColorAt(1.0, colors.accent.darker(130));
  p.setPen(QPen(colors.accent.lighter(110), 1));
  p.setBrush(grad);
  p.drawRoundedRect(rangeRect.adjusted(0, 0, -1, 0), 3, 3);

  const QRectF leftHandleRect(clampedX1 - kHandleHalfW, 2, kHandleW, height() - 4);
  const QRectF rightHandleRect(clampedX2 - kHandleHalfW, 2, kHandleW, height() - 4);
  p.setBrush(colors.handle);
  p.setPen(QPen(colors.background.darker(180), 1));
  p.drawRoundedRect(leftHandleRect, 2, 2);
  p.drawRoundedRect(rightHandleRect, 2, 2);

  p.setPen(colors.accent);
  p.drawLine(rangeRect.left(), outer.bottom() - 1, rangeRect.right(), outer.bottom() - 1);
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
