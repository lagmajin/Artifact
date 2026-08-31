module;
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaObject>
#include <QImageReader>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <wobjectimpl.h>


#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <QDebug>
module Artifact.Service.Playback;

import Frame.Position;
import Frame.Rate;
import Frame.Range;
import Frame.Debug;
import Frame.SkipTracker;
import Core.Diagnostics.Trace;
import Diagnostics.Logger;
import Image.ImageF32x4RGBAWithCache;
import Artifact.Composition.PlaybackController;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Service.Project;
import Event.Bus;
import Artifact.Event.Types;
import Undo.UndoManager;

namespace Artifact {

using namespace ArtifactCore;

namespace {
void recordInOutPointsMutation(ArtifactInOutPoints* points,
                               const QJsonObject& before,
                               const QJsonObject& after) {
  if (!points || before == after) {
    return;
  }
  if (auto* undo = UndoManager::instance()) {
    if (!undo->push(std::make_unique<InOutPointsSnapshotCommand>(
            points, before, after))) {
      points->fromJson(before);
    }
  }
}
}

QString ramPreviewStatusNote(const ArtifactRamPreviewFrameCacheState &state) {
  if (state.failed) {
    return QStringLiteral("failed");
  }
  if (state.ready && !state.imageAvailable) {
    return QStringLiteral("ready-missing-image");
  }
  if (!state.requested) {
    return QStringLiteral("-");
  }
  if (state.requested && !state.ready && !state.reason.trimmed().isEmpty()) {
    return state.reason.trimmed();
  }
  return QStringLiteral("-");
}

QString ramPreviewNotReadyReason(const ArtifactRamPreviewFrameCacheState &state) {
  if (state.failed) {
    return QStringLiteral("failed");
  }
  if (!state.requested) {
    return QStringLiteral("not-requested");
  }
  if (state.onDisk && !state.inRam) {
    return QStringLiteral("on-disk-not-hydrated");
  }
  if (state.ready && !state.imageAvailable) {
    return QStringLiteral("ready-missing-image");
  }
  if (state.reason == QStringLiteral("playback-tick")) {
    return QStringLiteral("playback-tick-not-playable");
  }
  if (!state.reason.trimmed().isEmpty()) {
    return QStringLiteral("requested:%1").arg(state.reason.trimmed());
  }
  return QStringLiteral("requested-not-ready");
}

QString ramPreviewPriorityNote(const ArtifactRamPreviewPriorityState &state) {
  if (!state.band.trimmed().isEmpty()) {
    return state.band.trimmed();
  }
  return QStringLiteral("unknown");
}

QString ramPreviewPriorityReason(const ArtifactRamPreviewPriorityState &state) {
  if (!state.inCompositionRange) {
    return QStringLiteral("out-of-range");
  }
  if (state.currentFrame) {
    return QStringLiteral("immediate");
  }
  if (!state.inWorkArea) {
    return QStringLiteral("out-of-range");
  }
  if (state.nextQueued) {
    return QStringLiteral("directional");
  }
  if (state.pendingBuild && state.playing) {
    return QStringLiteral("directional");
  }
  if (state.distanceFromCurrent <= 3) {
    return QStringLiteral("near");
  }
  if (state.pendingBuild) {
    return QStringLiteral("safety-backfill");
  }
  if (state.inWorkArea) {
    return QStringLiteral("work-area");
  }
  if (!state.band.trimmed().isEmpty()) {
    return state.band.trimmed();
  }
  return QStringLiteral("unknown");
}

class ArtifactPlaybackService; // forward declaration to use pointer in Impl

W_OBJECT_IMPL(ArtifactPlaybackService)

class ArtifactPlaybackService::Impl {
public:
  ArtifactPlaybackService *owner_ = nullptr;
  ArtifactCompositionPlaybackController *controller_ = nullptr;
  ArtifactPlaybackEngine *engine_ = nullptr; // 新しいマルチスレッドエンジン
  ArtifactCompositionPtr currentComposition_;
  // std::unique_ptr<FrameCache> frameCache_;  // FrameCache module is disabled
  QElapsedTimer audioTimer_;
  double audioOffsetSeconds_ = 0.0;
  std::int64_t droppedFrameCount_ = 0;
  std::atomic_bool audioRunning_{false};
  std::function<double()> externalAudioClockProvider_;
  std::function<double()> playbackClockProvider_;
  float audioMasterVolume_ = 1.0f;
  bool audioMasterMuted_ = false;
  bool ramPreviewEnabled_ = true;
  bool ramPreviewPlaybackFallbackWhilePlaying_ = false;
  int ramPreviewRadiusFrames_ = 48;
  FrameRange ramPreviewRange_{FramePosition(0), FramePosition(0)};
  std::vector<bool> cacheBitmap_;
  std::vector<ArtifactRamPreviewFrameCacheState> frameCacheStates_;
  std::unordered_map<int64_t, ArtifactCore::ImageF32x4RGBAWithCache>
      ramPreviewImageCache_;
  size_t ramPreviewImageCacheBudgetFrames_ = 128;
  std::list<int64_t> ramPreviewImageLru_;
  std::unordered_map<int64_t, std::list<int64_t>::iterator>
      ramPreviewImageLruIndex_;
  struct PreviewDiskWriteTask {
    int64_t frame = -1;
    QString filePath;
    ArtifactCore::ImageF32x4_RGBA image;
    QString compositionId;
    QString renderContract;
    QString stateHash;
    uint64_t generation = 0;
  };
  std::mutex previewDiskWriteMutex_;
  std::condition_variable previewDiskWriteCv_;
  std::deque<PreviewDiskWriteTask> previewDiskWriteQueue_;
  std::thread previewDiskWriterThread_;
  bool previewDiskWriterStop_ = false;
  static constexpr qint64 kDefaultPreviewDiskCacheBudgetBytes =
      512LL * 1024LL * 1024LL;
  static constexpr int kPreviewDiskManifestSchema = 1;
  size_t previewDiskWritesSinceGlobalBudgetCheck_ = 0;
  // A disk cache invalidation must also invalidate queued writes.  Without
  // this generation, an old encode can recreate a cache file after its
  // composition directory was removed.
  std::atomic<uint64_t> previewDiskGeneration_{1};
  struct RamPreviewBuildQueue {
    uint64_t generation = 0;
    bool active = false;
    FrameRange range{FramePosition(0), FramePosition(0)};
    QString reason;
    std::deque<int64_t> pendingFrames;
  };
  RamPreviewBuildQueue ramPreviewBuildQueue_;
  // Set when the user requests playback before the selected preview range is
  // cached.  Playback starts only after the range becomes fully playable.
  bool ramPreviewAutoPlayPending_ = false;
  bool ramPreviewAutoPlaybackActive_ = false;
  PlaybackRangeMode playbackRangeMode_ = PlaybackRangeMode::All;
  std::atomic<int64_t> pendingCompositionFrame_{0};
  std::atomic_bool compositionFrameSyncQueued_{false};
  std::atomic_bool shuttingDown_{false};
  QString previewDiskCacheRoot_;
  std::atomic_bool previewDiskCacheEnabled_{true};
  std::atomic<qint64> previewDiskCacheBudgetBytes_{
      kDefaultPreviewDiskCacheBudgetBytes};
  // The namespace last used for the active composition.  Keep this separate
  // from the namespace derived from current state so invalidation after an
  // edit can remove the old cache directory as well.
  QString previewDiskActiveNamespace_;
  QString previewDiskCompositionStateHash_;
  // The final-frame cache is valid only for the render contract that produced
  // it.  Layer state is still invalidated by the composition edit paths;
  // this contract closes the quality/render-path gap at the service boundary.
  QString previewDiskRenderContract_{QStringLiteral("unbound")};
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  bool playbackSessionCaptureActive_ = false;
  QDateTime playbackSessionStartedAt_;
  int64_t playbackSessionStartFrame_ = 0;
  int64_t playbackSessionStartDroppedFrames_ = 0;
  std::atomic_uint64_t playbackSessionSyncRequests_{0};
  std::atomic_uint64_t playbackSessionSyncApplied_{0};
  std::atomic_uint64_t playbackSessionSyncCoalesced_{0};
  std::atomic_uint64_t playbackSessionPublishedFrames_{0};
  std::atomic_uint64_t playbackSessionConcreteFrames_{0};
  QElapsedTimer seekPreviewMaintenanceClock_;
  uint64_t seekPreviewMaintenanceGeneration_ = 0;

  void beginPlaybackSessionCapture() {
    if (playbackSessionCaptureActive_) {
      return;
    }
    ArtifactCore::Logger::instance()->install();
    playbackSessionCaptureActive_ = true;
    playbackSessionStartedAt_ = QDateTime::currentDateTime();
    playbackSessionStartFrame_ =
        engine_ ? engine_->currentFrame().framePosition() : 0;
    playbackSessionStartDroppedFrames_ = droppedFrameCount_;
    playbackSessionSyncRequests_.store(0, std::memory_order_relaxed);
    playbackSessionSyncApplied_.store(0, std::memory_order_relaxed);
    playbackSessionSyncCoalesced_.store(0, std::memory_order_relaxed);
    playbackSessionPublishedFrames_.store(0, std::memory_order_relaxed);
    playbackSessionConcreteFrames_.store(0, std::memory_order_relaxed);
    qInfo() << "[PlaybackSession] begin"
            << "startFrame=" << playbackSessionStartFrame_
            << "composition="
            << (currentComposition_ ? currentComposition_->id().toString()
                                    : QStringLiteral("null"));
  }

  void finishPlaybackSessionCapture(const QString &endReason) {
    if (!playbackSessionCaptureActive_) {
      return;
    }
    playbackSessionCaptureActive_ = false;

    const int64_t endFrame =
        engine_ ? engine_->currentFrame().framePosition()
                : playbackSessionStartFrame_;
    qInfo() << "[PlaybackSession] end"
            << "reason=" << endReason
            << "endFrame=" << endFrame;
    const QDateTime finishedAt = QDateTime::currentDateTime();
    const auto logs = ArtifactCore::Logger::instance()->getLogs();
    std::vector<ArtifactCore::LogMessage> sessionLogs;
    sessionLogs.reserve(logs.size());
    bool hasError = false;
    for (const auto &log : logs) {
      if (log.timestamp < playbackSessionStartedAt_ ||
          log.timestamp > finishedAt) {
        continue;
      }
      sessionLogs.push_back(log);
      hasError = hasError || log.level == ArtifactCore::LogLevel::Error ||
                 log.level == ArtifactCore::LogLevel::Fatal;
    }
    QString appData =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appData.isEmpty()) {
      appData = QDir::homePath();
    }
    QDir outputDir(
        QDir(appData).filePath(QStringLiteral("Logs/PlaybackSessions")));
    if (!outputDir.mkpath(QStringLiteral("."))) {
      qWarning() << "[PlaybackService] Failed to create playback session log directory:"
                 << outputDir.absolutePath();
      return;
    }

    const QFileInfoList oldLogs = outputDir.entryInfoList(
        {QStringLiteral("playback_session_*.log")}, QDir::Files, QDir::Time);
    const QDateTime retentionCutoff = finishedAt.addDays(-7);
    for (int index = 0; index < oldLogs.size(); ++index) {
      if (index >= 99 || oldLogs[index].lastModified() < retentionCutoff) {
        QFile::remove(oldLogs[index].absoluteFilePath());
      }
    }

    const QString fileName = QStringLiteral("playback_session_%1.log")
                                 .arg(playbackSessionStartedAt_.toString(
                                     QStringLiteral("yyyyMMdd_HHmmss_zzz")));
    QSaveFile file(outputDir.filePath(fileName));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      qWarning() << "[PlaybackService] Failed to open playback session log:"
                 << file.fileName();
      return;
    }

    // The raw session log remains below, but make the route actually selected
    // by preview immediately visible. PlaybackRender is deliberately emitted
    // on a route change and periodically, so these are decision samples rather
    // than a claim of an exact per-frame counter.
    const auto valueAfterToken = [](const QString &message,
                                    const QString &token) -> QString {
      const int tokenOffset = message.indexOf(token);
      if (tokenOffset < 0) {
        return {};
      }
      const int valueStart = tokenOffset + token.size();
      const int valueEnd = message.indexOf(QLatin1Char(' '), valueStart);
      return message.mid(valueStart,
                         valueEnd < 0 ? -1 : valueEnd - valueStart).trimmed();
    };
    std::map<QString, int> playbackFallbackReasons;
    std::map<QString, int> presentationPaths;
    std::map<QString, int> videoBranches;
    int playbackRenderSamples = 0;
    int ramPreviewSamples = 0;
    int liveRenderSamples = 0;
    QString lastPlaybackRender;
    QString lastVideoPresentation;
    QString lastVideoDecode;
    for (const auto &log : sessionLogs) {
      const QString &message = log.message;
      if (message.contains(QStringLiteral("[PlaybackRender]"))) {
        ++playbackRenderSamples;
        lastPlaybackRender = message;
        const QString fallback = valueAfterToken(
            message, QStringLiteral("ramPreviewFallback="));
        if (fallback == QStringLiteral("true")) {
          ++ramPreviewSamples;
        } else if (fallback == QStringLiteral("false")) {
          ++liveRenderSamples;
        }
        const QString reason = valueAfterToken(
            message, QStringLiteral("fallbackReason="));
        if (!reason.isEmpty()) {
          ++playbackFallbackReasons[reason];
        }
      }
      if (message.contains(QStringLiteral("[Video] phase=present"))) {
        lastVideoPresentation = message;
        const QString path = valueAfterToken(
            message, QStringLiteral("compositionCache="));
        if (!path.isEmpty()) {
          ++presentationPaths[path];
        }
        lastVideoDecode = message;
      }
      if (message.contains(QStringLiteral("[Video] branch="))) {
        const QString branch = valueAfterToken(
            message, QStringLiteral("branch="));
        if (!branch.isEmpty()) {
          ++videoBranches[branch];
        }
        lastVideoDecode = message;
      }
    }
    const auto formatBreakdown = [](const std::map<QString, int> &values) {
      if (values.empty()) {
        return QStringLiteral("none");
      }
      QStringList parts;
      for (const auto &[name, count] : values) {
        parts.push_back(QStringLiteral("%1=%2").arg(name).arg(count));
      }
      return parts.join(QStringLiteral(", "));
    };

    QString output;
    output.reserve(static_cast<qsizetype>(sessionLogs.size() * 160 + 512));
    output += QStringLiteral("Artifact playback diagnostic session\n");
    output += QStringLiteral("Started: %1\n")
                  .arg(playbackSessionStartedAt_.toString(Qt::ISODateWithMs));
    output += QStringLiteral("Finished: %1\n")
                  .arg(finishedAt.toString(Qt::ISODateWithMs));
    output += QStringLiteral("Duration: %1 ms\n")
                  .arg(playbackSessionStartedAt_.msecsTo(finishedAt));
    output += QStringLiteral("End reason: %1\n").arg(endReason);
    output += QStringLiteral("Start frame: %1  End frame: %2  Advanced: %3\n")
                  .arg(playbackSessionStartFrame_)
                  .arg(endFrame)
                  .arg(endFrame - playbackSessionStartFrame_);
    if (currentComposition_) {
      output += QStringLiteral("Composition: %1 (%2)\n")
                    .arg(currentComposition_->settings().compositionName().toQString(),
                         currentComposition_->id().toString());
      const FrameRange range = currentComposition_->frameRange();
      output += QStringLiteral("Composition frame range: %1 - %2\n")
                    .arg(range.startPosition().framePosition())
                    .arg(range.endPosition().framePosition());
      output += QStringLiteral("Composition current frame: %1\n")
                    .arg(currentComposition_->framePosition().framePosition());
    } else {
      output += QStringLiteral("Composition: null\n");
    }
    const int engineState =
        !engine_ ? -1
                 : static_cast<int>(engine_->isPlaying()
                                        ? PlaybackState::Playing
                                        : (engine_->isPaused()
                                               ? PlaybackState::Paused
                                               : PlaybackState::Stopped));
    output += QStringLiteral("Engine state: %1\n").arg(engineState);
    output += QStringLiteral("Dropped frames this session: %1  total: %2\n")
                  .arg(droppedFrameCount_ - playbackSessionStartDroppedFrames_)
                  .arg(droppedFrameCount_);
    output += QStringLiteral("Frame sync: requested=%1 applied=%2 coalesced=%3\n")
                  .arg(playbackSessionSyncRequests_.load())
                  .arg(playbackSessionSyncApplied_.load())
                  .arg(playbackSessionSyncCoalesced_.load());
    output += QStringLiteral("Published ticks: %1  concrete images: %2\n")
                  .arg(playbackSessionPublishedFrames_.load())
                  .arg(playbackSessionConcreteFrames_.load());
    output += QStringLiteral("RAM preview: enabled=%1 requested=%2 ready=%3 "
                             "failed=%4 inRam=%5 onDisk=%6 hitRate=%7%%\n")
                  .arg(ramPreviewEnabled_)
                  .arg(ramPreviewRequestedFrameCount())
                  .arg(ramPreviewReadyFrameCountInRange())
                  .arg(ramPreviewFailedFrameCountInRange())
                  .arg(ramPreviewCachedFrameCount())
                  .arg(ramPreviewDiskFrameCountInRange())
                  .arg(ramPreviewHitRate() * 100.0f, 0, 'f', 2);
    output += QStringLiteral("\n=== Preview route summary ===\n");
    output += QStringLiteral("Route decision samples: total=%1 ram-cache=%2 live-render=%3\n")
                  .arg(playbackRenderSamples)
                  .arg(ramPreviewSamples)
                  .arg(liveRenderSamples);
    output += QStringLiteral("Fallback reasons: %1\n")
                  .arg(formatBreakdown(playbackFallbackReasons));
    output += QStringLiteral("Video presentation paths: %1\n")
                  .arg(formatBreakdown(presentationPaths));
    output += QStringLiteral("Video branches: %1\n")
                  .arg(formatBreakdown(videoBranches));
    if (!lastPlaybackRender.isEmpty()) {
      output += QStringLiteral("Last route decision: %1\n").arg(lastPlaybackRender);
    }
    if (!lastVideoPresentation.isEmpty()) {
      output += QStringLiteral("Last video presentation: %1\n")
                    .arg(lastVideoPresentation);
    }
    if (!lastVideoDecode.isEmpty()) {
      output += QStringLiteral("Last video decode state: %1\n")
                    .arg(lastVideoDecode);
    }
    output += QStringLiteral(
        "Route decisions are sampled when the route changes and every 120 frames.\n");
    output += QStringLiteral("Captured errors: %1\n\n=== Session log ===\n")
                  .arg(hasError);

