module;

#include <QBoxLayout>
#include <QBrush>
#include <QComboBox>
#include <QHash>
#include <QEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QKeySequence>
#include <QShortcut>
#include <QStandardItem>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>
#include <QPaintEvent>
#include <QPointer>
#include <QPolygonF>
#include <limits>
#include <qtmetamacros.h>
#include <wobjectdefs.h>
#include <wobjectimpl.h>

module Artifact.Widgets.Timeline;

import std;

import Widgets.Utils.CSS;

import Artifact.Layers.Hierarchy.Model;
import Artifact.Widget.WorkAreaControlWidget;

import Artifact.Widgets.LayerPanelWidget;
import Artifact.Timeline.ScrubBar;
import Artifact.Widgets.Timeline.Label;
import Artifact.Timeline.NavigatorWidget;
import Artifact.Timeline.TrackPainterView;
import Artifact.Timeline.TimeCodeWidget;
import Artifact.Layers.Selection.Manager;
import Panel.DraggableSplitter;
import Artifact.Widgets.Timeline.GlobalSwitches;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Event.Bus;
import Artifact.Event.Types;
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
import Property.Abstract;
import Frame.Position;
import Time.Rational;

namespace Artifact {

using namespace ArtifactCore;
using namespace ArtifactWidgets;

namespace {
constexpr double kTimelineRowHeight = 28.0;
constexpr int kTimelineTopRowHeight = 16; // aligns with right ruler row
constexpr int kTimelineHeaderRowHeight =
    42; // matches the timecode widget height so the readout is not compressed
constexpr int kTimelineWorkAreaRowHeight = 26;
constexpr int kDefaultTimelineFrames = 300;
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

QString humanizeTimelinePropertyLabel(QString name)
{
  static const QHash<QString, QString> explicitLabels = {
      {QStringLiteral("transform.position.x"), QStringLiteral("Position X")},
      {QStringLiteral("transform.position.y"), QStringLiteral("Position Y")},
      {QStringLiteral("transform.scale.x"), QStringLiteral("Scale X")},
      {QStringLiteral("transform.scale.y"), QStringLiteral("Scale Y")},
      {QStringLiteral("transform.rotation"), QStringLiteral("Rotation")},
      {QStringLiteral("transform.anchor.x"), QStringLiteral("Anchor X")},
      {QStringLiteral("transform.anchor.y"), QStringLiteral("Anchor Y")},
      {QStringLiteral("layer.opacity"), QStringLiteral("Opacity")},
      {QStringLiteral("time.inPoint"), QStringLiteral("In Point")},
      {QStringLiteral("time.outPoint"), QStringLiteral("Out Point")},
      {QStringLiteral("time.startTime"), QStringLiteral("Start Time")}};
  if (const auto it = explicitLabels.constFind(name); it != explicitLabels.constEnd()) {
    return it.value();
  }

  const int dot = name.lastIndexOf('.');
  if (dot >= 0 && dot + 1 < name.size()) {
    name = name.mid(dot + 1);
  }

  QString out;
  out.reserve(name.size() * 2);
  for (int i = 0; i < name.size(); ++i) {
    const QChar ch = name.at(i);
    if (ch == '_' || ch == '-') {
      out += ' ';
      continue;
    }
    if (i > 0 && ch.isUpper() && name.at(i - 1).isLetterOrNumber()) {
      out += ' ';
    }
    out += ch;
  }

  bool cap = true;
  for (int i = 0; i < out.size(); ++i) {
    if (out.at(i).isSpace()) {
      cap = true;
      continue;
    }
    if (cap) {
      out[i] = out.at(i).toUpper();
      cap = false;
    }
  }
  return out;
}

QString timelineRowKey(const LayerID& layerId, const QString& propertyPath)
{
  return QStringLiteral("%1|%2").arg(layerId.toString(), propertyPath);
}

struct TimelineRow
{
  enum class Kind
  {
    LayerHeader,
    Property
  };

  Kind kind = Kind::LayerHeader;
  LayerID layerId;
  QString propertyPath;
  QString label;
};

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
  
  // layer::changed() でレンダラーとタイムライン Track ビューに通知する。
  // Qt signal projectChanged() は廃止済み。直接 layer->changed() を発火する。
  emit layer->changed();
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
                   ArtifactCore::EventBus *eventBus,
                   QObject *parent = nullptr)
      : QObject(parent), trackView_(trackView), scrubBar_(scrubBar), eventBus_(eventBus) {}

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
        const auto debugEvent = TimelineDebugMessageEvent{
            QStringLiteral("Playhead: %1 (Seek)").arg(frame)};
        if (eventBus_) eventBus_->post<TimelineDebugMessageEvent>(debugEvent);
        else ArtifactCore::globalEventBus().post<TimelineDebugMessageEvent>(debugEvent);

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

    const auto debugEvent = TimelineDebugMessageEvent{
        QStringLiteral("Playhead: %1 (Scrubbing)").arg(frame)};
    if (eventBus_) eventBus_->post<TimelineDebugMessageEvent>(debugEvent);
    else ArtifactCore::globalEventBus().post<TimelineDebugMessageEvent>(debugEvent);

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
  ArtifactCore::EventBus *eventBus_ = nullptr;
  static constexpr int kReservedClickDragThresholdPx = 4;
};

