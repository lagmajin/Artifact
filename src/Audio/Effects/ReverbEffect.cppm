module;
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>
#include <memory>
#include <cstdint>
#include <QVector>
module Artifact.Audio.Effects.Reverb;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.DSP.DelayLine;
import Audio.DSP.AllPassFilter;
import Audio.DSP.LFO;

namespace Artifact {

// FDN delay line lengths (coprime primes ~41-77ms at 44.1kHz)
static constexpr int kFDNDelays[] = { 1811, 1931, 2111, 2237, 2399, 2521, 2689, 2803 };

// FWHT-8: fast in-place Walsh-Hadamard transform (unnormalized)
void ReverbEffect::fwht8(float* x) {
    float t;
    for (int len = 1; len < 8; len <<= 1) {
        for (int i = 0; i < 8; i += len * 2) {
            for (int j = 0; j < len; ++j) {
                t = x[i + j] + x[i + j + len];
                x[i + j + len] = x[i + j] - x[i + j + len];
                x[i + j] = t;
            }
        }
    }
}

ReverbEffect::ReverbEffect() {
    initEngine();
}

void ReverbEffect::setSampleRate(int sampleRate) {
    sampleRate_ = sampleRate;
    initEngine();
}

float ReverbEffect::scaleDelay(float refSamples) const {
    return refSamples * (static_cast<float>(sampleRate_) / kRefSampleRate) * size_;
}

void ReverbEffect::initEngine() {
    initDattorro();
    initFDN();
}

// ── Dattorro Plate initialisation ──────────────────────────────────
void ReverbEffect::initDattorro() {
    float sr = static_cast<float>(sampleRate_);

    preDelay_.initialize(0.25f, sr);

    for (int i = 0; i < 2; ++i) {
        inputDiff1_[i].initialize(0.05f, sr);
        inputDiff2_[i].initialize(0.05f, sr);
    }
    inputDiff1_[0].setParametersSamples(scaleDelay(kInDiff1a), diffusion_ * 0.75f);
    inputDiff1_[1].setParametersSamples(scaleDelay(kInDiff1b), diffusion_ * 0.75f);
    inputDiff2_[0].setParametersSamples(scaleDelay(kInDiff2a), diffusion_ * 0.625f);
    inputDiff2_[1].setParametersSamples(scaleDelay(kInDiff2b), diffusion_ * 0.625f);

    for (int i = 0; i < 2; ++i) {
        tankDelay_[i].initialize(2.0f, sr);
        tankAP_[i].initialize(0.1f, sr);
    }
    tankAP_[0].setParametersSamples(scaleDelay(kTankAP1), -decay_ * 0.7f);
    tankAP_[1].setParametersSamples(scaleDelay(kTankAP2), -decay_ * 0.7f);

    lfoPhase_[0] = 0.0f;
    lfoPhase_[1] = 0.0f;
    tankAccum_[0] = 0.0f;
    tankAccum_[1] = 0.0f;
    dampState_[0] = 0.0f;
    dampState_[1] = 0.0f;
}

// ── FDN Hall initialisation ────────────────────────────────────────
void ReverbEffect::initFDN() {
    float sr = static_cast<float>(sampleRate_);

    for (int i = 0; i < kNumFDNLines; ++i) {
        int len = static_cast<int>(kFDNDelays[i] * (sr / kRefSampleRate) * size_);
        if (len < 64) len = 64;
        fdnLines_[i].length = len;
        fdnLines_[i].buffer.assign(len, 0.0f);
        fdnLines_[i].writeIndex = 0;
        fdnLines_[i].state = 0.0f;

        // Per-line feedback gain (gentle rolloff for smoother decay)
        float t = static_cast<float>(i) / (kNumFDNLines - 1);
        fdnLines_[i].fbGain = 0.85f - t * 0.15f;

        // Damping coefficient (more damping on longer lines)
        fdnLines_[i].dampCoeff = 0.05f + t * 0.15f;

        // Input mix: spread stereo across lines
        fdnInputMix_[i] = (i % 2 == 0) ? 0.5f : 0.5f;

        // Output mix: alternating signs for stereo spread
        fdnOutputMix_[i] = (i < 4) ? 1.0f : -1.0f;

        // Early reflection tap offset (fraction of line length)
        erTaps_[i] = 0.03f + t * 0.12f;
    }
    // Normalise output mix
    float sum = 0.0f;
    for (int i = 0; i < kNumFDNLines; ++i) sum += std::abs(fdnOutputMix_[i]);
    float inv = 1.0f / sum;
    for (int i = 0; i < kNumFDNLines; ++i) fdnOutputMix_[i] *= inv;
}

// ── Main process ───────────────────────────────────────────────────
void ReverbEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment*) {
    if (!enabled_) return;

    int channels = segment.channelCount();
    int frames = segment.frameCount();
    if (frames <= 0) return;

    float* ch0 = segment.channelData[0].data();
    float* ch1 = (channels > 1) ? segment.channelData[1].data() : nullptr;

    // Early-out: nothing to process for fully dry signal
    if (wetLevel_ <= 0.0f) return;

    for (int i = 0; i < frames; ++i) {
        float inL = ch0[i];
        float inR = ch1 ? ch1[i] : inL;

        float wetL = 0.0f, wetR = 0.0f;
        switch (algorithm_) {
            case ReverbAlgorithm::DattorroPlate:
                processDattorroSample(inL, inR, wetL, wetR);
                break;
            case ReverbAlgorithm::FDNHall:
                processFDNSample(inL, inR, wetL, wetR);
                break;
            case ReverbAlgorithm::Hybrid:
                processHybridSample(inL, inR, wetL, wetR);
                break;
        }

        ch0[i] = inL * dryLevel_ + wetL * wetLevel_;
        if (ch1) ch1[i] = inR * dryLevel_ + wetR * wetLevel_;
    }
}

