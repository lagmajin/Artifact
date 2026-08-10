module;
#include <utility>
#include <QMutex>
#include <QString>

module Audio.AudioClockProvider;

import std;

namespace Artifact {

namespace {

int64_t saturatingDifference(int64_t left, int64_t right)
{
    const auto limits = std::numeric_limits<int64_t>{};
    if (right < 0 && left > limits.max() + right) return limits.max();
    if (right > 0 && left < limits.min() + right) return limits.min();
    return left - right;
}

int64_t saturatingAdd(int64_t left, int64_t right)
{
    const auto limits = std::numeric_limits<int64_t>{};
    if (right > 0 && left > limits.max() - right) return limits.max();
    if (right < 0 && left < limits.min() - right) return limits.min();
    return left + right;
}

uint64_t unsignedMagnitude(int64_t value)
{
    return value < 0
        ? static_cast<uint64_t>(-(value + 1)) + 1u
        : static_cast<uint64_t>(value);
}

}

class AudioClockProvider::Impl {
public:
    mutable QMutex mutex_;

    AudioClockSource source_ = AudioClockSource::SystemClock;
    AudioClockFn externalFn_;

    // Time tracking
    std::chrono::microseconds audioTime_{0};
    std::chrono::microseconds videoTime_{0};
    std::chrono::microseconds smoothedDrift_{0};  // EMA-filtered drift
    std::chrono::microseconds lastCorrection_{0};

    // Drift compensation
    AudioDriftCompensation driftConfig_;

    // Statistics
    double effectiveSampleRate_ = 48000.0;
    int64_t samplesProcessed_ = 0;
    int64_t underflowCount_ = 0;
    int64_t overflowCount_ = 0;
    int64_t driftCorrectionCount_ = 0;

    // Start time reference
    std::chrono::high_resolution_clock::time_point startTime_;
    bool hasStartTime_ = false;

    Impl() {
        startTime_ = std::chrono::high_resolution_clock::now();
    }

    Impl(const Impl& other)
        : source_(other.source_)
        , externalFn_(other.externalFn_)
        , audioTime_(other.audioTime_)
        , videoTime_(other.videoTime_)
        , smoothedDrift_(other.smoothedDrift_)
        , lastCorrection_(other.lastCorrection_)
        , driftConfig_(other.driftConfig_)
        , effectiveSampleRate_(other.effectiveSampleRate_)
        , samplesProcessed_(other.samplesProcessed_)
        , underflowCount_(other.underflowCount_)
        , overflowCount_(other.overflowCount_)
        , driftCorrectionCount_(other.driftCorrectionCount_)
        , startTime_(other.startTime_)
        , hasStartTime_(other.hasStartTime_)
    {}

    Impl& operator=(const Impl& other) {
        if (this != &other) {
            QMutexLocker lockerA(&mutex_);
            QMutexLocker lockerB(&other.mutex_);
            source_ = other.source_;
            externalFn_ = other.externalFn_;
            audioTime_ = other.audioTime_;
            videoTime_ = other.videoTime_;
            smoothedDrift_ = other.smoothedDrift_;
            lastCorrection_ = other.lastCorrection_;
            driftConfig_ = other.driftConfig_;
            effectiveSampleRate_ = other.effectiveSampleRate_;
            samplesProcessed_ = other.samplesProcessed_;
            underflowCount_ = other.underflowCount_;
            overflowCount_ = other.overflowCount_;
            driftCorrectionCount_ = other.driftCorrectionCount_;
            startTime_ = other.startTime_;
            hasStartTime_ = other.hasStartTime_;
        }
        return *this;
    }
};

AudioClockProvider::AudioClockProvider() : impl_(new Impl()) {}
AudioClockProvider::~AudioClockProvider() { delete impl_; }

void AudioClockProvider::setClockSource(AudioClockSource source) {
    QMutexLocker locker(&impl_->mutex_);
    impl_->source_ = source;
}

AudioClockSource AudioClockProvider::clockSource() const {
    QMutexLocker locker(&impl_->mutex_);
    return impl_->source_;
}

void AudioClockProvider::setProvider(const AudioClockFn& fn) {
    QMutexLocker locker(&impl_->mutex_);
    impl_->externalFn_ = fn;
    impl_->source_ = AudioClockSource::External;
}

double AudioClockProvider::now() const {
    QMutexLocker locker(&impl_->mutex_);

    if (impl_->source_ == AudioClockSource::External && impl_->externalFn_) {
        return impl_->externalFn_();
    }

    // Default: system clock
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        now - impl_->startTime_);
    return elapsed.count() / 1000000.0;
}

void AudioClockProvider::reportAudioTime(std::chrono::microseconds audioTimeUs) {
    QMutexLocker locker(&impl_->mutex_);
    impl_->audioTime_ = audioTimeUs;
}

void AudioClockProvider::reportVideoTime(std::chrono::microseconds videoTimeUs) {
    QMutexLocker locker(&impl_->mutex_);
    impl_->videoTime_ = videoTimeUs;
}

