module;
#include <QThread>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QImage>
#include <QPainter>
#include <QDebug>
#include <wobjectimpl.h>
#include <QFont>

#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include "../../../out/build/x64-Debug/vcpkg_installed/x64-windows/include/Qt6/QtGui/QFont"

module Artifact.Playback.Engine;

import Frame.Position;
import Frame.Rate;
import Frame.Range;
import Artifact.Composition.Abstract;
import Artifact.Composition.InOutPoints;
import AudioRenderer;
import Audio.Segment;

namespace Artifact {

using namespace ArtifactCore;

W_OBJECT_IMPL(ArtifactPlaybackEngine)

/// 内部実装クラス
class ArtifactPlaybackEngine::Impl {
public:
    ArtifactPlaybackEngine* owner_;
    QThread* workerThread_;
    
    // 再生状態
    std::atomic<bool> playing_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> stopped_{true};
    
    // フレーム状態
    std::atomic<int64_t> currentFrame_{0};
    FrameRange frameRange_{FramePosition(0), FramePosition(299)};  // デフォルト 300 フレーム
    FrameRate frameRate_{30.0f};
    float playbackSpeed_{1.0f};
    std::atomic<bool> looping_{false};
    
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
    int audioSampleRate_ = 48000;
    float audioMasterVolume_ = 1.0f;
    bool audioMasterMuted_ = false;
    int64_t audioNextFrame_ = 0;
    size_t audioTargetBufferedFrames_ = 0;
    
    // タイミング
    QElapsedTimer elapsedTimer_;
    std::chrono::time_point<std::chrono::steady_clock> playbackStartTime_;
    int64_t playbackStartFrame_ = 0;
    std::atomic<int64_t> lastEmittedFrame_{-1};
    
    std::chrono::microseconds frameBudget_{0};
    int64_t droppedFrameCount_{0};
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
    }
    
    ~Impl() {
        stop();
        if (workerThread_->isRunning()) {
            workerThread_->quit();
            workerThread_->wait(3000);
        }
        delete workerThread_;
    }
    
    void start() {
        if (!workerThread_->isRunning()) {
            playing_ = true;
            paused_ = false;
            stopped_ = false;
            playbackStartTime_ = std::chrono::steady_clock::now();
            playbackStartFrame_ = currentFrame_.load();
            audioNextFrame_ = currentFrame_.load();
            workerThread_->start(QThread::TimeCriticalPriority);
        } else {
            paused_ = false;
            playing_ = true;
            stopped_ = false;
            playbackStartTime_ = std::chrono::steady_clock::now();
            playbackStartFrame_ = currentFrame_.load();
            audioNextFrame_ = currentFrame_.load();
            condition_.notify_one();
        }
    }
    
    void stop() {
        playing_ = false;
        paused_ = false;
        stopped_ = true;
        condition_.notify_one();
        audioNextFrame_ = currentFrame_.load();
        if (audioRenderer_) {
            audioRenderer_->stop();
            audioRenderer_->closeDevice();
        }
        if (workerThread_) {
            workerThread_->quit();
        }
    }
    
    void pause() {
        paused_ = true;
        playing_ = false;
    }
    
    /// メイン再生ループ（ワーカースレッドで実行）
    void runPlaybackLoop() {
        elapsedTimer_.start();
        lastFrameTime_ = std::chrono::steady_clock::now();
        
        const double fps = frameRate_.framerate();
        // インターバルはあくまでベース。実際には毎ループ時間をチェックする。
        const int64_t frameIntervalUs = static_cast<int64_t>(1000000.0 / (fps * std::abs(playbackSpeed_)));
        
        qDebug() << "[PlaybackEngine] Starting high-precision playback loop at" << (fps * std::abs(playbackSpeed_)) << "fps";
        
        while (playing_ || paused_) {
            if (paused_) {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this]() { return !paused_ || stopped_; });
                // 再開時にベース時間を再調整（現在のフレーム位置から再開）
                playbackStartTime_ = std::chrono::steady_clock::now();
                playbackStartFrame_ = currentFrame_.load();
                continue;
            }
            
            auto now = std::chrono::steady_clock::now();
            
