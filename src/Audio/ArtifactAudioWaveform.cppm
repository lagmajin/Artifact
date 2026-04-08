module;

#include <QRandomGenerator>
#include <QDebug>
#include <wobjectimpl.h>



//#include <wobjectimpl.h>

module Artifact.Audio.Waveform;

import std;
import Audio.Segment;

namespace Artifact {

namespace {

	

QVector<float> monoSamples(const AudioSegment& segment)
{
    if (segment.channelData.isEmpty()) {
        return {};
    }
    if (segment.channelData.size() == 1) {
        return segment.channelData[0];
    }

    const int frames = segment.frameCount();
    QVector<float> mono(frames, 0.0f);
    const int channels = segment.channelCount();
    for (int ch = 0; ch < channels; ++ch) {
        const auto& data = segment.channelData[ch];
        const int n = std::min(frames, static_cast<int>(data.size()));
        for (int i = 0; i < n; ++i) {
            mono[i] += data[i];
        }
    }
    const float inv = 1.0f / static_cast<float>(channels);
    for (int i = 0; i < frames; ++i) {
        mono[i] *= inv;
    }
    return mono;
}

AudioAnalyzer::LevelInfo computeLevelFromVector(const QVector<float>& samples)
{
    AudioAnalyzer::LevelInfo info;
    if (samples.isEmpty()) {
        return info;
    }

    float peak = 0.0f;
    float sumSquares = 0.0f;

    for (int i = 0; i < samples.size(); ++i) {
        const float s = samples[i];
        const float absVal = std::abs(s);
        if (absVal > peak) {
            peak = absVal;
            info.peakSample = i;
        }
        sumSquares += s * s;
    }

    info.rms = std::sqrt(sumSquares / static_cast<float>(samples.size()));
    info.peak = peak;
    info.average = sumSquares / static_cast<float>(samples.size());
    return info;
}

float linearToDbValue(float linear)
{
    return (linear > 0.0f) ? 20.0f * std::log10(linear) : -60.0f;
}

float dbToLinearValue(float db)
{
    return std::pow(10.0f, db / 20.0f);
}

} // namespace

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

    const QVector<float> mono = monoSamples(segment);
    if (mono.isEmpty() || displayWidth <= 0) {
        return data;
    }

    const int numSamples = mono.size();
    const int samplesPerPixel = std::max(1, numSamples / displayWidth);

    data.peaks.resize(displayWidth);
    data.rms.resize(displayWidth);
    data.displayPoints.reserve(displayWidth * 2);

    float minVal = 0.0f;
    float maxVal = 0.0f;

    for (int i = 0; i < displayWidth; ++i) {
        const int start = i * samplesPerPixel;
        const int end = std::min(start + samplesPerPixel, numSamples);

        if (start >= numSamples) {
            break;
        }

        float peak = 0.0f;
        float sumSquares = 0.0f;

        for (int j = start; j < end; ++j) {
            const float val = mono[j];
            const float absVal = std::abs(val);
            if (absVal > peak) {
                peak = absVal;
            }
            sumSquares += val * val;
        }

        const float rms = std::sqrt(sumSquares / std::max(1, end - start));
        data.peaks[i] = peak;
        data.rms[i] = rms;

        minVal = std::min(minVal, -peak);
        maxVal = std::max(maxVal, peak);

        data.displayPoints.append(QPointF(i, -peak));
        data.displayPoints.append(QPointF(i, peak));
    }

    data.minSample = minVal;
    data.maxSample = maxVal;
    return data;
}

WaveformData AudioWaveformGenerator::generateRange(const AudioSegment& segment,
                                                   qint64 startSample,
                                                   qint64 sampleCount,
                                                   int displayWidth) {
    WaveformData data;
    data.width = displayWidth;
    data.sampleRate = segment.sampleRate;
    data.startFrame = segment.startFrame + startSample;

    const QVector<float> mono = monoSamples(segment);
    if (mono.isEmpty() || displayWidth <= 0 || startSample < 0 || sampleCount <= 0) {
        return data;
    }

    const int start = static_cast<int>(std::min<qint64>(startSample, mono.size()));
    const int end = static_cast<int>(std::min<qint64>(startSample + sampleCount, mono.size()));
    if (start >= end) {
        return data;
    }

    AudioSegment sub;
    sub.sampleRate = segment.sampleRate;
    sub.startFrame = data.startFrame;
    sub.layout = AudioChannelLayout::Mono;
    sub.channelData.resize(1);
    sub.channelData[0] = mono.sliced(start, end - start);
    return generate(sub, displayWidth);
}

