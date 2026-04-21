module;

#include <QString>
#include <QVariant>
#include <QVector>

export module Artifact.Effect.WhiteBalance;

import std;
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

    void syncImpls();

public:
    WhiteBalanceEffect();
    ~WhiteBalanceEffect() override;

    float temperature() const { return temperature_; }
    void setTemperature(float t) { temperature_ = std::clamp(t, 1000.0f, 20000.0f); syncImpls(); }

    float tint() const { return tint_; }
    void setTint(float t) { tint_ = std::clamp(t, -1.0f, 1.0f); syncImpls(); }

    float brightness() const { return brightness_; }
    void setBrightness(float b) { brightness_ = std::clamp(b, -1.0f, 1.0f); syncImpls(); }

    // プリセット
    void setPreset(const QString& preset);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
