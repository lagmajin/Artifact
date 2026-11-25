module;
#include <optional>
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module HueAndSaturation;

import Artifact.Render.PointwiseEffectFusion;
import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

// 色相・彩度・明度 (Hue, Saturation, Lightness) エフェクト
// CPU/GPU 両対応、同一アルゴリズム
class HueAndSaturation : public ArtifactAbstractEffect {
private:
    float hueShift_ = 0.0f;
    float saturationScale_ = 1.0f;
    float lightnessShift_ = 0.0f;
    bool colorize_ = false;

    void syncImpls();

public:
    HueAndSaturation();
    ~HueAndSaturation() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.color.hue"),
                QStringLiteral("Hue"),
                -180.0, 180.0, 0.1, 0.9, 1.0, QStringLiteral("deg")};
        }
        if (index == 2) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.color.saturation"),
                QStringLiteral("Saturation"),
                0.0, 2.0, 0.01, 0.005, 1.0, QString()};
        }
        if (index == 3) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.color.lightness"),
                QStringLiteral("Lightness"),
                -1.0, 1.0, 0.01, 0.005, 1.0, QString()};
        }
        return std::nullopt;
    }

    void setHue(float v) { hueShift_ = std::isfinite(v) ? std::clamp(v, -180.0f, 180.0f) : 0.0f; syncImpls(); }
    float hue() const { return hueShift_; }

    void setSaturation(float v) { saturationScale_ = std::isfinite(v) ? std::clamp(v, 0.0f, 2.0f) : 1.0f; syncImpls(); }
    float saturation() const { return saturationScale_; }

    void setLightness(float v) { lightnessShift_ = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f; syncImpls(); }
    float lightness() const { return lightnessShift_; }

    void setColorize(bool v) { colorize_ = v; syncImpls(); }
    bool isColorize() const { return colorize_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Pointwise;
    }
    bool appendGpuPointwiseNodes(ArtifactCore::PointwiseEffectStack& stack,
                                 std::uint32_t& slot) const override {
        if (std::abs(lightnessShift_) > 1.0e-6f || colorize_) return false;
        if (std::abs(saturationScale_ - 1.0f) > 1.0e-6f) { stack.addNode(ArtifactCore::PointwiseNodeKind::Saturation, slot); stack.setParameter(slot++, saturationScale_); }
        if (std::abs(hueShift_) > 1.0e-6f) { constexpr float kPi = 3.14159265358979323846f; stack.addNode(ArtifactCore::PointwiseNodeKind::HueRotate, slot); stack.setParameter(slot++, hueShift_ * kPi / 180.0f); }
        return slot < ArtifactCore::PointwiseEffectStack::kParameterSlotCount;
    }
};

} // namespace Artifact
