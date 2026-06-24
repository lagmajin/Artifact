module;
#include <utility>
#include <QThread>
#include <QTimer>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QObject>
#include <QDebug>
#include <cstdlib>
#include <cmath>
#include <wobjectdefs.h>
#include <wobjectimpl.h>
module Artifact.Audio.ScrubController;

import Audio.Cache;
import Audio.Segment;
import AudioRenderer;
import Frame.Position;
import Artifact.Composition.Abstract;
import Core.Diagnostics.ProjectDiagnostic;
import Core.FastSettingsStore;

namespace Artifact
{
    using namespace ArtifactCore;

    static constexpr int kScrubFrameCount = 2400;
    static constexpr int kScrubSampleRate = 48000;
    static constexpr float kSpeedVolumeFactor = 0.35f;

    // ──────────────────────────────────────────
    // ScrubWorker — runs on dedicated worker thread
    // Pure data access only (no QObject APIs beyond Qt core)
    // ──────────────────────────────────────────
    class ScrubWorker : public QObject
    {
        W_OBJECT(ScrubWorker)
    public:
        void setCache(AudioCache* cache) { cache_ = cache; }

        void checkFrame(int64_t frame)
        {
            if (!cache_) return;
            AudioSegment segment;
            if (!cache_->getCached(frame, segment)) {
                Q_EMIT cacheMiss(FramePosition(frame));
            }
        }

        void cacheMiss(FramePosition frame) W_SIGNAL(cacheMiss, frame);

    private:
        AudioCache* cache_ = nullptr;
    };

    W_OBJECT_IMPL(ScrubWorker)

    // ──────────────────────────────────────────
    // ArtifactAudioScrubController
    // ──────────────────────────────────────────
    class ArtifactAudioScrubController::Impl
    {
    public:
        QThread workerThread;
        ScrubWorker* worker = nullptr;
        QTimer* debounceTimer = nullptr;

        bool enabled = true;
        bool scrubActive = false;
        int latencyTarget_ = 10;
        float volumeScale_ = 0.5f;
        int measureLatencyMs_ = 0;

        int64_t lastFrame_ = -1;
        int64_t pendingFrame_ = -1;
        qint64 lastFrameTime_ = 0;
        float currentSpeedFps_ = 0.0f;

        std::unique_ptr<AudioRenderer> audioRenderer_;
        ArtifactCompositionPtr composition_;

        bool deviceOpenFailed_ = false;

        ArtifactAudioScrubController* owner = nullptr;

        void onDebounceTick()
        {
            if (!scrubActive || pendingFrame_ < 0) return;

            const int64_t f = pendingFrame_;
            pendingFrame_ = -1;

            if (!audioRenderer_ || !composition_) return;

            if (!audioRenderer_->isDeviceOpen()) {
                if (!audioRenderer_->openDevice(QStringLiteral(""))) {
                    deviceOpenFailed_ = true;
                    return;
                }
                deviceOpenFailed_ = false;
            }

            AudioSegment segment;
            if (!composition_->getAudio(segment, FramePosition(f),
                                        kScrubFrameCount, kScrubSampleRate)) {
                Q_EMIT owner->cacheMiss(FramePosition(f));
                return;
            }

            const float vol = calcScrubVolume();
            if (vol < 0.001f) return;

            for (auto& ch : segment.channelData) {
                for (auto& s : ch) {
                    s *= vol;
                }
            }

            if (!audioRenderer_->isActive()) {
                if (audioRenderer_->bufferedFrames() <= kScrubFrameCount * 2) {
                    audioRenderer_->enqueue(segment);
                }
                audioRenderer_->start();
            } else {
                audioRenderer_->enqueue(segment);
            }
        }

        float calcScrubVolume() const
        {
            if (lastFrame_ < 0 || lastFrameTime_ <= 0) return 0.0f;
            if (currentSpeedFps_ <= 1.0f) return 0.0f;
            if (currentSpeedFps_ <= 10.0f) return volumeScale_;
            const float v = currentSpeedFps_;
            float vol = 1.0f - std::log10(v / 10.0f) * kSpeedVolumeFactor;
            return qBound(0.0f, vol * volumeScale_, volumeScale_);
        }

