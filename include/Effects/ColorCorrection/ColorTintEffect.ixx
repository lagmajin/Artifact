module;
#include <utility>
#include <algorithm>
#include <memory>
#include <vector>
#include <cmath>
#include <cstdint>
#include <QString>
#include <QVariant>
#include <QColor>

export module ColorTintEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

/// Color Tint (2色着色 / トーンマップ) エフェクト
/// 輝度に基づいて黒（シャドウ）と白（ハイライト）を任意の色にマッピングし、着色率を調整
class ColorTintEffect : public ArtifactAbstractEffect {
private:
    QColor mapBlackTo_ = QColor(0, 0, 0);       // 黒のマップ先
    QColor mapWhiteTo_ = QColor(255, 255, 255); // 白のマップ先
    float amountToTint_ = 1.0f;                 // 着色量 (0.0 ~ 1.0, デフォルト 1.0)

    void syncImpls();

public:
    ColorTintEffect();
    ~ColorTintEffect() override;

    void setMapBlackTo(const QColor& color) {
        mapBlackTo_ = color;
        syncImpls();
    }
    QColor mapBlackTo() const { return mapBlackTo_; }

    void setMapWhiteTo(const QColor& color) {
        mapWhiteTo_ = color;
        syncImpls();
    }
    QColor mapWhiteTo() const { return mapWhiteTo_; }

    void setAmountToTint(float value) {
        amountToTint_ = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 1.0f;
        syncImpls();
    }
    float amountToTint() const { return amountToTint_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
    static constexpr const char* kGpuGenericKeyString = "color_tint";
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
        node.parameters[0] = static_cast<float>(mapBlackTo_.redF());
        node.parameters[1] = static_cast<float>(mapBlackTo_.greenF());
        node.parameters[2] = static_cast<float>(mapBlackTo_.blueF());
        node.parameters[3] = static_cast<float>(mapWhiteTo_.redF());
        node.parameters[4] = static_cast<float>(mapWhiteTo_.greenF());
        node.parameters[5] = static_cast<float>(mapWhiteTo_.blueF());
        node.parameters[6] = amountToTint_;
        return stack.append(node);
    }
};

} // namespace Artifact