class HeaderScrollFilter final : public QObject {
public:
  HeaderScrollFilter(ArtifactTimelineTrackPainterView *trackView,
                     std::function<void(double)> horizontalOffsetSync,
                     QObject *parent = nullptr)
      : QObject(parent), trackView_(trackView),
        horizontalOffsetSync_(std::move(horizontalOffsetSync)) {}

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
          const double offset =
              std::max(0.0, trackView_->horizontalOffset() - delta);
          if (horizontalOffsetSync_) {
            horizontalOffsetSync_(offset);
          } else {
            trackView_->setHorizontalOffset(offset);
          }
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
      const double offset =
          std::max(0.0, trackView_->horizontalOffset() - delta.x());
      if (horizontalOffsetSync_) {
        horizontalOffsetSync_(offset);
      } else {
        trackView_->setHorizontalOffset(offset);
      }
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
  std::function<void(double)> horizontalOffsetSync_;
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

class TimelineRightPanelWidget final : public QWidget {
public:
  explicit TimelineRightPanelWidget(QWidget *parent = nullptr)
      : QWidget(parent) {
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
  }

  void setPlayheadOverlay(QWidget *overlay)
  {
    if (playheadOverlay_ == overlay) {
      return;
    }
    playheadOverlay_ = overlay;
    if (playheadOverlay_) {
      playheadOverlay_->setParent(this);
      playheadOverlay_->setGeometry(rect());
      playheadOverlay_->raise();
      playheadOverlay_->show();
    }
  }

protected:
  void resizeEvent(QResizeEvent *event) override
  {
    QWidget::resizeEvent(event);
    if (playheadOverlay_) {
      playheadOverlay_->setGeometry(rect());
      playheadOverlay_->raise();
    }
  }

  void paintEvent(QPaintEvent *event) override {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRect bounds = rect();
    const QColor base = palette().color(QPalette::Window);
    const QColor border = palette().color(QPalette::Mid).darker(120);
    const QColor topShade = palette().color(QPalette::Shadow);

    painter.fillRect(bounds, base);
    painter.setPen(QPen(border, 1));
    painter.drawRect(bounds.adjusted(0, 0, -1, -1));

    QColor accent = topShade;
    accent.setAlpha(72);
    painter.fillRect(QRect(bounds.left(), bounds.top(), bounds.width(), 1),
                     accent);
  }

private:
  QPointer<QWidget> playheadOverlay_;
};

class TimelinePlayheadOverlayWidget final : public QWidget {
public:
  explicit TimelinePlayheadOverlayWidget(ArtifactTimelineTrackPainterView *trackView,
                                        QWidget *parent = nullptr)
      : QWidget(parent), trackView_(trackView)
  {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    if (!trackView_) {
      return;
    }

    const double ppf = std::max(0.01, trackView_->pixelsPerFrame());
    const double xOffset = trackView_->horizontalOffset();
    const double frame = trackView_->currentFrame();
    const qreal playheadX = static_cast<qreal>(frame * ppf - xOffset);
    if (playheadX < -12.0 || playheadX > width() + 12.0) {
      return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor playheadColor(255, 106, 71);
    const qreal headHeight = 13.0;
    const qreal headWidth = 14.0;
    const qreal stemTop = headHeight + 2.0;
    const qreal stemBottom = static_cast<qreal>(height()) - 1.0;

    p.setPen(QPen(playheadColor, 2, Qt::SolidLine, Qt::FlatCap));
    p.drawLine(QPointF(playheadX, stemTop), QPointF(playheadX, stemBottom));

    QPolygonF head;
    head << QPointF(playheadX - headWidth * 0.5, 1.5)
         << QPointF(playheadX + headWidth * 0.5, 1.5)
         << QPointF(playheadX, stemTop);
    p.setBrush(playheadColor);
    p.setPen(QPen(QColor(18, 18, 18, 180), 1));
    p.drawPolygon(head);
  }

private:
  ArtifactTimelineTrackPainterView *trackView_ = nullptr;
};

class TimelineStatusClickFilter final : public QObject {
public:
  TimelineStatusClickFilter(QLabel *label, std::function<void()> callback,
                            QObject *parent = nullptr)
      : QObject(parent), label_(label), callback_(std::move(callback)) {}

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (watched != label_ || !callback_) {
      return QObject::eventFilter(watched, event);
    }
    if (event->type() != QEvent::MouseButtonRelease) {
      return QObject::eventFilter(watched, event);
    }
    auto *mouseEvent = dynamic_cast<QMouseEvent *>(event);
    if (!mouseEvent || mouseEvent->button() != Qt::LeftButton) {
      return QObject::eventFilter(watched, event);
    }
    callback_();
    return true;
  }

private:
  QLabel *label_ = nullptr;
  std::function<void()> callback_;
};

class SearchStatusClickFilter final : public QObject {
public:
  SearchStatusClickFilter(QLabel *label, std::function<void()> nextCallback,
                          std::function<void()> prevCallback,
                          QObject *parent = nullptr)
      : QObject(parent), label_(label), nextCallback_(std::move(nextCallback)),
        prevCallback_(std::move(prevCallback)) {}

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (watched != label_) {
      return QObject::eventFilter(watched, event);
    }
    if (event->type() != QEvent::MouseButtonRelease) {
      return QObject::eventFilter(watched, event);
    }
    auto *mouseEvent = dynamic_cast<QMouseEvent *>(event);
    if (!mouseEvent) {
      return QObject::eventFilter(watched, event);
    }
    if (mouseEvent->button() == Qt::LeftButton && nextCallback_) {
      nextCallback_();
      return true;
    }
    if (mouseEvent->button() == Qt::RightButton && prevCallback_) {
      prevCallback_();
      return true;
    }
    return QObject::eventFilter(watched, event);
  }

private:
  QLabel *label_ = nullptr;
  std::function<void()> nextCallback_;
  std::function<void()> prevCallback_;
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
  QLabel *currentLayerLabel_ = nullptr;
  QLabel *frameSummaryLabel_ = nullptr;
  QLabel *selectionSummaryLabel_ = nullptr;
  ArtifactLayerTimelinePanelWrapper *layerTimelinePanel_ = nullptr;
  ArtifactTimelineTrackPainterView *painterTrackView_ = nullptr;
  ArtifactTimelineScrubBar *scrubBar_ = nullptr;
  WorkAreaControl *workArea_ = nullptr;
  ArtifactTimelineNavigatorWidget *navigator_ = nullptr;
  TimelinePlayheadOverlayWidget *playheadOverlay_ = nullptr;
  CompositionID compositionId_;
  bool shyActive_ = false;
  QString filterText_;
  QVector<LayerID> searchResultLayerIds_;
  int searchResultIndex_ = -1;
  QVector<QString> trackRowKeys_;
  bool syncingLayerSelection_ = false;
  double currentFrame_ = 0.0;
  QMetaObject::Connection compositionChangedConnection_;
  ArtifactCore::EventBus eventBus_;
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  // refreshTracks() の重複キューイング防止フラグ。
  // ProjectChangedEvent と LayerChangedEvent が同一フレームで両方発火した場合に
  // refreshTracks() が 2 回実行されるのを防ぐ。
  bool pendingRefresh_ = false;
  // selection sync の重複キューイング防止フラグ。
  // SelectionChangedEvent と LayerSelectionChangedEvent が同時に来ても
  // painter / labels 更新を 1 回にまとめる。
  bool pendingSelectionSync_ = false;
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
  impl_->layerTimelinePanel_ = layerTreeView;