        void updateSpeed(int64_t currentFrame, qint64 nowMs)
        {
            if (lastFrame_ >= 0 && lastFrameTime_ > 0) {
                const qint64 dt = nowMs - lastFrameTime_;
                const int64_t df = std::abs(currentFrame - lastFrame_);
                if (dt > 0) {
                    currentSpeedFps_ = static_cast<float>(df) * 1000.0f / static_cast<float>(dt);
                }
            } else {
                currentSpeedFps_ = 0.0f;
            }
        }

        ~Impl()
        {
            if (audioRenderer_) {
                if (audioRenderer_->isActive()) audioRenderer_->stop();
                audioRenderer_->closeDevice();
            }
            workerThread.quit();
            workerThread.wait();
            delete worker;
        }
    };

    W_OBJECT_IMPL(ArtifactAudioScrubController)

    ArtifactAudioScrubController& ArtifactAudioScrubController::instance()
    {
        static ArtifactAudioScrubController inst;
        return inst;
    }

    ArtifactAudioScrubController::ArtifactAudioScrubController(QObject* parent)
        : QObject(parent), impl_(new Impl)
    {
        impl_->owner = this;
        impl_->worker = new ScrubWorker();
        impl_->worker->moveToThread(&impl_->workerThread);

        impl_->debounceTimer = new QTimer(this);
        impl_->debounceTimer->setInterval(16);
        impl_->debounceTimer->setSingleShot(false);
        QObject::connect(impl_->debounceTimer, &QTimer::timeout,
                         this, [this]() { impl_->onDebounceTick(); });

        QObject::connect(impl_->worker, &ScrubWorker::cacheMiss,
                         this, [this](FramePosition frame) {
                             Q_EMIT cacheMiss(frame);
                         });

        impl_->audioRenderer_ = std::make_unique<AudioRenderer>();
        impl_->workerThread.start();

        loadSettings();
    }

    ArtifactAudioScrubController::~ArtifactAudioScrubController()
    {
        delete impl_;
    }

    void ArtifactAudioScrubController::setComposition(ArtifactCompositionPtr comp)
    {
        impl_->composition_ = std::move(comp);
    }

    void ArtifactAudioScrubController::setEnabled(bool enabled)
    {
        impl_->enabled = enabled;
        if (!enabled && impl_->scrubActive) {
            stopScrub();
        }
    }

    bool ArtifactAudioScrubController::isEnabled() const
    {
        return impl_->enabled;
    }

    void ArtifactAudioScrubController::setLatencyTargetMs(int ms)
    {
        impl_->latencyTarget_ = qBound(5, ms, 200);
    }

    int ArtifactAudioScrubController::latencyTargetMs() const
    {
        return impl_->latencyTarget_;
    }

    void ArtifactAudioScrubController::setVolumeScale(float scale)
    {
        impl_->volumeScale_ = qBound(0.0f, scale, 1.0f);
    }

    float ArtifactAudioScrubController::volumeScale() const
    {
        return impl_->volumeScale_;
    }

    void ArtifactAudioScrubController::startScrub()
    {
        if (!impl_->enabled) return;
        impl_->scrubActive = true;
        impl_->lastFrame_ = -1;
        impl_->pendingFrame_ = -1;
        impl_->lastFrameTime_ = 0;
        impl_->currentSpeedFps_ = 0.0f;
        Q_EMIT scrubStarted();
    }

    void ArtifactAudioScrubController::stopScrub()
    {
        impl_->scrubActive = false;
        impl_->debounceTimer->stop();
        impl_->pendingFrame_ = -1;
        if (impl_->audioRenderer_) {
            impl_->audioRenderer_->clearBuffer();
        }
        Q_EMIT scrubStopped();
    }

