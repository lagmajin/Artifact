module;
#include <QFontMetrics>
#include <QContextMenuEvent>
#include <QPainter>
#include <QPen>
#include <QRectF>
#include <QPolygonF>
#include <QSize>
#include <QtGlobal>
#include <QCursor>
#include <QMenu>
#include <wobjectimpl.h>

module Artifact.Timeline.TrackPainterView;

import std;
import Widgets.Utils.CSS;
import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layers.Selection.Manager;
import Artifact.Service.Project;
import Frame.Position;
import Property.Abstract;
import Time.Rational;

namespace Artifact
{
W_OBJECT_IMPL(ArtifactTimelineTrackPainterView)

namespace
{
struct TimelineThemeColors
{
 QColor background;
 QColor surface;
 QColor border;
 QColor accent;
 QColor text;
};

TimelineThemeColors timelineThemeColors()
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

constexpr int kDefaultTrackHeight = 30;
constexpr int kTrackSpacing = 1;
constexpr int kClipCorner = 4;
constexpr int kClipPadding = 6;
constexpr int kMinTrackCount = 1;
constexpr double kMarkerLaneStep = 8.0;

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

struct MarkerHitResult {
 int markerIndex = -1;
};

HitResult hitTestClips(
 const QVector<ArtifactTimelineTrackPainterView::TrackClipVisual>& clips,
 const QVector<int>& heights,
 const double mouseX, const double mouseY,
 const double ppf, const double xOffset,
 const double yOffset)
{
 const double localMouseY = mouseY + yOffset;
 for (int i = 0; i < clips.size(); ++i) {
  const auto& clip = clips[i];
  if (clip.trackIndex < 0 || clip.trackIndex >= heights.size()) continue;
  const int trackTop = trackTopAt(heights, clip.trackIndex);
  const int trackH = heights[clip.trackIndex];
  if (localMouseY < trackTop || localMouseY > trackTop + trackH) continue;
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

QPointF markerCenterFor(
 const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual& marker,
 const QVector<int>& heights,
 const double ppf,
 const double xOffset,
 const double yOffset)
{
 if (marker.trackIndex < 0 || marker.trackIndex >= heights.size()) {
  return {};
 }
 const int trackTop = trackTopAt(heights, marker.trackIndex);
 const int trackH = heights[marker.trackIndex];
 const int laneCount = std::max(1, marker.laneCount);
 const int laneIndex = std::clamp(marker.laneIndex, 0, laneCount - 1);
 const double laneOffset = (laneIndex - (laneCount - 1) * 0.5) * kMarkerLaneStep;
 return QPointF(marker.frame * ppf - xOffset, trackTop + trackH * 0.5 - yOffset + laneOffset);
}

QRectF markerHitRectFor(
 const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual& marker,
 const QVector<int>& heights,
 const double ppf,
 const double xOffset,
 const double yOffset)
{
 const QPointF center = markerCenterFor(marker, heights, ppf, xOffset, yOffset);
  if (center.isNull()) {
  return {};
  }
 const qreal size = marker.laneCount > 1 ? 7.0 : 8.0;
 return QRectF(center.x() - size, center.y() - size, size * 2.0, size * 2.0);
}

QRectF clipRectFor(
 const ArtifactTimelineTrackPainterView::TrackClipVisual& clip,
 const QVector<int>& heights,
 const double ppf,
 const double xOffset,
 const double yOffset)
{
 if (clip.trackIndex < 0 || clip.trackIndex >= heights.size()) {
  return {};
 }

 const int trackTop = trackTopAt(heights, clip.trackIndex);
 const int trackH = heights[clip.trackIndex];
 const double clipX = clip.startFrame * ppf - xOffset;
 const double clipW = std::max(2.0, clip.durationFrame * ppf);
 return QRectF(clipX, trackTop + 2.0 - yOffset, clipW, std::max(8, trackH - 4));
}

MarkerHitResult hitTestMarkers(
 const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>& markers,
 const QVector<int>& heights,
 const double mouseX, const double mouseY,
 const double ppf, const double xOffset,
 const double yOffset)
{
 for (int i = 0; i < markers.size(); ++i) {
  const auto& marker = markers[i];
  if (marker.trackIndex < 0 || marker.trackIndex >= heights.size()) {
   continue;
  }
  const QPointF center = markerCenterFor(marker, heights, ppf, xOffset, yOffset);
  if (center.isNull()) {
   continue;
  }
  const QRectF hitRect(center.x() - 8.0, center.y() - 8.0, 16.0, 16.0);
  if (hitRect.contains(QPointF(mouseX, mouseY))) {
   return {i};
  }
 }
 return {};
}

QVector<ArtifactAbstractLayerPtr> selectedTimelineLayers()
{
 QVector<ArtifactAbstractLayerPtr> layers;
 if (auto *app = ArtifactApplicationManager::instance()) {
  if (auto *selection = app->layerSelectionManager()) {
   const auto selected = selection->selectedLayers();
   layers.reserve(selected.size());
   for (const auto &layer : selected) {
    layers.push_back(layer);
   }
   if (layers.isEmpty()) {
    if (auto current = selection->currentLayer()) {
     layers.push_back(current);
    }
   }
  }
 }
 return layers;
}

bool applyKeyframeEditAtFrame(const ArtifactCompositionPtr &composition,
                              const ArtifactAbstractLayerPtr &layer,
                              const qint64 frame,
                              const bool removeKeyframes)
{
 if (!composition || !layer) {
  return false;
 }

 const double fps = std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
 const RationalTime nowTime(frame, static_cast<int64_t>(std::llround(fps)));

 bool changed = false;
 for (const auto &group : layer->getLayerPropertyGroups()) {
  for (const auto &property : group.sortedProperties()) {
   if (!property || !property->isAnimatable()) {
    continue;
   }
   if (removeKeyframes) {
    if (property->hasKeyFrameAt(nowTime)) {
     property->removeKeyFrame(nowTime);
     changed = true;
    }
   } else {
    const QVariant value = property->interpolateValue(nowTime);
    property->addKeyFrame(nowTime, value.isValid() ? value : property->getValue());
    changed = true;
   }
  }
 }

 if (changed) {
  layer->changed();
 }
 return changed;
}

bool applyTimelineLayerRangeEdit(const ArtifactAbstractLayerPtr &layer,
                                 const qint64 startFrame,
                                 const qint64 durationFrame,
                                 const bool preserveExistingDuration)
{
 if (!layer) {
  return false;
 }

 const qint64 oldInPoint = layer->inPoint().framePosition();
 const qint64 oldOutPoint = layer->outPoint().framePosition();
 const qint64 oldStartTime = layer->startTime().framePosition();
 const qint64 oldDuration = std::max<qint64>(1, oldOutPoint - oldInPoint);

 const qint64 inPoint = std::max<qint64>(0, startFrame);
 const qint64 outPoint = preserveExistingDuration
                             ? std::max<qint64>(inPoint + 1, inPoint + oldDuration)
                             : std::max<qint64>(inPoint + 1, startFrame + durationFrame);
 const qint64 inPointDelta = inPoint - oldInPoint;

 layer->setInPoint(FramePosition(inPoint));
 layer->setOutPoint(FramePosition(outPoint));

 if (!preserveExistingDuration && inPointDelta != 0) {
  layer->setStartTime(FramePosition(oldStartTime + inPointDelta));
 }

 return oldInPoint != inPoint || oldOutPoint != outPoint || oldStartTime != layer->startTime().framePosition();
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
 double verticalOffset_ = 0.0;
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
 int hoverMarkerIndex_ = -1;
 bool panning_ = false;
 QPoint lastPanPoint_;
 QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> keyframeMarkers_;
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
 setAttribute(Qt::WA_OpaquePaintEvent, true);
 setAttribute(Qt::WA_StaticContents, true);
 setAttribute(Qt::WA_NoSystemBackground, true);
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
 const double oldFrame = impl_->currentFrame_;
 impl_->currentFrame_ = sanitized;
 const double oldX = oldFrame * impl_->pixelsPerFrame_ - impl_->horizontalOffset_;
 const double newX = impl_->currentFrame_ * impl_->pixelsPerFrame_ - impl_->horizontalOffset_;
 const QRect dirtyRect = QRect(
     static_cast<int>(std::floor(std::min(oldX, newX))) - 4,
     0,
     static_cast<int>(std::ceil(std::abs(newX - oldX))) + 8,
     height());
 update(dirtyRect);
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

void ArtifactTimelineTrackPainterView::setVerticalOffset(const double value)
{
 if (std::abs(impl_->verticalOffset_ - value) < 0.0001) {
  return;
 }
 impl_->verticalOffset_ = std::max(0.0, value);
 update();
}

double ArtifactTimelineTrackPainterView::verticalOffset() const
{
 return impl_->verticalOffset_;
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

void ArtifactTimelineTrackPainterView::setKeyframeMarkers(
 const QVector<KeyframeMarkerVisual>& markers)
{
 impl_->keyframeMarkers_ = markers;
 update();
}

void ArtifactTimelineTrackPainterView::setSelectedLayerIds(const QSet<LayerID>& layerIds)
{
 QRectF dirtyRect;
 bool hasDirty = false;
 bool changed = false;
 for (auto& clip : impl_->clips_) {
  const bool selected = layerIds.contains(clip.layerId);
  if (clip.selected != selected) {
   const QRectF rect = clipRectFor(clip, impl_->trackHeights_, impl_->pixelsPerFrame_, impl_->horizontalOffset_, impl_->verticalOffset_);
   clip.selected = selected;
   if (rect.isValid()) {
    dirtyRect = hasDirty ? dirtyRect.united(rect) : rect;
    hasDirty = true;
   }
   changed = true;
  }
 }
 if (changed) {
  update((hasDirty ? dirtyRect : QRectF(rect())).adjusted(-2.0, -2.0, 2.0, 2.0).toAlignedRect());
 }
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
 QPainter p(this);
 p.setRenderHint(QPainter::Antialiasing, true);
 const TimelineThemeColors theme = timelineThemeColors();
 
  const QRect dirtyRect = event->rect();
 p.fillRect(dirtyRect, theme.background);

 const QRect fullRect = rect();
 const double ppf = impl_->pixelsPerFrame_;
 const double xOffset = impl_->horizontalOffset_;
 const double yOffset = impl_->verticalOffset_;

 // Track rows (Virtualization)
 int currentY = 0;
 for (int i = 0; i < impl_->trackHeights_.size(); ++i) {
  const int rowH = impl_->trackHeights_[i];
  const double rowTop = currentY - yOffset;
  
  // 画面外（dirtyRect外）の行は描画をスキップ
  if (rowTop + rowH >= dirtyRect.top() && rowTop <= dirtyRect.bottom()) {
    const QColor rowColor = (i % 2 == 0) ? theme.surface.lighter(106) : theme.surface.darker(108);
    p.fillRect(QRectF(0.0, rowTop, fullRect.width(), rowH), rowColor);
    p.setPen(QPen(theme.border.darker(160), 1));
    p.drawLine(0, rowTop + rowH, fullRect.width(), rowTop + rowH);
  }
  
  currentY += rowH + kTrackSpacing;
 }

 // Vertical frame grid (tick lines only, no labels).
 const int majorStep = 10;
 const int minorStep = 5;
 const int startFrame = std::max(0, static_cast<int>(std::floor((xOffset + dirtyRect.left()) / ppf)));
 const int endFrame = static_cast<int>(std::ceil((xOffset + dirtyRect.right()) / ppf));
 for (int f = startFrame; f <= endFrame; ++f) {
  const double x = f * ppf - xOffset;
  const bool major = (f % majorStep) == 0;
  const bool minor = !major && (f % minorStep) == 0;
  if (!major && !minor) {
   continue;
  }
  p.setPen(QPen(major ? theme.border.lighter(125) : theme.border.darker(115), 1));
  p.drawLine(QPointF(x, dirtyRect.top()), QPointF(x, dirtyRect.bottom()));
 }

 // Clips.
 for (int i = 0; i < impl_->clips_.size(); ++i) {
  const auto& clip = impl_->clips_[i];
  
  // クリップの描画範囲を計算
  const double clipX = clip.startFrame * ppf - xOffset;
  const double clipW = clip.durationFrame * ppf;
  
  // Y座標を特定するために再度ループ（キャッシュしておくと高速だが、まずは単純に）
  int clipY = 0;
  for (int t = 0; t < clip.trackIndex && t < impl_->trackHeights_.size(); ++t) {
      clipY += impl_->trackHeights_[t] + kTrackSpacing;
  }
  clipY -= static_cast<int>(std::round(yOffset));
  const int clipH = (clip.trackIndex >= 0 && clip.trackIndex < impl_->trackHeights_.size()) 
                    ? impl_->trackHeights_[clip.trackIndex] : kDefaultTrackHeight;

  // 可視性チェック
  if (clipX + clipW < dirtyRect.left() || clipX > dirtyRect.right() ||
      clipY + clipH < dirtyRect.top() || clipY > dirtyRect.bottom()) {
      continue;
  }

  if (clip.trackIndex < 0 || clip.trackIndex >= impl_->trackHeights_.size()) {
   continue;
  }
  const int trackTop = trackTopAt(impl_->trackHeights_, clip.trackIndex);
  const int trackH = impl_->trackHeights_[clip.trackIndex];
  const double x = clip.startFrame * ppf - xOffset;
  const double w = std::max(2.0, clip.durationFrame * ppf);
  QRectF clipRect(x, trackTop + 2.0 - yOffset, w, std::max(8, trackH - 4));
  if (!clipRect.intersects(QRectF(fullRect))) {
   continue;
  }

  const bool isHovered = (i == impl_->hoverClipIndex_);
  const bool isSelected = clip.selected;
  const QColor fill = clip.selected
   ? clip.fillColor.lighter(126)
   : (isHovered ? clip.fillColor.lighter(118) : clip.fillColor);
  p.setPen(QPen(clip.selected ? theme.accent.lighter(155) : theme.border.darker(170), clip.selected ? 2 : 1));
  p.setBrush(fill);
  p.drawRoundedRect(clipRect, kClipCorner, kClipCorner);

  if (isSelected || isHovered) {
   const QColor rim = isSelected
       ? QColor(theme.accent.lighter(160))
       : QColor(255, 255, 255, 90);
   p.setBrush(Qt::NoBrush);
   p.setPen(QPen(rim, isSelected ? 2.0 : 1.0));
   p.drawRoundedRect(clipRect.adjusted(1.0, 1.0, -1.0, -1.0), kClipCorner, kClipCorner);
  }

  if (!clip.title.isEmpty() && clipRect.width() > 28.0) {
   p.setPen(clip.selected ? theme.background : theme.text);
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

 // Keyframe markers.
 for (int markerIndex = 0; markerIndex < impl_->keyframeMarkers_.size(); ++markerIndex) {
  const auto& marker = impl_->keyframeMarkers_[markerIndex];
  if (marker.trackIndex < 0 || marker.trackIndex >= impl_->trackHeights_.size()) {
   continue;
  }
  const QPointF center = markerCenterFor(marker, impl_->trackHeights_, ppf, xOffset, yOffset);
  if (!dirtyRect.adjusted(-8, -8, 8, 8).contains(center.toPoint())) {
   continue;
  }
  const bool isHovered = markerIndex == impl_->hoverMarkerIndex_;
  const int size = marker.laneCount > 1 ? (isHovered ? 6 : 5) : (isHovered ? 7 : 6);
  const QRectF diamondRect(center.x() - size, center.y() - size,
                           size * 2.0, size * 2.0);
  QPolygonF diamond;
  diamond << QPointF(diamondRect.center().x(), diamondRect.top())
          << QPointF(diamondRect.right(), diamondRect.center().y())
          << QPointF(diamondRect.center().x(), diamondRect.bottom())
          << QPointF(diamondRect.left(), diamondRect.center().y());
  if (marker.selectedLayer) {
   QPolygonF outer = diamond;
   p.setPen(QPen(theme.background.darker(190), 2.0));
   p.setBrush(Qt::NoBrush);
   p.drawPolygon(outer);
   p.setPen(QPen(theme.text.lighter(120), 1.0));
   p.setBrush(Qt::white);
   p.drawPolygon(diamond);
  } else {
   p.setPen(QPen(theme.border.darker(170), 1));
   p.setBrush(marker.eased ? marker.color.lighter(105) : marker.color);
   p.drawPolygon(diamond);
  }
  if (!marker.label.isEmpty()) {
   p.setPen(marker.selectedLayer ? theme.text.lighter(120) : (isHovered ? theme.accent.lighter(180) : theme.text));
   p.drawText(QRectF(center.x() + 8.0, center.y() - 8.0, 150.0, 16.0),
              Qt::AlignLeft | Qt::AlignVCenter,
              marker.label);
  }
 }

 // Current frame marker.
 const double playheadX = impl_->currentFrame_ * ppf - xOffset;
 if (playheadX >= dirtyRect.left() - 4.0 && playheadX <= dirtyRect.right() + 4.0) {
  p.setPen(QPen(theme.accent, 3));
  p.drawLine(QPointF(playheadX, dirtyRect.top()), QPointF(playheadX, dirtyRect.bottom()));
  p.setBrush(theme.accent);
  p.setPen(Qt::NoPen);
  const QPointF tip(playheadX, dirtyRect.top() + 2.0);
  const QPolygonF head({
      tip + QPointF(-7.0, 0.0),
      tip + QPointF(7.0, 0.0),
      tip + QPointF(0.0, 11.0),
  });
  p.drawPolygon(head);
 }

 // Small HUD for track state.
 const QRect hudRect(10, 10, 204, 44);
 p.setPen(Qt::NoPen);
 p.setBrush(theme.background.darker(180));
 p.drawRoundedRect(hudRect, 8, 8);
 p.setPen(theme.text);
 const int selectedCount = std::count_if(
     impl_->clips_.cbegin(), impl_->clips_.cend(),
     [](const auto& clip) { return clip.selected; });
 const QString hoveredText = (impl_->hoverClipIndex_ >= 0 && impl_->hoverClipIndex_ < impl_->clips_.size())
     ? (impl_->clips_[impl_->hoverClipIndex_].title.isEmpty()
            ? impl_->clips_[impl_->hoverClipIndex_].clipId
            : impl_->clips_[impl_->hoverClipIndex_].title)
     : QStringLiteral("-");
 const QString hudText = QStringLiteral("F%1 | R%2 | KF%3")
                             .arg(static_cast<int>(std::round(impl_->currentFrame_)))
                             .arg(impl_->trackHeights_.size())
                             .arg(impl_->keyframeMarkers_.size());
 p.drawText(hudRect.adjusted(10, 4, -10, -18), Qt::AlignLeft | Qt::AlignVCenter, hudText);
 p.setPen(theme.text.darker(130));
 p.drawText(hudRect.adjusted(10, 20, -10, -4), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("Sel:%1  Hov:%2").arg(selectedCount).arg(hoveredText));

 }

void ArtifactTimelineTrackPainterView::mousePressEvent(QMouseEvent* event)
{
 if (event->button() == Qt::MiddleButton) {
  impl_->panning_ = true;
  impl_->lastPanPoint_ = event->position().toPoint();
  setCursor(Qt::ClosedHandCursor);
  event->accept();
  return;
 }

 if (event->button() == Qt::LeftButton) {
  const double mouseX = event->position().x();
  const double mouseY = event->position().y();
  const auto markerHit = hitTestMarkers(impl_->keyframeMarkers_, impl_->trackHeights_,
                                        mouseX, mouseY, impl_->pixelsPerFrame_,
                                        impl_->horizontalOffset_, impl_->verticalOffset_);
  if (markerHit.markerIndex >= 0) {
   const auto& marker = impl_->keyframeMarkers_[markerHit.markerIndex];
   const double frame = std::clamp(marker.frame, 0.0, impl_->durationFrames_);
   seekRequested(frame);
   setCurrentFrame(frame);
   event->accept();
   return;
  }
  const auto hit = hitTestClips(impl_->clips_, impl_->trackHeights_,
                                 mouseX, mouseY,
                                 impl_->pixelsPerFrame_, impl_->horizontalOffset_,
                                 impl_->verticalOffset_);
  if (hit.mode != DragMode::None) {
   impl_->dragMode_           = hit.mode;
   impl_->dragClipIndex_      = hit.clipIndex;
   impl_->dragStartX_         = mouseX;
   impl_->dragOrigStartFrame_ = impl_->clips_[hit.clipIndex].startFrame;
   impl_->dragOrigDuration_   = impl_->clips_[hit.clipIndex].durationFrame;
   const auto& clip = impl_->clips_[hit.clipIndex];
   clipSelected(clip.clipId, clip.layerId);
   if (hit.mode == DragMode::MoveBody) setCursor(Qt::ClosedHandCursor);
   event->accept();
   return;
  }
  clipDeselected();
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
 const auto markerHit = hitTestMarkers(impl_->keyframeMarkers_, impl_->trackHeights_,
                                       mouseX, mouseY, ppf, impl_->horizontalOffset_,
                                       impl_->verticalOffset_);

 if (impl_->dragMode_ != DragMode::None && impl_->dragClipIndex_ >= 0) {
  const auto oldClip = impl_->clips_[impl_->dragClipIndex_];
  const double deltaFrames = (mouseX - impl_->dragStartX_) / std::max(0.001, ppf);
  auto& clip = impl_->clips_[impl_->dragClipIndex_];
  switch (impl_->dragMode_) {
  case DragMode::MoveBody:
   clip.startFrame = impl_->dragOrigStartFrame_ + deltaFrames;
   break;
  case DragMode::ResizeLeft: {
   const double end      = impl_->dragOrigStartFrame_ + impl_->dragOrigDuration_;
   clip.startFrame       = std::min(impl_->dragOrigStartFrame_ + deltaFrames, end - 1.0);
   clip.durationFrame    = end - clip.startFrame;
   break;
  }
  case DragMode::ResizeRight:
   clip.durationFrame = std::max(1.0, impl_->dragOrigDuration_ + deltaFrames);
   break;
  default: break;
  }
  
  // Debug message emission
  const QString status = QStringLiteral("Layer: %1 | Start: %2 | Dur: %3")
      .arg(clip.title.isEmpty() ? clip.clipId : clip.title)
      .arg(QString::number(clip.startFrame, 'f', 1))
      .arg(QString::number(clip.durationFrame, 'f', 1));
  Q_EMIT timelineDebugMessage(status);

  const QRectF dirtyRect =
      clipRectFor(oldClip, impl_->trackHeights_, ppf, impl_->horizontalOffset_, impl_->verticalOffset_)
          .united(clipRectFor(clip, impl_->trackHeights_, ppf, impl_->horizontalOffset_, impl_->verticalOffset_));
  update(dirtyRect.adjusted(-2.0, -2.0, 2.0, 2.0).toAlignedRect());
  event->accept();
  return;
 }

  const auto hit = hitTestClips(impl_->clips_, impl_->trackHeights_,
                                mouseX, mouseY, ppf, impl_->horizontalOffset_,
                                impl_->verticalOffset_);
 const bool changed = (hit.clipIndex != impl_->hoverClipIndex_ || hit.mode != impl_->hoverEdge_);
 const auto oldHoverClipIndex = impl_->hoverClipIndex_;
 const auto oldHoverEdge = impl_->hoverEdge_;
 const int oldHoverMarkerIndex = impl_->hoverMarkerIndex_;
 impl_->hoverClipIndex_ = hit.clipIndex;
 impl_->hoverEdge_      = hit.mode;
 impl_->hoverMarkerIndex_ = markerHit.markerIndex;

 if (impl_->hoverMarkerIndex_ >= 0) {
  setCursor(Qt::PointingHandCursor);
 } else {
  switch (hit.mode) {
  case DragMode::ResizeLeft:
  case DragMode::ResizeRight: setCursor(Qt::SizeHorCursor);  break;
  case DragMode::MoveBody:    setCursor(Qt::OpenHandCursor); break;
  default:                    setCursor(Qt::ArrowCursor);    break;
  }
 }

 if (changed) {
  QRectF dirtyRect;
  if (oldHoverClipIndex >= 0 && oldHoverClipIndex < impl_->clips_.size()) {
   dirtyRect = clipRectFor(impl_->clips_[oldHoverClipIndex], impl_->trackHeights_,
                           ppf, impl_->horizontalOffset_, impl_->verticalOffset_);
  }
  if (hit.clipIndex >= 0 && hit.clipIndex < impl_->clips_.size()) {
   const QRectF rect = clipRectFor(impl_->clips_[hit.clipIndex], impl_->trackHeights_,
                                   ppf, impl_->horizontalOffset_, impl_->verticalOffset_);
   dirtyRect = dirtyRect.isValid() ? dirtyRect.united(rect) : rect;
  }
  if (!dirtyRect.isValid() && oldHoverEdge != hit.mode) {
   dirtyRect = QRectF(0.0, 0.0, width(), height());
  }
  update(dirtyRect.adjusted(-2.0, -2.0, 2.0, 2.0).toAlignedRect());
 }
 if (oldHoverMarkerIndex != impl_->hoverMarkerIndex_) {
  QRectF dirtyRect;
 if (oldHoverMarkerIndex >= 0 && oldHoverMarkerIndex < impl_->keyframeMarkers_.size()) {
  dirtyRect = markerHitRectFor(impl_->keyframeMarkers_[oldHoverMarkerIndex],
                                impl_->trackHeights_, ppf, impl_->horizontalOffset_,
                                impl_->verticalOffset_);
  }
  if (impl_->hoverMarkerIndex_ >= 0 && impl_->hoverMarkerIndex_ < impl_->keyframeMarkers_.size()) {
   const QRectF rect = markerHitRectFor(impl_->keyframeMarkers_[impl_->hoverMarkerIndex_],
                                        impl_->trackHeights_, ppf, impl_->horizontalOffset_,
                                        impl_->verticalOffset_);
   dirtyRect = dirtyRect.isValid() ? dirtyRect.united(rect) : rect;
  }
  update((dirtyRect.isValid() ? dirtyRect : QRectF(0.0, 0.0, width(), height()))
             .adjusted(-2.0, -2.0, 2.0, 2.0)
             .toAlignedRect());
 }
 if (impl_->panning_ && (event->buttons() & Qt::MiddleButton)) {
  const QPoint current = event->position().toPoint();
  const QPoint delta = current - impl_->lastPanPoint_;
  impl_->lastPanPoint_ = current;
  if (delta.x() != 0) {
   setHorizontalOffset(std::max(0.0, impl_->horizontalOffset_ - static_cast<double>(delta.x())));
  }
  if (delta.y() != 0) {
   setVerticalOffset(std::max(0.0, impl_->verticalOffset_ - static_cast<double>(delta.y())));
  }
  event->accept();
  return;
 }
 QWidget::mouseMoveEvent(event);
}

void ArtifactTimelineTrackPainterView::mouseReleaseEvent(QMouseEvent* event)
{
 if (event->button() == Qt::MiddleButton && impl_->panning_) {
  impl_->panning_ = false;
  setCursor(Qt::ArrowCursor);
  event->accept();
  return;
 }

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

void ArtifactTimelineTrackPainterView::contextMenuEvent(QContextMenuEvent* event)
{
 if (!event) {
  return;
 }

 ArtifactCompositionPtr composition;
 if (auto *svc = ArtifactProjectService::instance()) {
  composition = svc->currentComposition().lock();
 }

 const double mouseX = static_cast<double>(event->pos().x());
 const double mouseY = static_cast<double>(event->pos().y());
 const auto clipHit = hitTestClips(impl_->clips_, impl_->trackHeights_, mouseX, mouseY,
                                   impl_->pixelsPerFrame_, impl_->horizontalOffset_,
                                   impl_->verticalOffset_);
 const bool clipUnderCursor = clipHit.mode != DragMode::None &&
                              clipHit.clipIndex >= 0 &&
                              clipHit.clipIndex < impl_->clips_.size();
 const auto layers = selectedTimelineLayers();
 const auto markerHit = hitTestMarkers(impl_->keyframeMarkers_, impl_->trackHeights_,
                                       mouseX, mouseY, impl_->pixelsPerFrame_,
                                       impl_->horizontalOffset_, impl_->verticalOffset_);
 const bool markerUnderCursor = markerHit.markerIndex >= 0 &&
                                markerHit.markerIndex < impl_->keyframeMarkers_.size();
 if (layers.isEmpty() && !clipUnderCursor && !markerUnderCursor) {
  event->ignore();
  return;
 }
 const qint64 markerFrame = markerUnderCursor
                                ? static_cast<qint64>(std::llround(
                                      std::clamp(impl_->keyframeMarkers_[markerHit.markerIndex].frame,
                                                 0.0, impl_->durationFrames_)))
                                : static_cast<qint64>(std::llround(
                                      std::clamp(impl_->currentFrame_, 0.0, impl_->durationFrames_)));

 QMenu menu(this);
 QAction* jumpToMarkerAct = nullptr;
 if (markerUnderCursor) {
  const auto &marker = impl_->keyframeMarkers_[markerHit.markerIndex];
  const QString label = marker.label.isEmpty() ? QStringLiteral("Keyframe") : marker.label;
  jumpToMarkerAct = menu.addAction(QStringLiteral("Jump to %1").arg(label));
 }
 menu.addAction(QStringLiteral("Add Keyframe at Playhead"));
 QAction* removeKeyframeAct = menu.addAction(QStringLiteral("Remove Keyframe at Playhead"));
 QAction* addMarkerFrameAct = nullptr;
 QAction* removeMarkerFrameAct = nullptr;
 if (markerUnderCursor) {
  menu.addSeparator();
  addMarkerFrameAct = menu.addAction(QStringLiteral("Add Keyframe at Marker Frame"));
  removeMarkerFrameAct = menu.addAction(QStringLiteral("Remove Keyframe at Marker Frame"));
 }

 QAction* splitClipAct = nullptr;
 QAction* duplicateClipAct = nullptr;
 QAction* trimInClipAct = nullptr;
 QAction* trimOutClipAct = nullptr;
 QAction* moveStartClipAct = nullptr;
 QAction* deleteClipAct = nullptr;
 if (clipUnderCursor) {
  if (!markerUnderCursor) {
   menu.addSeparator();
  }
  splitClipAct = menu.addAction(QStringLiteral("Split Layer at Playhead"));
  duplicateClipAct = menu.addAction(QStringLiteral("Duplicate Layer"));
  moveStartClipAct = menu.addAction(QStringLiteral("Move Start to Playhead"));
  trimInClipAct = menu.addAction(QStringLiteral("Trim In at Playhead"));
  trimOutClipAct = menu.addAction(QStringLiteral("Trim Out at Playhead"));
  deleteClipAct = menu.addAction(QStringLiteral("Delete Layer"));
 }

 const QAction* chosen = menu.exec(event->globalPos());
 if (!chosen) {
  event->accept();
  return;
 }

 if (chosen == jumpToMarkerAct) {
  const double targetFrame =
      std::clamp(static_cast<double>(markerFrame), 0.0, impl_->durationFrames_);
  setCurrentFrame(targetFrame);
  seekRequested(targetFrame);
  if (auto *svc = ArtifactProjectService::instance()) {
   if (auto comp = svc->currentComposition().lock()) {
    comp->goToFrame(static_cast<int64_t>(std::llround(targetFrame)));
   }
  }
  event->accept();
  return;
 }

 if (clipUnderCursor && composition) {
  const auto &clip = impl_->clips_[clipHit.clipIndex];
  const auto layer = composition->layerById(clip.layerId);
  if (chosen == splitClipAct) {
   if (auto *svc = ArtifactProjectService::instance()) {
    svc->splitLayerAtCurrentTime(composition->id(), clip.layerId);
   }
   event->accept();
   return;
  }
  if (chosen == duplicateClipAct) {
   if (auto *svc = ArtifactProjectService::instance()) {
    if (svc->duplicateLayerInCurrentComposition(clip.layerId)) {
     svc->projectChanged();
    }
   }
   event->accept();
   return;
  }
  if (chosen == deleteClipAct) {
   if (auto *svc = ArtifactProjectService::instance()) {
    svc->removeLayerFromComposition(composition->id(), clip.layerId);
   }
   event->accept();
   return;
  }
  if (layer && (chosen == moveStartClipAct || chosen == trimInClipAct || chosen == trimOutClipAct)) {
   const qint64 currentFrame = static_cast<qint64>(std::llround(
       std::clamp(impl_->currentFrame_, 0.0, impl_->durationFrames_)));
   bool changed = false;
   if (chosen == moveStartClipAct) {
    changed = applyTimelineLayerRangeEdit(layer, currentFrame, 0, true);
   } else if (chosen == trimInClipAct) {
    changed = applyTimelineLayerRangeEdit(layer, currentFrame, 0, false);
   } else if (chosen == trimOutClipAct) {
    const qint64 duration = layer->outPoint().framePosition() - layer->inPoint().framePosition();
    changed = applyTimelineLayerRangeEdit(layer, currentFrame - duration, duration, false);
   }
   if (changed) {
    if (auto *svc = ArtifactProjectService::instance()) {
     svc->projectChanged();
    }
    Q_EMIT timelineDebugMessage(
        QStringLiteral("Edited %1 at F%2")
            .arg(clip.title.isEmpty() ? clip.clipId : clip.title)
            .arg(currentFrame));
   }
   event->accept();
   return;
  }
 }

 const bool removeKeyframes = (chosen == removeKeyframeAct || chosen == removeMarkerFrameAct);
 const qint64 editFrame = (chosen == addMarkerFrameAct || chosen == removeMarkerFrameAct)
                              ? markerFrame
                              : static_cast<qint64>(
                                    std::llround(std::clamp(impl_->currentFrame_, 0.0, impl_->durationFrames_)));
 bool changed = false;
 for (const auto &layer : layers) {
  changed |= applyKeyframeEditAtFrame(composition, layer, editFrame, removeKeyframes);
 }

 if (changed) {
  if (auto *svc = ArtifactProjectService::instance()) {
   svc->projectChanged();
  }
  const QString actionText = removeKeyframes ? QStringLiteral("Removed") : QStringLiteral("Added");
  Q_EMIT timelineDebugMessage(
      QStringLiteral("%1 keyframe at F%2 for %3 layer(s)")
          .arg(actionText)
          .arg(editFrame)
          .arg(layers.size()));
  update();
 }

 event->accept();
}

void ArtifactTimelineTrackPainterView::wheelEvent(QWheelEvent* event)
{
 if (!event) {
  return;
 }

 const QPoint angle = event->angleDelta();
 if (angle.isNull()) {
  event->ignore();
  return;
 }

 const double delta = static_cast<double>(angle.y()) / 120.0 * 40.0;
 if (event->modifiers() & Qt::ShiftModifier) {
  setHorizontalOffset(std::max(0.0, impl_->horizontalOffset_ - delta));
 } else {
  setVerticalOffset(std::max(0.0, impl_->verticalOffset_ - delta));
 }
 event->accept();
}

void ArtifactTimelineTrackPainterView::leaveEvent(QEvent* event)
{
 QWidget::leaveEvent(event);

 const bool hadHover = impl_->hoverClipIndex_ >= 0 || impl_->hoverEdge_ != DragMode::None;
 const bool hadMarkerHover = impl_->hoverMarkerIndex_ >= 0;
 impl_->hoverClipIndex_ = -1;
 impl_->hoverEdge_ = DragMode::None;
 impl_->hoverMarkerIndex_ = -1;

 if (impl_->dragMode_ == DragMode::None) {
  setCursor(Qt::ArrowCursor);
 }

 if (hadHover || hadMarkerHover) {
  update();
 }
 Q_UNUSED(event);
}

} // namespace Artifact
