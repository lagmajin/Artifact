module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Stripes;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Parametric stripe pattern generator.
class StripesEffect : public ArtifactAbstractEffect {
public:
    StripesEffect();
    ~StripesEffect() override;

    float frequency() const;
    void  setFrequency(float v);
    float angle() const;
    void  setAngle(float v);
    float thickness() const;
    void  setThickness(float v);
    float offset() const;
    void  setOffset(float v);

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Stripes;
        node.parameters[0] = frequency_;
        node.parameters[1] = angle_;
        node.parameters[2] = thickness_;
        node.parameters[3] = offset_;
        return stack.append(node);
    }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return true; }

private:
    float frequency_=10.0f,angle_=0.0f,thickness_=0.5f,offset_=0.0f;
    void syncImpls();
};

} // namespace Artifact