            // 経過時間から現在の論理的なターゲットフレームを計算
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - playbackStartTime_);
            double elapsedSeconds = elapsed.count() / 1000000.0;
            
            // 重要：フレーム = 開始フレーム + (経過秒 * fps * 再生速度)
            int64_t frameOffset = static_cast<int64_t>(std::round(elapsedSeconds * fps * playbackSpeed_));
            int64_t targetFrame = playbackStartFrame_ + frameOffset;
            
            // ループ・範囲チェック
            FramePosition startPos = effectiveStartFrame();
            FramePosition endPos = effectiveEndFrame();
            int64_t totalFramesInRange = endPos.framePosition() - startPos.framePosition() + 1;
            
            if (totalFramesInRange > 0) {
                if (targetFrame > endPos.framePosition()) {
                    if (looping_) {
                        // ループ時はベース時間をリセットして、最初から回す
                        playbackStartTime_ = now;
                        playbackStartFrame_ = startPos.framePosition();
                        targetFrame = startPos.framePosition();
                    } else {
                        playing_ = false;
                        stopped_ = true;
                        QMetaObject::invokeMethod(owner_, [this]() {
                            Q_EMIT owner_->playbackStateChanged(PlaybackState::Stopped);
                        }, Qt::QueuedConnection);
                        break;
                    }
                } else if (targetFrame < startPos.framePosition()) {
                    if (looping_) {
                        playbackStartTime_ = now;
                        playbackStartFrame_ = endPos.framePosition();
                        targetFrame = endPos.framePosition();
                    } else {
                        targetFrame = startPos.framePosition();
                    }
                }
            }
            
            // フレームが更新された場合のみ描画と通知を行う
            if (targetFrame != lastEmittedFrame_) {
                currentFrame_ = targetFrame;
                updateFrame(targetFrame);
                lastEmittedFrame_ = targetFrame;
            }
            
            // オーディオパケットの供給
            updateAudio();
            
            // オーディオ同期
            if (audioClockProvider_) {
                syncWithAudioClock();
            }

            // CPU 負荷を抑えるための待機。
            // ターゲットインターバルの半分程度を上限にスリープ
            // [Optimization] 1ms sleep is too aggressive, making loop run 1000Hz.
            // Target is ~60fps (16.6ms). Let's sleep for 4ms to keep it responsive but less tight.
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }

        if (workerThread_) {
            workerThread_->quit();
        }
    }
    
    /// フレーム更新処理
    void updateFrame(int64_t targetFrame) {
        // コンポジションの状態更新
        if (composition_) {
            composition_->setFramePosition(FramePosition(targetFrame));
        }

        // フレームを描画（現在はダミー）
        QImage renderedFrame = renderFrame(FramePosition(targetFrame));
        
        // バッファに格納
        {
            QMutexLocker locker(&bufferMutex_);
            // renderedFrame already points to backBuffer_ due to optimization
            std::swap(frontBuffer_, backBuffer_);
        }
        
        // メインスレッドに通知
        // DirectConnection だとワーカースレッドで UI を触ってしまうため QueuedConnection を維持
        QMetaObject::invokeMethod(owner_, [this, pos = FramePosition(targetFrame), frame = frontBuffer_]() {
            Q_EMIT owner_->frameChanged(pos, frame);
        }, Qt::QueuedConnection);
    }
    
    /// フレーム描画
    QImage renderFrame(const FramePosition& position) {
        QSize sz(1280, 720); // Default preview size
        if (composition_) {
            auto compSz = composition_->settings().compositionSize();
            sz = QSize(compSz.width(), compSz.height());
        }

        // Re-use frontBuffer if size matches to avoid allocations
        if (backBuffer_.size() != sz || backBuffer_.format() != QImage::Format_ARGB32_Premultiplied) {
            backBuffer_ = QImage(sz, QImage::Format_ARGB32_Premultiplied);
        }
        
        backBuffer_.fill(QColor(32, 34, 38));
        
        QPainter painter(&backBuffer_);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Segoe UI", 24, QFont::Bold));
        painter.drawText(backBuffer_.rect(), Qt::AlignCenter, 
            QString("Frame %1").arg(position.framePosition()));
        painter.end();
        
        return backBuffer_;
    }
    
    /// オーディオ更新
    void updateAudio() {
        if (!composition_ || !audioRenderer_) return;

        if (!composition_->hasAudio()) {
            if (audioRenderer_->isActive()) {
                qDebug() << "[PlaybackEngine][Audio] composition has no audio. Stopping output.";
                audioRenderer_->stop();
                audioRenderer_->closeDevice();
            }
            return;
        }

        if (!audioRenderer_->isActive()) {
            if (!audioRenderer_->openDevice("")) {
                return;
            }
            audioSampleRate_ = std::max(1, audioRenderer_->sampleRate());
        }

        audioRenderer_->setMasterVolume(std::clamp(audioMasterVolume_, 0.0f, 2.0f) <= 0.0001f
            ? -144.0f
            : 20.0f * std::log10(std::clamp(audioMasterVolume_, 0.0f, 2.0f)));
        audioRenderer_->setMute(audioMasterMuted_);

        const double safeFrameRate = std::max<double>(1e-6, static_cast<double>(frameRate_.framerate()));
        int samplesPerFrame = static_cast<int>(std::round(static_cast<double>(audioSampleRate_) / safeFrameRate));
        if (samplesPerFrame <= 0) return;

        if (audioTargetBufferedFrames_ == 0) {
            audioTargetBufferedFrames_ = static_cast<size_t>(
                std::max<int>(samplesPerFrame * 8, audioSampleRate_ / 2));
        }
        const size_t audioStartBufferedFrames_ = static_cast<size_t>(
            std::max<int>(samplesPerFrame * 2, audioSampleRate_ / 8));

        const int64_t currentFrame = currentFrame_.load();
        if (audioNextFrame_ < currentFrame) {
            audioNextFrame_ = currentFrame;
        }

        bool audioExhausted = false;
        while (audioRenderer_->bufferedFrames() < audioTargetBufferedFrames_) {
            AudioSegment segment;
            if (!composition_->getAudio(segment, FramePosition(audioNextFrame_), samplesPerFrame, audioSampleRate_)) {
                audioExhausted = true;
                break;
            }
            audioRenderer_->enqueue(segment);
            ++audioNextFrame_;
        }

        if (!audioRenderer_->isActive()) {
            if (audioRenderer_->bufferedFrames() >= audioStartBufferedFrames_ ||
                (audioExhausted && audioRenderer_->bufferedFrames() > 0)) {
                audioRenderer_->start();
                if (!audioRenderer_->isActive()) {
                    return;
                }
            } else {
                return;
            }
        }
    }
    
    /// オーディオ同期
    void syncWithAudioClock() {
        if (!audioClockProvider_) return;
        if (composition_ && !composition_->hasAudio()) return;
        
        double audioTime = audioClockProvider_();
        if (audioTime <= 0.001) return;

        double currentEngineTime = static_cast<double>(currentFrame_.load()) / frameRate_.framerate();
        double diff = audioTime - currentEngineTime;
        
        // 50ms 以上ずれていたら「ベース時間」自体を書き換えて、滑らかに追従させる
        // これにより、毎ループ +1 フレームするのではなく、絶対時間へ収束させる
        if (std::abs(diff) > 0.05) {
            auto now = std::chrono::steady_clock::now();
            playbackStartTime_ = now;
            playbackStartFrame_ = static_cast<int64_t>(std::round(audioTime * frameRate_.framerate()));
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
    impl_->playbackSpeed_ = speed;
    Q_EMIT playbackSpeedChanged(speed);
}

float ArtifactPlaybackEngine::playbackSpeed() const {
    return impl_->playbackSpeed_;
}

void ArtifactPlaybackEngine::setLooping(bool loop) {
    impl_->looping_ = loop;
    Q_EMIT loopingChanged(loop);
}

bool ArtifactPlaybackEngine::isLooping() const {
    return impl_->looping_;
}

void ArtifactPlaybackEngine::play() {
    if (impl_->playing_) return;
    
    impl_->playing_ = true;
    impl_->paused_ = false;
    impl_->stopped_ = false;
    impl_->start();
    
    Q_EMIT playbackStateChanged(PlaybackState::Playing);
}

void ArtifactPlaybackEngine::pause() {
    if (!impl_->playing_) return;
    
    impl_->pause();
    Q_EMIT playbackStateChanged(PlaybackState::Paused);
}

void ArtifactPlaybackEngine::stop() {
    impl_->stop();
    impl_->currentFrame_ = impl_->effectiveStartFrame().framePosition();
    impl_->audioNextFrame_ = impl_->currentFrame_.load();
    Q_EMIT playbackStateChanged(PlaybackState::Stopped);
    Q_EMIT frameChanged(FramePosition(impl_->currentFrame_), QImage());
}

void ArtifactPlaybackEngine::togglePlayPause() {
    if (impl_->playing_) {
        pause();
    } else {
        play();
    }
}

void ArtifactPlaybackEngine::goToFrame(const FramePosition& position) {
    impl_->currentFrame_ = position.framePosition();
    impl_->audioNextFrame_ = impl_->currentFrame_.load();
    if (impl_->audioRenderer_) {
        impl_->audioRenderer_->clearBuffer();
    }
    Q_EMIT frameChanged(position, QImage());
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
    // TODO: マーカー実装時に実装
}

void ArtifactPlaybackEngine::goToPreviousMarker() {
    // TODO: マーカー実装時に実装
}

void ArtifactPlaybackEngine::goToNextChapter() {
    // TODO: チャプター実装時に実装
}

void ArtifactPlaybackEngine::goToPreviousChapter() {
    // TODO: チャプター実装時に実装
}

bool ArtifactPlaybackEngine::isPlaying() const {
    return impl_->playing_;
}

bool ArtifactPlaybackEngine::isPaused() const {
    return impl_->paused_;
}

bool ArtifactPlaybackEngine::isStopped() const {
    return impl_->stopped_;
}

FramePosition ArtifactPlaybackEngine::currentFrame() const {
    return FramePosition(impl_->currentFrame_.load());
}

void ArtifactPlaybackEngine::setCurrentFrame(const FramePosition& position) {
    impl_->currentFrame_ = position.framePosition();
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
    impl_->audioMasterVolume_ = std::clamp(volume, 0.0f, 2.0f);
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

void ArtifactPlaybackEngine::setComposition(ArtifactCompositionPtr composition) {
    impl_->composition_ = composition;
    impl_->audioTargetBufferedFrames_ = 0;
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
