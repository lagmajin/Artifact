module;
#include <utility>
#include <QDebug>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPolygon>
#include <QRect>
#include <QSizePolicy>
#include <QWidget>
#include <wobjectimpl.h>

module Artifact.Timeline.ScrubBar;

import std;
import ArtifactCore.Utils.PerformanceProfiler;

import Frame.Position;
import Widgets.Utils.CSS;

namespace Artifact
{
 W_OBJECT_IMPL(ArtifactTimelineScrubBar)

 namespace
 {
  constexpr int kScrubBarHorizontalPadding = 8;

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

 class ArtifactTimelineScrubBar::Impl
 {
 public:
  Impl();
  ~Impl();

  FramePosition currentFrame_;
  int totalFrames_ = 100;
  bool dragging_ = false;
  int handleWidth_ = 10;
  int handleHeight_ = 22;
  bool seekLockDuringPlayback_ = true;
  bool isPlaying_ = false;
  bool hover_ = false;
  bool interactiveSeekingEnabled_ = true;
  bool cacheRangeVisible_ = false;
  int cacheRangeStart_ = 0;
  int cacheRangeEnd_ = 0;
  std::vector<bool> cacheBitmap_;
  int fps_ = 30;
  double rulerPixelsPerFrame_ = 0.0;  // 0 = ruler無効
  double rulerHorizontalOffset_ = 0.0;

  int trackLeft(int width) const
  {
   if (width <= 1) return 0;
   return std::min(kScrubBarHorizontalPadding, width - 1);
  }

  int trackRight(int width) const
  {
   if (width <= 1) return 0;
   return std::max(trackLeft(width), width - trackLeft(width) - 1);
  }

  int frameToX(int frame, int width) const
  {
   const int left = trackLeft(width);
   const int right = trackRight(width);
   const int span = std::max(0, right - left);
   if (totalFrames_ <= 1 || span == 0) return left;
   const double ratio = static_cast<double>(frame) / (totalFrames_ - 1);
   return left + static_cast<int>(std::lround(ratio * span));
  }

  int xToFrame(int x, int width) const
  {
   const int left = trackLeft(width);
   const int right = trackRight(width);
   const int span = std::max(0, right - left);
   if (totalFrames_ <= 1 || span == 0) return 0;
   const int clampedX = std::clamp(x, left, right);
   const double ratio = static_cast<double>(clampedX - left) / span;
   return static_cast<int>(std::lround(ratio * (totalFrames_ - 1)));
  }

  // When the ruler overlay is active, use zoom-based coordinates so that
  // the handle position and mouse-seek both align with the ruler tick marks.
  int resolveFrameToX(int frame, int width) const
  {
   if (rulerPixelsPerFrame_ > 0.001) {
    return static_cast<int>(std::round(frame * rulerPixelsPerFrame_ - rulerHorizontalOffset_));
   }
   return frameToX(frame, width);
  }

  int resolveXToFrame(int x, int width) const
  {
   if (rulerPixelsPerFrame_ > 0.001) {
    const double f = (static_cast<double>(x) + rulerHorizontalOffset_)
                     / std::max(0.001, rulerPixelsPerFrame_);
    return std::clamp(static_cast<int>(std::round(f)), 0, std::max(0, totalFrames_ - 1));
   }
   return xToFrame(x, width);
  }
 };

 ArtifactTimelineScrubBar::Impl::Impl()
  : currentFrame_(-1)
 {
 }

 ArtifactTimelineScrubBar::Impl::~Impl()
 {
 }