    QString diagnosis;
    const auto published = playbackSessionPublishedFrames_.load();
    const auto syncApplied = playbackSessionSyncApplied_.load();
    const qint64 durationMs = playbackSessionStartedAt_.msecsTo(finishedAt);
    if (endReason == QStringLiteral("rejected-no-composition")) {
      diagnosis = QStringLiteral(
          "Playback was rejected because no current composition could be bound.");
    } else if (hasError) {
      diagnosis = QStringLiteral(
          "One or more ERROR/FATAL records occurred; inspect the session log below.");
    } else if (published == 0 && durationMs >= 100) {
      diagnosis = QStringLiteral(
          "The playback engine published no frame ticks. Inspect [PlaybackEngine][Loop] "
          "and [PlaybackEngine][Stall] records.");
    } else if (published > 0 && syncApplied == 0) {
      diagnosis = QStringLiteral(
          "Frame ticks were published, but none reached composition goToFrame(). "
          "The composition-thread dispatch path is stalled or shutting down.");
    } else if (ramPreviewFailedFrameCountInRange() > 0) {
      diagnosis = QStringLiteral(
          "RAM preview contains failed frames; inspect [PlaybackRender] fallbackReason "
          "and previewState.reason.");
    } else if (endFrame == playbackSessionStartFrame_ && durationMs < 100) {
      diagnosis = QStringLiteral(
          "Playback ended before one frame interval could reliably elapse.");
    } else if (endFrame == playbackSessionStartFrame_) {
      diagnosis = QStringLiteral(
          "Playback ran but the visible frame did not advance. Compare engine ticks, "
          "frame-sync counts, and [PlaybackRender] records below.");
    } else {
      diagnosis = QStringLiteral(
          "Playback advanced. Inspect dropped-frame and render-fallback records for "
          "visual stutter or stale presentation.");
    }
    output.insert(output.indexOf(QStringLiteral("=== Session log ===")),
                  QStringLiteral("=== Automatic diagnosis ===\n%1\n\n")
                      .arg(diagnosis));
    for (const auto &log : sessionLogs) {
      QString level;
      switch (log.level) {
      case ArtifactCore::LogLevel::Debug: level = QStringLiteral("DEBUG"); break;
      case ArtifactCore::LogLevel::Info: level = QStringLiteral("INFO"); break;
      case ArtifactCore::LogLevel::Warning: level = QStringLiteral("WARNING"); break;
      case ArtifactCore::LogLevel::Error: level = QStringLiteral("ERROR"); break;
      case ArtifactCore::LogLevel::Fatal: level = QStringLiteral("FATAL"); break;
      }
      output += QStringLiteral("[%1][%2] %3")
                    .arg(log.timestamp.toString(
                             QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                         level, log.message);
      if (!log.context.isEmpty()) {
        output += QStringLiteral(" (%1)").arg(log.context);
      }
      output += QLatin1Char('\n');
    }

    if (file.write(output.toUtf8()) < 0 || !file.commit()) {
      qWarning() << "[PlaybackService] Failed to save playback session log:"
                 << file.fileName();
      return;
    }
    qInfo() << "[PlaybackService] Playback session log saved:" << file.fileName();
  }

  void applyCurrentPlaybackFrameRangeToEngine() {
    if (!engine_ || !currentComposition_) {
      return;
    }
    FrameRange range = currentComposition_->frameRange();
    if (playbackRangeMode_ == PlaybackRangeMode::WorkArea) {
      range = currentComposition_->workAreaRange();
    } else if (playbackRangeMode_ == PlaybackRangeMode::Selection) {
      // Selection range is intentionally left as a future extension.
    }
    engine_->setFrameRange(range);
  }

  bool ensureCurrentCompositionBound() {
    if (shuttingDown_.load(std::memory_order_acquire) ||
        !currentComposition_) {
      if (auto *projectService = ArtifactProjectService::instance()) {
        if (auto fallbackComposition = projectService->currentComposition().lock()) {
          owner_->setCurrentComposition(fallbackComposition);
        }
      }
    }

    if (!currentComposition_) {
      return false;
    }

    if (engine_ && engine_->composition() != currentComposition_) {
      applyCurrentPlaybackFrameRangeToEngine();
      engine_->setFrameRate(currentComposition_->frameRate());
      engine_->setCurrentFrame(currentComposition_->framePosition());
      engine_->setComposition(currentComposition_);
    }

    if (controller_) {
      controller_->setFrameRange(currentComposition_->frameRange());
      controller_->setFrameRate(currentComposition_->frameRate());
      controller_->setCurrentFrame(currentComposition_->framePosition());
    }

    return true;
  }

  explicit Impl(ArtifactPlaybackService *owner) : owner_(owner) {
    controller_ = new ArtifactCompositionPlaybackController();
    engine_ = new ArtifactPlaybackEngine();
    previewDiskWriterThread_ = std::thread([this]() {
      while (true) {
        PreviewDiskWriteTask task;
        {
          std::unique_lock<std::mutex> lock(previewDiskWriteMutex_);
          previewDiskWriteCv_.wait(lock, [this]() {
            return previewDiskWriterStop_ || !previewDiskWriteQueue_.empty();
          });
          if (previewDiskWriterStop_ && previewDiskWriteQueue_.empty()) {
            break;
          }
          task = std::move(previewDiskWriteQueue_.front());
          previewDiskWriteQueue_.pop_front();
        }

        bool savedToDisk = false;
        std::vector<int64_t> evictedFrames;
        {
          // Serialize persistence with directory removal so a stale task
          // cannot write back into a cache that has just been invalidated.
          std::lock_guard<std::mutex> lock(previewDiskWriteMutex_);
          if (task.generation == previewDiskGeneration_.load()) {
            savedToDisk = persistPreviewFrameToDisk(task.filePath, task.image);
            if (savedToDisk) {
              evictedFrames = enforcePreviewDiskCacheBudget(task.filePath);
              ++previewDiskWritesSinceGlobalBudgetCheck_;
              if (previewDiskWritesSinceGlobalBudgetCheck_ >= 8) {
                previewDiskWritesSinceGlobalBudgetCheck_ = 0;
                const auto globalEvictions =
                    enforcePreviewDiskCacheGlobalBudget(task.filePath);
                evictedFrames.insert(evictedFrames.end(), globalEvictions.begin(),
                                     globalEvictions.end());
              }
              writePreviewDiskManifest(
                  QFileInfo(task.filePath).absolutePath(),
                  QFileInfo(task.filePath).absoluteDir().dirName(),
                  task.compositionId, task.renderContract, task.stateHash);
            }
          }
        }
        QMetaObject::invokeMethod(
            owner_, [this, frame = task.frame,
                     compositionId = task.compositionId,
                     generation = task.generation, savedToDisk,
                     evictedFrames = std::move(evictedFrames)]() {
              if (generation != previewDiskGeneration_.load()) {
                return;
              }
              const QString currentCompositionId =
                  currentComposition_ ? currentComposition_->id().toString()
                                      : QString();
              if (compositionId.isEmpty() ||
                  currentCompositionId != compositionId) {
                return;
              }
              markFrameOnDisk(frame, savedToDisk);
              for (const int64_t evictedFrame : evictedFrames) {
                markFrameOnDisk(evictedFrame, false);
              }
            },
            Qt::QueuedConnection);
      }
    });

    // エンジンのシグナルをサービスに転送
    QObject::connect(
        engine_, &ArtifactPlaybackEngine::playbackStateChanged, owner_,
        [this](PlaybackState state) {
          const auto publishState = [this, state]() {
            if (state == PlaybackState::Playing) {
              beginPlaybackSessionCapture();
            } else if (state == PlaybackState::Paused) {
              finishPlaybackSessionCapture(QStringLiteral("paused"));
            } else if (state == PlaybackState::Stopped) {
              finishPlaybackSessionCapture(QStringLiteral("stopped"));
            }
            if (state == PlaybackState::Paused) {
              pauseAudioClock();
            } else if (state == PlaybackState::Stopped) {
              stopAudioClock();
            }


            ArtifactCore::globalEventBus().publish<PlaybackStateChangedEvent>(
                PlaybackStateChangedEvent{state});
          };
          QMetaObject::invokeMethod(owner_, publishState, Qt::QueuedConnection);
        },
        Qt::DirectConnection);

    QObject::connect(
        engine_, &ArtifactPlaybackEngine::frameChanged, owner_,
        [this](const FramePosition &position, const QImage &frame) {
          if (shuttingDown_.load(std::memory_order_acquire)) {
            return;
          }

          const QString compositionId =
              currentComposition_ ? currentComposition_->id().toString()
                                  : QString();

          const int64_t frameNumber = position.framePosition();
          ArtifactCore::ImageF32x4_RGBA frameBuffer;
          const bool hasConcreteFrame = !frame.isNull();
          if (hasConcreteFrame) {
            const QImage rgba = frame.format() == QImage::Format_RGBA8888
                                    ? frame
                                    : frame.convertToFormat(QImage::Format_RGBA8888);
            frameBuffer.setFromRGBA8(rgba.constBits(), rgba.width(), rgba.height());
          }
          const QString diskCacheFramePath =
              currentComposition_
                  ? previewDiskCacheFramePathForNamespace(
                        currentCompositionDiskCacheNamespace(), frameNumber)
                  : QString();

          const auto publishFrame = [this, position, compositionId, frameNumber,
                                     frameBuffer, hasConcreteFrame,
                                     diskCacheFramePath]() {
            // The engine emits from its worker thread. Keep all composition
            // mutation on the PlaybackService/GUI thread together with the
            // cache publication instead of calling into the composition from
            // the DirectConnection callback.
            syncCurrentCompositionFrame(position);
            playbackSessionPublishedFrames_.fetch_add(
                1, std::memory_order_relaxed);
            if (hasConcreteFrame) {
              playbackSessionConcreteFrames_.fetch_add(
                  1, std::memory_order_relaxed);
            }
            const QString currentCompositionId =
                currentComposition_ ? currentComposition_->id().toString()
                                    : QString();
            if (compositionId.isEmpty() ||
                currentCompositionId != compositionId) {
              qWarning() << "[PlaybackService][PublishFrame] dropped"
                         << "frame=" << frameNumber
                         << "reason=composition-mismatch"
                         << "capturedComposition=" << compositionId
                         << "currentComposition=" << currentCompositionId;
              return;
            }
            if (frameNumber == playbackSessionStartFrame_ ||
                playbackSessionPublishedFrames_.load(
                    std::memory_order_relaxed) % 120 == 0) {
              qDebug() << "[PlaybackService][PublishFrame]"
                       << "frame=" << frameNumber
                       << "hasImage=" << hasConcreteFrame
                       << "composition=" << compositionId;
            }
            if (hasConcreteFrame) {
              FrameSkipTracker::instance()->commitFrame(frameNumber);
              storeFrameImageInRam(frameNumber, frameBuffer,
                                   QStringLiteral("playback-frame"));
              if (previewDiskCacheEnabled_.load()) {
                {
                  std::lock_guard<std::mutex> lock(previewDiskWriteMutex_);
                  previewDiskWriteQueue_.push_back(
                      PreviewDiskWriteTask{frameNumber, diskCacheFramePath,
                                           frameBuffer, compositionId,
                                           previewDiskRenderContract_,
                                           currentCompositionStateHash(),
                                           previewDiskGeneration_.load()});
                }
                previewDiskWriteCv_.notify_one();
              }
              markFrameOnDisk(frameNumber, false);
            } else {
              markFrameRequested(frameNumber, QStringLiteral("playback-tick"));
              markFrameOnDisk(frameNumber, hasPreviewFrameOnDisk(frameNumber));
              if (!hasFrameImageInRam(frameNumber)) {
                clearFrameFailure(frameNumber);
              }
            }
            ArtifactCore::globalEventBus().publish<FrameChangedEvent>(
                FrameChangedEvent{QString(compositionId),
                                  position.framePosition()});
            emitRamPreviewStats();
          };
          // The every-frame engine policy dispatches this signal through a
          // blocking GUI invocation. Publish inline in that case so the
          // worker receives real composition-sync backpressure instead of
          // enqueueing a second, coalescible GUI event.
          if (engine_ && engine_->isPlaying() &&
              QThread::currentThread() == owner_->thread()) {
            publishFrame();
          } else {
            QMetaObject::invokeMethod(owner_, publishFrame, Qt::QueuedConnection);
          }
        },
        Qt::DirectConnection);

    QObject::connect(engine_, &ArtifactPlaybackEngine::droppedFrameDetected,
                     owner_, [this](int64_t count) {
                       droppedFrameCount_ += count;
                       qDebug() << "[PlaybackService] Dropped frames:" << count
                                << "total=" << droppedFrameCount_;
                     });

    QObject::connect(
        engine_, &ArtifactPlaybackEngine::audioLevelChanged, owner_,
        [this](float leftRms, float rightRms, float leftPeak, float rightPeak) {
          ArtifactCore::globalEventBus().publish<AudioLevelChangedEvent>(
              AudioLevelChangedEvent{leftRms, rightRms, leftPeak, rightPeak});
        },
        Qt::QueuedConnection);

    // オーディオクロックプロバイダーを設定
    controller_->setAudioClockProvider([this]() -> double {
      if (externalAudioClockProvider_) {
        return externalAudioClockProvider_();
      }

      double seconds = audioOffsetSeconds_;
      if (audioRunning_) {
        seconds += static_cast<double>(audioTimer_.elapsed()) / 1000.0;
      }
      return seconds;
    });

    // エンジンにも設定
    engine_->setAudioClockProvider([this]() -> double {
      if (externalAudioClockProvider_) {
        return externalAudioClockProvider_();
      }
      double seconds = audioOffsetSeconds_;
      if (audioRunning_) {
        seconds += static_cast<double>(audioTimer_.elapsed()) / 1000.0;
      }
      return seconds;
    });

    engine_->setAudioMasterVolume(audioMasterVolume_);
    engine_->setAudioMasterMuted(audioMasterMuted_);

    eventBusSubscriptions_.push_back(
        eventBus_.subscribe<LayerChangedEvent>([this](const LayerChangedEvent &event) {
          const QString compositionId =
              currentComposition_ ? currentComposition_->id().toString()
                                  : QString();
          if (compositionId.isEmpty() || event.compositionId != compositionId) {
            return;
          }

          QMetaObject::invokeMethod(owner_, [this]() {
            invalidateRamPreviewForCurrentComposition(
                QStringLiteral("layer-changed"));
          }, Qt::QueuedConnection);
        }));
  }

  ~Impl() {
    shuttingDown_.store(true, std::memory_order_release);
    compositionFrameSyncQueued_.store(false, std::memory_order_release);
    if (engine_) {
      engine_->stop();
      engine_->waitForStop();
    }
    finishPlaybackSessionCapture(QStringLiteral("service-destroyed"));
    {
      std::lock_guard<std::mutex> lock(previewDiskWriteMutex_);
      previewDiskWriterStop_ = true;
    }
    previewDiskWriteCv_.notify_all();
    if (previewDiskWriterThread_.joinable()) {
      previewDiskWriterThread_.join();
    }
    delete engine_;
    delete controller_;
  }

