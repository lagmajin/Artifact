module;
#include <QFontMetrics>
#include <QPainter>
#include <QPen>
#include <QRectF>
#include <QSize>
#include <QtGlobal>
#include <QCursor>
#include <wobjectimpl.h>

module Artifact.Timeline.TrackPainterView;

import std;

namespace Artifact
{
W_OBJECT_IMPL(ArtifactTimelineTrackPainterView)

namespace
{
constexpr int kDefaultTrackHeight = 30;
constexpr int kTrackSpacing = 1;
constexpr int kClipCorner = 4;
constexpr int kClipPadding = 6;
constexpr int kMinTrackCount = 1;

double clampDurationFrames(const double value)
{
 return std::max(1.0, value);
}

double clampPixelsPerFrame(const double value)
{
 return std::clamp(value, 0.05, 64.0);
}

int trackTopAt(const QVector<int>& heights, const int trackIndex)
{
 int y = 0;
 for (int i = 0; i < trackIndex && i < heights.size(); ++i) {
  y += heights[i] + kTrackSpacing;
 }
 return y;
}

constexpr int kEdgeHitZone = 6;

enum class DragMode { None, MoveBody, ResizeLeft, ResizeRight };

struct HitResult {
 DragMode mode = DragMode::None;
 int clipIndex = -1;
};

HitResult hitTestClips(
 const QVector<ArtifactTimelineTrackPainterView::TrackClipVisual>& clips,
 const QVector<int>& heights,
 const double mouseX, const double mouseY,
 const double ppf, const double xOffset)
{
 for (int i = 0; i < clips.size(); ++i) {
  const auto& clip = clips[i];
  if (clip.trackIndex < 0 || clip.trackIndex >= heights.size()) continue;
  const int trackTop = trackTopAt(heights, clip.trackIndex);
  const int trackH = heights[clip.trackIndex];
  if (mouseY < trackTop || mouseY > trackTop + trackH) continue;
  const double clipX = clip.startFrame * ppf - xOffset;
  const double clipW = std::max(2.0, clip.durationFrame * ppf);
  if (mouseX >= clipX - kEdgeHitZone && mouseX <= clipX + kEdgeHitZone)
   return {DragMode::ResizeLeft, i};
  if (mouseX >= clipX + clipW - kEdgeHitZone && mouseX <= clipX + clipW + kEdgeHitZone)
   return {DragMode::ResizeRight, i};
  if (mouseX > clipX && mouseX < clipX + clipW)
   return {DragMode::MoveBody, i};
 }
 return {};
}
} // namespace

class ArtifactTimelineTrackPainterView::Impl
{
public:
 Impl();
 ~Impl();

 double durationFrames_ = 300.0;
 double currentFrame_ = 0.0;
 double pixelsPerFrame_ = 2.0;
 double horizontalOffset_ = 0.0;
 QVector<int> trackHeights_;
 QVector<TrackClipVisual> clips_;

