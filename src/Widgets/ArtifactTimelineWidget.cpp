module;

#include <QBoxLayout>
#include <QBrush>
#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QSignalBlocker>
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

module Artifact.Widgets.Timeline;

import std;

import Widgets.Utils.CSS;

import Artifact.Layers.Hierarchy.Model;
import Artifact.Widget.WorkAreaControlWidget;

import Artifact.Widgets.LayerPanelWidget;
import Widget.CurveEditor;
import Artifact.Timeline.ScrubBar;
import Artifact.Timeline.KeyframeModel;
import Artifact.Widgets.Timeline.Label;
import Artifact.Timeline.NavigatorWidget;
import Artifact.Timeline.TrackPainterView;
import Artifact.Timeline.TimeCodeWidget;
import Artifact.Layers.Selection.Manager;
import Panel.DraggableSplitter;
import Artifact.Widgets.Timeline.GlobalSwitches;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Clipboard.ClipboardManager;
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
import Property.Abstract;
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

  for (const auto &layer : selected) {
    if (!layer || (current && layer == current)) {
      continue;
    }
    layers.push_back(layer);
  }

  std::sort(layers.begin(), layers.end(),
            [](const ArtifactAbstractLayerPtr &lhs,
               const ArtifactAbstractLayerPtr &rhs) {
              if (!lhs || !rhs) {
                return static_cast<bool>(lhs) && !static_cast<bool>(rhs);
              }
              const int nameCompare = lhs->layerName().compare(rhs->layerName(), Qt::CaseInsensitive);
              if (nameCompare != 0) {
                return nameCompare < 0;
              }
              return lhs->id().toString() < rhs->id().toString();
            });
  return layers;
}

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

CurveEditorSnapshot buildCurveEditorSnapshot(
    const ArtifactCompositionPtr &composition,
    ArtifactLayerSelectionManager *selectionManager)
{
  CurveEditorSnapshot snapshot;
  const auto layers = selectedTimelineLayers(selectionManager);
  if (layers.isEmpty()) {
    snapshot.summary = QStringLiteral("No selection");
    return snapshot;
  }

  const double fps = std::max(
      1.0, static_cast<double>(composition ? composition->frameRate().framerate() : 30.0));

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
          if (keyframes[i].easing != ArtifactCore::EasingType::Hold) {
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
    snapshot.summary = QStringLiteral("%1 curve track(s), %2 keyframe(s)")
                          .arg(snapshot.tracks.size())
                          .arg(totalKeyCount);
  }
  return snapshot;
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
    record.insert(QStringLiteral("easing"), static_cast<int>(it->easing));
    keyframes.append(record);
  }

  return keyframes;
}

bool pasteKeyframesToLayers(
    const ArtifactCompositionPtr& composition,
    const QVector<ArtifactAbstractLayerPtr>& targetLayers,
    const QJsonArray& records,
    const qint64 targetFrame)
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
      const auto easingValue =
          static_cast<ArtifactCore::EasingType>(record.value(QStringLiteral("easing")).toInt(
              static_cast<int>(ArtifactCore::EasingType::Linear)));

      property->addKeyFrame(time, value.isValid() ? value : property->getValue(), easingValue);
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

