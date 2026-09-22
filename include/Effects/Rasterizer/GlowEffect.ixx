module;
#include <cstdint>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Glow;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Classic bloom/glow effect: extracts bright areas and blurs
/// them back over the original for a soft luminous look.
class RasterizerGlowEffect : public ArtifactAbstractEffect {
public:
    RasterizerGlowEffect();
    ~RasterizerGlowEffect() override;

    float threshold() const;
    void  setThreshold(float v);
    float radius() const;
    void  setRadius(float v);
    float intensity() const;
    void  setIntensity(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;

    static constexpr const char* kGpuGenericKeyString = "rasterizer_glow";
    static constexpr std::uint32_t kGpuGenericKey =
        gpuGenericKeyFromString(kGpuGenericKeyString);
    std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        // The resident shader evaluates the CPU Gaussian equation for this
        // bounded radius range. Larger kernels
        // remain on the CPU path until a separable multi-pass contract exists.
        if (radius_ > 4.0f) return false;
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Generic;
        node.genericKey = kGpuGenericKey;
        node.parameters[0] = threshold_;
        node.parameters[1] = radius_;
        node.parameters[2] = intensity_;
        node.resolutionScaledParameterMask = (1u << 1);
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }

private:
    float threshold_=0.5f,radius_=20.0f,intensity_=1.0f;
    void syncImpls();
};

} // namespace Artifact
