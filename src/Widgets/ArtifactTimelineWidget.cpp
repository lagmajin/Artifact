module;

#include <QBoxLayout>
#include <QBrush>
#include <QComboBox>
#include <QEvent>
#include <QGraphicsRectItem>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardItem>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>
#include <cmath>
#include <limits>
#include <qtmetamacros.h>
#include <wobjectdefs.h>
#include <wobjectimpl.h>

module Artifact.Widgets.Timeline;

import std;

import Widgets.Utils.CSS;

import Artifact.Layers.Hierarchy.Model;
import Artifact.Widget.WorkAreaControlWidget;

import ArtifactTimelineIconModel;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Timeline.ScrubBar;
import Artifact.Widgets.Timeline.Label;
import Artifact.Timeline.NavigatorWidget;
import Artifact.Timeline.TrackPainterView;
import Artifact.Timeline.TimeCodeWidget;
import Artifact.Layers.Selection.Manager;
import Panel.DraggableSplitter;
import Artifact.Timeline.Objects;
import Artifact.Widgets.Timeline.GlobalSwitches;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Audio;
import Artifact.Layer.Camera;
import Artifact.Layer.Image;
import Artifact.Layer.Light;
import Artifact.Layer.Particle;
import Artifact.Layer.Shape;
import Artifact.Layer.Solid2D;
import Artifact.Layer.Svg;
import Artifact.Layer.Text;
import Artifact.Layer.Video;
import Artifact.Effect.Abstract;
import Frame.Position;
import Time.Rational;

namespace Artifact {

using namespace ArtifactCore;
using namespace ArtifactWidgets;

namespace {
constexpr double kTimelineRowHeight = 28.0;
constexpr int kTimelineTopRowHeight = 16; // aligns with right ruler row
constexpr int kTimelineHeaderRowHeight =
    34; // fits timecode + frame labels without compression
constexpr int kTimelineWorkAreaRowHeight = 26;
constexpr int kDefaultTimelineFrames = 300;
constexpr int kTopSeekHotZonePx =
    28; // allow seeking from top band of clip scene
constexpr double kTimelineScrollPaddingFrames = 120.0;
inline double timelineFrameMax(const double duration) {
  return std::max(0.0, duration - 1.0);
}
inline int64_t timelineCompositionFrameCount(const ArtifactCompositionPtr &comp,
                                             const int64_t fallbackFrames) {
  if (!comp) {
    return std::max<int64_t>(1, fallbackFrames);
  }
  return std::max<int64_t>(1, comp->frameRange().duration());
}
inline double timelineViewportPaddingFrames(const QGraphicsView *view,
                                            const double zoomLevel) {
  if (!view || !view->viewport()) {
    return kTimelineScrollPaddingFrames;
  }

  const int viewportWidth = view->viewport()->width();
  if (viewportWidth <= 0 || zoomLevel <= 1e-5) {
    return kTimelineScrollPaddingFrames;
  }

  return std::max(kTimelineScrollPaddingFrames,
                  static_cast<double>(viewportWidth) / zoomLevel);
}
inline double timelineSceneWidth(const double duration,
                                 const QGraphicsView *view = nullptr,
                                 const double zoomLevel = 1.0) {
  return std::max(1.0,
                  duration + timelineViewportPaddingFrames(view, zoomLevel));
}
inline int wheelScrollDelta(const QWheelEvent *event, const bool horizontal) {
  if (!event) {
    return 0;
  }

  const QPoint pixelDelta = event->pixelDelta();
  if (!pixelDelta.isNull()) {
    return horizontal ? pixelDelta.x() : pixelDelta.y();
  }

  const QPoint angleDelta = event->angleDelta();
  return horizontal ? angleDelta.x() : angleDelta.y();
}
inline int
layerInsertionIndexForTrackDrop(const QVector<LayerID> &trackLayerIds,
                                const LayerID &draggedLayerId,
                                const int trackIndex) {
  int targetLayerIndex = 0;
  const int trackCount = static_cast<int>(trackLayerIds.size());
  const int upperBound = std::clamp<int>(trackIndex, 0, trackCount);
  for (int i = 0; i < upperBound; ++i) {
    const auto &candidate = trackLayerIds[i];
    if (candidate.isNil() || candidate == draggedLayerId) {
      continue;
    }
    ++targetLayerIndex;
  }
  return targetLayerIndex;
}

QColor layerTimelineColor(const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) {
    return QColor(94, 124, 189);
  }
  if (std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
    return QColor(79, 142, 230);
  }
  if (std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
    return QColor(86, 180, 120);
  }
  if (std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
    return QColor(165, 108, 255);
  }
  if (std::dynamic_pointer_cast<ArtifactShapeLayer>(layer) ||
      std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
    return QColor(146, 106, 235);
  }
  if (std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
    return QColor(84, 163, 255);
  }
  if (std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer)) {
    return QColor(255, 145, 86);
  }
  if (std::dynamic_pointer_cast<ArtifactCameraLayer>(layer)) {
    return QColor(255, 193, 79);
  }
  if (std::dynamic_pointer_cast<ArtifactLightLayer>(layer)) {
    return QColor(255, 221, 102);
  }
  if (std::dynamic_pointer_cast<ArtifactParticleLayer>(layer)) {
    return QColor(255, 110, 180);
  }
  return QColor(94, 124, 189);
}

double layerTimelineMaxDurationFrames(const ArtifactAbstractLayerPtr& layer,
                                      const double fps)
{
  if (!layer || fps <= 0.0) {
    return 0.0;
  }
  if (const auto video = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
    const qint64 sourceFrames = video->effectiveFrameCount();
    return sourceFrames > 0 ? static_cast<double>(sourceFrames) : 0.0;
  }
  if (const auto audio = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
    const double sourceFrames = audio->duration() * fps;
    return sourceFrames > 0.0 ? std::ceil(sourceFrames) : 0.0;
  }
  return 0.0;
}

bool applyTimelineLayerRangeEdit(const CompositionID &compositionId,
                                 const QString &layerIdText,
                                 const double startFrame,
                                 const double durationFrame,
                                 const bool preserveExistingDuration) {
  if (layerIdText.trimmed().isEmpty()) {
    return false;
  }

  auto *svc = ArtifactProjectService::instance();
  if (!svc) {
    return false;
  }

  auto result = svc->findComposition(compositionId);
  if (!result.success) {
    return false;
  }

  auto comp = result.ptr.lock();
  if (!comp) {
    return false;
  }

  auto layer = comp->layerById(LayerID(layerIdText));
  if (!layer) {
    return false;
  }

  const int64_t oldInPoint = layer->inPoint().framePosition();
  const int64_t oldOutPoint = layer->outPoint().framePosition();
  const int64_t oldStartTime = layer->startTime().framePosition();
  const int64_t oldDuration = std::max<int64_t>(1, oldOutPoint - oldInPoint);

  const int64_t inPoint =
      std::max<int64_t>(0, static_cast<int64_t>(std::llround(startFrame)));
  const int64_t outPoint = preserveExistingDuration
                               ? std::max<int64_t>(inPoint + 1, inPoint + oldDuration)
                               : std::max<int64_t>(
                                     inPoint + 1,
                                     static_cast<int64_t>(
                                         std::llround(startFrame + durationFrame)));
  const int64_t inPointDelta = inPoint - oldInPoint;

  layer->setInPoint(FramePosition(inPoint));
  layer->setOutPoint(FramePosition(outPoint));

  // Move keeps the source offset stable.
  // Trim-in shifts the source offset by the same amount as the new in-point.
  if (!preserveExistingDuration && inPointDelta != 0) {
    layer->setStartTime(FramePosition(oldStartTime + inPointDelta));
  }
  
  svc->projectChanged();
  return true;
}

bool applyTimelineLayerMove(const CompositionID &compositionId,
                            const QString &layerIdText,
                            const double startFrame,
                            const double durationFrame) {
  return applyTimelineLayerRangeEdit(compositionId, layerIdText, startFrame,
                                     durationFrame, true);
}

bool applyTimelineLayerTrim(const CompositionID &compositionId,
                            const QString &layerIdText,
                            const double startFrame,
                            const double durationFrame) {
  return applyTimelineLayerRangeEdit(compositionId, layerIdText, startFrame,
                                     durationFrame, false);
}

std::shared_ptr<ArtifactAbstractComposition>
safeCompositionLookup(const CompositionID &id) {
  if (id.isNil())
    return nullptr;
  auto *service = ArtifactProjectService::instance();
  if (!service)
    return nullptr;
  auto result = service->findComposition(id);
  if (!result.success)
    return nullptr;
  return result.ptr.lock();
}

QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>
collectKeyframeMarkers(const ArtifactCompositionPtr& composition,
                       ArtifactLayerSelectionManager* selectionManager,
                       const QHash<LayerID, int>& trackIndexByLayerId)
{
  QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> markers;
  if (!composition) {
    return markers;
  }

  QVector<ArtifactAbstractLayerPtr> layers;
  layers.reserve(trackIndexByLayerId.size());
  if (const auto visibleLayerIds = trackIndexByLayerId.keys(); !visibleLayerIds.isEmpty()) {
    for (const auto& layerId : visibleLayerIds) {
      if (layerId.isNil()) {
        continue;
      }
      if (auto layer = composition->layerById(layerId)) {
        layers.push_back(layer);
      }
    }
  }

  if (layers.isEmpty()) {
    return markers;
  }

  QSet<ArtifactAbstractLayerPtr> highlightLayers;
  if (selectionManager) {
    highlightLayers = selectionManager->selectedLayers();
    if (highlightLayers.isEmpty()) {
      if (auto currentLayer = selectionManager->currentLayer()) {
        highlightLayers.insert(currentLayer);
      }
    }
  }

  const double fps = std::max(
      1.0, static_cast<double>(composition->frameRate().framerate()));
  for (const auto& layer : layers) {
    if (!layer) {
      continue;
    }
    const auto trackIt = trackIndexByLayerId.constFind(layer->id());
    if (trackIt == trackIndexByLayerId.constEnd()) {
      continue;
    }
    const int trackIndex = trackIt.value();
    const auto groups = layer->getLayerPropertyGroups();
    struct DraftMarker {
      qint64 frame = 0;
      QString label;
      QString propertyPath;
      QColor color;
      bool selectedLayer = false;
      bool eased = false;
    };
    QVector<DraftMarker> drafts;
    for (const auto& group : groups) {
      for (const auto& property : group.sortedProperties()) {
        if (!property || !property->isAnimatable()) {
          continue;
        }
        const auto keyframes = property->getKeyFrames();
        for (const auto& keyframe : keyframes) {
          const qint64 frame = keyframe.time.rescaledTo(static_cast<int64_t>(std::round(fps)));
          const QString propertyName = property->getName();
          const bool selectedLayer = highlightLayers.contains(layer);
          const bool eased = keyframe.easing != EasingType::Linear &&
                             keyframe.easing != EasingType::Hold;
          QColor color = selectedLayer
                             ? QColor(255, 255, 255)
                             : (eased ? QColor(82, 208, 255)
                                      : QColor(247, 204, 83));
          color.setAlpha(selectedLayer ? 255 : 245);
          drafts.push_back(DraftMarker{frame, propertyName, propertyName, color, selectedLayer, eased});
        }
      }
    }

    if (drafts.isEmpty()) {
      continue;
    }

    std::sort(drafts.begin(), drafts.end(), [](const DraftMarker& lhs, const DraftMarker& rhs) {
      if (lhs.frame != rhs.frame) {
        return lhs.frame < rhs.frame;
      }
      return lhs.label < rhs.label;
    });

    QHash<qint64, int> laneCounts;
    for (const auto& draft : drafts) {
      laneCounts[draft.frame] += 1;
    }

    QHash<qint64, int> laneCursor;
    for (const auto& draft : drafts) {
      ArtifactTimelineTrackPainterView::KeyframeMarkerVisual marker;
      marker.layerId = layer->id();
      marker.trackIndex = trackIndex;
      marker.frame = static_cast<double>(draft.frame);
      marker.label = draft.label;
      marker.propertyPath = draft.propertyPath;
      marker.color = draft.color;
      marker.selectedLayer = draft.selectedLayer;
      marker.eased = draft.eased;
      marker.laneCount = std::max(1, laneCounts.value(draft.frame, 1));
      marker.laneIndex = laneCursor.value(draft.frame, 0);
      laneCursor[draft.frame] = marker.laneIndex + 1;
      markers.push_back(std::move(marker));
    }
  }

  return markers;
}

