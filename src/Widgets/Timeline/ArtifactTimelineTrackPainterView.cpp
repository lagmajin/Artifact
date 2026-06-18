module;
#include <QContextMenuEvent>
#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHash>
#include <QIcon>
#include <QKeyEvent>
#include <QInputDialog>
#include <QMimeData>
#include <QMessageBox>
#include <cmath>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QRect>
#include <QRectF>
#include <QPointer>
#include <QSet>
#include <QSize>
#include <QStringList>
#include <QToolTip>
#include <QtGlobal>
#include <wobjectimpl.h>
#include "TimelinePlayheadDraw.hpp"


module Artifact.Timeline.TrackPainterView;

import std;
import Application.AppSettings;
import Clipboard.ClipboardManager;
import ArtifactCore.Utils.PerformanceProfiler;
import Widgets.Utils.CSS;
import Artifact.Application.Manager;
import Artifact.Service.Application;
import Artifact.Tool.Service;
import Artifact.Tool.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Artifact.Layers.Selection.Manager;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Audio.Waveform;
import Artifact.Timeline.KeyframeModel;
import Artifact.Timeline.KeyBinding;
import Artifact.Service.Project;
import Event.Bus;
import Artifact.Event.Types;
import File.TypeDetector;
import Frame.Position;
import Property.Abstract;
import Undo.UndoManager;
import Time.Rational;
import UI.ShortcutBindings;
import Utils.Path;

namespace Artifact {
W_OBJECT_IMPL(ArtifactTimelineTrackPainterView)

namespace {
std::shared_ptr<ArtifactCore::AbstractProperty> findLayerPropertyByPath(
    const ArtifactAbstractLayerPtr &layer, const QString &propertyPath);

struct TimelineThemeColors {
  QColor background;
  QColor surface;
  QColor border;
  QColor accent;
  QColor text;
};

TimelineThemeColors timelineThemeColors() {
  const auto &theme = ArtifactCore::currentDCCTheme();
  return {
      QColor(theme.backgroundColor), QColor(theme.secondaryBackgroundColor),
      QColor(theme.borderColor),     QColor(theme.accentColor),
      QColor(theme.textColor),
  };
}

constexpr int kDefaultTrackHeight = 28;
constexpr int kTrackSpacing = 0;
constexpr int kClipCorner = 4;
constexpr int kClipPadding = 6;
constexpr int kMinTrackCount = 1;
constexpr double kMarkerLaneStep = 8.0;
constexpr double kKeyframeSnapToPlayheadThresholdFrames = 0.35;

QIcon timelineStudioIcon(const QString &name) {
  return QIcon(ArtifactCore::resolveIconPath(QStringLiteral("Studio/%1.svg").arg(name)));
}

void setActionIcon(QAction *action, const QString &name) {
  if (action) {
    action->setIcon(timelineStudioIcon(name));
  }
}

void setMenuIcon(QMenu *menu, const QString &name) {
  if (menu) {
    menu->setIcon(timelineStudioIcon(name));
  }
}

LayerType inferDroppedLayerType(const QString &filePath) {
  if (filePath.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
    return LayerType::Shape;
  }
  ArtifactCore::FileTypeDetector detector;
  switch (detector.detectByExtension(filePath)) {
  case ArtifactCore::FileType::Image:
    return LayerType::Image;
  case ArtifactCore::FileType::Video:
    return LayerType::Video;
  case ArtifactCore::FileType::Audio:
    return LayerType::Audio;
  default:
    return LayerType::Unknown;
  }
}

bool isAcceptedDroppedLayerType(const LayerType type) {
  return type == LayerType::Image || type == LayerType::Video ||
         type == LayerType::Audio || type == LayerType::Shape;
}

QStringList collectDroppedAssetPaths(const QMimeData *mime) {
  QStringList paths;
  if (!mime) {
    return paths;
  }

  if (mime->hasUrls()) {
    for (const auto &url : mime->urls()) {
      if (!url.isLocalFile()) {
        continue;
      }
      const QString filePath = url.toLocalFile();
      const QFileInfo info(filePath);
      if (!info.exists() || info.isDir()) {
        continue;
      }
      if (isAcceptedDroppedLayerType(inferDroppedLayerType(filePath))) {
        paths.append(filePath);
      }
    }
  }

  if (!paths.isEmpty() || !mime->hasText()) {
    return paths;
  }

  const QStringList lines =
      mime->text().split(QStringLiteral("\n"), Qt::SkipEmptyParts);
  for (const QString &line : lines) {
    const QString filePath = line.trimmed();
    if (filePath.isEmpty()) {
      continue;
    }
    const QFileInfo info(filePath);
    if (!info.exists() || info.isDir()) {
      continue;
    }
    if (isAcceptedDroppedLayerType(inferDroppedLayerType(filePath))) {
      paths.append(filePath);
    }
  }
  return paths;
}

void enqueueDroppedTimelineAssets(const QStringList &validPaths) {
  auto *svc = ArtifactProjectService::instance();
  if (!svc || validPaths.isEmpty()) {
    return;
  }

  svc->importAssetsFromPathsAsync(validPaths, [svc](QStringList imported) {
    if (!svc || imported.isEmpty()) {
      return;
    }

    for (const auto &path : imported) {
      const LayerType type = inferDroppedLayerType(path);
      if (type == LayerType::Image) {
        ArtifactImageInitParams params(QFileInfo(path).baseName());
        params.setImagePath(path);
        svc->addLayerToCurrentComposition(params);
      } else if (type == LayerType::Shape) {
        ArtifactSvgInitParams params(QFileInfo(path).baseName());
        params.setSvgPath(path);
        svc->addLayerToCurrentComposition(params);
      } else if (type == LayerType::Audio) {
        ArtifactAudioInitParams params(QFileInfo(path).baseName());
        params.setAudioPath(path);
        svc->addLayerToCurrentComposition(params);
      } else if (type == LayerType::Video) {
        ArtifactVideoInitParams params(QFileInfo(path).baseName());
        params.setVideoPath(path);
        svc->addLayerToCurrentComposition(params);
      } else {
        ArtifactLayerInitParams params(QFileInfo(path).baseName(), type);
        svc->addLayerToCurrentComposition(params);
      }
    }
  });
}

double clampDurationFrames(const double value) { return std::max(1.0, value); }

double clampPixelsPerFrame(const double value) {
  return std::clamp(value, 0.05, 64.0);
}

bool timelineAllowOverscroll() {
  if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
    return settings->timelineAllowOverscroll();
  }
  return false;
}

bool deleteSelectedLayersFromTimeline(QWidget *parent) {
  auto *service = ArtifactProjectService::instance();
  auto *selection = ArtifactLayerSelectionManager::instance();
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
    confirmed = QMessageBox::question(parent, QStringLiteral("Delete Layer"),
                                      message, QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No) == QMessageBox::Yes;
  } else {
    confirmed = QMessageBox::question(
                   parent, QStringLiteral("Delete Layers"),
                   QStringLiteral("Delete %1 selected layers?")
                       .arg(layerIds.size()),
                   QMessageBox::Yes | QMessageBox::No, QMessageBox::No) ==
               QMessageBox::Yes;
  }
  if (!confirmed) {
    return false;
  }

  if (auto *selectionManager = ArtifactLayerSelectionManager::instance()) {
    selectionManager->clearSelection();
  }

  bool removed = false;
  for (const auto &layerId : layerIds) {
    removed = service->removeLayerFromComposition(comp->id(), layerId) || removed;
  }
  return removed;
}

double timelineOverscrollPaddingPx(const QWidget *widget) {
  const double widthHint = widget ? static_cast<double>(widget->width()) : 640.0;
  return std::clamp(std::max(96.0, widthHint * 0.25), 96.0, 320.0);
}

double clampTimelineHorizontalOffset(const QWidget *widget,
                                     const double durationFrames,
                                     const double pixelsPerFrame,
                                     const double offset) {
  const double maxOffset =
      std::max(0.0, durationFrames * std::max(0.001, pixelsPerFrame) -
                        static_cast<double>(std::max(1, widget ? widget->width() : 1)));
  if (timelineAllowOverscroll()) {
    const double pad = timelineOverscrollPaddingPx(widget);
    return std::clamp(offset, -pad, maxOffset + pad);
  }
  return std::clamp(offset, 0.0, maxOffset);
}

int trackTopAt(const QVector<int> &heights, const int trackIndex) {
  int y = 0;
  for (int i = 0; i < trackIndex && i < heights.size(); ++i) {
    y += heights[i] + kTrackSpacing;
  }
  return y;
}

int trackTopAt(const QVector<int> &trackTops, const QVector<int> &heights,
               const int trackIndex) {
  if (trackIndex < 0 || trackIndex >= heights.size()) {
    return 0;
  }
  if (trackIndex < trackTops.size()) {
    return trackTops[trackIndex];
  }
  return trackTopAt(heights, trackIndex);
}

int totalTrackContentHeight(const QVector<int> &heights) {
  int h = 0;
  for (int i = 0; i < heights.size(); ++i) {
    h += heights[i];
    if (i + 1 < heights.size()) {
      h += kTrackSpacing;
    }
  }
  return h;
}

bool applyTimelineLayerRangeEdit(const ArtifactAbstractLayerPtr &layer,
                                 qint64 startFrame,
                                 qint64 durationFrame,
                                 bool preserveExistingDuration);

constexpr int kEdgeHitZone = 6;

enum class DragMode { None, MoveBody, ResizeLeft, ResizeRight };

struct HitResult {
  DragMode mode = DragMode::None;
  int clipIndex = -1;
};

struct MarkerHitResult {
  int markerIndex = -1;
};

enum class KeyframeAreaHitPart { None, Body, LeftEdge, RightEdge };

struct KeyframeAreaVisual {
  int startMarkerIndex = -1;
  int endMarkerIndex = -1;
  int trackIndex = -1;
  LayerID layerId;
  QString propertyPath;
  double startFrame = 0.0;
  double endFrame = 0.0;
  QVariant value;
  QRectF bodyRect;
  QRectF leftHandleRect;
  QRectF rightHandleRect;
};

struct KeyframeAreaHitResult {
  int areaIndex = -1;
  KeyframeAreaHitPart part = KeyframeAreaHitPart::None;
};

bool isFlatAreaCandidate(const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual &lhs,
                         const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual &rhs) {
  if (lhs.layerId != rhs.layerId || lhs.propertyPath != rhs.propertyPath ||
      lhs.trackIndex != rhs.trackIndex) {
    return false;
  }
  if (static_cast<qint64>(std::llround(rhs.frame)) -
          static_cast<qint64>(std::llround(lhs.frame)) < 1) {
    return false;
  }
  if (lhs.value.isValid() && rhs.value.isValid() && lhs.value != rhs.value) {
    return false;
  }
  return true;
}

QVector<KeyframeAreaVisual> collectKeyframeAreas(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    const QVector<int> &heights, const QVector<int> &trackTops,
    const double ppf, const double xOffset, const double yOffset) {
  QVector<KeyframeAreaVisual> areas;
  for (int i = 0; i + 1 < markers.size(); ++i) {
    const auto &start = markers[i];
    const auto &end = markers[i + 1];
    if (!isFlatAreaCandidate(start, end)) {
      continue;
    }
    if (start.trackIndex < 0 || start.trackIndex >= heights.size()) {
      continue;
    }
    const int trackTop = trackTopAt(trackTops, heights, start.trackIndex);
    const int trackH = heights.value(start.trackIndex, kDefaultTrackHeight);
    const double leftX = start.frame * ppf - xOffset;
    const double rightX = end.frame * ppf - xOffset;
    const double width = std::max(8.0, rightX - leftX);
    const QRectF bodyRect(leftX, trackTop + 5.0 - yOffset, width,
                          std::max(4, trackH - 10));
    const QRectF leftHandleRect(leftX - 3.0, bodyRect.top(), 8.0, bodyRect.height());
    const QRectF rightHandleRect(rightX - 5.0, bodyRect.top(), 8.0, bodyRect.height());
    areas.push_back({i, i + 1, start.trackIndex, start.layerId, start.propertyPath,
                     start.frame, end.frame, start.value, bodyRect, leftHandleRect,
                     rightHandleRect});
  }
  return areas;
}

KeyframeAreaHitResult hitTestKeyframeAreas(
    const QVector<KeyframeAreaVisual> &areas, const double mouseX,
    const double mouseY) {
  for (int i = 0; i < areas.size(); ++i) {
    const auto &area = areas[i];
    const QPointF pos(mouseX, mouseY);
    if (area.leftHandleRect.contains(pos)) {
      return {i, KeyframeAreaHitPart::LeftEdge};
    }
    if (area.rightHandleRect.contains(pos)) {
      return {i, KeyframeAreaHitPart::RightEdge};
    }
    if (area.bodyRect.contains(pos)) {
      return {i, KeyframeAreaHitPart::Body};
    }
  }
  return {};
}

