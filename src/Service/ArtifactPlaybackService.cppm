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
import Core.Diagnostics.Trace;
import Image.ImageF32x4RGBAWithCache;
import Artifact.Composition.PlaybackController;
import Artifact.Composition.Abstract;
import Event.Bus;
import Artifact.Event.Types;

namespace Artifact {

using namespace ArtifactCore;

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
  int ramPreviewRadiusFrames_ = 48;
  FrameRange ramPreviewRange_{FramePosition(0), FramePosition(0)};
  std::vector<bool> cacheBitmap_;
  std::vector<ArtifactRamPreviewFrameCacheState> frameCacheStates_;
  std::unordered_map<int64_t, ArtifactCore::ImageF32x4RGBAWithCache>
      ramPreviewImageCache_;
  PlaybackRangeMode playbackRangeMode_ = PlaybackRangeMode::All;
  std::atomic<int64_t> pendingCompositionFrame_{0};
  std::atomic_bool compositionFrameSyncQueued_{false};
  QString previewDiskCacheRoot_;

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

  explicit Impl(ArtifactPlaybackService *owner) : owner_(owner) {
    controller_ = new ArtifactCompositionPlaybackController();
    engine_ = new ArtifactPlaybackEngine();

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
          const QString compositionId =
              currentComposition_ ? currentComposition_->id().toString()
                                  : QString();
          
          markFrameReady(position.framePosition());
          const int64_t frameNumber = position.framePosition();
          const QImage frameCopy = frame;
          const QString diskCacheFramePath =
              currentComposition_
                  ? previewDiskCacheFramePathForNamespace(
                        currentCompositionDiskCacheNamespace(), frameNumber)
                  : QString();

          syncCurrentCompositionFrame(position);
          const auto publishFrame = [this, position, compositionId, frameNumber,
                                     frameCopy, diskCacheFramePath]() {
            storeFrameImageInRam(frameNumber, frameCopy);
            const bool savedToDisk =
                persistPreviewFrameToDisk(diskCacheFramePath, frameCopy);
            markFrameOnDisk(frameNumber, savedToDisk);
            ArtifactCore::globalEventBus().publish<FrameChangedEvent>(
                FrameChangedEvent{QString(compositionId),
                                  position.framePosition()});
            Q_EMIT owner_->frameChanged(position);
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
    FramePosition& position) { syncCurrentCompositionFrame(position); Q_EMIT
    owner_->frameChanged(position);
     }, Qt::DirectConnection);

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
  }

  ~Impl() {
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
      // Playback only needs the composition's current frame marker.
      // Propagating the frame into every layer is reserved for explicit
      // timeline edits / seeks, because that path is much heavier.
      composition->setFramePosition(position);
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
            composition->setFramePosition(FramePosition(latestFrame));
          }
        },
        Qt::QueuedConnection);
  }

  void emitRamPreviewStats() {
    Q_EMIT owner_->ramPreviewStatsChanged(ramPreviewHitRate(),
                                          ramPreviewCachedFrameCount());
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
  }

  void syncLegacyBitmapFromStates() {
    cacheBitmap_.assign(frameCacheStates_.size(), false);
    for (size_t i = 0; i < frameCacheStates_.size(); ++i) {
      const auto &state = frameCacheStates_[i];
      cacheBitmap_[i] = state.ready && state.inRam && !state.failed;
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
        if (!state.failed && !state.inRam) {
          state.ready = state.onDisk;
        }
      } else {
        state.onDisk = false;
        ramPreviewImageCache_.erase(frame);
        state.inRam = false;
        cacheBitmap_[static_cast<size_t>(frame)] = false;
      }
      if (!inRange && !state.ready) {
        state.failed = false;
        state.reason.clear();
      }
    }

    syncLegacyBitmapFromStates();
  }

  void markFrameReady(const int64_t frame) {
    if (frame < 0 || frame >= static_cast<int64_t>(frameCacheStates_.size())) {
      return;
    }

    auto &state = frameCacheStates_[static_cast<size_t>(frame)];
    state.requested = true;
    state.ready = true;
    state.failed = false;
    state.inRam = true;
    state.reason.clear();
    cacheBitmap_[static_cast<size_t>(frame)] = true;
  }

  void storeFrameImageInRam(const int64_t frame, const QImage &image) {
    if (frame < 0 || frame >= static_cast<int64_t>(frameCacheStates_.size()) ||
        image.isNull()) {
      return;
    }

    const QImage rgba =
        image.format() == QImage::Format_RGBA8888
            ? image
            : image.convertToFormat(QImage::Format_RGBA8888);
    ArtifactCore::ImageF32x4_RGBA cpuImage;
    cpuImage.setFromRGBA8(rgba.constBits(), rgba.width(), rgba.height());
    ramPreviewImageCache_[frame] =
        ArtifactCore::ImageF32x4RGBAWithCache(cpuImage);
    auto &state = frameCacheStates_[static_cast<size_t>(frame)];
    state.requested = true;
    state.ready = true;
    state.failed = false;
    state.inRam = true;
    cacheBitmap_[static_cast<size_t>(frame)] = true;
  }

  void markFrameRequested(const int64_t frame, const QString &reason = {}) {
    if (frame < 0 || frame >= static_cast<int64_t>(frameCacheStates_.size())) {
      return;
    }

    auto &state = frameCacheStates_[static_cast<size_t>(frame)];
    state.requested = true;
    if (state.failed && reason.trimmed().isEmpty()) {
      return;
    }
    if (!reason.trimmed().isEmpty()) {
      state.reason = reason.trimmed();
    }
  }

  void markFrameFailed(const int64_t frame, const QString &reason) {
    if (frame < 0 || frame >= static_cast<int64_t>(frameCacheStates_.size())) {
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
  }

  void markFrameOnDisk(const int64_t frame, const bool onDisk) {
    if (frame < 0 || frame >= static_cast<int64_t>(frameCacheStates_.size())) {
      return;
    }

    auto &state = frameCacheStates_[static_cast<size_t>(frame)];
    state.onDisk = onDisk;
    if (!state.failed && !state.inRam) {
      state.ready = onDisk;
    }
  }

  void clearFrameFailure(const int64_t frame) {
    if (frame < 0 || frame >= static_cast<int64_t>(frameCacheStates_.size())) {
      return;
    }

    auto &state = frameCacheStates_[static_cast<size_t>(frame)];
    state.failed = false;
    if (!state.ready) {
      state.reason.clear();
    }
  }

  bool hydrateFrameFromDisk(const int64_t frame) {
    if (frame < 0 || frame >= static_cast<int64_t>(frameCacheStates_.size())) {
      return false;
    }

    if (ramPreviewImageCache_.find(frame) != ramPreviewImageCache_.end()) {
      auto &state = frameCacheStates_[static_cast<size_t>(frame)];
      state.inRam = true;
      state.ready = true;
      cacheBitmap_[static_cast<size_t>(frame)] = true;
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

    storeFrameImageInRam(frame, image);
    markFrameOnDisk(frame, true);
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
    if (!ramPreviewEnabled_ || !engine_) {
      return;
    }

    const FrameRange range = clampedRamPreviewRange(position);
    ramPreviewRange_ = range;
    clearFrameRequestFlagsOutsideRange(ramPreviewRange_);
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
    for (const auto &state : frameCacheStates_) {
      if (state.ready && state.inRam && !state.failed) {
        ++hits;
      }
    }
    return static_cast<float>(hits) /
           static_cast<float>(frameCacheStates_.size());
  }

  int ramPreviewCachedFrameCount() const {
    return static_cast<int>(std::count_if(
        frameCacheStates_.begin(), frameCacheStates_.end(),
        [](const ArtifactRamPreviewFrameCacheState &state) {
          return state.ready && state.inRam && !state.failed;
        }));
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
      const auto &state = frameCacheStates_[static_cast<size_t>(frame)];
      if (state.ready && !state.failed) {
        ++ready;
      }
    }
    return ready;
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

  ArtifactRamPreviewFrameCacheState ramPreviewFrameState(
      const int64_t frame) const {
    if (frame < 0 || frame >= static_cast<int64_t>(frameCacheStates_.size())) {
      return {};
    }
    return frameCacheStates_[static_cast<size_t>(frame)];
  }

  ArtifactRamPreviewSummary ramPreviewSummary() const {
    ArtifactRamPreviewSummary summary;
    summary.enabled = ramPreviewEnabled_;
    summary.range = ramPreviewRange_;
    summary.requestedFrames = ramPreviewRequestedFrameCount();
    summary.readyFrames = ramPreviewReadyFrameCountInRange();
    summary.failedFrames = ramPreviewFailedFrameCountInRange();
    summary.inRamFrames = ramPreviewCachedFrameCount();
    summary.onDiskFrames = ramPreviewDiskFrameCountInRange();
    summary.hitRate = ramPreviewHitRate();
    return summary;
  }

  bool tryGetRamPreviewFrameImage(const int64_t frame,
                                  ArtifactCore::ImageF32x4_RGBA &outImage) const {
    const auto it = ramPreviewImageCache_.find(frame);
    if (it == ramPreviewImageCache_.end()) {
      return false;
    }
    outImage = it->second.image().DeepCopy();
    return !outImage.isEmpty();
  }

  std::vector<bool> ramPreviewCacheBitmap() const { return cacheBitmap_; }

  void prewarmRamPreviewRange(const FrameRange &range) {
    if (!ramPreviewEnabled_ || !engine_) {
      return;
    }

    ramPreviewRange_ = range;
    clearFrameRequestFlagsOutsideRange(ramPreviewRange_);
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
  impl_->startAudioClock();
  
  // 再生開始直前に最新の範囲を適用
  impl_->applyCurrentPlaybackFrameRangeToEngine();

  // 新しいエンジンを使用
  if (impl_->engine_) {
    impl_->engine_->play();
  }
  impl_->prewarmRamPreviewAround(currentFrame());
}

void ArtifactPlaybackService::pause() {
  impl_->pauseAudioClock();
  if (impl_->engine_) {
    impl_->engine_->pause();
  }
  /*
  if (impl_->controller_) {
      impl_->controller_->pause();
  }
  */
}

void ArtifactPlaybackService::stop() {
  impl_->stopAudioClock();
  if (impl_->engine_) {
    impl_->engine_->stop();
  }
  /*
  if (impl_->controller_) {
      impl_->controller_->stop();
  }
  */
}

void ArtifactPlaybackService::togglePlayPause() {
  if (impl_->engine_) {
    impl_->engine_->togglePlayPause();
  } else if (impl_->controller_) {
    impl_->controller_->togglePlayPause();
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
  if (impl_->engine_) {
    impl_->engine_->setCurrentFrame(position);
  } else if (impl_->controller_) {
    impl_->controller_->setCurrentFrame(position);
  }
  impl_->prewarmRamPreviewAround(position);
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
    impl_->clearFrameRequestFlagsOutsideRange(impl_->ramPreviewRange_);
    impl_->emitRamPreviewStats();

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
    Q_EMIT currentCompositionChanged(composition);
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
    Q_EMIT ramPreviewStateChanged(false, impl_->ramPreviewRange_);
    return;
  }

  prewarmRamPreviewAroundCurrentFrame();
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
    impl_->prewarmRamPreviewRange(range);
  } else {
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

  impl_->resetRamPreviewCache();
  Q_EMIT ramPreviewStateChanged(impl_->ramPreviewEnabled_,
                                impl_->ramPreviewRange_);
}

void ArtifactPlaybackService::prewarmRamPreviewAroundCurrentFrame() {
  if (!impl_) {
    return;
  }

  impl_->prewarmRamPreviewAround(currentFrame());
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

ArtifactRamPreviewSummary ArtifactPlaybackService::ramPreviewSummary() const {
  return impl_ ? impl_->ramPreviewSummary() : ArtifactRamPreviewSummary{};
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