  void startAudioClock() {
    if (!audioRunning_) {
      qDebug() << "[PlaybackService][AudioClock] start"
               << "offsetSeconds=" << audioOffsetSeconds_;
      audioTimer_.start();
      audioRunning_ = true;
    }
  }
  void pauseAudioClock() {
    if (audioRunning_) {
      audioOffsetSeconds_ +=
          static_cast<double>(audioTimer_.elapsed()) / 1000.0;
      qDebug() << "[PlaybackService][AudioClock] pause"
               << "offsetSeconds=" << audioOffsetSeconds_;
      audioRunning_ = false;
    }
  }
  void stopAudioClock() {
    qDebug() << "[PlaybackService][AudioClock] stop"
             << "previousOffsetSeconds=" << audioOffsetSeconds_;
    audioOffsetSeconds_ = 0.0;
    audioRunning_ = false;
  }
  void setExternalAudioClockProvider(const std::function<double()> &provider) {
    externalAudioClockProvider_ = provider;
  }
  void setPlaybackClockProvider(const std::function<double()> &provider) {
    setExternalAudioClockProvider(provider);
  }

  void syncCurrentCompositionFrame(const FramePosition &position) {
    playbackSessionSyncRequests_.fetch_add(1, std::memory_order_relaxed);
    if (!currentComposition_) {
      qWarning() << "[PlaybackService][SyncFrame] rejected"
                 << "frame=" << position.framePosition()
                 << "reason=no-composition";
      return;
    }

    const auto composition = currentComposition_;
    const bool sameThread = composition->thread() == QThread::currentThread();
    pendingCompositionFrame_.store(position.framePosition(),
                                   std::memory_order_relaxed);

    if (sameThread) {
      composition->goToFrame(position.framePosition());
      playbackSessionSyncApplied_.fetch_add(1, std::memory_order_relaxed);
      return;
    }

    if (compositionFrameSyncQueued_.exchange(true, std::memory_order_acq_rel)) {
      playbackSessionSyncCoalesced_.fetch_add(1, std::memory_order_relaxed);
      return;
    }

    QMetaObject::invokeMethod(
        composition.get(),
        [this, composition]() {
          if (shuttingDown_.load(std::memory_order_acquire)) {
            compositionFrameSyncQueued_.store(false,
                                              std::memory_order_release);
            return;
          }
          const int64_t latestFrame =
              pendingCompositionFrame_.load(std::memory_order_relaxed);
          compositionFrameSyncQueued_.store(false, std::memory_order_release);
          if (composition) {
            composition->goToFrame(latestFrame);
            const auto applied = playbackSessionSyncApplied_.fetch_add(
                                     1, std::memory_order_relaxed) +
                                 1;
            if (latestFrame == playbackSessionStartFrame_ || applied % 120 == 0) {
              qDebug() << "[PlaybackService][SyncFrame]"
                       << "frame=" << latestFrame
                       << "sameThread=false"
                       << "composition=" << composition->id().toString()
                       << "coalescedTotal="
                       << playbackSessionSyncCoalesced_.load(
                              std::memory_order_relaxed);
            }
          }
        },
        Qt::QueuedConnection);
  }

  void emitRamPreviewStats() {
    ArtifactCore::globalEventBus().publish<PlaybackRamPreviewStatsChangedEvent>(
        PlaybackRamPreviewStatsChangedEvent{ramPreviewHitRate(),
                                             ramPreviewCachedFrameCount()});
  }

  void publishRamPreviewStateChanged(const bool enabled,
                                     const FrameRange &range) {
    ArtifactCore::globalEventBus().publish<PlaybackRamPreviewStateChangedEvent>(
        PlaybackRamPreviewStateChangedEvent{enabled, range.start(),
                                             range.end()});
  }

  void touchRamPreviewImageLru(const int64_t frame) {
    const auto it = ramPreviewImageLruIndex_.find(frame);
    if (it != ramPreviewImageLruIndex_.end()) {
      ramPreviewImageLru_.erase(it->second);
      ramPreviewImageLruIndex_.erase(it);
    }
    ramPreviewImageLru_.push_front(frame);
    ramPreviewImageLruIndex_[frame] = ramPreviewImageLru_.begin();
  }

  void eraseRamPreviewImageLru(const int64_t frame) {
    const auto it = ramPreviewImageLruIndex_.find(frame);
    if (it == ramPreviewImageLruIndex_.end()) {
      return;
    }
    ramPreviewImageLru_.erase(it->second);
    ramPreviewImageLruIndex_.erase(it);
  }

  void evictRamPreviewImagesIfNeeded() {
    while (ramPreviewImageCache_.size() > ramPreviewImageCacheBudgetFrames_ &&
           !ramPreviewImageLru_.empty()) {
      const int64_t frame = ramPreviewImageLru_.back();
      ramPreviewImageLru_.pop_back();
      ramPreviewImageLruIndex_.erase(frame);
      ramPreviewImageCache_.erase(frame);
      if (isValidFrameIndex(frame)) {
        auto &state = frameCacheStates_[static_cast<size_t>(frame)];
        state.inRam = false;
        state.ready = false;
        state.reason = QStringLiteral("evicted-lru");
        cacheBitmap_[static_cast<size_t>(frame)] = false;
      }
    }
  }

  void completeRamPreviewBuildFrame(const int64_t frame) {
    if (!ramPreviewBuildQueue_.active ||
        ramPreviewBuildQueue_.pendingFrames.empty()) {
      return;
    }

    const auto it = std::find(ramPreviewBuildQueue_.pendingFrames.begin(),
                              ramPreviewBuildQueue_.pendingFrames.end(), frame);
    if (it != ramPreviewBuildQueue_.pendingFrames.end()) {
      ramPreviewBuildQueue_.pendingFrames.erase(it);
    }
    if (ramPreviewBuildQueue_.pendingFrames.empty()) {
      ramPreviewBuildQueue_.active = false;
      if (ramPreviewAutoPlayPending_ && ramPreviewPlaybackStartReady()) {
        ramPreviewAutoPlayPending_ = false;
        QMetaObject::invokeMethod(owner_, [this]() {
          if (!shuttingDown_.load(std::memory_order_acquire)) {
            owner_->play();
          }
        }, Qt::QueuedConnection);
      }
    }
  }

  bool isFrameReadyForRamPreview(const int64_t frame) const {
    if (!isValidFrameIndex(frame)) {
      return false;
    }

    const auto &state = frameCacheStates_[static_cast<size_t>(frame)];
    return state.ready && state.inRam && !state.failed &&
           hasFrameImageInRam(frame);
  }

  bool frameNeedsRamPreviewBuild(const int64_t frame) const {
    if (!isValidFrameIndex(frame)) {
      return false;
    }

    const auto &state = frameCacheStates_[static_cast<size_t>(frame)];
    return !state.failed && !isFrameReadyForRamPreview(frame);
  }

  bool isRamPreviewFramePendingBuild(const int64_t frame) const {
    if (ramPreviewBuildQueue_.pendingFrames.empty()) {
      return false;
    }

    return std::find(ramPreviewBuildQueue_.pendingFrames.begin(),
                     ramPreviewBuildQueue_.pendingFrames.end(),
                     frame) != ramPreviewBuildQueue_.pendingFrames.end();
  }

  int64_t nextRamPreviewBuildFrame() const {
    return ramPreviewBuildQueue_.pendingFrames.empty()
               ? int64_t{-1}
               : ramPreviewBuildQueue_.pendingFrames.front();
  }

  std::vector<int64_t> orderedRamPreviewFramesForRange(const FrameRange &range) const {
    std::vector<int64_t> orderedFrames;
    if (!currentComposition_) {
      return orderedFrames;
    }

    const int64_t start = std::max<int64_t>(0, range.start());
    const int64_t endExclusive = std::max<int64_t>(
        start, std::min<int64_t>(static_cast<int64_t>(frameCacheStates_.size()),
                                 range.end()));
    if (endExclusive <= start) {
      return orderedFrames;
    }

    const int64_t currentFrame =
        engine_ ? engine_->currentFrame().framePosition()
                : (controller_ ? controller_->currentFrame().framePosition() : 0);
    const bool playing = owner_ && owner_->state() == PlaybackState::Playing;
    const bool reverse = owner_ && owner_->playbackSpeed() < 0.0f;

    std::vector<int64_t> forwardBand;
    std::vector<int64_t> backwardBand;
    orderedFrames.reserve(static_cast<size_t>(endExclusive - start));
    forwardBand.reserve(static_cast<size_t>(endExclusive - start));
    backwardBand.reserve(static_cast<size_t>(endExclusive - start));

    for (int64_t frame = start; frame < endExclusive; ++frame) {
      if (frame == currentFrame) {
        orderedFrames.push_back(frame);
        continue;
      }
      const bool onDirectionalSide = reverse ? frame < currentFrame : frame > currentFrame;
      if (playing && onDirectionalSide) {
        forwardBand.push_back(frame);
      } else {
        backwardBand.push_back(frame);
      }
    }

    auto appendByDistance = [&](std::vector<int64_t> &frames) {
      std::stable_sort(frames.begin(), frames.end(), [&](int64_t a, int64_t b) {
        const int64_t da = std::llabs(a - currentFrame);
        const int64_t db = std::llabs(b - currentFrame);
        if (da != db) {
          return da < db;
        }
        return reverse ? a > b : a < b;
      });
      orderedFrames.insert(orderedFrames.end(), frames.begin(), frames.end());
    };

    appendByDistance(forwardBand);
    appendByDistance(backwardBand);
    return orderedFrames;
  }

  void cancelRamPreviewBuild(const QString &reason = {}) {
    ++ramPreviewBuildQueue_.generation;
    ramPreviewBuildQueue_.active = false;
    ramPreviewBuildQueue_.pendingFrames.clear();
    ramPreviewAutoPlayPending_ = false;
    if (!reason.trimmed().isEmpty()) {
      ramPreviewBuildQueue_.reason = reason.trimmed();
    } else {
      ramPreviewBuildQueue_.reason.clear();
    }
  }

  void requestRamPreviewBuild(const FrameRange &range,
                              const QString &reason = {}) {
    if (!currentComposition_) {
      cancelRamPreviewBuild(reason);
      return;
    }

    const QString normalizedReason =
        reason.trimmed().isEmpty() ? QStringLiteral("ram-preview-build")
                                   : reason.trimmed();
    const bool sameRange =
        ramPreviewBuildQueue_.active &&
        ramPreviewBuildQueue_.range.start() == range.start() &&
        ramPreviewBuildQueue_.range.end() == range.end() &&
        ramPreviewBuildQueue_.reason == normalizedReason;
    if (sameRange) {
      return;
    }

    ++ramPreviewBuildQueue_.generation;
    ramPreviewBuildQueue_.active = true;
    ramPreviewBuildQueue_.range = range;
    ramPreviewBuildQueue_.pendingFrames.clear();
    ramPreviewBuildQueue_.reason = normalizedReason;

    clearFrameRequestFlagsOutsideRange(range);

    const int64_t start = std::max<int64_t>(0, range.start());
    const int64_t endExclusive = std::max<int64_t>(
        start, std::min<int64_t>(static_cast<int64_t>(frameCacheStates_.size()),
                                 range.end()));
    const auto orderedFrames = orderedRamPreviewFramesForRange(range);
    for (const int64_t frame : orderedFrames) {
      markFrameRequested(frame, normalizedReason);
      if (frameNeedsRamPreviewBuild(frame)) {
        ramPreviewBuildQueue_.pendingFrames.push_back(frame);
      }
    }
    if (ramPreviewBuildQueue_.pendingFrames.empty()) {
      ramPreviewBuildQueue_.active = false;
    }
  }

  QString previewDiskCacheRoot() {
    if (!previewDiskCacheRoot_.trimmed().isEmpty()) {
      return previewDiskCacheRoot_;
    }

    QString root =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.trimmed().isEmpty()) {
      root = QDir::homePath();
    }

