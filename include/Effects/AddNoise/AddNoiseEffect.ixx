module;
#include <cstdint>
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>
#include <QColor>

export module Artifact.Effect.Rasterizer.AddNoise;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class AddNoiseEffect : public ArtifactAbstractEffect {
private:
    float amount_ = 0.15f;
    float size_ = 1.0f;
    bool colorNoise_ = true;
    bool monochrome_ = false;
    int seed_ = 0;
    void syncImpls();

public:
    AddNoiseEffect();
    ~AddNoiseEffect() override;

    float amount() const;
    void setAmount(float v);
    float size() const;
    void setSize(float v);
    bool colorNoise() const;
    void setColorNoise(bool v);
    bool monochrome() const;
    void setMonochrome(bool v);
    int seed() const;
    void setSeed(int v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    static constexpr const char* kGpuGenericKeyString = "add_noise";
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
        node.parameters[2] = colorNoise_ ? 1.0f : 0.0f;
        node.parameters[3] = monochrome_ ? 1.0f : 0.0f;
        node.parameters[4] = static_cast<float>(seed_);
        node.resolutionScaledParameterMask = (1u << 1);
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

}
