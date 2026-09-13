module;
#include <algorithm>
#include <cmath>
#include <cstdint>
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

    static constexpr const char* kGpuGenericKeyString = "edge_bloom";
    static constexpr std::uint32_t kGpuGenericKey =
        gpuGenericKeyFromString(kGpuGenericKeyString);
    std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Generic;
        node.genericKey = kGpuGenericKey;
        node.parameters[0] = threshold_;
        node.parameters[1] = thresholdSoftness_;
        node.parameters[2] = radius_;
        node.parameters[3] = amount_;
        node.parameters[4] = edgeBoost_;
        node.parameters[5] = tintMix_;
        node.parameters[6] = static_cast<float>(quality_);
        node.resolutionScaledParameterMask = (1u << 2);
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