    QDir dir(root);
    dir.mkpath(QStringLiteral("PreviewDiskCache"));
    previewDiskCacheRoot_ = dir.filePath(QStringLiteral("PreviewDiskCache"));
    return previewDiskCacheRoot_;
  }

  QString currentCompositionStateHash() {
    if (!previewDiskCompositionStateHash_.isEmpty()) {
      return previewDiskCompositionStateHash_;
    }
    if (!currentComposition_) {
      return QStringLiteral("no-composition");
    }
    const QByteArray compositionState =
        currentComposition_->toJson().toJson(QJsonDocument::Compact);
    previewDiskCompositionStateHash_ = QString::fromLatin1(
        QCryptographicHash::hash(compositionState, QCryptographicHash::Sha256)
            .toHex()
            .left(24));
    return previewDiskCompositionStateHash_;
  }

  QString currentCompositionDiskCacheNamespace() {
    if (!currentComposition_) {
      return QStringLiteral("no-composition");
    }

    const auto settings = currentComposition_->settings();
    const QSize compSize = settings.compositionSize();
    // v3 adds the serialized composition state hash so edits cannot reuse a
    // prior composition namespace.  Older namespaces remain unreachable.
    // v2 deliberately separated files produced before disk-write generation
    // checks were introduced.  Do not reuse a v1 frame whose invalidation
    // provenance cannot be established.
    const QString basis = QStringLiteral("preview-frame-v3|%1|%2|%3x%4|%5|%6|%7|%8")
                              .arg(currentComposition_->id().toString(),
                                   settings.compositionName()
                                       .toQString()
                                       .trimmed(),
                                   QString::number(std::max(1, compSize.width())),
                                   QString::number(std::max(1, compSize.height())),
                                   QString::number(
                                       currentComposition_->frameRate()
                                           .framerate(),
                                       'f', 3),
                                   QString::number(currentComposition_
                                                       ->frameRange()
                                                       .duration()),
                                   previewDiskRenderContract_,
                                   currentCompositionStateHash());
    const QByteArray digest =
        QCryptographicHash::hash(basis.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex().left(24));
  }

  QString currentCompositionDiskCacheDir() {
    QDir root(previewDiskCacheRoot());
    const QString compositionKey = currentCompositionDiskCacheNamespace();
    previewDiskActiveNamespace_ = compositionKey;
    root.mkpath(compositionKey);
    return root.filePath(compositionKey);
  }

  void clearPreviewDiskCacheForCurrentComposition() {
    if (!currentComposition_) {
      previewDiskActiveNamespace_.clear();
      previewDiskCompositionStateHash_.clear();
      return;
    }

    const QString compositionId = currentComposition_->id().toString();
    QString namespaceToClear = previewDiskActiveNamespace_;
    if (namespaceToClear.isEmpty()) {
      namespaceToClear = currentCompositionDiskCacheNamespace();
    }
    QDir root(previewDiskCacheRoot());
    QDir dir(root.filePath(namespaceToClear));
    {
      // A queued frame write belongs to the old cache generation and must not
      // recreate this directory after it has been cleared.
      std::lock_guard<std::mutex> lock(previewDiskWriteMutex_);
      ++previewDiskGeneration_;
      std::erase_if(previewDiskWriteQueue_, [&compositionId](
                                            const PreviewDiskWriteTask &task) {
        return task.compositionId == compositionId;
      });
      if (dir.exists()) {
        dir.removeRecursively();
      }
      if (previewDiskActiveNamespace_ == namespaceToClear) {
        previewDiskActiveNamespace_.clear();
      }
      previewDiskCompositionStateHash_.clear();
    }
  }

  void updatePreviewDiskRenderContract(const int previewDownsample,
                                       const int effectiveDownsample,
                                       const QString &renderPath) {
    const QString basis =
        QStringLiteral("preview-contract-v1|previewDownsample=%1|"
                       "effectiveDownsample=%2|path=%3")
            .arg(std::max(1, previewDownsample))
            .arg(std::max(1, effectiveDownsample))
            .arg(renderPath.trimmed().isEmpty() ? QStringLiteral("unknown")
                                                : renderPath.trimmed());
    const QString contract = QString::fromLatin1(
        QCryptographicHash::hash(basis.toUtf8(), QCryptographicHash::Sha256)
            .toHex()
            .left(24));
    if (previewDiskRenderContract_ == contract) {
      return;
    }

    // Clear the prior namespace before switching contracts.  Keeping the
    // RAM state would let a previous-quality frame survive this transition.
    clearPreviewDiskCacheForCurrentComposition();
    previewDiskRenderContract_ = contract;
    cancelRamPreviewBuild(QStringLiteral("preview-render-contract-changed"));
    resetRamPreviewCache();
  }

  QString previewDiskCacheFramePathForNamespace(const QString &compositionKey,
                                                const int64_t frame) {
    QDir root(previewDiskCacheRoot());
    root.mkpath(compositionKey);
    return root.filePath(compositionKey + QStringLiteral("/frame_%1.png")
                                              .arg(frame, 8, 10, QChar('0')));
  }

  QString previewDiskCacheFramePath(const int64_t frame) {
    return previewDiskCacheFramePathForNamespace(
        currentCompositionDiskCacheNamespace(), frame);
  }

  std::vector<int64_t> enforcePreviewDiskCacheBudget(
      const QString &savedFramePath) {
    std::vector<int64_t> evictedFrames;
    const QFileInfo savedInfo(savedFramePath);
    QDir directory(savedInfo.absolutePath());
    if (!directory.exists()) {
      return evictedFrames;
    }

    QFileInfoList frames = directory.entryInfoList(
        QStringList{QStringLiteral("frame_*.png")}, QDir::Files, QDir::NoSort);
    qint64 totalBytes = 0;
    for (const QFileInfo &frame : frames) {
      totalBytes += frame.size();
    }
    if (totalBytes <= previewDiskCacheBudgetBytes_.load()) {
      return evictedFrames;
    }

    std::sort(frames.begin(), frames.end(), [](const QFileInfo &a,
                                                const QFileInfo &b) {
      if (a.lastModified() != b.lastModified()) {
        return a.lastModified() < b.lastModified();
      }
      return a.fileName() < b.fileName();
    });
    for (const QFileInfo &frame : frames) {
      if (totalBytes <= previewDiskCacheBudgetBytes_.load()) {
        break;
      }
      // Preserve the just-completed frame even when one image exceeds budget.
      if (frame.absoluteFilePath() == savedInfo.absoluteFilePath()) {
        continue;
      }
      const QString baseName = frame.completeBaseName();
      bool validFrame = false;
      const int64_t frameNumber = baseName.mid(QStringLiteral("frame_").size())
                                      .toLongLong(&validFrame);
      if (QFile::remove(frame.absoluteFilePath())) {
        totalBytes -= frame.size();
        if (validFrame) {
          evictedFrames.push_back(frameNumber);
        }
      }
    }
    return evictedFrames;
  }

  std::vector<int64_t> enforcePreviewDiskCacheGlobalBudget(
      const QString &savedFramePath) {
    std::vector<int64_t> evictedCurrentFrames;
    // The writer owns a concrete frame path.  Derive the cache root from it
    // instead of reading the UI-thread-owned composition/root state here.
    QDir root(QFileInfo(savedFramePath).absoluteDir());
    if (!root.cdUp() || !root.exists()) {
      return evictedCurrentFrames;
    }

    std::vector<QFileInfo> frames;
    qint64 totalBytes = 0;
    QDirIterator it(root.absolutePath(), QStringList{QStringLiteral("frame_*.png")},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
      it.next();
      const QFileInfo frame = it.fileInfo();
      totalBytes += frame.size();
      frames.push_back(frame);
    }
    const qint64 globalBudgetBytes =
        previewDiskCacheBudgetBytes_.load() * 4;
    if (totalBytes <= globalBudgetBytes) {
      return evictedCurrentFrames;
    }

    const QFileInfo savedInfo(savedFramePath);
    const QString savedDirectory = savedInfo.absolutePath();
    std::sort(frames.begin(), frames.end(), [](const QFileInfo &a,
                                                const QFileInfo &b) {
      if (a.lastModified() != b.lastModified()) {
        return a.lastModified() < b.lastModified();
      }
      return a.absoluteFilePath() < b.absoluteFilePath();
    });
    for (const QFileInfo &frame : frames) {
      if (totalBytes <= globalBudgetBytes) {
        break;
      }
      if (frame.absoluteFilePath() == savedInfo.absoluteFilePath()) {
        continue;
      }
      if (!QFile::remove(frame.absoluteFilePath())) {
        continue;
      }
      totalBytes -= frame.size();
      if (frame.absolutePath() != savedDirectory) {
        continue;
      }
      bool validFrame = false;
      const int64_t frameNumber =
          frame.completeBaseName().mid(QStringLiteral("frame_").size())
              .toLongLong(&validFrame);
      if (validFrame) {
        evictedCurrentFrames.push_back(frameNumber);
      }
    }

    // Empty namespace directories contain only generated preview data.  The
    // manifest is generated data too, so it must not keep an otherwise empty
    // namespace alive after the last frame was evicted.
    for (const QFileInfo &entry : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
      QDir namespaceDir(entry.absoluteFilePath());
      const QStringList namespaceEntries =
          namespaceDir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
      const bool containsOnlyManifest =
          !namespaceEntries.isEmpty() &&
          std::all_of(namespaceEntries.begin(), namespaceEntries.end(),
                      [](const QString &name) {
                        return name == QStringLiteral("manifest.json");
                      });
      if (namespaceEntries.isEmpty() || containsOnlyManifest) {
        if (containsOnlyManifest) {
          namespaceDir.remove(QStringLiteral("manifest.json"));
        }
        root.rmdir(entry.fileName());
      }
    }
    return evictedCurrentFrames;
  }

  bool writePreviewDiskManifest(const QString &directoryPath,
                                const QString &namespaceKey,
                                const QString &compositionId,
                                const QString &renderContract,
                                const QString &stateHash) {
    QDir directory(directoryPath);
    if (!directory.exists()) {
      return false;
    }

    QJsonArray frames;
    const QFileInfoList files = directory.entryInfoList(
        QStringList{QStringLiteral("frame_*.png")}, QDir::Files, QDir::Name);
    for (const QFileInfo &file : files) {
      bool validFrame = false;
      const int64_t frame = file.completeBaseName()
                                .mid(QStringLiteral("frame_").size())
                                .toLongLong(&validFrame);
      if (!validFrame) {
        continue;
      }
      QJsonObject frameEntry;
      frameEntry.insert(QStringLiteral("frame"), frame);
      frameEntry.insert(QStringLiteral("file"), file.fileName());
      frameEntry.insert(QStringLiteral("bytes"), file.size());
      frames.append(frameEntry);
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("schema"), kPreviewDiskManifestSchema);
    manifest.insert(QStringLiteral("namespace"), namespaceKey);
    manifest.insert(QStringLiteral("compositionId"), compositionId);
    manifest.insert(QStringLiteral("renderContract"), renderContract);
    manifest.insert(QStringLiteral("stateHash"), stateHash);
    manifest.insert(QStringLiteral("frameCount"), frames.size());
    manifest.insert(QStringLiteral("frames"), frames);

    QSaveFile file(directory.filePath(QStringLiteral("manifest.json")));
    if (!file.open(QIODevice::WriteOnly)) {
      return false;
    }
    file.write(QJsonDocument(manifest).toJson(QJsonDocument::Compact));
    return file.commit();
  }

  bool persistPreviewFrameToDisk(const QString &filePath, const QImage &image) {
    if (filePath.trimmed().isEmpty() || image.isNull()) {
      return false;
    }
    QFileInfo info(filePath);
    QDir().mkpath(info.absolutePath());

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
      return false;
    }

    if (!image.save(&file, "PNG")) {
      file.cancelWriting();
      return false;
    }

    return file.commit();
  }

  bool persistPreviewFrameToDisk(const QString &filePath,
                                 const ArtifactCore::ImageF32x4_RGBA &image) {
    if (filePath.trimmed().isEmpty() || image.isEmpty()) {
      return false;
    }
    return persistPreviewFrameToDisk(filePath, image.toQImage());
  }

  ArtifactCore::ImageF32x4_RGBA imageToCpuPreviewFrame(const QImage &image) {
    ArtifactCore::ImageF32x4_RGBA cpuImage;
    if (image.isNull()) {
      return cpuImage;
    }

    const QImage rgba =
        image.format() == QImage::Format_RGBA8888
            ? image
            : image.convertToFormat(QImage::Format_RGBA8888);
    cpuImage.setFromRGBA8(rgba.constBits(), rgba.width(), rgba.height());
    return cpuImage;
  }

  bool isPreviewDiskManifestFrameValid(const int64_t frame,
                                       const QString &filePath) {
    const QFileInfo frameInfo(filePath);
    if (!frameInfo.isFile() || frameInfo.size() <= 0) {
      return false;
    }
    QImageReader imageReader(filePath);
    if (!imageReader.canRead()) {
      return false;
    }
    const QString manifestPath =
        QDir(frameInfo.absolutePath()).filePath(QStringLiteral("manifest.json"));
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
      return false;
    }
    if (manifestFile.size() <= 0 || manifestFile.size() > 16 * 1024 * 1024) {
      return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
      return false;
    }

    const QJsonObject object = document.object();
    const QJsonArray frames = object.value(QStringLiteral("frames")).toArray();
    if (frames.size() > 100000) {
      return false;
    }
    if (object.value(QStringLiteral("schema")).toInt() !=
            kPreviewDiskManifestSchema ||
        object.value(QStringLiteral("frameCount")).toInt() != frames.size() ||
        object.value(QStringLiteral("namespace")).toString() !=
            currentCompositionDiskCacheNamespace() ||
        object.value(QStringLiteral("compositionId")).toString() !=
            currentComposition_->id().toString() ||
        object.value(QStringLiteral("renderContract")).toString() !=
            previewDiskRenderContract_ ||
        object.value(QStringLiteral("stateHash")).toString() !=
            currentCompositionStateHash()) {
      return false;
    }

    std::unordered_set<qint64> manifestFrames;
    manifestFrames.reserve(static_cast<size_t>(frames.size()));
    for (const QJsonValue &value : frames) {
      if (!value.isObject()) {
        return false;
      }
      const QJsonObject entry = value.toObject();
      const qint64 entryFrame =
          entry.value(QStringLiteral("frame")).toVariant().toLongLong();
      const qint64 entryBytes =
          entry.value(QStringLiteral("bytes")).toVariant().toLongLong();
      const QString entryFile = entry.value(QStringLiteral("file")).toString();
      if (entryFrame < 0 || entryBytes <= 0 || entryFile.isEmpty() ||
          QFileInfo(entryFile).fileName() != entryFile) {
        return false;
      }
      if (!manifestFrames.insert(entryFrame).second) {
        return false;
      }
      if (entryFile == frameInfo.fileName() &&
              entryFrame == frame &&
              entryBytes == frameInfo.size()) {
        return true;
      }
    }
    return false;
  }

  bool hasPreviewFrameOnDisk(const int64_t frame) {
    if (!previewDiskCacheEnabled_.load() || !currentComposition_ || frame < 0 ||
        previewDiskRenderContract_ == QStringLiteral("unbound")) {
      return false;
    }
    const QString filePath = previewDiskCacheFramePath(frame);
    return QFileInfo::exists(filePath) &&
           isPreviewDiskManifestFrameValid(frame, filePath);
  }

  void resizeFrameCacheStateStorage(const int64_t frameCount) {
    const size_t targetSize =
        static_cast<size_t>(std::max<int64_t>(0, frameCount));
    frameCacheStates_.assign(targetSize, ArtifactRamPreviewFrameCacheState{});
    cacheBitmap_.assign(targetSize, false);
    ramPreviewImageCache_.clear();
    ramPreviewImageLru_.clear();
    ramPreviewImageLruIndex_.clear();
  }

  void syncLegacyBitmapFromStates() {
    cacheBitmap_.assign(frameCacheStates_.size(), false);
    for (size_t i = 0; i < frameCacheStates_.size(); ++i) {
      cacheBitmap_[i] = isFrameReadyForRamPreview(static_cast<int64_t>(i));
    }
  }

  void clearFrameRequestFlagsOutsideRange(const FrameRange &range) {
    if (frameCacheStates_.empty()) {
      return;
    }

    const int64_t start = std::max<int64_t>(0, range.start());
    const int64_t endExclusive = std::max<int64_t>(
        start, std::min<int64_t>(static_cast<int64_t>(frameCacheStates_.size()),
                                 range.end()));

    for (int64_t frame = 0;
         frame < static_cast<int64_t>(frameCacheStates_.size()); ++frame) {
      auto &state = frameCacheStates_[static_cast<size_t>(frame)];
      const bool inRange = frame >= start && frame < endExclusive;
      state.requested = inRange;
      if (inRange) {
        state.onDisk = hasPreviewFrameOnDisk(frame);
        state.ready = state.inRam && !state.failed;
      } else {
        state.onDisk = false;
        ramPreviewImageCache_.erase(frame);
        eraseRamPreviewImageLru(frame);
        state.inRam = false;
        state.ready = false;
        cacheBitmap_[static_cast<size_t>(frame)] = false;
      }
      if (!inRange && !state.ready) {
        state.failed = false;
        state.reason.clear();
      }
    }

    syncLegacyBitmapFromStates();
  }

  bool isValidFrameIndex(const int64_t frame) const {
    return frame >= 0 &&
           frame < static_cast<int64_t>(frameCacheStates_.size());
  }

  void markFrameReady(const int64_t frame, const QString &reason = {}) {
    if (!isValidFrameIndex(frame)) {
      return;
    }

    auto &state = frameCacheStates_[static_cast<size_t>(frame)];
    if (!hasFrameImageInRam(frame)) {
      state.requested = true;
      state.ready = false;
      state.failed = false;
      state.inRam = false;
      cacheBitmap_[static_cast<size_t>(frame)] = false;
      if (!reason.trimmed().isEmpty()) {
        state.reason = reason.trimmed();
      } else {
        state.reason.clear();
      }
      return;
    }

    state.requested = true;
    state.ready = true;
    state.failed = false;
    state.inRam = true;
    if (!reason.trimmed().isEmpty()) {
      state.reason = reason.trimmed();
    } else {
      state.reason.clear();
    }
    cacheBitmap_[static_cast<size_t>(frame)] = true;
    completeRamPreviewBuildFrame(frame);
  }

  void storeFrameImageInRam(const int64_t frame, const QImage &image,
                            const QString &reason = {}) {
    if (!isValidFrameIndex(frame) || image.isNull()) {
      return;
    }

    const ArtifactCore::ImageF32x4_RGBA cpuImage = imageToCpuPreviewFrame(image);
    ramPreviewImageCache_[frame] =
        ArtifactCore::ImageF32x4RGBAWithCache(cpuImage);
    touchRamPreviewImageLru(frame);
    evictRamPreviewImagesIfNeeded();
    auto &state = frameCacheStates_[static_cast<size_t>(frame)];
    state.requested = true;
    state.ready = true;
    state.failed = false;
    state.inRam = true;
    stampFrameDependencies(state);
    if (!reason.trimmed().isEmpty()) {
      state.reason = reason.trimmed();
    } else {
      state.reason.clear();
    }
    cacheBitmap_[static_cast<size_t>(frame)] = true;
    completeRamPreviewBuildFrame(frame);
  }

  void storeFrameImageInRam(const int64_t frame,
                            const ArtifactCore::ImageF32x4_RGBA &image,
                            const QString &reason = {}) {
    if (!isValidFrameIndex(frame) || image.isEmpty()) {
      return;
    }

    ramPreviewImageCache_[frame] =
        ArtifactCore::ImageF32x4RGBAWithCache(image);
    touchRamPreviewImageLru(frame);
    evictRamPreviewImagesIfNeeded();
    auto &state = frameCacheStates_[static_cast<size_t>(frame)];
    state.requested = true;
    state.ready = true;
    state.failed = false;
    state.inRam = true;
    stampFrameDependencies(state);
    if (!reason.trimmed().isEmpty()) {
      state.reason = reason.trimmed();
    } else {
      state.reason.clear();
    }
    cacheBitmap_[static_cast<size_t>(frame)] = true;
    completeRamPreviewBuildFrame(frame);
  }

  void stampFrameDependencies(ArtifactRamPreviewFrameCacheState &state) const {
    state.compositionId = currentComposition_
        ? currentComposition_->id().toString() : QString();
    state.compositionRevision = currentComposition_
        ? currentComposition_->revision() : 0;
    state.layerIds.clear();
    state.effectIds.clear();
    if (!currentComposition_) {
      return;
    }
    for (const auto &layer : currentComposition_->allLayer()) {
      if (!layer) {
        continue;
      }
      state.layerIds.push_back(layer->id().toString());
      for (const auto &effect : layer->getEffects()) {
        if (effect) {
          state.effectIds.push_back(effect->effectID().toQString());
        }
      }
    }
    for (const auto &effect : currentComposition_->getEffects()) {
      if (effect) {
        state.effectIds.push_back(effect->effectID().toQString());
      }
    }
  }

  bool storeRamPreviewFrameImage(const int64_t frame, const QImage &image,
                                 const QString &reason = {},
                                 const bool persistToDisk = true) {
    if (!isValidFrameIndex(frame) || image.isNull()) {
      return false;
    }

    storeFrameImageInRam(frame, image, reason);
    if (persistToDisk && previewDiskCacheEnabled_.load()) {
      const QString filePath = previewDiskCacheFramePath(frame);
      {
        std::lock_guard<std::mutex> lock(previewDiskWriteMutex_);
        previewDiskWriteQueue_.push_back(
            PreviewDiskWriteTask{
                frame, filePath, imageToCpuPreviewFrame(image),
                currentComposition_ ? currentComposition_->id().toString()
                                    : QString(),
                previewDiskRenderContract_,
                currentCompositionStateHash(),
                previewDiskGeneration_.load()});
      }
      previewDiskWriteCv_.notify_one();
      markFrameOnDisk(frame, false);
    } else {
      markFrameOnDisk(frame, false);
    }
    clearFrameFailure(frame);
    return true;
  }

  bool storeRamPreviewFrameImage(const int64_t frame,
                                 const ArtifactCore::ImageF32x4_RGBA &image,
                                 const QString &reason = {},
                                 const bool persistToDisk = true) {
    if (!isValidFrameIndex(frame) || image.isEmpty()) {
      return false;
    }

    storeFrameImageInRam(frame, image, reason);
    if (persistToDisk && previewDiskCacheEnabled_.load()) {
      const QString filePath = previewDiskCacheFramePath(frame);
      {
        std::lock_guard<std::mutex> lock(previewDiskWriteMutex_);
        previewDiskWriteQueue_.push_back(
            PreviewDiskWriteTask{
                frame, filePath, image,
                currentComposition_ ? currentComposition_->id().toString()
                                    : QString(),
                previewDiskRenderContract_,
                currentCompositionStateHash(),
                previewDiskGeneration_.load()});
      }
      previewDiskWriteCv_.notify_one();
      markFrameOnDisk(frame, false);
    } else {
      markFrameOnDisk(frame, false);
    }
    clearFrameFailure(frame);
    return true;
  }

  bool hasFrameImageInRam(const int64_t frame) const {
    if (!isValidFrameIndex(frame)) {
      return false;
    }
    return ramPreviewImageCache_.find(frame) != ramPreviewImageCache_.end();
  }

  void markFrameRequested(const int64_t frame, const QString &reason = {}) {
    if (!isValidFrameIndex(frame)) {
      return;
    }

    auto &state = frameCacheStates_[static_cast<size_t>(frame)];
    state.requested = true;
    const bool hasRamImage = hasFrameImageInRam(frame);
    state.inRam = hasRamImage;
    // The image map is the source of truth for RAM residency. Keep the
    // state table and bitmap in the same invariant so a stale `ready` bit
    // cannot survive a playback tick.
    state.ready = hasRamImage && !state.failed;
    cacheBitmap_[static_cast<size_t>(frame)] = state.ready;
    if (state.failed && reason.trimmed().isEmpty()) {
      return;
    }
    if (!reason.trimmed().isEmpty()) {
      state.reason = reason.trimmed();
    }
  }

  void markFrameFailed(const int64_t frame, const QString &reason) {
    if (!isValidFrameIndex(frame)) {
      return;
    }

    auto &state = frameCacheStates_[static_cast<size_t>(frame)];
    state.requested = true;
    state.ready = false;
    state.failed = true;
    state.inRam = false;
    state.reason = reason.trimmed().isEmpty() ? QStringLiteral("render-failed")
                                              : reason.trimmed();
    cacheBitmap_[static_cast<size_t>(frame)] = false;
    completeRamPreviewBuildFrame(frame);
  }

  void markFrameOnDisk(const int64_t frame, const bool onDisk) {
    if (!isValidFrameIndex(frame)) {
      return;
    }

    auto &state = frameCacheStates_[static_cast<size_t>(frame)];
    state.onDisk = onDisk;
  }

  void clearFrameFailure(const int64_t frame) {
    if (!isValidFrameIndex(frame)) {
      return;
    }

    auto &state = frameCacheStates_[static_cast<size_t>(frame)];
    state.failed = false;
    if (!state.ready) {
      state.reason.clear();
    }
  }

  bool hydrateFrameFromDisk(const int64_t frame) {
    if (!previewDiskCacheEnabled_.load() || !isValidFrameIndex(frame) ||
        previewDiskRenderContract_ == QStringLiteral("unbound")) {
      return false;
    }

    if (ramPreviewImageCache_.find(frame) != ramPreviewImageCache_.end()) {
      auto &state = frameCacheStates_[static_cast<size_t>(frame)];
      state.inRam = true;
      state.ready = true;
      touchRamPreviewImageLru(frame);
      cacheBitmap_[static_cast<size_t>(frame)] = true;
      completeRamPreviewBuildFrame(frame);
      return true;
    }

    const QString filePath = previewDiskCacheFramePath(frame);
    if (!QFileInfo::exists(filePath) ||
        !isPreviewDiskManifestFrameValid(frame, filePath)) {
      auto &state = frameCacheStates_[static_cast<size_t>(frame)];
      state.onDisk = false;
      if (!state.failed && !state.inRam) {
        state.ready = false;
      }
      return false;
    }

    const QImage image(filePath);
    if (image.isNull()) {
      return false;
    }

    storeFrameImageInRam(frame, image, QStringLiteral("disk-hydrated"));
    markFrameOnDisk(frame, true);
    completeRamPreviewBuildFrame(frame);
    return true;
  }

  void hydrateFramesFromDiskNear(const int64_t focusFrame,
                                 const FrameRange &range,
                                 const int maxFramesToTouch) {
    if (frameCacheStates_.empty() || maxFramesToTouch <= 0) {
      return;
    }

    const int64_t maxFrameIndex =
        static_cast<int64_t>(frameCacheStates_.size()) - 1;
    const int64_t start =
        std::clamp(std::min(range.start(), range.end()), int64_t{0}, maxFrameIndex);
    const int64_t end =
        std::clamp(std::max(range.start(), range.end()), int64_t{0}, maxFrameIndex);
    if (end < start) {
      return;
    }

    const int64_t center = std::clamp(focusFrame, start, end);
    int touchedFrames = 0;
    auto tryTouchFrame = [&](const int64_t frame) {
      if (touchedFrames >= maxFramesToTouch || frame < start || frame > end) {
        return;
      }
      if (hydrateFrameFromDisk(frame)) {
        ++touchedFrames;
      }
    };

    tryTouchFrame(center);
    for (int64_t offset = 1;
         touchedFrames < maxFramesToTouch &&
         (center - offset >= start || center + offset <= end);
         ++offset) {
      if (center - offset >= start) {
        tryTouchFrame(center - offset);
      }
      if (center + offset <= end) {
        tryTouchFrame(center + offset);
      }
    }
  }

  void resetRamPreviewCache() {
    const int64_t frameCount =
        currentComposition_ ? currentComposition_->frameRange().duration() : 0;
    resizeFrameCacheStateStorage(frameCount);
    clearFrameRequestFlagsOutsideRange(ramPreviewRange_);
    emitRamPreviewStats();
  }

  void invalidateRamPreviewForCurrentComposition(const QString &reason) {
    cancelRamPreviewBuild(reason);
    clearPreviewDiskCacheForCurrentComposition();
    resetRamPreviewCache();
    publishRamPreviewStateChanged(ramPreviewEnabled_, ramPreviewRange_);
  }

  void invalidateRamPreviewRangeForCurrentComposition(const FrameRange &range,
                                                       const QString &reason) {
    if (!currentComposition_ || frameCacheStates_.empty()) {
      return;
    }
    const int64_t first = std::max<int64_t>(0, range.start());
    const int64_t last = std::min<int64_t>(
        static_cast<int64_t>(frameCacheStates_.size()) - 1, range.end());
    if (last < first) {
      return;
    }

    // Reject queued disk writes from the old render contract. The generation
    // is global on purpose: a stale writer must never recreate an invalidated
    // frame after the file has been removed.
    ++previewDiskGeneration_;
    for (int64_t frame = first; frame <= last; ++frame) {
      ramPreviewImageCache_.erase(frame);
      eraseRamPreviewImageLru(frame);
      auto &state = frameCacheStates_[static_cast<size_t>(frame)];
      state = ArtifactRamPreviewFrameCacheState{};
      state.reason = reason.trimmed().isEmpty()
          ? QStringLiteral("range-invalidated") : reason.trimmed();
      cacheBitmap_[static_cast<size_t>(frame)] = false;
      if (previewDiskCacheEnabled_.load()) {
        QFile::remove(previewDiskCacheFramePath(frame));
      }
    }
    emitRamPreviewStats();
    publishRamPreviewStateChanged(ramPreviewEnabled_, ramPreviewRange_);
  }

  FrameRange clampedRamPreviewRange(const FramePosition &center) const {
    const int64_t centerFrame = center.framePosition();
    const int64_t radius = std::max(0, ramPreviewRadiusFrames_);
    int64_t start = std::max<int64_t>(0, centerFrame - radius);
    int64_t end = centerFrame + radius;
    if (currentComposition_) {
      const auto compositionRange = currentComposition_->frameRange();
      start = std::max<int64_t>(start, compositionRange.start());
      end = std::min<int64_t>(end, compositionRange.end());
    }
    if (end < start) {
      end = start;
    }
    return FrameRange(start, end);
  }

  void prewarmRamPreviewAround(const FramePosition &position) {
    if (!ramPreviewEnabled_ || !engine_ || !currentComposition_) {
      return;
    }

    const FrameRange range = clampedRamPreviewRange(position);
    ramPreviewRange_ = range;
    requestRamPreviewBuild(ramPreviewRange_,
                           QStringLiteral("prewarm-around-current"));

    // Interactive timeline seeking can arrive hundreds of times per second.
    // Queue priority immediately, but coalesce disk probes, whole-cache stats
    // scans and cache-strip repaint signals to one maintenance pass per 40 ms.
    const uint64_t generation = ++seekPreviewMaintenanceGeneration_;
    const auto runMaintenance = [this, generation, position, range]() {
      if (generation != seekPreviewMaintenanceGeneration_ || !owner_) {
        return;
      }
      const int maxHydrationFrames =
          std::max(1, std::min(ramPreviewRadiusFrames_ * 2 + 1, 9));
      hydrateFramesFromDiskNear(position.framePosition(), range,
                                maxHydrationFrames);
      emitRamPreviewStats();
      publishRamPreviewStateChanged(ramPreviewEnabled_, range);
      seekPreviewMaintenanceClock_.restart();
    };
    constexpr int kSeekMaintenanceIntervalMs = 40;
    if (!seekPreviewMaintenanceClock_.isValid() ||
        seekPreviewMaintenanceClock_.elapsed() >= kSeekMaintenanceIntervalMs) {
      runMaintenance();
    } else {
      const int delay = std::max(
          1, kSeekMaintenanceIntervalMs -
                 static_cast<int>(seekPreviewMaintenanceClock_.elapsed()));
      QTimer::singleShot(delay, owner_, runMaintenance);
    }
  }

  // Accessors for ram preview statistics
  float ramPreviewHitRate() const {
    if (frameCacheStates_.empty()) {
      return 0.0f;
    }
    const int64_t start = std::max<int64_t>(0, ramPreviewRange_.start());
    const int64_t endExclusive = std::max<int64_t>(
        start, std::min<int64_t>(static_cast<int64_t>(frameCacheStates_.size()),
                                 ramPreviewRange_.end()));
    int requested = 0;
    int hits = 0;
    for (int64_t frame = start; frame < endExclusive; ++frame) {
      const auto &state = frameCacheStates_[static_cast<size_t>(frame)];
      if (!state.requested) {
        continue;
      }
      ++requested;
      if (isFrameReadyForRamPreview(frame)) {
        ++hits;
      }
    }
    return requested > 0 ? static_cast<float>(hits) /
                               static_cast<float>(requested)
                         : 0.0f;
  }

  int ramPreviewCachedFrameCount() const {
    int cached = 0;
    for (size_t i = 0; i < frameCacheStates_.size(); ++i) {
      if (isFrameReadyForRamPreview(static_cast<int64_t>(i))) {
        ++cached;
      }
    }
    return cached;
  }

  int ramPreviewRequestedFrameCount() const {
    if (frameCacheStates_.empty()) {
      return 0;
    }
    const int64_t start = std::max<int64_t>(0, ramPreviewRange_.start());
    const int64_t endExclusive = std::max<int64_t>(
        start, std::min<int64_t>(static_cast<int64_t>(frameCacheStates_.size()),
                                 ramPreviewRange_.end()));
    int requested = 0;
    for (int64_t frame = start; frame < endExclusive; ++frame) {
      if (frameCacheStates_[static_cast<size_t>(frame)].requested) {
        ++requested;
      }
    }
    return requested;
  }

  int ramPreviewReadyFrameCountInRange() const {
    if (frameCacheStates_.empty()) {
      return 0;
    }

    const int64_t start = std::max<int64_t>(0, ramPreviewRange_.start());
    const int64_t endExclusive = std::min<int64_t>(
        static_cast<int64_t>(frameCacheStates_.size()), ramPreviewRange_.end());
    if (endExclusive <= start) {
      return 0;
    }

    int ready = 0;
    for (int64_t frame = start; frame < endExclusive; ++frame) {
      if (isFrameReadyForRamPreview(frame)) {
        ++ready;
      }
    }
    return ready;
  }

  int ramPreviewPlayableFrameCountInRange() const {
    return ramPreviewReadyFrameCountInRange();
  }

  int ramPreviewReadyMissingImageFrameCountInRange() const {
    if (frameCacheStates_.empty()) {
      return 0;
    }

    const int64_t start = std::max<int64_t>(0, ramPreviewRange_.start());
    const int64_t endExclusive = std::min<int64_t>(
        static_cast<int64_t>(frameCacheStates_.size()), ramPreviewRange_.end());
    if (endExclusive <= start) {
      return 0;
    }

    int missing = 0;
    for (int64_t frame = start; frame < endExclusive; ++frame) {
      const auto &state = frameCacheStates_[static_cast<size_t>(frame)];
      if (state.ready && !state.failed && !hasFrameImageInRam(frame)) {
        ++missing;
      }
    }
    return missing;
  }

  int ramPreviewFailedFrameCountInRange() const {
    if (frameCacheStates_.empty()) {
      return 0;
    }

    const int64_t start = std::max<int64_t>(0, ramPreviewRange_.start());
    const int64_t endExclusive = std::min<int64_t>(
        static_cast<int64_t>(frameCacheStates_.size()), ramPreviewRange_.end());
    if (endExclusive <= start) {
      return 0;
    }

    int failed = 0;
    for (int64_t frame = start; frame < endExclusive; ++frame) {
      if (frameCacheStates_[static_cast<size_t>(frame)].failed) {
        ++failed;
      }
    }
    return failed;
  }

  int ramPreviewDiskFrameCountInRange() const {
    if (frameCacheStates_.empty()) {
      return 0;
    }

    const int64_t start = std::max<int64_t>(0, ramPreviewRange_.start());
    const int64_t endExclusive = std::min<int64_t>(
        static_cast<int64_t>(frameCacheStates_.size()), ramPreviewRange_.end());
    if (endExclusive <= start) {
      return 0;
    }

    int onDisk = 0;
    for (int64_t frame = start; frame < endExclusive; ++frame) {
      if (frameCacheStates_[static_cast<size_t>(frame)].onDisk) {
        ++onDisk;
      }
    }
    return onDisk;
  }

  int ramPreviewRangeFrameCount() const {
    if (frameCacheStates_.empty()) {
      return 0;
    }

    const int64_t start = std::max<int64_t>(0, ramPreviewRange_.start());
    const int64_t endExclusive = std::min<int64_t>(
        static_cast<int64_t>(frameCacheStates_.size()), ramPreviewRange_.end());
    return static_cast<int>(std::max<int64_t>(0, endExclusive - start));
  }

  bool ramPreviewBuildRangeReady() const {
    const int frameCount = ramPreviewRangeFrameCount();
    if (frameCount <= 0) {
      return false;
    }

    return ramPreviewReadyFrameCountInRange() == frameCount &&
           ramPreviewBuildQueue_.pendingFrames.empty();
  }

  bool ramPreviewPlaybackStartReady() const {
    if (frameCacheStates_.empty()) {
      return false;
    }
    const int64_t start = std::max<int64_t>(0, ramPreviewRange_.start());
    const int64_t end = std::min<int64_t>(
        static_cast<int64_t>(frameCacheStates_.size()), ramPreviewRange_.end());
    if (end <= start) {
      return false;
    }
    const int64_t current = std::clamp<int64_t>(
        engine_ ? engine_->currentFrame().framePosition() : start,
        start, end - 1);
    const bool reverse = owner_ && owner_->playbackSpeed() < 0.0f;
    const int64_t leadFrames = std::min<int64_t>(8, end - start);
    for (int64_t i = 0; i < leadFrames; ++i) {
      const int64_t frame = current + (reverse ? -i : i);
      if (frame < start || frame >= end || !isFrameReadyForRamPreview(frame)) {
        return false;
      }
    }
    return true;
  }

  ArtifactRamPreviewFrameCacheState ramPreviewFrameState(
      const int64_t frame) const {
    if (frame < 0 || frame >= static_cast<int64_t>(frameCacheStates_.size())) {
      return {};
    }
    auto state = frameCacheStates_[static_cast<size_t>(frame)];
    state.imageAvailable = hasFrameImageInRam(frame);
    state.playable =
        state.ready && state.inRam && !state.failed && state.imageAvailable;
    return state;
  }

