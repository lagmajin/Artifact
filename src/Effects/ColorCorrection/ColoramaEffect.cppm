module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QVariant>

module ColoramaEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.Colorama;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

class ColoramaEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ColoramaProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        const int width = dst.image().width();
        const int height = dst.image().height();
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                processor_.applyPixel(pixel[0], pixel[1], pixel[2]);
            }
        }
    }
};

class ColoramaEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ColoramaProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        const int width = dst.image().width();
        const int height = dst.image().height();
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                processor_.applyPixel(pixel[0], pixel[1], pixel[2]);
            }
        }
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
};

ColoramaEffect::ColoramaEffect() {
    setEffectID(UniString("effect.colorcorrection.colorama"));
    setDisplayName(UniString("Colorama"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ColoramaEffectCPUImpl>());
    setGPUImpl(std::make_shared<ColoramaEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

ColoramaEffect::~ColoramaEffect() = default;

void ColoramaEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Rainbow:
        settings_ = ColoramaSettings::rainbow();
        break;
    case Preset::Fire:
        settings_ = ColoramaSettings::fire();
        break;
    case Preset::Ocean:
        settings_ = ColoramaSettings::ocean();
        break;
    case Preset::Neon:
        settings_.sourceMode = ColoramaSourceMode::Hue;
        settings_.palette = ColoramaPalette::Neon;
        settings_.phase = 0.0f;
        settings_.spread = 1.0f;
        settings_.strength = 1.0f;
        settings_.saturationBoost = 1.4f;
        settings_.contrast = 1.15f;
        settings_.preserveLuma = false;
        break;
    case Preset::Sunset:
        settings_.sourceMode = ColoramaSourceMode::Luma;
        settings_.palette = ColoramaPalette::Sunset;
        settings_.phase = 0.05f;
        settings_.spread = 1.0f;
        settings_.strength = 1.0f;
        settings_.saturationBoost = 1.1f;
        settings_.contrast = 1.05f;
        settings_.preserveLuma = true;
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void ColoramaEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 5);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void ColoramaEffect::setSourceMode(ColoramaSourceMode mode) {
    preset_ = Preset::Custom;
    settings_.sourceMode = mode;
    syncImpls();
}

void ColoramaEffect::setPalette(ColoramaPalette palette) {
    preset_ = Preset::Custom;
    settings_.palette = palette;
    syncImpls();
}

void ColoramaEffect::setPhase(float value) {
    preset_ = Preset::Custom;
    settings_.phase = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void ColoramaEffect::setSpread(float value) {
    preset_ = Preset::Custom;
    settings_.spread = std::max(0.0f, value);
    syncImpls();
}

void ColoramaEffect::setStrength(float value) {
    preset_ = Preset::Custom;
    settings_.strength = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void ColoramaEffect::setSaturationBoost(float value) {
    preset_ = Preset::Custom;
    settings_.saturationBoost = std::clamp(value, 0.0f, 2.5f);
    syncImpls();
}

void ColoramaEffect::setContrast(float value) {
    preset_ = Preset::Custom;
    settings_.contrast = std::clamp(value, 0.0f, 2.5f);
    syncImpls();
}

void ColoramaEffect::setPreserveLuma(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveLuma = value;
    syncImpls();
}

void ColoramaEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<ColoramaEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<ColoramaEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> ColoramaEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(8);

    AbstractProperty sourceModeProp;
    sourceModeProp.setName("Source Mode");
    sourceModeProp.setType(PropertyType::Integer);
    sourceModeProp.setValue(static_cast<int>(settings_.sourceMode));
    sourceModeProp.setDisplayPriority(-20);
    props.push_back(sourceModeProp);

    AbstractProperty paletteProp;
    paletteProp.setName("Palette");
    paletteProp.setType(PropertyType::Integer);
    paletteProp.setValue(static_cast<int>(settings_.palette));
    paletteProp.setDisplayPriority(-15);
    props.push_back(paletteProp);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    auto addFloat = [&props](const char* name, float value) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        props.push_back(prop);
    };

    addFloat("Phase", settings_.phase);
    addFloat("Spread", settings_.spread);
    addFloat("Strength", settings_.strength);
    addFloat("Saturation Boost", settings_.saturationBoost);
    addFloat("Contrast", settings_.contrast);

    if (props.size() >= 8) {
        props[3].setDisplayPriority(0);
        props[4].setDisplayPriority(5);
        props[5].setDisplayPriority(10);
        props[6].setDisplayPriority(15);
        props[7].setDisplayPriority(20);
    }

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Luma");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveLuma);
    preserveProp.setDisplayPriority(25);
    props.push_back(preserveProp);

    return props;
}

void ColoramaEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Source Mode")) {
        setSourceMode(static_cast<ColoramaSourceMode>(value.toInt()));
    } else if (key == QStringLiteral("Palette")) {
        setPalette(static_cast<ColoramaPalette>(value.toInt()));
    } else if (key == QStringLiteral("Phase")) {
        setPhase(value.toFloat());
    } else if (key == QStringLiteral("Spread")) {
        setSpread(value.toFloat());
    } else if (key == QStringLiteral("Strength")) {
        setStrength(value.toFloat());
    } else if (key == QStringLiteral("Saturation Boost")) {
        setSaturationBoost(value.toFloat());
    } else if (key == QStringLiteral("Contrast")) {
        setContrast(value.toFloat());
    } else if (key == QStringLiteral("Preserve Luma")) {
        setPreserveLuma(value.toBool());
    } else if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    }
}

} // namespace Artifact
