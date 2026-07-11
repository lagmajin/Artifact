module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.AnisotropicFlowBlur;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import ImageProcessing;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

class AnisotropicFlowBlurCPUImpl final : public ArtifactEffectImplBase {
public:
    AnisotropicFlowBlurSettings settings;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        AnisotropicFlowBlur processor;
        processor.process(dst.image(), settings);
    }
};

AnisotropicFlowBlurEffect::AnisotropicFlowBlurEffect() {
    setDisplayName(UniString("Anisotropic Flow Blur"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<AnisotropicFlowBlurCPUImpl>());
    syncImpl();
}

AnisotropicFlowBlurEffect::~AnisotropicFlowBlurEffect() = default;

void AnisotropicFlowBlurEffect::syncImpl() {
    if (auto* impl = dynamic_cast<AnisotropicFlowBlurCPUImpl*>(cpuImpl().get())) {
        impl->settings.blurAmount = blurAmount_;
        impl->settings.tensorNoiseScale = tensorNoiseScale_;
        impl->settings.tensorIntegrationScale = tensorIntegrationScale_;
        impl->settings.edgeAdherence = edgeAdherence_;
    }
}

std::vector<AbstractProperty> AnisotropicFlowBlurEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& amount = props.emplace_back(); amount.setName("Blur Amount"); amount.setType(PropertyType::Float); amount.setValue(blurAmount_);
    auto& noise = props.emplace_back(); noise.setName("Tensor Noise Scale"); noise.setType(PropertyType::Float); noise.setValue(tensorNoiseScale_);
    auto& integration = props.emplace_back(); integration.setName("Tensor Integration Scale"); integration.setType(PropertyType::Float); integration.setValue(tensorIntegrationScale_);
    auto& adherence = props.emplace_back(); adherence.setName("Edge Adherence"); adherence.setType(PropertyType::Float); adherence.setValue(edgeAdherence_);
    return props;
}

void AnisotropicFlowBlurEffect::setPropertyValue(const UniString& name,
                                                  const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Blur Amount")) blurAmount_ = std::clamp(value.toFloat(), 0.0f, 100.0f);
    else if (key == QStringLiteral("Tensor Noise Scale")) tensorNoiseScale_ = std::clamp(value.toFloat(), 0.1f, 20.0f);
    else if (key == QStringLiteral("Tensor Integration Scale")) tensorIntegrationScale_ = std::clamp(value.toFloat(), 0.1f, 20.0f);
    else if (key == QStringLiteral("Edge Adherence")) edgeAdherence_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
    syncImpl();
}

}
