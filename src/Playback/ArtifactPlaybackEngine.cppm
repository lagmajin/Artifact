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
    
    // タイミング
    QElapsedTimer elapsedTimer_;
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
        // connect は別途 QObject::connect で記述
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
            QObject::connect(workerThread_, &QThread::started, [this]() { onThreadStarted(); });
            QObject::connect(workerThread_, &QThread::finished, [this]() { onThreadFinished(); });
            workerThread_->start(QThread::TimeCriticalPriority);
        } else {
            paused_ = false;
            playing_ = true;
            stopped_ = false;
            condition_.notify_one();
        }
    }
    
    void stop() {
        playing_ = false;
        paused_ = false;
        stopped_ = true;
        condition_.notify_one();
        if (audioRenderer_) {
            audioRenderer_->stop();
            audioRenderer_->closeDevice();
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
        
        const double fps = frameRate_.framerate() * std::abs(playbackSpeed_);
        const int64_t frameIntervalUs = static_cast<int64_t>(1000000.0 / fps);
        frameBudget_ = std::chrono::microseconds(frameIntervalUs);
        
        qDebug() << "[PlaybackEngine] Starting playback at" << fps << "fps,"
                 << "interval:" << frameIntervalUs << "us";
        
        while (playing_ || paused_) {
            if (paused_) {
                // 一時停止中は待機
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this]() { return !paused_ || stopped_; });
                continue;
            }
            
            auto loopStart = std::chrono::steady_clock::now();
            
            // フレーム更新
            updateFrame();
            
            // オーディオ更新
            updateAudio();
            
            // 経過時間計算
            auto loopEnd = std::chrono::steady_clock::now();
            auto loopDuration = std::chrono::duration_cast<std::chrono::microseconds>(loopEnd - loopStart).count();
            
            // ドロップフレーム検出
            if (loopDuration > frameIntervalUs * 1.5) {
                ++droppedFrameCount_;
                QMetaObject::invokeMethod(owner_, [this, count = droppedFrameCount_]() {
                    Q_EMIT owner_->droppedFrameDetected(count);
                }, Qt::QueuedConnection);
            }
            
            // 次のフレームまで待機
            int64_t waitTime = frameIntervalUs - loopDuration;
            if (waitTime > 0) {
                // 高精度待機
                QThread::usleep(static_cast<unsigned long>(waitTime));
            }
            
            // オーディオ同期（プロバイダーが設定されている場合）
            if (audioClockProvider_) {
                syncWithAudioClock();
            }
        }
    }
    
    /// フレーム更新処理
    void updateFrame() {
        // 次のフレームを計算
        int64_t nextFrame = currentFrame_.load();
        
        if (playbackSpeed_ >= 0) {
            nextFrame += static_cast<int64_t>(playbackSpeed_);
        } else {
            nextFrame -= static_cast<int64_t>(std::abs(playbackSpeed_));
        }
        
        // In/Out Points を考慮
        FramePosition startFrame = effectiveStartFrame();
        FramePosition endFrame = effectiveEndFrame();
        
        if (nextFrame > endFrame.framePosition()) {
            if (looping_) {
                nextFrame = startFrame.framePosition();
            } else {
                // 再生終了
                playing_ = false;
                stopped_ = true;
                QMetaObject::invokeMethod(owner_, [this]() {
                    Q_EMIT owner_->playbackStateChanged(false, false, true);
                }, Qt::QueuedConnection);
                return;
            }
        } else if (nextFrame < startFrame.framePosition()) {
            if (looping_) {
                nextFrame = endFrame.framePosition();
            } else {
                nextFrame = startFrame.framePosition();
            }
        }
        
        currentFrame_ = nextFrame;
        
        // フレームを描画
        QImage renderedFrame = renderFrame(FramePosition(nextFrame));
        
        // バッファに格納（スレッドセーフ）
        QMutexLocker locker(&bufferMutex_);
        backBuffer_ = renderedFrame;
        std::swap(frontBuffer_, backBuffer_);
        
        // メインスレッドにフレーム通知
        QMetaObject::invokeMethod(owner_, [this, pos = FramePosition(nextFrame), frame = frontBuffer_]() {
            Q_EMIT owner_->frameChanged(pos, frame);
        }, Qt::QueuedConnection);
    }
    
    /// フレーム描画
    QImage renderFrame(const FramePosition& position) {
        if (!composition_) {
            QImage blank(1920, 1080, QImage::Format_ARGB32_Premultiplied);
            blank.fill(Qt::black);
            return blank;
        }
        
        // コンポジションにフレーム位置を設定してレンダリング
        composition_->setFramePosition(position);
        
        // TODO: 実際のレンダリング処理
        // 現在はダミー画像を返す
        QImage frame(1920, 1080, QImage::Format_ARGB32_Premultiplied);
        frame.fill(QColor(32, 34, 38));
        
        QPainter painter(&frame);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Segoe UI", 24, QFont::Bold));
        painter.drawText(frame.rect(), Qt::AlignCenter, 
            QString("Frame %1").arg(position.framePosition()));
        
        return frame;
    }
    
    /// オーディオ更新
    void updateAudio() {
        if (!composition_ || !audioRenderer_) return;

        if (!composition_->hasAudio()) {
            if (audioRenderer_->isActive()) {
                audioRenderer_->stop();
                audioRenderer_->closeDevice();
            }
            return;
        }
        
        // デバイスのオープン確認
        if (!audioRenderer_->isActive()) {
            if (audioRenderer_->openDevice("")) {
                audioRenderer_->start();
            } else {
                return;
            }
        }

        audioRenderer_->setMasterVolume(std::clamp(audioMasterVolume_, 0.0f, 2.0f) <= 0.0001f
            ? -144.0f
            : 20.0f * std::log10(std::clamp(audioMasterVolume_, 0.0f, 2.0f)));
        audioRenderer_->setMute(audioMasterMuted_);
        
        // 1フレーム分のサンプル数を計算
        int samplesPerFrame = static_cast<int>(audioSampleRate_ / frameRate_.framerate());
        if (samplesPerFrame <= 0) return;
        
        AudioSegment segment;
        if (composition_->getAudio(segment, FramePosition(currentFrame_), samplesPerFrame, audioSampleRate_)) {
            audioRenderer_->enqueue(segment);
        }
    }
    
    /// オーディオ同期
    void syncWithAudioClock() {
        if (!audioClockProvider_) return;
        if (composition_ && !composition_->hasAudio()) return;
        
        double audioTime = audioClockProvider_();  // 秒

        // 【修正】オーディオ側の時間がまったく進んでいない（0.0付近のまま）場合は、
        //  開始直後や音声停止中のため、同期を行わないようスキップ
        if (audioTime <= 0.001) return;

        double expectedTime = static_cast<double>(currentFrame_.load()) / frameRate_.framerate();
        double diff = audioTime - expectedTime;
        
        // 100ms 以上ずれていたら補正
        if (std::abs(diff) > 0.1) {
            // qDebug() << "[PlaybackEngine] Audio sync: diff =" << diff << "s, adjusting frame";
            
            int64_t newFrame = static_cast<int64_t>(audioTime * frameRate_.framerate());
            currentFrame_ = std::clamp(newFrame, 
                effectiveStartFrame().framePosition(),
                effectiveEndFrame().framePosition());
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
    
    Q_EMIT playbackStateChanged(true, false, false);
}

void ArtifactPlaybackEngine::pause() {
    if (!impl_->playing_) return;
    
    impl_->pause();
    Q_EMIT playbackStateChanged(false, true, false);
}

void ArtifactPlaybackEngine::stop() {
    impl_->stop();
    impl_->currentFrame_ = impl_->effectiveStartFrame().framePosition();
    Q_EMIT playbackStateChanged(false, false, true);
    Q_EMIT frameChanged(FramePosition(impl_->currentFrame_), QImage());
}

void ArtifactPlaybackEngine::togglePlayPause() {
    if (impl_->playing_ || impl_->paused_) {
        pause();
    } else {
        play();
    }
}

void ArtifactPlaybackEngine::goToFrame(const FramePosition& position) {
    impl_->currentFrame_ = position.framePosition();
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
    if (composition) {
        impl_->frameRange_ = composition->frameRange();
        impl_->frameRate_ = composition->frameRate();
    }
}

ArtifactCompositionPtr ArtifactPlaybackEngine::composition() const {
    return impl_->composition_;
}

} // namespace Artifact

//#include "ArtifactPlaybackEngine.moc"
