module;
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include <QString>
#include <QVariant>

export module Artifact.Effect.Glow.LiquidGlow;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class LiquidGlowEffect : public ArtifactAbstractEffect {
private:
    float threshold_ = 0.55f;
    float radius_ = 16.0f;
    float intensity_ = 1.1f;
    float flowScale_ = 42.0f;
    float distortion_ = 8.0f;
    float phase_ = 0.0f;

    void syncImpls();

public:
    LiquidGlowEffect();
    ~LiquidGlowEffect() override;

    void setThreshold(float value);
    void setRadius(float value);
    void setIntensity(float value);
    void setFlowScale(float value);
    void setDistortion(float value);
    void setPhase(float value);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    static constexpr const char* kGpuGenericKeyString = "liquid_glow";
    static constexpr std::uint32_t kGpuGenericKey =
        gpuGenericKeyFromString(kGpuGenericKeyString);
    std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        // Large radii stay on the CPU reference: the resident separable blur
        // is capped at 16 taps per direction (radius <= 8).
        if (radius_ > 8.0f) return false;
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Generic;
        node.genericKey = kGpuGenericKey;
        node.parameters[0] = threshold_;
        node.parameters[1] = radius_;
        node.parameters[2] = intensity_;
        node.parameters[3] = flowScale_;
        node.parameters[4] = distortion_;
        node.parameters[5] = phase_;
        node.resolutionScaledParameterMask = (1u << 1);
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
