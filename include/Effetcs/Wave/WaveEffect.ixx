module;
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.Effect.Wave;




import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;

export namespace Artifact {

// Wave Effect - displaces pixels in a sinusoidal wave pattern
class WaveEffectCPUImpl : public ArtifactEffectImplBase {
private:
    float amplitude_ = 10.0f;      // 波の振幅（ピクセル）
    float frequency_ = 0.1f;       // 波の周波数
    float phase_ = 0.0f;           // 波の位相
    int waveType_ = 0;             // 0=Sine, 1=Cosine
    int orientation_ = 0;          // 0=Horizontal, 1=Vertical

public:
    WaveEffectCPUImpl() = default;

    void setAmplitude(float amp) { amplitude_ = amp; }
    float amplitude() const { return amplitude_; }

    void setFrequency(float freq) { frequency_ = freq; }
    float frequency() const { return frequency_; }

    void setPhase(float phase) { phase_ = phase; }
    float phase() const { return phase_; }

    void setWaveType(int type) { waveType_ = type; }
    int waveType() const { return waveType_; }

    void setOrientation(int ori) { orientation_ = ori; }
    int orientation() const { return orientation_; }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class WaveEffectGPUImpl : public ArtifactEffectImplBase {
private:
    float amplitude_ = 10.0f;
    float frequency_ = 0.1f;
    float phase_ = 0.0f;
    int waveType_ = 0;
    int orientation_ = 0;

public:
    WaveEffectGPUImpl() = default;

    void setAmplitude(float amp) { amplitude_ = amp; }
    float amplitude() const { return amplitude_; }

    void setFrequency(float freq) { frequency_ = freq; }
    float frequency() const { return frequency_; }

    void setPhase(float phase) { phase_ = phase; }
    float phase() const { return phase_; }

    void setWaveType(int type) { waveType_ = type; }
    int waveType() const { return waveType_; }

    void setOrientation(int ori) { orientation_ = ori; }
    int orientation() const { return orientation_; }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class WaveEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;
public:
    WaveEffect();
    ~WaveEffect();

    // Expose properties via AbstractProperty API
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) override;

    void setAmplitude(float amp);
    float amplitude() const;

    void setFrequency(float freq);
    float frequency() const;

    void setPhase(float phase);
    float phase() const;

    void setWaveType(int type);
    int waveType() const;

    void setOrientation(int ori);
    int orientation() const;

    bool supportsGPU() const override {
        return true;
    }
};

};
