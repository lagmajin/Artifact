module;
#include <wobjectimpl.h>
#include <QRect>
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QDebug>
module Artifact.Widgets.SeekBar;

import std;
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

  FramePosition currentFrame_;  // 現在のフレーム位置
  int totalFrames_ = 100;       // 総フレーム数
  bool dragging_ = false;       // ドラッグ中か
  int handleWidth_ = 12;        // ハンドル幅
  int handleHeight_ = 20;       // ハンドル高さ
  bool seekLockDuringPlayback_ = true; // 再生中のシークをロック
  bool isPlaying_ = false;      // 再生中かどうか

  // フレーム位置からX座標を計算
  int frameToX(int frame) const
  {
   if (totalFrames_ <= 1) return 0;
   return static_cast<int>((static_cast<double>(frame) / (totalFrames_ - 1)) * (widgetWidth_ - 1));
  }

  // X座標からフレーム位置を計算
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

  setMouseTracking(true);  // マウス追跡を有効化
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
  // 範囲を制限
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
   // 現在フレームが範囲外になった場合は調整
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

  // 背景を透明に
  p.fillRect(rect(), Qt::transparent);

  // 現在フレーム位置を計算
  int currentX = impl_->frameToX(impl_->currentFrame_.framePosition());

  // 再生ヘッドの線を描画（垂直ライン）
  p.setPen(QPen(QColor(255, 50, 50), 2));
  p.drawLine(currentX, 0, currentX, height());

  // ハンドルを描画（上部）
  int handleHalfWidth = impl_->handleWidth_ / 2;
  QRect handleRect(currentX - handleHalfWidth, 0, impl_->handleWidth_, impl_->handleHeight_);

  // ハンドルの背景（三角形風）
  QPolygon handlePolygon;
  handlePolygon << QPoint(currentX, 0)
    << QPoint(currentX - handleHalfWidth, impl_->handleHeight_)
    << QPoint(currentX + handleHalfWidth, impl_->handleHeight_);

  p.setBrush(QColor(255, 50, 50));
  p.setPen(Qt::NoPen);
  p.drawPolygon(handlePolygon);

  // ハンドルの枠線
  p.setPen(QPen(QColor(200, 200, 200), 1));
  p.drawPolygon(handlePolygon);
 }

 void ArtifactSeekBar::mousePressEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::LeftButton) {
   // 再生中でシークロックが有効な場合は操作を無視
   if (impl_->isPlaying_ && impl_->seekLockDuringPlayback_) {
    event->ignore();
    return;
   }
   
   // ハンドル領域または線上でクリックされた場合
   int currentX = impl_->frameToX(impl_->currentFrame_.framePosition());
   int handleHalfWidth = impl_->handleWidth_ / 2;

   // クリック位置がハンドルまたは線の近くかチェック
   if (qAbs(event->pos().x() - currentX) <= handleHalfWidth + 5) {
    impl_->dragging_ = true;
    Q_EMIT frameDragStarted();
    event->accept();
   }
  }
 }

 void ArtifactSeekBar::mouseMoveEvent(QMouseEvent* event)
 {
  if (impl_->dragging_) {
   // ドラッグ中はフレーム位置を更新
   int newFrame = impl_->xToFrame(event->pos().x());
   newFrame = qBound(0, newFrame, impl_->totalFrames_ - 1);

   if (impl_->currentFrame_.framePosition() != newFrame) {
    impl_->currentFrame_ = FramePosition(newFrame);
    update();
    Q_EMIT frameChanged(impl_->currentFrame_);
   }
   event->accept();
  } else {
   // ホバー時のカーソル変更
   int currentX = impl_->frameToX(impl_->currentFrame_.framePosition());
   int handleHalfWidth = impl_->handleWidth_ / 2;

   if (qAbs(event->pos().x() - currentX) <= handleHalfWidth + 5) {
    setCursor(Qt::PointingHandCursor);
   } else {
    setCursor(Qt::ArrowCursor);
   }
  }
 }

 void ArtifactSeekBar::mouseReleaseEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::LeftButton && impl_->dragging_) {
   impl_->dragging_ = false;
   Q_EMIT frameDragFinished();
   event->accept();
  }
 }

};