ArtifactRamPreviewPriorityState ramPreviewPriorityState(
      const int64_t frame) const {
    ArtifactRamPreviewPriorityState state;

    const int64_t currentFrame =
        engine_ ? engine_->currentFrame().framePosition()
                : (controller_ ? controller_->currentFrame().framePosition() : 0);
    state.distanceFromCurrent =
        static_cast<int>(std::clamp<int64_t>(std::llabs(frame - currentFrame), 0,
                                             static_cast<int64_t>(
                                                 std::numeric_limits<int>::max())));
    state.currentFrame = frame == currentFrame;
    state.pendingBuild = isRamPreviewFramePendingBuild(frame);
    state.nextQueued = nextRamPreviewBuildFrame() == frame;
    state.playing = owner_ && owner_->state() == PlaybackState::Playing;
    state.reverse = owner_ && owner_->playbackSpeed() < 0.0f;

    if (currentComposition_) {
      const FrameRange compositionRange = currentComposition_->frameRange();
      state.inCompositionRange =
          frame >= compositionRange.start() && frame < compositionRange.end();

      const FrameRange workAreaRange = currentComposition_->workAreaRange();
      const int64_t workAreaStart =
          std::min<int64_t>(workAreaRange.start(), workAreaRange.end());
      const int64_t workAreaEnd =
          std::max<int64_t>(workAreaRange.start(), workAreaRange.end());
      state.inWorkArea = frame >= workAreaStart && frame < workAreaEnd;
    } else {
      state.inCompositionRange = isValidFrameIndex(frame);
      state.inWorkArea = state.inCompositionRange;
    }

    const int nearRadius = std::clamp(ramPreviewRadiusFrames_ / 4, 1, 6);
    const bool directionalSide =
        state.reverse ? frame < currentFrame : frame > currentFrame;

    if (!state.inCompositionRange) {
      state.band = QStringLiteral("out-of-range");
    } else if (state.currentFrame) {
      state.band = QStringLiteral("immediate");
    } else if (state.distanceFromCurrent <= nearRadius) {
      state.band = QStringLiteral("near");
    } else if (state.nextQueued ||
               (state.pendingBuild && state.playing && directionalSide)) {
      state.band = QStringLiteral("directional");
    } else if (state.pendingBuild) {
      state.band = QStringLiteral("safety-backfill");
    } else if (state.inWorkArea) {
      state.band = QStringLiteral("work-area");
    } else {
      state.band = QStringLiteral("unknown");
    }

    return state;
  }

  QString ramPreviewPriorityReason(const int64_t frame) const {
    return Artifact::ramPreviewPriorityReason(ramPreviewPriorityState(frame));
  }

  ArtifactRamPreviewSummary ramPreviewSummary() const {
    ArtifactRamPreviewSummary summary;
    const auto currentPriority = ramPreviewPriorityState(
        engine_ ? engine_->currentFrame().framePosition()
                : (controller_ ? controller_->currentFrame().framePosition() : 0));
    summary.enabled = ramPreviewEnabled_;
    summary.range = ramPreviewRange_;
    summary.requestedFrames = ramPreviewRequestedFrameCount();
    summary.readyFrames = ramPreviewReadyFrameCountInRange();
    summary.playableFrames = ramPreviewPlayableFrameCountInRange();
    summary.readyMissingImageFrames =
        ramPreviewReadyMissingImageFrameCountInRange();
    summary.failedFrames = ramPreviewFailedFrameCountInRange();
    summary.inRamFrames = ramPreviewCachedFrameCount();
    summary.onDiskFrames = ramPreviewDiskFrameCountInRange();
    summary.rangeFrames = ramPreviewRangeFrameCount();
    summary.buildRangeProgress = summary.rangeFrames > 0
                                     ? std::clamp(
                                           static_cast<float>(summary.readyFrames) /
                                               static_cast<float>(summary.rangeFrames),
                                           0.0f, 1.0f)
                                     : 0.0f;
    summary.buildQueuePendingFrames =
        static_cast<int>(ramPreviewBuildQueue_.pendingFrames.size());
    summary.buildQueueNextFrame = nextRamPreviewBuildFrame();
    summary.buildQueueActive = ramPreviewBuildQueue_.active;
    summary.buildRangeReady = ramPreviewBuildRangeReady();
    summary.playbackFallbackWhilePlaying =
        ramPreviewPlaybackFallbackWhilePlaying_;
    summary.buildQueueGeneration = ramPreviewBuildQueue_.generation;
    summary.buildQueueReason = ramPreviewBuildQueue_.reason;
    summary.currentPriorityBand = ramPreviewPriorityNote(currentPriority);
    summary.currentPriorityReason = Artifact::ramPreviewPriorityReason(currentPriority);
    summary.hitRate = ramPreviewHitRate();
    return summary;
  }

  bool tryGetRamPreviewFrameImage(const int64_t frame,
                                  ArtifactCore::ImageF32x4_RGBA &outImage) const {
    if (!isValidFrameIndex(frame)) {
      return false;
    }

    const auto &state = frameCacheStates_[static_cast<size_t>(frame)];
    if (!state.ready || !state.inRam || state.failed) {
      return false;
    }

    const auto it = ramPreviewImageCache_.find(frame);
    if (it == ramPreviewImageCache_.end()) {
      return false;
    }
    outImage = it->second.image().DeepCopy();
    return !outImage.isEmpty();
  }

  std::vector<bool> ramPreviewCacheBitmap() const {
    std::vector<bool> bitmap(frameCacheStates_.size(), false);
    for (size_t i = 0; i < frameCacheStates_.size(); ++i) {
      bitmap[i] = isFrameReadyForRamPreview(static_cast<int64_t>(i));
    }
    return bitmap;
  }

  void prewarmRamPreviewRange(const FrameRange &range) {
    if (!ramPreviewEnabled_ || !engine_ || !currentComposition_) {
      return;
    }

    ramPreviewRange_ = range;
    requestRamPreviewBuild(ramPreviewRange_, QStringLiteral("prewarm-range"));
    const int64_t current =
        engine_ ? engine_->currentFrame().framePosition() : ramPreviewRange_.start();
    if (ramPreviewRange_.contains(current)) {
      const int64_t rangeSpan =
          std::max<int64_t>(1, std::abs(ramPreviewRange_.end() - ramPreviewRange_.start()));
      const int maxHydrationFrames = static_cast<int>(
          std::clamp<int64_t>(rangeSpan, 1, 9));
      hydrateFramesFromDiskNear(current, ramPreviewRange_, maxHydrationFrames);
    }
    emitRamPreviewStats();
    publishRamPreviewStateChanged(ramPreviewEnabled_, ramPreviewRange_);
  }
};

