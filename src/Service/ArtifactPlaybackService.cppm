module;
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMetaObject>
#include <QSaveFile>
#include <QStandardPaths>
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
import Image.ImageF32x4RGBAWithCache;
import Artifact.Composition.PlaybackController;
import Artifact.Composition.Abstract;
import Artifact.Service.Project;
import Event.Bus;
import Artifact.Event.Types;

namespace Artifact {

using namespace ArtifactCore;

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
  };
  std::mutex previewDiskWriteMutex_;
  std::condition_variable previewDiskWriteCv_;
  std::deque<PreviewDiskWriteTask> previewDiskWriteQueue_;
  std::thread previewDiskWriterThread_;
  bool previewDiskWriterStop_ = false;
  std::atomic_bool shuttingDown_{false};
  struct RamPreviewBuildQueue {
    uint64_t generation = 0;
    bool active = false;
    FrameRange range{FramePosition(0), FramePosition(0)};
    QString reason;
    std::deque<int64_t> pendingFrames;
  };
  RamPreviewBuildQueue ramPreviewBuildQueue_;
  PlaybackRangeMode playbackRangeMode_ = PlaybackRangeMode::All;
  std::atomic<int64_t> pendingCompositionFrame_{0};
  std::atomic_bool compositionFrameSyncQueued_{false};
  QString previewDiskCacheRoot_;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

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
    if (!currentComposition_) {
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

        const bool savedToDisk =
            persistPreviewFrameToDisk(task.filePath, task.image);
        if (shuttingDown_.load(std::memory_order_acquire)) {
          continue;
        }
        QMetaObject::invokeMethod(
            owner_, [this, frame = task.frame,
                     compositionId = task.compositionId, savedToDisk]() {
              const QString currentCompositionId =
                  currentComposition_ ? currentComposition_->id().toString()
                                      : QString();
              if (compositionId.isEmpty() ||
                  currentCompositionId != compositionId) {
                return;
              }
              markFrameOnDisk(frame, savedToDisk);
            },
            Qt::QueuedConnection);
      }
    });

    // エンジンのシグナルをサービスに転送
    QObject::connect(
        engine_, &ArtifactPlaybackEngine::playbackStateChanged, owner_,
        [this](PlaybackState state) {
          const auto publishState = [this, state]() {
            if (state == PlaybackState::Paused) {
              pauseAudioClock();
            } else if (state == PlaybackState::Stopped) {
              stopAudioClock();
            }


            ArtifactCore::globalEventBus().publish<PlaybackStateChangedEvent>(
                PlaybackStateChangedEvent{state});
            Q_EMIT owner_->playbackStateChanged(state);
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

          syncCurrentCompositionFrame(position);
          const auto publishFrame = [this, position, compositionId, frameNumber,
                                     frameBuffer, hasConcreteFrame,
                                     diskCacheFramePath]() {
            const QString currentCompositionId =
                currentComposition_ ? currentComposition_->id().toString()
                                    : QString();
            if (compositionId.isEmpty() ||
                currentCompositionId != compositionId) {
              return;
            }
            if (hasConcreteFrame) {
              FrameSkipTracker::instance()->commitFrame(frameNumber);
              storeFrameImageInRam(frameNumber, frameBuffer,
                                   QStringLiteral("playback-frame"));
              {
                std::lock_guard<std::mutex> lock(previewDiskWriteMutex_);
                previewDiskWriteQueue_.push_back(
                    PreviewDiskWriteTask{frameNumber, diskCacheFramePath,
                                         frameBuffer, compositionId});
              }
              previewDiskWriteCv_.notify_one();
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
          QMetaObject::invokeMethod(owner_, publishFrame, Qt::QueuedConnection);
        },
        Qt::DirectConnection);

    QObject::connect(
        engine_, &ArtifactPlaybackEngine::playbackSpeedChanged, owner_,
        [this](float speed) {
          ArtifactCore::globalEventBus().publish<PlaybackSpeedChangedEvent>(
              PlaybackSpeedChangedEvent{speed});
          Q_EMIT owner_->playbackSpeedChanged(speed);
        },
        Qt::DirectConnection);

    QObject::connect(
        engine_, &ArtifactPlaybackEngine::loopingChanged, owner_,
        [this](bool loop) {
          ArtifactCore::globalEventBus().publish<PlaybackLoopingChangedEvent>(
              PlaybackLoopingChangedEvent{loop});
          Q_EMIT owner_->loopingChanged(loop);
        },
        Qt::DirectConnection);

    QObject::connect(
        engine_, &ArtifactPlaybackEngine::frameRangeChanged, owner_,
        [this](const FrameRange &range) {
          ArtifactCore::globalEventBus()
              .publish<PlaybackFrameRangeChangedEvent>(
                  PlaybackFrameRangeChangedEvent{range.start(), range.end()});
          Q_EMIT owner_->frameRangeChanged(range);
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
          Q_EMIT owner_->audioLevelChanged(leftRms, rightRms, leftPeak,
                                           rightPeak);
        },
        Qt::QueuedConnection);

    // コントローラーのシグナルも転送（後方互換性）
    // NOTE: controller は現在 engine
    // に置き換えられているため、シグナル転送を無効化して二重通知を防止
    /*
    QObject::connect(controller_,
    &ArtifactCompositionPlaybackController::playbackStateChanged, owner_,
    &ArtifactPlaybackService::playbackStateChanged, Qt::DirectConnection);

    QObject::connect(controller_,
    &ArtifactCompositionPlaybackController::frameChanged, owner_, [this](const
    FramePosition& position) { syncCurrentCompositionFrame(position); },
                     Qt::DirectConnection);

    QObject::connect(controller_,
    &ArtifactCompositionPlaybackController::playbackSpeedChanged, owner_,
    &ArtifactPlaybackService::playbackSpeedChanged, Qt::DirectConnection);

    QObject::connect(controller_,
    &ArtifactCompositionPlaybackController::loopingChanged, owner_,
    &ArtifactPlaybackService::loopingChanged, Qt::DirectConnection);

    QObject::connect(controller_,
    &ArtifactCompositionPlaybackController::frameRangeChanged, owner_,
    &ArtifactPlaybackService::frameRangeChanged, Qt::DirectConnection);
    */

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
      return 0.0;
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
    if (engine_) {
      engine_->stop();
      engine_->waitForStop();
    }
    {
      std::lock_guard<std::mutex> lock(previewDiskWriteMutex_);
      previewDiskWriterStop_ = true;
      // Shutdown must not drain a potentially large RAM-preview write queue.
      // Those entries are disposable cache data; keeping them made the process
      // remain alive after every window had already disappeared.
      previewDiskWriteQueue_.clear();
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
    if (!currentComposition_) {
      return;
    }

    const auto composition = currentComposition_;
    pendingCompositionFrame_.store(position.framePosition(),
                                   std::memory_order_relaxed);

    if (composition->thread() == QThread::currentThread()) {
      composition->goToFrame(position.framePosition());
      return;
    }

    if (compositionFrameSyncQueued_.exchange(true, std::memory_order_acq_rel)) {
      return;
    }

    QMetaObject::invokeMethod(
        composition.get(),
        [this, composition]() {
          const int64_t latestFrame =
              pendingCompositionFrame_.load(std::memory_order_relaxed);
          compositionFrameSyncQueued_.store(false, std::memory_order_release);
          if (composition) {
            composition->goToFrame(latestFrame);
          }
        },
        Qt::QueuedConnection);
  }

  void emitRamPreviewStats() {
    Q_EMIT owner_->ramPreviewStatsChanged(ramPreviewHitRate(),
                                          ramPreviewCachedFrameCount());
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

  QString currentCompositionDiskCacheNamespace() const {
    if (!currentComposition_) {
      return QStringLiteral("no-composition");
    }

    const auto settings = currentComposition_->settings();
    const QSize compSize = settings.compositionSize();
    const QString basis = QStringLiteral("%1|%2|%3x%4|%5|%6")
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
                                                       .duration()));
    const QByteArray digest =
        QCryptographicHash::hash(basis.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex().left(24));
  }

  QString currentCompositionDiskCacheDir() {
    QDir root(previewDiskCacheRoot());
    const QString compositionKey = currentCompositionDiskCacheNamespace();
    root.mkpath(compositionKey);
    return root.filePath(compositionKey);
  }

  void clearPreviewDiskCacheForCurrentComposition() {
    if (!currentComposition_) {
      return;
    }

    QDir dir(currentCompositionDiskCacheDir());
    if (dir.exists()) {
      dir.removeRecursively();
    }
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

  bool hasPreviewFrameOnDisk(const int64_t frame) {
    if (!currentComposition_ || frame < 0) {
      return false;
    }
    return QFileInfo::exists(previewDiskCacheFramePath(frame));
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
    if (!reason.trimmed().isEmpty()) {
      state.reason = reason.trimmed();
    } else {
      state.reason.clear();
    }
    cacheBitmap_[static_cast<size_t>(frame)] = true;
    completeRamPreviewBuildFrame(frame);
  }

  bool storeRamPreviewFrameImage(const int64_t frame, const QImage &image,
                                 const QString &reason = {},
                                 const bool persistToDisk = true) {
    if (!isValidFrameIndex(frame) || image.isNull()) {
      return false;
    }

    storeFrameImageInRam(frame, image, reason);
    if (persistToDisk) {
      const QString filePath = previewDiskCacheFramePath(frame);
      {
        std::lock_guard<std::mutex> lock(previewDiskWriteMutex_);
        previewDiskWriteQueue_.push_back(
            PreviewDiskWriteTask{
                frame, filePath, imageToCpuPreviewFrame(image),
                currentComposition_ ? currentComposition_->id().toString()
                                    : QString()});
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
    if (persistToDisk) {
      const QString filePath = previewDiskCacheFramePath(frame);
      {
        std::lock_guard<std::mutex> lock(previewDiskWriteMutex_);
        previewDiskWriteQueue_.push_back(
            PreviewDiskWriteTask{
                frame, filePath, image,
                currentComposition_ ? currentComposition_->id().toString()
                                    : QString()});
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
    if (!hasRamImage) {
      state.ready = false;
      cacheBitmap_[static_cast<size_t>(frame)] = false;
    } else if (state.failed) {
      cacheBitmap_[static_cast<size_t>(frame)] = false;
    } else if (state.ready) {
      cacheBitmap_[static_cast<size_t>(frame)] = true;
    }
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
    if (!isValidFrameIndex(frame)) {
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
    if (!QFileInfo::exists(filePath)) {
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
    Q_EMIT owner_->ramPreviewStateChanged(ramPreviewEnabled_, ramPreviewRange_);
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
    const int maxHydrationFrames =
        std::max(1, std::min(ramPreviewRadiusFrames_ * 2 + 1, 9));
    hydrateFramesFromDiskNear(position.framePosition(), ramPreviewRange_,
                              maxHydrationFrames);
    emitRamPreviewStats();
    Q_EMIT owner_->ramPreviewStateChanged(ramPreviewEnabled_, ramPreviewRange_);
  }

  // Accessors for ram preview statistics
  float ramPreviewHitRate() const {
    if (frameCacheStates_.empty()) {
      return 0.0f;
    }
    size_t hits = 0;
    for (size_t i = 0; i < frameCacheStates_.size(); ++i) {
      if (isFrameReadyForRamPreview(static_cast<int64_t>(i))) {
        ++hits;
      }
    }
    return static_cast<float>(hits) /
           static_cast<float>(frameCacheStates_.size());
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
    Q_EMIT owner_->ramPreviewStateChanged(ramPreviewEnabled_, ramPreviewRange_);
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
  Q_EMIT playbackRangeModeChanged(mode);
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
  Q_EMIT playbackSkipModeChanged(mode);
}

PlaybackSkipMode ArtifactPlaybackService::playbackSkipMode() const {
  return impl_->engine_ ? impl_->engine_->playbackSkipMode() : PlaybackSkipMode::None;
}

void ArtifactPlaybackService::play() {
  if (!impl_->ensureCurrentCompositionBound()) {
    qWarning() << "[PlaybackService] play ignored: no current composition bound";
    return;
  }

  impl_->startAudioClock();
  
  // 再生開始直前に最新の範囲を適用
  impl_->applyCurrentPlaybackFrameRangeToEngine();

  // 新しいエンジンを使用
  if (impl_->currentComposition_) {
    impl_->currentComposition_->play();
  }
  if (impl_->engine_) {
    impl_->engine_->play();
  }
  if (impl_->ramPreviewEnabled_) {
    impl_->prewarmRamPreviewAround(currentFrame());
  }
}

void ArtifactPlaybackService::pause() {
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
  impl_->stopAudioClock();
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
  Q_EMIT frameChanged(position);
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
  Q_EMIT ramPreviewStateChanged(impl_->ramPreviewEnabled_,
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

void ArtifactPlaybackService::setWorkAreaStartAtCurrentFrame() {
  if (!impl_->currentComposition_) {
    return;
  }
  const int64_t activeFrame = currentFrame().framePosition();
  const int64_t outPoint = impl_->currentComposition_->workAreaRange().end();
  impl_->currentComposition_->setWorkAreaRange(
      FrameRange(activeFrame, std::max<int64_t>(activeFrame + 1, outPoint)));
  ArtifactCore::globalEventBus().publish<WorkAreaChangedEvent>({
      impl_->currentComposition_->id().toString(),
      impl_->currentComposition_->workAreaRange().start(),
      impl_->currentComposition_->workAreaRange().end()});
  impl_->applyCurrentPlaybackFrameRangeToEngine();
}

void ArtifactPlaybackService::setWorkAreaEndAtCurrentFrame() {
  if (!impl_->currentComposition_) {
    return;
  }
  const int64_t activeFrame = currentFrame().framePosition();
  const int64_t inPoint = impl_->currentComposition_->workAreaRange().start();
  impl_->currentComposition_->setWorkAreaRange(
      FrameRange(std::min<int64_t>(inPoint, activeFrame),
                 std::max<int64_t>(activeFrame + 1, inPoint)));
  ArtifactCore::globalEventBus().publish<WorkAreaChangedEvent>({
      impl_->currentComposition_->id().toString(),
      impl_->currentComposition_->workAreaRange().start(),
      impl_->currentComposition_->workAreaRange().end()});
  impl_->applyCurrentPlaybackFrameRangeToEngine();
}

void ArtifactPlaybackService::moveWorkAreaToCurrentFrame() {
  if (!impl_->currentComposition_) {
    return;
  }
  const auto range = impl_->currentComposition_->workAreaRange();
  const int64_t duration = std::max<int64_t>(1, range.end() - range.start());
  const int64_t activeFrame = currentFrame().framePosition();
  impl_->currentComposition_->setWorkAreaRange(
      FrameRange(activeFrame, activeFrame + duration));
  ArtifactCore::globalEventBus().publish<WorkAreaChangedEvent>({
      impl_->currentComposition_->id().toString(),
      impl_->currentComposition_->workAreaRange().start(),
      impl_->currentComposition_->workAreaRange().end()});
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

void ArtifactPlaybackService::setCurrentComposition(
    ArtifactCompositionPtr composition) {
  if (impl_->currentComposition_ != composition) {
    impl_->currentComposition_ = composition;
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
    Q_EMIT currentCompositionChanged();
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

std::optional<FramePosition> ArtifactPlaybackService::inPoint() const {
  if (const auto *points = inOutPoints()) {
    return points->inPoint();
  }
  return std::nullopt;
}

std::optional<FramePosition> ArtifactPlaybackService::outPoint() const {
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
    points->setInPoint(currentFrame());
  }
}

void ArtifactPlaybackService::setOutPointAtCurrentFrame() {
  if (auto *points = inOutPoints()) {
    points->setOutPoint(currentFrame());
  }
}

void ArtifactPlaybackService::clearInPoint() {
  if (auto *points = inOutPoints()) {
    points->clearInPoint();
  }
}

void ArtifactPlaybackService::clearOutPoint() {
  if (auto *points = inOutPoints()) {
    points->clearOutPoint();
  }
}

void ArtifactPlaybackService::clearInOutPoints() {
  if (auto *points = inOutPoints()) {
    points->clearAllPoints();
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
    points->addMarker(currentFrame(), comment, MarkerType::Comment);
  }
}

void ArtifactPlaybackService::addChapterMarkerAtCurrentFrame(const QString &name) {
  if (auto *points = inOutPoints()) {
    points->addMarker(currentFrame(), name, MarkerType::Chapter);
  }
}

void ArtifactPlaybackService::deleteMarkerAtCurrentFrame() {
  if (auto *points = inOutPoints()) {
    points->removeMarker(currentFrame());
  }
}

void ArtifactPlaybackService::clearAllMarkers() {
  if (auto *points = inOutPoints()) {
    points->clearAllMarkers();
  }
}

void ArtifactPlaybackService::goToNextMarker() {
  if (impl_ && impl_->engine_) {
    impl_->engine_->goToNextMarker();
  } else if (impl_ && impl_->controller_) {
    impl_->controller_->goToNextMarker();
  }
}

void ArtifactPlaybackService::goToPreviousMarker() {
  if (impl_ && impl_->engine_) {
    impl_->engine_->goToPreviousMarker();
  } else if (impl_ && impl_->controller_) {
    impl_->controller_->goToPreviousMarker();
  }
}

void ArtifactPlaybackService::goToNextChapter() {
  if (impl_ && impl_->engine_) {
    impl_->engine_->goToNextChapter();
  } else if (impl_ && impl_->controller_) {
    impl_->controller_->goToNextChapter();
  }
}

void ArtifactPlaybackService::goToPreviousChapter() {
  if (impl_ && impl_->engine_) {
    impl_->engine_->goToPreviousChapter();
  } else if (impl_ && impl_->controller_) {
    impl_->controller_->goToPreviousChapter();
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
    Q_EMIT ramPreviewStateChanged(false, impl_->ramPreviewRange_);
    return;
  }

  prewarmRamPreviewAroundCurrentFrame();
  if (!impl_->currentComposition_) {
    Q_EMIT ramPreviewStateChanged(true, impl_->ramPreviewRange_);
  }
}

bool ArtifactPlaybackService::isRamPreviewEnabled() const {
  return impl_ && impl_->ramPreviewEnabled_;
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
    Q_EMIT ramPreviewStateChanged(false, range);
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

  impl_->cancelRamPreviewBuild(QStringLiteral("cache-cleared"));
  impl_->resetRamPreviewCache();
  Q_EMIT ramPreviewStateChanged(impl_->ramPreviewEnabled_,
                                impl_->ramPreviewRange_);
}

void ArtifactPlaybackService::invalidateRamPreviewCache(const QString &reason) {
  if (!impl_) {
    return;
  }

  impl_->invalidateRamPreviewForCurrentComposition(
      reason.trimmed().isEmpty() ? QStringLiteral("ram-preview-invalidated")
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
  Q_EMIT ramPreviewStateChanged(impl_->ramPreviewEnabled_, range);
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

  const auto summary = impl_->ramPreviewSummary();
  const QString detailReason =
      QStringLiteral(
          "composition-preview-readback;composition=%1;frame=%2;"
          "previewDownsample=%3;effectiveDownsample=%4;"
          "backend=composition-view;path=%5;policy=viewport-preview-v1;"
          "diskKeyLimit=composition-settings-only;queue=%6;gen=%7;"
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

  const auto summary = impl_->ramPreviewSummary();
  const QString detailReason =
      QStringLiteral(
          "composition-preview-readback;composition=%1;frame=%2;"
          "previewDownsample=%3;effectiveDownsample=%4;"
          "backend=composition-view;path=%5;policy=viewport-preview-v1;"
          "diskKeyLimit=composition-settings-only;queue=%6;gen=%7;"
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
