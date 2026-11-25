module;
#include <memory>
#include <optional>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Vignette;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Soft vignette: darkens edges with configurable center,
/// radius, and feathering.
class VignetteEffect : public ArtifactAbstractEffect {
public:
    VignetteEffect();
    ~VignetteEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.vignette.amount"),
                QStringLiteral("amount"),
                0.0, 1.0, 0.01, 0.0025, 100.0, QStringLiteral("%")};
        }
        return std::nullopt;
    }

    float amount() const;
    void  setAmount(float v);
    float radius() const;
    void  setRadius(float v);
    float feather() const;
    void  setFeather(float v);
    float centerX() const;
    void  setCenterX(float v);
    float centerY() const;
    void  setCenterY(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return true; }
    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }
    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        if (radius_ <= 0.0f) return false;
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Vignette;
        node.parameters[0] = amount_;
        node.parameters[1] = radius_;
        node.parameters[2] = feather_;
        node.parameters[3] = cx_;
        node.parameters[4] = cy_;
        return stack.append(node);
    }

private:
    float amount_=0.7f,radius_=0.8f,feather_=0.4f,cx_=0.5f,cy_=0.5f;
    void syncImpls();
};

} // namespace Artifact
