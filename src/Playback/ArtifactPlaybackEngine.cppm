module;
#include <utility>
#include <atomic>
#include <cstdint>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <cmath>
#include <algorithm>
#include <limits>
#include <QThread>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QImage>
#include <QPainter>
#include <QDebug>
#include <wobjectimpl.h>
#include <QFont>
module Artifact.Playback.Engine;


import Frame.Position;
import Frame.Rate;
import Frame.Range;
import Frame.SkipTracker;
import Artifact.Composition.Abstract;
import Artifact.Composition.InOutPoints;
import Artifact.Widgets.SoftwareRenderInspectors;
import AudioRenderer;
import Audio.Segment;
import Playback.State;
import Core.Diagnostics.Trace;
import ArtifactCore.Utils.PerformanceProfiler;

namespace Artifact {

using namespace ArtifactCore;

namespace {

AudioSegment resampleAudioSegment(const AudioSegment& source,
                                  const int targetSampleRate) {
    if (targetSampleRate <= 0 || source.sampleRate <= 0 ||
        source.sampleRate == targetSampleRate || source.frameCount() <= 0) {
        return source;
    }

    const int sourceFrames = source.frameCount();
    const int targetFrames = std::max(
        1, static_cast<int>(std::llround(
               static_cast<double>(sourceFrames) * targetSampleRate /
               source.sampleRate)));
    AudioSegment result = source;
    result.sampleRate = targetSampleRate;
    for (int channel = 0; channel < source.channelCount(); ++channel) {
        const auto& input = source.channelData[channel];
        auto& output = result.channelData[channel];
        output.resize(targetFrames);
        const int inputFrames = std::min(
            sourceFrames, static_cast<int>(input.size()));
        if (inputFrames <= 0) {
            std::fill(output.begin(), output.end(), 0.0f);
            continue;
        }
        for (int frame = 0; frame < targetFrames; ++frame) {
            const double sourcePosition =
                static_cast<double>(frame) * source.sampleRate /
                targetSampleRate;
            const double clampedPosition = std::clamp(
                sourcePosition, 0.0, static_cast<double>(inputFrames - 1));
            const int first = static_cast<int>(std::floor(clampedPosition));
            const int second = std::min(inputFrames - 1, first + 1);
            const float fraction = static_cast<float>(clampedPosition - first);
            output[frame] = input[first] * (1.0f - fraction) +
                            input[second] * fraction;
        }
    }
    return result;
}

AudioSegment timeScaleAudioSegment(const AudioSegment& source,
                                   const float playbackSpeed) {
    if (source.frameCount() <= 0 || !std::isfinite(playbackSpeed) ||
        std::abs(playbackSpeed) <= 0.0001f) {
        return {};
    }
    const int sourceFrames = source.frameCount();
    const double requestedTargetFrames =
        static_cast<double>(sourceFrames) / std::abs(playbackSpeed);
    if (!std::isfinite(requestedTargetFrames) ||
        requestedTargetFrames > std::numeric_limits<int>::max()) {
        return {};
    }
    const int targetFrames = std::max(
        1, static_cast<int>(std::llround(requestedTargetFrames)));
    AudioSegment result = source;
    for (int channel = 0; channel < source.channelCount(); ++channel) {
        const auto& input = source.channelData[channel];
        auto& output = result.channelData[channel];
        output.resize(targetFrames);
        const int inputFrames = std::min(
            sourceFrames, static_cast<int>(input.size()));
        if (inputFrames <= 0) {
            std::fill(output.begin(), output.end(), 0.0f);
            continue;
        }
        for (int frame = 0; frame < targetFrames; ++frame) {
            const double normalized = targetFrames > 1
                ? static_cast<double>(frame) / (targetFrames - 1)
                : 0.0;
            const double sourcePosition = playbackSpeed > 0.0f
                ? normalized * (inputFrames - 1)
                : (1.0 - normalized) * (inputFrames - 1);
            const int first = std::clamp(static_cast<int>(std::floor(sourcePosition)),
                                         0, inputFrames - 1);
            const int second = std::min(inputFrames - 1, first + 1);
            const float fraction = static_cast<float>(sourcePosition - first);
            output[frame] = input[first] * (1.0f - fraction) +
                            input[second] * fraction;
        }
    }
    return result;
}

} // namespace

W_OBJECT_IMPL(ArtifactPlaybackEngine)

/// 内部実装クラス
class ArtifactPlaybackEngine::Impl {
public:
    ArtifactPlaybackEngine* owner_;
    QThread* workerThread_;
    
    // 再生状態
    std::atomic<PlaybackState> state_{PlaybackState::Stopped};
    
    // フレーム状態
    std::atomic<int64_t> currentFrame_{0};
    FrameRange frameRange_{FramePosition(0), FramePosition(299)};  // デフォルト 300 フレーム
    FrameRate frameRate_{30.0f};
    // Written by the UI/service thread and consumed by the playback worker.
    // The worker re-bases its timeline before applying a changed value.
    std::atomic<float> playbackSpeed_{1.0f};
    float appliedPlaybackSpeed_ = 1.0f;
    std::atomic<bool> looping_{false};
    std::atomic<bool> pingPong_{false};
    std::atomic<PlaybackSkipMode> skipMode_{PlaybackSkipMode::None};
    
    // In/Out Points
    ArtifactInOutPoints* inOutPoints_ = nullptr;
    
    // オーディオクロック
    std::function<double()> audioClockProvider_;
    
    // 同期プリミティブ
    std::mutex mutex_;
    std::condition_variable condition_;
    
    // フレームバッファ（ダブルバッファリング）
    QImage frontBuffer_;
    QImage backBuffer_;
    QMutex bufferMutex_;
    
    // オーディオ
    std::unique_ptr<AudioRenderer> audioRenderer_;
    QString audioOutputDeviceName_;
    int audioSampleRate_ = 48000;
    float audioMasterVolume_ = 1.0f;
    bool audioMasterMuted_ = false;
    // A non-realtime preview must not let audio run ahead of the sequential
    // frame transport. This is intentionally separate from the user's mute.
    bool audioMutedForSlowPreview_ = false;
    // PERF: setMasterVolume/setMute は std::pow 計算 + atomic store を伴うため、
    // 変化時のみ呼び出す。sentinel 値 (-999.0f) で初回の強制送信を保証。
    float audioLastSentVolume_ = -999.0f;
    bool audioLastSentMuted_ = false;
    int64_t audioNextFrame_ = 0;
    size_t audioTargetBufferedFrames_ = 0;
    double audioSampleAccumulator_ = 0.0;
    size_t audioOpenRetryCount_ = 0;
    size_t audioResyncClearCount_ = 0;
    size_t audioClockCorrectionCount_ = 0;
    size_t audioFormatMismatchCount_ = 0;
    std::atomic<bool> audioSeekPending_{true};
    bool audioExhausted_ = false;  // 音声データが尽きたフラグ（再シーク時にリセット）
    
