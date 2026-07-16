module;
#include <algorithm>
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
#include <limits>
#include <optional>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMenu>
#include <QToolButton>
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
import Settings.Accessibility;
import Clipboard.ClipboardManager;
import ArtifactCore.Utils.PerformanceProfiler;
import Widgets.Utils.CSS;
import Artifact.Widgets.Timeline;
import Artifact.Application.Manager;
import Artifact.Service.Application;
import Artifact.Tool.Service;
import Artifact.Tool.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Text;
import Artifact.Layer.InitParams;
import Artifact.Layers.Selection.Manager;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Audio.Waveform;
import Artifact.Timeline.KeyframeModel;
import Artifact.Timeline.KeyBinding;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Service.ActiveContext;
import Event.Bus;
import Artifact.Event.Types;
import Translation.Manager;
import File.TypeDetector;
import Frame.Position;
import Property.Abstract;
import Undo.UndoManager;
import Time.Rational;
import UI.ShortcutBindings;
import Utils.Path;

namespace Artifact {

// AE Easy Ease (F9) 互換: 選択キーフレームの前後キーフレームから velocity に基づき
// ベジェ制御点を自動算出する。スカラー値のみ対応し、非スカラー値（カラー等）や
// 隣接キーフレームがない場合は AE 標準の固定ハンドルへフォールバックする。
bool tryComputeEasyEaseHandles(const std::vector<ArtifactCore::KeyFrame>& keyframes,
                               const ArtifactCore::KeyFrame& cur,
                               const bool easeIn,
                               const bool easeOut,
                               float& outCp1x, float& outCp1y,
                               float& outCp2x, float& outCp2y)
{
    const auto toScalar = [](const QVariant& v, bool& ok) -> double {
        ok = v.canConvert<double>() && v.userType() != QMetaType::QColor
             && v.userType() != QMetaType::QVector2D
             && v.userType() != QMetaType::QVector3D
             && v.userType() != QMetaType::QVector4D;
        return ok ? v.toDouble() : 0.0;
    };

    bool curOk = false;
    const double curVal = toScalar(cur.value, curOk);
    if (!curOk) {
        return false;
    }

    const ArtifactCore::KeyFrame* prev = nullptr;
    const ArtifactCore::KeyFrame* next = nullptr;
    for (const auto& kf : keyframes) {
        if (kf.time < cur.time) {
            prev = &kf;
        } else if (kf.time > cur.time) {
            next = &kf;
            break;
        }
    }
    if (!prev || !next) {
        return false;
    }

    bool prevOk = false;
    bool nextOk = false;
    const double prevVal = toScalar(prev->value, prevOk);
    const double nextVal = toScalar(next->value, nextOk);
    if (!prevOk || !nextOk) {
        return false;
    }

    const double d1 = (cur.time - prev->time).toDouble();
    const double d2 = (next->time - cur.time).toDouble();
    if (d1 <= 0.0 || d2 <= 0.0) {
        return false;
    }
    const double v1 = curVal - prevVal;
    const double v2 = nextVal - curVal;

    const double dSum = d1 + d2;
    const double vSum = std::fabs(v1) + std::fabs(v2);
    const double vEps = 1e-6;

    // AE 互換: in-tangent を区間の 1/3、out-tangent を 2/3 の time 位置に置く。
    // value 方向は前後速度に比例させ、slow-in / slow-out で各ハンドルを減衰。
    float inX = static_cast<float>(d1 / (3.0 * dSum));
    float inY = static_cast<float>(v1 / (3.0 * (vSum + vEps)));
    float outX = static_cast<float>(d2 / (3.0 * dSum) + d2 / (3.0 * dSum));
    float outY = static_cast<float>(v2 / (3.0 * (vSum + vEps)));

    if (easeIn) {
        inY *= 0.0f;
        inX = std::min(inX, 0.33f);
    }
    if (easeOut) {
        outY *= 0.0f;
        outX = std::max(outX, 0.67f);
    }

    outCp1x = inX;
    outCp1y = inY;
    outCp2x = outX;
    outCp2y = outY;
    return true;
}

W_OBJECT_IMPL(ArtifactTimelineTrackPainterView)

namespace {
QString tt(const char* key, const char* fallback)
{
  return Artifact::TranslationManager::instance().tr(QString::fromUtf8(key), QString::fromUtf8(fallback));
}

QMessageBox::StandardButton centeredQuestion(QWidget* parent,
                                             const QString& title,
                                             const QString& text)
{
  QMessageBox box(parent ? parent->window() : nullptr);
  box.setWindowTitle(title);
  box.setIcon(QMessageBox::Question);
  box.setText(text);
  box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
  box.setDefaultButton(QMessageBox::No);
  box.adjustSize();
  if (QWidget* owner = parent ? parent->window() : nullptr) {
    const QRect ownerRect = owner->frameGeometry();
    const QSize dialogSize = box.sizeHint();
    box.move(ownerRect.center() -
             QPoint(dialogSize.width() / 2, dialogSize.height() / 2));
  }
  return static_cast<QMessageBox::StandardButton>(box.exec());
}

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

bool moveTimelineLayer(const CompositionID &compositionId,
                       const QString &layerIdText,
                       const qint64 startFrame,
                       const qint64 durationFrame) {
  auto *svc = ArtifactProjectService::instance();
  if (!svc || layerIdText.trimmed().isEmpty()) {
    return false;
  }
  auto result = svc->findComposition(compositionId);
  if (!result.success) {
    return false;
  }
  const auto composition = result.ptr.lock();
  if (!composition) {
    return false;
  }
  const auto layer = composition->layerById(LayerID(layerIdText));
  if (!layer) {
    return false;
  }
  layer->setInPoint(FramePosition(startFrame));
  layer->setOutPoint(FramePosition(std::max<qint64>(startFrame + 1, startFrame + durationFrame)));
  layer->setStartTime(FramePosition(startFrame));
  layer->changed();
  return true;
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

bool hasAcceptedDroppedAssetUrl(const QMimeData *mime) {
  if (!mime || !mime->hasUrls()) {
    return false;
  }
  // dragMoveEvent can fire many times per second. Keep hover validation free
  // of filesystem probes; the drop path performs the authoritative exists /
  // directory checks once before importing.
  for (const auto &url : mime->urls()) {
    if (url.isLocalFile() &&
        isAcceptedDroppedLayerType(inferDroppedLayerType(url.toLocalFile()))) {
      return true;
    }
  }
  return false;
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
    confirmed = centeredQuestion(parent, QStringLiteral("Delete Layer"),
                                 message) == QMessageBox::Yes;
  } else {
    confirmed =
        centeredQuestion(parent, QStringLiteral("Delete Layers"),
                         QStringLiteral("Delete %1 selected layers?")
                             .arg(layerIds.size())) == QMessageBox::Yes;
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

static int edgeHitZone() {
    return Artifact::Accessibility::scaledSize(kEdgeHitZone);
}

enum class DragMode { None, MoveBody, ResizeLeft, ResizeRight, SlideBody };

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
  QVariant endValue;
  QRectF bodyRect;
  QRectF leftHandleRect;
  QRectF rightHandleRect;
};

struct KeyframeAreaHitResult {
  int areaIndex = -1;
  KeyframeAreaHitPart part = KeyframeAreaHitPart::None;
};

struct KeyframeFrameMoveRequest {
  LayerID layerId;
  QString propertyPath;
  qint64 fromFrame = -1;
  qint64 toFrame = -1;
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
                     start.frame, end.frame, start.value, end.value, bodyRect,
                     leftHandleRect, rightHandleRect});
  }
  return areas;
}

QColor keyframeAreaTintFor(const KeyframeAreaVisual& area, const TimelineThemeColors& theme)
{
  auto numeric = [](const QVariant& v, bool* ok) -> double {
    if (v.canConvert<double>()) {
      return v.toDouble(ok);
    }
    if (ok) {
      *ok = false;
    }
    return 0.0;
  };

  bool okStart = false;
  bool okEnd = false;
  const double startValue = numeric(area.value, &okStart);
  const double endValue = numeric(area.endValue, &okEnd);
  if (!okStart || !okEnd) {
    QColor neutral = theme.accent;
    neutral.setAlpha(52);
    return neutral;
  }

  const double delta = endValue - startValue;
  if (std::abs(delta) < 1e-6) {
    QColor neutral = theme.text;
    neutral.setAlpha(50);
    return neutral;
  }

  QColor tint = delta > 0.0 ? QColor(255, 134, 95) : QColor(90, 170, 255);
  tint.setAlpha(60);
  return tint;
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
                             const QString &propertyPath, qint64 frame);

struct KeyframePropertyRef {
  LayerID layerId;
  QString propertyPath;
};

struct KeyframePropertySnapshot {
  LayerID layerId;
  QString propertyPath;
  std::vector<ArtifactCore::KeyFrame> keyframes;
};

QVector<KeyframePropertyRef> collectPropertyRefsFromMarkers(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers);
QVector<KeyframePropertySnapshot> captureKeyframePropertySnapshots(
    const ArtifactCompositionPtr &composition,
    const QVector<KeyframePropertyRef> &refs);
void applyKeyframePropertySnapshots(
    const ArtifactCompositionPtr &composition,
    const QVector<KeyframePropertySnapshot> &snapshots);

struct InterpolationChangeRecord;

QSet<QString> keyframeSelectionSetForArea(const KeyframeAreaVisual &area) {
  QSet<QString> selection;
  selection.insert(keyframeSelectionKey(
      area.layerId, area.propertyPath,
      static_cast<qint64>(std::llround(area.startFrame))));
  selection.insert(keyframeSelectionKey(
      area.layerId, area.propertyPath,
      static_cast<qint64>(std::llround(area.endFrame))));
  return selection;
}

std::optional<KeyframeAreaVisual> selectedKeyframeAreaFromMarkers(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    const QSet<QString> &selectedKeys, const QVector<int> &heights,
    const QVector<int> &trackTops, const double ppf, const double xOffset,
    const double yOffset) {
  if (selectedKeys.size() != 2) {
    return std::nullopt;
  }
  const auto areas =
      collectKeyframeAreas(markers, heights, trackTops, ppf, xOffset, yOffset);
  for (const auto &area : areas) {
    if (keyframeSelectionSetForArea(area) == selectedKeys) {
      return area;
    }
  }
  return std::nullopt;
}

bool applyValueToKeyframeArea(
    const ArtifactCompositionPtr &composition, const KeyframeAreaVisual &area,
    const QVariant &value, QVector<KeyframePropertySnapshot> *outBeforeSnapshots,
    QVector<KeyframePropertySnapshot> *outAfterSnapshots) {
  if (!composition || area.layerId.isNil() || area.propertyPath.trimmed().isEmpty()) {
    return false;
  }
  const QVector<KeyframePropertyRef> refs{{area.layerId, area.propertyPath}};
  const auto beforeSnapshots = captureKeyframePropertySnapshots(composition, refs);
  if (beforeSnapshots.isEmpty()) {
    return false;
  }
  auto afterSnapshots = beforeSnapshots;
  const double fps =
      std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
  const int64_t scale = static_cast<int64_t>(std::llround(fps));
  const RationalTime startTime(
      static_cast<qint64>(std::llround(area.startFrame)), scale);
  const RationalTime endTime(
      static_cast<qint64>(std::llround(area.endFrame)), scale);

  bool changed = false;
  for (auto &snapshot : afterSnapshots) {
    if (snapshot.layerId != area.layerId ||
        snapshot.propertyPath != area.propertyPath) {
      continue;
    }
    for (auto &keyframe : snapshot.keyframes) {
      if (keyframe.time == startTime || keyframe.time == endTime) {
        if (keyframe.value != value) {
          keyframe.value = value;
          changed = true;
        }
      }
    }
  }
  if (!changed) {
    return false;
  }

  if (outBeforeSnapshots) {
    *outBeforeSnapshots = beforeSnapshots;
  }
  if (outAfterSnapshots) {
    *outAfterSnapshots = afterSnapshots;
  }
  applyKeyframePropertySnapshots(composition, afterSnapshots);
  return true;
}

bool applyNumericValueToSelectedKeyframeMarkers(
    const ArtifactCompositionPtr &composition,
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    const double numericValue, QVector<KeyframePropertySnapshot> *outBeforeSnapshots,
    QVector<KeyframePropertySnapshot> *outAfterSnapshots) {
  if (!composition || markers.isEmpty()) {
    return false;
  }

  const auto refs = collectPropertyRefsFromMarkers(markers);
  const auto beforeSnapshots = captureKeyframePropertySnapshots(composition, refs);
  if (beforeSnapshots.isEmpty()) {
    return false;
  }

  const int fpsScale =
      std::max(1, static_cast<int>(std::llround(composition->frameRate().framerate())));
  QSet<QString> selectedKeys;
  for (const auto &marker : markers) {
    const qint64 frame = static_cast<qint64>(std::llround(marker.frame));
    selectedKeys.insert(keyframeSelectionKey(marker.layerId, marker.propertyPath, frame));
  }

  auto afterSnapshots = beforeSnapshots;
  bool changed = false;
  for (auto &snapshot : afterSnapshots) {
    const auto layer = composition->layerById(snapshot.layerId);
    if (!layer) {
      continue;
    }
    const auto property = findLayerPropertyByPath(layer, snapshot.propertyPath);
    if (!property) {
      continue;
    }
    const auto type = property->getType();
    if (type != ArtifactCore::PropertyType::Float &&
        type != ArtifactCore::PropertyType::Integer) {
      continue;
    }

    for (auto &keyframe : snapshot.keyframes) {
      const qint64 frame = keyframe.time.rescaledTo(fpsScale);
      const QString key =
          keyframeSelectionKey(snapshot.layerId, snapshot.propertyPath, frame);
      if (!selectedKeys.contains(key)) {
        continue;
      }
      const QVariant nextValue =
          type == ArtifactCore::PropertyType::Integer
              ? QVariant(static_cast<qint64>(std::llround(numericValue)))
              : QVariant(numericValue);
      if (keyframe.value == nextValue) {
        continue;
      }
      keyframe.value = nextValue;
      changed = true;
    }
  }

  if (!changed) {
    return false;
  }

  if (outBeforeSnapshots) {
    *outBeforeSnapshots = beforeSnapshots;
  }
  if (outAfterSnapshots) {
    *outAfterSnapshots = afterSnapshots;
  }
  applyKeyframePropertySnapshots(composition, afterSnapshots);
  return true;
}

bool sameKeyframeLane(
    const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual &lhs,
    const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual &rhs) {
  return lhs.layerId == rhs.layerId && lhs.propertyPath == rhs.propertyPath &&
         lhs.trackIndex == rhs.trackIndex;
}

double keyframeAreaBodyTargetDelta(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    const KeyframeAreaVisual &area, const double rawDeltaFrames,
    const double maxFrame) {
  double minDelta = -area.startFrame;
  double maxDelta = maxFrame - area.endFrame;
  if (area.startMarkerIndex > 0) {
    const auto &prev = markers[area.startMarkerIndex - 1];
    const auto &start = markers[area.startMarkerIndex];
    if (sameKeyframeLane(prev, start)) {
      minDelta = std::max(minDelta, prev.frame + 1.0 - area.startFrame);
    }
  }
  if (area.endMarkerIndex + 1 < markers.size()) {
    const auto &next = markers[area.endMarkerIndex + 1];
    const auto &end = markers[area.endMarkerIndex];
    if (sameKeyframeLane(next, end)) {
      maxDelta = std::min(maxDelta, next.frame - 1.0 - area.endFrame);
    }
  }
  return std::clamp(rawDeltaFrames, minDelta, maxDelta);
}

double keyframeAreaEdgeTargetFrame(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    const KeyframeAreaVisual &area, const KeyframeAreaHitPart part,
    const double rawTargetFrame, const double maxFrame) {
  if (part == KeyframeAreaHitPart::LeftEdge) {
    double minFrame = 0.0;
    if (area.startMarkerIndex > 0) {
      const auto &prev = markers[area.startMarkerIndex - 1];
      const auto &start = markers[area.startMarkerIndex];
      if (sameKeyframeLane(prev, start)) {
        minFrame = prev.frame + 1.0;
      }
    }
    return std::clamp(rawTargetFrame, minFrame, area.endFrame - 1.0);
  }

  if (part == KeyframeAreaHitPart::RightEdge) {
    double maxAllowedFrame = maxFrame;
    if (area.endMarkerIndex + 1 < markers.size()) {
      const auto &next = markers[area.endMarkerIndex + 1];
      const auto &end = markers[area.endMarkerIndex];
      if (sameKeyframeLane(next, end)) {
        maxAllowedFrame = next.frame - 1.0;
      }
    }
    return std::clamp(rawTargetFrame, area.startFrame + 1.0, maxAllowedFrame);
  }

  return rawTargetFrame;
}

QString keyframeSelectionKey(const LayerID &layerId,
                             const QString &propertyPath, const qint64 frame) {
  return QStringLiteral("%1|%2|%3")
      .arg(layerId.toString(), propertyPath, QString::number(frame));
}

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
    if (ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup(
            group.name())) {
      continue;
    }
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
    if (ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup(
            group.name())) {
      continue;
    }
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

bool applyTimelineLayerSlide(const CompositionID &compositionId,
                             const QString &layerIdText,
                             const qint64 newStartFrame) {
  if (layerIdText.trimmed().isEmpty()) {
    return false;
  }

  const auto composition = lookupTimelineComposition(compositionId);
  if (!composition) {
    return false;
  }

  const auto layer = composition->layerById(LayerID(layerIdText));
  if (!layer || layer->isTimingLocked()) {
    return false;
  }

  const qint64 oldInPoint = layer->inPoint().framePosition();
  const qint64 oldOutPoint = layer->outPoint().framePosition();
  const qint64 oldDuration = std::max<qint64>(1, oldOutPoint - oldInPoint);
  const qint64 inPoint = std::max<qint64>(0, newStartFrame);
  const qint64 outPoint = std::max<qint64>(inPoint + 1, inPoint + oldDuration);
  const qint64 inPointDelta = inPoint - oldInPoint;

  layer->setInPoint(FramePosition(inPoint));
  layer->setOutPoint(FramePosition(outPoint));

  if (inPointDelta != 0) {
    shiftAnimatableLayerKeyframes(composition, layer, inPointDelta);
  }

  layer->changed();
  ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
      LayerChangedEvent{compositionId.toString(), layerIdText,
                        LayerChangedEvent::ChangeType::Modified});
  return true;
}

class SlideClipCommand final : public UndoCommand {
public:
  SlideClipCommand(CompositionID compositionId, LayerID layerId,
                   qint64 newStartFrame,
                   QVector<TimelineLayerStateSnapshot> beforeSnapshots)
      : compositionId_(std::move(compositionId)),
        layerId_(std::move(layerId)), newStartFrame_(newStartFrame),
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
    if (applyTimelineLayerSlide(compositionId_, layerId_.toString(),
                                newStartFrame_)) {
      if (auto *mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
      }
    }
  }

  QString label() const override { return QStringLiteral("Slide Clip"); }

private:
  CompositionID compositionId_;
  LayerID layerId_;
  qint64 newStartFrame_ = 0;
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

bool isPercentScalePropertyPath(const QString &propertyPath) {
  return propertyPath.compare(QStringLiteral("transform.scale.x"),
                              Qt::CaseInsensitive) == 0 ||
         propertyPath.compare(QStringLiteral("transform.scale.y"),
                              Qt::CaseInsensitive) == 0;
}

bool isDegreeRotationPropertyPath(const QString &propertyPath) {
  return propertyPath.compare(QStringLiteral("transform.rotation"),
                              Qt::CaseInsensitive) == 0;
}

QString formatTimelinePropertyValue(const QString &propertyPath,
                                    const QVariant &value) {
  bool numeric = false;
  const double numericValue = value.toDouble(&numeric);
  if (!numeric) {
    return value.toString();
  }

  if (isPercentScalePropertyPath(propertyPath)) {
    return QStringLiteral("%1%").arg(
        QLocale::system().toString(numericValue * 100.0, 'f', 0));
  }
  if (isDegreeRotationPropertyPath(propertyPath)) {
    return QStringLiteral("%1 deg").arg(
        QLocale::system().toString(numericValue, 'f', 1));
  }
  return QLocale::system().toString(numericValue, 'f', 3);
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
  qint64 targetStartFrame = 0;
  double scale = 1.0;
  double valueScale = 1.0;
  double valueOffset = 0.0;
  bool useTargetStartFrame = false;
  bool reverseOrder = false;
  int randomJitter = 0;
};