void AudioClockProvider::setDriftCompensation(const AudioDriftCompensation& config) {
    QMutexLocker locker(&impl_->mutex_);
    impl_->driftConfig_ = config;
    impl_->driftConfig_.smoothingFactor =
        std::isfinite(config.smoothingFactor)
            ? std::clamp(config.smoothingFactor, 0.0, 1.0) : 0.1;
    impl_->driftConfig_.thresholdUs = std::max<int64_t>(0, config.thresholdUs);
    impl_->driftConfig_.maxCorrectionUs =
        std::max<int64_t>(0, config.maxCorrectionUs);
}

AudioDriftCompensation AudioClockProvider::driftCompensation() const {
    QMutexLocker locker(&impl_->mutex_);
    return impl_->driftConfig_;
}

std::chrono::microseconds AudioClockProvider::computeDriftCompensation(
    std::chrono::microseconds currentVideoTime)
{
    QMutexLocker locker(&impl_->mutex_);

    if (!impl_->driftConfig_.enabled) {
        return currentVideoTime;
    }

    // Calculate raw drift
    const int64_t rawDrift = saturatingDifference(
        impl_->audioTime_.count(), currentVideoTime.count());

    // Apply exponential moving average filter
    const double alpha = impl_->driftConfig_.smoothingFactor;
    const int64_t prevSmoothed = impl_->smoothedDrift_.count();
    const long double blended =
        static_cast<long double>(alpha) * rawDrift +
        static_cast<long double>(1.0 - alpha) * prevSmoothed;
    const long double minInt64 = static_cast<long double>(
        std::numeric_limits<int64_t>::min());
    const long double maxInt64 = static_cast<long double>(
        std::numeric_limits<int64_t>::max());
    const int64_t newSmoothed = static_cast<int64_t>(std::clamp(
        blended, minInt64, maxInt64));
    impl_->smoothedDrift_ = std::chrono::microseconds(newSmoothed);

    // Only correct if drift exceeds threshold
    const uint64_t absSmoothed = unsignedMagnitude(newSmoothed);
    if (absSmoothed < impl_->driftConfig_.thresholdUs) {
        return currentVideoTime;
    }

    // Clamp correction to max per frame
    const int64_t maxCorr = impl_->driftConfig_.maxCorrectionUs;
    int64_t correction = newSmoothed;
    if (correction > maxCorr) correction = maxCorr;
    if (correction < -maxCorr) correction = -maxCorr;

    impl_->lastCorrection_ = std::chrono::microseconds(correction);
    impl_->driftCorrectionCount_++;

    // Return adjusted video time (move toward audio time)
    return std::chrono::microseconds(saturatingAdd(
        currentVideoTime.count(), correction));
}

AudioClockState AudioClockProvider::state() const {
    QMutexLocker locker(&impl_->mutex_);
    AudioClockState s;
    s.source = impl_->source_;
    s.audioTime = impl_->audioTime_;
    s.videoTime = impl_->videoTime_;
    s.drift = impl_->smoothedDrift_;
    s.effectiveSampleRate = impl_->effectiveSampleRate_;
    s.samplesProcessed = impl_->samplesProcessed_;
    s.underflowCount = impl_->underflowCount_;
    s.overflowCount = impl_->overflowCount_;
    s.driftCorrectionCount = impl_->driftCorrectionCount_;
    return s;
}

QString AudioClockProvider::statistics() const {
    QMutexLocker locker(&impl_->mutex_);

    const QString sourceStr =
        impl_->source_ == AudioClockSource::SystemClock ? QStringLiteral("SystemClock") :
        impl_->source_ == AudioClockSource::AudioBackend ? QStringLiteral("AudioBackend") :
                                                           QStringLiteral("External");

    return QStringLiteral(
        "Audio Clock State:\n"
        "  Source: %1\n"
        "  Audio Time: %2 ms\n"
        "  Video Time: %3 ms\n"
        "  Smoothed Drift: %4 ms\n"
        "  Last Correction: %5 µs\n"
        "  Effective Sample Rate: %6 Hz\n"
        "  Samples Processed: %7\n"
        "  Underflows: %8\n"
        "  Overflows: %9\n"
        "  Drift Corrections: %10\n")
        .arg(sourceStr)
        .arg(impl_->audioTime_.count() / 1000.0, 0, 'f', 2)
        .arg(impl_->videoTime_.count() / 1000.0, 0, 'f', 2)
        .arg(impl_->smoothedDrift_.count() / 1000.0, 0, 'f', 2)
        .arg(impl_->lastCorrection_.count())
        .arg(impl_->effectiveSampleRate_, 0, 'f', 1)
        .arg(impl_->samplesProcessed_)
        .arg(impl_->underflowCount_)
        .arg(impl_->overflowCount_)
        .arg(impl_->driftCorrectionCount_);
}

void AudioClockProvider::reset() {
    QMutexLocker locker(&impl_->mutex_);
    impl_->audioTime_ = std::chrono::microseconds{0};
    impl_->videoTime_ = std::chrono::microseconds{0};
    impl_->smoothedDrift_ = std::chrono::microseconds{0};
    impl_->lastCorrection_ = std::chrono::microseconds{0};
    impl_->samplesProcessed_ = 0;
    impl_->underflowCount_ = 0;
    impl_->overflowCount_ = 0;
    impl_->driftCorrectionCount_ = 0;
    impl_->startTime_ = std::chrono::high_resolution_clock::now();
    impl_->hasStartTime_ = true;
}

} // namespace Artifact
