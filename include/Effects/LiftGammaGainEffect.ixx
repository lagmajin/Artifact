module;
#include <utility>
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>
#include <QString>
#include <QVariant>
#include <QVector>

export module Artifact.Effect.LiftGammaGain;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import CvUtils;

export namespace Artifact {

using namespace ArtifactCore;

// Lift / Gamma / Gain エフェクト
// DaVinci Resolve 等でお馴染みの3ウェイカラーコレクション
// Lift: シャドウ領域のカラーシフト
// Gamma: ミッドトーンのガンマ補正
// Gain: ハイライト領域のゲイン（ホワイトバランス相当）
class LiftGammaGainEffect : public ArtifactAbstractEffect {
private:
    // Lift (RGB: -1.0 ~ 1.0)
    float liftR_ = 0.0f;
    float liftG_ = 0.0f;
    float liftB_ = 0.0f;

    // Gamma (RGB: 0.1 ~ 5.0)
    float gammaR_ = 1.0f;
    float gammaG_ = 1.0f;
    float gammaB_ = 1.0f;

    // Gain (RGB: 0.0 ~ 4.0)
    float gainR_ = 1.0f;
    float gainG_ = 1.0f;
    float gainB_ = 1.0f;

    void syncImpls();

public:
    LiftGammaGainEffect();
    ~LiftGammaGainEffect() override;

    // Lift
    void setLift(float r, float g, float b) {
        liftR_ = std::clamp(r, -1.0f, 1.0f);
        liftG_ = std::clamp(g, -1.0f, 1.0f);
        liftB_ = std::clamp(b, -1.0f, 1.0f);
        syncImpls();
    }
    void setLiftR(float v) { liftR_ = std::clamp(v, -1.0f, 1.0f); syncImpls(); }
    void setLiftG(float v) { liftG_ = std::clamp(v, -1.0f, 1.0f); syncImpls(); }
    void setLiftB(float v) { liftB_ = std::clamp(v, -1.0f, 1.0f); syncImpls(); }
    float liftR() const { return liftR_; }
    float liftG() const { return liftG_; }
    float liftB() const { return liftB_; }

    // Gamma
    void setGamma(float r, float g, float b) {
        gammaR_ = std::clamp(r, 0.1f, 5.0f);
        gammaG_ = std::clamp(g, 0.1f, 5.0f);
        gammaB_ = std::clamp(b, 0.1f, 5.0f);
        syncImpls();
    }
    void setGammaR(float v) { gammaR_ = std::clamp(v, 0.1f, 5.0f); syncImpls(); }
    void setGammaG(float v) { gammaG_ = std::clamp(v, 0.1f, 5.0f); syncImpls(); }
    void setGammaB(float v) { gammaB_ = std::clamp(v, 0.1f, 5.0f); syncImpls(); }
    float gammaR() const { return gammaR_; }
    float gammaG() const { return gammaG_; }
    float gammaB() const { return gammaB_; }

    // Gain
    void setGain(float r, float g, float b) {
        gainR_ = std::clamp(r, 0.0f, 4.0f);
        gainG_ = std::clamp(g, 0.0f, 4.0f);
        gainB_ = std::clamp(b, 0.0f, 4.0f);
        syncImpls();
    }
    void setGainR(float v) { gainR_ = std::clamp(v, 0.0f, 4.0f); syncImpls(); }
    void setGainG(float v) { gainG_ = std::clamp(v, 0.0f, 4.0f); syncImpls(); }
    void setGainB(float v) { gainB_ = std::clamp(v, 0.0f, 4.0f); syncImpls(); }
    float gainR() const { return gainR_; }
    float gainG() const { return gainG_; }
    float gainB() const { return gainB_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
