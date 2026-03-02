module;

#include <QString>
#include <QVariant>

export module ExposureEffect;

import std;
import Artifact.Effect.Abstract;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

// 露出 (Exposure) エフェクト
// 写真撮影における露出補正をシミュレート
class ExposureEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;

protected:
    void apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;

public:
    ExposureEffect();
    ~ExposureEffect() override;

    // 露出 EV値 (-5.0 ~ 5.0, default: 0.0)
    // 1EV = 2倍の明るさ変化
    void setExposure(float ev);
    float exposure() const;

    // オフセット (-0.5 ~ 0.5, default: 0.0)
    // リニア空間でのシフト（暗部の底上げ等）
    void setOffset(float offset);
    float offset() const;

    // ガンマ補正 (0.2 ~ 5.0, default: 1.0)
    void setGammaCorrection(float gamma);
    float gammaCorrection() const;

    // プロパティ取得
    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

} // namespace Artifact