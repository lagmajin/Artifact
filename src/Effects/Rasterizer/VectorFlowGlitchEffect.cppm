module;
#include <algorithm>
#include <cmath>
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
import Memory.SharedPtr;

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
    setCPUImpl(ArtifactCore::makeShared<VectorFlowGlitchCPUImpl>());
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
    if (key == QStringLiteral("Glitch Amount")) { const float v = value.toFloat(); glitchAmount_ = std::isfinite(v) ? std::clamp(v, 0.0f, 200.0f) : 20.0f; }
    else if (key == QStringLiteral("Frequency")) { const float v = value.toFloat(); frequency_ = std::isfinite(v) ? std::clamp(v, 0.001f, 1.0f) : 0.05f; }
    else if (key == QStringLiteral("Chromatic Aberration")) { const float v = value.toFloat(); chromaticAberration_ = std::isfinite(v) ? std::clamp(v, 0.0f, 50.0f) : 5.0f; }
    else if (key == QStringLiteral("Edge Flow Influence")) { const float v = value.toFloat(); edgeFlowInfluence_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.7f; }
    else if (key == QStringLiteral("Evolution")) { const float v = value.toFloat(); seed_ = std::isfinite(v) ? v : 0.0f; }
    syncImpl();
}

}
