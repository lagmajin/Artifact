module;

#include <QSize>
#include <QMatrix4x4>
#include <QJsonDocument>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector3D>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QWaitCondition>
#include <QtGlobal>
#include <cmath>

export module Artifact.Widgets.CompositionHostFrameSnapshot;

import Artifact.Composition.Abstract;
import Artifact.Service.Playback;

export namespace Artifact {

enum class CompositionHostToolKind : quint8 {
  Other = 0,
  TrackPoint,
  RigSelect,
  RigWeight,
  Clone,
  Pen,
  Rectangle
};

// GUI-owned host facts and viewport transform copied into a value object
// before render work begins. It intentionally carries no QWidget, QObject,
// or renderer references.
struct CompositionHostFrameSnapshot final {
  bool hasHost = false;
  bool visible = true;
  bool viewportInteracting = false;
  bool cancelInvalidGizmoSession = false;
  bool showDebugProbeLines = false;
  bool showMaskPathLines = false;
  bool showMaskHandleLines = false;
  bool showBoundsLines = false;
  bool showSelectionRectLines = false;
  bool trackerToolActive = false;
  int selectedLayerCount = 0;
  bool shiftModifierActive = false;
  bool altModifierActive = false;
  bool controlModifierActive = false;
  CompositionHostToolKind activeToolKind = CompositionHostToolKind::Other;
  QString activeToolLabel;
  QSize logicalSize;
  qreal devicePixelRatio = 1.0;
  float viewportZoom = 1.0f;
  float viewportPanX = 0.0f;
  float viewportPanY = 0.0f;
  bool viewportTransformCaptured = false;
  bool interactiveRenderRegionActive = false;
  QRectF interactiveRenderRegionRect;
  float interactiveRenderRegionResolutionScale = 1.0f;
  QMatrix4x4 viewportViewMatrix;
  QMatrix4x4 viewportProjectionMatrix;
  bool viewportCameraCaptured = false;
  QSize compositionSize;
  bool compositionSizeCaptured = false;
  qint64 compositionFramePosition = 0;
  bool compositionFrameCaptured = false;
  // Identity for rejecting a queued frame after authored scene changes. This
  // does not make the referenced Composition immutable; render work must still
  // consume an owned scene snapshot before leaving the GUI thread.
  quint64 compositionRevision = 0;
  quint64 sceneMutationGeneration = 0;
  bool playbackRunning = false;
  QString playbackCompositionId;
  qint64 playbackFramePosition = 0;
  bool playbackFrameCaptured = false;
  bool ramPreviewStateCaptured = false;
  bool ramPreviewEnabled = false;
  bool ramPreviewPendingBuild = false;
  bool ramPreviewFallbackWhilePlaying = false;
  ArtifactRamPreviewFrameCacheState ramPreviewFrameState;
  ArtifactRamPreviewSummary ramPreviewSummary;
  ArtifactRamPreviewFrameUploadSnapshot ramPreviewUpload;
  quintptr windowId = 0;
  bool windowIdCaptured = false;
};

// Capture the playback/cache facts on the service's owning thread. The frame
// image itself remains cache-owned and is deliberately not copied into a
// worker packet; only the immutable prepared upload is retained.
bool captureCompositionPlaybackFrameSnapshot(
    CompositionHostFrameSnapshot& snapshot,
    const ArtifactCompositionPtr& composition,
    const ArtifactPlaybackService* playback) {
  if (!composition || !playback ||
      QThread::currentThread() != playback->thread()) {
    return false;
  }
  if (playback->currentComposition() != composition) {
    return false;
  }

  const qint64 frame = playback->currentFrame().framePosition();
  snapshot.playbackRunning = playback->isPlaying();
  snapshot.playbackCompositionId = composition->id().toString();
  snapshot.playbackFramePosition = frame;
  snapshot.playbackFrameCaptured = true;
  snapshot.ramPreviewStateCaptured = true;
  snapshot.ramPreviewEnabled = playback->isRamPreviewEnabled();
  snapshot.ramPreviewPendingBuild =
      playback->isRamPreviewFramePendingBuild(frame);
  snapshot.ramPreviewFallbackWhilePlaying =
      playback->ramPreviewPlaybackFallbackWhilePlaying();
  snapshot.ramPreviewFrameState = playback->ramPreviewFrameState(frame);
  snapshot.ramPreviewSummary = playback->ramPreviewSummary();
  snapshot.ramPreviewUpload = playback->pinRamPreviewFrameUpload(frame);
  return true;
}

// Freeze the camera transform while the host and renderer are still owned by
// the GUI thread. A worker can consume the matrices without consulting either
// live object or duplicating viewport-fit calculations.
bool captureCompositionViewportCamera(
    CompositionHostFrameSnapshot& snapshot) {
  if (!snapshot.viewportTransformCaptured || snapshot.logicalSize.width() <= 0 ||
      snapshot.logicalSize.height() <= 0 ||
      !std::isfinite(snapshot.devicePixelRatio) ||
      snapshot.devicePixelRatio <= 0.0 ||
      !std::isfinite(snapshot.viewportZoom) ||
      !std::isfinite(snapshot.viewportPanX) ||
      !std::isfinite(snapshot.viewportPanY)) {
    snapshot.viewportCameraCaptured = false;
    return false;
  }
  if (snapshot.viewportCameraCaptured) {
    return true;
  }

  const float viewportWidth = static_cast<float>(qMax(
      1, qRound(snapshot.logicalSize.width() * snapshot.devicePixelRatio)));
  const float viewportHeight = static_cast<float>(qMax(
      1, qRound(snapshot.logicalSize.height() * snapshot.devicePixelRatio)));
  const float zoom = qMax(0.001f, snapshot.viewportZoom);
  const QPointF target(
      (viewportWidth * 0.5f - snapshot.viewportPanX) / zoom,
      (viewportHeight * 0.5f - snapshot.viewportPanY) / zoom);
  constexpr float fovDegrees = 45.0f;
  constexpr float degreesToRadians =
      3.14159265358979323846f / 180.0f;
  const float distance = (viewportHeight * 0.5f) /
      (zoom * std::tan(fovDegrees * 0.5f * degreesToRadians));
  const QVector3D target3D(static_cast<float>(target.x()),
                           static_cast<float>(target.y()), 0.0f);

  snapshot.viewportViewMatrix.setToIdentity();
  snapshot.viewportViewMatrix.lookAt(
      target3D + QVector3D(0.0f, 0.0f, distance), target3D,
      QVector3D(0.0f, 1.0f, 0.0f));
  snapshot.viewportProjectionMatrix.setToIdentity();
  snapshot.viewportProjectionMatrix.perspective(
      fovDegrees, viewportWidth / viewportHeight, 1.0f, 100000.0f);
  snapshot.viewportProjectionMatrix(1, 1) =
      -snapshot.viewportProjectionMatrix(1, 1);
  snapshot.viewportCameraCaptured = true;
  return true;
}

// One owned scene payload paired with the GUI-captured host values. Keep only
// the newest pending packet; callers should recapture the JSON scene document
// when its mutation identity changes, not on every playback frame.
struct CompositionRenderFramePacket final {
  CompositionHostFrameSnapshot host;
  QString compositionId;
  QJsonDocument serializedComposition;
  ArtifactRamPreviewFrameUploadSnapshot ramPreviewUpload;

