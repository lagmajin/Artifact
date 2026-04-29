module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QColor>
#include <QVariant>

module TritoneEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.Tritone;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

class TritoneEffectCPUImpl : public ArtifactEffectImplBase {
public:
    TritoneProcessor processor_;

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

class TritoneEffectGPUImpl : public ArtifactEffectImplBase {
public:
    TritoneProcessor processor_;

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

namespace {
QColor toQColor(const TritoneColor& color) {
    return QColor::fromRgbF(color.r, color.g, color.b);
}

TritoneColor toColor(const QColor& color) {
    return {
        static_cast<float>(color.redF()),
        static_cast<float>(color.greenF()),
        static_cast<float>(color.blueF())
    };
}
}

TritoneEffect::TritoneEffect() {
    setEffectID(UniString("effect.colorcorrection.tritone"));
    setDisplayName(UniString("Tritone"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<TritoneEffectCPUImpl>());
    setGPUImpl(std::make_shared<TritoneEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

TritoneEffect::~TritoneEffect() = default;

void TritoneEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Cinematic:
        settings_ = TritoneSettings::cinematic();
        break;
    case Preset::TealAndOrange:
        settings_ = TritoneSettings::tealAndOrange();
        break;
    case Preset::WarmGold:
        settings_.shadowColor = {0.12f, 0.18f, 0.24f};
        settings_.midtoneColor = {0.52f, 0.47f, 0.38f};
        settings_.highlightColor = {0.98f, 0.86f, 0.56f};
        settings_.balance = 0.50f;
        settings_.softness = 0.60f;
        settings_.shadowStrength = 0.85f;
        settings_.midtoneStrength = 0.90f;
        settings_.highlightStrength = 1.00f;
        settings_.masterStrength = 1.0f;
        settings_.colorMix = 0.88f;
        settings_.preserveLuma = true;
        break;
    case Preset::ColdBlue:
        settings_.shadowColor = {0.06f, 0.14f, 0.28f};
        settings_.midtoneColor = {0.42f, 0.50f, 0.58f};
        settings_.highlightColor = {0.82f, 0.92f, 1.00f};
        settings_.balance = 0.47f;
        settings_.softness = 0.58f;
        settings_.shadowStrength = 1.00f;
        settings_.midtoneStrength = 0.88f;
        settings_.highlightStrength = 0.92f;
        settings_.masterStrength = 1.0f;
        settings_.colorMix = 0.82f;
        settings_.preserveLuma = true;
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void TritoneEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 4);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void TritoneEffect::setShadowColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.shadowColor = toColor(color);
    syncImpls();
}

void TritoneEffect::setMidtoneColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.midtoneColor = toColor(color);
    syncImpls();
}

void TritoneEffect::setHighlightColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.highlightColor = toColor(color);
    syncImpls();
}

void TritoneEffect::setBalance(float value) {
    preset_ = Preset::Custom;
    settings_.balance = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void TritoneEffect::setSoftness(float value) {
    preset_ = Preset::Custom;
    settings_.softness = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void TritoneEffect::setMasterStrength(float value) {
    preset_ = Preset::Custom;
    settings_.masterStrength = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void TritoneEffect::setColorMix(float value) {
    preset_ = Preset::Custom;
    settings_.colorMix = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void TritoneEffect::setPreserveLuma(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveLuma = value;
    syncImpls();
}

void TritoneEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<TritoneEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<TritoneEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> TritoneEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(8);

    auto addColor = [&props](const char* name, const QColor& color) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Color);
        prop.setColorValue(color);
        prop.setValue(color);
        props.push_back(prop);
    };

    auto addFloat = [&props](const char* name, float value) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        props.push_back(prop);
    };

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-20);
    props.push_back(presetProp);

    addColor("Shadow Color", toQColor(settings_.shadowColor));
    addColor("Midtone Color", toQColor(settings_.midtoneColor));
    addColor("Highlight Color", toQColor(settings_.highlightColor));
    if (!props.empty()) {
        props[1].setDisplayPriority(-10);
        props[2].setDisplayPriority(-9);
        props[3].setDisplayPriority(-8);
    }
    addFloat("Balance", settings_.balance);
    addFloat("Softness", settings_.softness);
    addFloat("Master Strength", settings_.masterStrength);
    addFloat("Color Mix", settings_.colorMix);

    if (props.size() >= 8) {
        props[4].setDisplayPriority(0);
        props[5].setDisplayPriority(5);
        props[6].setDisplayPriority(10);
        props[7].setDisplayPriority(15);
    }

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Luma");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveLuma);
    preserveProp.setDisplayPriority(20);
    props.push_back(preserveProp);

    return props;
}

void TritoneEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Shadow Color")) {
        setShadowColor(value.value<QColor>());
    } else if (key == QStringLiteral("Midtone Color")) {
        setMidtoneColor(value.value<QColor>());
    } else if (key == QStringLiteral("Highlight Color")) {
        setHighlightColor(value.value<QColor>());
    } else if (key == QStringLiteral("Balance")) {
        setBalance(value.toFloat());
    } else if (key == QStringLiteral("Softness")) {
        setSoftness(value.toFloat());
    } else if (key == QStringLiteral("Master Strength")) {
        setMasterStrength(value.toFloat());
    } else if (key == QStringLiteral("Color Mix")) {
        setColorMix(value.toFloat());
    } else if (key == QStringLiteral("Preserve Luma")) {
        setPreserveLuma(value.toBool());
    } else if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    }
}

} // namespace Artifact
