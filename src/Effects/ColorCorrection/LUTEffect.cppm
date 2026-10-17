module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

module LUTEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageSurfaceView;
import Property.Abstract;
import Utils.String.UniString;
import Color.LUT;
import Core.Parallel;
import Memory.SharedPtr;

namespace Artifact {

namespace {
inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}
} // namespace

class LUTEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ColorLUT lut_;
    float intensity_ = 1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        if (!lut_.isValid()) {
            return;
        }

        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        const float intensity = intensity_;
        const auto& lut = lut_;

        ArtifactCore::withMutableColorFloat4View(dst.image().surfaceView(), pixels, [&](auto& view) {
            ArtifactCore::Parallel::ForPixels(0, height, width, height, [&](int y) {
            auto row = view.row(y);
            for (int x = 0; x < width; ++x) {
                auto pixel = row[x];
                const float origR = pixel.r;
                const float origG = pixel.g;
                const float origB = pixel.b;

                float r = origR;
                float g = origG;
                float b = origB;
                lut.apply(r, g, b);

                if (intensity >= 0.9999f) {
                    pixel.r = r;
                    pixel.g = g;
                    pixel.b = b;
                } else if (intensity > 0.0001f) {
                    pixel.r = lerp(origR, r, intensity);
                    pixel.g = lerp(origG, g, intensity);
                    pixel.b = lerp(origB, b, intensity);
                }
            }
            });
        });
    }
};

LUTEffect::LUTEffect() {
    setEffectID(UniString("effect.colorcorrection.lut"));
    setDisplayName(UniString("Apply LUT"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<LUTEffectCPUImpl>());
    setComputeMode(ComputeMode::CPU);
    updateLUT();
}

LUTEffect::~LUTEffect() = default;

void LUTEffect::updateLUT() {
    ColorLUT lut;
    switch (preset_) {
        case Preset::Cinematic:
            lut = BuiltinLUTs::cinematic();
            break;
        case Preset::Vintage:
            lut = BuiltinLUTs::vintage();
            break;
        case Preset::Cold:
            lut = BuiltinLUTs::cold();
            break;
        case Preset::Warm:
            lut = BuiltinLUTs::warm();
            break;
        case Preset::HighContrast:
            lut = BuiltinLUTs::highContrast();
            break;
        case Preset::LowContrast:
            lut = BuiltinLUTs::lowContrast();
            break;
        case Preset::Desaturated:
            lut = BuiltinLUTs::desaturated();
            break;
        case Preset::Kodak2383:
            lut = BuiltinLUTs::kodak2383();
            break;
        case Preset::Fuji3510:
            lut = BuiltinLUTs::fuji3510();
            break;
        case Preset::Custom:
            if (!filePath_.isEmpty()) {
                lut.load(filePath_);
            }
            break;
    }

    if (auto cpu = ArtifactCore::dynamicPointerCast<LUTEffectCPUImpl>(cpuImpl())) {
        cpu->lut_ = std::move(lut);
        cpu->intensity_ = intensity_;
    }
}

void LUTEffect::setPreset(int preset) {
    preset_ = static_cast<Preset>(std::clamp(preset, 0, 9));
    updateLUT();
}

void LUTEffect::setFilePath(const QString& path) {
    filePath_ = path;
    if (preset_ == Preset::Custom) {
        updateLUT();
    }
}

void LUTEffect::syncImpls() {
    if (auto cpu = ArtifactCore::dynamicPointerCast<LUTEffectCPUImpl>(cpuImpl())) {
        cpu->intensity_ = intensity_;
    }
}

std::vector<AbstractProperty> LUTEffect::getProperties() const {
    std::vector<AbstractProperty> props(3);

    props[0].setName("Preset");
    props[0].setType(ArtifactCore::PropertyType::Integer);
    props[0].setValue(QVariant(static_cast<int>(preset_)));

    props[1].setName("LUT File");
    props[1].setType(ArtifactCore::PropertyType::String);
    props[1].setValue(QVariant(filePath_));

    props[2].setName("Intensity");
    props[2].setType(ArtifactCore::PropertyType::Float);
    props[2].setValue(QVariant(static_cast<double>(intensity_)));

    return props;
}

void LUTEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    if (name == "Preset") setPreset(value.toInt());
    else if (name == "LUT File") setFilePath(value.toString());
    else if (name == "Intensity") setIntensity(value.toFloat());
}

} // namespace Artifact
