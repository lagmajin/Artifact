module;
#include <optional>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.AnisotropicFlowBlur;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class AnisotropicFlowBlurEffect : public ArtifactAbstractEffect {
private:
    float blurAmount_ = 10.0f;
    float tensorNoiseScale_ = 1.0f;
    float tensorIntegrationScale_ = 4.0f;
    float edgeAdherence_ = 0.8f;
    void syncImpl();

public:
    AnisotropicFlowBlurEffect();
    ~AnisotropicFlowBlurEffect() override;

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        switch (index) {
        case 1:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.anisotropic_flow_blur.amount"),
                QStringLiteral("Blur Amount"),
                0.0, 100.0, 0.1, 0.25, 1.0, QStringLiteral("px")};
        case 2:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.anisotropic_flow_blur.tensor_noise_scale"),
                QStringLiteral("Tensor Noise Scale"),
                0.1, 20.0, 0.01, 0.05, 1.0, QString()};
        case 3:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.anisotropic_flow_blur.tensor_integration_scale"),
                QStringLiteral("Tensor Integration Scale"),
                0.1, 20.0, 0.01, 0.05, 1.0, QString()};
        case 4:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.anisotropic_flow_blur.edge_adherence"),
                QStringLiteral("Edge Adherence"),
                0.0, 1.0, 0.01, 0.0025, 100.0, QStringLiteral("%")};
        default:
            return std::nullopt;
        }
    }
};

}