  bool isValid() const noexcept {
    return !compositionId.isEmpty() && !serializedComposition.isNull();
  }

  // Cached scene JSON can contain an older currentFrame value. Apply the
  // separately captured frame after worker-side rehydration before rendering.
  bool applyCapturedFramePosition(
      ArtifactAbstractComposition& composition) const {
    const bool playbackFrameMatchesComposition =
        host.playbackFrameCaptured &&
        host.playbackCompositionId == compositionId;
    if (!host.compositionFrameCaptured && !playbackFrameMatchesComposition) {
      return false;
    }
    const qint64 targetFrame = playbackFrameMatchesComposition
        ? host.playbackFramePosition
        : host.compositionFramePosition;
    if (composition.framePosition().framePosition() !=
        targetFrame) {
      composition.goToFrame(targetFrame);
    }
    return composition.framePosition().framePosition() ==
           targetFrame;
  }
};

// GUI-thread cache for immutable scene JSON. Playback position and host state
// are refreshed for every request, while full scene serialization is repeated
// only after an authored mutation identity or composition binding changes.
class CompositionRenderFramePacketCache final {
public:
  CompositionRenderFramePacket capture(
      const CompositionHostFrameSnapshot& host,
      const ArtifactCompositionPtr& composition) {
    CompositionRenderFramePacket packet;
    packet.host = host;
    (void)captureCompositionViewportCamera(packet.host);
    if (!composition) {
      reset();
      return packet;
    }

    packet.compositionId = composition->id().toString();
    packet.host.compositionRevision = composition->revision();
    packet.host.compositionSize = composition->effectiveCompositionSize();
    packet.host.compositionSizeCaptured = true;
    packet.host.compositionFramePosition =
        composition->framePosition().framePosition();
    packet.host.compositionFrameCaptured = true;

    const bool sceneChanged =
        !hasScene_ || compositionIdentity_ != composition.get() ||
        compositionId_ != packet.compositionId ||
        revision_ != packet.host.compositionRevision ||
        sceneMutationGeneration_ != packet.host.sceneMutationGeneration ||
        compositionSize_ != packet.host.compositionSize ||
        serializedComposition_.isNull();
    if (sceneChanged) {
      serializedComposition_ = composition->toJson();
      compositionIdentity_ = composition.get();
      compositionId_ = packet.compositionId;
      revision_ = packet.host.compositionRevision;
      sceneMutationGeneration_ = packet.host.sceneMutationGeneration;
      compositionSize_ = packet.host.compositionSize;
      hasScene_ = !serializedComposition_.isNull();
      ++sceneSerializationCount_;
    }
    packet.serializedComposition = serializedComposition_;
    packet.ramPreviewUpload = packet.host.ramPreviewUpload;
    // PlaybackService is QObject-backed and GUI-thread-owned. Its frame and
    // upload state must already be pinned in the host snapshot before packet
    // construction; never fill gaps by consulting live playback state here.
    return packet;
  }