 ArtifactTimelineScrubBar::ArtifactTimelineScrubBar(QWidget* parent)
  : QWidget(parent), impl_(new Impl)
 {
  setAttribute(Qt::WA_NoSystemBackground, false);
  setAttribute(Qt::WA_OpaquePaintEvent, false);
  setAttribute(Qt::WA_StyledBackground, true);
  setMouseTracking(true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setMinimumHeight(34);
 }

 ArtifactTimelineScrubBar::~ArtifactTimelineScrubBar()
 {
  delete impl_;
 }

 QSize ArtifactTimelineScrubBar::sizeHint() const
 {
  return QSize(640, 20);
 }

 QSize ArtifactTimelineScrubBar::minimumSizeHint() const
 {
  return QSize(120, 20);
 }

 FramePosition ArtifactTimelineScrubBar::currentFrame() const
 {
  return impl_->currentFrame_;
 }

 void ArtifactTimelineScrubBar::setCurrentFrame(const FramePosition& frame)
 {
  int frameValue = frame.framePosition();
  frameValue = qBound(0, frameValue, impl_->totalFrames_ - 1);

  if (impl_->currentFrame_.framePosition() != frameValue) {
   impl_->currentFrame_ = FramePosition(frameValue);
   update();
   Q_EMIT frameChanged(impl_->currentFrame_);
  }
 }

 void ArtifactTimelineScrubBar::setTotalFrames(int totalFrames)
 {
  if (totalFrames > 0 && impl_->totalFrames_ != totalFrames) {
   impl_->totalFrames_ = totalFrames;
   impl_->cacheRangeStart_ = std::clamp(impl_->cacheRangeStart_, 0, totalFrames - 1);
   impl_->cacheRangeEnd_ = std::clamp(impl_->cacheRangeEnd_, 0, totalFrames - 1);
   if (impl_->currentFrame_.framePosition() >= totalFrames) {
    setCurrentFrame(FramePosition(totalFrames - 1));
   }
   update();
  }
 }

 int ArtifactTimelineScrubBar::totalFrames() const
 {
  return impl_->totalFrames_;
 }

 void ArtifactTimelineScrubBar::setSeekLockDuringPlayback(bool lock)
 {
  impl_->seekLockDuringPlayback_ = lock;
 }

 bool ArtifactTimelineScrubBar::isSeekLockedDuringPlayback() const
 {
  return impl_->seekLockDuringPlayback_;
 }

 void ArtifactTimelineScrubBar::setIsPlaying(bool playing)
 {
  impl_->isPlaying_ = playing;
 }

  bool ArtifactTimelineScrubBar::isPlaying() const
  {
   return impl_->isPlaying_;
  }

  void ArtifactTimelineScrubBar::setFps(int fps)
  {
   if (fps > 0 && impl_->fps_ != fps) {
    impl_->fps_ = fps;
    update();
   }
  }

  int ArtifactTimelineScrubBar::fps() const
  {
   return impl_->fps_;
  }

 void ArtifactTimelineScrubBar::setCachedFrameRange(int startFrame, int endFrame, bool visible)
 {
  const int maxFrame = std::max(0, impl_->totalFrames_ - 1);
  const int clampedStart = std::clamp(std::min(startFrame, endFrame), 0, maxFrame);
  const int clampedEnd = std::clamp(std::max(startFrame, endFrame), 0, maxFrame);
  const bool changed = impl_->cacheRangeVisible_ != visible ||
                       impl_->cacheRangeStart_ != clampedStart ||
                       impl_->cacheRangeEnd_ != clampedEnd;
  impl_->cacheRangeVisible_ = visible;
  impl_->cacheRangeStart_ = clampedStart;
  impl_->cacheRangeEnd_ = clampedEnd;
  if (changed) {
   update();
  }
 }

 void ArtifactTimelineScrubBar::setCacheBitmap(const std::vector<bool>& bitmap)
 {
  if (impl_->cacheBitmap_ != bitmap) {
   impl_->cacheBitmap_ = bitmap;
   update();
  }
 }

 void ArtifactTimelineScrubBar::setInteractiveSeekingEnabled(bool enabled)
 {
  if (impl_->interactiveSeekingEnabled_ == enabled) {
   return;
  }
  impl_->interactiveSeekingEnabled_ = enabled;
  if (!enabled) {
   impl_->dragging_ = false;
   impl_->hover_ = false;
   setCursor(Qt::ArrowCursor);
  }
  update();
 }

 bool ArtifactTimelineScrubBar::interactiveSeekingEnabled() const
 {
  return impl_->interactiveSeekingEnabled_;
 }

  void ArtifactTimelineScrubBar::setRulerPixelsPerFrame(double ppf)
 {
  if (std::abs(impl_->rulerPixelsPerFrame_ - ppf) > 0.0001) {
   impl_->rulerPixelsPerFrame_ = ppf;
   update();
  }
 }

 void ArtifactTimelineScrubBar::setRulerHorizontalOffset(double offset)
 {
  if (std::abs(impl_->rulerHorizontalOffset_ - offset) > 0.0001) {
   impl_->rulerHorizontalOffset_ = offset;
   update();
  }
 }

 void ArtifactTimelineScrubBar::paintEvent(QPaintEvent* event)
 {
  ArtifactCore::ProfileTimer _profTimer("ScrubBarPaint",
                                        ArtifactCore::ProfileCategory::UI);
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  const TimelineTheme theme = timelineTheme();

  const QRect r = rect();
  const int w = r.width();
  const int h = r.height();
  const int currentX = impl_->resolveFrameToX(std::max(0, static_cast<int>(impl_->currentFrame_.framePosition())), w);
  const int railHalfH = std::max(3, h / 7);
  const int railBottomInset = std::max(3, h / 10);
  const int centerY = h - railBottomInset - railHalfH;
  const int trackLeft = impl_->trackLeft(w);
  const int trackRight = impl_->trackRight(w);
  const int topBandHeight = std::max(11, h / 3);
  const QRect railRect(trackLeft, centerY - railHalfH + 2, std::max(1, trackRight - trackLeft + 1), railHalfH * 2);

  const QColor bgTop = theme.background.lighter(112);
  const QColor bgBottom = theme.background.darker(116);
  const QColor railColor = theme.surface.darker(108);
  const QColor railBorder = theme.border;
  const QColor cacheBaseColor(84, 198, 120);
  const QColor playheadColor = theme.accent;
  
  QLinearGradient bgGrad(r.topLeft(), r.bottomLeft());
  bgGrad.setColorAt(0.0, bgTop);
  bgGrad.setColorAt(1.0, bgBottom);
  p.fillRect(r, bgGrad);

  QRect topBand = r;
  topBand.setHeight(topBandHeight);
  QLinearGradient topBandGrad(topBand.topLeft(), topBand.bottomLeft());
  topBandGrad.setColorAt(0.0, theme.surface.lighter(118));
  topBandGrad.setColorAt(1.0, theme.surface.darker(112));
  p.fillRect(topBand, topBandGrad);

  p.setPen(theme.background.darker(180));
  p.drawLine(r.topLeft(), r.topRight());
  p.setPen(theme.background.darker(220));
  p.drawLine(r.bottomLeft(), r.bottomRight());
  p.setPen(theme.border.lighter(110));
  p.drawRect(r.adjusted(0, 0, -1, -1));

  // ── ルーラー描画 ──────────────────────
  if (impl_->rulerPixelsPerFrame_ > 0.001) {
   const double ppf    = impl_->rulerPixelsPerFrame_;
   const double xOff   = impl_->rulerHorizontalOffset_;
   constexpr int kMajorStepCandidates[] = {1,2,5,10,15,20,30,50,100,150,200,300,600};
   int majorStep = 10;
   for (int c : kMajorStepCandidates) {
    if (c * ppf >= 60.0) { majorStep = c; break; }
   }
   const int minorStep = std::max(1, majorStep / 5);

   const int fStart = std::max(0, static_cast<int>(std::floor(xOff / ppf)));
   const int fEnd   = std::min(impl_->totalFrames_,
                               static_cast<int>(std::ceil((xOff + w) / ppf)) + 1);

   QFont rulerFont;
   rulerFont.setPixelSize(9);
   p.setFont(rulerFont);

   double lastLabelRight = -1.0;
   for (int f = fStart; f <= fEnd; f += minorStep) {
    const double rx = f * ppf - xOff;
    if (rx < 0.0 || rx > w) continue;
    const bool isMajor = (f % majorStep) == 0;
    const int tickH = isMajor ? (topBandHeight - 2) : (topBandHeight / 2);
    p.setPen(QPen(isMajor ? theme.border.lighter(150) : theme.border.darker(115), 1));
    p.drawLine(QPointF(rx, topBandHeight - tickH), QPointF(rx, topBandHeight - 1));
    if (isMajor) {
     const QString label = QString::number(f);
     const double labelW = static_cast<double>(QFontMetrics(p.font()).horizontalAdvance(label));
     const double labelX = rx + 3.0;
     if (labelX <= lastLabelRight + 6.0) continue;
     p.setPen(theme.text.darker(130));
     p.drawText(QRectF(labelX, 0.0, labelW + 6.0, topBandHeight - 2), Qt::AlignLeft | Qt::AlignVCenter, label);
     lastLabelRight = labelX + labelW;
    }
   }
  }

  // ── レール描画 ──────────────────────
  p.setPen(QPen(railBorder, 1));
  p.setBrush(railColor);
  p.drawRoundedRect(railRect, railHalfH, railHalfH);

  // ── キャッシュ描画 (Bitmap or Range) ──────────────────────
  if (!impl_->cacheBitmap_.empty()) {
   p.setPen(Qt::NoPen);
   p.setBrush(cacheBaseColor);
   
   int startF = -1;
   for (int f = 0; f < static_cast<int>(impl_->cacheBitmap_.size()); ++f) {
    if (impl_->cacheBitmap_[f]) {
     if (startF == -1) startF = f;
    } else {
     if (startF != -1) {
      const int x1 = impl_->resolveFrameToX(startF, w);
      const int x2 = impl_->resolveFrameToX(f, w);
      p.drawRect(QRect(x1, railRect.top() + 2, std::max(1, x2 - x1), railRect.height() - 4));
      startF = -1;
     }
    }
   }
   if (startF != -1) {
    const int x1 = impl_->resolveFrameToX(startF, w);
    const int x2 = impl_->resolveFrameToX(static_cast<int>(impl_->cacheBitmap_.size()), w);
    p.drawRect(QRect(x1, railRect.top() + 2, std::max(1, x2 - x1), railRect.height() - 4));
   }
  } else if (impl_->cacheRangeVisible_) {
   const int cacheStartX = impl_->resolveFrameToX(impl_->cacheRangeStart_, w);
   const int cacheEndX = impl_->resolveFrameToX(impl_->cacheRangeEnd_, w);
   const int cacheLeft = std::clamp(std::min(cacheStartX, cacheEndX), railRect.left(), railRect.right());
   const int cacheRight = std::clamp(std::max(cacheStartX, cacheEndX), railRect.left(), railRect.right());
   const QRect cacheRect(cacheLeft, railRect.top(), std::max(1, cacheRight - cacheLeft + 1), railRect.height());
   if (cacheRect.width() > 1) {
    p.setPen(QPen(cacheBaseColor.lighter(124), 1));
    p.setBrush(cacheBaseColor.darker(110));
    p.drawRoundedRect(cacheRect.adjusted(0, 0, -1, 0), railHalfH, railHalfH);
   }
  }

  // ── 再生ヘッド描画 ──────────────────────
  const int clampedX = std::clamp(currentX, railRect.left(), railRect.right());
  if (railRect.width() > 0) {
   p.setPen(Qt::NoPen);
   p.setBrush(QColor(playheadColor.red(), playheadColor.green(), playheadColor.blue(), 220));
   p.drawRect(QRect(clampedX - 1, railRect.top() - 2, 3, railRect.height() + 4));
  }

  // ── 目盛り描画 ──────────────────────
  if (impl_->totalFrames_ > 1 && railRect.width() > 20) {
   const int approxMajorCount = std::clamp(railRect.width() / 90, 6, 16);
   const int majorStepFrames = std::max(1, (impl_->totalFrames_ - 1) / approxMajorCount);
   const int minorStepFrames = std::max(1, majorStepFrames / 5);
   p.setPen(QPen(theme.border.darker(110), 1));
   for (int f = 0; f < impl_->totalFrames_; f += minorStepFrames) {
    const int x = impl_->frameToX(f, w);
    if (x < railRect.left() || x > railRect.right()) continue;
    const bool major = (f % majorStepFrames) == 0;
    const int tickH = major ? 6 : 3;
    p.drawLine(x, railRect.top() - tickH, x, railRect.top() - 1);
   }
  }

  // ── ラベル描画 ──────────────────────
  const int frame = impl_->currentFrame_.framePosition();
  const int fps = impl_->fps_ > 0 ? impl_->fps_ : 30;
  const int totalSeconds = frame / fps;
  const int ff = frame % fps;
  const int ss = totalSeconds % 60;
  const int mm = (totalSeconds / 60) % 60;
  const int hh = totalSeconds / 3600;
  const QString leftLabel = QStringLiteral("RAM Cache");
  const QString rightLabel = QString("%1:%2:%3:%4")
   .arg(hh, 2, 10, QChar('0')).arg(mm, 2, 10, QChar('0'))
   .arg(ss, 2, 10, QChar('0')).arg(ff, 2, 10, QChar('0'));

  p.setPen(theme.text);
  p.drawText(QRect(10, 0, 88, h), Qt::AlignVCenter | Qt::AlignLeft, leftLabel);
  p.setPen(theme.text.darker(130));
  p.drawText(QRect(w - 128, 0, 118, h), Qt::AlignVCenter | Qt::AlignRight, rightLabel);
 }

 void ArtifactTimelineScrubBar::mousePressEvent(QMouseEvent* event)
 {
  if (!impl_->interactiveSeekingEnabled_) {
   event->ignore();
   return;
  }
  if (event->button() == Qt::LeftButton) {
   if (impl_->isPlaying_ && impl_->seekLockDuringPlayback_) {
    event->ignore();
    return;
   }

   int newFrame = impl_->resolveXToFrame(event->pos().x(), width());
   newFrame = qBound(0, newFrame, impl_->totalFrames_ - 1);
   if (impl_->currentFrame_.framePosition() != newFrame) {
    impl_->currentFrame_ = FramePosition(newFrame);
    update();
    Q_EMIT frameChanged(impl_->currentFrame_);
   }

   const int currentX = impl_->resolveFrameToX(impl_->currentFrame_.framePosition(), width());
   const int handleHalfWidth = impl_->handleWidth_ / 2;
   const bool onHandle = qAbs(event->pos().x() - currentX) <= handleHalfWidth + 3 &&
    event->pos().y() >= 0 && event->pos().y() <= impl_->handleHeight_ + 4;
   if (onHandle) {
    impl_->dragging_ = true;
    Q_EMIT frameDragStarted();
   } else {
    impl_->dragging_ = false;
   }
   event->accept();
  }
 }

 void ArtifactTimelineScrubBar::mouseMoveEvent(QMouseEvent* event)
 {
  if (!impl_->interactiveSeekingEnabled_) {
   event->ignore();
   return;
  }
  if (impl_->dragging_) {
   int newFrame = impl_->resolveXToFrame(event->pos().x(), width());
   newFrame = qBound(0, newFrame, impl_->totalFrames_ - 1);

   if (impl_->currentFrame_.framePosition() != newFrame) {
    impl_->currentFrame_ = FramePosition(newFrame);
    update();
    Q_EMIT frameChanged(impl_->currentFrame_);
   }
   event->accept();
  } else {
   const int currentX = impl_->resolveFrameToX(impl_->currentFrame_.framePosition(), width());
   const int handleHalfWidth = impl_->handleWidth_ / 2;

   if (qAbs(event->pos().x() - currentX) <= handleHalfWidth + 3 &&
    event->pos().y() >= 0 && event->pos().y() <= impl_->handleHeight_ + 4) {
    setCursor(Qt::PointingHandCursor);
    impl_->hover_ = true;
   } else {
    setCursor(Qt::ArrowCursor);
    impl_->hover_ = false;
   }
   update();
  }
 }

 void ArtifactTimelineScrubBar::mouseReleaseEvent(QMouseEvent* event)
 {
  if (!impl_->interactiveSeekingEnabled_) {
   event->ignore();
   return;
  }
  if (event->button() == Qt::LeftButton && impl_->dragging_) {
   impl_->dragging_ = false;
   impl_->hover_ = false;
   Q_EMIT frameDragFinished();
   event->accept();
   update();
  }
 }
}
