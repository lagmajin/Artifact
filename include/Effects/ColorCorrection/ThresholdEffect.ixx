module;
#include <utility>
#include <algorithm>
#include <memory>
#include <vector>
#include <cmath>
#include <cstdint>
#include <QString>
#include <QVariant>

export module ThresholdEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

/// Threshold (二値化 / しきい値) エフェクト
/// 輝度を指定したしきい値と比較し、白黒（0.0 または 1.0）のハイコントラスト二値画像に変換
class ThresholdEffect : public ArtifactAbstractEffect {
private:
    float threshold_ = 0.5f; // 0.0 ~ 1.0 (デフォルト 0.5)

    void syncImpls();

public:
    ThresholdEffect();
    ~ThresholdEffect() override;

    void setThreshold(float value) {
        threshold_ = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.5f;
        syncImpls();
    }
    float threshold() const { return threshold_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
    static constexpr const char* kGpuGenericKeyString = "threshold";
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
        node.parameters[0] = threshold_;
        return stack.append(node);
    }
};

} // namespace Artifact
