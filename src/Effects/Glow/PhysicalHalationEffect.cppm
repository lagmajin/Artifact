module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

module Artifact.Effect.Glow.PhysicalHalation;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ArtifactCore.ImageProcessing.Halation;
import Image.ImageF32x4RGBAWithCache;
import Particle;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

class PhysicalHalationCPUImpl final : public ArtifactEffectImplBase {
public:
    PhysicalHalation::Settings settings;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        auto& image = dst.image();
        auto* pixels = image.rgba32fData();
        if (!pixels || image.width() <= 0 || image.height() <= 0) return;
        PhysicalHalation processor;
        processor.process(reinterpret_cast<float4*>(pixels), image.width(),
                          image.height(), settings);
    }
};

PhysicalHalationEffect::PhysicalHalationEffect() {
    setDisplayName(UniString("Physical Halation"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<PhysicalHalationCPUImpl>());
    syncImpl();
}

PhysicalHalationEffect::~PhysicalHalationEffect() = default;

void PhysicalHalationEffect::syncImpl() {
    if (auto* impl = dynamic_cast<PhysicalHalationCPUImpl*>(cpuImpl().get())) {
        impl->settings.threshold = threshold_;
        impl->settings.spread = spread_;
        impl->settings.intensity = intensity_;
        impl->settings.redDiffusion = redDiffusion_;
        impl->settings.softness = softness_;
    }
}

std::vector<AbstractProperty> PhysicalHalationEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& threshold = props.emplace_back(); threshold.setName("Threshold"); threshold.setType(PropertyType::Float); threshold.setValue(threshold_);
    auto& spread = props.emplace_back(); spread.setName("Spread"); spread.setType(PropertyType::Float); spread.setValue(spread_);
    auto& intensity = props.emplace_back(); intensity.setName("Intensity"); intensity.setType(PropertyType::Float); intensity.setValue(intensity_);
    auto& red = props.emplace_back(); red.setName("Red Diffusion"); red.setType(PropertyType::Float); red.setValue(redDiffusion_);
    auto& softness = props.emplace_back(); softness.setName("Softness"); softness.setType(PropertyType::Float); softness.setValue(softness_);
    return props;
}

void PhysicalHalationEffect::setPropertyValue(const UniString& name,
                                               const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Threshold")) threshold_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
    else if (key == QStringLiteral("Spread")) spread_ = std::clamp(value.toFloat(), 0.0f, 100.0f);
    else if (key == QStringLiteral("Intensity")) intensity_ = std::clamp(value.toFloat(), 0.0f, 5.0f);
    else if (key == QStringLiteral("Red Diffusion")) redDiffusion_ = std::clamp(value.toFloat(), 0.1f, 5.0f);
    else if (key == QStringLiteral("Softness")) softness_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
    syncImpl();
}

}
