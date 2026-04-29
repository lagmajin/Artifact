module;
#include <utility>

#include <algorithm>
#include <memory>
#include <vector>
#include <QVariant>
#include <opencv2/opencv.hpp>

module ColorWheelsEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ColorCollection.ColorGrading;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

class ColorWheelsEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ColorWheelsProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        for (int y = 0; y < height; ++y) {
            processor_.process(pixels + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u, width, 1);
        }
    }
};

class ColorWheelsEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ColorWheelsProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        for (int y = 0; y < height; ++y) {
            processor_.process(pixels + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u, width, 1);
        }
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
};

ColorWheelsEffect::ColorWheelsEffect() {
    setEffectID(UniString("effect.colorcorrection.colorwheels"));
    setDisplayName(UniString("Color Wheels"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ColorWheelsEffectCPUImpl>());
    setGPUImpl(std::make_shared<ColorWheelsEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    syncImpls();
}

ColorWheelsEffect::~ColorWheelsEffect() = default;

void ColorWheelsEffect::setLift(float r, float g, float b) {
    wheels_.liftR = std::clamp(r, -2.0f, 2.0f);
    wheels_.liftG = std::clamp(g, -2.0f, 2.0f);
    wheels_.liftB = std::clamp(b, -2.0f, 2.0f);
    syncImpls();
}

void ColorWheelsEffect::setGamma(float r, float g, float b) {
    wheels_.gammaR = std::clamp(r, 0.1f, 5.0f);
    wheels_.gammaG = std::clamp(g, 0.1f, 5.0f);
    wheels_.gammaB = std::clamp(b, 0.1f, 5.0f);
    syncImpls();
}

void ColorWheelsEffect::setGain(float r, float g, float b) {
    wheels_.gainR = std::clamp(r, 0.0f, 4.0f);
    wheels_.gainG = std::clamp(g, 0.0f, 4.0f);
    wheels_.gainB = std::clamp(b, 0.0f, 4.0f);
    syncImpls();
}

void ColorWheelsEffect::setOffset(float r, float g, float b) {
    wheels_.offsetR = std::clamp(r, -2.0f, 2.0f);
    wheels_.offsetG = std::clamp(g, -2.0f, 2.0f);
    wheels_.offsetB = std::clamp(b, -2.0f, 2.0f);
    syncImpls();
}

void ColorWheelsEffect::setLiftMaster(float v) {
    wheels_.liftMaster = std::clamp(v, -2.0f, 2.0f);
    syncImpls();
}

void ColorWheelsEffect::setGammaMaster(float v) {
    wheels_.gammaMaster = std::clamp(v, 0.1f, 5.0f);
    syncImpls();
}

void ColorWheelsEffect::setGainMaster(float v) {
    wheels_.gainMaster = std::clamp(v, 0.0f, 4.0f);
    syncImpls();
}

void ColorWheelsEffect::setOffsetMaster(float v) {
    wheels_.offsetMaster = std::clamp(v, -2.0f, 2.0f);
    syncImpls();
}

void ColorWheelsEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<ColorWheelsEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setWheelType(wheelType_);
        cpu->processor_.wheels() = wheels_;
    }
    if (auto* gpu = dynamic_cast<ColorWheelsEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setWheelType(wheelType_);
        gpu->processor_.wheels() = wheels_;
    }
}

std::vector<AbstractProperty> ColorWheelsEffect::getProperties() const {
    std::vector<AbstractProperty> props;

    auto addFloat = [&props](const char* name, float value) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        props.push_back(prop);
    };

    AbstractProperty modeProp;
    modeProp.setName("Wheel Type");
    modeProp.setType(PropertyType::Integer);
    modeProp.setValue(static_cast<int>(wheelType_));
    props.push_back(modeProp);

    addFloat("Lift Master", wheels_.liftMaster);
    addFloat("Lift R", wheels_.liftR);
    addFloat("Lift G", wheels_.liftG);
    addFloat("Lift B", wheels_.liftB);
    addFloat("Gamma Master", wheels_.gammaMaster);
    addFloat("Gamma R", wheels_.gammaR);
    addFloat("Gamma G", wheels_.gammaG);
    addFloat("Gamma B", wheels_.gammaB);
    addFloat("Gain Master", wheels_.gainMaster);
    addFloat("Gain R", wheels_.gainR);
    addFloat("Gain G", wheels_.gainG);
    addFloat("Gain B", wheels_.gainB);
    addFloat("Offset Master", wheels_.offsetMaster);
    addFloat("Offset R", wheels_.offsetR);
    addFloat("Offset G", wheels_.offsetG);
    addFloat("Offset B", wheels_.offsetB);

    return props;
}

void ColorWheelsEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Wheel Type")) {
        setWheelType(static_cast<ColorWheelType>(value.toInt()));
    } else if (key == QStringLiteral("Lift Master")) {
        setLiftMaster(value.toFloat());
    } else if (key == QStringLiteral("Lift R")) {
        setLift(value.toFloat(), wheels_.liftG, wheels_.liftB);
    } else if (key == QStringLiteral("Lift G")) {
        setLift(wheels_.liftR, value.toFloat(), wheels_.liftB);
    } else if (key == QStringLiteral("Lift B")) {
        setLift(wheels_.liftR, wheels_.liftG, value.toFloat());
    } else if (key == QStringLiteral("Gamma Master")) {
        setGammaMaster(value.toFloat());
    } else if (key == QStringLiteral("Gamma R")) {
        setGamma(value.toFloat(), wheels_.gammaG, wheels_.gammaB);
    } else if (key == QStringLiteral("Gamma G")) {
        setGamma(wheels_.gammaR, value.toFloat(), wheels_.gammaB);
    } else if (key == QStringLiteral("Gamma B")) {
        setGamma(wheels_.gammaR, wheels_.gammaG, value.toFloat());
    } else if (key == QStringLiteral("Gain Master")) {
        setGainMaster(value.toFloat());
    } else if (key == QStringLiteral("Gain R")) {
        setGain(value.toFloat(), wheels_.gainG, wheels_.gainB);
    } else if (key == QStringLiteral("Gain G")) {
        setGain(wheels_.gainR, value.toFloat(), wheels_.gainB);
    } else if (key == QStringLiteral("Gain B")) {
        setGain(wheels_.gainR, wheels_.gainG, value.toFloat());
    } else if (key == QStringLiteral("Offset Master")) {
        setOffsetMaster(value.toFloat());
    } else if (key == QStringLiteral("Offset R")) {
        setOffset(value.toFloat(), wheels_.offsetG, wheels_.offsetB);
    } else if (key == QStringLiteral("Offset G")) {
        setOffset(wheels_.offsetR, value.toFloat(), wheels_.offsetB);
    } else if (key == QStringLiteral("Offset B")) {
        setOffset(wheels_.offsetR, wheels_.offsetG, value.toFloat());
    }
}

} // namespace Artifact
