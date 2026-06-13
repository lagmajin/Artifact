module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include <QString>
#include <QVariant>

export module Artifact.Effect.Glow.ReactiveGlow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class ReactiveGlowEffect : public ArtifactAbstractEffect {
private:
    float threshold_ = 0.5f;
    float radius_ = 14.0f;
    float intensity_ = 1.0f;
    float reaction_ = 0.8f;
    float saturationWeight_ = 0.65f;
    float tintMix_ = 0.25f;

    void syncImpls();

public:
    ReactiveGlowEffect();
    ~ReactiveGlowEffect() override;

    float threshold() const { return threshold_; }
    void setThreshold(float v) { threshold_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }

    float radius() const { return radius_; }
    void setRadius(float v) { radius_ = std::clamp(v, 0.5f, 48.0f); syncImpls(); }

    float intensity() const { return intensity_; }
    void setIntensity(float v) { intensity_ = std::clamp(v, 0.0f, 4.0f); syncImpls(); }

    float reaction() const { return reaction_; }
    void setReaction(float v) { reaction_ = std::clamp(v, 0.0f, 3.0f); syncImpls(); }

    float saturationWeight() const { return saturationWeight_; }
    void setSaturationWeight(float v) { saturationWeight_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }

    float tintMix() const { return tintMix_; }
    void setTintMix(float v) { tintMix_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
