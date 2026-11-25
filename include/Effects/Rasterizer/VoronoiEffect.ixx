module;
#include <memory>
#include <optional>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Voronoi;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Voronoi / cellular noise pattern generator.
/// mode: 0=distance to nearest, 1=2nd minus 1st (edge),
/// 2=distance to 2nd, 3=cell value (random per cell).
class VoronoiEffect : public ArtifactAbstractEffect {
public:
    VoronoiEffect();
    ~VoronoiEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        switch (index) {
        case 1:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.voronoi.scale"),
                QStringLiteral("Cell Scale"), 1.0, 200.0, 0.25, 0.125,
                1.0, QStringLiteral("px")};
        case 2:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.voronoi.jitter"),
                QStringLiteral("Jitter"), 0.0, 2.0, 0.01, 0.005,
                1.0, QStringLiteral("cells")};
        case 3:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.voronoi.seed"),
                QStringLiteral("Seed"), 0.0, 9999.0, 1.0, 1.0,
                1.0, QString()};
        case 4: {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.voronoi.mode"), QStringLiteral("mode"),
                0.0, 3.0, 1.0, 0.05, 1.0, QString()};
            parameter.choiceCount = 4;
            return parameter;
        }
        default:
            return std::nullopt;
        }
    }

    float scale() const;
    void  setScale(float v);
    float jitter() const;
    void  setJitter(float v);
    int   mode() const;
    void  setMode(int v);
    int   seed() const;
    void  setSeed(int v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Voronoi;
        node.parameters[0] = scale_;
        node.parameters[1] = jitter_;
        node.parameters[2] = static_cast<float>(mode_);
        node.parameters[3] = static_cast<float>(seed_);
        node.resolutionScaledParameterMask = (1u << 0);
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }

private:
    float scale_=20.0f,jitter_=1.0f; int mode_=0,seed_=0;
    void syncImpls();
};

} // namespace Artifact
