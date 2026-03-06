module;

#include <cmath>
#include <algorithm>
#include <opencv2/opencv.hpp>

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
    dst = src;
    cv::Mat mat = dst.image().toCVMat();
    if (mat.empty()) return;

    float exposureMultiplier = std::pow(2.0f, impl_->exposure);
    float offsetVal = impl_->offset;
    float gammaInv = 1.0f / std::max(0.0001f, impl_->gammaCorrection);

    // BGR/RGBA のうちRGB(A)のRGB部分だけ処理
    // cv::MatはCV_32FC4と仮定
    for (int y = 0; y < mat.rows; ++y) {
        for (int x = 0; x < mat.cols; ++x) {
            cv::Vec4f& pixel = mat.at<cv::Vec4f>(y, x);
            
            for (int c = 0; c < 3; ++c) {
                // 1. Exposure
                float val = pixel[c] * exposureMultiplier;
                // 2. Offset
                val += offsetVal;
                // Clamp before gamma to avoid NaN on negative
                val = std::max(0.0f, val);
                // 3. Gamma
                if (gammaInv != 1.0f) {
                    val = std::pow(val, gammaInv);
                }
                pixel[c] = std::clamp(val, 0.0f, 1.0f);
            }
        }
    }

    dst.image().setFromCVMat(mat);
    dst.UpdateGpuTextureFromCpuData();
}

std::vector<AbstractProperty> ExposureEffect::getProperties() const {
    return {
        {"Exposure", impl_->exposure},
        {"Offset", impl_->offset},
        {"Gamma", impl_->gammaCorrection}
    };
}

void ExposureEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    if (name == "Exposure") {
        setExposure(value.toFloat());
    } else if (name == "Offset") {
        setOffset(value.toFloat());
    } else if (name == "Gamma") {
        setGammaCorrection(value.toFloat());
    }
}

} // namespace Artifact