QVector<qint64> collectSelectedKeyframeFrames(
    const ArtifactCompositionPtr& composition,
    ArtifactLayerSelectionManager* selectionManager)
{
  QVector<qint64> frames;
  if (!composition || !selectionManager) {
    return frames;
  }

  const auto selectedLayers = selectionManager->selectedLayers();
  QSet<ArtifactAbstractLayerPtr> layers = selectedLayers;
  if (layers.isEmpty()) {
    if (auto currentLayer = selectionManager->currentLayer()) {
      layers.insert(currentLayer);
    }
  }
  if (layers.isEmpty()) {
    return frames;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  QSet<qint64> seenFrames;
  for (const auto& layer : layers) {
    if (!layer) {
      continue;
    }
    const auto groups = layer->getLayerPropertyGroups();
    for (const auto& group : groups) {
      for (const auto& property : group.sortedProperties()) {
        if (!property || !property->isAnimatable()) {
          continue;
        }
        const auto keyframes = property->getKeyFrames();
        for (const auto& keyframe : keyframes) {
          const qint64 frame =
              keyframe.time.rescaledTo(static_cast<int64_t>(std::round(fps)));
          if (seenFrames.contains(frame)) {
            continue;
          }
          seenFrames.insert(frame);
          frames.push_back(frame);
        }
      }
    }
  }

  std::sort(frames.begin(), frames.end());
  return frames;
}

QHash<LayerID, int> buildTrackIndexByLayerId(const QVector<LayerID>& trackLayerIds)
{
  QHash<LayerID, int> map;
  for (int i = 0; i < trackLayerIds.size(); ++i) {
    const auto& layerId = trackLayerIds[i];
    if (!layerId.isNil()) {
      map.insert(layerId, i);
    }
  }
  return map;
}

bool applyKeyframeEditAtPlayhead(const ArtifactCompositionPtr& composition,
                                 const LayerID& layerId,
                                 const bool removeKeyframes)
{
  if (!composition || layerId.isNil()) {
    return false;
  }

  auto layer = composition->layerById(layerId);
  if (!layer) {
    return false;
  }

  const double fps = std::max(
      1.0, static_cast<double>(composition->frameRate().framerate()));
  const RationalTime nowTime(static_cast<int64_t>(std::llround(
                                std::max<int64_t>(0, composition->framePosition().framePosition()))),
                             static_cast<int64_t>(std::llround(fps)));

  bool changed = false;
  for (const auto& group : layer->getLayerPropertyGroups()) {
    for (const auto& property : group.sortedProperties()) {
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

bool moveKeyframeAtFrame(const ArtifactCompositionPtr& composition,
                         const LayerID& layerId,
                         const QString& propertyPath,
                         const qint64 fromFrame,
                         const qint64 toFrame)
{
  if (!composition || layerId.isNil() || propertyPath.trimmed().isEmpty()) {
    return false;
  }

  auto layer = composition->layerById(layerId);
  if (!layer) {
    return false;
  }

  auto property = layer->getProperty(propertyPath);
  if (!property || !property->isAnimatable()) {
    return false;
  }

  const double fps = std::max(
      1.0, static_cast<double>(composition->frameRate().framerate()));
  const RationalTime fromTime(fromFrame, static_cast<int64_t>(std::llround(fps)));
  const RationalTime toTime(toFrame, static_cast<int64_t>(std::llround(fps)));
  if (fromTime == toTime) {
    return false;
  }

  const auto keyframes = property->getKeyFrames();
  const auto it = std::find_if(
      keyframes.begin(), keyframes.end(),
      [&fromTime](const KeyFrame& keyframe) { return keyframe.time == fromTime; });
  if (it == keyframes.end()) {
    return false;
  }

  const KeyFrame moved = *it;
  property->removeKeyFrame(fromTime);
  property->addKeyFrame(toTime, moved.value, moved.easing);
  layer->changed();
  return true;
}

struct KeyframeNavigationState {
  int totalFrames = 0;
  bool currentFrameHasKeyframe = false;
  qint64 previousKeyframe = -1;
  qint64 nextKeyframe = -1;
};

KeyframeNavigationState collectKeyframeNavigationState(
    const ArtifactCompositionPtr& composition,
    ArtifactLayerSelectionManager* selectionManager,
    const qint64 currentFrame)
{
  KeyframeNavigationState state;
  const auto frames = collectSelectedKeyframeFrames(composition, selectionManager);
  state.totalFrames = frames.size();
  if (frames.isEmpty()) {
    return state;
  }

  state.currentFrameHasKeyframe = std::binary_search(
      frames.cbegin(), frames.cend(), currentFrame);

  for (const qint64 frame : frames) {
    if (frame < currentFrame) {
      state.previousKeyframe = frame;
    } else if (frame > currentFrame) {
      state.nextKeyframe = frame;
      break;
    }
  }
  return state;
}

QString formatKeyframeNavigationText(const KeyframeNavigationState& state)
{
  if (state.totalFrames <= 0) {
    return QStringLiteral("Keyframes: -");
  }

  const QString currentMark =
      state.currentFrameHasKeyframe ? QStringLiteral("Yes") : QStringLiteral("No");
  const QString previousMark =
      state.previousKeyframe >= 0 ? QString::number(state.previousKeyframe)
                                  : QStringLiteral("-");
  const QString nextMark =
      state.nextKeyframe >= 0 ? QString::number(state.nextKeyframe)
                              : QStringLiteral("-");

  return QStringLiteral("Key:%1 Now:%2 Prev:%3 Next:%4")
      .arg(state.totalFrames)
      .arg(currentMark)
      .arg(previousMark)
      .arg(nextMark);
}

class HeaderSeekFilter final : public QObject {
public:
  HeaderSeekFilter(ArtifactTimelineTrackPainterView *trackView,
                   ArtifactTimelineScrubBar *scrubBar,
                   QObject *parent = nullptr)
      : QObject(parent), trackView_(trackView), scrubBar_(scrubBar) {}

  void setDebugCallback(std::function<void(const QString&)> callback) {
    debugCallback_ = std::move(callback);
  }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (!trackView_ || !scrubBar_) {
      return QObject::eventFilter(watched, event);
    }
    if (event->type() != QEvent::MouseButtonPress &&
        event->type() != QEvent::MouseMove &&
        event->type() != QEvent::MouseButtonRelease) {
      return QObject::eventFilter(watched, event);
    }

    auto *mouseEvent = dynamic_cast<QMouseEvent *>(event);
    auto *sourceWidget = qobject_cast<QWidget *>(watched);
    if (!mouseEvent || !sourceWidget) {
      return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress &&
        mouseEvent->button() != Qt::LeftButton) {
      reservedClickCandidate_ = false;
      reservedClickSource_ = nullptr;
      return QObject::eventFilter(watched, event);
    }
    if (event->type() == QEvent::MouseMove &&
        !(mouseEvent->buttons() & Qt::LeftButton)) {
      seeking_ = false;
      seekSource_ = nullptr;
      reservedClickCandidate_ = false;
      reservedClickSource_ = nullptr;
      return QObject::eventFilter(watched, event);
    }

    if (!trackView_ || !scrubBar_) {
      return QObject::eventFilter(watched, event);
    }

    const double frameMax =
        std::max(1.0, timelineFrameMax(trackView_->durationFrames()));
    const auto seekFromHeaderWidget = [&](QWidget *widget,
                                          const QPoint &pos) -> double {
      const QRect widgetRect = widget->rect();
      if (widgetRect.isEmpty()) {
        return 0.0;
      }

      const int localX = std::clamp(pos.x(), widgetRect.left(), widgetRect.right());
      const double normalized =
          static_cast<double>(localX - widgetRect.left()) /
          std::max(1.0, static_cast<double>(widgetRect.width() - 1));
      const double frame =
          normalized * std::max(0.0, trackView_->durationFrames());
      return std::clamp(frame, 0.0, frameMax);
    };

    if (event->type() == QEvent::MouseButtonRelease) {
      const bool reservedClick = reservedClickCandidate_ &&
                                 reservedClickSource_ == sourceWidget &&
                                 mouseEvent->button() == Qt::LeftButton;
      const int dragDistance =
          reservedClick ? (sourceWidget->mapToGlobal(mouseEvent->pos()) -
                           reservedPressGlobalPos_)
                              .manhattanLength()
                        : 0;

      if (mouseEvent->button() == Qt::LeftButton) {
        seeking_ = false;
        seekSource_ = nullptr;
      }

      reservedClickCandidate_ = false;
      reservedClickSource_ = nullptr;

      if (reservedClick && dragDistance <= kReservedClickDragThresholdPx) {
        const double clamped =
            seekFromHeaderWidget(sourceWidget, mouseEvent->pos());
        const int frame = static_cast<int>(std::round(clamped));
        trackView_->setCurrentFrame(clamped);
        scrubBar_->setCurrentFrame(FramePosition(frame));
        
        if (debugCallback_) {
          debugCallback_(QStringLiteral("Playhead: %1 (Seek)").arg(frame));
        }

        event->accept();
        return true;
      }

      return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseMove && reservedClickCandidate_ &&
        reservedClickSource_ == sourceWidget) {
      const int dragDistance = (sourceWidget->mapToGlobal(mouseEvent->pos()) -
                                reservedPressGlobalPos_)
                                   .manhattanLength();
      if (dragDistance > kReservedClickDragThresholdPx) {
        reservedClickCandidate_ = false;
        reservedClickSource_ = nullptr;
      }
      return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseMove &&
        (!seeking_ || seekSource_ != sourceWidget)) {
      return QObject::eventFilter(watched, event);
    }
    double clamped = 0.0;

    if (event->type() == QEvent::MouseButtonPress &&
        isReservedRangeInteraction(sourceWidget, mouseEvent->pos())) {
      seeking_ = false;
      seekSource_ = nullptr;
      reservedClickCandidate_ = true;
      reservedClickSource_ = sourceWidget;
      reservedPressGlobalPos_ = sourceWidget->mapToGlobal(mouseEvent->pos());
      return QObject::eventFilter(watched, event);
    }
    reservedClickCandidate_ = false;
    reservedClickSource_ = nullptr;
    clamped = seekFromHeaderWidget(sourceWidget, mouseEvent->pos());

    seeking_ = true;
    seekSource_ = sourceWidget;
    const int frame = static_cast<int>(std::round(clamped));

    trackView_->setCurrentFrame(clamped);
    scrubBar_->setCurrentFrame(FramePosition(frame));

    if (debugCallback_) {
      debugCallback_(QStringLiteral("Playhead: %1 (Scrubbing)").arg(frame));
    }

    event->accept();
    return true;
  }

private:
  static bool isReservedRangeInteraction(QWidget *widget, const QPoint &pos) {
    if (auto *navigator =
            dynamic_cast<ArtifactTimelineNavigatorWidget *>(widget)) {
      return isHandleInteraction(pos, navigator->width(), navigator->height(),
                                 navigator->startValue(),
                                 navigator->endValue());
    }
    if (auto *workArea = dynamic_cast<WorkAreaControl *>(widget)) {
      return isHandleInteraction(pos, workArea->width(), workArea->height(),
                                 workArea->startValue(), workArea->endValue());
    }
    return false;
  }

  static bool isHandleInteraction(const QPoint &pos, const int width,
                                  const int height, const float start,
                                  const float end) {
    const int handleHalfW = 6;
    const int handleW = handleHalfW * 2;
    const int usableWidth = std::max(1, width - handleW);
    const int x1 = handleHalfW + static_cast<int>(start * usableWidth);
    const int x2 = handleHalfW + static_cast<int>(end * usableWidth);

    const QRect leftHandleRect(x1 - handleHalfW, 0, handleW, height);
    const QRect rightHandleRect(x2 - handleHalfW, 0, handleW, height);
    if (leftHandleRect.contains(pos) || rightHandleRect.contains(pos)) {
      return true;
    }

    // When the range spans nearly the whole widget, reserving the whole bar
    // creates an oversized dead zone for playhead seeking. In that case only
    // the handles stay reserved.
    if (start <= 0.001f && end >= 0.999f) {
      return false;
    }

    const QRect rangeRect(x1 + handleHalfW, 0, std::max(0, x2 - x1 - handleW),
                          height);
    return rangeRect.contains(pos);
  }

  ArtifactTimelineTrackPainterView *trackView_ = nullptr;
  ArtifactTimelineScrubBar *scrubBar_ = nullptr;
  bool seeking_ = false;
  QWidget *seekSource_ = nullptr;
  bool reservedClickCandidate_ = false;
  QWidget *reservedClickSource_ = nullptr;
  QPoint reservedPressGlobalPos_;
  std::function<void(const QString&)> debugCallback_;
  static constexpr int kReservedClickDragThresholdPx = 4;
};

class HeaderScrollFilter final : public QObject {
public:
  HeaderScrollFilter(ArtifactTimelineTrackPainterView *trackView,
                     QObject *parent = nullptr)
      : QObject(parent), trackView_(trackView) {}

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (!trackView_) {
      return QObject::eventFilter(watched, event);
    }

    auto *widget = qobject_cast<QWidget *>(watched);
    if (!widget) {
      return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::Wheel: {
      auto *wheelEvent = static_cast<QWheelEvent *>(event);
      if (wheelEvent->modifiers() & Qt::ControlModifier) {
        return QObject::eventFilter(watched, event);
      }

      const bool wantsHorizontal =
          (wheelEvent->modifiers() & Qt::ShiftModifier);

      if (wantsHorizontal) {
        int delta = wheelScrollDelta(wheelEvent, true);
        if (delta == 0) {
          delta = wheelScrollDelta(wheelEvent, false);
        }
        if (delta != 0) {
          trackView_->setHorizontalOffset(
              std::max(0.0, trackView_->horizontalOffset() - delta));
          event->accept();
          return true;
        }
      }

      int delta = wheelScrollDelta(wheelEvent, false);
      if (delta == 0) {
        delta = wheelScrollDelta(wheelEvent, true);
      }
      if (delta != 0) {
        trackView_->setVerticalOffset(
            std::max(0.0, trackView_->verticalOffset() - delta));
        event->accept();
        return true;
      }

      return QObject::eventFilter(watched, event);
    }
    case QEvent::MouseButtonPress: {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->button() != Qt::MiddleButton) {
        return QObject::eventFilter(watched, event);
      }
      panning_ = true;
      lastGlobalPos_ = mouseEvent->globalPosition().toPoint();
      widget->setCursor(Qt::ClosedHandCursor);
      event->accept();
      return true;
    }
    case QEvent::MouseMove: {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (!panning_ || !(mouseEvent->buttons() & Qt::MiddleButton)) {
        return QObject::eventFilter(watched, event);
      }
      const QPoint currentGlobalPos = mouseEvent->globalPosition().toPoint();
      const QPoint delta = currentGlobalPos - lastGlobalPos_;
      lastGlobalPos_ = currentGlobalPos;
      trackView_->setHorizontalOffset(
          std::max(0.0, trackView_->horizontalOffset() - delta.x()));
      trackView_->setVerticalOffset(
          std::max(0.0, trackView_->verticalOffset() - delta.y()));
      event->accept();
      return true;
    }
    case QEvent::MouseButtonRelease: {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->button() != Qt::MiddleButton) {
        return QObject::eventFilter(watched, event);
      }
      panning_ = false;
      widget->unsetCursor();
      event->accept();
      return true;
    }
    default:
      break;
    }

    return QObject::eventFilter(watched, event);
  }

private:
  ArtifactTimelineTrackPainterView *trackView_ = nullptr;
  bool panning_ = false;
  QPoint lastGlobalPos_;
};

