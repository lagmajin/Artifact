module;
#include <utility>
#include <optional>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Mosaic;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class MosaicEffect : public ArtifactAbstractEffect {
private:
    float cellSize_ = 8.0f;
    bool shapeMode_ = false;
    void syncImpls();

public:
    MosaicEffect();
    ~MosaicEffect() override;

    float cellSize() const;
    void setCellSize(float v);
    bool shapeMode() const;
    void setShapeMode(bool v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.mosaic.cell_size"),
                QStringLiteral("Cell Size"),
                1.0, 512.0, 1.0, 0.125, 1.0, QStringLiteral("px")};
        }
        return std::nullopt;
    }

    std::optional<EffectViewportToggleParameter>
    viewportToggleParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::viewportToggleParameter(index);
        }
        if (index == 1) {
            return EffectViewportToggleParameter{
                QStringLiteral("effect.mosaic.shape_mode"),
                QStringLiteral("Shape Mode")};
        }
        return std::nullopt;
    }
};

}
