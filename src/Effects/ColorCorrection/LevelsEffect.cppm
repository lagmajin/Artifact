module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QVariant>

module LevelsEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.LevelsCurves;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

class LevelsEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::LevelsEffect processor_;

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

class LevelsEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::LevelsEffect processor_;

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

LevelsEffect::LevelsEffect() {
    setEffectID(UniString("effect.colorcorrection.levels"));
    setDisplayName(UniString("Levels"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<LevelsEffectCPUImpl>());
    setGPUImpl(std::make_shared<LevelsEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

LevelsEffect::~LevelsEffect() = default;

void LevelsEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Normal:
        settings_ = ArtifactCore::LevelsSettings::normal();
        break;
    case Preset::HighContrast:
        settings_ = ArtifactCore::LevelsSettings::highContrast();
        break;
    case Preset::LowContrast:
        settings_ = ArtifactCore::LevelsSettings::lowContrast();
        break;
    case Preset::Brighten:
        settings_ = ArtifactCore::LevelsSettings::brighten();
        break;
    case Preset::Darken:
        settings_ = ArtifactCore::LevelsSettings::darken();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void LevelsEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 5);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void LevelsEffect::setInputBlack(float value) {
    preset_ = Preset::Custom;
    settings_.inputBlack = value;
    syncImpls();
}

void LevelsEffect::setInputWhite(float value) {
    preset_ = Preset::Custom;
    settings_.inputWhite = value;
    syncImpls();
}

void LevelsEffect::setInputGamma(float value) {
    preset_ = Preset::Custom;
    settings_.inputGamma = std::clamp(static_cast<double>(value), 0.01, 10.0);
    syncImpls();
}

void LevelsEffect::setOutputBlack(float value) {
    preset_ = Preset::Custom;
    settings_.outputBlack = value;
    syncImpls();
}

void LevelsEffect::setOutputWhite(float value) {
    preset_ = Preset::Custom;
    settings_.outputWhite = value;
    syncImpls();
}

void LevelsEffect::setPerChannel(bool value) {
    preset_ = Preset::Custom;
    settings_.perChannel = value;
    syncImpls();
}

void LevelsEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<LevelsEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<LevelsEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> LevelsEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(20);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    auto addFloat = [&props](const char* name, double value, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(value));
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };

    addFloat("Input Black", settings_.inputBlack, -20);
    addFloat("Input White", settings_.inputWhite, -19);
    addFloat("Input Gamma", settings_.inputGamma, -18);
    addFloat("Output Black", settings_.outputBlack, -10);
    addFloat("Output White", settings_.outputWhite, -9);

    AbstractProperty perChannelProp;
    perChannelProp.setName("Per Channel");
    perChannelProp.setType(PropertyType::Boolean);
    perChannelProp.setValue(settings_.perChannel);
    perChannelProp.setDisplayPriority(0);
    props.push_back(perChannelProp);

    addFloat("Red Input Black", settings_.red.inputBlack, 10);
    addFloat("Red Input White", settings_.red.inputWhite, 11);
    addFloat("Red Input Gamma", settings_.red.inputGamma, 12);
    addFloat("Red Output Black", settings_.red.outputBlack, 13);
    addFloat("Red Output White", settings_.red.outputWhite, 14);

    addFloat("Green Input Black", settings_.green.inputBlack, 20);
    addFloat("Green Input White", settings_.green.inputWhite, 21);
    addFloat("Green Input Gamma", settings_.green.inputGamma, 22);
    addFloat("Green Output Black", settings_.green.outputBlack, 23);
    addFloat("Green Output White", settings_.green.outputWhite, 24);

    addFloat("Blue Input Black", settings_.blue.inputBlack, 30);
    addFloat("Blue Input White", settings_.blue.inputWhite, 31);
    addFloat("Blue Input Gamma", settings_.blue.inputGamma, 32);
    addFloat("Blue Output Black", settings_.blue.outputBlack, 33);
    addFloat("Blue Output White", settings_.blue.outputWhite, 34);

    return props;
}

void LevelsEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    } else if (key == QStringLiteral("Input Black")) {
        setInputBlack(static_cast<float>(value.toDouble()));
    } else if (key == QStringLiteral("Input White")) {
        setInputWhite(static_cast<float>(value.toDouble()));
    } else if (key == QStringLiteral("Input Gamma")) {
        setInputGamma(static_cast<float>(value.toDouble()));
    } else if (key == QStringLiteral("Output Black")) {
        setOutputBlack(static_cast<float>(value.toDouble()));
    } else if (key == QStringLiteral("Output White")) {
        setOutputWhite(static_cast<float>(value.toDouble()));
    } else if (key == QStringLiteral("Per Channel")) {
        setPerChannel(value.toBool());
    } else if (key == QStringLiteral("Red Input Black")) {
        preset_ = Preset::Custom;
        settings_.red.inputBlack = value.toDouble();
        syncImpls();
    } else if (key == QStringLiteral("Red Input White")) {
        preset_ = Preset::Custom;
        settings_.red.inputWhite = value.toDouble();
        syncImpls();
    } else if (key == QStringLiteral("Red Input Gamma")) {
        preset_ = Preset::Custom;
        settings_.red.inputGamma = value.toDouble();
        syncImpls();
    } else if (key == QStringLiteral("Red Output Black")) {
        preset_ = Preset::Custom;
        settings_.red.outputBlack = value.toDouble();
        syncImpls();
    } else if (key == QStringLiteral("Red Output White")) {
        preset_ = Preset::Custom;
        settings_.red.outputWhite = value.toDouble();
        syncImpls();
    } else if (key == QStringLiteral("Green Input Black")) {
        preset_ = Preset::Custom;
        settings_.green.inputBlack = value.toDouble();
        syncImpls();
    } else if (key == QStringLiteral("Green Input White")) {
        preset_ = Preset::Custom;
        settings_.green.inputWhite = value.toDouble();
        syncImpls();
    } else if (key == QStringLiteral("Green Input Gamma")) {
        preset_ = Preset::Custom;
        settings_.green.inputGamma = value.toDouble();
        syncImpls();
    } else if (key == QStringLiteral("Green Output Black")) {
        preset_ = Preset::Custom;
        settings_.green.outputBlack = value.toDouble();
        syncImpls();
    } else if (key == QStringLiteral("Green Output White")) {
        preset_ = Preset::Custom;
        settings_.green.outputWhite = value.toDouble();
        syncImpls();
    } else if (key == QStringLiteral("Blue Input Black")) {
        preset_ = Preset::Custom;
        settings_.blue.inputBlack = value.toDouble();
        syncImpls();
    } else if (key == QStringLiteral("Blue Input White")) {
        preset_ = Preset::Custom;
        settings_.blue.inputWhite = value.toDouble();
        syncImpls();
    } else if (key == QStringLiteral("Blue Input Gamma")) {
        preset_ = Preset::Custom;
        settings_.blue.inputGamma = value.toDouble();
        syncImpls();
    } else if (key == QStringLiteral("Blue Output Black")) {
        preset_ = Preset::Custom;
        settings_.blue.outputBlack = value.toDouble();
        syncImpls();
    } else if (key == QStringLiteral("Blue Output White")) {
        preset_ = Preset::Custom;
        settings_.blue.outputWhite = value.toDouble();
        syncImpls();
    }
}

} // namespace Artifact
