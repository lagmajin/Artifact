module;
#include <wobjectimpl.h>
#include <QRect>
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QDebug>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QLinearGradient>
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
module Artifact.Widgets.SeekBar;




import Frame.Position;

namespace Artifact
{
 W_OBJECT_IMPL(ArtifactSeekBar)

 class ArtifactSeekBar::Impl
 {
 private:
 public:
  Impl();
  ~Impl();

  FramePosition currentFrame_;  // ݂̃t[ʒu
  int totalFrames_ = 100;       // t[
  bool dragging_ = false;       // hbO
  int handleWidth_ = 10;        // nh
  int handleHeight_ = 22;       // nh
  bool seekLockDuringPlayback_ = true; // Đ̃V[NbN
  bool isPlaying_ = false;      // Đǂ
  bool hover_ = false;

  // t[ʒuXWvZ
  int frameToX(int frame, int width) const
  {
   if (totalFrames_ <= 1 || width <= 1) return 0;
   return static_cast<int>((static_cast<double>(frame) / (totalFrames_ - 1)) * (width - 1));
  }

  // XWt[ʒuvZ
  int xToFrame(int x, int width) const
  {
   if (width <= 1) return 0;
   double ratio = static_cast<double>(x) / (width - 1);
   return static_cast<int>(ratio * (totalFrames_ - 1) + 0.5);
  }
 };

 ArtifactSeekBar::Impl::Impl()
  : currentFrame_(0)
 {
 }

 ArtifactSeekBar::Impl::~Impl()
 {
 }

 ArtifactSeekBar::ArtifactSeekBar(QWidget* parent)
  : QWidget(parent), impl_(new Impl)
 {
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAttribute(Qt::WA_TranslucentBackground);

  setMouseTracking(true);  // }EXǐՂL
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
 }

 ArtifactSeekBar::~ArtifactSeekBar()
 {
  delete impl_;
 }

 QSize ArtifactSeekBar::sizeHint() const
 {
  return QSize(640, 20);
 }

 QSize ArtifactSeekBar::minimumSizeHint() const
 {
  return QSize(120, 20);
 }

 FramePosition ArtifactSeekBar::currentFrame() const
 {
  return impl_->currentFrame_;
 }

 void ArtifactSeekBar::setCurrentFrame(const FramePosition& frame)
 {
  int frameValue = frame.framePosition();
  // ͈͂𐧌
  frameValue = qBound(0, frameValue, impl_->totalFrames_ - 1);

  if (impl_->currentFrame_.framePosition() != frameValue) {
   impl_->currentFrame_ = FramePosition(frameValue);
   update();
   Q_EMIT frameChanged(impl_->currentFrame_);
  }
 }

 void ArtifactSeekBar::setTotalFrames(int totalFrames)
 {
  if (totalFrames > 0 && impl_->totalFrames_ != totalFrames) {
   impl_->totalFrames_ = totalFrames;
   // ݃t[͈͊OɂȂꍇ͒
   if (impl_->currentFrame_.framePosition() >= totalFrames) {
    setCurrentFrame(FramePosition(totalFrames - 1));
   }
   update();
  }
 }

 int ArtifactSeekBar::totalFrames() const
 {
  return impl_->totalFrames_;
 }

 void ArtifactSeekBar::setSeekLockDuringPlayback(bool lock)
 {
  impl_->seekLockDuringPlayback_ = lock;
 }

 bool ArtifactSeekBar::isSeekLockedDuringPlayback() const
 {
  return impl_->seekLockDuringPlayback_;
 }

 void ArtifactSeekBar::setIsPlaying(bool playing)
 {
  impl_->isPlaying_ = playing;
 }

