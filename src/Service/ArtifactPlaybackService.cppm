module;
#include <QTimer>
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

W_OBJECT_IMPL(ArtifactPlaybackService)

class ArtifactPlaybackService::Impl {
public:
    ArtifactCompositionPlaybackController* controller_ = nullptr;
    ArtifactCompositionPtr currentComposition_;
    
    Impl() {
        controller_ = new ArtifactCompositionPlaybackController();
        
        // コントローラーのシグナルをサービスに転送
        QObject::connect(controller_, &ArtifactCompositionPlaybackController::playbackStateChanged,
                        [this](PlaybackState state) {
            Q_EMIT static_cast<ArtifactPlaybackService*>(this)->playbackStateChanged(state);
        });
        
        QObject::connect(controller_, &ArtifactCompositionPlaybackController::frameChanged,
                        [this](const FramePosition& position) {
            // コンポジションのフレーム位置も更新
            if (currentComposition_) {
                currentComposition_->setFramePosition(position);
            }
            Q_EMIT static_cast<ArtifactPlaybackService*>(this)->frameChanged(position);
        });
        
        QObject::connect(controller_, &ArtifactCompositionPlaybackController::playbackSpeedChanged,
                        [this](float speed) {
            Q_EMIT static_cast<ArtifactPlaybackService*>(this)->playbackSpeedChanged(speed);
        });
        
        QObject::connect(controller_, &ArtifactCompositionPlaybackController::loopingChanged,
                        [this](bool loop) {
            Q_EMIT static_cast<ArtifactPlaybackService*>(this)->loopingChanged(loop);
        });
        
        QObject::connect(controller_, &ArtifactCompositionPlaybackController::frameRangeChanged,
                        [this](const FrameRange& range) {
            Q_EMIT static_cast<ArtifactPlaybackService*>(this)->frameRangeChanged(range);
        });
    }
    
    ~Impl() {
        delete controller_;
    }
};

ArtifactPlaybackService::ArtifactPlaybackService(QObject* parent)
    : QObject(parent), impl_(new Impl()) {
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
    return impl_->controller_ ? impl_->controller_->isPlaying() : false;
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
