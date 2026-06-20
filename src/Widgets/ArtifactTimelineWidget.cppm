module;

#include <QBoxLayout>
#include <QApplication>
#include <QBrush>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFocusEvent>
#include <QEvent>
#include <QHash>
#include <QLabel>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMessageBox>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QSplitter>
#include <QListWidget>
#include <QKeySequence>
#include <QDebug>
#include <QSet>
#include <QShortcut>
#include <QStandardItem>
#include <QStringList>
#include <QToolButton>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>
#include <QPaintEvent>
#include <QPointer>
#include <QPolygonF>
#include <QStackedWidget>
#include <limits>
#include <qtmetamacros.h>
#include <wobjectdefs.h>
#include <wobjectimpl.h>
#include "Timeline/TimelinePlayheadDraw.hpp"

module Artifact.Widgets.Timeline;

import std;

import Widgets.Utils.CSS;

import Artifact.Layers.Hierarchy.Model;
import Artifact.Widget.WorkAreaControlWidget;

import Artifact.Widgets.LayerPanelWidget;
import Widget.CurveEditor;
import Artifact.Timeline.ScrubBar;
import Artifact.Timeline.KeyBinding;
import Input.Operator;
import Artifact.Timeline.KeyframeModel;
import Artifact.Widgets.Timeline.Label;
import Artifact.Timeline.NavigatorWidget;
import Artifact.Timeline.TrackPainterView;
import Artifact.Timeline.TimeCodeWidget;
import Artifact.Widgets.Timeline.EasingLab;
import Artifact.Widgets.Timeline.KeyPatternDialog;
import Artifact.Layers.Selection.Manager;
import Panel.DraggableSplitter;
import Artifact.Widgets.Timeline.GlobalSwitches;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Service.ActiveContext;
import Clipboard.ClipboardManager;
import Event.Bus;
import Artifact.Event.Types;
import Artifact.Application.Manager;
import Artifact.Tool.Manager;
import Application.AppSettings;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Audio;
import Artifact.Audio.Waveform;
import Artifact.Layer.Camera;
import Artifact.Layer.Image;
import Artifact.Layer.Light;
import Artifact.Layer.Particle;
import Artifact.Layer.Shape;
import Artifact.Layer.Solid2D;
import Artifact.Layer.Svg;
import Artifact.Layer.Text;
import Artifact.Layer.Video;
import Property.Abstract;
import Artifact.Effect.Abstract;
import Frame.Position;
import Undo.UndoManager;
import Time.Rational;
import UI.ShortcutBindings;

namespace Artifact {

using namespace ArtifactCore;
using namespace ArtifactWidgets;

namespace {
std::shared_ptr<ArtifactCore::AbstractProperty> findLayerPropertyByPath(
    const ArtifactAbstractLayerPtr& layer, const QString& propertyPath);

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

bool timelineAllowOverscroll() {
  if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
    return settings->timelineAllowOverscroll();
  }
  return false;
}

double timelineOverscrollPaddingPx(const QWidget *widget) {
  const double widthHint = widget ? static_cast<double>(widget->width()) : 640.0;
  return std::clamp(std::max(96.0, widthHint * 0.25), 96.0, 320.0);
}

double timelineFrameRateFallback(const ArtifactCompositionPtr& composition)
{
  if (composition) {
    return std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  }
  if (auto* playback = ArtifactPlaybackService::instance()) {
    return std::max(1.0, static_cast<double>(playback->frameRate().framerate()));
  }
  return 1.0;
}

double clampTimelineHorizontalOffset(const ArtifactTimelineTrackPainterView *trackView,
                                     const double offset) {
  if (!trackView) {
    return std::max(0.0, offset);
  }
  const double maxOffset = std::max(
      0.0, trackView->durationFrames() * std::max(0.001, trackView->pixelsPerFrame()) -
               static_cast<double>(std::max(1, trackView->width())));
  if (timelineAllowOverscroll()) {
    const double pad = timelineOverscrollPaddingPx(trackView);
    return std::clamp(offset, -pad, maxOffset + pad);
  }
  return std::clamp(offset, 0.0, maxOffset);
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

struct TimelineCacheVisuals {
  std::vector<bool> readyFrames;
  std::vector<bool> failedFrames;
  std::vector<bool> onDiskFrames;
};

TimelineCacheVisuals buildTimelineCacheVisuals(ArtifactPlaybackService *playback) {
  TimelineCacheVisuals visuals;
  if (!playback) {
    return visuals;
  }

  const auto readyBitmap = playback->ramPreviewCacheBitmap();
  const int64_t frameCount = static_cast<int64_t>(readyBitmap.size());
  visuals.readyFrames = readyBitmap;
  visuals.failedFrames.assign(readyBitmap.size(), false);
  visuals.onDiskFrames.assign(readyBitmap.size(), false);

  for (int64_t frame = 0; frame < frameCount; ++frame) {
    const auto state = playback->ramPreviewFrameState(frame);
    visuals.failedFrames[static_cast<size_t>(frame)] = state.failed;
    visuals.onDiskFrames[static_cast<size_t>(frame)] = state.onDisk;
    if (!visuals.readyFrames[static_cast<size_t>(frame)]) {
      visuals.readyFrames[static_cast<size_t>(frame)] = state.playable;
    }
  }

  return visuals;
}

QColor layerTimelineColor(const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) {
    return QColor(94, 124, 189);
  }
  const auto* raw = layer.get();
  if (dynamic_cast<const ArtifactVideoLayer*>(raw)) {
    return QColor(79, 142, 230);
  }
  if (dynamic_cast<const ArtifactAudioLayer*>(raw)) {
    return QColor(86, 180, 120);
  }
  if (dynamic_cast<const ArtifactTextLayer*>(raw)) {
    return QColor(165, 108, 255);
  }
  if (dynamic_cast<const ArtifactShapeLayer*>(raw) ||
      dynamic_cast<const ArtifactSvgLayer*>(raw)) {
    return QColor(146, 106, 235);
  }
  if (dynamic_cast<const ArtifactImageLayer*>(raw)) {
    return QColor(84, 163, 255);
  }
  if (dynamic_cast<const ArtifactSolid2DLayer*>(raw)) {
    return QColor(255, 145, 86);
  }
  if (dynamic_cast<const ArtifactCameraLayer*>(raw)) {
    return QColor(255, 193, 79);
  }
  if (dynamic_cast<const ArtifactLightLayer*>(raw)) {
    return QColor(255, 221, 102);
  }
  if (dynamic_cast<const ArtifactParticleLayer*>(raw)) {
    return QColor(255, 110, 180);
  }
  return QColor(94, 124, 189);
}

QString formatSelectedKeyframeSummary(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    const qint64 currentFrame) {
  if (markers.isEmpty()) {
    return QStringLiteral("goal: inspect keyframes | now: none | warning: lane empty | next: add/remove at playhead");
  }

  qint64 minFrame = std::numeric_limits<qint64>::max();
  qint64 maxFrame = std::numeric_limits<qint64>::min();
  qint64 nearestFrame = -1;
  qint64 nearestDelta = std::numeric_limits<qint64>::max();
  QSet<QString> propertyLabels;
  QStringList sampleLabels;
  bool hitsCurrentFrame = false;
  for (const auto &marker : markers) {
    const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
    minFrame = std::min(minFrame, frame);
    maxFrame = std::max(maxFrame, frame);
    const QString displayLabel =
        marker.label.isEmpty()
            ? ArtifactTimelineKeyframeModel::displayLabelForPropertyPath(
                  marker.propertyPath)
            : marker.label;
    hitsCurrentFrame |= (frame == currentFrame);
    const qint64 delta = std::llabs(frame - currentFrame);
    if (delta < nearestDelta) {
      nearestDelta = delta;
      nearestFrame = frame;
    }
    propertyLabels.insert(displayLabel);
    if (sampleLabels.size() < 3 && !sampleLabels.contains(displayLabel)) {
      sampleLabels.push_back(displayLabel);
    }
  }

  const QString propertyText =
      propertyLabels.size() == 1
          ? QStringLiteral("%1").arg(*propertyLabels.begin())
          : QStringLiteral("%1 props").arg(propertyLabels.size());

  const QString frameText =
      minFrame == maxFrame ? QStringLiteral("F%1").arg(minFrame)
                           : QStringLiteral("F%1-F%2").arg(minFrame).arg(maxFrame);
  QString previewText;
  if (!sampleLabels.isEmpty()) {
    previewText = QStringLiteral("%1")
                      .arg(sampleLabels.join(QStringLiteral(", ")));
    if (propertyLabels.size() > sampleLabels.size()) {
      previewText += QStringLiteral(" (+%1 more)")
                         .arg(propertyLabels.size() - sampleLabels.size());
    }
  }
  const QString nowText =
      QStringLiteral("keys=%1 prop=%2 frame=%3 current=%4 state=%5")
          .arg(markers.size())
          .arg(propertyText)
          .arg(frameText)
          .arg(hitsCurrentFrame ? QStringLiteral("yes") : QStringLiteral("no"))
          .arg(hitsCurrentFrame ? QStringLiteral("selected+current")
                                : QStringLiteral("selected"));
  const QString warningText =
      hitsCurrentFrame ? QStringLiteral("at current frame")
                       : QStringLiteral("off current frame");
  const QString nextText =
      markers.size() == 1 ? QStringLiteral("add one more")
                          : QStringLiteral("narrow selection");
  const QString proximityText =
      nearestFrame >= 0
          ? (nearestDelta == 0
                 ? QStringLiteral("nearest=current")
                 : QStringLiteral("nearest=F%1 (%2)")
                       .arg(nearestFrame)
                       .arg(QString::number(nearestFrame - currentFrame)))
          : QStringLiteral("nearest=none");
  return QStringLiteral("goal: keep keyframes readable | now: %1 | warning: %2 | next: %3 | %4%5")
      .arg(nowText)
      .arg(warningText)
      .arg(nextText)
      .arg(proximityText)
      .arg(previewText.isEmpty() ? QString() : QStringLiteral(" | %1").arg(previewText));
}

QString formatHoveredKeyframeSummary(
    const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual &marker,
    const qint64 currentFrame) {
  if (marker.trackIndex < 0) {
    return QString();
  }

  const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
  const QString displayLabel =
      marker.label.isEmpty()
          ? ArtifactTimelineKeyframeModel::displayLabelForPropertyPath(
                marker.propertyPath)
          : marker.label;
  const QString relationText =
      frame == currentFrame ? QStringLiteral("current") : QStringLiteral("hover");
  return QStringLiteral("%1 @ F%2 (%3)")
      .arg(displayLabel)
      .arg(frame)
      .arg(relationText);
}

struct CachedAudioWaveform {
  QString signature;
  QVector<float> peaks;
  QVector<float> rms;
};

class TimelineToolCallbackButton final : public QToolButton {
public:
  using Callback = std::function<void()>;

  explicit TimelineToolCallbackButton(QWidget *parent = nullptr)
      : QToolButton(parent) {}

  void setCallback(Callback callback) { callback_ = std::move(callback); }

protected:
  void mouseReleaseEvent(QMouseEvent *event) override {
    QToolButton::mouseReleaseEvent(event);
    if (!isEnabled() || !callback_ || !event ||
        event->button() != Qt::LeftButton || !rect().contains(event->pos())) {
      return;
    }
    callback_();
  }

private:
  Callback callback_;
};

QString audioWaveformCacheKey(const CompositionID &compositionId,
                              const LayerID &layerId) {
  return QStringLiteral("%1|%2").arg(compositionId.toString(), layerId.toString());
}

QString audioWaveformSignatureForLayer(const ArtifactAbstractLayer &layer,
                                       const double fps) {
  QString signature = QStringLiteral("%1|%2|%3|%4|%5|%6")
                          .arg(layer.className().toQString(),
                               QString::number(layer.inPoint().framePosition()),
                               QString::number(layer.outPoint().framePosition()),
                               QString::number(layer.startTime().framePosition()),
                               QString::number(fps, 'f', 3),
                               layer.hasAudio() ? QStringLiteral("1")
                                                : QStringLiteral("0"));

  if (const auto *audioLayer = dynamic_cast<const ArtifactAudioLayer *>(&layer)) {
    signature += QStringLiteral("|audio:%1:%2:%3:%4")
                     .arg(audioLayer->sourcePath().trimmed(),
                          QString::number(audioLayer->totalFrames()),
                          QString::number(audioLayer->sampleRate()),
                          QString::number(audioLayer->channelCount()));
  } else if (const auto *videoLayer = dynamic_cast<const ArtifactVideoLayer *>(&layer)) {
    const auto &streamInfo = videoLayer->streamInfo();
    signature += QStringLiteral("|video:%1:%2:%3:%4:%5")
                     .arg(videoLayer->sourcePath().trimmed(),
                          QString::number(streamInfo.duration, 'f', 3),
                          QString::number(streamInfo.audioSampleRate),
                          QString::number(streamInfo.audioChannels),
                          videoLayer->isAudioMuted() ? QStringLiteral("muted")
                                                     : QStringLiteral("audible"));
  }

  return signature;
}

std::optional<CachedAudioWaveform> buildAudioWaveformForLayer(
    ArtifactAbstractLayer &layer,
    const double fps,
    const int waveformBins = 128) {
  if (!layer.hasAudio() || fps <= 0.0) {
    return std::nullopt;
  }

  const int bins = std::clamp(waveformBins, 64, 256);
  CachedAudioWaveform cached;
  cached.signature = audioWaveformSignatureForLayer(layer, fps);

  if (const auto *audioLayer = dynamic_cast<const ArtifactAudioLayer *>(&layer)) {
    if (!audioLayer->isLoaded() || audioLayer->sourcePath().trimmed().isEmpty() ||
        audioLayer->sampleRate() <= 0 || audioLayer->duration() <= 0.0) {
      return std::nullopt;
    }

    const auto waveform = audioLayer->buildWaveformData(bins);
    if (waveform.peaks.isEmpty()) {
      return std::nullopt;
    }

    cached.peaks = waveform.peaks;
    cached.rms = waveform.rms;
    return cached;
  }

  AudioSegment segment;
  const int64_t clipFrames = std::max<int64_t>(
      1, layer.outPoint().framePosition() - layer.inPoint().framePosition());
  const int sampleRate = 48000;
  const int frameCount = std::max<int>(
      1, static_cast<int>(std::ceil((static_cast<double>(clipFrames) / fps) * sampleRate)));
  if (!layer.getAudio(segment, layer.inPoint(), frameCount, sampleRate) ||
      segment.frameCount() <= 0) {
    return std::nullopt;
  }

  AudioWaveformGenerator generator;
  const auto waveform = generator.generate(segment, bins);
  if (waveform.peaks.isEmpty()) {
    return std::nullopt;
  }

  cached.peaks = waveform.peaks;
  cached.rms = waveform.rms;
  return cached;
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

  if (preserveExistingDuration && inPointDelta != 0) {
    const double fps = std::max(
        1.0, static_cast<double>(comp->frameRate().framerate()));
    const int64_t frameScale = static_cast<int64_t>(std::llround(fps));
    for (const auto &group : layer->getLayerPropertyGroups()) {
      for (const auto &property : group.sortedProperties()) {
        if (!property || !property->isAnimatable()) {
          continue;
        }

        const auto keyframes = property->getKeyFrames();
        if (keyframes.empty()) {
          continue;
        }

        property->clearKeyFrames();
        for (const auto &keyframe : keyframes) {
          const int64_t oldFrame = keyframe.time.rescaledTo(frameScale);
          const int64_t newFrame =
              std::max<int64_t>(0, oldFrame + inPointDelta);
          property->addKeyFrame(
              RationalTime(newFrame, frameScale),
              keyframe.value.isValid() ? keyframe.value : property->getValue(),
              keyframe.interpolation, keyframe.cp1_x, keyframe.cp1_y,
              keyframe.cp2_x, keyframe.cp2_y, keyframe.roving);
        }
      }
    }
  }

  layer->changed();
  
  // layer::changed() でレンダラーとタイムライン Track ビューに通知する。
  ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
      LayerChangedEvent{compositionId.toString(), layerIdText,
                        LayerChangedEvent::ChangeType::Modified});
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

QVector<ArtifactAbstractLayerPtr> selectedTimelineLayers(
    ArtifactLayerSelectionManager *selectionManager)
{
  QVector<ArtifactAbstractLayerPtr> layers;
  if (!selectionManager) {
    return layers;
  }

  QSet<ArtifactAbstractLayerPtr> selected = selectionManager->selectedLayers();
  const auto current = selectionManager->currentLayer();

  if (selected.isEmpty()) {
    if (current) {
      layers.push_back(current);
    }
    return layers;
  }

  if (selected.size() == 1) {
    layers.push_back(*selected.begin());
    return layers;
  }

  if (current && selected.contains(current)) {
    layers.push_back(current);
  }

  for (const auto& layer : selected) {
    if (!layer || (current && layer == current)) {
      continue;
    }
    layers.push_back(layer);
  }

  std::sort(layers.begin(), layers.end(),
            [](const ArtifactAbstractLayerPtr& lhs,
               const ArtifactAbstractLayerPtr& rhs) {
              if (!lhs || !rhs) {
                return static_cast<bool>(lhs) && !static_cast<bool>(rhs);
              }
              const int nameCompare =
                  lhs->layerName().compare(rhs->layerName(), Qt::CaseInsensitive);
              if (nameCompare != 0) {
                return nameCompare < 0;
              }
              return lhs->id().toString() < rhs->id().toString();
            });
  return layers;
}

QString keyframeSelectionKey(const LayerID& layerId,
                             const QString& propertyPath,
                             const qint64 frame)
{
  return QStringLiteral("%1|%2|%3")
      .arg(layerId.toString(), propertyPath, QString::number(frame));
}

struct KeyframePropertyRef {
  LayerID layerId;
  QString propertyPath;
};

struct KeyframePropertySnapshot {
  LayerID layerId;
  QString propertyPath;
  std::vector<ArtifactCore::KeyFrame> keyframes;
};

struct KeyPatternTarget {
  ArtifactAbstractLayerPtr layer;
  LayerID layerId;
  QString propertyPath;
};

QSet<QString> keyframeSelectionKeysFromMarkers(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>& markers)
{
  QSet<QString> keys;
  for (const auto& marker : markers) {
    const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
    keys.insert(keyframeSelectionKey(marker.layerId, marker.propertyPath, frame));
  }
  return keys;
}

QVector<KeyframePropertyRef> collectAnimatablePropertyRefs(
    const QVector<ArtifactAbstractLayerPtr>& layers)
{
  QVector<KeyframePropertyRef> refs;
  QSet<QString> seen;
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
        const QString key = QStringLiteral("%1|%2")
                                .arg(layer->id().toString(), property->getName());
        if (seen.contains(key)) {
          continue;
        }
        seen.insert(key);
        refs.push_back({layer->id(), property->getName()});
      }
    }
  }
  return refs;
}

QVector<KeyframePropertyRef> collectPropertyRefsFromMarkers(
    const ArtifactCompositionPtr& composition,
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>& markers)
{
  QVector<KeyframePropertyRef> refs;
  if (!composition) {
    return refs;
  }

  QSet<QString> seen;
  for (const auto& marker : markers) {
    const QString key = QStringLiteral("%1|%2")
                            .arg(marker.layerId.toString(), marker.propertyPath);
    if (seen.contains(key)) {
      continue;
    }
    seen.insert(key);
    refs.push_back({marker.layerId, marker.propertyPath});
  }
  return refs;
}

QVector<KeyframePropertyRef> collectPropertyRefsFromClipboardRecords(
    const ArtifactCompositionPtr& composition,
    const QVector<ArtifactAbstractLayerPtr>& layers,
    const QJsonArray& records)
{
  QVector<KeyframePropertyRef> refs;
  if (!composition || layers.isEmpty() || records.isEmpty()) {
    return refs;
  }

  QSet<QString> propertyPaths;
  for (const auto& value : records) {
    if (!value.isObject()) {
      continue;
    }
    const QString propertyPath =
        value.toObject().value(QStringLiteral("propertyPath")).toString().trimmed();
    if (propertyPath.isEmpty()) {
      continue;
    }
    propertyPaths.insert(propertyPath);
  }

  if (propertyPaths.isEmpty()) {
    return refs;
  }

  QSet<QString> seen;
  for (const auto& layer : layers) {
    if (!layer) {
      continue;
    }
    for (const auto& propertyPath : propertyPaths) {
      const QString key =
          QStringLiteral("%1|%2").arg(layer->id().toString(), propertyPath);
      if (seen.contains(key)) {
        continue;
      }
      seen.insert(key);
      refs.push_back({layer->id(), propertyPath});
    }
  }
  return refs;
}

QVector<KeyPatternTarget> collectKeyPatternTargets(
    const QVector<ArtifactAbstractLayerPtr>& layers, const QString& propertyPath)
{
  QVector<KeyPatternTarget> targets;
  const QString trimmedPath = propertyPath.trimmed();
  if (layers.isEmpty() || trimmedPath.isEmpty()) {
    return targets;
  }

  for (const auto& layer : layers) {
    if (!layer) {
      continue;
    }
    targets.push_back({layer, layer->id(), trimmedPath});
  }

  return targets;
}

QVector<KeyframePropertySnapshot> captureKeyframePropertySnapshots(
    const ArtifactCompositionPtr& composition,
    const QVector<KeyframePropertyRef>& refs);

void applyKeyframePropertySnapshots(const ArtifactCompositionPtr& composition,
                                    const QVector<KeyframePropertySnapshot>& snapshots);

class TimelineKeyframeSnapshotCommand final : public UndoCommand {
public:
  TimelineKeyframeSnapshotCommand(QString label, std::function<void()> redoFunc,
                                  std::function<void()> undoFunc)
      : label_(std::move(label)), redoFunc_(std::move(redoFunc)),
        undoFunc_(std::move(undoFunc)) {}

  void undo() override {
    if (undoFunc_) {
      undoFunc_();
    }
  }

  void redo() override {
    if (redoFunc_) {
      redoFunc_();
    }
  }

  QString label() const override { return label_; }

private:
  QString label_;
  std::function<void()> redoFunc_;
  std::function<void()> undoFunc_;
};

bool applyKeyPatternToTargets(
    const ArtifactCompositionPtr& composition,
    const QVector<KeyPatternTarget>& targets,
    const ArtifactCore::KeyframePatternRequest& request,
    QString* outMessage = nullptr,
    std::function<void()> afterChange = {})
{
  if (!composition || targets.isEmpty()) {
    if (outMessage) {
      *outMessage = QStringLiteral("No valid key pattern targets were found.");
    }
    return false;
  }

  QVector<KeyframePropertyRef> refs;
  refs.reserve(targets.size());
  for (const auto& target : targets) {
    refs.push_back({target.layerId, target.propertyPath});
  }
  const auto beforeSnapshots = captureKeyframePropertySnapshots(composition, refs);

  const int frameScale = std::max(
      1, static_cast<int>(std::llround(composition->frameRate().framerate())));
  bool appliedAny = false;
  int targetIndex = 0;
  for (const auto& target : targets) {
    if (!target.layer) {
      ++targetIndex;
      continue;
    }
    const auto property = target.layer->getProperty(target.propertyPath);
    if (!property) {
      ++targetIndex;
      continue;
    }

    ArtifactCore::KeyframePatternRequest perTarget = request;
    perTarget.frameScale = frameScale;
    perTarget.selectionIndex = targetIndex;
    perTarget.selectionCount = std::max(1, static_cast<int>(targets.size()));
    perTarget.baseValue = property->getValue();
    if (!perTarget.targetValue.isValid()) {
      bool ok = false;
      const double baseNumeric = perTarget.baseValue.toDouble(&ok);
      if (ok) {
        perTarget.targetValue = baseNumeric + std::max(1.0, perTarget.amplitude);
      } else {
        perTarget.targetValue = perTarget.baseValue;
      }
    }

    const auto generated = ArtifactCore::KeyframePatternGenerator::generate(perTarget);
    if (generated.keyframes.isEmpty()) {
      ++targetIndex;
      continue;
    }

    property->setAnimatable(true);
    property->clearKeyFrames();
    for (const auto& keyframe : generated.keyframes) {
      property->addKeyFrame(keyframe.time, keyframe.value, keyframe.interpolation,
                            keyframe.cp1_x, keyframe.cp1_y, keyframe.cp2_x,
                            keyframe.cp2_y, keyframe.roving);
    }
    target.layer->setDirty();
    target.layer->changed();
    appliedAny = true;
    ++targetIndex;
  }

  if (!appliedAny) {
    if (outMessage) {
      *outMessage = QStringLiteral("No animatable properties were updated.");
    }
    return false;
  }

  if (auto* mgr = UndoManager::instance()) {
    const auto afterSnapshots = captureKeyframePropertySnapshots(composition, refs);
    mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
        QStringLiteral("Generate Key Pattern"),
        [composition, afterSnapshots, afterChange]() {
          applyKeyframePropertySnapshots(composition, afterSnapshots);
          if (afterChange) {
            afterChange();
          }
        },
        [composition, beforeSnapshots, afterChange]() {
          applyKeyframePropertySnapshots(composition, beforeSnapshots);
          if (afterChange) {
            afterChange();
          }
        }));
  }

  if (afterChange) {
    afterChange();
  }

  if (outMessage) {
    *outMessage = QStringLiteral("Generated key pattern for %1 target(s).")
                      .arg(static_cast<int>(targets.size()));
  }
  return true;
}

QVector<KeyframePropertySnapshot> captureKeyframePropertySnapshots(
    const ArtifactCompositionPtr& composition,
    const QVector<KeyframePropertyRef>& refs)
{
  QVector<KeyframePropertySnapshot> snapshots;
  if (!composition || refs.isEmpty()) {
    return snapshots;
  }

  QSet<QString> seen;
  for (const auto& ref : refs) {
    const QString key =
        QStringLiteral("%1|%2").arg(ref.layerId.toString(), ref.propertyPath);
    if (seen.contains(key)) {
      continue;
    }
    seen.insert(key);

    const auto layer = composition->layerById(ref.layerId);
    if (!layer) {
      continue;
    }
    const auto property = findLayerPropertyByPath(layer, ref.propertyPath);
    if (!property) {
      continue;
    }

    snapshots.push_back(
        KeyframePropertySnapshot{ref.layerId, ref.propertyPath, property->getKeyFrames()});
  }

  return snapshots;
}

