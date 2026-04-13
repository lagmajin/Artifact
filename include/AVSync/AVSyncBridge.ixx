module;
#include <memory>
#include <chrono>
#include <QMutex>
#include <QString>

export module Artifact.AVSyncBridge;

import std;
import Audio.AudioClockProvider;

export namespace Artifact {

// ─────────────────────────────────────────────
// A/V Sync Bridge
// Connects AudioRenderer, PlaybackClock, and AudioClockProvider
// ─────────────────────────────────────────────

class AVSyncBridge {
private:
    class Impl;
    Impl* impl_;

public:
    AVSyncBridge();
    ~AVSyncBridge();

    // ── Setup ──
    void setAudioClockProvider(AudioClockProvider* provider);
    AudioClockProvider* audioClockProvider() const;

    // ── Synchronization ──
    // Call every frame from the video render loop
    // Returns adjusted video time (with drift compensation applied)
    std::chrono::microseconds syncFrame(
        std::chrono::microseconds currentVideoTime,
        std::chrono::microseconds currentAudioTime);

    // ── Drift compensation config ──
    void setDriftCompensationEnabled(bool enabled);
    bool isDriftCompensationEnabled() const;

    void setDriftThresholdUs(int64_t thresholdUs);
    int64_t driftThresholdUs() const;

    void setDriftSmoothingFactor(double factor);
    double driftSmoothingFactor() const;

    // ── Statistics ──
    QString statistics() const;
    int64_t totalDriftCorrections() const;
    int64_t currentDriftUs() const;

    // ── Reset ──
    void reset();
};

} // namespace Artifact
