module;
#include <utility>
#include <algorithm>
#include <vector>
#include <QString>
#include <QVariant>

export module BrightnessEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

// 明度 (Brightness) エフェクト
// 画像全体の明るさを調整する基本的なカラーコレクション
// CPU/GPU 両対応、同一アルゴリズム
class BrightnessEffect : public ArtifactAbstractEffect {
private:
    float brightness_ = 0.0f;
    float contrast_ = 0.0f;
    float highlights_ = 0.0f;
    float shadows_ = 0.0f;

    void syncImpls();

public:
    BrightnessEffect();
    ~BrightnessEffect() override;

    void setBrightness(float v) { brightness_ = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f; syncImpls(); }
    float brightness() const { return brightness_; }

    void setContrast(float v) { contrast_ = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f; syncImpls(); }
    float contrast() const { return contrast_; }

    void setHighlights(float v) { highlights_ = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f; syncImpls(); }
    float highlights() const { return highlights_; }

    void setShadows(float v) { shadows_ = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f; syncImpls(); }
    float shadows() const { return shadows_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Pointwise;
    }
    bool appendGpuPointwiseNodes(ArtifactCore::PointwiseEffectStack& stack,
                                 std::uint32_t& slot) const override {
        if (std::abs(highlights_) > 1.0e-6f || std::abs(shadows_) > 1.0e-6f) return false;
        if (std::abs(brightness_) > 1.0e-6f) {
            stack.addNode(ArtifactCore::PointwiseNodeKind::Offset, slot);
            stack.setParameter(slot++, brightness_);
        }
        if (std::abs(contrast_) > 1.0e-6f) {
            const float c = std::clamp(contrast_, -0.999f, 0.999f);
            stack.addNode(ArtifactCore::PointwiseNodeKind::Contrast, slot);
            stack.setParameter(slot++, (1.0f + c) / (1.0f - c));
        }
        return slot < ArtifactCore::PointwiseEffectStack::kParameterSlotCount;
    }
};

} // namespace Artifact
