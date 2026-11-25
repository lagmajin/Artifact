module;
#include <optional>
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.FindEdges;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class FindEdgesEffect : public ArtifactAbstractEffect {
private:
    float amount_ = 1.0f;
    bool invert_ = false;
    void syncImpls();

public:
    FindEdgesEffect();
    ~FindEdgesEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.find_edges.amount"),
                QStringLiteral("Amount"),
                0.0, 5.0, 0.05, 0.01, 1.0, QStringLiteral("×")};
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
                QStringLiteral("effect.find_edges.invert"),
                QStringLiteral("Invert")};
        }
        return std::nullopt;
    }

    float amount() const;
    void setAmount(float v);
    bool invert() const;
    void setInvert(bool v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
    bool supportsGPU() const override { return true; }
};

}
