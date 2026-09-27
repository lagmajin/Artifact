module;
#include <cstdint>
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Ripple;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

// Concentric ripple distortion. The CPU path delegates to the shared
// ArtifactCore mapper so the radial wave matches every other distort effect;
// the GPU path uses the generic resident shader registered here.
class RippleEffect : public ArtifactAbstractEffect {
private:
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float amplitude_ = 10.0f;
    float frequency_ = 0.01f;
    float decay_ = 0.005f;
    float phase_ = 0.0f;
    void syncImpls();

public:
    RippleEffect();
    ~RippleEffect() override;

    float centerX() const;
    void setCenterX(float v);
    float centerY() const;
    void setCenterY(float v);
    float amplitude() const;
    void setAmplitude(float v);
    float frequency() const;
    void setFrequency(float v);
    float decay() const;
    void setDecay(float v);
    float phase() const;
    void setPhase(float v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    static constexpr const char* kGpuGenericKeyString = "ripple";
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
        node.parameters[0] = centerX_;
        node.parameters[1] = centerY_;
        node.parameters[2] = amplitude_;
        node.parameters[3] = frequency_;
        node.parameters[4] = decay_;
        node.parameters[5] = phase_;
        // Amplitude is measured in source pixels.
        node.resolutionScaledParameterMask = (1u << 2);
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

}
