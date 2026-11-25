module;
#include <vector>
#include <optional>
#include <QString>
#include <QVariant>

export module InvertEffect;

import Artifact.Render.PointwiseEffectFusion;
import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

/// Per-channel inversion (AE-style Invert).
/// Channel: 0=RGB, 1=Red, 2=Green, 3=Blue, 4=Alpha.
class InvertEffect : public ArtifactAbstractEffect {
private:
    int channel_ = 0;
    float strength_ = 1.0f;

    void syncImpls();

public:
    InvertEffect();
    ~InvertEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.colorcorrection.invert.strength"),
                QStringLiteral("Strength"), 0.0, 1.0, 0.01, 0.0025, 100.0,
                QStringLiteral("%")};
        }
        if (index == 2) {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.colorcorrection.invert.channel"),
                QStringLiteral("Channel"), 0.0, 4.0, 1.0, 0.05, 1.0,
                QString()};
            parameter.choiceCount = 5;
            return parameter;
        }
        return std::nullopt;
    }

    void setChannel(int value);
    int channel() const { return channel_; }

    void setStrength(float value);
    float strength() const { return strength_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Pointwise;
    }
    bool appendGpuPointwiseNodes(ArtifactCore::PointwiseEffectStack& stack, std::uint32_t& slot) const override {
        if (channel_ != 0) return false;
        const float factor = 1.0f - 2.0f * std::clamp(strength_, 0.0f, 1.0f);
        if (std::abs(factor - 1.0f) > 1.0e-6f) { stack.addNode(ArtifactCore::PointwiseNodeKind::Contrast, slot); stack.setParameter(slot++, factor); }
        return slot < ArtifactCore::PointwiseEffectStack::kParameterSlotCount;
    }
};

} // namespace Artifact
