module;

#include <QString>

module Artifact.Widgets.CompositionFooter;

import Artifact.Service.Playback;

namespace Artifact {

struct CompositionFooterPlaybackSnapshot {
    int requestedFrames = 0;
    int playableFrames = 0;
    int cachedFrames = 0;
    float hitRate = 0.0f;
    QString text;
};

namespace {

QString ramPreviewStateNote(ArtifactPlaybackService* playback)
{
    if (!playback) {
        return QString();
    }
    const auto currentFrame = playback->currentFrame().framePosition();
    const auto state = playback->ramPreviewFrameState(currentFrame);
    const QString note = ramPreviewStatusNote(state);
    if (note == QStringLiteral("-")) {
        return QString();
    }
    return QStringLiteral(" | note %1").arg(note);
}

QString ramPreviewFooterText(ArtifactPlaybackService* playback,
                             const float hitRateFallback,
                             const int cachedFrameCountFallback)
{
    if (!playback) {
        return QStringLiteral("RAM: N/A");
    }

    const auto summary = playback->ramPreviewSummary();
    const auto currentFrame = playback->currentFrame().framePosition();
    const float hitRate = summary.hitRate > 0.0f ? summary.hitRate : hitRateFallback;
    const int inRamFrames = summary.inRamFrames > 0
        ? summary.inRamFrames : cachedFrameCountFallback;
    const QString prioritySuffix = summary.currentPriorityReason.trimmed().isEmpty()
        ? QString()
        : QStringLiteral(" | prio %1").arg(summary.currentPriorityReason);
    return QStringLiteral("RAM: playable %1/%2 | requested %3 | pending %4 | next %5 | range %6 | progress %7 | failed %8 | inRam %9 | onDisk %10 | readyMissingImage %11 | current %12%13 | hit%14")
        .arg(summary.playableFrames)
        .arg(summary.rangeFrames)
        .arg(summary.requestedFrames)
        .arg(summary.buildQueuePendingFrames)
        .arg(summary.buildQueueNextFrame)
        .arg(summary.buildRangeReady ? QStringLiteral("ready")
                                     : QStringLiteral("building"))
        .arg(QString::number(summary.buildRangeProgress * 100.0f, 'f', 0)
             + QStringLiteral("%"))
        .arg(summary.failedFrames)
        .arg(inRamFrames)
        .arg(summary.onDiskFrames)
        .arg(summary.readyMissingImageFrames)
        .arg(currentFrame)
        .arg(ramPreviewStateNote(playback) + prioritySuffix)
        .arg(QString::number(hitRate * 100.0f, 'f', 0) + QStringLiteral("%"));
}

} // namespace

void stopCompositionFooterPlayback()
{
    if (auto* playback = ArtifactPlaybackService::instance()) {
        playback->stop();
    }
}

CompositionFooterPlaybackSnapshot compositionFooterPlaybackSnapshot(
    const float fallbackHitRate, const int fallbackCachedFrameCount)
{
    CompositionFooterPlaybackSnapshot snapshot;
    auto* playback = ArtifactPlaybackService::instance();
    if (playback) {
        const auto summary = playback->ramPreviewSummary();
        snapshot.requestedFrames = summary.requestedFrames;
        snapshot.playableFrames = summary.playableFrames;
        snapshot.cachedFrames = summary.inRamFrames > 0
            ? summary.inRamFrames : fallbackCachedFrameCount;
        snapshot.hitRate = summary.hitRate > 0.0f
            ? summary.hitRate : fallbackHitRate;
    } else {
        snapshot.cachedFrames = fallbackCachedFrameCount;
        snapshot.hitRate = fallbackHitRate;
    }
    snapshot.text = ramPreviewFooterText(
        playback, snapshot.hitRate, snapshot.cachedFrames);
    return snapshot;
}

} // namespace Artifact
