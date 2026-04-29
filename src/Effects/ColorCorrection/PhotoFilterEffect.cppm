module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QColor>
#include <QVariant>

module PhotoFilterEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.PhotoFilter;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

class PhotoFilterEffectCPUImpl : public ArtifactEffectImplBase {
public:
    PhotoFilterProcessor processor_;

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

class PhotoFilterEffectGPUImpl : public ArtifactEffectImplBase {
public:
    PhotoFilterProcessor processor_;

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
QColor toQColor(const PhotoFilterColor& color) {
    return QColor::fromRgbF(color.r, color.g, color.b);
}

PhotoFilterColor toColor(const QColor& color) {
    return {
        static_cast<float>(color.redF()),
        static_cast<float>(color.greenF()),
        static_cast<float>(color.blueF())
    };
}
}

PhotoFilterEffect::PhotoFilterEffect() {
    setEffectID(UniString("effect.colorcorrection.photofilter"));
    setDisplayName(UniString("Photo Filter"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<PhotoFilterEffectCPUImpl>());
    setGPUImpl(std::make_shared<PhotoFilterEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

PhotoFilterEffect::~PhotoFilterEffect() = default;

void PhotoFilterEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Warm:
        settings_ = PhotoFilterSettings::warm();
        break;
    case Preset::Cool:
        settings_ = PhotoFilterSettings::cool();
        break;
    case Preset::Sepia:
        settings_ = PhotoFilterSettings::sepia();
        break;
    case Preset::Cyan:
        settings_ = PhotoFilterSettings::cyan();
        break;
    case Preset::Rose:
        settings_ = PhotoFilterSettings::rose();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void PhotoFilterEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 5);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void PhotoFilterEffect::setColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.color = toColor(color);
    syncImpls();
}

void PhotoFilterEffect::setDensity(float value) {
    preset_ = Preset::Custom;
    settings_.density = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void PhotoFilterEffect::setBrightness(float value) {
    preset_ = Preset::Custom;
    settings_.brightness = std::clamp(value, -1.0f, 1.0f);
    syncImpls();
}

void PhotoFilterEffect::setContrast(float value) {
    preset_ = Preset::Custom;
    settings_.contrast = std::clamp(value, 0.0f, 2.0f);
    syncImpls();
}

void PhotoFilterEffect::setSaturationBoost(float value) {
    preset_ = Preset::Custom;
    settings_.saturationBoost = std::clamp(value, 0.0f, 2.5f);
    syncImpls();
}

void PhotoFilterEffect::setPreserveLuma(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveLuma = value;
    syncImpls();
}

void PhotoFilterEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<PhotoFilterEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<PhotoFilterEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> PhotoFilterEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(8);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    AbstractProperty colorProp;
    colorProp.setName("Filter Color");
    colorProp.setType(PropertyType::Color);
    colorProp.setColorValue(toQColor(settings_.color));
    colorProp.setValue(toQColor(settings_.color));
    colorProp.setDisplayPriority(-20);
    props.push_back(colorProp);

    auto addFloat = [&props](const char* name, float value, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };

    addFloat("Density", settings_.density, 0);
    addFloat("Brightness", settings_.brightness, 10);
    addFloat("Contrast", settings_.contrast, 20);
    addFloat("Saturation Boost", settings_.saturationBoost, 30);

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Luma");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveLuma);
    preserveProp.setDisplayPriority(40);
    props.push_back(preserveProp);

    return props;
}

void PhotoFilterEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    } else if (key == QStringLiteral("Filter Color")) {
        setColor(value.value<QColor>());
    } else if (key == QStringLiteral("Density")) {
        setDensity(value.toFloat());
    } else if (key == QStringLiteral("Brightness")) {
        setBrightness(value.toFloat());
    } else if (key == QStringLiteral("Contrast")) {
        setContrast(value.toFloat());
    } else if (key == QStringLiteral("Saturation Boost")) {
        setSaturationBoost(value.toFloat());
    } else if (key == QStringLiteral("Preserve Luma")) {
        setPreserveLuma(value.toBool());
    }
}

} // namespace Artifact
