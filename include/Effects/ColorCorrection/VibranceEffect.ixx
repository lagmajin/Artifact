module;
#include <utility>
#include <algorithm>
#include <memory>
#include <vector>
#include <cmath>
#include <cstdint>
#include <QString>
#include <QVariant>

export module VibranceEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

/// Vibrance (自然な彩度) エフェクト
/// 低彩度領域を選択的にブーストし、高彩度部や肌色の飽和を抑制
class VibranceEffect : public ArtifactAbstractEffect {
private:
    float vibrance_ = 0.0f;    // -1.0 ~ 1.0 (デフォルト 0.0)
    float saturation_ = 0.0f;  // -1.0 ~ 1.0 (デフォルト 0.0)

    void syncImpls();

public:
    VibranceEffect();
    ~VibranceEffect() override;

    void setVibrance(float value) {
        vibrance_ = std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f;
        syncImpls();
    }
    float vibrance() const { return vibrance_; }

    void setSaturation(float value) {
        saturation_ = std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f;
        syncImpls();
    }
    float saturation() const { return saturation_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
    static constexpr const char* kGpuGenericKeyString = "vibrance";
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
        node.parameters[0] = vibrance_;
        node.parameters[1] = saturation_;
        return stack.append(node);
    }
};

} // namespace Artifact