    // タイミング
    QElapsedTimer elapsedTimer_;
    std::chrono::time_point<std::chrono::steady_clock> playbackStartTime_;
    int64_t playbackStartFrame_ = 0;
    std::atomic<int64_t> lastEmittedFrame_{-1};
    
    std::chrono::microseconds frameBudget_{0};
    int64_t droppedFrameCount_{0};
    int64_t lastDropWarningTotal_{0};
    std::chrono::time_point<std::chrono::steady_clock> lastFrameTime_;
    
    // コンポジション
    ArtifactCompositionPtr composition_;
    
    Impl(ArtifactPlaybackEngine* owner)
        : owner_(owner)
    {
        workerThread_ = new QThread();
        audioRenderer_ = std::make_unique<AudioRenderer>();
        QObject::connect(workerThread_, &QThread::started, [this]() { onThreadStarted(); });
        QObject::connect(workerThread_, &QThread::finished, [this]() { onThreadFinished(); });

        audioRenderer_->setLevelCallback([this](const AudioLevelData& levels) {
            QMetaObject::invokeMethod(owner_, [this, levels]() {
                Q_EMIT owner_->audioLevelChanged(levels.leftRms, levels.rightRms, levels.leftPeak, levels.rightPeak);
            }, Qt::QueuedConnection);
        });
    }
    
    ~Impl() {
        stop();
        // stop() requests the worker to quit; destruction is the only place that waits.
        if (workerThread_ && workerThread_->isRunning()) {
            workerThread_->wait(3000);
        }
        delete workerThread_;
    }
    
    void start() {
        qDebug() << "[PlaybackEngine] start requested"
                 << "workerRunning=" << (workerThread_ && workerThread_->isRunning())
                 << "currentFrame=" << currentFrame_.load()
                 << "audioNextFrame=" << audioNextFrame_;
        
        PlaybackState oldState = state_.load();
        state_ = PlaybackState::Playing;
        // The current frame has already been presented by seek/stop. Rebase
        // drop detection here so a new playback session never reports the
        // previous session's end-to-start jump as a decoder/render drop.
        lastEmittedFrame_.store(currentFrame_.load());
        qDebug() << "[PlaybackEngine] state transition:" << (int)oldState << "->" << (int)PlaybackState::Playing;

        if (!workerThread_->isRunning()) {
            playbackStartTime_ = std::chrono::steady_clock::now();
            playbackStartFrame_ = currentFrame_.load();
            audioNextFrame_ = currentFrame_.load();
            workerThread_->start(QThread::TimeCriticalPriority);
        } else {
            playbackStartTime_ = std::chrono::steady_clock::now();
            playbackStartFrame_ = currentFrame_.load();
            audioNextFrame_ = currentFrame_.load();
            condition_.notify_one();
        }
    }
    
    void stop() {
        qDebug() << "[PlaybackEngine] stop requested"
                 << "workerRunning=" << (workerThread_ && workerThread_->isRunning())
                 << "currentFrame=" << currentFrame_.load()
                 << "audioNextFrame=" << audioNextFrame_
                 << "audioResyncClears=" << audioResyncClearCount_
                 << "audioClockCorrections=" << audioClockCorrectionCount_;
        
        PlaybackState oldState = state_.load();
        state_ = PlaybackState::Stopped;
        qDebug() << "[PlaybackEngine] state transition:" << (int)oldState << "->" << (int)PlaybackState::Stopped;

        condition_.notify_one();
        audioNextFrame_ = currentFrame_.load();
        audioSeekPending_ = true;

        // Keep stop responsive: closing the backend device can block on some
        // drivers, so stop playback and drain our buffer without tearing down.
        if (audioRenderer_) {
            audioRenderer_->stop();
            audioRenderer_->clearBuffer();
        }
        audioTargetBufferedFrames_ = 0;

        if (workerThread_ && workerThread_->isRunning()) {
            // The finished handler restarts the worker if play() wins a rapid
            // stop -> play race while this quit request is still pending.
            workerThread_->quit();
        }
    }
    
    void pause() {
        PlaybackState oldState = state_.load();
        state_ = PlaybackState::Paused;
        qDebug() << "[PlaybackEngine] state transition:" << (int)oldState << "->" << (int)PlaybackState::Paused;

        playbackStartFrame_ = currentFrame_.load();
        playbackStartTime_ = std::chrono::steady_clock::now();
        // ポーズ時にaudioTargetBufferedFrames_をリセットして再開時の同期問題を防ぐ
        audioTargetBufferedFrames_ = 0;
    }

    void publishPlaybackSpeedChanged(const float speed) {
        QMetaObject::invokeMethod(owner_, [this, speed]() {
            Q_EMIT owner_->playbackSpeedChanged(speed);
        }, Qt::QueuedConnection);
    }

    void reportDroppedFrames(int64_t count, int64_t fromFrame,
                             int64_t toFrame) {
        if (count <= 0) {
            return;
        }

        droppedFrameCount_ += count;
        const int64_t total = droppedFrameCount_;
        QMetaObject::invokeMethod(owner_, [this, count]() {
            Q_EMIT owner_->droppedFrameDetected(count);
        }, Qt::QueuedConnection);

        // qWarning is collected by the automatic log sink. Keep it useful for
        // post-run AI diagnosis without producing one line per playback tick.
        if (lastDropWarningTotal_ == 0 ||
            total - lastDropWarningTotal_ >= 30) {
            lastDropWarningTotal_ = total;
            qWarning() << "[PlaybackEngine][RealtimePolicy] timeline frames dropped"
                       << "count=" << count
                       << "total=" << total
                       << "from=" << fromFrame
                       << "to=" << toFrame
                       << "policy=wall-clock/latest-frame/hold-last-good";
        }
    }
    
    /// メイン再生ループ（ワーカースレッドで実行）
    void runPlaybackLoop() {
        elapsedTimer_.start();
        lastFrameTime_ = std::chrono::steady_clock::now();
        
        const double fps = frameRate_.framerate();
        appliedPlaybackSpeed_ = playbackSpeed_.load();
        std::uint64_t loopIterations = 0;
        std::uint64_t emittedFrames = 0;
        QElapsedTimer progressTimer;
        progressTimer.start();
        int64_t lastProgressFrame = currentFrame_.load();
        auto nextSequentialFrameTime = std::chrono::steady_clock::now();
        int sequentialFramesOnTime = 0;
        
        qInfo() << "[PlaybackEngine][Loop] enter"
                << "state=" << static_cast<int>(state_.load())
                << "currentFrame=" << currentFrame_.load()
                << "lastEmitted=" << lastEmittedFrame_.load()
                << "range=" << effectiveStartFrame().framePosition()
                << "-" << effectiveEndFrame().framePosition()
                << "sourceFps=" << fps
                << "speed=" << appliedPlaybackSpeed_
                << "effectiveFps="
                << (fps * std::abs(appliedPlaybackSpeed_));
        
        while (state_ != PlaybackState::Stopped) {
            ++loopIterations;
            if (state_ == PlaybackState::Paused) {
                ArtifactCore::TraceLockScope traceLock(QStringLiteral("ArtifactPlaybackEngine::mutex_"));
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this]() { return state_ != PlaybackState::Paused; });
                if (state_ == PlaybackState::Stopped) break;
                // 再開時にベース時間を再調整（現在のフレーム位置から再開）
                playbackStartTime_ = std::chrono::steady_clock::now();
                playbackStartFrame_ = currentFrame_.load();
                nextSequentialFrameTime = std::chrono::steady_clock::now();
                sequentialFramesOnTime = 0;
                continue;
            }
            