 bool ArtifactSeekBar::isPlaying() const
 {
  return impl_->isPlaying_;
 }
  void ArtifactSeekBar::paintEvent(QPaintEvent* event)
   {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect();
    const int w = r.width();
    const int h = r.height();
    const int currentX = impl_->frameToX(impl_->currentFrame_.framePosition(), w);
    const int centerY = h / 2;
    const int railHalfH = std::max(3, h / 7);
    const QRect railRect(8, centerY - railHalfH, std::max(10, w - 16), railHalfH * 2);

    const QColor bgTop(32, 32, 36);
    const QColor bgBottom(24, 24, 27);
    const QColor railColor(62, 62, 68);
    const QColor railBorder(78, 78, 86);
    const QColor accent(104, 171, 234);
    const QColor accentStrong = impl_->dragging_ ? accent.lighter(120) : (impl_->hover_ ? accent.lighter(110) : accent);

    QLinearGradient bgGrad(r.topLeft(), r.bottomLeft());
    bgGrad.setColorAt(0.0, bgTop);
    bgGrad.setColorAt(1.0, bgBottom);
    p.fillRect(r, bgGrad);

    p.setPen(QColor(18, 18, 18, 180));
    p.drawLine(r.topLeft(), r.topRight());
    p.setPen(QColor(0, 0, 0, 160));
    p.drawLine(r.bottomLeft(), r.bottomRight());

    p.setPen(QPen(railBorder, 1));
    p.setBrush(railColor);
    p.drawRoundedRect(railRect, railHalfH, railHalfH);

    const int clampedX = std::clamp(currentX, railRect.left(), railRect.right());
    QRect activeRect = railRect;
    activeRect.setRight(std::max(activeRect.left(), clampedX));
    if (activeRect.width() > 1) {
      QLinearGradient activeGrad(activeRect.topLeft(), activeRect.bottomLeft());
      activeGrad.setColorAt(0.0, accentStrong.lighter(118));
      activeGrad.setColorAt(1.0, accentStrong.darker(120));
      p.setPen(Qt::NoPen);
      p.setBrush(activeGrad);
      p.drawRoundedRect(activeRect, railHalfH, railHalfH);
    }

    if (impl_->totalFrames_ > 1 && railRect.width() > 20) {
      const int approxMajorCount = std::clamp(railRect.width() / 90, 6, 16);
      const int majorStepFrames = std::max(1, (impl_->totalFrames_ - 1) / approxMajorCount);
      const int minorStepFrames = std::max(1, majorStepFrames / 5);

      p.setPen(QPen(QColor(95, 95, 105, 170), 1));
      for (int f = 0; f < impl_->totalFrames_; f += minorStepFrames) {
        const int x = impl_->frameToX(f, w);
        if (x < railRect.left() || x > railRect.right()) continue;
        const bool major = (f % majorStepFrames) == 0;
        const int tickH = major ? 6 : 3;
        p.drawLine(x, railRect.top() - tickH, x, railRect.top() - 1);
      }
    }

    p.setPen(QPen(accentStrong, 1));
    p.drawLine(clampedX, 0, clampedX, h);

    const int handleW = impl_->handleWidth_;
    const int handleH = std::min(std::max(16, h - 6), impl_->handleHeight_);
    const QRect handleRect(clampedX - handleW / 2, 2, handleW, handleH);
    p.setPen(QColor(18, 18, 18, 200));
    p.setBrush(QColor(220, 226, 235, impl_->dragging_ ? 255 : 235));
    p.drawRoundedRect(handleRect, 3, 3);
    p.setPen(QColor(120, 130, 145, 220));
    p.drawLine(handleRect.center().x(), handleRect.top() + 3, handleRect.center().x(), handleRect.bottom() - 3);

    const int frame = impl_->currentFrame_.framePosition();
    const int totalSeconds = frame / 30;
    const int ff = frame % 30;
    const int ss = totalSeconds % 60;
    const int mm = (totalSeconds / 60) % 60;
    const int hh = totalSeconds / 3600;
    const QString leftLabel = QString("F%1").arg(frame);
    const QString rightLabel = QString("%1:%2:%3:%4")
      .arg(hh, 2, 10, QChar('0'))
      .arg(mm, 2, 10, QChar('0'))
      .arg(ss, 2, 10, QChar('0'))
      .arg(ff, 2, 10, QChar('0'));

    p.setPen(QColor(225, 225, 230));
    p.drawText(QRect(10, 0, 88, h), Qt::AlignVCenter | Qt::AlignLeft, leftLabel);
    p.setPen(QColor(170, 170, 178));
    p.drawText(QRect(w - 128, 0, 118, h), Qt::AlignVCenter | Qt::AlignRight, rightLabel);
   }

   void ArtifactSeekBar::mousePressEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::LeftButton) {
   // ĐŃV[NbNLȏꍇ͑𖳎
   if (impl_->isPlaying_ && impl_->seekLockDuringPlayback_) {
    event->ignore();
    return;
   }
   
   // Click anywhere to seek.
   int newFrame = impl_->xToFrame(event->pos().x(), width());
   newFrame = qBound(0, newFrame, impl_->totalFrames_ - 1);
   if (impl_->currentFrame_.framePosition() != newFrame) {
    impl_->currentFrame_ = FramePosition(newFrame);
    update();
    Q_EMIT frameChanged(impl_->currentFrame_);
   }

   // Start drag only when pressing near the handle.
   const int currentX = impl_->frameToX(impl_->currentFrame_.framePosition(), width());
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

 void ArtifactSeekBar::mouseMoveEvent(QMouseEvent* event)
 {
  if (impl_->dragging_) {
   // hbO̓t[ʒuXV
   int newFrame = impl_->xToFrame(event->pos().x(), width());
   newFrame = qBound(0, newFrame, impl_->totalFrames_ - 1);

   if (impl_->currentFrame_.framePosition() != newFrame) {
    impl_->currentFrame_ = FramePosition(newFrame);
    update();
    Q_EMIT frameChanged(impl_->currentFrame_);
   }
   event->accept();
  } else {
   // hover cursor state
   int currentX = impl_->frameToX(impl_->currentFrame_.framePosition(), width());
   int handleHalfWidth = impl_->handleWidth_ / 2;

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

 void ArtifactSeekBar::mouseReleaseEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::LeftButton && impl_->dragging_) {
   impl_->dragging_ = false;
   impl_->hover_ = false;
   Q_EMIT frameDragFinished();
   event->accept();
   update();
  }
 }

};