// ── Dattorro Plate per-sample ──────────────────────────────────────
void ReverbEffect::processDattorroSample(float inL, float inR, float& outL, float& outR) {
    float sr = static_cast<float>(sampleRate_);
    float monoIn = (inL + inR) * 0.5f;

    // Pre-delay
    preDelay_.write(monoIn);
    float pre = preDelay_.read(preDelayMs_ * 0.001f * sr);

    // LFO (sinusoidal)
    float lfo0 = std::sin(lfoPhase_[0]);
    float lfo1 = std::sin(lfoPhase_[1]);
    lfoPhase_[0] += 2.0f * 3.14159265f * modRate_ / sr;
    lfoPhase_[1] += 2.0f * 3.14159265f * modRate_ * 1.07f / sr;
    if (lfoPhase_[0] >= 2.0f * 3.14159265f) lfoPhase_[0] -= 2.0f * 3.14159265f;
    if (lfoPhase_[0] < 0.0f) lfoPhase_[0] += 2.0f * 3.14159265f;
    if (lfoPhase_[1] >= 2.0f * 3.14159265f) lfoPhase_[1] -= 2.0f * 3.14159265f;
    if (lfoPhase_[1] < 0.0f) lfoPhase_[1] += 2.0f * 3.14159265f;

    float modAmt = modDepth_ * 4.0f; // ±4 sample modulation

    // Input diffusion (4 all-pass series)
    float d = pre;
    d = inputDiff1_[0].process(d, lfo0 * modAmt);
    d = inputDiff1_[1].process(d, lfo1 * modAmt);
    d = inputDiff2_[0].process(d, lfo0 * modAmt * 0.5f);
    d = inputDiff2_[1].process(d, lfo1 * modAmt * 0.5f);

    // Cross-coupled tank
    float tankInL = d + tankAccum_[1] * decay_;
    float tankInR = d + tankAccum_[0] * decay_;

    // --- Left tank path ---
    float tdL = scaleDelay(kTankDelay1);
    tankDelay_[0].write(tankInL);
    float delL = tankDelay_[0].read(tdL + lfo0 * modAmt * 2.0f);
    // One-pole LPF + wet/dry mix for damping
    constexpr float kDampCoeff = 0.4f;
    dampState_[0] += kDampCoeff * (delL - dampState_[0]);
    float apInL = delL * (1.0f - decayHF_) + dampState_[0] * decayHF_;
    float apL = tankAP_[0].process(apInL, lfo0 * modAmt);

    // --- Right tank path ---
    float tdR = scaleDelay(kTankDelay2);
    tankDelay_[1].write(tankInR);
    float delR = tankDelay_[1].read(tdR + lfo1 * modAmt * 2.0f);
    dampState_[1] += kDampCoeff * (delR - dampState_[1]);
    float apInR = delR * (1.0f - decayHF_) + dampState_[1] * decayHF_;
    float apR = tankAP_[1].process(apInR, lfo1 * modAmt);

    tankAccum_[0] = apL;
    tankAccum_[1] = apR;

    // Stereo output with width control
    float w = stereoWidth_;
    outL = apL * (1.0f - w * 0.5f) + apR * w * 0.5f;
    outR = apR * (1.0f - w * 0.5f) + apL * w * 0.5f;
}