  void reset() {
    compositionIdentity_ = nullptr;
    compositionId_.clear();
    revision_ = 0;
    sceneMutationGeneration_ = 0;
    compositionSize_ = QSize();
    serializedComposition_ = QJsonDocument();
    hasScene_ = false;
    sceneSerializationCount_ = 0;
  }

  quint64 sceneSerializationCount() const noexcept {
    return sceneSerializationCount_;
  }

private:
  const ArtifactAbstractComposition* compositionIdentity_ = nullptr;
  QString compositionId_;
  quint64 revision_ = 0;
  quint64 sceneMutationGeneration_ = 0;
  QSize compositionSize_;
  QJsonDocument serializedComposition_;
  bool hasScene_ = false;
  quint64 sceneSerializationCount_ = 0;
};

// A render worker must not accumulate obsolete frame requests while the GPU
// is busy. Keep exactly one pending owned packet and identify each request by
// a caller-provided monotonic sequence. The consumer can reject a completed
// frame if a newer request arrived while it was rendering.
class CompositionRenderFramePacketMailbox final {
public:
  bool publish(const CompositionRenderFramePacket& packet,
               quint64 requestSequence) {
    if (!packet.isValid() || requestSequence == 0) {
      return false;
    }
    QMutexLocker lock(&mutex_);
    if (closed_ || requestSequence <= latestRequestSequence_) {
      return false;
    }
    packet_ = packet;
    packetSequence_ = requestSequence;
    latestRequestSequence_ = requestSequence;
    pending_ = true;
    available_.wakeOne();
    return true;
  }

  bool tryTakeLatest(CompositionRenderFramePacket& packet,
                     quint64& requestSequence) {
    QMutexLocker lock(&mutex_);
    return takeLocked(packet, requestSequence);
  }

  // Intended for the persistent render worker, never the GUI thread.
  bool waitTakeLatest(CompositionRenderFramePacket& packet,
                      quint64& requestSequence) {
    QMutexLocker lock(&mutex_);
    while (!pending_ && !closed_) {
      available_.wait(&mutex_);
    }
    return takeLocked(packet, requestSequence);
  }

  bool isCurrent(quint64 requestSequence) const {
    QMutexLocker lock(&mutex_);
    return !closed_ && requestSequence != 0 &&
           requestSequence == latestRequestSequence_;
  }

  quint64 latestRequestSequence() const {
    QMutexLocker lock(&mutex_);
    return latestRequestSequence_;
  }

  void close() {
    QMutexLocker lock(&mutex_);
    closed_ = true;
    pending_ = false;
    packet_ = {};
    packetSequence_ = 0;
    available_.wakeAll();
  }

private:
  bool takeLocked(CompositionRenderFramePacket& packet,
                  quint64& requestSequence) {
    if (!pending_) {
      return false;
    }
    packet = packet_;
    requestSequence = packetSequence_;
    pending_ = false;
    return true;
  }

  mutable QMutex mutex_;
  QWaitCondition available_;
  CompositionRenderFramePacket packet_;
  quint64 packetSequence_ = 0;
  quint64 latestRequestSequence_ = 0;
  bool pending_ = false;
  bool closed_ = false;
};

// Call on the Composition's owning (normally GUI) thread and only when the
// scene mutation identity changes. This serializes the live QObject scene.
CompositionRenderFramePacket captureCompositionRenderFramePacket(
    const CompositionHostFrameSnapshot& host,
    const ArtifactCompositionPtr& composition) {
  CompositionRenderFramePacketCache cache;
  return cache.capture(host, composition);
}

} // namespace Artifact
