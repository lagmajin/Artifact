module;
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>
#include <wobjectimpl.h>

module Artifact.Composition.PlaybackController;

import std;
import Frame.Position;
import Frame.Rate;
import Frame.Range;

namespace Artifact {

using namespace ArtifactCore;

W_OBJECT_IMPL(ArtifactCompositionPlaybackController)

class ArtifactCompositionPlaybackController::Impl {
public:
    PlaybackState state_ = PlaybackState::Stopped;
    FramePosition currentFrame_;
    FrameRange frameRange_;
    FrameRate frameRate_ = FrameRate(30.0f);
    float playbackSpeed_ = 1.0f;
    bool looping_ = false;
    bool realTime_ = true;
    
    QTimer* timer_ = nullptr;
    QElapsedTimer elapsedTimer_;
    qint64 lastFrameTime_ = 0;
    std::function<double()> audioClockProvider_ = nullptr;
    
    Impl() {
        timer_ = new QTimer();
        timer_->setTimerType(Qt::PreciseTimer);
    }
    
    ~Impl() {
        delete timer_;
    }
    
    void startTimer() {
        if (!timer_->isActive()) {
            elapsedTimer_.start();
            lastFrameTime_ = 0;
            timer_->start();
        }
    }
    
    void stopTimer() {
        if (timer_->isActive()) {
            timer_->stop();
        }
    }
    
    int calculateInterval() const {
        // フレームレートと再生速度からタイマー間隔を計算
        double fps = frameRate_.framerate() * playbackSpeed_;
        return static_cast<int>(1000.0 / fps);
    }
    