// ── FDN Hall per-sample ────────────────────────────────────────────
void ReverbEffect::processFDNSample(float inL, float inR, float& outL, float& outR) {
    float sr = static_cast<float>(sampleRate_);
    float lfo = std::sin(lfoPhase_[0]);
    lfoPhase_[0] += 2.0f * 3.14159265f * modRate_ / sr;
    if (lfoPhase_[0] >= 2.0f * 3.14159265f) lfoPhase_[0] -= 2.0f * 3.14159265f;
    if (lfoPhase_[0] < 0.0f) lfoPhase_[0] += 2.0f * 3.14159265f;

    // 1. Read delay-line outputs (stored at writeIndex = oldest sample)
    float outputs[kNumFDNLines];
    for (int i = 0; i < kNumFDNLines; ++i) {
        outputs[i] = fdnLines_[i].buffer[fdnLines_[i].writeIndex];
    }

    // 2. Compute feedback via Hadamard mix of outputs
    float feedback[kNumFDNLines];
    for (int i = 0; i < kNumFDNLines; ++i) feedback[i] = outputs[i];
    fwht8(feedback);
    constexpr float kInv8 = 0.125f;
    for (int i = 0; i < kNumFDNLines; ++i) feedback[i] *= kInv8;

    // 3. Write to each delay line: input + damped feedback
    for (int i = 0; i < kNumFDNLines; ++i) {
        auto& line = fdnLines_[i];
        float inSig = (i % 2 == 0 ? inL : inR) * fdnInputMix_[i];

        float fbSig = feedback[i] * line.fbGain;
        line.state += line.dampCoeff * (fbSig - line.state);
        float dampFb = fbSig * (1.0f - decayHF_) + line.state * decayHF_;

        // Write and advance
        line.buffer[line.writeIndex] = inSig + dampFb;
        line.writeIndex = (line.writeIndex + 1) % line.length;
    }

    // 4. Mix delay-line outputs → stereo (not feedback, for natural tail)
    //    Also tap early reflections from partial reads
    float erL = 0.0f, erR = 0.0f;
    outL = 0.0f;
    outR = 0.0f;
    for (int i = 0; i < kNumFDNLines; ++i) {
        outL += outputs[i] * fdnOutputMix_[i];
        outR += outputs[i] * fdnOutputMix_[kNumFDNLines - 1 - i];

        // Early reflection tap: read from near the write head
        auto& line = fdnLines_[i];
        int erOffset = static_cast<int>(line.length * erTaps_[i] * erDelay_);
        if (erOffset < 1) erOffset = 1;
        if (erOffset >= line.length) erOffset = line.length - 1;
        int erIdx = line.writeIndex - erOffset;
        if (erIdx < 0) erIdx += line.length;
        float erTap = line.buffer[erIdx] * erLevel_;
        if (i % 2 == 0) erL += erTap; else erR += erTap;
    }
    outL += erL;
    outR += erR;

    // Normalize output level
    constexpr float kOutScale = 0.5f;
    outL *= kOutScale;
    outR *= kOutScale;
}