WaveformData AudioWaveformGenerator::generateStereo(const AudioSegment& left,
                                                    const AudioSegment& right,
                                                    int displayWidth) {
    const QVector<float> leftMono = monoSamples(left);
    const QVector<float> rightMono = monoSamples(right);

    AudioSegment combined;
    combined.sampleRate = left.sampleRate;
    combined.startFrame = left.startFrame;
    combined.layout = AudioChannelLayout::Mono;
    combined.channelData.resize(1);

    const int n = std::min(leftMono.size(), rightMono.size());
    combined.channelData[0].resize(n);
    for (int i = 0; i < n; ++i) {
        combined.channelData[0][i] = (leftMono[i] + rightMono[i]) * 0.5f;
    }

    return generate(combined, displayWidth);
}

void AudioWaveformGenerator::generateAsync(const AudioSegment& segment, int displayWidth) {
    auto result = generate(segment, displayWidth);
    emit waveformGenerated(result);
}

// ==================== AudioAnalyzer ====================

class AudioAnalyzer::Impl {
public:
    std::vector<std::complex<float>> fft(const std::vector<float>& input) {
        const int n = static_cast<int>(input.size());
        std::vector<std::complex<float>> output(n);

        for (int k = 0; k < n; ++k) {
            std::complex<float> sum = 0;
            for (int t = 0; t < n; ++t) {
                const float angle = -2.0f * 3.14159f * k * t / std::max(1, n);
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
    setWindow(WindowType::Hann);
}

AudioAnalyzer::~AudioAnalyzer() = default;

void AudioAnalyzer::setFFTSize(int size) {
    fftSize_ = size;
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
            for (int i = 0; i < fftSize_; ++i) {
                window_[i] = 1.0f;
            }
            break;
    }
}

std::vector<float> AudioAnalyzer::computeSpectrum(const AudioSegment& segment) {
    const QVector<float> mono = monoSamples(segment);
    if (mono.size() < fftSize_) {
        return {};
    }

    std::vector<float> windowed(fftSize_);
    for (int i = 0; i < fftSize_; ++i) {
        windowed[i] = mono[i] * window_[i];
    }

    const auto spectrum = impl_->fft(windowed);

    std::vector<float> magnitude(fftSize_ / 2);
    for (int i = 0; i < fftSize_ / 2; ++i) {
        magnitude[i] = std::abs(spectrum[i]) / static_cast<float>(fftSize_);
    }

    return magnitude;
}

std::vector<float> AudioAnalyzer::computeMagnitude(const AudioSegment& segment) {
    return computeSpectrum(segment);
}

std::vector<float> AudioAnalyzer::computePhase(const AudioSegment& segment) {
    const QVector<float> mono = monoSamples(segment);
    if (mono.size() < fftSize_) {
        return {};
    }

    std::vector<float> windowed(fftSize_);
    for (int i = 0; i < fftSize_; ++i) {
        windowed[i] = mono[i] * window_[i];
    }

    const auto spectrum = impl_->fft(windowed);

    std::vector<float> phase(fftSize_ / 2);
    for (int i = 0; i < fftSize_ / 2; ++i) {
        phase[i] = std::arg(spectrum[i]);
    }

    return phase;
}

std::vector<std::vector<float>> AudioAnalyzer::computeSpectrogram(const AudioSegment& segment,
                                                                  int timeSteps) {
    std::vector<std::vector<float>> spectrogram;
    const QVector<float> mono = monoSamples(segment);
    if (mono.isEmpty() || timeSteps <= 0) {
        return spectrogram;
    }

    int stepSize = mono.size() / timeSteps;
    if (stepSize < fftSize_) {
        stepSize = fftSize_;
    }

    for (int t = 0; t < timeSteps; ++t) {
        const int start = t * stepSize;
        if (start + fftSize_ > mono.size()) {
            break;
        }

        AudioSegment frame;
        frame.sampleRate = segment.sampleRate;
        frame.layout = AudioChannelLayout::Mono;
        frame.channelData.resize(1);
        frame.channelData[0] = mono.sliced(start, fftSize_);

        auto spec = computeSpectrum(frame);
        spectrogram.push_back(std::move(spec));
    }

    return spectrogram;
}

AudioAnalyzer::LevelInfo AudioAnalyzer::computeLevel(const AudioSegment& segment) {
    return computeLevelFromVector(monoSamples(segment));
}

float AudioAnalyzer::computeCorrelation(const AudioSegment& left,
                                        const AudioSegment& right) {
    const QVector<float> l = monoSamples(left);
    const QVector<float> r = monoSamples(right);
    const int n = std::min(l.size(), r.size());
    if (n <= 0) {
        return 0.0f;
    }

    float sumXY = 0.0f;
    float sumX2 = 0.0f;
    float sumY2 = 0.0f;

    for (int i = 0; i < n; ++i) {
        sumXY += l[i] * r[i];
        sumX2 += l[i] * l[i];
        sumY2 += r[i] * r[i];
    }

    if (sumX2 == 0.0f || sumY2 == 0.0f) {
        return 0.0f;
    }

    return sumXY / std::sqrt(sumX2 * sumY2);
}

std::vector<AudioAnalyzer::BandInfo> AudioAnalyzer::computeBands(const AudioSegment& segment) {
    std::vector<BandInfo> bands = {
        {"Sub Bass", 20, 60, 0},
        {"Bass", 60, 250, 0},
        {"Low Mid", 250, 500, 0},
        {"Mid", 500, 2000, 0},
        {"High Mid", 2000, 4000, 0},
        {"Presence", 4000, 6000, 0},
        {"Brilliance", 6000, 20000, 0}
    };

    const auto spectrum = computeSpectrum(segment);
    if (spectrum.empty()) {
        return bands;
    }

    const float binWidth = static_cast<float>(segment.sampleRate) / static_cast<float>(fftSize_);

    for (auto& band : bands) {
        const int lowBin = static_cast<int>(band.lowFreq / binWidth);
        const int highBin = static_cast<int>(band.highFreq / binWidth);

        float energy = 0.0f;
        for (int b = lowBin; b < highBin && b < static_cast<int>(spectrum.size()); ++b) {
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
    const auto level = computeLevelFromVector(monoSamples(segment));
    const float db = linearToDbValue(level.rms);
    const float peakDb = linearToDbValue(level.peak);

    impl_->leftLevel_ = db;
    impl_->rightLevel_ = db;

    if (peakDb > impl_->leftPeak_) {
        impl_->leftPeak_ = peakDb;
        impl_->leftPeakHoldCounter_ = impl_->peakHoldFrames_;
    }

    if (impl_->leftPeakHoldCounter_ > 0) {
        impl_->leftPeakHoldCounter_--;
    } else {
        impl_->leftPeak_ = db;
    }

    emit levelsChanged(impl_->leftLevel_, impl_->rightLevel_);

    if (peakDb >= 0.0f) {
        emit clipDetected();
    }
}

void AudioLevelMeter::processStereo(const AudioSegment& left, const AudioSegment& right) {
    const auto leftLevel = computeLevelFromVector(monoSamples(left));
    const auto rightLevel = computeLevelFromVector(monoSamples(right));

    const float leftDb = linearToDbValue(leftLevel.rms);
    const float rightDb = linearToDbValue(rightLevel.rms);

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
    return linearToDbValue(linear);
}

float AudioLevelMeter::dbToLinear(float db) {
    return dbToLinearValue(db);
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

    if (segment.channelData.isEmpty() || rate <= 0.0f) {
        return result;
    }

    const int channels = segment.channelCount();
    const int oldFrames = segment.frameCount();
    const int newFrames = static_cast<int>(oldFrames / rate);

    result.channelData.resize(channels);
    for (int ch = 0; ch < channels; ++ch) {
        const auto& src = segment.channelData[ch];
        auto& dst = result.channelData[ch];
        dst.resize(newFrames);

        for (int i = 0; i < newFrames; ++i) {
            const float srcIdxF = i * rate;
            const int idx0 = static_cast<int>(srcIdxF);
            const int idx1 = std::min(idx0 + 1, std::max(0, static_cast<int>(src.size()) - 1));
            const float frac = srcIdxF - static_cast<float>(idx0);
            dst[i] = src[idx0] * (1.0f - frac) + src[idx1] * frac;
        }
    }

    return result;
}

AudioSegment AudioSyncTools::pitchShift(const AudioSegment& segment, float semitones) {
    const float rate = std::pow(2.0f, semitones / 12.0f);
    return timeStretch(segment, rate);
}

AudioSyncTools::AlignmentResult AudioSyncTools::align(const AudioSegment& reference,
                                                      const AudioSegment& toAlign) {
    AlignmentResult result;
    result.offset = 0;
    result.correlation = 0.0f;

    const QVector<float> ref = monoSamples(reference);
    const QVector<float> trg = monoSamples(toAlign);
    const int refN = ref.size();
    const int trgN = trg.size();
    if (refN == 0 || trgN == 0) {
        return result;
    }

    const int maxLag = std::min(refN, trgN) / 4;

    for (int lag = -maxLag; lag <= maxLag; ++lag) {
        float corr = 0.0f;
        int count = 0;

        for (int i = 0; i < refN; ++i) {
            const int j = i + lag;
            if (j >= 0 && j < trgN) {
                corr += ref[i] * trg[j];
                ++count;
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
    const QVector<float> mono = monoSamples(segment);
    if (mono.isEmpty()) {
        return beats;
    }

    const int windowSize = std::max(1, segment.sampleRate / 10);
    const float threshold = 0.5f;

    float prevEnergy = 0.0f;

    for (int i = 0; i + windowSize < mono.size(); i += std::max(1, windowSize / 2)) {
        float energy = 0.0f;
        for (int j = i; j < i + windowSize && j < mono.size(); ++j) {
            energy += mono[j] * mono[j];
        }
        energy /= static_cast<float>(windowSize);

        if (energy > prevEnergy * 1.5f && energy > threshold) {
            beats.push_back(static_cast<qint64>(i));
        }
        prevEnergy = energy;
    }

    return beats;
}

float AudioSyncTools::detectTempo(const AudioSegment& segment) {
    const auto beats = detectBeats(segment);
    if (beats.size() < 2) {
        return 120.0f;
    }

    float avgInterval = 0.0f;
    for (size_t i = 1; i < beats.size(); ++i) {
        avgInterval += static_cast<float>(beats[i] - beats[i - 1]);
    }
    avgInterval /= static_cast<float>(beats.size() - 1);

    float bpm = static_cast<float>(segment.sampleRate) * 60.0f / std::max(1.0f, avgInterval);
    while (bpm < 60.0f) bpm *= 2.0f;
    while (bpm > 180.0f) bpm /= 2.0f;

    return bpm;
}

AudioSegment AudioSyncTools::normalize(const AudioSegment& segment, float targetDb) {
    AudioSegment result = segment;

    const auto level = computeLevelFromVector(monoSamples(segment));
    const float currentDb = linearToDbValue(level.rms);
    const float gainDb = targetDb - currentDb;
    const float gain = dbToLinearValue(gainDb);

    for (auto& channel : result.channelData) {
        for (auto& sample : channel) {
            sample *= gain;
        }
    }

    return result;
}

AudioSegment AudioSyncTools::fadeIn(const AudioSegment& segment, qint64 samples) {
    AudioSegment result = segment;
    if (result.channelData.isEmpty()) {
        return result;
    }

    const qint64 fadeSamples = std::min(samples, static_cast<qint64>(result.frameCount()));
    if (fadeSamples <= 0) {
        return result;
    }

    for (auto& channel : result.channelData) {
        for (qint64 i = 0; i < fadeSamples && i < channel.size(); ++i) {
            const float gain = static_cast<float>(i) / static_cast<float>(fadeSamples);
            channel[static_cast<int>(i)] *= gain;
        }
    }

    return result;
}

AudioSegment AudioSyncTools::fadeOut(const AudioSegment& segment, qint64 samples) {
    AudioSegment result = segment;
    if (result.channelData.isEmpty()) {
        return result;
    }

    const qint64 fadeSamples = std::min(samples, static_cast<qint64>(result.frameCount()));
    if (fadeSamples <= 0) {
        return result;
    }

    const qint64 startIdx = static_cast<qint64>(result.frameCount()) - fadeSamples;

    for (auto& channel : result.channelData) {
        for (qint64 i = 0; i < fadeSamples; ++i) {
            const float gain = 1.0f - static_cast<float>(i) / static_cast<float>(fadeSamples);
            channel[static_cast<int>(startIdx + i)] *= gain;
        }
    }

    return result;
}

} // namespace Artifact
