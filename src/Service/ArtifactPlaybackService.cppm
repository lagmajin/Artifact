module;
#include <QElapsedTimer>
#include <QMetaObject>
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
  std::atomic_bool audioRunning_{false};
  std::function<double()> externalAudioClockProvider_;
  std::function<double()> playbackClockProvider_;
  float audioMasterVolume_ = 1.0f;
  bool audioMasterMuted_ = false;
  bool ramPreviewEnabled_ = true;
  int ramPreviewRadiusFrames_ = 48;
  FrameRange ramPreviewRange_{FramePosition(0), FramePosition(0)};
  std::vector<bool> cacheBitmap_;
  PlaybackRangeMode playbackRangeMode_ = PlaybackRangeMode::All;
  std::atomic<int64_t> pendingCompositionFrame_{0};
  std::atomic_bool compositionFrameSyncQueued_{false};

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
          
          // Mark as cached
          int64_t f = position.framePosition();
          if (f >= 0 && f < static_cast<int64_t>(cacheBitmap_.size())) {
              cacheBitmap_[f] = true;
          }

          syncCurrentCompositionFrame(position);
          // FrameCache disabled
          const auto publishFrame = [this, position, compositionId]() {
            ArtifactCore::globalEventBus().publish<FrameChangedEvent>(
                FrameChangedEvent{QString(compositionId),
                                  position.framePosition()});
            Q_EMIT owner_->frameChanged(position);
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
                       qDebug() << "[PlaybackService] Dropped frames:" << count;
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

  void emitRamPreviewStats() { Q_EMIT owner_->ramPreviewStatsChanged(0.0f, 0); }

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
    emitRamPreviewStats();
    Q_EMIT owner_->ramPreviewStateChanged(ramPreviewEnabled_, ramPreviewRange_);
  }

  // Accessors for ram preview statistics
  float ramPreviewHitRate() const {
    const auto &bitmap = cacheBitmap_;
    if (bitmap.empty()) return 0.0f;
    size_t hits = 0;
    for (bool b : bitmap) if (b) ++hits;
    return static_cast<float>(hits) / static_cast<float>(bitmap.size());
  }

  int ramPreviewCachedFrameCount() const {
    return static_cast<int>(std::count(cacheBitmap_.begin(), cacheBitmap_.end(), true));
  }

  std::vector<bool> ramPreviewCacheBitmap() const { return cacheBitmap_; }

  void prewarmRamPreviewRange(const FrameRange &range) {
    if (!ramPreviewEnabled_ || !engine_) {
      return;
    }

    ramPreviewRange_ = range;
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
  if (impl_->currentComposition_) {
    FrameRange range = impl_->currentComposition_->frameRange();
    if (mode == PlaybackRangeMode::WorkArea) {
      range = impl_->currentComposition_->workAreaRange();
    } else if (mode == PlaybackRangeMode::Selection) {
      // 選択範囲の実装は将来的に拡張
    }

    if (impl_->engine_) {
      impl_->engine_->setFrameRange(range);
    }
  }

  ArtifactCore::globalEventBus().publish<PlaybackRangeModeChangedEvent>(
      PlaybackRangeModeChangedEvent{mode});
  Q_EMIT playbackRangeModeChanged(mode);
}

PlaybackRangeMode ArtifactPlaybackService::playbackRangeMode() const {
  return impl_->playbackRangeMode_;
}

void ArtifactPlaybackService::play() {
  impl_->startAudioClock();
  
  // 再生開始直前に最新の範囲を適用
  if (impl_->currentComposition_) {
    FrameRange range = impl_->currentComposition_->frameRange();
    if (impl_->playbackRangeMode_ == PlaybackRangeMode::WorkArea) {
      range = impl_->currentComposition_->workAreaRange();
    }
    if (impl_->engine_) {
      impl_->engine_->setFrameRange(range);
    }
  }

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
        impl_->cacheBitmap_.assign(composition->frameRange().duration(), false);
    } else {
        impl_->cacheBitmap_.clear();
    }

    // エンジンにコンポジションの設定を反映
    if (impl_->engine_ && composition) {
      impl_->engine_->setFrameRange(composition->frameRange());
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
  // FrameCache disabled
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

ArtifactCompositionPlaybackController *
ArtifactPlaybackService::controller() const {
  return impl_->controller_;
}

} // namespace Artifact