// ── Hybrid: Dattorro diffusers → FDN tail per-sample ──────────────
void ReverbEffect::processHybridSample(float inL, float inR, float& outL, float& outR) {
    float sr = static_cast<float>(sampleRate_);
    float monoIn = (inL + inR) * 0.5f;

    preDelay_.write(monoIn);
    float pre = preDelay_.read(preDelayMs_ * 0.001f * sr);

    float lfo0 = std::sin(lfoPhase_[0]);
    float lfo1 = std::sin(lfoPhase_[1]);
    lfoPhase_[0] += 2.0f * 3.14159265f * modRate_ / sr;
    lfoPhase_[1] += 2.0f * 3.14159265f * modRate_ * 1.07f / sr;
    if (lfoPhase_[0] >= 2.0f * 3.14159265f) lfoPhase_[0] -= 2.0f * 3.14159265f;
    if (lfoPhase_[0] < 0.0f) lfoPhase_[0] += 2.0f * 3.14159265f;
    if (lfoPhase_[1] >= 2.0f * 3.14159265f) lfoPhase_[1] -= 2.0f * 3.14159265f;
    if (lfoPhase_[1] < 0.0f) lfoPhase_[1] += 2.0f * 3.14159265f;

    float modAmt = modDepth_ * 4.0f;

    // Dattorro input diffusion
    float d = pre;
    d = inputDiff1_[0].process(d, lfo0 * modAmt);
    d = inputDiff1_[1].process(d, lfo1 * modAmt);
    d = inputDiff2_[0].process(d, lfo0 * modAmt * 0.5f);
    d = inputDiff2_[1].process(d, lfo1 * modAmt * 0.5f);

    // Feed into FDN tank
    float outputs[kNumFDNLines];
    for (int i = 0; i < kNumFDNLines; ++i) {
        outputs[i] = fdnLines_[i].buffer[fdnLines_[i].writeIndex];
    }

    float feedback[kNumFDNLines];
    for (int i = 0; i < kNumFDNLines; ++i) feedback[i] = outputs[i];
    fwht8(feedback);
    constexpr float kInv8 = 0.125f;
    for (int i = 0; i < kNumFDNLines; ++i) feedback[i] *= kInv8;

    for (int i = 0; i < kNumFDNLines; ++i) {
        auto& line = fdnLines_[i];
        float inSig = d * fdnInputMix_[i];

        float fbSig = feedback[i] * line.fbGain;
        line.state += line.dampCoeff * (fbSig - line.state);
        float dampFb = fbSig * (1.0f - decayHF_) + line.state * decayHF_;

        line.buffer[line.writeIndex] = inSig + dampFb;
        line.writeIndex = (line.writeIndex + 1) % line.length;
    }

    // Output mix + early reflections
    float erL = 0.0f, erR = 0.0f;
    outL = 0.0f;
    outR = 0.0f;
    for (int i = 0; i < kNumFDNLines; ++i) {
        outL += outputs[i] * fdnOutputMix_[i];
        outR += outputs[i] * fdnOutputMix_[kNumFDNLines - 1 - i];

        auto& line = fdnLines_[i];
        int erOffset = static_cast<int>(line.length * erTaps_[i] * erDelay_);
        if (erOffset < 1) erOffset = 1;
        if (erOffset >= line.length) erOffset = line.length - 1;
        int erIdx = line.writeIndex - erOffset;
        if (erIdx < 0) erIdx += line.length;
        float erTap = line.buffer[erIdx] * erLevel_;
        if (i % 2 == 0) erL += erTap; else erR += erTap;
    }
    outL += erL;
    outR += erR;

    constexpr float kOutScale = 0.5f;
    outL *= kOutScale;
    outR *= kOutScale;
}

