module;
#include <cstdint>
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Magnify;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

// Spherical magnifier. Delegates to ArtifactCore::makeMagnify on the CPU so the
// radial lens math stays identical to every other radial distort effect.
class MagnifyEffect : public ArtifactAbstractEffect {
private:
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float radius_ = 0.5f;
    float amount_ = 50.0f;
    void syncImpls();

public:
    MagnifyEffect();
    ~MagnifyEffect() override;

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

    static constexpr const char* kGpuGenericKeyString = "magnify";
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
        // Radius is a fraction of the image size, not a pixel count.
        node.resolutionScaledParameterMask = 0u;
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

}