class PanelVerticalScrollFilter final : public QObject {
public:
  explicit PanelVerticalScrollFilter(QScrollBar *verticalBar,
                                     QObject *parent = nullptr)
      : QObject(parent), verticalBar_(verticalBar) {}

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    Q_UNUSED(watched);
    if (!verticalBar_ || event->type() != QEvent::Wheel) {
      return QObject::eventFilter(watched, event);
    }

    auto *wheelEvent = static_cast<QWheelEvent *>(event);
    if ((wheelEvent->modifiers() & Qt::ControlModifier) ||
        (wheelEvent->modifiers() & Qt::ShiftModifier)) {
      return QObject::eventFilter(watched, event);
    }
    if (verticalBar_->maximum() <= verticalBar_->minimum()) {
      return QObject::eventFilter(watched, event);
    }

    int delta = wheelScrollDelta(wheelEvent, false);
    if (delta == 0) {
      delta = wheelScrollDelta(wheelEvent, true);
    }
    if (delta == 0) {
      return QObject::eventFilter(watched, event);
    }

    verticalBar_->setValue(verticalBar_->value() - delta);
    wheelEvent->accept();
    return true;
  }

private:
  QScrollBar *verticalBar_ = nullptr;
};

class LeftHeaderPriorityFilter final : public QObject {
public:
  LeftHeaderPriorityFilter(QWidget *host, QWidget *timecode, QWidget *searchBar,
                           QWidget *switches, QObject *parent = nullptr)
      : QObject(parent), host_(host), timecode_(timecode),
        searchBar_(searchBar), switches_(switches) {
    if (searchBar_) {
      searchPreferredWidth_ =
          std::max(searchBar_->width(), searchBar_->sizeHint().width());
      searchMinimumWidth_ = std::max(96, searchBar_->minimumSizeHint().width());
    }
  }

  void sync() {
    if (!host_ || !timecode_ || !searchBar_ || !switches_) {
      return;
    }

    auto *layout = qobject_cast<QHBoxLayout *>(host_->layout());
    const int spacing = layout ? layout->spacing() : 0;
    const QMargins margins = layout ? layout->contentsMargins() : QMargins();
    const int availableWidth =
        std::max(0, host_->width() - margins.left() - margins.right());

    const int timecodeWidth = std::max(timecode_->minimumSizeHint().width(),
                                       timecode_->sizeHint().width());
    const int switchesWidth = std::max(switches_->minimumSizeHint().width(),
                                       switches_->sizeHint().width());
    const int requiredForSearch = timecodeWidth + spacing + searchMinimumWidth_;
    const int requiredForSwitches = requiredForSearch + spacing + switchesWidth;

    const bool showSearch = availableWidth >= requiredForSearch;
    const bool showSwitches = availableWidth >= requiredForSwitches;

    timecode_->setVisible(true);
    searchBar_->setVisible(showSearch);
    switches_->setVisible(showSwitches);

    if (!showSearch) {
      return;
    }

    const int reservedForSwitches = showSwitches ? spacing + switchesWidth : 0;
    const int maxSearchWidth =
        std::max(searchMinimumWidth_, availableWidth - timecodeWidth - spacing -
                                          reservedForSwitches);
    searchBar_->setFixedWidth(
        std::clamp(maxSearchWidth, searchMinimumWidth_, searchPreferredWidth_));
  }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    Q_UNUSED(watched);
    switch (event->type()) {
    case QEvent::Resize:
    case QEvent::LayoutRequest:
    case QEvent::Show:
      sync();
      break;
    default:
      break;
    }
    return QObject::eventFilter(watched, event);
  }

