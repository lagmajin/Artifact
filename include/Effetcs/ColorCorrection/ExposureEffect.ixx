module;
#include <utility>
#include <algorithm>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module ExposureEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

// 露出 (Exposure) エフェクト
// 写真撮影における露出補正をシミュレート
// CPU/GPU 両対応、同一アルゴリズム
class ExposureEffect : public ArtifactAbstractEffect {
private:
    float exposure_ = 0.0f;        // EV (-5.0 ~ 5.0)
    float offset_ = 0.0f;          // (-0.5 ~ 0.5)
    float gammaCorrection_ = 1.0f; // (0.2 ~ 5.0)

    void syncImpls();

public:
    ExposureEffect();
    ~ExposureEffect() override;

    void setExposure(float ev) { exposure_ = std::clamp(ev, -5.0f, 5.0f); syncImpls(); }
    float exposure() const { return exposure_; }

    void setOffset(float offset) { offset_ = std::clamp(offset, -0.5f, 0.5f); syncImpls(); }
    float offset() const { return offset_; }

    void setGammaCorrection(float gamma) { gammaCorrection_ = std::clamp(gamma, 0.2f, 5.0f); syncImpls(); }
    float gammaCorrection() const { return gammaCorrection_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
