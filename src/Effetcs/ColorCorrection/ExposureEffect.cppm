module;

#include <cmath>
#include <algorithm>

module ExposureEffect;

import std;

namespace Artifact {

class ExposureEffect::Impl {
public:
    float exposure = 0.0f;        // EV (-5.0 ~ 5.0)
    float offset = 0.0f;          // (-0.5 ~ 0.5)
    float gammaCorrection = 1.0f; // (0.2 ~ 5.0)
};

ExposureEffect::ExposureEffect()
    : ArtifactAbstractEffect(), impl_(new Impl()) {
    setEffectID(UniString("effect.colorcorrection.exposure"));
    setDisplayName(UniString("Exposure"));
}

ExposureEffect::~ExposureEffect() {
    delete impl_;
}

void ExposureEffect::setExposure(float ev) {
    impl_->exposure = std::clamp(ev, -5.0f, 5.0f);
}

float ExposureEffect::exposure() const {
    return impl_->exposure;
}

void ExposureEffect::setOffset(float offset) {
    impl_->offset = std::clamp(offset, -0.5f, 0.5f);
}

float ExposureEffect::offset() const {
    return impl_->offset;
}

void ExposureEffect::setGammaCorrection(float gamma) {
    impl_->gammaCorrection = std::clamp(gamma, 0.2f, 5.0f);
}

float ExposureEffect::gammaCorrection() const {
    return impl_->gammaCorrection;
}

void ExposureEffect::apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    // ピクセルごとの処理:
    // 1. Exposure (EV): pixel * pow(2.0, exposure)
    //    → 1EV上がると2倍の明るさ（実際のカメラの絞り・シャッタースピードに相当）
    // 2. Offset: pixel + offset
    //    → リニア空間でのシフト（暗部の底上げに有効）
    // 3. Gamma Correction: pow(pixel, 1.0 / gamma)
    //    → ガンマカーブによる中間調の調整
    //
    // 処理順: Exposure → Offset → Gamma
    // Alphaチャンネルは変更しない
    //
    // 実際のピクセル操作は ImageF32x4RGBAWithCache の
    // イテレーションAPIに依存する。
}

std::vector<AbstractProperty> ExposureEffect::getProperties() const {
    // TODO: AbstractPropertyの生成仕様に準じてプロパティリストを返す
    return {};
}

void ExposureEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    // TODO: プロパティ名に応じたセッター呼び出し
}

} // namespace Artifact