private:
  QWidget *host_ = nullptr;
  QWidget *timecode_ = nullptr;
  QWidget *searchBar_ = nullptr;
  QWidget *switches_ = nullptr;
  int searchPreferredWidth_ = 190;
  int searchMinimumWidth_ = 96;
};

class ViewportResizeFilter final : public QObject {
public:
  ViewportResizeFilter(QWidget *context, std::function<void()> callback,
                       QObject *parent = nullptr)
      : QObject(parent), context_(context), callback_(std::move(callback)) {}

protected:
  bool eventFilter(QObject *, QEvent *event) override {
    if (event->type() == QEvent::Resize && !updating_) {
      // 同期的にコールバックを呼び出し、次の描画前にズームを確定させる。
      // callback_ がスクロールバーの表示/非表示を変更してビューポートが
      // 再リサイズされた場合、updating_ ガードにより再帰を防止する。
      // フローティングモードではこの再帰が振動ループの原因だったため、
      // 遅延リトライは行わない。
      updating_ = true;
      callback_();
      updating_ = false;
    }
    return false;
  }

private:
  QWidget *context_;
  std::function<void()> callback_;
  bool updating_ = false;
};
} // namespace

// ===== ArtifactTimelineWidget Implementation =====

W_OBJECT_IMPL(ArtifactTimelineWidget)

class ArtifactTimelineWidget::Impl {
private:
public:
  Impl();
  ~Impl();
  ArtifactTimelineBottomLabel *timelineLabel_ = nullptr;
  ArtifactTimelineSearchBarWidget *searchBar_ = nullptr;
  QLabel *searchStatusLabel_ = nullptr;
  QLabel *keyframeStatusLabel_ = nullptr;
  QComboBox *propertyModeCombo_ = nullptr;
  ArtifactLayerTimelinePanelWrapper *layerTimelinePanel_ = nullptr;
  ArtifactTimelineTrackPainterView *painterTrackView_ = nullptr;
  ArtifactTimelineScrubBar *scrubBar_ = nullptr;
  WorkAreaControl *workArea_ = nullptr;
  ArtifactTimelineNavigatorWidget *navigator_ = nullptr;
  CompositionID compositionId_;
  bool shyActive_ = false;
  QString filterText_;
  QVector<LayerID> searchResultLayerIds_;
  int searchResultIndex_ = -1;
  QVector<LayerID> trackLayerIds_;
  bool syncingLayerSelection_ = false;
  double currentFrame_ = 0.0;
  QMetaObject::Connection compositionChangedConnection_;
};

ArtifactTimelineWidget::Impl::Impl() {}

ArtifactTimelineWidget::Impl::~Impl() {}

