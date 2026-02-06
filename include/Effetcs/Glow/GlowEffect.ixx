module;
export module Artifact.Effect.Glow;

import std;
import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;

export namespace Artifact {

class GlowEffectCPUImpl : public ArtifactEffectImplBase {
private:
    float glowGain_ = 1.0f;
    int layerCount_ = 4;
    float baseSigma_ = 5.0f;
    float sigmaGrowth_ = 1.8f;
    float baseAlpha_ = 0.3f;
    float alphaFalloff_ = 0.6f;

public:
    GlowEffectCPUImpl() = default;

    void setGlowGain(float gain) { glowGain_ = gain; }
    float glowGain() const { return glowGain_; }

    void setLayerCount(int count) { layerCount_ = count; }
    int layerCount() const { return layerCount_; }

    void setBaseSigma(float sigma) { baseSigma_ = sigma; }
    float baseSigma() const { return baseSigma_; }

    void setSigmaGrowth(float growth) { sigmaGrowth_ = growth; }
    float sigmaGrowth() const { return sigmaGrowth_; }

    void setBaseAlpha(float alpha) { baseAlpha_ = alpha; }
    float baseAlpha() const { return baseAlpha_; }

    void setAlphaFalloff(float falloff) { alphaFalloff_ = falloff; }
    float alphaFalloff() const { return alphaFalloff_; }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class GlowEffectGPUImpl : public ArtifactEffectImplBase {
private:
    float glowGain_ = 1.0f;
    int layerCount_ = 4;
    float baseSigma_ = 5.0f;
    float sigmaGrowth_ = 1.8f;
    float baseAlpha_ = 0.3f;
    float alphaFalloff_ = 0.6f;

public:
    GlowEffectGPUImpl() = default;

    void setGlowGain(float gain) { glowGain_ = gain; }
    float glowGain() const { return glowGain_; }

    void setLayerCount(int count) { layerCount_ = count; }
    int layerCount() const { return layerCount_; }

    void setBaseSigma(float sigma) { baseSigma_ = sigma; }
    float baseSigma() const { return baseSigma_; }

    void setSigmaGrowth(float growth) { sigmaGrowth_ = growth; }
    float sigmaGrowth() const { return sigmaGrowth_; }

    void setBaseAlpha(float alpha) { baseAlpha_ = alpha; }
    float baseAlpha() const { return baseAlpha_; }

    void setAlphaFalloff(float falloff) { alphaFalloff_ = falloff; }
    float alphaFalloff() const { return alphaFalloff_; }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class GlowEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;
public:
    GlowEffect();
    ~GlowEffect();

    void setGlowGain(float gain);
    float glowGain() const;

    void setLayerCount(int count);
    int layerCount() const;

    void setBaseSigma(float sigma);
    float baseSigma() const;

    void setSigmaGrowth(float growth);
    float sigmaGrowth() const;

    void setBaseAlpha(float alpha);
    float baseAlpha() const;

    void setAlphaFalloff(float falloff);
    float alphaFalloff() const;

    bool supportsGPU() const override {
        return true;
    }
};

};