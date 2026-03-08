module;
#include <wobjectimpl.h>
#include <QRect>
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QDebug>
#include <QPaintEvent>
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
  int handleWidth_ = 12;        // nh
  int handleHeight_ = 20;       // nh
  bool seekLockDuringPlayback_ = true; // Đ̃V[NbN
  bool isPlaying_ = false;      // Đǂ
  bool hover_ = false;

  // t[ʒuXWvZ
  int frameToX(int frame) const
  {
   if (totalFrames_ <= 1) return 0;
   return static_cast<int>((static_cast<double>(frame) / (totalFrames_ - 1)) * (widgetWidth_ - 1));
  }

  // XWt[ʒuvZ
  int xToFrame(int x) const
  {
   if (widgetWidth_ <= 1) return 0;
   double ratio = static_cast<double>(x) / (widgetWidth_ - 1);
   return static_cast<int>(ratio * (totalFrames_ - 1) + 0.5);
  }

  int widgetWidth_ = 0;
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
  resize(6, 800);

  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAttribute(Qt::WA_TranslucentBackground);

  setMouseTracking(true);  // }EXǐՂL
 }

 ArtifactSeekBar::~ArtifactSeekBar()
 {
  delete impl_;
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
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    impl_->widgetWidth_ = width();
    p.fillRect(rect(), QColor(24, 24, 24));

    int currentX = impl_->frameToX(impl_->currentFrame_.framePosition());

    QColor playheadColor(86, 156, 214);
    QColor railColor(62, 62, 62);

    // Rail
    p.setPen(QPen(railColor, 2));
    p.drawLine(0, height() / 2, width(), height() / 2);

    // Ticks
    if (impl_->totalFrames_ > 1) {
      const int majorTicks = 10;
      p.setPen(QPen(QColor(90, 90, 90), 1));
      for (int i = 0; i <= majorTicks; ++i) {
        const int x = static_cast<int>((static_cast<double>(i) / majorTicks) * (width() - 1));
        const int tickH = (i % 5 == 0) ? 8 : 5;
        p.drawLine(x, height() / 2 - tickH, x, height() / 2 + tickH);
      }
    }

    p.setPen(QPen(playheadColor, 1));
    p.drawLine(currentX, 0, currentX, height());

    int halfW = impl_->handleWidth_ / 2;
    int h = impl_->handleHeight_;
    int pointOffset = 6;

    QPolygon handlePolygon;
    handlePolygon << QPoint(currentX - halfW, 0)
                  << QPoint(currentX + halfW, 0)
                  << QPoint(currentX + halfW, h - pointOffset)
                  << QPoint(currentX, h)
                  << QPoint(currentX - halfW, h - pointOffset);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 100));
    QPolygon shadowPolygon = handlePolygon;
    shadowPolygon.translate(1, 1);
    p.drawPolygon(shadowPolygon);

    p.setBrush(impl_->hover_ || impl_->dragging_ ? playheadColor.lighter(120) : playheadColor);
    p.setPen(QPen(playheadColor.darker(120), 1));
    p.drawPolygon(handlePolygon);
    
    p.setPen(QPen(playheadColor.lighter(130), 1));
    p.drawLine(currentX - halfW + 1, 1, currentX + halfW - 1, 1);

    // Current frame label
    const QString label = QString("F%1").arg(impl_->currentFrame_.framePosition());
    const QRect textRect(6, 2, 90, height() - 4);
    p.setPen(QColor(210, 210, 210));
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, label);
   }

   void ArtifactSeekBar::mousePressEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::LeftButton) {
   // ĐŃV[NbNLȏꍇ͑𖳎
   if (impl_->isPlaying_ && impl_->seekLockDuringPlayback_) {
    event->ignore();
    return;
   }
   
   // Click anywhere to seek, then start drag immediately.
   int newFrame = impl_->xToFrame(event->pos().x());
   newFrame = qBound(0, newFrame, impl_->totalFrames_ - 1);
   if (impl_->currentFrame_.framePosition() != newFrame) {
    impl_->currentFrame_ = FramePosition(newFrame);
    update();
    Q_EMIT frameChanged(impl_->currentFrame_);
   }
   impl_->dragging_ = true;
   Q_EMIT frameDragStarted();
   event->accept();
  }
 }

 void ArtifactSeekBar::mouseMoveEvent(QMouseEvent* event)
 {
  if (impl_->dragging_) {
   // hbO̓t[ʒuXV
   int newFrame = impl_->xToFrame(event->pos().x());
   newFrame = qBound(0, newFrame, impl_->totalFrames_ - 1);

   if (impl_->currentFrame_.framePosition() != newFrame) {
    impl_->currentFrame_ = FramePosition(newFrame);
    update();
    Q_EMIT frameChanged(impl_->currentFrame_);
   }
   event->accept();
  } else {
   // hover cursor state
   int currentX = impl_->frameToX(impl_->currentFrame_.framePosition());
   int handleHalfWidth = impl_->handleWidth_ / 2;

   if (qAbs(event->pos().x() - currentX) <= handleHalfWidth + 5) {
    setCursor(Qt::PointingHandCursor);
    impl_->hover_ = true;
   } else {
    setCursor(Qt::SizeHorCursor);
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
