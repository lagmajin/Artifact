module;
#include <utility>
#include <vector>
#include <optional>
#include <QString>
#include <QVariant>

export module GrayscaleEffect;

import Artifact.Render.PointwiseEffectFusion;
import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class GrayscaleEffect : public ArtifactAbstractEffect {
private:
    enum class Mode {
        Perceptual = 0,
        LinearLuminance = 1,
        Desaturate = 2,
    };

    float strength_ = 1.0f;
    Mode mode_ = Mode::LinearLuminance;

    void syncImpls();

public:
    GrayscaleEffect();
    ~GrayscaleEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.colorcorrection.grayscale.strength"),
                QStringLiteral("Strength"), 0.0, 1.0, 0.01, 0.0025, 100.0,
                QStringLiteral("%")};
        }
        if (index == 2) {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.colorcorrection.grayscale.mode"),
                QStringLiteral("Mode"), 0.0, 2.0, 1.0, 0.05, 1.0,
                QString()};
            parameter.choiceCount = 3;
            return parameter;
        }
        return std::nullopt;
    }

    void setMode(int value);
    int mode() const { return static_cast<int>(mode_); }

    void setStrength(float value);
    float strength() const { return strength_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Pointwise;
    }
    bool appendGpuPointwiseNodes(ArtifactCore::PointwiseEffectStack& stack, std::uint32_t& slot) const override {
        if (mode() != 0) return false;
        if (std::abs(strength_ - 1.0f) > 1.0e-6f) { stack.addNode(ArtifactCore::PointwiseNodeKind::Saturation, slot); stack.setParameter(slot++, 1.0f - strength_); }
        return slot < ArtifactCore::PointwiseEffectStack::kParameterSlotCount;
    }
};

} // namespace Artifact
