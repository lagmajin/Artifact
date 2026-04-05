module;
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>
#include <wobjectimpl.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Composition.PlaybackController;




import Frame.Position;
import Frame.Rate;
import Frame.Range;
import Artifact.Composition.InOutPoints;

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

    // In/Out Points
    ArtifactInOutPoints* inOutPoints_ = nullptr;

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

    /// In/Out Points を考慮した有効なフレーム範囲を取得
    FramePosition effectiveStartFrame() const {
        FramePosition start = frameRange_.startPosition();
        
        // InPoint が設定されていれば、それとフレーム範囲の大きい方を採用
        if (inOutPoints_ && inOutPoints_->hasInPoint()) {
            auto inPoint = inOutPoints_->inPoint().value();
            start = std::max(start, inPoint);
        }
        
        return start;
    }

    /// In/Out Points を考慮した有効なフレーム範囲を取得
    FramePosition effectiveEndFrame() const {
        FramePosition end = frameRange_.endPosition();
        
        // OutPoint が設定されていれば、それとフレーム範囲の小さい方を採用
        if (inOutPoints_ && inOutPoints_->hasOutPoint()) {
            auto outPoint = inOutPoints_->outPoint().value();
            end = std::min(end, outPoint);
        }
        
        return end;
    }

    FramePosition nextFrame() const {
        // Compute next frame depending on realTime mode or simple increment
        if (realTime_ && audioClockProvider_) {
            // Use audio clock to derive next frame
            double seconds = audioClockProvider_();
            double fps = frameRate_.framerate() * playbackSpeed_;
            int64_t frameIndex = static_cast<int64_t>(seconds * fps);
            FramePosition next(frameIndex);

            // Apply effective in/out points
            FramePosition start = effectiveStartFrame();
            FramePosition end = effectiveEndFrame();

            if (next < start) {
                return start;
            }
            if (next > end) {
                if (looping_) {
                    return start;
                }
                return FramePosition(-1);
            }
            return next;
        }

        FramePosition next = currentFrame_ + 1;

        // Apply effective in/out points
        FramePosition start = effectiveStartFrame();
        FramePosition end = effectiveEndFrame();

        if (next > end) {
            if (looping_) return start;
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
    
    PlaybackState oldState = impl_->state_;
    impl_->state_ = PlaybackState::Playing;
    impl_->timer_->setInterval(impl_->calculateInterval());
    impl_->startTimer();
    
    qDebug() << "[PlaybackController] state transition:" << (int)oldState << "->" << (int)PlaybackState::Playing
             << "- interval" << impl_->calculateInterval();
    Q_EMIT playbackStateChanged(impl_->state_);
}

void ArtifactCompositionPlaybackController::pause() {
    if (impl_->state_ != PlaybackState::Playing) return;
    
    PlaybackState oldState = impl_->state_;
    impl_->state_ = PlaybackState::Paused;
    impl_->stopTimer();
    
    qDebug() << "[PlaybackController] state transition:" << (int)oldState << "->" << (int)PlaybackState::Paused;
    Q_EMIT playbackStateChanged(impl_->state_);
}

void ArtifactCompositionPlaybackController::stop() {
    if (impl_->state_ == PlaybackState::Stopped) return;
    
    PlaybackState oldState = impl_->state_;
    impl_->state_ = PlaybackState::Stopped;
    impl_->stopTimer();
    impl_->currentFrame_ = impl_->frameRange_.start();
    
    qDebug() << "[PlaybackController] state transition:" << (int)oldState << "->" << (int)PlaybackState::Stopped;
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

    FramePosition start = impl_->effectiveStartFrame();
    FramePosition end = impl_->effectiveEndFrame();

    if (prev < start) {
        if (impl_->looping_) {
            prev = end;
        } else {
            prev = start;
        }
    }

    goToFrame(prev);
}

void ArtifactCompositionPlaybackController::goToStartFrame() {
    goToFrame(impl_->effectiveStartFrame());
}

void ArtifactCompositionPlaybackController::goToEndFrame() {
    goToFrame(impl_->effectiveEndFrame());
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

void ArtifactCompositionPlaybackController::setInOutPoints(ArtifactInOutPoints* inOutPoints) {
    impl_->inOutPoints_ = inOutPoints;
    qDebug() << "PlaybackController::setInOutPoints -" << (inOutPoints ? "set" : "cleared");
}

ArtifactInOutPoints* ArtifactCompositionPlaybackController::inOutPoints() const {
    return impl_->inOutPoints_;
}

void ArtifactCompositionPlaybackController::goToNextMarker() {
    if (!impl_->inOutPoints_ || !impl_->currentFrame_.isValid()) {
        return;
    }
    
    auto nextMarkerPos = impl_->inOutPoints_->nextMarker(impl_->currentFrame_);
    if (nextMarkerPos.has_value()) {
        goToFrame(nextMarkerPos.value());
        qDebug() << "PlaybackController::goToNextMarker -" << nextMarkerPos.value().framePosition();
    }
}

void ArtifactCompositionPlaybackController::goToPreviousMarker() {
    if (!impl_->inOutPoints_ || !impl_->currentFrame_.isValid()) {
        return;
    }
    
    auto prevMarkerPos = impl_->inOutPoints_->previousMarker(impl_->currentFrame_);
    if (prevMarkerPos.has_value()) {
        goToFrame(prevMarkerPos.value());
        qDebug() << "PlaybackController::goToPreviousMarker -" << prevMarkerPos.value().framePosition();
    }
}

void ArtifactCompositionPlaybackController::goToNextChapter() {
    if (!impl_->inOutPoints_ || !impl_->currentFrame_.isValid()) {
        return;
    }
    
    auto nextChapterPos = impl_->inOutPoints_->nextChapter(impl_->currentFrame_);
    if (nextChapterPos.has_value()) {
        goToFrame(nextChapterPos.value());
        qDebug() << "PlaybackController::goToNextChapter -" << nextChapterPos.value().framePosition();
    }
}

void ArtifactCompositionPlaybackController::goToPreviousChapter() {
    if (!impl_->inOutPoints_ || !impl_->currentFrame_.isValid()) {
        return;
    }
    
    auto prevChapterPos = impl_->inOutPoints_->previousChapter(impl_->currentFrame_);
    if (prevChapterPos.has_value()) {
        goToFrame(prevChapterPos.value());
        qDebug() << "PlaybackController::goToPreviousChapter -" << prevChapterPos.value().framePosition();
    }
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