            auto now = std::chrono::steady_clock::now();

            const float requestedSpeed = playbackSpeed_.load();
            if (requestedSpeed != appliedPlaybackSpeed_) {
                // Rebase at the displayed frame so a live speed change never
                // reinterprets the whole elapsed playback interval at a new rate.
                playbackStartFrame_ = currentFrame_.load();
                playbackStartTime_ = now;
                appliedPlaybackSpeed_ = requestedSpeed;
                audioSeekPending_ = true;
                nextSequentialFrameTime = now;
                sequentialFramesOnTime = 0;
            }

            // `None` means every composition frame, not merely no explicit
            // skip multiplier. Keep it on a sequential transport clock so a
            // slow GUI/render path cannot turn elapsed wall-clock time into a
            // jump over undispatched frames. Explicit skip modes retain the
            // realtime wall-clock policy below.
            const PlaybackSkipMode skipMode = skipMode_.load();
            const bool playEveryFrame = skipMode == PlaybackSkipMode::None;
            const double speedMagnitude = std::abs(appliedPlaybackSpeed_);
            if (playEveryFrame && speedMagnitude > 0.0001 && fps > 0.0) {
                const auto frameInterval = std::chrono::duration_cast<
                    std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>(1.0 / (fps * speedMagnitude)));
                if (now < nextSequentialFrameTime) {
                    std::this_thread::sleep_until(nextSequentialFrameTime);
                    continue;
                }

                const bool fellBehind =
                    now - nextSequentialFrameTime > frameInterval;
                if (fellBehind) {
                    sequentialFramesOnTime = 0;
                    audioMutedForSlowPreview_ = true;
                } else if (audioMutedForSlowPreview_ &&
                           ++sequentialFramesOnTime >= 6) {
                    // Do not unmute on one transient fast frame; a short
                    // stable interval indicates that realtime playback has
                    // recovered (for example after cache warm-up).
                    audioMutedForSlowPreview_ = false;
                }
                nextSequentialFrameTime += frameInterval;
            } else {
                audioMutedForSlowPreview_ = false;
                sequentialFramesOnTime = 0;
            }