ArtifactTimelineWidget::ArtifactTimelineWidget(QWidget *parent /*=nullptr*/)
    : QWidget(parent), impl_(new Impl()) {

  setWindowFlags(Qt::FramelessWindowHint);

  setWindowTitle("Timeline");
  setMinimumHeight(500);
  setBaseSize(1200, 500);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  // auto iconView = new ArtifactTimelineIconView();
  // iconView->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  // iconView->setFixedWidth(80);

  auto layerTreeView = new ArtifactLayerTimelinePanelWrapper();
  layerTreeView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  layerTreeView->setPropertyDisplayMode(TimelinePropertyDisplayMode::KeyframesOnly);
  impl_->layerTimelinePanel_ = layerTreeView;

  auto leftSplitter = new DraggableSplitter(Qt::Horizontal);
  // leftSplitter->addWidget(iconView);
  leftSplitter->addWidget(layerTreeView);
  leftSplitter->setStretchFactor(0, 0); // ACR fixed
  leftSplitter->setStretchFactor(1, 1); // Layer Panel flexible
  leftSplitter->setHandleWidth(4);
  {
    const QColor handleColor = palette().mid().color().darker(115);
    leftSplitter->setStyleSheet(QStringLiteral("QSplitter::handle { background: %1; }")
                                    .arg(handleColor.name()));
  }

  auto leftHeader = new ArtifactTimeCodeWidget();             // Timecode
  auto searchBar = new ArtifactTimelineSearchBarWidget();     // Search
  auto searchModeCombo = new QComboBox();                     // Search mode
  auto displayModeCombo = new QComboBox();                    // Layer display mode
  auto propertyModeCombo = new QComboBox();                   // Property display mode
  auto densityCombo = new QComboBox();                        // Row density
  auto globalSwitches = new ArtifactTimelineGlobalSwitches(); // AE Switches
  auto searchStatusLabel = new QLabel();
  auto keyframeStatusLabel = new QLabel();

  impl_->searchBar_ = searchBar;
  impl_->searchStatusLabel_ = searchStatusLabel;
  impl_->keyframeStatusLabel_ = keyframeStatusLabel;
  impl_->propertyModeCombo_ = propertyModeCombo;

  QObject::connect(searchBar, &ArtifactTimelineSearchBarWidget::searchTextChanged,
                   this, &ArtifactTimelineWidget::onSearchTextChanged);
  QObject::connect(searchBar, &ArtifactTimelineSearchBarWidget::searchNextRequested,
                   this, [this]() { jumpToSearchHit(+1); });
  QObject::connect(searchBar, &ArtifactTimelineSearchBarWidget::searchPrevRequested,
                   this, [this]() { jumpToSearchHit(-1); });
  QObject::connect(searchBar, &ArtifactTimelineSearchBarWidget::searchCleared,
                   this, [this]() { onSearchTextChanged(QString()); });
  searchModeCombo->addItem(QStringLiteral("All Visible"), static_cast<int>(SearchMatchMode::AllVisible));
  searchModeCombo->addItem(QStringLiteral("Highlight Only"), static_cast<int>(SearchMatchMode::HighlightOnly));
  searchModeCombo->addItem(QStringLiteral("Filter Only"), static_cast<int>(SearchMatchMode::FilterOnly));
  searchModeCombo->setCurrentIndex(2);
  displayModeCombo->addItem(QStringLiteral("All Layers"), static_cast<int>(TimelineLayerDisplayMode::AllLayers));
  displayModeCombo->addItem(QStringLiteral("Selected"), static_cast<int>(TimelineLayerDisplayMode::SelectedOnly));
  displayModeCombo->addItem(QStringLiteral("Animated"), static_cast<int>(TimelineLayerDisplayMode::AnimatedOnly));
  displayModeCombo->addItem(QStringLiteral("Keyframes + Important"), static_cast<int>(TimelineLayerDisplayMode::ImportantAndKeyframed));
  displayModeCombo->addItem(QStringLiteral("Audio"), static_cast<int>(TimelineLayerDisplayMode::AudioOnly));
  displayModeCombo->addItem(QStringLiteral("Video"), static_cast<int>(TimelineLayerDisplayMode::VideoOnly));
  displayModeCombo->setCurrentIndex(0);
  propertyModeCombo->addItem(QStringLiteral("All Properties"), static_cast<int>(TimelinePropertyDisplayMode::GroupedProperties));
  propertyModeCombo->addItem(QStringLiteral("Keyframes Only"), static_cast<int>(TimelinePropertyDisplayMode::KeyframesOnly));
  propertyModeCombo->setCurrentIndex(1);
  densityCombo->addItem(QStringLiteral("Compact"), 24);
  densityCombo->addItem(QStringLiteral("Normal"), 28);
  densityCombo->addItem(QStringLiteral("Comfortable"), 36);
  densityCombo->setCurrentIndex(1);
  QObject::connect(searchModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                   [this, layerTreeView, searchModeCombo](int index) {
                     if (!layerTreeView || searchModeCombo->count() <= 0) {
                       return;
                     }
                     const int dataIndex = std::clamp(index, 0, searchModeCombo->count() - 1);
                     const auto mode = static_cast<SearchMatchMode>(
                         searchModeCombo->itemData(dataIndex).toInt());
                     layerTreeView->setSearchMatchMode(mode);
                     updateSearchState();
                   });
  QObject::connect(displayModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                   [layerTreeView, displayModeCombo](int index) {
                     if (!layerTreeView || displayModeCombo->count() <= 0) {
                       return;
                     }
                     const int dataIndex = std::clamp(index, 0, displayModeCombo->count() - 1);
                    const auto mode = static_cast<TimelineLayerDisplayMode>(
                        displayModeCombo->itemData(dataIndex).toInt());
                     layerTreeView->setDisplayMode(mode);
                   });
  QObject::connect(propertyModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                   [layerTreeView, propertyModeCombo](int index) {
                     if (!layerTreeView || propertyModeCombo->count() <= 0) {
                       return;
                     }
                     const int dataIndex = std::clamp(index, 0, propertyModeCombo->count() - 1);
                     const auto mode = static_cast<TimelinePropertyDisplayMode>(
                         propertyModeCombo->itemData(dataIndex).toInt());
                     layerTreeView->setPropertyDisplayMode(mode);
                   });
  QObject::connect(densityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                   [layerTreeView, densityCombo](int index) {
                     if (!layerTreeView || densityCombo->count() <= 0) {
                       return;
                     }
                     const int dataIndex = std::clamp(index, 0, densityCombo->count() - 1);
                     const int rowHeight = densityCombo->itemData(dataIndex).toInt();
                     layerTreeView->setRowHeight(rowHeight);
                   });
  leftHeader->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  searchBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  searchBar->setMinimumWidth(96);
  searchBar->setFixedWidth(190);
  searchModeCombo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  searchModeCombo->setMinimumWidth(120);
  displayModeCombo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  displayModeCombo->setMinimumWidth(108);
  propertyModeCombo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  propertyModeCombo->setMinimumWidth(138);
  densityCombo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  densityCombo->setMinimumWidth(98);
  searchStatusLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  searchStatusLabel->setMinimumWidth(90);
  searchStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  {
    QPalette pal = searchStatusLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(169, 183, 198));
    searchStatusLabel->setPalette(pal);
  }
  searchStatusLabel->setText("");
  keyframeStatusLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  keyframeStatusLabel->setMinimumWidth(180);
  keyframeStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  {
    QPalette pal = keyframeStatusLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(168, 214, 255));
    keyframeStatusLabel->setPalette(pal);
  }
  keyframeStatusLabel->setText("");
  globalSwitches->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  globalSwitches->setFixedWidth(globalSwitches->sizeHint().width());

  auto searchBarLayout = new QHBoxLayout();
  searchBarLayout->setSpacing(8);
  searchBarLayout->setContentsMargins(0, 0, 8, 0);
  searchBarLayout->addWidget(leftHeader);
  searchBarLayout->addWidget(searchBar);
  searchBarLayout->addWidget(searchModeCombo);
  searchBarLayout->addWidget(displayModeCombo);
  searchBarLayout->addWidget(propertyModeCombo);
  searchBarLayout->addWidget(densityCombo);
  searchBarLayout->addWidget(searchStatusLabel);
  searchBarLayout->addWidget(keyframeStatusLabel);
  searchBarLayout->addWidget(globalSwitches);
  searchBarLayout->addStretch(1);
  searchBarLayout->setStretch(0, 0);
  searchBarLayout->setStretch(1, 0);
  searchBarLayout->setStretch(2, 0);
  searchBarLayout->setStretch(3, 0);
  searchBarLayout->setStretch(4, 0);
  searchBarLayout->setStretch(5, 0);
  searchBarLayout->setStretch(6, 0);
  searchBarLayout->setStretch(7, 0);
  searchBarLayout->setStretch(8, 0);
  searchBarLayout->setStretch(9, 1);

  QObject::connect(globalSwitches, &ArtifactTimelineGlobalSwitches::shyChanged,
                   this, &ArtifactTimelineWidget::onShyChanged);
  QObject::connect(layerTreeView,
                   &ArtifactLayerTimelinePanelWrapper::visibleRowsChanged, this,
                   [this]() {
                    refreshTracks();
                    updateSearchState();
                   });

  auto headerWidget = new QWidget();
  headerWidget->setLayout(searchBarLayout);
  headerWidget->setFixedHeight(kTimelineHeaderRowHeight);

  auto *leftHeaderPriorityFilter = new LeftHeaderPriorityFilter(
      headerWidget, leftHeader, searchBar, globalSwitches, headerWidget);
  headerWidget->installEventFilter(leftHeaderPriorityFilter);
  leftHeaderPriorityFilter->sync();

  auto leftTopSpacer = new QWidget();
  leftTopSpacer->setFixedHeight(kTimelineTopRowHeight);
  leftTopSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  leftTopSpacer->setAutoFillBackground(true);

  auto leftSubHeaderSpacer = new QWidget();
  leftSubHeaderSpacer->setFixedHeight(0);
  leftSubHeaderSpacer->setSizePolicy(QSizePolicy::Expanding,
                                     QSizePolicy::Fixed);
  leftSubHeaderSpacer->setAutoFillBackground(true);

  auto leftLayout = new QVBoxLayout();
  leftLayout->setSpacing(0);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->addWidget(leftTopSpacer);
  leftLayout->addWidget(headerWidget);
  leftLayout->addWidget(leftSubHeaderSpacer);
  leftLayout->addWidget(leftSplitter);

  auto leftPanel = new QWidget();
  leftPanel->setLayout(leftLayout);

  auto *rightPanelLayout = new QVBoxLayout();
  rightPanelLayout->setSpacing(0);
  rightPanelLayout->setContentsMargins(0, 0, 0, 0);
  auto timeNavigatorWidget = impl_->navigator_ =
      new ArtifactTimelineNavigatorWidget();
  auto workAreaWidget = impl_->workArea_ = new WorkAreaControl();
  auto scrubBar = impl_->scrubBar_ = new ArtifactTimelineScrubBar();

  auto painterTrackView = impl_->painterTrackView_ =
      new ArtifactTimelineTrackPainterView();
  painterTrackView->setDurationFrames(kDefaultTimelineFrames);
  painterTrackView->setTrackCount(1);
  timeNavigatorWidget->setTotalFrames(kDefaultTimelineFrames);
  timeNavigatorWidget->setFixedHeight(kTimelineTopRowHeight);
  timeNavigatorWidget->setSizePolicy(QSizePolicy::Expanding,
                                     QSizePolicy::Fixed);
  scrubBar->setFixedHeight(kTimelineHeaderRowHeight);
  scrubBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  workAreaWidget->setFixedHeight(kTimelineWorkAreaRowHeight);
  workAreaWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  scrubBar->setTotalFrames(kDefaultTimelineFrames);
  scrubBar->setCurrentFrame(FramePosition(0));
  scrubBar->setVisible(true);
  scrubBar->update();

  auto *trackHost = new QWidget();
  auto *trackLayout = new QVBoxLayout();
  trackLayout->setContentsMargins(0, 0, 0, 0);
  trackLayout->setSpacing(0);
  trackLayout->addWidget(painterTrackView);
  trackHost->setLayout(trackLayout);

  auto updateZoom = [this]() {
    if (!impl_->painterTrackView_ || !impl_->workArea_)
      return;
    double duration = impl_->painterTrackView_->durationFrames();
    float s = impl_->navigator_->startValue();
    float e = impl_->navigator_->endValue();
    double range = std::max(0.01f, e - s);

    int viewW = impl_->painterTrackView_->width();
    if (viewW > 0) {
      double newZoom = viewW / (duration * range);
      impl_->painterTrackView_->setPixelsPerFrame(newZoom);
      if (impl_->scrubBar_) {
        impl_->scrubBar_->setRulerPixelsPerFrame(newZoom);
      }

      // ズームレベルをシグナルで通知
      Q_EMIT zoomLevelChanged(newZoom * 100.0);

      const double offset = s * duration * newZoom;
      impl_->painterTrackView_->setHorizontalOffset(offset);
      if (impl_->scrubBar_) {
        impl_->scrubBar_->setRulerHorizontalOffset(offset);
      }
    }
  };

  auto syncPainterSelectionState = [this]() {
    if (!impl_ || !impl_->painterTrackView_) {
      return;
    }

    QSet<LayerID> selectedLayerIds;
    QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> markers;
    if (auto *app = ArtifactApplicationManager::instance()) {
      if (auto *selection = app->layerSelectionManager()) {
        const auto layers = selection->selectedLayers();
        selectedLayerIds.reserve(layers.size());
        for (const auto &layer : layers) {
          if (layer) {
            selectedLayerIds.insert(layer->id());
          }
        }
        const auto composition = safeCompositionLookup(impl_->compositionId_);
        markers = collectKeyframeMarkers(
            composition, selection, buildTrackIndexByLayerId(impl_->trackLayerIds_));
      }
    }
    impl_->painterTrackView_->setSelectedLayerIds(selectedLayerIds);
    impl_->painterTrackView_->setKeyframeMarkers(markers);
    updateKeyframeState();
  };

  syncWorkAreaFromCurrentComposition();

  QObject::connect(timeNavigatorWidget,
                   &ArtifactTimelineNavigatorWidget::startChanged, this,
                   updateZoom);
  QObject::connect(timeNavigatorWidget,
                   &ArtifactTimelineNavigatorWidget::endChanged, this,
                   updateZoom);
  QObject::connect(
      workAreaWidget, &WorkAreaControl::startChanged, this,
      [this, workAreaWidget](float) {
        const auto comp = safeCompositionLookup(impl_->compositionId_);
        if (!comp) {
          return;
        }
        const int64_t totalFrames =
            std::max<int64_t>(1, comp->frameRange().duration());
        const int64_t startFrame = std::clamp<int64_t>(
            static_cast<int64_t>(
                std::llround(workAreaWidget->startValue() * totalFrames)),
            0, totalFrames);
        const int64_t endFrame =
            std::clamp<int64_t>(static_cast<int64_t>(std::llround(
                                    workAreaWidget->endValue() * totalFrames)),
                                startFrame, totalFrames);
        comp->setWorkAreaRange(FrameRange(
            startFrame, std::max<int64_t>(startFrame + 1, endFrame)));
      });
  QObject::connect(
      workAreaWidget, &WorkAreaControl::endChanged, this,
      [this, workAreaWidget](float) {
        const auto comp = safeCompositionLookup(impl_->compositionId_);
        if (!comp) {
          return;
        }
        const int64_t totalFrames =
            std::max<int64_t>(1, comp->frameRange().duration());
        const int64_t startFrame = std::clamp<int64_t>(
            static_cast<int64_t>(
                std::llround(workAreaWidget->startValue() * totalFrames)),
            0, totalFrames);
        const int64_t endFrame =
            std::clamp<int64_t>(static_cast<int64_t>(std::llround(
                                    workAreaWidget->endValue() * totalFrames)),
                                startFrame, totalFrames);
        comp->setWorkAreaRange(FrameRange(
            startFrame, std::max<int64_t>(startFrame + 1, endFrame)));
      });
  QObject::connect(scrubBar, &ArtifactTimelineScrubBar::frameChanged, this,
                   [painterTrackView](const auto &frame) {
                     painterTrackView->setCurrentFrame(
                         static_cast<double>(frame.framePosition()));
                     if (auto *app = ArtifactApplicationManager::instance()) {
                       if (auto *ctx = app->activeContextService()) {
                         ctx->seekToFrame(frame.framePosition());
                         return;
                       }
                     }
                     if (auto *playback = ArtifactPlaybackService::instance()) {
                       playback->goToFrame(frame);
                     }
                   });
  QObject::connect(
      painterTrackView, &ArtifactTimelineTrackPainterView::seekRequested, this,
      [this, scrubBar](double frame) {
        const int clampedFrame =
            std::clamp(static_cast<int>(std::llround(frame)), 0,
                       std::max(0, scrubBar->totalFrames() - 1));
        scrubBar->setCurrentFrame(FramePosition(clampedFrame));
      });
  QObject::connect(
      painterTrackView, &ArtifactTimelineTrackPainterView::clipSelected, this,
      [this](const QString& clipId, const ArtifactCore::LayerID& layerId) {
        Q_UNUSED(clipId);
        if (impl_->syncingLayerSelection_) {
          return;
        }
        if (auto* svc = ArtifactProjectService::instance()) {
          svc->selectLayer(layerId);
        }
      });
  QObject::connect(
      painterTrackView, &ArtifactTimelineTrackPainterView::clipDeselected, this,
      [this]() {
        if (impl_->syncingLayerSelection_) {
          return;
        }
        if (auto* svc = ArtifactProjectService::instance()) {
          svc->selectLayer(LayerID());
        }
      });
  QObject::connect(
      painterTrackView, &ArtifactTimelineTrackPainterView::clipMoved, this,
      [this](const QString &clipId, const double startFrame) {
        applyTimelineLayerMove(impl_->compositionId_, clipId, startFrame, 0.0);
      });
  QObject::connect(
      painterTrackView, &ArtifactTimelineTrackPainterView::clipResized, this,
      [this](const QString &clipId, const double startFrame,
             const double durationFrame) {
        applyTimelineLayerTrim(impl_->compositionId_, clipId, startFrame,
                               durationFrame);
      });
  QObject::connect(
      painterTrackView, &ArtifactTimelineTrackPainterView::keyframeMoveRequested, this,
      [this](const ArtifactCore::LayerID &layerId, const QString &propertyPath,
             const qint64 fromFrame, const qint64 toFrame) {
        const ArtifactCompositionPtr composition =
            safeCompositionLookup(impl_->compositionId_);
        if (!composition) {
          return;
        }
        if (!moveKeyframeAtFrame(composition, layerId, propertyPath, fromFrame, toFrame)) {
          return;
        }
        if (auto *svc = ArtifactProjectService::instance()) {
          svc->projectChanged();
        }
        refreshTracks();
      });
  QObject::connect(painterTrackView, &ArtifactTimelineTrackPainterView::timelineDebugMessage, this, &ArtifactTimelineWidget::timelineDebugMessage);
  // auto layerTimelinePanel = new ArtifactLayerTimelinePanelWrapper();
  // layerTimelinePanel->setMinimumWidth(220);
  // layerTimelinePanel->setMaximumWidth(320);

  auto rightPanel = new QWidget();
  rightPanelLayout->addWidget(timeNavigatorWidget);
  rightPanelLayout->addWidget(scrubBar);
  rightPanelLayout->addWidget(workAreaWidget);
  rightPanelLayout->addWidget(trackHost);
  rightPanel->setLayout(rightPanelLayout);

  auto *headerSeekFilter =
      new HeaderSeekFilter(painterTrackView, scrubBar, rightPanel);
  headerSeekFilter->setDebugCallback([this](const QString &msg) {
    Q_EMIT timelineDebugMessage(msg);
  });
  timeNavigatorWidget->installEventFilter(headerSeekFilter);
  workAreaWidget->installEventFilter(headerSeekFilter);
  scrubBar->installEventFilter(headerSeekFilter); // Install on scrub bar

  auto *headerScrollFilter =
      new HeaderScrollFilter(painterTrackView, rightPanel);
  rightPanel->installEventFilter(headerScrollFilter);
  timeNavigatorWidget->installEventFilter(headerScrollFilter);
  scrubBar->installEventFilter(headerScrollFilter);
  workAreaWidget->installEventFilter(headerScrollFilter);

  if (painterTrackView) {
    auto *viewResizeFilter =
        new ViewportResizeFilter(rightPanel, updateZoom, rightPanel);
    painterTrackView->installEventFilter(viewResizeFilter);
  }

  QObject::connect(scrubBar, &ArtifactTimelineScrubBar::frameChanged, this,
                   [this](const auto &frame) {
                     impl_->currentFrame_ = static_cast<double>(frame.framePosition());
                     // Emit debug message for any playhead change (playback, scroll, scrub)
                     Q_EMIT timelineDebugMessage(QStringLiteral("Playhead: %1").arg(frame.framePosition()));
                     if (impl_->painterTrackView_) {
                       impl_->painterTrackView_->setCurrentFrame(
                           static_cast<double>(frame.framePosition()));
                     }
                     updateKeyframeState();
                   });
  if (auto *playbackService = ArtifactPlaybackService::instance()) {
  QObject::connect(
      playbackService, &ArtifactPlaybackService::frameChanged, this,
        [this, scrubBar,
         playbackService](const FramePosition &frame) {
          const auto currentComp = playbackService->currentComposition();
          if (!currentComp || currentComp->id() != impl_->compositionId_) {
            return;
          }

          if (impl_->painterTrackView_) {
            impl_->painterTrackView_->setCurrentFrame(
                static_cast<double>(frame.framePosition()));
          }
          impl_->currentFrame_ = static_cast<double>(frame.framePosition());
          const QSignalBlocker blocker(scrubBar);
          scrubBar->setCurrentFrame(frame);
          // 再生中は毎フレーム全レイヤーをスキャンするコストを避けるため15フレームに1回
          if (frame.framePosition() % 15 == 0) {
            updateKeyframeState();
          }
        });
  }
  QTimer::singleShot(0, this, [updateZoom]() { updateZoom(); });

  // Ŝ̃^CCXvb^[
  auto mainSplitter = new QSplitter(Qt::Horizontal);
  {
    const QColor handleColor = palette().mid().color().darker(115);
    mainSplitter->setStyleSheet(QStringLiteral("QSplitter::handle { background: %1; }")
                                    .arg(handleColor.name()));
  }
  mainSplitter->setHandleWidth(4);
  mainSplitter->addWidget(leftPanel);
  mainSplitter->addWidget(rightPanel);
  mainSplitter->setChildrenCollapsible(false);
  leftPanel->setMinimumWidth(360);
  rightPanel->setMinimumWidth(480);
  // mainSplitter->addWidget(leftSplitter);

  mainSplitter->setStretchFactor(0, 3);
  mainSplitter->setStretchFactor(1, 5);
  mainSplitter->setSizes({520, 700});

  auto label = new ArtifactTimelineBottomLabel();

  auto layout = new QVBoxLayout();
  layout->addWidget(mainSplitter);
  layout->addWidget(label);
  layout->setSpacing(0);
  layout->setContentsMargins(0, 0, 0, 0);

  setLayout(layout);

  // Sync scrollbars
  if (auto *leftScroll = layerTreeView->verticalScrollBar()) {
    auto *leftPanelWheelFilter =
        new PanelVerticalScrollFilter(leftScroll, leftPanel);
    leftPanel->installEventFilter(leftPanelWheelFilter);
    headerWidget->installEventFilter(leftPanelWheelFilter);
    leftTopSpacer->installEventFilter(leftPanelWheelFilter);
    leftSubHeaderSpacer->installEventFilter(leftPanelWheelFilter);

    Q_UNUSED(leftScroll);
  }

  // Connect Layer Signals
  if (auto *svc = ArtifactProjectService::instance()) {
    QObject::connect(svc, &ArtifactProjectService::layerCreated, this,
                     &ArtifactTimelineWidget::onLayerCreated);
    QObject::connect(svc, &ArtifactProjectService::layerRemoved, this,
                     &ArtifactTimelineWidget::onLayerRemoved);
    QObject::connect(svc, &ArtifactProjectService::layerSelected, this,
                     [this, syncPainterSelectionState](const LayerID &layerId) {
                       Q_UNUSED(layerId);
                       if (!impl_ || !impl_->painterTrackView_) {
                         return;
                       }
                       syncPainterSelectionState();
                     });
    if (auto *app = ArtifactApplicationManager::instance()) {
      if (auto *selection = app->layerSelectionManager()) {
        QObject::connect(
            selection, &ArtifactLayerSelectionManager::selectionChanged, this,
            [this, syncPainterSelectionState]() {
              if (!impl_) {
                return;
              }
              QMetaObject::invokeMethod(
                  this,
                  [syncPainterSelectionState]() { syncPainterSelectionState(); },
                  Qt::QueuedConnection);
            });
      }
    }
    QObject::connect(svc, &ArtifactProjectService::projectChanged, this,
                     [this]() {
                       QMetaObject::invokeMethod(
                           this,
                           [this]() {
                             if (!impl_ || !impl_->painterTrackView_) {
                               return;
                             }
                             refreshTracks();
                           },
                           Qt::QueuedConnection);
                     });
  }
}

