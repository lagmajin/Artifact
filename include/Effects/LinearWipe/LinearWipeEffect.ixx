module;
#include <cstdint>
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.LinearWipe;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class LinearWipeEffect : public ArtifactAbstractEffect {
private:
    float angle_ = 0.0f;
    float softness_ = 0.1f;
    float feather_ = 0.0f;
    void syncImpls();

public:
    LinearWipeEffect();
    ~LinearWipeEffect() override;

    float angle() const;
    void setAngle(float v);
    float softness() const;
    void setSoftness(float v);
    float feather() const;
    void setFeather(float v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    static constexpr const char* kGpuGenericKeyString = "linear_wipe";
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
        node.parameters[0] = angle_;
        node.parameters[1] = softness_;
        node.parameters[2] = feather_;
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

}
