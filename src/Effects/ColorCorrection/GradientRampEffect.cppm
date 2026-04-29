module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QVariant>

module GradientRampEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.GradientRamp;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

class GradientRampEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::GradientRampProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        processor_.apply(pixels, dst.image().width(), dst.image().height());
    }
};

class GradientRampEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::GradientRampProcessor processor_;

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

GradientRampEffect::GradientRampEffect() {
    setEffectID(UniString("effect.colorcorrection.gradientramp"));
    setDisplayName(UniString("Gradient Ramp"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<GradientRampEffectCPUImpl>());
    setGPUImpl(std::make_shared<GradientRampEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

GradientRampEffect::~GradientRampEffect() = default;

void GradientRampEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Sunrise:
        settings_ = ArtifactCore::GradientRampSettings::sunrise();
        break;
    case Preset::Ocean:
        settings_ = ArtifactCore::GradientRampSettings::ocean();
        break;
    case Preset::Neon:
        settings_ = ArtifactCore::GradientRampSettings::neon();
        break;
    case Preset::Mono:
        settings_ = ArtifactCore::GradientRampSettings::mono();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void GradientRampEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 3);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void GradientRampEffect::setStartColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.startColor = color;
    syncImpls();
}

void GradientRampEffect::setEndColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.endColor = color;
    syncImpls();
}

void GradientRampEffect::setStartPoint(float x, float y) {
    preset_ = Preset::Custom;
    settings_.startX = std::clamp(x, 0.0f, 1.0f);
    settings_.startY = std::clamp(y, 0.0f, 1.0f);
    syncImpls();
}

void GradientRampEffect::setEndPoint(float x, float y) {
    preset_ = Preset::Custom;
    settings_.endX = std::clamp(x, 0.0f, 1.0f);
    settings_.endY = std::clamp(y, 0.0f, 1.0f);
    syncImpls();
}

void GradientRampEffect::setOpacity(float value) {
    preset_ = Preset::Custom;
    settings_.opacity = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void GradientRampEffect::setPreserveAlpha(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveAlpha = value;
    syncImpls();
}

void GradientRampEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<GradientRampEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<GradientRampEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> GradientRampEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(8);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    auto addColor = [&props](const char* name, const QColor& color, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Color);
        prop.setColorValue(color);
        prop.setValue(color);
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };
    auto addFloat = [&props](const char* name, float value, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };

    addColor("Start Color", settings_.startColor, -20);
    addColor("End Color", settings_.endColor, -19);
    addFloat("Start X", settings_.startX, -10);
    addFloat("Start Y", settings_.startY, -9);
    addFloat("End X", settings_.endX, -8);
    addFloat("End Y", settings_.endY, -7);
    addFloat("Opacity", settings_.opacity, 0);

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Alpha");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveAlpha);
    preserveProp.setDisplayPriority(10);
    props.push_back(preserveProp);

    return props;
}

void GradientRampEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    } else if (key == QStringLiteral("Start Color")) {
        setStartColor(value.value<QColor>());
    } else if (key == QStringLiteral("End Color")) {
        setEndColor(value.value<QColor>());
    } else if (key == QStringLiteral("Start X")) {
        setStartPoint(value.toFloat(), settings_.startY);
    } else if (key == QStringLiteral("Start Y")) {
        setStartPoint(settings_.startX, value.toFloat());
    } else if (key == QStringLiteral("End X")) {
        setEndPoint(value.toFloat(), settings_.endY);
    } else if (key == QStringLiteral("End Y")) {
        setEndPoint(settings_.endX, value.toFloat());
    } else if (key == QStringLiteral("Opacity")) {
        setOpacity(value.toFloat());
    } else if (key == QStringLiteral("Preserve Alpha")) {
        setPreserveAlpha(value.toBool());
    }
}

} // namespace Artifact