            // 経過時間から現在の論理的なターゲットフレームを計算
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - playbackStartTime_);
            double elapsedSeconds = elapsed.count() / 1000000.0;
            int64_t frameOffset = 0;
            int64_t targetFrame = lastEmittedFrame_.load();

            // スキップモードに応じた補正
            int skipStep = 1;
            switch (skipMode) {
                case PlaybackSkipMode::Skip1: skipStep = 2; break;
                case PlaybackSkipMode::Skip3: skipStep = 4; break;
                default: skipStep = 1; break;
            }
            if (playEveryFrame) {
                targetFrame += appliedPlaybackSpeed_ >= 0.0f ? 1 : -1;
            } else {
                // Explicit skip modes remain realtime and may select the
                // latest eligible frame from the wall clock.
                const double rawFrameOffset =
                    elapsedSeconds * fps * appliedPlaybackSpeed_;
                frameOffset = rawFrameOffset >= 0.0
                    ? static_cast<int64_t>(std::floor(rawFrameOffset))
                    : static_cast<int64_t>(std::ceil(rawFrameOffset));
                targetFrame = playbackStartFrame_ + frameOffset;
                if (skipStep > 1) {
                    targetFrame = playbackStartFrame_ + (frameOffset / skipStep) * skipStep;
                }
            }

            // ループ・範囲チェック
            FramePosition startPos = effectiveStartFrame();
            FramePosition endPos = effectiveEndFrame();
            int64_t totalFramesInRange = endPos.framePosition() - startPos.framePosition() + 1;
            bool crossedPlaybackBoundary = false;
            
            if (totalFramesInRange > 0) {
                if (targetFrame > endPos.framePosition()) {
                    if (pingPong_) {
                        // Ping-Pong: reverse direction at boundary
                        appliedPlaybackSpeed_ = -appliedPlaybackSpeed_;
                        playbackSpeed_.store(appliedPlaybackSpeed_);
                        publishPlaybackSpeedChanged(appliedPlaybackSpeed_);
                        playbackStartTime_ = now;
                        playbackStartFrame_ = endPos.framePosition();
                        targetFrame = endPos.framePosition();
                        audioSeekPending_ = true;
                        crossedPlaybackBoundary = true;
                    } else if (looping_) {
                        // ループ時はベース時間をリセットして、最初から回す
                        playbackStartTime_ = now;
                        playbackStartFrame_ = startPos.framePosition();
                        targetFrame = startPos.framePosition();
                        audioSeekPending_ = true;
                        crossedPlaybackBoundary = true;
                    } else {
                        state_ = PlaybackState::Stopped;
                        QMetaObject::invokeMethod(owner_, [this]() {
                            Q_EMIT owner_->playbackStateChanged(PlaybackState::Stopped);
                        }, Qt::QueuedConnection);
                        break;
                    }
                } else if (targetFrame < startPos.framePosition()) {
                    if (pingPong_) {
                        appliedPlaybackSpeed_ = -appliedPlaybackSpeed_;
                        playbackSpeed_.store(appliedPlaybackSpeed_);
                        publishPlaybackSpeedChanged(appliedPlaybackSpeed_);
                        playbackStartTime_ = now;
                        playbackStartFrame_ = startPos.framePosition();
                        targetFrame = startPos.framePosition();
                        audioSeekPending_ = true;
                        crossedPlaybackBoundary = true;
                    } else if (looping_) {
                        playbackStartTime_ = now;
                        playbackStartFrame_ = endPos.framePosition();
                        targetFrame = endPos.framePosition();
                        crossedPlaybackBoundary = true;
                    } else {
                        targetFrame = startPos.framePosition();
                    }
                }
            }
            
            // フレームが更新された場合のみ描画と通知を行う
            bool emittedFrame = false;
            if (targetFrame != lastEmittedFrame_) {
                const int64_t previousFrame = lastEmittedFrame_.load();
                const bool normalRealtimeRate =
                    std::abs(appliedPlaybackSpeed_) > 0.0001f &&
                    std::abs(appliedPlaybackSpeed_) <= 1.0001f;
                if (!playEveryFrame && !crossedPlaybackBoundary && skipStep == 1 &&
                    normalRealtimeRate &&
                    previousFrame >= startPos.framePosition() &&
                    previousFrame <= endPos.framePosition()) {
                    const int64_t signedAdvance =
                        appliedPlaybackSpeed_ >= 0.0f
                            ? targetFrame - previousFrame
                            : previousFrame - targetFrame;
                    if (signedAdvance > 1) {
                        reportDroppedFrames(signedAdvance - 1, previousFrame,
                                            targetFrame);
                    }
                }
                currentFrame_ = targetFrame;
                updateFrame(targetFrame);
                lastEmittedFrame_ = targetFrame;
                emittedFrame = true;
                ++emittedFrames;
                lastProgressFrame = targetFrame;
                progressTimer.restart();
                if (emittedFrames == 1 || emittedFrames % 120 == 0) {
                    qDebug() << "[PlaybackEngine][Tick]"
                             << "targetFrame=" << targetFrame
                             << "previousFrame=" << previousFrame
                             << "elapsedSeconds=" << elapsedSeconds
                             << "speed=" << appliedPlaybackSpeed_
                             << "skipStep=" << skipStep
                             << "playEveryFrame=" << playEveryFrame
                             << "slowPreviewAudioMuted=" << audioMutedForSlowPreview_
                             << "event=EMIT"
                             << "emittedTotal=" << emittedFrames;
                }
            } else if (progressTimer.elapsed() >= 2000) {
                qWarning() << "[PlaybackEngine][Stall] no frame progress"
                           << "durationMs=" << progressTimer.elapsed()
                           << "frame=" << lastProgressFrame
                           << "targetFrame=" << targetFrame
                           << "state=" << static_cast<int>(state_.load())
                           << "speed=" << appliedPlaybackSpeed_
                           << "fps=" << fps
                           << "elapsedSeconds=" << elapsedSeconds
                           << "range=" << startPos.framePosition()
                           << "-" << endPos.framePosition();
                progressTimer.restart();
            }
            
            // オーディオパケットの供給
            updateAudio();
            
            // オーディオ同期
            if (audioClockProvider_ && audioRenderer_ && audioRenderer_->isActive()) {
                syncWithAudioClock();
            }

            // 音声バッファが十分あるときだけ軽く待機する。
            // 充填が追いついていない間は sleep を飛ばして先読みを優先する。
            // PERF: バッファ半分以下の場合は 500us の最小スリープを挿入し、
            // 完全な tight spin を防いで UI スレッドの応答性を維持する。
            // 48kHz/30fps の場合 1 フレーム = 33ms なので 0.5ms は許容範囲。
            if (!emittedFrame) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else if (audioRenderer_) {
                const size_t buffered = audioRenderer_->bufferedFrames();
                const size_t halfTarget = audioTargetBufferedFrames_ / 2;
                if (buffered >= audioTargetBufferedFrames_) {
                    const size_t surplus = buffered - audioTargetBufferedFrames_;
                    const int samplesPerMs = audioSampleRate_ / 1000;
                    const int maxSleepMs = 2;
                    const int sleepMs = std::min(maxSleepMs, std::max(0, static_cast<int>(surplus / std::max(1, samplesPerMs))));
                    if (sleepMs > 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
                    } else {
                        std::this_thread::yield();
                    }
                } else if (buffered >= halfTarget) {
                    std::this_thread::yield();
                } else {
                    // バッファ半分以下 — 最小限の待機で CPU 独占を防止しつつ先読みを優先
                    std::this_thread::sleep_for(std::chrono::microseconds(500));
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        // Cleanup after exiting the playback loop（stop() で既に処理済み）
        if (audioRenderer_ && !audioRenderer_->isActive()) {
            audioRenderer_->clearBuffer();
        }
        qInfo() << "[PlaybackEngine][Loop] exit"
                << "state=" << static_cast<int>(state_.load())
                << "finalFrame=" << currentFrame_.load()
                << "lastEmitted=" << lastEmittedFrame_.load()
                << "iterations=" << loopIterations
                << "emitted=" << emittedFrames
                << "droppedTotal=" << droppedFrameCount_;
        // workerThread_ は stop() で既に quit() されているため不要
    }
    
    /// フレーム更新処理
    void updateFrame(int64_t targetFrame) {
        FrameSkipTracker::instance()->beginDispatch(targetFrame);
        // PlaybackService owns composition-frame sync and viewport rendering.
        // Do not render a QImage here: playback ticks must stay lightweight.
        // A sequential preview needs backpressure at the composition-sync
        // boundary. The service executes its frame publication inline on the
        // GUI thread, so the worker cannot enqueue a newer frame until this
        // frame has been applied to the composition. Explicit skip modes
        // retain asynchronous realtime dispatch.
        const Qt::ConnectionType dispatchType =
            skipMode_.load() == PlaybackSkipMode::None
                ? Qt::BlockingQueuedConnection
                : Qt::QueuedConnection;
        QMetaObject::invokeMethod(owner_, [this, pos = FramePosition(targetFrame)]() {
            Q_EMIT owner_->frameChanged(pos, QImage());
        }, dispatchType);
    }

    /// フレーム描画
    QImage renderFrame(const FramePosition& position) {
        QSize sz(1280, 720); // Default preview size
        if (composition_) {
            auto compSz = composition_->settings().compositionSize();
            sz = QSize(compSz.width(), compSz.height());
        }

        FramePosition previousPosition(0);
        const bool restorePosition = composition_ &&
                                     composition_->framePosition() != position;
        if (restorePosition) {
            previousPosition = composition_->framePosition();
            composition_->setFramePosition(position);
        }

        if (composition_) {
            QImage preview = generateCompositionThumbnail(composition_, sz);
            if (!preview.isNull()) {
                if (preview.size() != sz) {
                    preview = preview.scaled(sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                }
                if (restorePosition) {
                    composition_->setFramePosition(previousPosition);
                }
                return preview;
            }
        }

        if (backBuffer_.size() != sz ||
            backBuffer_.format() != QImage::Format_ARGB32_Premultiplied) {
            backBuffer_ = QImage(sz, QImage::Format_ARGB32_Premultiplied);
        }

        backBuffer_.fill(QColor(24, 26, 30));

        QPainter painter(&backBuffer_);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QColor(229, 231, 235));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 20, QFont::Bold));
        painter.drawText(backBuffer_.rect(), Qt::AlignCenter,
                         QStringLiteral("Preview unavailable"));
        if (restorePosition) {
            composition_->setFramePosition(previousPosition);
        }
        return backBuffer_;
    }
    
    /// オーディオ更新
    void updateAudio() {
        // 停止状態なら何もしない（競合防止）
        if (state_.load() == PlaybackState::Stopped) return;
        if (!composition_ || !audioRenderer_) return;

        if (!std::isfinite(appliedPlaybackSpeed_) ||
            std::abs(appliedPlaybackSpeed_) <= 0.0001f) {
            if (audioRenderer_->isActive() || audioRenderer_->bufferedFrames() > 0) {
                audioRenderer_->stop();
                audioRenderer_->clearBuffer();
            }
            audioTargetBufferedFrames_ = 0;
            audioSeekPending_ = true;
            return;
        }

        const auto fillStart = std::chrono::high_resolution_clock::now();

        if (!composition_->hasAudio()) {
            if (audioRenderer_->isActive() || audioRenderer_->bufferedFrames() > 0) {
                qDebug() << "[PlaybackEngine][Audio] composition has no audio. Stopping output.";
                audioRenderer_->stop();
                audioRenderer_->clearBuffer();
            }
            if (audioRenderer_->isDeviceOpen()) {
                audioRenderer_->closeDevice();
            }
            audioTargetBufferedFrames_ = 0;
            audioSampleAccumulator_ = 0.0;
            audioSeekPending_ = true;
            audioExhausted_ = false;
            return;
        }

        if (!audioRenderer_->isDeviceOpen()) {
            ++audioOpenRetryCount_;
            if (audioOpenRetryCount_ <= 12 || (audioOpenRetryCount_ % 50) == 0) {
                qWarning() << "[PlaybackEngine][Audio] renderer device not open during preroll"
                           << "openRetry=" << audioOpenRetryCount_
                           << "bufferedFrames=" << audioRenderer_->bufferedFrames()
                           << "targetBufferedFrames=" << audioTargetBufferedFrames_;
            }
            const double probeFrameRate = static_cast<double>(frameRate_.framerate());
            const double probeSampleCount =
                static_cast<double>(audioSampleRate_) / probeFrameRate;
            constexpr double maxSamplesPerFrame = static_cast<double>(
                std::numeric_limits<int>::max() / 16);
            if (!std::isfinite(probeFrameRate) || probeFrameRate <= 0.0 ||
                !std::isfinite(probeSampleCount) || probeSampleCount < 1.0 ||
                probeSampleCount > maxSamplesPerFrame) {
                qWarning() << "[PlaybackEngine][Audio] invalid frame rate for audio probe"
                           << "frameRate=" << probeFrameRate;
                return;
            }
            const int probeFrames = std::max(
                1, static_cast<int>(std::round(probeSampleCount)));
            AudioSegment probeSegment;
            if (composition_->getAudio(
                    probeSegment, FramePosition(audioNextFrame_), probeFrames,
                    audioSampleRate_)) {
                const int sourceChannels = probeSegment.channelCount();
                audioRenderer_->setPreferredChannelCount(
                    sourceChannels >= 8 ? 8 : sourceChannels >= 6 ? 6 : 2);
            }
            if (!audioRenderer_->openDevice(audioOutputDeviceName_)) {
                return;
            }
            const int previousSampleRate = audioSampleRate_;
            audioSampleRate_ = std::max(1, audioRenderer_->sampleRate());
            if (audioSampleRate_ != previousSampleRate) {
                // Buffer targets are expressed in frames but represent a
                // time budget derived from the active device sample rate.
                audioTargetBufferedFrames_ = 0;
            }
        }

        const float targetDb = std::clamp(audioMasterVolume_, 0.0f, 2.0f) <= 0.0001f
            ? -144.0f
            : 20.0f * std::log10(std::clamp(audioMasterVolume_, 0.0f, 2.0f));
        if (targetDb != audioLastSentVolume_) {
            audioRenderer_->setMasterVolume(targetDb);
            audioLastSentVolume_ = targetDb;
        }
        const bool effectiveAudioMuted =
            audioMasterMuted_ || audioMutedForSlowPreview_;
        if (effectiveAudioMuted != audioLastSentMuted_) {
            audioRenderer_->setMute(effectiveAudioMuted);
            audioLastSentMuted_ = effectiveAudioMuted;
        }

        const double safeFrameRate = static_cast<double>(frameRate_.framerate());
        const double exactSamplesPerFrame =
            static_cast<double>(audioSampleRate_) / safeFrameRate;
        constexpr double maxSamplesPerFrame = static_cast<double>(
            std::numeric_limits<int>::max() / 16);
        if (!std::isfinite(safeFrameRate) || safeFrameRate <= 0.0 ||
            !std::isfinite(exactSamplesPerFrame) ||
            exactSamplesPerFrame < 1.0 ||
            exactSamplesPerFrame > maxSamplesPerFrame) {
            qWarning() << "[PlaybackEngine][Audio] invalid frame rate for audio fill"
                       << "frameRate=" << safeFrameRate;
            return;
        }
        const int samplesPerFrame = static_cast<int>(
            std::round(exactSamplesPerFrame));
        if (samplesPerFrame <= 0) return;

        // Exact (fractional) samples-per-frame used by the accumulator to eliminate
        // integer rounding drift at non-integer frame rates (e.g. 29.97, 23.976 fps).
        if (audioTargetBufferedFrames_ == 0) {
            // 先読みは16フレーム分程度に抑える（48フレームは重すぎる）
            audioTargetBufferedFrames_ = static_cast<size_t>(
                std::max<int>(samplesPerFrame * 16, audioSampleRate_));
        }
        const size_t audioStartBufferedFrames_ = static_cast<size_t>(
            std::max<int>(samplesPerFrame * 8, audioSampleRate_ / 2));

        const int64_t currentFrame = currentFrame_.load();
        if (audioSeekPending_.exchange(false)) {
            ++audioResyncClearCount_;
            if (audioResyncClearCount_ <= 4 || (audioResyncClearCount_ % 200) == 0) {
                qWarning() << "[PlaybackEngine][Audio] resetting audio stream state"
                           << "count=" << audioResyncClearCount_
                           << "audioNextFrame(before)=" << audioNextFrame_
                           << "currentFrame=" << currentFrame
                           << "bufferedFrames=" << audioRenderer_->bufferedFrames();
            }
            audioRenderer_->clearBuffer();
            audioNextFrame_ = currentFrame;
            audioSampleAccumulator_ = 0.0;
            audioExhausted_ = false;  // シーク時にリセット
        }

        // 既に音声が尽きているなら再フィルしない（末尾での毎ループ CPU 消費を防止）
        if (audioExhausted_) {
            return;
        }

        bool audioExhausted = false;
        while (audioRenderer_->bufferedFrames() < audioTargetBufferedFrames_) {
            // 停止信号をチェック（競合防止）
            if (state_.load() == PlaybackState::Stopped) return;
            audioSampleAccumulator_ += exactSamplesPerFrame;
            int samplesThisFrame = static_cast<int>(audioSampleAccumulator_);
            audioSampleAccumulator_ -= static_cast<double>(samplesThisFrame);
            if (samplesThisFrame <= 0) samplesThisFrame = 1;

            AudioSegment segment;
            if (!composition_->getAudio(segment, FramePosition(audioNextFrame_), samplesThisFrame, audioSampleRate_)) {
                audioExhausted = true;
                qWarning() << "[PlaybackEngine][Audio] composition getAudio exhausted"
                           << "requestFrame=" << audioNextFrame_
                           << "samplesThisFrame=" << samplesThisFrame
                           << "bufferedFrames=" << audioRenderer_->bufferedFrames();
                break;
            }
            const int rendererSampleRate = audioRenderer_->sampleRate();
            const int rendererChannels = audioRenderer_->channelCount();
            if ((rendererSampleRate > 0 && segment.sampleRate != rendererSampleRate) ||
                (rendererChannels > 0 && segment.channelCount() != rendererChannels)) {
                ++audioFormatMismatchCount_;
                if (audioFormatMismatchCount_ <= 4) {
                    qWarning() << "[PlaybackEngine][Audio] format mismatch"
                               << "sourceSampleRate=" << segment.sampleRate
                               << "rendererSampleRate=" << rendererSampleRate
                               << "sourceChannels=" << segment.channelCount()
                               << "rendererChannels=" << rendererChannels;
                }
            }
            const AudioSegment speedAdjustedSegment =
                std::abs(appliedPlaybackSpeed_ - 1.0f) > 0.0001f
                    ? timeScaleAudioSegment(segment, appliedPlaybackSpeed_)
                    : segment;
            const AudioSegment enqueueSegment =
                rendererSampleRate > 0 && segment.sampleRate != rendererSampleRate
                    ? resampleAudioSegment(speedAdjustedSegment, rendererSampleRate)
                    : speedAdjustedSegment;
            if (!audioRenderer_->enqueue(enqueueSegment)) {
                qWarning() << "[PlaybackEngine][Audio] renderer rejected segment"
                           << "requestFrame=" << audioNextFrame_
                           << "samplesThisFrame=" << samplesThisFrame
                           << "bufferedFrames=" << audioRenderer_->bufferedFrames();
                break;
            }
            ++audioNextFrame_;
        }

        // オーディオ尽きた場合、残りをサイレンスで1回だけ埋めてフラグを立てる
        if (audioExhausted) {
            audioExhausted_ = true;
            const size_t buffered = audioRenderer_->bufferedFrames();
            if (buffered > 0 && buffered < audioTargetBufferedFrames_) {
                const size_t silenceFrames = audioTargetBufferedFrames_ - buffered;
                const int channels = std::max(1, audioRenderer_->channelCount());
                AudioSegment silence;
                silence.sampleRate = audioRenderer_->sampleRate() > 0
                    ? audioRenderer_->sampleRate()
                    : audioSampleRate_;
                silence.channelData.resize(channels);
                for (int ch = 0; ch < channels; ++ch) {
                    silence.channelData[ch].resize(silenceFrames, 0.0f);
                }
                audioRenderer_->enqueue(silence);
            }
        }

        if (!audioRenderer_->isActive()) {
            if (audioRenderer_->bufferedFrames() >= audioStartBufferedFrames_ ||
                (audioExhausted && audioRenderer_->bufferedFrames() > 0)) {
                audioRenderer_->start();
                if (!audioRenderer_->isActive()) {
                    ArtifactCore::AudioEngineProfiler::instance().recordFillLoop(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::high_resolution_clock::now() - fillStart).count());
                    return;
                }
                audioOpenRetryCount_ = 0;
            } else {
                ArtifactCore::AudioEngineProfiler::instance().recordFillLoop(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::high_resolution_clock::now() - fillStart).count());
                return;
            }
        } else if (audioRenderer_->bufferedFrames() < static_cast<size_t>(samplesPerFrame * 2)) {
            static size_t lowBufWarnCount = 0;
            if (++lowBufWarnCount <= 4 || (lowBufWarnCount % 100) == 0) {
                qWarning() << "[PlaybackEngine][Audio] low buffered frames during playback"
                           << "bufferedFrames=" << audioRenderer_->bufferedFrames()
                           << "samplesPerFrame=" << samplesPerFrame
                           << "targetBufferedFrames=" << audioTargetBufferedFrames_;
            }
        }

        // Record fill-loop timing and buffer level for the profiler
        const std::int64_t fillNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now() - fillStart).count();
        ArtifactCore::AudioEngineProfiler::instance().recordFillLoop(fillNs);

        if (audioTargetBufferedFrames_ > 0) {
            const double pct = static_cast<double>(audioRenderer_->bufferedFrames()) /
                               static_cast<double>(audioTargetBufferedFrames_) * 100.0;
            ArtifactCore::AudioEngineProfiler::instance().setBufferLevel(pct);
        }
    }
    
    /// オーディオ同期
    void syncWithAudioClock() {
        std::function<double()> provider;
        {
            // ロックなしで provider を読み取り（停止信号は audioClockProvider_ = {} で送信）
            provider = audioClockProvider_;
        }
        if (!provider) return;
        if (std::abs(appliedPlaybackSpeed_ - 1.0f) > 0.0001f) return;
        if (state_.load() != PlaybackState::Playing) return;
        if (!audioRenderer_ || !audioRenderer_->isActive()) return;
        if (composition_ && !composition_->hasAudio()) return;
        
        double audioTime = provider();
        if (!std::isfinite(audioTime) || audioTime <= 0.001) return;

        const double safeFrameRate = static_cast<double>(frameRate_.framerate());
        if (!std::isfinite(safeFrameRate) || safeFrameRate <= 0.0) {
            qWarning() << "[PlaybackEngine][AudioClock] invalid frame rate"
                       << "frameRate=" << safeFrameRate;
            return;
        }
        const int64_t currentFrame = currentFrame_.load();
        const int64_t endFrame = effectiveEndFrame().framePosition();
        if (endFrame > 0 && currentFrame >= endFrame - 2) {
            return;
        }

        double currentEngineTime = static_cast<double>(currentFrame) / safeFrameRate;
        double diff = audioTime - currentEngineTime;
        if (!std::isfinite(currentEngineTime) || !std::isfinite(diff)) return;

        // Small drift (1–33 ms): shift playbackStartFrame_ gradually so the engine
        // timeline converges to audio time without audible jumps.
        // Large drift (>33 ms): snap to audio time immediately.
        if (std::abs(diff) > 0.001) {
            if (std::abs(diff) > 0.033) {
                ++audioClockCorrectionCount_;
                qWarning() << "[PlaybackEngine][AudioClock] hard-correcting engine timeline"
                           << "count=" << audioClockCorrectionCount_
                           << "audioTime=" << audioTime
                           << "engineTime=" << currentEngineTime
                           << "diff=" << diff
                           << "currentFrame=" << currentFrame;
                auto now = std::chrono::steady_clock::now();
                playbackStartTime_ = now;
                const double targetFrame = audioTime * safeFrameRate;
                if (std::isfinite(targetFrame) &&
                    std::abs(targetFrame) <= static_cast<double>(
                        std::numeric_limits<int64_t>::max())) {
                    playbackStartFrame_ = static_cast<int64_t>(std::round(targetFrame));
                }
            } else {
                // Gradual correction: shift start frame by a fraction of the drift
                // to converge over ~10 refresh cycles (≈100 ms).
                constexpr double correctionGain = 0.1;
                const double shiftValue = diff * safeFrameRate * correctionGain;
                if (std::isfinite(shiftValue) &&
                    std::abs(shiftValue) <= static_cast<double>(
                        std::numeric_limits<int64_t>::max())) {
                    const int64_t shift = static_cast<int64_t>(std::round(shiftValue));
                    if (shift != 0) {
                        playbackStartFrame_ += shift;
                    }
                }
            }
        }
    }
    
    /// In/Out Points を考慮した開始フレーム
    FramePosition effectiveStartFrame() const {
        FramePosition start = frameRange_.startPosition();
        
        if (inOutPoints_ && inOutPoints_->hasInPoint()) {
            auto inPoint = inOutPoints_->inPoint().value();
            start = std::max(start, inPoint);
        }
        
        return start;
    }
    
    /// In/Out Points を考慮した終了フレーム
    FramePosition effectiveEndFrame() const {
        FramePosition end = frameRange_.endPosition();
        
        if (inOutPoints_ && inOutPoints_->hasOutPoint()) {
            auto outPoint = inOutPoints_->outPoint().value();
            end = std::min(end, outPoint);
        }
        
        return end;
    }
    
