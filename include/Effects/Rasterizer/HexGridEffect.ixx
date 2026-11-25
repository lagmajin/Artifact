module;
#include <memory>
#include <optional>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.HexGrid;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Hexagonal grid / honeycomb pattern generator.
class HexGridEffect : public ArtifactAbstractEffect {
public:
    HexGridEffect();
    ~HexGridEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        switch (index) {
        case 1:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.hex_grid.cell_size"),
                QStringLiteral("Cell Size"), 4.0, 200.0, 0.25, 0.125,
                1.0, QStringLiteral("px")};
        case 2:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.hex_grid.line_width"),
                QStringLiteral("Line Width"), 0.5, 20.0, 0.1, 0.05,
                1.0, QStringLiteral("px")};
        case 3:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.hex_grid.angle"),
                QStringLiteral("Angle"), 0.0, 359.9, 0.1, 0.25,
                1.0, QStringLiteral("°")};
        default:
            return std::nullopt;
        }
    }

    float cellSize() const;
    void  setCellSize(float v);
    float lineWidth() const;
    void  setLineWidth(float v);
    float angle() const;
    void  setAngle(float v);

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::HexGrid;
        node.parameters[0] = cellSize_;
        node.parameters[1] = lineWidth_;
        node.parameters[2] = angle_;
        node.resolutionScaledParameterMask = (1u << 0) | (1u << 1);
        return stack.append(node);
    }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return true; }

private:
    float cellSize_=32.0f,lineWidth_=2.0f,angle_=0.0f;
    void syncImpls();
};

} // namespace Artifact
