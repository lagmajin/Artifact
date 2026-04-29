module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QVariant>

module ColorBalanceEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.ColorBalance;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

class ColorBalanceEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ColorBalanceProcessor processor_;

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

class ColorBalanceEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ColorBalanceProcessor processor_;

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

ColorBalanceEffect::ColorBalanceEffect() {
    setEffectID(UniString("effect.colorcorrection.colorbalance"));
    setDisplayName(UniString("Color Balance"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ColorBalanceEffectCPUImpl>());
    setGPUImpl(std::make_shared<ColorBalanceEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

ColorBalanceEffect::~ColorBalanceEffect() = default;

void ColorBalanceEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Neutral:
        settings_ = ColorBalanceSettings::neutral();
        break;
    case Preset::CoolShadows:
        settings_ = ColorBalanceSettings::coolShadows();
        break;
    case Preset::WarmHighlights:
        settings_ = ColorBalanceSettings::warmHighlights();
        break;
    case Preset::Cinematic:
        settings_ = ColorBalanceSettings::cinematic();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void ColorBalanceEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 4);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void ColorBalanceEffect::setShadowBalance(float r, float g, float b) {
    preset_ = Preset::Custom;
    settings_.shadowR = std::clamp(r, -1.0f, 1.0f);
    settings_.shadowG = std::clamp(g, -1.0f, 1.0f);
    settings_.shadowB = std::clamp(b, -1.0f, 1.0f);
    syncImpls();
}

void ColorBalanceEffect::setMidtoneBalance(float r, float g, float b) {
    preset_ = Preset::Custom;
    settings_.midtoneR = std::clamp(r, -1.0f, 1.0f);
    settings_.midtoneG = std::clamp(g, -1.0f, 1.0f);
    settings_.midtoneB = std::clamp(b, -1.0f, 1.0f);
    syncImpls();
}

void ColorBalanceEffect::setHighlightBalance(float r, float g, float b) {
    preset_ = Preset::Custom;
    settings_.highlightR = std::clamp(r, -1.0f, 1.0f);
    settings_.highlightG = std::clamp(g, -1.0f, 1.0f);
    settings_.highlightB = std::clamp(b, -1.0f, 1.0f);
    syncImpls();
}

void ColorBalanceEffect::setShadowRange(float value) {
    preset_ = Preset::Custom;
    settings_.shadowRange = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void ColorBalanceEffect::setHighlightRange(float value) {
    preset_ = Preset::Custom;
    settings_.highlightRange = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void ColorBalanceEffect::setMasterStrength(float value) {
    preset_ = Preset::Custom;
    settings_.masterStrength = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void ColorBalanceEffect::setPreserveLuma(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveLuma = value;
    syncImpls();
}

void ColorBalanceEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<ColorBalanceEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<ColorBalanceEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> ColorBalanceEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(14);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    auto addFloat = [&props](const char* name, float value, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };

    addFloat("Shadow R", settings_.shadowR, -20);
    addFloat("Shadow G", settings_.shadowG, -19);
    addFloat("Shadow B", settings_.shadowB, -18);
    addFloat("Midtone R", settings_.midtoneR, -10);
    addFloat("Midtone G", settings_.midtoneG, -9);
    addFloat("Midtone B", settings_.midtoneB, -8);
    addFloat("Highlight R", settings_.highlightR, 0);
    addFloat("Highlight G", settings_.highlightG, 1);
    addFloat("Highlight B", settings_.highlightB, 2);
    addFloat("Shadow Range", settings_.shadowRange, 10);
    addFloat("Highlight Range", settings_.highlightRange, 11);
    addFloat("Strength", settings_.masterStrength, 20);

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Luma");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveLuma);
    preserveProp.setDisplayPriority(30);
    props.push_back(preserveProp);

    return props;
}

void ColorBalanceEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    } else if (key == QStringLiteral("Shadow R")) {
        setShadowBalance(value.toFloat(), settings_.shadowG, settings_.shadowB);
    } else if (key == QStringLiteral("Shadow G")) {
        setShadowBalance(settings_.shadowR, value.toFloat(), settings_.shadowB);
    } else if (key == QStringLiteral("Shadow B")) {
        setShadowBalance(settings_.shadowR, settings_.shadowG, value.toFloat());
    } else if (key == QStringLiteral("Midtone R")) {
        setMidtoneBalance(value.toFloat(), settings_.midtoneG, settings_.midtoneB);
    } else if (key == QStringLiteral("Midtone G")) {
        setMidtoneBalance(settings_.midtoneR, value.toFloat(), settings_.midtoneB);
    } else if (key == QStringLiteral("Midtone B")) {
        setMidtoneBalance(settings_.midtoneR, settings_.midtoneG, value.toFloat());
    } else if (key == QStringLiteral("Highlight R")) {
        setHighlightBalance(value.toFloat(), settings_.highlightG, settings_.highlightB);
    } else if (key == QStringLiteral("Highlight G")) {
        setHighlightBalance(settings_.highlightR, value.toFloat(), settings_.highlightB);
    } else if (key == QStringLiteral("Highlight B")) {
        setHighlightBalance(settings_.highlightR, settings_.highlightG, value.toFloat());
    } else if (key == QStringLiteral("Shadow Range")) {
        setShadowRange(value.toFloat());
    } else if (key == QStringLiteral("Highlight Range")) {
        setHighlightRange(value.toFloat());
    } else if (key == QStringLiteral("Strength")) {
        setMasterStrength(value.toFloat());
    } else if (key == QStringLiteral("Preserve Luma")) {
        setPreserveLuma(value.toBool());
    }
}

} // namespace Artifact
