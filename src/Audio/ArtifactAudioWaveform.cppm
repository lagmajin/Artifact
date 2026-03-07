module;

#include <cmath>
#include <algorithm>
#include <numeric>
#include <QRandomGenerator>
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
module Artifact.Audio.Waveform;




import Audio.Segment;

namespace Artifact {

W_OBJECT_IMPL(AudioWaveformGenerator)
W_OBJECT_IMPL(AudioAnalyzer)
W_OBJECT_IMPL(AudioLevelMeter)
W_OBJECT_IMPL(AudioSyncTools)

// ==================== AudioWaveformGenerator ====================

class AudioWaveformGenerator::Impl {
public:
    bool useRMS_ = true;
};

AudioWaveformGenerator::AudioWaveformGenerator(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

AudioWaveformGenerator::~AudioWaveformGenerator() = default;

WaveformData AudioWaveformGenerator::generate(const AudioSegment& segment, int displayWidth) {
    WaveformData data;
    data.width = displayWidth;
    data.sampleRate = segment.sampleRate;
    data.startFrame = segment.startFrame;
    
    if (segment.samples.empty() || displayWidth <= 0) {
        return data;
    }
    
    int numSamples = (int)segment.samples.size();
    int samplesPerPixel = std::max(1, numSamples / displayWidth);
    
    data.peaks.resize(displayWidth);
    data.rms.resize(displayWidth);
    data.displayPoints.reserve(displayWidth * 2);
    
    float minVal = 0, maxVal = 0;
    
    for (int i = 0; i < displayWidth; ++i) {
        int start = i * samplesPerPixel;
        int end = std::min(start + samplesPerPixel, numSamples);
        
        if (start >= numSamples) break;
        
        // Find peak
        float peak = 0;
        float sumSquares = 0;
        
        for (int j = start; j < end; ++j) {
            float val = segment.samples[j];
            float absVal = std::abs(val);
            if (absVal > peak) peak = absVal;
            sumSquares += val * val;
        }
        
        float rms = std::sqrt(sumSquares / (end - start));
        
        data.peaks[i] = peak;
        data.rms[i] = rms;
        
        // Update min/max
        minVal = std::min(minVal, -peak);
        maxVal = std::max(maxVal, peak);
        
        // Display point (for line rendering)
        data.displayPoints.append(QPointF(i, -peak));  // Top
        data.displayPoints.append(QPointF(i, peak));   // Bottom
    }
    
    data.minSample = minVal;
    data.maxSample = maxVal;
    
    return data;
}

WaveformData AudioWaveformGenerator::generateRange(const AudioSegment& segment,
                                                   qint64 startSample,
                                                   qint64 sampleCount,
                                                   int displayWidth) {
    // Similar to generate but with range
    return generate(segment, displayWidth);
}

WaveformData AudioWaveformGenerator::generateStereo(const AudioSegment& left,
                                                   const AudioSegment& right,
                                                   int displayWidth) {
    WaveformData mono;
    // Combine to mono
    AudioSegment combined;
    combined.sampleRate = left.sampleRate;
    combined.startFrame = left.startFrame;
    combined.samples.resize(std::min(left.samples.size(), right.samples.size()));
    
    for (size_t i = 0; i < combined.samples.size(); ++i) {
        combined.samples[i] = (left.samples[i] + right.samples[i]) * 0.5f;
    }
    
    return generate(combined, displayWidth);
}

void AudioWaveformGenerator::generateAsync(const AudioSegment& segment, int displayWidth) {
    // Would use QtConcurrent in real implementation
    auto result = generate(segment, displayWidth);
    emit waveformGenerated(result);
}

// ==================== AudioAnalyzer ====================

class AudioAnalyzer::Impl {
public:
    // Simple FFT implementation placeholder
    std::vector<std::complex<float>> fft(const std::vector<float>& input) {
        int n = input.size();
        std::vector<std::complex<float>> output(n);
        
        // Simple DFT (slow but works)
        for (int k = 0; k < n; ++k) {
            std::complex<float> sum = 0;
            for (int t = 0; t < n; ++t) {
                float angle = -2.0f * 3.14159f * k * t / n;
                sum += input[t] * std::complex<float>(std::cos(angle), std::sin(angle));
            }
            output[k] = sum;
        }
        
        return output;
    }
};

AudioAnalyzer::AudioAnalyzer(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
    // Default to Hann window
    setWindow(WindowType::Hann);
}

AudioAnalyzer::~AudioAnalyzer() = default;

void AudioAnalyzer::setFFTSize(int size) {
    fftSize_ = size;
    // Recreate window
    setWindow(WindowType::Hann);
}

void AudioAnalyzer::setHopSize(int size) {
    hopSize_ = size;
}

void AudioAnalyzer::setWindow(WindowType type) {
    window_.resize(fftSize_);
    
    switch (type) {
        case WindowType::Hann:
            for (int i = 0; i < fftSize_; ++i) {
                window_[i] = 0.5f * (1.0f - std::cos(2.0f * 3.14159f * i / (fftSize_ - 1)));
            }
            break;
        case WindowType::Hamming:
            for (int i = 0; i < fftSize_; ++i) {
                window_[i] = 0.54f - 0.46f * std::cos(2.0f * 3.14159f * i / (fftSize_ - 1));
            }
            break;
        case WindowType::Blackman:
            for (int i = 0; i < fftSize_; ++i) {
                window_[i] = 0.42f - 0.5f * std::cos(2.0f * 3.14159f * i / (fftSize_ - 1)) 
                          + 0.08f * std::cos(4.0f * 3.14159f * i / (fftSize_ - 1));
            }
            break;
        default:
            for (int i = 0; i < fftSize_; ++i) window_[i] = 1.0f;
    }
}

std::vector<float> AudioAnalyzer::computeSpectrum(const AudioSegment& segment) {
    if (segment.samples.size() < (size_t)fftSize_) {
        return {};
    }
    
    // Apply window
    std::vector<float> windowed(fftSize_);
    for (int i = 0; i < fftSize_; ++i) {
        windowed[i] = segment.samples[i] * window_[i];
    }
    
    // FFT
    auto spectrum = impl_->fft(windowed);
    
    // Magnitude
    std::vector<float> magnitude(fftSize_ / 2);
    for (int i = 0; i < fftSize_ / 2; ++i) {
        magnitude[i] = std::abs(spectrum[i]) / fftSize_;
    }
    
    return magnitude;
}

std::vector<float> AudioAnalyzer::computeMagnitude(const AudioSegment& segment) {
    return computeSpectrum(segment);
}

std::vector<float> AudioAnalyzer::computePhase(const AudioSegment& segment) {
    if (segment.samples.size() < (size_t)fftSize_) {
        return {};
    }
    
    std::vector<float> windowed(fftSize_);
    for (int i = 0; i < fftSize_; ++i) {
        windowed[i] = segment.samples[i] * window_[i];
    }
    
    auto spectrum = impl_->fft(windowed);
    
    std::vector<float> phase(fftSize_ / 2);
    for (int i = 0; i < fftSize_ / 2; ++i) {
        phase[i] = std::arg(spectrum[i]);
    }
    
    return phase;
}

std::vector<std::vector<float>> AudioAnalyzer::computeSpectrogram(const AudioSegment& segment,
                                                                    int timeSteps) {
    std::vector<std::vector<float>> spectrogram;
    
    int stepSize = (int)segment.samples.size() / timeSteps;
    if (stepSize < fftSize_) stepSize = fftSize_;
    
    for (int t = 0; t < timeSteps; ++t) {
        int start = t * stepSize;
        if (start + fftSize_ > (int)segment.samples.size()) break;
        
        AudioSegment frame;
        frame.samples = std::vector<float>(segment.samples.begin() + start,
                                          segment.samples.begin() + start + fftSize_);
        frame.sampleRate = segment.sampleRate;
        
        auto spec = computeSpectrum(frame);
        spectrogram.push_back(spec);
    }
    
    return spectrogram;
}

AudioAnalyzer::LevelInfo AudioAnalyzer::computeLevel(const AudioSegment& segment) {
    LevelInfo info;
    
    if (segment.samples.empty()) return info;
    
    float peak = 0;
    float sumSquares = 0;
    
    for (size_t i = 0; i < segment.samples.size(); ++i) {
        float val = std::abs(segment.samples[i]);
        if (val > peak) {
            peak = val;
            info.peakSample = i;
        }
        sumSquares += segment.samples[i] * segment.samples[i];
    }
    
    info.rms = std::sqrt(sumSquares / segment.samples.size());
    info.peak = peak;
    info.average = sumSquares / segment.samples.size();
    
    return info;
}

float AudioAnalyzer::computeCorrelation(const AudioSegment& left,
                                      const AudioSegment& right) {
    if (left.samples.size() != right.samples.size() || left.samples.empty()) {
        return 0;
    }
    
    float sumXY = 0, sumX2 = 0, sumY2 = 0;
    
    for (size_t i = 0; i < left.samples.size(); ++i) {
        sumXY += left.samples[i] * right.samples[i];
        sumX2 += left.samples[i] * left.samples[i];
        sumY2 += right.samples[i] * right.samples[i];
    }
    
    if (sumX2 == 0 || sumY2 == 0) return 0;
    
    return sumXY / std::sqrt(sumX2 * sumY2);
}

std::vector<AudioAnalyzer::BandInfo> AudioAnalyzer::computeBands(const AudioSegment& segment) {
    // Standard frequency bands
    std::vector<BandInfo> bands = {
        {"Sub Bass", 20, 60, 0},
        {"Bass", 60, 250, 0},
        {"Low Mid", 250, 500, 0},
        {"Mid", 500, 2000, 0},
        {"High Mid", 2000, 4000, 0},
        {"Presence", 4000, 6000, 0},
        {"Brilliance", 6000, 20000, 0}
    };
    
    auto spectrum = computeSpectrum(segment);
    if (spectrum.empty()) return bands;
    
    float binWidth = (float)segment.sampleRate / fftSize_;
    
    for (auto& band : bands) {
        int lowBin = (int)(band.lowFreq / binWidth);
        int highBin = (int)(band.highFreq / binWidth);
        
        float energy = 0;
        for (int b = lowBin; b < highBin && b < (int)spectrum.size(); ++b) {
            energy += spectrum[b] * spectrum[b];
        }
        band.energy = std::sqrt(energy);
    }
    
    return bands;
}

// ==================== AudioLevelMeter ====================

class AudioLevelMeter::Impl {
public:
    float leftLevel_ = -60.0f;
    float rightLevel_ = -60.0f;
    float leftPeak_ = -60.0f;
    float rightPeak_ = -60.0f;
    float leftPeakHold_ = -60.0f;
    float rightPeakHold_ = -60.0f;
    
    int peakHoldFrames_ = 30;
    int leftPeakHoldCounter_ = 0;
    int rightPeakHoldCounter_ = 0;
};

AudioLevelMeter::AudioLevelMeter(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

AudioLevelMeter::~AudioLevelMeter() = default;

void AudioLevelMeter::setAttackTime(float ms) {
    attackTime_ = ms;
}

void AudioLevelMeter::setReleaseTime(float ms) {
    releaseTime_ = ms;
}

void AudioLevelMeter::process(const AudioSegment& segment) {
    auto level = computeLevel(segment);
    float db = (level.rms > 0) ? 20.0f * std::log10(level.rms) : -60.0f;
    float peakDb = (level.rms > 0) ? 20.0f * std::log10(level.rms) : -60.0f;
    
    impl_->leftLevel_ = db;
    impl_->rightLevel_ = db;
    
    if (peakDb > impl_->leftPeak_) {
        impl_->leftPeak_ = peakDb;
        impl_->leftPeakHoldCounter_ = peakHoldFrames_;
    }
    
    if (impl_->leftPeakHoldCounter_ > 0) {
        impl_->leftPeakHoldCounter_--;
    } else {
        impl_->leftPeak_ = db;
    }
    
    emit levelsChanged(impl_->leftLevel_, impl_->rightLevel_);
    
    if (peakDb >= 0) {
        emit clipDetected();
    }
}

void AudioLevelMeter::processStereo(const AudioSegment& left, const AudioSegment& right) {
    auto leftLevel = computeLevel(left);
    auto rightLevel = computeLevel(right);
    
    float leftDb = (leftLevel.rms > 0) ? 20.0f * std::log10(leftLevel.rms) : -60.0f;
    float rightDb = (rightLevel.rms > 0) ? 20.0f * std::log10(rightLevel.rms) : -60.0f;
    
    impl_->leftLevel_ = leftDb;
    impl_->rightLevel_ = rightDb;
    
    emit levelsChanged(leftDb, rightDb);
}

float AudioLevelMeter::leftLevel() const { return impl_->leftLevel_; }
float AudioLevelMeter::rightLevel() const { return impl_->rightLevel_; }
float AudioLevelMeter::leftPeak() const { return impl_->leftPeak_; }
float AudioLevelMeter::rightPeak() const { return impl_->rightPeak_; }

void AudioLevelMeter::reset() {
    impl_->leftLevel_ = -60.0f;
    impl_->rightLevel_ = -60.0f;
    impl_->leftPeak_ = -60.0f;
    impl_->rightPeak_ = -60.0f;
}

float AudioLevelMeter::linearToDb(float linear) {
    return (linear > 0) ? 20.0f * std::log10(linear) : -60.0f;
}

float AudioLevelMeter::dbToLinear(float db) {
    return std::pow(10.0f, db / 20.0f);
}

// ==================== AudioSyncTools ====================

class AudioSyncTools::Impl {};

AudioSyncTools::AudioSyncTools(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

AudioSyncTools::~AudioSyncTools() = default;

AudioSegment AudioSyncTools::timeStretch(const AudioSegment& segment, float rate) {
    AudioSegment result;
    result.sampleRate = segment.sampleRate;
    result.startFrame = segment.startFrame;
    result.layout = segment.layout;
    
    if (segment.samples.empty()) return result;
    
    size_t newSize = (size_t)(segment.samples.size() / rate);
    result.samples.resize(newSize);
    
    for (size_t i = 0; i < newSize; ++i) {
        float srcIdx = i * rate;
        size_t idx0 = (size_t)srcIdx;
        size_t idx1 = std::min(idx0 + 1, segment.samples.size() - 1);
        float frac = srcIdx - idx0;
        
        result.samples[i] = segment.samples[idx0] * (1.0f - frac) 
                          + segment.samples[idx1] * frac;
    }
    
    return result;
}

AudioSegment AudioSyncTools::pitchShift(const AudioSegment& segment, float semitones) {
    float rate = std::pow(2.0f, semitones / 12.0f);
    return timeStretch(segment, rate);
}

AudioSyncTools::AlignmentResult AudioSyncTools::align(const AudioSegment& reference,
                                                      const AudioSegment& toAlign) {
    AlignmentResult result;
    result.offset = 0;
    result.correlation = 0;
    
    // Simple cross-correlation
    int maxLag = std::min((int)reference.samples.size(), (int)toAlign.samples.size()) / 4;
    
    for (int lag = -maxLag; lag <= maxLag; ++lag) {
        float corr = 0;
        int count = 0;
        
        for (size_t i = 0; i < reference.samples.size() - std::abs(lag); ++i) {
            int j = i + lag;
            if (j >= 0 && j < (int)toAlign.samples.size()) {
                corr += reference.samples[i] * toAlign.samples[j];
                count++;
            }
        }
        
        if (count > 0 && corr > result.correlation) {
            result.correlation = corr;
            result.offset = lag;
        }
    }
    
    return result;
}

std::vector<qint64> AudioSyncTools::detectBeats(const AudioSegment& segment) {
    std::vector<qint64> beats;
    
    // Simple onset detection
    int windowSize = segment.sampleRate / 10; // 100ms windows
    float threshold = 0.5f;
    
    float prevEnergy = 0;
    
    for (size_t i = 0; i < segment.samples.size() - windowSize; i += windowSize / 2) {
        float energy = 0;
        for (size_t j = i; j < i + windowSize && j < segment.samples.size(); ++j) {
            energy += segment.samples[j] * segment.samples[j];
        }
        energy /= windowSize;
        
        if (energy > prevEnergy * 1.5f && energy > threshold) {
            beats.push_back((qint64)i);
        }
        prevEnergy = energy;
    }
    
    return beats;
}

float AudioSyncTools::detectTempo(const AudioSegment& segment) {
    auto beats = detectBeats(segment);
    
    if (beats.size() < 2) return 120.0f;
    
    // Calculate average interval
    float avgInterval = 0;
    for (size_t i = 1; i < beats.size(); ++i) {
        avgInterval += beats[i] - beats[i-1];
    }
    avgInterval /= (beats.size() - 1);
    
    // Convert samples to BPM
    float bpm = (float)segment.sampleRate * 60.0f / avgInterval;
    
    // Normalize to typical range
    while (bpm < 60) bpm *= 2;
    while (bpm > 180) bpm /= 2;
    
    return bpm;
}

AudioSegment AudioSyncTools::normalize(const AudioSegment& segment, float targetDb) {
    AudioSegment result = segment;
    
    auto level = computeLevel(segment);
    float currentDb = linearToDb(level.rms);
    float gainDb = targetDb - currentDb;
    float gain = dbToLinear(gainDb);
    
    for (auto& sample : result.samples) {
        sample *= gain;
    }
    
    return result;
}

AudioSegment AudioSyncTools::fadeIn(const AudioSegment& segment, qint64 samples) {
    AudioSegment result = segment;
    
    qint64 fadeSamples = std::min(samples, (qint64)result.samples.size());
    
    for (qint64 i = 0; i < fadeSamples; ++i) {
        float gain = (float)i / fadeSamples;
        result.samples[i] *= gain;
    }
    
    return result;
}

AudioSegment AudioSyncTools::fadeOut(const AudioSegment& segment, qint64 samples) {
    AudioSegment result = segment;
    
    qint64 fadeSamples = std::min(samples, (qint64)result.samples.size());
    qint64 startIdx = result.samples.size() - fadeSamples;
    
    for (qint64 i = 0; i < fadeSamples; ++i) {
        float gain = 1.0f - (float)i / fadeSamples;
        result.samples[startIdx + i] *= gain;
    }
    
    return result;
}

} // namespace Artifact
