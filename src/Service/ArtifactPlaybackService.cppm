module;
#include <QTimer>
#include <QElapsedTimer>
#include <wobjectimpl.h>

module Artifact.Service.Playback;

import std;
import Frame.Position;
import Frame.Rate;
import Frame.Range;
import Artifact.Composition.PlaybackController;
import Artifact.Composition.Abstract;

namespace Artifact {

using namespace ArtifactCore;

class ArtifactPlaybackService; // forward declaration to use pointer in Impl

W_OBJECT_IMPL(ArtifactPlaybackService)

class ArtifactPlaybackService::Impl {
public:
    ArtifactPlaybackService* owner_ = nullptr;
    ArtifactCompositionPlaybackController* controller_ = nullptr;
    ArtifactCompositionPtr currentComposition_;
    QElapsedTimer audioTimer_;
    double audioOffsetSeconds_ = 0.0;
    bool audioRunning_ = false;
    std::function<double()> externalAudioClockProvider_;

    explicit Impl(ArtifactPlaybackService* owner)
        : owner_(owner) {
        controller_ = new ArtifactCompositionPlaybackController();

        // コントローラーのシグナルをサービスに転送（owner に直接接続）
        QObject::connect(controller_, &ArtifactCompositionPlaybackController::playbackStateChanged,
                         owner_, &ArtifactPlaybackService::playbackStateChanged,
                         Qt::DirectConnection);

        QObject::connect(controller_, &ArtifactCompositionPlaybackController::frameChanged,
                         owner_, [this](const FramePosition& position) {
            // コンポジションのフレーム位置も更新（同期で即時評価）
            if (currentComposition_) {
                currentComposition_->setFramePosition(position);
            }
            Q_EMIT owner_->frameChanged(position);
        }, Qt::DirectConnection);

        QObject::connect(controller_, &ArtifactCompositionPlaybackController::playbackSpeedChanged,
                         owner_, &ArtifactPlaybackService::playbackSpeedChanged,
                         Qt::DirectConnection);

        QObject::connect(controller_, &ArtifactCompositionPlaybackController::loopingChanged,
                         owner_, &ArtifactPlaybackService::loopingChanged,
                         Qt::DirectConnection);

        QObject::connect(controller_, &ArtifactCompositionPlaybackController::frameRangeChanged,
                         owner_, &ArtifactPlaybackService::frameRangeChanged,
                         Qt::DirectConnection);

        // Provide an audio clock provider backed by a local elapsed timer.
        controller_->setAudioClockProvider([this]() -> double {
            // If an external provider is set by other modules, prefer it.
            if (externalAudioClockProvider_) {
                return externalAudioClockProvider_();
            }

            double seconds = audioOffsetSeconds_;
            if (audioRunning_) {
                seconds += static_cast<double>(audioTimer_.elapsed()) / 1000.0;
            }
            return seconds;
        });
    }

