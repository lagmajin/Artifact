module;
#include <cstdint>
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.TurbulentDisplace;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class TurbulentDisplaceEffect : public ArtifactAbstractEffect {
private:
    float amount_ = 20.0f;
    float size_ = 30.0f;
    int octaves_ = 4;
    int seed_ = 0;
    float domainWarp_ = 0.0f;
    void syncImpls();

public:
    TurbulentDisplaceEffect();
    ~TurbulentDisplaceEffect() override;

    float amount() const;
    void setAmount(float v);
    float size() const;
    void setSize(float v);
    int octaves() const;
    void setOctaves(int v);
    int seed() const;
    void setSeed(int v);
    float domainWarp() const;
    void setDomainWarp(float v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    static constexpr const char* kGpuGenericKeyString = "turbulent_displace";
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
        node.parameters[0] = amount_;
        node.parameters[1] = size_;
        node.parameters[2] = static_cast<float>(octaves_);
        node.parameters[3] = static_cast<float>(seed_);
        node.parameters[4] = domainWarp_;
        // Amount and size are measured in source pixels.
        node.resolutionScaledParameterMask = (1u << 0) | (1u << 1);
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

}