QVariant transformKeyframeValue(const ArtifactCore::AbstractProperty *property,
                                const QVariant &value,
                                const KeyframeRangeTransformOptions &options) {
  if (!value.isValid() || !property) {
    return value;
  }

  switch (property->getType()) {
  case ArtifactCore::PropertyType::Float:
  case ArtifactCore::PropertyType::Integer: {
    const double base = value.toDouble();
    const double transformed =
        options.kind == KeyframeRangeTransformKind::ScaleValues
            ? base * options.valueScale
            : base + options.valueOffset;
    return property->getType() == ArtifactCore::PropertyType::Integer
               ? QVariant(static_cast<int>(std::llround(transformed)))
               : QVariant(transformed);
  }
  case ArtifactCore::PropertyType::Boolean:
    return value;
  case ArtifactCore::PropertyType::Color:
    return value;
  default:
    return value;
  }
}

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

      if (options.useTargetStartFrame &&
          options.kind != KeyframeRangeTransformKind::ScaleValues &&
          options.kind != KeyframeRangeTransformKind::OffsetValues) {
        newFrame += options.targetStartFrame - firstFrame;
      }

      ArtifactCore::KeyFrame keyframe;
      keyframe.time = RationalTime(newFrame, fpsScale);
      keyframe.value = transformKeyframeValue(
          property.get(), record.value.isValid() ? record.value : property->getValue(),
          options);
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
    const bool wasBezier = before.interpolation == ArtifactCore::InterpolationType::Bezier;
    if (interpolationType == ArtifactCore::InterpolationType::Bezier && !wasBezier) {
      after.cp1_x = 0.42f;
      after.cp1_y = 0.0f;
      after.cp2_x = 0.58f;
      after.cp2_y = 1.0f;
    } else if (interpolationType == ArtifactCore::InterpolationType::EaseIn
               || interpolationType == ArtifactCore::InterpolationType::EaseOut
               || interpolationType == ArtifactCore::InterpolationType::EaseInOut) {
      // AE Easy Ease: velocity に基づき Bezier ハンドルを自動算出する。
      after.interpolation = ArtifactCore::InterpolationType::Bezier;
      float cp1x = 0.42f, cp1y = 0.0f, cp2x = 0.58f, cp2y = 1.0f;
      const bool easeIn = interpolationType == ArtifactCore::InterpolationType::EaseIn
                          || interpolationType == ArtifactCore::InterpolationType::EaseInOut;
      const bool easeOut = interpolationType == ArtifactCore::InterpolationType::EaseOut
                           || interpolationType == ArtifactCore::InterpolationType::EaseInOut;
      if (tryComputeEasyEaseHandles(keyframes, before, easeIn, easeOut, cp1x, cp1y, cp2x, cp2y)) {
        after.cp1_x = cp1x;
        after.cp1_y = cp1y;
        after.cp2_x = cp2x;
        after.cp2_y = cp2y;
      } else {
        after.cp1_x = 0.42f;
        after.cp1_y = 0.0f;
        after.cp2_x = 0.58f;
        after.cp2_y = 1.0f;
      }
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
      return tt("timeline.kind_audio", "Kind: Audio");
    case ArtifactTimelineTrackPainterView::TrackClipVisual::Kind::Video:
      return tt("timeline.kind_video", "Kind: Video");
    case ArtifactTimelineTrackPainterView::TrackClipVisual::Kind::Generic:
    default:
      return tt("timeline.kind_generic", "Kind: Generic");
    }
  }();
  const QString startText = tt("timeline.start_frame", "Start: F%1")
                                .arg(QString::number(clip.startFrame, 'f', 1));
  const QString endText =
      tt("timeline.end_frame", "End: F%1")
          .arg(QString::number(clip.startFrame + clip.durationFrame, 'f', 1));
  const QString durationText =
      tt("timeline.duration_frames", "Duration: %1 frames")
          .arg(QString::number(clip.durationFrame, 'f', 1));
  const QString stateText = clip.selected ? tt("timeline.state_selected", "State: Selected")
                                          : tt("timeline.state_idle", "State: Idle");
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
      tt("timeline.frame", "Frame: F%1").arg(QString::number(marker.frame, 'f', 1));
  const QString pathText = tt("timeline.path", "Path: %1").arg(marker.propertyPath);
  const QString laneText = marker.laneCount > 1 ? tt("timeline.lane_with_count", "Lane: %1/%2")
                                                      .arg(marker.laneIndex + 1)
                                                      .arg(marker.laneCount)
                                                : tt("timeline.lane_single", "Lane: 1/1");
  const QString easingText = tt("timeline.incoming_outgoing", "Incoming: %1 | Outgoing: %2")
                                 .arg(marker.incomingEased ? tt("timeline.eased", "eased")
                                                           : tt("timeline.linear", "linear"))
                                 .arg(marker.outgoingEased ? tt("timeline.eased", "eased")
                                                           : tt("timeline.linear", "linear"));
  const QString interpolationText =
      tt("timeline.interpolation_value", "Interpolation: %1")
          .arg(keyframeInterpolationLabel(marker.interpolation));
  const QString anchorText =
      tt("timeline.anchor_value", "Anchor: %1").arg(keyframeAnchorLabel(marker.anchor));
  const QString rovingText =
      marker.roving ? tt("timeline.roving_on", "Roving: On")
                    : tt("timeline.roving_off", "Roving: Off");
  const QString colorText = marker.labelColor.isValid()
                                ? tt("timeline.label_value", "Label: %1").arg(marker.labelColor.name())
                                : tt("timeline.label_none", "Label: None");
  const QString selectionText =
      marker.selected
          ? tt("timeline.selection_keyframe", "Selection: Selected keyframe")
          : (marker.selectedLayer ? tt("timeline.selection_layer", "Selection: Selected layer")
                                  : tt("timeline.selection_idle", "Selection: Idle"));
  const QString relationText =
      markerAtCurrentFrame(marker, currentFrame)
          ? tt("timeline.relation_current", "Relation: At current frame")
          : (nearestToCurrent ? tt("timeline.relation_nearest", "Relation: Nearest to current")
                              : tt("timeline.relation_off_current", "Relation: Off current"));
  const QString hoverText =
      hovered ? tt("timeline.state_hovered", "State: Hovered")
              : tt("timeline.state_visible", "State: Visible");
  const QString valueText = marker.value.isValid()
      ? tt("timeline.keyframe_value", "Value: %1")
            .arg(formatTimelinePropertyValue(marker.propertyPath, marker.value))
      : tt("timeline.value_none", "Value: N/A");
  QString tooltip = label + QStringLiteral("\n") + frameText + QStringLiteral("\n") +
         valueText + QStringLiteral("\n") +
         pathText + QStringLiteral("\n") + laneText + QStringLiteral("\n") +
         easingText + QStringLiteral("\n") + interpolationText +
         QStringLiteral("\n") + anchorText + QStringLiteral("\n") +
         rovingText + QStringLiteral("\n") + colorText + QStringLiteral("\n") +
         selectionText +
         QStringLiteral("\n") + relationText + QStringLiteral("\n") +
         hoverText;
  if (marker.selected) {
    tooltip += QStringLiteral("\n") +
        tt("timeline.shortcuts_keyframe",
           "Shortcuts: Delete/Backspace remove, Ctrl+D duplicate, Left/Right move, Shift+Left/Right move 10f, Shift+Drag=time lock, Ctrl+Drag=value lock, O=proportional, [ ]=radius");
  }
  if (marker.bezier || marker.incomingBezier || marker.outgoingBezier) {
    tooltip += QStringLiteral("\n") +
        tt("timeline.bezier_active", "Bezier handles available in Mini or Curve Editor");
  }
  return tooltip;
}

QString formatKeyframeDragTooltip(const int selectionCount, const double deltaFrames,
                                  const double targetFrame,
                                  const QString &snapLabel,
                                  const bool proportionalEditing = false,
                                  const double proportionalRadius = 0.0) {
  const QString countText =
      selectionCount == 1 ? tt("timeline.one_keyframe", "1 keyframe")
                          : tt("timeline.many_keyframes", "%1 keyframes").arg(selectionCount);
  QString tooltip = tt("timeline.moving_keyframes", "Moving %1\nDelta: %2 frames\nTarget: F%3")
      .arg(countText)
      .arg(QString::number(deltaFrames, 'f', 1))
      .arg(QString::number(targetFrame, 'f', 1));
  if (proportionalEditing && selectionCount > 1) {
    tooltip += QStringLiteral("\n") +
        tt("timeline.proportional_editing", "Proportional: radius %1f")
            .arg(QString::number(proportionalRadius, 'f', 1));
  }
  if (!snapLabel.isEmpty()) {
    tooltip += QStringLiteral("\n") + tt("timeline.snap_label", "Snap: %1").arg(snapLabel);
  }
  return tooltip;
}

constexpr double kMinTimelineProportionalRadius = 1.0;
constexpr double kMaxTimelineProportionalRadius = 240.0;

double proportionalTimelineWeight(const double distance, const double radius) {
  if (radius <= 0.0) {
    return distance <= 0.0 ? 1.0 : 0.0;
  }
  if (distance >= radius) {
    return 0.0;
  }
  const double t = std::clamp(1.0 - (distance / radius), 0.0, 1.0);
  return t * t * (3.0 - 2.0 * t);
}

double proportionalTimelineDelta(const double originalFrame,
                                 const double pivotFrame,
                                 const double pivotDeltaFrames,
                                 const bool proportionalEditing,
                                 const double proportionalRadius,
                                 const int selectionCount) {
  if (!proportionalEditing || selectionCount <= 1) {
    return pivotDeltaFrames;
  }
  return pivotDeltaFrames *
         proportionalTimelineWeight(std::abs(originalFrame - pivotFrame),
                                    proportionalRadius);
}

double proportionalTimelineScaledFrame(const double originalFrame,
                                       const double draggedEdgeOriginalFrame,
                                       const double pivotFrame,
                                       const double targetDraggedEdgeFrame,
                                       const bool proportionalEditing,
                                       const double proportionalRadius,
                                       const int selectionCount) {
  const double originalSpan = draggedEdgeOriginalFrame - pivotFrame;
  if (!proportionalEditing || selectionCount <= 2 ||
      std::abs(originalSpan) < 0.0001) {
    const double targetSpan = targetDraggedEdgeFrame - pivotFrame;
    const double scale = targetSpan / originalSpan;
    return pivotFrame + (originalFrame - pivotFrame) * scale;
  }
  const double targetSpan = targetDraggedEdgeFrame - pivotFrame;
  const double globalScale = targetSpan / originalSpan;
  const double weight = proportionalTimelineWeight(
      std::abs(originalFrame - draggedEdgeOriginalFrame),
      proportionalRadius);
  const double weightedScale = 1.0 + (globalScale - 1.0) * weight;
  return pivotFrame + (originalFrame - pivotFrame) * weightedScale;
}

void drawTimelineProportionalGuide(
    QPainter &p, const QRect &dirtyRect, const TimelineThemeColors &theme,
    const QVector<int> &trackHeights, const QVector<int> &trackTops,
    const QVector<int> &trackIndices, const double ppf, const double xOffset,
    const double yOffset, const double pivotFrame, const double radius) {
  if (radius <= 0.0 || trackIndices.isEmpty()) {
    return;
  }

  const qreal leftX = static_cast<qreal>((pivotFrame - radius) * ppf - xOffset);
  const qreal rightX = static_cast<qreal>((pivotFrame + radius) * ppf - xOffset);
  const qreal centerX = static_cast<qreal>(pivotFrame * ppf - xOffset);
  const qreal bandLeft = std::min(leftX, rightX);
  const qreal bandWidth = std::max<qreal>(1.0, std::abs(rightX - leftX));

  QColor fill = theme.accent;
  fill.setAlpha(18);
  QColor border = theme.accent.lighter(126);
  border.setAlpha(144);
  QColor center = theme.accent.lighter(150);
  center.setAlpha(200);

  for (const int trackIndex : trackIndices) {
    if (trackIndex < 0 || trackIndex >= trackHeights.size()) {
      continue;
    }
    const int trackTop = trackTopAt(trackTops, trackHeights, trackIndex);
    const int trackH = trackHeights[trackIndex];
    const qreal laneTop = static_cast<qreal>(trackTop + 3.0 - yOffset);
    const qreal laneBottom = static_cast<qreal>(trackTop + trackH - 3.0 - yOffset);
    const QRectF guideRect(bandLeft, laneTop, bandWidth, laneBottom - laneTop);
    if (!guideRect.intersects(QRectF(dirtyRect))) {
      continue;
    }

    p.fillRect(guideRect, fill);
    p.setPen(QPen(border, 1.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(leftX, laneTop), QPointF(leftX, laneBottom));
    p.drawLine(QPointF(rightX, laneTop), QPointF(rightX, laneBottom));
    p.setPen(QPen(center, 1.2, Qt::DashLine, Qt::RoundCap));
    p.drawLine(QPointF(centerX, laneTop), QPointF(centerX, laneBottom));
  }
}

QString formatKeyframeAreaTooltip(const KeyframeAreaVisual &area,
                                  const bool selected,
                                  const QString &modeText = QString()) {
  const QString label = ArtifactTimelineKeyframeModel::displayLabelForPropertyPath(
      area.propertyPath);
  const qint64 startFrame = static_cast<qint64>(std::llround(area.startFrame));
  const qint64 endFrame = static_cast<qint64>(std::llround(area.endFrame));
  const qint64 durationFrames = std::max<qint64>(1, endFrame - startFrame);
  QStringList lines;
  lines << (label.isEmpty() ? tt("timeline.keyframe_area", "Keyframe Area") : label);
  lines << tt("timeline.area_range", "Range: F%1 - F%2")
               .arg(startFrame)
               .arg(endFrame);
  lines << tt("timeline.area_duration", "Span: %1 frames").arg(durationFrames);
  if (area.value.isValid()) {
    lines << tt("timeline.area_value", "Value: %1")
                 .arg(formatTimelinePropertyValue(area.propertyPath, area.value));
  }
  lines << (selected ? tt("timeline.state_selected", "State: Selected")
                     : tt("timeline.state_idle", "State: Idle"));
  if (!modeText.isEmpty()) {
    lines << modeText;
  }
  return lines.join(QStringLiteral("\n"));
}

QString appendRamPreviewPriorityHint(const QString &tooltipText) {
  auto *playback = ArtifactPlaybackService::instance();
  if (!playback) {
    return tooltipText;
  }
  const auto currentFrame = playback->currentFrame().framePosition();
  const QString priorityReason = playback->ramPreviewPriorityReason(currentFrame);
  if (priorityReason.trimmed().isEmpty()) {
    return tooltipText;
  }
  const QString hint = QStringLiteral("RAM preview: %1").arg(priorityReason);
  return tooltipText.isEmpty() ? hint : tooltipText + QStringLiteral("\n") + hint;
}

QString formatKeyframeCollisionLabel(const int count) {
  return count == 1
             ? tt("timeline.collision_single", "collides with 1 existing keyframe")
             : tt("timeline.collision_many", "collides with %1 existing keyframes").arg(count);
}

QString formatKeyframeNoun(const int count) {
  return count == 1 ? tt("timeline.keyframe_singular", "keyframe") : tt("timeline.keyframe_plural", "keyframes");
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
  return count == 1 ? tt("timeline.frame_singular", "frame") : tt("timeline.frame_plural", "frames");
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
  case ArtifactCore::InterpolationType::BounceIn:
  case ArtifactCore::InterpolationType::BounceOut:
  case ArtifactCore::InterpolationType::BounceInOut:
  case ArtifactCore::InterpolationType::ElasticIn:
  case ArtifactCore::InterpolationType::ElasticOut:
  case ArtifactCore::InterpolationType::ElasticInOut:
  case ArtifactCore::InterpolationType::BackOut:
  case ArtifactCore::InterpolationType::BackIn:
  case ArtifactCore::InterpolationType::BackInOut:
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
    return tt("timeline.lock_to_in", "Lock to In");
  case ArtifactCore::KeyFrame::Anchor::LockToOut:
    return tt("timeline.lock_to_out", "Lock to Out");
  case ArtifactCore::KeyFrame::Anchor::StretchWithLayer:
    return tt("timeline.stretch_with_layer", "Stretch with Layer");
  case ArtifactCore::KeyFrame::Anchor::Absolute:
  default:
    return tt("timeline.absolute", "Absolute");
  }
}

