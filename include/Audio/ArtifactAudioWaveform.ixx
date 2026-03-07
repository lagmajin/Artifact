module;
#include <QObject>
#include <QVector>
#include <QPointF>
#include <cmath>
#include <vector>
#include <wobjectdefs.h>

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
export module Artifact.Audio.Waveform;




import Audio.Segment;

export namespace Artifact {

using namespace ArtifactCore;

/**
 * @brief Waveform display data
 */
struct WaveformData {
    QVector<float> peaks;        // Peak values per sample
    QVector<float> rms;           // RMS values per sample
    QVector<QPointF> displayPoints; // Pre-calculated display points
    int width = 0;                // Display width in pixels
    float minSample = 0;
    float maxSample = 0;
    int sampleRate = 44100;
    qint64 startFrame = 0;
};

/**
 * @brief Audio waveform generator
 */
class AudioWaveformGenerator : public QObject {
    W_OBJECT(AudioWaveformGenerator)
private:
    class Impl;
    Impl* impl_;
    
public:
    explicit AudioWaveformGenerator(QObject* parent = nullptr);
    ~AudioWaveformGenerator();
    
    // Generate waveform from audio segment
    WaveformData generate(const AudioSegment& segment, int displayWidth);
    
    // Generate for specific range
    WaveformData generateRange(const AudioSegment& segment, 
                              qint64 startSample, 
                              qint64 sampleCount,
                              int displayWidth);
    
    // Stereo support
    WaveformData generateStereo(const AudioSegment& left, 
                               const AudioSegment& right,
                               int displayWidth);
    
    // Async generation
    void generateAsync(const AudioSegment& segment, int displayWidth);
    
signals:
    void waveformGenerated(const WaveformData& data) W_SIGNAL(waveformGenerated, data);
    void generationProgress(int percent) W_SIGNAL(generationProgress, percent);
};

/**
 * @brief Audio analysis types
 */
enum class AnalyzerType {
    Spectrum,      // FFT spectrum
    Waveform,     // Time domain
    Spectrogram,  // Time-frequency
    Level,        // Level meter
    Phase,        // Phase scope
    Lissajous     // Stereo correlation
};

/**
 * @brief FFT-based audio analyzer
 */
class AudioAnalyzer : public QObject {
    W_OBJECT(AudioAnalyzer)
private:
    class Impl;
    Impl* impl_;
    
    int fftSize_ = 2048;
    int hopSize_ = 512;
    std::vector<float> window_;
    
public:
    explicit AudioAnalyzer(QObject* parent = nullptr);
    ~AudioAnalyzer();
    
    // Configuration
    void setFFTSize(int size);
    int fftSize() const { return fftSize_; }
    
    void setHopSize(int size);
    int hopSize() const { return hopSize_; }
    
    // Window functions
    enum class WindowType {
        Rectangular,
        Hann,
        Hamming,
        Blackman,
        Nuttall
    };
    void setWindow(WindowType type);
    
    // Analysis
    std::vector<float> computeSpectrum(const AudioSegment& segment);
    std::vector<float> computeMagnitude(const AudioSegment& segment);
    std::vector<float> computePhase(const AudioSegment& segment);
    
    // Spectrogram (multiple frames)
    std::vector<std::vector<float>> computeSpectrogram(
        const AudioSegment& segment,
        int timeSteps);
    
    // Level analysis
    struct LevelInfo {
        float peak = -60.0f;
        float rms = -60.0f;
        float average = -60.0f;
        int peakSample = 0;
    };
    LevelInfo computeLevel(const AudioSegment& segment);
    
    // Stereo correlation
    float computeCorrelation(const AudioSegment& left, 
                           const AudioSegment& right);
    
    // Frequency bands
    struct BandInfo {
        QString name;
        float lowFreq;
        float highFreq;
        float energy;
    };
    std::vector<BandInfo> computeBands(const AudioSegment& segment);
    
signals:
    void analysisComplete() W_SIGNAL(analysisComplete);
};

/**
 * @brief Real-time level meter
 */
class AudioLevelMeter : public QObject {
    W_OBJECT(AudioLevelMeter)
private:
    class Impl;
    Impl* impl_;
    
    float attackTime_ = 10.0f;    // ms
    float releaseTime_ = 200.0f;  // ms
    
public:
    explicit AudioLevelMeter(QObject* parent = nullptr);
    ~AudioLevelMeter();
    
    // Time constants
    void setAttackTime(float ms);
    float attackTime() const { return attackTime_; }
    
    void setReleaseTime(float ms);
    float releaseTime() const { return releaseTime_; }
    
    // Process audio
    void process(const AudioSegment& segment);
    void processStereo(const AudioSegment& left, const AudioSegment& right);
    
    // Current levels (dB)
    float leftLevel() const;
    float rightLevel() const;
    float leftPeak() const;
    float rightPeak() const;
    
    // Reset
    void reset();
    
    // dB conversion
    static float linearToDb(float linear);
    static float dbToLinear(float db);
    
signals:
    void levelsChanged(float left, float right) W_SIGNAL(levelsChanged, left, right);
    void clipDetected() W_SIGNAL(clipDetected);
};

/**
 * @brief Audio synchronization tools
 */
class AudioSyncTools : public QObject {
    W_OBJECT(AudioSyncTools)
private:
    class Impl;
    Impl* impl_;
    
public:
    explicit AudioSyncTools(QObject* parent = nullptr);
    ~AudioSyncTools();
    
    // Time stretch (without pitch change)
    AudioSegment timeStretch(const AudioSegment& segment, float rate);
    
    // Pitch shift (without time change)
    AudioSegment pitchShift(const AudioSegment& segment, float semitones);
    
    // Time align two segments
    struct AlignmentResult {
        int offset;              // Sample offset
        float correlation;       // Correlation coefficient
    };
    AlignmentResult align(const AudioSegment& reference,
                        const AudioSegment& toAlign);
    
    // Beat detection
    std::vector<qint64> detectBeats(const AudioSegment& segment);
    
    // Tempo detection
    float detectTempo(const AudioSegment& segment);
    
    // Normalize
    AudioSegment normalize(const AudioSegment& segment, float targetDb = -3.0f);
    
    // Fade in/out
    AudioSegment fadeIn(const AudioSegment& segment, qint64 samples);
    AudioSegment fadeOut(const AudioSegment& segment, qint64 samples);
    
signals:
    void processingComplete() W_SIGNAL(processingComplete);
    void progress(int percent) W_SIGNAL(progress, percent);
};

} // namespace Artifact