    void ArtifactAudioScrubController::updateScrubPosition(FramePosition frame)
    {
        if (!impl_->scrubActive || !impl_->enabled) return;

        const int64_t f = frame.framePosition();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();

        impl_->pendingFrame_ = f;
        impl_->updateSpeed(f, now);

        if (impl_->lastFrame_ >= 0 && impl_->lastFrameTime_ > 0) {
            impl_->measureLatencyMs_ = static_cast<int>(now - impl_->lastFrameTime_);
            Q_EMIT latencyUpdated(impl_->measureLatencyMs_);
        }
        impl_->lastFrame_ = f;
        impl_->lastFrameTime_ = now;

        if (!impl_->debounceTimer->isActive()) {
            impl_->debounceTimer->start();
        }
    }

    bool ArtifactAudioScrubController::isScrubbing() const
    {
        return impl_->scrubActive;
    }

    int ArtifactAudioScrubController::latencyMs() const
    {
        return impl_->measureLatencyMs_;
    }

    float ArtifactAudioScrubController::scrubSpeedFps() const
    {
        return impl_->currentSpeedFps_;
    }

    float ArtifactAudioScrubController::scrubVolume() const
    {
        return impl_->calcScrubVolume();
    }

    static QString scrubSettingsPath()
    {
        const QString appData = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);
        return QDir(appData).filePath(QStringLiteral("audio_scrub.cbor"));
    }

    void ArtifactAudioScrubController::loadSettings()
    {
        FastSettingsStore store(scrubSettingsPath());
        setEnabled(store.valueBool(QStringLiteral("scrub.enabled"), true));
        setLatencyTargetMs(static_cast<int>(
            store.valueInt64(QStringLiteral("scrub.latencyMs"), 10)));
        setVolumeScale(static_cast<float>(
            store.value(QStringLiteral("scrub.volumeScale"), 0.5).toDouble()));
    }

    void ArtifactAudioScrubController::saveSettings()
    {
        FastSettingsStore store(scrubSettingsPath());
        store.setValue(QStringLiteral("scrub.enabled"), isEnabled());
        store.setValue(QStringLiteral("scrub.latencyMs"), latencyTargetMs());
        store.setValue(QStringLiteral("scrub.volumeScale"), volumeScale());
        store.sync();
    }

    std::vector<ArtifactCore::ProjectDiagnostic> ArtifactAudioScrubController::gatherDiagnostics() const
    {
        std::vector<ArtifactCore::ProjectDiagnostic> result;

        if (!impl_->enabled) {
            ProjectDiagnostic diag(
                DiagnosticSeverity::Info,
                DiagnosticCategory::Audio,
                QStringLiteral("Audio scrubbing is disabled"));
            diag.setDescription(QStringLiteral(
                "Audio scrubbing during timeline drag is turned off. "
                "Enable it in Settings > Audio > Scrubbing."));
            diag.setFixAction(QStringLiteral("Enable audio scrubbing in Settings"));
            result.push_back(std::move(diag));
        }

        if (impl_->measureLatencyMs_ > 50) {
            ProjectDiagnostic diag(
                DiagnosticSeverity::Warning,
                DiagnosticCategory::Performance,
                QStringLiteral("Audio scrub latency high: %1 ms")
                    .arg(impl_->measureLatencyMs_));
            diag.setDescription(QStringLiteral(
                "Audio scrubbing latency exceeds 50ms threshold. "
                "This may cause audio-visual desync during scrub."));
            diag.setFixAction(QStringLiteral(
                "Reduce latency target in Settings > Audio > Scrubbing "
                "or check system load"));
            result.push_back(std::move(diag));
        }

        if (impl_->deviceOpenFailed_) {
            ProjectDiagnostic diag(
                DiagnosticSeverity::Warning,
                DiagnosticCategory::Audio,
                QStringLiteral("Audio scrub device open failed"));
            diag.setDescription(QStringLiteral(
                "The audio scrub controller could not open the audio output "
                "device. Scrub audio will be silent."));
            diag.setFixAction(QStringLiteral(
                "Check audio device configuration in system settings"));
            result.push_back(std::move(diag));
        }

        return result;
    }

} // namespace Artifact