    ~Impl() {
        delete controller_;
    }
    void startAudioClock() {
        if (!audioRunning_) {
            audioTimer_.start();
            audioRunning_ = true;
        }
    }
    void pauseAudioClock() {
        if (audioRunning_) {
            audioOffsetSeconds_ += static_cast<double>(audioTimer_.elapsed()) / 1000.0;
            audioRunning_ = false;
        }
    }
    void stopAudioClock() {
        audioOffsetSeconds_ = 0.0;
        audioRunning_ = false;
    }
    void setExternalAudioClockProvider(const std::function<double()>& provider) {
        externalAudioClockProvider_ = provider;
    }
};

ArtifactPlaybackService::ArtifactPlaybackService(QObject* parent)
    : QObject(parent), impl_(new Impl(this)) {
}

ArtifactPlaybackService::~ArtifactPlaybackService() {
    delete impl_;
}

ArtifactPlaybackService* ArtifactPlaybackService::instance() {
    static ArtifactPlaybackService service;
    return &service;
}

void ArtifactPlaybackService::play() {
    if (impl_->controller_) {
        impl_->controller_->play();
    }
}

void ArtifactPlaybackService::pause() {
    if (impl_->controller_) {
        impl_->controller_->pause();
    }
}

void ArtifactPlaybackService::stop() {
    if (impl_->controller_) {
        impl_->controller_->stop();
    }
}

void ArtifactPlaybackService::togglePlayPause() {
    if (impl_->controller_) {
        impl_->controller_->togglePlayPause();
    }
}

void ArtifactPlaybackService::goToFrame(const FramePosition& position) {
    if (impl_->controller_) {
        impl_->controller_->goToFrame(position);
    }
}

void ArtifactPlaybackService::goToNextFrame() {
    if (impl_->controller_) {
        impl_->controller_->goToNextFrame();
    }
}

void ArtifactPlaybackService::goToPreviousFrame() {
    if (impl_->controller_) {
        impl_->controller_->goToPreviousFrame();
    }
}

void ArtifactPlaybackService::goToStartFrame() {
    if (impl_->controller_) {
        impl_->controller_->goToStartFrame();
    }
}

void ArtifactPlaybackService::goToEndFrame() {
    if (impl_->controller_) {
        impl_->controller_->goToEndFrame();
    }
}

bool ArtifactPlaybackService::isPlaying() const {
    return impl_ && impl_->controller_ ? impl_->controller_->isPlaying() : false;
}

bool ArtifactPlaybackService::isPaused() const {
    return impl_->controller_ ? impl_->controller_->isPaused() : false;
}

bool ArtifactPlaybackService::isStopped() const {
    return impl_->controller_ ? impl_->controller_->isStopped() : false;
}

PlaybackState ArtifactPlaybackService::state() const {
    return impl_->controller_ ? impl_->controller_->state() : PlaybackState::Stopped;
}

FramePosition ArtifactPlaybackService::currentFrame() const {
    return impl_->controller_ ? impl_->controller_->currentFrame() : FramePosition(0);
}

void ArtifactPlaybackService::setCurrentFrame(const FramePosition& position) {
    if (impl_->controller_) {
        impl_->controller_->setCurrentFrame(position);
    }
}

FrameRange ArtifactPlaybackService::frameRange() const {
    return impl_->controller_ ? impl_->controller_->frameRange() : FrameRange(FramePosition(0), FramePosition(100));
}

void ArtifactPlaybackService::setFrameRange(const FrameRange& range) {
    if (impl_->controller_) {
        impl_->controller_->setFrameRange(range);
    }
}

FrameRate ArtifactPlaybackService::frameRate() const {
    return impl_->controller_ ? impl_->controller_->frameRate() : FrameRate(30.0f);
}

void ArtifactPlaybackService::setFrameRate(const FrameRate& rate) {
    if (impl_->controller_) {
        impl_->controller_->setFrameRate(rate);
    }
}

float ArtifactPlaybackService::playbackSpeed() const {
    return impl_->controller_ ? impl_->controller_->playbackSpeed() : 1.0f;
}

void ArtifactPlaybackService::setPlaybackSpeed(float speed) {
    if (impl_->controller_) {
        impl_->controller_->setPlaybackSpeed(speed);
    }
}

bool ArtifactPlaybackService::isLooping() const {
    return impl_->controller_ ? impl_->controller_->isLooping() : false;
}

void ArtifactPlaybackService::setLooping(bool loop) {
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

void ArtifactPlaybackService::setAudioClockProvider(const std::function<double()>& provider) {
    if (!impl_) return;
    impl_->setExternalAudioClockProvider(provider);
    if (impl_->controller_) {
        impl_->controller_->setAudioClockProvider(provider);
    }
}

void ArtifactPlaybackService::setCurrentComposition(ArtifactCompositionPtr composition) {
    if (impl_->currentComposition_ != composition) {
        impl_->currentComposition_ = composition;
        
        // コントローラーにコンポジションの設定を反映
        if (impl_->controller_ && composition) {
            impl_->controller_->setFrameRange(composition->frameRange());
            impl_->controller_->setFrameRate(composition->frameRate());
            impl_->controller_->setCurrentFrame(composition->framePosition());
        }
        
        Q_EMIT currentCompositionChanged(composition);
    }
}

ArtifactCompositionPtr ArtifactPlaybackService::currentComposition() const {
    return impl_->currentComposition_;
}

}
