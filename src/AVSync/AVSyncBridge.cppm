module;
#include <utility>
#include <QMutex>
#include <QString>

module Artifact.AVSyncBridge;

import std;
import Audio.AudioClockProvider;

namespace Artifact {

class AVSyncBridge::Impl {
public:
    mutable QMutex mutex_;
    AudioClockProvider* clockProvider_ = nullptr;

    // Statistics tracking
    int64_t totalDriftCorrections_ = 0;
    int64_t currentDriftUs_ = 0;

    Impl() = default;

    Impl(const Impl& other)
        : clockProvider_(other.clockProvider_)
        , totalDriftCorrections_(other.totalDriftCorrections_)
        , currentDriftUs_(other.currentDriftUs_)
    {}

    Impl& operator=(const Impl& other) {
        if (this != &other) {
            QMutexLocker lockerA(&mutex_);
            QMutexLocker lockerB(&other.mutex_);
            clockProvider_ = other.clockProvider_;
            totalDriftCorrections_ = other.totalDriftCorrections_;
            currentDriftUs_ = other.currentDriftUs_;
        }
        return *this;
    }
};

AVSyncBridge::AVSyncBridge() : impl_(new Impl()) {}
AVSyncBridge::~AVSyncBridge() { delete impl_; }

void AVSyncBridge::setAudioClockProvider(AudioClockProvider* provider) {
    QMutexLocker locker(&impl_->mutex_);
    impl_->clockProvider_ = provider;
}

AudioClockProvider* AVSyncBridge::audioClockProvider() const {
    QMutexLocker locker(&impl_->mutex_);
    return impl_->clockProvider_;
}

std::chrono::microseconds AVSyncBridge::syncFrame(
    std::chrono::microseconds currentVideoTime,
    std::chrono::microseconds currentAudioTime)
{
    QMutexLocker locker(&impl_->mutex_);

    if (!impl_->clockProvider_) {
        return currentVideoTime;
    }

    // Report times to the clock provider
    impl_->clockProvider_->reportAudioTime(currentAudioTime);
    impl_->clockProvider_->reportVideoTime(currentVideoTime);

    // Compute drift
    const int64_t drift = currentAudioTime.count() - currentVideoTime.count();
    impl_->currentDriftUs_ = drift;

    // Apply drift compensation
    const auto adjustedVideoTime =
        impl_->clockProvider_->computeDriftCompensation(currentVideoTime);

    // Track corrections
    const auto state = impl_->clockProvider_->state();
    if (state.driftCorrectionCount > impl_->totalDriftCorrections_) {
        impl_->totalDriftCorrections_ = state.driftCorrectionCount;
    }

    return adjustedVideoTime;
}

void AVSyncBridge::setDriftCompensationEnabled(bool enabled) {
    QMutexLocker locker(&impl_->mutex_);
    if (impl_->clockProvider_) {
        auto config = impl_->clockProvider_->driftCompensation();
        config.enabled = enabled;
        impl_->clockProvider_->setDriftCompensation(config);
    }
}

bool AVSyncBridge::isDriftCompensationEnabled() const {
    QMutexLocker locker(&impl_->mutex_);
    if (impl_->clockProvider_) {
        return impl_->clockProvider_->driftCompensation().enabled;
    }
    return false;
}

void AVSyncBridge::setDriftThresholdUs(int64_t thresholdUs) {
    QMutexLocker locker(&impl_->mutex_);
    if (impl_->clockProvider_) {
        auto config = impl_->clockProvider_->driftCompensation();
        config.thresholdUs = thresholdUs;
        impl_->clockProvider_->setDriftCompensation(config);
    }
}

int64_t AVSyncBridge::driftThresholdUs() const {
    QMutexLocker locker(&impl_->mutex_);
    if (impl_->clockProvider_) {
        return impl_->clockProvider_->driftCompensation().thresholdUs;
    }
    return 5000;
}

void AVSyncBridge::setDriftSmoothingFactor(double factor) {
    QMutexLocker locker(&impl_->mutex_);
    if (impl_->clockProvider_) {
        auto config = impl_->clockProvider_->driftCompensation();
        config.smoothingFactor = std::clamp(factor, 0.01, 1.0);
        impl_->clockProvider_->setDriftCompensation(config);
    }
}

double AVSyncBridge::driftSmoothingFactor() const {
    QMutexLocker locker(&impl_->mutex_);
    if (impl_->clockProvider_) {
        return impl_->clockProvider_->driftCompensation().smoothingFactor;
    }
    return 0.1;
}

QString AVSyncBridge::statistics() const {
    QMutexLocker locker(&impl_->mutex_);
    QString stats;
    stats += QStringLiteral("A/V Sync Bridge Statistics:\n");
    stats += QStringLiteral("  Total Drift Corrections: %1\n").arg(impl_->totalDriftCorrections_);
    stats += QStringLiteral("  Current Drift: %1 ms\n").arg(impl_->currentDriftUs_ / 1000.0, 0, 'f', 2);
    if (impl_->clockProvider_) {
        stats += impl_->clockProvider_->statistics();
    }
    return stats;
}

int64_t AVSyncBridge::totalDriftCorrections() const {
    QMutexLocker locker(&impl_->mutex_);
    return impl_->totalDriftCorrections_;
}

int64_t AVSyncBridge::currentDriftUs() const {
    QMutexLocker locker(&impl_->mutex_);
    return impl_->currentDriftUs_;
}

void AVSyncBridge::reset() {
    QMutexLocker locker(&impl_->mutex_);
    impl_->totalDriftCorrections_ = 0;
    impl_->currentDriftUs_ = 0;
    if (impl_->clockProvider_) {
        impl_->clockProvider_->reset();
    }
}

} // namespace Artifact
