module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include <QString>
#include <QVariant>

export module Artifact.Effect.Glow.EdgeBloom;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class EdgeBloomEffect : public ArtifactAbstractEffect {
private:
    float threshold_ = 0.65f;
    float thresholdSoftness_ = 0.0f;
    float radius_ = 10.0f;
    int quality_ = 1;
    float amount_ = 1.15f;
    float edgeBoost_ = 1.8f;
    float tintMix_ = 0.35f;

    void syncImpls();

public:
    EdgeBloomEffect();
    ~EdgeBloomEffect() override;

    float threshold() const { return threshold_; }
    void setThreshold(float v) { threshold_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.65f; syncImpls(); }

    float thresholdSoftness() const { return thresholdSoftness_; }
    void setThresholdSoftness(float v) { thresholdSoftness_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f; syncImpls(); }

    float radius() const { return radius_; }
    void setRadius(float v) { radius_ = std::isfinite(v) ? std::clamp(v, 0.5f, 32.0f) : 10.0f; syncImpls(); }
    int quality() const { return quality_; }
    void setQuality(int v) { quality_ = std::clamp(v, 0, 2); syncImpls(); }

    float amount() const { return amount_; }
    void setAmount(float v) { amount_ = std::isfinite(v) ? std::clamp(v, 0.0f, 4.0f) : 1.15f; syncImpls(); }

    float edgeBoost() const { return edgeBoost_; }
    void setEdgeBoost(float v) { edgeBoost_ = std::isfinite(v) ? std::clamp(v, 0.0f, 4.0f) : 1.8f; syncImpls(); }

    float tintMix() const { return tintMix_; }
    void setTintMix(float v) { tintMix_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.35f; syncImpls(); }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
