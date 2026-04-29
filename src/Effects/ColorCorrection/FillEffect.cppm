module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QVariant>

module FillEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.Fill;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

class FillEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::SolidFillProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        processor_.apply(pixels, dst.image().width(), dst.image().height());
    }
};

class FillEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::SolidFillProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        processor_.apply(pixels, dst.image().width(), dst.image().height());
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
};

FillEffect::FillEffect() {
    setEffectID(UniString("effect.colorcorrection.fill"));
    setDisplayName(UniString("Fill"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<FillEffectCPUImpl>());
    setGPUImpl(std::make_shared<FillEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

FillEffect::~FillEffect() = default;

void FillEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::White:
        settings_ = ArtifactCore::SolidFillSettings::white();
        break;
    case Preset::Black:
        settings_ = ArtifactCore::SolidFillSettings::black();
        break;
    case Preset::Red:
        settings_ = ArtifactCore::SolidFillSettings::red();
        break;
    case Preset::Blue:
        settings_ = ArtifactCore::SolidFillSettings::blue();
        break;
    case Preset::Green:
        settings_ = ArtifactCore::SolidFillSettings::green();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void FillEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 4);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void FillEffect::setColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.color = color;
    syncImpls();
}

void FillEffect::setOpacity(float value) {
    preset_ = Preset::Custom;
    settings_.opacity = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void FillEffect::setPreserveAlpha(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveAlpha = value;
    syncImpls();
}

void FillEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<FillEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<FillEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> FillEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(5);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    AbstractProperty colorProp;
    colorProp.setName("Color");
    colorProp.setType(PropertyType::Color);
    colorProp.setColorValue(settings_.color);
    colorProp.setValue(settings_.color);
    colorProp.setDisplayPriority(-20);
    props.push_back(colorProp);

    AbstractProperty opacityProp;
    opacityProp.setName("Opacity");
    opacityProp.setType(PropertyType::Float);
    opacityProp.setValue(QVariant(static_cast<double>(settings_.opacity)));
    opacityProp.setDisplayPriority(0);
    props.push_back(opacityProp);

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Alpha");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveAlpha);
    preserveProp.setDisplayPriority(10);
    props.push_back(preserveProp);

    return props;
}

void FillEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    } else if (key == QStringLiteral("Color")) {
        setColor(value.value<QColor>());
    } else if (key == QStringLiteral("Opacity")) {
        setOpacity(value.toFloat());
    } else if (key == QStringLiteral("Preserve Alpha")) {
        setPreserveAlpha(value.toBool());
    }
}

} // namespace Artifact