public slots:
    void onThreadStarted() {
        qDebug() << "[PlaybackEngine] Worker thread started";
        runPlaybackLoop();
    }
    
    void onThreadFinished() {
        qDebug() << "[PlaybackEngine] Worker thread finished";
        // A rapid stop -> play can set Playing while the previous worker is
        // finishing. Restart after QThread has fully transitioned to stopped.
        QMetaObject::invokeMethod(owner_, [this]() {
            if (state_.load() == PlaybackState::Playing &&
                workerThread_ && !workerThread_->isRunning()) {
                playbackStartTime_ = std::chrono::steady_clock::now();
                playbackStartFrame_ = currentFrame_.load();
                audioNextFrame_ = currentFrame_.load();
                workerThread_->start(QThread::TimeCriticalPriority);
            }
        }, Qt::QueuedConnection);
    }
};

// ============================================================================
// ArtifactPlaybackEngine Implementation
// ============================================================================

ArtifactPlaybackEngine::ArtifactPlaybackEngine(QObject* parent)
    : QObject(parent), impl_(new Impl(this))
{
}

ArtifactPlaybackEngine::~ArtifactPlaybackEngine() {
    delete impl_;
}

void ArtifactPlaybackEngine::setFrameRate(const FrameRate& rate) {
    impl_->frameRate_ = rate;
    Q_EMIT frameRangeChanged(impl_->frameRange_);
}

