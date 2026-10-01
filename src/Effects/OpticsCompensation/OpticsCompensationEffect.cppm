module;

#include <algorithm>
#include <cmath>
#include <vector>

module Artifact.Effect.Rasterizer.OpticsCompensation;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Property.Abstract;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import ImageProcessing.Distortion;
import Utils.String.UniString;
import Memory.SharedPtr;

namespace Artifact {

namespace {

class OpticsCompensationEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float fov_ = 45.0f;
    int direction_ = 1;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        const ImageF32x4_RGBA& srcImg = src.image();
        ImageF32x4_RGBA tmp;
        ArtifactCore::applyDisplacement(
            srcImg, tmp,
            ArtifactCore::makeOpticsCompensation(centerX_, centerY_, fov_, direction_)
        );
        dst.image().setFromRGBA32F(
            tmp.rgba32fData(), srcImg.width(), srcImg.height(), srcImg.colorDescriptor());
    }
};

} // namespace

OpticsCompensationEffect::OpticsCompensationEffect() {
    setDisplayName(UniString("Optics Compensation"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setComputeMode(ComputeMode::CPU);
    setCPUImpl(ArtifactCore::makeShared<OpticsCompensationEffectCPUImpl>());
}

float OpticsCompensationEffect::centerX() const { return centerX_; }
void OpticsCompensationEffect::setCenterX(float v) { centerX_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f; syncImpls(); }
float OpticsCompensationEffect::centerY() const { return centerY_; }
void OpticsCompensationEffect::setCenterY(float v) { centerY_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f; syncImpls(); }
float OpticsCompensationEffect::fov() const { return fov_; }
void OpticsCompensationEffect::setFov(float v) { fov_ = std::isfinite(v) ? std::clamp(v, 1.0f, 180.0f) : 45.0f; syncImpls(); }
int OpticsCompensationEffect::direction() const { return direction_; }
void OpticsCompensationEffect::setDirection(int v) { direction_ = (v >= 0 ? 1 : -1); syncImpls(); }

void OpticsCompensationEffect::syncImpls() {
    if (auto cpu = ArtifactCore::dynamicPointerCast<OpticsCompensationEffectCPUImpl>(cpuImpl())) {
        cpu->centerX_ = centerX_;
        cpu->centerY_ = centerY_;
        cpu->fov_ = fov_;
        cpu->direction_ = direction_;
    }
}

std::vector<ArtifactCore::AbstractProperty> OpticsCompensationEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    auto& cx = props.emplace_back(); cx.setName("Center X"); cx.setType(PropertyType::Float); cx.setValue(centerX_); cx.setMinValue(QVariant(0.0)); cx.setMaxValue(QVariant(1.0));
    auto& cy = props.emplace_back(); cy.setName("Center Y"); cy.setType(PropertyType::Float); cy.setValue(centerY_); cy.setMinValue(QVariant(0.0)); cy.setMaxValue(QVariant(1.0));
    auto& f = props.emplace_back(); f.setName("FOV"); f.setType(PropertyType::Float); f.setValue(fov_); f.setMinValue(QVariant(1.0)); f.setMaxValue(QVariant(180.0));
    auto& d = props.emplace_back(); d.setName("Direction"); d.setType(PropertyType::Integer); d.setValue(direction_); d.setMinValue(QVariant(-1)); d.setMaxValue(QVariant(1));
    return props;
}

void OpticsCompensationEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Center X") setCenterX(v.toFloat());
    else if (k == "Center Y") setCenterY(v.toFloat());
    else if (k == "FOV") setFov(v.toFloat());
    else if (k == "Direction") setDirection(v.toInt());
}

} // namespace Artifact