ArtifactPlaybackService::ArtifactPlaybackService(QObject *parent)
    : QObject(parent), impl_(new Impl(this)) {
  // デフォルトの再生範囲を設定
  setFrameRange(FrameRange(FramePosition(0), FramePosition(300)));
}

ArtifactPlaybackService::~ArtifactPlaybackService() { delete impl_; }

ArtifactPlaybackService *ArtifactPlaybackService::instance() {
  static ArtifactPlaybackService service;
  return &service;
}

void ArtifactPlaybackService::setPlaybackRangeMode(PlaybackRangeMode mode) {
  if (impl_->playbackRangeMode_ == mode) {
    return;
  }
  impl_->playbackRangeMode_ = mode;

  // 再生範囲を更新
  impl_->applyCurrentPlaybackFrameRangeToEngine();

  ArtifactCore::globalEventBus().publish<PlaybackRangeModeChangedEvent>(
      PlaybackRangeModeChangedEvent{mode});
}

PlaybackRangeMode ArtifactPlaybackService::playbackRangeMode() const {
  return impl_->playbackRangeMode_;
}

void ArtifactPlaybackService::setPlaybackSkipMode(PlaybackSkipMode mode) {
  if (impl_->engine_) {
    impl_->engine_->setPlaybackSkipMode(mode);
  }
  ArtifactCore::globalEventBus().publish<PlaybackSkipModeChangedEvent>(
      PlaybackSkipModeChangedEvent{mode});
}

PlaybackSkipMode ArtifactPlaybackService::playbackSkipMode() const {
  return impl_->engine_ ? impl_->engine_->playbackSkipMode() : PlaybackSkipMode::None;
}

void ArtifactPlaybackService::play() {
  impl_->beginPlaybackSessionCapture();
  if (!impl_->ensureCurrentCompositionBound()) {
    qWarning() << "[PlaybackService] play ignored: no current composition bound";
    impl_->finishPlaybackSessionCapture(QStringLiteral("rejected-no-composition"));
    return;
  }

  qInfo() << "[PlaybackService] play requested"
          << "frame=" << impl_->engine_->currentFrame().framePosition()
          << "state="
          << static_cast<int>(impl_->engine_->isPlaying()
                                  ? PlaybackState::Playing
                                  : (impl_->engine_->isPaused()
                                         ? PlaybackState::Paused
                                         : PlaybackState::Stopped))
          << "composition=" << impl_->currentComposition_->id().toString()
          << "range="
          << impl_->currentComposition_->frameRange().startPosition().framePosition()
          << "-"
          << impl_->currentComposition_->frameRange().endPosition().framePosition()
          << "fps=" << impl_->currentComposition_->frameRate().framerate()
          << "ramPreviewEnabled=" << impl_->ramPreviewEnabled_
          << "ramReady=" << impl_->ramPreviewPlaybackStartReady();

  // Request a RAM preview build, but do not gate transport on cache readiness.
  // The previous gate made Play appear inert when the asynchronous build could
  // not receive a frame-completion callback (for example during a GPU/readback
  // fallback). Playback must remain responsive while the cache warms up.
  if (impl_->ramPreviewEnabled_ && !impl_->ramPreviewPlaybackStartReady()) {
    FrameRange previewRange = impl_->currentComposition_->frameRange();
    if (impl_->playbackRangeMode_ == PlaybackRangeMode::WorkArea) {
      previewRange = impl_->currentComposition_->workAreaRange();
    }
    impl_->ramPreviewAutoPlayPending_ = false;
    impl_->ramPreviewRange_ = previewRange;
    impl_->requestRamPreviewBuild(previewRange,
                                  QStringLiteral("playback-auto-preview"));
    impl_->emitRamPreviewStats();
    impl_->publishRamPreviewStateChanged(true, previewRange);
  }

  impl_->startAudioClock();
  if (impl_->ramPreviewEnabled_) {
    impl_->ramPreviewAutoPlaybackActive_ = true;
  }
  
  // 再生開始直前に最新の範囲を適用
  impl_->applyCurrentPlaybackFrameRangeToEngine();

  // 新しいエンジンを使用
  if (impl_->currentComposition_) {
    impl_->currentComposition_->play();
  }
  if (impl_->engine_) {
    impl_->engine_->play();
  }
  if (!impl_->engine_ || !impl_->engine_->isPlaying()) {
    qCritical() << "[PlaybackService] playback start failed"
                << "reason=engine-rejected-play"
                << "composition=" << impl_->currentComposition_->id().toString();
    impl_->stopAudioClock();
    impl_->finishPlaybackSessionCapture(
        QStringLiteral("rejected-engine-start"));
    return;
  }
  if (impl_->ramPreviewEnabled_) {
    impl_->prewarmRamPreviewAround(currentFrame());
  }
  QTimer::singleShot(500, this, [this]() {
    if (!impl_->playbackSessionCaptureActive_ || !impl_->engine_ ||
        !impl_->engine_->isPlaying() ||
        impl_->playbackSessionPublishedFrames_.load(std::memory_order_relaxed) > 0) {
      return;
    }
    qCritical() << "[PlaybackService] playback start stalled"
                << "reason=no-frame-tick-within-500ms"
                << "composition="
                << (impl_->currentComposition_
                        ? impl_->currentComposition_->id().toString()
                        : QStringLiteral("null"));
    impl_->finishPlaybackSessionCapture(
        QStringLiteral("startup-no-frame-tick"));
  });
}

void ArtifactPlaybackService::pause() {
  qInfo() << "[PlaybackService] pause requested"
          << "frame="
          << (impl_->engine_ ? impl_->engine_->currentFrame().framePosition()
                             : -1)
          << "enginePlaying="
          << (impl_->engine_ && impl_->engine_->isPlaying());
  impl_->pauseAudioClock();
  if (impl_->engine_) {
    impl_->engine_->pause();
  }
  if (impl_->currentComposition_) {
    impl_->currentComposition_->pause();
  }
  /*
  if (impl_->controller_) {
      impl_->controller_->pause();
  }
  */
}

void ArtifactPlaybackService::stop() {
  qInfo() << "[PlaybackService] stop requested"
          << "frame="
          << (impl_->engine_ ? impl_->engine_->currentFrame().framePosition()
                             : -1)
          << "enginePlaying="
          << (impl_->engine_ && impl_->engine_->isPlaying());
  impl_->stopAudioClock();
  impl_->ramPreviewAutoPlaybackActive_ = false;
  impl_->cancelRamPreviewBuild(QStringLiteral("playback-stopped"));
  // Composition stop invalidates media decode generations before the engine
  // emits its rewind frame. This prevents stale playback results from winning
  // over the final start-frame request.
  if (impl_->currentComposition_) {
    impl_->currentComposition_->stop();
  }
  if (impl_->engine_) {
    impl_->engine_->stop();
  }
  /*
  if (impl_->controller_) {
      impl_->controller_->stop();
  }
  */
}

void ArtifactPlaybackService::waitForStop() {
  if (impl_ && impl_->engine_) {
    impl_->engine_->waitForStop();
  }
}

void ArtifactPlaybackService::togglePlayPause() {
  if (isPlaying()) {
    pause();
  } else {
    play();
  }
}

void ArtifactPlaybackService::playFromFrame(const FramePosition &position) {
  goToFrame(position);
  play();
}

void ArtifactPlaybackService::pauseAndGoToFrame(const FramePosition &position) {
  pause();
  goToFrame(position);
}

void ArtifactPlaybackService::shuttleForward() {
  const float speed = playbackSpeed();
  if (speed >= 0.0f && speed < 8.0f) {
    setPlaybackSpeed(speed <= 0.0f ? 1.0f : speed * 2.0f);
  }
  play();
}

void ArtifactPlaybackService::shuttleReverse() {
  const float speed = playbackSpeed();
  if (speed <= 0.0f && speed > -8.0f) {
    setPlaybackSpeed(speed >= 0.0f ? -1.0f : speed * 2.0f);
  } else {
    setPlaybackSpeed(-1.0f);
  }
  play();
}

void ArtifactPlaybackService::shuttleStop() {
  setPlaybackSpeed(0.0f);
  stop();
}

void ArtifactPlaybackService::goToFrame(const FramePosition &position) {
  if (impl_->engine_) {
    impl_->engine_->goToFrame(position);
  } else if (impl_->controller_) {
    impl_->controller_->goToFrame(position);
  }
  impl_->prewarmRamPreviewAround(position);
}

void ArtifactPlaybackService::goToNextFrame() {
  if (impl_->engine_) {
    impl_->engine_->goToNextFrame();
  } else if (impl_->controller_) {
    impl_->controller_->goToNextFrame();
  }
  impl_->prewarmRamPreviewAround(currentFrame());
}

void ArtifactPlaybackService::goToPreviousFrame() {
  if (impl_->engine_) {
    impl_->engine_->goToPreviousFrame();
  } else if (impl_->controller_) {
    impl_->controller_->goToPreviousFrame();
  }
  impl_->prewarmRamPreviewAround(currentFrame());
}

void ArtifactPlaybackService::goToStartFrame() {
  if (impl_->engine_) {
    impl_->engine_->goToStartFrame();
  } else if (impl_->controller_) {
    impl_->controller_->goToStartFrame();
  }
  impl_->prewarmRamPreviewAround(currentFrame());
}

void ArtifactPlaybackService::goToEndFrame() {
  if (impl_->engine_) {
    impl_->engine_->goToEndFrame();
  } else if (impl_->controller_) {
    impl_->controller_->goToEndFrame();
  }
  impl_->prewarmRamPreviewAround(currentFrame());
}

bool ArtifactPlaybackService::isPlaying() const {
  return impl_ && impl_->engine_ ? impl_->engine_->isPlaying() : false;
}

bool ArtifactPlaybackService::isPaused() const {
  return impl_->engine_ ? impl_->engine_->isPaused() : false;
}

bool ArtifactPlaybackService::isStopped() const {
  return impl_->engine_ ? impl_->engine_->isStopped() : false;
}

PlaybackState ArtifactPlaybackService::state() const {
  if (impl_->engine_) {
    if (impl_->engine_->isPlaying())
      return PlaybackState::Playing;
    if (impl_->engine_->isPaused())
      return PlaybackState::Paused;
    return PlaybackState::Stopped;
  }
  return impl_->controller_ ? impl_->controller_->state()
                            : PlaybackState::Stopped;
}

FramePosition ArtifactPlaybackService::currentFrame() const {
  return impl_->engine_
             ? impl_->engine_->currentFrame()
             : (impl_->controller_ ? impl_->controller_->currentFrame()
                                   : FramePosition(0));
}

void ArtifactPlaybackService::setCurrentFrame(const FramePosition &position) {
  if (currentFrame() == position) {
    return;
  }
  const bool wasPlaying = isPlaying();
  if (impl_->engine_) {
    impl_->engine_->setCurrentFrame(position);
  } else if (impl_->controller_) {
    impl_->controller_->setCurrentFrame(position);
  }
  impl_->syncCurrentCompositionFrame(position);
  const QString compositionId =
      impl_->currentComposition_ ? impl_->currentComposition_->id().toString()
                                 : QString();
  ArtifactCore::globalEventBus().publish<FrameChangedEvent>(
      FrameChangedEvent{compositionId, position.framePosition()});
  if (!wasPlaying) {
    impl_->prewarmRamPreviewAround(position);
  }
}

FrameRange ArtifactPlaybackService::frameRange() const {
  return impl_->engine_
             ? impl_->engine_->frameRange()
             : (impl_->controller_
                    ? impl_->controller_->frameRange()
                    : FrameRange(FramePosition(0), FramePosition(100)));
}