void applyKeyframePropertySnapshots(const ArtifactCompositionPtr& composition,
                                    const QVector<KeyframePropertySnapshot>& snapshots)
{
  if (!composition || snapshots.isEmpty()) {
    return;
  }

  QSet<QString> changedLayerKeys;
  QVector<LayerID> changedLayers;
  for (const auto& snapshot : snapshots) {
    const auto layer = composition->layerById(snapshot.layerId);
    if (!layer) {
      continue;
    }
    const auto property = findLayerPropertyByPath(layer, snapshot.propertyPath);
    if (!property) {
      continue;
    }

    property->clearKeyFrames();
    const double fps =
        std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
    for (const auto& keyframe : snapshot.keyframes) {
      const int64_t scale = static_cast<int64_t>(std::llround(fps));
      property->addKeyFrame(RationalTime(keyframe.time.rescaledTo(scale), scale), keyframe.value,
                            keyframe.interpolation, keyframe.cp1_x,
                            keyframe.cp1_y, keyframe.cp2_x, keyframe.cp2_y,
                            keyframe.roving);
    }
    const QString layerKey = layer->id().toString();
    if (!changedLayerKeys.contains(layerKey)) {
      changedLayerKeys.insert(layerKey);
      changedLayers.push_back(layer->id());
    }
  }

  for (const auto& layerId : changedLayers) {
    const auto layer = composition->layerById(layerId);
    if (!layer) {
      continue;
    }
    layer->changed();
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{composition->id().toString(), layer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }
}

void shiftAnimatableLayerKeyframes(const ArtifactCompositionPtr& composition,
                                   const ArtifactAbstractLayerPtr& layer,
                                   const qint64 frameDelta)
{
  if (!composition || !layer || frameDelta == 0) {
    return;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  const int64_t frameScale = static_cast<int64_t>(std::llround(fps));

  for (const auto& group : layer->getLayerPropertyGroups()) {
    for (const auto& property : group.sortedProperties()) {
      if (!property || !property->isAnimatable()) {
        continue;
      }

      const auto keyframes = property->getKeyFrames();
      if (keyframes.empty()) {
        continue;
      }

      property->clearKeyFrames();
      for (const auto& keyframe : keyframes) {
        const int64_t oldFrame = keyframe.time.rescaledTo(frameScale);
        const int64_t newFrame =
            std::max<int64_t>(0, oldFrame + frameDelta);
        property->addKeyFrame(
            RationalTime(newFrame, frameScale),
            keyframe.value.isValid() ? keyframe.value : property->getValue(),
            keyframe.interpolation, keyframe.cp1_x, keyframe.cp1_y,
            keyframe.cp2_x, keyframe.cp2_y, keyframe.roving);
      }
    }
  }
}

struct TimelineLayerStateSnapshot {
  LayerID layerId;
  qint64 inPoint = 0;
  qint64 outPoint = 0;
  qint64 startTime = 0;
  QVector<KeyframePropertySnapshot> keyframes;
};

TimelineLayerStateSnapshot captureTimelineLayerStateSnapshot(
    const ArtifactCompositionPtr& composition,
    const ArtifactAbstractLayerPtr& layer)
{
  TimelineLayerStateSnapshot snapshot;
  if (!composition || !layer) {
    return snapshot;
  }

  snapshot.layerId = layer->id();
  snapshot.inPoint = layer->inPoint().framePosition();
  snapshot.outPoint = layer->outPoint().framePosition();
  snapshot.startTime = layer->startTime().framePosition();

  const QVector<ArtifactAbstractLayerPtr> layers{layer};
  snapshot.keyframes = captureKeyframePropertySnapshots(
      composition, collectAnimatablePropertyRefs(layers));
  return snapshot;
}

QVector<TimelineLayerStateSnapshot> captureTimelineLayerStateSnapshots(
    const ArtifactCompositionPtr& composition,
    const QVector<ArtifactAbstractLayerPtr>& layers)
{
  QVector<TimelineLayerStateSnapshot> snapshots;
  if (!composition || layers.isEmpty()) {
    return snapshots;
  }

  snapshots.reserve(layers.size());
  for (const auto& layer : layers) {
    if (!layer) {
      continue;
    }
    snapshots.push_back(captureTimelineLayerStateSnapshot(composition, layer));
  }
  return snapshots;
}

void restoreTimelineLayerStateSnapshot(const ArtifactCompositionPtr& composition,
                                      const TimelineLayerStateSnapshot& snapshot)
{
  if (!composition || snapshot.layerId.isNil()) {
    return;
  }

  const auto layer = composition->layerById(snapshot.layerId);
  if (!layer) {
    return;
  }

  layer->setInPoint(FramePosition(snapshot.inPoint));
  layer->setOutPoint(FramePosition(snapshot.outPoint));
  layer->setStartTime(FramePosition(snapshot.startTime));

  if (!snapshot.keyframes.isEmpty()) {
    applyKeyframePropertySnapshots(composition, snapshot.keyframes);
    return;
  }

  layer->changed();
  ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
      LayerChangedEvent{composition->id().toString(), layer->id().toString(),
                        LayerChangedEvent::ChangeType::Modified});
}

void restoreTimelineLayerStateSnapshots(
    const ArtifactCompositionPtr& composition,
    const QVector<TimelineLayerStateSnapshot>& snapshots)
{
  if (!composition || snapshots.isEmpty()) {
    return;
  }

  for (const auto& snapshot : snapshots) {
    restoreTimelineLayerStateSnapshot(composition, snapshot);
  }
}

QVector<ArtifactAbstractLayerPtr> collectRippleLaterLayers(
    const ArtifactCompositionPtr& composition, const LayerID& targetLayerId,
    const qint64 boundaryFrame)
{
  QVector<ArtifactAbstractLayerPtr> layers;
  if (!composition) {
    return layers;
  }

  for (const auto& layer : composition->allLayer()) {
    if (!layer || layer->id() == targetLayerId || layer->isLocked()) {
      continue;
    }
    if (layer->inPoint().framePosition() < boundaryFrame) {
      continue;
    }
    layers.push_back(layer);
  }
  return layers;
}

bool applyTimelineRippleTrimOut(const CompositionID& compositionId,
                                const QString& layerIdText,
                                const qint64 currentFrame)
{
  if (layerIdText.trimmed().isEmpty()) {
    return false;
  }

  auto* svc = ArtifactProjectService::instance();
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

  const qint64 oldInPoint = layer->inPoint().framePosition();
  const qint64 oldOutPoint = layer->outPoint().framePosition();
  const qint64 newOutPoint =
      std::max<qint64>(oldInPoint + 1, currentFrame);
  const qint64 oldDuration = std::max<qint64>(1, oldOutPoint - oldInPoint);
  const qint64 newDuration = std::max<qint64>(1, newOutPoint - oldInPoint);
  const qint64 rippleDelta = newDuration - oldDuration;

  const auto rippleLayers =
      collectRippleLaterLayers(comp, layer->id(), oldOutPoint);

  if (!applyTimelineLayerRangeEdit(compositionId, layerIdText, oldInPoint,
                                   newDuration, false)) {
    return false;
  }

  if (rippleDelta == 0 || rippleLayers.isEmpty()) {
    return true;
  }

  for (const auto& rippleLayer : rippleLayers) {
    if (!rippleLayer) {
      continue;
    }

    const qint64 followerOldIn = rippleLayer->inPoint().framePosition();
    const qint64 followerOldOut = rippleLayer->outPoint().framePosition();
    const qint64 followerOldStart = rippleLayer->startTime().framePosition();
    const qint64 followerNewIn = std::max<qint64>(0, followerOldIn + rippleDelta);
    const qint64 actualDelta = followerNewIn - followerOldIn;
    if (actualDelta == 0) {
      continue;
    }

    rippleLayer->setInPoint(FramePosition(followerNewIn));
    rippleLayer->setOutPoint(
        FramePosition(std::max<qint64>(followerNewIn + 1,
                                       followerOldOut + actualDelta)));
    // target 側の applyTimelineLayerRangeEdit と整合させるため、startTime も
    // in/out と同じ delta で移動させる。これを忘れると follower の内部タイミング
    // （source clip の再生開始位置）が in/out だけズレて破綻する。
    rippleLayer->setStartTime(
        FramePosition(std::max<qint64>(0, followerOldStart + actualDelta)));
    shiftAnimatableLayerKeyframes(comp, rippleLayer, actualDelta);
    rippleLayer->changed();
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{compositionId.toString(), rippleLayer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }

  return true;
}

class RippleTrimOutCommand final : public UndoCommand {
public:
  RippleTrimOutCommand(CompositionID compositionId, LayerID layerId,
                       qint64 currentFrame,
                       QVector<TimelineLayerStateSnapshot> beforeSnapshots)
      : compositionId_(std::move(compositionId)),
        layerId_(std::move(layerId)), currentFrame_(currentFrame),
        beforeSnapshots_(std::move(beforeSnapshots)) {}

  void undo() override
  {
    auto comp = safeCompositionLookup(compositionId_);
    if (!comp) {
      return;
    }

    restoreTimelineLayerStateSnapshots(comp, beforeSnapshots_);
    if (auto* mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  void redo() override
  {
    if (applyTimelineRippleTrimOut(compositionId_, layerId_.toString(),
                                   currentFrame_)) {
      if (auto* mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
      }
    }
  }

  QString label() const override { return QStringLiteral("Ripple Trim Out"); }

private:
  CompositionID compositionId_;
  LayerID layerId_;
  qint64 currentFrame_ = 0;
  QVector<TimelineLayerStateSnapshot> beforeSnapshots_;
};

// Phase 2: Ripple Trim In
bool applyTimelineRippleTrimIn(const CompositionID& compositionId,
                               const QString& layerIdText,
                               const qint64 currentFrame)
{
  if (layerIdText.trimmed().isEmpty()) {
    return false;
  }

  auto* svc = ArtifactProjectService::instance();
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

  const qint64 oldInPoint = layer->inPoint().framePosition();
  const qint64 oldOutPoint = layer->outPoint().framePosition();
  const qint64 newInPoint =
      std::max<qint64>(oldInPoint,
                       std::min<qint64>(oldOutPoint - 1, currentFrame));
  const qint64 rippleDelta = -(newInPoint - oldInPoint);

  const auto rippleLayers =
      collectRippleLaterLayers(comp, layer->id(), oldInPoint);

  // target 側: inPoint を詰め、startTime と keyframe を追従させる。outPoint は動かさない。
  const qint64 newOutPoint = oldOutPoint;
  const qint64 inPointDelta = newInPoint - oldInPoint;

  layer->setInPoint(FramePosition(newInPoint));
  layer->setOutPoint(FramePosition(newOutPoint));
  if (inPointDelta != 0) {
    const qint64 oldStartTime = layer->startTime().framePosition();
    layer->setStartTime(FramePosition(std::max<qint64>(0, oldStartTime + inPointDelta)));
    shiftAnimatableLayerKeyframes(comp, layer, inPointDelta);
    layer->changed();
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{compositionId.toString(), layer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }

  if (rippleDelta == 0 || rippleLayers.isEmpty()) {
    return true;
  }

  for (const auto& rippleLayer : rippleLayers) {
    if (!rippleLayer) {
      continue;
    }

    const qint64 followerOldIn = rippleLayer->inPoint().framePosition();
    const qint64 followerOldOut = rippleLayer->outPoint().framePosition();
    const qint64 followerOldStart = rippleLayer->startTime().framePosition();
    const qint64 followerNewIn = std::max<qint64>(0, followerOldIn + rippleDelta);
    const qint64 actualDelta = followerNewIn - followerOldIn;
    if (actualDelta == 0) {
      continue;
    }

    rippleLayer->setInPoint(FramePosition(followerNewIn));
    rippleLayer->setOutPoint(
        FramePosition(std::max<qint64>(followerNewIn + 1,
                                       followerOldOut + actualDelta)));
    rippleLayer->setStartTime(
        FramePosition(std::max<qint64>(0, followerOldStart + actualDelta)));
    shiftAnimatableLayerKeyframes(comp, rippleLayer, actualDelta);
    rippleLayer->changed();
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{compositionId.toString(), rippleLayer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }

  return true;
}

// Phase 2: Ripple Delete (Close Gap) - target を 0 幅に潰して後続を詰める。
bool applyTimelineRippleDelete(const CompositionID& compositionId,
                               const QString& layerIdText)
{
  if (layerIdText.trimmed().isEmpty()) {
    return false;
  }

  auto* svc = ArtifactProjectService::instance();
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

  const qint64 oldInPoint = layer->inPoint().framePosition();
  const qint64 oldOutPoint = layer->outPoint().framePosition();
  const qint64 oldDuration = std::max<qint64>(1, oldOutPoint - oldInPoint);
  const qint64 rippleDelta = -oldDuration;

  const auto rippleLayers =
      collectRippleLaterLayers(comp, layer->id(), oldInPoint);

  layer->setInPoint(FramePosition(oldInPoint));
  layer->setOutPoint(FramePosition(oldInPoint + 1));
  layer->changed();
  ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
      LayerChangedEvent{compositionId.toString(), layer->id().toString(),
                        LayerChangedEvent::ChangeType::Modified});

  if (rippleDelta == 0 || rippleLayers.isEmpty()) {
    return true;
  }

  for (const auto& rippleLayer : rippleLayers) {
    if (!rippleLayer) {
      continue;
    }

    const qint64 followerOldIn = rippleLayer->inPoint().framePosition();
    const qint64 followerOldOut = rippleLayer->outPoint().framePosition();
    const qint64 followerOldStart = rippleLayer->startTime().framePosition();
    const qint64 followerNewIn = std::max<qint64>(0, followerOldIn + rippleDelta);
    const qint64 actualDelta = followerNewIn - followerOldIn;
    if (actualDelta == 0) {
      continue;
    }

    rippleLayer->setInPoint(FramePosition(followerNewIn));
    rippleLayer->setOutPoint(
        FramePosition(std::max<qint64>(followerNewIn + 1,
                                       followerOldOut + actualDelta)));
    rippleLayer->setStartTime(
        FramePosition(std::max<qint64>(0, followerOldStart + actualDelta)));
    shiftAnimatableLayerKeyframes(comp, rippleLayer, actualDelta);
    rippleLayer->changed();
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{compositionId.toString(), rippleLayer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }

  return true;
}

class RippleTrimInCommand final : public UndoCommand {
public:
  RippleTrimInCommand(CompositionID compositionId, LayerID layerId,
                      qint64 currentFrame,
                      QVector<TimelineLayerStateSnapshot> beforeSnapshots)
      : compositionId_(std::move(compositionId)),
        layerId_(std::move(layerId)), currentFrame_(currentFrame),
        beforeSnapshots_(std::move(beforeSnapshots)) {}

  void undo() override
  {
    auto comp = safeCompositionLookup(compositionId_);
    if (!comp) {
      return;
    }
    restoreTimelineLayerStateSnapshots(comp, beforeSnapshots_);
    if (auto* mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  void redo() override
  {
    if (applyTimelineRippleTrimIn(compositionId_, layerId_.toString(),
                                  currentFrame_)) {
      if (auto* mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
      }
    }
  }

  QString label() const override { return QStringLiteral("Ripple Trim In"); }

private:
  CompositionID compositionId_;
  LayerID layerId_;
  qint64 currentFrame_ = 0;
  QVector<TimelineLayerStateSnapshot> beforeSnapshots_;
};

class RippleDeleteCommand final : public UndoCommand {
public:
  RippleDeleteCommand(CompositionID compositionId, LayerID layerId,
                      QVector<TimelineLayerStateSnapshot> beforeSnapshots)
      : compositionId_(std::move(compositionId)),
        layerId_(std::move(layerId)),
        beforeSnapshots_(std::move(beforeSnapshots)) {}

  void undo() override
  {
    auto comp = safeCompositionLookup(compositionId_);
    if (!comp) {
      return;
    }
    restoreTimelineLayerStateSnapshots(comp, beforeSnapshots_);
    if (auto* mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  void redo() override
  {
    if (applyTimelineRippleDelete(compositionId_, layerId_.toString())) {
      if (auto* mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
      }
    }
  }

  QString label() const override { return QStringLiteral("Ripple Delete"); }

private:
  CompositionID compositionId_;
  LayerID layerId_;
  QVector<TimelineLayerStateSnapshot> beforeSnapshots_;
};

bool propertyValueAsCurveNumber(const ArtifactCore::AbstractProperty &property,
                               const QVariant &value, double &outValue)
{
  switch (property.getType()) {
  case ArtifactCore::PropertyType::Float:
  case ArtifactCore::PropertyType::Integer:
  case ArtifactCore::PropertyType::Boolean:
    outValue = value.toDouble();
    return true;
  default:
    return false;
  }
}

QString curveTrackLabel(const ArtifactAbstractLayerPtr &layer,
                        const ArtifactCore::AbstractProperty &property)
{
  const QString layerName = layer ? layer->layerName().trimmed() : QString();
  const QString propertyLabel = property.metadata().displayLabel.trimmed();
  const QString effectivePropertyLabel =
      propertyLabel.isEmpty() ? property.getName() : propertyLabel;
  if (layerName.isEmpty()) {
    return effectivePropertyLabel;
  }
  return QStringLiteral("%1 / %2").arg(layerName, effectivePropertyLabel);
}

QColor curveTrackColor(const ArtifactAbstractLayerPtr &layer,
                       const QString &propertyPath)
{
  QColor color = layerTimelineColor(layer);
  if (!color.isValid()) {
    color = QColor(159, 138, 255);
  }

  const int hueShift = static_cast<int>(qHash(propertyPath) % 31) - 15;
  int hue = color.hsvHue();
  if (hue < 0) {
    hue = 220;
  }
  hue = (hue + hueShift + 360) % 360;
  const int sat = std::clamp(color.hsvSaturation() + 16, 70, 255);
  const int val = std::clamp(color.value() + 8, 110, 255);
  color.setHsv(hue, sat, val, 255);
  return color;
}

struct CurveTrackBinding {
  LayerID layerId;
  QString propertyPath;
};

struct CurveEditorSnapshot {
  std::vector<ArtifactCore::CurveTrack> tracks;
  QVector<CurveTrackBinding> bindings;
  QString signature;
  QString summary;
};

QString formatKeyframeCountSummary(int count);
QString formatCurveTrackCountSummary(int count);

int curveTrackIndexForSelection(
    const QVector<CurveTrackBinding>& bindings,
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>& markers)
{
  if (bindings.isEmpty() || markers.isEmpty()) {
    return -1;
  }

  for (const auto& marker : markers) {
    for (int i = 0; i < bindings.size(); ++i) {
      const auto& binding = bindings[i];
      if (binding.layerId == marker.layerId &&
          binding.propertyPath == marker.propertyPath) {
        return i;
      }
    }
  }
  return -1;
}

CurveEditorSnapshot buildCurveEditorSnapshot(
    const ArtifactCompositionPtr &composition,
    ArtifactLayerSelectionManager *selectionManager)
{
  CurveEditorSnapshot snapshot;
  const auto layers = selectedTimelineLayers(selectionManager);
  if (layers.isEmpty()) {
    snapshot.summary = QStringLiteral("Select a layer to continue");
    return snapshot;
  }

  const double fps = timelineFrameRateFallback(composition);

  QStringList signatureParts;
  int totalKeyCount = 0;
  for (const auto &layer : layers) {
    if (!layer) {
      continue;
    }

    const auto layerGroups = layer->getLayerPropertyGroups();
    for (const auto &group : layerGroups) {
      for (const auto &property : group.sortedProperties()) {
        if (!property || !property->isAnimatable()) {
          continue;
        }

        const auto keyframes = property->getKeyFrames();
        if (keyframes.empty()) {
          continue;
        }

        double firstValue = 0.0;
        if (!propertyValueAsCurveNumber(*property, keyframes.front().value, firstValue)) {
          continue;
        }

        ArtifactCore::CurveTrack track;
        track.name = curveTrackLabel(layer, *property);
        track.color = curveTrackColor(layer, property->getName());
        track.visible = true;
        track.keys.reserve(keyframes.size());

        QVector<double> framePositions;
        QVector<double> numericValues;
        framePositions.reserve(static_cast<int>(keyframes.size()));
        numericValues.reserve(static_cast<int>(keyframes.size()));

        bool supported = true;
        for (const auto &keyframe : keyframes) {
          double numericValue = 0.0;
          if (!propertyValueAsCurveNumber(*property, keyframe.value, numericValue)) {
            supported = false;
            break;
          }
          framePositions.push_back(static_cast<double>(
              keyframe.time.rescaledTo(static_cast<int64_t>(std::round(fps)))));
          numericValues.push_back(numericValue);
        }

        if (!supported || framePositions.isEmpty()) {
          continue;
        }

        for (int i = 0; i < static_cast<int>(framePositions.size()); ++i) {
          const double frame = framePositions[i];
          const double value = numericValues[i];

          const double prevFrame = (i > 0) ? framePositions[i - 1] : frame;
          const double nextFrame = (i + 1 < framePositions.size())
                                       ? framePositions[i + 1]
                                       : frame;
          const double prevValue = (i > 0) ? numericValues[i - 1] : value;
          const double nextValue = (i + 1 < numericValues.size())
                                       ? numericValues[i + 1]
                                       : value;

          ArtifactCore::CurveKey curveKey;
          curveKey.frame = static_cast<int64_t>(std::llround(frame));
          curveKey.value = static_cast<float>(value);
           if (keyframes[i].interpolation != ArtifactCore::InterpolationType::Constant) {
            const double inFrameSpan = std::max(1.0, (frame - prevFrame) / 3.0);
            const double outFrameSpan = std::max(1.0, (nextFrame - frame) / 3.0);
            const double inSlope = (frame > prevFrame) ? ((value - prevValue) / (frame - prevFrame)) : 0.0;
            const double outSlope = (nextFrame > frame) ? ((nextValue - value) / (nextFrame - frame)) : 0.0;

            curveKey.inHandleFrame = -static_cast<int64_t>(std::llround(inFrameSpan));
            curveKey.outHandleFrame = static_cast<int64_t>(std::llround(outFrameSpan));
            curveKey.inHandleValue = static_cast<float>(-inSlope * inFrameSpan);
            curveKey.outHandleValue = static_cast<float>(outSlope * outFrameSpan);
          }

          track.keys.push_back(curveKey);
          signatureParts.push_back(QStringLiteral("%1:%2:%3:%4")
                                       .arg(layer->id().toString(),
                                            property->getName(),
                                            QString::number(curveKey.frame),
                                            QString::number(curveKey.value, 'f', 6)));
        }

        totalKeyCount += static_cast<int>(track.keys.size());
        snapshot.bindings.push_back(CurveTrackBinding{layer->id(), property->getName()});
        snapshot.tracks.push_back(std::move(track));
      }
    }
  }

  snapshot.signature = signatureParts.join(QLatin1Char('|'));
  if (snapshot.tracks.empty()) {
    snapshot.summary = QStringLiteral("No numeric keyframes");
  } else {
    snapshot.summary = QStringLiteral("%1, %2")
                          .arg(formatCurveTrackCountSummary(static_cast<int>(snapshot.tracks.size())))
                          .arg(formatKeyframeCountSummary(totalKeyCount));
  }
  return snapshot;
}

QString curveEditorSummaryForTracks(
    const std::vector<ArtifactCore::CurveTrack> &tracks) {
  int totalKeyCount = 0;
  for (const auto &track : tracks) {
    totalKeyCount += static_cast<int>(track.keys.size());
  }
  if (tracks.empty()) {
    return QStringLiteral("No numeric keyframes");
  }
  return QStringLiteral("%1, %2")
      .arg(formatCurveTrackCountSummary(static_cast<int>(tracks.size())))
      .arg(formatKeyframeCountSummary(totalKeyCount));
}

std::shared_ptr<ArtifactCore::AbstractProperty> findLayerPropertyByPath(
    const ArtifactAbstractLayerPtr& layer, const QString& propertyPath);

QVector<qint64> collectSelectedKeyframeFrames(
    const ArtifactCompositionPtr& composition,
    ArtifactLayerSelectionManager* selectionManager)
{
  QVector<qint64> frames;
  if (!composition || !selectionManager) {
    return frames;
  }

  const auto layers = selectedTimelineLayers(selectionManager);
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

QJsonArray serializeSelectedKeyframes(
    const ArtifactCompositionPtr& composition,
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>& markers)
{
  QJsonArray keyframes;
  if (!composition || markers.isEmpty()) {
    return keyframes;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  QSet<QString> seen;
  for (const auto& marker : markers) {
    const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
    const QString dedupeKey =
        QStringLiteral("%1|%2|%3").arg(marker.layerId.toString(), marker.propertyPath,
                                        QString::number(frame));
    if (seen.contains(dedupeKey)) {
      continue;
    }
    seen.insert(dedupeKey);

    auto layer = composition->layerById(marker.layerId);
    if (!layer) {
      continue;
    }
    const auto property = findLayerPropertyByPath(layer, marker.propertyPath);
    if (!property) {
      continue;
    }

    const RationalTime time(frame, static_cast<int64_t>(std::llround(fps)));
    const auto keyframesAtProperty = property->getKeyFrames();
    const auto it = std::find_if(keyframesAtProperty.cbegin(), keyframesAtProperty.cend(),
                                 [&time](const ArtifactCore::KeyFrame& keyframe) {
                                   return keyframe.time == time;
                                 });
    if (it == keyframesAtProperty.cend()) {
      continue;
    }

    QJsonObject record;
    record.insert(QStringLiteral("layerId"), marker.layerId.toString());
    record.insert(QStringLiteral("propertyPath"), marker.propertyPath);
    record.insert(QStringLiteral("frame"), static_cast<qint64>(frame));
    record.insert(QStringLiteral("value"), QJsonValue::fromVariant(it->value));
    record.insert(QStringLiteral("interpolation"), static_cast<int>(it->interpolation));
    record.insert(QStringLiteral("cp1_x"), it->cp1_x);
    record.insert(QStringLiteral("cp1_y"), it->cp1_y);
    record.insert(QStringLiteral("cp2_x"), it->cp2_x);
    record.insert(QStringLiteral("cp2_y"), it->cp2_y);
    keyframes.append(record);
  }

  return keyframes;
}

bool pasteKeyframesToLayers(
    const ArtifactCompositionPtr& composition,
    const QVector<ArtifactAbstractLayerPtr>& targetLayers,
    const QJsonArray& records,
    const qint64 targetFrame,
    QSet<QString>* outSelectionKeys = nullptr,
    int* outMergedExistingKeyframeCount = nullptr)
{
  if (!composition || targetLayers.isEmpty() || records.isEmpty()) {
    return false;
  }

  QVector<QJsonObject> sourceRecords;
  sourceRecords.reserve(records.size());
  qint64 minFrame = std::numeric_limits<qint64>::max();
  for (const auto& value : records) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject record = value.toObject();
    const qint64 frame = record.value(QStringLiteral("frame")).toVariant().toLongLong();
    if (record.value(QStringLiteral("propertyPath")).toString().trimmed().isEmpty()) {
      continue;
    }
    minFrame = std::min(minFrame, frame);
    sourceRecords.push_back(record);
  }

  if (sourceRecords.isEmpty() || minFrame == std::numeric_limits<qint64>::max()) {
    return false;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  if (outSelectionKeys) {
    outSelectionKeys->clear();
  }
  int mergedExistingKeyframeCount = 0;
  if (outMergedExistingKeyframeCount) {
    *outMergedExistingKeyframeCount = 0;
  }
  bool changed = false;
  for (const auto& layer : targetLayers) {
    if (!layer) {
      continue;
    }
    bool layerChanged = false;
    for (const auto& record : sourceRecords) {
      const QString propertyPath = record.value(QStringLiteral("propertyPath")).toString();
      const auto property = findLayerPropertyByPath(layer, propertyPath);
      if (!property || !property->isAnimatable()) {
        continue;
      }

      const qint64 sourceFrame =
          record.value(QStringLiteral("frame")).toVariant().toLongLong();
      const qint64 offset = sourceFrame - minFrame;
      const qint64 newFrame = std::max<qint64>(0, targetFrame + offset);
      const RationalTime time(newFrame, static_cast<int64_t>(std::llround(fps)));
      const QVariant value = record.value(QStringLiteral("value")).toVariant();
      const auto interpolationValue =
          static_cast<ArtifactCore::InterpolationType>(
              record.value(QStringLiteral("interpolation"))
                  .toInt(static_cast<int>(ArtifactCore::InterpolationType::Linear)));
      const float cp1_x =
          static_cast<float>(record.value(QStringLiteral("cp1_x")).toDouble(0.42));
      const float cp1_y =
          static_cast<float>(record.value(QStringLiteral("cp1_y")).toDouble(0.0));
      const float cp2_x =
          static_cast<float>(record.value(QStringLiteral("cp2_x")).toDouble(0.58));
      const float cp2_y =
          static_cast<float>(record.value(QStringLiteral("cp2_y")).toDouble(1.0));

      if (property->hasKeyFrameAt(time)) {
        ++mergedExistingKeyframeCount;
      }
      property->addKeyFrame(time,
                            value.isValid() ? value : property->getValue(),
                            interpolationValue, cp1_x, cp1_y, cp2_x, cp2_y);
      if (outSelectionKeys) {
        outSelectionKeys->insert(keyframeSelectionKey(
            layer->id(), propertyPath, newFrame));
      }
      layerChanged = true;
    }
    if (layerChanged) {
      layer->changed();
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{composition->id().toString(),
                            layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
      changed = true;
    }
  }

  if (outMergedExistingKeyframeCount) {
    *outMergedExistingKeyframeCount = mergedExistingKeyframeCount;
  }

  return changed;
}

bool applyKeyframeEditAtFrame(const ArtifactCompositionPtr& composition,
                              const ArtifactAbstractLayerPtr& layer,
                              const qint64 frame,
                              const bool removeKeyframes)
{
  if (!composition || !layer) {
    return false;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  const RationalTime nowTime(frame, static_cast<int64_t>(std::llround(fps)));

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
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{composition->id().toString(), layer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }
  return changed;
}

QString interpolationTypeLabel(const ArtifactCore::InterpolationType type)
{
  switch (type) {
  case ArtifactCore::InterpolationType::Constant:
    return QStringLiteral("hold");
  case ArtifactCore::InterpolationType::Linear:
    return QStringLiteral("linear");
  case ArtifactCore::InterpolationType::EaseIn:
    return QStringLiteral("ease-in");
  case ArtifactCore::InterpolationType::EaseOut:
    return QStringLiteral("ease-out");
  case ArtifactCore::InterpolationType::EaseInOut:
    return QStringLiteral("ease-in-out");
  case ArtifactCore::InterpolationType::Bezier:
    return QStringLiteral("bezier");
  default:
    return QStringLiteral("interpolation");
  }
}

std::shared_ptr<ArtifactCore::AbstractProperty> findLayerPropertyByPath(
    const ArtifactAbstractLayerPtr& layer,
    const QString& propertyPath);

struct InterpolationChangeRecord {
  ArtifactAbstractLayerWeak layer;
  QString propertyPath;
  RationalTime time;
  ArtifactCore::KeyFrame before;
  ArtifactCore::KeyFrame after;
};

class ApplyInterpolationCommand final : public UndoCommand {
public:
  explicit ApplyInterpolationCommand(QVector<InterpolationChangeRecord> records)
      : records_(std::move(records)) {}

  void undo() override { apply(false); }
  void redo() override { apply(true); }
  QString label() const override { return QStringLiteral("Apply Interpolation"); }

private:
  void apply(const bool useAfter)
  {
    QSet<QString> changedLayerIds;
    for (const auto& record : records_) {
      auto layer = record.layer.lock();
      if (!layer) {
        continue;
      }
      const auto property = findLayerPropertyByPath(layer, record.propertyPath);
      if (!property) {
        continue;
      }
      const auto& keyframe = useAfter ? record.after : record.before;
      property->addKeyFrame(keyframe.time,
                            keyframe.value.isValid() ? keyframe.value : property->getValue(),
                            keyframe.interpolation,
                            keyframe.cp1_x,
                            keyframe.cp1_y,
                            keyframe.cp2_x,
                            keyframe.cp2_y,
                            keyframe.roving);
      layer->changed();
      changedLayerIds.insert(layer->id().toString());
    }

    for (const auto& record : records_) {
      auto layer = record.layer.lock();
      if (!layer) {
        continue;
      }
      const QString layerKey = layer->id().toString();
      if (!changedLayerIds.contains(layerKey)) {
        continue;
      }
      if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
      }
    }

    if (auto* mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  QVector<InterpolationChangeRecord> records_;
};

int applyInterpolationToSelectedKeyframesImpl(
    const ArtifactCompositionPtr& composition,
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>& markers,
    const ArtifactCore::InterpolationType interpolationType)
{
  if (!composition || markers.isEmpty()) {
    return 0;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  QSet<QString> seen;
  QVector<InterpolationChangeRecord> records;

  for (const auto& marker : markers) {
    const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
    const QString dedupeKey =
        QStringLiteral("%1|%2|%3").arg(marker.layerId.toString(), marker.propertyPath,
                                        QString::number(frame));
    if (seen.contains(dedupeKey)) {
      continue;
    }
    seen.insert(dedupeKey);

    const auto layer = composition->layerById(marker.layerId);
    if (!layer) {
      continue;
    }
    const auto property = findLayerPropertyByPath(layer, marker.propertyPath);
    if (!property || !property->isAnimatable()) {
      continue;
    }

    const RationalTime time(frame, static_cast<int64_t>(std::llround(fps)));
    const auto keyframes = property->getKeyFrames();
    const auto it = std::find_if(keyframes.cbegin(), keyframes.cend(),
                                 [&time](const ArtifactCore::KeyFrame& keyframe) {
                                   return keyframe.time == time;
                                 });
    if (it == keyframes.cend()) {
      continue;
    }

    const ArtifactCore::KeyFrame before = *it;
    ArtifactCore::KeyFrame after = before;
    after.interpolation = interpolationType;
    if (interpolationType == ArtifactCore::InterpolationType::Bezier &&
        before.interpolation != ArtifactCore::InterpolationType::Bezier) {
      after.cp1_x = 0.42f;
      after.cp1_y = 0.0f;
      after.cp2_x = 0.58f;
      after.cp2_y = 1.0f;
    }
    records.push_back(InterpolationChangeRecord{
        layer,
        marker.propertyPath,
        time,
        before,
        after,
    });
  }

  if (records.isEmpty()) {
    return 0;
  }

  const int appliedCount = static_cast<int>(records.size());
  if (auto* mgr = UndoManager::instance()) {
    mgr->push(std::make_unique<ApplyInterpolationCommand>(std::move(records)));
    return appliedCount;
  }
  return 0;
}

bool moveTimelineKeyframe(const ArtifactCompositionPtr& composition,
                          const ArtifactAbstractLayerPtr& layer,
                          const QString& propertyPath,
                          const qint64 fromFrame,
                          const qint64 toFrame,
                          bool* outMergedExistingKeyframe = nullptr)
{
  if (!composition || !layer || propertyPath.trimmed().isEmpty()) {
    return false;
  }
  if (fromFrame == toFrame) {
    return false;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  const auto frameRange = composition->frameRange();
  const qint64 minFrame = frameRange.start();
  const qint64 maxFrame = std::max(minFrame, frameRange.end());
  const qint64 clampedFromFrame = std::clamp(fromFrame, minFrame, maxFrame);
  const qint64 clampedToFrame = std::clamp(toFrame, minFrame, maxFrame);
  const RationalTime fromTime(clampedFromFrame, static_cast<int64_t>(std::llround(fps)));
  const RationalTime toTime(clampedToFrame, static_cast<int64_t>(std::llround(fps)));

  ArtifactTimelineKeyframeModel model;
  if (outMergedExistingKeyframe) {
    auto prop = layer->getProperty(propertyPath);
    *outMergedExistingKeyframe = prop && prop->hasKeyFrameAt(toTime);
  }
  return model.moveKeyframe(composition->id(), layer->id(), propertyPath,
                            fromTime, toTime);
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
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{composition->id().toString(), layer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }
  return changed;
}

struct KeyframeNavigationState {
  int totalFrames = 0;
  int selectedLayerCount = 0;
  qint64 currentFrame = -1;
  bool currentFrameHasKeyframe = false;
  qint64 previousKeyframe = -1;
  qint64 nextKeyframe = -1;
  qint64 nearestKeyframe = -1;
  qint64 nearestKeyframeDelta = 0;
};

struct CurveEditorBinding {
  LayerID layerId;
  QString propertyPath;
};

struct CurveEditorPayload {
  std::vector<CurveTrack> tracks;
  QVector<CurveTrackBinding> bindings;
  QString signature;
  QString summary;
};

enum class CurveEditorGraphMode {
  Value,
  Speed,
};

std::shared_ptr<ArtifactCore::AbstractProperty> findLayerPropertyByPath(
    const ArtifactAbstractLayerPtr& layer, const QString& propertyPath)
{
  if (!layer || propertyPath.trimmed().isEmpty()) {
    return {};
  }

  const auto groups = layer->getLayerPropertyGroups();
  for (const auto& group : groups) {
    for (const auto& property : group.sortedProperties()) {
      if (!property) {
        continue;
      }
      if (property->getName() == propertyPath) {
        return property;
      }
    }
  }
  return {};
}

ArtifactCore::InterpolationType selectedKeyframeInterpolationType(
    const ArtifactCompositionPtr& composition,
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>& markers)
{
  if (!composition || markers.isEmpty()) {
    return ArtifactCore::InterpolationType::EaseInOut;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  QHash<int, int> interpolationCounts;
  for (const auto& marker : markers) {
    const auto layer = composition->layerById(marker.layerId);
    if (!layer) {
      continue;
    }
    const auto property = findLayerPropertyByPath(layer, marker.propertyPath);
    if (!property) {
      continue;
    }

    const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
    const RationalTime time(frame, static_cast<int64_t>(std::llround(fps)));
    const auto keyframes = property->getKeyFrames();
    const auto it = std::find_if(keyframes.cbegin(), keyframes.cend(),
                                 [&time](const ArtifactCore::KeyFrame& keyframe) {
                                   return keyframe.time == time;
                                 });
    if (it != keyframes.cend()) {
      interpolationCounts[static_cast<int>(it->interpolation)] += 1;
    }
  }

  if (!interpolationCounts.isEmpty()) {
    int bestInterpolation = static_cast<int>(ArtifactCore::InterpolationType::EaseInOut);
    int bestCount = -1;
    for (auto it = interpolationCounts.cbegin(); it != interpolationCounts.cend(); ++it) {
      if (it.value() > bestCount) {
        bestCount = it.value();
        bestInterpolation = it.key();
      }
    }
    return static_cast<ArtifactCore::InterpolationType>(bestInterpolation);
  }

  return ArtifactCore::InterpolationType::EaseInOut;
}

QColor curveTrackColorForKey(const QString& key)
{
  const uint hash = qHash(key);
  return QColor::fromHsv(static_cast<int>(hash % 360), 170, 220, 255);
}

CurveEditorPayload collectCurveEditorPayload(
    const ArtifactCompositionPtr& composition,
    ArtifactLayerSelectionManager* selectionManager)
{
  CurveEditorPayload payload;
  if (!composition || !selectionManager) {
    return payload;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));

  const auto layers = selectedTimelineLayers(selectionManager);
  if (layers.isEmpty()) {
    return payload;
  }

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
        if (keyframes.empty()) {
          continue;
        }

        CurveTrack track;
        track.name = QStringLiteral("%1 / %2")
                         .arg(layer->layerName())
                         .arg(property->metadata().displayLabel.isEmpty()
                                  ? property->getName()
                                  : property->metadata().displayLabel);
        track.color = curveTrackColorForKey(layer->id().toString() + QLatin1Char('/') +
                                            property->getName());
        track.visible = true;

        QVector<qint64> frames;
        QVector<double> values;
        QVector<ArtifactCore::InterpolationType> interpolations;
        frames.reserve(static_cast<int>(keyframes.size()));
        values.reserve(static_cast<int>(keyframes.size()));
        interpolations.reserve(static_cast<int>(keyframes.size()));

        bool anyNumeric = false;
        for (const auto& keyframe : keyframes) {
          const qint64 frame = keyframe.time.rescaledTo(static_cast<int64_t>(std::round(fps)));
          const QVariant value = keyframe.value;
          if (!value.canConvert<double>()) {
            continue;
          }

          frames.push_back(frame);
          values.push_back(value.toDouble());
          interpolations.push_back(keyframe.interpolation);
          anyNumeric = true;
        }

        if (!anyNumeric || frames.isEmpty()) {
          continue;
        }

        for (int i = 0; i < frames.size(); ++i) {
          CurveKey curveKey;
          curveKey.frame = frames[i];
          curveKey.value = static_cast<float>(values[i]);
          curveKey.smooth = interpolations[i] == ArtifactCore::InterpolationType::Bezier;
          const auto& sourceKeyframe = keyframes[static_cast<size_t>(i)];

          if (i > 0 && keyframes[static_cast<size_t>(i - 1)].interpolation ==
                           ArtifactCore::InterpolationType::Bezier) {
            const auto& prevKeyframe = keyframes[static_cast<size_t>(i - 1)];
            const double dt = std::max(
                1.0, static_cast<double>(frames[i] - frames[i - 1]));
            const double dv = values[i] - values[i - 1];
            curveKey.inHandleFrame = static_cast<int64_t>(
                std::llround((prevKeyframe.cp2_x - 1.0f) * dt));
            curveKey.inHandleValue = static_cast<float>(
                std::abs(dv) > 1e-6 ? (prevKeyframe.cp2_y - 1.0f) * dv : 0.0);
          }

          if (i + 1 < frames.size() &&
              sourceKeyframe.interpolation == ArtifactCore::InterpolationType::Bezier) {
            const double dt = std::max(
                1.0, static_cast<double>(frames[i + 1] - frames[i]));
            const double dv = values[i + 1] - values[i];
            curveKey.outHandleFrame = static_cast<int64_t>(
                std::llround(sourceKeyframe.cp1_x * dt));
            curveKey.outHandleValue = static_cast<float>(
                std::abs(dv) > 1e-6 ? sourceKeyframe.cp1_y * dv : 0.0);
          } else if (interpolations[i] != ArtifactCore::InterpolationType::Constant) {
            const double prevFrame = (i > 0) ? static_cast<double>(frames[i - 1])
                                             : static_cast<double>(frames[i]);
            const double nextFrame =
                (i + 1 < frames.size()) ? static_cast<double>(frames[i + 1])
                                        : static_cast<double>(frames[i]);
            const double prevValue = (i > 0) ? values[i - 1] : values[i];
            const double nextValue =
                (i + 1 < values.size()) ? values[i + 1] : values[i];
            const double inSpan =
                std::max(1.0, (static_cast<double>(frames[i]) - prevFrame) / 3.0);
            const double outSpan =
                std::max(1.0, (nextFrame - static_cast<double>(frames[i])) / 3.0);
            const double inSlope =
                (static_cast<double>(frames[i]) > prevFrame)
                    ? ((values[i] - prevValue) /
                       (static_cast<double>(frames[i]) - prevFrame))
                    : 0.0;
            const double outSlope =
                (nextFrame > static_cast<double>(frames[i]))
                    ? ((nextValue - values[i]) /
                       (nextFrame - static_cast<double>(frames[i])))
                    : 0.0;

            if (i > 0) {
              curveKey.inHandleFrame = -static_cast<int64_t>(std::llround(inSpan));
              curveKey.inHandleValue = static_cast<float>(-inSlope * inSpan);
            }
            if (i + 1 < frames.size()) {
              curveKey.outHandleFrame = static_cast<int64_t>(std::llround(outSpan));
              curveKey.outHandleValue = static_cast<float>(outSlope * outSpan);
            }
          }

          track.keys.push_back(curveKey);
        }

        payload.bindings.push_back(CurveTrackBinding{layer->id(), property->getName()});
        payload.tracks.push_back(std::move(track));
      }
    }
  }

  return payload;
}

CurveEditorPayload collectCurveEditorSpeedPayload(
    const ArtifactCompositionPtr& composition,
    ArtifactLayerSelectionManager* selectionManager)
{
  CurveEditorPayload payload;
  if (!composition || !selectionManager) {
    return payload;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));

  const auto layers = selectedTimelineLayers(selectionManager);
  if (layers.isEmpty()) {
    return payload;
  }

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
        if (keyframes.size() < 2) {
          continue;
        }

        QVector<double> frames;
        QVector<double> values;
        QVector<ArtifactCore::InterpolationType> interpolations;
        frames.reserve(static_cast<int>(keyframes.size()));
        values.reserve(static_cast<int>(keyframes.size()));
        interpolations.reserve(static_cast<int>(keyframes.size()));

        bool supported = true;
        for (const auto& keyframe : keyframes) {
          const QVariant value = keyframe.value;
          if (!value.canConvert<double>()) {
            supported = false;
            break;
          }
          frames.push_back(static_cast<double>(
              keyframe.time.rescaledTo(static_cast<int64_t>(std::round(fps)))));
          values.push_back(value.toDouble());
          interpolations.push_back(keyframe.interpolation);
        }

        if (!supported || frames.isEmpty()) {
          continue;
        }

        CurveTrack track;
        track.name = QStringLiteral("%1 / %2 (speed)")
                         .arg(layer->layerName())
                         .arg(property->metadata().displayLabel.isEmpty()
                                  ? property->getName()
                                  : property->metadata().displayLabel);
        track.color = curveTrackColorForKey(layer->id().toString() + QLatin1Char('/') +
                                            property->getName() + QStringLiteral("/speed"));
        track.visible = true;

        for (int i = 0; i < frames.size(); ++i) {
          const double frame = frames[i];
          const double value = values[i];
          const auto interpolation = interpolations[i];

          double speedSum = 0.0;
          int speedSamples = 0;

          if (interpolation != ArtifactCore::InterpolationType::Constant && i > 0) {
            const double prevFrame = frames[i - 1];
            const double prevValue = values[i - 1];
            const double deltaFrame = std::max(1.0, frame - prevFrame);
            speedSum += std::abs((value - prevValue) / deltaFrame) * fps;
            ++speedSamples;
          }
          if (interpolation != ArtifactCore::InterpolationType::Constant &&
              i + 1 < frames.size()) {
            const double nextFrame = frames[i + 1];
            const double nextValue = values[i + 1];
            const double deltaFrame = std::max(1.0, nextFrame - frame);
            speedSum += std::abs((nextValue - value) / deltaFrame) * fps;
            ++speedSamples;
          }

          CurveKey curveKey;
          curveKey.frame = static_cast<int64_t>(std::llround(frame));
          curveKey.value =
              static_cast<float>(speedSamples > 0 ? speedSum / speedSamples : 0.0);
          curveKey.inHandleFrame = 0;
          curveKey.outHandleFrame = 0;
          curveKey.inHandleValue = 0.0f;
          curveKey.outHandleValue = 0.0f;
          track.keys.push_back(curveKey);
        }

        for (int i = 0; i < track.keys.size(); ++i) {
          auto& key = track.keys[i];
          if (key.value != key.value) {
            continue;
          }
          const double currentFrame = static_cast<double>(key.frame);
          const double prevFrame = (i > 0) ? static_cast<double>(track.keys[i - 1].frame)
                                           : currentFrame;
          const double nextFrame = (i + 1 < track.keys.size())
                                       ? static_cast<double>(track.keys[i + 1].frame)
                                       : currentFrame;
          const double prevValue = (i > 0) ? static_cast<double>(track.keys[i - 1].value)
                                           : static_cast<double>(key.value);
          const double nextValue = (i + 1 < track.keys.size())
                                       ? static_cast<double>(track.keys[i + 1].value)
                                       : static_cast<double>(key.value);

          const double inSpan = std::max(1.0, (currentFrame - prevFrame) / 3.0);
          const double outSpan = std::max(1.0, (nextFrame - currentFrame) / 3.0);
          const double inSlope =
              (currentFrame > prevFrame) ? ((static_cast<double>(key.value) - prevValue) /
                                            (currentFrame - prevFrame))
                                         : 0.0;
          const double outSlope =
              (nextFrame > currentFrame) ? ((nextValue - static_cast<double>(key.value)) /
                                            (nextFrame - currentFrame))
                                         : 0.0;

          key.inHandleFrame = -static_cast<int64_t>(std::llround(inSpan));
          key.outHandleFrame = static_cast<int64_t>(std::llround(outSpan));
          key.inHandleValue = static_cast<float>(-inSlope * inSpan);
          key.outHandleValue = static_cast<float>(outSlope * outSpan);
          key.smooth = true;
        }

        payload.bindings.push_back(CurveTrackBinding{layer->id(), property->getName()});
        payload.tracks.push_back(std::move(track));
      }
    }
  }

  return payload;
}

bool applyCurveEditorMove(
    const ArtifactCompositionPtr& composition,
    const CurveTrackBinding& binding,
    const CurveTrack& track,
    const int keyIndex,
    const int64_t newFrame,
    const float newValue)
{
  if (!composition || binding.layerId.isNil() || keyIndex < 0 ||
      keyIndex >= static_cast<int>(track.keys.size())) {
    return false;
  }

  auto layer = composition->layerById(binding.layerId);
  if (!layer) {
    return false;
  }

  const auto property = findLayerPropertyByPath(layer, binding.propertyPath);
  if (!property) {
    return false;
  }

  const CurveKey& oldKey = track.keys[keyIndex];
  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  const RationalTime oldTime(oldKey.frame, static_cast<int64_t>(std::llround(fps)));
  const RationalTime newTime(newFrame, static_cast<int64_t>(std::llround(fps)));

  const auto keyframes = property->getKeyFrames();
  const auto it = std::find_if(
      keyframes.cbegin(), keyframes.cend(),
      [&oldTime](const ArtifactCore::KeyFrame& keyframe) {
        return keyframe.time == oldTime;
      });
  if (it == keyframes.cend()) {
    return false;
  }

  const auto preservedKeyframe = *it;
  if (property->hasKeyFrameAt(oldTime)) {
    property->removeKeyFrame(oldTime);
  }
  property->addKeyFrame(newTime, QVariant(newValue),
                        preservedKeyframe.interpolation, preservedKeyframe.cp1_x,
                        preservedKeyframe.cp1_y, preservedKeyframe.cp2_x,
                        preservedKeyframe.cp2_y, preservedKeyframe.roving);
  layer->changed();
  return true;
}

bool applyCurveEditorTrackToProperty(
    const ArtifactCompositionPtr& composition, const CurveTrackBinding& binding,
    const CurveTrack& track)
{
  if (!composition || binding.layerId.isNil() || track.keys.empty()) {
    return false;
  }

  auto layer = composition->layerById(binding.layerId);
  if (!layer) {
    return false;
  }

  const auto property = findLayerPropertyByPath(layer, binding.propertyPath);
  if (!property) {
    return false;
  }

  const auto originalKeyframes = property->getKeyFrames();
  if (originalKeyframes.size() != track.keys.size()) {
    return false;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  std::vector<ArtifactCore::KeyFrame> updatedKeyframes = originalKeyframes;
  for (int i = 0; i < static_cast<int>(track.keys.size()); ++i) {
    const auto& trackKey = track.keys[static_cast<size_t>(i)];
    auto& keyframe = updatedKeyframes[static_cast<size_t>(i)];
    keyframe.time = RationalTime(
        trackKey.frame, static_cast<int64_t>(std::llround(fps)));
    keyframe.value = QVariant(trackKey.value);
  }

  for (int i = 0; i + 1 < static_cast<int>(track.keys.size()); ++i) {
    const auto& current = track.keys[static_cast<size_t>(i)];
    const auto& next = track.keys[static_cast<size_t>(i + 1)];
    auto& keyframe = updatedKeyframes[static_cast<size_t>(i)];

    if (keyframe.interpolation == ArtifactCore::InterpolationType::Constant) {
      continue;
    }

    const double dt = std::max<int64_t>(1, next.frame - current.frame);
    const double dv = static_cast<double>(next.value) - current.value;
    const bool bezierSegment = current.smooth || next.smooth;

    if (!bezierSegment) {
      if (keyframe.interpolation == ArtifactCore::InterpolationType::Bezier) {
        keyframe.interpolation = ArtifactCore::InterpolationType::Linear;
      }
      keyframe.cp1_x = 0.42f;
      keyframe.cp1_y = 0.0f;
      keyframe.cp2_x = 0.58f;
      keyframe.cp2_y = 1.0f;
      continue;
    }

    keyframe.interpolation = ArtifactCore::InterpolationType::Bezier;
    keyframe.cp1_x = static_cast<float>(
        std::clamp(static_cast<double>(current.outHandleFrame) / dt, 0.0, 1.0));
    keyframe.cp2_x = static_cast<float>(
        std::clamp(1.0 + static_cast<double>(next.inHandleFrame) / dt, 0.0, 1.0));
    if (std::abs(dv) > 1e-6) {
      keyframe.cp1_y = static_cast<float>(current.outHandleValue / dv);
      keyframe.cp2_y = static_cast<float>(1.0 + next.inHandleValue / dv);
    } else {
      keyframe.cp1_y = 0.0f;
      keyframe.cp2_y = 1.0f;
    }
  }

  property->clearKeyFrames();
  for (const auto& keyframe : updatedKeyframes) {
    property->addKeyFrame(keyframe.time, keyframe.value, keyframe.interpolation,
                          keyframe.cp1_x, keyframe.cp1_y, keyframe.cp2_x,
                          keyframe.cp2_y, keyframe.roving);
  }
  layer->changed();
  return true;
}

KeyframeNavigationState collectKeyframeNavigationState(
    const ArtifactCompositionPtr& composition,
    ArtifactLayerSelectionManager* selectionManager,
    const qint64 currentFrame)
{
  KeyframeNavigationState state;
  if (selectionManager) {
    const auto layers = selectedTimelineLayers(selectionManager);
    state.selectedLayerCount = layers.size();
  }
  state.currentFrame = currentFrame;
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

  if (state.currentFrameHasKeyframe) {
    state.nearestKeyframe = currentFrame;
    state.nearestKeyframeDelta = 0;
  } else if (state.previousKeyframe >= 0 && state.nextKeyframe >= 0) {
    const qint64 previousDelta = currentFrame - state.previousKeyframe;
    const qint64 nextDelta = state.nextKeyframe - currentFrame;
    if (previousDelta <= nextDelta) {
      state.nearestKeyframe = state.previousKeyframe;
      state.nearestKeyframeDelta = -previousDelta;
    } else {
      state.nearestKeyframe = state.nextKeyframe;
      state.nearestKeyframeDelta = nextDelta;
    }
  } else if (state.previousKeyframe >= 0) {
    state.nearestKeyframe = state.previousKeyframe;
    state.nearestKeyframeDelta = state.previousKeyframe - currentFrame;
  } else if (state.nextKeyframe >= 0) {
    state.nearestKeyframe = state.nextKeyframe;
    state.nearestKeyframeDelta = state.nextKeyframe - currentFrame;
  }
  return state;
}

QString formatKeyframeNavigationText(const KeyframeNavigationState& state)
{
  if (state.selectedLayerCount <= 0) {
    return QStringLiteral("Keys: - | Select a layer");
  }

  if (state.totalFrames <= 0) {
    return state.selectedLayerCount == 1
               ? QStringLiteral("Keys: 0 | Selected layer has no keyframes")
               : QStringLiteral("Keys: 0 | Selected layers have no keyframes");
  }

  const QString currentMark =
      state.currentFrameHasKeyframe ? QStringLiteral("On keyframe")
                                    : QStringLiteral("Between keys");
  const QString previousMark =
      state.previousKeyframe >= 0
          ? QStringLiteral("F%1 (%2)")
                .arg(state.previousKeyframe)
                .arg(state.currentFrame >= 0
                         ? QString::number(state.currentFrame - state.previousKeyframe)
                         : QStringLiteral("-"))
          : QStringLiteral("-");
  const QString nextMark =
      state.nextKeyframe >= 0
          ? QStringLiteral("F%1 (+%2)")
                .arg(state.nextKeyframe)
                .arg(state.currentFrame >= 0
                         ? QString::number(state.nextKeyframe - state.currentFrame)
                         : QStringLiteral("-"))
          : QStringLiteral("-");
  const QString nearestMark =
      state.nearestKeyframe >= 0
          ? (state.nearestKeyframeDelta == 0
                 ? QStringLiteral("F%1 (current)").arg(state.nearestKeyframe)
                 : QStringLiteral("F%1 (%2)")
                       .arg(state.nearestKeyframe)
                       .arg(QString::number(state.nearestKeyframeDelta)))
          : QStringLiteral("-");

  return QStringLiteral("Keys: %1 | Scope: %2 layer(s) | Frame: %3 | Nearest: %6 | Prev: %4 | Next: %5")
      .arg(state.totalFrames)
      .arg(state.selectedLayerCount)
      .arg(currentMark)
      .arg(previousMark)
      .arg(nextMark)
      .arg(nearestMark);
}

QString formatKeyframeCountSummary(const int count) {
  return count == 1 ? QStringLiteral("1 keyframe")
                    : QStringLiteral("%1 keyframes").arg(count);
}

QString formatCurveTrackCountSummary(const int count) {
  return count == 1 ? QStringLiteral("1 curve track")
                    : QStringLiteral("%1 curve tracks").arg(count);
}

QString formatRecentLayersText(const QStringList& recentLayerNames)
{
  if (recentLayerNames.isEmpty()) {
    return QStringLiteral("Recent: -");
  }

  return QStringLiteral("Recent: %1").arg(recentLayerNames.join(QStringLiteral(", ")));
}

void pushRecentLayerName(QStringList& recentLayerNames, const QString& layerName)
{
  const QString trimmed = layerName.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }

  recentLayerNames.removeAll(trimmed);
  recentLayerNames.prepend(trimmed);
  while (recentLayerNames.size() > 3) {
    recentLayerNames.removeLast();
  }
}

class HeaderSeekFilter final : public QObject {
public:
  HeaderSeekFilter(ArtifactTimelineTrackPainterView *trackView,
                   ArtifactTimelineScrubBar *scrubBar,
                   std::function<void(double)> seekCallback,
                   QObject *parent = nullptr)
      : QObject(parent), trackView_(trackView), scrubBar_(scrubBar),
        seekCallback_(std::move(seekCallback)) {}

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
        scrubBar_->setVisualFrame(clamped);
        if (seekCallback_) {
          seekCallback_(clamped);
        }
        
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
    scrubBar_->setVisualFrame(clamped);
    if (seekCallback_) {
      seekCallback_(clamped);
    }

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
    return leftHandleRect.contains(pos) || rightHandleRect.contains(pos);
  }

  ArtifactTimelineTrackPainterView *trackView_ = nullptr;
  ArtifactTimelineScrubBar *scrubBar_ = nullptr;
  std::function<void(double)> seekCallback_;
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
          const double offset = trackView_->horizontalOffset() - delta;
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
      const double offset = trackView_->horizontalOffset() - delta.x();
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

class LeftHeaderPriorityFilter final : public QObject {
public:
  LeftHeaderPriorityFilter(QWidget *host, QWidget *timecode, QWidget *searchBar,
                           QWidget *switches, QObject *parent = nullptr)
      : QObject(parent), host_(host), timecode_(timecode),
        searchBar_(searchBar), switches_(switches) {
    if (searchBar_) {
      searchPreferredWidth_ =
          std::max(searchBar_->width(), searchBar_->sizeHint().width());
      searchMinimumWidth_ = std::max(220, searchBar_->minimumSizeHint().width());
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
  int searchPreferredWidth_ = 260;
  int searchMinimumWidth_ = 220;
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

class TimelinePlayheadOverlayWidget final : public QWidget {
public:
  TimelinePlayheadOverlayWidget(ArtifactTimelineScrubBar *scrubBar,
                                ArtifactTimelineTrackPainterView *trackView,
                                QWidget *parent)
      : QWidget(parent), scrubBar_(scrubBar), trackView_(trackView) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);
  }

  void syncGeometryToPanel() {
    auto *panel = parentWidget();
    if (!panel || !scrubBar_ || !trackView_) {
      hide();
      return;
    }

    const int top = scrubBar_->mapTo(panel, QPoint(0, 0)).y();
    const int panelHeight = std::max(0, panel->height());
    const QRect nextGeometry(0, std::clamp(top, 0, panelHeight),
                             std::max(0, panel->width()),
                             std::max(0, panelHeight - top));
    if (geometry() != nextGeometry) {
      setGeometry(nextGeometry);
      lastX_ = -9999;
      update();
    }
    show();
    raise();
  }

  void updatePlayhead() {
    syncGeometryToPanel();
    if (!isVisible()) {
      return;
    }

    const int newX = currentPlayheadX();
    constexpr int kMargin = 16;
    if (lastX_ == -9999) {
      update();
    } else {
      const int left = std::min(lastX_, newX) - kMargin;
      const int right = std::max(lastX_, newX) + kMargin + 1;
      update(QRect(left, 0, right - left, height()));
    }
    lastX_ = newX;
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    Q_UNUSED(event);
    if (!trackView_ || width() <= 0 || height() <= 0) {
      return;
    }

    const int x = currentPlayheadX();
    if (x < -16 || x > width() + 16) {
      return;
    }

    QPainter painter(this);
    TimelinePlayheadDraw::drawPlayhead(
        painter, static_cast<qreal>(x), 0.0,
        static_cast<qreal>(height()) - 1.0, true, 0.0, 12.0, 14.0);
  }

private:
  int currentPlayheadX() const {
    if (!trackView_ || !parentWidget()) {
      return 0;
    }

    const double ppf = std::max(0.01, trackView_->pixelsPerFrame());
    const double frame = std::max(0.0, trackView_->currentFrame());
    const double localTrackX = frame * ppf - trackView_->horizontalOffset();
    const QPoint panelPoint = trackView_->mapTo(
        parentWidget(), QPoint(static_cast<int>(std::round(localTrackX)), 0));
    return panelPoint.x() - x();
  }

  ArtifactTimelineScrubBar *scrubBar_ = nullptr;
  ArtifactTimelineTrackPainterView *trackView_ = nullptr;
  int lastX_ = -9999;
};

class TimelineRightPanelWidget final : public QWidget {
public:
  TimelineRightPanelWidget(ArtifactTimelineNavigatorWidget *navigator,
                          ArtifactTimelineScrubBar *scrubBar,
                          WorkAreaControl *workArea,
                          ArtifactTimelineTrackPainterView *painterTrackView,
                          QWidget *curveHeader,
                          ArtifactCurveEditorWidget *curveEditor,
                          QWidget *parent = nullptr)
      : QWidget(parent), navigator_(navigator), scrubBar_(scrubBar),
        workArea_(workArea), painterTrackView_(painterTrackView) {
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    auto *rightPanelLayout = new QVBoxLayout(this);
    rightPanelLayout->setSpacing(0);
    rightPanelLayout->setContentsMargins(0, 0, 0, 0);

    timelinePainterPage_ = new QWidget(this);
    auto *timelinePainterLayout = new QVBoxLayout(timelinePainterPage_);
    timelinePainterLayout->setContentsMargins(0, 0, 0, 0);
    timelinePainterLayout->setSpacing(0);
    if (painterTrackView_) {
      timelinePainterLayout->addWidget(painterTrackView_, 1);
    }

    curveEditorPage_ = new QWidget(this);
    auto *curvePanelLayout = new QVBoxLayout(curveEditorPage_);
    curvePanelLayout->setContentsMargins(0, 0, 0, 0);
    curvePanelLayout->setSpacing(0);
    if (curveHeader) {
      curvePanelLayout->addWidget(curveHeader);
    }
    if (curveEditor) {
      curvePanelLayout->addWidget(curveEditor, 1);
    }

    timelineModeStack_ = new QStackedWidget(this);
    timelineModeStack_->addWidget(timelinePainterPage_);
    timelineModeStack_->addWidget(curveEditorPage_);
    timelineModeStack_->setCurrentWidget(timelinePainterPage_);

    if (navigator_) {
      rightPanelLayout->addWidget(navigator_);
    }
    if (scrubBar_) {
      rightPanelLayout->addWidget(scrubBar_);
    }
    if (workArea_) {
      rightPanelLayout->addWidget(workArea_);
    }
    rightPanelLayout->addWidget(timelineModeStack_, 1);

    playheadOverlay_ =
        new TimelinePlayheadOverlayWidget(scrubBar_, painterTrackView_, this);
    playheadOverlay_->syncGeometryToPanel();
  }

  QWidget *timelinePainterPage() const { return timelinePainterPage_; }
  QWidget *curveEditorPage() const { return curveEditorPage_; }
  QStackedWidget *timelineModeStack() const { return timelineModeStack_; }
  void syncPlayheadOverlay() {
    if (playheadOverlay_) {
      playheadOverlay_->updatePlayhead();
    }
  }

protected:
  void resizeEvent(QResizeEvent *event) override
  {
    QWidget::resizeEvent(event);
    if (playheadOverlay_) {
      playheadOverlay_->syncGeometryToPanel();
    }
  }

  void showEvent(QShowEvent *event) override
  {
    QWidget::showEvent(event);
    if (playheadOverlay_) {
      playheadOverlay_->syncGeometryToPanel();
    }
  }

  void paintEvent(QPaintEvent *event) override {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRect bounds = rect();
    const QColor base = palette().color(QPalette::Window).lighter(108);
    const QColor border = palette().color(QPalette::Mid).darker(110);
    const QColor topShade = palette().color(QPalette::Shadow);

    painter.fillRect(bounds, base);
    painter.setPen(QPen(border, 1));
    painter.drawRect(bounds.adjusted(0, 0, -1, -1));

    QColor accent = topShade;
    accent.setAlpha(28);
    painter.fillRect(QRect(bounds.left(), bounds.top(), bounds.width(), 1),
                     accent);
  }

private:
  ArtifactTimelineNavigatorWidget *navigator_ = nullptr;
  ArtifactTimelineScrubBar *scrubBar_ = nullptr;
  WorkAreaControl *workArea_ = nullptr;
  ArtifactTimelineTrackPainterView *painterTrackView_ = nullptr;
  QWidget *timelinePainterPage_ = nullptr;
  QWidget *curveEditorPage_ = nullptr;
  QStackedWidget *timelineModeStack_ = nullptr;
  TimelinePlayheadOverlayWidget *playheadOverlay_ = nullptr;
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
  QToolButton *easingLabButton_ = nullptr;
  QToolButton *keyPatternButton_ = nullptr;
  QLabel *currentLayerLabel_ = nullptr;
  QLabel *recentLayerLabel_ = nullptr;
  QLabel *frameSummaryLabel_ = nullptr;
  QLabel *zoomSummaryLabel_ = nullptr;
  QLabel *selectionSummaryLabel_ = nullptr;
  ArtifactLayerTimelinePanelWrapper *layerTimelinePanel_ = nullptr;
  ArtifactTimelineTrackPainterView *painterTrackView_ = nullptr;
  QWidget *timelinePainterPage_ = nullptr;
  ArtifactCurveEditorWidget *curveEditor_ = nullptr;
  QWidget *curveEditorPage_ = nullptr;
  QStackedWidget *timelineModeStack_ = nullptr;
  QWidget *curvePropertyPanel_ = nullptr;
  QLabel *curvePropertySummaryLabel_ = nullptr;
  QListWidget *curvePropertyList_ = nullptr;
  int focusedCurveTrackIndex_ = -1;
  bool curveFocusPinned_ = false;
  QLabel *curveEditorSummaryLabel_ = nullptr;
  QToolButton *curveEditorModeButton_ = nullptr;
  QToolButton *curveEditorFitButton_ = nullptr;
  QToolButton *curveEditorHandleButton_ = nullptr;
  QToolButton *curveEditorAutoTangentButton_ = nullptr;
  QToolButton *curveEditorFlatTangentButton_ = nullptr;
  QToolButton *curveEditorLinearTangentButton_ = nullptr;
  QToolButton *curveEditorPinButton_ = nullptr;
  bool curveHandleEditingEnabled_ = false;
  ArtifactTimelineScrubBar *scrubBar_ = nullptr;
  WorkAreaControl *workArea_ = nullptr;
  TimelineRightPanelWidget *rightPanel_ = nullptr;
  ArtifactTimelineNavigatorWidget *navigator_ = nullptr;
  ArtifactTimelineGlobalSwitches *globalSwitches_ = nullptr;
  CompositionID compositionId_;
  bool shyActive_ = false;
  QString filterText_;
  QStringList recentLayerNames_;
  QString lastCurrentLayerName_;
  LayerID lastAutoScrolledLayerId_;
  QVector<LayerID> searchResultLayerIds_;
  int searchResultIndex_ = -1;
  QVector<TimelineRowDescriptor> trackRows_;
  std::vector<CurveTrack> curveTracks_;
  QVector<CurveTrackBinding> curveBindings_;
  QString curveEditorSignature_;
  CurveEditorGraphMode curveEditorGraphMode_ = CurveEditorGraphMode::Value;
  bool syncingLayerSelection_ = false;
  bool syncingVerticalOffset_ = false;
  double currentFrame_ = 0.0;
  bool curveEditorDragging_ = false;
  ArtifactCore::EventBus::Subscription compositionChangedSubscription_;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  // refreshTracks() の重複キューイング防止フラグ。
  // ProjectChangedEvent と LayerChangedEvent が同一フレームで両方発火した場合に
  // refreshTracks() が 2 回実行されるのを防ぐ。
  bool pendingRefresh_ = false;
  bool pendingKeyframeMoveRefresh_ = false;
  // selection sync の重複キューイング防止フラグ。
  // SelectionChangedEvent と LayerSelectionChangedEvent が同時に来ても
  // painter / labels 更新を 1 回にまとめる。
  bool pendingSelectionSync_ = false;
  bool pendingSelectionSyncForceRefresh_ = false;
  QTimer* curveEditorRefreshTimer_ = nullptr;
  QTimer* playbackVisualTimer_ = nullptr;
  QTimer* audioPreviewStopTimer_ = nullptr;
  QElapsedTimer playbackVisualClock_;
  int audioPreviewFrame_ = 0;
  double playbackVisualBaseFrame_ = 0.0;
  double playbackVisualRateFps_ = 1.0;
  double playbackVisualSpeed_ = 1.0;
  bool graphEditorVisible_ = false;
  bool graphEditorNeedsFit_ = false;
  bool audioPreviewActive_ = false;
  QString audioWaveformSummary_;
  QHash<QString, CachedAudioWaveform> audioWaveformCache_;
};

CurveEditorGraphMode curveEditorGraphModeFromSettings()
{
  if (const auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
    return settings->timelineGraphEditorModeText().compare(
               QStringLiteral("Speed"), Qt::CaseInsensitive) == 0
               ? CurveEditorGraphMode::Speed
               : CurveEditorGraphMode::Value;
  }
  return CurveEditorGraphMode::Value;
}

void persistCurveEditorGraphMode(CurveEditorGraphMode mode)
{
  if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
    settings->setTimelineGraphEditorModeText(
        mode == CurveEditorGraphMode::Speed ? QStringLiteral("Speed")
                                            : QStringLiteral("Value"));
  }
}

ArtifactTimelineWidget::Impl::Impl() {}

ArtifactTimelineWidget::Impl::~Impl() {}

void ArtifactTimelineWidget::updateCacheVisuals()
{
  if (!impl_ || !impl_->scrubBar_) {
    return;
  }

  if (auto* svc = ArtifactPlaybackService::instance()) {
    const auto visuals = buildTimelineCacheVisuals(svc);
    impl_->scrubBar_->setFrameStateBitmaps(visuals.readyFrames,
                                           visuals.failedFrames,
                                           visuals.onDiskFrames);
    impl_->scrubBar_->setCachedFrameRange(
        static_cast<int>(svc->ramPreviewRange().start()),
        static_cast<int>(svc->ramPreviewRange().end()),
        svc->isRamPreviewEnabled());
    const auto summary = svc->ramPreviewSummary();
    const auto currentFrame = svc->currentFrame().framePosition();
    const auto currentState = svc->ramPreviewFrameState(currentFrame);
    const auto currentPriority = svc->ramPreviewPriorityState(currentFrame);
    const bool currentPending =
        svc->isRamPreviewFramePendingBuild(currentFrame);
    const QString currentNote = ramPreviewStatusNote(currentState);
    const QString currentPriorityNote = ramPreviewPriorityNote(currentPriority);
    impl_->scrubBar_->setToolTip(
        QStringLiteral("RAM preview cache | playable %1/%2 | requested %3 | pending %4 | next %5 | rangeReady %6 | progress %7 | playFallback %8 | failed %9 | inRam %10 | onDisk %11 | readyMissingImage %12 | current %13 | currentPending %14 | currentReady %15 | currentPlayable %16 | note %17 | priority %18")
            .arg(summary.playableFrames)
            .arg(summary.rangeFrames)
            .arg(summary.requestedFrames)
            .arg(summary.buildQueuePendingFrames)
            .arg(summary.buildQueueNextFrame)
            .arg(summary.buildRangeReady ? 1 : 0)
            .arg(QString::number(summary.buildRangeProgress * 100.0f, 'f', 0) + QStringLiteral("%"))
            .arg(summary.playbackFallbackWhilePlaying ? 1 : 0)
            .arg(summary.failedFrames)
            .arg(summary.inRamFrames)
            .arg(summary.onDiskFrames)
            .arg(summary.readyMissingImageFrames)
            .arg(currentFrame)
            .arg(currentPending ? 1 : 0)
            .arg(currentState.ready ? 1 : 0)
            .arg(currentState.playable ? 1 : 0)
            .arg(currentNote)
            .arg(currentPriorityNote));
  }
}

void ArtifactTimelineWidget::refreshCurveEditorTracks()
{
  if (!impl_ || !impl_->curveEditor_ || impl_->curveEditorDragging_) {
    return;
  }

  const bool editableValueGraph =
      impl_->curveEditorGraphMode_ == CurveEditorGraphMode::Value;
  impl_->curveEditor_->setKeyEditingEnabled(editableValueGraph);
  impl_->curveEditor_->setHandleEditingEnabled(editableValueGraph &&
                                               impl_->curveHandleEditingEnabled_);

  ArtifactCompositionPtr composition;
  if (auto* svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }

  ArtifactLayerSelectionManager* selectionManager = nullptr;
  if (auto* app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }

  const auto payload = (impl_->curveEditorGraphMode_ == CurveEditorGraphMode::Speed)
                           ? collectCurveEditorSpeedPayload(composition, selectionManager)
                           : collectCurveEditorPayload(composition, selectionManager);
  const QString modeTag = (impl_->curveEditorGraphMode_ == CurveEditorGraphMode::Speed)
                              ? QStringLiteral("speed")
                              : QStringLiteral("value");
  const QString combinedSignature =
      payload.signature + QLatin1Char('|') + modeTag;
  const bool signatureChanged = combinedSignature != impl_->curveEditorSignature_;
  if (signatureChanged) {
    impl_->curveEditorSignature_ = combinedSignature;
    impl_->curveTracks_ = payload.tracks;
    impl_->curveBindings_ = payload.bindings;
    qDebug() << "[CurveEditor] refresh"
             << "mode=" << modeTag
             << "tracks=" << payload.tracks.size()
             << "bindings=" << payload.bindings.size()
             << "summary=" << payload.summary;
    impl_->curveEditor_->setTracks(payload.tracks);
  }

  const auto markers = impl_->painterTrackView_
                           ? impl_->painterTrackView_->selectedKeyframeMarkers()
                           : QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>();
  const int selectionFocusTrack =
      curveTrackIndexForSelection(impl_->curveBindings_, markers);
  if (selectionFocusTrack >= 0) {
    impl_->focusedCurveTrackIndex_ = selectionFocusTrack;
    impl_->curveFocusPinned_ = false;
  } else if (!markers.isEmpty()) {
    impl_->focusedCurveTrackIndex_ = -1;
    impl_->curveFocusPinned_ = false;
  } else if (!impl_->curveFocusPinned_) {
    impl_->focusedCurveTrackIndex_ = -1;
  }

  if (impl_->curveEditorSummaryLabel_) {
    QString summary =
        signatureChanged ? payload.summary
                         : curveEditorSummaryForTracks(impl_->curveTracks_);
    const auto selectedMarkers =
        impl_->painterTrackView_ ? impl_->painterTrackView_->selectedKeyframeMarkers()
                                 : QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>();
    summary = QStringLiteral("%1 | %2")
                  .arg(impl_->curveEditorGraphMode_ == CurveEditorGraphMode::Speed
                           ? QStringLiteral("Speed Graph")
                           : QStringLiteral("Value Graph"),
                       summary);
    if (!selectedMarkers.isEmpty()) {
      summary = QStringLiteral("%1 | %2")
                    .arg(summary,
                         formatSelectedKeyframeSummary(
                             selectedMarkers,
                             static_cast<qint64>(std::llround(
                                 std::max(0.0, impl_->currentFrame_)))));
    } else {
      summary = QStringLiteral("%1 | Keys: none selected").arg(summary);
    }
    if (impl_->graphEditorVisible_ &&
        selectionFocusTrack >= 0 &&
        selectionFocusTrack < impl_->curveTracks_.size()) {
      summary = QStringLiteral("%1 | Focus: %2")
                    .arg(summary)
                    .arg(impl_->curveTracks_[selectionFocusTrack].name);
    }
    summary = QStringLiteral("%1 | Display: %2")
                  .arg(summary,
                       impl_->curveFocusPinned_ ? QStringLiteral("solo")
                                                : QStringLiteral("all tracks"));
    summary = QStringLiteral("%1 | Handles: %2")
                  .arg(summary,
                       impl_->curveHandleEditingEnabled_ ? QStringLiteral("on")
                                                         : QStringLiteral("off"));
    impl_->curveEditorSummaryLabel_->setText(summary);
  }

  if (impl_->curveEditorModeButton_) {
    const QSignalBlocker blocker(impl_->curveEditorModeButton_);
    impl_->curveEditorModeButton_->setText(
        impl_->curveEditorGraphMode_ == CurveEditorGraphMode::Speed
            ? QStringLiteral("Speed")
            : QStringLiteral("Value"));
    impl_->curveEditorModeButton_->setToolTip(
        impl_->curveEditorGraphMode_ == CurveEditorGraphMode::Speed
            ? QStringLiteral("Speed graph mode (click to switch to Value)")
            : QStringLiteral("Value graph mode (click to switch to Speed)"));
  }

  impl_->curveEditor_->setCurrentFrame(
      static_cast<int64_t>(std::llround(std::max(0.0, impl_->currentFrame_))));
  impl_->curveEditor_->setHandleEditingEnabled(editableValueGraph &&
                                               impl_->curveHandleEditingEnabled_);
  if (impl_->graphEditorVisible_ && impl_->curveEditor_) {
    impl_->curveEditor_->focusTrack(impl_->focusedCurveTrackIndex_);
  }
  if (impl_->graphEditorVisible_ && impl_->graphEditorNeedsFit_) {
    impl_->curveEditor_->fitToContent();
    impl_->graphEditorNeedsFit_ = false;
  }
  if (impl_->curveEditorHandleButton_) {
    const QSignalBlocker blocker(impl_->curveEditorHandleButton_);
    impl_->curveEditorHandleButton_->setEnabled(editableValueGraph);
    impl_->curveEditorHandleButton_->setChecked(editableValueGraph &&
                                                impl_->curveHandleEditingEnabled_);
    impl_->curveEditorHandleButton_->setText(
        !editableValueGraph
            ? QStringLiteral("Handles N/A")
            : (impl_->curveHandleEditingEnabled_ ? QStringLiteral("Handles On")
                                                 : QStringLiteral("Handles Off")));
    impl_->curveEditorHandleButton_->setToolTip(
        !editableValueGraph
            ? QStringLiteral("Speed Graph is read-only")
            : impl_->curveHandleEditingEnabled_
            ? QStringLiteral("Bezier handle editing is enabled")
            : QStringLiteral("Bezier handle editing is disabled for safer graph edits"));
  }
  for (auto *button : {impl_->curveEditorAutoTangentButton_,
                       impl_->curveEditorFlatTangentButton_,
                       impl_->curveEditorLinearTangentButton_}) {
    if (button) {
      button->setEnabled(editableValueGraph);
    }
  }
  if (impl_->curveEditorPinButton_) {
    const QSignalBlocker blocker(impl_->curveEditorPinButton_);
    impl_->curveEditorPinButton_->setChecked(impl_->curveFocusPinned_);
    impl_->curveEditorPinButton_->setText(
        impl_->curveFocusPinned_ ? QStringLiteral("Solo")
                                 : QStringLiteral("Solo Off"));
    impl_->curveEditorPinButton_->setToolTip(
        impl_->curveFocusPinned_
            ? QStringLiteral("Only the selected parameter is shown in the graph editor")
            : QStringLiteral("Show all curve tracks in the graph editor"));
    QPalette pal = impl_->curveEditorPinButton_->palette();
    pal.setColor(QPalette::ButtonText,
                 impl_->curveFocusPinned_ ? QColor(255, 240, 170)
                                          : QColor(200, 220, 255));
    pal.setColor(QPalette::Button,
                 impl_->curveFocusPinned_ ? QColor(92, 70, 18)
                                          : QColor(36, 44, 58));
    impl_->curveEditorPinButton_->setPalette(pal);
    impl_->curveEditorPinButton_->setAutoFillBackground(true);
  }
  updateCurvePropertyList();
}

void ArtifactTimelineWidget::showValueGraph()
{
  if (!impl_) {
    return;
  }
  impl_->curveEditorGraphMode_ = CurveEditorGraphMode::Value;
  persistCurveEditorGraphMode(impl_->curveEditorGraphMode_);
  impl_->graphEditorNeedsFit_ = true;
  toggleGraphEditorMode(true, Qt::ShortcutFocusReason);
  refreshCurveEditorTracks();
}

void ArtifactTimelineWidget::showSpeedGraph()
{
  if (!impl_) {
    return;
  }
  impl_->curveEditorGraphMode_ = CurveEditorGraphMode::Speed;
  persistCurveEditorGraphMode(impl_->curveEditorGraphMode_);
  impl_->graphEditorNeedsFit_ = true;
  toggleGraphEditorMode(true, Qt::ShortcutFocusReason);
  refreshCurveEditorTracks();
}

void ArtifactTimelineWidget::showKeyPatternDialog()
{
  if (!impl_) {
    return;
  }

  ArtifactCompositionPtr composition;
  if (auto *svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition || !impl_->layerTimelinePanel_) {
    return;
  }

  ArtifactLayerSelectionManager *selectionManager = nullptr;
  if (auto *app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }

  const auto selectedLayers = selectedTimelineLayers(selectionManager);
  const QString propertyPath = impl_->layerTimelinePanel_->currentPropertyPath().trimmed();
  const auto targets = collectKeyPatternTargets(selectedLayers, propertyPath);
  if (targets.isEmpty()) {
    Q_EMIT timelineDebugMessage(QStringLiteral("Key Pattern Dialog needs a selected property and at least one layer."));
    return;
  }

  ArtifactCore::KeyframePatternRequest request;
  request.preset = ArtifactCore::KeyframePatternPreset::Ramp;
  request.startFrame = std::max(0.0, impl_->currentFrame_);
  request.endFrame = request.startFrame + 24.0;
  request.cycles = 1.0;
  request.amplitude = 60.0;
  request.phase = 0.0;
  request.delayFrames = 2.0;
  request.bpm = composition->frameRate().framerate() > 0.0
                    ? composition->frameRate().framerate() * 2.0
                    : 120.0;
  request.damping = 4.0;
  request.settleOscillation = 3.0;
  request.stepCount = 6;
  request.sampleCount = 12;
  request.seed = 1;
  request.frameScale = std::max(
      1, static_cast<int>(std::llround(composition->frameRate().framerate())));

  if (const auto firstLayer = targets.front().layer) {
    if (const auto property = firstLayer->getProperty(propertyPath)) {
      request.baseValue = property->getValue();
      bool ok = false;
      const double baseNumeric = request.baseValue.toDouble(&ok);
      if (ok) {
        request.targetValue = baseNumeric + request.amplitude;
      } else {
        request.targetValue = request.baseValue;
      }
    }
  }

  auto *dialog = new KeyPatternDialog(
      this,
      [this, composition, targets](const ArtifactCore::KeyframePatternRequest &request) {
        QString message;
        QPointer<ArtifactTimelineWidget> self(this);
        if (applyKeyPatternToTargets(
                composition, targets, request, &message,
                [self]() {
                  if (!self) {
                    return;
                  }
                  self->refreshTracks();
                  self->updateKeyframeState();
                  self->updateSelectionState();
                })) {
          if (!message.isEmpty()) {
            Q_EMIT timelineDebugMessage(message);
          }
        } else if (!message.isEmpty()) {
          Q_EMIT timelineDebugMessage(message);
        }
      },
      request);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
}

void ArtifactTimelineWidget::applyKeyPattern(
    const ArtifactCore::KeyframePatternRequest &request)
{
  if (!impl_) {
    return;
  }

  ArtifactCompositionPtr composition;
  if (auto *svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition || !impl_->layerTimelinePanel_) {
    return;
  }

  ArtifactLayerSelectionManager *selectionManager = nullptr;
  if (auto *app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }

  const auto selectedLayers = selectedTimelineLayers(selectionManager);
  const QString propertyPath = impl_->layerTimelinePanel_->currentPropertyPath().trimmed();
  const auto targets = collectKeyPatternTargets(selectedLayers, propertyPath);
  QString message;
  QPointer<ArtifactTimelineWidget> self(this);
  if (applyKeyPatternToTargets(
          composition, targets, request, &message,
          [self]() {
            if (!self) {
              return;
            }
            self->refreshTracks();
            self->updateKeyframeState();
            self->updateSelectionState();
          })) {
    Q_EMIT timelineDebugMessage(message);
  } else if (!message.isEmpty()) {
    Q_EMIT timelineDebugMessage(message);
  }
}

bool ArtifactTimelineWidget::isGraphEditorFocusWidget(const QWidget *widget) const
{
  if (!impl_ || !widget) {
    return false;
  }

  const QWidget *cursor = widget;
  while (cursor) {
    if (cursor == impl_->curveEditor_ || cursor == impl_->curveEditorPage_ ||
        cursor == impl_->curvePropertyList_ ||
        cursor == impl_->curveEditorModeButton_ ||
        cursor == impl_->curveEditorFitButton_ ||
        cursor == impl_->curveEditorHandleButton_ ||
        cursor == impl_->curveEditorAutoTangentButton_ ||
        cursor == impl_->curveEditorFlatTangentButton_ ||
        cursor == impl_->curveEditorLinearTangentButton_ ||
        cursor == impl_->curveEditorPinButton_) {
      return true;
    }
    cursor = cursor->parentWidget();
  }
  return false;
}

void ArtifactTimelineWidget::advanceGraphEditorFocus(const bool reverse)
{
  if (!impl_ || !impl_->graphEditorVisible_) {
    return;
  }

  QVector<QWidget *> focusOrder;
  if (impl_->curvePropertyList_) {
    focusOrder.push_back(impl_->curvePropertyList_);
  }
  if (impl_->curveEditor_) {
    focusOrder.push_back(impl_->curveEditor_);
  }
  if (impl_->curveEditorFitButton_) {
    focusOrder.push_back(impl_->curveEditorFitButton_);
  }
  if (impl_->curveEditorHandleButton_) {
    focusOrder.push_back(impl_->curveEditorHandleButton_);
  }
  if (impl_->curveEditorAutoTangentButton_) {
    focusOrder.push_back(impl_->curveEditorAutoTangentButton_);
  }
  if (impl_->curveEditorFlatTangentButton_) {
    focusOrder.push_back(impl_->curveEditorFlatTangentButton_);
  }
  if (impl_->curveEditorLinearTangentButton_) {
    focusOrder.push_back(impl_->curveEditorLinearTangentButton_);
  }
  if (impl_->curveEditorPinButton_) {
    focusOrder.push_back(impl_->curveEditorPinButton_);
  }
  if (focusOrder.isEmpty()) {
    return;
  }

  QWidget *currentFocus = QApplication::focusWidget();
  int currentIndex = -1;
  for (int i = 0; i < focusOrder.size(); ++i) {
    if (focusOrder[i] == currentFocus ||
        (currentFocus && focusOrder[i]->isAncestorOf(currentFocus))) {
      currentIndex = i;
      break;
    }
  }

  const int nextIndex =
      currentIndex < 0
          ? (reverse ? focusOrder.size() - 1 : 0)
          : (currentIndex + (reverse ? focusOrder.size() - 1 : 1)) %
                focusOrder.size();
  if (auto *target = focusOrder[nextIndex]) {
    target->setFocus(Qt::TabFocusReason);
  }
}

void ArtifactTimelineWidget::toggleGraphEditorMode(const bool active,
                                                   const Qt::FocusReason reason)
{
  if (!impl_ || !impl_->globalSwitches_) {
    return;
  }

  impl_->globalSwitches_->setGraphEditorActive(active);
  if (!impl_->graphEditorVisible_) {
    if (impl_->painterTrackView_) {
      impl_->painterTrackView_->setFocus(reason);
    }
    return;
  }

  if (impl_->curveEditor_) {
    impl_->curveEditor_->setFocus(reason);
  } else if (impl_->curvePropertyList_) {
    impl_->curvePropertyList_->setFocus(reason);
  }
}

void ArtifactTimelineWidget::updateCurvePropertyList()
{
  if (!impl_ || !impl_->curvePropertyList_ || !impl_->curvePropertySummaryLabel_) {
    return;
  }

  const QSignalBlocker blocker(impl_->curvePropertyList_);
  impl_->curvePropertyList_->clear();
  int visibleCount = 0;
  int propertyCount = 0;
  for (int i = 0; i < static_cast<int>(impl_->curveTracks_.size()); ++i) {
    const auto &track = impl_->curveTracks_[i];
    QString label = track.name;
    if (!track.keys.empty()) {
      const int keyCount = static_cast<int>(track.keys.size());
      label += QStringLiteral(" (%1 key%2)")
                   .arg(keyCount)
                   .arg(keyCount == 1 ? QString() : QStringLiteral("s"));
    }
    auto *item = new QListWidgetItem(label);
    item->setData(Qt::UserRole, i);
    item->setToolTip(track.name);
    item->setForeground(track.color);
    if (impl_->focusedCurveTrackIndex_ >= 0 && impl_->focusedCurveTrackIndex_ != i) {
      item->setHidden(true);
    } else {
      ++visibleCount;
    }
    impl_->curvePropertyList_->addItem(item);
    ++propertyCount;
  }
  if (propertyCount == 0) {
    impl_->curvePropertyList_->addItem(QStringLiteral("No visible curve tracks"));
    impl_->curvePropertySummaryLabel_->setText(QStringLiteral("Curve Targets: 0"));
    return;
  }

  if (impl_->focusedCurveTrackIndex_ >= propertyCount) {
    impl_->focusedCurveTrackIndex_ = -1;
    impl_->curveFocusPinned_ = false;
  }
  if (impl_->focusedCurveTrackIndex_ >= 0) {
    if (auto *item = impl_->curvePropertyList_->item(impl_->focusedCurveTrackIndex_)) {
      item->setSelected(true);
      impl_->curvePropertyList_->setCurrentItem(item);
    }
  } else {
    impl_->curvePropertyList_->setCurrentRow(-1);
  }
  impl_->curvePropertySummaryLabel_->setText(
      QStringLiteral("Curve Targets: %1 shown / %2 total")
          .arg(visibleCount)
          .arg(propertyCount));
}

ArtifactTimelineWidget::ArtifactTimelineWidget(QWidget *parent /*=nullptr*/)
    : QWidget(parent), impl_(new Impl()) {
  QElapsedTimer ctorTimer;
  ctorTimer.start();

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
  auto easingLabButton = new QToolButton();
  auto keyPatternButton = new QToolButton();
  easingLabButton->setText(QStringLiteral("Ease+"));
  easingLabButton->setToolTip(QStringLiteral("Open Easing Lab (Comparison Tool)"));
  easingLabButton->setAutoRaise(true);
  easingLabButton->setVisible(false);
  easingLabButton->setObjectName(QStringLiteral("timelineEasingLabButton"));
  keyPatternButton->setText(QStringLiteral("Pattern+"));
  keyPatternButton->setToolTip(QStringLiteral("Open Key Pattern Dialog"));
  keyPatternButton->setAutoRaise(true);
  keyPatternButton->setVisible(false);
  keyPatternButton->setObjectName(QStringLiteral("timelineKeyPatternButton"));
  auto currentLayerLabel = new QLabel();
  auto recentLayerLabel = new QLabel();
  auto frameSummaryLabel = new QLabel();
  auto zoomSummaryLabel = new QLabel();
  auto selectionSummaryLabel = new QLabel();

  impl_->searchBar_ = searchBar;
  impl_->searchStatusLabel_ = searchStatusLabel;
  impl_->keyframeStatusLabel_ = keyframeStatusLabel;
  impl_->easingLabButton_ = easingLabButton;
  impl_->keyPatternButton_ = keyPatternButton;
  impl_->currentLayerLabel_ = currentLayerLabel;
  impl_->recentLayerLabel_ = recentLayerLabel;
  impl_->frameSummaryLabel_ = frameSummaryLabel;
  impl_->zoomSummaryLabel_ = zoomSummaryLabel;
  impl_->selectionSummaryLabel_ = selectionSummaryLabel;
  impl_->globalSwitches_ = globalSwitches;

  keyframeStatusLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  keyframeStatusLabel->setMinimumWidth(260);
  keyframeStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  {
    QFont font = keyframeStatusLabel->font();
    font.setWeight(QFont::DemiBold);
    keyframeStatusLabel->setFont(font);
    QPalette pal = keyframeStatusLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(255, 216, 148));
    keyframeStatusLabel->setPalette(pal);
  }
  keyframeStatusLabel->setCursor(Qt::PointingHandCursor);
  keyframeStatusLabel->setToolTip(
      QStringLiteral("Current keyframe navigation state. Left click: next. Right click: previous. F3 / Enter also jump through search hits or keyframes."));

  QObject::connect(searchBar, &ArtifactTimelineSearchBarWidget::searchTextChanged,
                   this, &ArtifactTimelineWidget::onSearchTextChanged);
  QObject::connect(searchBar, &ArtifactTimelineSearchBarWidget::searchNextRequested,
                   this, [this]() { jumpToSearchHit(+1); });
  QObject::connect(searchBar, &ArtifactTimelineSearchBarWidget::searchPrevRequested,
                   this, [this]() { jumpToSearchHit(-1); });
  QObject::connect(searchBar, &ArtifactTimelineSearchBarWidget::searchCleared,
                   this, [this]() { onSearchTextChanged(QString()); });
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
  auto *findNextShortcut = new QShortcut(QKeySequence(Qt::Key_F3), this);
  findNextShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(findNextShortcut, &QShortcut::activated, this, [this, searchBar]() {
    if (!impl_) {
      return;
    }
    if (searchBar && searchBar->hasSearchText()) {
      jumpToSearchHit(+1);
      return;
    }
    jumpToKeyframeHit(+1);
  });
  auto *findPrevShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3), this);
  findPrevShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(findPrevShortcut, &QShortcut::activated, this, [this, searchBar]() {
    if (!impl_) {
      return;
    }
    if (searchBar && searchBar->hasSearchText()) {
      jumpToSearchHit(-1);
      return;
    }
    jumpToKeyframeHit(-1);
  });
  auto *findNextEnterShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
  findNextEnterShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(findNextEnterShortcut, &QShortcut::activated, this,
                   [this, searchBar]() {
                     if (!impl_) {
                       return;
                     }
                     if (searchBar && searchBar->hasSearchText()) {
                       jumpToSearchHit(+1);
                       return;
                     }
                     jumpToKeyframeHit(+1);
                   });
  auto *findNextEnterPadShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), this);
  findNextEnterPadShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(findNextEnterPadShortcut, &QShortcut::activated, this,
                   [this, searchBar]() {
                     if (!impl_) {
                       return;
                     }
                     if (searchBar && searchBar->hasSearchText()) {
                       jumpToSearchHit(+1);
                       return;
                     }
                     jumpToKeyframeHit(+1);
                   });
  auto *findPrevEnterShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return), this);
  findPrevEnterShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(findPrevEnterShortcut, &QShortcut::activated, this,
                   [this, searchBar]() {
                     if (!impl_) {
                       return;
                     }
                     if (searchBar && searchBar->hasSearchText()) {
                       jumpToSearchHit(-1);
                       return;
                     }
                     jumpToKeyframeHit(-1);
                   });
  auto *findPrevEnterPadShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Enter), this);
  findPrevEnterPadShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(findPrevEnterPadShortcut, &QShortcut::activated, this,
                   [this, searchBar]() {
                     if (!impl_) {
                       return;
                     }
                     if (searchBar && searchBar->hasSearchText()) {
                       jumpToSearchHit(-1);
                       return;
                     }
                     jumpToKeyframeHit(-1);
                   });
  auto *toggleCurveEditorShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_G), this);
  QObject::connect(toggleCurveEditorShortcut, &QShortcut::activated, this, [this, globalSwitches]() {
    if (!impl_ || !globalSwitches) {
      return;
    }
    toggleGraphEditorMode(!impl_->graphEditorVisible_, Qt::ShortcutFocusReason);
  });
  auto *toggleGraphModeShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_G), this);
  QObject::connect(toggleGraphModeShortcut, &QShortcut::activated, this, [this]() {
    if (!impl_) {
      return;
    }
    if (impl_->curveEditorGraphMode_ == CurveEditorGraphMode::Speed) {
      showValueGraph();
    } else {
      showSpeedGraph();
    }
  });
  auto *tabCurveEditorShortcut = new QShortcut(QKeySequence(Qt::Key_Tab), this);
  tabCurveEditorShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(tabCurveEditorShortcut, &QShortcut::activated, this, [this]() {
    if (!impl_) {
      return;
    }
    QWidget *currentFocus = QApplication::focusWidget();
    if (impl_->graphEditorVisible_ && isGraphEditorFocusWidget(currentFocus)) {
      advanceGraphEditorFocus(false);
      return;
    }
    toggleGraphEditorMode(!impl_->graphEditorVisible_, Qt::ShortcutFocusReason);
  });
  auto *backtabCurveEditorShortcut =
      new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Tab), this);
  backtabCurveEditorShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(backtabCurveEditorShortcut, &QShortcut::activated, this,
                   [this]() {
                     if (!impl_ || !impl_->graphEditorVisible_) {
                       return;
                     }
                     if (isGraphEditorFocusWidget(QApplication::focusWidget())) {
                       advanceGraphEditorFocus(true);
                     }
                   });
  auto applyInterpolationShortcut =
      [this](const QKeySequence &sequence,
             const ArtifactCore::InterpolationType interpolation) {
        auto *shortcut = new QShortcut(sequence, this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(shortcut, &QShortcut::activated, this,
                         [this, interpolation]() {
                           applyInterpolationToSelectedKeyframes(interpolation);
                         });
      };
  applyInterpolationShortcut(QKeySequence(Qt::Key_F9),
                             ArtifactCore::InterpolationType::EaseInOut);
  applyInterpolationShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F9),
                             ArtifactCore::InterpolationType::EaseIn);
  applyInterpolationShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F9),
                              ArtifactCore::InterpolationType::EaseOut);
  auto *undoShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z), this);
  QObject::connect(undoShortcut, &QShortcut::activated, this, []() {
    if (auto *mgr = UndoManager::instance()) {
      mgr->undo();
    }
  });
  auto *redoShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y), this);
  QObject::connect(redoShortcut, &QShortcut::activated, this, []() {
    if (auto *mgr = UndoManager::instance()) {
      mgr->redo();
    }
  });
  auto setToolShortcut = [this](QKeySequence seq, ToolType type) {
    auto *shortcut = new QShortcut(seq, this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(shortcut, &QShortcut::activated, this, [this, type]() {
      if (auto *app = ArtifactApplicationManager::instance()) {
        app->toolManager()->setActiveTool(type);
      }
    });
  };
  setToolShortcut(QKeySequence(Qt::Key_V), ToolType::Selection);
  setToolShortcut(QKeySequence(Qt::Key_H), ToolType::Hand);
  setToolShortcut(QKeySequence(Qt::Key_Z), ToolType::Zoom);
  setToolShortcut(QKeySequence(Qt::Key_R), ToolType::Rotation);
  auto *markerShortcut = new QShortcut(QKeySequence(Qt::Key_M), this);
  QObject::connect(markerShortcut, &QShortcut::activated, this, []() {
    if (auto *svc = ArtifactPlaybackService::instance()) {
      svc->addMarkerAtCurrentFrame();
    }
  });
  auto *nextMarkerShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_M), this);
  QObject::connect(nextMarkerShortcut, &QShortcut::activated, this, []() {
    if (auto *svc = ArtifactPlaybackService::instance()) {
      svc->goToNextMarker();
    }
  });
  auto *prevMarkerShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M), this);
  QObject::connect(prevMarkerShortcut, &QShortcut::activated, this, []() {
    if (auto *svc = ArtifactPlaybackService::instance()) {
      svc->goToPreviousMarker();
    }
  });
  searchModeCombo->addItem(QStringLiteral("All Visible"), static_cast<int>(SearchMatchMode::AllVisible));
  searchModeCombo->addItem(QStringLiteral("Highlight Only"), static_cast<int>(SearchMatchMode::HighlightOnly));
  searchModeCombo->addItem(QStringLiteral("Filter Only"), static_cast<int>(SearchMatchMode::FilterOnly));
  searchModeCombo->setCurrentIndex(2);
  searchModeCombo->setVisible(false);
  searchModeCombo->setMaximumWidth(0);
  searchModeCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  displayModeCombo->addItem(QStringLiteral("All Layers"), static_cast<int>(TimelineLayerDisplayMode::AllLayers));
  displayModeCombo->addItem(QStringLiteral("Selected"), static_cast<int>(TimelineLayerDisplayMode::SelectedOnly));
  displayModeCombo->addItem(QStringLiteral("Animated"), static_cast<int>(TimelineLayerDisplayMode::AnimatedOnly));
  displayModeCombo->addItem(QStringLiteral("Keyframes + Important"), static_cast<int>(TimelineLayerDisplayMode::ImportantAndKeyframed));
  displayModeCombo->addItem(QStringLiteral("Audio"), static_cast<int>(TimelineLayerDisplayMode::AudioOnly));
  displayModeCombo->addItem(QStringLiteral("Video"), static_cast<int>(TimelineLayerDisplayMode::VideoOnly));
  displayModeCombo->setCurrentIndex(0);
  displayModeCombo->setVisible(false);
  displayModeCombo->setMaximumWidth(0);
  displayModeCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  densityCombo->addItem(QStringLiteral("Compact"), 24);
  densityCombo->addItem(QStringLiteral("Normal"), 28);
  densityCombo->addItem(QStringLiteral("Comfortable"), 36);
  densityCombo->setCurrentIndex(1);
  densityCombo->setVisible(false);
  densityCombo->setMaximumWidth(0);
  densityCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
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
  searchBar->setMinimumWidth(220);
  searchBar->setFixedWidth(260);
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
  searchStatusLabel->setMaximumHeight(24);
  searchStatusLabel->setText("");
  searchStatusLabel->setToolTip(
      QStringLiteral("Click to jump between search hits. Enter / F3 jump forward, Shift+Enter / Shift+F3 jump backward."));
  keyframeStatusLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  keyframeStatusLabel->setMinimumWidth(180);
  keyframeStatusLabel->setMaximumHeight(24);
  keyframeStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  {
    QPalette pal = keyframeStatusLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(168, 214, 255));
    keyframeStatusLabel->setPalette(pal);
  }
  keyframeStatusLabel->setCursor(Qt::PointingHandCursor);
  keyframeStatusLabel->setToolTip(
      QStringLiteral("Click to jump between selected keyframes. Enter / F3 jump forward, Shift+Enter / Shift+F3 jump backward. Shortcuts: Shift+J/K jump to first/last keyframe, Ctrl+PageUp/PageDown jump to previous/next keyframe."));
  keyframeStatusLabel->setText("");
  keyframeStatusLabel->setVisible(false);
  currentLayerLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  currentLayerLabel->setMinimumWidth(160);
  currentLayerLabel->setMaximumHeight(24);
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
  currentLayerLabel->setVisible(false);
  recentLayerLabel = new QLabel();
  recentLayerLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  recentLayerLabel->setMinimumWidth(280);
  recentLayerLabel->setMaximumHeight(24);
  recentLayerLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  {
    QFont font = recentLayerLabel->font();
    font.setWeight(QFont::DemiBold);
    recentLayerLabel->setFont(font);
    QPalette pal = recentLayerLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(192, 206, 255));
    recentLayerLabel->setPalette(pal);
  }
  recentLayerLabel->setText("");
  recentLayerLabel->setVisible(false);
  frameSummaryLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  frameSummaryLabel->setMinimumWidth(180);
  frameSummaryLabel->setMaximumHeight(24);
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
  frameSummaryLabel->setVisible(false);
  zoomSummaryLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  zoomSummaryLabel->setMinimumWidth(92);
  zoomSummaryLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  {
    QFont font = zoomSummaryLabel->font();
    font.setWeight(QFont::DemiBold);
    zoomSummaryLabel->setFont(font);
    QPalette pal = zoomSummaryLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(186, 219, 255));
    zoomSummaryLabel->setPalette(pal);
  }
   zoomSummaryLabel->setText(QStringLiteral("Zoom: 100%"));
  // Hide zoom percentage label on the left header — user prefers it not shown.
   zoomSummaryLabel->setVisible(false);
  currentLayerLabel->setCursor(Qt::PointingHandCursor);
  currentLayerLabel->setToolTip(QStringLiteral("Click to focus the current layer and scroll it into view."));
  recentLayerLabel->setCursor(Qt::PointingHandCursor);
  recentLayerLabel->setToolTip(QStringLiteral("Click to jump to the most recent layer name."));
  frameSummaryLabel->setCursor(Qt::PointingHandCursor);
  frameSummaryLabel->setToolTip(QStringLiteral("Click to center the playhead in the timeline."));
  zoomSummaryLabel->setCursor(Qt::PointingHandCursor);
  zoomSummaryLabel->setToolTip(QStringLiteral("Click to restore the current viewport to the playhead."));
  selectionSummaryLabel->setCursor(Qt::PointingHandCursor);
  selectionSummaryLabel->setToolTip(QStringLiteral("Click to focus the current selection in the timeline."));
  selectionSummaryLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  selectionSummaryLabel->setMinimumWidth(220);
  selectionSummaryLabel->setMaximumHeight(24);
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
  selectionSummaryLabel->setVisible(false);
  globalSwitches->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  globalSwitches->setFixedWidth(globalSwitches->sizeHint().width());

  auto searchBarLayout = new QHBoxLayout();
  searchBarLayout->setSpacing(10);
  searchBarLayout->setContentsMargins(0, 0, 8, 0);
  searchBarLayout->addWidget(leftHeader);
  searchBarLayout->addWidget(searchBar);
  searchBarLayout->addWidget(displayModeCombo);
  searchBarLayout->addWidget(densityCombo);
  searchBarLayout->addWidget(searchStatusLabel);
  searchBarLayout->addWidget(keyframeStatusLabel);
  searchBarLayout->addWidget(easingLabButton);
  searchBarLayout->addWidget(keyPatternButton);
  searchBarLayout->addStretch(1);
  searchBarLayout->addWidget(globalSwitches);
  searchBarLayout->setStretch(0, 0);
  searchBarLayout->setStretch(1, 0);
  searchBarLayout->setStretch(2, 0);
  searchBarLayout->setStretch(3, 0);
  searchBarLayout->setStretch(4, 0);
  searchBarLayout->setStretch(5, 0);
  searchBarLayout->setStretch(6, 0);
  searchBarLayout->setStretch(7, 0);
  searchBarLayout->setStretch(8, 1);
  searchBarLayout->setStretch(9, 0);

  // legacy direct signal connection disabled — prefer EventBus
  if (false) QObject::connect(globalSwitches, &ArtifactTimelineGlobalSwitches::shyChanged,
                   this, &ArtifactTimelineWidget::onShyChanged);

  // Subscribe to global timeline switches via EventBus
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineShyChangedEvent>([this](const TimelineShyChangedEvent& e) {
        QMetaObject::invokeMethod(this, [this, e]() { onShyChanged(e.shy); }, Qt::QueuedConnection);
      }));
  // Migrated to EventBus — subscribe to TimelineVisibleRowsChangedEvent instead
  // of the Qt visibleRowsChanged signal, so the connection survives widget
  // identity changes and avoids Qt signal cross-threading issues.
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineVisibleRowsChangedEvent>(
          [this](const TimelineVisibleRowsChangedEvent &) {
            QMetaObject::invokeMethod(this, [this]() {
              refreshTracks();
              updateSelectionState();
              updateSearchState();
            }, Qt::QueuedConnection);
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

  auto *recentLayerClickFilter = new TimelineStatusClickFilter(
      recentLayerLabel, [this]() {
        if (!impl_ || !impl_->recentLayerNames_.isEmpty()) {
          if (auto *svc = ArtifactProjectService::instance()) {
            const auto name = impl_->recentLayerNames_.front();
            if (auto composition = svc->currentComposition().lock()) {
              const auto layers = composition->allLayer();
              for (const auto &layer : layers) {
                if (layer && layer->layerName() == name) {
                  svc->selectLayer(layer->id());
                  if (impl_ && impl_->layerTimelinePanel_) {
                    impl_->layerTimelinePanel_->scrollToLayer(layer->id());
                  }
                  break;
                }
              }
            }
          }
        }
      },
      headerWidget);
  recentLayerLabel->installEventFilter(recentLayerClickFilter);

  auto *keyframeStatusClickFilter = new SearchStatusClickFilter(
      keyframeStatusLabel, [this]() { jumpToKeyframeHit(+1); },
      [this]() { jumpToKeyframeHit(-1); }, headerWidget);
  keyframeStatusLabel->installEventFilter(keyframeStatusClickFilter);

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

  auto *zoomSummaryClickFilter = new TimelineStatusClickFilter(
      zoomSummaryLabel, [this]() {
        if (!impl_ || !impl_->painterTrackView_) {
          return;
        }
        syncTimelineViewportFromNavigator();
        impl_->painterTrackView_->setFocus(Qt::MouseFocusReason);
      },
      headerWidget);
  zoomSummaryLabel->installEventFilter(zoomSummaryClickFilter);

  auto *searchStatusClickFilter = new SearchStatusClickFilter(
      searchStatusLabel, [this]() { jumpToSearchHit(+1); },
      [this]() { jumpToSearchHit(-1); }, headerWidget);
  searchStatusLabel->installEventFilter(searchStatusClickFilter);

  QObject::connect(easingLabButton, &QToolButton::clicked, this, [this]() {
    if (!impl_ || !impl_->painterTrackView_) {
      return;
    }
    const auto markers = impl_->painterTrackView_->selectedKeyframeMarkers();
    if (markers.isEmpty()) {
      return;
    }
    
    auto comp = safeCompositionLookup(impl_->compositionId_);
    if (!comp) return;
    const auto initialInterpolation =
        selectedKeyframeInterpolationType(comp, markers);

    auto *dialog = new EasingLabDialog(this, [this, markers, comp](ArtifactCore::InterpolationType type) {
       applyInterpolationToSelectedKeyframesImpl(comp, markers, type);
       refreshTracks();
       updateKeyframeState();
    }, initialInterpolation);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
  });

  QObject::connect(keyPatternButton, &QToolButton::clicked, this,
                   [this]() { showKeyPatternDialog(); });

  auto leftTopSpacer = new QWidget();
  leftTopSpacer->setFixedHeight(kTimelineTopRowHeight);
  leftTopSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  leftTopSpacer->setAutoFillBackground(true);

  auto leftSubHeaderSpacer = new QWidget();
  leftSubHeaderSpacer->setFixedHeight(0);
  leftSubHeaderSpacer->setSizePolicy(QSizePolicy::Expanding,
                                     QSizePolicy::Fixed);
  leftSubHeaderSpacer->setAutoFillBackground(true);

  auto *curvePropertyPanel = impl_->curvePropertyPanel_ = new QWidget();
  auto *curvePropertyLayout = new QVBoxLayout(curvePropertyPanel);
  curvePropertyLayout->setContentsMargins(8, 8, 8, 8);
  curvePropertyLayout->setSpacing(6);
  auto *curvePropertySummary = impl_->curvePropertySummaryLabel_ =
      new QLabel(QStringLiteral("Curve Targets: 0"));
  auto *curvePropertyList = impl_->curvePropertyList_ = new QListWidget();
  curvePropertySummary->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  {
    QFont font = curvePropertySummary->font();
    font.setWeight(QFont::DemiBold);
    curvePropertySummary->setFont(font);
    QPalette pal = curvePropertySummary->palette();
    pal.setColor(QPalette::WindowText, QColor(191, 224, 255));
    curvePropertySummary->setPalette(pal);
  }
  curvePropertyList->setSelectionMode(QAbstractItemView::SingleSelection);
  curvePropertyList->setFocusPolicy(Qt::StrongFocus);
  curvePropertyList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  curvePropertyList->setMinimumHeight(108);
  curvePropertyList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
  curvePropertyList->setAlternatingRowColors(false);
  curvePropertyList->setUniformItemSizes(true);
  curvePropertyLayout->addWidget(curvePropertySummary);
  curvePropertyLayout->addWidget(curvePropertyList, 1);
  curvePropertyPanel->setVisible(false);
  QObject::connect(curvePropertyList, &QListWidget::itemClicked, this,
                   [this](QListWidgetItem *item) {
                     if (!impl_ || !impl_->curveEditor_ || !item) {
                       return;
                     }
                     const int trackIndex = item->data(Qt::UserRole).toInt();
                     impl_->focusedCurveTrackIndex_ =
                         (impl_->focusedCurveTrackIndex_ == trackIndex) ? -1 : trackIndex;
      impl_->curveFocusPinned_ =
          impl_->focusedCurveTrackIndex_ >= 0;
      impl_->curveEditor_->focusTrack(impl_->focusedCurveTrackIndex_);
                     updateCurvePropertyList();
                     impl_->curveEditor_->setFocus(Qt::MouseFocusReason);
                   });
  QObject::connect(curvePropertyList, &QListWidget::itemDoubleClicked, this,
                   [this](QListWidgetItem *item) {
                     if (!impl_ || !impl_->curveEditor_ || !item) {
                       return;
                     }
                     const int trackIndex = item->data(Qt::UserRole).toInt();
                     if (trackIndex < 0) {
                       impl_->focusedCurveTrackIndex_ = -1;
                     } else {
                       impl_->focusedCurveTrackIndex_ = trackIndex;
                     }
                     impl_->curveFocusPinned_ =
                         impl_->focusedCurveTrackIndex_ >= 0;
                     impl_->curveEditor_->focusTrack(impl_->focusedCurveTrackIndex_);
                     updateCurvePropertyList();
                     impl_->curveEditor_->setFocus(Qt::MouseFocusReason);
                   });

  auto leftLayout = new QVBoxLayout();
  leftLayout->setSpacing(0);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->addWidget(leftTopSpacer);
  leftLayout->addWidget(headerWidget);
  leftLayout->addWidget(leftSubHeaderSpacer);
  leftLayout->addWidget(leftSplitter, 1);
  leftLayout->addWidget(curvePropertyPanel, 0);

  auto leftPanel = new QWidget();
  leftPanel->setLayout(leftLayout);

  auto timeNavigatorWidget = impl_->navigator_ =
      new ArtifactTimelineNavigatorWidget();
  auto workAreaWidget = impl_->workArea_ = new WorkAreaControl();
  auto scrubBar = impl_->scrubBar_ = new ArtifactTimelineScrubBar();

  auto painterTrackView = impl_->painterTrackView_ =
      new ArtifactTimelineTrackPainterView();
  workAreaWidget->setProperty("timelineDrawPlayhead", false);
  scrubBar->setProperty("timelineDrawPlayhead", false);
  painterTrackView->setProperty("timelineDrawPlayhead", false);
  auto curveEditor = impl_->curveEditor_ = new ArtifactCurveEditorWidget();
  painterTrackView->setDurationFrames(kDefaultTimelineFrames);
  painterTrackView->setTrackCount(1);
  painterTrackView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  curveEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  curveEditor->setMinimumHeight(180);
  curveEditor->setHandleEditingEnabled(true);
  impl_->curveHandleEditingEnabled_ = true;
  curveEditor->setVisible(true);
  impl_->curveEditorGraphMode_ = curveEditorGraphModeFromSettings();

  auto *curveHeader = new QWidget();
  curveHeader->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto *curveHeaderLayout = new QHBoxLayout(curveHeader);
  curveHeaderLayout->setContentsMargins(6, 4, 6, 4);
  curveHeaderLayout->setSpacing(6);
  impl_->curveEditorSummaryLabel_ = new QLabel(QStringLiteral("カーブエディタ"));
  impl_->curveEditorSummaryLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  impl_->curveEditorSummaryLabel_->setToolTip(QStringLiteral("選択したキーフレームのカーブ編集ビュー"));
  impl_->curveEditorModeButton_ = new QToolButton(curveHeader);
  impl_->curveEditorModeButton_->setAutoRaise(true);
  impl_->curveEditorModeButton_->setCursor(Qt::PointingHandCursor);
  impl_->curveEditorModeButton_->setToolTip(QStringLiteral("Switch between Value and Speed graph modes"));
  QObject::connect(impl_->curveEditorModeButton_, &QToolButton::clicked, this, [this]() {
    if (!impl_) {
      return;
    }
    if (impl_->curveEditorGraphMode_ == CurveEditorGraphMode::Speed) {
      showValueGraph();
    } else {
      showSpeedGraph();
    }
  });
  impl_->curveEditorModeButton_->setText(
      impl_->curveEditorGraphMode_ == CurveEditorGraphMode::Speed
          ? QStringLiteral("Speed")
          : QStringLiteral("Value"));
  impl_->curveEditorFitButton_ = new QToolButton(curveHeader);
  impl_->curveEditorFitButton_->setText(QStringLiteral("Fit"));
  impl_->curveEditorFitButton_->setAutoRaise(true);
  impl_->curveEditorFitButton_->setToolTip(QStringLiteral("表示中のカーブに合わせてビューを調整"));
  QObject::connect(impl_->curveEditorFitButton_, &QToolButton::clicked, this, [this]() {
    if (impl_ && impl_->curveEditor_) {
      impl_->curveEditor_->fitToContent();
    }
  });
  impl_->curveEditorHandleButton_ = new QToolButton(curveHeader);
  impl_->curveEditorHandleButton_->setAutoRaise(true);
  impl_->curveEditorHandleButton_->setCheckable(true);
  impl_->curveEditorHandleButton_->setChecked(true);
  impl_->curveEditorHandleButton_->setText(QStringLiteral("Handles On"));
  impl_->curveEditorHandleButton_->setToolTip(
      QStringLiteral("Bezier handle editing is enabled for Value graph keyframes"));
  QObject::connect(impl_->curveEditorHandleButton_, &QToolButton::toggled, this,
                   [this](bool enabled) {
                     if (!impl_ || !impl_->curveEditor_) {
                       return;
                     }
                     impl_->curveHandleEditingEnabled_ = enabled;
                     impl_->curveEditor_->setHandleEditingEnabled(enabled);
                     refreshCurveEditorTracks();
                   });
  const auto makeTangentButton =
      [curveHeader](const QString& text, const QString& tooltip) {
        auto *button = new TimelineToolCallbackButton(curveHeader);
        button->setAutoRaise(true);
        button->setText(text);
        button->setToolTip(tooltip);
        return button;
      };
  impl_->curveEditorAutoTangentButton_ = makeTangentButton(
      QStringLiteral("Auto"), QStringLiteral("Auto tangent for selected FCurve key"));
  static_cast<TimelineToolCallbackButton *>(impl_->curveEditorAutoTangentButton_)
      ->setCallback([this]() {
        if (impl_ && impl_->curveEditor_ &&
            impl_->curveEditor_->setSelectedKeyAutoTangents()) {
          refreshCurveEditorTracks();
        }
      });
  impl_->curveEditorFlatTangentButton_ = makeTangentButton(
      QStringLiteral("Flat"), QStringLiteral("Flat tangent for selected FCurve key"));
  static_cast<TimelineToolCallbackButton *>(impl_->curveEditorFlatTangentButton_)
      ->setCallback([this]() {
        if (impl_ && impl_->curveEditor_ &&
            impl_->curveEditor_->setSelectedKeyFlatTangents()) {
          refreshCurveEditorTracks();
        }
      });
  impl_->curveEditorLinearTangentButton_ = makeTangentButton(
      QStringLiteral("Linear"), QStringLiteral("Linear tangent for selected FCurve key"));
  static_cast<TimelineToolCallbackButton *>(impl_->curveEditorLinearTangentButton_)
      ->setCallback([this]() {
        if (impl_ && impl_->curveEditor_ &&
            impl_->curveEditor_->setSelectedKeyLinearTangents()) {
          refreshCurveEditorTracks();
        }
      });
  impl_->curveEditorPinButton_ = new QToolButton(curveHeader);
  impl_->curveEditorPinButton_->setAutoRaise(true);
  impl_->curveEditorPinButton_->setCheckable(true);
  impl_->curveEditorPinButton_->setChecked(false);
  impl_->curveEditorPinButton_->setText(QStringLiteral("Solo Off"));
  impl_->curveEditorPinButton_->setToolTip(
      QStringLiteral("Show all curve tracks in the graph editor"));
  {
    QFont pinFont = impl_->curveEditorPinButton_->font();
    pinFont.setBold(true);
    impl_->curveEditorPinButton_->setFont(pinFont);
  }
  QObject::connect(impl_->curveEditorPinButton_, &QToolButton::toggled, this,
                   [this](bool pinned) {
                     if (!impl_) {
                       return;
                     }
                     impl_->curveFocusPinned_ = pinned;
                     if (impl_->curveEditor_) {
                       impl_->curveEditor_->focusTrack(impl_->focusedCurveTrackIndex_);
                     }
                     refreshCurveEditorTracks();
                   });
  curveHeaderLayout->addWidget(impl_->curveEditorSummaryLabel_);
  curveHeaderLayout->addWidget(impl_->curveEditorModeButton_);
  curveHeaderLayout->addWidget(impl_->curveEditorFitButton_);
  curveHeaderLayout->addWidget(impl_->curveEditorHandleButton_);
  curveHeaderLayout->addWidget(impl_->curveEditorAutoTangentButton_);
  curveHeaderLayout->addWidget(impl_->curveEditorFlatTangentButton_);
  curveHeaderLayout->addWidget(impl_->curveEditorLinearTangentButton_);
  curveHeaderLayout->addWidget(impl_->curveEditorPinButton_);
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
  scrubBar->setInteractiveSeekingEnabled(false);
  scrubBar->setToolTip(QStringLiteral("RAM preview cache"));
  scrubBar->setVisible(true);
  scrubBar->update();

  if (auto *playback = ArtifactPlaybackService::instance()) {
    const auto visuals = buildTimelineCacheVisuals(playback);
    scrubBar->setFrameStateBitmaps(visuals.readyFrames, visuals.failedFrames,
                                   visuals.onDiskFrames);
    scrubBar->setCachedFrameRange(static_cast<int>(playback->ramPreviewRange().start()),
                                  static_cast<int>(playback->ramPreviewRange().end()),
                                  playback->isRamPreviewEnabled());
  } else {
    scrubBar->setCachedFrameRange(0, 0, false);
  }

  auto updateZoom = [this]() { syncTimelineViewportFromNavigator(); };

  auto syncPainterSelectionState = [this](bool forceRefresh = false) {
    this->syncPainterSelectionState(forceRefresh);
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
  auto applyTimelineSeek = [this, scrubBar, painterTrackView](double frame) {
    if (!impl_ || !painterTrackView || !scrubBar) {
      return;
    }
    const double maxFrame =
        std::max(0.0, painterTrackView->durationFrames() - 1.0);
    const double clamped = std::clamp(frame, 0.0, maxFrame);
    const int clampedFrame =
        std::clamp(static_cast<int>(std::llround(clamped)), 0,
                   std::max(0, scrubBar->totalFrames() - 1));

    impl_->currentFrame_ = clamped;
    painterTrackView->setCurrentFrame(clamped);
    scrubBar->setCurrentFrame(FramePosition(clampedFrame));
    scrubBar->setVisualFrame(clamped);
    syncPlayheadOverlay();

    if (auto *app = ArtifactApplicationManager::instance()) {
      if (auto *ctx = app->activeContextService()) {
        ctx->seekToFrame(clampedFrame);
      }
    }
    auto *playback = ArtifactPlaybackService::instance();
    const auto composition = safeCompositionLookup(impl_->compositionId_);
    if (playback && composition && composition->hasAudio() &&
        !playback->isPlaying()) {
      if (!impl_->audioPreviewActive_) {
        impl_->audioPreviewActive_ = true;
        impl_->audioPreviewFrame_ = clampedFrame;
        playback->playFromFrame(FramePosition(clampedFrame));
      } else {
        impl_->audioPreviewFrame_ = clampedFrame;
        playback->goToFrame(FramePosition(clampedFrame));
      }
      if (impl_->audioPreviewStopTimer_) {
        impl_->audioPreviewStopTimer_->start(220);
      }
    }
  };

  QObject::connect(
      painterTrackView, &ArtifactTimelineTrackPainterView::seekRequested, this,
      [applyTimelineSeek](double frame) {
        applyTimelineSeek(frame);
      });
  QObject::connect(
      scrubBar, &ArtifactTimelineScrubBar::frameDragStarted, this, [this]() {
        if (!impl_) {
          return;
        }
        impl_->audioPreviewActive_ = false;
        if (impl_->audioPreviewStopTimer_) {
          impl_->audioPreviewStopTimer_->stop();
        }
      });
  QObject::connect(
      scrubBar, &ArtifactTimelineScrubBar::frameDragFinished, this, [this]() {
        if (!impl_) {
          return;
        }
        auto *playback = ArtifactPlaybackService::instance();
        if (!playback || playback->isPlaying()) {
          return;
        }
        const auto composition = safeCompositionLookup(impl_->compositionId_);
        if (!composition || !composition->hasAudio() || !impl_->scrubBar_) {
          return;
        }

        const int previewFrame = std::clamp<int>(
            static_cast<int>(impl_->scrubBar_->currentFrame().framePosition()),
            0,
            std::max<int>(0, impl_->scrubBar_->totalFrames() - 1));
        const int currentFrame = playback->currentFrame().framePosition();
        if (previewFrame == currentFrame) {
          return;
        }

        impl_->audioPreviewActive_ = true;
        impl_->audioPreviewFrame_ = previewFrame;
        playback->playFromFrame(FramePosition(previewFrame));
        if (impl_->audioPreviewStopTimer_) {
          impl_->audioPreviewStopTimer_->start(350);
        }
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
        Q_UNUSED(this);
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
      painterTrackView, &ArtifactTimelineTrackPainterView::keyframeMoveRequested,
      this,
      [this](const LayerID &layerId, const QString &propertyPath,
             const qint64 fromFrame, const qint64 toFrame) {
        const auto schedulePostMoveRefresh = [this]() {
          if (!impl_->pendingKeyframeMoveRefresh_) {
            impl_->pendingKeyframeMoveRefresh_ = true;
            QMetaObject::invokeMethod(
                this,
                [this]() {
                  if (!impl_) {
                    return;
                  }
                  impl_->pendingKeyframeMoveRefresh_ = false;
                  if (!impl_->painterTrackView_) {
                    return;
                  }
                  refreshTracks();
                  updateKeyframeState();
                  updateSelectionState();
              },
              Qt::QueuedConnection);
          }
        };
        const auto composition = safeCompositionLookup(impl_->compositionId_);
        if (!composition || layerId.isNil() || propertyPath.trimmed().isEmpty()) {
          schedulePostMoveRefresh();
          Q_EMIT timelineDebugMessage(
              QStringLiteral("Failed to move keyframe for invalid timeline target"));
          return;
        }
        const auto layer = composition->layerById(layerId);
        if (!layer) {
          schedulePostMoveRefresh();
          Q_EMIT timelineDebugMessage(
              QStringLiteral("Failed to move keyframe because the layer was not found"));
          return;
        }
        const QString safePropertyPath =
            propertyPath.isNull() ? QStringLiteral("<null>") : propertyPath;
        const QVector<KeyframePropertyRef> refs{{layerId, propertyPath}};
        const auto beforeSnapshots = captureKeyframePropertySnapshots(composition, refs);
        const QSet<QString> beforeSelectionKeys{
            keyframeSelectionKey(layerId, propertyPath, fromFrame)};
        const QSet<QString> afterSelectionKeys{
            keyframeSelectionKey(layerId, propertyPath, toFrame)};
        bool mergedExistingKeyframe = false;
        if (moveTimelineKeyframe(composition, layer, propertyPath, fromFrame,
                                 toFrame, &mergedExistingKeyframe)) {
          const auto afterSnapshots =
              captureKeyframePropertySnapshots(composition, refs);
          if (auto* mgr = UndoManager::instance()) {
            QPointer<ArtifactTimelineWidget> self(this);
            mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
                QStringLiteral("Move Keyframe"),
                [self, composition, afterSnapshots, afterSelectionKeys]() {
                  applyKeyframePropertySnapshots(composition, afterSnapshots);
                  if (!self) {
                    return;
                  }
                  self->refreshTracks();
                  if (self->impl_ && self->impl_->painterTrackView_) {
                    self->impl_->painterTrackView_->setSelectedKeyframeKeys(
                        afterSelectionKeys);
                  }
                  self->updateKeyframeState();
                  self->updateSelectionState();
                },
                [self, composition, beforeSnapshots, beforeSelectionKeys]() {
                  applyKeyframePropertySnapshots(composition, beforeSnapshots);
                  if (!self) {
                    return;
                  }
                  self->refreshTracks();
                  if (self->impl_ && self->impl_->painterTrackView_) {
                    self->impl_->painterTrackView_->setSelectedKeyframeKeys(
                        beforeSelectionKeys);
                  }
                  self->updateKeyframeState();
                  self->updateSelectionState();
                }));
          }
          const QString safeFrom = QString::number(fromFrame);
          const QString safeTo = QString::number(toFrame);
          const QString mergeNote = mergedExistingKeyframe
                                        ? QStringLiteral(" (merged existing keyframe at destination)")
                                        : QString();
          Q_EMIT timelineDebugMessage(
              QStringLiteral("Moved keyframe ") + safeFrom +
              QStringLiteral(" -> ") + safeTo +
              QStringLiteral(" for ") + safePropertyPath + mergeNote);
        } else {
          schedulePostMoveRefresh();
          Q_EMIT timelineDebugMessage(
              QStringLiteral("Failed to move keyframe ") +
              QString::number(fromFrame) + QStringLiteral(" -> ") +
              QString::number(toFrame) + QStringLiteral(" for ") +
              safePropertyPath);
        }
      });
  QObject::connect(
      painterTrackView,
      &ArtifactTimelineTrackPainterView::keyframeSelectionChanged, this,
      [this](int) {
        updateKeyframeState();
        updateSelectionState();
      });
  QObject::connect(
      layerTreeView, &ArtifactLayerTimelinePanelWrapper::propertyFocusChanged,
      painterTrackView,
      [painterTrackView](const LayerID &layerId, const QString &propertyPath) {
        if (painterTrackView) {
          painterTrackView->setKeyframeContext(layerId, propertyPath);
        }
      });
  QObject::connect(
      layerTreeView, &ArtifactLayerTimelinePanelWrapper::propertyFocusChanged, this,
      [this](const LayerID &, const QString &) {
        updateSelectionState();
      });
  painterTrackView->setKeyframeContext(layerTreeView->selectedLayerId(),
                                       layerTreeView->currentPropertyPath());
  QObject::connect(
      painterTrackView, &ArtifactTimelineTrackPainterView::zoomLevelChanged, this,
      [this](double zoomPercent) {
        if (!impl_ || !impl_->painterTrackView_) {
          return;
        }

        const double duration = std::max(1.0, impl_->painterTrackView_->durationFrames());
        const double zoom = std::max(0.001, impl_->painterTrackView_->pixelsPerFrame());
        const double offset = std::max(0.0, impl_->painterTrackView_->horizontalOffset());
        const int viewW = std::max(1, impl_->painterTrackView_->width());

        if (impl_->scrubBar_) {
          impl_->scrubBar_->setRulerPixelsPerFrame(zoom);
          impl_->scrubBar_->setRulerHorizontalOffset(offset);
        }
        if (impl_->workArea_) {
          impl_->workArea_->setRulerPixelsPerFrame(zoom);
          impl_->workArea_->setRulerHorizontalOffset(offset);
        }

        if (impl_->navigator_) {
          const double visibleStart = offset / (duration * zoom);
          const double visibleEnd = (offset + static_cast<double>(viewW)) / (duration * zoom);
          const QSignalBlocker blocker(impl_->navigator_);
          impl_->navigator_->setStart(static_cast<float>(std::clamp(visibleStart, 0.0, 1.0)));
          impl_->navigator_->setEnd(static_cast<float>(
              std::clamp(std::max(visibleStart, visibleEnd), 0.0, 1.0)));
        }

        if (impl_->zoomSummaryLabel_) {
          impl_->zoomSummaryLabel_->setText(
              QStringLiteral("Zoom: %1%")
                  .arg(QString::number(std::clamp(zoomPercent, 1.0, 6400.0), 'f', 0)));
        }

        syncPlayheadOverlay();
        Q_EMIT zoomLevelChanged(zoomPercent);
      });
  QObject::connect(
      painterTrackView, &ArtifactTimelineTrackPainterView::trackRowHeightChanged, this,
      [this](int rowHeight) {
        if (!impl_ || !impl_->layerTimelinePanel_) {
          return;
        }
        impl_->layerTimelinePanel_->setRowHeight(std::max(16, rowHeight));
      });
  QObject::connect(painterTrackView, &ArtifactTimelineTrackPainterView::timelineDebugMessage, this, &ArtifactTimelineWidget::timelineDebugMessage);
  QObject::connect(globalSwitches, &ArtifactTimelineGlobalSwitches::graphEditorToggled,
                   this, [this](bool active) {
                     if (!impl_ || !impl_->curveEditor_) {
                       return;
                     }
                     impl_->graphEditorVisible_ = active;
                     if (impl_->timelineModeStack_) {
                       impl_->timelineModeStack_->setCurrentWidget(
                           active ? impl_->curveEditorPage_ : impl_->timelinePainterPage_);
                     }
                     if (impl_->curvePropertyPanel_) {
                       impl_->curvePropertyPanel_->setVisible(active);
                     }
                     if (!active) {
                       impl_->curveEditorDragging_ = false;
                       impl_->focusedCurveTrackIndex_ = -1;
                       impl_->curveFocusPinned_ = false;
                       if (impl_->curveEditorRefreshTimer_) {
                         impl_->curveEditorRefreshTimer_->stop();
                       }
                     } else {
                       impl_->graphEditorNeedsFit_ = true;
                     }
                     if (active) {
                       refreshCurveEditorTracks();
                       if (impl_->curveEditor_) {
                         impl_->curveEditor_->focusTrack(impl_->focusedCurveTrackIndex_);
                       }
                       updateCurvePropertyList();
                     }
                   });
  if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
    globalSwitches->setShyActive(settings->timelineShyActive());
    globalSwitches->setGraphEditorActive(settings->timelineGraphEditorActive());
  }
  QObject::connect(curveEditor, &ArtifactCurveEditorWidget::interactionStarted, this,
                   [this]() {
                     if (!impl_) {
                       return;
                     }
                     impl_->curveEditorDragging_ = true;
                     if (impl_->curveEditorRefreshTimer_) {
                       impl_->curveEditorRefreshTimer_->stop();
                     }
                   });
  QObject::connect(curveEditor, &ArtifactCurveEditorWidget::keySelected, this,
                   [this](int trackIndex, int /*keyIndex*/) {
                     if (!impl_ || !impl_->curvePropertyList_) {
                       return;
                     }
                     impl_->focusedCurveTrackIndex_ = trackIndex;
                     impl_->curveFocusPinned_ =
                         impl_->focusedCurveTrackIndex_ >= 0;
                     updateCurvePropertyList();
                     if (trackIndex >= 0 &&
                         trackIndex < impl_->curvePropertyList_->count()) {
                       if (auto *item = impl_->curvePropertyList_->item(trackIndex)) {
                         impl_->curvePropertyList_->scrollToItem(item);
                       }
                     }
                   });
  QObject::connect(curveEditor, &ArtifactCurveEditorWidget::interactionFinished, this,
                   [this]() {
                     if (!impl_) {
                       return;
                     }
                     impl_->curveEditorDragging_ = false;
                     if (impl_->curveEditorGraphMode_ == CurveEditorGraphMode::Value &&
                         impl_->curveEditor_ &&
                         impl_->curveBindings_.size() == impl_->curveEditor_->tracks().size()) {
                       const auto composition = safeCompositionLookup(impl_->compositionId_);
                       if (composition) {
                         for (int i = 0; i < impl_->curveBindings_.size(); ++i) {
                           applyCurveEditorTrackToProperty(
                               composition, impl_->curveBindings_[i],
                               impl_->curveEditor_->tracks()[static_cast<size_t>(i)]);
                         }
                       }
                     }
                     refreshCurveEditorTracks();
                   });
  QObject::connect(curveEditor, &ArtifactCurveEditorWidget::keyMoved, this,
                   [this](int trackIndex, int keyIndex, int64_t newFrame, float newValue) {
                     if (!impl_ || trackIndex < 0 ||
                         trackIndex >= static_cast<int>(impl_->curveBindings_.size()) ||
                         trackIndex >= static_cast<int>(impl_->curveTracks_.size())) {
                       return;
                     }

                     const auto composition = safeCompositionLookup(impl_->compositionId_);
                     if (!composition) {
                       return;
                     }

                     const auto& binding = impl_->curveBindings_[trackIndex];
                     const auto& track = impl_->curveTracks_[trackIndex];
                     if (!applyCurveEditorMove(composition, binding, track, keyIndex,
                                               newFrame, newValue)) {
                       return;
                     }

                     auto& cachedTrack = impl_->curveTracks_[trackIndex];
                     if (keyIndex >= 0 && keyIndex < static_cast<int>(cachedTrack.keys.size())) {
                       cachedTrack.keys[keyIndex].frame = newFrame;
                       cachedTrack.keys[keyIndex].value = newValue;
                     }
                     if (impl_->curveEditorRefreshTimer_) {
                       impl_->curveEditorRefreshTimer_->start(180);
                     }
                   });
  QObject::connect(curveEditor, &ArtifactCurveEditorWidget::keyDeleted, this,
                   [this](int trackIndex, int keyIndex) {
                     if (!impl_ || trackIndex < 0 ||
                         trackIndex >= static_cast<int>(impl_->curveBindings_.size()) ||
                         trackIndex >= static_cast<int>(impl_->curveTracks_.size())) {
                       return;
                     }

                     const auto composition = safeCompositionLookup(impl_->compositionId_);
                     if (!composition) {
                       return;
                     }

                     const auto& binding = impl_->curveBindings_[trackIndex];
                     auto layer = composition->layerById(binding.layerId);
                     if (!layer) {
                       return;
                     }
                     const auto property = findLayerPropertyByPath(layer, binding.propertyPath);
                     if (!property) {
                       return;
                     }

                     const auto& track = impl_->curveTracks_[trackIndex];
                     if (keyIndex < 0 || keyIndex >= static_cast<int>(track.keys.size())) {
                       return;
                     }
                     const auto& key = track.keys[keyIndex];
                     const double fps = std::max(
                         1.0, static_cast<double>(composition->frameRate().framerate()));
                     const RationalTime time(
                         static_cast<int64_t>(std::llround(key.frame)),
                         static_cast<int64_t>(std::llround(fps)));
                     if (property->hasKeyFrameAt(time)) {
                       property->removeKeyFrame(time);
                     }
                     refreshCurveEditorTracks();
                   });
  QObject::connect(curveEditor, &ArtifactCurveEditorWidget::currentFrameChanged, this,
                   [this](int64_t frame) {
                     impl_->currentFrame_ = static_cast<double>(frame);
                     if (impl_->painterTrackView_) {
                       impl_->painterTrackView_->setCurrentFrame(static_cast<double>(frame));
                     }
                     if (impl_->scrubBar_) {
                       const QSignalBlocker blocker(impl_->scrubBar_);
                       impl_->scrubBar_->setCurrentFrame(FramePosition(static_cast<int>(frame)));
                       impl_->scrubBar_->setVisualFrame(static_cast<double>(frame));
                     }
                     syncPlayheadOverlay();
                     if (auto *app = ArtifactApplicationManager::instance()) {
                       if (auto *ctx = app->activeContextService()) {
                         ctx->seekToFrame(frame);
                       }
                     }
                   });
  if (!impl_->curveEditorRefreshTimer_) {
    impl_->curveEditorRefreshTimer_ = new QTimer(this);
    impl_->curveEditorRefreshTimer_->setSingleShot(true);
    QObject::connect(impl_->curveEditorRefreshTimer_, &QTimer::timeout, this,
                     [this]() { refreshCurveEditorTracks(); });
  }
  // auto layerTimelinePanel = new ArtifactLayerTimelinePanelWrapper();
  // layerTimelinePanel->setMinimumWidth(220);
  // layerTimelinePanel->setMaximumWidth(320);

  auto rightPanel = new TimelineRightPanelWidget(
      timeNavigatorWidget, scrubBar, workAreaWidget, painterTrackView,
      curveHeader, curveEditor);
  impl_->rightPanel_ = rightPanel;
  impl_->timelinePainterPage_ = rightPanel->timelinePainterPage();
  impl_->curveEditorPage_ = rightPanel->curveEditorPage();
  impl_->timelineModeStack_ = rightPanel->timelineModeStack();

  auto *headerSeekFilter =
      new HeaderSeekFilter(painterTrackView, scrubBar, applyTimelineSeek,
                           rightPanel);
  headerSeekFilter->setDebugCallback([this](const QString &msg) {
    Q_EMIT timelineDebugMessage(msg);
  });
  timeNavigatorWidget->installEventFilter(headerSeekFilter);
  scrubBar->installEventFilter(headerSeekFilter);
  workAreaWidget->installEventFilter(headerSeekFilter);

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

  // Migrated from Qt signal connections for verticalOffsetChanged to a single
  // EventBus subscription. Both panels (TrackPainterView and LayerPanel) publish
  // TimelineVerticalScrollEvent; we sync the other panel via
  // syncTimelineVerticalOffset() with a re-entry guard to prevent feedback.
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineVerticalScrollEvent>(
          [this](const TimelineVerticalScrollEvent &e) {
            syncTimelineVerticalOffset(e.offset);
          }));

  if (painterTrackView) {
    auto *viewResizeFilter =
        new ViewportResizeFilter(rightPanel, updateZoom, rightPanel);
    painterTrackView->installEventFilter(viewResizeFilter);
  }

  impl_->playbackVisualTimer_ = new QTimer(this);
  impl_->playbackVisualTimer_->setTimerType(Qt::PreciseTimer);
  impl_->playbackVisualTimer_->setInterval(16);
  impl_->audioPreviewStopTimer_ = new QTimer(this);
  impl_->audioPreviewStopTimer_->setSingleShot(true);
  QObject::connect(impl_->audioPreviewStopTimer_, &QTimer::timeout, this,
                   [this]() {
                     if (!impl_ || !impl_->audioPreviewActive_) {
                       return;
                     }
                     impl_->audioPreviewActive_ = false;
                     if (auto *playback = ArtifactPlaybackService::instance()) {
                       playback->pauseAndGoToFrame(FramePosition(impl_->audioPreviewFrame_));
                     }
                   });
  const auto updateSmoothPlaybackPlayhead = [this]() {
    if (!impl_ || !impl_->painterTrackView_) {
      return;
    }
    auto* playback = ArtifactPlaybackService::instance();
    if (!playback || !playback->isPlaying()) {
      if (impl_->playbackVisualTimer_) {
        impl_->playbackVisualTimer_->stop();
      }
      return;
    }

    const double elapsedSeconds =
        impl_->playbackVisualClock_.isValid()
            ? static_cast<double>(impl_->playbackVisualClock_.elapsed()) / 1000.0
            : 0.0;
    double visualFrame = impl_->playbackVisualBaseFrame_ +
                         elapsedSeconds * impl_->playbackVisualRateFps_ *
                             impl_->playbackVisualSpeed_;
    const FrameRange range = playback->frameRange();
    const double startFrame = static_cast<double>(std::min(range.start(), range.end()));
    const double endFrame = static_cast<double>(std::max(range.start(), range.end()));
    visualFrame = std::clamp(visualFrame, startFrame, endFrame);

    impl_->currentFrame_ = visualFrame;
    impl_->painterTrackView_->setCurrentFrame(visualFrame);
    if (impl_->navigator_) {
      impl_->navigator_->setCurrentFrame(visualFrame);
    }
    if (impl_->scrubBar_) {
      const QSignalBlocker blocker(impl_->scrubBar_);
      impl_->scrubBar_->setVisualFrame(visualFrame);
    }
    syncPlayheadOverlay();
  };
  QObject::connect(impl_->playbackVisualTimer_, &QTimer::timeout, this,
                   updateSmoothPlaybackPlayhead);
  if (auto *playback = ArtifactPlaybackService::instance()) {
    QObject::connect(playback, &ArtifactPlaybackService::ramPreviewStateChanged,
                     this, [this](bool, const FrameRange &) {
                       updateCacheVisuals();
                     });
    QObject::connect(playback, &ArtifactPlaybackService::ramPreviewStatsChanged,
                     this, [this](float, int) { updateCacheVisuals(); });
  }

  const auto restartSmoothPlaybackPlayhead = [this]() {
    if (!impl_) {
      return;
    }
    auto* playback = ArtifactPlaybackService::instance();
    if (!playback || !playback->isPlaying()) {
      return;
    }
    impl_->playbackVisualBaseFrame_ =
        static_cast<double>(playback->currentFrame().framePosition());
    impl_->playbackVisualRateFps_ =
        std::max(1.0, static_cast<double>(playback->frameRate().framerate()));
    impl_->playbackVisualSpeed_ =
        static_cast<double>(playback->playbackSpeed());
    impl_->playbackVisualClock_.restart();
    if (impl_->playbackVisualTimer_ && !impl_->playbackVisualTimer_->isActive()) {
      impl_->playbackVisualTimer_->start();
    }
  };

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<FrameChangedEvent>(
          [this, scrubBar, restartSmoothPlaybackPlayhead](const FrameChangedEvent &event) {
            if (impl_->compositionId_.isNil() ||
                event.compositionId != impl_->compositionId_.toString()) {
              return;
            }

            const FramePosition frame(event.frame);
            const bool isPlaying = ArtifactPlaybackService::instance() &&
                                   ArtifactPlaybackService::instance()->isPlaying();
            if (impl_->painterTrackView_ && !isPlaying) {
              impl_->painterTrackView_->setCurrentFrame(
                  static_cast<double>(frame.framePosition()));
            }
            impl_->currentFrame_ = static_cast<double>(frame.framePosition());
            if (impl_->curveEditor_ && !isPlaying) {
              impl_->curveEditor_->setCurrentFrame(frame.framePosition());
            }
            syncPlayheadOverlay();
            const QSignalBlocker blocker(scrubBar);
            scrubBar->setCurrentFrame(frame);
            scrubBar->setVisualFrame(static_cast<double>(frame.framePosition()));
            if (isPlaying) {
              // Only re-anchor the smooth interpolation clock on large drift
              // (audio sync correction, seek). Normal per-frame events must NOT
              // reset the clock — that causes the saw-tooth "breaststroke" stutter.
              const bool timerRunning = impl_->playbackVisualTimer_ &&
                                        impl_->playbackVisualTimer_->isActive();
              if (!timerRunning) {
                restartSmoothPlaybackPlayhead();
              } else if (impl_->playbackVisualClock_.isValid()) {
                const double elapsedSec =
                    static_cast<double>(impl_->playbackVisualClock_.elapsed()) / 1000.0;
                const double visualFrame = impl_->playbackVisualBaseFrame_ +
                                           elapsedSec * impl_->playbackVisualRateFps_ *
                                               impl_->playbackVisualSpeed_;
                const double actualFrame =
                    static_cast<double>(frame.framePosition());
                if (std::abs(actualFrame - visualFrame) > 1.5) {
                  restartSmoothPlaybackPlayhead();
                }
              } else {
                restartSmoothPlaybackPlayhead();
              }
            }

            // 再生中でも cache 可視化だけは追随させる。
            // selection / keyframe の再構築は重いので停止時にだけ行うが、
            // RAM preview の帯は frameChanged に合わせて更新しておく。
            updateCacheVisuals();
            if (!isPlaying) {
              updateSelectionState();
              if (frame.framePosition() % 15 == 0) {
                updateKeyframeState();
              }
            }
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<WorkAreaChangedEvent>(
          [this](const WorkAreaChangedEvent &event) {
            if (!impl_ || impl_->compositionId_.isNil() ||
                event.compositionId != impl_->compositionId_.toString()) {
              return;
            }
            syncWorkAreaFromCurrentComposition();
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
  const auto scheduleSelectionSync = [this, syncPainterSelectionState](bool forceRefresh = false) {
    if (forceRefresh) {
      impl_->pendingSelectionSyncForceRefresh_ = true;
    }
    if (!impl_->pendingSelectionSync_) {
      impl_->pendingSelectionSync_ = true;
      QMetaObject::invokeMethod(
          this,
          [this, syncPainterSelectionState]() {
            if (!impl_) {
              return;
            }
            const bool forceRefreshNow = impl_->pendingSelectionSyncForceRefresh_;
            impl_->pendingSelectionSync_ = false;
            impl_->pendingSelectionSyncForceRefresh_ = false;
            if (!impl_->painterTrackView_) {
              return;
            }
            syncPainterSelectionState(forceRefreshNow);
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
          [this, scheduleRefresh, scheduleSelectionSync](const LayerChangedEvent& event) {
            if (event.changeType == LayerChangedEvent::ChangeType::Created) {
              onLayerCreated(CompositionID(event.compositionId), LayerID(event.layerId));
            } else if (event.changeType == LayerChangedEvent::ChangeType::Removed) {
              onLayerRemoved(CompositionID(event.compositionId), LayerID(event.layerId));
            } else {
              if (!impl_ || !impl_->painterTrackView_ ||
                  impl_->compositionId_.isNil() ||
                  event.compositionId != impl_->compositionId_.toString()) {
                return;
              }
              scheduleSelectionSync(true);
            }
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<SelectionChangedEvent>(
          [this, scheduleSelectionSync](const SelectionChangedEvent&) {
            if (!impl_ || !impl_->painterTrackView_) {
              return;
            }
            scheduleSelectionSync(true);
            if (impl_->curveEditor_ && impl_->graphEditorVisible_) {
              QMetaObject::invokeMethod(
                  this,
                  [this]() {
                    if (!impl_) {
                      return;
                    }
                    refreshCurveEditorTracks();
                  },
                  Qt::QueuedConnection);
            }
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerSelectionChangedEvent>(
          [this, scheduleSelectionSync](const LayerSelectionChangedEvent&) {
            if (!impl_ || !impl_->painterTrackView_) {
              return;
            }
            scheduleSelectionSync();
            if (impl_->curveEditor_ && impl_->graphEditorVisible_) {
              QMetaObject::invokeMethod(
                  this,
                  [this]() {
                    if (!impl_) {
                      return;
                    }
                    refreshCurveEditorTracks();
                  },
                  Qt::QueuedConnection);
            }
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
  // 再生停止/一時停止時に、再生中スキップしていたUI更新を実行する
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<PlaybackStateChangedEvent>(
          [this, restartSmoothPlaybackPlayhead](const PlaybackStateChangedEvent &event) {
            if (event.state == ArtifactCore::PlaybackState::Playing) {
              restartSmoothPlaybackPlayhead();
              return;
            }
            if (event.state == ArtifactCore::PlaybackState::Stopped ||
                event.state == ArtifactCore::PlaybackState::Paused) {
              if (impl_->playbackVisualTimer_) {
                impl_->playbackVisualTimer_->stop();
              }
              if (impl_->audioPreviewStopTimer_) {
                impl_->audioPreviewStopTimer_->stop();
              }
              impl_->audioPreviewActive_ = false;
              if (auto* playback = ArtifactPlaybackService::instance()) {
                const double frame =
                    static_cast<double>(playback->currentFrame().framePosition());
                impl_->currentFrame_ = frame;
                if (impl_->painterTrackView_) {
                  impl_->painterTrackView_->setCurrentFrame(frame);
                }
                if (impl_->navigator_) {
                  impl_->navigator_->setCurrentFrame(frame);
                }
                syncPlayheadOverlay();
              }
              updateCacheVisuals();
              updateSelectionState();
              updateKeyframeState();
            }
          }));
  qInfo() << "[TimelineWidget][Ctor] total ms=" << ctorTimer.elapsed();
}

ArtifactTimelineWidget::~ArtifactTimelineWidget() {
  delete impl_;
}

void ArtifactTimelineWidget::setComposition(const CompositionID &id) {
  impl_->compositionId_ = id;
  impl_->audioWaveformCache_.clear();
  impl_->lastAutoScrolledLayerId_ = LayerID();
  if (impl_->layerTimelinePanel_) {
    impl_->layerTimelinePanel_->setComposition(id);
  }

  if (impl_->painterTrackView_) {
    struct UpdateRestoreGuard {
      QWidget* widget = nullptr;
      bool restore = true;
      ~UpdateRestoreGuard() {
        if (widget) {
          widget->setUpdatesEnabled(restore);
          widget->update();
        }
      }
    };
    const bool restoreUpdates = updatesEnabled();
    setUpdatesEnabled(false);
    UpdateRestoreGuard restoreGuard{this, restoreUpdates};

    if (auto svc = ArtifactProjectService::instance()) {
      auto res = svc->findComposition(id);
      if (res.success && !res.ptr.expired()) {
        auto comp = res.ptr.lock();
        
        // Listen to composition-level changes via the shared internal event bus.
        impl_->compositionChangedSubscription_.disconnect();
        const QString compositionId = comp->id().toString();
        impl_->compositionChangedSubscription_ =
            impl_->eventBus_.subscribe<CompositionChangedEvent>(
                [this, compositionId](const CompositionChangedEvent& event) {
                  if (event.compositionId != compositionId) {
                    return;
                  }
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
        if (impl_->curveEditor_ && impl_->graphEditorVisible_) {
          refreshCurveEditorTracks();
        }
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
  impl_->audioWaveformCache_.remove(audioWaveformCacheKey(compId, lid));
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
  const double compositionFps = timelineFrameRateFallback(composition);

  QVector<TimelineRowDescriptor> visibleRows;
  if (impl_->layerTimelinePanel_) {
    visibleRows = impl_->layerTimelinePanel_->visibleTimelineRowDescriptors();
  }

  if (!impl_->layerTimelinePanel_) {
    if (composition) {
      auto layers = composition->allLayer();
      std::reverse(layers.begin(), layers.end());
      auto summarizeLayerState = [](const ArtifactAbstractLayerPtr& layer) -> QString {
        if (!layer) {
          return {};
        }
        if (!layer->isVisible()) return QStringLiteral("Hidden");
        if (layer->isLocked()) return QStringLiteral("Locked");
        if (layer->isSolo()) return QStringLiteral("Solo");
        if (layer->isShy()) return QStringLiteral("Shy");
        const bool hasMasks = layer->hasMasks();
        const bool hasMattes = !layer->matteReferences().empty();
        if (hasMasks && hasMattes) return QStringLiteral("Mask + Matte");
        if (hasMasks) return QStringLiteral("Masked");
        if (hasMattes) return QStringLiteral("Matted");
        if (layer->hasParent()) return QStringLiteral("Child");
        return {};
      };
      auto summarizeLayerStateTone = [](const ArtifactAbstractLayerPtr& layer) -> LayerPresentationBadgeTone {
        if (!layer) {
          return LayerPresentationBadgeTone::Neutral;
        }
        if (!layer->isVisible()) return LayerPresentationBadgeTone::Neutral;
        if (layer->isLocked() || layer->isShy()) return LayerPresentationBadgeTone::Special;
        if (layer->isSolo()) return LayerPresentationBadgeTone::Motion;
        if (layer->hasMasks() || !layer->matteReferences().empty()) return LayerPresentationBadgeTone::Special;
        if (layer->hasParent()) return LayerPresentationBadgeTone::Container;
        return LayerPresentationBadgeTone::Neutral;
      };
      for (auto &layer : layers) {
        if (!layer)
          continue;
        if (impl_->shyActive_ && layer->isShy())
          continue;
        TimelineRowDescriptor descriptor;
        const auto presentation = describeLayerPresentation(layer);
        descriptor.layerId = layer->id();
        descriptor.kind = TimelineRowKind::Layer;
        descriptor.auxiliaryText = presentation.timelineBadgeText;
        descriptor.auxiliaryTone = presentation.badgeTone;
        descriptor.stateText = summarizeLayerState(layer);
        descriptor.stateTone = summarizeLayerStateTone(layer);
        visibleRows.push_back(std::move(descriptor));
      }
    }
  }

  impl_->trackRows_ = visibleRows;
  QVector<int> trackHeights;
  const int trackCount = std::max(1, static_cast<int>(visibleRows.size()));
  trackHeights.reserve(trackCount);
  for (int i = 0; i < trackCount; ++i) {
    trackHeights.push_back(impl_->layerTimelinePanel_ ? impl_->layerTimelinePanel_->rowHeight() : static_cast<int>(kTimelineRowHeight));
  }
  QVector<ArtifactTimelineTrackPainterView::TrackClipVisual> painterClips;
  painterClips.reserve(visibleRows.size());
  int audioTrackCount = 0;
  int waveformReadyCount = 0;
  QString firstWaveformPreview;
  if (impl_->painterTrackView_) {
    impl_->painterTrackView_->setTrackHeights(trackHeights);
  }
  for (int rowIndex = 0; rowIndex < visibleRows.size(); ++rowIndex) {
    const auto &row = visibleRows[rowIndex];
    if (row.kind != TimelineRowKind::Layer || row.layerId.isNil()) {
      continue;
    }
    const int trackIndex = rowIndex;

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
      const auto audioLayer = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer);
      ArtifactTimelineTrackPainterView::TrackClipVisual visual;
      visual.clipId = row.layerId.toString();
      visual.layerId = row.layerId;
      visual.trackIndex = trackIndex;
      visual.startFrame = clipStart;
      visual.durationFrame = clipDuration;
      visual.trimMinStartFrame = clipStart;
      visual.trimMaxEndFrame = clipStart + clipDuration;
      visual.title = layer->layerName();
      visual.fillColor = layerTimelineColor(layer);
      if (layer->hasAudio()) {
        ++audioTrackCount;
        visual.kind = audioLayer
                          ? ArtifactTimelineTrackPainterView::TrackClipVisual::Kind::Audio
                          : ArtifactTimelineTrackPainterView::TrackClipVisual::Kind::Video;
        const QString cacheKey = audioWaveformCacheKey(impl_->compositionId_, row.layerId);
        const QString signature = audioWaveformSignatureForLayer(*layer, compositionFps);
        auto cachedIt = impl_->audioWaveformCache_.find(cacheKey);
        if (cachedIt == impl_->audioWaveformCache_.end() ||
            cachedIt->signature != signature) {
          if (auto waveform = buildAudioWaveformForLayer(*layer, compositionFps)) {
            cachedIt = impl_->audioWaveformCache_.insert(cacheKey, std::move(*waveform));
          } else {
            impl_->audioWaveformCache_.remove(cacheKey);
            cachedIt = impl_->audioWaveformCache_.end();
          }
        }
        if (cachedIt != impl_->audioWaveformCache_.end()) {
          visual.waveformPeaks = cachedIt->peaks;
          visual.waveformRms = cachedIt->rms;
          ++waveformReadyCount;
          if (firstWaveformPreview.isEmpty()) {
            firstWaveformPreview = audioLayer
                                       ? audioLayer->waveformPreviewSummary()
                                       : waveformPreviewSummary(cachedIt->peaks, cachedIt->rms);
          }
        }
      } else if (std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
        visual.kind = ArtifactTimelineTrackPainterView::TrackClipVisual::Kind::Video;
      }
      const qint64 inPointFrame = layer->inPoint().framePosition();
      const qint64 startTimeFrame = layer->startTime().framePosition();
      std::optional<double> sourceDurationFrames;
      if (audioLayer && audioLayer->duration() > 0.0) {
        sourceDurationFrames =
            std::max(1.0, std::round(audioLayer->duration() * compositionFps));
      } else if (const auto videoLayer =
                     std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
        const auto &streamInfo = videoLayer->streamInfo();
        if (streamInfo.frameCount > 0) {
          sourceDurationFrames = static_cast<double>(streamInfo.frameCount);
        } else if (streamInfo.duration > 0.0) {
          sourceDurationFrames =
              std::max(1.0, std::round(streamInfo.duration * compositionFps));
        }
      }
      if (sourceDurationFrames && *sourceDurationFrames > 1.0) {
        visual.trimMinStartFrame =
            std::max(0.0, static_cast<double>(inPointFrame - startTimeFrame));
        visual.trimMaxEndFrame =
            std::max(visual.trimMinStartFrame + 1.0,
                     visual.trimMinStartFrame + *sourceDurationFrames);
        visual.hasTrimSourceRange =
            visual.trimMinStartFrame < visual.startFrame - 0.0001 ||
            visual.trimMaxEndFrame >
                (visual.startFrame + visual.durationFrame + 0.0001);
      }
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
    const double syncedOffset = impl_->layerTimelinePanel_
        ? impl_->layerTimelinePanel_->verticalOffset()
        : impl_->painterTrackView_->verticalOffset();
    syncTimelineVerticalOffset(syncedOffset);
    syncPainterSelectionState();
  }
  if (audioTrackCount > 0) {
    const QString waveformSummary = QStringLiteral("Audio waveform preview: %1/%2 clips ready%3")
                                        .arg(waveformReadyCount)
                                        .arg(audioTrackCount)
                                        .arg(firstWaveformPreview.isEmpty()
                                                 ? QString()
                                                 : QStringLiteral(" | %1").arg(firstWaveformPreview));
    if (waveformSummary != impl_->audioWaveformSummary_) {
      impl_->audioWaveformSummary_ = waveformSummary;
      Q_EMIT timelineDebugMessage(waveformSummary);
    }
  } else if (!impl_->audioWaveformSummary_.isEmpty()) {
    impl_->audioWaveformSummary_.clear();
    Q_EMIT timelineDebugMessage(QStringLiteral("Audio waveform preview: none"));
  }
  if (impl_->curveEditor_ && impl_->graphEditorVisible_) {
    refreshCurveEditorTracks();
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
  accent.setAlpha(32);
  painter.fillRect(QRect(bounds.left(), bounds.top(), bounds.width(), 1),
                   accent);
}

void ArtifactTimelineWidget::focusInEvent(QFocusEvent* event) {
  QWidget::focusInEvent(event);
  if (auto* input = ArtifactCore::InputOperator::instance()) {
    input->setActiveContext(QStringLiteral("Workspace.Timeline"));
    if (auto* playbackKeyMap = input->getKeyMap(QStringLiteral("Playback"))) {
      input->registerWidgetKeyMap(this, playbackKeyMap);
    }
  }
}

void ArtifactTimelineWidget::focusOutEvent(QFocusEvent* event) {
  QWidget::focusOutEvent(event);
  if (auto* input = ArtifactCore::InputOperator::instance()) {
    input->unregisterWidgetKeyMap(this);
    if (input->activeContext() == QStringLiteral("Workspace.Timeline")) {
      input->setActiveContext(QStringLiteral("Global"));
    }
  }
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
    impl_->painterTrackView_->seekRequested(
        std::clamp(newPos, 0.0,
                   std::max(0.0, impl_->painterTrackView_->durationFrames() -
                                        1.0)));
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
  const double fps = comp ? std::max(1.0, static_cast<double>(
                                             comp->frameRate().framerate()))
                          : 30.0;
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
  impl_->workArea_->setTotalFrames(static_cast<float>(totalFrames));
  impl_->workArea_->setFrameRate(fps);
}

void ArtifactTimelineWidget::keyPressEvent(QKeyEvent *event) {
  if (auto* input = ArtifactCore::InputOperator::instance()) {
    input->setActiveContext(QStringLiteral("Workspace.Timeline"));
    if (event && input->processKeyPress(this, event->key(), event->modifiers())) {
      event->accept();
      return;
    }
  }

  const auto &shortcuts = ArtifactCore::ShortcutBindings::instance();
  const auto zoomTimelineBy = [this](const double scale) {
    if (!impl_ || !impl_->painterTrackView_) {
      return false;
    }
    const double oldPpf =
        std::max<double>(0.001, static_cast<double>(impl_->painterTrackView_->pixelsPerFrame()));
    const double newPpf = std::clamp(oldPpf * scale, 0.05, 64.0);
    if (std::abs(newPpf - oldPpf) < 0.0001) {
      return false;
    }
    const double anchorX =
        static_cast<double>(std::max(1, impl_->painterTrackView_->width())) * 0.5;
    const double anchorFrame =
        (anchorX + impl_->painterTrackView_->horizontalOffset()) / oldPpf;
    impl_->painterTrackView_->setPixelsPerFrame(newPpf);
    impl_->painterTrackView_->setHorizontalOffset(anchorFrame * newPpf - anchorX);
    Q_EMIT zoomLevelChanged(newPpf * 100.0);
    return true;
  };
  const auto resetTimelineZoom = [this, &zoomTimelineBy]() {
    if (!impl_ || !impl_->painterTrackView_) {
      return false;
    }
    const double currentPpf =
        std::max<double>(0.001, static_cast<double>(impl_->painterTrackView_->pixelsPerFrame()));
    return zoomTimelineBy(2.0 / currentPpf);
  };
  if (shortcuts.matches(event, ArtifactCore::ShortcutId::TimelineZoomIn)) {
    if (zoomTimelineBy(1.12)) {
      event->accept();
      return;
    }
  } else if (shortcuts.matches(event, ArtifactCore::ShortcutId::TimelineZoomOut)) {
    if (zoomTimelineBy(1.0 / 1.12)) {
      event->accept();
      return;
    }
  } else if (shortcuts.matches(event, ArtifactCore::ShortcutId::TimelineZoomReset)) {
    if (resetTimelineZoom()) {
      event->accept();
      return;
    }
  }

  auto deleteSelectedLayersFromTimeline = [this]() -> bool {
    auto *service = ArtifactProjectService::instance();
    auto *selection = ArtifactApplicationManager::instance()
                          ? ArtifactApplicationManager::instance()->layerSelectionManager()
                          : nullptr;
    const auto selectedLayers = selection ? selection->selectedLayers()
                                          : QSet<ArtifactAbstractLayerPtr>{};
    if (!service || selectedLayers.isEmpty()) {
      return false;
    }

    auto comp = service->currentComposition().lock();
    if (!comp) {
      return false;
    }

    QVector<LayerID> layerIds;
    layerIds.reserve(selectedLayers.size());
    for (const auto &layer : selectedLayers) {
      if (layer) {
        layerIds.push_back(layer->id());
      }
    }
    if (layerIds.isEmpty()) {
      return false;
    }

    bool confirmed = false;
    if (layerIds.size() == 1) {
      const QString message =
          service->layerRemovalConfirmationMessage(comp->id(), layerIds.front());
      confirmed = QMessageBox::question(this, QStringLiteral("Delete Layer"),
                                        message, QMessageBox::Yes | QMessageBox::No,
                                        QMessageBox::No) == QMessageBox::Yes;
    } else {
      confirmed = QMessageBox::question(
                     this, QStringLiteral("Delete Layers"),
                     QStringLiteral("Delete %1 selected layers?")
                         .arg(layerIds.size()),
                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No) ==
                 QMessageBox::Yes;
    }
    if (!confirmed) {
      return false;
    }

    if (auto *selectionManager = ArtifactApplicationManager::instance()
                                     ? ArtifactApplicationManager::instance()->layerSelectionManager()
                                     : nullptr) {
      selectionManager->clearSelection();
    }

    bool removed = false;
    for (const auto &layerId : layerIds) {
      removed = service->removeLayerFromComposition(comp->id(), layerId) || removed;
    }
    return removed;
  };

  if (event && !event->isAutoRepeat()) {
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    if (event->key() == Qt::Key_Tab && impl_ && impl_->graphEditorVisible_ &&
        isGraphEditorFocusWidget(QApplication::focusWidget())) {
      advanceGraphEditorFocus(modifiers.testFlag(Qt::ShiftModifier));
      event->accept();
      return;
    }
  }

  if (shortcuts.matches(event, ArtifactCore::ShortcutId::LayerDeleteSelected)) {
    auto *selection = ArtifactApplicationManager::instance()
                          ? ArtifactApplicationManager::instance()->layerSelectionManager()
                          : nullptr;
    const auto selectedLayers = selection ? selection->selectedLayers()
                                          : QSet<ArtifactAbstractLayerPtr>{};
    if (!selectedLayers.isEmpty()) {
      deleteSelectedLayersFromTimeline();
      event->accept();
      return;
    }
  }

  const ArtifactTimelineAction action = resolveTimelineAction(event);
  if (action != ArtifactTimelineAction::None && handleTimelineAction(action)) {
    event->accept();
    return;
  }

  if (event->key() == Qt::Key_X && (event->modifiers() & Qt::ControlModifier)) {
    if (impl_ && impl_->painterTrackView_) {
      const auto selectedMarkers = impl_->painterTrackView_->selectedKeyframeMarkers();
      const auto hoveredMarker = impl_->painterTrackView_->hoveredKeyframeMarker();
      if (!selectedMarkers.isEmpty() || hoveredMarker.trackIndex >= 0) {
        copySelectedKeyframes();
        if (impl_->painterTrackView_->deleteSelectedKeyframeMarkers()) {
          event->accept();
          return;
        }
        event->accept();
        return;
      }
    }
  }

  // J/K/L シャトル操作
  if (event->key() == Qt::Key_J || event->key() == Qt::Key_K || event->key() == Qt::Key_L) {
    if (auto *svc = ArtifactPlaybackService::instance()) {
      if (event->key() == Qt::Key_K) {
        svc->shuttleStop();
      } else if (event->key() == Qt::Key_L) {
        svc->shuttleForward();
      } else if (event->key() == Qt::Key_J) {
        svc->shuttleReverse();
      }
    }
    event->accept();
    return;
  }

  // I / O キーでワークエリアの IN / OUT を設定
  if (event->key() == Qt::Key_I || event->key() == Qt::Key_O) {
    if (auto *svc = ArtifactPlaybackService::instance()) {
      if (event->key() == Qt::Key_I) {
        svc->setWorkAreaStartAtCurrentFrame();
      } else if (event->key() == Qt::Key_O) {
        svc->setWorkAreaEndAtCurrentFrame();
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
      const bool isShift = event->modifiers() & Qt::ShiftModifier;

      if (event->key() == Qt::Key_BracketRight && isAlt && isShift) {
        ArtifactAbstractLayerPtr rippleTarget = selManager->currentLayer();
        if (!rippleTarget && !selectedLayers.isEmpty()) {
          rippleTarget = *selectedLayers.begin();
        }
        if (rippleTarget) {
          const qint64 oldOut = rippleTarget->outPoint().framePosition();
          const auto rippleLayers =
              collectRippleLaterLayers(comp, rippleTarget->id(), oldOut);
          QVector<ArtifactAbstractLayerPtr> snapshotLayers;
          snapshotLayers.reserve(rippleLayers.size() + 1);
          snapshotLayers.push_back(rippleTarget);
          for (const auto& rippleLayer : rippleLayers) {
            snapshotLayers.push_back(rippleLayer);
          }

          const auto beforeSnapshots =
              captureTimelineLayerStateSnapshots(comp, snapshotLayers);
          if (auto* mgr = UndoManager::instance()) {
            mgr->push(std::make_unique<RippleTrimOutCommand>(
                impl_->compositionId_, rippleTarget->id(), currentFrame,
                std::move(beforeSnapshots)));
            changed = true;
          } else {
            changed = applyTimelineRippleTrimOut(
                impl_->compositionId_, rippleTarget->id().toString(),
                currentFrame);
          }
        }

        if (changed) {
          Q_EMIT timelineDebugMessage(
              QStringLiteral("Ripple trimmed later layers at F%1")
                  .arg(currentFrame));
        }
        event->accept();
        return;
      }

      // Phase 2: Alt+Shift+[ で Ripple Trim In。
      // BracketLeft + Alt+Shift のとき、選択レイヤーの inPoint を詰めて後続を前に詰める。
      if (event->key() == Qt::Key_BracketLeft && isAlt && isShift) {
        ArtifactAbstractLayerPtr rippleTarget = selManager->currentLayer();
        if (!rippleTarget && !selectedLayers.isEmpty()) {
          rippleTarget = *selectedLayers.begin();
        }
        if (rippleTarget) {
          const qint64 oldIn = rippleTarget->inPoint().framePosition();
          const auto rippleLayers =
              collectRippleLaterLayers(comp, rippleTarget->id(), oldIn);
          QVector<ArtifactAbstractLayerPtr> snapshotLayers;
          snapshotLayers.reserve(rippleLayers.size() + 1);
          snapshotLayers.push_back(rippleTarget);
          for (const auto& rippleLayer : rippleLayers) {
            snapshotLayers.push_back(rippleLayer);
          }

          const auto beforeSnapshots =
              captureTimelineLayerStateSnapshots(comp, snapshotLayers);
          if (auto* mgr = UndoManager::instance()) {
            mgr->push(std::make_unique<RippleTrimInCommand>(
                impl_->compositionId_, rippleTarget->id(), currentFrame,
                std::move(beforeSnapshots)));
            changed = true;
          } else {
            changed = applyTimelineRippleTrimIn(
                impl_->compositionId_, rippleTarget->id().toString(),
                currentFrame);
          }
        }

        if (changed) {
          Q_EMIT timelineDebugMessage(
              QStringLiteral("Ripple Trim In at F%1").arg(currentFrame));
        }
        event->accept();
        return;
      }

      // Phase 2: Alt+Shift+Delete/Backspace で Ripple Delete (Close Gap)。
      // target を 0 幅に潰して後続を詰める。レイヤー完全削除はしない。
      if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
          isAlt && isShift) {
        ArtifactAbstractLayerPtr rippleTarget = selManager->currentLayer();
        if (!rippleTarget && !selectedLayers.isEmpty()) {
          rippleTarget = *selectedLayers.begin();
        }
        if (rippleTarget) {
          const qint64 oldIn = rippleTarget->inPoint().framePosition();
          const auto rippleLayers =
              collectRippleLaterLayers(comp, rippleTarget->id(), oldIn);
          QVector<ArtifactAbstractLayerPtr> snapshotLayers;
          snapshotLayers.reserve(rippleLayers.size() + 1);
          snapshotLayers.push_back(rippleTarget);
          for (const auto& rippleLayer : rippleLayers) {
            snapshotLayers.push_back(rippleLayer);
          }

          const auto beforeSnapshots =
              captureTimelineLayerStateSnapshots(comp, snapshotLayers);
          if (auto* mgr = UndoManager::instance()) {
            mgr->push(std::make_unique<RippleDeleteCommand>(
                impl_->compositionId_, rippleTarget->id(),
                std::move(beforeSnapshots)));
            changed = true;
          } else {
            changed = applyTimelineRippleDelete(
                impl_->compositionId_, rippleTarget->id().toString());
          }
        }

        if (changed) {
          Q_EMIT timelineDebugMessage(
              QStringLiteral("Ripple Delete (Close Gap) at F%1")
                  .arg(currentFrame));
        }
        event->accept();
        return;
      }

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

  if (event->key() == Qt::Key_PageUp || event->key() == Qt::Key_PageDown) {
   if (event->modifiers() & Qt::ControlModifier) {
    if (event->key() == Qt::Key_PageUp) {
     jumpToKeyframeHit(-1);
    } else {
     jumpToKeyframeHit(+1);
    }
    event->accept();
    return;
   }
   if (auto *svc = ArtifactProjectService::instance()) {
    auto comp = svc->currentComposition().lock();
    if (comp) {
     const int64_t delta = event->key() == Qt::Key_PageDown ? 10 : -10;
     comp->goToFrame(comp->framePosition().framePosition() + delta);
     event->accept();
     return;
    }
   }
  }

  if (event->key() == Qt::Key_Comma || event->key() == Qt::Key_Period) {
    if (auto *svc = ArtifactPlaybackService::instance()) {
    if (event->key() == Qt::Key_Comma) {
     svc->goToPreviousFrame();
    } else {
     svc->goToNextFrame();
    }
    event->accept();
    return;
   }
  }

  QWidget::keyPressEvent(event);
}

bool ArtifactTimelineWidget::handleTimelineAction(const ArtifactTimelineAction action)
{
  switch (action) {
  case ArtifactTimelineAction::CopySelectedKeyframes:
    copySelectedKeyframes();
    return true;
  case ArtifactTimelineAction::PasteKeyframesAtPlayhead:
    pasteKeyframesAtPlayhead();
    return true;
  case ArtifactTimelineAction::SelectAllKeyframes:
    selectAllKeyframes();
    return true;
  case ArtifactTimelineAction::AddKeyframeAtPlayhead:
    addKeyframeAtPlayhead();
    return true;
  case ArtifactTimelineAction::RemoveKeyframeAtPlayhead:
    removeKeyframeAtPlayhead();
    return true;
  case ArtifactTimelineAction::JumpToFirstKeyframe:
    jumpToFirstKeyframe();
    return true;
  case ArtifactTimelineAction::JumpToLastKeyframe:
    jumpToLastKeyframe();
    return true;
  case ArtifactTimelineAction::JumpToNextKeyframe:
    jumpToKeyframeHit(+1);
    return true;
  case ArtifactTimelineAction::JumpToPreviousKeyframe:
    jumpToKeyframeHit(-1);
    return true;
  case ArtifactTimelineAction::None:
    return false;
  }
  return false;
}

void ArtifactTimelineWidget::keyReleaseEvent(QKeyEvent *event) {
  if (!event || event->isAutoRepeat()) {
    return;
  }

  if (event->key() == Qt::Key_J || event->key() == Qt::Key_L) {
    if (auto *svc = ArtifactPlaybackService::instance()) {
      svc->shuttleStop();
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

  ArtifactAbstractLayerPtr currentLayer;
  int selectedCount = 0;
  if (selectionManager) {
    currentLayer = selectionManager->currentLayer();
    selectedCount = static_cast<int>(selectionManager->selectedLayers().size());
  }

  const auto state = collectKeyframeNavigationState(
      composition, selectionManager,
      static_cast<qint64>(std::llround(std::max(0.0, impl_->currentFrame_))));
  const int selectedKeyframeCount =
      impl_->painterTrackView_
          ? static_cast<int>(
                impl_->painterTrackView_->selectedKeyframeMarkers().size())
          : 0;
  const auto hoveredMarker =
      impl_->painterTrackView_ ? impl_->painterTrackView_->hoveredKeyframeMarker()
                               : ArtifactTimelineTrackPainterView::KeyframeMarkerVisual{};
  const qint64 currentFrameValue =
      static_cast<qint64>(std::llround(std::max(0.0, impl_->currentFrame_)));
  QString summary = formatKeyframeNavigationText(state);
  if (selectedKeyframeCount > 0) {
    summary += QStringLiteral(" | Selected keys: %1").arg(selectedKeyframeCount);
  }
  if (hoveredMarker.trackIndex >= 0) {
    const QString hoveredText =
        formatHoveredKeyframeSummary(hoveredMarker, currentFrameValue);
    if (!hoveredText.isEmpty()) {
      summary += QStringLiteral(" | %1").arg(hoveredText);
    }
  }
  impl_->keyframeStatusLabel_->setText(summary);
  if (impl_->easingLabButton_) {
    const bool hasSelection =
        impl_->painterTrackView_ &&
        !impl_->painterTrackView_->selectedKeyframeMarkers().isEmpty();
    impl_->easingLabButton_->setVisible(hasSelection);
  }
  if (impl_->keyPatternButton_) {
    const bool hasPropertyContext = impl_->layerTimelinePanel_ &&
                                    !impl_->layerTimelinePanel_->currentPropertyPath().trimmed().isEmpty();
    const bool hasLayerSelection = selectedCount > 0 || static_cast<bool>(currentLayer);
    impl_->keyPatternButton_->setVisible(hasPropertyContext && hasLayerSelection);
  }
}

void ArtifactTimelineWidget::updateSelectionState()
{
  if (!impl_ || !impl_->currentLayerLabel_) {
    return;
  }

  ArtifactAbstractLayerPtr currentLayer;
  int selectedCount = 0;
  int selectedKeyframeCount = 0;
  QString compositionLabel = QStringLiteral("-");
  QString currentLayerName;
  qint64 frameLabelValue = qRound64(std::max(0.0, impl_->currentFrame_));
  if (auto *app = ArtifactApplicationManager::instance()) {
    if (auto *selection = app->layerSelectionManager()) {
      currentLayer = selection->currentLayer();
      selectedCount = static_cast<int>(selection->selectedLayers().size());
      if (impl_->layerTimelinePanel_ && currentLayer &&
          currentLayer->id() != impl_->lastAutoScrolledLayerId_) {
        impl_->layerTimelinePanel_->scrollToLayer(currentLayer->id());
        impl_->lastAutoScrolledLayerId_ = currentLayer->id();
      }
    }
  }
  if (auto *svc = ArtifactProjectService::instance()) {
    if (auto composition = svc->currentComposition().lock()) {
      const QString name = composition->settings().compositionName().toQString().trimmed();
      compositionLabel = name.isEmpty() ? composition->id().toString() : name;
    }
  }

  const int effectiveSelectedCount =
      selectedCount > 0 ? selectedCount : (currentLayer ? 1 : 0);

  if (currentLayer) {
    currentLayerName = currentLayer->layerName().trimmed();
    if (!impl_->lastCurrentLayerName_.isEmpty() &&
        impl_->lastCurrentLayerName_ != currentLayerName) {
      pushRecentLayerName(impl_->recentLayerNames_, impl_->lastCurrentLayerName_);
    }
    impl_->lastCurrentLayerName_ = currentLayerName;
    impl_->currentLayerLabel_->setText(QStringLiteral("Current: %1")
                                          .arg(currentLayerName.isEmpty() ? QStringLiteral("(unnamed)")
                                                                          : currentLayerName));
  } else {
    impl_->currentLayerLabel_->setText(QStringLiteral("Current: Open a composition"));
  }
  impl_->currentLayerLabel_->setVisible(false);
  if (impl_->recentLayerLabel_) {
    impl_->recentLayerLabel_->setText(formatRecentLayersText(impl_->recentLayerNames_));
    impl_->recentLayerLabel_->setVisible(false);
  }
  if (impl_->frameSummaryLabel_) {
    impl_->frameSummaryLabel_->setText(
        QStringLiteral("Status: %1 | Frame: %2").arg(compositionLabel).arg(frameLabelValue));
    impl_->frameSummaryLabel_->setVisible(false);
  }
  if (impl_->selectionSummaryLabel_) {
    QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>
        selectedMarkers;
    if (impl_->painterTrackView_) {
      selectedMarkers = impl_->painterTrackView_->selectedKeyframeMarkers();
      selectedKeyframeCount = static_cast<int>(selectedMarkers.size());
    }
    const auto hoveredMarker =
        impl_->painterTrackView_ ? impl_->painterTrackView_->hoveredKeyframeMarker()
                                 : ArtifactTimelineTrackPainterView::KeyframeMarkerVisual{};
    if (effectiveSelectedCount <= 0 && selectedKeyframeCount <= 0) {
      impl_->selectionSummaryLabel_->setText(
          QStringLiteral("Selection: 0 layers | Select a layer to continue"));
    } else if (effectiveSelectedCount > 0 && selectedKeyframeCount <= 0) {
      impl_->selectionSummaryLabel_->setText(
          QStringLiteral("Selection: %1 layers | Keys: 0 | Lane: empty | Current: F%2 | Add/remove keyframe at playhead")
              .arg(effectiveSelectedCount)
              .arg(frameLabelValue));
    } else {
      QString selectionText =
          QStringLiteral("Selection: %1 layers | %2")
              .arg(effectiveSelectedCount)
              .arg(formatSelectedKeyframeSummary(selectedMarkers, frameLabelValue));
      if (hoveredMarker.trackIndex >= 0) {
        const QString hoveredText =
            formatHoveredKeyframeSummary(hoveredMarker, frameLabelValue);
        if (!hoveredText.isEmpty()) {
          const bool hoveredAtCurrent = std::llround(hoveredMarker.frame) == frameLabelValue;
          selectionText += hoveredAtCurrent
                               ? QStringLiteral(" | Hovered(current): %1").arg(hoveredText)
                               : QStringLiteral(" | Hovered: %1").arg(hoveredText);
        }
      }
      if (selectedKeyframeCount > 0) {
        qint64 nearestFrame = -1;
        qint64 nearestDelta = std::numeric_limits<qint64>::max();
        for (const auto& marker : selectedMarkers) {
          const qint64 markerFrame = static_cast<qint64>(std::llround(marker.frame));
          const qint64 delta = std::llabs(markerFrame - frameLabelValue);
          if (delta < nearestDelta) {
            nearestDelta = delta;
            nearestFrame = markerFrame;
          }
        }
        if (nearestFrame >= 0) {
          selectionText += nearestDelta == 0
                               ? QStringLiteral(" | Nearest: F%1 (current)").arg(nearestFrame)
                               : QStringLiteral(" | Nearest: F%1 (%2)")
                                     .arg(nearestFrame)
                                     .arg(QString::number(nearestFrame - frameLabelValue));
        }
      }
      impl_->selectionSummaryLabel_->setText(selectionText);
    }
    impl_->selectionSummaryLabel_->setVisible(false);
  }
  if (impl_->curveEditorSummaryLabel_) {
    const int curveCount = static_cast<int>(impl_->curveTracks_.size());
    impl_->curveEditorSummaryLabel_->setText(
        QStringLiteral("%1 | %2")
            .arg(compositionLabel)
            .arg(formatCurveTrackCountSummary(curveCount)));
  }
  if (impl_->curveEditor_ && impl_->graphEditorVisible_) {
    refreshCurveEditorTracks();
    if (impl_->curveEditor_) {
      impl_->curveEditor_->focusTrack(impl_->focusedCurveTrackIndex_);
    }
  } else {
    updateCurvePropertyList();
  }
}

void ArtifactTimelineWidget::syncPainterSelectionState(const bool forceRefresh)
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
      composition, selection, impl_->trackRows_, forceRefresh);
}

void ArtifactTimelineWidget::syncTimelineHorizontalOffset(const double offset)
{
  if (!impl_) {
    return;
  }
  const double clampedOffset =
      clampTimelineHorizontalOffset(impl_->painterTrackView_, offset);
  if (impl_->painterTrackView_) {
    impl_->painterTrackView_->setHorizontalOffset(clampedOffset);
  }
  if (impl_->scrubBar_) {
    impl_->scrubBar_->setRulerHorizontalOffset(clampedOffset);
  }
  if (impl_->workArea_) {
    impl_->workArea_->setRulerHorizontalOffset(clampedOffset);
  }
  syncPlayheadOverlay();
}

void ArtifactTimelineWidget::syncTimelineVerticalOffset(const double offset)
{
  if (!impl_) {
    return;
  }
  // Re-entry guard: setting verticalOffset on one panel triggers its EventBus
  // publish (for ArtifactLayerPanelWidget) which would re-fire this subscription.
  // The guard ensures only the first call runs to completion.
  if (impl_->syncingVerticalOffset_) {
    return;
  }
  impl_->syncingVerticalOffset_ = true;
  if (impl_->painterTrackView_) {
    const QSignalBlocker blocker(impl_->painterTrackView_);
    if (std::abs(impl_->painterTrackView_->verticalOffset() - offset) > 0.0001) {
      impl_->painterTrackView_->setVerticalOffset(offset);
    }
  }
  if (impl_->layerTimelinePanel_) {
    const QSignalBlocker blocker(impl_->layerTimelinePanel_);
    if (std::abs(impl_->layerTimelinePanel_->verticalOffset() - offset) > 0.0001) {
      impl_->layerTimelinePanel_->setVerticalOffset(offset);
    }
  }
  impl_->syncingVerticalOffset_ = false;
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
  if (impl_->workArea_) {
    impl_->workArea_->setRulerPixelsPerFrame(newZoom);
  }

  Q_EMIT zoomLevelChanged(newZoom * 100.0);

  const double offset =
      impl_->navigator_->startValue() * duration * newZoom;
  syncTimelineHorizontalOffset(offset);
  syncPlayheadOverlay();
}

void ArtifactTimelineWidget::syncPlayheadOverlay()
{
  if (!impl_ || !impl_->painterTrackView_) {
    return;
  }
  const double frame = std::max(0.0, impl_->currentFrame_);

  if (impl_->navigator_) {
    impl_->navigator_->setCurrentFrame(frame);
  }
  if (impl_->workArea_) {
    impl_->workArea_->setTotalFrames(
        static_cast<float>(std::max(1.0, impl_->painterTrackView_->durationFrames())));
    impl_->workArea_->setCurrentFrame(static_cast<float>(frame));
  }

  if (impl_->rightPanel_) {
    impl_->rightPanel_->syncPlayheadOverlay();
  }
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
    const double ppf = std::max(0.01, impl_->painterTrackView_->pixelsPerFrame());
    const double centeredOffset = std::max(
        0.0, static_cast<double>(targetFrame) * ppf -
                 static_cast<double>(impl_->painterTrackView_->width()) * 0.5);
    syncTimelineHorizontalOffset(centeredOffset);
  } else {
    syncPlayheadOverlay();
  }
  if (impl_->scrubBar_) {
    impl_->scrubBar_->setCurrentFrame(FramePosition(static_cast<int>(targetFrame)));
  }
  updateKeyframeState();
}

void ArtifactTimelineWidget::jumpToFirstKeyframe()
{
  if (!impl_) {
    return;
  }

  ArtifactCompositionPtr composition;
  if (auto* svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return;
  }

  ArtifactLayerSelectionManager* selectionManager = nullptr;
  if (auto* app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }
  const auto frames = collectSelectedKeyframeFrames(composition, selectionManager);
  if (frames.isEmpty()) {
    return;
  }

  const qint64 targetFrame = frames.front();
  impl_->currentFrame_ = static_cast<double>(targetFrame);
  if (impl_->painterTrackView_) {
    impl_->painterTrackView_->setCurrentFrame(static_cast<double>(targetFrame));
    const double ppf = std::max(0.01, impl_->painterTrackView_->pixelsPerFrame());
    const double centeredOffset = std::max(
        0.0, static_cast<double>(targetFrame) * ppf -
                 static_cast<double>(impl_->painterTrackView_->width()) * 0.5);
    syncTimelineHorizontalOffset(centeredOffset);
  } else {
    syncPlayheadOverlay();
  }
  if (impl_->scrubBar_) {
    impl_->scrubBar_->setCurrentFrame(FramePosition(static_cast<int>(targetFrame)));
  }
  updateKeyframeState();
}

void ArtifactTimelineWidget::jumpToLastKeyframe()
{
  if (!impl_) {
    return;
  }

  ArtifactCompositionPtr composition;
  if (auto* svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return;
  }

  ArtifactLayerSelectionManager* selectionManager = nullptr;
  if (auto* app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }
  const auto frames = collectSelectedKeyframeFrames(composition, selectionManager);
  if (frames.isEmpty()) {
    return;
  }

  const qint64 targetFrame = frames.back();
  impl_->currentFrame_ = static_cast<double>(targetFrame);
  if (impl_->painterTrackView_) {
    impl_->painterTrackView_->setCurrentFrame(static_cast<double>(targetFrame));
    const double ppf = std::max(0.01, impl_->painterTrackView_->pixelsPerFrame());
    const double centeredOffset = std::max(
        0.0, static_cast<double>(targetFrame) * ppf -
                 static_cast<double>(impl_->painterTrackView_->width()) * 0.5);
    syncTimelineHorizontalOffset(centeredOffset);
  } else {
    syncPlayheadOverlay();
  }
  if (impl_->scrubBar_) {
    impl_->scrubBar_->setCurrentFrame(FramePosition(static_cast<int>(targetFrame)));
  }
  updateKeyframeState();
}

void ArtifactTimelineWidget::addKeyframeAtPlayhead()
{
  if (!impl_) {
    return;
  }

  ArtifactCompositionPtr composition;
  if (auto* svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return;
  }

  ArtifactLayerSelectionManager* selectionManager = nullptr;
  if (auto* app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }

  auto layers = selectedTimelineLayers(selectionManager);
  if (layers.isEmpty() && selectionManager && selectionManager->currentLayer()) {
    layers.push_back(selectionManager->currentLayer());
  }
  if (layers.isEmpty()) {
    return;
  }

  const qint64 frame = static_cast<qint64>(std::llround(std::max(0.0, impl_->currentFrame_)));
  const auto refs = collectAnimatablePropertyRefs(layers);
  const auto beforeSnapshots = captureKeyframePropertySnapshots(composition, refs);
  bool changed = false;
  for (const auto& layer : layers) {
    changed |= applyKeyframeEditAtFrame(composition, layer, frame, false);
  }
  if (changed) {
    const auto afterSnapshots = captureKeyframePropertySnapshots(composition, refs);
    if (auto* mgr = UndoManager::instance()) {
      QPointer<ArtifactTimelineWidget> self(this);
      mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
          QStringLiteral("Add Keyframe at Playhead"),
          [self, composition, afterSnapshots]() {
            applyKeyframePropertySnapshots(composition, afterSnapshots);
            if (!self) {
              return;
            }
            self->refreshTracks();
            self->updateKeyframeState();
            self->updateSelectionState();
          },
          [self, composition, beforeSnapshots]() {
            applyKeyframePropertySnapshots(composition, beforeSnapshots);
            if (!self) {
              return;
            }
            self->refreshTracks();
            self->updateKeyframeState();
            self->updateSelectionState();
          }));
    }
    updateKeyframeState();
    Q_EMIT timelineDebugMessage(
        QStringLiteral("Added keyframe at F%1 for %2 layer(s)")
            .arg(frame)
            .arg(layers.size()));
  }
}

void ArtifactTimelineWidget::removeKeyframeAtPlayhead()
{
  if (!impl_) {
    return;
  }

  ArtifactCompositionPtr composition;
  if (auto* svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return;
  }

  ArtifactLayerSelectionManager* selectionManager = nullptr;
  if (auto* app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }

  auto layers = selectedTimelineLayers(selectionManager);
  if (layers.isEmpty() && selectionManager && selectionManager->currentLayer()) {
    layers.push_back(selectionManager->currentLayer());
  }
  if (layers.isEmpty()) {
    return;
  }

  const qint64 frame = static_cast<qint64>(std::llround(std::max(0.0, impl_->currentFrame_)));
  const auto refs = collectAnimatablePropertyRefs(layers);
  const auto beforeSnapshots = captureKeyframePropertySnapshots(composition, refs);
  bool changed = false;
  for (const auto& layer : layers) {
    changed |= applyKeyframeEditAtFrame(composition, layer, frame, true);
  }
  if (changed) {
    const auto afterSnapshots = captureKeyframePropertySnapshots(composition, refs);
    if (auto* mgr = UndoManager::instance()) {
      QPointer<ArtifactTimelineWidget> self(this);
      mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
          QStringLiteral("Remove Keyframe at Playhead"),
          [self, composition, afterSnapshots]() {
            applyKeyframePropertySnapshots(composition, afterSnapshots);
            if (!self) {
              return;
            }
            self->refreshTracks();
            self->updateKeyframeState();
            self->updateSelectionState();
          },
          [self, composition, beforeSnapshots]() {
            applyKeyframePropertySnapshots(composition, beforeSnapshots);
            if (!self) {
              return;
            }
            self->refreshTracks();
            self->updateKeyframeState();
            self->updateSelectionState();
          }));
    }
    updateKeyframeState();
    Q_EMIT timelineDebugMessage(
        QStringLiteral("Removed keyframe at F%1 for %2 layer(s)")
            .arg(frame)
            .arg(layers.size()));
  }
}

void ArtifactTimelineWidget::applyInterpolationToSelectedKeyframes(
    const ArtifactCore::InterpolationType type)
{
  if (!impl_ || !impl_->painterTrackView_) {
    return;
  }

  ArtifactCompositionPtr composition;
  if (auto* svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return;
  }

  const auto markers = impl_->painterTrackView_->selectedKeyframeMarkers();
  if (markers.isEmpty()) {
    return;
  }

  const int applied = applyInterpolationToSelectedKeyframesImpl(composition, markers, type);
  if (applied <= 0) {
    return;
  }

  refreshTracks();
  updateKeyframeState();
  Q_EMIT timelineDebugMessage(
      QStringLiteral("Applied %1 interpolation to %2 %3")
          .arg(interpolationTypeLabel(type))
          .arg(applied)
          .arg(applied == 1 ? QStringLiteral("keyframe")
                            : QStringLiteral("keyframes")));
}

void ArtifactTimelineWidget::selectAllKeyframes()
{
  if (!impl_ || !impl_->painterTrackView_) {
    return;
  }
  impl_->painterTrackView_->selectAllKeyframeMarkers();
  updateKeyframeState();
}

void ArtifactTimelineWidget::copySelectedKeyframes()
{
  if (!impl_ || !impl_->painterTrackView_) {
    return;
  }

  ArtifactCompositionPtr composition;
  if (auto* svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return;
  }

  auto markers = impl_->painterTrackView_->selectedKeyframeMarkers();
  if (markers.isEmpty()) {
    const auto hovered = impl_->painterTrackView_->hoveredKeyframeMarker();
    if (hovered.trackIndex >= 0) {
      markers.push_back(hovered);
    }
  }
  const QJsonArray keyframes = serializeSelectedKeyframes(composition, markers);
  if (keyframes.isEmpty()) {
    return;
  }

  const QString layerId = markers.isEmpty() ? QString() : markers.front().layerId.toString();
  ClipboardManager::instance().copyKeyframes(QStringLiteral("timeline"), keyframes, layerId);
  Q_EMIT timelineDebugMessage(
      QStringLiteral("Copied %1 %2 to clipboard")
          .arg(keyframes.size())
          .arg(keyframes.size() == 1 ? QStringLiteral("keyframe")
                                     : QStringLiteral("keyframes")));
}

void ArtifactTimelineWidget::pasteKeyframesAtPlayhead()
{
  if (!impl_) {
    return;
  }

  ArtifactCompositionPtr composition;
  if (auto* svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return;
  }

  ClipboardManager::instance().syncFromSystemClipboard();
  if (!ClipboardManager::instance().hasKeyframeData()) {
    return;
  }

  ArtifactLayerSelectionManager* selectionManager = nullptr;
  if (auto* app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }
  auto targetLayers = selectedTimelineLayers(selectionManager);
  if (targetLayers.isEmpty() && selectionManager && selectionManager->currentLayer()) {
    targetLayers.push_back(selectionManager->currentLayer());
  }
  if (targetLayers.isEmpty()) {
    return;
  }

  const qint64 frame = static_cast<qint64>(std::llround(std::max(0.0, impl_->currentFrame_)));
  const QJsonArray keyframes = ClipboardManager::instance().pasteKeyframes();
  const auto beforeSelectionKeys = impl_->painterTrackView_
                                       ? keyframeSelectionKeysFromMarkers(
                                             impl_->painterTrackView_->selectedKeyframeMarkers())
                                       : QSet<QString>{};
  const auto refs = collectPropertyRefsFromClipboardRecords(composition, targetLayers, keyframes);
  const auto beforeSnapshots = captureKeyframePropertySnapshots(composition, refs);
  QSet<QString> selectionKeys;
  int mergedExistingKeyframeCount = 0;
  if (!pasteKeyframesToLayers(composition, targetLayers, keyframes, frame,
                              &selectionKeys, &mergedExistingKeyframeCount)) {
    return;
  }
  const auto afterSnapshots = captureKeyframePropertySnapshots(composition, refs);

  if (auto* svc = ArtifactProjectService::instance()) {
    svc->projectChanged();
  }
  refreshTracks();
  if (impl_->painterTrackView_ && !selectionKeys.isEmpty()) {
    impl_->painterTrackView_->setSelectedKeyframeKeys(selectionKeys);
  }
  if (auto* mgr = UndoManager::instance()) {
    QPointer<ArtifactTimelineWidget> self(this);
    const QSet<QString> afterSelectionKeys = selectionKeys;
    mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
        QStringLiteral("Paste Keyframes at Playhead"),
        [self, composition, afterSnapshots, afterSelectionKeys]() {
          applyKeyframePropertySnapshots(composition, afterSnapshots);
          if (!self) {
            return;
          }
          self->refreshTracks();
          if (self->impl_ && self->impl_->painterTrackView_) {
            self->impl_->painterTrackView_->setSelectedKeyframeKeys(afterSelectionKeys);
          }
          self->updateKeyframeState();
          self->updateSelectionState();
        },
        [self, composition, beforeSnapshots, beforeSelectionKeys]() {
          applyKeyframePropertySnapshots(composition, beforeSnapshots);
          if (!self) {
            return;
          }
          self->refreshTracks();
          if (self->impl_ && self->impl_->painterTrackView_) {
            self->impl_->painterTrackView_->setSelectedKeyframeKeys(beforeSelectionKeys);
          }
          self->updateKeyframeState();
          self->updateSelectionState();
        }));
  }
  updateKeyframeState();
  const QString mergeNote = mergedExistingKeyframeCount > 0
                                ? (mergedExistingKeyframeCount == 1
                                       ? QStringLiteral(" (merged 1 existing keyframe at destination)")
                                       : QStringLiteral(" (merged %1 existing keyframes at destination)")
                                             .arg(mergedExistingKeyframeCount))
                                : QString();
  Q_EMIT timelineDebugMessage(
      QStringLiteral("Pasted %1 %3 at F%2%4")
          .arg(keyframes.size())
          .arg(frame)
          .arg(keyframes.size() == 1 ? QStringLiteral("keyframe")
                                     : QStringLiteral("keyframes"))
          .arg(mergeNote));
}

}; // namespace Artifact

