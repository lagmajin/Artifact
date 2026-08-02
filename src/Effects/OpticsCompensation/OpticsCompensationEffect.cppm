module Artifact.Effect.Rasterizer.OpticsCompensation;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import ImageProcessing.Distortion;
import std;

namespace Artifact {

OpticsCompensationEffect::OpticsCompensationEffect() {
    setDisplayName(UniString("Optics Compensation"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setComputeMode(ComputeMode::CPU);
}

float OpticsCompensationEffect::centerX() const { return centerX_; }
void OpticsCompensationEffect::setCenterX(float v) { centerX_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f; }
float OpticsCompensationEffect::centerY() const { return centerY_; }
void OpticsCompensationEffect::setCenterY(float v) { centerY_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f; }
float OpticsCompensationEffect::fov() const { return fov_; }
void OpticsCompensationEffect::setFov(float v) { fov_ = std::isfinite(v) ? std::clamp(v, 1.0f, 180.0f) : 45.0f; }
int OpticsCompensationEffect::direction() const { return direction_; }
void OpticsCompensationEffect::setDirection(int v) { direction_ = (v >= 0 ? 1 : -1); }

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

void OpticsCompensationEffect::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const ImageF32x4_RGBA& srcImg = src.image();
    int w = srcImg.width();
    int h = srcImg.height();
    ImageF32x4_RGBA tmp;
    ArtifactCore::applyDisplacement(
        srcImg, tmp,
        ArtifactCore::makeOpticsCompensation(centerX_, centerY_, fov_, direction_)
    );
    dst.image().setFromRGBA32F(
        tmp.rgba32fData(), w, h, srcImg.colorDescriptor());
}

}