FrameRate ArtifactPlaybackEngine::frameRate() const {
    return impl_->frameRate_;
}

void ArtifactPlaybackEngine::setFrameRange(const FrameRange& range) {
    impl_->frameRange_ = range;
    Q_EMIT frameRangeChanged(range);
}

FrameRange ArtifactPlaybackEngine::frameRange() const {
    return impl_->frameRange_;
}

void ArtifactPlaybackEngine::setPlaybackSpeed(float speed) {
    const float safeSpeed = std::isfinite(speed)
        ? std::clamp(speed, -8.0f, 8.0f)
        : 1.0f;
    impl_->playbackSpeed_.store(safeSpeed);
    Q_EMIT playbackSpeedChanged(safeSpeed);
}

float ArtifactPlaybackEngine::playbackSpeed() const {
    return impl_->playbackSpeed_.load();
}

void ArtifactPlaybackEngine::setLooping(bool loop) {
    impl_->looping_ = loop;
    Q_EMIT loopingChanged(loop);
}

bool ArtifactPlaybackEngine::isLooping() const {
    return impl_->looping_;
}

void ArtifactPlaybackEngine::setPingPong(bool enabled) {
    impl_->pingPong_ = enabled;
}

bool ArtifactPlaybackEngine::isPingPong() const {
    return impl_->pingPong_;
}

