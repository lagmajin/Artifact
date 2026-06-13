module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include <QString>
#include <QVariant>

export module Artifact.Effect.Glow.ChromaticGlow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class ChromaticGlowEffect : public ArtifactAbstractEffect {
private:
    float threshold_ = 0.62f;
    float radius_ = 12.0f;
    float intensity_ = 1.0f;
    float dispersion_ = 0.35f;
    float angle_ = 35.0f;
    float tintMix_ = 0.2f;

    void syncImpls();

public:
    ChromaticGlowEffect();
    ~ChromaticGlowEffect() override;

    float threshold() const { return threshold_; }
    void setThreshold(float v) { threshold_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }

    float radius() const { return radius_; }
    void setRadius(float v) { radius_ = std::clamp(v, 0.5f, 48.0f); syncImpls(); }

    float intensity() const { return intensity_; }
    void setIntensity(float v) { intensity_ = std::clamp(v, 0.0f, 4.0f); syncImpls(); }

    float dispersion() const { return dispersion_; }
    void setDispersion(float v) { dispersion_ = std::clamp(v, 0.0f, 2.0f); syncImpls(); }

    float angle() const { return angle_; }
    void setAngle(float v) { angle_ = v; syncImpls(); }

    float tintMix() const { return tintMix_; }
    void setTintMix(float v) { tintMix_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
