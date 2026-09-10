module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Sharpen;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class SharpenEffect : public ArtifactAbstractEffect {
private:
    float amount_ = 1.0f;
    float sigma_ = 1.0f;
    float threshold_ = 0.0f;

    void syncImpls();

public:
    SharpenEffect();
    ~SharpenEffect() override;

    float amount() const;
    void setAmount(float v);
    float sigma() const;
    void setSigma(float v);
    float threshold() const;
    void setThreshold(float v);

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Sharpen;
        node.parameters[0] = amount_;
        node.parameters[1] = sigma_;
        node.parameters[2] = threshold_;
        node.resolutionScaledParameterMask = 1u << 1;
        return stack.append(node);
    }

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

class MagicSharpEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache& src,
               ImageF32x4RGBAWithCache& dst) override;
public:
    MagicSharpEffect();
    ~MagicSharpEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
    EffectROIHint roiHint() const override;
};

}