ArtifactTimelineWidget::~ArtifactTimelineWidget() {
  delete impl_;
}

void ArtifactTimelineWidget::setComposition(const CompositionID &id) {
  impl_->compositionId_ = id;
  if (impl_->layerTimelinePanel_) {
    impl_->layerTimelinePanel_->setComposition(id);
  }

  if (impl_->painterTrackView_) {
    if (auto svc = ArtifactProjectService::instance()) {
      svc->changeCurrentComposition(id);
      auto res = svc->findComposition(id);
      if (res.success && !res.ptr.expired()) {
        auto comp = res.ptr.lock();
        
        // Listen to composition changes (e.g. layer additions, timeline range updates)
        if (impl_->compositionChangedConnection_) {
          disconnect(impl_->compositionChangedConnection_);
        }
        impl_->compositionChangedConnection_ = connect(comp.get(), &ArtifactAbstractComposition::changed, this, [this]() {
          QMetaObject::invokeMethod(this, [this]() {
            if (!impl_) {
              return;
            }
            syncWorkAreaFromCurrentComposition();
            refreshTracks();
          }, Qt::QueuedConnection);
        });

        const QString compositionLabel =
            comp->settings().compositionName().toQString().trimmed();
        setWindowTitle(compositionLabel.isEmpty() ? id.toString()
                                                  : compositionLabel);
        if (auto *app = ArtifactApplicationManager::instance()) {
          if (auto *ctx = app->activeContextService()) {
            ctx->setActiveComposition(comp);
          }
          if (auto *selectionManager = app->layerSelectionManager()) {
            selectionManager->setActiveComposition(comp);
          }
        }
        const int totalFrames = static_cast<int>(
            timelineCompositionFrameCount(comp, kDefaultTimelineFrames));
        if (auto *playbackService = ArtifactPlaybackService::instance()) {
          playbackService->setCurrentComposition(comp);
        }
        impl_->painterTrackView_->setDurationFrames(static_cast<double>(totalFrames));
        syncWorkAreaFromCurrentComposition();
        if (impl_->scrubBar_) {
          impl_->scrubBar_->setTotalFrames(std::max(1, totalFrames));
          impl_->scrubBar_->setCurrentFrame(FramePosition(0));
          // FPS をスクラブバーに反映
          if (comp) {
            impl_->scrubBar_->setFps(static_cast<int>(comp->frameRate().framerate()));
          }
        }
        if (impl_->navigator_) {
          impl_->navigator_->setTotalFrames(std::max(1, totalFrames));
        }
        impl_->painterTrackView_->setCurrentFrame(0.0);
        impl_->currentFrame_ = 0.0;
        if (auto *app = ArtifactApplicationManager::instance()) {
          if (auto *ctx = app->activeContextService()) {
            ctx->seekToFrame(0);
          }
        }
        QTimer::singleShot(0, this, [this]() {
          if (!impl_ || !impl_->navigator_ || !impl_->painterTrackView_) {
            return;
          }
          const double duration = impl_->painterTrackView_->durationFrames();
          const double range = std::max(
              0.01, static_cast<double>(impl_->navigator_->endValue() -
                                        impl_->navigator_->startValue()));
          const int viewW = impl_->painterTrackView_->width();
          if (viewW <= 0) {
            return;
          }
          const double newZoom = viewW / (duration * range);
          impl_->painterTrackView_->setPixelsPerFrame(newZoom);
          const double offset =
              impl_->navigator_->startValue() * duration * newZoom;
          impl_->painterTrackView_->setHorizontalOffset(offset);
          if (impl_->scrubBar_) {
            impl_->scrubBar_->setRulerPixelsPerFrame(newZoom);
            impl_->scrubBar_->setRulerHorizontalOffset(offset);
          }
          updateKeyframeState();
        });

        refreshTracks();
      }
    }
  }
  updateSearchState();
}