void ArtifactPlaybackService::setFrameRange(const FrameRange &range) {
  if (impl_->engine_) {
    impl_->engine_->setFrameRange(range);
  }
  if (impl_->controller_) {
    impl_->controller_->setFrameRange(range);
  }
  impl_->ramPreviewRange_ = range;
  impl_->cancelRamPreviewBuild(QStringLiteral("frame-range-changed"));
  impl_->emitRamPreviewStats();
  impl_->publishRamPreviewStateChanged(impl_->ramPreviewEnabled_,
                                       impl_->ramPreviewRange_);
}

FrameRate ArtifactPlaybackService::frameRate() const {
  return impl_->engine_ ? impl_->engine_->frameRate()
                        : (impl_->controller_ ? impl_->controller_->frameRate()
                                              : FrameRate(30.0f));
}

void ArtifactPlaybackService::setFrameRate(const FrameRate &rate) {
  if (impl_->engine_) {
    impl_->engine_->setFrameRate(rate);
  }
  if (impl_->controller_) {
    impl_->controller_->setFrameRate(rate);
  }
}

void ArtifactPlaybackService::syncWorkAreaAfterUndo(
    const ArtifactCompositionPtr &composition) {
  if (!impl_ || !composition || impl_->currentComposition_ != composition) {
    return;
  }
  const auto range = composition->workAreaRange();
  ArtifactCore::globalEventBus().publish<WorkAreaChangedEvent>({
      composition->id().toString(), range.start(), range.end()});
  impl_->applyCurrentPlaybackFrameRangeToEngine();
}

void ArtifactPlaybackService::setWorkAreaStartAtCurrentFrame() {
  if (!impl_->currentComposition_) {
    return;
  }
  const int64_t activeFrame = currentFrame().framePosition();
  const auto composition = impl_->currentComposition_;
  const auto before = composition->workAreaRange();
  const int64_t outPoint = before.end();
  const int64_t afterStart = activeFrame;
  const int64_t afterEnd = std::max<int64_t>(activeFrame + 1, outPoint);
  if (before.start() == afterStart && before.end() == afterEnd) {
    return;
  }
  if (auto* undo = UndoManager::instance()) {
    if (!undo->push(std::make_unique<SetCompositionWorkAreaCommand>(
            composition, before.start(), before.end(), afterStart, afterEnd,
            [this](const ArtifactCompositionPtr &changed, qint64, qint64) {
              syncWorkAreaAfterUndo(changed);
            }))) {
      return;
    }
    return;
  }
  composition->setWorkAreaRange(FrameRange(afterStart, afterEnd));
  if (composition->workAreaRange().start() != afterStart ||
      composition->workAreaRange().end() != afterEnd) {
    composition->setWorkAreaRange(before);
    return;
  }
  ArtifactCore::globalEventBus().publish<WorkAreaChangedEvent>({
      composition->id().toString(), composition->workAreaRange().start(),
      composition->workAreaRange().end()});
  impl_->applyCurrentPlaybackFrameRangeToEngine();
}

void ArtifactPlaybackService::setWorkAreaEndAtCurrentFrame() {
  if (!impl_->currentComposition_) {
    return;
  }
  const int64_t activeFrame = currentFrame().framePosition();
  const auto composition = impl_->currentComposition_;
  const auto before = composition->workAreaRange();
  const int64_t inPoint = before.start();
  const int64_t afterStart = std::min<int64_t>(inPoint, activeFrame);
  const int64_t afterEnd = std::max<int64_t>(activeFrame + 1, inPoint);
  if (before.start() == afterStart && before.end() == afterEnd) {
    return;
  }
  if (auto* undo = UndoManager::instance()) {
    if (!undo->push(std::make_unique<SetCompositionWorkAreaCommand>(
            composition, before.start(), before.end(), afterStart, afterEnd,
            [this](const ArtifactCompositionPtr &changed, qint64, qint64) {
              syncWorkAreaAfterUndo(changed);
            }))) {
      return;
    }
    return;
  }
  composition->setWorkAreaRange(FrameRange(afterStart, afterEnd));
  if (composition->workAreaRange().start() != afterStart ||
      composition->workAreaRange().end() != afterEnd) {
    composition->setWorkAreaRange(before);
    return;
  }
  ArtifactCore::globalEventBus().publish<WorkAreaChangedEvent>({
      composition->id().toString(), composition->workAreaRange().start(),
      composition->workAreaRange().end()});
  impl_->applyCurrentPlaybackFrameRangeToEngine();
}

void ArtifactPlaybackService::moveWorkAreaToCurrentFrame() {
  if (!impl_->currentComposition_) {
    return;
  }
  const auto composition = impl_->currentComposition_;
  const auto range = composition->workAreaRange();
  const int64_t duration = std::max<int64_t>(1, range.end() - range.start());
  const int64_t activeFrame = currentFrame().framePosition();
  const int64_t afterStart = activeFrame;
  const int64_t afterEnd = activeFrame + duration;
  if (range.start() == afterStart && range.end() == afterEnd) {
    return;
  }
  if (auto* undo = UndoManager::instance()) {
    if (!undo->push(std::make_unique<SetCompositionWorkAreaCommand>(
            composition, range.start(), range.end(), afterStart, afterEnd,
            [this](const ArtifactCompositionPtr &changed, qint64, qint64) {
              syncWorkAreaAfterUndo(changed);
            }))) {
      return;
    }
    return;
  }
  composition->setWorkAreaRange(FrameRange(afterStart, afterEnd));
  if (composition->workAreaRange().start() != afterStart ||
      composition->workAreaRange().end() != afterEnd) {
    composition->setWorkAreaRange(range);
    return;
  }
  ArtifactCore::globalEventBus().publish<WorkAreaChangedEvent>({
      composition->id().toString(), composition->workAreaRange().start(),
      composition->workAreaRange().end()});
  impl_->applyCurrentPlaybackFrameRangeToEngine();
}

float ArtifactPlaybackService::playbackSpeed() const {
  return impl_->engine_
             ? impl_->engine_->playbackSpeed()
             : (impl_->controller_ ? impl_->controller_->playbackSpeed()
                                   : 1.0f);
}

void ArtifactPlaybackService::setPlaybackSpeed(float speed) {
  if (impl_->engine_) {
    impl_->engine_->setPlaybackSpeed(speed);
  }
  if (impl_->controller_) {
    impl_->controller_->setPlaybackSpeed(speed);
  }
}

bool ArtifactPlaybackService::isLooping() const {
  return impl_->engine_
             ? impl_->engine_->isLooping()
             : (impl_->controller_ ? impl_->controller_->isLooping() : false);
}

void ArtifactPlaybackService::setPingPong(bool enabled) {
  if (impl_->engine_) {
    impl_->engine_->setPingPong(enabled);
  }
}

bool ArtifactPlaybackService::isPingPong() const {
  return impl_->engine_ && impl_->engine_->isPingPong();
}

void ArtifactPlaybackService::setLooping(bool loop) {
  if (impl_->engine_) {
    impl_->engine_->setLooping(loop);
  }
  if (impl_->controller_) {
    impl_->controller_->setLooping(loop);
  }
}

bool ArtifactPlaybackService::isRealTime() const {
  return impl_->controller_ ? impl_->controller_->isRealTime() : true;
}

void ArtifactPlaybackService::setRealTime(bool realTime) {
  if (impl_->controller_) {
    impl_->controller_->setRealTime(realTime);
  }
}

void ArtifactPlaybackService::setAudioClockProvider(
    const std::function<double()> &provider) {
  if (!impl_)
    return;
  impl_->setExternalAudioClockProvider(provider);
  if (impl_->controller_) {
    impl_->controller_->setAudioClockProvider(provider);
  }
}

void ArtifactPlaybackService::setAudioMasterVolume(float volume) {
  if (!impl_)
    return;
  impl_->audioMasterVolume_ = std::clamp(volume, 0.0f, 2.0f);
  if (impl_->engine_) {
    impl_->engine_->setAudioMasterVolume(impl_->audioMasterVolume_);
  }
}

void ArtifactPlaybackService::setAudioMasterMuted(bool muted) {
  if (!impl_)
    return;
  impl_->audioMasterMuted_ = muted;
  if (impl_->engine_) {
    impl_->engine_->setAudioMasterMuted(muted);
  }
}

void ArtifactPlaybackService::setAudioOutputDeviceName(
    const QString &deviceName) {
  if (impl_ && impl_->engine_) {
    impl_->engine_->setAudioOutputDeviceName(deviceName);
  }
}

ArtifactPlaybackAudioDiagnostics ArtifactPlaybackService::audioDiagnostics() const {
  return impl_ && impl_->engine_ ? impl_->engine_->audioDiagnostics()
                                 : ArtifactPlaybackAudioDiagnostics{};
}

QString ArtifactPlaybackService::audioOutputDeviceName() const {
  return impl_ && impl_->engine_ ? impl_->engine_->audioOutputDeviceName()
                                 : QString();
}

void ArtifactPlaybackService::setCurrentComposition(
    ArtifactCompositionPtr composition) {
  if (impl_->currentComposition_ != composition) {
    // Remove the previously active namespace before replacing the composition
    // pointer; after replacement its state hash would address a new namespace.
    impl_->clearPreviewDiskCacheForCurrentComposition();
    impl_->currentComposition_ = composition;
    impl_->previewDiskRenderContract_ = QStringLiteral("unbound");
    impl_->cancelRamPreviewBuild(QStringLiteral("composition-changed"));
    
    // Clear and resize cache bitmap
    if (composition) {
        impl_->resizeFrameCacheStateStorage(composition->frameRange().duration());
        impl_->ramPreviewRange_ =
            impl_->ramPreviewEnabled_
                ? impl_->clampedRamPreviewRange(composition->framePosition())
                : composition->frameRange();
    } else {
        impl_->resizeFrameCacheStateStorage(0);
        impl_->ramPreviewRange_ = FrameRange(FramePosition(0), FramePosition(0));
    }
    if (!impl_->ramPreviewEnabled_) {
      impl_->clearFrameRequestFlagsOutsideRange(impl_->ramPreviewRange_);
      impl_->emitRamPreviewStats();
    }

    // エンジンにコンポジションの設定を反映
    if (impl_->engine_ && composition) {
      impl_->applyCurrentPlaybackFrameRangeToEngine();
      impl_->engine_->setFrameRate(composition->frameRate());
      impl_->engine_->setCurrentFrame(composition->framePosition());
      impl_->engine_->setComposition(composition);
    }

    // コントローラーにも設定を反映
    if (impl_->controller_ && composition) {
      impl_->controller_->setFrameRange(composition->frameRange());
      impl_->controller_->setFrameRate(composition->frameRate());
      impl_->controller_->setCurrentFrame(composition->framePosition());
    }

    ArtifactCore::globalEventBus().publish<PlaybackCompositionChangedEvent>(
        PlaybackCompositionChangedEvent{
            composition ? composition->id().toString() : QString()});
    if (impl_->ramPreviewEnabled_) {
      impl_->prewarmRamPreviewAround(composition ? composition->framePosition()
                                                 : currentFrame());
    }
  }
}

ArtifactCompositionPtr ArtifactPlaybackService::currentComposition() const {
  return impl_->currentComposition_;
}

ArtifactCore::FrameDebugSnapshot ArtifactPlaybackService::frameDebugSnapshot() const {
  struct TraceScopeGuard {
    ArtifactCore::TraceScopeRecord scope;
    QElapsedTimer timer;
    TraceScopeGuard() {
      scope.name = QStringLiteral("ArtifactPlaybackService::frameDebugSnapshot");
      scope.domain = ArtifactCore::TraceDomain::UI;
      timer.start();
    }
    ~TraceScopeGuard() {
      scope.endNs = timer.nsecsElapsed();
      if (scope.endNs <= scope.startNs) {
        scope.endNs = scope.startNs + 1;
      }
      ArtifactCore::TraceRecorder::instance().recordScope(scope);
    }
  } traceGuard;

  ArtifactCore::FrameDebugSnapshot snapshot;
  snapshot.frame = currentFrame();
  snapshot.playbackState = state() == PlaybackState::Playing
                               ? QStringLiteral("playing")
                               : (state() == PlaybackState::Paused
                                      ? QStringLiteral("paused")
                                      : QStringLiteral("stopped"));
  snapshot.timestampMs = static_cast<std::int64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
  if (const auto comp = currentComposition()) {
    snapshot.compositionName = comp->settings().compositionName().toQString();
  } else {
    snapshot.compositionName = QStringLiteral("<none>");
  }
  snapshot.selectedLayerName = QStringLiteral("<none>");
  snapshot.renderLastFrameMs = 0.0;
  snapshot.renderAverageFrameMs = 0.0;
  snapshot.renderBackend = QStringLiteral("playback");
  snapshot.compareMode = ArtifactCore::FrameDebugCompareMode::Disabled;
  ArtifactCore::TraceRecorder::instance().recordFrameDebugSnapshot(snapshot);
  return snapshot;
}

// ==================== In/Out Points ====================

void ArtifactPlaybackService::setInOutPoints(ArtifactInOutPoints *inOutPoints) {
  if (impl_ && impl_->engine_) {
    impl_->engine_->setInOutPoints(inOutPoints);
  } else if (impl_ && impl_->controller_) {
    impl_->controller_->setInOutPoints(inOutPoints);
  }
}

ArtifactInOutPoints *ArtifactPlaybackService::inOutPoints() const {
  if (impl_->engine_) {
    return impl_->engine_->inOutPoints();
  }
  return impl_ && impl_->controller_ ? impl_->controller_->inOutPoints()
                                     : nullptr;
}

ArtifactCore::Optional<FramePosition> ArtifactPlaybackService::inPoint() const {
  if (const auto *points = inOutPoints()) {
    return points->inPoint();
  }
  return std::nullopt;
}

ArtifactCore::Optional<FramePosition> ArtifactPlaybackService::outPoint() const {
  if (const auto *points = inOutPoints()) {
    return points->outPoint();
  }
  return std::nullopt;
}

bool ArtifactPlaybackService::hasInPoint() const {
  return inPoint().has_value();
}

bool ArtifactPlaybackService::hasOutPoint() const {
  return outPoint().has_value();
}

void ArtifactPlaybackService::setInPointAtCurrentFrame() {
  if (auto *points = inOutPoints()) {
    const QJsonObject before = points->toJson();
    points->setInPoint(currentFrame());
    recordInOutPointsMutation(points, before, points->toJson());
  }
}

void ArtifactPlaybackService::setOutPointAtCurrentFrame() {
  if (auto *points = inOutPoints()) {
    const QJsonObject before = points->toJson();
    points->setOutPoint(currentFrame());
    recordInOutPointsMutation(points, before, points->toJson());
  }
}

void ArtifactPlaybackService::clearInPoint() {
  if (auto *points = inOutPoints()) {
    const QJsonObject before = points->toJson();
    points->clearInPoint();
    recordInOutPointsMutation(points, before, points->toJson());
  }
}

void ArtifactPlaybackService::clearOutPoint() {
  if (auto *points = inOutPoints()) {
    const QJsonObject before = points->toJson();
    points->clearOutPoint();
    recordInOutPointsMutation(points, before, points->toJson());
  }
}

void ArtifactPlaybackService::clearInOutPoints() {
  if (auto *points = inOutPoints()) {
    const QJsonObject before = points->toJson();
    points->clearAllPoints();
    recordInOutPointsMutation(points, before, points->toJson());
  }
}

void ArtifactPlaybackService::goToInPoint() {
  if (auto *points = inOutPoints()) {
    if (const auto inPoint = points->inPoint()) {
      goToFrame(*inPoint);
    }
  }
}

void ArtifactPlaybackService::goToOutPoint() {
  if (auto *points = inOutPoints()) {
    if (const auto outPoint = points->outPoint()) {
      goToFrame(*outPoint);
    }
  }
}

void ArtifactPlaybackService::addMarkerAtCurrentFrame(const QString &comment) {
  if (auto *points = inOutPoints()) {
    const QJsonObject before = points->toJson();
    points->addMarker(currentFrame(), comment, MarkerType::Comment);
    recordInOutPointsMutation(points, before, points->toJson());
  }
}

void ArtifactPlaybackService::addChapterMarkerAtCurrentFrame(const QString &name) {
  if (auto *points = inOutPoints()) {
    const QJsonObject before = points->toJson();
    points->addMarker(currentFrame(), name, MarkerType::Chapter);
    recordInOutPointsMutation(points, before, points->toJson());
  }
}

void ArtifactPlaybackService::deleteMarkerAtCurrentFrame() {
  if (auto *points = inOutPoints()) {
    const QJsonObject before = points->toJson();
    points->removeMarker(currentFrame());
    recordInOutPointsMutation(points, before, points->toJson());
  }
}

void ArtifactPlaybackService::clearAllMarkers() {
  if (auto *points = inOutPoints()) {
    const QJsonObject before = points->toJson();
    points->clearAllMarkers();
    recordInOutPointsMutation(points, before, points->toJson());
  }
}