    FramePosition nextFrame() const {
        // Compute next frame depending on realTime mode or simple increment
        if (realTime_ && audioClockProvider_) {
            // Use audio clock to derive next frame
            double seconds = audioClockProvider_();
            double fps = frameRate_.framerate() * playbackSpeed_;
            int64_t frameIndex = static_cast<int64_t>(seconds * fps);
            FramePosition next(frameIndex);

            if (next >= frameRange_.endPosition()) {
                if (looping_) {
                    return frameRange_.startPosition();
                }
                return FramePosition(-1);
            }
            return next;
        }

        FramePosition next = currentFrame_ + 1;
        if (next >= frameRange_.endPosition()) {
            if (looping_) return frameRange_.startPosition();
            return FramePosition(-1);
        }
        return next;
    }
};

ArtifactCompositionPlaybackController::ArtifactCompositionPlaybackController(QObject* parent)
    : QObject(parent), impl_(new Impl()) {
    
    // タイマーのタイムアウトを接続
    connect(impl_->timer_, &QTimer::timeout, this, &ArtifactCompositionPlaybackController::onTimerTick);
}

ArtifactCompositionPlaybackController::~ArtifactCompositionPlaybackController() {
    delete impl_;
}

void ArtifactCompositionPlaybackController::play() {
    if (impl_->state_ == PlaybackState::Playing) return;
    
    impl_->state_ = PlaybackState::Playing;
    impl_->timer_->setInterval(impl_->calculateInterval());
    impl_->startTimer();
    
    qDebug() << "PlaybackController::play - interval" << impl_->calculateInterval();
    Q_EMIT playbackStateChanged(impl_->state_);
}

void ArtifactCompositionPlaybackController::pause() {
    if (impl_->state_ != PlaybackState::Playing) return;
    
    impl_->state_ = PlaybackState::Paused;
    impl_->stopTimer();
    
    Q_EMIT playbackStateChanged(impl_->state_);
}

void ArtifactCompositionPlaybackController::stop() {
    if (impl_->state_ == PlaybackState::Stopped) return;
    
    impl_->state_ = PlaybackState::Stopped;
    impl_->stopTimer();
    impl_->currentFrame_ = impl_->frameRange_.start();
    
    qDebug() << "PlaybackController::stop";
    Q_EMIT playbackStateChanged(impl_->state_);
    Q_EMIT frameChanged(impl_->currentFrame_);
}

void ArtifactCompositionPlaybackController::togglePlayPause() {
    if (impl_->state_ == PlaybackState::Playing) {
        pause();
    } else {
        play();
    }
}

void ArtifactCompositionPlaybackController::goToFrame(const FramePosition& position) {
    impl_->currentFrame_ = position;
    qDebug() << "PlaybackController::goToFrame" << impl_->currentFrame_.framePosition();
    Q_EMIT frameChanged(impl_->currentFrame_);
}

void ArtifactCompositionPlaybackController::goToNextFrame() {
    FramePosition next = impl_->nextFrame();
    // FramePosition supports isValid() to check
    if (next.isValid()) {
        goToFrame(next);
    }
}

void ArtifactCompositionPlaybackController::goToPreviousFrame() {
    FramePosition prev = impl_->currentFrame_;
    prev = prev - 1;

    if (prev < impl_->frameRange_.startPosition()) {
        if (impl_->looping_) {
            prev = impl_->frameRange_.endPosition() - 1;
        } else {
            prev = impl_->frameRange_.startPosition();
        }
    }

    goToFrame(prev);
}

void ArtifactCompositionPlaybackController::goToStartFrame() {
    goToFrame(impl_->frameRange_.startPosition());
}

void ArtifactCompositionPlaybackController::goToEndFrame() {
    goToFrame(impl_->frameRange_.endPosition());
}

bool ArtifactCompositionPlaybackController::isPlaying() const {
    return impl_->state_ == PlaybackState::Playing;
}

bool ArtifactCompositionPlaybackController::isPaused() const {
    return impl_->state_ == PlaybackState::Paused;
}

bool ArtifactCompositionPlaybackController::isStopped() const {
    return impl_->state_ == PlaybackState::Stopped;
}

PlaybackState ArtifactCompositionPlaybackController::state() const {
    return impl_->state_;
}

FramePosition ArtifactCompositionPlaybackController::currentFrame() const {
    return impl_->currentFrame_;
}

void ArtifactCompositionPlaybackController::setCurrentFrame(const FramePosition& position) {
    impl_->currentFrame_ = position;
    Q_EMIT frameChanged(impl_->currentFrame_);
}

FrameRange ArtifactCompositionPlaybackController::frameRange() const {
    return impl_->frameRange_;
}

void ArtifactCompositionPlaybackController::setFrameRange(const FrameRange& range) {
    impl_->frameRange_ = range;
    Q_EMIT frameRangeChanged(impl_->frameRange_);
}

FrameRate ArtifactCompositionPlaybackController::frameRate() const {
    return impl_->frameRate_;
}

void ArtifactCompositionPlaybackController::setFrameRate(const FrameRate& rate) {
    impl_->frameRate_ = rate;
    if (impl_->state_ == PlaybackState::Playing) {
        impl_->timer_->setInterval(impl_->calculateInterval());
    }
    Q_EMIT playbackSpeedChanged(impl_->playbackSpeed_);
}

float ArtifactCompositionPlaybackController::playbackSpeed() const {
    return impl_->playbackSpeed_;
}

void ArtifactCompositionPlaybackController::setPlaybackSpeed(float speed) {
    impl_->playbackSpeed_ = qBound(0.1f, speed, 10.0f);
    if (impl_->state_ == PlaybackState::Playing) {
        impl_->timer_->setInterval(impl_->calculateInterval());
    }
    Q_EMIT playbackSpeedChanged(impl_->playbackSpeed_);
}

bool ArtifactCompositionPlaybackController::isLooping() const {
    return impl_->looping_;
}

void ArtifactCompositionPlaybackController::setLooping(bool loop) {
    impl_->looping_ = loop;
    Q_EMIT loopingChanged(impl_->looping_);
}

bool ArtifactCompositionPlaybackController::isRealTime() const {
    return impl_->realTime_;
}

void ArtifactCompositionPlaybackController::setRealTime(bool realTime) {
    impl_->realTime_ = realTime;
}

void ArtifactCompositionPlaybackController::setAudioClockProvider(const std::function<double()>& provider) {
    impl_->audioClockProvider_ = provider;
}

void ArtifactCompositionPlaybackController::onTimerTick() {
    if (impl_->state_ != PlaybackState::Playing) return;
    
    FramePosition next = impl_->nextFrame();
    if (!next.isValid()) {
        // ループなしで終了
        qDebug() << "PlaybackController::onTimerTick - end of range, stopping";
        stop();
        return;
    }
    
    impl_->currentFrame_ = next;
    qDebug() << "PlaybackController::onTimerTick - frame" << impl_->currentFrame_.framePosition();
    Q_EMIT frameChanged(impl_->currentFrame_);
}

}