void ArtifactTimelineWidget::onLayerCreated(const CompositionID &compId,
                                            const LayerID &lid) {
  if (compId != impl_->compositionId_)
    return;
  if (!impl_->painterTrackView_)
    return;

  qDebug() << "[ArtifactTimelineWidget::onLayerCreated] Layer created:"
           << lid.toString();
  refreshTracks();
}

void ArtifactTimelineWidget::onLayerRemoved(const CompositionID &compId,
                                            const LayerID &lid) {
  if (compId != impl_->compositionId_)
    return;
  qDebug() << "[ArtifactTimelineWidget::onLayerRemoved] Layer removed:"
           << lid.toString();
  refreshTracks();
}

void ArtifactTimelineWidget::onShyChanged(bool active) {
  impl_->shyActive_ = active;
  if (impl_->layerTimelinePanel_) {
    refreshTracks();
  }
}

void ArtifactTimelineWidget::refreshTracks() {
  if (!impl_->painterTrackView_)
    return;
  if (impl_->painterTrackView_) {
    impl_->painterTrackView_->clearClips();
  }

  QVector<LayerID> visibleRows;
  if (impl_->layerTimelinePanel_) {
    visibleRows = impl_->layerTimelinePanel_->visibleTimelineRows();
  }

  if (!impl_->layerTimelinePanel_) {
    auto comp = safeCompositionLookup(impl_->compositionId_);
    if (!comp)
      return;

    auto layers = comp->allLayer();
    std::reverse(layers.begin(), layers.end());
    for (auto &layer : layers) {
      if (!layer)
        continue;
      if (impl_->shyActive_ && layer->isShy())
        continue;
      visibleRows.push_back(layer->id());
    }
  }

  QVector<LayerID> timelineRows;
  timelineRows.reserve(visibleRows.size());
  std::unordered_set<std::string> seenLayerIds;
  for (const auto &rowLayerId : visibleRows) {
    if (rowLayerId.isNil()) {
      timelineRows.push_back(rowLayerId);
      continue;
    }

    const std::string layerKey = rowLayerId.toString().toStdString();
    if (!seenLayerIds.insert(layerKey).second) {
      continue;
    }

    timelineRows.push_back(rowLayerId);
  }

  impl_->trackLayerIds_ = timelineRows;
  const auto composition = safeCompositionLookup(impl_->compositionId_);
  QVector<ArtifactTimelineTrackPainterView::TrackClipVisual> painterClips;
  painterClips.reserve(timelineRows.size());
  QHash<LayerID, int> trackIndexByLayerId;
  ArtifactLayerSelectionManager *selectionManager = nullptr;
  if (auto *app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }
  const double fps = composition
                         ? std::max(1.0, static_cast<double>(composition->frameRate().framerate()))
                         : 30.0;
  if (impl_->painterTrackView_) {
    impl_->painterTrackView_->setTrackCount(
        std::max(1, static_cast<int>(timelineRows.size())));
  }
  for (int rowIndex = 0; rowIndex < timelineRows.size(); ++rowIndex) {
    const auto &rowLayerId = timelineRows[rowIndex];
    const int trackIndex = rowIndex;
    if (!rowLayerId.isNil()) {
      trackIndexByLayerId.insert(rowLayerId, trackIndex);
    }
    if (impl_->painterTrackView_) {
      impl_->painterTrackView_->setTrackHeight(
          trackIndex, static_cast<int>(kTimelineRowHeight));
    }
    if (rowLayerId.isNil()) {
      continue;
    }

    const auto layer =
        composition ? composition->layerById(rowLayerId) : nullptr;
    if (!layer) {
      continue;
    }

    const double clipStart =
        static_cast<double>(layer->inPoint().framePosition());
    const double clipDuration =
        std::max(1.0, static_cast<double>(layer->outPoint().framePosition() -
                                          layer->inPoint().framePosition()));
    if (impl_->painterTrackView_) {
      ArtifactTimelineTrackPainterView::TrackClipVisual visual;
      visual.clipId = rowLayerId.toString();
      visual.layerId = rowLayerId;
      visual.trackIndex = trackIndex;
      visual.startFrame = clipStart;
      visual.durationFrame = clipDuration;
      visual.title = layer->layerName();
      visual.fillColor = layerTimelineColor(layer);
      visual.maxDurationFrame = layerTimelineMaxDurationFrames(layer, fps);
      visual.resizeEnabled =
          (visual.maxDurationFrame <= 0.0) ||
          (std::abs(visual.durationFrame - visual.maxDurationFrame) > 0.001);
      if (selectionManager) {
        visual.selected = selectionManager->isSelected(layer);
      }
      painterClips.push_back(std::move(visual));
    }
  }

  if (impl_->painterTrackView_) {
    impl_->painterTrackView_->setDurationFrames(
        std::max(1.0, static_cast<double>(
            safeCompositionLookup(impl_->compositionId_)
                ? safeCompositionLookup(impl_->compositionId_)->frameRange().duration()
                : kDefaultTimelineFrames)));
    impl_->painterTrackView_->setCurrentFrame(impl_->currentFrame_);
    impl_->painterTrackView_->setClips(painterClips);
    impl_->painterTrackView_->setKeyframeMarkers(
        collectKeyframeMarkers(composition, selectionManager, trackIndexByLayerId));
  }

  if (impl_->painterTrackView_) {
    impl_->painterTrackView_->update();
  }
  updateKeyframeState();
}

void ArtifactTimelineWidget::setLayerNameEditable(bool enabled) {
  if (impl_->layerTimelinePanel_) {
    impl_->layerTimelinePanel_->setLayerNameEditable(enabled);
  }
}

bool ArtifactTimelineWidget::isLayerNameEditable() const {
  return impl_->layerTimelinePanel_ ? impl_->layerTimelinePanel_->isLayerNameEditable() : false;
}

void ArtifactTimelineWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QRect bounds = rect();
  const QColor base = palette().color(QPalette::Window);
  const QColor border = palette().color(QPalette::Mid).darker(115);
  const QColor topShade = palette().color(QPalette::Shadow);

  painter.fillRect(bounds, base);

  // Provide a subtle shell so the timeline reads as one owner-drawn surface.
  painter.setPen(QPen(border, 1));
  painter.drawRect(bounds.adjusted(0, 0, -1, -1));

  QColor accent = topShade;
  accent.setAlpha(90);
  painter.fillRect(QRect(bounds.left(), bounds.top(), bounds.width(), 1),
                   accent);
}

void ArtifactTimelineWidget::mousePressEvent(QMouseEvent *event) {
  if (impl_ && impl_->painterTrackView_ && event &&
      event->button() == Qt::LeftButton) {
    impl_->painterTrackView_->setFocus(Qt::MouseFocusReason);
  }
  QWidget::mousePressEvent(event);
}

void ArtifactTimelineWidget::mouseMoveEvent(QMouseEvent *event) {
  QWidget::mouseMoveEvent(event);
}

void ArtifactTimelineWidget::wheelEvent(QWheelEvent *event) {
  // ホイールでシーク (Ctrl+ホイールで ±10)
  const int delta = event->angleDelta().y();
  if (delta == 0) { event->ignore(); return; }

  const int step = (event->modifiers() & Qt::ControlModifier) ? 10 : 1;
  const int direction = (delta > 0) ? 1 : -1;
  const int frameDelta = direction * step;

  if (impl_ && impl_->painterTrackView_) {
    double newPos = impl_->painterTrackView_->currentFrame() + frameDelta;
    impl_->painterTrackView_->setCurrentFrame(std::max(0.0, newPos));
    impl_->painterTrackView_->seekRequested(
        std::clamp(newPos, 0.0, impl_->painterTrackView_->durationFrames()));
  }
  event->accept();
}

void ArtifactTimelineWidget::syncWorkAreaFromCurrentComposition() {
  if (!impl_ || !impl_->workArea_) {
    return;
  }

  const auto comp = safeCompositionLookup(impl_->compositionId_);
  const double totalFrames = static_cast<double>(
      timelineCompositionFrameCount(comp, kDefaultTimelineFrames));
  const FrameRange workArea =
      comp ? comp->workAreaRange()
           : FrameRange(0, static_cast<int64_t>(totalFrames));
  const double startNorm =
      std::clamp(static_cast<double>(workArea.start()) / totalFrames, 0.0, 1.0);
  const double endNorm =
      std::clamp(static_cast<double>(workArea.end()) / totalFrames, startNorm, 1.0);
  QSignalBlocker blocker(impl_->workArea_);
  impl_->workArea_->setStart(static_cast<float>(startNorm));
  impl_->workArea_->setEnd(static_cast<float>(endNorm));
}

