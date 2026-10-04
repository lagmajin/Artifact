module;
#include <cmath>
#include <limits>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.Limiter;

import Audio.Segment;

namespace Artifact {

namespace {
float finiteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

float sanitizeLimiterSample(float value)
{
    if (std::isfinite(value)) return value;
    if (std::isnan(value)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), value);
}
}

static inline float dbToLinear(float db) {
    return std::pow(10.0f, db / 20.0f);
}

LimiterEffect::LimiterEffect() {
    initializeEngine();
}

void LimiterEffect::initializeEngine() {
    const float sr = static_cast<float>(sampleRate_);
    int lookahead = static_cast<int>(0.005f * sr);
    if (lookahead < 1) lookahead = 1;
    if (lookahead > kMaxLookahead) lookahead = kMaxLookahead;
    lookaheadSamples_ = lookahead;

    // 入力 n を書きながら n-L を読むため、n と n-L が同じスロットを共有しない
    // ことが必要。つまり ringSize_ > L。
    ringSize_ = lookaheadSamples_ + 1;
    delayRing_.assign(static_cast<size_t>(ringSize_) * kMaxChannels, 0.0f);
    writePos_ = 0;
    queueFront_ = 0;
    queueBack_  = 0;
    currentGain_ = 1.0f;
}

void LimiterEffect::reinitOnSampleRate() {
    initializeEngine();
}

void LimiterEffect::setSampleRate(int sampleRate) {
    ArtifactAbstractAudioEffect::setSampleRate(sampleRate);
    initializeEngine();
}

// 末尾に追加する。末尾の peak が新しい peak 以下なら pop してから追加する。
// これにより deque は常に単調減少を保ち、先頭が最大値になる。
void LimiterEffect::pushPeak(qint64 index, float peak) {
    int backPrev = (queueBack_ - 1 + kQueueCapacity) % kQueueCapacity;
    while (queueFront_ != queueBack_ && queuePeak_[backPrev] <= peak) {
        queueBack_ = backPrev;
        backPrev = (queueBack_ - 1 + kQueueCapacity) % kQueueCapacity;
    }
    queueIndex_[queueBack_] = index;
    queuePeak_[queueBack_] = peak;
    queueBack_ = (queueBack_ + 1) % kQueueCapacity;
}

// [windowStart, +inf) に含まれる peak の最大値。呼び出し側で先に
// windowStart 未満のエントリを pop しておく。
float LimiterEffect::windowPeak(qint64 windowStart) {
    while (queueFront_ != queueBack_ && queueIndex_[queueFront_] < windowStart) {
        queueFront_ = (queueFront_ + 1) % kQueueCapacity;
    }
    return (queueFront_ != queueBack_) ? queuePeak_[queueFront_] : 0.0f;
}

void LimiterEffect::process(ArtifactCore::AudioSegment& segment,
                            const ArtifactCore::AudioSegment*) {
    if (!enabled_ || segment.channelData.isEmpty()) return;
    // オフライン書き出しとリアルタイム再生で実レートが異なるため、
    // バスから渡された segment のレートへ同期する。
    syncSampleRate(segment);

    const float sr = static_cast<float>(sampleRate_);
    int numChannels = static_cast<int>(segment.channelData.size());
    int numSamples = (numChannels > 0)
        ? static_cast<int>(segment.channelData[0].size()) : 0;
    for (int ch = 1; ch < numChannels; ++ch) {
        numSamples = std::min(numSamples,
                              static_cast<int>(segment.channelData[ch].size()));
    }
    if (numSamples == 0 || sr <= 0.0f || ringSize_ <= 0) return;

    const int channels = std::min(numChannels, kMaxChannels);
    const int ring = ringSize_;
    const qint64 windowLen = static_cast<qint64>(lookaheadSamples_);

    const float ceilingLinear = dbToLinear(ceiling_);
    const float inputGainLinear = dbToLinear(inputGain_);
    const float releaseCoeff = std::exp(-1.0f / (releaseMs_ * 0.001f * sr));

    for (int i = 0; i < numSamples; ++i) {
        // 出力時刻は writePos_ - L。ここから L サンプル前にさかのぼる。
        const qint64 readIndex = writePos_ - windowLen;
        const int readPos = static_cast<int>(((readIndex % ring) + ring) % ring);

        // ゲインは [readIndex-L+1, readIndex] の最大 peak から決める。
        // ここまでに push 済みの peak しか参照しないので causal になる。
        const float peakNow = windowPeak(readIndex - windowLen + 1);
        float targetGain = 1.0f;
        if (peakNow > ceilingLinear) {
            targetGain = ceilingLinear / peakNow;
        }
        if (targetGain < currentGain_) {
            currentGain_ = targetGain;
        } else {
            currentGain_ = releaseCoeff * currentGain_ +
                          (1.0f - releaseCoeff) * targetGain;
        }
        const float gain = inputGainLinear * currentGain_;

        // 入力サンプルは先にリングへ退避してから出力を書き戻す。
        // 逆順にすると元の入力が上書きされて失われる。
        const int writeIdx = static_cast<int>(((writePos_ % ring) + ring) % ring);
        float peak = 0.0f;
        for (int ch = 0; ch < channels; ++ch) {
            const float sample = finiteOr(segment.channelData[ch][i], 0.0f);
            delayRing_[static_cast<size_t>(ch) * ring + writeIdx] = sample;
            const float val = std::fabs(sample * inputGainLinear);
            if (val > peak) peak = val;
        }

        for (int ch = 0; ch < channels; ++ch) {
            const float delayed =
                delayRing_[static_cast<size_t>(ch) * ring + readPos];
            segment.channelData[ch][i] = sanitizeLimiterSample(delayed * gain);
        }

        pushPeak(writePos_, peak);
        writePos_ += 1;
    }
}

std::vector<AudioEffectParameter> LimiterEffect::getUiParameters() const {
    return {
        {"ceiling",    "Ceiling (dB)",     AudioEffectParameterType::Float, -6.0f,  0.0f,   -0.3f},
        {"release",    "Release (ms)",     AudioEffectParameterType::Float, 5.0f,   500.0f, 50.0f},
        {"input_gain", "Input Gain (dB)",  AudioEffectParameterType::Float, 0.0f,   24.0f,  0.0f},
    };
}

void LimiterEffect::setParameter(const String& name, float value) {
    if      (name == "ceiling")    ceiling_   = std::clamp(finiteOr(value, -0.3f), -6.0f, 0.0f);
    else if (name == "release")    releaseMs_ = std::clamp(finiteOr(value, 50.0f), 5.0f, 500.0f);
    else if (name == "input_gain") inputGain_ = std::clamp(finiteOr(value, 0.0f), 0.0f, 24.0f);
}

float LimiterEffect::getParameter(const String& name) const {
    if      (name == "ceiling")    return ceiling_;
    else if (name == "release")    return releaseMs_;
    else if (name == "input_gain") return inputGain_;
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createLimiterEffect() {
    return std::make_unique<LimiterEffect>();
}

} // namespace Artifact
