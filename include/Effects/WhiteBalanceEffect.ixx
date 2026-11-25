module;

#include <QString>
#include <QVariant>
#include <QVector>
#include <optional>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

export module Artifact.Effect.WhiteBalance;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import CvUtils;

export namespace Artifact {

using namespace ArtifactCore;

// ホワイトバランス エフェクト
// 色温度 (Temperature) と 色調 (Tint) でホワイトポイントを調整
// Temperature: 青←→橙 (K Kelvin 換算: 2000K-12000K)
// Tint: 緑←→マゼンタ
class WhiteBalanceEffect : public ArtifactAbstractEffect {
private:
    float temperature_ = 6500.0f;  // 色温度 (Kelvin)
    float tint_ = 0.0f;            // 色調 (-1.0 ~ 1.0, 緑←→マゼンタ)
    float brightness_ = 0.0f;      // 明度補正
    int preset_ = 0;

    void syncImpls();

public:
    WhiteBalanceEffect();
    ~WhiteBalanceEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.white_balance.temperature"),
                QStringLiteral("Temperature (K)"),
                1000.0, 20000.0, 10.0, 18.75, 1.0, QStringLiteral("K")};
        }
        if (index == 2) {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.white_balance.preset"),
                QStringLiteral("Preset"), 0.0, 6.0, 1.0, 0.05, 1.0,
                QString()};
            parameter.choiceCount = 7;
            return parameter;
        }
        return std::nullopt;
    }

    float temperature() const { return temperature_; }
    void setTemperature(float t) { temperature_ = std::isfinite(t) ? std::clamp(t, 1000.0f, 20000.0f) : 6500.0f; preset_ = 0; syncImpls(); }

    float tint() const { return tint_; }
    void setTint(float t) { tint_ = std::isfinite(t) ? std::clamp(t, -1.0f, 1.0f) : 0.0f; syncImpls(); }

    float brightness() const { return brightness_; }
    void setBrightness(float b) { brightness_ = std::isfinite(b) ? std::clamp(b, -1.0f, 1.0f) : 0.0f; syncImpls(); }

    // プリセット
    void setPreset(const QString& preset);
    int preset() const { return preset_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
    static constexpr const char* kGpuGenericKeyString = "white_balance";
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
        node.parameters[0] = temperature_;
        node.parameters[1] = tint_;
        node.parameters[2] = brightness_;
        return stack.append(node);
    }
};

} // namespace Artifact
