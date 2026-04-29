module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QVariant>

module ChannelMixerEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.ChannelMixer;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

class ChannelMixerEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ChannelMixerProcessor processor_;

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

class ChannelMixerEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ChannelMixerProcessor processor_;

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

ChannelMixerEffect::ChannelMixerEffect() {
    setEffectID(UniString("effect.colorcorrection.channelmixer"));
    setDisplayName(UniString("Channel Mixer"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ChannelMixerEffectCPUImpl>());
    setGPUImpl(std::make_shared<ChannelMixerEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

ChannelMixerEffect::~ChannelMixerEffect() = default;

void ChannelMixerEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Identity:
        settings_ = ArtifactCore::ChannelMixerSettings::identityMix();
        break;
    case Preset::Warm:
        settings_ = ArtifactCore::ChannelMixerSettings::warm();
        break;
    case Preset::Cool:
        settings_ = ArtifactCore::ChannelMixerSettings::cool();
        break;
    case Preset::CrossProcess:
        settings_ = ArtifactCore::ChannelMixerSettings::crossProcess();
        break;
    case Preset::Monochrome:
        settings_ = ArtifactCore::ChannelMixerSettings::monochromeMix();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void ChannelMixerEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 5);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void ChannelMixerEffect::setStrength(float value) {
    preset_ = Preset::Custom;
    settings_.strength = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void ChannelMixerEffect::setMonochrome(bool value) {
    preset_ = Preset::Custom;
    settings_.monochrome = value;
    syncImpls();
}

void ChannelMixerEffect::setPreserveLuma(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveLuma = value;
    syncImpls();
}

void ChannelMixerEffect::setMatrix(float rr, float rg, float rb,
                                   float gr, float gg, float gb,
                                   float br, float bg, float bb) {
    preset_ = Preset::Custom;
    settings_.matrix = {{{rr, rg, rb}, {gr, gg, gb}, {br, bg, bb}}};
    syncImpls();
}

void ChannelMixerEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<ChannelMixerEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<ChannelMixerEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> ChannelMixerEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(14);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    AbstractProperty strengthProp;
    strengthProp.setName("Strength");
    strengthProp.setType(PropertyType::Float);
    strengthProp.setValue(QVariant(static_cast<double>(settings_.strength)));
    strengthProp.setDisplayPriority(-25);
    props.push_back(strengthProp);

    auto addFloat = [&props](const char* name, float value, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };

    addFloat("Red From Red", settings_.matrix[0][0], -20);
    addFloat("Red From Green", settings_.matrix[0][1], -19);
    addFloat("Red From Blue", settings_.matrix[0][2], -18);
    addFloat("Green From Red", settings_.matrix[1][0], -10);
    addFloat("Green From Green", settings_.matrix[1][1], -9);
    addFloat("Green From Blue", settings_.matrix[1][2], -8);
    addFloat("Blue From Red", settings_.matrix[2][0], 0);
    addFloat("Blue From Green", settings_.matrix[2][1], 1);
    addFloat("Blue From Blue", settings_.matrix[2][2], 2);

    AbstractProperty monoProp;
    monoProp.setName("Monochrome");
    monoProp.setType(PropertyType::Boolean);
    monoProp.setValue(settings_.monochrome);
    monoProp.setDisplayPriority(10);
    props.push_back(monoProp);

    AbstractProperty lumaProp;
    lumaProp.setName("Preserve Luma");
    lumaProp.setType(PropertyType::Boolean);
    lumaProp.setValue(settings_.preserveLuma);
    lumaProp.setDisplayPriority(11);
    props.push_back(lumaProp);

    return props;
}

void ChannelMixerEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    } else if (key == QStringLiteral("Strength")) {
        setStrength(value.toFloat());
    } else if (key == QStringLiteral("Monochrome")) {
        setMonochrome(value.toBool());
    } else if (key == QStringLiteral("Preserve Luma")) {
        setPreserveLuma(value.toBool());
    } else if (key == QStringLiteral("Red From Red")) {
        setMatrix(value.toFloat(), settings_.matrix[0][1], settings_.matrix[0][2],
                  settings_.matrix[1][0], settings_.matrix[1][1], settings_.matrix[1][2],
                  settings_.matrix[2][0], settings_.matrix[2][1], settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Red From Green")) {
        setMatrix(settings_.matrix[0][0], value.toFloat(), settings_.matrix[0][2],
                  settings_.matrix[1][0], settings_.matrix[1][1], settings_.matrix[1][2],
                  settings_.matrix[2][0], settings_.matrix[2][1], settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Red From Blue")) {
        setMatrix(settings_.matrix[0][0], settings_.matrix[0][1], value.toFloat(),
                  settings_.matrix[1][0], settings_.matrix[1][1], settings_.matrix[1][2],
                  settings_.matrix[2][0], settings_.matrix[2][1], settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Green From Red")) {
        setMatrix(settings_.matrix[0][0], settings_.matrix[0][1], settings_.matrix[0][2],
                  value.toFloat(), settings_.matrix[1][1], settings_.matrix[1][2],
                  settings_.matrix[2][0], settings_.matrix[2][1], settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Green From Green")) {
        setMatrix(settings_.matrix[0][0], settings_.matrix[0][1], settings_.matrix[0][2],
                  settings_.matrix[1][0], value.toFloat(), settings_.matrix[1][2],
                  settings_.matrix[2][0], settings_.matrix[2][1], settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Green From Blue")) {
        setMatrix(settings_.matrix[0][0], settings_.matrix[0][1], settings_.matrix[0][2],
                  settings_.matrix[1][0], settings_.matrix[1][1], value.toFloat(),
                  settings_.matrix[2][0], settings_.matrix[2][1], settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Blue From Red")) {
        setMatrix(settings_.matrix[0][0], settings_.matrix[0][1], settings_.matrix[0][2],
                  settings_.matrix[1][0], settings_.matrix[1][1], settings_.matrix[1][2],
                  value.toFloat(), settings_.matrix[2][1], settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Blue From Green")) {
        setMatrix(settings_.matrix[0][0], settings_.matrix[0][1], settings_.matrix[0][2],
                  settings_.matrix[1][0], settings_.matrix[1][1], settings_.matrix[1][2],
                  settings_.matrix[2][0], value.toFloat(), settings_.matrix[2][2]);
    } else if (key == QStringLiteral("Blue From Blue")) {
        setMatrix(settings_.matrix[0][0], settings_.matrix[0][1], settings_.matrix[0][2],
                  settings_.matrix[1][0], settings_.matrix[1][1], settings_.matrix[1][2],
                  settings_.matrix[2][0], settings_.matrix[2][1], value.toFloat());
    }
}

} // namespace Artifact