QString keyframeInterpolationLabel(const ArtifactCore::InterpolationType type) {
  switch (type) {
  case ArtifactCore::InterpolationType::Constant:
    return tt("timeline.hold", "Hold");
  case ArtifactCore::InterpolationType::EaseIn:
    return tt("timeline.ease_in", "Ease In");
  case ArtifactCore::InterpolationType::EaseOut:
    return tt("timeline.ease_out", "Ease Out");
  case ArtifactCore::InterpolationType::EaseInOut:
    return tt("timeline.ease_in_out", "Ease In/Out");
  case ArtifactCore::InterpolationType::Bezier:
    return tt("timeline.bezier", "Bezier");
  case ArtifactCore::InterpolationType::BounceIn:
    return tt("timeline.bounce_in", "Bounce In");
  case ArtifactCore::InterpolationType::BounceOut:
    return tt("timeline.bounce_out", "Bounce Out");
  case ArtifactCore::InterpolationType::BounceInOut:
    return tt("timeline.bounce_in_out", "Bounce In/Out");
  case ArtifactCore::InterpolationType::ElasticIn:
    return tt("timeline.elastic_in", "Elastic In");
  case ArtifactCore::InterpolationType::ElasticOut:
    return tt("timeline.elastic_out", "Elastic Out");
  case ArtifactCore::InterpolationType::ElasticInOut:
    return tt("timeline.elastic_in_out", "Elastic In/Out");
  case ArtifactCore::InterpolationType::BackIn:
    return tt("timeline.back_in", "Back In");
  case ArtifactCore::InterpolationType::BackOut:
    return tt("timeline.back", "Back");
  case ArtifactCore::InterpolationType::BackInOut:
    return tt("timeline.back_in_out", "Back In/Out");
  case ArtifactCore::InterpolationType::Sine:
    return tt("timeline.sine", "Sine");
  case ArtifactCore::InterpolationType::Cubic:
    return tt("timeline.cubic", "Cubic");
  case ArtifactCore::InterpolationType::Exponential:
    return tt("timeline.exponential", "Exponential");
  case ArtifactCore::InterpolationType::Linear:
  default:
    return tt("timeline.linear", "Linear");
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
  case ArtifactCore::InterpolationType::BounceIn:
  case ArtifactCore::InterpolationType::BounceOut:
  case ArtifactCore::InterpolationType::BounceInOut:
  case ArtifactCore::InterpolationType::ElasticIn:
  case ArtifactCore::InterpolationType::ElasticOut:
  case ArtifactCore::InterpolationType::ElasticInOut:
  case ArtifactCore::InterpolationType::BackOut:
  case ArtifactCore::InterpolationType::BackIn:
  case ArtifactCore::InterpolationType::BackInOut:
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

double snapTimelineFrameToEditTargets(
    const double rawFrame, const QVector<ArtifactTimelineTrackPainterView::TrackClipVisual> &clips,
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> &markers,
    const double currentFrame, const Qt::KeyboardModifiers modifiers,
    QString *outSnapLabel = nullptr, const int ignoreClipIndex = -1) {
  double targetFrame = rawFrame;
  if (outSnapLabel) {
    outSnapLabel->clear();
  }

  if (modifiers & Qt::AltModifier) {
    if (outSnapLabel) {
      *outSnapLabel = QStringLiteral("snap override");
    }
    return targetFrame;
  }

  auto trySnap = [&](const double candidate, const QString &label) {
    if (std::abs(targetFrame - candidate) <= kKeyframeSnapToPlayheadThresholdFrames) {
      targetFrame = candidate;
      if (outSnapLabel) {
        *outSnapLabel = label;
      }
      return true;
    }
    return false;
  };

  if (modifiers & Qt::ShiftModifier) {
    targetFrame = std::round(targetFrame / 10.0) * 10.0;
    if (outSnapLabel) {
      *outSnapLabel = QStringLiteral("10 frame increments");
    }
  }

  if (trySnap(currentFrame, QStringLiteral("current frame"))) {
    return targetFrame;
  }

  if (auto *svc = ArtifactProjectService::instance()) {
    if (auto comp = svc->currentComposition().lock()) {
      const ArtifactCore::FrameRange workArea = comp->workAreaRange();
      if (trySnap(static_cast<double>(workArea.start()), QStringLiteral("work area start")) ||
          trySnap(static_cast<double>(workArea.end()), QStringLiteral("work area end"))) {
        return targetFrame;
      }
    }
  }

  for (int i = 0; i < clips.size(); ++i) {
    if (i == ignoreClipIndex) {
      continue;
    }
    const auto &clip = clips.at(i);
    if (trySnap(clip.startFrame, QStringLiteral("layer in point")) ||
        trySnap(clip.startFrame + clip.durationFrame, QStringLiteral("layer out point"))) {
      return targetFrame;
    }
  }

  for (const auto &marker : markers) {
    if (trySnap(marker.frame, QStringLiteral("keyframe"))) {
      return targetFrame;
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
    const double xOffset, const double yOffset,
    const DragMode bodyMode = DragMode::MoveBody) {
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
    if (mouseX >= clipX - edgeHitZone() && mouseX <= clipX + edgeHitZone())
      return {DragMode::ResizeLeft, i};
    if (mouseX >= clipX + clipW - edgeHitZone() &&
        mouseX <= clipX + clipW + edgeHitZone())
      return {DragMode::ResizeRight, i};
    if (mouseX > clipX && mouseX < clipX + clipW)
      return {bodyMode, i};
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

QPointF markerHandlePositionFor(
    const ArtifactTimelineTrackPainterView::KeyframeMarkerVisual &marker,
    const QVector<int> &heights, const QVector<int> &trackTops,
    const double ppf, const double xOffset, const double yOffset,
    const bool incoming) {
  const QPointF center =
      markerCenterFor(marker, heights, trackTops, ppf, xOffset, yOffset);
  if (center.isNull()) {
    return {};
  }
  const int trackHeight =
      marker.trackIndex >= 0 && marker.trackIndex < heights.size()
          ? heights[marker.trackIndex]
          : kDefaultTrackHeight;
  const qreal valueScale =
      std::max<qreal>(2.0, static_cast<qreal>(trackHeight) * 0.25);
  const qreal frameOffset =
      incoming ? marker.inHandleFrameOffset : marker.outHandleFrameOffset;
  const qreal valueOffset =
      incoming ? marker.inHandleValueOffset : marker.outHandleValueOffset;
  if (std::abs(frameOffset) < 0.001 && std::abs(valueOffset) < 0.0001) {
    return {};
  }
  return QPointF(center.x() + frameOffset * ppf,
                 center.y() - valueOffset * valueScale);
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
  const qreal size = marker.laneCount > 1
      ? static_cast<qreal>(Artifact::Accessibility::scaledSize(10))
      : static_cast<qreal>(Artifact::Accessibility::scaledSize(11));
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

int nearestMarkerIndexByFrameOrder(
    const QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>
        &markers,
    const QVector<int> &sortedIndices, const double currentFrame) {
  if (sortedIndices.isEmpty()) {
    return -1;
  }

  auto frameAt = [&markers, &sortedIndices](const int sortedIndex) {
    return markers[sortedIndices[sortedIndex]].frame;
  };

  int lower = 0;
  int upper = sortedIndices.size();
  while (lower < upper) {
    const int mid = lower + ((upper - lower) / 2);
    if (frameAt(mid) < currentFrame) {
      lower = mid + 1;
    } else {
      upper = mid;
    }
  }

  int bestIndex = -1;
  double bestDistance = std::numeric_limits<double>::max();
  int left = lower - 1;
  int right = lower;
  while (left >= 0 || right < sortedIndices.size()) {
    const double leftDistance =
        left >= 0 ? std::abs(frameAt(left) - currentFrame)
                  : std::numeric_limits<double>::max();
    const double rightDistance =
        right < sortedIndices.size()
            ? std::abs(frameAt(right) - currentFrame)
            : std::numeric_limits<double>::max();
    const double nextDistance = std::min(leftDistance, rightDistance);
    if (bestIndex >= 0 && nextDistance > bestDistance) {
      break;
    }

    const bool useLeft = leftDistance <= rightDistance;
    const int markerIndex = sortedIndices[useLeft ? left-- : right++];
    if (markerAtCurrentFrame(markers[markerIndex], currentFrame)) {
      continue;
    }
    bestIndex = markerIndex;
    bestDistance = nextDistance;
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
  QPointF control1;
  QPointF control2;
  QColor color;
};

bool shouldDrawConnectionSegment(const KeyframeConnectionSegment &segment,
                                 const double ppf, const QRect &dirtyRect) {
  const qreal dx = segment.to.x() - segment.from.x();
  const qreal dy = segment.to.y() - segment.from.y();
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
  const qreal minX = std::min({segment.from.x(), segment.to.x(), segment.control1.x(),
                               segment.control2.x()});
  const qreal maxX = std::max({segment.from.x(), segment.to.x(), segment.control1.x(),
                               segment.control2.x()});
  const qreal minY = std::min({segment.from.y(), segment.to.y(), segment.control1.y(),
                               segment.control2.y()});
  const qreal maxY = std::max({segment.from.y(), segment.to.y(), segment.control1.y(),
                               segment.control2.y()});
  const QRectF bounds(QPointF(minX, minY), QPointF(maxX, maxY));
  const QRectF paddedBounds = bounds.normalized().adjusted(-3.0, -3.0, 3.0, 3.0);
  return dirtyRect.intersects(paddedBounds.toAlignedRect());
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
      QPointF control1((from.x() + to.x()) * 0.5, from.y());
      QPointF control2((from.x() + to.x()) * 0.5, to.y());
      if (fromMarker.outgoingBezier) {
        const QPointF handle = markerHandlePositionFor(
            fromMarker, heights, trackTops, ppf, xOffset, yOffset, false);
        if (!handle.isNull()) {
          control1 = handle;
        }
      }
      if (toMarker.incomingBezier) {
        const QPointF handle = markerHandlePositionFor(
            toMarker, heights, trackTops, ppf, xOffset, yOffset, true);
        if (!handle.isNull()) {
          control2 = handle;
        }
      }
      QColor color = fromMarker.selectedLayer ? fromMarker.color.lighter(120)
                                              : fromMarker.color.lighter(108);
      color.setAlpha(fromMarker.selected ? 110 : (fromMarker.selectedLayer ? 92 : 72));
      segments.push_back({from, to, control1, control2, color});
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
      if (ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup(
              group.name())) {
        continue;
      }
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
  int dragHandleMarkerIndex_ = -1;
  bool dragHandleIncoming_ = false;
  double dragHandleOrigFrameOffset_ = 0.0;
  double dragHandleOrigValueOffset_ = 0.0;
  bool draggingHandle_ = false;
  int dragAreaIndex_ = -1;
  KeyframeAreaHitPart dragAreaPart_ = KeyframeAreaHitPart::None;
  QPoint dragMarkerStartPoint_;
  QPoint dragAreaStartPoint_;
  double dragMarkerOrigFrame_ = 0.0;
  double dragMarkerTargetFrame_ = 0.0;
  qint64 dragMarkerLastPaintFrame_ = std::numeric_limits<qint64>::min();
  double dragAreaOrigStartFrame_ = 0.0;
  double dragAreaOrigEndFrame_ = 0.0;
  double dragAreaPivotFrame_ = 0.0;
  QString dragMarkerSnapLabel_;
  bool proportionalEditingEnabled_ = false;
  double proportionalEditRadius_ = 12.0;

  // Waveform render cache: keyed by clipId, stores rendered QImage + rect size
  struct WaveformRenderEntry {
    QImage image;
    QSizeF size;
  };
  QHash<QString, WaveformRenderEntry> waveformRenderCache_;
  QString dragAreaSnapLabel_;
  QVector<int> dragMarkerSelectionIndices_;
  QVector<double> dragMarkerSelectionOrigFrames_;
  QVector<QVariant> dragMarkerSelectionOrigValues_;
  QVector<KeyframePropertySnapshot> dragMarkerBeforeSnapshots_;
  QVector<std::optional<ArtifactCore::PropertyType>> dragMarkerSelectionOrigTypes_;
  QVector<int> dragAreaSelectionIndices_;
  QVector<double> dragAreaSelectionOrigFrames_;
  QVariant dragAreaValue_;
  bool pendingMarkerSingleClick_ = false;
  QString pendingMarkerSingleClickKey_;
  QString pendingMarkerSingleClickLabel_;
  double pendingMarkerSingleClickFrame_ = 0.0;
  bool draggingMarker_ = false;
  bool dragMarkerValueChanged_ = false;
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
  QVector<int> selectedMarkerFrameSortedIndices_;
  QVector<int> selectedLayerMarkerFrameSortedIndices_;
  QVector<int> normalMarkerFrameSortedIndices_;
  QVector<int> keyframeCountsByTrack_;
  QSet<int> selectedMarkerTracks_;
  QSet<int> selectedKeyframeTracks_;
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
  void rebuildMarkerCaches();
  int nearestMarkerIndexForFrame(const double frame) const;
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

void ArtifactTimelineTrackPainterView::Impl::rebuildMarkerCaches() {
  keyframeCountsByTrack_.resize(trackHeights_.size());
  keyframeCountsByTrack_.fill(0);
  selectedMarkerFrameSortedIndices_.clear();
  selectedLayerMarkerFrameSortedIndices_.clear();
  normalMarkerFrameSortedIndices_.clear();
  selectedMarkerTracks_.clear();
  selectedKeyframeTracks_.clear();

  for (int i = 0; i < keyframeMarkers_.size(); ++i) {
    const auto &marker = keyframeMarkers_[i];
    if (marker.trackIndex >= 0 && marker.trackIndex < keyframeCountsByTrack_.size()) {
      ++keyframeCountsByTrack_[marker.trackIndex];
    }
    if (marker.selected) {
      selectedKeyframeTracks_.insert(marker.trackIndex);
      selectedMarkerFrameSortedIndices_.push_back(i);
    } else if (marker.selectedLayer) {
      selectedLayerMarkerFrameSortedIndices_.push_back(i);
    } else {
      normalMarkerFrameSortedIndices_.push_back(i);
    }
    if (marker.selectedLayer) {
      selectedMarkerTracks_.insert(marker.trackIndex);
    }
  }

  auto sortByFrame = [this](QVector<int> &indices) {
    std::sort(indices.begin(), indices.end(), [this](const int lhs, const int rhs) {
      const auto &left = keyframeMarkers_[lhs];
      const auto &right = keyframeMarkers_[rhs];
      if (left.frame != right.frame) {
        return left.frame < right.frame;
      }
      return lhs < rhs;
    });
  };
  sortByFrame(selectedMarkerFrameSortedIndices_);
  sortByFrame(selectedLayerMarkerFrameSortedIndices_);
  sortByFrame(normalMarkerFrameSortedIndices_);
}

int ArtifactTimelineTrackPainterView::Impl::nearestMarkerIndexForFrame(
    const double frame) const {
  if (const int selected = nearestMarkerIndexByFrameOrder(
          keyframeMarkers_, selectedMarkerFrameSortedIndices_, frame);
      selected >= 0) {
    return selected;
  }
  if (const int selectedLayer = nearestMarkerIndexByFrameOrder(
          keyframeMarkers_, selectedLayerMarkerFrameSortedIndices_, frame);
      selectedLayer >= 0) {
    return selectedLayer;
  }
  return nearestMarkerIndexByFrameOrder(keyframeMarkers_,
                                        normalMarkerFrameSortedIndices_, frame);
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
  const int oldNearestMarkerIndex = impl_->nearestMarkerIndexForFrame(oldFrame);
  impl_->currentFrame_ = sanitized;
  const int newNearestMarkerIndex =
      impl_->nearestMarkerIndexForFrame(impl_->currentFrame_);
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
  impl_->rebuildMarkerCaches();
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
  impl_->rebuildMarkerCaches();
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
  impl_->rebuildMarkerCaches();
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
      impl_->rebuildMarkerCaches();
      Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
      update();
    }
    return;
  }
  impl_->keyframeMarkers_ = markers;
  impl_->rebuildMarkerCaches();
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

bool ArtifactTimelineTrackPainterView::reverseSelectedKeyframeMarkers() {
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
  const auto beforeSnapshots =
      captureKeyframePropertySnapshots(composition, refs);
  const QSet<QString> beforeSelectionKeys = impl_->selectedMarkerKeys_;

  KeyframeRangeTransformOptions options;
  options.kind = KeyframeRangeTransformKind::Reverse;
  QSet<QString> nextSelectionKeys;
  int affectedCount = 0;
  const bool changed = applySelectedKeyframeRangeTransform(
      composition, targets, options, &nextSelectionKeys, &affectedCount);
  if (!changed) {
    return false;
  }

  const auto afterSnapshots =
      captureKeyframePropertySnapshots(composition, refs);
  if (auto *mgr = UndoManager::instance()) {
    QPointer<ArtifactTimelineTrackPainterView> self(this);
    const QSet<QString> afterSelectionKeys = nextSelectionKeys;
    mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
        QStringLiteral("Reverse Selected Keyframes"),
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

  impl_->selectedMarkerKeys_ = std::move(nextSelectionKeys);
  ArtifactLayerSelectionManager *selectionManager = nullptr;
  if (auto *app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }
  syncSelectionState(composition, selectionManager, impl_->trackRows_, true);
  Q_EMIT timelineDebugMessage(
      QStringLiteral("Reversed %1 %2")
          .arg(affectedCount)
          .arg(formatKeyframeNoun(affectedCount)));
  update();
  return true;
}

namespace {
QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual>
collectAllKeyframeMarkersForLayers(
    const QVector<ArtifactAbstractLayerPtr> &layers) {
  QVector<ArtifactTimelineTrackPainterView::KeyframeMarkerVisual> markers;
  for (const auto &layer : layers) {
    if (!layer) {
      continue;
    }
    for (const auto &group : layer->getLayerPropertyGroups()) {
      if (ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup(
              group.name())) {
        continue;
      }
      for (const auto &property : group.sortedProperties()) {
        if (!property || !property->isAnimatable()) {
          continue;
        }
        const auto keyframes = property->getKeyFrames();
        for (const auto &keyframe : keyframes) {
          ArtifactTimelineTrackPainterView::KeyframeMarkerVisual marker;
          marker.layerId = layer->id();
          marker.propertyPath = property->getName();
          marker.frame = static_cast<double>(
              keyframe.time.rescaledTo(keyframe.time.scale()));
          marker.value = keyframe.value;
          marker.interpolation = keyframe.interpolation;
          marker.anchor = keyframe.anchor;
          marker.roving = keyframe.roving;
          markers.push_back(std::move(marker));
        }
      }
    }
  }
  return markers;
}

bool reverseAllKeyframeMarkersWithSelection(
    ArtifactTimelineTrackPainterView *self,
    const ArtifactCompositionPtr &composition,
    const QVector<ArtifactAbstractLayerPtr> &layers,
    const QVector<TimelineRowDescriptor> &trackRows,
    const QSet<QString> &beforeSelectionKeys,
    const QString &undoText,
    const QString &debugText) {
  if (!self || !composition || layers.isEmpty()) {
    return false;
  }

  const auto targets = collectAllKeyframeMarkersForLayers(layers);
  if (targets.isEmpty()) {
    return false;
  }

  const auto refs = collectPropertyRefsFromMarkers(targets);
  const auto beforeSnapshots =
      captureKeyframePropertySnapshots(composition, refs);

  KeyframeRangeTransformOptions options;
  options.kind = KeyframeRangeTransformKind::Reverse;
  QSet<QString> nextSelectionKeys;
  int affectedCount = 0;
  const bool changed = applySelectedKeyframeRangeTransform(
      composition, targets, options, &nextSelectionKeys, &affectedCount);
  if (!changed) {
    return false;
  }

  const auto afterSnapshots =
      captureKeyframePropertySnapshots(composition, refs);
  if (auto *mgr = UndoManager::instance()) {
    QPointer<ArtifactTimelineTrackPainterView> view(self);
    const QSet<QString> afterSelectionKeys = nextSelectionKeys;
    mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
        undoText,
        [view, composition, afterSnapshots, afterSelectionKeys, trackRows]() {
          applyKeyframePropertySnapshots(composition, afterSnapshots);
          if (!view) {
            return;
          }
          ArtifactLayerSelectionManager *selectionManager = nullptr;
          if (auto *app = ArtifactApplicationManager::instance()) {
            selectionManager = app->layerSelectionManager();
          }
          view->syncSelectionState(composition, selectionManager, trackRows, true);
          view->setSelectedKeyframeKeys(afterSelectionKeys);
        },
        [view, composition, beforeSnapshots, beforeSelectionKeys, trackRows]() {
          applyKeyframePropertySnapshots(composition, beforeSnapshots);
          if (!view) {
            return;
          }
          ArtifactLayerSelectionManager *selectionManager = nullptr;
          if (auto *app = ArtifactApplicationManager::instance()) {
            selectionManager = app->layerSelectionManager();
          }
          view->syncSelectionState(composition, selectionManager, trackRows, true);
          view->setSelectedKeyframeKeys(beforeSelectionKeys);
        }));
  }

  self->setSelectedKeyframeKeys(nextSelectionKeys);
  ArtifactLayerSelectionManager *selectionManager = nullptr;
  if (auto *app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }
  self->syncSelectionState(composition, selectionManager, trackRows, true);
  Q_EMIT self->timelineDebugMessage(
      QStringLiteral("%1 %2 %3")
          .arg(debugText)
          .arg(affectedCount)
          .arg(formatKeyframeNoun(affectedCount)));
  self->update();
  return true;
}
} // namespace

bool ArtifactTimelineTrackPainterView::reverseAllKeyframesInCurrentLayer() {
  ArtifactCompositionPtr composition;
  if (auto *svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return false;
  }

  ArtifactAbstractLayerPtr layer;
  if (auto *app = ArtifactApplicationManager::instance()) {
    if (auto *selection = app->layerSelectionManager()) {
      layer = selection->currentLayer();
    }
  }
  if (!layer) {
    return false;
  }

  return reverseAllKeyframeMarkersWithSelection(
      this, composition, QVector<ArtifactAbstractLayerPtr>{layer},
      impl_ ? impl_->trackRows_ : QVector<TimelineRowDescriptor>{},
      impl_ ? impl_->selectedMarkerKeys_ : QSet<QString>{},
      QStringLiteral("Reverse All Keyframes in Layer"),
      QStringLiteral("Reversed all keyframes in layer:"));
}

bool ArtifactTimelineTrackPainterView::reverseAllKeyframesInSelectedLayers() {
  ArtifactCompositionPtr composition;
  if (auto *svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return false;
  }

  QVector<ArtifactAbstractLayerPtr> layers;
  if (auto *app = ArtifactApplicationManager::instance()) {
    if (auto *selection = app->layerSelectionManager()) {
      const auto selectedLayers = selection->selectedLayers();
      if (selectedLayers.isEmpty()) {
        if (auto currentLayer = selection->currentLayer()) {
          layers.push_back(currentLayer);
        }
      } else {
        if (auto currentLayer = selection->currentLayer();
            currentLayer && selectedLayers.contains(currentLayer)) {
          layers.push_back(currentLayer);
        }
        for (const auto &selectedLayer : selectedLayers) {
          if (!selectedLayer ||
              (!layers.isEmpty() && selectedLayer == layers.front())) {
            continue;
          }
          layers.push_back(selectedLayer);
        }
      }
    }
  }
  if (layers.isEmpty()) {
    return false;
  }

  return reverseAllKeyframeMarkersWithSelection(
      this, composition, layers,
      impl_ ? impl_->trackRows_ : QVector<TimelineRowDescriptor>{},
      impl_ ? impl_->selectedMarkerKeys_ : QSet<QString>{},
      QStringLiteral("Reverse All Keyframes in Selected Layers"),
      QStringLiteral("Reversed all keyframes in selected layers:"));
}

bool ArtifactTimelineTrackPainterView::reverseAllKeyframesInComposition() {
  ArtifactCompositionPtr composition;
  if (auto *svc = ArtifactProjectService::instance()) {
    composition = svc->currentComposition().lock();
  }
  if (!composition) {
    return false;
  }

  QVector<ArtifactAbstractLayerPtr> layers;
  const auto allLayers = composition->allLayer();
  layers.reserve(allLayers.size());
  for (const auto &layer : allLayers) {
    if (layer) {
      layers.push_back(layer);
    }
  }
  if (layers.isEmpty()) {
    return false;
  }

  return reverseAllKeyframeMarkersWithSelection(
      this, composition, layers,
      impl_ ? impl_->trackRows_ : QVector<TimelineRowDescriptor>{},
      impl_ ? impl_->selectedMarkerKeys_ : QSet<QString>{},
      QStringLiteral("Reverse All Keyframes in Composition"),
      QStringLiteral("Reversed all keyframes in composition:"));
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
  impl_->rebuildMarkerCaches();
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
  impl_->rebuildMarkerCaches();
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
  impl_->rebuildMarkerCaches();
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
  impl_->rebuildMarkerCaches();
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
  impl_->rebuildMarkerCaches();
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

  const auto refs = collectPropertyRefsFromMarkers(targets);
  const auto beforeSnapshots = captureKeyframePropertySnapshots(composition, refs);
  const QSet<QString> beforeSelectionKeys = impl_->selectedMarkerKeys_;
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
    const auto afterSnapshots = captureKeyframePropertySnapshots(composition, refs);
    if (auto *mgr = UndoManager::instance()) {
      QPointer<ArtifactTimelineTrackPainterView> self(this);
      mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
          QStringLiteral("Set Keyframe Anchor"),
          [self, composition, afterSnapshots, beforeSelectionKeys]() {
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
            self->setSelectedKeyframeKeys(beforeSelectionKeys);
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

  const auto refs = collectPropertyRefsFromMarkers(targets);
  const auto beforeSnapshots = captureKeyframePropertySnapshots(composition, refs);
  const QSet<QString> beforeSelectionKeys = impl_->selectedMarkerKeys_;
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
    const auto afterSnapshots = captureKeyframePropertySnapshots(composition, refs);
    if (auto *mgr = UndoManager::instance()) {
      QPointer<ArtifactTimelineTrackPainterView> self(this);
      mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
          QStringLiteral("Set Keyframe Color Label"),
          [self, composition, afterSnapshots, beforeSelectionKeys]() {
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
            self->setSelectedKeyframeKeys(beforeSelectionKeys);
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
    Q_EMIT timelineDebugMessage(
        QStringLiteral("Applied keyframe color label to %1 %2")
            .arg(applied)
            .arg(formatKeyframeNoun(applied)));
    update();
    return true;
  }
  return false;
}

bool ArtifactTimelineTrackPainterView::hasNumericSelectedKeyframes() const {
  if (!impl_) {
    return false;
  }

  ArtifactCompositionPtr currentComposition;
  if (auto *svc = ArtifactProjectService::instance()) {
    currentComposition = svc->currentComposition().lock();
  }
  if (!currentComposition) {
    return false;
  }

  const auto selectedMarkers = selectedKeyframeMarkers();
  if (selectedMarkers.isEmpty()) {
    return false;
  }

  for (const auto &marker : selectedMarkers) {
    const auto layer = currentComposition->layerById(marker.layerId);
    if (!layer) {
      return false;
    }
    const auto property = findLayerPropertyByPath(layer, marker.propertyPath);
    if (!property) {
      return false;
    }
    const auto type = property->getType();
    if (type != ArtifactCore::PropertyType::Float &&
        type != ArtifactCore::PropertyType::Integer) {
      return false;
    }
  }

  return true;
}

bool ArtifactTimelineTrackPainterView::promptSetSelectedKeyframeValue() {
  if (!impl_) {
    return false;
  }

  ArtifactCompositionPtr currentComposition;
  if (auto *svc = ArtifactProjectService::instance()) {
    currentComposition = svc->currentComposition().lock();
  }
  if (!currentComposition) {
    return false;
  }

  const QVector<KeyframeMarkerVisual> selectedMarkers = selectedKeyframeMarkers();
  if (selectedMarkers.isEmpty()) {
    return false;
  }
  if (!hasNumericSelectedKeyframes()) {
    return false;
  }

  bool accepted = false;
  const double nextValue = QInputDialog::getDouble(
      this, tt("timeline.set_keyframe_value", "Set Keyframe Value"),
      tt("timeline.keyframe_value_prompt", "Value"), 0.0, -1000000.0,
      1000000.0, 3, &accepted);
  if (!accepted) {
    return false;
  }

  QVector<KeyframePropertySnapshot> beforeSnapshots;
  QVector<KeyframePropertySnapshot> afterSnapshots;
  if (!applyNumericValueToSelectedKeyframeMarkers(
          currentComposition, selectedMarkers, nextValue, &beforeSnapshots,
          &afterSnapshots)) {
    return false;
  }

  if (auto *mgr = UndoManager::instance()) {
    QPointer<ArtifactTimelineTrackPainterView> self(this);
    const auto trackRows = impl_->trackRows_;
    const QSet<QString> selectionKeys = impl_->selectedMarkerKeys_;
    mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
        QStringLiteral("Set Keyframe Value"),
        [self, currentComposition, afterSnapshots, selectionKeys, trackRows]() {
          applyKeyframePropertySnapshots(currentComposition, afterSnapshots);
          if (!self) {
            return;
          }
          ArtifactLayerSelectionManager *selectionManager = nullptr;
          if (auto *app = ArtifactApplicationManager::instance()) {
            selectionManager = app->layerSelectionManager();
          }
          self->syncSelectionState(currentComposition, selectionManager,
                                   trackRows, true);
          self->setSelectedKeyframeKeys(selectionKeys);
        },
        [self, currentComposition, beforeSnapshots, selectionKeys, trackRows]() {
          applyKeyframePropertySnapshots(currentComposition, beforeSnapshots);
          if (!self) {
            return;
          }
          ArtifactLayerSelectionManager *selectionManager = nullptr;
          if (auto *app = ArtifactApplicationManager::instance()) {
            selectionManager = app->layerSelectionManager();
          }
          self->syncSelectionState(currentComposition, selectionManager,
                                   trackRows, true);
          self->setSelectedKeyframeKeys(selectionKeys);
        }));
  }

  ArtifactLayerSelectionManager *selectionManager = nullptr;
  if (auto *app = ArtifactApplicationManager::instance()) {
    selectionManager = app->layerSelectionManager();
  }
  syncSelectionState(currentComposition, selectionManager, impl_->trackRows_, true);
  setSelectedKeyframeKeys(impl_->selectedMarkerKeys_);
  Q_EMIT timelineDebugMessage(
      QStringLiteral("Set value on %1 %2")
          .arg(selectedMarkers.size())
          .arg(formatKeyframeNoun(selectedMarkers.size())));
  update();
  return true;
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
  if (changed || selectionChanged) {
    impl_->rebuildMarkerCaches();
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
  const QColor canvasColor = theme.background;
  p.fillRect(dirtyRect, canvasColor);

  const QRect fullRect = rect();
  const double ppf = impl_->pixelsPerFrame_;
  const double xOffset = impl_->horizontalOffset_;
  const double yOffset = impl_->verticalOffset_;
  int firstVisibleTrack = 0;
  while (firstVisibleTrack < impl_->trackHeights_.size()) {
    const int rowTop =
        impl_->trackTops_.value(firstVisibleTrack) - static_cast<int>(std::round(yOffset));
    const int rowBottom = rowTop + impl_->trackHeights_[firstVisibleTrack];
    if (rowBottom >= dirtyRect.top()) {
      break;
    }
    ++firstVisibleTrack;
  }
  int lastVisibleTrack = impl_->trackHeights_.size() - 1;
  while (lastVisibleTrack >= firstVisibleTrack) {
    const int rowTop =
        impl_->trackTops_.value(lastVisibleTrack) - static_cast<int>(std::round(yOffset));
    if (rowTop <= dirtyRect.bottom()) {
      break;
    }
    --lastVisibleTrack;
  }

  for (int i = firstVisibleTrack; i <= lastVisibleTrack; ++i) {
    const int rowH = impl_->trackHeights_[i];
    const double rowTop = impl_->trackTops_.value(i) - yOffset;
    const auto &row =
        (i >= 0 && i < impl_->trackRows_.size()) ? impl_->trackRows_.at(i)
                                                 : TimelineRowDescriptor{};
    const bool isSelectedPropertyLane =
        row.kind == TimelineRowKind::Property && !row.layerId.isNil() &&
        impl_->lastSyncedSelectedLayerIds_.contains(row.layerId);

    if (isSelectedPropertyLane) {
      QColor laneTint = theme.accent;
      laneTint.setAlpha(10);
      p.fillRect(QRectF(0.0, rowTop, fullRect.width(), rowH), laneTint);
      QColor laneStrip = theme.accent;
      laneStrip.setAlpha(104);
      p.fillRect(QRectF(0.0, rowTop, 2.0, rowH), laneStrip);
    }
    QColor rowDivider = theme.border;
    rowDivider.setAlpha(62);
    p.setPen(QPen(rowDivider, 1));
    p.drawLine(0, rowTop + rowH, fullRect.width(), rowTop + rowH);
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
    QColor gridColor = theme.border;
    gridColor.setAlpha(major ? 76 : 30);
    p.setPen(QPen(gridColor, 1));
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
    const int clipY = impl_->trackTops_.value(clip.trackIndex) -
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
    const int trackTop = impl_->trackTops_.value(clip.trackIndex);
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
      const bool slideActive = (impl_->activeTool_ == ToolType::Slide);
      const int gripWidth = slideActive ? 3 : 2;
      const int gripAlpha = slideActive ? 200 : (isHovered ? 130 : 80);
      p.setPen(QPen(QColor(255, 255, 255, gripAlpha), gripWidth,
                    Qt::SolidLine, Qt::RoundCap));
      p.drawLine(QPointF(clipRect.left() + 4.0, gripY1),
                 QPointF(clipRect.left() + 4.0, gripY2));
      p.drawLine(QPointF(clipRect.right() - 4.0, gripY1),
                 QPointF(clipRect.right() - 4.0, gripY2));
      if (slideActive) {
        p.setPen(QPen(theme.accent.lighter(140), 1.0, Qt::SolidLine, Qt::RoundCap));
        const qreal midX = clipRect.center().x();
        p.drawLine(QPointF(midX - 3.0, gripY1), QPointF(midX - 3.0, gripY2));
        p.drawLine(QPointF(midX + 3.0, gripY1), QPointF(midX + 3.0, gripY2));
      }
    }

    if (!clip.waveformPeaks.isEmpty() &&
        clipRect.width() > 20.0 && clipRect.height() > 10.0) {
      const qreal innerTop = clipRect.top() + 5.0;
      const qreal innerBottom = clipRect.bottom() - 5.0;
      const qreal centerY = (innerTop + innerBottom) * 0.5;
      const qreal halfSpan = std::max<qreal>(2.0, (innerBottom - innerTop) * 0.42);
      const QColor waveformColor = isSelected ? theme.background.darker(140)
                                              : theme.text.lighter(110);
      QColor waveformSoft = waveformColor;
      waveformSoft.setAlpha(90);
      if (!clip.waveformPeaks.isEmpty()) {
        const int binCount = clip.waveformPeaks.size();
        const bool hasRms = clip.waveformRms.size() == binCount;
        const qreal innerLeft = clipRect.left() + 4.0;
        const qreal innerRight = clipRect.right() - 4.0;
        const qreal span = std::max<qreal>(1.0, innerRight - innerLeft);

        // Batch-build all bars into a single QPainterPath for faster rasterization
        QPainterPath peakPath;
        QPainterPath rmsPath;
        peakPath.setFillRule(Qt::WindingFill);
        for (int bar = 0; bar < binCount; ++bar) {
          const qreal t = binCount > 1 ? static_cast<qreal>(bar) / static_cast<qreal>(binCount - 1) : 0.0;
          const qreal x = innerLeft + t * span;
          const qreal peak = std::clamp(static_cast<qreal>(clip.waveformPeaks[bar]), 0.0, 1.0);
          const qreal top = centerY - halfSpan * peak;
          const qreal bottom = centerY + halfSpan * peak;
          peakPath.moveTo(x, top);
          peakPath.lineTo(x, bottom);
          if (hasRms) {
            const qreal rms = std::clamp(static_cast<qreal>(clip.waveformRms[bar]), 0.0, 1.0);
            rmsPath.moveTo(x, centerY - halfSpan * rms);
            rmsPath.lineTo(x, centerY + halfSpan * rms);
          }
        }
        if (hasRms && !rmsPath.isEmpty()) {
          QColor rmsColor = waveformColor;
          rmsColor.setAlpha(90);
          p.strokePath(rmsPath, QPen(rmsColor, 1.0));
        }
        p.strokePath(peakPath, QPen(waveformColor, 1.0));
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
  QVector<int> currentFrameKeyframeCountsByTrack(impl_->trackHeights_.size(), 0);
  for (const auto &marker : impl_->keyframeMarkers_) {
    if (marker.trackIndex >= 0 &&
        marker.trackIndex < currentFrameKeyframeCountsByTrack.size()) {
      if (markerAtCurrentFrame(marker, impl_->currentFrame_)) {
        ++currentFrameKeyframeCountsByTrack[marker.trackIndex];
      }
    }
  }

  for (const int trackIndex : impl_->selectedMarkerTracks_) {
    if (trackIndex < 0 || trackIndex >= impl_->trackHeights_.size()) {
      continue;
    }
    const int trackTop = impl_->trackTops_.value(trackIndex);
    const int trackH = impl_->trackHeights_[trackIndex];
    const int keyframeCount = impl_->keyframeCountsByTrack_.value(trackIndex, 0);
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

  const bool miniEditorEnabled =
      property("timelineMiniEditorEnabled").toBool();
  LayerID miniEditorLayerId = impl_->contextLayerId_;
  QString miniEditorPropertyPath = impl_->contextPropertyPath_.trimmed();
  if (miniEditorPropertyPath.isEmpty()) {
    for (const auto &marker : impl_->keyframeMarkers_) {
      if (!marker.selected) {
        continue;
      }
      miniEditorLayerId = marker.layerId;
      miniEditorPropertyPath = marker.propertyPath.trimmed();
      break;
    }
  }
  QVector<KeyframeMarkerVisual> miniEditorMarkers;
  if (miniEditorEnabled && !miniEditorLayerId.isNil() &&
      !miniEditorPropertyPath.isEmpty()) {
    miniEditorMarkers.reserve(impl_->keyframeMarkers_.size());
    for (const auto &marker : impl_->keyframeMarkers_) {
      if (marker.layerId == miniEditorLayerId &&
          marker.propertyPath.trimmed() == miniEditorPropertyPath) {
        miniEditorMarkers.push_back(marker);
      }
    }
  }
  const QVector<KeyframeConnectionSegment> connectionSegments =
      collectKeyframeConnectionSegments(miniEditorMarkers,
                                        impl_->trackHeights_, impl_->trackTops_,
                                        ppf, xOffset, yOffset);
  for (const auto &segment : connectionSegments) {
    if (!shouldDrawConnectionSegment(segment, ppf, dirtyRect)) {
      continue;
    }
    const qreal lineWidth = std::clamp(2.2 + ppf * 0.06, 2.2, 4.0);
    QPainterPath curvePath;
    curvePath.moveTo(segment.from);
    curvePath.cubicTo(segment.control1, segment.control2, segment.to);
    p.setPen(QPen(segment.color, lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.strokePath(curvePath, p.pen());
  }

  const auto *timelineSettings = ArtifactCore::ArtifactAppSettings::instance();
  const bool timelineGhostingEnabled =
      timelineSettings && timelineSettings->timelineGhostingEnabled();
  const int timelineGhostFrameCount =
      timelineSettings ? timelineSettings->timelineGhostingFrameCount() : 3;
  const qreal timelineGhostOpacity =
      timelineSettings ? static_cast<qreal>(timelineSettings->timelineGhostingOpacity()) / 100.0
                       : 0.18;
  if (timelineGhostingEnabled && timelineGhostFrameCount > 0) {
    const qreal ghostRadius = 1.0 + static_cast<qreal>(timelineGhostFrameCount);
    for (const auto &marker : impl_->keyframeMarkers_) {
      if (marker.trackIndex < 0 ||
          marker.trackIndex >= impl_->trackHeights_.size()) {
        continue;
      }
      const qreal distance = std::abs(marker.frame - impl_->currentFrame_);
      if (distance <= 0.001 || distance > static_cast<qreal>(timelineGhostFrameCount)) {
        continue;
      }
      const QPointF center =
          markerCenterFor(marker, impl_->trackHeights_, impl_->trackTops_, ppf,
                          xOffset, yOffset);
      if (!dirtyRect.adjusted(-8, -8, 8, 8).contains(center.toPoint())) {
        continue;
      }
      const qreal fade = std::clamp(
          1.0 - (distance / static_cast<qreal>(timelineGhostFrameCount + 1)), 0.0, 1.0);
      QColor ghostColor = marker.selectedLayer ? theme.accent : marker.color;
      ghostColor.setAlphaF(std::clamp(timelineGhostOpacity * fade, 0.04, 0.28));
      const int size = marker.selectedLayer ? (marker.laneCount > 1 ? 5 : 6)
                                            : (marker.laneCount > 1 ? 4 : 5);
      const QRectF ghostRect(center.x() - size, center.y() - size, size * 2.0,
                             size * 2.0);
      const QPolygonF ghostShape =
          keyframeShapePolygon(ghostRect, marker.interpolation);
      p.setPen(QPen(ghostColor.lighter(115), marker.selected ? 1.5 : 1.1));
      p.setBrush(Qt::NoBrush);
      p.drawPolygon(ghostShape);
      p.setPen(Qt::NoPen);
      p.setBrush(ghostColor);
      p.drawEllipse(center, ghostRadius, ghostRadius);
    }
  }

  for (const int trackIndex : impl_->selectedKeyframeTracks_) {
    if (trackIndex < 0 || trackIndex >= impl_->trackHeights_.size()) {
      continue;
    }
    const int trackTop = impl_->trackTops_.value(trackIndex);
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
    if (impl_->proportionalEditingEnabled_ &&
        impl_->dragMarkerSelectionIndices_.size() > 1) {
      QSet<int> trackIndices;
      trackIndices.reserve(impl_->dragMarkerSelectionIndices_.size());
      for (const int selectedIndex : impl_->dragMarkerSelectionIndices_) {
        if (selectedIndex < 0 ||
            selectedIndex >= impl_->keyframeMarkers_.size()) {
          continue;
        }
        trackIndices.insert(impl_->keyframeMarkers_[selectedIndex].trackIndex);
      }
      QVector<int> trackIndexList;
      trackIndexList.reserve(trackIndices.size());
      for (const int trackIndex : trackIndices) {
        trackIndexList.push_back(trackIndex);
      }
      drawTimelineProportionalGuide(
          p, dirtyRect, theme, impl_->trackHeights_, impl_->trackTops_,
          trackIndexList, ppf, xOffset, yOffset, impl_->dragMarkerOrigFrame_,
          impl_->proportionalEditRadius_);
    }
  }

  if (impl_->dragAreaIndex_ >= 0 &&
      impl_->dragAreaPart_ != KeyframeAreaHitPart::None &&
      impl_->proportionalEditingEnabled_ &&
      impl_->dragAreaSelectionIndices_.size() > 2) {
    QSet<int> trackIndices;
    trackIndices.reserve(impl_->dragAreaSelectionIndices_.size());
    for (const int selectedIndex : impl_->dragAreaSelectionIndices_) {
      if (selectedIndex < 0 ||
          selectedIndex >= impl_->keyframeMarkers_.size()) {
        continue;
      }
      trackIndices.insert(impl_->keyframeMarkers_[selectedIndex].trackIndex);
    }
    QVector<int> trackIndexList;
    trackIndexList.reserve(trackIndices.size());
    for (const int trackIndex : trackIndices) {
      trackIndexList.push_back(trackIndex);
    }
    drawTimelineProportionalGuide(
        p, dirtyRect, theme, impl_->trackHeights_, impl_->trackTops_,
        trackIndexList, ppf, xOffset, yOffset, impl_->dragAreaPivotFrame_,
        impl_->proportionalEditRadius_);
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
    QColor fill = keyframeAreaTintFor(area, theme);
    if (isSelected) {
      fill = theme.accent.lighter(145);
      fill.setAlpha(isHovered ? 122 : 78);
    } else if (isHovered) {
      fill.setAlpha(std::min(140, fill.alpha() + 24));
    }
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
      impl_->nearestMarkerIndexForFrame(impl_->currentFrame_);

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
    const bool isMiniEditorMarker =
        miniEditorEnabled && marker.layerId == miniEditorLayerId &&
        marker.propertyPath.trimmed() == miniEditorPropertyPath;
    if (isMiniEditorMarker && marker.bezier) {
      QColor bezierColor = marker.selectedLayer ? theme.accent.lighter(135)
                                                : marker.color.lighter(135);
      bezierColor.setAlpha(marker.selected ? 190 : 130);
      p.setPen(QPen(bezierColor, marker.selected ? 1.7 : 1.3));
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(diamondRect.adjusted(-2.0, -2.0, 2.0, 2.0), 2, 2);
      // タンジェントハンドル描画（選択時のみ表示）
      if (marker.selected) {
        const double valueToPixelScale = std::max(2.0, static_cast<double>(
            impl_->trackHeights_.isEmpty() ? kDefaultTrackHeight : impl_->trackHeights_[marker.trackIndex])) * 0.25;
        auto drawHandle = [&](double handleFrameOffset, double handleValueOffset, QColor handleColor) {
          if (std::abs(handleFrameOffset) < 0.001 && std::abs(handleValueOffset) < 0.0001)
            return;
          const QPointF handlePos(
              center.x() + handleFrameOffset * ppf,
              center.y() - handleValueOffset * valueToPixelScale);
          // マーカー中心からハンドルへの接続線
          handleColor.setAlpha(marker.selected ? 160 : 90);
          p.setPen(QPen(handleColor, 0.8, Qt::DotLine));
          p.drawLine(center, handlePos);
          // ハンドルドット
          handleColor.setAlpha(marker.selected ? 220 : 140);
          p.setPen(QPen(handleColor.darker(120), 1.0));
          p.setBrush(handleColor);
          p.drawEllipse(handlePos, 3.5, 3.5);
        };
        if (marker.incomingBezier) {
          drawHandle(marker.inHandleFrameOffset, marker.inHandleValueOffset,
                     marker.color.lighter(130));
        }
        if (marker.outgoingBezier) {
          drawHandle(marker.outHandleFrameOffset, marker.outHandleValueOffset,
                     marker.color.lighter(130));
        }
      }
    }
    if (marker.roving) {
      QColor rovingColor = marker.selectedLayer ? theme.accent.lighter(140)
                                                : theme.text.lighter(145);
      rovingColor.setAlpha(marker.selected ? 180 : 120);
      p.setPen(QPen(rovingColor, marker.selected ? 1.6 : 1.2, Qt::DashLine));
      p.setBrush(Qt::NoBrush);
      p.drawEllipse(center, size + 5.0, size + 5.0);
      p.setPen(rovingColor);
      QFont badgeFont = p.font();
      badgeFont.setBold(true);
      badgeFont.setPointSizeF(std::max<qreal>(6.0, badgeFont.pointSizeF() - 1.5));
      p.setFont(badgeFont);
      const QRectF badgeRect(center.x() - 5.0, center.y() - 5.0, 10.0, 10.0);
      p.drawText(badgeRect, Qt::AlignCenter, QStringLiteral("R"));
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
      if (auto* playback = ArtifactPlaybackService::instance()) {
        if (playback->isPlaying()) {
          playback->pause();
        }
      }
      event->accept();
      return;
    }

    const auto markerHit =
        hitTestMarkers(impl_->keyframeMarkers_, impl_->trackHeights_,
                       impl_->trackTops_, mouseX, mouseY,
                       impl_->pixelsPerFrame_, impl_->horizontalOffset_,
                       impl_->verticalOffset_);
    const auto keyframeAreas = collectKeyframeAreas(
        impl_->keyframeMarkers_, impl_->trackHeights_, impl_->trackTops_,
        impl_->pixelsPerFrame_, impl_->horizontalOffset_, impl_->verticalOffset_);
    const auto areaHit = hitTestKeyframeAreas(keyframeAreas, mouseX, mouseY);
    if (markerHit.markerIndex >= 0) {
      const auto &marker = impl_->keyframeMarkers_[markerHit.markerIndex];
      // ハンドルヒットテスト（選択済みのベジェマーカーに対して）
      const bool miniEditorEnabled =
          property("timelineMiniEditorEnabled").toBool();
      const bool isMiniEditorMarker =
          miniEditorEnabled && marker.layerId == impl_->contextLayerId_ &&
          marker.propertyPath.trimmed() == impl_->contextPropertyPath_.trimmed();
      if (isMiniEditorMarker && marker.selected &&
          (marker.incomingBezier || marker.outgoingBezier)) {
        const double ppf = impl_->pixelsPerFrame_;
        const QPointF center = markerCenterFor(
            marker, impl_->trackHeights_, impl_->trackTops_, ppf,
            impl_->horizontalOffset_, impl_->verticalOffset_);
        const double valueToPixelScale = std::max(2.0, static_cast<double>(
            impl_->trackHeights_.isEmpty() ? kDefaultTrackHeight
            : impl_->trackHeights_[marker.trackIndex])) * 0.25;
        const double handleHitRadius = 8.0;
        auto checkHandle = [&](double hFrameOffset, double hValueOffset, bool incoming) -> bool {
          if (std::abs(hFrameOffset) < 0.001 && std::abs(hValueOffset) < 0.0001)
            return false;
          const QPointF handlePos(
              center.x() + hFrameOffset * ppf,
              center.y() - hValueOffset * valueToPixelScale);
          const double dx = mouseX - handlePos.x();
          const double dy = mouseY - handlePos.y();
          if (dx * dx + dy * dy <= handleHitRadius * handleHitRadius) {
            impl_->dragHandleMarkerIndex_ = markerHit.markerIndex;
            impl_->dragHandleIncoming_ = incoming;
            impl_->dragHandleOrigFrameOffset_ = hFrameOffset;
            impl_->dragHandleOrigValueOffset_ = hValueOffset;
            impl_->draggingHandle_ = false;
            setCursor(Qt::CrossCursor);
            update();
            return true;
          }
          return false;
        };
        if (marker.incomingBezier &&
            checkHandle(marker.inHandleFrameOffset, marker.inHandleValueOffset, true)) {
          event->accept();
          return;
        }
        if (marker.outgoingBezier &&
            checkHandle(marker.outHandleFrameOffset, marker.outHandleValueOffset, false)) {
          event->accept();
          return;
        }
      }
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
        impl_->rebuildMarkerCaches();
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
        auto *svc = ArtifactProjectService::instance();
        const auto composition = svc ? svc->currentComposition().lock()
                                     : ArtifactCompositionPtr{};
        const bool keepGroupForPotentialDrag =
            clickedWasSelected && impl_->selectedMarkerKeys_.size() > 1;
        impl_->dragMarkerIndex_ = markerHit.markerIndex;
        impl_->dragMarkerStartPoint_ = event->position().toPoint();
        impl_->dragMarkerOrigFrame_ = marker.frame;
        impl_->dragMarkerLastPaintFrame_ = std::numeric_limits<qint64>::min();
        impl_->dragMarkerSelectionIndices_ =
            selectedMarkerIndices(impl_->keyframeMarkers_);
        impl_->dragMarkerSelectionOrigFrames_.clear();
        impl_->dragMarkerSelectionOrigFrames_.reserve(
            impl_->dragMarkerSelectionIndices_.size());
        impl_->dragMarkerSelectionOrigValues_.clear();
        impl_->dragMarkerSelectionOrigValues_.reserve(
            impl_->dragMarkerSelectionIndices_.size());
        impl_->dragMarkerSelectionOrigTypes_.clear();
        impl_->dragMarkerSelectionOrigTypes_.reserve(
            impl_->dragMarkerSelectionIndices_.size());
        impl_->dragMarkerBeforeSnapshots_.clear();
        for (const int selectedIndex : impl_->dragMarkerSelectionIndices_) {
          if (selectedIndex < 0 ||
              selectedIndex >= impl_->keyframeMarkers_.size()) {
            impl_->dragMarkerSelectionOrigFrames_.push_back(0.0);
            impl_->dragMarkerSelectionOrigValues_.push_back(QVariant());
            impl_->dragMarkerSelectionOrigTypes_.push_back(std::nullopt);
            continue;
          }
          impl_->dragMarkerSelectionOrigFrames_.push_back(
              impl_->keyframeMarkers_[selectedIndex].frame);
          impl_->dragMarkerSelectionOrigValues_.push_back(
              impl_->keyframeMarkers_[selectedIndex].value);
          std::optional<ArtifactCore::PropertyType> propertyType;
          if (composition) {
            const auto &selectedMarker = impl_->keyframeMarkers_[selectedIndex];
            const auto layer = composition->layerById(selectedMarker.layerId);
            if (layer) {
              const auto property = findLayerPropertyByPath(layer, selectedMarker.propertyPath);
              if (property) {
                propertyType = property->getType();
              }
            }
          }
          impl_->dragMarkerSelectionOrigTypes_.push_back(propertyType);
        }
        if (composition) {
          QVector<KeyframePropertyRef> dragRefs;
          dragRefs.reserve(impl_->dragMarkerSelectionIndices_.size());
          for (const int selectedIndex : impl_->dragMarkerSelectionIndices_) {
            if (selectedIndex < 0 ||
                selectedIndex >= impl_->keyframeMarkers_.size()) {
              continue;
            }
            const auto &selectedMarker = impl_->keyframeMarkers_[selectedIndex];
            dragRefs.push_back({selectedMarker.layerId, selectedMarker.propertyPath});
          }
          impl_->dragMarkerBeforeSnapshots_ =
              captureKeyframePropertySnapshots(composition, dragRefs);
        }
        impl_->dragMarkerValueChanged_ = false;
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
    if (areaHit.areaIndex >= 0 && areaHit.areaIndex < keyframeAreas.size()) {
      const auto &area = keyframeAreas[areaHit.areaIndex];
      QSet<QString> nextSelection = impl_->selectedMarkerKeys_;
      const qint64 startFrame = static_cast<qint64>(std::llround(area.startFrame));
      const qint64 endFrame = static_cast<qint64>(std::llround(area.endFrame));
      const QString startKey =
          keyframeSelectionKey(area.layerId, area.propertyPath, startFrame);
      const QString endKey =
          keyframeSelectionKey(area.layerId, area.propertyPath, endFrame);
      const bool areaAlreadySelected =
          impl_->selectedMarkerKeys_.contains(startKey) &&
          impl_->selectedMarkerKeys_.contains(endKey);
      if (event->modifiers() & Qt::ControlModifier) {
        auto toggleKey = [&nextSelection](const QString &key) {
          if (nextSelection.contains(key)) {
            nextSelection.remove(key);
          } else {
            nextSelection.insert(key);
          }
        };
        toggleKey(startKey);
        toggleKey(endKey);
      } else if (event->modifiers() & Qt::ShiftModifier) {
        nextSelection.insert(startKey);
        nextSelection.insert(endKey);
      } else {
        nextSelection.clear();
        nextSelection.insert(startKey);
        nextSelection.insert(endKey);
      }
      if (applyMarkerSelectionSet(impl_->keyframeMarkers_, impl_->selectedMarkerKeys_,
                                  nextSelection)) {
        impl_->rebuildMarkerCaches();
        Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
      }
      impl_->dragAreaIndex_ = areaHit.areaIndex;
      impl_->dragAreaPart_ = areaHit.part;
      impl_->dragAreaStartPoint_ = event->position().toPoint();
      impl_->dragAreaOrigStartFrame_ = area.startFrame;
      impl_->dragAreaOrigEndFrame_ = area.endFrame;
      impl_->dragAreaPivotFrame_ =
          (area.startFrame + area.endFrame) * 0.5;
      impl_->dragAreaSelectionIndices_.clear();
      impl_->dragAreaSelectionOrigFrames_.clear();
      if (impl_->proportionalEditingEnabled_) {
        const auto selectedIndices =
            selectedMarkerIndices(impl_->keyframeMarkers_);
        for (const int selectedIndex : selectedIndices) {
          if (selectedIndex < 0 ||
              selectedIndex >= impl_->keyframeMarkers_.size()) {
            continue;
          }
          const auto &selectedMarker =
              impl_->keyframeMarkers_[selectedIndex];
          if (selectedMarker.layerId != area.layerId ||
              selectedMarker.propertyPath != area.propertyPath) {
            continue;
          }
          impl_->dragAreaSelectionIndices_.push_back(selectedIndex);
          impl_->dragAreaSelectionOrigFrames_.push_back(
              selectedMarker.frame);
        }
      }
      if (impl_->dragAreaSelectionIndices_.isEmpty()) {
        impl_->dragAreaSelectionIndices_.push_back(area.startMarkerIndex);
        impl_->dragAreaSelectionIndices_.push_back(area.endMarkerIndex);
        impl_->dragAreaSelectionOrigFrames_.push_back(area.startFrame);
        impl_->dragAreaSelectionOrigFrames_.push_back(area.endFrame);
      }
      impl_->dragAreaSnapLabel_.clear();
      impl_->dragAreaValue_ = area.value;
      setKeyframeContext(area.layerId, area.propertyPath);
      clipSelected(QString(), area.layerId);
      updateHoverToolTip(
          this, event->globalPosition().toPoint(),
          formatKeyframeAreaTooltip(
              area, areaAlreadySelected || impl_->selectedMarkerKeys_.contains(startKey),
              areaHit.part == KeyframeAreaHitPart::Body
                  ? tt("timeline.area_mode_move", "Mode: Move Area")
                  : tt("timeline.area_mode_resize", "Mode: Resize Area")),
          impl_->hoverToolTipText_);
      setCursor(areaHit.part == KeyframeAreaHitPart::Body ? Qt::ClosedHandCursor
                                                          : Qt::SizeHorCursor);
      update();
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
    const DragMode bodyMode = (impl_->activeTool_ == ToolType::Slide)
                                  ? DragMode::SlideBody
                                  : DragMode::MoveBody;
    const auto hit =
        hitTestClips(impl_->clips_, impl_->trackHeights_, impl_->trackTops_,
                     mouseX, mouseY, impl_->pixelsPerFrame_,
                     impl_->horizontalOffset_, impl_->verticalOffset_, bodyMode);
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
      if (hit.mode == DragMode::MoveBody || hit.mode == DragMode::SlideBody)
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
        impl_->rebuildMarkerCaches();
        Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
      }
      update();
      event->accept();
      return;
    }
  }

  if ((event->buttons() & Qt::LeftButton) && impl_->dragHandleMarkerIndex_ >= 0) {
    const QPoint currentPos = event->position().toPoint();
    const int dragDistance =
        (currentPos - impl_->dragMarkerStartPoint_).manhattanLength();
    if (!impl_->draggingHandle_ &&
        dragDistance >= QApplication::startDragDistance()) {
      impl_->draggingHandle_ = true;
      setCursor(Qt::CrossCursor);
    }
    if (impl_->draggingHandle_ &&
        impl_->dragHandleMarkerIndex_ >= 0 &&
        impl_->dragHandleMarkerIndex_ < impl_->keyframeMarkers_.size()) {
      auto &marker = impl_->keyframeMarkers_[impl_->dragHandleMarkerIndex_];
      const double ppf = impl_->pixelsPerFrame_;
      const double deltaFramePx = mouseX - impl_->dragMarkerStartPoint_.x();
      const double deltaValuePx = mouseY - impl_->dragMarkerStartPoint_.y();
      const double newFrameOffset = impl_->dragHandleOrigFrameOffset_ + deltaFramePx / ppf;
      const double valueToPixelScale = std::max(2.0, static_cast<double>(
          impl_->trackHeights_.isEmpty() ? kDefaultTrackHeight
          : impl_->trackHeights_[marker.trackIndex])) * 0.25;
      const double newValueOffset = impl_->dragHandleOrigValueOffset_ - deltaValuePx / valueToPixelScale;
      if (impl_->dragHandleIncoming_) {
        marker.inHandleFrameOffset = newFrameOffset;
        marker.inHandleValueOffset = newValueOffset;
      } else {
        marker.outHandleFrameOffset = newFrameOffset;
        marker.outHandleValueOffset = newValueOffset;
      }
      update();
    }
    event->accept();
    return;
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
      // 水平ドラッグ成分 = 時間移動
      const double rawDeltaFrames =
          (event->position().x() - impl_->dragMarkerStartPoint_.x()) / 
          std::max<double>(0.001, static_cast<double>(impl_->pixelsPerFrame_));
      // 垂直ドラッグ成分 = 値変更（ピクセル → 値変換係数: トラック高さ = 概算値幅）
      const double rawDeltaYPixels =
          event->position().y() - impl_->dragMarkerStartPoint_.y();
      // 修飾キーによる軸ロック: Shift=時間のみ（値固定）, Ctrl=値のみ（時間固定）
      const bool timeOnlyDrag = (event->modifiers() & Qt::ShiftModifier) != 0;
      const bool valueOnlyDrag = (event->modifiers() & Qt::ControlModifier) != 0;
      const bool freeMoveDrag = !timeOnlyDrag && !valueOnlyDrag;
      const bool applyTimeChange = !valueOnlyDrag;
      const bool applyValueChange = !timeOnlyDrag && (freeMoveDrag || valueOnlyDrag);
      const auto &dragMarker = impl_->keyframeMarkers_[impl_->dragMarkerIndex_];
      const int markerTrackHeight =
          (dragMarker.trackIndex >= 0 &&
           dragMarker.trackIndex < impl_->trackHeights_.size())
              ? impl_->trackHeights_[dragMarker.trackIndex]
              : kDefaultTrackHeight;
      const double valueScalePerPixel = (freeMoveDrag || valueOnlyDrag)
          ? std::max(0.0001, 1.0 / std::max(4.0, static_cast<double>(markerTrackHeight)))
          : 0.0;
      const double rawDeltaValue = rawDeltaYPixels * valueScalePerPixel;
      QString snapLabel;
      double targetFrame = impl_->dragMarkerOrigFrame_;
      if (applyTimeChange) {
        targetFrame = std::clamp(
            snappedKeyframeDragTargetFrame(
                impl_->dragMarkerOrigFrame_, rawDeltaFrames, impl_->currentFrame_,
                event->modifiers(), &snapLabel),
            0.0, std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)));
      } else {
        targetFrame = impl_->dragMarkerOrigFrame_;
      }
      const double deltaFrames = targetFrame - impl_->dragMarkerOrigFrame_;
      const double deltaValue = applyValueChange ? rawDeltaValue : 0.0;
      if (applyValueChange && std::abs(deltaValue) > 0.0001) {
        if (snapLabel.isEmpty()) {
          snapLabel = QStringLiteral("Val %1").arg(
              QString::number(deltaValue, 'f', 3));
        } else {
          snapLabel += QStringLiteral(", Val %1").arg(
              QString::number(deltaValue, 'f', 3));
        }
      }
      if (!impl_->proportionalEditingEnabled_ ||
          impl_->dragMarkerSelectionIndices_.size() <= 1) {
        const int dragCollisionCount = keyframeDragCollisionCount(
            impl_->keyframeMarkers_, impl_->dragMarkerSelectionIndices_,
            impl_->dragMarkerSelectionOrigFrames_, deltaFrames,
            std::max<double>(0.0,
                             static_cast<double>(impl_->durationFrames_ - 1.0)));
        if (dragCollisionCount > 0) {
          if (snapLabel.isEmpty()) {
            snapLabel = formatKeyframeCollisionLabel(dragCollisionCount);
          } else {
            snapLabel += QStringLiteral(", %1")
                             .arg(formatKeyframeCollisionLabel(dragCollisionCount));
          }
        }
      }
      const qint64 roundedTargetFrame =
          static_cast<qint64>(std::llround(targetFrame));
      if (roundedTargetFrame == impl_->dragMarkerLastPaintFrame_ &&
          snapLabel == impl_->dragMarkerSnapLabel_) {
        event->accept();
        return;
      }
      impl_->dragMarkerTargetFrame_ = targetFrame;
      impl_->dragMarkerLastPaintFrame_ = roundedTargetFrame;
      impl_->dragMarkerSnapLabel_ = snapLabel;
      updateHoverToolTip(
          this, event->globalPosition().toPoint(),
          formatKeyframeDragTooltip(static_cast<int>(impl_->dragMarkerSelectionIndices_.size()),
                                    deltaFrames, targetFrame, snapLabel,
                                    impl_->proportionalEditingEnabled_,
                                    impl_->proportionalEditRadius_),
          impl_->hoverToolTipText_);
      QRectF dirtyRect;
      bool hasDirty = false;
      const int selectionCount =
          static_cast<int>(impl_->dragMarkerSelectionIndices_.size());
      for (int i = 0; i < impl_->dragMarkerSelectionIndices_.size(); ++i) {
        const int selectedIndex = impl_->dragMarkerSelectionIndices_[i];
        if (selectedIndex < 0 || selectedIndex >= impl_->keyframeMarkers_.size() ||
            i >= impl_->dragMarkerSelectionOrigFrames_.size()) {
          continue;
        }
        const auto originalMarker = impl_->keyframeMarkers_[selectedIndex];
        const double originalFrame = impl_->dragMarkerSelectionOrigFrames_[i];
        const double markerDeltaFrames = proportionalTimelineDelta(
            originalFrame, impl_->dragMarkerOrigFrame_, deltaFrames,
            impl_->proportionalEditingEnabled_, impl_->proportionalEditRadius_,
            selectionCount);
        const double newFrame = std::clamp(
            originalFrame + markerDeltaFrames, 0.0,
            std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)));
        auto &mutableMarker = impl_->keyframeMarkers_[selectedIndex];
        mutableMarker.frame = newFrame;
        if (applyValueChange && i < impl_->dragMarkerSelectionOrigValues_.size()) {
          const QVariant &originalValue = impl_->dragMarkerSelectionOrigValues_[i];
          QVariant nextValue;
          bool valueChanged = false;
          const std::optional<ArtifactCore::PropertyType> originalType =
              i < impl_->dragMarkerSelectionOrigTypes_.size()
                  ? impl_->dragMarkerSelectionOrigTypes_[i]
                  : std::nullopt;
          if (originalType && *originalType == ArtifactCore::PropertyType::Integer) {
            bool doubleOk = false;
            const double numericValue = originalValue.toDouble(&doubleOk);
            if (doubleOk) {
              nextValue = QVariant(static_cast<qint64>(
                  std::llround(numericValue + deltaValue)));
              valueChanged = (nextValue != originalValue);
            }
          } else if (originalType && *originalType == ArtifactCore::PropertyType::Float) {
            bool doubleOk = false;
            const double numericValue = originalValue.toDouble(&doubleOk);
            if (doubleOk) {
              nextValue = QVariant(numericValue + deltaValue);
              valueChanged = (nextValue != originalValue);
            }
          } else if (originalValue.canConvert<double>()) {
            nextValue = QVariant(originalValue.toDouble() + deltaValue);
            valueChanged = (nextValue != originalValue);
          }
          if (valueChanged) {
            mutableMarker.value = nextValue;
            impl_->dragMarkerValueChanged_ = true;
          }
        }
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

  if ((event->buttons() & Qt::LeftButton) && impl_->dragAreaIndex_ >= 0 &&
      impl_->dragAreaIndex_ < keyframeAreas.size() &&
      impl_->dragAreaSelectionIndices_.size() >= 2) {
    const auto &area = keyframeAreas[impl_->dragAreaIndex_];
    const double maxFrame =
        std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0));
    const double rawDeltaFrames =
        (event->position().x() - impl_->dragAreaStartPoint_.x()) /
        std::max<double>(0.001, static_cast<double>(impl_->pixelsPerFrame_));
    double targetStartFrame = impl_->dragAreaOrigStartFrame_;
    double targetEndFrame = impl_->dragAreaOrigEndFrame_;
    if (impl_->dragAreaPart_ == KeyframeAreaHitPart::Body) {
      const double deltaFrames = keyframeAreaBodyTargetDelta(
          impl_->keyframeMarkers_, area, rawDeltaFrames, maxFrame);
      targetStartFrame = impl_->dragAreaOrigStartFrame_ + deltaFrames;
      targetEndFrame = impl_->dragAreaOrigEndFrame_ + deltaFrames;
    } else if (impl_->dragAreaPart_ == KeyframeAreaHitPart::LeftEdge) {
      targetStartFrame = keyframeAreaEdgeTargetFrame(
          impl_->keyframeMarkers_, area, impl_->dragAreaPart_,
          impl_->dragAreaOrigStartFrame_ + rawDeltaFrames, maxFrame);
    } else if (impl_->dragAreaPart_ == KeyframeAreaHitPart::RightEdge) {
      targetEndFrame = keyframeAreaEdgeTargetFrame(
          impl_->keyframeMarkers_, area, impl_->dragAreaPart_,
          impl_->dragAreaOrigEndFrame_ + rawDeltaFrames, maxFrame);
    }

    const qint64 roundedStartFrame =
        static_cast<qint64>(std::llround(targetStartFrame));
    const qint64 roundedEndFrame =
        static_cast<qint64>(std::llround(targetEndFrame));
    const QString snapLabel =
        QStringLiteral("F%1 - F%2").arg(roundedStartFrame).arg(roundedEndFrame);
    if (snapLabel == impl_->dragAreaSnapLabel_) {
      event->accept();
      return;
    }

    impl_->dragAreaSnapLabel_ = snapLabel;
    const bool proportionalAreaEdit =
        impl_->proportionalEditingEnabled_ &&
        impl_->dragAreaSelectionIndices_.size() > 2;
    const QString areaModeText =
        proportionalAreaEdit
            ? tt("timeline.area_drag_preview_proportional",
                 "Preview: F%1 -> F%2 / Proportional %3f")
                  .arg(roundedStartFrame)
                  .arg(roundedEndFrame)
                  .arg(QString::number(impl_->proportionalEditRadius_, 'f', 1))
            : tt("timeline.area_drag_preview", "Preview: F%1 -> F%2")
                  .arg(roundedStartFrame)
                  .arg(roundedEndFrame);
    updateHoverToolTip(
        this, event->globalPosition().toPoint(),
        formatKeyframeAreaTooltip(
            area, true, areaModeText),
        impl_->hoverToolTipText_);

    QRectF dirtyRect;
    bool hasDirty = false;
    const int selectionCount =
        static_cast<int>(impl_->dragAreaSelectionIndices_.size());
    for (int i = 0; i < impl_->dragAreaSelectionIndices_.size(); ++i) {
      const int selectedIndex = impl_->dragAreaSelectionIndices_[i];
      if (selectedIndex < 0 || selectedIndex >= impl_->keyframeMarkers_.size() ||
          i >= impl_->dragAreaSelectionOrigFrames_.size()) {
        continue;
      }
      const auto originalMarker = impl_->keyframeMarkers_[selectedIndex];
      auto &mutableMarker = impl_->keyframeMarkers_[selectedIndex];
      if (impl_->dragAreaPart_ == KeyframeAreaHitPart::Body &&
          proportionalAreaEdit) {
        const double originalFrame = impl_->dragAreaSelectionOrigFrames_[i];
        const double markerDeltaFrames = proportionalTimelineDelta(
            originalFrame, impl_->dragAreaPivotFrame_,
            targetStartFrame - impl_->dragAreaOrigStartFrame_,
            true, impl_->proportionalEditRadius_, selectionCount);
        mutableMarker.frame = std::clamp(
            originalFrame + markerDeltaFrames, 0.0, maxFrame);
      } else if (impl_->dragAreaPart_ == KeyframeAreaHitPart::LeftEdge &&
                 proportionalAreaEdit) {
        const double originalFrame = impl_->dragAreaSelectionOrigFrames_[i];
        mutableMarker.frame = std::clamp(
            proportionalTimelineScaledFrame(
                originalFrame,
                impl_->dragAreaOrigStartFrame_,
                impl_->dragAreaOrigEndFrame_,
                targetStartFrame,
                true,
                impl_->proportionalEditRadius_,
                selectionCount),
            0.0, maxFrame);
      } else if (impl_->dragAreaPart_ == KeyframeAreaHitPart::RightEdge &&
                 proportionalAreaEdit) {
        const double originalFrame = impl_->dragAreaSelectionOrigFrames_[i];
        mutableMarker.frame = std::clamp(
            proportionalTimelineScaledFrame(
                originalFrame,
                impl_->dragAreaOrigEndFrame_,
                impl_->dragAreaOrigStartFrame_,
                targetEndFrame,
                true,
                impl_->proportionalEditRadius_,
                selectionCount),
            0.0, maxFrame);
      } else {
        mutableMarker.frame = (i == 0) ? targetStartFrame : targetEndFrame;
      }
      const QRectF originalRect = markerHitRectFor(
          originalMarker, impl_->trackHeights_, impl_->trackTops_,
          impl_->pixelsPerFrame_, impl_->horizontalOffset_, impl_->verticalOffset_);
      const QRectF updatedRect = markerHitRectFor(
          mutableMarker, impl_->trackHeights_, impl_->trackTops_,
          impl_->pixelsPerFrame_, impl_->horizontalOffset_, impl_->verticalOffset_);
      if (originalRect.isValid()) {
        dirtyRect = hasDirty ? dirtyRect.united(originalRect) : originalRect;
        hasDirty = true;
      }
      if (updatedRect.isValid()) {
        dirtyRect = hasDirty ? dirtyRect.united(updatedRect) : updatedRect;
        hasDirty = true;
      }
    }
    if (area.bodyRect.isValid()) {
      dirtyRect = hasDirty ? dirtyRect.united(area.bodyRect) : area.bodyRect;
      hasDirty = true;
    }
    if (hasDirty) {
      update(dirtyRect.adjusted(-8.0, -8.0, 8.0, 8.0).toAlignedRect());
    } else {
      update();
    }
    event->accept();
    return;
  }

  if (impl_->dragMode_ != DragMode::None && impl_->dragClipIndex_ >= 0) {
    const auto oldClip = impl_->clips_[impl_->dragClipIndex_];
    const double deltaFrames =
        (mouseX - impl_->dragStartX_) / std::max(0.001, ppf);
    auto &clip = impl_->clips_[impl_->dragClipIndex_];
    QString clipSnapLabel;
    switch (impl_->dragMode_) {
    case DragMode::MoveBody: {
      clip.startFrame = snapTimelineFrameToEditTargets(
          impl_->dragOrigStartFrame_ + deltaFrames, impl_->clips_,
          impl_->keyframeMarkers_, impl_->currentFrame_, event->modifiers(),
          &clipSnapLabel, impl_->dragClipIndex_);
      if (clip.hasTrimSourceRange) {
        const double rangeDelta = clip.startFrame - impl_->dragOrigStartFrame_;
        clip.trimMinStartFrame = impl_->dragOrigTrimMinStartFrame_ + rangeDelta;
        clip.trimMaxEndFrame = impl_->dragOrigTrimMaxEndFrame_ + rangeDelta;
      }
      break;
    }
    case DragMode::ResizeLeft: {
      const double end = impl_->dragOrigStartFrame_ + impl_->dragOrigDuration_;
      const double minStart = clip.hasTrimSourceRange
                                  ? impl_->dragOrigTrimMinStartFrame_
                                  : 0.0;
      const double snappedStart = snapTimelineFrameToEditTargets(
          impl_->dragOrigStartFrame_ + deltaFrames, impl_->clips_,
          impl_->keyframeMarkers_, impl_->currentFrame_, event->modifiers(),
          &clipSnapLabel, impl_->dragClipIndex_);
      clip.startFrame = std::clamp(snappedStart, minStart, end - 1.0);
      clip.durationFrame = end - clip.startFrame;
      break;
    }
    case DragMode::ResizeRight: {
      const double maxEnd = clip.hasTrimSourceRange
                                ? impl_->dragOrigTrimMaxEndFrame_
                                : std::numeric_limits<double>::max();
      const double snappedEnd = snapTimelineFrameToEditTargets(
          impl_->dragOrigStartFrame_ + impl_->dragOrigDuration_ + deltaFrames,
          impl_->clips_, impl_->keyframeMarkers_, impl_->currentFrame_,
          event->modifiers(), &clipSnapLabel, impl_->dragClipIndex_);
      const double newEnd = std::clamp(snappedEnd, clip.startFrame + 1.0, maxEnd);
      clip.durationFrame = std::max(1.0, newEnd - clip.startFrame);
      break;
    }
    case DragMode::SlideBody: {
      const double snappedStart = snapTimelineFrameToEditTargets(
          impl_->dragOrigStartFrame_ + deltaFrames, impl_->clips_,
          impl_->keyframeMarkers_, impl_->currentFrame_, event->modifiers(),
          &clipSnapLabel, impl_->dragClipIndex_);
      double minBound = 0.0;
      double maxBound = std::numeric_limits<double>::max();
      if (clip.hasTrimSourceRange) {
        minBound = impl_->dragOrigTrimMinStartFrame_;
        maxBound = impl_->dragOrigTrimMaxEndFrame_ - impl_->dragOrigDuration_;
      }
      clip.startFrame = std::clamp(snappedStart, minBound, maxBound);
      clip.durationFrame = impl_->dragOrigDuration_;
      break;
    }
    default:
      break;
    }

    // Debug message emission
    const QString status =
        QStringLiteral("Layer: %1 | Start: %2 | Dur: %3%4")
            .arg(clip.title.isEmpty() ? clip.clipId : clip.title)
            .arg(QString::number(clip.startFrame, 'f', 1))
            .arg(QString::number(clip.durationFrame, 'f', 1))
            .arg(clipSnapLabel.isEmpty()
                     ? QString()
                     : QStringLiteral(" | Snap: %1").arg(clipSnapLabel));
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

  const DragMode hoverBodyMode = (impl_->activeTool_ == ToolType::Slide)
                                     ? DragMode::SlideBody
                                     : DragMode::MoveBody;
  const auto hit =
      hitTestClips(impl_->clips_, impl_->trackHeights_, impl_->trackTops_,
                   mouseX, mouseY, ppf, impl_->horizontalOffset_,
                   impl_->verticalOffset_, hoverBodyMode);
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
    case DragMode::SlideBody:
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
        impl_->nearestMarkerIndexForFrame(impl_->currentFrame_);
    if (impl_->hoverMarkerIndex_ >= 0 &&
        impl_->hoverMarkerIndex_ < impl_->keyframeMarkers_.size()) {
      tooltipText = formatMarkerTooltip(
          impl_->keyframeMarkers_[impl_->hoverMarkerIndex_], impl_->currentFrame_,
          true, impl_->hoverMarkerIndex_ == nearestMarkerIndex);
    } else if (impl_->hoverAreaIndex_ >= 0 &&
               impl_->hoverAreaIndex_ < keyframeAreas.size()) {
      const auto &hoveredArea = keyframeAreas[impl_->hoverAreaIndex_];
      tooltipText = formatKeyframeAreaTooltip(
          hoveredArea,
          hoveredArea.startMarkerIndex >= 0 &&
              hoveredArea.startMarkerIndex < impl_->keyframeMarkers_.size() &&
              impl_->keyframeMarkers_[hoveredArea.startMarkerIndex].selected,
          areaHit.part == KeyframeAreaHitPart::Body
              ? tt("timeline.area_mode_move", "Mode: Move Area")
              : tt("timeline.area_mode_resize", "Mode: Resize Area"));
    } else if (impl_->hoverClipIndex_ >= 0 &&
               impl_->hoverClipIndex_ < impl_->clips_.size()) {
      tooltipText = formatClipTooltip(impl_->clips_[impl_->hoverClipIndex_]);
    }
    tooltipText = appendRamPreviewPriorityHint(tooltipText);
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

  if (event->button() == Qt::LeftButton && impl_->dragHandleMarkerIndex_ >= 0) {
    impl_->draggingHandle_ = false;
    impl_->dragHandleMarkerIndex_ = -1;
    impl_->dragHandleIncoming_ = false;
    impl_->dragMarkerStartPoint_ = QPoint();
    setCursor(Qt::PointingHandCursor);
    Q_EMIT timelineDebugMessage(QStringLiteral("Adjusted bezier handle"));
    update();
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
        impl_->rebuildMarkerCaches();
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

  if (event->button() == Qt::LeftButton && impl_->dragAreaIndex_ >= 0 &&
      impl_->dragAreaSelectionIndices_.size() >= 2) {
    ArtifactCompositionPtr composition;
    if (auto *svc = ArtifactProjectService::instance()) {
      composition = svc->currentComposition().lock();
    }
    QVector<KeyframeFrameMoveRequest> requests;
    QSet<QString> nextSelectionKeys;
    const int selectionCount =
        static_cast<int>(impl_->dragAreaSelectionIndices_.size());
    for (int i = 0; i < impl_->dragAreaSelectionIndices_.size(); ++i) {
      const int selectedIndex = impl_->dragAreaSelectionIndices_[i];
      if (selectedIndex < 0 || selectedIndex >= impl_->keyframeMarkers_.size() ||
          i >= impl_->dragAreaSelectionOrigFrames_.size()) {
        continue;
      }
      const auto marker = impl_->keyframeMarkers_[selectedIndex];
      const qint64 fromFrame =
          static_cast<qint64>(std::llround(impl_->dragAreaSelectionOrigFrames_[i]));
      const qint64 toFrame =
          static_cast<qint64>(std::llround(marker.frame));
      nextSelectionKeys.insert(
          keyframeSelectionKey(marker.layerId, marker.propertyPath, toFrame));
      if (fromFrame == toFrame) {
        continue;
      }
      requests.push_back(
          {marker.layerId, marker.propertyPath, fromFrame, toFrame});
    }

    const auto resetAreaDragState = [this]() {
      impl_->dragAreaIndex_ = -1;
      impl_->dragAreaPart_ = KeyframeAreaHitPart::None;
      impl_->dragAreaSelectionIndices_.clear();
      impl_->dragAreaSelectionOrigFrames_.clear();
      impl_->dragAreaOrigStartFrame_ = 0.0;
      impl_->dragAreaOrigEndFrame_ = 0.0;
      impl_->dragAreaPivotFrame_ = 0.0;
      impl_->dragAreaSnapLabel_.clear();
      impl_->dragAreaValue_.clear();
    };

    if (composition && !requests.isEmpty()) {
      QVector<KeyframePropertyRef> refs;
      refs.reserve(requests.size());
      for (const auto &request : requests) {
        refs.push_back({request.layerId, request.propertyPath});
      }
      const auto beforeSnapshots = captureKeyframePropertySnapshots(composition, refs);
      auto afterSnapshots = beforeSnapshots;
      const double fps =
          std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
      const int64_t scale = static_cast<int64_t>(std::llround(fps));
      for (auto &snapshot : afterSnapshots) {
        for (const auto &request : requests) {
          if (snapshot.layerId != request.layerId ||
              snapshot.propertyPath != request.propertyPath) {
            continue;
          }
          const RationalTime fromTime(request.fromFrame, scale);
          const RationalTime toTime(request.toFrame, scale);
          auto it = std::find_if(snapshot.keyframes.begin(), snapshot.keyframes.end(),
                                 [&fromTime](const ArtifactCore::KeyFrame &keyframe) {
                                   return keyframe.time == fromTime;
                                 });
          if (it != snapshot.keyframes.end()) {
            it->time = toTime;
          }
        }
        std::sort(snapshot.keyframes.begin(), snapshot.keyframes.end(),
                  [](const ArtifactCore::KeyFrame &lhs,
                     const ArtifactCore::KeyFrame &rhs) {
                    return lhs.time < rhs.time;
                  });
      }

      if (auto *mgr = UndoManager::instance()) {
        QPointer<ArtifactTimelineTrackPainterView> self(this);
        const auto trackRows = impl_->trackRows_;
        const QSet<QString> beforeSelectionKeys = impl_->selectedMarkerKeys_;
        const QSet<QString> afterSelectionKeys = nextSelectionKeys;
        mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
            impl_->dragAreaPart_ == KeyframeAreaHitPart::Body
                ? QStringLiteral("Move Keyframe Area")
                : QStringLiteral("Resize Keyframe Area"),
            [self, composition, afterSnapshots, afterSelectionKeys, trackRows]() {
              applyKeyframePropertySnapshots(composition, afterSnapshots);
              if (!self) {
                return;
              }
              ArtifactLayerSelectionManager *selectionManager = nullptr;
              if (auto *app = ArtifactApplicationManager::instance()) {
                selectionManager = app->layerSelectionManager();
              }
              self->syncSelectionState(composition, selectionManager, trackRows, true);
              self->setSelectedKeyframeKeys(afterSelectionKeys);
            },
            [self, composition, beforeSnapshots, beforeSelectionKeys, trackRows]() {
              applyKeyframePropertySnapshots(composition, beforeSnapshots);
              if (!self) {
                return;
              }
              ArtifactLayerSelectionManager *selectionManager = nullptr;
              if (auto *app = ArtifactApplicationManager::instance()) {
                selectionManager = app->layerSelectionManager();
              }
              self->syncSelectionState(composition, selectionManager, trackRows, true);
              self->setSelectedKeyframeKeys(beforeSelectionKeys);
            }));
      }

      applyKeyframePropertySnapshots(composition, afterSnapshots);
      ArtifactLayerSelectionManager *selectionManager = nullptr;
      if (auto *app = ArtifactApplicationManager::instance()) {
        selectionManager = app->layerSelectionManager();
      }
      syncSelectionState(composition, selectionManager, impl_->trackRows_, true);
      setSelectedKeyframeKeys(nextSelectionKeys);
      if (impl_->proportionalEditingEnabled_ && selectionCount > 2) {
        Q_EMIT timelineDebugMessage(
            QStringLiteral("%1 keyframe area with proportional falloff (radius %2f)")
                .arg(impl_->dragAreaPart_ == KeyframeAreaHitPart::Body
                         ? QStringLiteral("Moved")
                         : QStringLiteral("Resized"))
                .arg(QString::number(impl_->proportionalEditRadius_, 'f', 1)));
      } else {
        Q_EMIT timelineDebugMessage(
            impl_->dragAreaPart_ == KeyframeAreaHitPart::Body
                ? QStringLiteral("Moved keyframe area")
                : QStringLiteral("Resized keyframe area"));
      }
    } else if (!nextSelectionKeys.isEmpty()) {
      applyMarkerSelectionSet(impl_->keyframeMarkers_, impl_->selectedMarkerKeys_,
                              nextSelectionKeys);
      impl_->rebuildMarkerCaches();
      Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
    }

    resetAreaDragState();
    updateHoverToolTip(this, event->globalPosition().toPoint(), QString(),
                       impl_->hoverToolTipText_);
    if (impl_->hoverAreaIndex_ >= 0) {
      setCursor(Qt::OpenHandCursor);
    } else {
      setCursor(Qt::ArrowCursor);
    }
    update();
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton && impl_->draggingMarker_ &&
      impl_->dragMarkerIndex_ >= 0 &&
      impl_->dragMarkerIndex_ < impl_->keyframeMarkers_.size()) {
    ArtifactCompositionPtr composition;
    if (auto *svc = ArtifactProjectService::instance()) {
      composition = svc->currentComposition().lock();
    }
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
    const int selectionCount =
        static_cast<int>(impl_->dragMarkerSelectionIndices_.size());
    const bool valueChanged = impl_->dragMarkerValueChanged_;
    for (int i = 0; i < impl_->dragMarkerSelectionIndices_.size(); ++i) {
      const int selectedIndex = impl_->dragMarkerSelectionIndices_[i];
      if (selectedIndex < 0 || selectedIndex >= impl_->keyframeMarkers_.size() ||
          i >= impl_->dragMarkerSelectionOrigFrames_.size()) {
        continue;
      }
      const auto marker = impl_->keyframeMarkers_[selectedIndex];
      const double originalFrame = impl_->dragMarkerSelectionOrigFrames_[i];
      const double markerDeltaFrames = proportionalTimelineDelta(
          originalFrame, impl_->dragMarkerOrigFrame_, deltaFrames,
          impl_->proportionalEditingEnabled_, impl_->proportionalEditRadius_,
          selectionCount);
      const double newFrame = std::clamp(
          originalFrame + markerDeltaFrames, 0.0,
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
    const bool hasFrameChanges = !requests.isEmpty();
    const bool hasValueChanges = valueChanged;
    if (!composition) {
      for (int i = 0; i < impl_->dragMarkerSelectionIndices_.size(); ++i) {
        const int selectedIndex = impl_->dragMarkerSelectionIndices_[i];
        if (selectedIndex < 0 || selectedIndex >= impl_->keyframeMarkers_.size()) {
          continue;
        }
        if (i < impl_->dragMarkerSelectionOrigFrames_.size()) {
          impl_->keyframeMarkers_[selectedIndex].frame =
              impl_->dragMarkerSelectionOrigFrames_[i];
        }
        if (i < impl_->dragMarkerSelectionOrigValues_.size()) {
          impl_->keyframeMarkers_[selectedIndex].value =
              impl_->dragMarkerSelectionOrigValues_[i];
        }
      }
      impl_->draggingMarker_ = false;
      impl_->dragMarkerIndex_ = -1;
      impl_->dragMarkerSelectionIndices_.clear();
      impl_->dragMarkerSelectionOrigFrames_.clear();
      impl_->dragMarkerSelectionOrigValues_.clear();
      impl_->dragMarkerSelectionOrigTypes_.clear();
      impl_->dragMarkerBeforeSnapshots_.clear();
      impl_->dragMarkerValueChanged_ = false;
      impl_->dragMarkerTargetFrame_ = 0.0;
      impl_->dragMarkerLastPaintFrame_ = std::numeric_limits<qint64>::min();
      impl_->dragMarkerSnapLabel_.clear();
      impl_->pendingMarkerSingleClick_ = false;
      impl_->pendingMarkerSingleClickKey_.clear();
      impl_->pendingMarkerSingleClickLabel_.clear();
      impl_->pendingMarkerSingleClickFrame_ = 0.0;
      update();
      event->accept();
      return;
    }
    if (hasFrameChanges || hasValueChanges) {
      if (composition && !impl_->dragMarkerBeforeSnapshots_.isEmpty()) {
        const auto beforeSnapshots = impl_->dragMarkerBeforeSnapshots_;
        auto afterSnapshots = beforeSnapshots;
        const double fps =
            std::max(1.0, static_cast<double>(composition->frameRate().framerate()));
        const int64_t scale = static_cast<int64_t>(std::llround(fps));
        auto snapshotFor = [&afterSnapshots](const LayerID &layerId,
                                             const QString &propertyPath)
            -> KeyframePropertySnapshot * {
          auto it = std::find_if(afterSnapshots.begin(), afterSnapshots.end(),
                                 [&layerId, &propertyPath](const KeyframePropertySnapshot &snapshot) {
                                   return snapshot.layerId == layerId &&
                                          snapshot.propertyPath == propertyPath;
                                 });
          return it != afterSnapshots.end() ? &(*it) : nullptr;
        };
        for (int i = 0; i < impl_->dragMarkerSelectionIndices_.size(); ++i) {
          const int selectedIndex = impl_->dragMarkerSelectionIndices_[i];
          if (selectedIndex < 0 || selectedIndex >= impl_->keyframeMarkers_.size() ||
              i >= impl_->dragMarkerSelectionOrigFrames_.size()) {
            continue;
          }
          const auto &marker = impl_->keyframeMarkers_[selectedIndex];
          auto *snapshot = snapshotFor(marker.layerId, marker.propertyPath);
          if (!snapshot) {
            continue;
          }
          const qint64 fromFrame =
              static_cast<qint64>(std::llround(impl_->dragMarkerSelectionOrigFrames_[i]));
          const qint64 toFrame =
              static_cast<qint64>(std::llround(marker.frame));
          const RationalTime fromTime(fromFrame, scale);
          const RationalTime toTime(toFrame, scale);
          auto keyframeIt = std::find_if(
              snapshot->keyframes.begin(), snapshot->keyframes.end(),
              [&fromTime](const ArtifactCore::KeyFrame &keyframe) {
                return keyframe.time == fromTime;
              });
          if (keyframeIt == snapshot->keyframes.end()) {
            continue;
          }
          keyframeIt->time = toTime;
          if (!hasValueChanges ||
              i >= impl_->dragMarkerSelectionOrigValues_.size()) {
            continue;
          }
          const QVariant &previewValue = marker.value;
          const QVariant &originalValue = impl_->dragMarkerSelectionOrigValues_[i];
          if (previewValue.isValid() && previewValue != originalValue) {
            keyframeIt->value = previewValue;
          }
        }
        for (auto &snapshot : afterSnapshots) {
          std::sort(snapshot.keyframes.begin(), snapshot.keyframes.end(),
                    [](const ArtifactCore::KeyFrame &lhs,
                       const ArtifactCore::KeyFrame &rhs) {
                      return lhs.time < rhs.time;
                    });
        }

        if (auto *mgr = UndoManager::instance()) {
          QPointer<ArtifactTimelineTrackPainterView> self(this);
          const auto trackRows = impl_->trackRows_;
          const QSet<QString> beforeSelectionKeys = impl_->selectedMarkerKeys_;
          const QSet<QString> afterSelectionKeys = nextSelectedKeys;
          const QString undoLabel =
              hasFrameChanges && hasValueChanges
                  ? QStringLiteral("Move Keyframes")
                  : (hasValueChanges
                         ? QStringLiteral("Adjust Keyframe Values")
                         : QStringLiteral("Move Keyframes"));
          mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
              undoLabel,
              [self, composition, afterSnapshots, afterSelectionKeys, trackRows]() {
                applyKeyframePropertySnapshots(composition, afterSnapshots);
                if (!self) {
                  return;
                }
                ArtifactLayerSelectionManager *selectionManager = nullptr;
                if (auto *app = ArtifactApplicationManager::instance()) {
                  selectionManager = app->layerSelectionManager();
                }
                self->syncSelectionState(composition, selectionManager, trackRows, true);
                self->setSelectedKeyframeKeys(afterSelectionKeys);
              },
              [self, composition, beforeSnapshots, beforeSelectionKeys, trackRows]() {
                applyKeyframePropertySnapshots(composition, beforeSnapshots);
                if (!self) {
                  return;
                }
                ArtifactLayerSelectionManager *selectionManager = nullptr;
                if (auto *app = ArtifactApplicationManager::instance()) {
                  selectionManager = app->layerSelectionManager();
                }
                self->syncSelectionState(composition, selectionManager, trackRows, true);
                self->setSelectedKeyframeKeys(beforeSelectionKeys);
              }));
        }

        applyKeyframePropertySnapshots(composition, afterSnapshots);
        ArtifactLayerSelectionManager *selectionManager = nullptr;
        if (auto *app = ArtifactApplicationManager::instance()) {
          selectionManager = app->layerSelectionManager();
        }
        syncSelectionState(composition, selectionManager, impl_->trackRows_, true);
        setSelectedKeyframeKeys(nextSelectedKeys);
      }
      applyMarkerSelectionSet(impl_->keyframeMarkers_, impl_->selectedMarkerKeys_,
                              nextSelectedKeys);
      impl_->rebuildMarkerCaches();
      Q_EMIT keyframeSelectionChanged(impl_->selectedMarkerKeys_.size());
      if (hasFrameChanges) {
        for (const auto &request : requests) {
          Q_EMIT keyframeMoveRequested(request.layerId, request.propertyPath,
                                       request.fromFrame, request.toFrame);
        }
      }
      if (hasFrameChanges) {
        if (impl_->proportionalEditingEnabled_ && selectionCount > 1) {
          Q_EMIT timelineDebugMessage(
              QStringLiteral("Dragged %1 %2 with proportional falloff (radius %3f)")
                  .arg(requests.size())
                  .arg(formatKeyframeNoun(requests.size()))
                  .arg(QString::number(impl_->proportionalEditRadius_, 'f', 1)));
        } else {
          Q_EMIT timelineDebugMessage(
              QStringLiteral("Dragged %1 %3 by %2 %4")
                  .arg(requests.size())
                  .arg(QString::number(
                      static_cast<qint64>(std::llround(deltaFrames))))
                  .arg(formatKeyframeNoun(requests.size()))
                  .arg(formatFrameUnit(static_cast<qint64>(std::llround(deltaFrames)))));
        }
      } else if (hasValueChanges) {
        Q_EMIT timelineDebugMessage(QStringLiteral("Adjusted keyframe values"));
      }
    }
    impl_->draggingMarker_ = false;
    impl_->dragMarkerIndex_ = -1;
    impl_->dragMarkerSelectionIndices_.clear();
    impl_->dragMarkerSelectionOrigFrames_.clear();
    impl_->dragMarkerSelectionOrigValues_.clear();
    impl_->dragMarkerSelectionOrigTypes_.clear();
    impl_->dragMarkerBeforeSnapshots_.clear();
    impl_->dragMarkerValueChanged_ = false;
    impl_->dragMarkerTargetFrame_ = 0.0;
    impl_->dragMarkerLastPaintFrame_ = std::numeric_limits<qint64>::min();
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
      else if (impl_->dragMode_ == DragMode::SlideBody)
        clipSlid(clip.clipId, clip.startFrame);
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
    case DragMode::SlideBody:
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
  const DragMode ctxBodyMode = (impl_->activeTool_ == ToolType::Slide)
                                   ? DragMode::SlideBody
                                   : DragMode::MoveBody;
  const auto clipHit = hitTestClips(
      impl_->clips_, impl_->trackHeights_, impl_->trackTops_, mouseX, mouseY,
      impl_->pixelsPerFrame_, impl_->horizontalOffset_, impl_->verticalOffset_, ctxBodyMode);
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
  const auto keyframeAreas = collectKeyframeAreas(
      impl_->keyframeMarkers_, impl_->trackHeights_, impl_->trackTops_,
      impl_->pixelsPerFrame_, impl_->horizontalOffset_, impl_->verticalOffset_);
  const auto areaHit = hitTestKeyframeAreas(keyframeAreas, mouseX, mouseY);
  const bool areaUnderCursor =
      areaHit.areaIndex >= 0 && areaHit.areaIndex < keyframeAreas.size();

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
  if (!canEditFocusedProperty && !clipUnderCursor && !markerUnderCursor &&
      !areaUnderCursor) {
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
         marker.label.isEmpty() ? tt("timeline.marker", "Keyframe") : marker.label;
     jumpToMarkerAct = menu.addAction(tt("timeline.jump_to_marker", "Jump to %1").arg(label));
     setActionIcon(jumpToMarkerAct, QStringLiteral("timeline_keyframe_select"));
   }
  QAction *addKeyframeAct = nullptr;
  QAction *removeKeyframeAct = nullptr;
  QAction *editSourceTextAct = nullptr;
  const bool isSourceTextProperty =
      targetPropertyPath.compare(QStringLiteral("text.value"), Qt::CaseInsensitive) == 0;
  if (canEditFocusedProperty) {
    menu.addSeparator();
    if (isSourceTextProperty) {
      editSourceTextAct = menu.addAction(
          tt("timeline.edit_source_text_at_playhead",
             "Edit Source Text at Playhead..."));
      setActionIcon(editSourceTextAct,
                    QStringLiteral("timeline_keyframe_interpolation"));
    }
    addKeyframeAct = menu.addAction(
        isSourceTextProperty
            ? tt("timeline.add_source_text_keyframe_at_playhead",
                 "Add Source Text Keyframe at Playhead")
            : tt("timeline.add_keyframe_at_playhead", "Add Keyframe at Playhead"));
    removeKeyframeAct =
        menu.addAction(isSourceTextProperty
                           ? tt("timeline.remove_source_text_keyframe_at_playhead",
                                "Remove Source Text Keyframe at Playhead")
                           : tt("timeline.remove_keyframe_at_playhead",
                                "Remove Keyframe at Playhead"));
    setActionIcon(addKeyframeAct, QStringLiteral("timeline_keyframe_add"));
    setActionIcon(removeKeyframeAct, QStringLiteral("timeline_keyframe_remove"));
  }

  const QVector<KeyframeMarkerVisual> selectedMarkers = selectedKeyframeMarkers();
  auto selectedMarkersAreNumeric = [&](const QVector<KeyframeMarkerVisual> &markers) {
    if (markers.isEmpty()) {
      return false;
    }
    if (!composition) {
      return false;
    }
    for (const auto &marker : markers) {
      const auto layer = composition->layerById(marker.layerId);
      if (!layer) {
        return false;
      }
      const auto property = findLayerPropertyByPath(layer, marker.propertyPath);
      if (!property) {
        return false;
      }
      const auto type = property->getType();
      if (type != ArtifactCore::PropertyType::Float &&
          type != ArtifactCore::PropertyType::Integer) {
        return false;
      }
    }
    return true;
  };
  QAction *selectAreaAct = nullptr;
  QAction *deleteAreaPointsAct = nullptr;
  QAction *setAreaValueAct = nullptr;
  if (areaUnderCursor) {
    menu.addSeparator();
    selectAreaAct = menu.addAction(tt("timeline.select_area", "Select Area"));
    deleteAreaPointsAct =
        menu.addAction(tt("timeline.delete_area_points", "Delete Area Points"));
    const auto &area = keyframeAreas[areaHit.areaIndex];
    bool numericArea = false;
    area.value.toDouble(&numericArea);
    if (numericArea) {
      setAreaValueAct =
          menu.addAction(tt("timeline.set_area_value", "Set Area Value..."));
      setActionIcon(setAreaValueAct, QStringLiteral("timeline_keyframe_interpolation"));
    }
    setActionIcon(selectAreaAct, QStringLiteral("timeline_keyframe_select"));
    setActionIcon(deleteAreaPointsAct, QStringLiteral("timeline_keyframe_remove"));
  }
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
        menu.addAction(tt("timeline.select_same_property_keyframes", "Select Same Property Keyframes"));
    selectNeighborKeyframesAct =
        menu.addAction(tt("timeline.select_neighbor_keyframes", "Select Neighbor Keyframes"));
    setActionIcon(selectSamePropertyKeyframesAct, QStringLiteral("timeline_keyframe_select"));
    setActionIcon(selectNeighborKeyframesAct, QStringLiteral("timeline_keyframe_select"));
  }
  QAction *duplicateSelectedKeyframesAct = nullptr;
  if (hasKeyframeTarget) {
    duplicateSelectedKeyframesAct =
        menu.addAction(tt("timeline.duplicate_keyframes_here", "Duplicate Keyframes Here"));
    setActionIcon(duplicateSelectedKeyframesAct, QStringLiteral("timeline_keyframe_duplicate"));
  }
  QAction *shrinkSelectedKeyframesAct = nullptr;
  QAction *stretchSelectedKeyframesAct = nullptr;
  QAction *reverseSelectedKeyframesAct = nullptr;
  QAction *reverseAllCurrentLayerKeyframesAct = nullptr;
  QAction *reverseAllSelectedLayersKeyframesAct = nullptr;
  QAction *reverseAllCompositionKeyframesAct = nullptr;
  QAction *normalizeSelectedKeyframesAct = nullptr;
  QAction *moveSelectedKeyframesToPlayheadAct = nullptr;
  QAction *fitSelectedKeyframesToWorkAreaAct = nullptr;
  QAction *scaleSelectedKeyframesAct = nullptr;
  QAction *offsetSelectedKeyframesAct = nullptr;
  QAction *setSelectedKeyframeValueAct = nullptr;
  if (!selectedMarkers.isEmpty()) {
    QMenu *editMenu = menu.addMenu(tt("timeline.keyframe_edit", "Keyframe Edit"));
    setMenuIcon(editMenu, QStringLiteral("timeline_keyframe_interpolation"));
    shrinkSelectedKeyframesAct = editMenu->addAction(tt("timeline.shrink_keys", "Shrink Keys"));
    stretchSelectedKeyframesAct = editMenu->addAction(tt("timeline.stretch_keys", "Stretch Keys"));
    reverseSelectedKeyframesAct = editMenu->addAction(tt("timeline.reverse_keys", "Reverse Keys"));
    reverseAllCurrentLayerKeyframesAct =
        editMenu->addAction(tt("timeline.reverse_all_layer_keys", "Reverse All Keys in Layer"));
    reverseAllSelectedLayersKeyframesAct =
        editMenu->addAction(tt("timeline.reverse_all_selected_layer_keys", "Reverse All Keys in Selected Layers"));
    reverseAllCompositionKeyframesAct =
        editMenu->addAction(tt("timeline.reverse_all_comp_keys", "Reverse All Keys in Composition"));
    normalizeSelectedKeyframesAct = editMenu->addAction(tt("timeline.normalize_duration", "Normalize Duration"));
    moveSelectedKeyframesToPlayheadAct =
        editMenu->addAction(tt("timeline.move_keys_to_playhead", "Move Keys to Playhead"));
    fitSelectedKeyframesToWorkAreaAct =
        editMenu->addAction(tt("timeline.fit_keys_to_work_area", "Fit Keys to Work Area"));
    if (selectedMarkersAreNumeric(selectedMarkers)) {
      setSelectedKeyframeValueAct =
          editMenu->addAction(tt("timeline.set_keyframe_value", "Set Keyframe Value..."));
      setActionIcon(setSelectedKeyframeValueAct, QStringLiteral("timeline_keyframe_interpolation"));
    }
    scaleSelectedKeyframesAct = editMenu->addAction(tt("timeline.scale_values", "Scale Values"));
    offsetSelectedKeyframesAct = editMenu->addAction(tt("timeline.offset_values", "Offset Values"));
  }
  QAction *distributeSelectedKeyframesAct = nullptr;
  QAction *repeatSelectedKeyframesAct = nullptr;
  if (selectedMarkers.size() >= 2) {
    QMenu *arrangeMenu = menu.addMenu(tt("timeline.arrange", "Arrange"));
    setMenuIcon(arrangeMenu, QStringLiteral("timeline_keyframe_interpolation"));
    distributeSelectedKeyframesAct =
        arrangeMenu->addAction(tt("timeline.distribute_evenly", "Distribute Evenly"));
    repeatSelectedKeyframesAct =
        arrangeMenu->addAction(tt("timeline.repeat_pattern", "Repeat Pattern..."));
    setActionIcon(distributeSelectedKeyframesAct, QStringLiteral("timeline_keyframe_interpolation"));
    setActionIcon(repeatSelectedKeyframesAct, QStringLiteral("timeline_keyframe_duplicate"));
  } else if (hasKeyframeTarget) {
    menu.addSeparator();
    repeatSelectedKeyframesAct =
        menu.addAction(tt("timeline.repeat_pattern", "Repeat Pattern..."));
    setActionIcon(repeatSelectedKeyframesAct, QStringLiteral("timeline_keyframe_duplicate"));
  }
  QAction *keyPatternDialogAct = nullptr;
  if (canEditFocusedProperty) {
    keyPatternDialogAct = menu.addAction(tt("timeline.key_pattern_dialog", "Key Pattern Dialog"));
    setActionIcon(keyPatternDialogAct, QStringLiteral("timeline_keyframe_interpolation"));
  }
  QAction *copySelectedKeyframesAct = nullptr;
  QAction *cutSelectedKeyframesAct = nullptr;
  QAction *pasteKeyframesAct = nullptr;
  if (hasKeyframeTarget) {
    copySelectedKeyframesAct =
        menu.addAction(tt("timeline.copy_selected_keyframes", "Copy Selected Keyframes"));
    cutSelectedKeyframesAct =
        menu.addAction(tt("timeline.cut_selected_keyframes", "Cut Selected Keyframes"));
    setActionIcon(copySelectedKeyframesAct, QStringLiteral("timeline_keyframe_copy"));
    setActionIcon(cutSelectedKeyframesAct, QStringLiteral("timeline_keyframe_remove"));
  }
  if (ClipboardManager::instance().hasKeyframeData()) {
    pasteKeyframesAct =
        menu.addAction(tt("timeline.paste_keyframes_at_playhead", "Paste Keyframes at Playhead"));
    setActionIcon(pasteKeyframesAct, QStringLiteral("timeline_keyframe_paste"));
  }
  QAction *deleteSelectedKeyframesAct = nullptr;
  if (hasKeyframeTarget) {
    menu.addSeparator();
    deleteSelectedKeyframesAct =
        menu.addAction(tt("timeline.delete_selected_keyframes", "Delete Selected Keyframes"));
    setActionIcon(deleteSelectedKeyframesAct, QStringLiteral("timeline_keyframe_remove"));
  }
  QAction *cleanSelectedKeyframesAct = nullptr;
  if (!cleanTargetRefs.isEmpty()) {
    cleanSelectedKeyframesAct =
        menu.addAction(tt("timeline.keyframe_clean", "Keyframe Clean"));
    setActionIcon(cleanSelectedKeyframesAct, QStringLiteral("timeline_keyframe_clean"));
  }
  QAction *staggerStartAct = nullptr;
  QAction *staggerEndAct = nullptr;
  QAction *cascadeClipsAct = nullptr;
  QAction *overlapClipsAct = nullptr;
  QAction *reverseOrderStaggerAct = nullptr;
  QAction *randomStaggerAct = nullptr;
  if (clipUnderCursor) {
    QMenu *staggerMenu = menu.addMenu(tt("timeline.clip_stagger", "Clip Stagger"));
    staggerStartAct = staggerMenu->addAction(tt("timeline.stagger_start_4f", "Stagger Start +4f"));
    staggerEndAct = staggerMenu->addAction(tt("timeline.stagger_end_4f", "Stagger End +4f"));
    cascadeClipsAct = staggerMenu->addAction(tt("timeline.cascade_clips", "Cascade Clips"));
    overlapClipsAct = staggerMenu->addAction(tt("timeline.overlap_by_4_frames", "Overlap by 4 Frames"));
    reverseOrderStaggerAct = staggerMenu->addAction(tt("timeline.reverse_order_stagger", "Reverse Order Stagger"));
    randomStaggerAct = staggerMenu->addAction(tt("timeline.random_stagger", "Random Stagger"));
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
    interpolationMenu = menu.addMenu(tt("timeline.interpolation", "Interpolation"));
    setMenuIcon(interpolationMenu, QStringLiteral("timeline_keyframe_interpolation"));
    interpLinearAct = interpolationMenu->addAction(tt("timeline.linear", "Linear"));
    interpEaseInAct = interpolationMenu->addAction(tt("timeline.ease_in", "Ease In"));
    interpEaseOutAct = interpolationMenu->addAction(tt("timeline.ease_out", "Ease Out"));
    interpEaseInOutAct = interpolationMenu->addAction(tt("timeline.ease_in_out", "Ease In/Out"));
    interpHoldAct = interpolationMenu->addAction(tt("timeline.hold", "Hold"));
    interpBezierAct = interpolationMenu->addAction(tt("timeline.bezier", "Bezier"));
    setActionIcon(interpLinearAct, QStringLiteral("timeline_keyframe_interpolation"));
    setActionIcon(interpEaseInAct, QStringLiteral("timeline_keyframe_interpolation"));
    setActionIcon(interpEaseOutAct, QStringLiteral("timeline_keyframe_interpolation"));
    setActionIcon(interpEaseInOutAct, QStringLiteral("timeline_keyframe_interpolation"));
    setActionIcon(interpHoldAct, QStringLiteral("timeline_keyframe_anchor"));
    setActionIcon(interpBezierAct, QStringLiteral("timeline_keyframe_interpolation"));
  }
  QAction *breakTangentsAct = nullptr;
  QAction *unifyTangentsAct = nullptr;
  if (!interpolationTargets.isEmpty()) {
    interpolationMenu->addSeparator();
    breakTangentsAct = interpolationMenu->addAction(
        tt("timeline.break_tangents", "Break Tangents"));
    unifyTangentsAct = interpolationMenu->addAction(
        tt("timeline.unify_tangents", "Unify Tangents"));
    setActionIcon(breakTangentsAct, QStringLiteral("timeline_keyframe_interpolation"));
    setActionIcon(unifyTangentsAct, QStringLiteral("timeline_keyframe_interpolation"));
  }
  QAction *rovingOnAct = nullptr;
  QAction *rovingOffAct = nullptr;
  QMenu *rovingMenu = nullptr;
  if (!interpolationTargets.isEmpty()) {
    rovingMenu = menu.addMenu(tt("timeline.roving", "Roving"));
    setMenuIcon(rovingMenu, QStringLiteral("timeline_keyframe_roving"));
    const bool anyRoving = std::any_of(interpolationTargets.cbegin(), interpolationTargets.cend(),
                                       [](const auto &marker) { return marker.roving; });
    const bool allRoving = std::all_of(interpolationTargets.cbegin(), interpolationTargets.cend(),
                                       [](const auto &marker) { return marker.roving; });
    rovingOnAct = rovingMenu->addAction(tt("timeline.mark_as_roving", "Mark as Roving"));
    rovingOffAct = rovingMenu->addAction(tt("timeline.clear_roving", "Clear Roving"));
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
    QMenu *anchorMenu = menu.addMenu(tt("timeline.keyframe_anchor", "Keyframe Anchor"));
    setMenuIcon(anchorMenu, QStringLiteral("timeline_keyframe_anchor"));
    anchorAbsoluteAct = anchorMenu->addAction(tt("timeline.absolute", "Absolute"));
    anchorLockToInAct = anchorMenu->addAction(tt("timeline.lock_to_in_point", "Lock to In Point"));
    anchorLockToOutAct = anchorMenu->addAction(tt("timeline.lock_to_out_point", "Lock to Out Point"));
    anchorStretchAct = anchorMenu->addAction(tt("timeline.stretch_with_layer", "Stretch with Layer"));
    setActionIcon(anchorAbsoluteAct, QStringLiteral("timeline_keyframe_anchor"));
    setActionIcon(anchorLockToInAct, QStringLiteral("timemenu_in_point"));
    setActionIcon(anchorLockToOutAct, QStringLiteral("timemenu_out_point"));
    setActionIcon(anchorStretchAct, QStringLiteral("timeline_keyframe_roving"));
  }
  if (!selectedMarkers.isEmpty()) {
    QMenu *colorMenu = menu.addMenu(tt("timeline.keyframe_color_label", "Keyframe Color Label"));
    setMenuIcon(colorMenu, QStringLiteral("timeline_keyframe_color"));
    colorNoneAct = colorMenu->addAction(tt("timeline.none", "None"));
    colorRedAct = colorMenu->addAction(tt("timeline.red", "Red"));
    colorBlueAct = colorMenu->addAction(tt("timeline.blue", "Blue"));
    colorYellowAct = colorMenu->addAction(tt("timeline.yellow", "Yellow"));
    colorGreenAct = colorMenu->addAction(tt("timeline.green", "Green"));
    colorPurpleAct = colorMenu->addAction(tt("timeline.purple", "Purple"));
    colorGrayAct = colorMenu->addAction(tt("timeline.gray", "Gray"));
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
        menu.addAction(tt("timeline.add_keyframe_at_marker_frame", "Add Keyframe at Marker Frame"));
    removeMarkerFrameAct =
        menu.addAction(tt("timeline.remove_keyframe_at_marker_frame", "Remove Keyframe at Marker Frame"));
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
  QAction *moveSelectedToPlayheadAct = nullptr;
  QAction *fitSelectedToWorkAreaAct = nullptr;
  QAction *setWorkAreaToSelectedAct = nullptr;
  QAction *deleteClipAct = nullptr;
  if (clipUnderCursor) {
    if (!markerUnderCursor) {
      menu.addSeparator();
    }
    splitClipAct = menu.addAction(tt("timeline.split_layer_at_playhead", "Split Layer at Playhead"));
    duplicateClipAct = menu.addAction(tt("timeline.duplicate_layer", "Duplicate Layer"));
    moveStartClipAct = menu.addAction(tt("timeline.move_start_to_playhead", "Move Start to Playhead"));
    trimInClipAct = menu.addAction(tt("timeline.trim_in_at_playhead", "Trim In at Playhead"));
    trimOutClipAct = menu.addAction(tt("timeline.trim_out_at_playhead", "Trim Out at Playhead"));
    rippleTrimOutClipAct =
        menu.addAction(tt("timeline.ripple_trim_out_at_playhead", "Ripple Trim Out at Playhead"));
    rippleTrimInClipAct =
        menu.addAction(tt("timeline.ripple_trim_in_at_playhead", "Ripple Trim In at Playhead"));
    rippleDeleteClipAct =
        menu.addAction(tt("timeline.ripple_delete_close_gap", "Ripple Delete (Close Gap)"));
    QMenu *rangeMenu = menu.addMenu(tt("timeline.selection_range", "Selection Range"));
    moveSelectedToPlayheadAct =
        rangeMenu->addAction(tt("timeline.move_selected_to_playhead", "Move Selected to Playhead"));
    fitSelectedToWorkAreaAct =
        rangeMenu->addAction(tt("timeline.fit_selected_to_work_area", "Fit Selected to Work Area"));
    setWorkAreaToSelectedAct =
        rangeMenu->addAction(tt("timeline.set_work_area_to_selected", "Set Work Area to Selected"));
    deleteClipAct = menu.addAction(tt("timeline.delete_layer", "Delete Layer"));
  }

  const QAction *chosen = menu.exec(event->globalPos());
  if (!chosen) {
    event->accept();
    return;
  }

  if (areaUnderCursor &&
      (chosen == selectAreaAct || chosen == deleteAreaPointsAct ||
       chosen == setAreaValueAct)) {
    const auto &area = keyframeAreas[areaHit.areaIndex];
    const QSet<QString> areaSelection = keyframeSelectionSetForArea(area);
    setSelectedKeyframeKeys(areaSelection);
    setKeyframeContext(area.layerId, area.propertyPath);
    clipSelected(QString(), area.layerId);
    if (chosen == setAreaValueAct) {
      bool numericArea = false;
      double currentValue = area.value.toDouble(&numericArea);
      const bool scalePercent = isPercentScalePropertyPath(area.propertyPath);
      if (numericArea && scalePercent) {
        currentValue *= 100.0;
      }
      if (numericArea && composition) {
        bool accepted = false;
        const double nextValue = QInputDialog::getDouble(
            this, tt("timeline.set_area_value", "Set Area Value"),
            scalePercent ? tt("timeline.area_value_prompt_percent", "Value (%)")
                         : tt("timeline.area_value_prompt", "Value"),
            currentValue, -1000000.0,
            1000000.0, 3, &accepted);
        if (accepted) {
          const double storedValue =
              scalePercent ? nextValue / 100.0 : nextValue;
          QVector<KeyframePropertySnapshot> beforeSnapshots;
          QVector<KeyframePropertySnapshot> afterSnapshots;
          if (applyValueToKeyframeArea(composition, area, QVariant(storedValue),
                                       &beforeSnapshots, &afterSnapshots)) {
            if (auto *mgr = UndoManager::instance()) {
              QPointer<ArtifactTimelineTrackPainterView> self(this);
              const auto trackRows = impl_->trackRows_;
              mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
                  QStringLiteral("Set Keyframe Area Value"),
                  [self, composition, afterSnapshots, areaSelection, trackRows]() {
                    applyKeyframePropertySnapshots(composition, afterSnapshots);
                    if (!self) {
                      return;
                    }
                    ArtifactLayerSelectionManager *selectionManager = nullptr;
                    if (auto *app = ArtifactApplicationManager::instance()) {
                      selectionManager = app->layerSelectionManager();
                    }
                    self->syncSelectionState(composition, selectionManager, trackRows,
                                             true);
                    self->setSelectedKeyframeKeys(areaSelection);
                  },
                  [self, composition, beforeSnapshots, areaSelection, trackRows]() {
                    applyKeyframePropertySnapshots(composition, beforeSnapshots);
                    if (!self) {
                      return;
                    }
                    ArtifactLayerSelectionManager *selectionManager = nullptr;
                    if (auto *app = ArtifactApplicationManager::instance()) {
                      selectionManager = app->layerSelectionManager();
                    }
                    self->syncSelectionState(composition, selectionManager, trackRows,
                                             true);
                    self->setSelectedKeyframeKeys(areaSelection);
                  }));
            }
            ArtifactLayerSelectionManager *selectionManager = nullptr;
            if (auto *app = ArtifactApplicationManager::instance()) {
              selectionManager = app->layerSelectionManager();
            }
            syncSelectionState(composition, selectionManager, impl_->trackRows_, true);
            setSelectedKeyframeKeys(areaSelection);
            Q_EMIT timelineDebugMessage(
                QStringLiteral("Set keyframe area value to %1")
                    .arg(QString::number(nextValue, 'f', 3)));
          }
        }
      }
    }
    if (chosen == deleteAreaPointsAct) {
      deleteSelectedKeyframeMarkers();
    }
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

  if (setSelectedKeyframeValueAct && chosen == setSelectedKeyframeValueAct) {
    promptSetSelectedKeyframeValue();
    event->accept();
    return;
  }

  if (breakTangentsAct && (chosen == breakTangentsAct || chosen == unifyTangentsAct)) {
    const bool breakMode = (chosen == breakTangentsAct);
    ArtifactCompositionPtr currentComposition = composition;
    if (!currentComposition) {
      if (auto *svc = ArtifactProjectService::instance()) {
        currentComposition = svc->currentComposition().lock();
      }
    }
    if (currentComposition) {
      const auto refs = collectPropertyRefsFromMarkers(interpolationTargets);
      const auto beforeSnapshots = captureKeyframePropertySnapshots(currentComposition, refs);
      QVector<KeyframePropertySnapshot> afterSnapshots = beforeSnapshots;
      const double fps = std::max(1.0, static_cast<double>(currentComposition->frameRate().framerate()));
      const int64_t scale = static_cast<int64_t>(std::llround(fps));
      for (auto &snapshot : afterSnapshots) {
        const auto layer = currentComposition->layerById(snapshot.layerId);
        if (!layer) continue;
        const auto property = findLayerPropertyByPath(layer, snapshot.propertyPath);
        if (!property) continue;
        for (auto &keyframe : snapshot.keyframes) {
          const RationalTime keyTime = keyframe.time;
          if (property->hasKeyFrameAt(keyTime)) {
            if (breakMode) {
              keyframe.cp1_x = std::abs(keyframe.cp1_x) > 0
                                   ? keyframe.cp1_x
                                   : static_cast<int64_t>(
                                         std::llround(static_cast<double>(scale) * 0.167));
              keyframe.cp2_x = std::abs(keyframe.cp2_x) > 0
                                   ? keyframe.cp2_x
                                   : static_cast<int64_t>(
                                         std::llround(static_cast<double>(scale) * 0.167));
            } else {
              const int64_t mirrorFrame = std::max<int64_t>(0, keyframe.cp2_x);
              const float mirrorValue = keyframe.cp2_y;
              keyframe.cp1_x = -mirrorFrame;
              keyframe.cp1_y = -mirrorValue;
              keyframe.cp2_x = mirrorFrame;
              keyframe.cp2_y = mirrorValue;
            }
          }
        }
      }
      applyKeyframePropertySnapshots(currentComposition, afterSnapshots);
      if (auto *mgr = UndoManager::instance()) {
        QPointer<ArtifactTimelineTrackPainterView> self(this);
        mgr->push(std::make_unique<TimelineKeyframeSnapshotCommand>(
            breakMode ? QStringLiteral("Break Tangents") : QStringLiteral("Unify Tangents"),
            [self, currentComposition, afterSnapshots]() {
              applyKeyframePropertySnapshots(currentComposition, afterSnapshots);
              if (self) {
                ArtifactLayerSelectionManager *sel = nullptr;
                if (auto *app = ArtifactApplicationManager::instance())
                  sel = app->layerSelectionManager();
                self->syncSelectionState(currentComposition, sel, self->impl_->trackRows_, true);
              }
            },
            [self, currentComposition, beforeSnapshots]() {
              applyKeyframePropertySnapshots(currentComposition, beforeSnapshots);
              if (self) {
                ArtifactLayerSelectionManager *sel = nullptr;
                if (auto *app = ArtifactApplicationManager::instance())
                  sel = app->layerSelectionManager();
                self->syncSelectionState(currentComposition, sel, self->impl_->trackRows_, true);
              }
            }));
      }
      Q_EMIT timelineDebugMessage(
          breakMode ? QStringLiteral("Broke tangents on selected keyframes")
                    : QStringLiteral("Unified tangents on selected keyframes"));
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
      chosen == reverseSelectedKeyframesAct || chosen == reverseAllCurrentLayerKeyframesAct ||
      chosen == reverseAllSelectedLayersKeyframesAct || chosen == reverseAllCompositionKeyframesAct ||
      chosen == normalizeSelectedKeyframesAct ||
      chosen == moveSelectedKeyframesToPlayheadAct ||
      chosen == fitSelectedKeyframesToWorkAreaAct ||
      chosen == scaleSelectedKeyframesAct || chosen == offsetSelectedKeyframesAct) {
    if (chosen == reverseSelectedKeyframesAct) {
      reverseSelectedKeyframeMarkers();
      event->accept();
      return;
    }
    if (chosen == reverseAllCurrentLayerKeyframesAct) {
      reverseAllKeyframesInCurrentLayer();
      event->accept();
      return;
    }
    if (chosen == reverseAllSelectedLayersKeyframesAct) {
      reverseAllKeyframesInSelectedLayers();
      event->accept();
      return;
    }
    if (chosen == reverseAllCompositionKeyframesAct) {
      reverseAllKeyframesInComposition();
      event->accept();
      return;
    }
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
      } else if (chosen == normalizeSelectedKeyframesAct) {
        options.kind = KeyframeRangeTransformKind::Normalize;
        options.targetDuration = std::max<qint64>(1, contextFrame + 1);
      } else if (chosen == moveSelectedKeyframesToPlayheadAct) {
        options.kind = KeyframeRangeTransformKind::Stretch;
        options.scale = 1.0;
        options.useTargetStartFrame = true;
        options.targetStartFrame = static_cast<qint64>(std::llround(
            std::clamp(impl_->currentFrame_, 0.0,
                       std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)))));
      } else if (chosen == fitSelectedKeyframesToWorkAreaAct) {
        const ArtifactCore::FrameRange workArea = currentComposition->workAreaRange();
        options.kind = KeyframeRangeTransformKind::Normalize;
        options.targetDuration = std::max<qint64>(1, workArea.end() - workArea.start());
        options.useTargetStartFrame = true;
        options.targetStartFrame = std::max<qint64>(0, workArea.start());
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
    QWidget *cursor = parentWidget();
    while (cursor) {
      if (auto *button = cursor->findChild<QToolButton *>(
              QStringLiteral("timelineKeyPatternButton"),
              Qt::FindChildrenRecursively)) {
        button->click();
        break;
      }
      cursor = cursor->parentWidget();
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
      chosen == cascadeClipsAct || chosen == overlapClipsAct ||
      chosen == reverseOrderStaggerAct || chosen == randomStaggerAct) {
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
    QVector<ArtifactAbstractLayerPtr> orderedLayers = selectedLayers.values().toVector();
    std::sort(orderedLayers.begin(), orderedLayers.end(),
              [](const auto &lhs, const auto &rhs) {
                if (!lhs || !rhs) {
                  return lhs && !rhs;
                }
                const qint64 lhsIn = lhs->inPoint().framePosition();
                const qint64 rhsIn = rhs->inPoint().framePosition();
                if (lhsIn != rhsIn) {
                  return lhsIn < rhsIn;
                }
                return lhs->id().toString() < rhs->id().toString();
              });
    if (chosen == reverseOrderStaggerAct) {
      std::reverse(orderedLayers.begin(), orderedLayers.end());
    } else if (chosen == randomStaggerAct) {
      std::shuffle(orderedLayers.begin(), orderedLayers.end(),
                   std::mt19937{static_cast<unsigned>(
                       std::max<qint64>(1, contextFrame + 1))});
    }
    const qint64 step = 4;
    qint64 cursor = contextFrame;
    for (const auto &layer : orderedLayers) {
      if (!layer) {
        continue;
      }
      if (chosen == staggerStartAct) {
        moveTimelineLayer(currentComp->id(), layer->id().toString(),
                          cursor, 0.0);
        cursor += step;
      } else if (chosen == staggerEndAct) {
        const qint64 duration = layer->outPoint().framePosition() -
                                layer->inPoint().framePosition();
        moveTimelineLayer(currentComp->id(), layer->id().toString(),
                          std::max<qint64>(0, cursor - duration), 0.0);
        cursor += step;
      } else if (chosen == cascadeClipsAct) {
        moveTimelineLayer(currentComp->id(), layer->id().toString(),
                          cursor, 0.0);
        cursor = layer->outPoint().framePosition();
      } else if (chosen == overlapClipsAct) {
        moveTimelineLayer(currentComp->id(), layer->id().toString(),
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

  if ((chosen == moveSelectedToPlayheadAct || chosen == fitSelectedToWorkAreaAct ||
       chosen == setWorkAreaToSelectedAct) &&
      composition) {
    auto *selManager = ArtifactApplicationManager::instance()
                           ? ArtifactApplicationManager::instance()->layerSelectionManager()
                           : nullptr;
    const auto selectedLayers = selManager ? selManager->selectedLayers()
                                           : QSet<ArtifactAbstractLayerPtr>{};
    QVector<ArtifactAbstractLayerPtr> orderedLayers = selectedLayers.values().toVector();
    std::sort(orderedLayers.begin(), orderedLayers.end(),
              [](const auto &lhs, const auto &rhs) {
                if (!lhs || !rhs) {
                  return lhs && !rhs;
                }
                const qint64 lhsIn = lhs->inPoint().framePosition();
                const qint64 rhsIn = rhs->inPoint().framePosition();
                if (lhsIn != rhsIn) {
                  return lhsIn < rhsIn;
                }
                return lhs->id().toString() < rhs->id().toString();
              });

    if (orderedLayers.isEmpty() && clipUnderCursor) {
      const auto &clip = impl_->clips_[clipHit.clipIndex];
      if (auto layer = composition->layerById(clip.layerId)) {
        orderedLayers.push_back(layer);
      }
    }

    bool changed = false;
    const qint64 currentFrame = static_cast<qint64>(std::llround(
        std::clamp(impl_->currentFrame_, 0.0,
                   std::max<double>(0.0, static_cast<double>(impl_->durationFrames_ - 1.0)))));

    if (chosen == moveSelectedToPlayheadAct) {
      for (const auto &layer : orderedLayers) {
        if (!layer || layer->isLocked()) {
          continue;
        }
        changed |= applyTimelineLayerRangeEdit(layer, currentFrame, 0, true);
      }
    } else if (chosen == fitSelectedToWorkAreaAct) {
      const ArtifactCore::FrameRange workArea = composition->workAreaRange();
      const qint64 workStart = std::max<qint64>(0, workArea.start());
      const qint64 workDuration = std::max<qint64>(1, workArea.end() - workStart);
      for (const auto &layer : orderedLayers) {
        if (!layer || layer->isLocked()) {
          continue;
        }
        changed |= applyTimelineLayerRangeEdit(layer, workStart, workDuration, false);
      }
    } else if (chosen == setWorkAreaToSelectedAct && !orderedLayers.isEmpty()) {
      qint64 minIn = std::numeric_limits<qint64>::max();
      qint64 maxOut = 0;
      for (const auto &layer : orderedLayers) {
        if (!layer) {
          continue;
        }
        minIn = std::min(minIn, layer->inPoint().framePosition());
        maxOut = std::max(maxOut, layer->outPoint().framePosition());
      }
      if (minIn != std::numeric_limits<qint64>::max() && maxOut > minIn) {
        composition->setWorkAreaRange(ArtifactCore::FrameRange(minIn, maxOut));
        changed = true;
      }
    }

    if (changed) {
      const QString actionText =
          chosen == moveSelectedToPlayheadAct
              ? QStringLiteral("Moved selected layers to playhead")
              : chosen == fitSelectedToWorkAreaAct
                    ? QStringLiteral("Fit selected layers to work area")
                    : QStringLiteral("Set work area to selected layers");
      Q_EMIT timelineDebugMessage(actionText);
      update();
    }
    event->accept();
    return;
  }

  if (clipUnderCursor && composition) {
    const auto &clip = impl_->clips_[clipHit.clipIndex];
    const auto layer = composition->layerById(clip.layerId);
    if (chosen == splitClipAct) {
      if (auto *svc = ArtifactProjectService::instance()) {
        svc->splitLayerWithUndo(composition->id(), clip.layerId);
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

  if (chosen == editSourceTextAct) {
    const auto editLayer =
        composition ? composition->layerById(targetLayerId)
                    : ArtifactAbstractLayerPtr{};
    if (!editLayer) {
      event->accept();
      return;
    }

    const auto textLayer =
        std::dynamic_pointer_cast<ArtifactTextLayer>(editLayer);
    if (!textLayer) {
      event->accept();
      return;
    }

    const QString currentText = textLayer->sourceTextAtFrame(contextFrame);
    const auto textProperty = textLayer->getProperty(QStringLiteral("text.value"));
    const auto beforeKeyframes =
        textProperty ? textProperty->getKeyFrames()
                     : std::vector<ArtifactCore::KeyFrame>{};
    bool ok = false;
    const QString editedText = QInputDialog::getMultiLineText(
        this, tt("timeline.edit_source_text_title", "Edit Source Text"),
        tt("timeline.edit_source_text_prompt", "Source text at the playhead:"),
        currentText, &ok);
    if (!ok) {
      event->accept();
      return;
    }

    auto afterKeyframes = beforeKeyframes;
    ArtifactCore::KeyFrame editedKeyframe;
    editedKeyframe.time = RationalTime(contextFrame, std::max(1, static_cast<int>(std::round(composition->frameRate().framerate()))));
    editedKeyframe.value = editedText;
    editedKeyframe.interpolation = ArtifactCore::InterpolationType::Constant;
    auto existing = std::find_if(afterKeyframes.begin(), afterKeyframes.end(),
                                 [editedKeyframe](const ArtifactCore::KeyFrame &keyframe) {
                                   return keyframe.time == editedKeyframe.time;
                                 });
    if (existing != afterKeyframes.end()) {
      *existing = editedKeyframe;
    } else {
      afterKeyframes.push_back(editedKeyframe);
    }
    if (auto *mgr = UndoManager::instance()) {
      mgr->push(std::make_unique<SetLayerPropertyKeyframesCommand>(
          editLayer, QStringLiteral("text.value"), beforeKeyframes,
          afterKeyframes, QStringLiteral("Edit Source Text")));
    } else {
      textLayer->setSourceTextAtFrame(contextFrame, editedText);
    }
    Q_EMIT timelineDebugMessage(
        QStringLiteral("Edited Source Text at F%1").arg(contextFrame));
    update();
    event->accept();
    return;
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

  if (!(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
    if (event->key() == Qt::Key_O) {
      impl_->proportionalEditingEnabled_ = !impl_->proportionalEditingEnabled_;
      Q_EMIT timelineDebugMessage(
          impl_->proportionalEditingEnabled_
              ? QStringLiteral("Timeline proportional editing enabled")
              : QStringLiteral("Timeline proportional editing disabled"));
      update();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_BracketLeft ||
        event->key() == Qt::Key_BracketRight) {
      const double scale =
          event->key() == Qt::Key_BracketLeft ? (1.0 / 1.15) : 1.15;
      impl_->proportionalEditRadius_ =
          std::clamp(impl_->proportionalEditRadius_ * scale,
                     kMinTimelineProportionalRadius,
                     kMaxTimelineProportionalRadius);
      Q_EMIT timelineDebugMessage(
          QStringLiteral("Timeline proportional radius: %1f")
              .arg(QString::number(impl_->proportionalEditRadius_, 'f', 1)));
      update();
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

  if (shortcuts.matches(event, ArtifactCore::ShortcutId::TimelineSplitLayerAtPlayhead)) {
    if (auto *activeContext = ArtifactActiveContextService::instance()) {
      activeContext->splitLayerAtCurrentTime();
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
  if ((event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) &&
      (event->modifiers() & Qt::AltModifier) &&
      !(event->modifiers() & Qt::ControlModifier)) {
    const auto composition =
        ArtifactProjectService::instance()
            ? ArtifactProjectService::instance()->currentComposition().lock()
            : ArtifactCompositionPtr{};
    auto *selectionManager = ArtifactLayerSelectionManager::instance();
    if (composition && selectionManager) {
      const bool isShift = event->modifiers() & Qt::ShiftModifier;
      const qint64 step = isShift ? 10 : 1;
      const qint64 direction = (event->key() == Qt::Key_Right) ? 1 : -1;
      const qint64 delta = direction * step;

      auto selectedLayers = selectionManager->selectedLayers();
      if (selectedLayers.isEmpty()) {
        if (auto current = selectionManager->currentLayer()) {
          selectedLayers.insert(current);
        }
      }

      QVector<ArtifactAbstractLayerPtr> layers;
      for (const auto &layer : selectedLayers) {
        if (layer && !layer->isTimingLocked()) {
          layers.push_back(layer);
        }
      }

      if (!layers.isEmpty()) {
        auto macro =
            std::make_unique<MacroUndoCommand>(QStringLiteral("Slide Clips"));
        int commandCount = 0;
        for (const auto &layer : layers) {
          if (!layer) {
            continue;
          }
          const QVector<ArtifactAbstractLayerPtr> singleLayer{layer};
          const auto beforeSnapshots =
              captureTimelineLayerStateSnapshots(composition, singleLayer);
          macro->addChild(std::make_unique<SlideClipCommand>(
              composition->id(), layer->id(),
              layer->inPoint().framePosition() + delta, beforeSnapshots));
          ++commandCount;
        }

        if (commandCount > 0) {
          if (auto *mgr = UndoManager::instance()) {
            mgr->push(std::move(macro));
          }
          Q_EMIT timelineDebugMessage(
              QStringLiteral("Slid %1 layer(s) by %2 frame(s)")
                  .arg(commandCount)
                  .arg(delta));
          event->accept();
          return;
        }
      }
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
  impl_->rebuildMarkerCaches();
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
  impl_->dragMarkerLastPaintFrame_ = std::numeric_limits<qint64>::min();
  impl_->dragMarkerSnapLabel_.clear();
  impl_->dragMarkerSelectionIndices_.clear();
  impl_->dragMarkerSelectionOrigFrames_.clear();
  impl_->dragAreaIndex_ = -1;
  impl_->dragAreaPart_ = KeyframeAreaHitPart::None;
  impl_->dragAreaSelectionIndices_.clear();
  impl_->dragAreaSelectionOrigFrames_.clear();
  impl_->dragAreaOrigStartFrame_ = 0.0;
  impl_->dragAreaOrigEndFrame_ = 0.0;
  impl_->dragAreaPivotFrame_ = 0.0;
  impl_->dragAreaSnapLabel_.clear();
  impl_->dragAreaValue_.clear();
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
  if (hasAcceptedDroppedAssetUrl(event->mimeData())) {
    event->acceptProposedAction();
    return;
  }
  event->ignore();
}

void ArtifactTimelineTrackPainterView::dragMoveEvent(QDragMoveEvent *event) {
  if (hasAcceptedDroppedAssetUrl(event->mimeData())) {
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
