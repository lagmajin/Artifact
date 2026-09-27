module;
#include <cstdint>
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.PinchBulge;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

// Pinch / Bulge. A single effect covers both because they share one radial
// power remap; the sign of Amount selects between them.
class PinchBulgeEffect : public ArtifactAbstractEffect {
private:
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float radius_ = 0.5f;
    float amount_ = 50.0f;
    void syncImpls();

public:
    PinchBulgeEffect();
    ~PinchBulgeEffect() override;

    float centerX() const;
    void setCenterX(float v);
    float centerY() const;
    void setCenterY(float v);
    float radius() const;
    void setRadius(float v);
    float amount() const;
    void setAmount(float v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    static constexpr const char* kGpuGenericKeyString = "pinch_bulge";
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
        node.parameters[2] = radius_;
        node.parameters[3] = amount_;
        node.resolutionScaledParameterMask = 0u;
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

}
