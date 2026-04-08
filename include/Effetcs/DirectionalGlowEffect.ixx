module;
#include <utility>
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
    Custom        // カスタム角度
};

class DirectionalGlowEffect : public ArtifactAbstractEffect {
private:
    float threshold_ = 0.8f;
    float intensity_ = 1.0f;
    float length1_ = 64.0f;
    float length2_ = 128.0f;
    float weight1_ = 0.6f;
    float weight2_ = 0.4f;
    StreakPattern pattern_ = StreakPattern::Horizontal;
    QVector<float> customAngles_; // カスタム角度（度）
    float angleOffset_ = 0.0f;    // 全体回転

    void syncImpls();

public:
    DirectionalGlowEffect();
    ~DirectionalGlowEffect() override;

    float threshold() const { return threshold_; }
    void setThreshold(float t) { threshold_ = std::clamp(t, 0.0f, 1.0f); syncImpls(); }

    float intensity() const { return intensity_; }
    void setIntensity(float i) { intensity_ = std::max(0.0f, i); syncImpls(); }

    float length1() const { return length1_; }
    void setLength1(float l) { length1_ = std::max(1.0f, l); syncImpls(); }

    float length2() const { return length2_; }
    void setLength2(float l) { length2_ = std::max(1.0f, l); syncImpls(); }

    float weight1() const { return weight1_; }
    void setWeight1(float w) { weight1_ = std::clamp(w, 0.0f, 1.0f); syncImpls(); }

    float weight2() const { return weight2_; }
    void setWeight2(float w) { weight2_ = std::clamp(w, 0.0f, 1.0f); syncImpls(); }

    StreakPattern pattern() const { return pattern_; }
    void setPattern(StreakPattern p) { pattern_ = p; syncImpls(); }

    const QVector<float>& customAngles() const { return customAngles_; }
    void setCustomAngles(const QVector<float>& angles) { customAngles_ = angles; syncImpls(); }

    float angleOffset() const { return angleOffset_; }
    void setAngleOffset(float a) { angleOffset_ = a; syncImpls(); }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