void ArtifactPlaybackEngine::setPlaybackSkipMode(PlaybackSkipMode mode) {
    impl_->skipMode_ = mode;
}

PlaybackSkipMode ArtifactPlaybackEngine::playbackSkipMode() const {
    return impl_->skipMode_.load();
}

void ArtifactPlaybackEngine::play() {
    if (impl_->state_ == PlaybackState::Playing) return;
    
    impl_->start();
    
    Q_EMIT playbackStateChanged(PlaybackState::Playing);
}

void ArtifactPlaybackEngine::pause() {
    if (impl_->state_ != PlaybackState::Playing) return;
    
    impl_->pause();
    Q_EMIT playbackStateChanged(PlaybackState::Paused);
}

void ArtifactPlaybackEngine::stop() {
    impl_->stop();
    impl_->currentFrame_ = impl_->effectiveStartFrame().framePosition();
    impl_->audioNextFrame_ = impl_->currentFrame_.load();
    Q_EMIT playbackStateChanged(PlaybackState::Stopped);
    const FramePosition position(impl_->currentFrame_.load());
    Q_EMIT frameChanged(position, QImage());
}

void ArtifactPlaybackEngine::waitForStop() {
    if (impl_->workerThread_ && impl_->workerThread_->isRunning()) {
        impl_->workerThread_->wait(3000);
    }
}

void ArtifactPlaybackEngine::togglePlayPause() {
    if (impl_->state_ == PlaybackState::Playing) {
        pause();
    } else {
        play();
    }
}

void ArtifactPlaybackEngine::goToFrame(const FramePosition& position) {
    const int64_t targetFrame = position.framePosition();
    FrameSkipTracker::instance()->beginDispatch(targetFrame);

    impl_->currentFrame_ = targetFrame;
    impl_->playbackStartFrame_ = impl_->currentFrame_.load();
    impl_->playbackStartTime_ = std::chrono::steady_clock::now();
    impl_->lastEmittedFrame_ = std::numeric_limits<int64_t>::min();
    impl_->audioNextFrame_ = impl_->currentFrame_.load();
    impl_->audioSeekPending_ = true;
    impl_->audioTargetBufferedFrames_ = 0;
    impl_->audioExhausted_ = false;
    if (impl_->audioRenderer_) {
        impl_->audioRenderer_->clearBuffer();
    }

    QImage preview = renderPreviewFrame(position);
    if (preview.isNull()) {
        FrameSkipTracker::instance()->recordSkip(
            targetFrame, impl_->currentFrame_.load(),
            FrameSkipReason::TooHeavy,
            QStringLiteral("renderPreviewFrame returned null"));
    }
    Q_EMIT frameChanged(position, preview);
}

