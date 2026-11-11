module;
#include <utility>
#include <cstdint>
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>
#include <QString>
#include <QVariant>
#include <QVector>

export module Artifact.Effect.DirectionalGlow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import CvUtils;

export namespace Artifact {

using namespace ArtifactCore;

// Directional Glow / Streaks エフェクト
// 光が特定方向に伸びるアナモルフィックレンズフレア風効果
enum class StreakPattern {
    Horizontal,   // 水平のみ (anamorphic)
    Cross,        // 十字 (0°, 90°)
    Star,         // 星型 (0°, 45°, 90°, 135°)
    Custom,       // カスタム角度
    Star6         // 6方向 (0°, 30°, 60°, 90°, 120°, 150°)
};

class DirectionalGlowEffect : public ArtifactAbstractEffect {
private:
    float threshold_ = 0.8f;
    float intensity_ = 1.0f;
    float length1_ = 64.0f;
    float length2_ = 128.0f;
    float length3_ = 256.0f;
    float weight1_ = 0.6f;
    float weight2_ = 0.4f;
    float weight3_ = 0.15f;
    bool halfSampling_ = false;
    int preset_ = 0; // 0=Custom, 1=Anamorphic 2.39, 2=Star 6, 3=Neon, 4=Dreamy
    StreakPattern pattern_ = StreakPattern::Horizontal;
    QVector<float> customAngles_; // カスタム角度（度）
    float angleOffset_ = 0.0f;    // 全体回転

    void syncImpls();

public:
    DirectionalGlowEffect();
    ~DirectionalGlowEffect() override;

    float threshold() const { return threshold_; }
    void setThreshold(float t) { threshold_ = std::isfinite(t) ? std::clamp(t, 0.0f, 1.0f) : 0.8f; syncImpls(); }

    float intensity() const { return intensity_; }
    void setIntensity(float i) { intensity_ = std::isfinite(i) ? std::max(0.0f, i) : 1.0f; syncImpls(); }

    float length1() const { return length1_; }
    void setLength1(float l) { length1_ = std::isfinite(l) ? std::max(1.0f, l) : 64.0f; syncImpls(); }

    float length2() const { return length2_; }
    void setLength2(float l) { length2_ = std::isfinite(l) ? std::max(1.0f, l) : 128.0f; syncImpls(); }

    float length3() const { return length3_; }
    void setLength3(float l) { length3_ = std::isfinite(l) ? std::clamp(l, 1.0f, 256.0f) : 256.0f; syncImpls(); }

    float weight1() const { return weight1_; }
    void setWeight1(float w) { weight1_ = std::isfinite(w) ? std::clamp(w, 0.0f, 1.0f) : 0.6f; syncImpls(); }

    float weight2() const { return weight2_; }
    void setWeight2(float w) { weight2_ = std::isfinite(w) ? std::clamp(w, 0.0f, 1.0f) : 0.4f; syncImpls(); }

    float weight3() const { return weight3_; }
    void setWeight3(float w) { weight3_ = std::isfinite(w) ? std::clamp(w, 0.0f, 1.0f) : 0.15f; syncImpls(); }

    bool halfSampling() const { return halfSampling_; }
    void setHalfSampling(bool enabled) { halfSampling_ = enabled; syncImpls(); }

    int preset() const { return preset_; }
    void applyPreset(int preset);

    StreakPattern pattern() const { return pattern_; }
    void setPattern(StreakPattern p) { pattern_ = p; syncImpls(); }

    const QVector<float>& customAngles() const { return customAngles_; }
    void setCustomAngles(const QVector<float>& angles) {
        customAngles_.clear();
        for (float angle : angles) {
            if (std::isfinite(angle)) customAngles_.append(angle);
        }
        if (!customAngles_.isEmpty()) pattern_ = StreakPattern::Custom;
        syncImpls();
    }

    float angleOffset() const { return angleOffset_; }
    void setAngleOffset(float a) { angleOffset_ = std::isfinite(a) ? a : 0.0f; syncImpls(); }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    static constexpr const char* kGpuGenericKeyString = "directional_glow";
    static constexpr std::uint32_t kGpuGenericKey =
        gpuGenericKeyFromString(kGpuGenericKeyString);
    std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        // Custom angles cannot travel in the fixed parameter payload.
        if (pattern_ == StreakPattern::Custom) return false;
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Generic;
        node.genericKey = kGpuGenericKey;
        node.parameters[0] = threshold_;
        node.parameters[1] = intensity_;
        node.parameters[2] = length1_;
        node.parameters[3] = length2_;
        node.parameters[4] = weight1_;
        node.parameters[5] = weight2_;
        node.parameters[6] = static_cast<float>(pattern_);
        node.parameters[7] = length3_;
        node.parameters[8] = angleOffset_;
        node.parameters[9] = weight3_;
        node.parameters[10] = halfSampling_ ? 1.0f : 0.0f;
        // length1/2/3 are the only pixel-space parameters; the 8-bit mask
        // covers up to parameter index 7, which is why length3 rides at 7.
        node.resolutionScaledParameterMask = (1u << 2) | (1u << 3) | (1u << 7);
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
