module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <QVariant>

module SurfaceMemoryEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageSurfaceView;
import Graphics.SurfaceColorContract;
import Property.Abstract;
import Utils.String.UniString;
import Memory.SharedPtr;

namespace Artifact {

namespace {

class SurfaceMemoryCPUImpl final : public ArtifactEffectImplBase {
public:
    float decay_ = 0.92f;
    float gain_ = 0.8f;

private:
    std::vector<float> memory_;
    int memoryWidth_ = 0;
    int memoryHeight_ = 0;
    std::int64_t lastFrame_ = -1;

public:
    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        const int width = dst.image().width();
        const int height = dst.image().height();
        if (width <= 0 || height <= 0) return;

        const std::int64_t frame = context_.compositionFrame;
        const bool discontinuous = lastFrame_ < 0 || frame != lastFrame_ + 1;
        if (discontinuous || width != memoryWidth_ || height != memoryHeight_) {
            memory_.assign(static_cast<std::size_t>(width) * height * 3, 0.0f);
            memoryWidth_ = width;
            memoryHeight_ = height;
        }
        lastFrame_ = frame;

        float* pixels = dst.image().rgba32fData();
        if (!pixels) return;
        const float decay = std::clamp(decay_, 0.0f, 1.0f);
        const float gain = std::clamp(gain_, 0.0f, 4.0f);

        ArtifactCore::withColorFloat4View(
            src.image().surfaceView(), [&](const auto& sourceView) {
            const bool written = ArtifactCore::withMutableColorFloat4View(
                dst.image().surfaceView(), pixels, [&](auto& view) {
                for (int y = 0; y < height; ++y) {
                    const auto sourceRow = sourceView.row(y);
                    auto targetRow = view.row(y);
                    for (int x = 0; x < width; ++x) {
                        const auto& source = sourceRow[x];
                        const std::size_t base =
                            (static_cast<std::size_t>(y) * width + x) * 3;
                        memory_[base] = decay * memory_[base] + source.r;
                        memory_[base + 1] = decay * memory_[base + 1] + source.g;
                        memory_[base + 2] = decay * memory_[base + 2] + source.b;
                        targetRow[x].r = source.r + gain * memory_[base];
                        targetRow[x].g = source.g + gain * memory_[base + 1];
                        targetRow[x].b = source.b + gain * memory_[base + 2];
                    }
                }
            });
            (void)written;
        });
    }
};

} // namespace

SurfaceMemoryEffect::SurfaceMemoryEffect() {
    setEffectID(UniString("effect.surface.memory"));
    setDisplayName(UniString("Surface Memory"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<SurfaceMemoryCPUImpl>());
    setComputeMode(ComputeMode::CPU);
}

SurfaceMemoryEffect::~SurfaceMemoryEffect() = default;

void SurfaceMemoryEffect::setDecay(float value) {
    decay_ = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.92f;
    syncImpls();
}

void SurfaceMemoryEffect::setGain(float value) {
    gain_ = std::isfinite(value) ? std::clamp(value, 0.0f, 4.0f) : 0.8f;
    syncImpls();
}

void SurfaceMemoryEffect::syncImpls() {
    if (auto cpu = ArtifactCore::dynamicPointerCast<SurfaceMemoryCPUImpl>(cpuImpl())) {
        cpu->decay_ = decay_;
        cpu->gain_ = gain_;
    }
}

std::vector<AbstractProperty> SurfaceMemoryEffect::getProperties() const {
    std::vector<AbstractProperty> props(2);
    props[0].setName("Decay");
    props[0].setType(ArtifactCore::PropertyType::Float);
    props[0].setValue(QVariant(static_cast<double>(decay_)));
    props[0].setDefaultValue(QVariant(0.92));
    props[0].setMinValue(QVariant(0.0));
    props[0].setMaxValue(QVariant(1.0));
    props[0].setHardRange(QVariant(0.0), QVariant(1.0));
    props[1].setName("Memory Gain");
    props[1].setType(ArtifactCore::PropertyType::Float);
    props[1].setValue(QVariant(static_cast<double>(gain_)));
    props[1].setDefaultValue(QVariant(0.8));
    props[1].setMinValue(QVariant(0.0));
    props[1].setMaxValue(QVariant(4.0));
    props[1].setHardRange(QVariant(0.0), QVariant(4.0));
    return props;
}

void SurfaceMemoryEffect::setPropertyValue(const UniString& name,
                                            const QVariant& value) {
    if (name == "Decay") {
        setDecay(value.toFloat());
    } else if (name == "Memory Gain") {
        setGain(value.toFloat());
    } else {
        ArtifactAbstractEffect::setPropertyValue(name, value);
    }
}

} // namespace Artifact