void ArtifactPlaybackEngine::goToNextFrame() {
    int64_t next = impl_->currentFrame_.load() + 1;
    FramePosition end = impl_->effectiveEndFrame();
    if (next <= end.framePosition()) {
        goToFrame(FramePosition(next));
    }
}

void ArtifactPlaybackEngine::goToPreviousFrame() {
    int64_t prev = impl_->currentFrame_.load() - 1;
    FramePosition start = impl_->effectiveStartFrame();
    if (prev >= start.framePosition()) {
        goToFrame(FramePosition(prev));
    }
}

void ArtifactPlaybackEngine::goToStartFrame() {
    goToFrame(impl_->effectiveStartFrame());
}

void ArtifactPlaybackEngine::goToEndFrame() {
    goToFrame(impl_->effectiveEndFrame());
}

void ArtifactPlaybackEngine::goToNextMarker() {
    if (!impl_->inOutPoints_) {
        return;
    }
    const auto next = impl_->inOutPoints_->nextMarker(currentFrame());
    if (next) {
        goToFrame(*next);
    }
}

void ArtifactPlaybackEngine::goToPreviousMarker() {
    if (!impl_->inOutPoints_) {
        return;
    }
    const auto prev = impl_->inOutPoints_->previousMarker(currentFrame());
    if (prev) {
        goToFrame(*prev);
    }
}

void ArtifactPlaybackEngine::goToNextChapter() {
    if (!impl_->inOutPoints_) {
        return;
    }
    const auto next = impl_->inOutPoints_->nextChapter(currentFrame());
    if (next) {
        goToFrame(*next);
    }
}

void ArtifactPlaybackEngine::goToPreviousChapter() {
    if (!impl_->inOutPoints_) {
        return;
    }
    const auto prev = impl_->inOutPoints_->previousChapter(currentFrame());
    if (prev) {
        goToFrame(*prev);
    }
}

bool ArtifactPlaybackEngine::isPlaying() const {
    return impl_->state_ == PlaybackState::Playing;
}

bool ArtifactPlaybackEngine::isPaused() const {
    return impl_->state_ == PlaybackState::Paused;
}

bool ArtifactPlaybackEngine::isStopped() const {
    return impl_->state_ == PlaybackState::Stopped;
}

FramePosition ArtifactPlaybackEngine::currentFrame() const {
    return FramePosition(impl_->currentFrame_.load());
}

void ArtifactPlaybackEngine::setCurrentFrame(const FramePosition& position) {
    impl_->currentFrame_ = position.framePosition();
}

QImage ArtifactPlaybackEngine::renderPreviewFrame(const FramePosition& position) {
    if (!impl_) {
        return QImage();
    }
    return impl_->renderFrame(position);
}

void ArtifactPlaybackEngine::setInOutPoints(ArtifactInOutPoints* inOutPoints) {
    impl_->inOutPoints_ = inOutPoints;
}

ArtifactInOutPoints* ArtifactPlaybackEngine::inOutPoints() const {
    return impl_->inOutPoints_;
}

void ArtifactPlaybackEngine::setAudioClockProvider(const std::function<double()>& provider) {
    impl_->audioClockProvider_ = provider;
}

void ArtifactPlaybackEngine::setAudioMasterVolume(float volume) {
    impl_->audioMasterVolume_ = std::isfinite(volume)
        ? std::clamp(volume, 0.0f, 2.0f)
        : 1.0f;
}

float ArtifactPlaybackEngine::audioMasterVolume() const {
    return impl_->audioMasterVolume_;
}

void ArtifactPlaybackEngine::setAudioMasterMuted(bool muted) {
    impl_->audioMasterMuted_ = muted;
}

bool ArtifactPlaybackEngine::audioMasterMuted() const {
    return impl_->audioMasterMuted_;
}

ArtifactPlaybackAudioDiagnostics ArtifactPlaybackEngine::audioDiagnostics() const {
    ArtifactPlaybackAudioDiagnostics diagnostics;
    if (impl_->audioRenderer_) {
        diagnostics.deviceOpen = impl_->audioRenderer_->isDeviceOpen();
        diagnostics.bufferedFrames = impl_->audioRenderer_->bufferedFrames();
    }
    diagnostics.targetBufferedFrames = impl_->audioTargetBufferedFrames_;
    diagnostics.openRetryCount = impl_->audioOpenRetryCount_;
    diagnostics.resyncClearCount = impl_->audioResyncClearCount_;
    diagnostics.clockCorrectionCount = impl_->audioClockCorrectionCount_;
    if (impl_->audioRenderer_) {
        diagnostics.underflowCount = impl_->audioRenderer_->underflowCount();
        diagnostics.overflowCount = impl_->audioRenderer_->overflowCount();
        diagnostics.channelCount = impl_->audioRenderer_->channelCount();
    }
    diagnostics.sampleRate = impl_->audioSampleRate_;
    diagnostics.formatMismatchCount = impl_->audioFormatMismatchCount_;
    return diagnostics;
}

void ArtifactPlaybackEngine::setAudioOutputDeviceName(const QString& deviceName) {
    const QString normalizedName = deviceName.trimmed();
    if (impl_->audioOutputDeviceName_ == normalizedName) {
        return;
    }
    impl_->audioOutputDeviceName_ = normalizedName;
    if (impl_->audioRenderer_ && impl_->audioRenderer_->isDeviceOpen()) {
        impl_->audioRenderer_->stop();
        impl_->audioRenderer_->closeDevice();
        impl_->audioRenderer_->clearBuffer();
        impl_->audioTargetBufferedFrames_ = 0;
        impl_->audioSeekPending_ = true;
    }
}

QString ArtifactPlaybackEngine::audioOutputDeviceName() const {
    return impl_->audioOutputDeviceName_;
}

void ArtifactPlaybackEngine::setComposition(ArtifactCompositionPtr composition) {
    impl_->composition_ = composition;
    impl_->audioTargetBufferedFrames_ = 0;
    impl_->audioSampleAccumulator_ = 0.0;
    impl_->audioSeekPending_ = true;
    impl_->audioExhausted_ = false;
    impl_->audioFormatMismatchCount_ = 0;
    impl_->audioLastSentVolume_ = -999.0f;
    impl_->audioLastSentMuted_ = false;
    if (impl_->audioRenderer_) {
        impl_->audioRenderer_->clearBuffer();
    }
    if (composition) {
        impl_->frameRange_ = composition->frameRange();
        impl_->frameRate_ = composition->frameRate();
        impl_->audioNextFrame_ = impl_->currentFrame_.load();
    }
}

ArtifactCompositionPtr ArtifactPlaybackEngine::composition() const {
    return impl_->composition_;
}

} // namespace Artifact

//#include "ArtifactPlaybackEngine.moc"