QString keyframeSelectionKey(const LayerID &layerId,
                             const QString &propertyPath, const qint64 frame) {
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

struct TimelineLayerStateSnapshot {
  LayerID layerId;
  qint64 inPoint = 0;
  qint64 outPoint = 0;
  qint64 startTime = 0;
  QVector<KeyframePropertySnapshot> keyframes;
};

QVector<KeyframePropertyRef> collectAnimatablePropertyRefs(
    const ArtifactAbstractLayerPtr &layer) {
  QVector<KeyframePropertyRef> refs;
  if (!layer) {
    return refs;
  }

  QSet<QString> seen;
  for (const auto &group : layer->getLayerPropertyGroups()) {
    for (const auto &property : group.sortedProperties()) {
      if (!property || !property->isAnimatable()) {
        continue;
      }
      const QString key =
          QStringLiteral("%1|%2").arg(layer->id().toString(), property->getName());
      if (seen.contains(key)) {
        continue;
      }
      seen.insert(key);
      refs.push_back({layer->id(), property->getName()});
    }
  }
  return refs;
}

QVector<KeyframePropertyRef> collectPropertyRefsFromMarkers(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers) {
  QVector<KeyframePropertyRef> refs;
  QSet<QString> seen;
  for (const auto &marker : markers) {
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

QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>
neighborMarkersForSelection(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    const QSet<QString> &selectedKeys) {
  QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> out;
  QHash<QString, QVector<int>> groups;
  for (int i = 0; i < markers.size(); ++i) {
    const auto &marker = markers[i];
    const QString key =
        keyframeSelectionKey(marker.layerId, marker.propertyPath,
                             static_cast<qint64>(std::llround(marker.frame)));
    groups[QStringLiteral("%1|%2").arg(marker.layerId.toString(), marker.propertyPath)]
        .push_back(i);
    if (selectedKeys.contains(key)) {
      out.push_back(marker);
    }
  }

  QSet<int> extraIndices;
  for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
    const auto &indices = it.value();
    for (int i = 0; i < indices.size(); ++i) {
      const int markerIndex = indices[i];
      const auto &marker = markers[markerIndex];
      const QString key =
          keyframeSelectionKey(marker.layerId, marker.propertyPath,
                               static_cast<qint64>(std::llround(marker.frame)));
      if (!selectedKeys.contains(key)) {
        continue;
      }
      if (i > 0) {
        extraIndices.insert(indices[i - 1]);
      }
      if (i + 1 < indices.size()) {
        extraIndices.insert(indices[i + 1]);
      }
    }
  }

  for (const int index : extraIndices) {
    if (index >= 0 && index < markers.size()) {
      out.push_back(markers[index]);
    }
  }

  std::sort(out.begin(), out.end(), [](const auto &lhs, const auto &rhs) {
    if (lhs.layerId != rhs.layerId) {
      return lhs.layerId.toString() < rhs.layerId.toString();
    }
    if (lhs.propertyPath != rhs.propertyPath) {
      return lhs.propertyPath < rhs.propertyPath;
    }
    return lhs.frame < rhs.frame;
  });
  out.erase(std::unique(out.begin(), out.end(), [](const auto &lhs, const auto &rhs) {
              return lhs.layerId == rhs.layerId &&
                     lhs.propertyPath == rhs.propertyPath &&
                     std::abs(lhs.frame - rhs.frame) < 0.0001;
            }),
            out.end());
  return out;
}

QVector<KeyframePropertySnapshot> captureKeyframePropertySnapshots(
    const ArtifactCompositionPtr &composition,
    const QVector<KeyframePropertyRef> &refs) {
  QVector<KeyframePropertySnapshot> snapshots;
  if (!composition || refs.isEmpty()) {
    return snapshots;
  }

  QSet<QString> seen;
  for (const auto &ref : refs) {
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

void applyKeyframePropertySnapshots(
    const ArtifactCompositionPtr &composition,
    const QVector<KeyframePropertySnapshot> &snapshots) {
  if (!composition || snapshots.isEmpty()) {
    return;
  }

  QSet<QString> changedLayerKeys;
  QVector<LayerID> changedLayers;
  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  const int64_t scale = static_cast<int64_t>(std::llround(fps));

  for (const auto &snapshot : snapshots) {
    const auto layer = composition->layerById(snapshot.layerId);
    if (!layer) {
      continue;
    }
    const auto property = findLayerPropertyByPath(layer, snapshot.propertyPath);
    if (!property) {
      continue;
    }

    property->clearKeyFrames();
    for (const auto &keyframe : snapshot.keyframes) {
      property->addKeyFrame(RationalTime(keyframe.time.rescaledTo(scale), scale), keyframe.value,
                            keyframe.interpolation, keyframe.cp1_x,
                            keyframe.cp1_y, keyframe.cp2_x, keyframe.cp2_y,
                            keyframe.roving);
      const RationalTime restoredTime(keyframe.time.rescaledTo(scale), scale);
      property->setKeyFrameAnchorAt(restoredTime, keyframe.anchor);
      property->setKeyFrameColorLabelAt(restoredTime, keyframe.colorLabel);
    }

    const QString layerKey = layer->id().toString();
    if (!changedLayerKeys.contains(layerKey)) {
      changedLayerKeys.insert(layerKey);
      changedLayers.push_back(layer->id());
    }
  }

  for (const auto &layerId : changedLayers) {
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

void shiftAnimatableLayerKeyframes(const ArtifactCompositionPtr &composition,
                                   const ArtifactAbstractLayerPtr &layer,
                                   const qint64 frameDelta) {
  if (!composition || !layer || frameDelta == 0) {
    return;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  const int64_t scale = static_cast<int64_t>(std::llround(fps));

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
        const int64_t oldFrame = keyframe.time.rescaledTo(scale);
        const int64_t newFrame = std::max<int64_t>(0, oldFrame + frameDelta);
        property->addKeyFrame(
            RationalTime(newFrame, scale),
            keyframe.value.isValid() ? keyframe.value : property->getValue(),
            keyframe.interpolation, keyframe.cp1_x, keyframe.cp1_y,
            keyframe.cp2_x, keyframe.cp2_y, keyframe.roving);
        const RationalTime newTime(newFrame, scale);
        property->setKeyFrameAnchorAt(newTime, keyframe.anchor);
        property->setKeyFrameColorLabelAt(newTime, keyframe.colorLabel);
      }
    }
  }
}

TimelineLayerStateSnapshot captureTimelineLayerStateSnapshot(
    const ArtifactCompositionPtr &composition, const ArtifactAbstractLayerPtr &layer) {
  TimelineLayerStateSnapshot snapshot;
  if (!composition || !layer) {
    return snapshot;
  }

  snapshot.layerId = layer->id();
  snapshot.inPoint = layer->inPoint().framePosition();
  snapshot.outPoint = layer->outPoint().framePosition();
  snapshot.startTime = layer->startTime().framePosition();
  snapshot.keyframes = captureKeyframePropertySnapshots(
      composition, collectAnimatablePropertyRefs(layer));
  return snapshot;
}

QVector<TimelineLayerStateSnapshot> captureTimelineLayerStateSnapshots(
    const ArtifactCompositionPtr &composition,
    const QVector<ArtifactAbstractLayerPtr> &layers) {
  QVector<TimelineLayerStateSnapshot> snapshots;
  if (!composition || layers.isEmpty()) {
    return snapshots;
  }

  snapshots.reserve(layers.size());
  for (const auto &layer : layers) {
    if (!layer) {
      continue;
    }
    snapshots.push_back(captureTimelineLayerStateSnapshot(composition, layer));
  }
  return snapshots;
}

void restoreTimelineLayerStateSnapshot(
    const ArtifactCompositionPtr &composition,
    const TimelineLayerStateSnapshot &snapshot) {
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
    const ArtifactCompositionPtr &composition,
    const QVector<TimelineLayerStateSnapshot> &snapshots) {
  if (!composition || snapshots.isEmpty()) {
    return;
  }

  for (const auto &snapshot : snapshots) {
    restoreTimelineLayerStateSnapshot(composition, snapshot);
  }
}

ArtifactCompositionPtr lookupTimelineComposition(const CompositionID &compositionId) {
  auto *svc = ArtifactProjectService::instance();
  if (!svc) {
    return nullptr;
  }

  auto result = svc->findComposition(compositionId);
  if (!result.success) {
    return nullptr;
  }
  return result.ptr.lock();
}

QVector<ArtifactAbstractLayerPtr> collectRippleLaterLayers(
    const ArtifactCompositionPtr &composition, const LayerID &targetLayerId,
    const qint64 boundaryFrame) {
  QVector<ArtifactAbstractLayerPtr> layers;
  if (!composition) {
    return layers;
  }

  for (const auto &layer : composition->allLayer()) {
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

bool applyTimelineRippleTrimOut(const CompositionID &compositionId,
                                const QString &layerIdText,
                                const qint64 currentFrame) {
  if (layerIdText.trimmed().isEmpty()) {
    return false;
  }

  const auto composition = lookupTimelineComposition(compositionId);
  if (!composition) {
    return false;
  }

  const auto layer = composition->layerById(LayerID(layerIdText));
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
      collectRippleLaterLayers(composition, layer->id(), oldOutPoint);

  if (!applyTimelineLayerRangeEdit(layer, oldInPoint, newDuration, false)) {
    return false;
  }

  if (rippleDelta == 0 || rippleLayers.isEmpty()) {
    return true;
  }

  for (const auto &rippleLayer : rippleLayers) {
    if (!rippleLayer) {
      continue;
    }

    const qint64 followerOldIn = rippleLayer->inPoint().framePosition();
    const qint64 followerOldOut = rippleLayer->outPoint().framePosition();
    const qint64 followerOldStart = rippleLayer->startTime().framePosition();
    const qint64 followerNewIn =
        std::max<qint64>(0, followerOldIn + rippleDelta);
    const qint64 actualDelta = followerNewIn - followerOldIn;
    if (actualDelta == 0) {
      continue;
    }

    rippleLayer->setInPoint(FramePosition(followerNewIn));
    rippleLayer->setOutPoint(FramePosition(
        std::max<qint64>(followerNewIn + 1, followerOldOut + actualDelta)));
    // target 側の applyTimelineLayerRangeEdit と整合させるため、startTime も
    // in/out と同じ delta で移動させる。これを忘れると follower の内部タイミング
    // （source clip の再生開始位置）が in/out だけズレて破綻する。
    rippleLayer->setStartTime(
        FramePosition(std::max<qint64>(0, followerOldStart + actualDelta)));
    shiftAnimatableLayerKeyframes(composition, rippleLayer, actualDelta);
    rippleLayer->changed();
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{compositionId.toString(), rippleLayer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }

  return true;
}

// Phase 2: Ripple Trim In
// 再生ヘッド位置で target の inPoint を詰め、後続レイヤーを前に詰める。
// delta = newInPoint - oldInPoint（負方向に詰める = follower を前に詰める）。
// target 側の keyframe は inPoint の移動に追従させる（Trim In の標準挙動）。
bool applyTimelineRippleTrimIn(const CompositionID &compositionId,
                               const QString &layerIdText,
                               const qint64 currentFrame) {
  if (layerIdText.trimmed().isEmpty()) {
    return false;
  }

  const auto composition = lookupTimelineComposition(compositionId);
  if (!composition) {
    return false;
  }

  const auto layer = composition->layerById(LayerID(layerIdText));
  if (!layer) {
    return false;
  }

  const qint64 oldInPoint = layer->inPoint().framePosition();
  const qint64 oldOutPoint = layer->outPoint().framePosition();
  const qint64 oldDuration = std::max<qint64>(1, oldOutPoint - oldInPoint);
  // newInPoint は [oldInPoint, oldOutPoint-1] にクランプする。
  const qint64 newInPoint =
      std::max<qint64>(oldInPoint,
                       std::min<qint64>(oldOutPoint - 1, currentFrame));
  // inPoint を後ろに詰めた分だけ、後続を前に詰める（負方向）。
  const qint64 rippleDelta = -(newInPoint - oldInPoint);

  const auto rippleLayers =
      collectRippleLaterLayers(composition, layer->id(), oldInPoint);

  // target 側: inPoint を詰める。keyframe も追従させる（preserveExistingDuration=true
  // で inPoint を動かすと、内部で keyframe shift が走る）。ただし duration は保持
  // されてしまうため、その直後に outPoint を元位置に戻して duration を縮める。
  // ただし applyTimelineLayerRangeEdit は1回の呼び出しで in/out 両方を設定するため、
  // ここでは専用に in/out/startTime/keyframe を設定する。
  const qint64 newOutPoint = oldOutPoint; // outPoint は動かさない（前詰め）
  const qint64 inPointDelta = newInPoint - oldInPoint;

  layer->setInPoint(FramePosition(newInPoint));
  layer->setOutPoint(FramePosition(newOutPoint));
  if (inPointDelta != 0) {
    const qint64 oldStartTime = layer->startTime().framePosition();
    layer->setStartTime(FramePosition(std::max<qint64>(0, oldStartTime + inPointDelta)));
    // target の keyframe も同じ delta で追従させる。
    shiftAnimatableLayerKeyframes(composition, layer, inPointDelta);
    layer->changed();
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{compositionId.toString(), layer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }

  if (rippleDelta == 0 || rippleLayers.isEmpty()) {
    return true;
  }

  for (const auto &rippleLayer : rippleLayers) {
    if (!rippleLayer) {
      continue;
    }

    const qint64 followerOldIn = rippleLayer->inPoint().framePosition();
    const qint64 followerOldOut = rippleLayer->outPoint().framePosition();
    const qint64 followerOldStart = rippleLayer->startTime().framePosition();
    const qint64 followerNewIn =
        std::max<qint64>(0, followerOldIn + rippleDelta);
    const qint64 actualDelta = followerNewIn - followerOldIn;
    if (actualDelta == 0) {
      continue;
    }

    rippleLayer->setInPoint(FramePosition(followerNewIn));
    rippleLayer->setOutPoint(FramePosition(
        std::max<qint64>(followerNewIn + 1, followerOldOut + actualDelta)));
    rippleLayer->setStartTime(
        FramePosition(std::max<qint64>(0, followerOldStart + actualDelta)));
    shiftAnimatableLayerKeyframes(composition, rippleLayer, actualDelta);
    rippleLayer->changed();
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{compositionId.toString(), rippleLayer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }

  return true;
}

// Phase 2: Ripple Delete (Close Gap)
// target レイヤーを 0 幅に潰して実質削除状態にし、後続レイヤーを duration 分だけ前に詰める。
// レイヤー自体は完全削除せず in/out を同一フレームに潰すだけなので、Undo は
// snapshot 復元（in/out/startTime/keyframe）で安全に完結する。完全削除が必要な場合は
// 既存の「Delete Layer」（removeLayerFromComposition）を使う。
bool applyTimelineRippleDelete(const CompositionID &compositionId,
                               const QString &layerIdText) {
  if (layerIdText.trimmed().isEmpty()) {
    return false;
  }

  const auto composition = lookupTimelineComposition(compositionId);
  if (!composition) {
    return false;
  }

  const auto layer = composition->layerById(LayerID(layerIdText));
  if (!layer) {
    return false;
  }

  const qint64 oldInPoint = layer->inPoint().framePosition();
  const qint64 oldOutPoint = layer->outPoint().framePosition();
  const qint64 oldDuration = std::max<qint64>(1, oldOutPoint - oldInPoint);
  const qint64 rippleDelta = -oldDuration; // 後続を前に詰める

  const auto rippleLayers =
      collectRippleLaterLayers(composition, layer->id(), oldInPoint);

  // target を 0 幅に潰す（in/out を oldInPoint に一致させる）。
  layer->setInPoint(FramePosition(oldInPoint));
  layer->setOutPoint(FramePosition(oldInPoint + 1));
  layer->changed();
  ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
      LayerChangedEvent{compositionId.toString(), layer->id().toString(),
                        LayerChangedEvent::ChangeType::Modified});

  if (rippleDelta == 0 || rippleLayers.isEmpty()) {
    return true;
  }

  for (const auto &rippleLayer : rippleLayers) {
    if (!rippleLayer) {
      continue;
    }

    const qint64 followerOldIn = rippleLayer->inPoint().framePosition();
    const qint64 followerOldOut = rippleLayer->outPoint().framePosition();
    const qint64 followerOldStart = rippleLayer->startTime().framePosition();
    const qint64 followerNewIn =
        std::max<qint64>(0, followerOldIn + rippleDelta);
    const qint64 actualDelta = followerNewIn - followerOldIn;
    if (actualDelta == 0) {
      continue;
    }

    rippleLayer->setInPoint(FramePosition(followerNewIn));
    rippleLayer->setOutPoint(FramePosition(
        std::max<qint64>(followerNewIn + 1, followerOldOut + actualDelta)));
    rippleLayer->setStartTime(
        FramePosition(std::max<qint64>(0, followerOldStart + actualDelta)));
    shiftAnimatableLayerKeyframes(composition, rippleLayer, actualDelta);
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

  void undo() override {
    const auto composition = lookupTimelineComposition(compositionId_);
    if (!composition) {
      return;
    }

    restoreTimelineLayerStateSnapshots(composition, beforeSnapshots_);
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  void redo() override {
    if (applyTimelineRippleTrimOut(compositionId_, layerId_.toString(),
                                   currentFrame_)) {
      if (auto *mgr = UndoManager::instance()) {
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

// Phase 2: Ripple Trim In の Undo コマンド。
// 構造は RippleTrimOutCommand と同じ。target + followers を1コマンドに束ねる。
class RippleTrimInCommand final : public UndoCommand {
public:
  RippleTrimInCommand(CompositionID compositionId, LayerID layerId,
                      qint64 currentFrame,
                      QVector<TimelineLayerStateSnapshot> beforeSnapshots)
      : compositionId_(std::move(compositionId)),
        layerId_(std::move(layerId)), currentFrame_(currentFrame),
        beforeSnapshots_(std::move(beforeSnapshots)) {}

  void undo() override {
    const auto composition = lookupTimelineComposition(compositionId_);
    if (!composition) {
      return;
    }
    restoreTimelineLayerStateSnapshots(composition, beforeSnapshots_);
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  void redo() override {
    if (applyTimelineRippleTrimIn(compositionId_, layerId_.toString(),
                                  currentFrame_)) {
      if (auto *mgr = UndoManager::instance()) {
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

// Phase 2: Ripple Delete (Close Gap) の Undo コマンド。
// target を 0 幅に潰す方式なので、snapshot 復元で target も復元される。
class RippleDeleteCommand final : public UndoCommand {
public:
  RippleDeleteCommand(CompositionID compositionId, LayerID layerId,
                      QVector<TimelineLayerStateSnapshot> beforeSnapshots)
      : compositionId_(std::move(compositionId)),
        layerId_(std::move(layerId)),
        beforeSnapshots_(std::move(beforeSnapshots)) {}

  void undo() override {
    const auto composition = lookupTimelineComposition(compositionId_);
    if (!composition) {
      return;
    }
    restoreTimelineLayerStateSnapshots(composition, beforeSnapshots_);
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  void redo() override {
    if (applyTimelineRippleDelete(compositionId_, layerId_.toString())) {
      if (auto *mgr = UndoManager::instance()) {
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

QVector<int> selectedMarkerIndices(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>
        &markers) {
  QVector<int> indices;
  for (int i = 0; i < markers.size(); ++i) {
    if (markers[i].selected) {
      indices.push_back(i);
    }
  }
  return indices;
}

void applyMarkerSelectionFlags(
    QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    const QSet<QString> &selectedKeys) {
  for (auto &marker : markers) {
    const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
    marker.selected = selectedKeys.contains(
        keyframeSelectionKey(marker.layerId, marker.propertyPath, frame));
  }
}

QSet<QString> markerSelectionKeys(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>
        &markers) {
  QSet<QString> keys;
  keys.reserve(markers.size());
  for (const auto &marker : markers) {
    const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
    keys.insert(keyframeSelectionKey(marker.layerId, marker.propertyPath, frame));
  }
  return keys;
}

bool reconcileMarkerSelection(
    QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    QSet<QString> &selectedKeys) {
  const QSet<QString> visibleKeys = markerSelectionKeys(markers);
  QSet<QString> nextSelection;
  nextSelection.reserve(selectedKeys.size());
  for (const auto &key : selectedKeys) {
    if (visibleKeys.contains(key)) {
      nextSelection.insert(key);
    }
  }
  const bool changed = nextSelection != selectedKeys;
  selectedKeys = std::move(nextSelection);
  applyMarkerSelectionFlags(markers, selectedKeys);
  return changed;
}

bool sameTrackClipVisual(
    const ArtifactTimelineTrackPainterView::TrackClipVisual &lhs,
    const ArtifactTimelineTrackPainterView::TrackClipVisual &rhs) {
  return lhs.clipId == rhs.clipId && lhs.layerId == rhs.layerId &&
         lhs.trackIndex == rhs.trackIndex &&
         std::abs(lhs.startFrame - rhs.startFrame) < 0.0001 &&
         std::abs(lhs.durationFrame - rhs.durationFrame) < 0.0001 &&
         std::abs(lhs.trimMinStartFrame - rhs.trimMinStartFrame) < 0.0001 &&
         std::abs(lhs.trimMaxEndFrame - rhs.trimMaxEndFrame) < 0.0001 &&
         lhs.hasTrimSourceRange == rhs.hasTrimSourceRange &&
         lhs.title == rhs.title && lhs.fillColor == rhs.fillColor &&
         lhs.kind == rhs.kind &&
         lhs.selected == rhs.selected &&
         lhs.waveformPeaks == rhs.waveformPeaks &&
         lhs.waveformRms == rhs.waveformRms;
}

bool sameKeyframeMarkerVisual(
    const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual &lhs,
    const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual &rhs) {
  return lhs.layerId == rhs.layerId && lhs.propertyPath == rhs.propertyPath &&
         lhs.trackIndex == rhs.trackIndex &&
         std::abs(lhs.frame - rhs.frame) < 0.0001 &&
         lhs.laneIndex == rhs.laneIndex && lhs.laneCount == rhs.laneCount &&
         lhs.selectedLayer == rhs.selectedLayer &&
         lhs.selected == rhs.selected && lhs.eased == rhs.eased &&
         lhs.incomingEased == rhs.incomingEased &&
         lhs.outgoingEased == rhs.outgoingEased &&
         lhs.incomingBezier == rhs.incomingBezier &&
         lhs.outgoingBezier == rhs.outgoingBezier &&
         lhs.bezier == rhs.bezier &&
         lhs.roving == rhs.roving &&
         lhs.interpolation == rhs.interpolation &&
         lhs.anchor == rhs.anchor &&
         lhs.color == rhs.color && lhs.label == rhs.label &&
         lhs.value == rhs.value;
}

std::shared_ptr<ArtifactCore::AbstractProperty> findLayerPropertyByPath(
    const ArtifactAbstractLayerPtr &layer, const QString &propertyPath) {
  if (!layer || propertyPath.trimmed().isEmpty()) {
    return {};
  }

  const auto groups = layer->getLayerPropertyGroups();
  for (const auto &group : groups) {
    for (const auto &property : group.sortedProperties()) {
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

struct SelectedKeyframeRecord {
  LayerID layerId;
  QString propertyPath;
  qint64 originalFrame = 0;
  qint64 targetFrame = 0;
  QVariant value;
  ArtifactCore::InterpolationType interpolation =
      ArtifactCore::InterpolationType::Linear;
  ArtifactCore::KeyFrame::ColorLabel colorLabel =
      ArtifactCore::KeyFrame::ColorLabel::None;
  ArtifactCore::KeyFrame::Anchor anchor =
      ArtifactCore::KeyFrame::Anchor::Absolute;
  float cp1_x = 0.42f;
  float cp1_y = 0.0f;
  float cp2_x = 0.58f;
  float cp2_y = 1.0f;
  bool roving = false;
};

QString selectedKeyframeRecordGroupKey(const SelectedKeyframeRecord &record) {
  return QStringLiteral("%1|%2")
      .arg(record.layerId.toString(), record.propertyPath);
}

QVector<SelectedKeyframeRecord> collectSelectedKeyframeRecords(
    const ArtifactCompositionPtr &composition,
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers) {
  QVector<SelectedKeyframeRecord> records;
  if (!composition || markers.isEmpty()) {
    return records;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  QSet<QString> seen;
  for (const auto &marker : markers) {
    const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
    const QString dedupeKey =
        QStringLiteral("%1|%2|%3")
            .arg(marker.layerId.toString(), marker.propertyPath,
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
                                 [&time](const ArtifactCore::KeyFrame &keyframe) {
                                   return keyframe.time == time;
                                 });
    if (it == keyframes.cend()) {
      continue;
    }

    records.push_back(SelectedKeyframeRecord{
        marker.layerId,
        marker.propertyPath,
        frame,
        frame,
        it->value.isValid() ? it->value : property->getValue(),
        it->interpolation,
        it->colorLabel,
        it->anchor,
        it->cp1_x,
        it->cp1_y,
        it->cp2_x,
        it->cp2_y,
        it->roving,
    });
  }

  std::sort(records.begin(), records.end(), [](const SelectedKeyframeRecord &lhs,
                                               const SelectedKeyframeRecord &rhs) {
    if (lhs.layerId != rhs.layerId) {
      return lhs.layerId.toString() < rhs.layerId.toString();
    }
    if (lhs.propertyPath != rhs.propertyPath) {
      return lhs.propertyPath < rhs.propertyPath;
    }
    if (lhs.originalFrame != rhs.originalFrame) {
      return lhs.originalFrame < rhs.originalFrame;
    }
    return lhs.targetFrame < rhs.targetFrame;
  });

  return records;
}

QVariant effectiveKeyframeValue(const ArtifactCore::AbstractProperty *property,
                                const ArtifactCore::KeyFrame &keyframe) {
  if (keyframe.value.isValid()) {
    return keyframe.value;
  }
  return property ? property->getValue() : QVariant();
}

bool approximatelyEqualValue(const QVariant &lhs, const QVariant &rhs,
                             const ArtifactCore::PropertyType type) {
  switch (type) {
  case ArtifactCore::PropertyType::Float: {
    const double a = lhs.toDouble();
    const double b = rhs.toDouble();
    const double scale = std::max({1.0, std::abs(a), std::abs(b)});
    return std::abs(a - b) <= 0.0001 * scale;
  }
  case ArtifactCore::PropertyType::Integer:
    return lhs.toInt() == rhs.toInt();
  case ArtifactCore::PropertyType::Boolean:
    return lhs.toBool() == rhs.toBool();
  case ArtifactCore::PropertyType::String:
    return lhs.toString() == rhs.toString();
  case ArtifactCore::PropertyType::Color: {
    const QColor a = lhs.value<QColor>();
    const QColor b = rhs.value<QColor>();
    if (!a.isValid() || !b.isValid()) {
      return false;
    }
    return std::abs(a.redF() - b.redF()) <= 0.001f &&
           std::abs(a.greenF() - b.greenF()) <= 0.001f &&
           std::abs(a.blueF() - b.blueF()) <= 0.001f &&
           std::abs(a.alphaF() - b.alphaF()) <= 0.001f;
  }
  default:
    return lhs == rhs;
  }
}

bool cleanNearDuplicateKeyframesForProperty(
    const std::shared_ptr<ArtifactCore::AbstractProperty> &property,
    int *outRemovedCount) {
  if (!property) {
    return false;
  }

  const auto keyframes = property->getKeyFrames();
  if (keyframes.size() < 3) {
    return false;
  }

  QVector<ArtifactCore::KeyFrame> cleaned;
  cleaned.reserve(keyframes.size());
  cleaned.push_back(keyframes.front());

  int removedCount = 0;
  const auto propertyType = property->getType();
  for (int i = 1; i + 1 < keyframes.size(); ++i) {
    const auto &previous = cleaned.back();
    const auto &current = keyframes[i];
    const auto &next = keyframes[i + 1];
    const QVariant previousValue =
        effectiveKeyframeValue(property.get(), previous);
    const QVariant currentValue =
        effectiveKeyframeValue(property.get(), current);
    const QVariant nextValue = effectiveKeyframeValue(property.get(), next);

    if (approximatelyEqualValue(previousValue, currentValue, propertyType) &&
        approximatelyEqualValue(currentValue, nextValue, propertyType)) {
      ++removedCount;
      continue;
    }

    cleaned.push_back(current);
  }

  cleaned.push_back(keyframes.back());
  if (removedCount == 0) {
    return false;
  }

  property->clearKeyFrames();
  for (const auto &keyframe : cleaned) {
    property->addKeyFrame(keyframe.time,
                          keyframe.value.isValid() ? keyframe.value
                                                   : property->getValue(),
                          keyframe.interpolation, keyframe.cp1_x,
                          keyframe.cp1_y, keyframe.cp2_x, keyframe.cp2_y,
                          keyframe.roving);
    property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
    property->setKeyFrameColorLabelAt(keyframe.time, keyframe.colorLabel);
  }

  if (outRemovedCount) {
    *outRemovedCount += removedCount;
  }
  return true;
}

bool cleanNearDuplicateKeyframes(
    const ArtifactCompositionPtr &composition,
    const QVector<KeyframePropertyRef> &refs,
    int *outRemovedCount) {
  if (!composition || refs.isEmpty()) {
    return false;
  }

  if (outRemovedCount) {
    *outRemovedCount = 0;
  }

  QSet<QString> changedLayers;
  bool changed = false;
  QSet<QString> seen;
  for (const auto &ref : refs) {
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
    if (!property || !property->isAnimatable()) {
      continue;
    }

    int removedCount = 0;
    if (!cleanNearDuplicateKeyframesForProperty(property, &removedCount)) {
      continue;
    }

    layer->changed();
    changedLayers.insert(layer->id().toString());
    changed = true;
    if (outRemovedCount) {
      *outRemovedCount += removedCount;
    }
  }

  for (const auto &layerId : changedLayers) {
    if (const auto layer = composition->layerById(LayerID(layerId))) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{composition->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
  }

  return changed;
}

bool applyEvenKeyframeDistribution(
    const ArtifactCompositionPtr &composition,
    QVector<SelectedKeyframeRecord> *records,
    QSet<QString> *outSelectionKeys,
    int *outMovedCount) {
  if (!composition || !records || records->isEmpty()) {
    return false;
  }

  if (outSelectionKeys) {
    outSelectionKeys->clear();
  }
  if (outMovedCount) {
    *outMovedCount = 0;
  }

  std::map<QString, QVector<int>> groupedIndices;
  for (int i = 0; i < records->size(); ++i) {
    groupedIndices[selectedKeyframeRecordGroupKey(records->at(i))].push_back(i);
  }

  const int fpsScale = std::max(
      1, static_cast<int>(std::llround(composition->frameRate().framerate())));
  QSet<QString> changedLayers;
  bool changed = false;

  for (auto &entry : groupedIndices) {
    auto &indices = entry.second;
    if (indices.size() < 2) {
      continue;
    }

    std::sort(indices.begin(), indices.end(),
              [&](const int lhs, const int rhs) {
                return records->at(lhs).originalFrame <
                       records->at(rhs).originalFrame;
              });

    const auto firstFrame = records->at(indices.front()).originalFrame;
    const auto lastFrame = records->at(indices.back()).originalFrame;
    const qint64 span = std::max<qint64>(0, lastFrame - firstFrame);
    for (int i = 0; i < indices.size(); ++i) {
      const qint64 targetFrame =
          (indices.size() == 1)
              ? firstFrame
              : firstFrame +
                    (span * static_cast<qint64>(i)) /
                        static_cast<qint64>(indices.size() - 1);
      (*records)[indices[i]].targetFrame = targetFrame;
    }
  }

  std::map<QString, QVector<SelectedKeyframeRecord>> groupedRecords;
  for (const auto &record : *records) {
    groupedRecords[selectedKeyframeRecordGroupKey(record)].push_back(record);
  }

  for (auto &entry : groupedRecords) {
    auto &groupRecords = entry.second;
    if (groupRecords.isEmpty()) {
      continue;
    }

    std::sort(groupRecords.begin(), groupRecords.end(),
              [](const SelectedKeyframeRecord &lhs,
                 const SelectedKeyframeRecord &rhs) {
                return lhs.originalFrame < rhs.originalFrame;
              });

    const auto layer = composition->layerById(groupRecords.front().layerId);
    if (!layer) {
      continue;
    }
    const auto property =
        findLayerPropertyByPath(layer, groupRecords.front().propertyPath);
    if (!property || !property->isAnimatable()) {
      continue;
    }

    const auto existingKeyframes = property->getKeyFrames();
    QSet<qint64> selectedOriginalFrames;
    for (const auto &record : groupRecords) {
      selectedOriginalFrames.insert(record.originalFrame);
    }

    QVector<ArtifactCore::KeyFrame> combined;
    combined.reserve(existingKeyframes.size());
    for (const auto &keyframe : existingKeyframes) {
      const qint64 oldFrame = keyframe.time.rescaledTo(fpsScale);
      if (selectedOriginalFrames.contains(oldFrame)) {
        continue;
      }
      combined.push_back(keyframe);
    }

    for (const auto &record : groupRecords) {
      ArtifactCore::KeyFrame keyframe;
      keyframe.time = RationalTime(record.targetFrame, fpsScale);
      keyframe.value = record.value.isValid() ? record.value : property->getValue();
      keyframe.interpolation = record.interpolation;
      keyframe.colorLabel = record.colorLabel;
      keyframe.anchor = record.anchor;
      keyframe.cp1_x = record.cp1_x;
      keyframe.cp1_y = record.cp1_y;
      keyframe.cp2_x = record.cp2_x;
      keyframe.cp2_y = record.cp2_y;
      keyframe.roving = record.roving;
      combined.push_back(keyframe);
      if (outSelectionKeys) {
        outSelectionKeys->insert(keyframeSelectionKey(
            record.layerId, record.propertyPath, record.targetFrame));
      }
    }

    std::sort(combined.begin(), combined.end(),
              [fpsScale](const ArtifactCore::KeyFrame &lhs,
                         const ArtifactCore::KeyFrame &rhs) {
                const qint64 lhsFrame = lhs.time.rescaledTo(fpsScale);
                const qint64 rhsFrame = rhs.time.rescaledTo(fpsScale);
                if (lhsFrame != rhsFrame) {
                  return lhsFrame < rhsFrame;
                }
                return lhs.interpolation < rhs.interpolation;
              });

    property->clearKeyFrames();
    for (const auto &keyframe : combined) {
      property->addKeyFrame(
          keyframe.time,
          keyframe.value.isValid() ? keyframe.value : property->getValue(),
          keyframe.interpolation, keyframe.cp1_x, keyframe.cp1_y,
          keyframe.cp2_x, keyframe.cp2_y, keyframe.roving);
      property->setKeyFrameColorLabelAt(keyframe.time, keyframe.colorLabel);
      property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
    }

    layer->changed();
    changedLayers.insert(layer->id().toString());
    changed = true;
  }

  for (const auto &layerId : changedLayers) {
    if (const auto layer = composition->layerById(LayerID(layerId))) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{composition->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
  }

  if (outMovedCount) {
    *outMovedCount = static_cast<int>(outSelectionKeys ? outSelectionKeys->size()
                                                      : records->size());
  }

  return changed;
}

bool repeatSelectedKeyframeRecords(
    const ArtifactCompositionPtr &composition,
    const QVector<SelectedKeyframeRecord> &records,
    const qint64 baseFrame,
    const int repeatCount,
    QSet<QString> *outSelectionKeys,
    int *outMergedExistingKeyframeCount) {
  if (!composition || records.isEmpty() || repeatCount <= 0) {
    return false;
  }

  if (outSelectionKeys) {
    outSelectionKeys->clear();
  }
  if (outMergedExistingKeyframeCount) {
    *outMergedExistingKeyframeCount = 0;
  }

  std::map<QString, QVector<SelectedKeyframeRecord>> groupedRecords;
  for (const auto &record : records) {
    groupedRecords[selectedKeyframeRecordGroupKey(record)].push_back(record);
  }

  const int fpsScale = std::max(
      1, static_cast<int>(std::llround(composition->frameRate().framerate())));
  QSet<QString> changedLayers;
  bool changed = false;

  for (auto &entry : groupedRecords) {
    auto &groupRecords = entry.second;
    if (groupRecords.isEmpty()) {
      continue;
    }

    std::sort(groupRecords.begin(), groupRecords.end(),
              [](const SelectedKeyframeRecord &lhs,
                 const SelectedKeyframeRecord &rhs) {
                return lhs.originalFrame < rhs.originalFrame;
              });

    const auto layer = composition->layerById(groupRecords.front().layerId);
    if (!layer) {
      continue;
    }
    const auto property =
        findLayerPropertyByPath(layer, groupRecords.front().propertyPath);
    if (!property || !property->isAnimatable()) {
      continue;
    }

    const qint64 firstFrame = groupRecords.front().originalFrame;
    const qint64 lastFrame = groupRecords.back().originalFrame;
    const qint64 cycleSpan = std::max<qint64>(1, lastFrame - firstFrame + 1);

    for (int repeatIndex = 0; repeatIndex < repeatCount; ++repeatIndex) {
      const qint64 repeatBase =
          baseFrame + static_cast<qint64>(repeatIndex) * cycleSpan;
      for (const auto &record : groupRecords) {
        const qint64 newFrame = repeatBase + (record.originalFrame - firstFrame);
        ArtifactCore::KeyFrame keyframe;
        keyframe.time = RationalTime(newFrame, fpsScale);
        keyframe.value = record.value.isValid() ? record.value : property->getValue();
        keyframe.interpolation = record.interpolation;
        keyframe.colorLabel = record.colorLabel;
        keyframe.anchor = record.anchor;
        keyframe.cp1_x = record.cp1_x;
        keyframe.cp1_y = record.cp1_y;
        keyframe.cp2_x = record.cp2_x;
        keyframe.cp2_y = record.cp2_y;
        keyframe.roving = record.roving;
        if (property->hasKeyFrameAt(keyframe.time)) {
          if (outMergedExistingKeyframeCount) {
            ++(*outMergedExistingKeyframeCount);
          }
        }
        property->addKeyFrame(keyframe.time, keyframe.value, keyframe.interpolation,
                              keyframe.cp1_x, keyframe.cp1_y, keyframe.cp2_x,
                              keyframe.cp2_y, keyframe.roving);
        property->setKeyFrameColorLabelAt(keyframe.time, keyframe.colorLabel);
        property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
        if (outSelectionKeys) {
          outSelectionKeys->insert(keyframeSelectionKey(
              record.layerId, record.propertyPath, newFrame));
        }
      }
    }

    layer->changed();
    changedLayers.insert(layer->id().toString());
    changed = true;
  }

  for (const auto &layerId : changedLayers) {
    if (const auto layer = composition->layerById(LayerID(layerId))) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{composition->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
  }

  return changed;
}

enum class KeyframeRangeTransformKind {
  Stretch,
  Reverse,
  Mirror,
  Normalize,
  ScaleValues,
  OffsetValues
};

struct KeyframeRangeTransformOptions {
  KeyframeRangeTransformKind kind = KeyframeRangeTransformKind::Stretch;
  qint64 targetDuration = 0;
  double scale = 1.0;
  double valueScale = 1.0;
  double valueOffset = 0.0;
};

bool applySelectedKeyframeRangeTransform(
    const ArtifactCompositionPtr &composition,
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    const KeyframeRangeTransformOptions &options,
    QSet<QString> *outSelectionKeys,
    int *outAffectedCount) {
  if (!composition || markers.isEmpty()) {
    return false;
  }

  if (outSelectionKeys) {
    outSelectionKeys->clear();
  }
  if (outAffectedCount) {
    *outAffectedCount = 0;
  }

  auto records = collectSelectedKeyframeRecords(composition, markers);
  if (records.isEmpty()) {
    return false;
  }

  std::map<QString, QVector<SelectedKeyframeRecord>> groupedRecords;
  for (const auto &record : records) {
    groupedRecords[selectedKeyframeRecordGroupKey(record)].push_back(record);
  }

  const int fpsScale = std::max(
      1, static_cast<int>(std::llround(composition->frameRate().framerate())));
  QSet<QString> changedLayers;
  bool changed = false;
  int affected = 0;

  for (auto &entry : groupedRecords) {
    auto &groupRecords = entry.second;
    if (groupRecords.isEmpty()) {
      continue;
    }

    std::sort(groupRecords.begin(), groupRecords.end(),
              [](const SelectedKeyframeRecord &lhs,
                 const SelectedKeyframeRecord &rhs) {
                return lhs.originalFrame < rhs.originalFrame;
              });

    const auto layer = composition->layerById(groupRecords.front().layerId);
    if (!layer) {
      continue;
    }
    const auto property =
        findLayerPropertyByPath(layer, groupRecords.front().propertyPath);
    if (!property || !property->isAnimatable()) {
      continue;
    }

    const qint64 firstFrame = groupRecords.front().originalFrame;
    const qint64 lastFrame = groupRecords.back().originalFrame;
    const qint64 span = std::max<qint64>(1, lastFrame - firstFrame);
    const qint64 targetDuration =
        options.kind == KeyframeRangeTransformKind::Normalize
            ? std::max<qint64>(1, options.targetDuration)
            : span;
    QVector<ArtifactCore::KeyFrame> transformed;
    transformed.reserve(groupRecords.size());

    for (int i = 0; i < groupRecords.size(); ++i) {
      const auto &record = groupRecords[i];
      qint64 newFrame = record.originalFrame;
      switch (options.kind) {
      case KeyframeRangeTransformKind::Stretch: {
        const double normalized = static_cast<double>(record.originalFrame - firstFrame) /
                                  static_cast<double>(std::max<qint64>(1, span));
        newFrame = firstFrame + static_cast<qint64>(std::llround(normalized * span * options.scale));
        break;
      }
      case KeyframeRangeTransformKind::Reverse:
        newFrame = firstFrame + (lastFrame - record.originalFrame);
        break;
      case KeyframeRangeTransformKind::Mirror: {
        const qint64 center = firstFrame + span / 2;
        newFrame = center - (record.originalFrame - center);
        break;
      }
      case KeyframeRangeTransformKind::Normalize: {
        const double ratio = groupRecords.size() == 1
                                 ? 0.0
                                 : static_cast<double>(i) /
                                       static_cast<double>(groupRecords.size() - 1);
        newFrame = firstFrame + static_cast<qint64>(std::llround(ratio * targetDuration));
        break;
      }
      case KeyframeRangeTransformKind::ScaleValues:
      case KeyframeRangeTransformKind::OffsetValues:
        break;
      }

      ArtifactCore::KeyFrame keyframe;
      keyframe.time = RationalTime(newFrame, fpsScale);
      keyframe.value = record.value.isValid() ? record.value : property->getValue();
      if (options.kind == KeyframeRangeTransformKind::ScaleValues ||
          options.kind == KeyframeRangeTransformKind::OffsetValues) {
        if (keyframe.value.canConvert<double>()) {
          const double v = keyframe.value.toDouble();
          const double scaled = options.kind == KeyframeRangeTransformKind::ScaleValues
                                    ? v * options.valueScale
                                    : v + options.valueOffset;
          keyframe.value = scaled;
        }
      }
      keyframe.interpolation = record.interpolation;
      keyframe.colorLabel = record.colorLabel;
      keyframe.anchor = record.anchor;
      keyframe.cp1_x = record.cp1_x;
      keyframe.cp1_y = record.cp1_y;
      keyframe.cp2_x = record.cp2_x;
      keyframe.cp2_y = record.cp2_y;
      keyframe.roving = record.roving;
      transformed.push_back(keyframe);
    }

    property->clearKeyFrames();
    for (const auto &keyframe : transformed) {
      property->addKeyFrame(keyframe.time,
                            keyframe.value.isValid() ? keyframe.value
                                                     : property->getValue(),
                            keyframe.interpolation, keyframe.cp1_x,
                            keyframe.cp1_y, keyframe.cp2_x, keyframe.cp2_y,
                            keyframe.roving);
      property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
      property->setKeyFrameColorLabelAt(keyframe.time, keyframe.colorLabel);
      if (outSelectionKeys) {
        outSelectionKeys->insert(keyframeSelectionKey(
            groupRecords.front().layerId, groupRecords.front().propertyPath,
            keyframe.time.rescaledTo(fpsScale)));
      }
    }
    layer->changed();
    changedLayers.insert(layer->id().toString());
    changed = true;
    affected += transformed.size();
  }

  for (const auto &layerId : changedLayers) {
    if (const auto layer = composition->layerById(LayerID(layerId))) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{composition->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
  }

  if (outAffectedCount) {
    *outAffectedCount = affected;
  }
  return changed;
}

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
  void apply(const bool useAfter) {
    QSet<QString> changedLayerIds;
    for (const auto &record : records_) {
      auto layer = record.layer.lock();
      if (!layer) {
        continue;
      }
      const auto property = findLayerPropertyByPath(layer, record.propertyPath);
      if (!property) {
        continue;
      }
      const auto &keyframe = useAfter ? record.after : record.before;
      property->addKeyFrame(keyframe.time,
                            keyframe.value.isValid() ? keyframe.value : property->getValue(),
                            keyframe.interpolation,
                            keyframe.cp1_x,
                            keyframe.cp1_y,
                            keyframe.cp2_x,
                            keyframe.cp2_y,
                            keyframe.roving);
      property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
      property->setKeyFrameColorLabelAt(keyframe.time, keyframe.colorLabel);
      layer->changed();
      changedLayerIds.insert(layer->id().toString());
    }

    for (const auto &record : records_) {
      auto layer = record.layer.lock();
      if (!layer) {
        continue;
      }
      const QString layerKey = layer->id().toString();
      if (!changedLayerIds.contains(layerKey)) {
        continue;
      }
      if (auto *comp = static_cast<ArtifactAbstractComposition *>(layer->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
      }
    }

    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  QVector<InterpolationChangeRecord> records_;
};

int applyInterpolationToSelectedKeyframesImpl(
    const ArtifactCompositionPtr &composition,
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    const ArtifactCore::InterpolationType interpolationType) {
  if (!composition || markers.isEmpty()) {
    return 0;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  QSet<QString> seen;
  QVector<InterpolationChangeRecord> records;

  for (const auto &marker : markers) {
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
                                 [&time](const ArtifactCore::KeyFrame &keyframe) {
                                   return keyframe.time == time;
                                 });
    if (it == keyframes.cend()) {
      continue;
    }

    const ArtifactCore::KeyFrame before = *it;
    ArtifactCore::KeyFrame after = before;
    after.interpolation = interpolationType;
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
  if (auto *mgr = UndoManager::instance()) {
    mgr->push(std::make_unique<ApplyInterpolationCommand>(std::move(records)));
    return appliedCount;
  }
  return 0;
}

struct RovingChangeRecord {
  ArtifactAbstractLayerWeak layer;
  QString propertyPath;
  RationalTime time;
  ArtifactCore::KeyFrame before;
  ArtifactCore::KeyFrame after;
};

class ApplyRovingCommand final : public UndoCommand {
public:
  explicit ApplyRovingCommand(QVector<RovingChangeRecord> records)
      : records_(std::move(records)) {}

  void undo() override { apply(false); }
  void redo() override { apply(true); }
  QString label() const override { return QStringLiteral("Apply Roving"); }

private:
  void apply(const bool useAfter) {
    QSet<QString> changedLayerIds;
    for (const auto &record : records_) {
      auto layer = record.layer.lock();
      if (!layer) {
        continue;
      }
      const auto property = findLayerPropertyByPath(layer, record.propertyPath);
      if (!property) {
        continue;
      }
      const auto &keyframe = useAfter ? record.after : record.before;
      property->addKeyFrame(keyframe.time,
                            keyframe.value.isValid() ? keyframe.value : property->getValue(),
                            keyframe.interpolation,
                            keyframe.cp1_x,
                            keyframe.cp1_y,
                            keyframe.cp2_x,
                            keyframe.cp2_y,
                            keyframe.roving);
      property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
      property->setKeyFrameColorLabelAt(keyframe.time, keyframe.colorLabel);
      layer->changed();
      changedLayerIds.insert(layer->id().toString());
    }

    for (const auto &record : records_) {
      auto layer = record.layer.lock();
      if (!layer) {
        continue;
      }
      const QString layerKey = layer->id().toString();
      if (!changedLayerIds.contains(layerKey)) {
        continue;
      }
      if (auto *comp = static_cast<ArtifactAbstractComposition *>(layer->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
      }
    }

    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  QVector<RovingChangeRecord> records_;
};

int applyRovingToSelectedKeyframesImpl(
    const ArtifactCompositionPtr &composition,
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    const bool roving) {
  if (!composition || markers.isEmpty()) {
    return 0;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  QSet<QString> seen;
  QVector<RovingChangeRecord> records;

  for (const auto &marker : markers) {
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
                                 [&time](const ArtifactCore::KeyFrame &keyframe) {
                                   return keyframe.time == time;
                                 });
    if (it == keyframes.cend()) {
      continue;
    }

    const ArtifactCore::KeyFrame before = *it;
    ArtifactCore::KeyFrame after = before;
    after.roving = roving;
    records.push_back(RovingChangeRecord{
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
  if (auto *mgr = UndoManager::instance()) {
    mgr->push(std::make_unique<ApplyRovingCommand>(std::move(records)));
    return appliedCount;
  }
  return 0;
}

bool sameTimelineRowDescriptor(const TimelineRowDescriptor &lhs,
                               const TimelineRowDescriptor &rhs) {
  return lhs.layerId == rhs.layerId && lhs.kind == rhs.kind &&
         lhs.label == rhs.label && lhs.propertyPath == rhs.propertyPath &&
         lhs.auxiliaryText == rhs.auxiliaryText &&
         lhs.auxiliaryTone == rhs.auxiliaryTone &&
         lhs.stateText == rhs.stateText &&
         lhs.stateTone == rhs.stateTone;
}

template <typename T>
bool sameVisualList(const QVector<T> &lhs, const QVector<T> &rhs,
                    bool (*equals)(const T &, const T &)) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (int i = 0; i < lhs.size(); ++i) {
    if (!equals(lhs[i], rhs[i])) {
      return false;
    }
  }
  return true;
}

QString formatClipTooltip(
    const ArtifactTimelineTrackPainterView::TrackClipVisual &clip) {
  const QString title = clip.title.isEmpty() ? clip.clipId : clip.title;
  const QString kindText = [&]() {
    switch (clip.kind) {
    case ArtifactTimelineTrackPainterView::TrackClipVisual::Kind::Audio:
      return QStringLiteral("Kind: Audio");
    case ArtifactTimelineTrackPainterView::TrackClipVisual::Kind::Video:
      return QStringLiteral("Kind: Video");
    case ArtifactTimelineTrackPainterView::TrackClipVisual::Kind::Generic:
    default:
      return QStringLiteral("Kind: Generic");
    }
  }();
  const QString startText = QStringLiteral("Start: F%1")
                                .arg(QString::number(clip.startFrame, 'f', 1));
  const QString endText =
      QStringLiteral("End: F%1")
          .arg(QString::number(clip.startFrame + clip.durationFrame, 'f', 1));
  const QString durationText =
      QStringLiteral("Duration: %1 frames")
          .arg(QString::number(clip.durationFrame, 'f', 1));
  const QString stateText = clip.selected ? QStringLiteral("State: Selected")
                                          : QStringLiteral("State: Idle");
  QStringList lines;
  lines << title << startText << endText << durationText << stateText << kindText;
  if (!clip.waveformPeaks.isEmpty()) {
    lines << waveformPreviewSummary(clip.waveformPeaks, clip.waveformRms);
  } else if (clip.kind == ArtifactTimelineTrackPainterView::TrackClipVisual::Kind::Audio) {
    lines << waveformPreviewSummary(clip.waveformPeaks, clip.waveformRms);
  }
  return lines.join(QStringLiteral("\n"));
}

bool markerAtCurrentFrame(
    const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual &marker,
    const double currentFrame);

QString keyframeAnchorLabel(ArtifactCore::KeyFrame::Anchor anchor);
QString keyframeInterpolationLabel(ArtifactCore::InterpolationType type);

QString formatMarkerTooltip(
    const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual &marker,
    const double currentFrame, const bool hovered, const bool nearestToCurrent) {
  const QString label =
      marker.label.isEmpty()
          ? ArtifactTimelineKeyframeModel::displayLabelForPropertyPath(
                marker.propertyPath)
          : marker.label;
  const QString frameText =
      QStringLiteral("Frame: F%1").arg(QString::number(marker.frame, 'f', 1));
  const QString pathText = QStringLiteral("Path: %1").arg(marker.propertyPath);
  const QString laneText = marker.laneCount > 1 ? QStringLiteral("Lane: %1/%2")
                                                      .arg(marker.laneIndex + 1)
                                                      .arg(marker.laneCount)
                                                : QStringLiteral("Lane: 1/1");
  const QString easingText = QStringLiteral("Incoming: %1 | Outgoing: %2")
                                 .arg(marker.incomingEased ? QStringLiteral("eased")
                                                           : QStringLiteral("linear"))
                                 .arg(marker.outgoingEased ? QStringLiteral("eased")
                                                           : QStringLiteral("linear"));
  const QString interpolationText =
      QStringLiteral("Interpolation: %1")
          .arg(keyframeInterpolationLabel(marker.interpolation));
  const QString anchorText =
      QStringLiteral("Anchor: %1").arg(keyframeAnchorLabel(marker.anchor));
  const QString rovingText =
      marker.roving ? QStringLiteral("Roving: On")
                    : QStringLiteral("Roving: Off");
  const QString colorText = marker.labelColor.isValid()
                                ? QStringLiteral("Label: %1").arg(marker.labelColor.name())
                                : QStringLiteral("Label: None");
  const QString selectionText =
      marker.selected
          ? QStringLiteral("Selection: Selected keyframe")
          : (marker.selectedLayer ? QStringLiteral("Selection: Selected layer")
                                  : QStringLiteral("Selection: Idle"));
  const QString relationText =
      markerAtCurrentFrame(marker, currentFrame)
          ? QStringLiteral("Relation: At current frame")
          : (nearestToCurrent ? QStringLiteral("Relation: Nearest to current")
                              : QStringLiteral("Relation: Off current"));
  const QString hoverText =
      hovered ? QStringLiteral("State: Hovered")
              : QStringLiteral("State: Visible");
  QString tooltip = label + QStringLiteral("\n") + frameText + QStringLiteral("\n") +
         pathText + QStringLiteral("\n") + laneText + QStringLiteral("\n") +
         easingText + QStringLiteral("\n") + interpolationText +
         QStringLiteral("\n") + anchorText + QStringLiteral("\n") +
         rovingText + QStringLiteral("\n") + colorText + QStringLiteral("\n") +
         selectionText +
         QStringLiteral("\n") + relationText + QStringLiteral("\n") +
         hoverText;
  if (marker.selected) {
    tooltip += QStringLiteral("\nShortcuts: Delete / Backspace remove, Ctrl+D duplicates here, Left / Right move, Shift+Left / Shift+Right move by 10 frames.");
  }
  return tooltip;
}

QString formatKeyframeDragTooltip(const int selectionCount, const double deltaFrames,
                                  const double targetFrame,
                                  const QString &snapLabel) {
  const QString countText =
      selectionCount == 1 ? QStringLiteral("1 keyframe")
                          : QStringLiteral("%1 keyframes").arg(selectionCount);
  QString tooltip = QStringLiteral("Moving %1\nDelta: %2 frames\nTarget: F%3")
      .arg(countText)
      .arg(QString::number(deltaFrames, 'f', 1))
      .arg(QString::number(targetFrame, 'f', 1));
  if (!snapLabel.isEmpty()) {
    tooltip += QStringLiteral("\nSnap: %1").arg(snapLabel);
  }
  return tooltip;
}

QString formatKeyframeCollisionLabel(const int count) {
  return count == 1
             ? QStringLiteral("collides with 1 existing keyframe")
             : QStringLiteral("collides with %1 existing keyframes").arg(count);
}

QString formatKeyframeNoun(const int count) {
  return count == 1 ? QStringLiteral("keyframe") : QStringLiteral("keyframes");
}

bool triggerTimelineShortcut(QWidget *source, const int key,
                             const Qt::KeyboardModifiers modifiers) {
  if (!source || !source->parentWidget()) {
    return false;
  }

  QKeyEvent press(QEvent::KeyPress, key, modifiers);
  QCoreApplication::sendEvent(source->parentWidget(), &press);
  return press.isAccepted();
}

QString formatFrameUnit(const qint64 count) {
  return count == 1 ? QStringLiteral("frame") : QStringLiteral("frames");
}

QColor keyframeColorLabelColor(const ArtifactCore::KeyFrame::ColorLabel label) {
  switch (label) {
  case ArtifactCore::KeyFrame::ColorLabel::Red:
    return QColor(236, 87, 87);
  case ArtifactCore::KeyFrame::ColorLabel::Blue:
    return QColor(89, 159, 255);
  case ArtifactCore::KeyFrame::ColorLabel::Yellow:
    return QColor(246, 207, 73);
  case ArtifactCore::KeyFrame::ColorLabel::Green:
    return QColor(89, 196, 130);
  case ArtifactCore::KeyFrame::ColorLabel::Purple:
    return QColor(177, 121, 255);
  case ArtifactCore::KeyFrame::ColorLabel::Gray:
    return QColor(156, 164, 174);
  case ArtifactCore::KeyFrame::ColorLabel::None:
  default:
    return QColor();
  }
}

QColor keyframeInterpolationColor(const ArtifactCore::InterpolationType type,
                                  const bool selectedLayer) {
  if (selectedLayer) {
    return QColor(255, 255, 255);
  }
  switch (type) {
  case ArtifactCore::InterpolationType::Constant:
    return QColor(236, 184, 74);
  case ArtifactCore::InterpolationType::Linear:
    return QColor(247, 204, 83);
  case ArtifactCore::InterpolationType::EaseIn:
    return QColor(101, 190, 255);
  case ArtifactCore::InterpolationType::EaseOut:
    return QColor(83, 217, 188);
  case ArtifactCore::InterpolationType::EaseInOut:
    return QColor(110, 214, 255);
  case ArtifactCore::InterpolationType::Bezier:
    return QColor(126, 176, 255);
  case ArtifactCore::InterpolationType::BackOut:
  case ArtifactCore::InterpolationType::BounceOut:
  case ArtifactCore::InterpolationType::ElasticOut:
    return QColor(255, 151, 101);
  case ArtifactCore::InterpolationType::Sine:
  case ArtifactCore::InterpolationType::Cubic:
  case ArtifactCore::InterpolationType::Exponential:
    return QColor(149, 222, 129);
  default:
    return QColor(247, 204, 83);
  }
}

QString keyframeAnchorLabel(const ArtifactCore::KeyFrame::Anchor anchor) {
  switch (anchor) {
  case ArtifactCore::KeyFrame::Anchor::LockToIn:
    return QStringLiteral("Lock to In");
  case ArtifactCore::KeyFrame::Anchor::LockToOut:
    return QStringLiteral("Lock to Out");
  case ArtifactCore::KeyFrame::Anchor::StretchWithLayer:
    return QStringLiteral("Stretch with Layer");
  case ArtifactCore::KeyFrame::Anchor::Absolute:
  default:
    return QStringLiteral("Absolute");
  }
}

QString keyframeInterpolationLabel(const ArtifactCore::InterpolationType type) {
  switch (type) {
  case ArtifactCore::InterpolationType::Constant:
    return QStringLiteral("Hold");
  case ArtifactCore::InterpolationType::EaseIn:
    return QStringLiteral("Ease In");
  case ArtifactCore::InterpolationType::EaseOut:
    return QStringLiteral("Ease Out");
  case ArtifactCore::InterpolationType::EaseInOut:
    return QStringLiteral("Ease In/Out");
  case ArtifactCore::InterpolationType::Bezier:
    return QStringLiteral("Bezier");
  case ArtifactCore::InterpolationType::BackOut:
    return QStringLiteral("Back");
  case ArtifactCore::InterpolationType::BounceOut:
    return QStringLiteral("Bounce");
  case ArtifactCore::InterpolationType::ElasticOut:
    return QStringLiteral("Elastic");
  case ArtifactCore::InterpolationType::Sine:
    return QStringLiteral("Sine");
  case ArtifactCore::InterpolationType::Cubic:
    return QStringLiteral("Cubic");
  case ArtifactCore::InterpolationType::Exponential:
    return QStringLiteral("Exponential");
  case ArtifactCore::InterpolationType::Linear:
  default:
    return QStringLiteral("Linear");
  }
}

QPolygonF keyframeShapePolygon(const QRectF &rect,
                               const ArtifactCore::InterpolationType type) {
  const QPointF c = rect.center();
  switch (type) {
  case ArtifactCore::InterpolationType::Constant:
    return QPolygonF{QPointF(rect.left(), rect.top()),
                     QPointF(rect.right(), rect.top()),
                     QPointF(rect.right(), rect.bottom()),
                     QPointF(rect.left(), rect.bottom())};
  case ArtifactCore::InterpolationType::EaseIn:
    return QPolygonF{QPointF(rect.right(), rect.top()),
                     QPointF(rect.right(), rect.bottom()),
                     QPointF(rect.left(), c.y())};
  case ArtifactCore::InterpolationType::EaseOut:
    return QPolygonF{QPointF(rect.left(), rect.top()),
                     QPointF(rect.right(), c.y()),
                     QPointF(rect.left(), rect.bottom())};
  case ArtifactCore::InterpolationType::EaseInOut:
    return QPolygonF{QPointF(c.x(), rect.top()),
                     QPointF(rect.right(), c.y()),
                     QPointF(c.x(), rect.bottom()),
                     QPointF(rect.left(), c.y())};
  case ArtifactCore::InterpolationType::Bezier:
    return QPolygonF{QPointF(c.x(), rect.top()),
                     QPointF(rect.right(), rect.top() + rect.height() * 0.28),
                     QPointF(rect.right(), rect.bottom() - rect.height() * 0.28),
                     QPointF(c.x(), rect.bottom()),
                     QPointF(rect.left(), rect.bottom() - rect.height() * 0.28),
                     QPointF(rect.left(), rect.top() + rect.height() * 0.28)};
  case ArtifactCore::InterpolationType::BackOut:
  case ArtifactCore::InterpolationType::BounceOut:
  case ArtifactCore::InterpolationType::ElasticOut:
    return QPolygonF{QPointF(c.x(), rect.top()),
                     QPointF(rect.right(), rect.top() + rect.height() * 0.36),
                     QPointF(rect.right() - rect.width() * 0.18, rect.bottom()),
                     QPointF(rect.left() + rect.width() * 0.18, rect.bottom()),
                     QPointF(rect.left(), rect.top() + rect.height() * 0.36)};
  default:
    return QPolygonF{QPointF(c.x(), rect.top()),
                     QPointF(rect.right(), c.y()),
                     QPointF(c.x(), rect.bottom()),
                     QPointF(rect.left(), c.y())};
  }
}

double snappedKeyframeDragTargetFrame(
    const double originalFrame, const double rawDeltaFrames,
    const double currentFrame, const Qt::KeyboardModifiers modifiers,
    QString *outSnapLabel = nullptr) {
  double targetFrame = originalFrame + rawDeltaFrames;
  if (outSnapLabel) {
    outSnapLabel->clear();
  }

  if (modifiers & Qt::AltModifier) {
    if (outSnapLabel) {
      *outSnapLabel = QStringLiteral("snap override");
    }
    return targetFrame;
  }

  if (modifiers & Qt::ShiftModifier) {
    targetFrame = std::round(targetFrame / 10.0) * 10.0;
    if (outSnapLabel) {
      *outSnapLabel = QStringLiteral("10 frame increments");
    }
  }

  if (std::abs(targetFrame - currentFrame) <=
      kKeyframeSnapToPlayheadThresholdFrames) {
    targetFrame = currentFrame;
    if (outSnapLabel) {
      *outSnapLabel = QStringLiteral("current frame");
    }
  }

  return targetFrame;
}

int keyframeDragCollisionCount(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    const QVector<int> &selectedIndices,
    const QVector<double> &selectedOrigFrames, const double deltaFrames,
    const double maxFrame) {
  if (selectedIndices.isEmpty() || selectedOrigFrames.isEmpty()) {
    return 0;
  }

  QSet<int> selectedIndexSet;
  QSet<QString> selectedTargetKeys;
  QSet<QString> occupiedTargetKeys;
  int collisionCount = 0;
  for (int i = 0; i < selectedIndices.size(); ++i) {
    const int selectedIndex = selectedIndices[i];
    if (selectedIndex < 0 || selectedIndex >= markers.size() ||
        i >= selectedOrigFrames.size()) {
      continue;
    }
    selectedIndexSet.insert(selectedIndex);
    const auto &marker = markers[selectedIndex];
    const qint64 mergedFrame = static_cast<qint64>(std::llround(std::clamp(
        selectedOrigFrames[i] + deltaFrames, 0.0,
        maxFrame)));
    const QString targetKey =
        keyframeSelectionKey(marker.layerId, marker.propertyPath, mergedFrame);
    if (selectedTargetKeys.contains(targetKey) ||
        occupiedTargetKeys.contains(targetKey)) {
      ++collisionCount;
      continue;
    }
    selectedTargetKeys.insert(targetKey);
    occupiedTargetKeys.insert(targetKey);
  }

  for (int i = 0; i < selectedIndices.size(); ++i) {
    const int selectedIndex = selectedIndices[i];
    if (selectedIndex < 0 || selectedIndex >= markers.size() ||
        i >= selectedOrigFrames.size()) {
      continue;
    }
    const qint64 targetFrameInt = static_cast<qint64>(std::llround(std::clamp(
        selectedOrigFrames[i] + deltaFrames, 0.0, maxFrame)));
    const QString targetKey =
        keyframeSelectionKey(markers[selectedIndex].layerId,
                             markers[selectedIndex].propertyPath,
                             targetFrameInt);
    for (int markerIndex = 0; markerIndex < markers.size(); ++markerIndex) {
      if (selectedIndexSet.contains(markerIndex)) {
        continue;
      }
      const auto &marker = markers[markerIndex];
      const qint64 markerFrame =
          static_cast<qint64>(std::llround(std::clamp(marker.frame, 0.0, maxFrame)));
      const QString existingKey =
          keyframeSelectionKey(marker.layerId, marker.propertyPath, markerFrame);
      if (existingKey == targetKey) {
        ++collisionCount;
        break;
      }
    }
  }

  return collisionCount;
}

void updateHoverToolTip(QWidget *widget, const QPoint &globalPos,
                        const QString &tooltipText, QString &currentTooltip) {
  if (!widget) {
    return;
  }
  if (tooltipText.isEmpty()) {
    if (!currentTooltip.isEmpty()) {
      QToolTip::hideText();
      currentTooltip.clear();
    }
    return;
  }
  if (tooltipText == currentTooltip) {
    return;
  }
  currentTooltip = tooltipText;
  QToolTip::showText(globalPos, tooltipText, widget);
}

HitResult hitTestClips(
    const QVector<ArtifactTimelineTrackPainterView::TrackClipVisual> &clips,
    const QVector<int> &heights, const QVector<int> &trackTops,
    const double mouseX, const double mouseY, const double ppf,
    const double xOffset, const double yOffset) {
  const double localMouseY = mouseY + yOffset;
  for (int i = 0; i < clips.size(); ++i) {
    const auto &clip = clips[i];
    if (clip.trackIndex < 0 || clip.trackIndex >= heights.size())
      continue;
    const int trackTop = trackTopAt(trackTops, heights, clip.trackIndex);
    const int trackH = heights[clip.trackIndex];
    if (localMouseY < trackTop || localMouseY > trackTop + trackH)
      continue;
    const double clipX = clip.startFrame * ppf - xOffset;
    const double clipW = std::max(2.0, clip.durationFrame * ppf);
    if (mouseX >= clipX - kEdgeHitZone && mouseX <= clipX + kEdgeHitZone)
      return {DragMode::ResizeLeft, i};
    if (mouseX >= clipX + clipW - kEdgeHitZone &&
        mouseX <= clipX + clipW + kEdgeHitZone)
      return {DragMode::ResizeRight, i};
    if (mouseX > clipX && mouseX < clipX + clipW)
      return {DragMode::MoveBody, i};
  }
  return {};
}

QPointF markerCenterFor(
    const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual &marker,
    const QVector<int> &heights, const QVector<int> &trackTops,
    const double ppf, const double xOffset, const double yOffset) {
  if (marker.trackIndex < 0 || marker.trackIndex >= heights.size()) {
    return {};
  }
  const int trackTop = trackTopAt(trackTops, heights, marker.trackIndex);
  const int trackH = heights[marker.trackIndex];
  const int laneCount = std::max(1, marker.laneCount);
  const int laneIndex = std::clamp(marker.laneIndex, 0, laneCount - 1);
  const double laneOffset =
      (laneIndex - (laneCount - 1) * 0.5) * kMarkerLaneStep;
  return QPointF(marker.frame * ppf - xOffset,
                 trackTop + trackH * 0.5 - yOffset + laneOffset);
}

QRectF markerHitRectFor(
    const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual &marker,
    const QVector<int> &heights, const QVector<int> &trackTops,
    const double ppf, const double xOffset, const double yOffset) {
  const QPointF center =
      markerCenterFor(marker, heights, trackTops, ppf, xOffset, yOffset);
  if (center.isNull()) {
    return {};
  }
  const qreal size = marker.laneCount > 1 ? 10.0 : 11.0;
  return QRectF(center.x() - size, center.y() - size, size * 2.0, size * 2.0);
}

bool markerAtCurrentFrame(
    const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual &marker,
    const double currentFrame) {
  return std::abs(marker.frame - currentFrame) < 0.5;
}

int nearestMarkerIndexToCurrentFrame(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>
        &markers,
    const double currentFrame) {
  int bestIndex = -1;
  double bestDistance = std::numeric_limits<double>::max();
  int bestPriority = std::numeric_limits<int>::max();
  for (int i = 0; i < markers.size(); ++i) {
    const auto &marker = markers[i];
    if (markerAtCurrentFrame(marker, currentFrame)) {
      continue;
    }
    const double distance = std::abs(marker.frame - currentFrame);
    const int priority = marker.selected ? 0 : (marker.selectedLayer ? 1 : 2);
    if (priority < bestPriority ||
        (priority == bestPriority && distance < bestDistance)) {
      bestPriority = priority;
      bestDistance = distance;
      bestIndex = i;
    }
  }
  return bestIndex;
}

QRect normalizedSelectionRect(const QPoint &anchor, const QPoint &current) {
  return QRect(anchor, current).normalized();
}

QSet<QString> markerKeysInSelectionRect(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>
        &markers,
    const QVector<int> &heights, const QVector<int> &trackTops,
    const double ppf, const double xOffset, const double yOffset,
    const QRect &selectionRect) {
  QSet<QString> keys;
  if (selectionRect.isNull() || selectionRect.width() <= 0 ||
      selectionRect.height() <= 0) {
    return keys;
  }

  const QRectF rect(selectionRect);
  for (const auto &marker : markers) {
    const QRectF hitRect =
        markerHitRectFor(marker, heights, trackTops, ppf, xOffset, yOffset);
    if (!hitRect.isValid() || !rect.intersects(hitRect)) {
      continue;
    }
    const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
    keys.insert(keyframeSelectionKey(marker.layerId, marker.propertyPath, frame));
  }
  return keys;
}

bool applyMarkerSelectionSet(
    QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    QSet<QString> &selectedKeys, const QSet<QString> &nextSelection) {
  if (selectedKeys == nextSelection) {
    return false;
  }
  selectedKeys = nextSelection;
  applyMarkerSelectionFlags(markers, selectedKeys);
  return true;
}

QRectF
clipRectFor(const ArtifactTimelineTrackPainterView::TrackClipVisual &clip,
            const QVector<int> &heights, const QVector<int> &trackTops,
            const double ppf, const double xOffset, const double yOffset) {
  if (clip.trackIndex < 0 || clip.trackIndex >= heights.size()) {
    return {};
  }

  const int trackTop = trackTopAt(trackTops, heights, clip.trackIndex);
  const int trackH = heights[clip.trackIndex];
  const double clipX = clip.startFrame * ppf - xOffset;
  const double clipW = std::max(2.0, clip.durationFrame * ppf);
  return QRectF(clipX, trackTop + 2.0 - yOffset, clipW,
                std::max(8, trackH - 4));
}

QRectF sourceClipRectFor(
    const ArtifactTimelineTrackPainterView::TrackClipVisual &clip,
    const QVector<int> &heights, const QVector<int> &trackTops,
    const double ppf, const double xOffset, const double yOffset) {
  if (!clip.hasTrimSourceRange || clip.trackIndex < 0 ||
      clip.trackIndex >= heights.size()) {
    return {};
  }

  const int trackTop = trackTopAt(trackTops, heights, clip.trackIndex);
  const int trackH = heights[clip.trackIndex];
  const double clipX = clip.trimMinStartFrame * ppf - xOffset;
  const double clipW =
      std::max(2.0, (clip.trimMaxEndFrame - clip.trimMinStartFrame) * ppf);
  return QRectF(clipX, trackTop + 2.0 - yOffset, clipW,
                std::max(8, trackH - 4));
}

QPointF markerCenterFor(
    const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual &marker,
    const QVector<int> &heights, const QVector<int> &trackTops,
    const double ppf, const double xOffset, const double yOffset);

struct KeyframeConnectionSegment {
  QPointF from;
  QPointF to;
  QColor color;
};

bool shouldDrawConnectionSegment(const QPointF &from, const QPointF &to,
                                 const double ppf, const QRect &dirtyRect) {
  const qreal dx = to.x() - from.x();
  const qreal dy = to.y() - from.y();
  const qreal length = std::hypot(dx, dy);
  if (length < 9.0) {
    return false;
  }
  if (ppf < 0.65 && std::abs(dx) < 10.0) {
    return false;
  }
  if (ppf < 0.9 && length < 22.0) {
    return false;
  }
  const QRectF bounds = QRectF(from, to).normalized().adjusted(-3.0, -3.0, 3.0, 3.0);
  return dirtyRect.intersects(bounds.toAlignedRect());
}

QVector<KeyframeConnectionSegment> collectKeyframeConnectionSegments(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    const QVector<int> &heights, const QVector<int> &trackTops,
    const double ppf, const double xOffset, const double yOffset) {
  QVector<KeyframeConnectionSegment> segments;
  if (markers.size() < 2) {
    return segments;
  }

  struct GroupItem {
    double frame = 0.0;
    int markerIndex = -1;
  };

  QHash<QString, QVector<GroupItem>> groups;
  groups.reserve(markers.size());
  for (int i = 0; i < markers.size(); ++i) {
    const auto &marker = markers[i];
    if (marker.trackIndex < 0 || marker.trackIndex >= heights.size()) {
      continue;
    }
    const QString key = QStringLiteral("%1|%2|%3")
                            .arg(marker.layerId.toString(),
                                 marker.propertyPath,
                                 QString::number(marker.trackIndex));
    groups[key].push_back(GroupItem{marker.frame, i});
  }

  for (auto it = groups.begin(); it != groups.end(); ++it) {
    auto &group = it.value();
    if (group.size() < 2) {
      continue;
    }

    std::sort(group.begin(), group.end(), [](const GroupItem &lhs, const GroupItem &rhs) {
      if (lhs.frame != rhs.frame) {
        return lhs.frame < rhs.frame;
      }
      return lhs.markerIndex < rhs.markerIndex;
    });

    for (int i = 1; i < group.size(); ++i) {
      const auto &fromMarker = markers[group[i - 1].markerIndex];
      const auto &toMarker = markers[group[i].markerIndex];
      const QPointF from =
          markerCenterFor(fromMarker, heights, trackTops, ppf, xOffset, yOffset);
      const QPointF to =
          markerCenterFor(toMarker, heights, trackTops, ppf, xOffset, yOffset);
      if (from.isNull() || to.isNull()) {
        continue;
      }
      QColor color = fromMarker.selectedLayer ? fromMarker.color.lighter(120)
                                              : fromMarker.color.lighter(108);
      color.setAlpha(fromMarker.selected ? 110 : (fromMarker.selectedLayer ? 92 : 72));
      segments.push_back({from, to, color});
    }
  }

  return segments;
}

MarkerHitResult hitTestMarkers(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>
        &markers,
    const QVector<int> &heights, const QVector<int> &trackTops,
    const double mouseX, const double mouseY, const double ppf,
    const double xOffset, const double yOffset) {
  for (int i = 0; i < markers.size(); ++i) {
    const auto &marker = markers[i];
    if (marker.trackIndex < 0 || marker.trackIndex >= heights.size()) {
      continue;
    }
    const QPointF center =
        markerCenterFor(marker, heights, trackTops, ppf, xOffset, yOffset);
    if (center.isNull()) {
      continue;
    }
    const qreal hitSize = marker.laneCount > 1 ? 12.0 : 14.0;
    const QRectF hitRect(center.x() - hitSize, center.y() - hitSize,
                         hitSize * 2.0, hitSize * 2.0);
    if (hitRect.contains(QPointF(mouseX, mouseY))) {
      return {i};
    }
  }
  return {};
}

QVector<ArtifactAbstractLayerPtr> selectedTimelineLayers() {
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

std::optional<int> trackIndexAt(const QVector<int> &heights,
                                const QVector<int> &trackTops,
                                const double mouseY,
                                const double yOffset) {
  const double localMouseY = mouseY + yOffset;
  for (int i = 0; i < heights.size(); ++i) {
    const int trackTop = trackTopAt(trackTops, heights, i);
    const int trackHeight = heights[i];
    if (localMouseY >= trackTop && localMouseY <= trackTop + trackHeight) {
      return i;
    }
  }
  return std::nullopt;
}

QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>
collectKeyframeMarkers(const ArtifactCompositionPtr &composition,
                       const ArtifactLayerSelectionManager *selectionManager,
                       const QVector<TimelineRowDescriptor> &trackRows) {
  QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> markers;
  if (!composition || trackRows.isEmpty()) {
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

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  for (int trackIndex = 0; trackIndex < trackRows.size(); ++trackIndex) {
    const auto &row = trackRows[trackIndex];
    if (row.kind != TimelineRowKind::Property || row.layerId.isNil()) {
      continue;
    }
    const QString propertyPath = row.propertyPath.trimmed();
    if (propertyPath.isEmpty()) {
      continue;
    }

    const auto layer = composition->layerById(row.layerId);
    if (!layer) {
      continue;
    }
    const auto property = layer->getProperty(propertyPath);
    if (!property || !property->isAnimatable()) {
      continue;
    }
    const auto keyframes = property->getKeyFrames();
    if (keyframes.empty()) {
      continue;
    }

    const bool selectedLayer = highlightLayers.contains(layer);
    for (int k = 0; k < static_cast<int>(keyframes.size()); ++k) {
      const auto &keyframe = keyframes[k];
      const auto *prevKeyframe = k > 0 ? &keyframes[k - 1] : nullptr;
      const qint64 frame =
          keyframe.time.rescaledTo(static_cast<int64_t>(std::round(fps)));
      auto isEased = [](const ArtifactCore::KeyFrame &kf) {
        return kf.interpolation != InterpolationType::Linear &&
               kf.interpolation != InterpolationType::Constant;
      };
      const bool incomingEased = prevKeyframe && isEased(*prevKeyframe);
      const bool outgoingEased = isEased(keyframe);
      const bool incomingBezier =
          prevKeyframe && prevKeyframe->interpolation == InterpolationType::Bezier;
      const bool outgoingBezier =
          keyframe.interpolation == InterpolationType::Bezier;
      const bool eased = incomingEased || outgoingEased;
      const bool bezier = incomingBezier || outgoingBezier;
      QColor color =
          keyframeInterpolationColor(keyframe.interpolation, selectedLayer);
      color.setAlpha(selectedLayer ? 255 : 245);
      const QColor labelColor =
          keyframeColorLabelColor(keyframe.colorLabel);
      ArtifactTimelineTrackPainterView::KeyframeMarkerVisual marker;
      marker.layerId = layer->id();
      marker.propertyPath = propertyPath;
      marker.trackIndex = trackIndex;
      marker.frame = static_cast<double>(frame);
      marker.label = row.label.isEmpty()
                         ? ArtifactTimelineKeyframeModel::
                               displayLabelForPropertyPath(propertyPath)
                         : row.label;
      marker.color = color;
      marker.labelColor = labelColor;
      marker.value = keyframe.value;
      marker.selectedLayer = selectedLayer;
      marker.eased = eased;
      marker.incomingEased = incomingEased;
      marker.outgoingEased = outgoingEased;
      marker.incomingBezier = incomingBezier;
      marker.outgoingBezier = outgoingBezier;
      marker.bezier = bezier;
      marker.roving = keyframe.roving;
      marker.interpolation = keyframe.interpolation;
      marker.anchor = keyframe.anchor;
      marker.laneCount = 1;
      marker.laneIndex = 0;
      markers.push_back(std::move(marker));
    }
  }

  std::sort(markers.begin(), markers.end(),
            [](const auto &lhs, const auto &rhs) {
              if (lhs.trackIndex != rhs.trackIndex) {
                return lhs.trackIndex < rhs.trackIndex;
              }
              if (lhs.frame != rhs.frame) {
                return lhs.frame < rhs.frame;
              }
              if (lhs.propertyPath != rhs.propertyPath) {
                return lhs.propertyPath < rhs.propertyPath;
              }
              return lhs.laneIndex < rhs.laneIndex;
            });

  for (int i = 0; i < markers.size();) {
    const int trackIndex = markers[i].trackIndex;
    const qint64 frame = static_cast<qint64>(std::llround(markers[i].frame));
    int j = i + 1;
    while (j < markers.size() && markers[j].trackIndex == trackIndex &&
           static_cast<qint64>(std::llround(markers[j].frame)) == frame) {
      ++j;
    }
    const int laneCount = std::max(1, j - i);
    for (int lane = i; lane < j; ++lane) {
      markers[lane].laneCount = laneCount;
      markers[lane].laneIndex = lane - i;
    }
    i = j;
  }

  return markers;
}

bool applyKeyframeEditAtFrame(const ArtifactCompositionPtr &composition,
                              const ArtifactAbstractLayerPtr &layer,
                              const QString &propertyPath,
                              const qint64 frame, const bool removeKeyframes) {
  if (!composition || !layer || propertyPath.trimmed().isEmpty()) {
    return false;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  const RationalTime nowTime(frame, static_cast<int64_t>(std::llround(fps)));

  std::shared_ptr<ArtifactCore::AbstractProperty> property;
  for (const auto &group : layer->getLayerPropertyGroups()) {
    for (const auto &candidate : group.sortedProperties()) {
      if (!candidate) {
        continue;
      }
      if (candidate->getName() == propertyPath) {
        property = candidate;
        break;
      }
    }
    if (property) {
      break;
    }
  }

  if (!property || !property->isAnimatable()) {
    return false;
  }

  bool changed = false;
  if (removeKeyframes) {
    if (property->hasKeyFrameAt(nowTime)) {
      property->removeKeyFrame(nowTime);
      changed = true;
    }
  } else {
    const QVariant value = property->interpolateValue(nowTime);
    property->addKeyFrame(nowTime,
                          value.isValid() ? value : property->getValue());
    changed = true;
  }

  if (changed) {
    layer->changed();
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(LayerChangedEvent{
        composition ? composition->id().toString() : QString(),
        layer->id().toString(), LayerChangedEvent::ChangeType::Modified});
  }
  return changed;
}

bool removeSelectedKeyframeMarkers(
    const ArtifactCompositionPtr &composition,
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers) {
  if (!composition || markers.isEmpty()) {
    return false;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  const double lastFrame = std::max(
      0.0, static_cast<double>(composition->frameRange().duration() - 1));
  QSet<QString> uniqueKeys;
  bool changed = false;
  for (const auto &marker : markers) {
    const qint64 frame =
        static_cast<qint64>(std::llround(std::clamp(marker.frame, 0.0, lastFrame)));
    const QString selectionKey =
        keyframeSelectionKey(marker.layerId, marker.propertyPath, frame);
    if (uniqueKeys.contains(selectionKey)) {
      continue;
    }
    uniqueKeys.insert(selectionKey);

    const auto layer = composition->layerById(marker.layerId);
    if (!layer) {
      continue;
    }
    const auto property = findLayerPropertyByPath(layer, marker.propertyPath);
    if (!property || !property->isAnimatable()) {
      continue;
    }
    const RationalTime time(frame, static_cast<int64_t>(std::llround(fps)));
    if (!property->hasKeyFrameAt(time)) {
      continue;
    }

    property->removeKeyFrame(time);
    layer->changed();
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{composition->id().toString(), layer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
    changed = true;
  }

  return changed;
}

QJsonArray serializeSelectedKeyframeMarkers(
    const ArtifactCompositionPtr &composition,
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers) {
  QJsonArray keyframes;
  if (!composition || markers.isEmpty()) {
    return keyframes;
  }

  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  QSet<QString> seen;
  for (const auto &marker : markers) {
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
    if (!property) {
      continue;
    }

    const RationalTime time(frame, static_cast<int64_t>(std::llround(fps)));
    const auto keyframesAtProperty = property->getKeyFrames();
    const auto it = std::find_if(keyframesAtProperty.cbegin(),
                                 keyframesAtProperty.cend(),
                                 [&time](const ArtifactCore::KeyFrame &keyframe) {
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
    record.insert(QStringLiteral("interpolation"),
                  static_cast<int>(it->interpolation));
    record.insert(QStringLiteral("anchor"), static_cast<int>(it->anchor));
    record.insert(QStringLiteral("colorLabel"), static_cast<int>(it->colorLabel));
    record.insert(QStringLiteral("roving"), it->roving);
    record.insert(QStringLiteral("cp1_x"), it->cp1_x);
    record.insert(QStringLiteral("cp1_y"), it->cp1_y);
    record.insert(QStringLiteral("cp2_x"), it->cp2_x);
    record.insert(QStringLiteral("cp2_y"), it->cp2_y);
    keyframes.append(record);
  }

  return keyframes;
}

bool pasteKeyframesToLayers(
    const ArtifactCompositionPtr &composition,
    const QVector<ArtifactAbstractLayerPtr> &targetLayers,
    const QJsonArray &records,
    const qint64 targetFrame,
    QSet<QString> *outSelectionKeys = nullptr,
    int *outMergedExistingKeyframeCount = nullptr) {
  if (!composition || targetLayers.isEmpty() || records.isEmpty()) {
    return false;
  }

  QVector<QJsonObject> sourceRecords;
  sourceRecords.reserve(records.size());
  qint64 minFrame = std::numeric_limits<qint64>::max();
  for (const auto &value : records) {
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
  if (outMergedExistingKeyframeCount) {
    *outMergedExistingKeyframeCount = 0;
  }
  int mergedExistingKeyframeCount = 0;

  bool changed = false;
  for (const auto &layer : targetLayers) {
    if (!layer) {
      continue;
    }
    bool layerChanged = false;
    for (const auto &record : sourceRecords) {
      const QString propertyPath =
          record.value(QStringLiteral("propertyPath")).toString();
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
      const auto anchorValue =
          static_cast<ArtifactCore::KeyFrame::Anchor>(
              record.value(QStringLiteral("anchor"))
                  .toInt(static_cast<int>(ArtifactCore::KeyFrame::Anchor::Absolute)));
      const auto colorLabelValue =
          static_cast<ArtifactCore::KeyFrame::ColorLabel>(
              record.value(QStringLiteral("colorLabel"))
                  .toInt(static_cast<int>(ArtifactCore::KeyFrame::ColorLabel::None)));
      const bool roving = record.value(QStringLiteral("roving")).toBool(false);
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
      property->addKeyFrame(time, value.isValid() ? value : property->getValue(),
                            interpolationValue, cp1_x, cp1_y, cp2_x, cp2_y,
                            roving);
      property->setKeyFrameAnchorAt(time, anchorValue);
      property->setKeyFrameColorLabelAt(time, colorLabelValue);
      if (outSelectionKeys) {
        outSelectionKeys->insert(
            keyframeSelectionKey(layer->id(), propertyPath, newFrame));
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

bool duplicateSelectedKeyframeMarkersAtFrame(
    const ArtifactCompositionPtr &composition,
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    const qint64 targetFrame,
    QSet<QString> *outSelectionKeys = nullptr,
    int *outMergedExistingKeyframeCount = nullptr) {
  if (!composition || markers.isEmpty()) {
    return false;
  }

  const auto targetLayers = [&]() {
    QVector<ArtifactAbstractLayerPtr> layers;
    QSet<LayerID> seen;
    for (const auto &marker : markers) {
      if (seen.contains(marker.layerId)) {
        continue;
      }
      seen.insert(marker.layerId);
      const auto layer = composition->layerById(marker.layerId);
      if (layer) {
        layers.push_back(layer);
      }
    }
    return layers;
  }();

  if (targetLayers.isEmpty()) {
    return false;
  }

  const QJsonArray records = serializeSelectedKeyframeMarkers(composition, markers);
  if (records.isEmpty()) {
    return false;
  }

  return pasteKeyframesToLayers(composition, targetLayers, records, targetFrame,
                                outSelectionKeys, outMergedExistingKeyframeCount);
}

bool applyTimelineLayerRangeEdit(const ArtifactAbstractLayerPtr &layer,
                                 const qint64 startFrame,
                                 const qint64 durationFrame,
                                 const bool preserveExistingDuration) {
  if (!layer) {
    return false;
  }

  const qint64 oldInPoint = layer->inPoint().framePosition();
  const qint64 oldOutPoint = layer->outPoint().framePosition();
  const qint64 oldStartTime = layer->startTime().framePosition();
  const qint64 oldDuration = std::max<qint64>(1, oldOutPoint - oldInPoint);

  const qint64 inPoint = std::max<qint64>(0, startFrame);
  const qint64 outPoint =
      preserveExistingDuration
          ? std::max<qint64>(inPoint + 1, inPoint + oldDuration)
          : std::max<qint64>(inPoint + 1, startFrame + durationFrame);
  const qint64 inPointDelta = inPoint - oldInPoint;

  layer->setInPoint(FramePosition(inPoint));
  layer->setOutPoint(FramePosition(outPoint));

  if (!preserveExistingDuration && inPointDelta != 0) {
    layer->setStartTime(FramePosition(oldStartTime + inPointDelta));
  }

  if (preserveExistingDuration && inPointDelta != 0) {
    auto *composition =
        static_cast<ArtifactAbstractComposition *>(layer->composition());
    const double fps = composition
                           ? std::max(1.0, static_cast<double>(
                                              composition->frameRate().framerate()))
                           : 30.0;
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

  return oldInPoint != inPoint || oldOutPoint != outPoint ||
         oldStartTime != layer->startTime().framePosition();
}
} // namespace

class ArtifactTimelineTrackPainterView::Impl {
public:
  Impl();
  ~Impl();

  double durationFrames_ = 300.0;
  double currentFrame_ = 0.0;
  LayerID contextLayerId_;
  QString contextPropertyPath_;
  double pixelsPerFrame_ = 2.0;
  double horizontalOffset_ = 0.0;
  double verticalOffset_ = 0.0;
  QVector<int> trackHeights_;
  QVector<int> trackTops_;
  QVector<TrackClipVisual> clips_;
  QVector<TimelineRowDescriptor> trackRows_;

  // ドラッグ / ホバー状態
  DragMode dragMode_ = DragMode::None;
  int dragClipIndex_ = -1;
  double dragStartX_ = 0.0;
  double dragOrigStartFrame_ = 0.0;
  double dragOrigDuration_ = 0.0;
  double dragOrigTrimMinStartFrame_ = 0.0;
  double dragOrigTrimMaxEndFrame_ = 0.0;
  int hoverClipIndex_ = -1;
  DragMode hoverEdge_ = DragMode::None;
  int hoverMarkerIndex_ = -1;
  int hoverAreaIndex_ = -1;
  int dragMarkerIndex_ = -1;
  int dragAreaIndex_ = -1;
  KeyframeAreaHitPart dragAreaPart_ = KeyframeAreaHitPart::None;
  QPoint dragMarkerStartPoint_;
  QPoint dragAreaStartPoint_;
  double dragMarkerOrigFrame_ = 0.0;
  double dragMarkerTargetFrame_ = 0.0;
  double dragAreaOrigStartFrame_ = 0.0;
  double dragAreaOrigEndFrame_ = 0.0;
  QString dragMarkerSnapLabel_;
  QString dragAreaSnapLabel_;
  QVector<int> dragMarkerSelectionIndices_;
  QVector<double> dragMarkerSelectionOrigFrames_;
  QVector<int> dragAreaSelectionIndices_;
  QVector<double> dragAreaSelectionOrigFrames_;
  QVariant dragAreaValue_;
  bool pendingMarkerSingleClick_ = false;
  QString pendingMarkerSingleClickKey_;
  QString pendingMarkerSingleClickLabel_;
  double pendingMarkerSingleClickFrame_ = 0.0;
  bool draggingMarker_ = false;
  bool panning_ = false;
  QPoint lastPanPoint_;
  QSet<QString> selectedMarkerKeys_;
  bool pendingBackgroundPress_ = false;
  QPoint backgroundPressPoint_;
  Qt::KeyboardModifiers backgroundPressModifiers_ = Qt::NoModifier;
  bool marqueeSelecting_ = false;
  QRect marqueeSelectionRect_;
  QSet<QString> marqueeAnchorSelectionKeys_;
  QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>
      keyframeMarkers_;
  QString hoverToolTipText_;
  bool selectionSyncDirty_ = true;
  const ArtifactAbstractComposition *lastSyncedComposition_ = nullptr;
  QSet<LayerID> lastSyncedSelectedLayerIds_;
  QVector<TimelineRowDescriptor> lastSyncedTrackRows_;

  // Scrub preview tool state
  ToolType activeTool_ = ToolType::Selection;
  bool scrubDragging_ = false;
  double scrubLastFrame_ = 0.0;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  void rebuildTrackTopCache();
};

ArtifactTimelineTrackPainterView::Impl::Impl() {
  trackHeights_.resize(kMinTrackCount);
  trackHeights_.fill(kDefaultTrackHeight);
  rebuildTrackTopCache();

  if (auto *app = Artifact::ApplicationService::instance()) {
    if (auto *toolService = app->toolService()) {
      activeTool_ = toolService->activeTool();
    }
  }

  eventBusSubscriptions_.push_back(
      eventBus_.subscribe<ToolChangedEvent>([this](const ToolChangedEvent &event) {
        activeTool_ = event.toolType;
        if (event.toolType != ToolType::ScrubPreview && scrubDragging_) {
          scrubDragging_ = false;
        }
      }));
}

ArtifactTimelineTrackPainterView::Impl::~Impl() = default;

void ArtifactTimelineTrackPainterView::Impl::rebuildTrackTopCache() {
  trackTops_.resize(trackHeights_.size());
  int currentY = 0;
  for (int i = 0; i < trackHeights_.size(); ++i) {
    trackTops_[i] = currentY;
    currentY += trackHeights_[i] + kTrackSpacing;
  }
}

ArtifactTimelineTrackPainterView::ArtifactTimelineTrackPainterView(
    QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  setAcceptDrops(true);
  setMouseTracking(true);
  setAutoFillBackground(false);
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAttribute(Qt::WA_StaticContents, true);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setFocusPolicy(Qt::StrongFocus);
}

ArtifactTimelineTrackPainterView::~ArtifactTimelineTrackPainterView() {
  delete impl_;
}

void ArtifactTimelineTrackPainterView::setDurationFrames(const double frames) {
  const double sanitized = clampDurationFrames(frames);
  if (std::abs(impl_->durationFrames_ - sanitized) < 0.0001) {
    return;
  }
  impl_->durationFrames_ = sanitized;
  update();
}

double ArtifactTimelineTrackPainterView::durationFrames() const {
  return impl_->durationFrames_;
}

void ArtifactTimelineTrackPainterView::setCurrentFrame(const double frame) {
  const double sanitized =
      std::clamp(frame, 0.0,
                 std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)));
  if (std::abs(impl_->currentFrame_ - sanitized) < 0.0001) {
    return;
  }
  const double oldFrame = impl_->currentFrame_;
  const int oldNearestMarkerIndex =
      nearestMarkerIndexToCurrentFrame(impl_->keyframeMarkers_, oldFrame);
  impl_->currentFrame_ = sanitized;
  const int newNearestMarkerIndex =
      nearestMarkerIndexToCurrentFrame(impl_->keyframeMarkers_, impl_->currentFrame_);
  const double oldX =
      oldFrame * impl_->pixelsPerFrame_ - impl_->horizontalOffset_;
  const double newX =
      impl_->currentFrame_ * impl_->pixelsPerFrame_ - impl_->horizontalOffset_;
  QRect dirtyRect =
      QRect(static_cast<int>(std::floor(std::min(oldX, newX))) - 10, 0,
            static_cast<int>(std::ceil(std::abs(newX - oldX))) + 20, height());
  auto addMarkerDirty = [this, &dirtyRect](const int markerIndex) {
    if (markerIndex < 0 || markerIndex >= impl_->keyframeMarkers_.size()) {
      return;
    }
    const QRectF markerRect = markerHitRectFor(
        impl_->keyframeMarkers_[markerIndex], impl_->trackHeights_,
        impl_->trackTops_, impl_->pixelsPerFrame_, impl_->horizontalOffset_,
        impl_->verticalOffset_);
    if (markerRect.isValid()) {
      dirtyRect = dirtyRect.united(
          markerRect.adjusted(-8.0, -8.0, 8.0, 8.0).toAlignedRect());
    }
  };
  if (oldNearestMarkerIndex != newNearestMarkerIndex) {
    addMarkerDirty(oldNearestMarkerIndex);
    addMarkerDirty(newNearestMarkerIndex);
  }
  update(dirtyRect);
}

double ArtifactTimelineTrackPainterView::currentFrame() const {
  return impl_->currentFrame_;
}

void ArtifactTimelineTrackPainterView::setPixelsPerFrame(const double value) {
  const double sanitized = clampPixelsPerFrame(value);
  if (std::abs(impl_->pixelsPerFrame_ - sanitized) < 0.0001) {
    return;
  }
  impl_->pixelsPerFrame_ = sanitized;
  update();
}

double ArtifactTimelineTrackPainterView::pixelsPerFrame() const {
  return impl_->pixelsPerFrame_;
}

void ArtifactTimelineTrackPainterView::setHorizontalOffset(const double value) {
  const double clamped = clampTimelineHorizontalOffset(
      this, impl_->durationFrames_, impl_->pixelsPerFrame_, value);
  if (std::abs(impl_->horizontalOffset_ - clamped) < 0.0001) {
    return;
  }
  impl_->horizontalOffset_ = clamped;
  update();
}

double ArtifactTimelineTrackPainterView::horizontalOffset() const {
  return impl_->horizontalOffset_;
}

void ArtifactTimelineTrackPainterView::setVerticalOffset(const double value) {
  const double maxOffset = std::max(
      0.0, static_cast<double>(totalTrackContentHeight(impl_->trackHeights_) -
                               height()));
  const double clamped = std::clamp(value, 0.0, maxOffset);
  if (std::abs(impl_->verticalOffset_ - clamped) < 0.0001) {
    return;
  }
  impl_->verticalOffset_ = clamped;
  Q_EMIT verticalOffsetChanged(impl_->verticalOffset_);
  ArtifactCore::globalEventBus().publish<TimelineVerticalScrollEvent>(
      {impl_->verticalOffset_, QStringLiteral("TrackPainterView")});
  update();
}

double ArtifactTimelineTrackPainterView::verticalOffset() const {
  return impl_->verticalOffset_;
}

void ArtifactTimelineTrackPainterView::setKeyframeContext(
    const LayerID &layerId, const QString &propertyPath) {
  const QString trimmedPath = propertyPath.trimmed();
  if (impl_->contextLayerId_ == layerId &&
      impl_->contextPropertyPath_ == trimmedPath) {
    return;
  }
  impl_->contextLayerId_ = layerId;
  impl_->contextPropertyPath_ = trimmedPath;
}

void ArtifactTimelineTrackPainterView::setTrackCount(const int count) {
  const int sanitized = std::max(kMinTrackCount, count);
  if (impl_->trackHeights_.size() == sanitized) {
    setVerticalOffset(impl_->verticalOffset_);
    return;
  }
  impl_->selectionSyncDirty_ = true;
  const int oldSize = impl_->trackHeights_.size();
  impl_->trackHeights_.resize(sanitized);
  for (int i = oldSize; i < sanitized; ++i) {
    impl_->trackHeights_[i] = kDefaultTrackHeight;
  }
  impl_->rebuildTrackTopCache();
  updateGeometry();
  setVerticalOffset(impl_->verticalOffset_);
  update();
}

int ArtifactTimelineTrackPainterView::trackCount() const {
  return impl_->trackHeights_.size();
}

void ArtifactTimelineTrackPainterView::setTrackHeights(
    const QVector<int> &heights) {
  const int sanitizedCount =
      std::max(kMinTrackCount, static_cast<int>(heights.size()));
  bool changed = (impl_->trackHeights_.size() != sanitizedCount);
  if (!changed) {
    for (int i = 0; i < sanitizedCount; ++i) {
      const int value =
          std::max(16, static_cast<int>(heights.value(i, kDefaultTrackHeight)));
      if (impl_->trackHeights_[i] != value) {
        changed = true;
        break;
      }
    }
  }
  if (!changed) {
    return;
  }

  impl_->selectionSyncDirty_ = true;
  impl_->trackHeights_.resize(sanitizedCount);
  for (int i = 0; i < sanitizedCount; ++i) {
    impl_->trackHeights_[i] =
        std::max(16, static_cast<int>(heights.value(i, kDefaultTrackHeight)));
  }
  impl_->rebuildTrackTopCache();
  updateGeometry();
  setVerticalOffset(impl_->verticalOffset_);
  update();
}

void ArtifactTimelineTrackPainterView::setTrackHeight(const int trackIndex,
                                                      const int height) {
  if (trackIndex < 0 || trackIndex >= impl_->trackHeights_.size()) {
    return;
  }
  const int sanitized = std::max(16, height);
  if (impl_->trackHeights_[trackIndex] == sanitized) {
    return;
  }
  impl_->selectionSyncDirty_ = true;
  impl_->trackHeights_[trackIndex] = sanitized;
  impl_->rebuildTrackTopCache();
  updateGeometry();
  update();
}

int ArtifactTimelineTrackPainterView::trackHeight(const int trackIndex) const {
  if (trackIndex < 0 || trackIndex >= impl_->trackHeights_.size()) {
    return kDefaultTrackHeight;
  }
  return impl_->trackHeights_[trackIndex];
}

void ArtifactTimelineTrackPainterView::clearClips() {
  if (impl_->clips_.isEmpty()) {
    return;
  }
  impl_->clips_.clear();
  impl_->selectionSyncDirty_ = true;
  update();
}

void ArtifactTimelineTrackPainterView::setClips(
    const QVector<TrackClipVisual> &clips) {
  if (sameVisualList(impl_->clips_, clips, sameTrackClipVisual)) {
    return;
  }
  impl_->clips_ = clips;
  impl_->selectionSyncDirty_ = true;
  update();
}

void ArtifactTimelineTrackPainterView::setKeyframeMarkers(
    const QVector<KeyframeMarkerVisual> &markers) {
  if (sameVisualList(impl_->keyframeMarkers_, markers,
                     sameKeyframeMarkerVisual)) {
    impl_->selectionSyncDirty_ = false;
    if (reconcileMarkerSelection(impl_->keyframeMarkers_,
                                 impl_->selectedMarkerKeys_)) {
      Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
      update();
    }
    return;
  }
  impl_->keyframeMarkers_ = markers;
  const bool selectionChanged =
      reconcileMarkerSelection(impl_->keyframeMarkers_, impl_->selectedMarkerKeys_);
  impl_->selectionSyncDirty_ = false;
  if (selectionChanged) {
    Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
  }
  update();
}

QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>
ArtifactTimelineTrackPainterView::keyframeMarkers() const {
  return impl_->keyframeMarkers_;
}

QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>
ArtifactTimelineTrackPainterView::selectedKeyframeMarkers() const {
  QVector<KeyframeMarkerVisual> selected;
  for (const auto &marker : impl_->keyframeMarkers_) {
    if (marker.selected) {
      selected.push_back(marker);
    }
  }
  return selected;
}

ArtifactTimelineTrackPainterView::KeyframeMarkerVisual
ArtifactTimelineTrackPainterView::hoveredKeyframeMarker() const {
  if (!impl_ || impl_->hoverMarkerIndex_ < 0 ||
      impl_->hoverMarkerIndex_ >= impl_->keyframeMarkers_.size()) {
    return {};
  }
  return impl_->keyframeMarkers_[impl_->hoverMarkerIndex_];
}

void ArtifactTimelineTrackPainterView::selectAllKeyframeMarkers() {
  QSet<QString> selectedKeys;
  for (const auto &marker : impl_->keyframeMarkers_) {
    const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
    selectedKeys.insert(
        keyframeSelectionKey(marker.layerId, marker.propertyPath, frame));
  }
  if (impl_->selectedMarkerKeys_ == selectedKeys) {
    return;
  }
  impl_->selectedMarkerKeys_ = std::move(selectedKeys);
  applyMarkerSelectionFlags(impl_->keyframeMarkers_,
                            impl_->selectedMarkerKeys_);
  Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
  update();
}

void ArtifactTimelineTrackPainterView::selectSamePropertyKeyframeMarkers() {
  if (!impl_) {
    return;
  }

  QVector<KeyframeMarkerVisual> seed = selectedKeyframeMarkers();
  if (seed.isEmpty() && impl_->hoverMarkerIndex_ >= 0 &&
      impl_->hoverMarkerIndex_ < impl_->keyframeMarkers_.size()) {
    seed.push_back(impl_->keyframeMarkers_[impl_->hoverMarkerIndex_]);
  }
  if (seed.isEmpty()) {
    return;
  }

  const QString propertyKey =
      QStringLiteral("%1|%2")
          .arg(seed.front().layerId.toString(), seed.front().propertyPath);
  QSet<QString> nextSelection;
  for (const auto &marker : impl_->keyframeMarkers_) {
    const QString markerKey =
        QStringLiteral("%1|%2")
            .arg(marker.layerId.toString(), marker.propertyPath);
    if (markerKey != propertyKey) {
      continue;
    }
    const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
    nextSelection.insert(
        keyframeSelectionKey(marker.layerId, marker.propertyPath, frame));
  }

  if (nextSelection == impl_->selectedMarkerKeys_) {
    return;
  }
  impl_->selectedMarkerKeys_ = std::move(nextSelection);
  applyMarkerSelectionFlags(impl_->keyframeMarkers_,
                            impl_->selectedMarkerKeys_);
  Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
  update();
}

void ArtifactTimelineTrackPainterView::selectNeighborKeyframeMarkers() {
  if (!impl_) {
    return;
  }

  QVector<KeyframeMarkerVisual> seed = selectedKeyframeMarkers();
  if (seed.isEmpty() && impl_->hoverMarkerIndex_ >= 0 &&
      impl_->hoverMarkerIndex_ < impl_->keyframeMarkers_.size()) {
    seed.push_back(impl_->keyframeMarkers_[impl_->hoverMarkerIndex_]);
  }
  if (seed.isEmpty()) {
    return;
  }

  QSet<QString> nextSelection = impl_->selectedMarkerKeys_;
  const auto neighbors =
      neighborMarkersForSelection(impl_->keyframeMarkers_, impl_->selectedMarkerKeys_);
  for (const auto &marker : neighbors) {
    const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
    nextSelection.insert(keyframeSelectionKey(marker.layerId, marker.propertyPath, frame));
  }

  if (nextSelection == impl_->selectedMarkerKeys_) {
    return;
  }
  impl_->selectedMarkerKeys_ = std::move(nextSelection);
  applyMarkerSelectionFlags(impl_->keyframeMarkers_,
                            impl_->selectedMarkerKeys_);
  Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
  update();
}

void ArtifactTimelineTrackPainterView::clearKeyframeSelection() {
  if (impl_->selectedMarkerKeys_.isEmpty()) {
    return;
  }
  impl_->selectedMarkerKeys_.clear();
  applyMarkerSelectionFlags(impl_->keyframeMarkers_,
                            impl_->selectedMarkerKeys_);
  Q_EMIT keyframeSelectionChanged(0);
  update();
}

void ArtifactTimelineTrackPainterView::setSelectedKeyframeKeys(
    const QSet<QString> &selectedKeys) {
  if (!impl_) {
    return;
  }
  if (impl_->selectedMarkerKeys_ == selectedKeys) {
    return;
  }
  impl_->selectedMarkerKeys_ = selectedKeys;
  applyMarkerSelectionFlags(impl_->keyframeMarkers_,
                            impl_->selectedMarkerKeys_);
  Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
  update();
}

bool ArtifactTimelineTrackPainterView::deleteSelectedKeyframeMarkers() {
  if (!impl_) {
    return false;
  }

  ArtifactCompositionPtr composition;
  if (auto *svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return false;
  }

  QVector<KeyframeMarkerVisual> targets = selectedKeyframeMarkers();
  if (targets.isEmpty() && impl_->hoverMarkerIndex_ >= 0 &&
      impl_->hoverMarkerIndex_ < impl_->keyframeMarkers_.size()) {
    targets.push_back(impl_->keyframeMarkers_[impl_->hoverMarkerIndex_]);
  }
  if (targets.isEmpty()) {
    return false;
  }

  const auto refs = collectPropertyRefsFromMarkers(targets);
  const auto beforeSnapshots = captureKeyframePropertySnapshots(composition, refs);
  const QSet<QString> beforeSelectionKeys = impl_->selectedMarkerKeys_;
  const int removedCount = static_cast<int>(targets.size());
  const bool changed = removeSelectedKeyframeMarkers(composition, targets);
  if (!changed) {
    return false;
  }

  const auto afterSnapshots = captureKeyframePropertySnapshots(composition, refs);
  const QSet<QString> afterSelectionKeys;
  if (auto *mgr = UndoManager::instance()) {
    QPointer<ArtifactTimelineTrackPainterView> self(this);
    mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
        QStringLiteral("Delete Selected Keyframes"),
        [self, composition, afterSnapshots, afterSelectionKeys]() {
          applyKeyframePropertySnapshots(composition, afterSnapshots);
          if (!self) {
            return;
          }
          ArtifactLayerSelectionManager *selectionManager = nullptr;
          if (auto *app = ArtifactApplicationManager::instance()) {
            selectionManager = app->layerSelectionManager();
          }
          self->syncSelectionState(composition, selectionManager,
                                   self->impl_->trackRows_, true);
          self->setSelectedKeyframeKeys(afterSelectionKeys);
        },
        [self, composition, beforeSnapshots, beforeSelectionKeys]() {
          applyKeyframePropertySnapshots(composition, beforeSnapshots);
          if (!self) {
            return;
          }
          ArtifactLayerSelectionManager *selectionManager = nullptr;
          if (auto *app = ArtifactApplicationManager::instance()) {
            selectionManager = app->layerSelectionManager();
          }
          self->syncSelectionState(composition, selectionManager,
                                   self->impl_->trackRows_, true);
          self->setSelectedKeyframeKeys(beforeSelectionKeys);
        }));
  }

  clearKeyframeSelection();
  Q_EMIT timelineDebugMessage(
      QStringLiteral("Deleted %1 %2")
          .arg(removedCount)
          .arg(formatKeyframeNoun(removedCount)));
  update();
  return true;
}

bool ArtifactTimelineTrackPainterView::setSelectedKeyframeAnchor(
    ArtifactCore::KeyFrame::Anchor anchor) {
  if (!impl_) {
    return false;
  }

  ArtifactCompositionPtr composition;
  if (auto *svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return false;
  }

  QVector<KeyframeMarkerVisual> targets = selectedKeyframeMarkers();
  if (targets.isEmpty() && impl_->hoverMarkerIndex_ >= 0 &&
      impl_->hoverMarkerIndex_ < impl_->keyframeMarkers_.size()) {
    targets.push_back(impl_->keyframeMarkers_[impl_->hoverMarkerIndex_]);
  }
  if (targets.isEmpty()) {
    return false;
  }

  int applied = 0;
  for (const auto &marker : targets) {
    const auto layer = composition->layerById(marker.layerId);
    if (!layer) {
      continue;
    }
    const auto property = findLayerPropertyByPath(layer, marker.propertyPath);
    if (!property || !property->isAnimatable()) {
      continue;
    }
    const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
    const double fps = std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
    const RationalTime time(frame, std::max<qint64>(1, static_cast<qint64>(std::llround(fps))));
    if (!property->hasKeyFrameAt(time)) {
      continue;
    }
    property->setKeyFrameAnchorAt(time, anchor);
    ++applied;
  }

  if (applied > 0) {
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
    Q_EMIT timelineDebugMessage(
        QStringLiteral("Applied keyframe anchor to %1 %2")
            .arg(applied)
            .arg(formatKeyframeNoun(applied)));
    update();
    return true;
  }
  return false;
}

bool ArtifactTimelineTrackPainterView::setSelectedKeyframeColorLabel(
    ArtifactCore::KeyFrame::ColorLabel label) {
  if (!impl_) {
    return false;
  }

  ArtifactCompositionPtr composition;
  if (auto *svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return false;
  }

  QVector<KeyframeMarkerVisual> targets = selectedKeyframeMarkers();
  if (targets.isEmpty() && impl_->hoverMarkerIndex_ >= 0 &&
      impl_->hoverMarkerIndex_ < impl_->keyframeMarkers_.size()) {
    targets.push_back(impl_->keyframeMarkers_[impl_->hoverMarkerIndex_]);
  }
  if (targets.isEmpty()) {
    return false;
  }

  int applied = 0;
  for (const auto &marker : targets) {
    const auto layer = composition->layerById(marker.layerId);
    if (!layer) {
      continue;
    }
    const auto property = findLayerPropertyByPath(layer, marker.propertyPath);
    if (!property || !property->isAnimatable()) {
      continue;
    }
    const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
    const double fps = std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
    const RationalTime time(frame, std::max<qint64>(1, static_cast<qint64>(std::llround(fps))));
    if (!property->hasKeyFrameAt(time)) {
      continue;
    }
    property->setKeyFrameColorLabelAt(time, label);
    ++applied;
  }

  if (applied > 0) {
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
    Q_EMIT timelineDebugMessage(
        QStringLiteral("Applied keyframe color label to %1 %2")
            .arg(applied)
            .arg(formatKeyframeNoun(applied)));
    update();
    return true;
  }
  return false;
}

bool ArtifactTimelineTrackPainterView::duplicateSelectedKeyframeMarkersAtCurrentFrame() {
  if (!impl_) {
    return false;
  }

  ArtifactCompositionPtr composition;
  if (auto *svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return false;
  }

  QVector<KeyframeMarkerVisual> targets = selectedKeyframeMarkers();
  if (targets.isEmpty() && impl_->hoverMarkerIndex_ >= 0 &&
      impl_->hoverMarkerIndex_ < impl_->keyframeMarkers_.size()) {
    targets.push_back(impl_->keyframeMarkers_[impl_->hoverMarkerIndex_]);
  }
  if (targets.isEmpty()) {
    return false;
  }

  const qint64 frame = static_cast<qint64>(std::llround(
      std::max<double>(0.0, std::min<double>(impl_->currentFrame_, impl_->durationFrames_ - 1.0))));
  const auto refs = collectPropertyRefsFromMarkers(targets);
  const auto beforeSnapshots = captureKeyframePropertySnapshots(composition, refs);
  const QSet<QString> beforeSelectionKeys = impl_->selectedMarkerKeys_;
  QSet<QString> nextSelectionKeys;
  int mergedExistingKeyframeCount = 0;
  const bool changed =
      duplicateSelectedKeyframeMarkersAtFrame(
          composition, targets, frame, &nextSelectionKeys, &mergedExistingKeyframeCount);
  if (!changed) {
    return false;
  }

  const auto afterSnapshots = captureKeyframePropertySnapshots(composition, refs);
  if (auto *mgr = UndoManager::instance()) {
    QPointer<ArtifactTimelineTrackPainterView> self(this);
    const QSet<QString> afterSelectionKeys = nextSelectionKeys;
    mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
        QStringLiteral("Duplicate Selected Keyframes Here"),
        [self, composition, afterSnapshots, afterSelectionKeys]() {
          applyKeyframePropertySnapshots(composition, afterSnapshots);
          if (!self) {
            return;
          }
          ArtifactLayerSelectionManager *selectionManager = nullptr;
          if (auto *app = ArtifactApplicationManager::instance()) {
            selectionManager = app->layerSelectionManager();
          }
          self->syncSelectionState(composition, selectionManager,
                                   self->impl_->trackRows_, true);
          self->setSelectedKeyframeKeys(afterSelectionKeys);
        },
        [self, composition, beforeSnapshots, beforeSelectionKeys]() {
          applyKeyframePropertySnapshots(composition, beforeSnapshots);
          if (!self) {
            return;
          }
          ArtifactLayerSelectionManager *selectionManager = nullptr;
          if (auto *app = ArtifactApplicationManager::instance()) {
            selectionManager = app->layerSelectionManager();
          }
          self->syncSelectionState(composition, selectionManager,
                                   self->impl_->trackRows_, true);
          self->setSelectedKeyframeKeys(beforeSelectionKeys);
        }));
  }

  if (!nextSelectionKeys.isEmpty()) {
    impl_->selectedMarkerKeys_ = std::move(nextSelectionKeys);
  }
  ArtifactLayerSelectionManager *selectionManager = nullptr;
  if (auto *app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }
  syncSelectionState(composition, selectionManager, impl_->trackRows_, true);
  const QString mergeNote = mergedExistingKeyframeCount > 0
                                ? (mergedExistingKeyframeCount == 1
                                       ? QStringLiteral(" (merged 1 existing keyframe at destination)")
                                       : QStringLiteral(" (merged %1 existing keyframes at destination)")
                                             .arg(mergedExistingKeyframeCount))
                                : QString();
  Q_EMIT timelineDebugMessage(
      QStringLiteral("Duplicated %1 %3 at F%2%4")
          .arg(static_cast<int>(targets.size()))
          .arg(frame)
          .arg(formatKeyframeNoun(static_cast<int>(targets.size())))
          .arg(mergeNote));
  update();
  return true;
}

bool ArtifactTimelineTrackPainterView::distributeSelectedKeyframeMarkersEvenly() {
  if (!impl_) {
    return false;
  }

  ArtifactCompositionPtr composition;
  if (auto *svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return false;
  }

  const QVector<KeyframeMarkerVisual> targets = selectedKeyframeMarkers();
  if (targets.size() < 2) {
    return false;
  }

  const auto refs = collectPropertyRefsFromMarkers(targets);
  const auto beforeSnapshots = captureKeyframePropertySnapshots(composition, refs);
  const QSet<QString> beforeSelectionKeys = impl_->selectedMarkerKeys_;
  auto records = collectSelectedKeyframeRecords(composition, targets);
  if (records.size() < 2) {
    return false;
  }

  QSet<QString> nextSelectionKeys;
  int movedCount = 0;
  const bool changed =
      applyEvenKeyframeDistribution(composition, &records, &nextSelectionKeys,
                                    &movedCount);
  if (!changed) {
    return false;
  }

  const auto afterSnapshots = captureKeyframePropertySnapshots(composition, refs);
  if (auto *mgr = UndoManager::instance()) {
    QPointer<ArtifactTimelineTrackPainterView> self(this);
    const QSet<QString> afterSelectionKeys = nextSelectionKeys;
    mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
        QStringLiteral("Distribute Selected Keyframes Evenly"),
        [self, composition, afterSnapshots, afterSelectionKeys]() {
          applyKeyframePropertySnapshots(composition, afterSnapshots);
          if (!self) {
            return;
          }
          ArtifactLayerSelectionManager *selectionManager = nullptr;
          if (auto *app = ArtifactApplicationManager::instance()) {
            selectionManager = app->layerSelectionManager();
          }
          self->syncSelectionState(composition, selectionManager,
                                   self->impl_->trackRows_, true);
          self->setSelectedKeyframeKeys(afterSelectionKeys);
        },
        [self, composition, beforeSnapshots, beforeSelectionKeys]() {
          applyKeyframePropertySnapshots(composition, beforeSnapshots);
          if (!self) {
            return;
          }
          ArtifactLayerSelectionManager *selectionManager = nullptr;
          if (auto *app = ArtifactApplicationManager::instance()) {
            selectionManager = app->layerSelectionManager();
          }
          self->syncSelectionState(composition, selectionManager,
                                   self->impl_->trackRows_, true);
          self->setSelectedKeyframeKeys(beforeSelectionKeys);
        }));
  }

  if (!nextSelectionKeys.isEmpty()) {
    impl_->selectedMarkerKeys_ = std::move(nextSelectionKeys);
  }
  ArtifactLayerSelectionManager *selectionManager = nullptr;
  if (auto *app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }
  syncSelectionState(composition, selectionManager, impl_->trackRows_, true);
  Q_EMIT timelineDebugMessage(
      QStringLiteral("Distributed %1 %2 evenly")
          .arg(movedCount)
          .arg(formatKeyframeNoun(movedCount)));
  update();
  return true;
}

bool ArtifactTimelineTrackPainterView::repeatSelectedKeyframeMarkersAtCurrentFrame() {
  if (!impl_) {
    return false;
  }

  ArtifactCompositionPtr composition;
  if (auto *svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return false;
  }

  QVector<KeyframeMarkerVisual> targets = selectedKeyframeMarkers();
  if (targets.isEmpty() && impl_->hoverMarkerIndex_ >= 0 &&
      impl_->hoverMarkerIndex_ < impl_->keyframeMarkers_.size()) {
    targets.push_back(impl_->keyframeMarkers_[impl_->hoverMarkerIndex_]);
  }
  if (targets.isEmpty()) {
    return false;
  }

  bool ok = false;
  const int repeatCount = QInputDialog::getInt(
      this, QStringLiteral("Repeat Selected Keyframes"),
      QStringLiteral("Copies to create:"), 2, 1, 24, 1, &ok);
  if (!ok) {
    return false;
  }

  const auto refs = collectPropertyRefsFromMarkers(targets);
  const auto beforeSnapshots = captureKeyframePropertySnapshots(composition, refs);
  const QSet<QString> beforeSelectionKeys = impl_->selectedMarkerKeys_;
  const auto records = collectSelectedKeyframeRecords(composition, targets);
  if (records.isEmpty()) {
    return false;
  }

  const qint64 baseFrame = static_cast<qint64>(
      std::llround(std::max(0.0, impl_->currentFrame_)));
  QSet<QString> nextSelectionKeys;
  int mergedExistingKeyframeCount = 0;
  const bool changed = repeatSelectedKeyframeRecords(
      composition, records, baseFrame, repeatCount, &nextSelectionKeys,
      &mergedExistingKeyframeCount);
  if (!changed) {
    return false;
  }

  const auto afterSnapshots = captureKeyframePropertySnapshots(composition, refs);
  if (auto *mgr = UndoManager::instance()) {
    QPointer<ArtifactTimelineTrackPainterView> self(this);
    const QSet<QString> afterSelectionKeys = nextSelectionKeys;
    mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
        QStringLiteral("Repeat Selected Keyframes"),
        [self, composition, afterSnapshots, afterSelectionKeys]() {
          applyKeyframePropertySnapshots(composition, afterSnapshots);
          if (!self) {
            return;
          }
          ArtifactLayerSelectionManager *selectionManager = nullptr;
          if (auto *app = ArtifactApplicationManager::instance()) {
            selectionManager = app->layerSelectionManager();
          }
          self->syncSelectionState(composition, selectionManager,
                                   self->impl_->trackRows_, true);
          self->setSelectedKeyframeKeys(afterSelectionKeys);
        },
        [self, composition, beforeSnapshots, beforeSelectionKeys]() {
          applyKeyframePropertySnapshots(composition, beforeSnapshots);
          if (!self) {
            return;
          }
          ArtifactLayerSelectionManager *selectionManager = nullptr;
          if (auto *app = ArtifactApplicationManager::instance()) {
            selectionManager = app->layerSelectionManager();
          }
          self->syncSelectionState(composition, selectionManager,
                                   self->impl_->trackRows_, true);
          self->setSelectedKeyframeKeys(beforeSelectionKeys);
        }));
  }

  if (!nextSelectionKeys.isEmpty()) {
    impl_->selectedMarkerKeys_ = std::move(nextSelectionKeys);
  }
  ArtifactLayerSelectionManager *selectionManager = nullptr;
  if (auto *app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }
  syncSelectionState(composition, selectionManager, impl_->trackRows_, true);
  const QString mergeNote =
      mergedExistingKeyframeCount > 0
          ? QStringLiteral(" (merged %1 existing keyframe%2)")
                .arg(mergedExistingKeyframeCount)
                .arg(mergedExistingKeyframeCount == 1 ? QString() : QStringLiteral("s"))
          : QString();
  Q_EMIT timelineDebugMessage(
      QStringLiteral("Repeated %1 %2 at F%3%4")
          .arg(static_cast<int>(targets.size()))
          .arg(formatKeyframeNoun(static_cast<int>(targets.size())))
          .arg(baseFrame)
          .arg(mergeNote));
  update();
  return true;
}

bool ArtifactTimelineTrackPainterView::hasSelectedKeyframes() const {
  return !selectedMarkerIndices(impl_->keyframeMarkers_).isEmpty();
}

void ArtifactTimelineTrackPainterView::syncSelectionState(
    const ArtifactCompositionPtr &composition,
    ArtifactLayerSelectionManager *selectionManager,
    const QVector<TimelineRowDescriptor> &trackRows,
    const bool forceRefresh) {
  impl_->trackRows_ = trackRows;
  QSet<LayerID> selectedLayerIds;
  if (selectionManager) {
    const auto selectedLayers = selectionManager->selectedLayers();
    selectedLayerIds.reserve(selectedLayers.size());
    for (const auto &layer : selectedLayers) {
      if (layer) {
        selectedLayerIds.insert(layer->id());
      }
    }
    if (selectedLayerIds.isEmpty()) {
      if (auto currentLayer = selectionManager->currentLayer()) {
        selectedLayerIds.insert(currentLayer->id());
      }
    }
  }

  const ArtifactAbstractComposition *compositionPtr = composition.get();
  if (!forceRefresh && !impl_->selectionSyncDirty_ &&
      impl_->lastSyncedComposition_ == compositionPtr &&
      impl_->lastSyncedSelectedLayerIds_ == selectedLayerIds &&
      sameVisualList(impl_->lastSyncedTrackRows_, trackRows,
                     sameTimelineRowDescriptor)) {
    return;
  }

  QRectF dirtyRect;
  bool hasDirty = false;
  bool changed = false;
  for (auto &clip : impl_->clips_) {
    const bool selected = selectedLayerIds.contains(clip.layerId);
    if (clip.selected != selected) {
      const QRectF rect =
          clipRectFor(clip, impl_->trackHeights_, impl_->trackTops_,
                      impl_->pixelsPerFrame_,
                      impl_->horizontalOffset_, impl_->verticalOffset_);
      clip.selected = selected;
      if (rect.isValid()) {
        dirtyRect = hasDirty ? dirtyRect.united(rect) : rect;
        hasDirty = true;
      }
      changed = true;
    }
  }

  const auto newMarkers =
      collectKeyframeMarkers(composition, selectionManager, trackRows);
  bool selectionChanged = false;
  if (!sameVisualList(impl_->keyframeMarkers_, newMarkers,
                      sameKeyframeMarkerVisual)) {
    impl_->keyframeMarkers_ = newMarkers;
    selectionChanged = reconcileMarkerSelection(impl_->keyframeMarkers_,
                                                impl_->selectedMarkerKeys_);
    changed = true;
  } else {
    selectionChanged = reconcileMarkerSelection(impl_->keyframeMarkers_,
                                                impl_->selectedMarkerKeys_);
  }

  if (changed) {
    update((hasDirty ? dirtyRect : QRectF(rect()))
               .adjusted(-2.0, -2.0, 2.0, 2.0)
               .toAlignedRect());
  } else if (selectionChanged) {
    update();
  }

  if (selectionChanged) {
    Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
  }

  impl_->lastSyncedComposition_ = compositionPtr;
  impl_->lastSyncedSelectedLayerIds_ = selectedLayerIds;
  impl_->lastSyncedTrackRows_ = trackRows;
  impl_->selectionSyncDirty_ = false;
}

QVector<ArtifactTimelineTrackPainterView::TrackClipVisual>
ArtifactTimelineTrackPainterView::clips() const {
  return impl_->clips_;
}

QSize ArtifactTimelineTrackPainterView::minimumSizeHint() const {
  int h = 0;
  for (int i = 0; i < impl_->trackHeights_.size(); ++i) {
    h += impl_->trackHeights_[i];
    if (i + 1 < impl_->trackHeights_.size()) {
      h += kTrackSpacing;
    }
  }
  return QSize(320, std::max(120, h));
}

void ArtifactTimelineTrackPainterView::paintEvent(QPaintEvent *event) {
  ArtifactCore::ProfileTimer _profTimer("TimelineTrackPaint",
                                        ArtifactCore::ProfileCategory::UI);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, false);
  const TimelineThemeColors theme = timelineThemeColors();

  const QRect dirtyRect = event->rect();
  p.fillRect(dirtyRect, theme.background);

  const QRect fullRect = rect();
  const double ppf = impl_->pixelsPerFrame_;
  const double xOffset = impl_->horizontalOffset_;
  const double yOffset = impl_->verticalOffset_;

  // Track rows (Virtualization)
  QSet<int> keyframeTracksWithMarkers;
  for (const auto &marker : impl_->keyframeMarkers_) {
    if (marker.trackIndex >= 0 && marker.trackIndex < impl_->trackHeights_.size()) {
      keyframeTracksWithMarkers.insert(marker.trackIndex);
    }
  }

  for (int i = 0; i < impl_->trackHeights_.size(); ++i) {
    const int rowH = impl_->trackHeights_[i];
    const double rowTop =
        trackTopAt(impl_->trackTops_, impl_->trackHeights_, i) - yOffset;
    const auto &row =
        (i >= 0 && i < impl_->trackRows_.size()) ? impl_->trackRows_.at(i)
                                                 : TimelineRowDescriptor{};
    const bool isSelectedPropertyLane =
        row.kind == TimelineRowKind::Property && !row.layerId.isNil() &&
        impl_->lastSyncedSelectedLayerIds_.contains(row.layerId);

    // 画面外（dirtyRect外）の行は描画をスキップ
    if (rowTop + rowH >= dirtyRect.top() && rowTop <= dirtyRect.bottom()) {
      const QColor rowColor =
          (i % 2 == 0) ? theme.surface.lighter(102) : theme.surface.darker(104);
      p.fillRect(QRectF(0.0, rowTop, fullRect.width(), rowH), rowColor);
      if (isSelectedPropertyLane) {
        QColor laneTint = theme.accent;
        laneTint.setAlpha(22);
        p.fillRect(QRectF(0.0, rowTop, fullRect.width(), rowH), laneTint);
        QColor laneStrip = theme.accent;
        laneStrip.setAlpha(112);
        p.fillRect(QRectF(0.0, rowTop, 3.0, rowH), laneStrip);

      }
      p.setPen(QPen(theme.border.darker(160), 1));
      p.drawLine(0, rowTop + rowH, fullRect.width(), rowTop + rowH);
    }
  }

  // Vertical frame grid (tick lines only, no labels).
  const int majorStep = 10;
  const int minorStep = 5;
  const int startFrame = std::max(
      0, static_cast<int>(std::floor((xOffset + dirtyRect.left()) / ppf)));
  const int endFrame =
      static_cast<int>(std::ceil((xOffset + dirtyRect.right()) / ppf));
  for (int f = startFrame; f <= endFrame; ++f) {
    const double x = f * ppf - xOffset;
    const bool major = (f % majorStep) == 0;
    const bool minor = !major && (f % minorStep) == 0;
    if (!major && !minor) {
      continue;
    }
    p.setPen(
        QPen(major ? theme.border.lighter(112) : theme.border.darker(112), 1));
    p.drawLine(QPointF(x, dirtyRect.top()), QPointF(x, dirtyRect.bottom()));
  }

  // Clips.
  p.setRenderHint(QPainter::Antialiasing, true);
  const QFontMetrics metrics = p.fontMetrics();
  for (int i = 0; i < impl_->clips_.size(); ++i) {
    const auto &clip = impl_->clips_[i];

    // クリップの描画範囲を計算
    const double clipX = clip.startFrame * ppf - xOffset;
    const double clipW = clip.durationFrame * ppf;

    // Y座標を特定するために再度ループ（キャッシュしておくと高速だが、まずは単純に）
    const int clipY =
        trackTopAt(impl_->trackTops_, impl_->trackHeights_, clip.trackIndex) -
        static_cast<int>(std::round(yOffset));
    const int clipH =
        (clip.trackIndex >= 0 && clip.trackIndex < impl_->trackHeights_.size())
            ? impl_->trackHeights_[clip.trackIndex]
            : kDefaultTrackHeight;

    // 可視性チェック
    if (clipX + clipW < dirtyRect.left() || clipX > dirtyRect.right() ||
        clipY + clipH < dirtyRect.top() || clipY > dirtyRect.bottom()) {
      continue;
    }

    if (clip.trackIndex < 0 || clip.trackIndex >= impl_->trackHeights_.size()) {
      continue;
    }
    const int trackTop =
        trackTopAt(impl_->trackTops_, impl_->trackHeights_, clip.trackIndex);
    const int trackH = impl_->trackHeights_[clip.trackIndex];
    const double x = clip.startFrame * ppf - xOffset;
    const double w = std::max(2.0, clip.durationFrame * ppf);
    QRectF clipRect(x, trackTop + 2.0 - yOffset, w, std::max(8, trackH - 4));
    if (!clipRect.intersects(QRectF(fullRect))) {
      continue;
    }

    if (clip.hasTrimSourceRange) {
      const QRectF sourceRect =
          sourceClipRectFor(clip, impl_->trackHeights_, impl_->trackTops_, ppf,
                            xOffset, yOffset);
      if (sourceRect.isValid()) {
        QColor sourceFill = clip.fillColor;
        sourceFill.setAlpha(54);
        QColor sourceBorder = clip.fillColor.lighter(118);
        sourceBorder.setAlpha(92);
        p.setPen(QPen(sourceBorder, 1.0));
        p.setBrush(sourceFill);
        p.drawRoundedRect(sourceRect, kClipCorner, kClipCorner);
      }
    }

    const bool isHovered = (i == impl_->hoverClipIndex_);
    const bool isSelected = clip.selected;
    QColor fill = clip.fillColor;
    fill.setAlpha(255);
    if (isHovered && !isSelected) {
      fill = fill.lighter(108);
    }
    QColor selectedFill = clip.fillColor;
    selectedFill.setAlpha(36);
    const QColor border = isSelected ? theme.accent.lighter(130)
                                     : theme.border.darker(160);
    p.setPen(QPen(border, isSelected ? 2 : 1));
    p.setBrush(isSelected ? selectedFill : fill);
    p.drawRoundedRect(clipRect, kClipCorner, kClipCorner);

    if (isSelected || isHovered) {
      const QColor rim = isSelected ? QColor(theme.accent.lighter(135))
                                    : QColor(255, 255, 255, 60);
      p.setBrush(Qt::NoBrush);
      p.setPen(QPen(rim, isSelected ? 2.0 : 1.0));
      p.drawRoundedRect(clipRect.adjusted(1.0, 1.0, -1.0, -1.0), kClipCorner,
                        kClipCorner);
    }

    if (!clip.title.isEmpty() && clipRect.width() > 28.0) {
      p.setPen(clip.selected ? theme.background : theme.text);
      const QString text = metrics.elidedText(
          clip.title, Qt::ElideRight,
          static_cast<int>(clipRect.width()) - (kClipPadding * 2));
      p.drawText(clipRect.adjusted(kClipPadding, 0, -kClipPadding, 0),
                 Qt::AlignVCenter | Qt::AlignLeft, text);
    }

    // リサイズグリップ (ホバー時 or 選択時にエッジに縦線を描画)
    if ((isHovered || clip.selected) && clipRect.width() > 16.0) {
      const qreal gripY1 = clipRect.center().y() - 5.0;
      const qreal gripY2 = clipRect.center().y() + 5.0;
      p.setPen(QPen(QColor(255, 255, 255, isHovered ? 130 : 80), 2.0,
                    Qt::SolidLine, Qt::RoundCap));
      p.drawLine(QPointF(clipRect.left() + 4.0, gripY1),
                 QPointF(clipRect.left() + 4.0, gripY2));
      p.drawLine(QPointF(clipRect.right() - 4.0, gripY1),
                 QPointF(clipRect.right() - 4.0, gripY2));
    }

    if (!clip.waveformPeaks.isEmpty() &&
        clipRect.width() > 20.0 && clipRect.height() > 10.0) {
      const QColor waveformColor = isSelected ? theme.background.darker(140)
                                              : theme.text.lighter(110);
      QColor waveformSoft = waveformColor;
      waveformSoft.setAlpha(90);
      const qreal innerTop = clipRect.top() + 5.0;
      const qreal innerBottom = clipRect.bottom() - 5.0;
      const qreal centerY = (innerTop + innerBottom) * 0.5;
      const qreal halfSpan = std::max<qreal>(2.0, (innerBottom - innerTop) * 0.42);
      if (!clip.waveformPeaks.isEmpty()) {
        const int binCount = clip.waveformPeaks.size();
        const bool hasRms = clip.waveformRms.size() == binCount;
        const qreal innerLeft = clipRect.left() + 4.0;
        const qreal innerRight = clipRect.right() - 4.0;
        const qreal span = std::max<qreal>(1.0, innerRight - innerLeft);
        for (int bar = 0; bar < binCount; ++bar) {
          const qreal t = binCount > 1 ? static_cast<qreal>(bar) / static_cast<qreal>(binCount - 1) : 0.0;
          const qreal x = innerLeft + t * span;
          const qreal peak = std::clamp(static_cast<qreal>(clip.waveformPeaks[bar]), 0.0, 1.0);
          const qreal top = centerY - halfSpan * peak;
          const qreal bottom = centerY + halfSpan * peak;
          if (hasRms) {
            const qreal rms = std::clamp(static_cast<qreal>(clip.waveformRms[bar]), 0.0, 1.0);
            const qreal rmsTop = centerY - halfSpan * rms;
            const qreal rmsBottom = centerY + halfSpan * rms;
            p.setPen(QPen(waveformSoft, 1.0));
            p.drawLine(QPointF(x, rmsTop), QPointF(x, rmsBottom));
          }
          p.setPen(QPen((bar % 3 == 0) ? waveformColor : waveformSoft, 1.0));
          p.drawLine(QPointF(x, top), QPointF(x, bottom));
        }
      } else {
        const int barCount = std::max(8, static_cast<int>(clipRect.width() / 10.0));
        const quint32 hashSeed = qHash(clip.clipId) ^ qHash(clip.title) ^
                                 static_cast<quint32>(clip.trackIndex * 131);
        for (int bar = 0; bar < barCount; ++bar) {
          const qreal t = barCount > 1 ? static_cast<qreal>(bar) / static_cast<qreal>(barCount - 1) : 0.0;
          const qreal x = clipRect.left() + 4.0 + t * (clipRect.width() - 8.0);
          const quint32 sample = (hashSeed >> (bar % 16)) ^ static_cast<quint32>(bar * 2654435761u);
          const qreal amplitude = 0.25 + 0.75 * (static_cast<qreal>(sample & 0xFFu) / 255.0);
          const qreal top = centerY - halfSpan * amplitude;
          const qreal bottom = centerY + halfSpan * amplitude;
          p.setPen(QPen((bar % 3 == 0) ? waveformColor : waveformSoft, 1.0));
          p.drawLine(QPointF(x, top), QPointF(x, bottom));
        }
      }
    }
  }

  // Keyframe markers.
  QSet<int> selectedMarkerTracks;
  QSet<int> selectedKeyframeTracks;
  QVector<int> keyframeCountsByTrack(impl_->trackHeights_.size(), 0);
  QVector<int> currentFrameKeyframeCountsByTrack(impl_->trackHeights_.size(), 0);
  for (const auto &marker : impl_->keyframeMarkers_) {
    if (marker.trackIndex >= 0 &&
        marker.trackIndex < keyframeCountsByTrack.size()) {
      ++keyframeCountsByTrack[marker.trackIndex];
      if (markerAtCurrentFrame(marker, impl_->currentFrame_)) {
        ++currentFrameKeyframeCountsByTrack[marker.trackIndex];
      }
    }
    if (marker.selectedLayer) {
      selectedMarkerTracks.insert(marker.trackIndex);
    }
    if (marker.selected) {
      selectedKeyframeTracks.insert(marker.trackIndex);
    }
  }

  for (const int trackIndex : selectedMarkerTracks) {
    if (trackIndex < 0 || trackIndex >= impl_->trackHeights_.size()) {
      continue;
    }
    const int trackTop =
        trackTopAt(impl_->trackTops_, impl_->trackHeights_, trackIndex);
    const int trackH = impl_->trackHeights_[trackIndex];
    const int keyframeCount = keyframeCountsByTrack.value(trackIndex, 0);
    const int currentFrameKeyframeCount =
        currentFrameKeyframeCountsByTrack.value(trackIndex, 0);
    const QRectF laneRect(
        0.0, trackTop + 2.0 - yOffset, static_cast<qreal>(width()),
        std::max(8, trackH - 4));
    if (!dirtyRect.intersects(laneRect.toAlignedRect().adjusted(0, -2, 0, 2))) {
      continue;
    }
    QColor laneFill = theme.accent;
    laneFill.setAlpha(24);
    p.fillRect(laneRect, laneFill);
    p.setPen(QPen(theme.accent.lighter(110), 1.0));
    p.drawLine(QPointF(laneRect.left(), laneRect.top() + 0.5),
               QPointF(laneRect.right(), laneRect.top() + 0.5));
    p.drawLine(QPointF(laneRect.left(), laneRect.bottom() - 0.5),
               QPointF(laneRect.right(), laneRect.bottom() - 0.5));

    if (keyframeCount > 0) {
      QString badgeText = keyframeCount == 1
                              ? QStringLiteral("1 key")
                              : QStringLiteral("%1 keys").arg(keyframeCount);
      if (currentFrameKeyframeCount > 0) {
        badgeText += QStringLiteral(" | Current");
      }
      const QFontMetrics fm = p.fontMetrics();
      const int badgeW = std::min(110, std::max(52, fm.horizontalAdvance(badgeText) + 14));
      const QRect badgeRect(fullRect.width() - badgeW - 10,
                            static_cast<int>(std::round(trackTop + 5.0 - yOffset)),
                            badgeW, std::max(14, trackH - 10));
      QColor badgeBg = theme.background;
      badgeBg.setAlpha(175);
      QColor badgeBorder = theme.accent.lighter(120);
      badgeBorder.setAlpha(160);
      QColor badgeTextColor = theme.accent.lighter(145);
      if (currentFrameKeyframeCount > 0) {
        badgeBorder = theme.accent.lighter(140);
        badgeBorder.setAlpha(220);
        badgeTextColor = theme.text.lighter(150);
      }
      p.setPen(QPen(badgeBorder, 1.0));
      p.setBrush(badgeBg);
      p.drawRoundedRect(badgeRect, 4, 4);
      p.setPen(badgeTextColor);
      p.drawText(badgeRect.adjusted(7, 0, -7, 0),
                 Qt::AlignVCenter | Qt::AlignLeft, badgeText);
    }
  }

  const QVector<KeyframeConnectionSegment> connectionSegments =
      collectKeyframeConnectionSegments(impl_->keyframeMarkers_,
                                        impl_->trackHeights_, impl_->trackTops_,
                                        ppf, xOffset, yOffset);
  for (const auto &segment : connectionSegments) {
    if (!shouldDrawConnectionSegment(segment.from, segment.to, ppf, dirtyRect)) {
      continue;
    }
    const qreal lineWidth = std::clamp(2.2 + ppf * 0.06, 2.2, 4.0);
    p.setPen(QPen(segment.color, lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawLine(segment.from, segment.to);
  }

  for (const int trackIndex : selectedKeyframeTracks) {
    if (trackIndex < 0 || trackIndex >= impl_->trackHeights_.size()) {
      continue;
    }
    const int trackTop =
        trackTopAt(impl_->trackTops_, impl_->trackHeights_, trackIndex);
    const int trackH = impl_->trackHeights_[trackIndex];
    const QRectF laneRect(
        0.0, trackTop + 5.0 - yOffset, static_cast<qreal>(width()),
        std::max(4, trackH - 10));
    if (!dirtyRect.intersects(laneRect.toAlignedRect().adjusted(0, -2, 0, 2))) {
      continue;
    }
    QColor laneFill = theme.accent.lighter(120);
    laneFill.setAlpha(38);
    p.fillRect(laneRect, laneFill);
  }

  if (impl_->draggingMarker_) {
    const double targetFrame = std::clamp(
        impl_->dragMarkerTargetFrame_, 0.0,
        std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)));
    const qreal targetX = static_cast<qreal>(targetFrame * ppf - xOffset);
    if (targetX >= dirtyRect.left() - 12 && targetX <= dirtyRect.right() + 12) {
      QColor guideColor = theme.accent.lighter(135);
      guideColor.setAlpha(180);
      p.setPen(QPen(guideColor, 1.25, Qt::DashLine));
      p.drawLine(QPointF(targetX, 0.0), QPointF(targetX, static_cast<qreal>(height())));
      if (!impl_->dragMarkerSnapLabel_.isEmpty()) {
        const QString label = impl_->dragMarkerSnapLabel_;
        const QFontMetrics fm = p.fontMetrics();
        const int labelW = fm.horizontalAdvance(label) + 12;
        const int labelX = std::clamp(
            static_cast<int>(std::round(targetX + 8.0)), 8,
            std::max(8, width() - labelW - 8));
        const QRect labelRect(labelX, 6, labelW, fm.height() + 6);
        QColor labelBg = theme.background;
        labelBg.setAlpha(200);
        p.setPen(QPen(guideColor.darker(120), 1.0));
        p.setBrush(labelBg);
        p.drawRoundedRect(labelRect, 4, 4);
        p.setPen(guideColor.lighter(145));
        p.drawText(labelRect.adjusted(6, 0, -6, 0),
                   Qt::AlignVCenter | Qt::AlignLeft, label);
      }
    }
  }

  const QVector<KeyframeAreaVisual> keyframeAreas =
      collectKeyframeAreas(impl_->keyframeMarkers_, impl_->trackHeights_,
                           impl_->trackTops_, ppf, xOffset, yOffset);
  for (int i = 0; i < keyframeAreas.size(); ++i) {
    const auto &area = keyframeAreas[i];
    if (!dirtyRect.intersects(area.bodyRect.toAlignedRect().adjusted(-2, -2, 2, 2))) {
      continue;
    }
    const bool isHovered = i == impl_->hoverAreaIndex_;
    const bool isSelected = area.startMarkerIndex >= 0 &&
                            area.startMarkerIndex < impl_->keyframeMarkers_.size() &&
                            impl_->keyframeMarkers_[area.startMarkerIndex].selected;
    QColor fill = isSelected ? theme.accent.lighter(150) : QColor(247, 204, 83);
    fill.setAlpha(isHovered ? 110 : 56);
    QColor border = isSelected ? theme.accent.lighter(138) : theme.border.lighter(112);
    border.setAlpha(isHovered ? 235 : 150);
    p.setPen(QPen(border, isHovered ? 2.0 : 1.0));
    p.setBrush(fill);
    p.drawRoundedRect(area.bodyRect, 3.0, 3.0);
    if (isSelected) {
      QColor innerGlow = theme.accent.lighter(160);
      innerGlow.setAlpha(isHovered ? 58 : 34);
      p.setPen(QPen(innerGlow, 1.0));
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(area.bodyRect.adjusted(1.0, 1.0, -1.0, -1.0), 2.0, 2.0);
    }
    QColor edge = border;
    edge.setAlpha(220);
    p.setPen(QPen(edge, isHovered ? 2.4 : 2.0));
    p.drawLine(area.leftHandleRect.center().x(), area.bodyRect.top() + 2.0,
               area.leftHandleRect.center().x(), area.bodyRect.bottom() - 2.0);
    p.drawLine(area.rightHandleRect.center().x(), area.bodyRect.top() + 2.0,
               area.rightHandleRect.center().x(), area.bodyRect.bottom() - 2.0);
    if (isHovered) {
      QColor hoverGlow = theme.accent.lighter(150);
      hoverGlow.setAlpha(70);
      p.setPen(Qt::NoPen);
      p.setBrush(hoverGlow);
      p.drawRoundedRect(area.bodyRect.adjusted(-1.5, -1.5, 1.5, 1.5), 3.5, 3.5);
      QColor handleGlow = theme.text.lighter(150);
      handleGlow.setAlpha(80);
      p.setBrush(handleGlow);
      p.drawEllipse(QPointF(area.leftHandleRect.center().x(), area.bodyRect.center().y()), 2.2, 2.2);
      p.drawEllipse(QPointF(area.rightHandleRect.center().x(), area.bodyRect.center().y()), 2.2, 2.2);
    }
  }

  const int nearestMarkerIndex =
      nearestMarkerIndexToCurrentFrame(impl_->keyframeMarkers_, impl_->currentFrame_);

  for (int markerIndex = 0; markerIndex < impl_->keyframeMarkers_.size();
       ++markerIndex) {
    const auto &marker = impl_->keyframeMarkers_[markerIndex];
    if (marker.trackIndex < 0 ||
        marker.trackIndex >= impl_->trackHeights_.size()) {
      continue;
    }
    const QPointF center =
        markerCenterFor(marker, impl_->trackHeights_, impl_->trackTops_, ppf,
                        xOffset, yOffset);
    if (!dirtyRect.adjusted(-8, -8, 8, 8).contains(center.toPoint())) {
      continue;
    }
    const bool isHovered = markerIndex == impl_->hoverMarkerIndex_;
    const bool atCurrentFrame = markerAtCurrentFrame(marker, impl_->currentFrame_);
    const bool nearestToCurrent = markerIndex == nearestMarkerIndex;
    const int size = marker.selectedLayer ? (marker.laneCount > 1 ? 6 : 7)
                                          : (marker.laneCount > 1 ? 5 : 6);
    const QRectF diamondRect(center.x() - size, center.y() - size, size * 2.0,
                             size * 2.0);
    const QPolygonF markerShape =
        keyframeShapePolygon(diamondRect, marker.interpolation);
    const QRectF coreRect(diamondRect.center().x() - size * 0.18,
                          diamondRect.center().y() - size * 0.18,
                          size * 0.36, size * 0.36);
    if (atCurrentFrame) {
      const qreal haloRadius = marker.selectedLayer ? 9.0 : 7.0;
      QColor haloColor = marker.selectedLayer ? theme.accent.lighter(120)
                                              : marker.color.lighter(120);
      haloColor.setAlpha(marker.selectedLayer ? 86 : 62);
      p.setPen(Qt::NoPen);
      p.setBrush(haloColor);
      p.drawEllipse(center, haloRadius, haloRadius);
    }
    if (nearestToCurrent && !atCurrentFrame) {
      QColor nearestGlow = theme.text.lighter(140);
      nearestGlow.setAlpha(42);
      p.setPen(Qt::NoPen);
      p.setBrush(nearestGlow);
      p.drawEllipse(center, 5.0, 5.0);
    }
    if (marker.laneCount > 1) {
      QColor stackFill = marker.selectedLayer ? theme.accent.darker(135)
                                              : marker.color.darker(145);
      stackFill.setAlpha(marker.selected ? 170 : 115);
      QColor stackStroke = theme.background.darker(180);
      stackStroke.setAlpha(marker.selected ? 210 : 150);
      p.setPen(QPen(stackStroke, 0.8));
      p.setBrush(stackFill);
      p.drawPolygon(keyframeShapePolygon(
          diamondRect.translated(2.2, -2.2), marker.interpolation));
    }
    if (marker.selected) {
      p.setPen(QPen(theme.accent.lighter(isHovered ? 178 : 160),
                    isHovered ? 2.8 : 2.2));
      p.setBrush(Qt::NoBrush);
      p.drawPolygon(markerShape);
      p.setPen(QPen(theme.background.darker(175), 2.0));
      p.drawPolygon(markerShape);
      p.setPen(QPen(theme.text.lighter(125), 1.0));
      p.setBrush(theme.accent.lighter(isHovered ? 150 : 140));
      p.drawPolygon(markerShape);
    } else if (marker.selectedLayer) {
      p.setPen(QPen(atCurrentFrame ? theme.accent.lighter(148)
                                   : theme.background.darker(175),
                    atCurrentFrame ? 2.6 : 2.1));
      p.setBrush(Qt::NoBrush);
      p.drawPolygon(markerShape);
      p.setPen(QPen(atCurrentFrame ? theme.accent.lighter(130)
                                   : theme.text.lighter(110),
                    atCurrentFrame ? 1.3 : 1.0));
      p.setBrush(atCurrentFrame
                     ? theme.accent.lighter(isHovered ? 145 : 132)
                     : (isHovered ? theme.text.lighter(125)
                                  : theme.text.lighter(110)));
      p.drawPolygon(markerShape);
    } else {
      p.setPen(QPen(isHovered ? theme.text.lighter(150)
                               : theme.border.darker(160),
                    isHovered ? 1.7 : 1.0));
      p.setBrush(isHovered ? marker.color.lighter(115)
                           : (marker.eased ? marker.color.lighter(102)
                                           : marker.color));
      p.drawPolygon(markerShape);
    }
    if (isHovered && !marker.selected) {
      QColor hoverStroke = theme.accent.lighter(145);
      hoverStroke.setAlpha(80);
      p.setPen(QPen(hoverStroke, 1.0));
      p.setBrush(Qt::NoBrush);
      p.drawPolygon(markerShape);
    }
    if (marker.incomingEased || marker.outgoingEased) {
      const QColor leftColor =
          marker.incomingEased ? marker.color.lighter(128)
                               : marker.color.darker(140);
      const QColor rightColor =
          marker.outgoingEased ? marker.color.lighter(128)
                               : marker.color.darker(140);
      QColor leftFill = leftColor;
      QColor rightFill = rightColor;
      leftFill.setAlpha(marker.selected ? 255 : 230);
      rightFill.setAlpha(marker.selected ? 255 : 230);
      QPolygonF leftHalf;
      leftHalf << QPointF(diamondRect.center().x(), diamondRect.top())
               << QPointF(diamondRect.center().x(), diamondRect.bottom())
               << QPointF(diamondRect.left(), diamondRect.center().y());
      QPolygonF rightHalf;
      rightHalf << QPointF(diamondRect.center().x(), diamondRect.top())
                << QPointF(diamondRect.right(), diamondRect.center().y())
                << QPointF(diamondRect.center().x(), diamondRect.bottom());
      p.setPen(Qt::NoPen);
      p.setBrush(leftFill);
      p.drawPolygon(leftHalf);
      p.setBrush(rightFill);
      p.drawPolygon(rightHalf);
      QColor divider = marker.selectedLayer ? theme.accent.lighter(130)
                                            : theme.text.lighter(120);
      divider.setAlpha(marker.selected ? 200 : 140);
      p.setPen(QPen(divider, marker.selected ? 1.2 : 1.0));
      p.setBrush(Qt::NoBrush);
      p.drawLine(QPointF(diamondRect.center().x(), diamondRect.top() + 1.0),
                 QPointF(diamondRect.center().x(), diamondRect.bottom() - 1.0));
    }
    if (marker.labelColor.isValid()) {
      QColor labelColor = marker.labelColor;
      labelColor.setAlpha(marker.selected ? 255 : (marker.selectedLayer ? 220 : 200));
      const QRectF tagRect = diamondRect.adjusted(0.5, 0.5, -0.5, -0.5);
      const qreal tagHeight = std::max<qreal>(2.0, size * 0.22);
      const QRectF stripe(tagRect.left(), tagRect.top(), tagRect.width(), tagHeight);
      p.setPen(Qt::NoPen);
      p.setBrush(labelColor);
      p.drawRoundedRect(stripe, 1.2, 1.2);
      QColor outline = labelColor.darker(140);
      outline.setAlpha(marker.selected ? 210 : 150);
      p.setPen(QPen(outline, marker.selected ? 1.1 : 0.9));
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(tagRect.adjusted(0.4, 0.4, -0.4, -0.4), 1.8, 1.8);
    }
    if (marker.anchor != ArtifactCore::KeyFrame::Anchor::Absolute) {
      QColor anchorColor = marker.selectedLayer ? theme.accent.lighter(145)
                                                : theme.text.lighter(135);
      anchorColor.setAlpha(marker.selected ? 230 : 175);
      p.setPen(QPen(anchorColor, marker.selected ? 1.6 : 1.25,
                    marker.anchor == ArtifactCore::KeyFrame::Anchor::StretchWithLayer
                        ? Qt::SolidLine
                        : Qt::DashLine,
                    Qt::RoundCap, Qt::RoundJoin));
      p.setBrush(Qt::NoBrush);
      const qreal y = diamondRect.bottom() + 2.2;
      if (marker.anchor == ArtifactCore::KeyFrame::Anchor::LockToIn ||
          marker.anchor == ArtifactCore::KeyFrame::Anchor::StretchWithLayer) {
        p.drawLine(QPointF(diamondRect.left() - 1.0, y - 2.3),
                   QPointF(diamondRect.left() - 1.0, y + 2.3));
      }
      if (marker.anchor == ArtifactCore::KeyFrame::Anchor::LockToOut ||
          marker.anchor == ArtifactCore::KeyFrame::Anchor::StretchWithLayer) {
        p.drawLine(QPointF(diamondRect.right() + 1.0, y - 2.3),
                   QPointF(diamondRect.right() + 1.0, y + 2.3));
      }
      if (marker.anchor == ArtifactCore::KeyFrame::Anchor::StretchWithLayer) {
        p.drawLine(QPointF(diamondRect.left() - 1.0, y),
                   QPointF(diamondRect.right() + 1.0, y));
      }
    }
    if (atCurrentFrame) {
      QColor coreFill = marker.selectedLayer ? theme.accent.lighter(165)
                                             : theme.text.lighter(150);
      coreFill.setAlpha(marker.selected ? 255 : 230);
      p.setPen(QPen(theme.background.darker(180), 1.0));
      p.setBrush(coreFill);
      p.drawEllipse(center, marker.selectedLayer ? 2.8 : 2.4,
                    marker.selectedLayer ? 2.8 : 2.4);
    }
    if (marker.bezier) {
      QColor bezierColor = marker.selectedLayer ? theme.accent.lighter(135)
                                                : marker.color.lighter(135);
      bezierColor.setAlpha(marker.selected ? 190 : 130);
      p.setPen(QPen(bezierColor, marker.selected ? 1.7 : 1.3));
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(diamondRect.adjusted(-2.0, -2.0, 2.0, 2.0), 2, 2);
    }
    if (marker.roving) {
      QColor rovingColor = marker.selectedLayer ? theme.accent.lighter(140)
                                                : theme.text.lighter(145);
      rovingColor.setAlpha(marker.selected ? 180 : 120);
      p.setPen(QPen(rovingColor, marker.selected ? 1.6 : 1.2, Qt::DashLine));
      p.setBrush(Qt::NoBrush);
      p.drawEllipse(center, size + 5.0, size + 5.0);
    }
    if (nearestToCurrent && !atCurrentFrame && !marker.selected) {
      QColor guideColor = marker.selectedLayer ? theme.accent.lighter(135)
                                               : marker.color.lighter(120);
      guideColor.setAlpha(marker.selectedLayer ? 110 : 78);
      p.setPen(QPen(guideColor, marker.selectedLayer ? 1.6 : 1.2));
      p.setBrush(Qt::NoBrush);
      p.drawEllipse(center, size + 4.0, size + 4.0);
    }
  }

  if (impl_->marqueeSelecting_ && !impl_->marqueeSelectionRect_.isNull()) {
    QColor fill = theme.accent;
    fill.setAlpha(32);
    QColor stroke = theme.accent.lighter(120);
    stroke.setAlpha(180);
    p.setPen(QPen(stroke, 1.0, Qt::DashLine));
    p.setBrush(fill);
    p.drawRect(impl_->marqueeSelectionRect_);
  }

  drawPlayhead(p);
}

void ArtifactTimelineTrackPainterView::drawPlayhead(QPainter& p) const {
  const double ppf = impl_->pixelsPerFrame_;
  const double xOffset = impl_->horizontalOffset_;
  const double frame = impl_->currentFrame_;
  const qreal playheadX = static_cast<qreal>(frame * ppf - xOffset);
  if (playheadX < -12.0 || playheadX > width() + 12.0) {
    return;
  }

  const auto drawPlayheadProperty = property("timelineDrawPlayhead");
  if (drawPlayheadProperty.isValid() && !drawPlayheadProperty.toBool()) {
    return;
  }

  // Track view keeps the stem only so the playhead head is drawn once in the
  // timeline chrome instead of repeating in each pane.
  TimelinePlayheadDraw::drawPlayhead(
      p, playheadX, 0.0, static_cast<qreal>(height()) - 1.0, false);
}

void ArtifactTimelineTrackPainterView::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::MiddleButton) {
    impl_->panning_ = true;
    impl_->lastPanPoint_ = event->position().toPoint();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton) {
    setFocus(Qt::MouseFocusReason);
    impl_->pendingBackgroundPress_ = false;
    impl_->marqueeSelecting_ = false;
    impl_->marqueeSelectionRect_ = QRect();
    impl_->marqueeAnchorSelectionKeys_.clear();
    impl_->pendingMarkerSingleClick_ = false;
    impl_->pendingMarkerSingleClickKey_.clear();
    impl_->pendingMarkerSingleClickLabel_.clear();
    impl_->pendingMarkerSingleClickFrame_ = 0.0;
    const double mouseX = event->position().x();
    const double mouseY = event->position().y();

    if (impl_->activeTool_ == ToolType::ScrubPreview &&
        !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier))) {
      impl_->scrubDragging_ = true;
      impl_->scrubLastFrame_ = impl_->currentFrame_;
      setCursor(Qt::ClosedHandCursor);
      event->accept();
      return;
    }

    const auto markerHit =
        hitTestMarkers(impl_->keyframeMarkers_, impl_->trackHeights_,
                       impl_->trackTops_, mouseX, mouseY,
                       impl_->pixelsPerFrame_, impl_->horizontalOffset_,
                       impl_->verticalOffset_);
    if (markerHit.markerIndex >= 0) {
      const auto &marker = impl_->keyframeMarkers_[markerHit.markerIndex];
      const double frame =
          std::clamp(marker.frame, 0.0,
                     std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)));
      const QString key =
          keyframeSelectionKey(marker.layerId, marker.propertyPath,
                               static_cast<qint64>(std::llround(marker.frame)));
      const bool clickedWasSelected = impl_->selectedMarkerKeys_.contains(key);
      const int previousSelectionCount = impl_->selectedMarkerKeys_.size();
      bool selectionChanged = false;
      enum class MarkerSelectionAction {
        None,
        Replace,
        Add,
        ToggleOff,
      };
      MarkerSelectionAction selectionAction = MarkerSelectionAction::None;
      if (event->modifiers() & Qt::ControlModifier) {
        QSet<QString> nextSelection = impl_->selectedMarkerKeys_;
        if (nextSelection.contains(key)) {
          nextSelection.remove(key);
          selectionAction = MarkerSelectionAction::ToggleOff;
        } else {
          nextSelection.insert(key);
          selectionAction = MarkerSelectionAction::Add;
        }
        selectionChanged =
            applyMarkerSelectionSet(impl_->keyframeMarkers_,
                                    impl_->selectedMarkerKeys_, nextSelection);
      } else if (event->modifiers() & Qt::ShiftModifier) {
        QSet<QString> nextSelection = impl_->selectedMarkerKeys_;
        nextSelection.insert(key);
        selectionAction = MarkerSelectionAction::Add;
        selectionChanged =
            applyMarkerSelectionSet(impl_->keyframeMarkers_,
                                    impl_->selectedMarkerKeys_, nextSelection);
      } else {
        if (!clickedWasSelected) {
          QSet<QString> nextSelection;
          nextSelection.insert(key);
          selectionAction = MarkerSelectionAction::Replace;
          selectionChanged =
              applyMarkerSelectionSet(impl_->keyframeMarkers_,
                                      impl_->selectedMarkerKeys_, nextSelection);
        }
      }
      if (selectionChanged ||
          impl_->selectedMarkerKeys_.size() != previousSelectionCount) {
        Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
      }
      if (selectionChanged) {
        QString debugAction;
        switch (selectionAction) {
        case MarkerSelectionAction::Replace:
          debugAction = QStringLiteral("Selected");
          break;
        case MarkerSelectionAction::Add:
          debugAction = QStringLiteral("Added");
          break;
        case MarkerSelectionAction::ToggleOff:
          debugAction = QStringLiteral("Deselected");
          break;
        case MarkerSelectionAction::None:
        default:
          break;
        }
        if (!debugAction.isEmpty()) {
          Q_EMIT timelineDebugMessage(
              QStringLiteral("%1 keyframe at F%2 for %3")
                  .arg(debugAction)
                  .arg(QString::number(frame, 'f', 1))
                  .arg(marker.label));
        }
      }
      clipSelected(QString(), marker.layerId);
      update();
      const bool modifiedSelection =
          event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
      if (!modifiedSelection && impl_->selectedMarkerKeys_.contains(key)) {
        const bool keepGroupForPotentialDrag =
            clickedWasSelected && impl_->selectedMarkerKeys_.size() > 1;
        impl_->dragMarkerIndex_ = markerHit.markerIndex;
        impl_->dragMarkerStartPoint_ = event->position().toPoint();
        impl_->dragMarkerOrigFrame_ = marker.frame;
        impl_->dragMarkerSelectionIndices_ =
            selectedMarkerIndices(impl_->keyframeMarkers_);
        impl_->dragMarkerSelectionOrigFrames_.clear();
        impl_->dragMarkerSelectionOrigFrames_.reserve(
            impl_->dragMarkerSelectionIndices_.size());
        for (const int selectedIndex : impl_->dragMarkerSelectionIndices_) {
          if (selectedIndex < 0 ||
              selectedIndex >= impl_->keyframeMarkers_.size()) {
            impl_->dragMarkerSelectionOrigFrames_.push_back(0.0);
            continue;
          }
          impl_->dragMarkerSelectionOrigFrames_.push_back(
              impl_->keyframeMarkers_[selectedIndex].frame);
        }
        if (keepGroupForPotentialDrag) {
          impl_->pendingMarkerSingleClick_ = true;
          impl_->pendingMarkerSingleClickKey_ = key;
          impl_->pendingMarkerSingleClickLabel_ = marker.label;
          impl_->pendingMarkerSingleClickFrame_ = frame;
        }
        impl_->draggingMarker_ = false;
      } else {
        impl_->dragMarkerIndex_ = -1;
        impl_->dragMarkerSelectionIndices_.clear();
        impl_->dragMarkerSelectionOrigFrames_.clear();
        impl_->pendingMarkerSingleClick_ = false;
        impl_->pendingMarkerSingleClickKey_.clear();
        impl_->pendingMarkerSingleClickLabel_.clear();
        impl_->pendingMarkerSingleClickFrame_ = 0.0;
        impl_->draggingMarker_ = false;
      }
      event->accept();
      return;
    }
    impl_->draggingMarker_ = false;
    impl_->dragMarkerIndex_ = -1;
    impl_->dragMarkerSelectionIndices_.clear();
    impl_->dragMarkerSelectionOrigFrames_.clear();
    impl_->pendingMarkerSingleClick_ = false;
    impl_->pendingMarkerSingleClickKey_.clear();
    impl_->pendingMarkerSingleClickLabel_.clear();
    impl_->pendingMarkerSingleClickFrame_ = 0.0;
    const auto hit =
        hitTestClips(impl_->clips_, impl_->trackHeights_, impl_->trackTops_,
                     mouseX, mouseY, impl_->pixelsPerFrame_,
                     impl_->horizontalOffset_, impl_->verticalOffset_);
    if (hit.mode != DragMode::None) {
      if (!(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) &&
          !impl_->selectedMarkerKeys_.isEmpty()) {
        clearKeyframeSelection();
      }
      impl_->dragMode_ = hit.mode;
      impl_->dragClipIndex_ = hit.clipIndex;
      impl_->dragStartX_ = mouseX;
      impl_->dragOrigStartFrame_ = impl_->clips_[hit.clipIndex].startFrame;
      impl_->dragOrigDuration_ = impl_->clips_[hit.clipIndex].durationFrame;
      impl_->dragOrigTrimMinStartFrame_ =
          impl_->clips_[hit.clipIndex].trimMinStartFrame;
      impl_->dragOrigTrimMaxEndFrame_ =
          impl_->clips_[hit.clipIndex].trimMaxEndFrame;
      const auto &clip = impl_->clips_[hit.clipIndex];
      clipSelected(clip.clipId, clip.layerId);
      if (hit.mode == DragMode::MoveBody)
        setCursor(Qt::ClosedHandCursor);
      event->accept();
      return;
    }
    impl_->pendingBackgroundPress_ = true;
    impl_->backgroundPressPoint_ = event->position().toPoint();
    impl_->backgroundPressModifiers_ = event->modifiers();
    impl_->marqueeAnchorSelectionKeys_ = impl_->selectedMarkerKeys_;
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void ArtifactTimelineTrackPainterView::mouseMoveEvent(QMouseEvent *event) {
  const double mouseX = event->position().x();
  const double mouseY = event->position().y();

  if (impl_->scrubDragging_ && (event->buttons() & Qt::LeftButton)) {
    const double ppf = std::max(0.001, impl_->pixelsPerFrame_);
    const double frame = (mouseX + impl_->horizontalOffset_) / ppf;
    const double clamped = std::clamp(
        frame, 0.0, std::max(0.0, impl_->durationFrames_ - 1.0));
    if (std::abs(clamped - impl_->scrubLastFrame_) >= 0.0001) {
      impl_->scrubLastFrame_ = clamped;
      if (auto *app = Artifact::ApplicationService::instance()) {
        if (auto *projectService = app->projectService()) {
          if (auto comp = projectService->currentComposition().lock()) {
            ArtifactCore::globalEventBus().publish<FrameChangedEvent>(
                FrameChangedEvent{comp->id().toString(),
                                  static_cast<qint64>(std::llround(clamped))});
          }
        }
      }
    }
    event->accept();
    return;
  }

  const double ppf = impl_->pixelsPerFrame_;
  const auto keyframeAreas = collectKeyframeAreas(
      impl_->keyframeMarkers_, impl_->trackHeights_, impl_->trackTops_, ppf,
      impl_->horizontalOffset_, impl_->verticalOffset_);
  const auto markerHit = hitTestMarkers(
      impl_->keyframeMarkers_, impl_->trackHeights_, impl_->trackTops_, mouseX,
      mouseY, ppf, impl_->horizontalOffset_, impl_->verticalOffset_);
  const auto areaHit = hitTestKeyframeAreas(keyframeAreas, mouseX, mouseY);

  if ((event->buttons() & Qt::LeftButton) && impl_->pendingBackgroundPress_ &&
      impl_->dragMode_ == DragMode::None && impl_->dragMarkerIndex_ < 0) {
    const QPoint currentPos = event->position().toPoint();
    const int dragDistance =
        (currentPos - impl_->backgroundPressPoint_).manhattanLength();
    if (!impl_->marqueeSelecting_ &&
        dragDistance >= QApplication::startDragDistance()) {
      impl_->marqueeSelecting_ = true;
      setCursor(Qt::CrossCursor);
      updateHoverToolTip(this, event->globalPosition().toPoint(), QString(),
                         impl_->hoverToolTipText_);
    }
    if (impl_->marqueeSelecting_) {
      impl_->marqueeSelectionRect_ =
          normalizedSelectionRect(impl_->backgroundPressPoint_, currentPos);
      QSet<QString> nextSelection = markerKeysInSelectionRect(
          impl_->keyframeMarkers_, impl_->trackHeights_, impl_->trackTops_,
          impl_->pixelsPerFrame_, impl_->horizontalOffset_,
          impl_->verticalOffset_, impl_->marqueeSelectionRect_);
      if (impl_->backgroundPressModifiers_ & Qt::ControlModifier) {
        QSet<QString> toggled = impl_->marqueeAnchorSelectionKeys_;
        for (const auto &key : nextSelection) {
          if (toggled.contains(key)) {
            toggled.remove(key);
          } else {
            toggled.insert(key);
          }
        }
        nextSelection = std::move(toggled);
      } else if (impl_->backgroundPressModifiers_ & Qt::ShiftModifier) {
        nextSelection.unite(impl_->marqueeAnchorSelectionKeys_);
      }
      if (applyMarkerSelectionSet(impl_->keyframeMarkers_,
                                  impl_->selectedMarkerKeys_, nextSelection)) {
        Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
      }
      update();
      event->accept();
      return;
    }
  }

  if ((event->buttons() & Qt::LeftButton) && impl_->dragMarkerIndex_ >= 0) {
    const QPoint currentPos = event->position().toPoint();
    const int dragDistance =
        (currentPos - impl_->dragMarkerStartPoint_).manhattanLength();
    if (!impl_->draggingMarker_ &&
        dragDistance >= QApplication::startDragDistance()) {
      impl_->draggingMarker_ = true;
      impl_->pendingMarkerSingleClick_ = false;
      impl_->pendingMarkerSingleClickKey_.clear();
      impl_->pendingMarkerSingleClickLabel_.clear();
      impl_->pendingMarkerSingleClickFrame_ = 0.0;
      setCursor(Qt::ClosedHandCursor);
    }
    if (impl_->draggingMarker_) {
      const double rawDeltaFrames =
          (event->position().x() - impl_->dragMarkerStartPoint_.x()) /
          std::max<double>(0.001, static_cast<double>(impl_->pixelsPerFrame_));
      QString snapLabel;
      const double targetFrame = std::clamp(
          snappedKeyframeDragTargetFrame(
              impl_->dragMarkerOrigFrame_, rawDeltaFrames, impl_->currentFrame_,
              event->modifiers(), &snapLabel),
          0.0, std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)));
      const double deltaFrames = targetFrame - impl_->dragMarkerOrigFrame_;
      const int dragCollisionCount = keyframeDragCollisionCount(
          impl_->keyframeMarkers_, impl_->dragMarkerSelectionIndices_,
          impl_->dragMarkerSelectionOrigFrames_, deltaFrames,
          std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)));
      if (dragCollisionCount > 0) {
        if (snapLabel.isEmpty()) {
          snapLabel = formatKeyframeCollisionLabel(dragCollisionCount);
        } else {
          snapLabel += QStringLiteral(", %1")
                           .arg(formatKeyframeCollisionLabel(dragCollisionCount));
        }
      }
      impl_->dragMarkerTargetFrame_ = targetFrame;
      impl_->dragMarkerSnapLabel_ = snapLabel;
      updateHoverToolTip(
          this, event->globalPosition().toPoint(),
          formatKeyframeDragTooltip(static_cast<int>(impl_->dragMarkerSelectionIndices_.size()),
                                    deltaFrames, targetFrame, snapLabel),
          impl_->hoverToolTipText_);
      QRectF dirtyRect;
      bool hasDirty = false;
      for (int i = 0; i < impl_->dragMarkerSelectionIndices_.size(); ++i) {
        const int selectedIndex = impl_->dragMarkerSelectionIndices_[i];
        if (selectedIndex < 0 || selectedIndex >= impl_->keyframeMarkers_.size() ||
            i >= impl_->dragMarkerSelectionOrigFrames_.size()) {
          continue;
        }
        const auto originalMarker = impl_->keyframeMarkers_[selectedIndex];
        const double originalFrame = impl_->dragMarkerSelectionOrigFrames_[i];
        const double newFrame = std::clamp(
            originalFrame + deltaFrames, 0.0,
            std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)));
        auto &mutableMarker = impl_->keyframeMarkers_[selectedIndex];
        mutableMarker.frame = newFrame;
        const QRectF originalRect = markerHitRectFor(
            originalMarker, impl_->trackHeights_, impl_->trackTops_,
            impl_->pixelsPerFrame_, impl_->horizontalOffset_,
            impl_->verticalOffset_);
        const QRectF updatedRect = markerHitRectFor(
            mutableMarker, impl_->trackHeights_, impl_->trackTops_,
            impl_->pixelsPerFrame_, impl_->horizontalOffset_,
            impl_->verticalOffset_);
        if (originalRect.isValid()) {
          dirtyRect = hasDirty ? dirtyRect.united(originalRect) : originalRect;
          hasDirty = true;
        }
        if (updatedRect.isValid()) {
          dirtyRect = hasDirty ? dirtyRect.united(updatedRect) : updatedRect;
          hasDirty = true;
        }
      }
      if (!dirtyRect.isValid()) {
        const double newFrame = std::clamp(
            impl_->dragMarkerOrigFrame_ + deltaFrames, 0.0,
            std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)));
        const qreal oldX = impl_->dragMarkerOrigFrame_ * impl_->pixelsPerFrame_ -
                           impl_->horizontalOffset_;
        const qreal newX =
            newFrame * impl_->pixelsPerFrame_ - impl_->horizontalOffset_;
        dirtyRect = QRectF(QPointF(std::min(oldX, newX) - 12.0, 0.0),
                           QPointF(std::max(oldX, newX) + 12.0, height()));
      }
      update(dirtyRect.adjusted(-6.0, -6.0, 6.0, 6.0).toAlignedRect());
      event->accept();
      return;
    }
  }

  if (impl_->dragMode_ != DragMode::None && impl_->dragClipIndex_ >= 0) {
    const auto oldClip = impl_->clips_[impl_->dragClipIndex_];
    const double deltaFrames =
        (mouseX - impl_->dragStartX_) / std::max(0.001, ppf);
    auto &clip = impl_->clips_[impl_->dragClipIndex_];
    switch (impl_->dragMode_) {
    case DragMode::MoveBody:
      clip.startFrame = impl_->dragOrigStartFrame_ + deltaFrames;
      if (clip.hasTrimSourceRange) {
        const double rangeDelta = clip.startFrame - impl_->dragOrigStartFrame_;
        clip.trimMinStartFrame = impl_->dragOrigTrimMinStartFrame_ + rangeDelta;
        clip.trimMaxEndFrame = impl_->dragOrigTrimMaxEndFrame_ + rangeDelta;
      }
      break;
    case DragMode::ResizeLeft: {
      const double end = impl_->dragOrigStartFrame_ + impl_->dragOrigDuration_;
      const double minStart = clip.hasTrimSourceRange
                                  ? impl_->dragOrigTrimMinStartFrame_
                                  : 0.0;
      clip.startFrame = std::clamp(impl_->dragOrigStartFrame_ + deltaFrames,
                                   minStart, end - 1.0);
      clip.durationFrame = end - clip.startFrame;
      break;
    }
    case DragMode::ResizeRight: {
      const double maxEnd = clip.hasTrimSourceRange
                                ? impl_->dragOrigTrimMaxEndFrame_
                                : std::numeric_limits<double>::max();
      const double newEnd = std::clamp(
          impl_->dragOrigStartFrame_ + impl_->dragOrigDuration_ + deltaFrames,
          clip.startFrame + 1.0, maxEnd);
      clip.durationFrame = std::max(1.0, newEnd - clip.startFrame);
      break;
    }
    default:
      break;
    }

    // Debug message emission
    const QString status =
        QStringLiteral("Layer: %1 | Start: %2 | Dur: %3")
            .arg(clip.title.isEmpty() ? clip.clipId : clip.title)
            .arg(QString::number(clip.startFrame, 'f', 1))
            .arg(QString::number(clip.durationFrame, 'f', 1));
    Q_EMIT timelineDebugMessage(status);

    const QRectF dirtyRect =
        clipRectFor(oldClip, impl_->trackHeights_, impl_->trackTops_, ppf,
                    impl_->horizontalOffset_, impl_->verticalOffset_)
            .united(sourceClipRectFor(oldClip, impl_->trackHeights_,
                                      impl_->trackTops_, ppf,
                                      impl_->horizontalOffset_,
                                      impl_->verticalOffset_))
            .united(clipRectFor(clip, impl_->trackHeights_, impl_->trackTops_, ppf,
                                impl_->horizontalOffset_,
                                impl_->verticalOffset_))
            .united(sourceClipRectFor(clip, impl_->trackHeights_,
                                      impl_->trackTops_, ppf,
                                      impl_->horizontalOffset_,
                                      impl_->verticalOffset_));
    update(dirtyRect.adjusted(-2.0, -2.0, 2.0, 2.0).toAlignedRect());
    event->accept();
    return;
  }

  const auto hit =
      hitTestClips(impl_->clips_, impl_->trackHeights_, impl_->trackTops_,
                   mouseX, mouseY, ppf, impl_->horizontalOffset_,
                   impl_->verticalOffset_);
  const bool clipHoverChanged = (hit.clipIndex != impl_->hoverClipIndex_ ||
                                 hit.mode != impl_->hoverEdge_);
  const auto oldHoverClipIndex = impl_->hoverClipIndex_;
  const auto oldHoverEdge = impl_->hoverEdge_;
  const int oldHoverMarkerIndex = impl_->hoverMarkerIndex_;
  const int oldHoverAreaIndex = impl_->hoverAreaIndex_;
  impl_->hoverClipIndex_ = hit.clipIndex;
  impl_->hoverEdge_ = hit.mode;
  impl_->hoverMarkerIndex_ = markerHit.markerIndex;
  impl_->hoverAreaIndex_ = areaHit.areaIndex;

  if (impl_->hoverMarkerIndex_ >= 0) {
    setCursor(Qt::PointingHandCursor);
  } else if (impl_->hoverAreaIndex_ >= 0) {
    setCursor(areaHit.part == KeyframeAreaHitPart::Body ? Qt::OpenHandCursor
                                                        : Qt::SizeHorCursor);
  } else {
    switch (hit.mode) {
    case DragMode::ResizeLeft:
    case DragMode::ResizeRight:
      setCursor(Qt::SizeHorCursor);
      break;
    case DragMode::MoveBody:
      setCursor(Qt::OpenHandCursor);
      break;
    default:
      setCursor(Qt::ArrowCursor);
      break;
    }
  }

  if (!impl_->panning_) {
    QString tooltipText;
    const int nearestMarkerIndex =
        nearestMarkerIndexToCurrentFrame(impl_->keyframeMarkers_, impl_->currentFrame_);
    if (impl_->hoverMarkerIndex_ >= 0 &&
        impl_->hoverMarkerIndex_ < impl_->keyframeMarkers_.size()) {
      tooltipText = formatMarkerTooltip(
          impl_->keyframeMarkers_[impl_->hoverMarkerIndex_], impl_->currentFrame_,
          true, impl_->hoverMarkerIndex_ == nearestMarkerIndex);
    } else if (impl_->hoverClipIndex_ >= 0 &&
               impl_->hoverClipIndex_ < impl_->clips_.size()) {
      tooltipText = formatClipTooltip(impl_->clips_[impl_->hoverClipIndex_]);
    }
    updateHoverToolTip(this, event->globalPosition().toPoint(), tooltipText,
                       impl_->hoverToolTipText_);
  }

  QRectF dirtyRect;
  bool hasDirty = false;
  if (clipHoverChanged) {
    if (oldHoverClipIndex >= 0 && oldHoverClipIndex < impl_->clips_.size()) {
      dirtyRect =
          clipRectFor(impl_->clips_[oldHoverClipIndex], impl_->trackHeights_,
                      impl_->trackTops_, ppf, impl_->horizontalOffset_,
                      impl_->verticalOffset_);
      hasDirty = true;
    }
    if (hit.clipIndex >= 0 && hit.clipIndex < impl_->clips_.size()) {
      const QRectF rect =
          clipRectFor(impl_->clips_[hit.clipIndex], impl_->trackHeights_,
                      impl_->trackTops_, ppf, impl_->horizontalOffset_,
                      impl_->verticalOffset_);
      dirtyRect = hasDirty ? dirtyRect.united(rect) : rect;
      hasDirty = true;
    }
    if (!dirtyRect.isValid() && oldHoverEdge != hit.mode) {
      dirtyRect = QRectF(0.0, 0.0, width(), height());
      hasDirty = true;
    }
  }
  if (oldHoverMarkerIndex != impl_->hoverMarkerIndex_) {
    if (oldHoverMarkerIndex >= 0 &&
        oldHoverMarkerIndex < impl_->keyframeMarkers_.size()) {
      const QRectF rect = markerHitRectFor(
          impl_->keyframeMarkers_[oldHoverMarkerIndex], impl_->trackHeights_,
          impl_->trackTops_, ppf, impl_->horizontalOffset_,
          impl_->verticalOffset_);
      dirtyRect = hasDirty ? dirtyRect.united(rect) : rect;
      hasDirty = true;
    }
    if (impl_->hoverMarkerIndex_ >= 0 &&
        impl_->hoverMarkerIndex_ < impl_->keyframeMarkers_.size()) {
      const QRectF rect =
          markerHitRectFor(impl_->keyframeMarkers_[impl_->hoverMarkerIndex_],
                           impl_->trackHeights_, impl_->trackTops_, ppf,
                           impl_->horizontalOffset_, impl_->verticalOffset_);
      dirtyRect = hasDirty ? dirtyRect.united(rect) : rect;
      hasDirty = true;
    }
  }
  if (oldHoverAreaIndex != impl_->hoverAreaIndex_) {
    if (oldHoverAreaIndex >= 0 && oldHoverAreaIndex < keyframeAreas.size()) {
      dirtyRect = hasDirty ? dirtyRect.united(keyframeAreas[oldHoverAreaIndex].bodyRect)
                           : keyframeAreas[oldHoverAreaIndex].bodyRect;
      hasDirty = true;
    }
    if (impl_->hoverAreaIndex_ >= 0 &&
        impl_->hoverAreaIndex_ < keyframeAreas.size()) {
      dirtyRect = hasDirty ? dirtyRect.united(keyframeAreas[impl_->hoverAreaIndex_].bodyRect)
                           : keyframeAreas[impl_->hoverAreaIndex_].bodyRect;
      hasDirty = true;
    }
  }
  if (hasDirty) {
    update(dirtyRect.adjusted(-2.0, -2.0, 2.0, 2.0).toAlignedRect());
  }
  if (impl_->panning_ && (event->buttons() & Qt::MiddleButton)) {
    const QPoint current = event->position().toPoint();
    const QPoint delta = current - impl_->lastPanPoint_;
    impl_->lastPanPoint_ = current;
    if (delta.x() != 0) {
      setHorizontalOffset(impl_->horizontalOffset_ -
                          static_cast<double>(delta.x()));
    }
    if (delta.y() != 0) {
      setVerticalOffset(std::max(0.0, impl_->verticalOffset_ -
                                          static_cast<double>(delta.y())));
    }
    event->accept();
    return;
  }
  QWidget::mouseMoveEvent(event);
}

void ArtifactTimelineTrackPainterView::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::MiddleButton && impl_->panning_) {
    impl_->panning_ = false;
    setCursor(Qt::ArrowCursor);
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton && impl_->scrubDragging_) {
    impl_->scrubDragging_ = false;
    setCursor(Qt::ArrowCursor);
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton && impl_->dragMarkerIndex_ >= 0 &&
      !impl_->draggingMarker_) {
    const bool pendingSingleClick = impl_->pendingMarkerSingleClick_;
    const QString singleClickKey = impl_->pendingMarkerSingleClickKey_;
    const QString singleClickLabel = impl_->pendingMarkerSingleClickLabel_;
    const double singleClickFrame = impl_->pendingMarkerSingleClickFrame_;
    impl_->dragMarkerIndex_ = -1;
    impl_->dragMarkerSelectionIndices_.clear();
    impl_->dragMarkerSelectionOrigFrames_.clear();
    impl_->pendingMarkerSingleClick_ = false;
    impl_->pendingMarkerSingleClickKey_.clear();
    impl_->pendingMarkerSingleClickLabel_.clear();
    impl_->pendingMarkerSingleClickFrame_ = 0.0;
    if (pendingSingleClick && !singleClickKey.isEmpty()) {
      QSet<QString> nextSelection;
      nextSelection.insert(singleClickKey);
      if (applyMarkerSelectionSet(impl_->keyframeMarkers_,
                                  impl_->selectedMarkerKeys_, nextSelection)) {
        Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
        Q_EMIT timelineDebugMessage(
            QStringLiteral("Selected keyframe at F%1 for %2")
                .arg(QString::number(singleClickFrame, 'f', 1))
                .arg(singleClickLabel));
      }
      update();
    }
    if (impl_->hoverMarkerIndex_ >= 0) {
      setCursor(Qt::PointingHandCursor);
    } else {
      setCursor(Qt::ArrowCursor);
    }
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton && impl_->marqueeSelecting_) {
    impl_->pendingBackgroundPress_ = false;
    impl_->marqueeSelecting_ = false;
    impl_->marqueeSelectionRect_ = QRect();
    impl_->marqueeAnchorSelectionKeys_.clear();
    setCursor(Qt::ArrowCursor);
    update();
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton && impl_->pendingBackgroundPress_) {
    impl_->pendingBackgroundPress_ = false;
    impl_->marqueeSelectionRect_ = QRect();
    impl_->marqueeAnchorSelectionKeys_.clear();
    clipDeselected();
    const bool modifiedSelectionIntent =
        impl_->backgroundPressModifiers_ &
        (Qt::ControlModifier | Qt::ShiftModifier);
    if (!modifiedSelectionIntent && !impl_->selectedMarkerKeys_.isEmpty()) {
      clearKeyframeSelection();
    }
    if (modifiedSelectionIntent) {
      event->accept();
      return;
    }
    const double clickedFrame = (event->position().x() + impl_->horizontalOffset_) /
                                std::max<double>(0.001, static_cast<double>(impl_->pixelsPerFrame_));
    const double clamped =
        std::clamp(clickedFrame, 0.0,
                   std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)));
    seekRequested(clamped);
    setCurrentFrame(clamped);
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton && impl_->draggingMarker_ &&
      impl_->dragMarkerIndex_ >= 0 &&
      impl_->dragMarkerIndex_ < impl_->keyframeMarkers_.size()) {
    const double targetFrame = std::clamp(
        impl_->dragMarkerTargetFrame_, 0.0,
        std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)));
    const double deltaFrames = targetFrame - impl_->dragMarkerOrigFrame_;
    struct DragMoveRequest {
      LayerID layerId;
      QString propertyPath;
      qint64 fromFrame = -1;
      qint64 toFrame = -1;
    };

    QVector<DragMoveRequest> requests;
    requests.reserve(impl_->dragMarkerSelectionIndices_.size());
    QSet<QString> nextSelectedKeys = impl_->selectedMarkerKeys_;
    for (int i = 0; i < impl_->dragMarkerSelectionIndices_.size(); ++i) {
      const int selectedIndex = impl_->dragMarkerSelectionIndices_[i];
      if (selectedIndex < 0 || selectedIndex >= impl_->keyframeMarkers_.size() ||
          i >= impl_->dragMarkerSelectionOrigFrames_.size()) {
        continue;
      }
      const auto marker = impl_->keyframeMarkers_[selectedIndex];
      const double originalFrame = impl_->dragMarkerSelectionOrigFrames_[i];
      const double newFrame = std::clamp(
          originalFrame + deltaFrames, 0.0,
          std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)));
      const qint64 fromFrame =
          static_cast<qint64>(std::llround(originalFrame));
      const qint64 toFrame = static_cast<qint64>(std::llround(newFrame));
      if (fromFrame == toFrame) {
        continue;
      }
      const QString oldKey = keyframeSelectionKey(
          marker.layerId, marker.propertyPath, fromFrame);
      const QString newKey =
          keyframeSelectionKey(marker.layerId, marker.propertyPath, toFrame);
      nextSelectedKeys.remove(oldKey);
      nextSelectedKeys.insert(newKey);
      requests.push_back(
          DragMoveRequest{marker.layerId, marker.propertyPath, fromFrame, toFrame});
    }
    if (!requests.isEmpty()) {
      applyMarkerSelectionSet(impl_->keyframeMarkers_, impl_->selectedMarkerKeys_,
                              nextSelectedKeys);
      Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
      for (const auto &request : requests) {
        Q_EMIT keyframeMoveRequested(request.layerId, request.propertyPath,
                                     request.fromFrame, request.toFrame);
      }
      Q_EMIT timelineDebugMessage(
          QStringLiteral("Dragged %1 %3 by %2 %4")
              .arg(requests.size())
              .arg(QString::number(
                  static_cast<qint64>(std::llround(deltaFrames))))
              .arg(formatKeyframeNoun(requests.size()))
              .arg(formatFrameUnit(static_cast<qint64>(std::llround(deltaFrames)))));
    }
    impl_->draggingMarker_ = false;
    impl_->dragMarkerIndex_ = -1;
    impl_->dragMarkerSelectionIndices_.clear();
    impl_->dragMarkerSelectionOrigFrames_.clear();
    impl_->dragMarkerTargetFrame_ = 0.0;
    impl_->dragMarkerSnapLabel_.clear();
    impl_->pendingMarkerSingleClick_ = false;
    impl_->pendingMarkerSingleClickKey_.clear();
    impl_->pendingMarkerSingleClickLabel_.clear();
    impl_->pendingMarkerSingleClickFrame_ = 0.0;
    updateHoverToolTip(this, event->globalPosition().toPoint(), QString(),
                       impl_->hoverToolTipText_);
    if (impl_->hoverMarkerIndex_ >= 0) {
      setCursor(Qt::PointingHandCursor);
    } else {
      setCursor(Qt::ArrowCursor);
    }
    update();
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton && impl_->dragMode_ != DragMode::None) {
    const int idx = impl_->dragClipIndex_;
    if (idx >= 0 && idx < impl_->clips_.size()) {
      const auto &clip = impl_->clips_[idx];
      if (impl_->dragMode_ == DragMode::MoveBody)
        clipMoved(clip.clipId, clip.startFrame);
      else
        clipResized(clip.clipId, clip.startFrame, clip.durationFrame);
    }
    impl_->dragMode_ = DragMode::None;
    impl_->dragClipIndex_ = -1;
    switch (impl_->hoverEdge_) {
    case DragMode::ResizeLeft:
    case DragMode::ResizeRight:
      setCursor(Qt::SizeHorCursor);
      break;
    case DragMode::MoveBody:
      setCursor(Qt::OpenHandCursor);
      break;
    default:
      setCursor(Qt::ArrowCursor);
      break;
    }
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

void ArtifactTimelineTrackPainterView::contextMenuEvent(
    QContextMenuEvent *event) {
  if (!event) {
    return;
  }

  ArtifactCompositionPtr composition;
  if (auto *svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }

  const double mouseX = static_cast<double>(event->pos().x());
  const double mouseY = static_cast<double>(event->pos().y());
  const auto clipHit = hitTestClips(
      impl_->clips_, impl_->trackHeights_, impl_->trackTops_, mouseX, mouseY,
      impl_->pixelsPerFrame_, impl_->horizontalOffset_, impl_->verticalOffset_);
  const bool clipUnderCursor = clipHit.mode != DragMode::None &&
                               clipHit.clipIndex >= 0 &&
                               clipHit.clipIndex < impl_->clips_.size();
  const auto markerHit = hitTestMarkers(
      impl_->keyframeMarkers_, impl_->trackHeights_, impl_->trackTops_, mouseX,
      mouseY, impl_->pixelsPerFrame_, impl_->horizontalOffset_,
      impl_->verticalOffset_);
  const bool markerUnderCursor =
      markerHit.markerIndex >= 0 &&
      markerHit.markerIndex < impl_->keyframeMarkers_.size();

  LayerID targetLayerId;
  QString targetPropertyPath;
  if (markerUnderCursor) {
    const auto &marker = impl_->keyframeMarkers_[markerHit.markerIndex];
    targetLayerId = marker.layerId;
    targetPropertyPath = marker.propertyPath.trimmed();
  } else if (clipUnderCursor) {
    targetLayerId = impl_->clips_[clipHit.clipIndex].layerId;
  } else if (const auto trackIndex =
                 trackIndexAt(impl_->trackHeights_, impl_->trackTops_, mouseY,
                               impl_->verticalOffset_);
             trackIndex.has_value()) {
    if (*trackIndex >= 0 && *trackIndex < impl_->trackRows_.size()) {
      const auto &row = impl_->trackRows_.at(*trackIndex);
      targetLayerId = row.layerId;
      if (row.kind == TimelineRowKind::Property) {
        targetPropertyPath = row.propertyPath.trimmed();
      }
    }
    if (targetLayerId.isNil()) {
      for (const auto &clip : impl_->clips_) {
        if (clip.trackIndex == *trackIndex) {
          targetLayerId = clip.layerId;
          break;
        }
      }
    }
  }

  LayerID fallbackLayerId = impl_->contextLayerId_;
  if (fallbackLayerId.isNil()) {
    if (auto *app = ArtifactApplicationManager::instance()) {
      if (auto *selection = app->layerSelectionManager()) {
        if (auto currentLayer = selection->currentLayer()) {
          fallbackLayerId = currentLayer->id();
        }
      }
    }
  }

  if (targetLayerId.isNil() && !fallbackLayerId.isNil()) {
    targetLayerId = fallbackLayerId;
  }
  if (targetPropertyPath.isEmpty() && !impl_->contextPropertyPath_.isEmpty() &&
      (impl_->contextLayerId_.isNil() || targetLayerId.isNil() ||
       impl_->contextLayerId_ == targetLayerId)) {
    targetPropertyPath = impl_->contextPropertyPath_;
  }

  const bool canEditFocusedProperty =
      composition && !targetLayerId.isNil() && !targetPropertyPath.isEmpty();
  if (!canEditFocusedProperty && !clipUnderCursor && !markerUnderCursor) {
    event->ignore();
    return;
  }

  const qint64 contextFrame = static_cast<qint64>(std::llround(std::clamp(
      (mouseX + impl_->horizontalOffset_) /
          std::max<double>(0.001, static_cast<double>(impl_->pixelsPerFrame_)),
      0.0, std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)))));

  QMenu menu(this);
  QAction *jumpToMarkerAct = nullptr;
  if (markerUnderCursor) {
    const auto &marker = impl_->keyframeMarkers_[markerHit.markerIndex];
    const QString label =
         marker.label.isEmpty() ? QStringLiteral("Keyframe") : marker.label;
     jumpToMarkerAct = menu.addAction(QStringLiteral("Jump to %1").arg(label));
     setActionIcon(jumpToMarkerAct, QStringLiteral("timeline_keyframe_select"));
   }
  QAction *addKeyframeAct = nullptr;
  QAction *removeKeyframeAct = nullptr;
  if (canEditFocusedProperty) {
    menu.addSeparator();
    addKeyframeAct = menu.addAction(QStringLiteral("Add Keyframe at Playhead"));
    removeKeyframeAct =
        menu.addAction(QStringLiteral("Remove Keyframe at Playhead"));
    setActionIcon(addKeyframeAct, QStringLiteral("timeline_keyframe_add"));
    setActionIcon(removeKeyframeAct, QStringLiteral("timeline_keyframe_remove"));
  }

  const QVector<KeyframeMarkerVisual> selectedMarkers = selectedKeyframeMarkers();
  QVector<KeyframeMarkerVisual> interpolationTargets = selectedMarkers;
  if (interpolationTargets.isEmpty() && markerUnderCursor) {
    interpolationTargets.push_back(impl_->keyframeMarkers_[markerHit.markerIndex]);
  }
  QVector<KeyframeMarkerVisual> cleanTargets = selectedMarkers;
  if (cleanTargets.isEmpty() && markerUnderCursor) {
    cleanTargets.push_back(impl_->keyframeMarkers_[markerHit.markerIndex]);
  }
  const QVector<KeyframePropertyRef> cleanTargetRefs =
      collectPropertyRefsFromMarkers(cleanTargets);
  const bool hasKeyframeTarget = markerUnderCursor || !selectedMarkers.isEmpty();
  QAction *selectSamePropertyKeyframesAct = nullptr;
  QAction *selectNeighborKeyframesAct = nullptr;
  if (hasKeyframeTarget) {
    menu.addSeparator();
    selectSamePropertyKeyframesAct =
        menu.addAction(QStringLiteral("Select Same Property Keyframes"));
    selectNeighborKeyframesAct =
        menu.addAction(QStringLiteral("Select Neighbor Keyframes"));
    setActionIcon(selectSamePropertyKeyframesAct, QStringLiteral("timeline_keyframe_select"));
    setActionIcon(selectNeighborKeyframesAct, QStringLiteral("timeline_keyframe_select"));
  }
  QAction *duplicateSelectedKeyframesAct = nullptr;
  if (hasKeyframeTarget) {
    duplicateSelectedKeyframesAct =
        menu.addAction(QStringLiteral("Duplicate Keyframes Here"));
    setActionIcon(duplicateSelectedKeyframesAct, QStringLiteral("timeline_keyframe_duplicate"));
  }
  QAction *shrinkSelectedKeyframesAct = nullptr;
  QAction *stretchSelectedKeyframesAct = nullptr;
  QAction *reverseSelectedKeyframesAct = nullptr;
  QAction *normalizeSelectedKeyframesAct = nullptr;
  QAction *scaleSelectedKeyframesAct = nullptr;
  QAction *offsetSelectedKeyframesAct = nullptr;
  if (!selectedMarkers.isEmpty()) {
    QMenu *editMenu = menu.addMenu(QStringLiteral("Keyframe Edit"));
    setMenuIcon(editMenu, QStringLiteral("timeline_keyframe_interpolation"));
    shrinkSelectedKeyframesAct = editMenu->addAction(QStringLiteral("Shrink Keys"));
    stretchSelectedKeyframesAct = editMenu->addAction(QStringLiteral("Stretch Keys"));
    reverseSelectedKeyframesAct = editMenu->addAction(QStringLiteral("Reverse Keys"));
    normalizeSelectedKeyframesAct = editMenu->addAction(QStringLiteral("Normalize Duration"));
    scaleSelectedKeyframesAct = editMenu->addAction(QStringLiteral("Scale Values"));
    offsetSelectedKeyframesAct = editMenu->addAction(QStringLiteral("Offset Values"));
  }
  QAction *distributeSelectedKeyframesAct = nullptr;
  QAction *repeatSelectedKeyframesAct = nullptr;
  if (selectedMarkers.size() >= 2) {
    QMenu *arrangeMenu = menu.addMenu(QStringLiteral("Arrange"));
    setMenuIcon(arrangeMenu, QStringLiteral("timeline_keyframe_interpolation"));
    distributeSelectedKeyframesAct =
        arrangeMenu->addAction(QStringLiteral("Distribute Evenly"));
    repeatSelectedKeyframesAct =
        arrangeMenu->addAction(QStringLiteral("Repeat Pattern..."));
    setActionIcon(distributeSelectedKeyframesAct, QStringLiteral("timeline_keyframe_interpolation"));
    setActionIcon(repeatSelectedKeyframesAct, QStringLiteral("timeline_keyframe_duplicate"));
  } else if (hasKeyframeTarget) {
    menu.addSeparator();
    repeatSelectedKeyframesAct =
        menu.addAction(QStringLiteral("Repeat Pattern..."));
    setActionIcon(repeatSelectedKeyframesAct, QStringLiteral("timeline_keyframe_duplicate"));
  }
  QAction *keyPatternDialogAct = nullptr;
  if (canEditFocusedProperty) {
    keyPatternDialogAct = menu.addAction(QStringLiteral("Key Pattern Dialog"));
    setActionIcon(keyPatternDialogAct, QStringLiteral("timeline_keyframe_interpolation"));
  }
  QAction *copySelectedKeyframesAct = nullptr;
  QAction *cutSelectedKeyframesAct = nullptr;
  QAction *pasteKeyframesAct = nullptr;
  if (hasKeyframeTarget) {
    copySelectedKeyframesAct =
        menu.addAction(QStringLiteral("Copy Selected Keyframes"));
    cutSelectedKeyframesAct =
        menu.addAction(QStringLiteral("Cut Selected Keyframes"));
    setActionIcon(copySelectedKeyframesAct, QStringLiteral("timeline_keyframe_copy"));
    setActionIcon(cutSelectedKeyframesAct, QStringLiteral("timeline_keyframe_remove"));
  }
  if (ClipboardManager::instance().hasKeyframeData()) {
    pasteKeyframesAct =
        menu.addAction(QStringLiteral("Paste Keyframes at Playhead"));
    setActionIcon(pasteKeyframesAct, QStringLiteral("timeline_keyframe_paste"));
  }
  QAction *deleteSelectedKeyframesAct = nullptr;
  if (hasKeyframeTarget) {
    menu.addSeparator();
    deleteSelectedKeyframesAct =
        menu.addAction(QStringLiteral("Delete Selected Keyframes"));
    setActionIcon(deleteSelectedKeyframesAct, QStringLiteral("timeline_keyframe_remove"));
  }
  QAction *cleanSelectedKeyframesAct = nullptr;
  if (!cleanTargetRefs.isEmpty()) {
    cleanSelectedKeyframesAct =
        menu.addAction(QStringLiteral("Keyframe Clean"));
    setActionIcon(cleanSelectedKeyframesAct, QStringLiteral("timeline_keyframe_clean"));
  }
  QAction *staggerStartAct = nullptr;
  QAction *staggerEndAct = nullptr;
  QAction *cascadeClipsAct = nullptr;
  QAction *overlapClipsAct = nullptr;
  if (clipUnderCursor) {
    QMenu *staggerMenu = menu.addMenu(QStringLiteral("Clip Stagger"));
    staggerStartAct = staggerMenu->addAction(QStringLiteral("Stagger Start +4f"));
    staggerEndAct = staggerMenu->addAction(QStringLiteral("Stagger End +4f"));
    cascadeClipsAct = staggerMenu->addAction(QStringLiteral("Cascade Clips"));
    overlapClipsAct = staggerMenu->addAction(QStringLiteral("Overlap by 4 Frames"));
  }
  QAction *interpLinearAct = nullptr;
  QAction *interpEaseInAct = nullptr;
  QAction *interpEaseOutAct = nullptr;
  QAction *interpEaseInOutAct = nullptr;
  QAction *interpHoldAct = nullptr;
  QAction *interpBezierAct = nullptr;
  QMenu *interpolationMenu = nullptr;
  if (!interpolationTargets.isEmpty()) {
    menu.addSeparator();
    interpolationMenu = menu.addMenu(QStringLiteral("Interpolation"));
    setMenuIcon(interpolationMenu, QStringLiteral("timeline_keyframe_interpolation"));
    interpLinearAct = interpolationMenu->addAction(QStringLiteral("Linear"));
    interpEaseInAct = interpolationMenu->addAction(QStringLiteral("Ease In"));
    interpEaseOutAct = interpolationMenu->addAction(QStringLiteral("Ease Out"));
    interpEaseInOutAct = interpolationMenu->addAction(QStringLiteral("Ease In/Out"));
    interpHoldAct = interpolationMenu->addAction(QStringLiteral("Hold"));
    interpBezierAct = interpolationMenu->addAction(QStringLiteral("Bezier"));
    setActionIcon(interpLinearAct, QStringLiteral("timeline_keyframe_interpolation"));
    setActionIcon(interpEaseInAct, QStringLiteral("timeline_keyframe_interpolation"));
    setActionIcon(interpEaseOutAct, QStringLiteral("timeline_keyframe_interpolation"));
    setActionIcon(interpEaseInOutAct, QStringLiteral("timeline_keyframe_interpolation"));
    setActionIcon(interpHoldAct, QStringLiteral("timeline_keyframe_anchor"));
    setActionIcon(interpBezierAct, QStringLiteral("timeline_keyframe_interpolation"));
  }
  QAction *rovingOnAct = nullptr;
  QAction *rovingOffAct = nullptr;
  QMenu *rovingMenu = nullptr;
  if (!interpolationTargets.isEmpty()) {
    rovingMenu = menu.addMenu(QStringLiteral("Roving"));
    setMenuIcon(rovingMenu, QStringLiteral("timeline_keyframe_roving"));
    const bool anyRoving = std::any_of(interpolationTargets.cbegin(), interpolationTargets.cend(),
                                       [](const auto &marker) { return marker.roving; });
    const bool allRoving = std::all_of(interpolationTargets.cbegin(), interpolationTargets.cend(),
                                       [](const auto &marker) { return marker.roving; });
    rovingOnAct = rovingMenu->addAction(QStringLiteral("Mark as Roving"));
    rovingOffAct = rovingMenu->addAction(QStringLiteral("Clear Roving"));
    setActionIcon(rovingOnAct, QStringLiteral("timeline_keyframe_roving"));
    setActionIcon(rovingOffAct, QStringLiteral("timeline_keyframe_anchor"));
    rovingOnAct->setCheckable(true);
    rovingOffAct->setCheckable(true);
    rovingOnAct->setChecked(allRoving);
    rovingOffAct->setChecked(!anyRoving);
  }
  QAction *anchorAbsoluteAct = nullptr;
  QAction *anchorLockToInAct = nullptr;
  QAction *anchorLockToOutAct = nullptr;
  QAction *anchorStretchAct = nullptr;
  QAction *colorNoneAct = nullptr;
  QAction *colorRedAct = nullptr;
  QAction *colorBlueAct = nullptr;
  QAction *colorYellowAct = nullptr;
  QAction *colorGreenAct = nullptr;
  QAction *colorPurpleAct = nullptr;
  QAction *colorGrayAct = nullptr;
  if (!selectedMarkers.isEmpty()) {
    QMenu *anchorMenu = menu.addMenu(QStringLiteral("Keyframe Anchor"));
    setMenuIcon(anchorMenu, QStringLiteral("timeline_keyframe_anchor"));
    anchorAbsoluteAct = anchorMenu->addAction(QStringLiteral("Absolute"));
    anchorLockToInAct = anchorMenu->addAction(QStringLiteral("Lock to In Point"));
    anchorLockToOutAct = anchorMenu->addAction(QStringLiteral("Lock to Out Point"));
    anchorStretchAct = anchorMenu->addAction(QStringLiteral("Stretch with Layer"));
    setActionIcon(anchorAbsoluteAct, QStringLiteral("timeline_keyframe_anchor"));
    setActionIcon(anchorLockToInAct, QStringLiteral("timemenu_in_point"));
    setActionIcon(anchorLockToOutAct, QStringLiteral("timemenu_out_point"));
    setActionIcon(anchorStretchAct, QStringLiteral("timeline_keyframe_roving"));
  }
  if (!selectedMarkers.isEmpty()) {
    QMenu *colorMenu = menu.addMenu(QStringLiteral("Keyframe Color Label"));
    setMenuIcon(colorMenu, QStringLiteral("timeline_keyframe_color"));
    colorNoneAct = colorMenu->addAction(QStringLiteral("None"));
    colorRedAct = colorMenu->addAction(QStringLiteral("Red"));
    colorBlueAct = colorMenu->addAction(QStringLiteral("Blue"));
    colorYellowAct = colorMenu->addAction(QStringLiteral("Yellow"));
    colorGreenAct = colorMenu->addAction(QStringLiteral("Green"));
    colorPurpleAct = colorMenu->addAction(QStringLiteral("Purple"));
    colorGrayAct = colorMenu->addAction(QStringLiteral("Gray"));
    setActionIcon(colorNoneAct, QStringLiteral("timeline_keyframe_color"));
    setActionIcon(colorRedAct, QStringLiteral("timeline_keyframe_color"));
    setActionIcon(colorBlueAct, QStringLiteral("timeline_keyframe_color"));
    setActionIcon(colorYellowAct, QStringLiteral("timeline_keyframe_color"));
    setActionIcon(colorGreenAct, QStringLiteral("timeline_keyframe_color"));
    setActionIcon(colorPurpleAct, QStringLiteral("timeline_keyframe_color"));
    setActionIcon(colorGrayAct, QStringLiteral("timeline_keyframe_color"));
  }
  QAction *addMarkerFrameAct = nullptr;
  QAction *removeMarkerFrameAct = nullptr;
  if (markerUnderCursor && canEditFocusedProperty) {
    menu.addSeparator();
    addMarkerFrameAct =
        menu.addAction(QStringLiteral("Add Keyframe at Marker Frame"));
    removeMarkerFrameAct =
        menu.addAction(QStringLiteral("Remove Keyframe at Marker Frame"));
    setActionIcon(addMarkerFrameAct, QStringLiteral("timeline_keyframe_add"));
    setActionIcon(removeMarkerFrameAct, QStringLiteral("timeline_keyframe_remove"));
  }

  QAction *splitClipAct = nullptr;
  QAction *duplicateClipAct = nullptr;
  QAction *trimInClipAct = nullptr;
  QAction *trimOutClipAct = nullptr;
  QAction *rippleTrimOutClipAct = nullptr;
  QAction *rippleTrimInClipAct = nullptr;
  QAction *rippleDeleteClipAct = nullptr;
  QAction *moveStartClipAct = nullptr;
  QAction *deleteClipAct = nullptr;
  if (clipUnderCursor) {
    if (!markerUnderCursor) {
      menu.addSeparator();
    }
    splitClipAct = menu.addAction(QStringLiteral("Split Layer at Playhead"));
    duplicateClipAct = menu.addAction(QStringLiteral("Duplicate Layer"));
    moveStartClipAct = menu.addAction(QStringLiteral("Move Start to Playhead"));
    trimInClipAct = menu.addAction(QStringLiteral("Trim In at Playhead"));
    trimOutClipAct = menu.addAction(QStringLiteral("Trim Out at Playhead"));
    rippleTrimOutClipAct =
        menu.addAction(QStringLiteral("Ripple Trim Out at Playhead"));
    rippleTrimInClipAct =
        menu.addAction(QStringLiteral("Ripple Trim In at Playhead"));
    rippleDeleteClipAct =
        menu.addAction(QStringLiteral("Ripple Delete (Close Gap)"));
    deleteClipAct = menu.addAction(QStringLiteral("Delete Layer"));
  }

  const QAction *chosen = menu.exec(event->globalPos());
  if (!chosen) {
    event->accept();
    return;
  }

  if (chosen == jumpToMarkerAct) {
    const double targetFrame = std::clamp(static_cast<double>(contextFrame), 0.0,
                                          std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)));
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

  if (interpolationMenu &&
      (chosen == interpLinearAct || chosen == interpEaseInAct ||
       chosen == interpEaseOutAct || chosen == interpEaseInOutAct ||
       chosen == interpHoldAct || chosen == interpBezierAct)) {
    const ArtifactCore::InterpolationType type =
        (chosen == interpLinearAct)   ? ArtifactCore::InterpolationType::Linear
        : (chosen == interpEaseInAct) ? ArtifactCore::InterpolationType::EaseIn
        : (chosen == interpEaseOutAct) ? ArtifactCore::InterpolationType::EaseOut
        : (chosen == interpEaseInOutAct)
            ? ArtifactCore::InterpolationType::EaseInOut
        : (chosen == interpHoldAct)   ? ArtifactCore::InterpolationType::Constant
                                      : ArtifactCore::InterpolationType::Bezier;

    ArtifactCompositionPtr currentComposition = composition;
    if (!currentComposition) {
      if (auto *svc = ArtifactProjectService::instance()) {
        currentComposition = svc->currentComposition().lock();
      }
    }
    int applied = 0;
    if (currentComposition) {
      applied = applyInterpolationToSelectedKeyframesImpl(
          currentComposition, interpolationTargets, type);
    }
    if (applied > 0) {
      const QString typeLabel =
          (type == ArtifactCore::InterpolationType::Linear)
              ? QStringLiteral("Linear")
              : (type == ArtifactCore::InterpolationType::EaseIn)
                    ? QStringLiteral("Ease In")
                    : (type == ArtifactCore::InterpolationType::EaseOut)
                          ? QStringLiteral("Ease Out")
                          : (type == ArtifactCore::InterpolationType::EaseInOut)
                                ? QStringLiteral("Ease In/Out")
                                : (type == ArtifactCore::InterpolationType::Constant)
                                      ? QStringLiteral("Hold")
                                      : QStringLiteral("Bezier");
      Q_EMIT timelineDebugMessage(
          QStringLiteral("Applied %1 interpolation to %2 %3")
              .arg(typeLabel)
              .arg(applied)
              .arg(formatKeyframeNoun(applied)));
      update();
    }
    event->accept();
    return;
  }

  if (rovingMenu && (chosen == rovingOnAct || chosen == rovingOffAct)) {
    ArtifactCompositionPtr currentComposition = composition;
    if (!currentComposition) {
      if (auto *svc = ArtifactProjectService::instance()) {
        currentComposition = svc->currentComposition().lock();
      }
    }
    const bool roving = chosen == rovingOnAct;
    int applied = 0;
    if (currentComposition) {
      applied = applyRovingToSelectedKeyframesImpl(currentComposition, interpolationTargets, roving);
    }
    if (applied > 0) {
      Q_EMIT timelineDebugMessage(
          QStringLiteral("%1 roving on %2 %3")
              .arg(roving ? QStringLiteral("Enabled") : QStringLiteral("Disabled"))
              .arg(applied)
              .arg(formatKeyframeNoun(applied)));
      update();
    }
    event->accept();
    return;
  }

  if (anchorAbsoluteAct &&
      (chosen == anchorAbsoluteAct || chosen == anchorLockToInAct ||
       chosen == anchorLockToOutAct || chosen == anchorStretchAct)) {
    const auto anchor =
        (chosen == anchorAbsoluteAct)   ? ArtifactCore::KeyFrame::Anchor::Absolute
        : (chosen == anchorLockToInAct) ? ArtifactCore::KeyFrame::Anchor::LockToIn
        : (chosen == anchorLockToOutAct) ? ArtifactCore::KeyFrame::Anchor::LockToOut
                                        : ArtifactCore::KeyFrame::Anchor::StretchWithLayer;
    if (setSelectedKeyframeAnchor(anchor)) {
      event->accept();
      return;
    }
    event->accept();
    return;
  }

  if (colorNoneAct &&
      (chosen == colorNoneAct || chosen == colorRedAct ||
       chosen == colorBlueAct || chosen == colorYellowAct ||
       chosen == colorGreenAct || chosen == colorPurpleAct ||
       chosen == colorGrayAct)) {
    const auto label =
        (chosen == colorNoneAct)    ? ArtifactCore::KeyFrame::ColorLabel::None
        : (chosen == colorRedAct)   ? ArtifactCore::KeyFrame::ColorLabel::Red
        : (chosen == colorBlueAct)  ? ArtifactCore::KeyFrame::ColorLabel::Blue
        : (chosen == colorYellowAct) ? ArtifactCore::KeyFrame::ColorLabel::Yellow
        : (chosen == colorGreenAct)  ? ArtifactCore::KeyFrame::ColorLabel::Green
        : (chosen == colorPurpleAct) ? ArtifactCore::KeyFrame::ColorLabel::Purple
                                     : ArtifactCore::KeyFrame::ColorLabel::Gray;
    if (setSelectedKeyframeColorLabel(label)) {
      event->accept();
      return;
    }
    event->accept();
    return;
  }

  if (chosen == duplicateSelectedKeyframesAct) {
    if (duplicateSelectedKeyframeMarkersAtCurrentFrame()) {
      event->accept();
      return;
    }
    event->accept();
    return;
  }

  if (chosen == shrinkSelectedKeyframesAct || chosen == stretchSelectedKeyframesAct ||
      chosen == reverseSelectedKeyframesAct || chosen == normalizeSelectedKeyframesAct ||
      chosen == scaleSelectedKeyframesAct || chosen == offsetSelectedKeyframesAct) {
    ArtifactCompositionPtr currentComposition = composition;
    if (!currentComposition) {
      if (auto *svc = ArtifactProjectService::instance()) {
        currentComposition = svc->currentComposition().lock();
      }
    }
    if (currentComposition && !selectedMarkers.isEmpty()) {
      const auto refs = collectPropertyRefsFromMarkers(selectedMarkers);
      const auto beforeSnapshots =
          captureKeyframePropertySnapshots(currentComposition, refs);
      const QSet<QString> beforeSelectionKeys = impl_->selectedMarkerKeys_;
      KeyframeRangeTransformOptions options;
      if (chosen == shrinkSelectedKeyframesAct) {
        options.kind = KeyframeRangeTransformKind::Stretch;
        options.scale = 0.5;
      } else if (chosen == stretchSelectedKeyframesAct) {
        options.kind = KeyframeRangeTransformKind::Stretch;
        options.scale = 2.0;
      } else if (chosen == reverseSelectedKeyframesAct) {
        options.kind = KeyframeRangeTransformKind::Reverse;
      } else if (chosen == normalizeSelectedKeyframesAct) {
        options.kind = KeyframeRangeTransformKind::Normalize;
        options.targetDuration = std::max<qint64>(1, contextFrame + 1);
      } else if (chosen == scaleSelectedKeyframesAct) {
        options.kind = KeyframeRangeTransformKind::ScaleValues;
        options.valueScale = 1.25;
      } else if (chosen == offsetSelectedKeyframesAct) {
        options.kind = KeyframeRangeTransformKind::OffsetValues;
        options.valueOffset = 5.0;
      }

      QSet<QString> nextSelectionKeys;
      int affectedCount = 0;
      const bool changed = applySelectedKeyframeRangeTransform(
          currentComposition, selectedMarkers, options, &nextSelectionKeys,
          &affectedCount);
      if (changed) {
        const auto afterSnapshots =
            captureKeyframePropertySnapshots(currentComposition, refs);
        if (auto *mgr = UndoManager::instance()) {
          QPointer<ArtifactTimelineTrackPainterView> self(this);
          const QSet<QString> afterSelectionKeys = nextSelectionKeys;
          mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
              QStringLiteral("Transform Selected Keyframes"),
              [self, currentComposition, afterSnapshots, afterSelectionKeys]() {
                applyKeyframePropertySnapshots(currentComposition, afterSnapshots);
                if (!self) {
                  return;
                }
                ArtifactLayerSelectionManager *selectionManager = nullptr;
                if (auto *app = ArtifactApplicationManager::instance()) {
                  selectionManager = app->layerSelectionManager();
                }
                self->syncSelectionState(currentComposition, selectionManager,
                                         self->impl_->trackRows_, true);
                self->setSelectedKeyframeKeys(afterSelectionKeys);
              },
              [self, currentComposition, beforeSnapshots, beforeSelectionKeys]() {
                applyKeyframePropertySnapshots(currentComposition, beforeSnapshots);
                if (!self) {
                  return;
                }
                ArtifactLayerSelectionManager *selectionManager = nullptr;
                if (auto *app = ArtifactApplicationManager::instance()) {
                  selectionManager = app->layerSelectionManager();
                }
                self->syncSelectionState(currentComposition, selectionManager,
                                         self->impl_->trackRows_, true);
                self->setSelectedKeyframeKeys(beforeSelectionKeys);
              }));
        }
        impl_->selectedMarkerKeys_ = std::move(nextSelectionKeys);
        ArtifactLayerSelectionManager *selectionManager = nullptr;
        if (auto *app = ArtifactApplicationManager::instance()) {
          selectionManager = app->layerSelectionManager();
        }
        syncSelectionState(currentComposition, selectionManager, impl_->trackRows_, true);
        Q_EMIT timelineDebugMessage(
            QStringLiteral("Transformed %1 %2")
                .arg(affectedCount)
                .arg(formatKeyframeNoun(affectedCount)));
        update();
      }
    }
    event->accept();
    return;
  }

  if (chosen == selectSamePropertyKeyframesAct) {
    selectSamePropertyKeyframeMarkers();
    event->accept();
    return;
  }

  if (chosen == selectNeighborKeyframesAct) {
    selectNeighborKeyframeMarkers();
    event->accept();
    return;
  }

  if (chosen == distributeSelectedKeyframesAct) {
    if (distributeSelectedKeyframeMarkersEvenly()) {
      event->accept();
      return;
    }
    event->accept();
    return;
  }

  if (chosen == repeatSelectedKeyframesAct) {
    if (repeatSelectedKeyframeMarkersAtCurrentFrame()) {
      event->accept();
      return;
    }
    event->accept();
    return;
  }

  if (chosen == keyPatternDialogAct) {
    if (auto *parentTimeline = qobject_cast<QWidget *>(parentWidget())) {
      QMetaObject::invokeMethod(parentTimeline, [parentTimeline]() {
        Q_UNUSED(parentTimeline);
      });
    }
    event->accept();
    return;
  }

  if (chosen == copySelectedKeyframesAct) {
    if (triggerTimelineShortcut(this, Qt::Key_C, Qt::ControlModifier)) {
      event->accept();
      return;
    }
    event->accept();
    return;
  }

  if (chosen == cutSelectedKeyframesAct) {
    if (triggerTimelineShortcut(this, Qt::Key_C, Qt::ControlModifier) &&
        deleteSelectedKeyframeMarkers()) {
      event->accept();
      return;
    }
    event->accept();
    return;
  }

  if (chosen == pasteKeyframesAct) {
    if (triggerTimelineShortcut(this, Qt::Key_V, Qt::ControlModifier)) {
      event->accept();
      return;
    }
    event->accept();
    return;
  }

  if (chosen == deleteSelectedKeyframesAct) {
    if (deleteSelectedKeyframeMarkers()) {
      event->accept();
      return;
    }
    event->accept();
    return;
  }

  if (chosen == staggerStartAct || chosen == staggerEndAct ||
      chosen == cascadeClipsAct || chosen == overlapClipsAct) {
    auto *svc = ArtifactProjectService::instance();
    auto currentComp = svc ? svc->currentComposition().lock() : nullptr;
    auto *selManager = ArtifactApplicationManager::instance()
                           ? ArtifactApplicationManager::instance()->layerSelectionManager()
                           : nullptr;
    if (!currentComp || !selManager) {
      event->accept();
      return;
    }
    const auto selectedLayers = selManager->selectedLayers();
    if (selectedLayers.isEmpty()) {
      event->accept();
      return;
    }
    const qint64 step = 4;
    qint64 cursor = contextFrame;
    for (const auto &layer : selectedLayers) {
      if (!layer) {
        continue;
      }
      if (chosen == staggerStartAct) {
        applyTimelineLayerMove(impl_->compositionId_, layer->id().toString(),
                               cursor, 0.0);
        cursor += step;
      } else if (chosen == staggerEndAct) {
        const qint64 duration = layer->outPoint().framePosition() -
                                layer->inPoint().framePosition();
        applyTimelineLayerMove(impl_->compositionId_, layer->id().toString(),
                               std::max<qint64>(0, cursor - duration), 0.0);
        cursor += step;
      } else if (chosen == cascadeClipsAct) {
        applyTimelineLayerMove(impl_->compositionId_, layer->id().toString(),
                               cursor, 0.0);
        cursor = layer->outPoint().framePosition();
      } else if (chosen == overlapClipsAct) {
        applyTimelineLayerMove(impl_->compositionId_, layer->id().toString(),
                               cursor, 0.0);
        cursor += std::max<qint64>(1, layer->outPoint().framePosition() -
                                        layer->inPoint().framePosition() - step);
      }
    }
    Q_EMIT timelineDebugMessage(QStringLiteral("Applied clip stagger"));
    event->accept();
    return;
  }

  if (chosen == cleanSelectedKeyframesAct) {
    ArtifactCompositionPtr currentComposition = composition;
    if (!currentComposition) {
      if (auto *svc = ArtifactProjectService::instance()) {
        currentComposition = svc->currentComposition().lock();
      }
    }
    if (currentComposition && !cleanTargetRefs.isEmpty()) {
      const auto beforeSnapshots =
          captureKeyframePropertySnapshots(currentComposition, cleanTargetRefs);
      int removedCount = 0;
      const bool changed = cleanNearDuplicateKeyframes(currentComposition,
                                                       cleanTargetRefs,
                                                       &removedCount);
      if (changed) {
        const auto afterSnapshots =
            captureKeyframePropertySnapshots(currentComposition, cleanTargetRefs);
        if (auto *mgr = UndoManager::instance()) {
          QPointer<ArtifactTimelineTrackPainterView> self(this);
          mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
              QStringLiteral("Clean Keyframes"),
              [self, currentComposition, afterSnapshots]() {
                applyKeyframePropertySnapshots(currentComposition,
                                               afterSnapshots);
                if (!self) {
                  return;
                }
                ArtifactLayerSelectionManager *selectionManager = nullptr;
                if (auto *app = ArtifactApplicationManager::instance()) {
                  selectionManager = app->layerSelectionManager();
                }
                self->syncSelectionState(currentComposition, selectionManager,
                                         self->impl_->trackRows_, true);
              },
              [self, currentComposition, beforeSnapshots]() {
                applyKeyframePropertySnapshots(currentComposition,
                                               beforeSnapshots);
                if (!self) {
                  return;
                }
                ArtifactLayerSelectionManager *selectionManager = nullptr;
                if (auto *app = ArtifactApplicationManager::instance()) {
                  selectionManager = app->layerSelectionManager();
                }
                self->syncSelectionState(currentComposition, selectionManager,
                                         self->impl_->trackRows_, true);
              }));
        }

        ArtifactLayerSelectionManager *selectionManager = nullptr;
        if (auto *app = ArtifactApplicationManager::instance()) {
          selectionManager = app->layerSelectionManager();
        }
        syncSelectionState(currentComposition, selectionManager, impl_->trackRows_,
                           true);
        Q_EMIT timelineDebugMessage(
            QStringLiteral("Cleaned %1 %2")
                .arg(removedCount)
                .arg(formatKeyframeNoun(removedCount)));
        update();
        event->accept();
        return;
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
        // This view only emits projectChanged after a local edit has already
        // been applied. Do not add ArtifactProjectService signal/slot wiring
        // here; keep refresh propagation on the service side or via the local
        // EventBus in higher-level widgets.
        svc->duplicateLayerInCurrentComposition(clip.layerId);
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
  if (layer && (chosen == moveStartClipAct || chosen == trimInClipAct ||
                  chosen == trimOutClipAct || chosen == rippleTrimOutClipAct ||
                  chosen == rippleTrimInClipAct ||
                  chosen == rippleDeleteClipAct)) {
      const qint64 currentFrame = static_cast<qint64>(std::llround(
          std::clamp(impl_->currentFrame_, 0.0,
                     std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)))));
      bool changed = false;
      if (chosen == moveStartClipAct) {
        changed = applyTimelineLayerRangeEdit(layer, currentFrame, 0, true);
      } else if (chosen == trimInClipAct) {
        changed = applyTimelineLayerRangeEdit(layer, currentFrame, 0, false);
      } else if (chosen == trimOutClipAct) {
        const qint64 duration = layer->outPoint().framePosition() -
                                layer->inPoint().framePosition();
        changed = applyTimelineLayerRangeEdit(layer, currentFrame - duration,
                                              duration, false);
      } else if (chosen == rippleTrimOutClipAct) {
        const auto beforeLayers =
            collectRippleLaterLayers(composition, layer->id(),
                                     layer->outPoint().framePosition());
        QVector<ArtifactAbstractLayerPtr> snapshotLayers;
        snapshotLayers.reserve(beforeLayers.size() + 1);
        snapshotLayers.push_back(layer);
        for (const auto &laterLayer : beforeLayers) {
          snapshotLayers.push_back(laterLayer);
        }
        const auto beforeSnapshots =
            captureTimelineLayerStateSnapshots(composition, snapshotLayers);
        if (auto *mgr = UndoManager::instance()) {
          mgr->push(std::make_unique<RippleTrimOutCommand>(
              composition->id(), layer->id(), currentFrame,
              std::move(beforeSnapshots)));
          changed = true;
        } else {
          changed = applyTimelineRippleTrimOut(composition->id(),
                                               layer->id().toString(),
                                               currentFrame);
        }
      } else if (chosen == rippleTrimInClipAct) {
        // Phase 2: Ripple Trim In。target と後続を snapshot に束ねて1コマンド化。
        const auto beforeLayers =
            collectRippleLaterLayers(composition, layer->id(),
                                     layer->inPoint().framePosition());
        QVector<ArtifactAbstractLayerPtr> snapshotLayers;
        snapshotLayers.reserve(beforeLayers.size() + 1);
        snapshotLayers.push_back(layer);
        for (const auto &laterLayer : beforeLayers) {
          snapshotLayers.push_back(laterLayer);
        }
        const auto beforeSnapshots =
            captureTimelineLayerStateSnapshots(composition, snapshotLayers);
        if (auto *mgr = UndoManager::instance()) {
          mgr->push(std::make_unique<RippleTrimInCommand>(
              composition->id(), layer->id(), currentFrame,
              std::move(beforeSnapshots)));
          changed = true;
        } else {
          changed = applyTimelineRippleTrimIn(composition->id(),
                                              layer->id().toString(),
                                              currentFrame);
        }
      } else if (chosen == rippleDeleteClipAct) {
        // Phase 2: Ripple Delete (Close Gap)。target を 0 幅に潰して後続を詰める。
        // レイヤー完全削除はしない（Undo の安全性のため）。完全削除が必要なら
        // 既存の「Delete Layer」を使う。
        const auto beforeLayers =
            collectRippleLaterLayers(composition, layer->id(),
                                     layer->inPoint().framePosition());
        QVector<ArtifactAbstractLayerPtr> snapshotLayers;
        snapshotLayers.reserve(beforeLayers.size() + 1);
        snapshotLayers.push_back(layer);
        for (const auto &laterLayer : beforeLayers) {
          snapshotLayers.push_back(laterLayer);
        }
        const auto beforeSnapshots =
            captureTimelineLayerStateSnapshots(composition, snapshotLayers);
        if (auto *mgr = UndoManager::instance()) {
          mgr->push(std::make_unique<RippleDeleteCommand>(
              composition->id(), layer->id(),
              std::move(beforeSnapshots)));
          changed = true;
        } else {
          changed = applyTimelineRippleDelete(composition->id(),
                                              layer->id().toString());
        }
      }
      if (changed) {
        // This view only emits projectChanged after a local edit has already
        // been applied. Do not add ArtifactProjectService signal/slot wiring
        // here; keep refresh propagation on the service side or via the local
        // EventBus in higher-level widgets.
        Q_EMIT timelineDebugMessage(
            QStringLiteral("Edited %1 at F%2")
                .arg(clip.title.isEmpty() ? clip.clipId : clip.title)
                .arg(currentFrame));
      }
      event->accept();
      return;
    }
  }

  if (!canEditFocusedProperty ||
      (chosen != addKeyframeAct && chosen != removeKeyframeAct &&
       chosen != addMarkerFrameAct && chosen != removeMarkerFrameAct)) {
    event->accept();
    return;
  }

  const bool removeKeyframes =
      (chosen == removeKeyframeAct || chosen == removeMarkerFrameAct);
  const qint64 editFrame = contextFrame;
  const auto layer =
      composition ? composition->layerById(targetLayerId)
                  : ArtifactAbstractLayerPtr{};
  const bool changed = applyKeyframeEditAtFrame(
      composition, layer, targetPropertyPath, editFrame, removeKeyframes);

  if (changed) {
    const QString actionText =
        removeKeyframes ? QStringLiteral("Removed") : QStringLiteral("Added");
    Q_EMIT timelineDebugMessage(
        QStringLiteral("%1 keyframe at F%2 for %3")
            .arg(actionText)
            .arg(editFrame)
            .arg(targetPropertyPath));
    update();
  }

  event->accept();
}

void ArtifactTimelineTrackPainterView::wheelEvent(QWheelEvent *event) {
  if (!event) {
    return;
  }

  const QPoint angle = event->angleDelta();
  if (angle.isNull()) {
    event->ignore();
    return;
  }

  if (event->modifiers() & Qt::ControlModifier) {
    const double steps = static_cast<double>(angle.y()) / 120.0;
    if (steps == 0.0) {
      event->ignore();
      return;
    }

    if (event->modifiers() & Qt::AltModifier) {
      const double scale = std::pow(1.12, steps);
      QVector<int> resizedHeights = impl_->trackHeights_;
      const int oldHeight = resizedHeights.isEmpty() ? kDefaultTrackHeight
                                                     : resizedHeights.front();
      int newHeight =
          static_cast<int>(std::lround(static_cast<double>(oldHeight) * scale));
      newHeight = std::clamp(newHeight, 16, 160);
      if (newHeight == oldHeight) {
        event->ignore();
        return;
      }
      for (auto &height : resizedHeights) {
        height = std::clamp(
            static_cast<int>(std::lround(static_cast<double>(height) * scale)),
            16, 160);
      }
      setTrackHeights(resizedHeights);
      Q_EMIT trackRowHeightChanged(newHeight);
      event->accept();
      return;
    }

    const double mouseX = event->position().x();
    const double oldPpf = std::max<double>(0.001, static_cast<double>(impl_->pixelsPerFrame_));
    const double anchorFrame = (mouseX + impl_->horizontalOffset_) / oldPpf;
    const double scale = std::pow(1.12, steps);
    const double newPpf = std::clamp(oldPpf * scale, 0.05, 64.0);
    const double newOffset = anchorFrame * newPpf - mouseX;

    setPixelsPerFrame(newPpf);
    setHorizontalOffset(newOffset);
    Q_EMIT zoomLevelChanged(newPpf * 100.0);
    event->accept();
    return;
  }

  const double delta = static_cast<double>(angle.y()) / 120.0 * 40.0;
  if (event->modifiers() & Qt::ShiftModifier) {
    setHorizontalOffset(impl_->horizontalOffset_ - delta);
  } else {
    setVerticalOffset(std::max(0.0, impl_->verticalOffset_ - delta));
  }
  event->accept();
}

void ArtifactTimelineTrackPainterView::keyPressEvent(QKeyEvent *event) {
  if (!event || !impl_) {
    QWidget::keyPressEvent(event);
    return;
  }

  const auto &shortcuts = ArtifactCore::ShortcutBindings::instance();
  const auto zoomTimelineBy = [this](const double scale) {
    const double oldPpf = std::max<double>(0.001, static_cast<double>(impl_->pixelsPerFrame_));
    const double newPpf = clampPixelsPerFrame(oldPpf * scale);
    if (std::abs(newPpf - oldPpf) < 0.0001) {
      return false;
    }
    const double anchorX = static_cast<double>(std::max(1, width())) * 0.5;
    const double anchorFrame = (anchorX + impl_->horizontalOffset_) / oldPpf;
    setPixelsPerFrame(newPpf);
    setHorizontalOffset(anchorFrame * newPpf - anchorX);
    Q_EMIT zoomLevelChanged(newPpf * 100.0);
    return true;
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
    const double oldPpf = std::max<double>(0.001, static_cast<double>(impl_->pixelsPerFrame_));
    if (std::abs(oldPpf - 2.0) > 0.0001) {
      const double anchorX = static_cast<double>(std::max(1, width())) * 0.5;
      const double anchorFrame = (anchorX + impl_->horizontalOffset_) / oldPpf;
      setPixelsPerFrame(2.0);
      setHorizontalOffset(anchorFrame * 2.0 - anchorX);
      Q_EMIT zoomLevelChanged(200.0);
      event->accept();
      return;
    }
  }

  if (shortcuts.matches(event, ArtifactCore::ShortcutId::LayerDeleteSelected)) {
    const auto selection = ArtifactLayerSelectionManager::instance();
    const auto selectedLayers = selection ? selection->selectedLayers()
                                          : QSet<ArtifactAbstractLayerPtr>{};
    if (!selectedLayers.isEmpty() && deleteSelectedLayersFromTimeline(this)) {
      event->accept();
      return;
    }
  }

  const int key = event->key();
  auto selectedIndices = selectedMarkerIndices(impl_->keyframeMarkers_);
  if (selectedIndices.isEmpty() && impl_->hoverMarkerIndex_ >= 0 &&
      impl_->hoverMarkerIndex_ < impl_->keyframeMarkers_.size()) {
    selectedIndices.push_back(impl_->hoverMarkerIndex_);
  }
  if (!selectedIndices.isEmpty()) {
    const int key = event->key();
    if (key == Qt::Key_Escape) {
      clearKeyframeSelection();
      event->accept();
      return;
    }
    if (shortcuts.matches(event,
                          ArtifactCore::ShortcutId::TimelineRemoveKeyframeAtPlayhead) ||
        key == Qt::Key_Backspace) {
      if (deleteSelectedKeyframeMarkers()) {
        event->accept();
        return;
      }
      QWidget::keyPressEvent(event);
      return;
    }
  }
  if (resolveTimelineAction(event) == ArtifactTimelineAction::CleanKeyframes) {
    ArtifactCompositionPtr currentComposition;
    if (auto *svc = ArtifactProjectService::instance()) {
      currentComposition = svc->currentComposition().lock();
    }
    QVector<KeyframeMarkerVisual> cleanTargets = selectedKeyframeMarkers();
    if (cleanTargets.isEmpty() && impl_->hoverMarkerIndex_ >= 0 &&
        impl_->hoverMarkerIndex_ < impl_->keyframeMarkers_.size()) {
      cleanTargets.push_back(impl_->keyframeMarkers_[impl_->hoverMarkerIndex_]);
    }
    const QVector<KeyframePropertyRef> cleanTargetRefs =
        collectPropertyRefsFromMarkers(cleanTargets);
    if (currentComposition && !cleanTargetRefs.isEmpty()) {
      const auto beforeSnapshots =
          captureKeyframePropertySnapshots(currentComposition, cleanTargetRefs);
      int removedCount = 0;
      const bool changed = cleanNearDuplicateKeyframes(currentComposition,
                                                       cleanTargetRefs,
                                                       &removedCount);
      if (changed) {
        const auto afterSnapshots =
            captureKeyframePropertySnapshots(currentComposition, cleanTargetRefs);
        if (auto *mgr = UndoManager::instance()) {
          QPointer<ArtifactTimelineTrackPainterView> self(this);
          mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
              QStringLiteral("Clean Keyframes"),
              [self, currentComposition, afterSnapshots]() {
                applyKeyframePropertySnapshots(currentComposition,
                                               afterSnapshots);
                if (!self) {
                  return;
                }
                ArtifactLayerSelectionManager *selectionManager = nullptr;
                if (auto *app = ArtifactApplicationManager::instance()) {
                  selectionManager = app->layerSelectionManager();
                }
                self->syncSelectionState(currentComposition, selectionManager,
                                         self->impl_->trackRows_, true);
              },
              [self, currentComposition, beforeSnapshots]() {
                applyKeyframePropertySnapshots(currentComposition,
                                               beforeSnapshots);
                if (!self) {
                  return;
                }
                ArtifactLayerSelectionManager *selectionManager = nullptr;
                if (auto *app = ArtifactApplicationManager::instance()) {
                  selectionManager = app->layerSelectionManager();
                }
                self->syncSelectionState(currentComposition, selectionManager,
                                         self->impl_->trackRows_, true);
              }));
        }

        ArtifactLayerSelectionManager *selectionManager = nullptr;
        if (auto *app = ArtifactApplicationManager::instance()) {
          selectionManager = app->layerSelectionManager();
        }
        syncSelectionState(currentComposition, selectionManager, impl_->trackRows_,
                           true);
        Q_EMIT timelineDebugMessage(
            QStringLiteral("Cleaned %1 %2")
                .arg(removedCount)
                .arg(formatKeyframeNoun(removedCount)));
        update();
      }
    }
    event->accept();
    return;
  }
  if (key == Qt::Key_D && (event->modifiers() & Qt::ControlModifier)) {
    if (duplicateSelectedKeyframeMarkersAtCurrentFrame()) {
      event->accept();
      return;
    }
    QWidget::keyPressEvent(event);
    return;
  }
  if (key == Qt::Key_X && (event->modifiers() & Qt::ControlModifier)) {
    if (triggerTimelineShortcut(this, Qt::Key_C, Qt::ControlModifier) &&
        deleteSelectedKeyframeMarkers()) {
      event->accept();
      return;
    }
    QWidget::keyPressEvent(event);
    return;
  }
  if (key != Qt::Key_Left && key != Qt::Key_Right) {
    QWidget::keyPressEvent(event);
    return;
  }

  const qint64 stepFrames = (event->modifiers() & Qt::ShiftModifier) ? 10 : 1;
  const qint64 deltaFrames = (key == Qt::Key_Left) ? -stepFrames : stepFrames;
  if (deltaFrames == 0) {
    event->ignore();
    return;
  }

  struct MoveRequest {
    LayerID layerId;
    QString propertyPath;
    qint64 fromFrame = -1;
    qint64 toFrame = -1;
    QString oldKey;
    QString newKey;
  };

  QVector<MoveRequest> requests;
  requests.reserve(selectedIndices.size());
  // locked layer の keyframe は nudge 対象から除外する（Phase 4: 境界安全性）。
  ArtifactCompositionPtr composition;
  if (auto *svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  for (const int idx : selectedIndices) {
    if (idx < 0 || idx >= impl_->keyframeMarkers_.size()) {
      continue;
    }
    const auto &marker = impl_->keyframeMarkers_[idx];
    // レイヤーが locked ならスキップ。
    if (composition) {
      const auto layer = composition->layerById(marker.layerId);
      if (layer && layer->isLocked()) {
        continue;
      }
    }
    const qint64 fromFrame = static_cast<qint64>(
        std::llround(std::clamp(marker.frame, 0.0,
                                std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)))));
    const qint64 toFrame =
        std::clamp(fromFrame + deltaFrames, qint64(0),
                   static_cast<qint64>(
                       std::llround(std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)))));
    if (fromFrame == toFrame) {
      continue;
    }

    MoveRequest request;
    request.layerId = marker.layerId;
    request.propertyPath = marker.propertyPath;
    request.fromFrame = fromFrame;
    request.toFrame = toFrame;
    request.oldKey =
        keyframeSelectionKey(marker.layerId, marker.propertyPath, fromFrame);
    request.newKey =
        keyframeSelectionKey(marker.layerId, marker.propertyPath, toFrame);
    requests.push_back(std::move(request));
  }

  if (requests.isEmpty()) {
    event->accept();
    return;
  }

  QSet<QString> nextSelectedKeys = impl_->selectedMarkerKeys_;
  for (const auto &request : requests) {
    nextSelectedKeys.remove(request.oldKey);
    nextSelectedKeys.insert(request.newKey);
  }

  applyMarkerSelectionSet(impl_->keyframeMarkers_, impl_->selectedMarkerKeys_,
                          nextSelectedKeys);
  Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
  for (const auto &request : requests) {
    Q_EMIT keyframeMoveRequested(request.layerId, request.propertyPath,
                                 request.fromFrame, request.toFrame);
  }
  Q_EMIT timelineDebugMessage(
      QStringLiteral("Moved %1 %3 by %2 %4%5")
          .arg(requests.size())
          .arg(deltaFrames)
          .arg(formatKeyframeNoun(requests.size()))
          .arg(formatFrameUnit(static_cast<qint64>(std::llround(deltaFrames))))
          .arg((event->modifiers() & Qt::AltModifier)
                   ? QStringLiteral(" (snap override)")
                   : QString()));
  update();
  event->accept();
}

void ArtifactTimelineTrackPainterView::leaveEvent(QEvent *event) {
  QWidget::leaveEvent(event);

  const bool hadHover =
      impl_->hoverClipIndex_ >= 0 || impl_->hoverEdge_ != DragMode::None;
  const bool hadMarkerHover = impl_->hoverMarkerIndex_ >= 0;
  impl_->hoverClipIndex_ = -1;
  impl_->hoverEdge_ = DragMode::None;
  impl_->hoverMarkerIndex_ = -1;
  impl_->pendingBackgroundPress_ = false;
  impl_->marqueeSelecting_ = false;
  impl_->marqueeSelectionRect_ = QRect();
  impl_->marqueeAnchorSelectionKeys_.clear();
  impl_->draggingMarker_ = false;
  impl_->dragMarkerIndex_ = -1;
  impl_->dragMarkerTargetFrame_ = 0.0;
  impl_->dragMarkerSnapLabel_.clear();
  impl_->dragMarkerSelectionIndices_.clear();
  impl_->dragMarkerSelectionOrigFrames_.clear();
  impl_->pendingMarkerSingleClick_ = false;
  impl_->pendingMarkerSingleClickKey_.clear();
  impl_->pendingMarkerSingleClickLabel_.clear();
  impl_->pendingMarkerSingleClickFrame_ = 0.0;
  impl_->hoverToolTipText_.clear();
  QToolTip::hideText();

  if (impl_->dragMode_ == DragMode::None) {
    setCursor(Qt::ArrowCursor);
  }

  if (hadHover || hadMarkerHover) {
    update();
  }
  Q_UNUSED(event);
}

void ArtifactTimelineTrackPainterView::dragEnterEvent(
    QDragEnterEvent *event) {
  const QStringList validPaths = collectDroppedAssetPaths(event->mimeData());
  if (!validPaths.isEmpty()) {
    event->acceptProposedAction();
    return;
  }
  event->ignore();
}

void ArtifactTimelineTrackPainterView::dragMoveEvent(QDragMoveEvent *event) {
  const QStringList validPaths = collectDroppedAssetPaths(event->mimeData());
  if (!validPaths.isEmpty()) {
    event->acceptProposedAction();
    return;
  }
  event->ignore();
}

void ArtifactTimelineTrackPainterView::dragLeaveEvent(
    QDragLeaveEvent *event) {
  event->accept();
}

void ArtifactTimelineTrackPainterView::dropEvent(QDropEvent *event) {
  const QStringList validPaths = collectDroppedAssetPaths(event->mimeData());
  if (validPaths.isEmpty()) {
    event->ignore();
    return;
  }

  enqueueDroppedTimelineAssets(validPaths);
  event->acceptProposedAction();
}

} // namespace Artifact