bool moveTimelineKeyframe(const ArtifactCompositionPtr& composition,
                          const ArtifactAbstractLayerPtr& layer,
                          const QString& propertyPath,
                          const qint64 fromFrame,
                          const qint64 toFrame)
{
  if (!composition || !layer || propertyPath.trimmed().isEmpty()) {
    return false;
  }
  if (fromFrame == toFrame) {
    return false;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  const RationalTime fromTime(fromFrame, static_cast<int64_t>(std::llround(fps)));
  const RationalTime toTime(toFrame, static_cast<int64_t>(std::llround(fps)));

  ArtifactTimelineKeyframeModel model;
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
  bool currentFrameHasKeyframe = false;
  qint64 previousKeyframe = -1;
  qint64 nextKeyframe = -1;
};

struct CurveEditorBinding {
  LayerID layerId;
  QString propertyPath;
};

struct CurveEditorPayload {
  std::vector<CurveTrack> tracks;
  std::vector<CurveEditorBinding> bindings;
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
        QVector<EasingType> easings;
        frames.reserve(static_cast<int>(keyframes.size()));
        values.reserve(static_cast<int>(keyframes.size()));
        easings.reserve(static_cast<int>(keyframes.size()));

        bool anyNumeric = false;
        for (const auto& keyframe : keyframes) {
          const qint64 frame = keyframe.time.rescaledTo(static_cast<int64_t>(std::round(fps)));
          const QVariant value = keyframe.value;
          if (!value.canConvert<double>()) {
            continue;
          }

          frames.push_back(frame);
          values.push_back(value.toDouble());
          easings.push_back(keyframe.easing);
          anyNumeric = true;
        }

        if (!anyNumeric || frames.isEmpty()) {
          continue;
        }

        for (int i = 0; i < frames.size(); ++i) {
          CurveKey curveKey;
          curveKey.frame = frames[i];
          curveKey.value = static_cast<float>(values[i]);
          curveKey.smooth = easings[i] == EasingType::Bezier;

          if (easings[i] != EasingType::Hold) {
            const double prevFrame = (i > 0) ? static_cast<double>(frames[i - 1])
                                             : static_cast<double>(frames[i]);
            const double nextFrame =
                (i + 1 < frames.size()) ? static_cast<double>(frames[i + 1])
                                        : static_cast<double>(frames[i]);
            const double prevValue = (i > 0) ? values[i - 1] : values[i];
            const double nextValue =
                (i + 1 < values.size()) ? values[i + 1] : values[i];
            const double inSpan = std::max(1.0, (static_cast<double>(frames[i]) - prevFrame) / 3.0);
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

            curveKey.inHandleFrame = -static_cast<int64_t>(std::llround(inSpan));
            curveKey.outHandleFrame = static_cast<int64_t>(std::llround(outSpan));
            curveKey.inHandleValue = static_cast<float>(-inSlope * inSpan);
            curveKey.outHandleValue = static_cast<float>(outSlope * outSpan);
          }

          track.keys.push_back(curveKey);
        }

        payload.bindings.push_back({layer->id(), property->getName()});
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

  if (property->hasKeyFrameAt(oldTime)) {
    property->removeKeyFrame(oldTime);
  }
  property->addKeyFrame(newTime, QVariant(newValue),
                        oldKey.smooth ? EasingType::Bezier : EasingType::Linear);
  return true;
}

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
    return leftHandleRect.contains(pos) || rightHandleRect.contains(pos);
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
  QPointer<QWidget> playheadOverlay_;
};

class TimelinePlayheadOverlayWidget final : public QWidget {
public:
  explicit TimelinePlayheadOverlayWidget(ArtifactTimelineTrackPainterView *trackView,
                                        QWidget *parent = nullptr)
      : QWidget(parent), trackView_(trackView)
  {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
  }