// ── Parameter system ───────────────────────────────────────────────
std::vector<AudioEffectParameter> ReverbEffect::getUiParameters() const {
    return {
        {"algorithm",    "Algorithm",       AudioEffectParameterType::Enum,  0.0f,    2.0f,    0.0f,
         {"Dattorro Plate", "FDN Hall", "Hybrid"}},
        {"pre_delay",    "Pre-Delay (ms)",  AudioEffectParameterType::Float, 0.0f,  200.0f,   20.0f},
        {"decay",        "Decay",           AudioEffectParameterType::Float, 0.0f,    0.99f,   0.75f},
        {"decay_lf_mult","Decay LF Mult",   AudioEffectParameterType::Float, 0.2f,    2.0f,    1.0f},
        {"decay_hf",     "Decay HF",        AudioEffectParameterType::Float, 0.0f,    1.0f,    0.5f},
        {"damping_freq", "Damping Freq",    AudioEffectParameterType::Float,200.0f,20000.0f,8000.0f},
        {"diffusion",    "Diffusion",       AudioEffectParameterType::Float, 0.0f,    1.0f,    0.75f},
        {"density",      "Density",         AudioEffectParameterType::Float, 0.0f,    1.0f,    0.7f},
        {"mod_depth",    "Mod Depth",       AudioEffectParameterType::Float, 0.0f,    1.0f,    0.5f},
        {"mod_rate",     "Mod Rate (Hz)",   AudioEffectParameterType::Float, 0.1f,   20.0f,    0.8f},
        {"size",         "Size",            AudioEffectParameterType::Float, 0.5f,    2.0f,    1.0f},
        {"stereo_width", "Stereo Width",    AudioEffectParameterType::Float, 0.0f,    1.0f,    1.0f},
        {"er_level",     "ER Level",        AudioEffectParameterType::Float, 0.0f,    1.0f,    0.3f},
        {"er_delay",     "ER Delay",        AudioEffectParameterType::Float, 0.0f,    1.0f,    0.5f},
        {"wet_level",    "Wet Level",       AudioEffectParameterType::Float, 0.0f,    1.0f,    0.35f},
        {"dry_level",    "Dry Level",       AudioEffectParameterType::Float, 0.0f,    1.0f,    0.65f},
    };
}

void ReverbEffect::setParameter(const std::string& name, float value) {
    if      (name == "algorithm")    algorithm_   = static_cast<ReverbAlgorithm>(static_cast<int>(value + 0.5f));
    else if (name == "pre_delay")    preDelayMs_  = value;
    else if (name == "decay")        decay_       = value;
    else if (name == "decay_lf_mult")decayLFMult_ = value;
    else if (name == "decay_hf")     decayHF_     = value;
    else if (name == "damping_freq") dampingFreq_ = value;
    else if (name == "diffusion")    diffusion_   = value;
    else if (name == "density")      density_     = value;
    else if (name == "mod_depth")    modDepth_    = value;
    else if (name == "mod_rate")     modRate_     = value;
    else if (name == "size")         size_        = value;
    else if (name == "stereo_width") stereoWidth_ = value;
    else if (name == "er_level")     erLevel_     = value;
    else if (name == "er_delay")     erDelay_     = value;
    else if (name == "wet_level")    wetLevel_    = value;
    else if (name == "dry_level")    dryLevel_    = value;
}

float ReverbEffect::getParameter(const std::string& name) const {
    if      (name == "algorithm")    return static_cast<float>(algorithm_);
    else if (name == "pre_delay")    return preDelayMs_;
    else if (name == "decay")        return decay_;
    else if (name == "decay_lf_mult")return decayLFMult_;
    else if (name == "decay_hf")     return decayHF_;
    else if (name == "damping_freq") return dampingFreq_;
    else if (name == "diffusion")    return diffusion_;
    else if (name == "density")      return density_;
    else if (name == "mod_depth")    return modDepth_;
    else if (name == "mod_rate")     return modRate_;
    else if (name == "size")         return size_;
    else if (name == "stereo_width") return stereoWidth_;
    else if (name == "er_level")     return erLevel_;
    else if (name == "er_delay")     return erDelay_;
    else if (name == "wet_level")    return wetLevel_;
    else if (name == "dry_level")    return dryLevel_;
    return 0.0f;
}

std::string ReverbEffect::getDescription() const {
    switch (algorithm_) {
        case ReverbAlgorithm::DattorroPlate: return "Dattorro Plate Reverb with modulated diffusion and cross-coupled tank";
        case ReverbAlgorithm::FDNHall:       return "FDN Hall Reverb with 8-voice feedback delay network and Hadamard mixing";
        case ReverbAlgorithm::Hybrid:        return "Hybrid reverb: Dattorro input diffusion with FDN tail";
        default:                             return "High-end algorithmic reverb";
    }
}

std::unique_ptr<ArtifactAbstractAudioEffect> createReverbEffect() {
    return std::make_unique<ReverbEffect>();
}

} // namespace Artifact
