module;
#include <functional>
#include <chrono>
#include <atomic>
#include <QMutex>
#include <QString>

export module Audio.AudioClockProvider;

import std;

export namespace Artifact {

// ─────────────────────────────────────────────
// Clock source types
// ─────────────────────────────────────────────
enum class AudioClockSource {
    SystemClock,     // std::chrono::high_resolution_clock
    AudioBackend,    // WASAPI/ASIO hardware clock
    External         // User-provided clock
};

// ─────────────────────────────────────────────
// Drift compensation settings
// ─────────────────────────────────────────────
struct AudioDriftCompensation {
    bool enabled = true;
    double smoothingFactor = 0.1;   // 0.0-1.0, lower = smoother but slower
    int64_t thresholdUs = 5000;     // 5ms threshold before correction
    int64_t maxCorrectionUs = 1000; // Max per-frame correction in microseconds
};

// ─────────────────────────────────────────────
// Audio clock state
// ─────────────────────────────────────────────
struct AudioClockState {
    AudioClockSource source = AudioClockSource::SystemClock;
    std::chrono::microseconds audioTime{0};
    std::chrono::microseconds videoTime{0};
    std::chrono::microseconds drift{0};        // audioTime - videoTime
    double effectiveSampleRate = 48000.0;
    int64_t samplesProcessed = 0;
    int64_t underflowCount = 0;
    int64_t overflowCount = 0;
    int64_t driftCorrectionCount = 0;
};

// ─────────────────────────────────────────────
// AudioClockFn type
// ─────────────────────────────────────────────
using AudioClockFn = std::function<double()>;

// ─────────────────────────────────────────────
// AudioClockProvider
// ─────────────────────────────────────────────
class AudioClockProvider {
private:
    class Impl;
    Impl* impl_;

public:
    AudioClockProvider();
    ~AudioClockProvider();

    // ── Clock source ──
    void setClockSource(AudioClockSource source);
    AudioClockSource clockSource() const;

    void setProvider(const AudioClockFn& fn);
    double now() const; // seconds

    // ── Audio time tracking ──
    void reportAudioTime(std::chrono::microseconds audioTimeUs);
    void reportVideoTime(std::chrono::microseconds videoTimeUs);

    // ── Drift compensation ──
    void setDriftCompensation(const AudioDriftCompensation& config);
    AudioDriftCompensation driftCompensation() const;

    // Call every frame to apply drift correction
    // Returns the adjusted video time (may be slightly offset to catch up)
    std::chrono::microseconds computeDriftCompensation(
        std::chrono::microseconds currentVideoTime);

    // ── State query ──
    AudioClockState state() const;
    QString statistics() const;

    // ── Reset ──
    void reset();
};

} // namespace Artifact