 // ドラッグ / ホバー状態
 DragMode dragMode_ = DragMode::None;
 int dragClipIndex_ = -1;
 double dragStartX_ = 0.0;
 double dragOrigStartFrame_ = 0.0;
 double dragOrigDuration_ = 0.0;
 int hoverClipIndex_ = -1;
 DragMode hoverEdge_ = DragMode::None;
};

ArtifactTimelineTrackPainterView::Impl::Impl()
{
 trackHeights_.resize(kMinTrackCount);
 trackHeights_.fill(kDefaultTrackHeight);
}

ArtifactTimelineTrackPainterView::Impl::~Impl() = default;

ArtifactTimelineTrackPainterView::ArtifactTimelineTrackPainterView(QWidget* parent)
 : QWidget(parent), impl_(new Impl())
{
 setMouseTracking(true);
 setAutoFillBackground(false);
 setFocusPolicy(Qt::StrongFocus);
}

ArtifactTimelineTrackPainterView::~ArtifactTimelineTrackPainterView()
{
 delete impl_;
}

void ArtifactTimelineTrackPainterView::setDurationFrames(const double frames)
{
 const double sanitized = clampDurationFrames(frames);
 if (std::abs(impl_->durationFrames_ - sanitized) < 0.0001) {
  return;
 }
 impl_->durationFrames_ = sanitized;
 update();
}

double ArtifactTimelineTrackPainterView::durationFrames() const
{
 return impl_->durationFrames_;
}

void ArtifactTimelineTrackPainterView::setCurrentFrame(const double frame)
{
 const double sanitized = std::clamp(frame, 0.0, impl_->durationFrames_);
 if (std::abs(impl_->currentFrame_ - sanitized) < 0.0001) {
  return;
 }
 impl_->currentFrame_ = sanitized;
 update();
}

double ArtifactTimelineTrackPainterView::currentFrame() const
{
 return impl_->currentFrame_;
}

void ArtifactTimelineTrackPainterView::setPixelsPerFrame(const double value)
{
 const double sanitized = clampPixelsPerFrame(value);
 if (std::abs(impl_->pixelsPerFrame_ - sanitized) < 0.0001) {
  return;
 }
 impl_->pixelsPerFrame_ = sanitized;
 update();
}

double ArtifactTimelineTrackPainterView::pixelsPerFrame() const
{
 return impl_->pixelsPerFrame_;
}

void ArtifactTimelineTrackPainterView::setHorizontalOffset(const double value)
{
 if (std::abs(impl_->horizontalOffset_ - value) < 0.0001) {
  return;
 }
 impl_->horizontalOffset_ = value;
 update();
}

double ArtifactTimelineTrackPainterView::horizontalOffset() const
{
 return impl_->horizontalOffset_;
}

void ArtifactTimelineTrackPainterView::setTrackCount(const int count)
{
 const int sanitized = std::max(kMinTrackCount, count);
 if (impl_->trackHeights_.size() == sanitized) {
  return;
 }
 const int oldSize = impl_->trackHeights_.size();
 impl_->trackHeights_.resize(sanitized);
 for (int i = oldSize; i < sanitized; ++i) {
  impl_->trackHeights_[i] = kDefaultTrackHeight;
 }
 updateGeometry();
 update();
}

int ArtifactTimelineTrackPainterView::trackCount() const
{
 return impl_->trackHeights_.size();
}

void ArtifactTimelineTrackPainterView::setTrackHeight(const int trackIndex, const int height)
{
 if (trackIndex < 0 || trackIndex >= impl_->trackHeights_.size()) {
  return;
 }
 const int sanitized = std::max(16, height);
 if (impl_->trackHeights_[trackIndex] == sanitized) {
  return;
 }
 impl_->trackHeights_[trackIndex] = sanitized;
 updateGeometry();
 update();
}

int ArtifactTimelineTrackPainterView::trackHeight(const int trackIndex) const
{
 if (trackIndex < 0 || trackIndex >= impl_->trackHeights_.size()) {
  return kDefaultTrackHeight;
 }
 return impl_->trackHeights_[trackIndex];
}

void ArtifactTimelineTrackPainterView::clearClips()
{
 if (impl_->clips_.isEmpty()) {
  return;
 }
 impl_->clips_.clear();
 update();
}

void ArtifactTimelineTrackPainterView::setClips(const QVector<TrackClipVisual>& clips)
{
 impl_->clips_ = clips;
 update();
}

QVector<ArtifactTimelineTrackPainterView::TrackClipVisual> ArtifactTimelineTrackPainterView::clips() const
{
 return impl_->clips_;
}

QSize ArtifactTimelineTrackPainterView::minimumSizeHint() const
{
 int h = 0;
 for (int i = 0; i < impl_->trackHeights_.size(); ++i) {
  h += impl_->trackHeights_[i];
  if (i + 1 < impl_->trackHeights_.size()) {
   h += kTrackSpacing;
  }
 }
 return QSize(320, std::max(120, h));
}

void ArtifactTimelineTrackPainterView::paintEvent(QPaintEvent* event)
{
 Q_UNUSED(event);
 QPainter p(this);
 p.setRenderHint(QPainter::Antialiasing, true);
 p.fillRect(rect(), QColor(28, 29, 33));

 const QRect fullRect = rect();
 const double ppf = impl_->pixelsPerFrame_;
 const double xOffset = impl_->horizontalOffset_;

 // Track rows.
 int y = 0;
 for (int i = 0; i < impl_->trackHeights_.size(); ++i) {
  const int rowH = impl_->trackHeights_[i];
  const QColor rowColor = (i % 2 == 0) ? QColor(33, 34, 39) : QColor(37, 38, 43);
  p.fillRect(QRect(0, y, fullRect.width(), rowH), rowColor);
  p.setPen(QPen(QColor(18, 18, 20), 1));
  p.drawLine(0, y + rowH, fullRect.width(), y + rowH);
  y += rowH + kTrackSpacing;
 }

 // Vertical frame grid (tick lines only, no labels).
 const int majorStep = 10;
 const int minorStep = 5;
 const int startFrame = std::max(0, static_cast<int>(std::floor(xOffset / ppf)));
 const int endFrame = static_cast<int>(std::ceil((xOffset + fullRect.width()) / ppf));
 for (int f = startFrame; f <= endFrame; ++f) {
  const double x = f * ppf - xOffset;
  if (x < 0.0 || x > fullRect.width()) {
   continue;
  }
  const bool major = (f % majorStep) == 0;
  const bool minor = !major && (f % minorStep) == 0;
  if (!major && !minor) {
   continue;
  }
  p.setPen(QPen(major ? QColor(78, 79, 88) : QColor(54, 55, 62), 1));
  p.drawLine(QPointF(x, 0.0), QPointF(x, fullRect.height()));
 }

 // Clips.
 for (int i = 0; i < impl_->clips_.size(); ++i) {
  const auto& clip = impl_->clips_[i];
  if (clip.trackIndex < 0 || clip.trackIndex >= impl_->trackHeights_.size()) {
   continue;
  }
  const int trackTop = trackTopAt(impl_->trackHeights_, clip.trackIndex);
  const int trackH = impl_->trackHeights_[clip.trackIndex];
  const double x = clip.startFrame * ppf - xOffset;
  const double w = std::max(2.0, clip.durationFrame * ppf);
  QRectF clipRect(x, trackTop + 2.0, w, std::max(8, trackH - 4));
  if (!clipRect.intersects(QRectF(fullRect))) {
   continue;
  }

  const bool isHovered = (i == impl_->hoverClipIndex_);
  const QColor fill = clip.selected
   ? clip.fillColor.lighter(130)
   : (isHovered ? clip.fillColor.lighter(115) : clip.fillColor);
  p.setPen(QPen(clip.selected ? QColor(230, 235, 255) : QColor(17, 17, 20), clip.selected ? 2 : 1));
  p.setBrush(fill);
  p.drawRoundedRect(clipRect, kClipCorner, kClipCorner);

  if (!clip.title.isEmpty() && clipRect.width() > 28.0) {
   p.setPen(QColor(235, 239, 247));
   const QString text = QFontMetrics(p.font()).elidedText(clip.title, Qt::ElideRight, static_cast<int>(clipRect.width()) - (kClipPadding * 2));
   p.drawText(clipRect.adjusted(kClipPadding, 0, -kClipPadding, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
  }

  // リサイズグリップ (ホバー時 or 選択時にエッジに縦線を描画)
  if ((isHovered || clip.selected) && clipRect.width() > 16.0) {
   const qreal gripY1 = clipRect.center().y() - 5.0;
   const qreal gripY2 = clipRect.center().y() + 5.0;
   p.setPen(QPen(QColor(255, 255, 255, isHovered ? 180 : 100), 2.0, Qt::SolidLine, Qt::RoundCap));
   p.drawLine(QPointF(clipRect.left()  + 4.0, gripY1), QPointF(clipRect.left()  + 4.0, gripY2));
   p.drawLine(QPointF(clipRect.right() - 4.0, gripY1), QPointF(clipRect.right() - 4.0, gripY2));
  }
 }

 }

void ArtifactTimelineTrackPainterView::mousePressEvent(QMouseEvent* event)
{
 if (event->button() == Qt::LeftButton) {
  const double mouseX = event->position().x();
  const double mouseY = event->position().y();
  const auto hit = hitTestClips(impl_->clips_, impl_->trackHeights_,
                                 mouseX, mouseY,
                                 impl_->pixelsPerFrame_, impl_->horizontalOffset_);
  if (hit.mode != DragMode::None) {
   impl_->dragMode_           = hit.mode;
   impl_->dragClipIndex_      = hit.clipIndex;
   impl_->dragStartX_         = mouseX;
   impl_->dragOrigStartFrame_ = impl_->clips_[hit.clipIndex].startFrame;
   impl_->dragOrigDuration_   = impl_->clips_[hit.clipIndex].durationFrame;
   if (hit.mode == DragMode::MoveBody) setCursor(Qt::ClosedHandCursor);
   event->accept();
   return;
  }
  const double clickedFrame = (mouseX + impl_->horizontalOffset_) / std::max(0.001, impl_->pixelsPerFrame_);
  const double clamped = std::clamp(clickedFrame, 0.0, impl_->durationFrames_);
  seekRequested(clamped);
  setCurrentFrame(clamped);
  event->accept();
  return;
 }
 QWidget::mousePressEvent(event);
}

void ArtifactTimelineTrackPainterView::mouseMoveEvent(QMouseEvent* event)
{
 const double mouseX = event->position().x();
 const double mouseY = event->position().y();
 const double ppf    = impl_->pixelsPerFrame_;

 if (impl_->dragMode_ != DragMode::None && impl_->dragClipIndex_ >= 0) {
  const double deltaFrames = (mouseX - impl_->dragStartX_) / std::max(0.001, ppf);
  auto& clip = impl_->clips_[impl_->dragClipIndex_];
  switch (impl_->dragMode_) {
  case DragMode::MoveBody:
   clip.startFrame = std::max(0.0, impl_->dragOrigStartFrame_ + deltaFrames);
   break;
  case DragMode::ResizeLeft: {
   const double end      = impl_->dragOrigStartFrame_ + impl_->dragOrigDuration_;
   clip.startFrame       = std::clamp(impl_->dragOrigStartFrame_ + deltaFrames, 0.0, end - 1.0);
   clip.durationFrame    = end - clip.startFrame;
   break;
  }
  case DragMode::ResizeRight:
   clip.durationFrame = std::max(1.0, impl_->dragOrigDuration_ + deltaFrames);
   break;
  default: break;
  }
  update();
  event->accept();
  return;
 }

 const auto hit = hitTestClips(impl_->clips_, impl_->trackHeights_,
                                mouseX, mouseY, ppf, impl_->horizontalOffset_);
 const bool changed = (hit.clipIndex != impl_->hoverClipIndex_ || hit.mode != impl_->hoverEdge_);
 impl_->hoverClipIndex_ = hit.clipIndex;
 impl_->hoverEdge_      = hit.mode;

 switch (hit.mode) {
 case DragMode::ResizeLeft:
 case DragMode::ResizeRight: setCursor(Qt::SizeHorCursor);  break;
 case DragMode::MoveBody:    setCursor(Qt::OpenHandCursor); break;
 default:                    setCursor(Qt::ArrowCursor);    break;
 }

 if (changed) update();
 QWidget::mouseMoveEvent(event);
}

void ArtifactTimelineTrackPainterView::mouseReleaseEvent(QMouseEvent* event)
{
 if (event->button() == Qt::LeftButton && impl_->dragMode_ != DragMode::None) {
  const int idx = impl_->dragClipIndex_;
  if (idx >= 0 && idx < impl_->clips_.size()) {
   const auto& clip = impl_->clips_[idx];
   if (impl_->dragMode_ == DragMode::MoveBody)
    clipMoved(clip.clipId, clip.startFrame);
   else
    clipResized(clip.clipId, clip.startFrame, clip.durationFrame);
  }
  impl_->dragMode_      = DragMode::None;
  impl_->dragClipIndex_ = -1;
  switch (impl_->hoverEdge_) {
  case DragMode::ResizeLeft:
  case DragMode::ResizeRight: setCursor(Qt::SizeHorCursor);  break;
  case DragMode::MoveBody:    setCursor(Qt::OpenHandCursor); break;
  default:                    setCursor(Qt::ArrowCursor);    break;
  }
  event->accept();
  return;
 }
 QWidget::mouseReleaseEvent(event);
}

} // namespace Artifact
