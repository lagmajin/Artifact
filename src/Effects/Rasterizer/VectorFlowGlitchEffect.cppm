module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.VectorFlowGlitch;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import ImageProcessing;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

class VectorFlowGlitchCPUImpl final : public ArtifactEffectImplBase {
public:
    VectorFlowGlitchSettings settings;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        VectorFlowGlitch processor;
        processor.process(dst.image(), settings);
    }
};

VectorFlowGlitchEffect::VectorFlowGlitchEffect() {
    setDisplayName(UniString("Vector Flow Glitch"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<VectorFlowGlitchCPUImpl>());
    syncImpl();
}

VectorFlowGlitchEffect::~VectorFlowGlitchEffect() = default;

void VectorFlowGlitchEffect::syncImpl() {
    if (auto* impl = dynamic_cast<VectorFlowGlitchCPUImpl*>(cpuImpl().get())) {
        impl->settings.glitchAmount = glitchAmount_;
        impl->settings.frequency = frequency_;
        impl->settings.chromaticAberration = chromaticAberration_;
        impl->settings.edgeFlowInfluence = edgeFlowInfluence_;
        impl->settings.seed = seed_;
    }
}

std::vector<AbstractProperty> VectorFlowGlitchEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& amount = props.emplace_back(); amount.setName("Glitch Amount"); amount.setType(PropertyType::Float); amount.setValue(glitchAmount_);
    auto& frequency = props.emplace_back(); frequency.setName("Frequency"); frequency.setType(PropertyType::Float); frequency.setValue(frequency_);
    auto& chroma = props.emplace_back(); chroma.setName("Chromatic Aberration"); chroma.setType(PropertyType::Float); chroma.setValue(chromaticAberration_);
    auto& flow = props.emplace_back(); flow.setName("Edge Flow Influence"); flow.setType(PropertyType::Float); flow.setValue(edgeFlowInfluence_);
    auto& seed = props.emplace_back(); seed.setName("Evolution"); seed.setType(PropertyType::Float); seed.setValue(seed_);
    return props;
}

void VectorFlowGlitchEffect::setPropertyValue(const UniString& name,
                                               const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Glitch Amount")) glitchAmount_ = std::clamp(value.toFloat(), 0.0f, 200.0f);
    else if (key == QStringLiteral("Frequency")) frequency_ = std::clamp(value.toFloat(), 0.001f, 1.0f);
    else if (key == QStringLiteral("Chromatic Aberration")) chromaticAberration_ = std::clamp(value.toFloat(), 0.0f, 50.0f);
    else if (key == QStringLiteral("Edge Flow Influence")) edgeFlowInfluence_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
    else if (key == QStringLiteral("Evolution")) seed_ = value.toFloat();
    syncImpl();
}

}
