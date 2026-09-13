module;
#include <cstdint>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Glow.LuminescenceCaustics;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class LuminescenceCausticsEffect : public ArtifactAbstractEffect {
private:
    float threshold_ = 0.55f;
    float edgeWeight_ = 0.8f;
    float scale_ = 22.0f;
    float intensity_ = 0.75f;
    float evolution_ = 0.0f;
    float colorShift_ = 0.35f;
    void syncImpl();

public:
    LuminescenceCausticsEffect();
    ~LuminescenceCausticsEffect() override;

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    static constexpr const char* kGpuGenericKeyString = "luminescence_caustics";
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
        node.parameters[1] = edgeWeight_;
        node.parameters[2] = scale_;
        node.parameters[3] = intensity_;
        node.parameters[4] = evolution_;
        node.parameters[5] = colorShift_;
        node.resolutionScaledParameterMask = 0;
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

}