void ArtifactPlaybackService::goToNextMarker() {
  if (impl_ && impl_->engine_) {
    impl_->engine_->goToNextMarker();
  } else if (impl_ && impl_->controller_) {
    impl_->controller_->goToNextMarker();
  } else if (auto *points = inOutPoints()) {
    if (const auto next = points->nextMarker(currentFrame())) {
      goToFrame(*next);
    }
  }
}

void ArtifactPlaybackService::goToPreviousMarker() {
  if (impl_ && impl_->engine_) {
    impl_->engine_->goToPreviousMarker();
  } else if (impl_ && impl_->controller_) {
    impl_->controller_->goToPreviousMarker();
  } else if (auto *points = inOutPoints()) {
    if (const auto previous = points->previousMarker(currentFrame())) {
      goToFrame(*previous);
    }
  }
}

void ArtifactPlaybackService::goToNextChapter() {
  if (impl_ && impl_->engine_) {
    impl_->engine_->goToNextChapter();
  } else if (impl_ && impl_->controller_) {
    impl_->controller_->goToNextChapter();
  } else if (auto *points = inOutPoints()) {
    if (const auto next = points->nextChapter(currentFrame())) {
      goToFrame(*next);
    }
  }
}

void ArtifactPlaybackService::goToPreviousChapter() {
  if (impl_ && impl_->engine_) {
    impl_->engine_->goToPreviousChapter();
  } else if (impl_ && impl_->controller_) {
    impl_->controller_->goToPreviousChapter();
  } else if (auto *points = inOutPoints()) {
    if (const auto previous = points->previousChapter(currentFrame())) {
      goToFrame(*previous);
    }
  }
}

void ArtifactPlaybackService::setRamPreviewEnabled(bool enabled) {
  if (!impl_) {
    return;
  }
  if (impl_->ramPreviewEnabled_ == enabled) {
    return;
  }

  impl_->ramPreviewEnabled_ = enabled;
  if (!enabled) {
    impl_->cancelRamPreviewBuild(QStringLiteral("ram-preview-disabled"));
    impl_->emitRamPreviewStats();
    impl_->publishRamPreviewStateChanged(false, impl_->ramPreviewRange_);
    return;
  }

  prewarmRamPreviewAroundCurrentFrame();
  if (!impl_->currentComposition_) {
    impl_->publishRamPreviewStateChanged(true, impl_->ramPreviewRange_);
  }
}

bool ArtifactPlaybackService::isRamPreviewEnabled() const {
  return impl_ && impl_->ramPreviewEnabled_;
}

void ArtifactPlaybackService::setDiskPreviewCacheEnabled(bool enabled) {
  if (!impl_) {
    return;
  }
  const bool previous = impl_->previewDiskCacheEnabled_.exchange(enabled);
  if (previous == enabled) {
    return;
  }
  if (!enabled) {
    std::lock_guard<std::mutex> lock(impl_->previewDiskWriteMutex_);
    ++impl_->previewDiskGeneration_;
    impl_->previewDiskWriteQueue_.clear();
  }
}

bool ArtifactPlaybackService::isDiskPreviewCacheEnabled() const {
  return impl_ && impl_->previewDiskCacheEnabled_.load();
}

void ArtifactPlaybackService::setDiskPreviewCacheBudgetMB(const int megabytes) {
  if (!impl_) {
    return;
  }
  const qint64 clampedMegabytes = std::clamp<qint64>(megabytes, 512, 32768);
  impl_->previewDiskCacheBudgetBytes_.store(clampedMegabytes * 1024LL * 1024LL);

  // Apply a reduced budget immediately instead of waiting for the next frame
  // write. The eviction helper derives the cache root from any existing frame.
  QDir cacheRoot(impl_->previewDiskCacheRoot());
  if (!cacheRoot.exists()) {
    return;
  }
  QDirIterator frames(cacheRoot.absolutePath(),
                      QStringList{QStringLiteral("frame_*.png")},
                      QDir::Files, QDirIterator::Subdirectories);
  if (!frames.hasNext()) {
    return;
  }
  frames.next();
  const auto evicted =
      impl_->enforcePreviewDiskCacheGlobalBudget(frames.filePath());
  for (const int64_t frame : evicted) {
    impl_->markFrameOnDisk(frame, false);
  }
}

void ArtifactPlaybackService::setRamPreviewRadius(int frames) {
  if (!impl_) {
    return;
  }

  impl_->ramPreviewRadiusFrames_ = std::max(0, frames);
  if (impl_->ramPreviewEnabled_) {
    impl_->cancelRamPreviewBuild(QStringLiteral("preview-quality-changed"));
    prewarmRamPreviewAroundCurrentFrame();
  }
}

int ArtifactPlaybackService::ramPreviewRadius() const {
  return impl_ ? impl_->ramPreviewRadiusFrames_ : 0;
}

void ArtifactPlaybackService::setRamPreviewRange(const FrameRange &range) {
  if (!impl_) {
    return;
  }

  impl_->ramPreviewRange_ = range;
  if (impl_->ramPreviewEnabled_) {
    requestRamPreviewBuild(range, QStringLiteral("ram-preview-range-changed"));
  } else {
    impl_->cancelRamPreviewBuild(QStringLiteral("ram-preview-range-changed"));
    impl_->emitRamPreviewStats();
    impl_->publishRamPreviewStateChanged(false, range);
  }
}

FrameRange ArtifactPlaybackService::ramPreviewRange() const {
  return impl_ ? impl_->ramPreviewRange_
               : FrameRange(FramePosition(0), FramePosition(0));
}

void ArtifactPlaybackService::clearRamPreviewCache() {
  if (!impl_) {
    return;
  }

  // The public "Clear Cache" action must not leave disk-backed preview
  // frames available for immediate rehydration.
  impl_->invalidateRamPreviewForCurrentComposition(
      QStringLiteral("cache-cleared"));
}

void ArtifactPlaybackService::invalidateRamPreviewCache(const QString &reason) {
  if (!impl_) {
    return;
  }

  impl_->invalidateRamPreviewForCurrentComposition(
      reason.trimmed().isEmpty() ? QStringLiteral("ram-preview-invalidated")
                                 : reason.trimmed());
}

void ArtifactPlaybackService::invalidateRamPreviewRange(
    const FrameRange &range, const QString &reason) {
  if (!impl_) {
    return;
  }
  impl_->invalidateRamPreviewRangeForCurrentComposition(
      range, reason.trimmed().isEmpty() ? QStringLiteral("range-invalidated")
                                        : reason.trimmed());
}

void ArtifactPlaybackService::prewarmRamPreviewAroundCurrentFrame() {
  if (!impl_) {
    return;
  }

  impl_->prewarmRamPreviewAround(currentFrame());
}

void ArtifactPlaybackService::requestRamPreviewBuild(
    const FrameRange &range, const QString &reason) {
  if (!impl_) {
    return;
  }

  impl_->requestRamPreviewBuild(range, reason);
  impl_->ramPreviewRange_ = range;
  impl_->emitRamPreviewStats();
  impl_->publishRamPreviewStateChanged(impl_->ramPreviewEnabled_, range);
}

void ArtifactPlaybackService::cancelRamPreviewBuild(const QString &reason) {
  if (!impl_) {
    return;
  }

  impl_->cancelRamPreviewBuild(reason);
  impl_->emitRamPreviewStats();
}

void ArtifactPlaybackService::setRamPreviewPlaybackFallbackWhilePlaying(
    const bool enabled) {
  if (!impl_ || impl_->ramPreviewPlaybackFallbackWhilePlaying_ == enabled) {
    return;
  }

  impl_->ramPreviewPlaybackFallbackWhilePlaying_ = enabled;
  impl_->emitRamPreviewStats();
}

bool ArtifactPlaybackService::ramPreviewPlaybackFallbackWhilePlaying() const {
  if (!impl_) {
    return false;
  }
  return impl_->ramPreviewPlaybackFallbackWhilePlaying_ ||
         impl_->ramPreviewAutoPlaybackActive_ ||
         impl_->ramPreviewBuildRangeReady();
}

std::vector<bool> ArtifactPlaybackService::ramPreviewCacheBitmap() const {
  return impl_->ramPreviewCacheBitmap();
}

float ArtifactPlaybackService::ramPreviewHitRate() const {
  return impl_ ? impl_->ramPreviewHitRate() : 0.0f;
}

int ArtifactPlaybackService::ramPreviewCachedFrameCount() const {
  return impl_ ? impl_->ramPreviewCachedFrameCount() : 0;
}

int ArtifactPlaybackService::ramPreviewRequestedFrameCount() const {
  return impl_ ? impl_->ramPreviewRequestedFrameCount() : 0;
}

int ArtifactPlaybackService::ramPreviewReadyFrameCountInRange() const {
  return impl_ ? impl_->ramPreviewReadyFrameCountInRange() : 0;
}

int ArtifactPlaybackService::ramPreviewFailedFrameCountInRange() const {
  return impl_ ? impl_->ramPreviewFailedFrameCountInRange() : 0;
}

int ArtifactPlaybackService::ramPreviewDiskFrameCountInRange() const {
  return impl_ ? impl_->ramPreviewDiskFrameCountInRange() : 0;
}

ArtifactRamPreviewFrameCacheState
ArtifactPlaybackService::ramPreviewFrameState(const int64_t frame) const {
  return impl_ ? impl_->ramPreviewFrameState(frame)
               : ArtifactRamPreviewFrameCacheState{};
}

ArtifactRamPreviewPriorityState
ArtifactPlaybackService::ramPreviewPriorityState(const int64_t frame) const {
  return impl_ ? impl_->ramPreviewPriorityState(frame)
               : ArtifactRamPreviewPriorityState{};
}

ArtifactRamPreviewSummary ArtifactPlaybackService::ramPreviewSummary() const {
  return impl_ ? impl_->ramPreviewSummary() : ArtifactRamPreviewSummary{};
}

QString ArtifactPlaybackService::ramPreviewPriorityReason(const int64_t frame) const {
  return impl_ ? impl_->ramPreviewPriorityReason(frame) : QString{};
}

bool ArtifactPlaybackService::isRamPreviewFramePendingBuild(
    const int64_t frame) const {
  return impl_ ? impl_->isRamPreviewFramePendingBuild(frame) : false;
}

int64_t ArtifactPlaybackService::nextRamPreviewBuildFrame() const {
  return impl_ ? impl_->nextRamPreviewBuildFrame() : int64_t{-1};
}

bool ArtifactPlaybackService::tryGetRamPreviewFrameImage(
    const int64_t frame, ArtifactCore::ImageF32x4_RGBA &outImage) const {
  if (!impl_) {
    return false;
  }
  return impl_->tryGetRamPreviewFrameImage(frame, outImage);
}

void ArtifactPlaybackService::markRamPreviewFrameRequested(
    const int64_t frame, const QString &reason) {
  if (!impl_) {
    return;
  }
  impl_->markFrameRequested(frame, reason);
  impl_->emitRamPreviewStats();
}

void ArtifactPlaybackService::markRamPreviewFrameReady(const int64_t frame) {
  if (!impl_) {
    return;
  }
  impl_->markFrameReady(frame);
  impl_->emitRamPreviewStats();
}

bool ArtifactPlaybackService::storeRamPreviewFrameImage(
    const int64_t frame, const QImage &image, const QString &reason,
    const bool persistToDisk) {
  if (!impl_) {
    return false;
  }

  const bool stored =
      impl_->storeRamPreviewFrameImage(frame, image, reason, persistToDisk);
  impl_->emitRamPreviewStats();
  return stored;
}

bool ArtifactPlaybackService::storeRamPreviewFrameImage(
    const int64_t frame, const ArtifactCore::ImageF32x4_RGBA &image,
    const QString &reason, const bool persistToDisk) {
  if (!impl_) {
    return false;
  }

  const bool stored =
      impl_->storeRamPreviewFrameImage(frame, image, reason, persistToDisk);
  impl_->emitRamPreviewStats();
  return stored;
}

bool ArtifactPlaybackService::storeCompositionPreviewFrameImage(
    const int64_t frame, const QImage &image, const QString &compositionId,
    const int previewDownsample, const int effectiveDownsample,
    const QString &renderPath, const QString &reason,
    const bool persistToDisk) {
  if (!impl_) {
    return false;
  }
  const QString currentCompositionId =
      impl_->currentComposition_
          ? impl_->currentComposition_->id().toString()
          : QString();
  if (compositionId.trimmed().isEmpty() ||
      currentCompositionId != compositionId.trimmed()) {
    return false;
  }
  impl_->updatePreviewDiskRenderContract(previewDownsample,
                                         effectiveDownsample, renderPath);

  const auto summary = impl_->ramPreviewSummary();
  const QString detailReason =
      QStringLiteral(
          "composition-preview-readback;composition=%1;frame=%2;"
          "previewDownsample=%3;effectiveDownsample=%4;"
          "backend=composition-view;path=%5;policy=viewport-preview-v1;"
          "diskContract=quality-render-path-v1;queue=%6;gen=%7;"
          "next=%8;range=%9-%10;rangeReady=%11%12")
          .arg(compositionId.trimmed().isEmpty() ? QStringLiteral("-")
                                                 : compositionId.trimmed())
          .arg(frame)
          .arg(previewDownsample)
          .arg(effectiveDownsample)
          .arg(renderPath.trimmed().isEmpty() ? QStringLiteral("unknown")
                                              : renderPath.trimmed())
          .arg(summary.buildQueueReason)
          .arg(static_cast<qulonglong>(summary.buildQueueGeneration))
          .arg(summary.buildQueueNextFrame)
          .arg(summary.range.start())
          .arg(summary.range.end())
          .arg(summary.buildRangeReady ? 1 : 0)
          .arg(reason.trimmed().isEmpty()
                   ? QString()
                   : QStringLiteral(";note=%1").arg(reason.trimmed()));
  return storeRamPreviewFrameImage(frame, image, detailReason, persistToDisk);
}

bool ArtifactPlaybackService::storeCompositionPreviewFrameImage(
    const int64_t frame, const ArtifactCore::ImageF32x4_RGBA &image,
    const QString &compositionId, const int previewDownsample,
    const int effectiveDownsample, const QString &renderPath,
    const QString &reason, const bool persistToDisk) {
  if (!impl_) {
    return false;
  }
  const QString currentCompositionId =
      impl_->currentComposition_
          ? impl_->currentComposition_->id().toString()
          : QString();
  if (compositionId.trimmed().isEmpty() ||
      currentCompositionId != compositionId.trimmed()) {
    return false;
  }
  impl_->updatePreviewDiskRenderContract(previewDownsample,
                                         effectiveDownsample, renderPath);

  const auto summary = impl_->ramPreviewSummary();
  const QString detailReason =
      QStringLiteral(
          "composition-preview-readback;composition=%1;frame=%2;"
          "previewDownsample=%3;effectiveDownsample=%4;"
          "backend=composition-view;path=%5;policy=viewport-preview-v1;"
          "diskContract=quality-render-path-v1;queue=%6;gen=%7;"
          "next=%8;range=%9-%10;rangeReady=%11%12")
          .arg(compositionId.trimmed().isEmpty() ? QStringLiteral("-")
                                                 : compositionId.trimmed())
          .arg(frame)
          .arg(previewDownsample)
          .arg(effectiveDownsample)
          .arg(renderPath.trimmed().isEmpty() ? QStringLiteral("unknown")
                                              : renderPath.trimmed())
          .arg(summary.buildQueueReason)
          .arg(static_cast<qulonglong>(summary.buildQueueGeneration))
          .arg(summary.buildQueueNextFrame)
          .arg(summary.range.start())
          .arg(summary.range.end())
          .arg(summary.buildRangeReady ? 1 : 0)
          .arg(reason.trimmed().isEmpty()
                   ? QString()
                   : QStringLiteral(";note=%1").arg(reason.trimmed()));
  return storeRamPreviewFrameImage(frame, image, detailReason, persistToDisk);
}

void ArtifactPlaybackService::markRamPreviewFrameFailed(const int64_t frame,
                                                        const QString &reason) {
  if (!impl_) {
    return;
  }
  impl_->markFrameFailed(frame, reason);
  impl_->emitRamPreviewStats();
}

void ArtifactPlaybackService::markRamPreviewFrameOnDisk(const int64_t frame,
                                                        const bool onDisk) {
  if (!impl_) {
    return;
  }
  impl_->markFrameOnDisk(frame, onDisk);
  impl_->emitRamPreviewStats();
}

void ArtifactPlaybackService::clearRamPreviewFrameFailure(const int64_t frame) {
  if (!impl_) {
    return;
  }
  impl_->clearFrameFailure(frame);
  impl_->emitRamPreviewStats();
}

double ArtifactPlaybackService::audioOffsetSeconds() const {
  return impl_ ? impl_->audioOffsetSeconds_ : 0.0;
}

std::int64_t ArtifactPlaybackService::droppedFrameCount() const {
  return impl_ ? impl_->droppedFrameCount_ : 0;
}

ArtifactCompositionPlaybackController *
ArtifactPlaybackService::controller() const {
  return impl_->controller_;
}

} // namespace Artifact