  auto leftSplitter = new DraggableSplitter(Qt::Horizontal);
  // leftSplitter->addWidget(iconView);
  leftSplitter->addWidget(layerTreeView);
  leftSplitter->setStretchFactor(0, 0); // ACR fixed
  leftSplitter->setStretchFactor(1, 1); // Layer Panel flexible
  leftSplitter->setHandleWidth(4);

  auto leftHeader = new ArtifactTimeCodeWidget();             // Timecode
  auto searchBar = new ArtifactTimelineSearchBarWidget();     // Search
  auto searchModeCombo = new QComboBox();                     // Search mode
  auto displayModeCombo = new QComboBox();                    // Layer display mode
  auto densityCombo = new QComboBox();                        // Row density
  auto globalSwitches = new ArtifactTimelineGlobalSwitches(); // AE Switches
  auto searchStatusLabel = new QLabel();
  auto keyframeStatusLabel = new QLabel();
  auto currentLayerLabel = new QLabel();
  auto frameSummaryLabel = new QLabel();
  auto selectionSummaryLabel = new QLabel();

  impl_->searchBar_ = searchBar;
  impl_->searchStatusLabel_ = searchStatusLabel;
  impl_->keyframeStatusLabel_ = keyframeStatusLabel;
  impl_->currentLayerLabel_ = currentLayerLabel;
  impl_->frameSummaryLabel_ = frameSummaryLabel;
  impl_->selectionSummaryLabel_ = selectionSummaryLabel;

