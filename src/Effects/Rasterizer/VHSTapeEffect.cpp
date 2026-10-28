module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.VHSTape;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import VHS_CV;
import Memory.SharedPtr;

namespace Artifact {

using namespace ArtifactCore;

class VHSTapeCPUImpl final : public ArtifactEffectImplBase {
public:
    VHSParams params;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const auto& image = src.image();
        const float* pixels = image.rgba32fData();
        const int width = image.width();
        const int height = image.height();
        if (!pixels || width <= 0 || height <= 0) {
            dst = src;
            return;
        }

        cv::Mat input(height, width, CV_32FC4, const_cast<float*>(pixels));
        cv::Mat output = vhsEffect(input, params);

        dst = src;
        dst.image().setFromCVMat(output, src.image().colorDescriptor());
    }
};

VHSTapeEffect::VHSTapeEffect() {
    setDisplayName(UniString("VHS Tape"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<VHSTapeCPUImpl>());
    syncImpl();
}

VHSTapeEffect::~VHSTapeEffect() = default;

void VHSTapeEffect::syncImpl() {
    if (auto* impl = dynamic_cast<VHSTapeCPUImpl*>(cpuImpl().get())) {
        impl->params.trackingError = trackingError_;
        impl->params.noiseAmount = noiseAmount_;
        impl->params.colorBleed = colorBleed_;
        impl->params.scanlineGap = scanlineGap_;
        impl->params.scanlineIntensity = scanlineIntensity_;
        impl->params.wobble = wobble_;
        impl->params.saturation = saturation_;
        impl->params.sharpness = sharpness_;
        impl->params.bottomJitter = bottomJitter_;
        impl->params.evolution = evolution_;
        impl->params.seed = seed_;
        impl->params.showOsd = showOsd_;
        impl->params.osdMode = osdMode_;
    }
}

std::vector<AbstractProperty> VHSTapeEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& tracking = props.emplace_back(); tracking.setName("Tracking Error"); tracking.setType(PropertyType::Float); tracking.setValue(trackingError_);
    auto& noise = props.emplace_back(); noise.setName("Noise Amount"); noise.setType(PropertyType::Float); noise.setValue(noiseAmount_);
    auto& bleed = props.emplace_back(); bleed.setName("Color Bleed"); bleed.setType(PropertyType::Float); bleed.setValue(colorBleed_);
    auto& scanGap = props.emplace_back(); scanGap.setName("Scanline Gap"); scanGap.setType(PropertyType::Float); scanGap.setValue(scanlineGap_);
    auto& scanInt = props.emplace_back(); scanInt.setName("Scanline Intensity"); scanInt.setType(PropertyType::Float); scanInt.setValue(scanlineIntensity_);
    auto& wobble = props.emplace_back(); wobble.setName("Wobble"); wobble.setType(PropertyType::Float); wobble.setValue(wobble_);
    auto& sat = props.emplace_back(); sat.setName("Saturation"); sat.setType(PropertyType::Float); sat.setValue(saturation_);
    auto& sharp = props.emplace_back(); sharp.setName("Sharpness"); sharp.setType(PropertyType::Float); sharp.setValue(sharpness_);
    auto& bottom = props.emplace_back(); bottom.setName("Bottom Jitter"); bottom.setType(PropertyType::Float); bottom.setValue(bottomJitter_);
    auto& osd = props.emplace_back(); osd.setName("Show OSD"); osd.setType(PropertyType::Boolean); osd.setValue(showOsd_);
    auto& osdM = props.emplace_back(); osdM.setName("OSD Mode"); osdM.setType(PropertyType::Integer); osdM.setValue(osdMode_);
    auto& evolution = props.emplace_back(); evolution.setName("Evolution"); evolution.setType(PropertyType::Float); evolution.setValue(evolution_);
    auto& seed = props.emplace_back(); seed.setName("Seed"); seed.setType(PropertyType::Integer); seed.setValue(seed_);
    return props;
}

void VHSTapeEffect::setPropertyValue(const UniString& name,
                                     const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Tracking Error")) { const float v = value.toFloat(); trackingError_ = std::isfinite(v) ? std::clamp(v, 0.0f, 2.0f) : 0.3f; }
    else if (key == QStringLiteral("Noise Amount")) { const float v = value.toFloat(); noiseAmount_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.15f; }
    else if (key == QStringLiteral("Color Bleed")) { const float v = value.toFloat(); colorBleed_ = std::isfinite(v) ? std::clamp(v, 0.0f, 2.0f) : 0.5f; }
    else if (key == QStringLiteral("Scanline Gap")) { const float v = value.toFloat(); scanlineGap_ = std::isfinite(v) ? std::clamp(v, 1.0f, 20.0f) : 2.0f; }
    else if (key == QStringLiteral("Scanline Intensity")) { const float v = value.toFloat(); scanlineIntensity_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.2f; }
    else if (key == QStringLiteral("Wobble")) { const float v = value.toFloat(); wobble_ = std::isfinite(v) ? std::clamp(v, 0.0f, 2.0f) : 0.2f; }
    else if (key == QStringLiteral("Saturation")) { const float v = value.toFloat(); saturation_ = std::isfinite(v) ? std::clamp(v, 0.0f, 2.0f) : 0.8f; }
    else if (key == QStringLiteral("Sharpness")) { const float v = value.toFloat(); sharpness_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.6f; }
    else if (key == QStringLiteral("Bottom Jitter")) { const float v = value.toFloat(); bottomJitter_ = std::isfinite(v) ? std::clamp(v, 0.0f, 2.0f) : 0.5f; }
    else if (key == QStringLiteral("Show OSD")) { showOsd_ = value.toBool(); }
    else if (key == QStringLiteral("OSD Mode")) { osdMode_ = std::clamp(value.toInt(), 0, 3); }
    else if (key == QStringLiteral("Evolution")) { const float v = value.toFloat(); evolution_ = std::isfinite(v) ? v : 0.0f; }
    else if (key == QStringLiteral("Seed")) { seed_ = value.toInt(); }
    syncImpl();
}

}