  protected:
  void paintEvent(QPaintEvent * /*event*/) override
  {
    QPainter p(this);

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

    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor playheadColor(255, 106, 71);
    const qreal headTop = 1.5;
    const qreal headHeight = 13.0;
    const qreal headWidth = 16.0;
    const qreal stemTop = headTop + headHeight + 2.0;
    const qreal stemBottom = static_cast<qreal>(height()) - 1.0;

    QPainterPath headPath;
    headPath.moveTo(playheadX, headTop + headHeight);
    headPath.lineTo(playheadX - headWidth * 0.5, headTop);
    headPath.lineTo(playheadX + headWidth * 0.5, headTop);
    headPath.closeSubpath();

    p.setPen(QPen(QColor(18, 18, 18, 150), 1));
    p.setBrush(playheadColor);
    p.drawPath(headPath);

    p.setPen(QPen(playheadColor, 2, Qt::SolidLine, Qt::FlatCap));
    p.drawLine(QPointF(playheadX, stemTop), QPointF(playheadX, stemBottom));
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
  QLabel *curveEditorSummaryLabel_ = nullptr;
  QToolButton *curveEditorFitButton_ = nullptr;
  ArtifactTimelineScrubBar *scrubBar_ = nullptr;
  WorkAreaControl *workArea_ = nullptr;
  ArtifactTimelineNavigatorWidget *navigator_ = nullptr;
  TimelinePlayheadOverlayWidget *playheadOverlay_ = nullptr;
  ArtifactTimelineGlobalSwitches *globalSwitches_ = nullptr;
  CompositionID compositionId_;
  bool shyActive_ = false;
  QString filterText_;
  QVector<LayerID> searchResultLayerIds_;
  int searchResultIndex_ = -1;
  QVector<TimelineRowDescriptor> trackRows_;
  std::vector<CurveTrack> curveTracks_;
  QVector<CurveTrackBinding> curveBindings_;
  QString curveEditorSignature_;
  bool syncingLayerSelection_ = false;
  double currentFrame_ = 0.0;
  bool curveEditorDragging_ = false;
  QMetaObject::Connection compositionChangedConnection_;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  // refreshTracks() の重複キューイング防止フラグ。
  // ProjectChangedEvent と LayerChangedEvent が同一フレームで両方発火した場合に
  // refreshTracks() が 2 回実行されるのを防ぐ。
  bool pendingRefresh_ = false;
  // selection sync の重複キューイング防止フラグ。
  // SelectionChangedEvent と LayerSelectionChangedEvent が同時に来ても
  // painter / labels 更新を 1 回にまとめる。
  bool pendingSelectionSync_ = false;
  bool pendingSelectionSyncForceRefresh_ = false;
  QTimer* curveEditorRefreshTimer_ = nullptr;
  bool graphEditorVisible_ = false;
  bool graphEditorNeedsFit_ = false;
  // Last playhead x-position in parent (rightPanel) coordinates, used to
  // dirty the correct strip when the playhead moves.
  int lastPlayheadParentX_ = -9999;
};

ArtifactTimelineWidget::Impl::Impl() {}

ArtifactTimelineWidget::Impl::~Impl() {}

void ArtifactTimelineWidget::updateCacheVisuals()
{
  if (!impl_ || !impl_->scrubBar_) {
    return;
  }

  if (auto* svc = ArtifactPlaybackService::instance()) {
    impl_->scrubBar_->setCacheBitmap(svc->ramPreviewCacheBitmap());
  }
}

void ArtifactTimelineWidget::refreshCurveEditorTracks()
{
  if (!impl_ || !impl_->curveEditor_ || impl_->curveEditorDragging_) {
    return;
  }

  ArtifactCompositionPtr composition;
  if (auto* svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }

  ArtifactLayerSelectionManager* selectionManager = nullptr;
  if (auto* app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }

  const auto payload = buildCurveEditorSnapshot(composition, selectionManager);
  if (payload.signature == impl_->curveEditorSignature_) {
    return;
  }

  impl_->curveEditorSignature_ = payload.signature;
  impl_->curveTracks_ = payload.tracks;
  impl_->curveBindings_ = payload.bindings;
  if (impl_->curveEditorSummaryLabel_) {
    impl_->curveEditorSummaryLabel_->setText(payload.summary);
  }
  qDebug() << "[CurveEditor] refresh"
           << "tracks=" << payload.tracks.size()
           << "bindings=" << payload.bindings.size()
           << "summary=" << payload.summary;
  impl_->curveEditor_->setTracks(payload.tracks);
  impl_->curveEditor_->setCurrentFrame(
      static_cast<int64_t>(std::llround(std::max(0.0, impl_->currentFrame_))));
  if (impl_->graphEditorVisible_ && impl_->graphEditorNeedsFit_) {
    impl_->curveEditor_->fitToContent();
    impl_->graphEditorNeedsFit_ = false;
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
  auto zoomSummaryLabel = new QLabel();
  auto selectionSummaryLabel = new QLabel();

  impl_->searchBar_ = searchBar;
  impl_->searchStatusLabel_ = searchStatusLabel;
  impl_->keyframeStatusLabel_ = keyframeStatusLabel;
  impl_->currentLayerLabel_ = currentLayerLabel;
  impl_->frameSummaryLabel_ = frameSummaryLabel;
  impl_->zoomSummaryLabel_ = zoomSummaryLabel;
  impl_->selectionSummaryLabel_ = selectionSummaryLabel;
  impl_->globalSwitches_ = globalSwitches;

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
  auto *toggleCurveEditorShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_G), this);
  QObject::connect(toggleCurveEditorShortcut, &QShortcut::activated, this, [this, globalSwitches]() {
    if (!impl_ || !globalSwitches) {
      return;
    }
    globalSwitches->setGraphEditorActive(!impl_->graphEditorVisible_);
    if (impl_->graphEditorVisible_ && impl_->curveEditor_) {
      impl_->curveEditor_->setFocus(Qt::ShortcutFocusReason);
    } else if (impl_->painterTrackView_) {
      impl_->painterTrackView_->setFocus(Qt::ShortcutFocusReason);
    }
  });
  auto *tabCurveEditorShortcut = new QShortcut(QKeySequence(Qt::Key_Tab), this);
  tabCurveEditorShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(tabCurveEditorShortcut, &QShortcut::activated, this, [this]() {
    if (!impl_ || !impl_->globalSwitches_) {
      return;
    }
    impl_->globalSwitches_->setGraphEditorActive(!impl_->graphEditorVisible_);
    if (impl_->graphEditorVisible_ && impl_->curveEditor_) {
      impl_->curveEditor_->setFocus(Qt::ShortcutFocusReason);
    } else if (impl_->painterTrackView_) {
      impl_->painterTrackView_->setFocus(Qt::ShortcutFocusReason);
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
  keyframeStatusLabel->setVisible(false);
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
  currentLayerLabel->setVisible(false);
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
  frameSummaryLabel->setCursor(Qt::PointingHandCursor);
  zoomSummaryLabel->setCursor(Qt::PointingHandCursor);
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
  selectionSummaryLabel->setVisible(false);
  globalSwitches->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  globalSwitches->setFixedWidth(globalSwitches->sizeHint().width());

  auto searchBarLayout = new QHBoxLayout();
  searchBarLayout->setSpacing(8);
  searchBarLayout->setContentsMargins(0, 0, 8, 0);
  searchBarLayout->addWidget(leftHeader);
  searchBarLayout->addWidget(searchBar);
  searchBarLayout->addWidget(displayModeCombo);
  searchBarLayout->addWidget(densityCombo);
  searchBarLayout->addWidget(searchStatusLabel);
  searchBarLayout->addWidget(keyframeStatusLabel);
  searchBarLayout->addWidget(currentLayerLabel);
  searchBarLayout->addWidget(frameSummaryLabel);
  searchBarLayout->addWidget(zoomSummaryLabel);
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

  // legacy direct signal connection disabled — prefer EventBus
  if (false) QObject::connect(globalSwitches, &ArtifactTimelineGlobalSwitches::shyChanged,
                   this, &ArtifactTimelineWidget::onShyChanged);

  // Subscribe to global timeline switches via EventBus
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<TimelineShyChangedEvent>([this](const TimelineShyChangedEvent& e) {
        QMetaObject::invokeMethod(this, [this, e]() { onShyChanged(e.shy); }, Qt::QueuedConnection);
      }));
    QObject::connect(layerTreeView,
                   &ArtifactLayerTimelinePanelWrapper::visibleRowsChanged, this,
                   [this]() {
                    refreshTracks();
                    updateSelectionState();
                    updateSearchState();
                   });

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
  curvePropertyList->setFocusPolicy(Qt::NoFocus);
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

  auto *rightPanelLayout = new QVBoxLayout();
  rightPanelLayout->setSpacing(0);
  rightPanelLayout->setContentsMargins(0, 0, 0, 0);
  auto timeNavigatorWidget = impl_->navigator_ =
      new ArtifactTimelineNavigatorWidget();
  auto workAreaWidget = impl_->workArea_ = new WorkAreaControl();
  auto scrubBar = impl_->scrubBar_ = new ArtifactTimelineScrubBar();

  auto painterTrackView = impl_->painterTrackView_ =
      new ArtifactTimelineTrackPainterView();
  auto curveEditor = impl_->curveEditor_ = new ArtifactCurveEditorWidget();
  painterTrackView->setDurationFrames(kDefaultTimelineFrames);
  painterTrackView->setTrackCount(1);
  painterTrackView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  curveEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  curveEditor->setMinimumHeight(180);
  curveEditor->setHandleEditingEnabled(false);
  curveEditor->setVisible(true);

  auto *curveHeader = new QWidget();
  curveHeader->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto *curveHeaderLayout = new QHBoxLayout(curveHeader);
  curveHeaderLayout->setContentsMargins(6, 4, 6, 4);
  curveHeaderLayout->setSpacing(6);
  impl_->curveEditorSummaryLabel_ = new QLabel(QStringLiteral("カーブエディタ"));
  impl_->curveEditorSummaryLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  impl_->curveEditorSummaryLabel_->setToolTip(QStringLiteral("選択したキーフレームのカーブ編集ビュー"));
  impl_->curveEditorFitButton_ = new QToolButton(curveHeader);
  impl_->curveEditorFitButton_->setText(QStringLiteral("Fit"));
  impl_->curveEditorFitButton_->setAutoRaise(true);
  impl_->curveEditorFitButton_->setToolTip(QStringLiteral("表示中のカーブに合わせてビューを調整"));
  QObject::connect(impl_->curveEditorFitButton_, &QToolButton::clicked, this, [this]() {
    if (impl_ && impl_->curveEditor_) {
      impl_->curveEditor_->fitToContent();
    }
  });
  curveHeaderLayout->addWidget(impl_->curveEditorSummaryLabel_);
  curveHeaderLayout->addWidget(impl_->curveEditorFitButton_);
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
  scrubBar->setToolTip(QStringLiteral("RAM preview cache range"));
  scrubBar->setVisible(true);
  scrubBar->update();

  if (auto *playback = ArtifactPlaybackService::instance()) {
    scrubBar->setCachedFrameRange(static_cast<int>(playback->ramPreviewRange().start()),
                                  static_cast<int>(playback->ramPreviewRange().end()),
                                  playback->isRamPreviewEnabled());
    QObject::connect(playback, &ArtifactPlaybackService::ramPreviewStateChanged, this,
                     [scrubBar](bool enabled, const FrameRange &range) {
                       scrubBar->setCachedFrameRange(static_cast<int>(range.start()),
                                                    static_cast<int>(range.end()),
                                                    enabled);
                     });
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
        const auto composition = safeCompositionLookup(impl_->compositionId_);
        if (!composition || layerId.isNil() || propertyPath.trimmed().isEmpty()) {
          return;
        }
        const auto layer = composition->layerById(layerId);
        if (!layer) {
          return;
        }
        if (moveTimelineKeyframe(composition, layer, propertyPath, fromFrame,
                                 toFrame)) {
          refreshTracks();
          updateKeyframeState();
          Q_EMIT timelineDebugMessage(
              QStringLiteral("Moved keyframe %1 -> %2 for %3")
                  .arg(fromFrame)
                  .arg(toFrame)
                  .arg(ArtifactTimelineKeyframeModel::displayLabelForPropertyPath(
                      propertyPath)));
        }
      });
  QObject::connect(
      layerTreeView, &ArtifactLayerTimelinePanelWrapper::propertyFocusChanged,
      painterTrackView,
      [painterTrackView](const LayerID &layerId, const QString &propertyPath) {
        if (painterTrackView) {
          painterTrackView->setKeyframeContext(layerId, propertyPath);
        }
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

  auto rightPanel = new TimelineRightPanelWidget();
  rightPanelLayout->addWidget(timeNavigatorWidget);
  rightPanelLayout->addWidget(scrubBar);
  rightPanelLayout->addWidget(workAreaWidget);
  impl_->timelinePainterPage_ = new QWidget();
  auto *timelinePainterLayout = new QVBoxLayout(impl_->timelinePainterPage_);
  timelinePainterLayout->setContentsMargins(0, 0, 0, 0);
  timelinePainterLayout->setSpacing(0);
  timelinePainterLayout->addWidget(painterTrackView, 1);

  impl_->curveEditorPage_ = new QWidget();
  auto *curvePanelLayout = new QVBoxLayout(impl_->curveEditorPage_);
  curvePanelLayout->setContentsMargins(0, 0, 0, 0);
  curvePanelLayout->setSpacing(0);
  curvePanelLayout->addWidget(curveHeader);
  curvePanelLayout->addWidget(curveEditor, 1);

  impl_->timelineModeStack_ = new QStackedWidget();
  impl_->timelineModeStack_->addWidget(impl_->timelinePainterPage_);
  impl_->timelineModeStack_->addWidget(impl_->curveEditorPage_);
  impl_->timelineModeStack_->setCurrentWidget(impl_->timelinePainterPage_);
  rightPanelLayout->addWidget(impl_->timelineModeStack_, 1);
  rightPanel->setLayout(rightPanelLayout);

  impl_->playheadOverlay_ =
      new TimelinePlayheadOverlayWidget(painterTrackView, rightPanel);
  rightPanel->setPlayheadOverlay(impl_->playheadOverlay_);

  auto *headerSeekFilter =
      new HeaderSeekFilter(painterTrackView, scrubBar, rightPanel);
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

  QObject::connect(
      painterTrackView, &ArtifactTimelineTrackPainterView::verticalOffsetChanged,
      this,
      [this](double offset) {
        syncTimelineVerticalOffset(offset);
      });
  QObject::connect(
      impl_->layerTimelinePanel_, &ArtifactLayerTimelinePanelWrapper::verticalOffsetChanged,
      this,
      [this](double offset) {
        syncTimelineVerticalOffset(offset);
      });

  if (painterTrackView) {
    auto *viewResizeFilter =
        new ViewportResizeFilter(rightPanel, updateZoom, rightPanel);
    painterTrackView->installEventFilter(viewResizeFilter);
  }

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
            if (impl_->curveEditor_) {
              impl_->curveEditor_->setCurrentFrame(frame.framePosition());
            }
            syncPlayheadOverlay();
            const QSignalBlocker blocker(scrubBar);
            scrubBar->setCurrentFrame(frame);
            updateCacheVisuals();
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

  QVector<TimelineRowDescriptor> visibleRows;
  if (impl_->layerTimelinePanel_) {
    visibleRows = impl_->layerTimelinePanel_->visibleTimelineRowDescriptors();
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
        TimelineRowDescriptor descriptor;
        descriptor.layerId = layer->id();
        descriptor.kind = TimelineRowKind::Layer;
        visibleRows.push_back(std::move(descriptor));
      }
    }
  }

  impl_->trackRows_ = visibleRows;
  QVector<int> trackHeights;
  const int trackCount = std::max(1, static_cast<int>(visibleRows.size()));
  trackHeights.reserve(trackCount);
  for (int i = 0; i < trackCount; ++i) {
    trackHeights.push_back(static_cast<int>(kTimelineRowHeight));
  }
  QVector<ArtifactTimelineTrackPainterView::TrackClipVisual> painterClips;
  painterClips.reserve(visibleRows.size());
  if (impl_->painterTrackView_) {
    impl_->painterTrackView_->setTrackHeights(trackHeights);
  }
  for (int rowIndex = 0; rowIndex < visibleRows.size(); ++rowIndex) {
    const auto &row = visibleRows[rowIndex];
    const int trackIndex = rowIndex;
    if (row.kind != TimelineRowKind::Layer || row.layerId.isNil()) {
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
      visual.trackIndex = trackIndex;
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
    syncTimelineVerticalOffset(impl_->painterTrackView_->verticalOffset());
    syncPainterSelectionState();
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
  if (impl_->curveEditorSummaryLabel_) {
    const int curveCount = static_cast<int>(impl_->curveTracks_.size());
    impl_->curveEditorSummaryLabel_->setText(
        QStringLiteral("%1 | %2 curve track(s)")
            .arg(compositionLabel)
            .arg(curveCount));
  }
  if (impl_->curveEditor_ && impl_->graphEditorVisible_) {
    refreshCurveEditorTracks();
    if (impl_->curveEditor_) {
      impl_->curveEditor_->focusTrack(impl_->focusedCurveTrackIndex_);
    }
  }
  updateCurvePropertyList();
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
  if (impl_->painterTrackView_) {
    impl_->painterTrackView_->setHorizontalOffset(offset);
  }
  if (impl_->scrubBar_) {
    impl_->scrubBar_->setRulerHorizontalOffset(offset);
  }
  syncPlayheadOverlay();
}

void ArtifactTimelineWidget::syncTimelineVerticalOffset(const double offset)
{
  if (!impl_) {
    return;
  }
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

  Q_EMIT zoomLevelChanged(newZoom * 100.0);

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
  impl_->playheadOverlay_->raise();

  // Update the parent strip so Qt re-composites the full z-stack in that region
  // (parent → track view → overlay).  The opaque track view paints over the old
  // playhead ghost before the overlay draws the new position on top.
  QWidget *parent = impl_->playheadOverlay_->parentWidget();
  if (parent && impl_->painterTrackView_) {
    const double ppf    = std::max(0.01, impl_->painterTrackView_->pixelsPerFrame());
    const double xOff   = impl_->painterTrackView_->horizontalOffset();
    const double frame  = impl_->painterTrackView_->currentFrame();
    const QPoint origin = impl_->painterTrackView_->mapTo(parent, QPoint(0, 0));
    const int newX      = origin.x() + static_cast<int>(frame * ppf - xOff);

    // headWidth is 16 px; add a few pixels for the line and anti-aliasing.
    constexpr int kMargin = 12;
    const int oldX = impl_->lastPlayheadParentX_;
    impl_->lastPlayheadParentX_ = newX;

    // Dirty a strip that covers both the previous and current playhead positions
    // so the old ghost is cleared and the new line is drawn.
    const int left  = std::min(oldX, newX) - kMargin;
    const int right = std::max(oldX, newX) + kMargin + 1;
    const QRect strip(left, 0, right - left, parent->height());
    // Parent must repaint first (clears the old playhead from the backing store),
    // then the overlay repaints on top to draw the new position.
    parent->update(strip);
    impl_->playheadOverlay_->update(strip);
  } else if (parent) {
    parent->update(impl_->playheadOverlay_->geometry());
    impl_->playheadOverlay_->update();
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
  }
  syncPlayheadOverlay();
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
  }
  syncPlayheadOverlay();
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
  }
  syncPlayheadOverlay();
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
  bool changed = false;
  for (const auto& layer : layers) {
    changed |= applyKeyframeEditAtFrame(composition, layer, frame, false);
  }
  if (changed) {
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
  bool changed = false;
  for (const auto& layer : layers) {
    changed |= applyKeyframeEditAtFrame(composition, layer, frame, true);
  }
  if (changed) {
    updateKeyframeState();
    Q_EMIT timelineDebugMessage(
        QStringLiteral("Removed keyframe at F%1 for %2 layer(s)")
            .arg(frame)
            .arg(layers.size()));
  }
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

  const auto markers = impl_->painterTrackView_->selectedKeyframeMarkers();
  const QJsonArray keyframes = serializeSelectedKeyframes(composition, markers);
  if (keyframes.isEmpty()) {
    return;
  }

  const QString layerId = markers.isEmpty() ? QString() : markers.front().layerId.toString();
  ClipboardManager::instance().copyKeyframes(QStringLiteral("timeline"), keyframes, layerId);
  Q_EMIT timelineDebugMessage(
      QStringLiteral("Copied %1 keyframe(s) to clipboard").arg(keyframes.size()));
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
  if (!pasteKeyframesToLayers(composition, targetLayers, keyframes, frame)) {
    return;
  }

  if (auto* svc = ArtifactProjectService::instance()) {
    svc->projectChanged();
  }
  refreshTracks();
  updateKeyframeState();
  Q_EMIT timelineDebugMessage(
      QStringLiteral("Pasted %1 keyframe(s) at F%2").arg(keyframes.size()).arg(frame));
}

}; // namespace Artifact