  searchBar->setEventBus(&impl_->eventBus_);
  auto *focusSearchShortcut = new QShortcut(QKeySequence::Find, this);
  QObject::connect(focusSearchShortcut, &QShortcut::activated, this, [searchBar]() {
    if (searchBar) {
      searchBar->focusSearch();
    }
  });
  auto *clearSearchShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  QObject::connect(clearSearchShortcut, &QShortcut::activated, this, [searchBar]() {
    if (searchBar && searchBar->hasSearchText()) {
      searchBar->clearSearch();
    }
  });
  searchModeCombo->addItem(QStringLiteral("All Visible"), static_cast<int>(SearchMatchMode::AllVisible));
  searchModeCombo->addItem(QStringLiteral("Highlight Only"), static_cast<int>(SearchMatchMode::HighlightOnly));
  searchModeCombo->addItem(QStringLiteral("Filter Only"), static_cast<int>(SearchMatchMode::FilterOnly));
  searchModeCombo->setCurrentIndex(2);
  searchModeCombo->setVisible(false);
  displayModeCombo->addItem(QStringLiteral("All Layers"), static_cast<int>(TimelineLayerDisplayMode::AllLayers));
  displayModeCombo->addItem(QStringLiteral("Selected"), static_cast<int>(TimelineLayerDisplayMode::SelectedOnly));
  displayModeCombo->addItem(QStringLiteral("Animated"), static_cast<int>(TimelineLayerDisplayMode::AnimatedOnly));
  displayModeCombo->addItem(QStringLiteral("Keyframes + Important"), static_cast<int>(TimelineLayerDisplayMode::ImportantAndKeyframed));
  displayModeCombo->addItem(QStringLiteral("Audio"), static_cast<int>(TimelineLayerDisplayMode::AudioOnly));
  displayModeCombo->addItem(QStringLiteral("Video"), static_cast<int>(TimelineLayerDisplayMode::VideoOnly));
  displayModeCombo->setCurrentIndex(0);
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
  searchStatusLabel->setCursor(Qt::PointingHandCursor);
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
  currentLayerLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  currentLayerLabel->setMinimumWidth(160);
  currentLayerLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  {
    QFont font = currentLayerLabel->font();
    font.setWeight(QFont::DemiBold);
    currentLayerLabel->setFont(font);
    QPalette pal = currentLayerLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(191, 224, 255));
    currentLayerLabel->setPalette(pal);
  }
  currentLayerLabel->setText("");
  frameSummaryLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  frameSummaryLabel->setMinimumWidth(110);
  frameSummaryLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  {
    QFont font = frameSummaryLabel->font();
    font.setWeight(QFont::DemiBold);
    frameSummaryLabel->setFont(font);
    QPalette pal = frameSummaryLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(255, 216, 153));
    frameSummaryLabel->setPalette(pal);
  }
  frameSummaryLabel->setText("");
  currentLayerLabel->setCursor(Qt::PointingHandCursor);
  frameSummaryLabel->setCursor(Qt::PointingHandCursor);
  selectionSummaryLabel->setCursor(Qt::PointingHandCursor);
  selectionSummaryLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  selectionSummaryLabel->setMinimumWidth(220);
  selectionSummaryLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  {
    QFont font = selectionSummaryLabel->font();
    font.setWeight(QFont::DemiBold);
    selectionSummaryLabel->setFont(font);
    QPalette pal = selectionSummaryLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(203, 232, 176));
    selectionSummaryLabel->setPalette(pal);
  }
  selectionSummaryLabel->setText("");
  globalSwitches->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  globalSwitches->setFixedWidth(globalSwitches->sizeHint().width());

  auto searchBarLayout = new QHBoxLayout();
  searchBarLayout->setSpacing(8);
  searchBarLayout->setContentsMargins(0, 0, 8, 0);
  searchBarLayout->addWidget(leftHeader);
  searchBarLayout->addWidget(searchBar);
  searchBarLayout->addWidget(searchModeCombo);
  searchBarLayout->addWidget(displayModeCombo);
  searchBarLayout->addWidget(densityCombo);
  searchBarLayout->addWidget(searchStatusLabel);
  searchBarLayout->addWidget(keyframeStatusLabel);
  searchBarLayout->addWidget(currentLayerLabel);
  searchBarLayout->addWidget(frameSummaryLabel);
  searchBarLayout->addWidget(selectionSummaryLabel);
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
  searchBarLayout->setStretch(9, 0);
  searchBarLayout->setStretch(10, 0);
  searchBarLayout->setStretch(11, 1);
  globalSwitches->setEventBus(&impl_->eventBus_);
  layerTreeView->setEventBus(&impl_->eventBus_);
  timeNavigatorWidget->setEventBus(&impl_->eventBus_);
  workAreaWidget->setEventBus(&impl_->eventBus_);
  scrubBar->setEventBus(&impl_->eventBus_);
  painterTrackView->setEventBus(&impl_->eventBus_);

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineSearchTextChangedEvent>(
          [this](const TimelineSearchTextChangedEvent& event) {
            onSearchTextChanged(event.text);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineSearchNextRequestedEvent>(
          [this](const TimelineSearchNextRequestedEvent&) {
            jumpToSearchHit(+1);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineSearchPrevRequestedEvent>(
          [this](const TimelineSearchPrevRequestedEvent&) {
            jumpToSearchHit(-1);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineSearchClearedEvent>(
          [this](const TimelineSearchClearedEvent&) {
            onSearchTextChanged(QString());
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineShyChangedEvent>([this](const TimelineShyChangedEvent& e) {
        QMetaObject::invokeMethod(this, [this, e]() { onShyChanged(e.shy); }, Qt::QueuedConnection);
      }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineVisibleRowsChangedEvent>(
          [this](const TimelineVisibleRowsChangedEvent&) {
            refreshTracks();
            updateSelectionState();
            updateSearchState();
          }));

  auto headerWidget = new QWidget();
  headerWidget->setLayout(searchBarLayout);
  headerWidget->setFixedHeight(kTimelineHeaderRowHeight);

  auto *leftHeaderPriorityFilter = new LeftHeaderPriorityFilter(
      headerWidget, leftHeader, searchBar, globalSwitches, headerWidget);
  headerWidget->installEventFilter(leftHeaderPriorityFilter);
  leftHeaderPriorityFilter->sync();

  auto *currentLayerClickFilter = new TimelineStatusClickFilter(
      currentLayerLabel, [this]() {
        ArtifactAbstractLayerPtr currentLayer;
        if (auto *app = ArtifactApplicationManager::instance()) {
          if (auto *selection = app->layerSelectionManager()) {
            currentLayer = selection->currentLayer();
          }
        }
        if (!currentLayer) {
          return;
        }
        if (auto *svc = ArtifactProjectService::instance()) {
          svc->selectLayer(currentLayer->id());
        }
        if (impl_ && impl_->layerTimelinePanel_) {
          impl_->layerTimelinePanel_->scrollToLayer(currentLayer->id());
        }
      },
      headerWidget);
  currentLayerLabel->installEventFilter(currentLayerClickFilter);

  auto *selectionSummaryClickFilter = new TimelineStatusClickFilter(
      selectionSummaryLabel, [this]() {
        QSet<ArtifactAbstractLayerPtr> selectedLayers;
        ArtifactAbstractLayerPtr currentLayer;
        if (auto *app = ArtifactApplicationManager::instance()) {
          if (auto *selection = app->layerSelectionManager()) {
            currentLayer = selection->currentLayer();
            selectedLayers = selection->selectedLayers();
          }
        }
        LayerID targetLayerId;
        if (currentLayer) {
          targetLayerId = currentLayer->id();
        } else if (!selectedLayers.isEmpty()) {
          targetLayerId = (*selectedLayers.begin())->id();
        }
        if (targetLayerId.isNil()) {
          return;
        }
        if (auto *svc = ArtifactProjectService::instance()) {
          svc->selectLayer(targetLayerId);
        }
        if (impl_ && impl_->layerTimelinePanel_) {
          impl_->layerTimelinePanel_->scrollToLayer(targetLayerId);
        }
      },
      headerWidget);
  selectionSummaryLabel->installEventFilter(selectionSummaryClickFilter);

  auto *frameSummaryClickFilter = new TimelineStatusClickFilter(
      frameSummaryLabel, [this]() {
        if (!impl_ || !impl_->painterTrackView_) {
          return;
        }
        const double frame = impl_->currentFrame_;
        const double ppf = std::max(0.01, impl_->painterTrackView_->pixelsPerFrame());
        const double centeredOffset = std::max(
            0.0, frame * ppf - static_cast<double>(impl_->painterTrackView_->width()) * 0.5);
        syncTimelineHorizontalOffset(centeredOffset);
        impl_->painterTrackView_->setFocus(Qt::MouseFocusReason);
        if (impl_->scrubBar_) {
          impl_->scrubBar_->setFocus(Qt::MouseFocusReason);
        }
      },
      headerWidget);
  frameSummaryLabel->installEventFilter(frameSummaryClickFilter);

  auto *searchStatusClickFilter = new SearchStatusClickFilter(
      searchStatusLabel, [this]() { jumpToSearchHit(+1); },
      [this]() { jumpToSearchHit(-1); }, headerWidget);
  searchStatusLabel->installEventFilter(searchStatusClickFilter);

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
  painterTrackView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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

  auto updateZoom = [this]() { syncTimelineViewportFromNavigator(); };

  auto syncPainterSelectionState = [this]() {
    this->syncPainterSelectionState();
    updateKeyframeState();
  };

  syncWorkAreaFromCurrentComposition();

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineNavigatorStartChangedEvent>(
          [updateZoom](const TimelineNavigatorStartChangedEvent&) { updateZoom(); }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineNavigatorEndChangedEvent>(
          [updateZoom](const TimelineNavigatorEndChangedEvent&) { updateZoom(); }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineWorkAreaStartChangedEvent>(
          [this, workAreaWidget](const TimelineWorkAreaStartChangedEvent&) {
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
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineWorkAreaEndChangedEvent>(
          [this, workAreaWidget](const TimelineWorkAreaEndChangedEvent&) {
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
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineSeekRequestedEvent>(
          [this, scrubBar](const TimelineSeekRequestedEvent& event) {
            const int clampedFrame =
                std::clamp(static_cast<int>(std::llround(event.frame)), 0,
                           std::max(0, scrubBar->totalFrames() - 1));
            scrubBar->setCurrentFrame(FramePosition(clampedFrame));
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineClipSelectedEvent>(
          [this](const TimelineClipSelectedEvent& event) {
            if (impl_->syncingLayerSelection_) {
              return;
            }
            if (auto* svc = ArtifactProjectService::instance()) {
              svc->selectLayer(LayerID(event.layerId));
            }
            Q_UNUSED(event);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineClipDeselectedEvent>(
          [this](const TimelineClipDeselectedEvent&) {
            if (impl_->syncingLayerSelection_) {
              return;
            }
            if (auto* svc = ArtifactProjectService::instance()) {
              svc->selectLayer(LayerID());
            }
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineClipMovedEvent>(
          [this](const TimelineClipMovedEvent& event) {
            applyTimelineLayerMove(impl_->compositionId_, event.clipId, event.startFrame, 0.0);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineClipResizedEvent>(
          [this](const TimelineClipResizedEvent& event) {
            applyTimelineLayerTrim(impl_->compositionId_, event.clipId, event.startFrame,
                                   event.durationFrame);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineDebugMessageEvent>(
          [this](const TimelineDebugMessageEvent& event) {
            if (impl_ && impl_->timelineLabel_) {
              impl_->timelineLabel_->setText(event.message);
            }
            ArtifactCore::globalEventBus().post<TimelineDebugMessageEvent>(event);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineKeyframeSelectionChangedEvent>(
          [this](const TimelineKeyframeSelectionChangedEvent&) {
            updateKeyframeState();
          }));
  // auto layerTimelinePanel = new ArtifactLayerTimelinePanelWrapper();
  // layerTimelinePanel->setMinimumWidth(220);
  // layerTimelinePanel->setMaximumWidth(320);

  auto rightPanel = new TimelineRightPanelWidget();
  rightPanelLayout->addWidget(timeNavigatorWidget);
  rightPanelLayout->addWidget(scrubBar);
  rightPanelLayout->addWidget(workAreaWidget);
  rightPanelLayout->addWidget(painterTrackView, 1);
  rightPanel->setLayout(rightPanelLayout);

  impl_->playheadOverlay_ =
      new TimelinePlayheadOverlayWidget(painterTrackView, rightPanel);
  rightPanel->setPlayheadOverlay(impl_->playheadOverlay_);

  auto *headerSeekFilter =
      new HeaderSeekFilter(painterTrackView, scrubBar, &impl_->eventBus_, rightPanel);
  timeNavigatorWidget->installEventFilter(headerSeekFilter);
  workAreaWidget->installEventFilter(headerSeekFilter);
  scrubBar->installEventFilter(headerSeekFilter); // Install on scrub bar

  auto *headerScrollFilter =
      new HeaderScrollFilter(
          painterTrackView,
          [this](double offset) {
            syncTimelineHorizontalOffset(offset);
          },
          rightPanel);
  rightPanel->installEventFilter(headerScrollFilter);
  timeNavigatorWidget->installEventFilter(headerScrollFilter);
  scrubBar->installEventFilter(headerScrollFilter);
  workAreaWidget->installEventFilter(headerScrollFilter);

  if (painterTrackView) {
    auto *viewResizeFilter =
        new ViewportResizeFilter(rightPanel, updateZoom, rightPanel);
    painterTrackView->installEventFilter(viewResizeFilter);
  }

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineScrubFrameChangedEvent>(
          [this, scrubBar](const TimelineScrubFrameChangedEvent& event) {
            const FramePosition frame(event.frame);
            impl_->currentFrame_ = static_cast<double>(frame.framePosition());
            if (impl_ && impl_->timelineLabel_) {
              impl_->timelineLabel_->setText(
                  QStringLiteral("Playhead: %1").arg(frame.framePosition()));
            }
            if (impl_->painterTrackView_) {
              impl_->painterTrackView_->setCurrentFrame(
                  static_cast<double>(frame.framePosition()));
            }
            syncPlayheadOverlay();
            updateSelectionState();
            updateKeyframeState();
            if (auto *app = ArtifactApplicationManager::instance()) {
              if (auto *ctx = app->activeContextService()) {
                ctx->seekToFrame(frame.framePosition());
                return;
              }
            }
            if (auto *playback = ArtifactPlaybackService::instance()) {
              playback->goToFrame(frame);
            }
            Q_UNUSED(scrubBar);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<FrameChangedEvent>(
          [this, scrubBar](const FrameChangedEvent &event) {
            if (impl_->compositionId_.isNil() ||
                event.compositionId != impl_->compositionId_.toString()) {
              return;
            }

            const FramePosition frame(event.frame);
            if (impl_->painterTrackView_) {
              impl_->painterTrackView_->setCurrentFrame(
                  static_cast<double>(frame.framePosition()));
            }
            impl_->currentFrame_ = static_cast<double>(frame.framePosition());
            syncPlayheadOverlay();
            const QSignalBlocker blocker(scrubBar);
            scrubBar->setCurrentFrame(frame);
            updateSelectionState();
            // 再生中は毎フレーム全レイヤーをスキャンするコストを避けるため15フレームに1回
            if (frame.framePosition() % 15 == 0) {
              updateKeyframeState();
            }
          }));
  QTimer::singleShot(0, this, [updateZoom]() { updateZoom(); });

  // Ŝ̃^CCXvb^[
  auto mainSplitter = new QSplitter(Qt::Horizontal);
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

  // EventBus に載っている広域状態変化を購読する
  // scheduleRefresh: ProjectChangedEvent と LayerChangedEvent が同一フレームで両方発火しても
  // refreshTracks() が 1 回だけ実行されるようにデバウンスする。
  const auto scheduleRefresh = [this]() {
    if (!impl_->pendingRefresh_) {
      impl_->pendingRefresh_ = true;
      QMetaObject::invokeMethod(
          this,
          [this]() {
            if (!impl_) return;
            impl_->pendingRefresh_ = false;
            if (!impl_->painterTrackView_) return;
            refreshTracks();
            updateSelectionState();
          },
          Qt::QueuedConnection);
    }
  };
  const auto scheduleSelectionSync = [this, syncPainterSelectionState]() {
    if (!impl_->pendingSelectionSync_) {
      impl_->pendingSelectionSync_ = true;
      QMetaObject::invokeMethod(
          this,
          [this, syncPainterSelectionState]() {
            if (!impl_) {
              return;
            }
            impl_->pendingSelectionSync_ = false;
            if (!impl_->painterTrackView_) {
              return;
            }
            syncPainterSelectionState();
            updateSelectionState();
            updateSearchState();
            updateKeyframeState();
          },
          Qt::QueuedConnection);
    }
  };

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectChangedEvent>(
          [this, scheduleRefresh](const ProjectChangedEvent&) {
            if (!impl_ || !impl_->painterTrackView_) return;
            scheduleRefresh();
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerChangedEvent>(
          [this, scheduleRefresh](const LayerChangedEvent& event) {
            if (event.changeType == LayerChangedEvent::ChangeType::Created) {
              onLayerCreated(CompositionID(event.compositionId), LayerID(event.layerId));
            } else if (event.changeType == LayerChangedEvent::ChangeType::Removed) {
              onLayerRemoved(CompositionID(event.compositionId), LayerID(event.layerId));
            } else {
              if (!impl_ || !impl_->painterTrackView_) return;
              scheduleRefresh();
            }
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<SelectionChangedEvent>(
          [this, scheduleSelectionSync](const SelectionChangedEvent&) {
            if (!impl_ || !impl_->painterTrackView_) {
              return;
            }
            scheduleSelectionSync();
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerSelectionChangedEvent>(
          [this, scheduleSelectionSync](const LayerSelectionChangedEvent&) {
            if (!impl_ || !impl_->painterTrackView_) {
              return;
            }
            scheduleSelectionSync();
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>(
          [this](const CurrentCompositionChangedEvent& event) {
            if (event.compositionId.trimmed().isEmpty()) {
              return;
            }
            const CompositionID compositionId(event.compositionId);
            if (!impl_ || compositionId == impl_->compositionId_) {
              return;
            }
            QMetaObject::invokeMethod(
                this,
                [this, compositionId]() {
                  if (!impl_) {
                    return;
                  }
                  setComposition(compositionId);
                },
                Qt::QueuedConnection);
          }));
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
        updateSelectionState();
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
        syncPlayheadOverlay();
        if (auto *app = ArtifactApplicationManager::instance()) {
          if (auto *ctx = app->activeContextService()) {
            ctx->seekToFrame(0);
          }
        }
        QTimer::singleShot(0, this, [this]() {
          if (!impl_) {
            return;
          }
          syncTimelineViewportFromNavigator();
          updateKeyframeState();
        });

        refreshTracks();
        updateSearchState();
        updateKeyframeState();
      }
    }
  }
  updateSelectionState();
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
  updateSelectionState();
}

void ArtifactTimelineWidget::onLayerRemoved(const CompositionID &compId,
                                            const LayerID &lid) {
  if (compId != impl_->compositionId_)
    return;
  qDebug() << "[ArtifactTimelineWidget::onLayerRemoved] Layer removed:"
           << lid.toString();
  refreshTracks();
  updateSelectionState();
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

  const auto composition = safeCompositionLookup(impl_->compositionId_);

  QVector<LayerID> visibleRows;
  if (impl_->layerTimelinePanel_) {
    visibleRows = impl_->layerTimelinePanel_->visibleTimelineRows();
  }

  if (!impl_->layerTimelinePanel_) {
    if (composition) {
      auto layers = composition->allLayer();
      std::reverse(layers.begin(), layers.end());
      for (auto &layer : layers) {
        if (!layer)
          continue;
        if (impl_->shyActive_ && layer->isShy())
          continue;
        visibleRows.push_back(layer->id());
      }
    }
  }

  QVector<TimelineRow> timelineRows;
  timelineRows.reserve(std::max(1, static_cast<int>(visibleRows.size()) * 4));
  std::unordered_set<std::string> seenLayerIds;
  for (const auto &rowLayerId : visibleRows) {
    if (rowLayerId.isNil()) {
      continue;
    }

    const std::string layerKey = rowLayerId.toString().toStdString();
    if (!seenLayerIds.insert(layerKey).second) {
      continue;
    }

    const auto layer = composition ? composition->layerById(rowLayerId) : nullptr;
    if (!layer) {
      continue;
    }

    timelineRows.push_back(TimelineRow{
        TimelineRow::Kind::LayerHeader,
        rowLayerId,
        QString(),
        layer->layerName()});

    for (const auto &group : layer->getLayerPropertyGroups()) {
      for (const auto &property : group.sortedProperties()) {
        if (!property || !property->isAnimatable()) {
          continue;
        }
        const QString propertyPath = property->getName();
        if (propertyPath.isEmpty()) {
          continue;
        }
        timelineRows.push_back(TimelineRow{
            TimelineRow::Kind::Property,
            rowLayerId,
            propertyPath,
            humanizeTimelinePropertyLabel(propertyPath)});
      }
    }
  }

  impl_->trackRowKeys_.clear();
  impl_->trackRowKeys_.reserve(timelineRows.size());
  QVector<int> trackHeights;
  const int trackCount = std::max(1, static_cast<int>(timelineRows.size()));
  trackHeights.reserve(trackCount);
  for (int i = 0; i < trackCount; ++i) {
    trackHeights.push_back(static_cast<int>(kTimelineRowHeight));
  }
  QVector<ArtifactTimelineTrackPainterView::TrackClipVisual> painterClips;
  painterClips.reserve(timelineRows.size());
  if (impl_->painterTrackView_) {
    impl_->painterTrackView_->setTrackHeights(trackHeights);
  }
  for (int rowIndex = 0; rowIndex < timelineRows.size(); ++rowIndex) {
    const auto &row = timelineRows[rowIndex];
    impl_->trackRowKeys_.push_back(timelineRowKey(row.layerId, row.propertyPath));

    if (row.kind != TimelineRow::Kind::LayerHeader) {
      continue;
    }

    const auto layer =
        composition ? composition->layerById(row.layerId) : nullptr;
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
      visual.clipId = row.layerId.toString();
      visual.layerId = row.layerId;
      visual.trackIndex = rowIndex;
      visual.startFrame = clipStart;
      visual.durationFrame = clipDuration;
      visual.title = layer->layerName();
      visual.fillColor = layerTimelineColor(layer);
      painterClips.push_back(std::move(visual));
    }
  }

  if (impl_->painterTrackView_) {
    const double durationFrames = composition
        ? static_cast<double>(composition->frameRange().duration())
        : static_cast<double>(kDefaultTimelineFrames);
    impl_->painterTrackView_->setDurationFrames(
        std::max(1.0, durationFrames));
    impl_->painterTrackView_->setCurrentFrame(impl_->currentFrame_);
    syncPlayheadOverlay();
    impl_->painterTrackView_->setClips(painterClips);
    syncPainterSelectionState();
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
    syncPlayheadOverlay();
    if (impl_->painterTrackView_->durationFrames() > 0.0) {
      const double targetFrame =
          std::clamp(newPos, 0.0, impl_->painterTrackView_->durationFrames());
      impl_->painterTrackView_->setCurrentFrame(targetFrame);
    }
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
        // applyTimelineLayerMove / applyTimelineLayerRangeEdit の中で
        // emit layer->changed() が発火済みのため、ここでは何もしない。
        // svc->projectChanged() は廃止済み。
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

ArtifactCore::EventBus* ArtifactTimelineWidget::eventBus() const
{
  return impl_ ? &impl_->eventBus_ : nullptr;
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
    const int totalHits = impl_->searchResultLayerIds_.size();
    if (totalHits <= 0) {
      impl_->searchStatusLabel_->setText(QStringLiteral("0 hits"));
    } else if (impl_->searchResultIndex_ >= 0) {
      impl_->searchStatusLabel_->setText(
          QStringLiteral("Hit %1/%2").arg(impl_->searchResultIndex_ + 1).arg(totalHits));
    } else {
      impl_->searchStatusLabel_->setText(QStringLiteral("%1 hits").arg(totalHits));
    }
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

void ArtifactTimelineWidget::updateSelectionState()
{
  if (!impl_ || !impl_->currentLayerLabel_) {
    return;
  }

  ArtifactAbstractLayerPtr currentLayer;
  int selectedCount = 0;
  QString compositionLabel = QStringLiteral("-");
  qint64 frameLabelValue = qRound64(std::max(0.0, impl_->currentFrame_));
  if (auto *app = ArtifactApplicationManager::instance()) {
    if (auto *selection = app->layerSelectionManager()) {
      currentLayer = selection->currentLayer();
      selectedCount = static_cast<int>(selection->selectedLayers().size());
      if (impl_->layerTimelinePanel_ && currentLayer) {
        impl_->layerTimelinePanel_->scrollToLayer(currentLayer->id());
      }
    }
  }
  if (auto *svc = ArtifactProjectService::instance()) {
    if (auto composition = svc->currentComposition().lock()) {
      const QString name = composition->settings().compositionName().toQString().trimmed();
      compositionLabel = name.isEmpty() ? composition->id().toString() : name;
    }
  }

  if (currentLayer) {
    impl_->currentLayerLabel_->setText(QStringLiteral("Current: %1").arg(currentLayer->layerName()));
  } else {
    impl_->currentLayerLabel_->setText(QStringLiteral("Current: none"));
  }
  if (impl_->frameSummaryLabel_) {
    impl_->frameSummaryLabel_->setText(QStringLiteral("Frame: %1").arg(frameLabelValue));
  }
  if (impl_->selectionSummaryLabel_) {
    impl_->selectionSummaryLabel_->setText(
        QStringLiteral("Selected: %1 | Comp: %2 | Frame: %3")
            .arg(selectedCount)
            .arg(compositionLabel)
            .arg(frameLabelValue));
  }
}

void ArtifactTimelineWidget::syncPainterSelectionState()
{
  if (!impl_ || !impl_->painterTrackView_) {
    return;
  }

  ArtifactLayerSelectionManager *selection = nullptr;
  if (auto *app = ArtifactApplicationManager::instance()) {
    selection = app->layerSelectionManager();
  }
  const auto composition = safeCompositionLookup(impl_->compositionId_);
  impl_->painterTrackView_->syncSelectionState(
      composition, selection, impl_->trackRowKeys_);
}

void ArtifactTimelineWidget::syncTimelineHorizontalOffset(const double offset)
{
  if (!impl_) {
    return;
  }
  if (impl_->painterTrackView_) {
    impl_->painterTrackView_->setHorizontalOffset(offset);
  }
  if (impl_->scrubBar_) {
    impl_->scrubBar_->setRulerHorizontalOffset(offset);
  }
  syncPlayheadOverlay();
}

void ArtifactTimelineWidget::syncTimelineViewportFromNavigator()
{
  if (!impl_ || !impl_->painterTrackView_ || !impl_->workArea_ ||
      !impl_->navigator_) {
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
  if (impl_->scrubBar_) {
    impl_->scrubBar_->setRulerPixelsPerFrame(newZoom);
  }

  const auto zoomEvent = TimelineZoomLevelChangedEvent{newZoom * 100.0};
  impl_->eventBus_.post<TimelineZoomLevelChangedEvent>(zoomEvent);
  ArtifactCore::globalEventBus().post<TimelineZoomLevelChangedEvent>(zoomEvent);

  const double offset =
      impl_->navigator_->startValue() * duration * newZoom;
  syncTimelineHorizontalOffset(offset);
  syncPlayheadOverlay();
}

void ArtifactTimelineWidget::syncPlayheadOverlay()
{
  if (!impl_ || !impl_->playheadOverlay_) {
    return;
  }
  impl_->playheadOverlay_->update();
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
  syncPlayheadOverlay();
  if (impl_->scrubBar_) {
    impl_->scrubBar_->setCurrentFrame(FramePosition(static_cast<int>(targetFrame)));
  }
  updateKeyframeState();
}

}; // namespace Artifact
