module;
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>
#include <QColor>

export module Artifact.Effect.Rasterizer.AddNoise;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class AddNoiseEffect : public ArtifactAbstractEffect {
private:
    float amount_ = 0.15f;
    float size_ = 1.0f;
    bool colorNoise_ = true;
    bool monochrome_ = false;
    int seed_ = 0;
    void syncImpls();

public:
    AddNoiseEffect();
    ~AddNoiseEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.add_noise.amount"),
                QStringLiteral("Amount"), 0.0, 1.0, 0.01, 0.0025,
                100.0, QStringLiteral("%")};
        }
        if (index == 2) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.add_noise.size"),
                QStringLiteral("Size"), 0.1, 64.0, 0.1, 0.16,
                1.0, QStringLiteral("px")};
        }
        if (index == 3) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.add_noise.seed"),
                QStringLiteral("Seed"), 0.0, 9999.0, 1.0, 1.0,
                1.0, QString()};
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
                QStringLiteral("effect.add_noise.color_noise"),
                QStringLiteral("Color Noise")};
        }
        if (index == 2) {
            return EffectViewportToggleParameter{
                QStringLiteral("effect.add_noise.monochrome"),
                QStringLiteral("Monochrome")};
        }
        return std::nullopt;
    }

    float amount() const;
    void setAmount(float v);
    float size() const;
    void setSize(float v);
    bool colorNoise() const;
    void setColorNoise(bool v);
    bool monochrome() const;
    void setMonochrome(bool v);
    int seed() const;
    void setSeed(int v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    static constexpr const char* kGpuGenericKeyString = "add_noise";
    static constexpr std::uint32_t kGpuGenericKey =
        gpuGenericKeyFromString(kGpuGenericKeyString);
    std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Generic;
        node.genericKey = kGpuGenericKey;
        node.parameters[0] = amount_;
        node.parameters[1] = size_;
        node.parameters[2] = colorNoise_ ? 1.0f : 0.0f;
        node.parameters[3] = monochrome_ ? 1.0f : 0.0f;
        node.parameters[4] = static_cast<float>(seed_);
        node.resolutionScaledParameterMask = (1u << 1);
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

}
