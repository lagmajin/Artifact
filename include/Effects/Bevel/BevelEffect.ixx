module;
#include <utility>
#include <optional>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Bevel;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class BevelEffect : public ArtifactAbstractEffect {
private:
    float strength_ = 1.0f;
    float softness_ = 2.0f;
    bool edgeMode_ = false;
    void syncImpls();

public:
    BevelEffect();
    ~BevelEffect() override;

    float strength() const;
    void setStrength(float v);
    float softness() const;
    void setSoftness(float v);
    bool edgeMode() const;
    void setEdgeMode(bool v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.bevel.strength"),
                QStringLiteral("Strength"),
                0.0, 5.0, 0.01, 0.005, 1.0, QStringLiteral("x")};
        }
        return std::nullopt;
    }

    bool supportsGPU() const override { return true; }
};

}
