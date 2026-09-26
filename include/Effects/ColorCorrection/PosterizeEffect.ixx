module;
#include <utility>
#include <algorithm>
#include <memory>
#include <vector>
#include <cmath>
#include <cstdint>
#include <QString>
#include <QVariant>

export module PosterizeEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

/// Posterize (階調化) エフェクト
/// 画素の階調を減らし（量子化し）、色数を制限したポスターのような質感を生成
class PosterizeEffect : public ArtifactAbstractEffect {
private:
    float levels_ = 4.0f; // 2 ~ 64 (デフォルト 4)

    void syncImpls();

public:
    PosterizeEffect();
    ~PosterizeEffect() override;

    void setLevels(float value) {
        levels_ = std::isfinite(value) ? std::clamp(value, 2.0f, 64.0f) : 4.0f;
        syncImpls();
    }
    float levels() const { return levels_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
    static constexpr const char* kGpuGenericKeyString = "posterize";
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
        node.parameters[0] = levels_;
        return stack.append(node);
    }
};

} // namespace Artifact