void ArtifactTimelineWidget::keyPressEvent(QKeyEvent *event) {
  if (event && (event->modifiers() & Qt::ControlModifier) &&
      (event->key() == Qt::Key_PageUp || event->key() == Qt::Key_PageDown)) {
    jumpToKeyframeHit(event->key() == Qt::Key_PageUp ? -1 : +1);
    event->accept();
    return;
  }

  // J/K/L シャトル操作
  if (event->key() == Qt::Key_J || event->key() == Qt::Key_K || event->key() == Qt::Key_L) {
    if (auto *svc = ArtifactPlaybackService::instance()) {
      if (event->key() == Qt::Key_K) {
        svc->setPlaybackSpeed(0.0f);
        svc->stop();
      } else if (event->key() == Qt::Key_L) {
        float spd = svc->playbackSpeed();
        if (spd >= 0.0f && spd < 8.0f) {
          svc->setPlaybackSpeed(spd <= 0.0f ? 1.0f : spd * 2.0f);
        }
        svc->play();
      } else if (event->key() == Qt::Key_J) {
        float spd = svc->playbackSpeed();
        if (spd <= 0.0f && spd > -8.0f) {
          svc->setPlaybackSpeed(spd >= 0.0f ? -1.0f : spd * 2.0f);
        } else {
          svc->setPlaybackSpeed(-1.0f);
        }
        svc->play();
      }
    }
    event->accept();
    return;
  }

  // I / O キーでワークエリアの IN / OUT を設定
  if (event->key() == Qt::Key_I || event->key() == Qt::Key_O) {
    auto *svc = ArtifactProjectService::instance();
    auto comp = svc ? svc->currentComposition().lock() : nullptr;
    if (comp) {
      const int64_t currentFrame =
          static_cast<int64_t>(impl_->painterTrackView_->currentFrame());

      if (event->key() == Qt::Key_I) {
        // IN ポイントを現在フレームに設定
        const int64_t outPoint = comp->workAreaRange().end();
        comp->setWorkAreaRange(
            FrameRange(currentFrame, std::max(currentFrame + 1, outPoint)));
      } else if (event->key() == Qt::Key_O) {
        // OUT ポイントを現在フレームに設定
        const int64_t inPoint = comp->workAreaRange().start();
        comp->setWorkAreaRange(FrameRange(std::min(inPoint, currentFrame),
                                          std::max(currentFrame + 1, inPoint)));
      }
      event->accept();
      return;
    }
  }

  // スペースキーで再生/一時停止
  if (event->key() == Qt::Key_U) {
    if (impl_ && impl_->propertyModeCombo_ && impl_->propertyModeCombo_->count() >= 2) {
      const int current = impl_->propertyModeCombo_->currentIndex();
      const int next = (current == 0) ? 1 : 0;
      impl_->propertyModeCombo_->setCurrentIndex(next);
      event->accept();
      return;
    }
  }
  if (event->key() == Qt::Key_Space) {
    if (auto *svc = ArtifactPlaybackService::instance()) {
      svc->togglePlayPause();
    }
    event->accept();
    return;
  }

  // Home / End キーで最初/最後のフレームへ
  if (event->key() == Qt::Key_Home || event->key() == Qt::Key_End) {
    auto *svc = ArtifactProjectService::instance();
    auto comp = svc ? svc->currentComposition().lock() : nullptr;
    if (comp) {
      if (event->key() == Qt::Key_Home) {
        comp->goToStartFrame();
      } else if (event->key() == Qt::Key_End) {
        comp->goToEndFrame();
      }
      event->accept();
      return;
    }
  }

  // [ / ] キーでレイヤーの移動またはトリミング
  if (event->key() == Qt::Key_BracketLeft || event->key() == Qt::Key_BracketRight) {
    auto *svc = ArtifactProjectService::instance();
    auto comp = svc ? svc->currentComposition().lock() : nullptr;
    auto *selManager = ArtifactApplicationManager::instance()->layerSelectionManager();
    if (comp && selManager) {
      const int64_t currentFrame = static_cast<int64_t>(std::round(
          impl_->painterTrackView_ ? impl_->painterTrackView_->currentFrame()
                                   : impl_->currentFrame_));
      auto selectedLayers = selManager->selectedLayers();
      
      bool changed = false;
      const bool isAlt = event->modifiers() & Qt::AltModifier;

      for (auto& layer : selectedLayers) {
        if (!layer) continue;

        if (event->key() == Qt::Key_BracketLeft) {
          if (isAlt) {
            // Alt + [ : Trim In (端を削る)
            changed |= applyTimelineLayerTrim(impl_->compositionId_, layer->id().toString(), currentFrame,
                                              layer->outPoint().framePosition() - currentFrame);
          } else {
            // [ : Move In (開始位置を移動)
            changed |= applyTimelineLayerMove(impl_->compositionId_, layer->id().toString(), currentFrame, 0.0);
          }
        } else if (event->key() == Qt::Key_BracketRight) {
          if (isAlt) {
            // Alt + ] : Trim Out
            changed |= applyTimelineLayerTrim(impl_->compositionId_, layer->id().toString(),
                                              layer->inPoint().framePosition(),
                                              currentFrame - layer->inPoint().framePosition());
          } else {
            // ] : Move Out (末尾を現在位置に合わせる)
            const int64_t duration = layer->outPoint().framePosition() - layer->inPoint().framePosition();
            changed |= applyTimelineLayerMove(impl_->compositionId_, layer->id().toString(),
                                              currentFrame - duration, 0.0);
          }
        }
      }

      if (changed) {
        svc->projectChanged();
      }
      event->accept();
      return;
    }
  }

  QWidget::keyPressEvent(event);
}

void ArtifactTimelineWidget::keyReleaseEvent(QKeyEvent *event) {
  if (!event || event->isAutoRepeat()) {
    return;
  }

  if (event->key() == Qt::Key_J || event->key() == Qt::Key_L) {
    if (auto *svc = ArtifactPlaybackService::instance()) {
      svc->setPlaybackSpeed(0.0f);
      svc->stop();
    }
    event->accept();
    return;
  }

  QWidget::keyReleaseEvent(event);
}


class ArtifactTimelineIconView::Impl {
private:
public:
  Impl();
  ~Impl();
};

ArtifactTimelineIconView::Impl::Impl() {}

ArtifactTimelineIconView::Impl::~Impl() {}

ArtifactTimelineIconView::ArtifactTimelineIconView(
    QWidget *parent /*= nullptr*/)
    : QTreeView(parent) {
  // setHeaderHidden(true); // optional compact header mode
  setColumnWidth(0, 16); // ACR
  setColumnWidth(1, 16);
  setColumnWidth(2, 16);
  setColumnWidth(3, 16);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  int iconW = 24;
  header()->setIconSize(QSize(iconW, iconW));

  auto model = new ArtifactTimelineIconModel();

  setModel(model);
  header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  header()->resizeSection(0, 20);
  header()->setStretchLastSection(false);
}

ArtifactTimelineIconView::~ArtifactTimelineIconView() {}

 void ArtifactTimelineWidget::onSearchTextChanged(const QString& text)
 {
  impl_->filterText_ = text;
  if (impl_->layerTimelinePanel_) {
   impl_->layerTimelinePanel_->setFilterText(text);
  } else {
   refreshTracks();
  }
  updateSearchState();
 }

 void ArtifactTimelineWidget::updateSearchState()
 {
  if (!impl_) {
   return;
  }

  impl_->searchResultLayerIds_.clear();
  impl_->searchResultIndex_ = -1;

  const QString query = impl_->filterText_.trimmed();
  if (!query.isEmpty() && impl_->layerTimelinePanel_) {
   const auto rows = impl_->layerTimelinePanel_->matchingTimelineRows();
   for (const auto& layerId : rows) {
    if (!layerId.isNil()) {
     impl_->searchResultLayerIds_.push_back(layerId);
    }
   }
   ArtifactAbstractLayerPtr currentLayer;
   if (auto *app = ArtifactApplicationManager::instance()) {
    if (auto *selection = app->layerSelectionManager()) {
     currentLayer = selection->currentLayer();
    }
   }
   if (currentLayer) {
    impl_->searchResultIndex_ = impl_->searchResultLayerIds_.indexOf(currentLayer->id());
   }
   if (impl_->searchStatusLabel_) {
    impl_->searchStatusLabel_->setText(QStringLiteral("%1 hits").arg(impl_->searchResultLayerIds_.size()));
   }
  } else if (impl_->searchStatusLabel_) {
   impl_->searchStatusLabel_->clear();
  }

  updateKeyframeState();
 }

void ArtifactTimelineWidget::updateKeyframeState()
{
  if (!impl_ || !impl_->keyframeStatusLabel_) {
    return;
  }

  ArtifactCompositionPtr composition;
  if (auto *svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }

  ArtifactLayerSelectionManager *selectionManager = nullptr;
  if (auto *app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }

  const auto state = collectKeyframeNavigationState(
      composition, selectionManager,
      static_cast<qint64>(std::llround(std::max(0.0, impl_->currentFrame_))));
  impl_->keyframeStatusLabel_->setText(formatKeyframeNavigationText(state));
}

 void ArtifactTimelineWidget::jumpToSearchHit(int step)
 {
  if (!impl_ || step == 0) {
    return;
  }

  if (impl_->searchResultLayerIds_.isEmpty()) {
   updateSearchState();
  }
  if (impl_->searchResultLayerIds_.isEmpty()) {
    return;
  }

  int currentIndex = -1;
  ArtifactAbstractLayerPtr currentLayer;
  if (auto *app = ArtifactApplicationManager::instance()) {
   if (auto *selection = app->layerSelectionManager()) {
    currentLayer = selection->currentLayer();
   }
  }
  if (currentLayer) {
   currentIndex = impl_->searchResultLayerIds_.indexOf(currentLayer->id());
  }
  if (currentIndex < 0 && impl_->searchResultIndex_ >= 0 &&
      impl_->searchResultIndex_ < impl_->searchResultLayerIds_.size()) {
   currentIndex = impl_->searchResultIndex_;
  }
  if (currentIndex < 0) {
   currentIndex = step > 0 ? -1 : 0;
  }

  const int count = impl_->searchResultLayerIds_.size();
  const int nextIndex = (currentIndex + step + count) % count;
  const LayerID nextLayerId = impl_->searchResultLayerIds_.value(nextIndex);
  if (nextLayerId.isNil()) {
   return;
  }

  impl_->searchResultIndex_ = nextIndex;
  if (auto* svc = ArtifactProjectService::instance()) {
   svc->selectLayer(nextLayerId);
  }
 }

void ArtifactTimelineWidget::jumpToKeyframeHit(int step)
{
  if (!impl_ || step == 0) {
    return;
  }

  ArtifactCompositionPtr composition;
  if (auto *svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return;
  }

  ArtifactLayerSelectionManager *selectionManager = nullptr;
  if (auto *app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }

  const auto frames = collectSelectedKeyframeFrames(composition, selectionManager);
  if (frames.isEmpty()) {
    return;
  }

  const qint64 currentFrame =
      static_cast<qint64>(std::llround(std::max(0.0, impl_->currentFrame_)));
  int currentIndex = -1;
  for (int i = 0; i < frames.size(); ++i) {
    if (frames[i] == currentFrame) {
      currentIndex = i;
      break;
    }
    if (frames[i] < currentFrame) {
      currentIndex = i;
    }
  }
  const int count = frames.size();
  if (currentIndex < 0) {
    currentIndex = step > 0 ? -1 : count;
  }

  const int nextIndex = (currentIndex + step + count) % count;
  const qint64 targetFrame = frames.value(nextIndex, currentFrame);
  if (targetFrame < 0) {
    return;
  }

  impl_->currentFrame_ = static_cast<double>(targetFrame);
  if (impl_->painterTrackView_) {
    impl_->painterTrackView_->setCurrentFrame(static_cast<double>(targetFrame));
  }
  if (impl_->scrubBar_) {
    impl_->scrubBar_->setCurrentFrame(FramePosition(static_cast<int>(targetFrame)));
  }
  updateKeyframeState();
}

}; // namespace Artifact

