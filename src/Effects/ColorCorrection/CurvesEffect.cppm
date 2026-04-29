module;
#include <utility>

#include <algorithm>
#include <memory>
#include <vector>
#include <QVariant>
#include <opencv2/opencv.hpp>

module CurvesEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ColorCollection.ColorGrading;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

class CurvesEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ColorCurves curves_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        for (int y = 0; y < height; ++y) {
            curves_.process(pixels + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u, width, 1);
        }
    }
};

class CurvesEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ColorCurves curves_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        for (int y = 0; y < height; ++y) {
            curves_.process(pixels + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u, width, 1);
        }
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
};

namespace {
std::vector<ArtifactCore::CurvePoint> makeSCurvePoints(float strength) {
    const float s = std::clamp(strength, 0.0f, 1.0f);
    const float lowY = 0.25f - 0.15f * s;
    const float highY = 0.75f + 0.15f * s;
    return {
        {0.0f, 0.0f},
        {0.25f, std::clamp(lowY, 0.0f, 1.0f)},
        {0.50f, 0.50f},
        {0.75f, std::clamp(highY, 0.0f, 1.0f)},
        {1.0f, 1.0f},
    };
}
}

CurvesEffect::CurvesEffect() {
    setEffectID(UniString("effect.colorcorrection.curves"));
    setDisplayName(UniString("Curves"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<CurvesEffectCPUImpl>());
    setGPUImpl(std::make_shared<CurvesEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    syncImpls();
}

CurvesEffect::~CurvesEffect() = default;

void CurvesEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 5);
    preset_ = static_cast<Preset>(clamped);
    syncImpls();
}

void CurvesEffect::setStrength(float strength) {
    strength_ = std::clamp(strength, 0.0f, 1.0f);
    syncImpls();
}

void CurvesEffect::setPosterizeLevels(int levels) {
    posterizeLevels_ = std::max(2, levels);
    syncImpls();
}

void CurvesEffect::applyPreset(ArtifactCore::ColorCurves& curves) const {
    curves.reset();
    switch (preset_) {
    case Preset::Identity:
        break;
    case Preset::SCurve:
        curves.setMasterCurve(makeSCurvePoints(strength_));
        break;
    case Preset::FadeIn:
        curves.applyFadeIn();
        break;
    case Preset::FadeOut:
        curves.applyFadeOut();
        break;
    case Preset::Invert:
        curves.applyInvert();
        break;
    case Preset::Posterize:
        curves.applyPosterize(std::max(2, posterizeLevels_));
        break;
    }
}

void CurvesEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<CurvesEffectCPUImpl*>(cpuImpl().get())) {
        applyPreset(cpu->curves_);
    }
    if (auto* gpu = dynamic_cast<CurvesEffectGPUImpl*>(gpuImpl().get())) {
        applyPreset(gpu->curves_);
    }
}

std::vector<AbstractProperty> CurvesEffect::getProperties() const {
    std::vector<AbstractProperty> props;

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    props.push_back(presetProp);

    AbstractProperty strengthProp;
    strengthProp.setName("Strength");
    strengthProp.setType(PropertyType::Float);
    strengthProp.setValue(QVariant(static_cast<double>(strength_)));
    props.push_back(strengthProp);

    AbstractProperty posterizeProp;
    posterizeProp.setName("Posterize Levels");
    posterizeProp.setType(PropertyType::Integer);
    posterizeProp.setValue(posterizeLevels_);
    props.push_back(posterizeProp);

    return props;
}

void CurvesEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    } else if (key == QStringLiteral("Strength")) {
        setStrength(value.toFloat());
    } else if (key == QStringLiteral("Posterize Levels")) {
        setPosterizeLevels(value.toInt());
    }
}

} // namespace Artifact
