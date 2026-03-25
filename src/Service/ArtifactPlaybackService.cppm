module;
#include <QTimer>
#include <QElapsedTimer>
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


#include <QDebug>
module Artifact.Service.Playback;




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
    ArtifactPlaybackEngine* engine_ = nullptr;  // 新しいマルチスレッドエンジン
    ArtifactCompositionPtr currentComposition_;
    QElapsedTimer audioTimer_;
    double audioOffsetSeconds_ = 0.0;
    bool audioRunning_ = false;
    std::function<double()> externalAudioClockProvider_;
    std::function<double()> playbackClockProvider_;
    float audioMasterVolume_ = 1.0f;
    bool audioMasterMuted_ = false;

    explicit Impl(ArtifactPlaybackService* owner)
        : owner_(owner) {
        controller_ = new ArtifactCompositionPlaybackController();
        engine_ = new ArtifactPlaybackEngine();
        
        // エンジンのシグナルをサービスに転送
        QObject::connect(engine_, &ArtifactPlaybackEngine::playbackStateChanged,
                         owner_, [this](bool playing, bool paused, bool stopped) {
            PlaybackState state = PlaybackState::Stopped;
            if (playing) state = PlaybackState::Playing;
            else if (paused) state = PlaybackState::Paused;
            else if (stopped) state = PlaybackState::Stopped;
            Q_EMIT owner_->playbackStateChanged(state);
        }, Qt::DirectConnection);
        
        QObject::connect(engine_, &ArtifactPlaybackEngine::frameChanged,
                         owner_, [this](const FramePosition& position, const QImage&) {
            if (currentComposition_) {
                currentComposition_->setFramePosition(position);
            }
            Q_EMIT owner_->frameChanged(position);
        }, Qt::DirectConnection);
        
        QObject::connect(engine_, &ArtifactPlaybackEngine::playbackSpeedChanged,
                         owner_, &ArtifactPlaybackService::playbackSpeedChanged,
                         Qt::DirectConnection);
        
        QObject::connect(engine_, &ArtifactPlaybackEngine::loopingChanged,
                         owner_, &ArtifactPlaybackService::loopingChanged,
                         Qt::DirectConnection);
        
        QObject::connect(engine_, &ArtifactPlaybackEngine::frameRangeChanged,
                         owner_, &ArtifactPlaybackService::frameRangeChanged,
                         Qt::DirectConnection);
        
        QObject::connect(engine_, &ArtifactPlaybackEngine::droppedFrameDetected,
                         owner_, [this](int64_t count) {
            qDebug() << "[PlaybackService] Dropped frames:" << count;
        });

        // コントローラーのシグナルも転送（後方互換性）
        QObject::connect(controller_, &ArtifactCompositionPlaybackController::playbackStateChanged,
                         owner_, &ArtifactPlaybackService::playbackStateChanged,
                         Qt::DirectConnection);

        QObject::connect(controller_, &ArtifactCompositionPlaybackController::frameChanged,
                         owner_, [this](const FramePosition& position) {
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

            double seconds = audioOffsetSeconds_;
            if (audioRunning_) {
                seconds += static_cast<double>(audioTimer_.elapsed()) / 1000.0;
            }
            return seconds;
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
    void setPlaybackClockProvider(const std::function<double()>& provider) {
        setExternalAudioClockProvider(provider);
    }
};

ArtifactPlaybackService::ArtifactPlaybackService(QObject* parent)
    : QObject(parent), impl_(new Impl(this)) {
    // デフォルトの再生範囲を設定
    setFrameRange(FrameRange(FramePosition(0), FramePosition(300)));
}

ArtifactPlaybackService::~ArtifactPlaybackService() {
    delete impl_;
}

ArtifactPlaybackService* ArtifactPlaybackService::instance() {
    static ArtifactPlaybackService service;
    return &service;
}

void ArtifactPlaybackService::play() {
    impl_->startAudioClock();
    // 新しいエンジンを使用
    if (impl_->engine_) {
        impl_->engine_->play();
    }
    /*
    if (impl_->controller_) {
        impl_->controller_->play();
    }
    */
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

void ArtifactPlaybackService::goToFrame(const FramePosition& position) {
    if (impl_->engine_) {
        impl_->engine_->goToFrame(position);
    } else if (impl_->controller_) {
        impl_->controller_->goToFrame(position);
    }
}

void ArtifactPlaybackService::goToNextFrame() {
    if (impl_->engine_) {
        impl_->engine_->goToNextFrame();
    } else if (impl_->controller_) {
        impl_->controller_->goToNextFrame();
    }
}

void ArtifactPlaybackService::goToPreviousFrame() {
    if (impl_->engine_) {
        impl_->engine_->goToPreviousFrame();
    } else if (impl_->controller_) {
        impl_->controller_->goToPreviousFrame();
    }
}

void ArtifactPlaybackService::goToStartFrame() {
    if (impl_->engine_) {
        impl_->engine_->goToStartFrame();
    } else if (impl_->controller_) {
        impl_->controller_->goToStartFrame();
    }
}

void ArtifactPlaybackService::goToEndFrame() {
    if (impl_->engine_) {
        impl_->engine_->goToEndFrame();
    } else if (impl_->controller_) {
        impl_->controller_->goToEndFrame();
    }
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
        if (impl_->engine_->isPlaying()) return PlaybackState::Playing;
        if (impl_->engine_->isPaused()) return PlaybackState::Paused;
        return PlaybackState::Stopped;
    }
    return impl_->controller_ ? impl_->controller_->state() : PlaybackState::Stopped;
}

FramePosition ArtifactPlaybackService::currentFrame() const {
    return impl_->engine_ ? impl_->engine_->currentFrame() : 
           (impl_->controller_ ? impl_->controller_->currentFrame() : FramePosition(0));
}

void ArtifactPlaybackService::setCurrentFrame(const FramePosition& position) {
    if (impl_->engine_) {
        impl_->engine_->setCurrentFrame(position);
    } else if (impl_->controller_) {
        impl_->controller_->setCurrentFrame(position);
    }
}

FrameRange ArtifactPlaybackService::frameRange() const {
    return impl_->engine_ ? impl_->engine_->frameRange() :
           (impl_->controller_ ? impl_->controller_->frameRange() : FrameRange(FramePosition(0), FramePosition(100)));
}

void ArtifactPlaybackService::setFrameRange(const FrameRange& range) {
    if (impl_->engine_) {
        impl_->engine_->setFrameRange(range);
    }
    if (impl_->controller_) {
        impl_->controller_->setFrameRange(range);
    }
}

FrameRate ArtifactPlaybackService::frameRate() const {
    return impl_->engine_ ? impl_->engine_->frameRate() :
           (impl_->controller_ ? impl_->controller_->frameRate() : FrameRate(30.0f));
}

void ArtifactPlaybackService::setFrameRate(const FrameRate& rate) {
    if (impl_->engine_) {
        impl_->engine_->setFrameRate(rate);
    }
    if (impl_->controller_) {
        impl_->controller_->setFrameRate(rate);
    }
}

float ArtifactPlaybackService::playbackSpeed() const {
    return impl_->engine_ ? impl_->engine_->playbackSpeed() :
           (impl_->controller_ ? impl_->controller_->playbackSpeed() : 1.0f);
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
    return impl_->engine_ ? impl_->engine_->isLooping() :
           (impl_->controller_ ? impl_->controller_->isLooping() : false);
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

void ArtifactPlaybackService::setAudioClockProvider(const std::function<double()>& provider) {
    if (!impl_) return;
    impl_->setExternalAudioClockProvider(provider);
    if (impl_->controller_) {
        impl_->controller_->setAudioClockProvider(provider);
    }
}

void ArtifactPlaybackService::setAudioMasterVolume(float volume) {
    if (!impl_) return;
    impl_->audioMasterVolume_ = std::clamp(volume, 0.0f, 2.0f);
    if (impl_->engine_) {
        impl_->engine_->setAudioMasterVolume(impl_->audioMasterVolume_);
    }
}

void ArtifactPlaybackService::setAudioMasterMuted(bool muted) {
    if (!impl_) return;
    impl_->audioMasterMuted_ = muted;
    if (impl_->engine_) {
        impl_->engine_->setAudioMasterMuted(muted);
    }
}

void ArtifactPlaybackService::setCurrentComposition(ArtifactCompositionPtr composition) {
    if (impl_->currentComposition_ != composition) {
        impl_->currentComposition_ = composition;

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

        Q_EMIT currentCompositionChanged(composition);
    }
}

ArtifactCompositionPtr ArtifactPlaybackService::currentComposition() const {
    return impl_->currentComposition_;
}

// ==================== In/Out Points ====================

void ArtifactPlaybackService::setInOutPoints(ArtifactInOutPoints* inOutPoints) {
    if (impl_ && impl_->engine_) {
        impl_->engine_->setInOutPoints(inOutPoints);
    } else if (impl_ && impl_->controller_) {
        impl_->controller_->setInOutPoints(inOutPoints);
    }
}

ArtifactInOutPoints* ArtifactPlaybackService::inOutPoints() const {
    if (impl_->engine_) {
        return impl_->engine_->inOutPoints();
    }
    return impl_ && impl_->controller_ ? impl_->controller_->inOutPoints() : nullptr;
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

} // namespace Artifact
