module;
#include <utility>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <QString>
#include <QVariant>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Creative;

import Artifact.Effect.Abstract;
import Image.ImageF32x4RGBAWithCache;
import Image.MultiChannelImage;
import Graphics.Effect.Creative.Factory;
import Image.GpuImageUpload;
import Math.Noise;
import Math.Random;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

ArtifactCoreCreativeEffect::ArtifactCoreCreativeEffect(const char* coreName,
                                                       const char* effectId,
                                                       const char* displayName,
                                                       EffectPipelineStage stage)
    : coreEffect_(ArtifactCore::CreativeEffectFactory::create(coreName)) {
    setEffectID(ArtifactCore::UniString::fromQString(QString::fromUtf8(effectId)));
    setDisplayName(ArtifactCore::UniString::fromQString(QString::fromUtf8(displayName)));
    setPipelineStage(stage);
    if (coreEffect_) {
        coreEffect_->initialize();
    }
}

void ArtifactCoreCreativeEffect::apply(const ImageF32x4RGBAWithCache& src,
                                       ImageF32x4RGBAWithCache& dst) {
    if (!coreEffect_ || src.width() <= 0 || src.height() <= 0) {
        dst = src;
        return;
    }

    const auto& source = src.image();
    auto result = source.DeepCopy();
    ArtifactCore::MultiChannelImage channels(source.width(), source.height());
    const float* sourcePixels = source.rgba32fData();
    const std::size_t count = source.totalPixels();
    auto red = channels.getChannel(ArtifactCore::ChannelType::Red);
    auto green = channels.getChannel(ArtifactCore::ChannelType::Green);
    auto blue = channels.getChannel(ArtifactCore::ChannelType::Blue);
    auto alpha = channels.getChannel(ArtifactCore::ChannelType::Alpha);
    if (!sourcePixels || !red || !green || !blue || !alpha) {
        dst = src;
        return;
    }
    for (std::size_t i = 0; i < count; ++i) {
        red->data()[i] = sourcePixels[i * 4 + 0];
        green->data()[i] = sourcePixels[i * 4 + 1];
        blue->data()[i] = sourcePixels[i * 4 + 2];
        alpha->data()[i] = sourcePixels[i * 4 + 3];
    }

    context_.renderWidth = source.width();
    context_.renderHeight = source.height();
    auto frame = channels.toVideoFrame();
    coreEffect_->process(frame, context_);
    channels.copyFrom(frame);
    red = channels.getChannel(ArtifactCore::ChannelType::Red);
    green = channels.getChannel(ArtifactCore::ChannelType::Green);
    blue = channels.getChannel(ArtifactCore::ChannelType::Blue);
    alpha = channels.getChannel(ArtifactCore::ChannelType::Alpha);
    if (!red || !green || !blue || !alpha) {
        dst = src;
        return;
    }
    float* pixels = result.rgba32fData();
    for (std::size_t i = 0; i < count; ++i) {
        pixels[i * 4 + 0] = red->data()[i];
        pixels[i * 4 + 1] = green->data()[i];
        pixels[i * 4 + 2] = blue->data()[i];
        pixels[i * 4 + 3] = alpha->data()[i];
    }
    dst = ImageF32x4RGBAWithCache(result);
    if (!hasHostContext_) {
        ++context_.frameIndex;
        context_.time = context_.frameIndex /
                        (context_.frameRate > 0.0f ? context_.frameRate : 30.0f);
    }
}

void ArtifactCoreCreativeEffect::onContextUpdated(const EffectContext& context) {
    context_.time = context.timeSeconds;
    context_.frameIndex = static_cast<int>(context.layerFrame);
    context_.frameRate = static_cast<float>(context.frameRate > 0.0 ? context.frameRate : 30.0);
    hasHostContext_ = true;
}

std::vector<ArtifactCore::AbstractProperty> ArtifactCoreCreativeEffect::getProperties() const {
    auto properties = ArtifactAbstractEffect::getProperties();
    if (!coreEffect_) return properties;
    auto& parameters = const_cast<ArtifactCore::CreativeEffect*>(coreEffect_.get())->getParameters();
    for (const auto& parameter : parameters) {
        if (parameter.type != ArtifactCore::EffectParameterType::Float) continue;
        ArtifactCore::AbstractProperty property;
        property.setName(QString::fromStdString(parameter.name));
        property.setType(ArtifactCore::PropertyType::Float);
        property.setDisplayLabel(QString::fromStdString(parameter.displayName));
        property.setValue(QVariant(std::get<float>(parameter.value)));
        property.setDefaultValue(QVariant(std::get<float>(parameter.value)));
        property.setHardRange(QVariant(parameter.minValue), QVariant(parameter.maxValue));
        property.setAnimatable(true);
        properties.push_back(property);
    }
    return properties;
}

void ArtifactCoreCreativeEffect::setPropertyValue(const ArtifactCore::UniString& name,
                                                  const QVariant& value) {
    ArtifactAbstractEffect::setPropertyValue(name, value);
    if (coreEffect_ && value.canConvert<float>()) {
        coreEffect_->setParameter(name.toQString().toStdString(), value.toFloat());
    }
}

class ArtifactFilmGrungeEffect::Impl {
public:
    float grainAmount = 0.075f;
    float grainSize = 1.4f;
    float scratchAmount = 0.22f;
    float dustAmount = 0.18f;
    float flicker = 0.055f;
    float gateWeave = 1.2f;
    float vignette = 0.18f;
    float stain = 0.10f;
    int seed = 1977;
    std::int64_t frame = 0;
};

ArtifactFilmGrungeEffect::ArtifactFilmGrungeEffect() : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.film_grunge"));
    setDisplayName(ArtifactCore::UniString("Film Grunge"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setAllowOverscan(true);
}

ArtifactFilmGrungeEffect::~ArtifactFilmGrungeEffect() {
    delete impl_;
    impl_ = nullptr;
}

void ArtifactFilmGrungeEffect::onContextUpdated(const EffectContext& context) {
    impl_->frame = context.compositionFrame;
}

void ArtifactFilmGrungeEffect::apply(const ImageF32x4RGBAWithCache& src,
                                     ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) {
        dst = src;
        return;
    }

    const std::uint32_t frameSeed = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(impl_->seed) * 1664525ull +
        static_cast<std::uint64_t>(impl_->frame) * 1013904223ull);
    std::mt19937 random(frameSeed);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::normal_distribution<float> normal(0.0f, 1.0f);
    const float weaveX = normal(random) * impl_->gateWeave;
    const float weaveY = normal(random) * impl_->gateWeave * 0.35f;
    const float exposure = 1.0f + normal(random) * impl_->flicker;

    cv::Mat sourceMat(height, width, CV_32FC4, const_cast<float*>(source));
    cv::Mat shifted;
    const cv::Mat transform = (cv::Mat_<double>(2, 3) <<
        1.0, 0.0, weaveX, 0.0, 1.0, weaveY);
    cv::warpAffine(sourceMat, shifted, transform, cv::Size(width, height),
                   cv::INTER_LINEAR, cv::BORDER_REPLICATE);

    auto result = image.DeepCopy();
    float* destination = result.rgba32fData();
    const int grainBlock = std::max(1, static_cast<int>(std::round(impl_->grainSize)));
    const int scratchCount = static_cast<int>(impl_->scratchAmount *
                                               std::max(1, width / 18));
    std::vector<int> scratchColumns;
    scratchColumns.reserve(static_cast<std::size_t>(scratchCount));
    for (int i = 0; i < scratchCount; ++i) {
        scratchColumns.push_back(static_cast<int>(unit(random) * width));
    }
    std::vector<float> damageField(
        static_cast<std::size_t>(width) * height, 0.0f);
    for (const int column : scratchColumns) {
        for (int y = 0; y < height; ++y) {
            const float modulation = 0.45f +
                0.55f * std::sin(y * 0.071f + column);
            for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                const int x = column + offsetX;
                if (x < 0 || x >= width) continue;
                damageField[static_cast<std::size_t>(y) * width + x] +=
                    (offsetX == 0 ? 0.28f : 0.10f) * modulation;
            }
        }
    }
    const int dustCount = static_cast<int>(impl_->dustAmount *
                                           std::max(1, width * height / 900));
    struct Dust { float x; float y; float radius; float opacity; };
    std::vector<Dust> dust;
    dust.reserve(static_cast<std::size_t>(dustCount));
    for (int i = 0; i < dustCount; ++i) {
        dust.push_back({unit(random) * width, unit(random) * height,
                        0.6f + unit(random) * 3.8f,
                        0.10f + unit(random) * 0.32f});
    }
    for (const Dust& particle : dust) {
        const int minX = std::max(0, static_cast<int>(std::floor(particle.x - particle.radius)));
        const int maxX = std::min(width - 1, static_cast<int>(std::ceil(particle.x + particle.radius)));
        const int minY = std::max(0, static_cast<int>(std::floor(particle.y - particle.radius)));
        const int maxY = std::min(height - 1, static_cast<int>(std::ceil(particle.y + particle.radius)));
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const float dx = x - particle.x;
                const float dy = y - particle.y;
                const float distance2 = dx * dx + dy * dy;
                if (distance2 >= particle.radius * particle.radius) continue;
                damageField[static_cast<std::size_t>(y) * width + x] -=
                    particle.opacity *
                    (1.0f - std::sqrt(distance2) / particle.radius);
            }
        }
    }

    for (int y = 0; y < height; ++y) {
        const cv::Vec4f* shiftedRow = shifted.ptr<cv::Vec4f>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            const std::uint32_t blockHash = frameSeed ^
                static_cast<std::uint32_t>((x / grainBlock) * 73856093) ^
                static_cast<std::uint32_t>((y / grainBlock) * 19349663);
            const float grain = (static_cast<float>(blockHash & 0xffffu) /
                                 32767.5f - 1.0f) * impl_->grainAmount;
            const float nx = (2.0f * x / std::max(1, width - 1)) - 1.0f;
            const float ny = (2.0f * y / std::max(1, height - 1)) - 1.0f;
            const float vignetteFactor = std::clamp(
                1.0f - impl_->vignette * std::pow(nx * nx + ny * ny, 1.35f),
                0.0f, 1.0f);
            const float stainWave = 1.0f + impl_->stain *
                std::sin(nx * 4.7f + ny * 2.9f + static_cast<float>(impl_->seed));
            const float damage = damageField[
                static_cast<std::size_t>(y) * width + x];
            for (int channel = 0; channel < 3; ++channel) {
                const float chromaGrain = grain *
                    (channel == 0 ? 1.05f : channel == 2 ? 0.92f : 1.0f);
                destination[offset + channel] =
                    shiftedRow[x][channel] * exposure * vignetteFactor * stainWave +
                    chromaGrain + damage;
            }
            destination[offset + 3] = shiftedRow[x][3];
        }
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty> ArtifactFilmGrungeEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    properties.reserve(9);
    auto addFloat = [&](const char* name, const char* label, float value,
                        float minimum, float maximum) {
        auto& property = properties.emplace_back();
        property.setName(QString::fromUtf8(name));
        property.setDisplayLabel(QString::fromUtf8(label));
        property.setType(ArtifactCore::PropertyType::Float);
        property.setValue(value);
        property.setDefaultValue(value);
        property.setHardRange(minimum, maximum);
        property.setAnimatable(true);
    };
    addFloat("grainAmount", "Grain Amount", impl_->grainAmount, 0.0f, 0.5f);
    addFloat("grainSize", "Grain Size", impl_->grainSize, 1.0f, 8.0f);
    addFloat("scratchAmount", "Scratches", impl_->scratchAmount, 0.0f, 1.0f);
    addFloat("dustAmount", "Dust and Dirt", impl_->dustAmount, 0.0f, 1.0f);
    addFloat("flicker", "Exposure Flicker", impl_->flicker, 0.0f, 0.5f);
    addFloat("gateWeave", "Gate Weave", impl_->gateWeave, 0.0f, 12.0f);
    addFloat("vignette", "Vignette", impl_->vignette, 0.0f, 1.0f);
    addFloat("stain", "Emulsion Stain", impl_->stain, 0.0f, 0.5f);
    auto& seedProperty = properties.emplace_back();
    seedProperty.setName(QStringLiteral("seed"));
    seedProperty.setDisplayLabel(QStringLiteral("Random Seed"));
    seedProperty.setType(ArtifactCore::PropertyType::Integer);
    seedProperty.setValue(impl_->seed);
    seedProperty.setDefaultValue(1977);
    seedProperty.setHardRange(0, 100000);
    return properties;
}

void ArtifactFilmGrungeEffect::setPropertyValue(const ArtifactCore::UniString& name,
                                                const QVariant& value) {
    const QString key = name.toQString();
    const float raw = static_cast<float>(value.toDouble());
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("grainAmount")) impl_->grainAmount = std::clamp(number, 0.0f, 0.5f);
    else if (key == QStringLiteral("grainSize")) impl_->grainSize = std::clamp(number, 1.0f, 8.0f);
    else if (key == QStringLiteral("scratchAmount")) impl_->scratchAmount = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("dustAmount")) impl_->dustAmount = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("flicker")) impl_->flicker = std::clamp(number, 0.0f, 0.5f);
    else if (key == QStringLiteral("gateWeave")) impl_->gateWeave = std::clamp(number, 0.0f, 12.0f);
    else if (key == QStringLiteral("vignette")) impl_->vignette = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("stain")) impl_->stain = std::clamp(number, 0.0f, 0.5f);
    else if (key == QStringLiteral("seed")) impl_->seed = std::clamp(value.toInt(), 0, 100000);
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint ArtifactFilmGrungeEffect::roiHint() const {
    return EffectROIHint{
        .kind = EffectROIHintKind::Displacement,
        .expansionPixels = std::ceil(impl_->gateWeave) + 1.0f,
        .requiresFullFrame = true
    };
}

class ArtifactHeatwaveEffect::Impl {
public:
    float amount = 12.0f;
    float scale = 54.0f;
    float speed = 0.65f;
    float turbulence = 0.55f;
    float dispersion = 1.5f;
    float verticalFlow = 0.75f;
    float mix = 1.0f;
    double time = 0.0;
};

namespace {

float sampleHeatwaveChannel(const float* pixels, int width, int height,
                            float x, float y, int channel) {
    x = std::clamp(x, 0.0f, static_cast<float>(width - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(height - 1));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, width - 1);
    const int y1 = std::min(y0 + 1, height - 1);
    const float tx = x - x0;
    const float ty = y - y0;
    const auto valueAt = [&](int px, int py) {
        return pixels[(static_cast<std::size_t>(py) * width + px) * 4u + channel];
    };
    const float top = std::lerp(valueAt(x0, y0), valueAt(x1, y0), tx);
    const float bottom = std::lerp(valueAt(x0, y1), valueAt(x1, y1), tx);
    return std::lerp(top, bottom, ty);
}

} // namespace

ArtifactHeatwaveEffect::ArtifactHeatwaveEffect() : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.heatwave"));
    setDisplayName(ArtifactCore::UniString("Heatwave"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setAllowOverscan(true);
}

ArtifactHeatwaveEffect::~ArtifactHeatwaveEffect() {
    delete impl_;
    impl_ = nullptr;
}

void ArtifactHeatwaveEffect::onContextUpdated(const EffectContext& context) {
    impl_->time = context.timeSeconds;
}

void ArtifactHeatwaveEffect::apply(const ImageF32x4RGBAWithCache& src,
                                   ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) {
        dst = src;
        return;
    }
    auto result = image.DeepCopy();
    float* destination = result.rgba32fData();
    const float scale = std::max(4.0f, impl_->scale);
    const float phase = static_cast<float>(impl_->time * impl_->speed);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float px = x / scale;
            const float py = y / scale;
            const float broad = std::sin(px * 1.73f + phase * 2.1f) *
                                std::cos(py * 1.21f - phase * 1.4f);
            const float detail = std::sin(px * 4.91f - py * 3.17f + phase * 3.7f) *
                                 impl_->turbulence;
            const float rising = std::sin(py * 2.4f - phase * 5.0f) *
                                 impl_->verticalFlow;
            const float dx = (broad + detail) * impl_->amount;
            const float dy = (detail * 0.45f + rising) * impl_->amount * 0.45f;
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            const float channelOffset[3] = {
                impl_->dispersion, 0.0f, -impl_->dispersion};
            for (int channel = 0; channel < 3; ++channel) {
                const float warped = sampleHeatwaveChannel(
                    source, width, height, x + dx + channelOffset[channel],
                    y + dy, channel);
                destination[offset + channel] = std::lerp(
                    source[offset + channel], warped, impl_->mix);
            }
            const float warpedAlpha = sampleHeatwaveChannel(
                source, width, height, x + dx, y + dy, 3);
            destination[offset + 3] = std::lerp(
                source[offset + 3], warpedAlpha, impl_->mix);
        }
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty> ArtifactHeatwaveEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    properties.reserve(7);
    auto addFloat = [&](const char* name, const char* label, float value,
                        float minimum, float maximum) {
        auto& property = properties.emplace_back();
        property.setName(QString::fromUtf8(name));
        property.setDisplayLabel(QString::fromUtf8(label));
        property.setType(ArtifactCore::PropertyType::Float);
        property.setValue(value);
        property.setDefaultValue(value);
        property.setHardRange(minimum, maximum);
        property.setAnimatable(true);
    };
    addFloat("amount", "Refraction Amount", impl_->amount, 0.0f, 100.0f);
    addFloat("scale", "Heat Scale", impl_->scale, 4.0f, 512.0f);
    addFloat("speed", "Flow Speed", impl_->speed, -5.0f, 5.0f);
    addFloat("turbulence", "Turbulence", impl_->turbulence, 0.0f, 2.0f);
    addFloat("dispersion", "Prism Dispersion", impl_->dispersion, 0.0f, 20.0f);
    addFloat("verticalFlow", "Vertical Flow", impl_->verticalFlow, -2.0f, 2.0f);
    addFloat("mix", "Mix", impl_->mix, 0.0f, 1.0f);
    return properties;
}

void ArtifactHeatwaveEffect::setPropertyValue(const ArtifactCore::UniString& name,
                                              const QVariant& value) {
    const QString key = name.toQString();
    const float raw = static_cast<float>(value.toDouble());
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("amount")) impl_->amount = std::clamp(number, 0.0f, 100.0f);
    else if (key == QStringLiteral("scale")) impl_->scale = std::clamp(number, 4.0f, 512.0f);
    else if (key == QStringLiteral("speed")) impl_->speed = std::clamp(number, -5.0f, 5.0f);
    else if (key == QStringLiteral("turbulence")) impl_->turbulence = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("dispersion")) impl_->dispersion = std::clamp(number, 0.0f, 20.0f);
    else if (key == QStringLiteral("verticalFlow")) impl_->verticalFlow = std::clamp(number, -2.0f, 2.0f);
    else if (key == QStringLiteral("mix")) impl_->mix = std::clamp(number, 0.0f, 1.0f);
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint ArtifactHeatwaveEffect::roiHint() const {
    return EffectROIHint{
        .kind = EffectROIHintKind::Displacement,
        .expansionPixels = impl_->amount + impl_->dispersion + 2.0f,
        .requiresFullFrame = false
    };
}

class ArtifactCinematicLensFlareEffect::Impl {
public:
    bool autoPosition = true;
    float positionX = 0.5f;
    float positionY = 0.5f;
    float threshold = 0.85f;
    float intensity = 1.3f;
    float haloSize = 0.12f;
    float streakLength = 0.65f;
    float streakStrength = 0.75f;
    int ghostCount = 6;
    float ghostSpacing = 0.72f;
    float chromatic = 0.32f;
};

ArtifactCinematicLensFlareEffect::ArtifactCinematicLensFlareEffect()
    : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.cinematic_lens_flare"));
    setDisplayName(ArtifactCore::UniString("Cinematic Lens Flare"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setAllowOverscan(true);
}

ArtifactCinematicLensFlareEffect::~ArtifactCinematicLensFlareEffect() {
    delete impl_;
    impl_ = nullptr;
}

void ArtifactCinematicLensFlareEffect::apply(
    const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) {
        dst = src;
        return;
    }

    float flareX = impl_->positionX * std::max(1, width - 1);
    float flareY = impl_->positionY * std::max(1, height - 1);
    float sourceEnergy = 1.0f;
    if (impl_->autoPosition) {
        double weightedX = 0.0;
        double weightedY = 0.0;
        double weightSum = 0.0;
        float peak = impl_->threshold;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t offset =
                    (static_cast<std::size_t>(y) * width + x) * 4u;
                const float luminance = source[offset] * 0.2126f +
                    source[offset + 1] * 0.7152f + source[offset + 2] * 0.0722f;
                const float weight = std::max(0.0f, luminance - impl_->threshold);
                weightedX += static_cast<double>(x) * weight;
                weightedY += static_cast<double>(y) * weight;
                weightSum += weight;
                peak = std::max(peak, luminance);
            }
        }
        if (weightSum <= 0.000001) {
            dst = src;
            return;
        }
        flareX = static_cast<float>(weightedX / weightSum);
        flareY = static_cast<float>(weightedY / weightSum);
        sourceEnergy = std::max(0.0f, peak - impl_->threshold);
    }

    auto result = image.DeepCopy();
    float* destination = result.rgba32fData();
    const float minimumDimension = static_cast<float>(std::min(width, height));
    const float haloRadius = std::max(2.0f, impl_->haloSize * minimumDimension);
    const float centerX = width * 0.5f;
    const float centerY = height * 0.5f;
    const float axisX = centerX - flareX;
    const float axisY = centerY - flareY;
    const float overall = impl_->intensity * sourceEnergy;

    ArtifactCore::Parallel::For(0, height, width * height, [&](int y) {
        for (int x = 0; x < width; ++x) {
            const float dx = x - flareX;
            const float dy = y - flareY;
            const float radius2 = dx * dx + dy * dy;
            const float core = std::exp(-radius2 /
                std::max(1.0f, haloRadius * haloRadius * 0.055f));
            const float haloDistance = std::sqrt(radius2) / haloRadius;
            const float halo = std::exp(-std::pow(haloDistance - 0.72f, 2.0f) * 7.0f);
            const float streakWidth = std::max(1.0f, haloRadius * 0.035f);
            const float streak = std::exp(-std::abs(dy) / streakWidth) *
                std::exp(-std::abs(dx) /
                         std::max(1.0f, width * impl_->streakLength));
            const float angle = std::atan2(dy, dx);
            const float rayPattern = std::pow(
                std::max(0.0f, std::cos(angle * 6.0f)), 18.0f);
            const float rays = rayPattern *
                std::exp(-std::sqrt(radius2) / std::max(1.0f, haloRadius * 2.8f));

            float ghostR = 0.0f;
            float ghostG = 0.0f;
            float ghostB = 0.0f;
            for (int ghostIndex = 1; ghostIndex <= impl_->ghostCount; ++ghostIndex) {
                const float t = (static_cast<float>(ghostIndex) /
                                 (impl_->ghostCount + 1.0f)) * impl_->ghostSpacing * 1.8f;
                const float gx = flareX + axisX * t;
                const float gy = flareY + axisY * t;
                const float ghostRadius = haloRadius *
                    (0.10f + 0.045f * static_cast<float>(ghostIndex));
                const float gdx = x - gx;
                const float gdy = y - gy;
                const float normalized = std::sqrt(gdx * gdx + gdy * gdy) /
                                         std::max(1.0f, ghostRadius);
                const float ring = std::exp(-std::pow(normalized - 0.72f, 2.0f) * 18.0f);
                const float disk = std::exp(-normalized * normalized * 3.5f);
                const float ghost = (ring * 0.55f + disk * 0.22f) /
                                    (1.0f + ghostIndex * 0.24f);
                const float spectral = impl_->chromatic *
                    (static_cast<float>(ghostIndex) / std::max(1, impl_->ghostCount));
                ghostR += ghost * (0.65f + spectral);
                ghostG += ghost * 0.72f;
                ghostB += ghost * (0.88f - spectral * 0.35f);
            }

            const float baseGlow = core * 1.45f + halo * 0.22f +
                                   streak * impl_->streakStrength + rays * 0.35f;
            const float flareColor[3] = {
                baseGlow * 1.05f + ghostR,
                baseGlow * 0.88f + ghostG,
                baseGlow * (0.68f + impl_->chromatic * 0.22f) + ghostB};
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            for (int channel = 0; channel < 3; ++channel) {
                destination[offset + channel] =
                    source[offset + channel] + flareColor[channel] * overall;
            }
            destination[offset + 3] = source[offset + 3];
        }
    });
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty>
ArtifactCinematicLensFlareEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    properties.reserve(11);
    auto& automatic = properties.emplace_back();
    automatic.setName(QStringLiteral("autoPosition"));
    automatic.setDisplayLabel(QStringLiteral("Auto Detect Highlight"));
    automatic.setType(ArtifactCore::PropertyType::Boolean);
    automatic.setValue(impl_->autoPosition);
    automatic.setDefaultValue(true);
    auto addFloat = [&](const char* name, const char* label, float value,
                        float minimum, float maximum) {
        auto& property = properties.emplace_back();
        property.setName(QString::fromUtf8(name));
        property.setDisplayLabel(QString::fromUtf8(label));
        property.setType(ArtifactCore::PropertyType::Float);
        property.setValue(value);
        property.setDefaultValue(value);
        property.setHardRange(minimum, maximum);
        property.setAnimatable(true);
    };
    addFloat("positionX", "Flare Position X", impl_->positionX, 0.0f, 1.0f);
    addFloat("positionY", "Flare Position Y", impl_->positionY, 0.0f, 1.0f);
    addFloat("threshold", "Highlight Threshold", impl_->threshold, 0.0f, 4.0f);
    addFloat("intensity", "Intensity", impl_->intensity, 0.0f, 8.0f);
    addFloat("haloSize", "Halo Size", impl_->haloSize, 0.01f, 1.0f);
    addFloat("streakLength", "Anamorphic Length", impl_->streakLength, 0.01f, 2.0f);
    addFloat("streakStrength", "Anamorphic Strength", impl_->streakStrength, 0.0f, 4.0f);
    auto& ghosts = properties.emplace_back();
    ghosts.setName(QStringLiteral("ghostCount"));
    ghosts.setDisplayLabel(QStringLiteral("Ghost Count"));
    ghosts.setType(ArtifactCore::PropertyType::Integer);
    ghosts.setValue(impl_->ghostCount);
    ghosts.setDefaultValue(6);
    ghosts.setHardRange(0, 16);
    addFloat("ghostSpacing", "Ghost Spacing", impl_->ghostSpacing, 0.0f, 2.0f);
    addFloat("chromatic", "Chromatic Dispersion", impl_->chromatic, 0.0f, 1.0f);
    return properties;
}

void ArtifactCinematicLensFlareEffect::setPropertyValue(
    const ArtifactCore::UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    const float raw = value.toFloat();
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("autoPosition")) impl_->autoPosition = value.toBool();
    else if (key == QStringLiteral("positionX")) impl_->positionX = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("positionY")) impl_->positionY = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("threshold")) impl_->threshold = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("intensity")) impl_->intensity = std::clamp(number, 0.0f, 8.0f);
    else if (key == QStringLiteral("haloSize")) impl_->haloSize = std::clamp(number, 0.01f, 1.0f);
    else if (key == QStringLiteral("streakLength")) impl_->streakLength = std::clamp(number, 0.01f, 2.0f);
    else if (key == QStringLiteral("streakStrength")) impl_->streakStrength = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("ghostCount")) impl_->ghostCount = std::clamp(value.toInt(), 0, 16);
    else if (key == QStringLiteral("ghostSpacing")) impl_->ghostSpacing = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("chromatic")) impl_->chromatic = std::clamp(number, 0.0f, 1.0f);
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint ArtifactCinematicLensFlareEffect::roiHint() const {
    return EffectROIHint{
        .kind = EffectROIHintKind::Glow,
        .expansionPixels = 0.0f,
        .requiresFullFrame = true
    };
}

namespace {

float creativeHash(int x, int y, int seed) {
    std::uint32_t value = static_cast<std::uint32_t>(x) * 0x8da6b343u ^
                          static_cast<std::uint32_t>(y) * 0xd8163841u ^
                          static_cast<std::uint32_t>(seed) * 0xcb1ab31fu;
    value ^= value >> 13; value *= 0x85ebca6bu; value ^= value >> 16;
    return static_cast<float>(value & 0xffffu) / 65535.0f;
}

void addCreativeFloat(std::vector<ArtifactCore::AbstractProperty>& properties,
                      const char* name, const char* label, float value,
                      float minimum, float maximum) {
    auto& property = properties.emplace_back();
    property.setName(QString::fromUtf8(name));
    property.setDisplayLabel(QString::fromUtf8(label));
    property.setType(ArtifactCore::PropertyType::Float);
    property.setValue(value); property.setDefaultValue(value);
    property.setHardRange(minimum, maximum); property.setAnimatable(true);
}

} // namespace

class ArtifactTexturizeMotionEffect::Impl {
public:
    float scale = 72.0f, speed = 0.45f, contrast = 0.65f, opacity = 0.32f;
    float displacement = 4.0f, colorSeparation = 0.7f, jitter = 0.15f;
    float warmth = 0.12f, mix = 1.0f;
    int seed = 3107;
    double time = 0.0;
};

ArtifactTexturizeMotionEffect::ArtifactTexturizeMotionEffect() : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.texturize_motion"));
    setDisplayName(ArtifactCore::UniString("Texturize Motion"));
    setPipelineStage(EffectPipelineStage::Rasterizer); setAllowOverscan(true);
}
ArtifactTexturizeMotionEffect::~ArtifactTexturizeMotionEffect() { delete impl_; impl_ = nullptr; }
void ArtifactTexturizeMotionEffect::onContextUpdated(const EffectContext& context) { impl_->time = context.timeSeconds; }

void ArtifactTexturizeMotionEffect::apply(const ImageF32x4RGBAWithCache& src,
                                          ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image(); const int width = image.width(), height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) { dst = src; return; }
    auto result = image.DeepCopy(); float* output = result.rgba32fData();
    const float phase = static_cast<float>(impl_->time * impl_->speed);
    const float scale = std::max(4.0f, impl_->scale);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const float u = x / scale, v = y / scale;
        const float coarse = std::sin(u * 2.31f + phase * 1.7f) *
                             std::cos(v * 1.83f - phase * 1.1f);
        const float fibers = std::sin((u + v) * 7.7f + phase * 3.2f) * 0.35f;
        const float cells = creativeHash(static_cast<int>(u * 4.0f),
                                         static_cast<int>(v * 4.0f), impl_->seed) - 0.5f;
        float texture = (coarse * 0.55f + fibers + cells * 0.6f) * impl_->contrast;
        texture = std::clamp(texture, -1.0f, 1.0f);
        const float dx = texture * impl_->displacement +
                         std::sin(v * 4.0f + phase) * impl_->jitter * 3.0f;
        const float dy = std::cos(u * 3.0f - phase) * impl_->displacement * 0.35f;
        const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4u;
        const float tint[3] = {1.0f + impl_->warmth * 0.18f, 1.0f,
                               1.0f - impl_->warmth * 0.28f};
        for (int channel = 0; channel < 3; ++channel) {
            const float separation = (channel - 1) * impl_->colorSeparation;
            const float warped = sampleHeatwaveChannel(source, width, height,
                x + dx + separation, y + dy, channel);
            const float textured = warped * (1.0f + texture * impl_->opacity) +
                                   std::max(0.0f, texture) * impl_->opacity * 0.08f * tint[channel];
            output[offset + channel] = std::lerp(source[offset + channel], textured, impl_->mix);
        }
        output[offset + 3] = source[offset + 3];
    }
    result.setColorDescriptor(image.colorDescriptor()); dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty> ArtifactTexturizeMotionEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> p;
    addCreativeFloat(p,"scale","Texture Scale",impl_->scale,4,512);
    addCreativeFloat(p,"speed","Texture Speed",impl_->speed,-5,5);
    addCreativeFloat(p,"contrast","Texture Contrast",impl_->contrast,0,3);
    addCreativeFloat(p,"opacity","Texture Opacity",impl_->opacity,0,1);
    addCreativeFloat(p,"displacement","Displacement",impl_->displacement,0,64);
    addCreativeFloat(p,"colorSeparation","Color Separation",impl_->colorSeparation,0,12);
    addCreativeFloat(p,"jitter","Jitter",impl_->jitter,0,2);
    addCreativeFloat(p,"warmth","Tint Warmth",impl_->warmth,-1,1);
    addCreativeFloat(p,"mix","Mix",impl_->mix,0,1);
    auto& seed=p.emplace_back(); seed.setName("seed"); seed.setDisplayLabel("Random Seed");
    seed.setType(ArtifactCore::PropertyType::Integer); seed.setValue(impl_->seed);
    seed.setDefaultValue(3107); seed.setHardRange(0,100000);
    return p;
}

void ArtifactTexturizeMotionEffect::setPropertyValue(const ArtifactCore::UniString& name,const QVariant& value){
    const QString k=name.toQString(); const float raw=value.toFloat();
    const float n=std::isfinite(raw)?raw:0.0f;
    if(k=="scale")impl_->scale=std::clamp(n,4.0f,512.0f); else if(k=="speed")impl_->speed=std::clamp(n,-5.0f,5.0f);
    else if(k=="contrast")impl_->contrast=std::clamp(n,0.0f,3.0f); else if(k=="opacity")impl_->opacity=std::clamp(n,0.0f,1.0f);
    else if(k=="displacement")impl_->displacement=std::clamp(n,0.0f,64.0f); else if(k=="colorSeparation")impl_->colorSeparation=std::clamp(n,0.0f,12.0f);
    else if(k=="jitter")impl_->jitter=std::clamp(n,0.0f,2.0f); else if(k=="warmth")impl_->warmth=std::clamp(n,-1.0f,1.0f);
    else if(k=="mix")impl_->mix=std::clamp(n,0.0f,1.0f); else if(k=="seed")impl_->seed=std::clamp(value.toInt(),0,100000);
    else ArtifactAbstractEffect::setPropertyValue(name,value);
}
EffectROIHint ArtifactTexturizeMotionEffect::roiHint() const { return {.kind=EffectROIHintKind::Displacement,.expansionPixels=impl_->displacement+impl_->colorSeparation+4}; }

class ArtifactDebandEffect::Impl { public: float strength=.72f,radius=5,edgeThreshold=.035f,grain=.006f,mix=1; int iterations=1; };
ArtifactDebandEffect::ArtifactDebandEffect():impl_(new Impl()){setEffectID(ArtifactCore::UniString("builtin.deband"));setDisplayName(ArtifactCore::UniString("Deband"));setPipelineStage(EffectPipelineStage::Rasterizer);}
ArtifactDebandEffect::~ArtifactDebandEffect(){delete impl_;impl_=nullptr;}
void ArtifactDebandEffect::apply(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst){
    const auto& image=src.image();const int w=image.width(),h=image.height();const float* data=image.rgba32fData();if(!data||w<=0||h<=0){dst=src;return;}
    cv::Mat rgba(h,w,CV_32FC4,const_cast<float*>(data));std::vector<cv::Mat> c;cv::split(rgba,c);cv::Mat rgb;cv::merge(std::vector<cv::Mat>{c[0],c[1],c[2]},rgb);cv::Mat smooth=rgb.clone();
    for(int i=0;i<impl_->iterations;++i){cv::Mat next;cv::bilateralFilter(smooth,next,std::max(3,static_cast<int>(impl_->radius)*2+1),impl_->edgeThreshold*4.0f,impl_->radius);smooth=next;}
    cv::Mat luma=c[0]*.2126f+c[1]*.7152f+c[2]*.0722f,gx,gy;cv::Sobel(luma,gx,CV_32F,1,0,3);cv::Sobel(luma,gy,CV_32F,0,1,3);
    auto result=image.DeepCopy();float* out=result.rgba32fData();for(int y=0;y<h;++y){const cv::Vec3f* sr=smooth.ptr<cv::Vec3f>(y);const float* xr=gx.ptr<float>(y);const float* yr=gy.ptr<float>(y);for(int x=0;x<w;++x){const float edge=std::sqrt(xr[x]*xr[x]+yr[x]*yr[x]);const float flat=1-std::clamp(edge/std::max(.0001f,impl_->edgeThreshold),0.0f,1.0f);const float amount=impl_->strength*flat*impl_->mix;const float noise=(creativeHash(x,y,731)-.5f)*2*impl_->grain;const size_t o=(static_cast<size_t>(y)*w+x)*4;for(int ch=0;ch<3;++ch)out[o+ch]=std::lerp(data[o+ch],sr[x][ch],amount)+noise;out[o+3]=data[o+3];}}
    result.setColorDescriptor(image.colorDescriptor());dst=ImageF32x4RGBAWithCache(result);
}
std::vector<ArtifactCore::AbstractProperty> ArtifactDebandEffect::getProperties()const{std::vector<ArtifactCore::AbstractProperty>p;addCreativeFloat(p,"strength","Strength",impl_->strength,0,1);addCreativeFloat(p,"radius","Radius",impl_->radius,1,16);addCreativeFloat(p,"edgeThreshold","Edge Threshold",impl_->edgeThreshold,.001f,.25f);addCreativeFloat(p,"grain","Dither Grain",impl_->grain,0,.05f);addCreativeFloat(p,"mix","Mix",impl_->mix,0,1);auto&i=p.emplace_back();i.setName("iterations");i.setDisplayLabel("Iterations");i.setType(ArtifactCore::PropertyType::Integer);i.setValue(impl_->iterations);i.setDefaultValue(1);i.setHardRange(1,3);return p;}
void ArtifactDebandEffect::setPropertyValue(const ArtifactCore::UniString&name,const QVariant&v){const QString k=name.toQString();const float raw=v.toFloat();const float n=std::isfinite(raw)?raw:0.0f;if(k=="strength")impl_->strength=std::clamp(n,0.0f,1.0f);else if(k=="radius")impl_->radius=std::clamp(n,1.0f,16.0f);else if(k=="edgeThreshold")impl_->edgeThreshold=std::clamp(n,.001f,.25f);else if(k=="grain")impl_->grain=std::clamp(n,0.0f,.05f);else if(k=="mix")impl_->mix=std::clamp(n,0.0f,1.0f);else if(k=="iterations")impl_->iterations=std::clamp(v.toInt(),1,3);else ArtifactAbstractEffect::setPropertyValue(name,v);}
EffectROIHint ArtifactDebandEffect::roiHint()const{return{.kind=EffectROIHintKind::Blur,.expansionPixels=impl_->radius*2};}

class ArtifactDeblockEffect::Impl { public:int blockSize=8;float strength=.65f,threshold=.08f,detailRecovery=.28f,mix=1;};
ArtifactDeblockEffect::ArtifactDeblockEffect():impl_(new Impl()){setEffectID(ArtifactCore::UniString("builtin.deblock"));setDisplayName(ArtifactCore::UniString("Deblock"));setPipelineStage(EffectPipelineStage::Rasterizer);}
ArtifactDeblockEffect::~ArtifactDeblockEffect(){delete impl_;impl_=nullptr;}
void ArtifactDeblockEffect::apply(const ImageF32x4RGBAWithCache&src,ImageF32x4RGBAWithCache&dst){const auto&im=src.image();const int w=im.width(),h=im.height();const float*d=im.rgba32fData();if(!d||w<=0||h<=0){dst=src;return;}auto result=im.DeepCopy();float*out=result.rgba32fData();auto smoothPair=[&](int a,int b){float la=out[a]*.2126f+out[a+1]*.7152f+out[a+2]*.0722f,lb=out[b]*.2126f+out[b+1]*.7152f+out[b+2]*.0722f;if(std::abs(la-lb)>impl_->threshold)return;for(int c=0;c<3;++c){const float mean=(out[a+c]+out[b+c])*.5f;out[a+c]=std::lerp(out[a+c],mean,impl_->strength);out[b+c]=std::lerp(out[b+c],mean,impl_->strength);}};for(int x=impl_->blockSize;x<w;x+=impl_->blockSize)for(int y=0;y<h;++y)smoothPair(static_cast<int>((static_cast<size_t>(y)*w+x-1)*4),static_cast<int>((static_cast<size_t>(y)*w+x)*4));for(int y=impl_->blockSize;y<h;y+=impl_->blockSize)for(int x=0;x<w;++x)smoothPair(static_cast<int>((static_cast<size_t>(y-1)*w+x)*4),static_cast<int>((static_cast<size_t>(y)*w+x)*4));for(size_t p=0;p<im.totalPixels();++p)for(int c=0;c<3;++c){const float processed=out[p*4+c];const float recovered=processed+(d[p*4+c]-processed)*impl_->detailRecovery;out[p*4+c]=std::lerp(d[p*4+c],recovered,impl_->mix);}result.setColorDescriptor(im.colorDescriptor());dst=ImageF32x4RGBAWithCache(result);}
std::vector<ArtifactCore::AbstractProperty> ArtifactDeblockEffect::getProperties()const{std::vector<ArtifactCore::AbstractProperty>p;auto&b=p.emplace_back();b.setName("blockSize");b.setDisplayLabel("Block Size");b.setType(ArtifactCore::PropertyType::Integer);b.setValue(impl_->blockSize);b.setDefaultValue(8);b.setHardRange(4,64);addCreativeFloat(p,"strength","Strength",impl_->strength,0,1);addCreativeFloat(p,"threshold","Boundary Threshold",impl_->threshold,0,.5f);addCreativeFloat(p,"detailRecovery","Detail Recovery",impl_->detailRecovery,0,1);addCreativeFloat(p,"mix","Mix",impl_->mix,0,1);return p;}
void ArtifactDeblockEffect::setPropertyValue(const ArtifactCore::UniString&name,const QVariant&v){const QString k=name.toQString();const float raw=v.toFloat();const float n=std::isfinite(raw)?raw:0.0f;if(k=="blockSize")impl_->blockSize=std::clamp(v.toInt(),4,64);else if(k=="strength")impl_->strength=std::clamp(n,0.0f,1.0f);else if(k=="threshold")impl_->threshold=std::clamp(n,0.0f,.5f);else if(k=="detailRecovery")impl_->detailRecovery=std::clamp(n,0.0f,1.0f);else if(k=="mix")impl_->mix=std::clamp(n,0.0f,1.0f);else ArtifactAbstractEffect::setPropertyValue(name,v);}
EffectROIHint ArtifactDeblockEffect::roiHint()const{return{.kind=EffectROIHintKind::Blur,.expansionPixels=2};}

class ArtifactBeautyStudioEffect::Impl {
public:
    float smoothing = 0.58f;
    float skinHue = 0.065f;
    float skinTolerance = 0.105f;
    float matteSoftness = 5.0f;
    float textureRecovery = 0.42f;
    float blemishReduction = 0.55f;
    float warmth = 0.08f;
    float mix = 1.0f;
    bool mattePreview = false;
};

ArtifactBeautyStudioEffect::ArtifactBeautyStudioEffect() : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.beauty_studio"));
    setDisplayName(ArtifactCore::UniString("Beauty Studio"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
}
ArtifactBeautyStudioEffect::~ArtifactBeautyStudioEffect(){delete impl_;impl_=nullptr;}

void ArtifactBeautyStudioEffect::apply(const ImageF32x4RGBAWithCache&src,ImageF32x4RGBAWithCache&dst){
    const auto&im=src.image();const int w=im.width(),h=im.height();const float*d=im.rgba32fData();if(!d||w<=0||h<=0){dst=src;return;}
    cv::Mat rgba(h,w,CV_32FC4,const_cast<float*>(d));std::vector<cv::Mat>ch;cv::split(rgba,ch);cv::Mat rgb;cv::merge(std::vector<cv::Mat>{ch[0],ch[1],ch[2]},rgb);
    cv::Mat smooth;const float sigmaColor=.04f+impl_->smoothing*.28f;const float sigmaSpace=2.0f+impl_->smoothing*14.0f;cv::bilateralFilter(rgb,smooth,-1,sigmaColor,sigmaSpace);
    cv::Mat baseBlur;cv::GaussianBlur(rgb,baseBlur,cv::Size(),1.0);cv::Mat matte(h,w,CV_32F);
    for(int y=0;y<h;++y){float*m=matte.ptr<float>(y);const cv::Vec3f*r=rgb.ptr<cv::Vec3f>(y);for(int x=0;x<w;++x){const float red=r[x][0],green=r[x][1],blue=r[x][2];const float mx=std::max({red,green,blue}),mn=std::min({red,green,blue}),delta=mx-mn;float hue=0;if(delta>.00001f){if(mx==red)hue=(green-blue)/delta/6.0f;else if(mx==green)hue=(2+(blue-red)/delta)/6.0f;else hue=(4+(red-green)/delta)/6.0f;if(hue<0)hue+=1;}const float saturation=mx>0?delta/mx:0;const float lum=red*.2126f+green*.7152f+blue*.0722f;float hd=std::abs(hue-impl_->skinHue);hd=std::min(hd,1-hd);const float hueScore=1-std::clamp(hd/std::max(.001f,impl_->skinTolerance),0.0f,1.0f);const float satScore=std::clamp((saturation-.06f)/.18f,0.0f,1.0f)*std::clamp((.95f-saturation)/.25f,0.0f,1.0f);const float lumScore=std::clamp((lum-.025f)/.16f,0.0f,1.0f);m[x]=hueScore*hueScore*satScore*lumScore;}}
    if(impl_->matteSoftness>0)cv::GaussianBlur(matte,matte,cv::Size(),impl_->matteSoftness);
    auto result=im.DeepCopy();float*out=result.rgba32fData();for(int y=0;y<h;++y){const cv::Vec3f*original=rgb.ptr<cv::Vec3f>(y),*sm=smooth.ptr<cv::Vec3f>(y),*bb=baseBlur.ptr<cv::Vec3f>(y);const float*m=matte.ptr<float>(y);for(int x=0;x<w;++x){const size_t o=(static_cast<size_t>(y)*w+x)*4;if(impl_->mattePreview){out[o]=out[o+1]=out[o+2]=m[x];out[o+3]=d[o+3];continue;}for(int c=0;c<3;++c){const float texture=(original[x][c]-bb[x][c])*impl_->textureRecovery;const float cleaned=std::lerp(original[x][c],sm[x][c],impl_->blemishReduction)+texture;float toned=cleaned;if(c==0)toned+=impl_->warmth*.018f;else if(c==2)toned-=impl_->warmth*.012f;out[o+c]=std::lerp(d[o+c],toned,m[x]*impl_->smoothing*impl_->mix);}out[o+3]=d[o+3];}}
    result.setColorDescriptor(im.colorDescriptor());dst=ImageF32x4RGBAWithCache(result);
}
std::vector<ArtifactCore::AbstractProperty> ArtifactBeautyStudioEffect::getProperties()const{std::vector<ArtifactCore::AbstractProperty>p;addCreativeFloat(p,"smoothing","Skin Smoothing",impl_->smoothing,0,1);addCreativeFloat(p,"skinHue","Skin Hue",impl_->skinHue,0,1);addCreativeFloat(p,"skinTolerance","Skin Range",impl_->skinTolerance,.01f,.5f);addCreativeFloat(p,"matteSoftness","Matte Softness",impl_->matteSoftness,0,32);addCreativeFloat(p,"textureRecovery","Texture Recovery",impl_->textureRecovery,0,1);addCreativeFloat(p,"blemishReduction","Blemish Reduction",impl_->blemishReduction,0,1);addCreativeFloat(p,"warmth","Skin Warmth",impl_->warmth,-1,1);addCreativeFloat(p,"mix","Mix",impl_->mix,0,1);auto&m=p.emplace_back();m.setName("mattePreview");m.setDisplayLabel("Matte Preview");m.setType(ArtifactCore::PropertyType::Boolean);m.setValue(impl_->mattePreview);m.setDefaultValue(false);return p;}
void ArtifactBeautyStudioEffect::setPropertyValue(const ArtifactCore::UniString&name,const QVariant&v){const QString k=name.toQString();const float raw=v.toFloat();const float n=std::isfinite(raw)?raw:0.0f;if(k=="smoothing")impl_->smoothing=std::clamp(n,0.0f,1.0f);else if(k=="skinHue")impl_->skinHue=std::clamp(n,0.0f,1.0f);else if(k=="skinTolerance")impl_->skinTolerance=std::clamp(n,.01f,.5f);else if(k=="matteSoftness")impl_->matteSoftness=std::clamp(n,0.0f,32.0f);else if(k=="textureRecovery")impl_->textureRecovery=std::clamp(n,0.0f,1.0f);else if(k=="blemishReduction")impl_->blemishReduction=std::clamp(n,0.0f,1.0f);else if(k=="warmth")impl_->warmth=std::clamp(n,-1.0f,1.0f);else if(k=="mix")impl_->mix=std::clamp(n,0.0f,1.0f);else if(k=="mattePreview")impl_->mattePreview=v.toBool();else ArtifactAbstractEffect::setPropertyValue(name,v);}
EffectROIHint ArtifactBeautyStudioEffect::roiHint()const{return{.kind=EffectROIHintKind::Blur,.expansionPixels=32};}

class ArtifactEnergyZapEffect::Impl {
public:
    float startX=.18f,startY=.5f,endX=.82f,endY=.5f,chaos=.16f,width=2.2f;
    float intensity=1.5f,glowRadius=12,branches=.45f,evolution=1,flicker=.18f,warmth=-.55f;
    int seed=404;QString pathInput=QStringLiteral("zap_path");
    IEffectFrameSampler*sampler=nullptr;std::int64_t frame=0;double time=0;
};
ArtifactEnergyZapEffect::ArtifactEnergyZapEffect():impl_(new Impl()){setEffectID(ArtifactCore::UniString("builtin.energy_zap"));setDisplayName(ArtifactCore::UniString("Energy Zap / Lightning"));setPipelineStage(EffectPipelineStage::Rasterizer);setAllowOverscan(true);}
ArtifactEnergyZapEffect::~ArtifactEnergyZapEffect(){delete impl_;impl_=nullptr;}
void ArtifactEnergyZapEffect::onContextUpdated(const EffectContext&c){impl_->sampler=c.sampler;impl_->frame=c.compositionFrame;impl_->time=c.timeSeconds;}
void ArtifactEnergyZapEffect::apply(const ImageF32x4RGBAWithCache& src,
                                    ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) {
        dst = src;
        return;
    }

    cv::Mat core(height, width, CV_32F, cv::Scalar(0));
    bool usedInput = false;
    ImageF32x4RGBAWithCache path;
    if (impl_->sampler && !impl_->pathInput.trimmed().isEmpty() &&
        impl_->sampler->sampleNamedInput(impl_->pathInput, impl_->frame, path) &&
        path.width() == width && path.height() == height) {
        const float* pathPixels = path.image().rgba32fData();
        if (pathPixels) {
            for (int y = 0; y < height; ++y) {
                float* row = core.ptr<float>(y);
                for (int x = 0; x < width; ++x) {
                    const std::size_t offset =
                        (static_cast<std::size_t>(y) * width + x) * 4u;
                    const float maskLuma = std::max({pathPixels[offset],
                        pathPixels[offset + 1], pathPixels[offset + 2]});
                    row[x] = std::clamp(maskLuma * pathPixels[offset + 3],
                                        0.0f, 1.0f);
                }
            }
            usedInput = true;
        }
    }

    if (!usedInput) {
        const cv::Point2f start(impl_->startX * width, impl_->startY * height);
        const cv::Point2f end(impl_->endX * width, impl_->endY * height);
        const cv::Point2f axis = end - start;
        const float length = std::max(1.0f,
            std::sqrt(axis.x * axis.x + axis.y * axis.y));
        const cv::Point2f normal(-axis.y / length, axis.x / length);
        constexpr int segmentCount = 72;
        std::vector<cv::Point> points;
        points.reserve(segmentCount + 1);
        const float phase = static_cast<float>(impl_->time * impl_->evolution);
        for (int index = 0; index <= segmentCount; ++index) {
            const float t = index / static_cast<float>(segmentCount);
            const float envelope = std::sin(t * static_cast<float>(CV_PI));
            const float noise = (creativeHash(index,
                static_cast<int>(impl_->frame), impl_->seed) - 0.5f) * 2.0f;
            const float wave = std::sin(t * 31.0f + phase * 5.0f) * 0.38f +
                               std::sin(t * 79.0f - phase * 3.0f) * 0.16f;
            const float offset = (noise + wave) * impl_->chaos * length * envelope;
            points.emplace_back(cvRound(start.x + axis.x * t + normal.x * offset),
                                cvRound(start.y + axis.y * t + normal.y * offset));
        }
        const std::vector<std::vector<cv::Point>> contours{points};
        cv::polylines(core, contours, false, cv::Scalar(1.0f),
                      std::max(1, cvRound(impl_->width)), cv::LINE_AA);

        const int branchCount = std::clamp(
            static_cast<int>(impl_->branches * 10.0f), 0, 10);
        for (int branch = 0; branch < branchCount; ++branch) {
            const int index = 5 + static_cast<int>(
                creativeHash(branch, 7, impl_->seed) * (segmentCount - 10));
            const cv::Point origin = points[index];
            const float side = creativeHash(branch, 11, impl_->seed) > 0.5f
                ? 1.0f : -1.0f;
            const float branchLength = length *
                (0.05f + creativeHash(branch, 13, impl_->seed) * 0.16f) *
                impl_->branches;
            const cv::Point tip(
                cvRound(origin.x + axis.x / length * branchLength * 0.35f +
                        normal.x * branchLength * side),
                cvRound(origin.y + axis.y / length * branchLength * 0.35f +
                        normal.y * branchLength * side));
            cv::line(core, origin, tip, cv::Scalar(0.7f),
                     std::max(1, cvRound(impl_->width * 0.55f)), cv::LINE_AA);
        }
    }

    cv::Mat glow;
    cv::GaussianBlur(core, glow, cv::Size(),
                     std::max(0.1f, impl_->glowRadius));
    const float flicker = 1.0f + std::sin(static_cast<float>(
        impl_->time * 37.0 + impl_->seed)) * impl_->flicker;
    auto result = image.DeepCopy();
    float* output = result.rgba32fData();
    for (int y = 0; y < height; ++y) {
        const float* coreRow = core.ptr<float>(y);
        const float* glowRow = glow.ptr<float>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            const float energy = (coreRow[x] * 1.8f + glowRow[x] * 2.6f) *
                                 impl_->intensity * flicker;
            output[offset] = source[offset] + energy *
                (0.72f + std::max(0.0f, impl_->warmth) * 0.28f);
            output[offset + 1] = source[offset + 1] + energy * 0.88f;
            output[offset + 2] = source[offset + 2] + energy *
                (1.15f + std::max(0.0f, -impl_->warmth) * 0.35f);
            output[offset + 3] = source[offset + 3];
        }
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}
std::vector<ArtifactCore::AbstractProperty> ArtifactEnergyZapEffect::getProperties()const{std::vector<ArtifactCore::AbstractProperty>p;addCreativeFloat(p,"startX","Start X",impl_->startX,0,1);addCreativeFloat(p,"startY","Start Y",impl_->startY,0,1);addCreativeFloat(p,"endX","End X",impl_->endX,0,1);addCreativeFloat(p,"endY","End Y",impl_->endY,0,1);addCreativeFloat(p,"chaos","Chaos",impl_->chaos,0,.6f);addCreativeFloat(p,"width","Core Width",impl_->width,.5f,24);addCreativeFloat(p,"intensity","Intensity",impl_->intensity,0,8);addCreativeFloat(p,"glowRadius","Glow Radius",impl_->glowRadius,.1f,128);addCreativeFloat(p,"branches","Branches",impl_->branches,0,1);addCreativeFloat(p,"evolution","Evolution Speed",impl_->evolution,-5,5);addCreativeFloat(p,"flicker","Flicker",impl_->flicker,0,1);addCreativeFloat(p,"warmth","Color Temperature",impl_->warmth,-1,1);auto&s=p.emplace_back();s.setName("seed");s.setDisplayLabel("Random Seed");s.setType(ArtifactCore::PropertyType::Integer);s.setValue(impl_->seed);s.setDefaultValue(404);s.setHardRange(0,100000);auto&i=p.emplace_back();i.setName("pathInput");i.setDisplayLabel("Mask / Path Input");i.setType(ArtifactCore::PropertyType::String);i.setValue(impl_->pathInput);i.setDefaultValue(QStringLiteral("zap_path"));return p;}
void ArtifactEnergyZapEffect::setPropertyValue(const ArtifactCore::UniString&name,const QVariant&v){const QString k=name.toQString();const float raw=v.toFloat();const float n=std::isfinite(raw)?raw:0.0f;if(k=="startX")impl_->startX=std::clamp(n,0.0f,1.0f);else if(k=="startY")impl_->startY=std::clamp(n,0.0f,1.0f);else if(k=="endX")impl_->endX=std::clamp(n,0.0f,1.0f);else if(k=="endY")impl_->endY=std::clamp(n,0.0f,1.0f);else if(k=="chaos")impl_->chaos=std::clamp(n,0.0f,.6f);else if(k=="width")impl_->width=std::clamp(n,.5f,24.0f);else if(k=="intensity")impl_->intensity=std::clamp(n,0.0f,8.0f);else if(k=="glowRadius")impl_->glowRadius=std::clamp(n,.1f,128.0f);else if(k=="branches")impl_->branches=std::clamp(n,0.0f,1.0f);else if(k=="evolution")impl_->evolution=std::clamp(n,-5.0f,5.0f);else if(k=="flicker")impl_->flicker=std::clamp(n,0.0f,1.0f);else if(k=="warmth")impl_->warmth=std::clamp(n,-1.0f,1.0f);else if(k=="seed")impl_->seed=std::clamp(v.toInt(),0,100000);else if(k=="pathInput")impl_->pathInput=v.toString();else ArtifactAbstractEffect::setPropertyValue(name,v);}
EffectROIHint ArtifactEnergyZapEffect::roiHint()const{return{.kind=EffectROIHintKind::Glow,.expansionPixels=impl_->glowRadius*3,.requiresFullFrame=true};}

namespace {

void addCreativeInteger(std::vector<ArtifactCore::AbstractProperty>& properties,
                        const char* name, const char* label, int value,
                        int minimum, int maximum) {
    auto& property = properties.emplace_back();
    property.setName(QString::fromUtf8(name));
    property.setDisplayLabel(QString::fromUtf8(label));
    property.setType(ArtifactCore::PropertyType::Integer);
    property.setValue(value);
    property.setDefaultValue(value);
    property.setHardRange(minimum, maximum);
    property.setAnimatable(true);
}

void addCreativeBoolean(std::vector<ArtifactCore::AbstractProperty>& properties,
                        const char* name, const char* label, bool value) {
    auto& property = properties.emplace_back();
    property.setName(QString::fromUtf8(name));
    property.setDisplayLabel(QString::fromUtf8(label));
    property.setType(ArtifactCore::PropertyType::Boolean);
    property.setValue(value);
    property.setDefaultValue(value);
}

void addCreativeString(std::vector<ArtifactCore::AbstractProperty>& properties,
                       const char* name, const char* label,
                       const QString& value) {
    auto& property = properties.emplace_back();
    property.setName(QString::fromUtf8(name));
    property.setDisplayLabel(QString::fromUtf8(label));
    property.setType(ArtifactCore::PropertyType::String);
    property.setValue(value);
    property.setDefaultValue(value);
}

bool prepareAuxiliaryImage(const ImageF32x4RGBAWithCache& input,
                           int width, int height, cv::Mat& output) {
    const float* pixels = input.image().rgba32fData();
    if (!pixels || input.width() <= 0 || input.height() <= 0) return false;
    cv::Mat wrapped(input.height(), input.width(), CV_32FC4,
                    const_cast<float*>(pixels));
    if (input.width() == width && input.height() == height) {
        output = wrapped.clone();
    } else {
        cv::resize(wrapped, output, cv::Size(width, height), 0.0, 0.0,
                   cv::INTER_LINEAR);
    }
    return !output.empty();
}

cv::Mat extractAuxiliaryMask(const cv::Mat& rgba) {
    if (rgba.empty() || rgba.type() != CV_32FC4) return {};
    std::vector<cv::Mat> channels;
    cv::split(rgba, channels);
    cv::Mat rgbMask;
    cv::max(channels[0], channels[1], rgbMask);
    cv::max(rgbMask, channels[2], rgbMask);
    double maximumRgb = 0.0;
    cv::minMaxLoc(rgbMask, nullptr, &maximumRgb);
    double minimumAlpha = 0.0;
    double maximumAlpha = 0.0;
    cv::minMaxLoc(channels[3], &minimumAlpha, &maximumAlpha);
    const bool hasUsefulAlpha = maximumRgb <= 1.0e-5 &&
        (maximumAlpha < 0.99999 || maximumAlpha - minimumAlpha > 1.0e-5);
    cv::Mat mask;
    if (hasUsefulAlpha) mask = channels[3].clone();
    else mask = rgbMask.mul(channels[3]);
    cv::min(mask, 1.0, mask);
    cv::max(mask, 0.0, mask);
    return mask;
}

float smoothMask(float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

} // namespace

class ArtifactLightWrapProEffect::Impl {
public:
    float radius = 18.0f;
    float intensity = 0.65f;
    float exposure = 0.0f;
    float saturation = 1.0f;
    float edgeGain = 4.0f;
    float mix = 1.0f;
    int blendMode = 0;
    QString backgroundInput = QStringLiteral("background");
    IEffectFrameSampler* sampler = nullptr;
    std::int64_t frame = 0;
};

ArtifactLightWrapProEffect::ArtifactLightWrapProEffect() : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.light_wrap_pro"));
    setDisplayName(ArtifactCore::UniString("Light Wrap Pro"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setAllowOverscan(true);
}

ArtifactLightWrapProEffect::~ArtifactLightWrapProEffect() {
    delete impl_;
    impl_ = nullptr;
}

void ArtifactLightWrapProEffect::onContextUpdated(const EffectContext& context) {
    impl_->sampler = context.sampler;
    impl_->frame = context.compositionFrame;
}

void ArtifactLightWrapProEffect::apply(const ImageF32x4RGBAWithCache& src,
                                       ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    ImageF32x4RGBAWithCache backgroundImage;
    cv::Mat background;
    if (!source || width <= 0 || height <= 0 || !impl_->sampler ||
        impl_->backgroundInput.trimmed().isEmpty() ||
        !impl_->sampler->sampleNamedInput(impl_->backgroundInput, impl_->frame,
                                          backgroundImage) ||
        !prepareAuxiliaryImage(backgroundImage, width, height, background)) {
        dst = src;
        return;
    }

    cv::Mat foreground(height, width, CV_32FC4, const_cast<float*>(source));
    std::vector<cv::Mat> foregroundChannels;
    std::vector<cv::Mat> backgroundChannels;
    cv::split(foreground, foregroundChannels);
    cv::split(background, backgroundChannels);
    cv::Mat blurredAlpha;
    cv::GaussianBlur(foregroundChannels[3], blurredAlpha, cv::Size(),
                     std::max(0.1f, impl_->radius));
    cv::Mat innerEdge = foregroundChannels[3] - blurredAlpha;
    cv::max(innerEdge, 0.0, innerEdge);
    innerEdge *= impl_->edgeGain;
    cv::min(innerEdge, 1.0, innerEdge);

    cv::Mat backgroundRgb;
    cv::merge(std::vector<cv::Mat>{backgroundChannels[0], backgroundChannels[1],
                                  backgroundChannels[2]}, backgroundRgb);
    cv::GaussianBlur(backgroundRgb, backgroundRgb, cv::Size(),
                     std::max(0.1f, impl_->radius * 0.45f));

    auto result = image.DeepCopy();
    float* output = result.rgba32fData();
    const float exposure = std::exp2(impl_->exposure);
    for (int y = 0; y < height; ++y) {
        const float* edgeRow = innerEdge.ptr<float>(y);
        const cv::Vec3f* backgroundRow = backgroundRgb.ptr<cv::Vec3f>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            const float amount = std::clamp(edgeRow[x] * impl_->intensity *
                                            impl_->mix, 0.0f, 1.0f);
            const cv::Vec3f rawWrap = backgroundRow[x] * exposure;
            const float wrapLuma = rawWrap[0] * 0.2126f + rawWrap[1] * 0.7152f +
                                   rawWrap[2] * 0.0722f;
            for (int channel = 0; channel < 3; ++channel) {
                const float wrap = wrapLuma +
                    (rawWrap[channel] - wrapLuma) * impl_->saturation;
                float treated = wrap;
                if (impl_->blendMode == 1) {
                    treated = source[offset + channel] + wrap;
                } else if (impl_->blendMode == 2) {
                    const float base = source[offset + channel];
                    treated = base < 0.5f
                        ? 2.0f * base * wrap
                        : 1.0f - 2.0f * (1.0f - base) * (1.0f - wrap);
                }
                output[offset + channel] = std::lerp(source[offset + channel],
                                                     treated, amount);
            }
            output[offset + 3] = source[offset + 3];
        }
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty>
ArtifactLightWrapProEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    addCreativeFloat(properties, "radius", "Wrap Radius", impl_->radius, 0.1f, 256.0f);
    addCreativeFloat(properties, "intensity", "Intensity", impl_->intensity, 0.0f, 2.0f);
    addCreativeFloat(properties, "exposure", "Background Exposure", impl_->exposure, -4.0f, 4.0f);
    addCreativeFloat(properties, "saturation", "Background Saturation", impl_->saturation, 0.0f, 2.0f);
    addCreativeFloat(properties, "edgeGain", "Edge Gain", impl_->edgeGain, 0.5f, 12.0f);
    addCreativeFloat(properties, "mix", "Mix", impl_->mix, 0.0f, 1.0f);
    addCreativeInteger(properties, "blendMode", "Blend Mode", impl_->blendMode, 0, 2);
    addCreativeString(properties, "backgroundInput", "Background Input",
                      impl_->backgroundInput);
    return properties;
}

void ArtifactLightWrapProEffect::setPropertyValue(
    const ArtifactCore::UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    const float raw = value.toFloat();
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("radius")) impl_->radius = std::clamp(number, 0.1f, 256.0f);
    else if (key == QStringLiteral("intensity")) impl_->intensity = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("exposure")) impl_->exposure = std::clamp(number, -4.0f, 4.0f);
    else if (key == QStringLiteral("saturation")) impl_->saturation = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("edgeGain")) impl_->edgeGain = std::clamp(number, 0.5f, 12.0f);
    else if (key == QStringLiteral("mix")) impl_->mix = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("blendMode")) impl_->blendMode = std::clamp(value.toInt(), 0, 2);
    else if (key == QStringLiteral("backgroundInput")) impl_->backgroundInput = value.toString();
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint ArtifactLightWrapProEffect::roiHint() const {
    return {.kind = EffectROIHintKind::Glow,
            .expansionPixels = impl_->radius * 3.0f};
}

class ArtifactMatchGrainEffect::Impl {
public:
    float grainSize = 1.15f;
    float amount = 1.0f;
    float lumaAmount = 1.0f;
    float chromaAmount = 0.65f;
    float shadowResponse = 0.25f;
    float highlightResponse = -0.2f;
    float temporalJitter = 1.0f;
    float mix = 1.0f;
    QString referenceInput = QStringLiteral("grain_reference");
    IEffectFrameSampler* sampler = nullptr;
    std::int64_t frame = 0;
};

ArtifactMatchGrainEffect::ArtifactMatchGrainEffect() : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.match_grain"));
    setDisplayName(ArtifactCore::UniString("Match Grain"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

ArtifactMatchGrainEffect::~ArtifactMatchGrainEffect() {
    delete impl_;
    impl_ = nullptr;
}

void ArtifactMatchGrainEffect::onContextUpdated(const EffectContext& context) {
    impl_->sampler = context.sampler;
    impl_->frame = context.compositionFrame;
}

void ArtifactMatchGrainEffect::apply(const ImageF32x4RGBAWithCache& src,
                                     ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    ImageF32x4RGBAWithCache referenceImage;
    cv::Mat reference;
    if (!source || width <= 0 || height <= 0 || !impl_->sampler ||
        impl_->referenceInput.trimmed().isEmpty() ||
        !impl_->sampler->sampleNamedInput(impl_->referenceInput, impl_->frame,
                                          referenceImage) ||
        !prepareAuxiliaryImage(referenceImage, width, height, reference)) {
        dst = src;
        return;
    }

    std::vector<cv::Mat> channels;
    cv::split(reference, channels);
    std::vector<cv::Mat> residual(3);
    for (int channel = 0; channel < 3; ++channel) {
        cv::Mat lowFrequency;
        cv::GaussianBlur(channels[channel], lowFrequency, cv::Size(),
                         std::max(0.1f, impl_->grainSize));
        residual[channel] = channels[channel] - lowFrequency;
    }
    const int jitterX = static_cast<int>((creativeHash(
        static_cast<int>(impl_->frame), 17, 9127) - 0.5f) * width *
        impl_->temporalJitter);
    const int jitterY = static_cast<int>((creativeHash(
        31, static_cast<int>(impl_->frame), 1777) - 0.5f) * height *
        impl_->temporalJitter);

    auto result = image.DeepCopy();
    float* output = result.rgba32fData();
    for (int y = 0; y < height; ++y) {
        const int sampleY = (y + jitterY % height + height) % height;
        const float* residualRows[3] = {residual[0].ptr<float>(sampleY),
                                        residual[1].ptr<float>(sampleY),
                                        residual[2].ptr<float>(sampleY)};
        for (int x = 0; x < width; ++x) {
            const int sampleX = (x + jitterX % width + width) % width;
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            const float sourceLuma = source[offset] * 0.2126f +
                                     source[offset + 1] * 0.7152f +
                                     source[offset + 2] * 0.0722f;
            const float residualLuma = residualRows[0][sampleX] * 0.2126f +
                                       residualRows[1][sampleX] * 0.7152f +
                                       residualRows[2][sampleX] * 0.0722f;
            const float response = std::max(0.0f,
                1.0f + (1.0f - std::clamp(sourceLuma, 0.0f, 1.0f)) *
                    impl_->shadowResponse + std::clamp(sourceLuma, 0.0f, 1.0f) *
                    impl_->highlightResponse);
            for (int channel = 0; channel < 3; ++channel) {
                const float chromaResidual = residualRows[channel][sampleX] -
                                             residualLuma;
                const float grain = (residualLuma * impl_->lumaAmount +
                                     chromaResidual * impl_->chromaAmount) *
                                    impl_->amount * response;
                output[offset + channel] = source[offset + channel] +
                                            grain * impl_->mix;
            }
            output[offset + 3] = source[offset + 3];
        }
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty>
ArtifactMatchGrainEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    addCreativeFloat(properties, "grainSize", "Grain Size", impl_->grainSize, 0.1f, 8.0f);
    addCreativeFloat(properties, "amount", "Match Amount", impl_->amount, 0.0f, 4.0f);
    addCreativeFloat(properties, "lumaAmount", "Luma Grain", impl_->lumaAmount, 0.0f, 2.0f);
    addCreativeFloat(properties, "chromaAmount", "Chroma Grain", impl_->chromaAmount, 0.0f, 2.0f);
    addCreativeFloat(properties, "shadowResponse", "Shadow Response", impl_->shadowResponse, -1.0f, 2.0f);
    addCreativeFloat(properties, "highlightResponse", "Highlight Response", impl_->highlightResponse, -1.0f, 2.0f);
    addCreativeFloat(properties, "temporalJitter", "Temporal Jitter", impl_->temporalJitter, 0.0f, 1.0f);
    addCreativeFloat(properties, "mix", "Mix", impl_->mix, 0.0f, 1.0f);
    addCreativeString(properties, "referenceInput", "Grain Reference Input",
                      impl_->referenceInput);
    return properties;
}

void ArtifactMatchGrainEffect::setPropertyValue(
    const ArtifactCore::UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    const float raw = value.toFloat();
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("grainSize")) impl_->grainSize = std::clamp(number, 0.1f, 8.0f);
    else if (key == QStringLiteral("amount")) impl_->amount = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("lumaAmount")) impl_->lumaAmount = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("chromaAmount")) impl_->chromaAmount = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("shadowResponse")) impl_->shadowResponse = std::clamp(number, -1.0f, 2.0f);
    else if (key == QStringLiteral("highlightResponse")) impl_->highlightResponse = std::clamp(number, -1.0f, 2.0f);
    else if (key == QStringLiteral("temporalJitter")) impl_->temporalJitter = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("mix")) impl_->mix = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("referenceInput")) impl_->referenceInput = value.toString();
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint ArtifactMatchGrainEffect::roiHint() const {
    return {.kind = EffectROIHintKind::Blur,
            .expansionPixels = impl_->grainSize * 3.0f};
}

class ArtifactWireObjectRemoverEffect::Impl {
public:
    float startX = 0.25f;
    float startY = 0.2f;
    float endX = 0.75f;
    float endY = 0.8f;
    float width = 8.0f;
    float feather = 5.0f;
    float cloneOffsetX = 32.0f;
    float cloneOffsetY = 0.0f;
    float mix = 1.0f;
    int method = 0;
    int temporalOffset = 0;
    bool lineEnabled = false;
    QString maskInput = QStringLiteral("remove_mask");
    QString cleanInput = QStringLiteral("clean_plate");
    IEffectFrameSampler* sampler = nullptr;
    std::int64_t frame = 0;
};

ArtifactWireObjectRemoverEffect::ArtifactWireObjectRemoverEffect()
    : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.wire_object_remover"));
    setDisplayName(ArtifactCore::UniString("Wire / Object Remover"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setAllowOverscan(true);
}

ArtifactWireObjectRemoverEffect::~ArtifactWireObjectRemoverEffect() {
    delete impl_;
    impl_ = nullptr;
}

void ArtifactWireObjectRemoverEffect::onContextUpdated(
    const EffectContext& context) {
    impl_->sampler = context.sampler;
    impl_->frame = context.compositionFrame;
}

void ArtifactWireObjectRemoverEffect::apply(
    const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) {
        dst = src;
        return;
    }

    cv::Mat mask(height, width, CV_32F, cv::Scalar(0.0f));
    cv::Mat namedMask;
    ImageF32x4RGBAWithCache maskImage;
    if (impl_->sampler && !impl_->maskInput.trimmed().isEmpty() &&
        impl_->sampler->sampleNamedInput(impl_->maskInput, impl_->frame,
                                         maskImage)) {
        cv::Mat maskRgba;
        if (prepareAuxiliaryImage(maskImage, width, height, maskRgba)) {
            namedMask = extractAuxiliaryMask(maskRgba);
            cv::max(mask, namedMask, mask);
        }
    }

    const cv::Point2f start(impl_->startX * (width - 1),
                            impl_->startY * (height - 1));
    const cv::Point2f end(impl_->endX * (width - 1),
                          impl_->endY * (height - 1));
    cv::Point2f direction = end - start;
    const float lengthSquared = direction.dot(direction);
    const float length = std::sqrt(std::max(lengthSquared, 1.0e-8f));
    const cv::Point2f normal(-direction.y / length, direction.x / length);
    if (impl_->lineEnabled && lengthSquared > 1.0e-8f) {
        const float innerRadius = impl_->width * 0.5f;
        const float outerRadius = innerRadius + impl_->feather;
        for (int y = 0; y < height; ++y) {
            float* maskRow = mask.ptr<float>(y);
            for (int x = 0; x < width; ++x) {
                const cv::Point2f point(static_cast<float>(x), static_cast<float>(y));
                const float t = std::clamp((point - start).dot(direction) /
                                           lengthSquared, 0.0f, 1.0f);
                const float distance = cv::norm(point - (start + direction * t));
                float weight = 0.0f;
                if (distance <= innerRadius) weight = 1.0f;
                else if (distance < outerRadius && impl_->feather > 0.0f) {
                    weight = 1.0f - smoothMask((distance - innerRadius) /
                                               impl_->feather);
                }
                maskRow[x] = std::max(maskRow[x], weight);
            }
        }
    }
    if (cv::countNonZero(mask > 0.0001f) == 0) {
        dst = src;
        return;
    }

    cv::Mat replacement;
    bool hasReplacement = false;
    ImageF32x4RGBAWithCache replacementImage;
    if (impl_->sampler && !impl_->cleanInput.trimmed().isEmpty() &&
        impl_->sampler->sampleNamedInput(impl_->cleanInput, impl_->frame,
                                         replacementImage)) {
        hasReplacement = prepareAuxiliaryImage(replacementImage, width, height,
                                               replacement);
    }
    if (!hasReplacement && impl_->sampler && impl_->temporalOffset != 0 &&
        impl_->sampler->sampleCurrentLayerFrame(
            impl_->frame + impl_->temporalOffset, replacementImage)) {
        hasReplacement = prepareAuxiliaryImage(replacementImage, width, height,
                                               replacement);
    }
    if (!hasReplacement) {
        replacement = cv::Mat(height, width, CV_32FC4,
                              const_cast<float*>(source)).clone();
    }

    cv::Mat blurredSource;
    if (impl_->method == 2 && !hasReplacement) {
        cv::GaussianBlur(replacement, blurredSource, cv::Size(),
                         std::max(1.0f, impl_->width));
    }

    auto result = image.DeepCopy();
    float* output = result.rgba32fData();
    const float sideDistance = impl_->width * 0.5f + impl_->feather + 2.0f;
    for (int y = 0; y < height; ++y) {
        const float* maskRow = mask.ptr<float>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            const float amount = std::clamp(maskRow[x] * impl_->mix, 0.0f, 1.0f);
            if (amount <= 0.0f) continue;
            for (int channel = 0; channel < 3; ++channel) {
                float fill = source[offset + channel];
                if (hasReplacement) {
                    fill = replacement.at<cv::Vec4f>(y, x)[channel];
                } else if (impl_->method == 0 && impl_->lineEnabled) {
                    const float a = sampleHeatwaveChannel(
                        source, width, height, x + normal.x * sideDistance,
                        y + normal.y * sideDistance, channel);
                    const float b = sampleHeatwaveChannel(
                        source, width, height, x - normal.x * sideDistance,
                        y - normal.y * sideDistance, channel);
                    fill = (a + b) * 0.5f;
                } else if (impl_->method == 2) {
                    fill = blurredSource.at<cv::Vec4f>(y, x)[channel];
                } else {
                    fill = sampleHeatwaveChannel(
                        source, width, height, x + impl_->cloneOffsetX,
                        y + impl_->cloneOffsetY, channel);
                }
                output[offset + channel] = std::lerp(source[offset + channel],
                                                     fill, amount);
            }
            output[offset + 3] = source[offset + 3];
        }
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty>
ArtifactWireObjectRemoverEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    addCreativeBoolean(properties, "lineEnabled", "Enable Wire Line", impl_->lineEnabled);
    addCreativeFloat(properties, "startX", "Start X", impl_->startX, 0.0f, 1.0f);
    addCreativeFloat(properties, "startY", "Start Y", impl_->startY, 0.0f, 1.0f);
    addCreativeFloat(properties, "endX", "End X", impl_->endX, 0.0f, 1.0f);
    addCreativeFloat(properties, "endY", "End Y", impl_->endY, 0.0f, 1.0f);
    addCreativeFloat(properties, "width", "Removal Width", impl_->width, 0.5f, 256.0f);
    addCreativeFloat(properties, "feather", "Feather", impl_->feather, 0.0f, 128.0f);
    addCreativeInteger(properties, "method", "Fill Method", impl_->method, 0, 2);
    addCreativeFloat(properties, "cloneOffsetX", "Clone Offset X", impl_->cloneOffsetX, -2048.0f, 2048.0f);
    addCreativeFloat(properties, "cloneOffsetY", "Clone Offset Y", impl_->cloneOffsetY, -2048.0f, 2048.0f);
    addCreativeInteger(properties, "temporalOffset", "Source Frame Offset", impl_->temporalOffset, -120, 120);
    addCreativeFloat(properties, "mix", "Mix", impl_->mix, 0.0f, 1.0f);
    addCreativeString(properties, "maskInput", "Removal Mask Input", impl_->maskInput);
    addCreativeString(properties, "cleanInput", "Clean Plate Input", impl_->cleanInput);
    return properties;
}

void ArtifactWireObjectRemoverEffect::setPropertyValue(
    const ArtifactCore::UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    const float raw = value.toFloat();
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("lineEnabled")) impl_->lineEnabled = value.toBool();
    else if (key == QStringLiteral("startX")) impl_->startX = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("startY")) impl_->startY = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("endX")) impl_->endX = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("endY")) impl_->endY = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("width")) impl_->width = std::clamp(number, 0.5f, 256.0f);
    else if (key == QStringLiteral("feather")) impl_->feather = std::clamp(number, 0.0f, 128.0f);
    else if (key == QStringLiteral("method")) impl_->method = std::clamp(value.toInt(), 0, 2);
    else if (key == QStringLiteral("cloneOffsetX")) impl_->cloneOffsetX = std::clamp(number, -2048.0f, 2048.0f);
    else if (key == QStringLiteral("cloneOffsetY")) impl_->cloneOffsetY = std::clamp(number, -2048.0f, 2048.0f);
    else if (key == QStringLiteral("temporalOffset")) impl_->temporalOffset = std::clamp(value.toInt(), -120, 120);
    else if (key == QStringLiteral("mix")) impl_->mix = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("maskInput")) impl_->maskInput = value.toString();
    else if (key == QStringLiteral("cleanInput")) impl_->cleanInput = value.toString();
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint ArtifactWireObjectRemoverEffect::roiHint() const {
    return {.kind = EffectROIHintKind::Mask,
            .expansionPixels = std::max({impl_->width + impl_->feather,
                                        std::abs(impl_->cloneOffsetX),
                                        std::abs(impl_->cloneOffsetY)}),
            .requiresFullFrame = impl_->temporalOffset != 0};
}

class ArtifactDepthRelightEffect::Impl {
public:
    float lightX = 0.35f;
    float lightY = 0.25f;
    float lightZ = 0.65f;
    float ambient = 0.55f;
    float diffuse = 0.85f;
    float specular = 0.35f;
    float shininess = 24.0f;
    float rim = 0.2f;
    float rimPower = 2.5f;
    float normalStrength = 3.0f;
    float depthScale = 0.45f;
    float falloff = 0.8f;
    float red = 1.0f;
    float green = 0.93f;
    float blue = 0.82f;
    float mix = 1.0f;
    bool invertDepth = false;
    bool useLumaFallback = true;
    QString depthInput = QStringLiteral("depth");
    QString normalInput = QStringLiteral("normal");
    IEffectFrameSampler* sampler = nullptr;
    std::int64_t frame = 0;
};

ArtifactDepthRelightEffect::ArtifactDepthRelightEffect() : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.depth_relight"));
    setDisplayName(ArtifactCore::UniString("Depth Relight"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

ArtifactDepthRelightEffect::~ArtifactDepthRelightEffect() {
    delete impl_;
    impl_ = nullptr;
}

void ArtifactDepthRelightEffect::onContextUpdated(const EffectContext& context) {
    impl_->sampler = context.sampler;
    impl_->frame = context.compositionFrame;
}

void ArtifactDepthRelightEffect::apply(const ImageF32x4RGBAWithCache& src,
                                       ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) {
        dst = src;
        return;
    }

    cv::Mat depth(height, width, CV_32F);
    bool hasDepth = false;
    ImageF32x4RGBAWithCache depthImage;
    cv::Mat depthRgba;
    if (impl_->sampler && !impl_->depthInput.trimmed().isEmpty() &&
        impl_->sampler->sampleNamedInput(impl_->depthInput, impl_->frame,
                                         depthImage) &&
        prepareAuxiliaryImage(depthImage, width, height, depthRgba)) {
        std::vector<cv::Mat> channels;
        cv::split(depthRgba, channels);
        depth = channels[0].clone();
        hasDepth = true;
    } else if (impl_->useLumaFallback) {
        for (int y = 0; y < height; ++y) {
            float* row = depth.ptr<float>(y);
            for (int x = 0; x < width; ++x) {
                const std::size_t offset =
                    (static_cast<std::size_t>(y) * width + x) * 4u;
                row[x] = source[offset] * 0.2126f +
                         source[offset + 1] * 0.7152f +
                         source[offset + 2] * 0.0722f;
            }
        }
        hasDepth = true;
    }
    if (!hasDepth) {
        dst = src;
        return;
    }
    cv::min(depth, 1.0, depth);
    cv::max(depth, 0.0, depth);
    if (impl_->invertDepth) depth = 1.0f - depth;

    cv::Mat normalRgba;
    bool hasNormalMap = false;
    ImageF32x4RGBAWithCache normalImage;
    if (impl_->sampler && !impl_->normalInput.trimmed().isEmpty() &&
        impl_->sampler->sampleNamedInput(impl_->normalInput, impl_->frame,
                                         normalImage)) {
        hasNormalMap = prepareAuxiliaryImage(normalImage, width, height,
                                             normalRgba);
    }
    cv::Mat gradientX;
    cv::Mat gradientY;
    if (!hasNormalMap) {
        cv::Sobel(depth, gradientX, CV_32F, 1, 0, 3, 0.125);
        cv::Sobel(depth, gradientY, CV_32F, 0, 1, 3, 0.125);
    }

    auto result = image.DeepCopy();
    float* output = result.rgba32fData();
    const float lightColor[3] = {impl_->red, impl_->green, impl_->blue};
    for (int y = 0; y < height; ++y) {
        const float* depthRow = depth.ptr<float>(y);
        const float* gradientXRow = hasNormalMap ? nullptr : gradientX.ptr<float>(y);
        const float* gradientYRow = hasNormalMap ? nullptr : gradientY.ptr<float>(y);
        const cv::Vec4f* normalRow = hasNormalMap
            ? normalRgba.ptr<cv::Vec4f>(y) : nullptr;
        for (int x = 0; x < width; ++x) {
            cv::Vec3f normal;
            if (hasNormalMap) {
                normal = cv::Vec3f(normalRow[x][0] * 2.0f - 1.0f,
                                   normalRow[x][1] * 2.0f - 1.0f,
                                   normalRow[x][2] * 2.0f - 1.0f);
            } else {
                normal = cv::Vec3f(-gradientXRow[x] * impl_->normalStrength,
                                   -gradientYRow[x] * impl_->normalStrength,
                                   1.0f);
            }
            const float normalLength = std::sqrt(normal.dot(normal));
            normal *= 1.0f / std::max(normalLength, 1.0e-6f);
            const float u = width > 1 ? static_cast<float>(x) / (width - 1) : 0.5f;
            const float v = height > 1 ? static_cast<float>(y) / (height - 1) : 0.5f;
            cv::Vec3f light(impl_->lightX - u, impl_->lightY - v,
                            impl_->lightZ - depthRow[x] * impl_->depthScale);
            const float distance = std::sqrt(light.dot(light));
            light *= 1.0f / std::max(distance, 1.0e-6f);
            const float attenuation = 1.0f /
                (1.0f + distance * distance * impl_->falloff);
            const float ndotl = std::max(0.0f, normal.dot(light));
            cv::Vec3f halfVector = light + cv::Vec3f(0.0f, 0.0f, 1.0f);
            halfVector *= 1.0f /
                std::max(std::sqrt(halfVector.dot(halfVector)), 1.0e-6f);
            const float specular = impl_->specular *
                std::pow(std::max(0.0f, normal.dot(halfVector)), impl_->shininess) *
                attenuation;
            const float rim = impl_->rim *
                std::pow(std::clamp(1.0f - std::max(0.0f, normal[2]), 0.0f, 1.0f),
                         impl_->rimPower);
            const float diffuse = impl_->diffuse * ndotl * attenuation;
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            for (int channel = 0; channel < 3; ++channel) {
                const float lit = source[offset + channel] *
                    (impl_->ambient + diffuse * lightColor[channel] + rim) +
                    specular * lightColor[channel];
                output[offset + channel] = std::lerp(source[offset + channel],
                                                     lit, impl_->mix);
            }
            output[offset + 3] = source[offset + 3];
        }
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty>
ArtifactDepthRelightEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    addCreativeFloat(properties, "lightX", "Light X", impl_->lightX, -1.0f, 2.0f);
    addCreativeFloat(properties, "lightY", "Light Y", impl_->lightY, -1.0f, 2.0f);
    addCreativeFloat(properties, "lightZ", "Light Z", impl_->lightZ, -1.0f, 4.0f);
    addCreativeFloat(properties, "ambient", "Ambient", impl_->ambient, 0.0f, 2.0f);
    addCreativeFloat(properties, "diffuse", "Diffuse", impl_->diffuse, 0.0f, 4.0f);
    addCreativeFloat(properties, "specular", "Specular", impl_->specular, 0.0f, 4.0f);
    addCreativeFloat(properties, "shininess", "Shininess", impl_->shininess, 1.0f, 256.0f);
    addCreativeFloat(properties, "rim", "Rim Light", impl_->rim, 0.0f, 4.0f);
    addCreativeFloat(properties, "rimPower", "Rim Falloff", impl_->rimPower, 0.25f, 12.0f);
    addCreativeFloat(properties, "normalStrength", "Depth Normal Strength", impl_->normalStrength, 0.0f, 32.0f);
    addCreativeFloat(properties, "depthScale", "Depth Scale", impl_->depthScale, -4.0f, 4.0f);
    addCreativeFloat(properties, "falloff", "Distance Falloff", impl_->falloff, 0.0f, 8.0f);
    addCreativeFloat(properties, "red", "Light Red", impl_->red, 0.0f, 4.0f);
    addCreativeFloat(properties, "green", "Light Green", impl_->green, 0.0f, 4.0f);
    addCreativeFloat(properties, "blue", "Light Blue", impl_->blue, 0.0f, 4.0f);
    addCreativeFloat(properties, "mix", "Mix", impl_->mix, 0.0f, 1.0f);
    addCreativeBoolean(properties, "invertDepth", "Invert Depth", impl_->invertDepth);
    addCreativeBoolean(properties, "useLumaFallback", "Use Luma Fallback", impl_->useLumaFallback);
    addCreativeString(properties, "depthInput", "Depth Input", impl_->depthInput);
    addCreativeString(properties, "normalInput", "Normal Input", impl_->normalInput);
    return properties;
}

void ArtifactDepthRelightEffect::setPropertyValue(
    const ArtifactCore::UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    const float raw = value.toFloat();
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("lightX")) impl_->lightX = std::clamp(number, -1.0f, 2.0f);
    else if (key == QStringLiteral("lightY")) impl_->lightY = std::clamp(number, -1.0f, 2.0f);
    else if (key == QStringLiteral("lightZ")) impl_->lightZ = std::clamp(number, -1.0f, 4.0f);
    else if (key == QStringLiteral("ambient")) impl_->ambient = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("diffuse")) impl_->diffuse = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("specular")) impl_->specular = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("shininess")) impl_->shininess = std::clamp(number, 1.0f, 256.0f);
    else if (key == QStringLiteral("rim")) impl_->rim = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("rimPower")) impl_->rimPower = std::clamp(number, 0.25f, 12.0f);
    else if (key == QStringLiteral("normalStrength")) impl_->normalStrength = std::clamp(number, 0.0f, 32.0f);
    else if (key == QStringLiteral("depthScale")) impl_->depthScale = std::clamp(number, -4.0f, 4.0f);
    else if (key == QStringLiteral("falloff")) impl_->falloff = std::clamp(number, 0.0f, 8.0f);
    else if (key == QStringLiteral("red")) impl_->red = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("green")) impl_->green = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("blue")) impl_->blue = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("mix")) impl_->mix = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("invertDepth")) impl_->invertDepth = value.toBool();
    else if (key == QStringLiteral("useLumaFallback")) impl_->useLumaFallback = value.toBool();
    else if (key == QStringLiteral("depthInput")) impl_->depthInput = value.toString();
    else if (key == QStringLiteral("normalInput")) impl_->normalInput = value.toString();
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint ArtifactDepthRelightEffect::roiHint() const {
    return {.kind = EffectROIHintKind::Matte,
            .expansionPixels = 2.0f};
}

class ArtifactMatteRefineEffect::Impl {
public:
    float growShrink = 0.0f;
    float feather = 1.5f;
    float blackClip = 0.02f;
    float whiteClip = 0.98f;
    float decontaminate = 0.55f;
    float decontaminateRadius = 3.0f;
    float mix = 1.0f;
    bool mattePreview = false;
    QString matteInput = QStringLiteral("matte");
    IEffectFrameSampler* sampler = nullptr;
    std::int64_t frame = 0;
};

ArtifactMatteRefineEffect::ArtifactMatteRefineEffect() : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.matte_refine"));
    setDisplayName(ArtifactCore::UniString("Matte Refine"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setAllowOverscan(true);
}

ArtifactMatteRefineEffect::~ArtifactMatteRefineEffect() {
    delete impl_;
    impl_ = nullptr;
}

void ArtifactMatteRefineEffect::onContextUpdated(const EffectContext& context) {
    impl_->sampler = context.sampler;
    impl_->frame = context.compositionFrame;
}

void ArtifactMatteRefineEffect::apply(const ImageF32x4RGBAWithCache& src,
                                      ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) {
        dst = src;
        return;
    }
    cv::Mat rgba(height, width, CV_32FC4, const_cast<float*>(source));
    std::vector<cv::Mat> channels;
    cv::split(rgba, channels);
    cv::Mat matte = channels[3].clone();
    ImageF32x4RGBAWithCache matteImage;
    cv::Mat matteRgba;
    if (impl_->sampler && !impl_->matteInput.trimmed().isEmpty() &&
        impl_->sampler->sampleNamedInput(impl_->matteInput, impl_->frame,
                                         matteImage) &&
        prepareAuxiliaryImage(matteImage, width, height, matteRgba)) {
        matte = extractAuxiliaryMask(matteRgba);
    }
    cv::min(matte, 1.0, matte);
    cv::max(matte, 0.0, matte);

    const int morphologyRadius = std::clamp(
        static_cast<int>(std::round(std::abs(impl_->growShrink))), 0, 32);
    if (morphologyRadius > 0) {
        const cv::Mat kernel = cv::getStructuringElement(
            cv::MORPH_ELLIPSE,
            cv::Size(morphologyRadius * 2 + 1, morphologyRadius * 2 + 1));
        if (impl_->growShrink >= 0.0f) cv::dilate(matte, matte, kernel);
        else cv::erode(matte, matte, kernel);
    }
    const float clipRange = std::max(impl_->whiteClip - impl_->blackClip,
                                     1.0e-5f);
    matte = (matte - impl_->blackClip) / clipRange;
    cv::min(matte, 1.0, matte);
    cv::max(matte, 0.0, matte);
    if (impl_->feather > 0.01f) {
        cv::GaussianBlur(matte, matte, cv::Size(), impl_->feather);
    }

    cv::Mat rgb;
    cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, rgb);
    cv::Mat alphaRgb;
    cv::merge(std::vector<cv::Mat>{channels[3], channels[3], channels[3]},
              alphaRgb);
    cv::Mat weightedRgb;
    cv::multiply(rgb, alphaRgb, weightedRgb);
    cv::Mat blurredWeighted;
    cv::Mat blurredAlpha;
    const float colorSearchRadius = std::max(
        impl_->decontaminateRadius, std::max(0.0f, impl_->growShrink) + 1.0f);
    cv::GaussianBlur(weightedRgb, blurredWeighted, cv::Size(),
                     std::max(0.1f, colorSearchRadius));
    cv::GaussianBlur(channels[3], blurredAlpha, cv::Size(),
                     std::max(0.1f, colorSearchRadius));

    auto result = image.DeepCopy();
    float* output = result.rgba32fData();
    for (int y = 0; y < height; ++y) {
        const float* matteRow = matte.ptr<float>(y);
        const float* alphaBlurRow = blurredAlpha.ptr<float>(y);
        const cv::Vec3f* weightedRow = blurredWeighted.ptr<cv::Vec3f>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            const float refinedAlpha = std::clamp(matteRow[x], 0.0f, 1.0f);
            if (impl_->mattePreview) {
                output[offset] = refinedAlpha;
                output[offset + 1] = refinedAlpha;
                output[offset + 2] = refinedAlpha;
                output[offset + 3] = 1.0f;
                continue;
            }
            const float edge = std::clamp(1.0f - std::abs(refinedAlpha * 2.0f - 1.0f),
                                          0.0f, 1.0f);
            const float alphaGrowth = std::max(0.0f,
                refinedAlpha - std::clamp(source[offset + 3], 0.0f, 1.0f));
            const float colorAmount = std::clamp(
                std::max(edge * impl_->decontaminate, alphaGrowth) * impl_->mix,
                0.0f, 1.0f);
            const float denominator = std::max(alphaBlurRow[x], 1.0e-5f);
            for (int channel = 0; channel < 3; ++channel) {
                const float cleanColor = weightedRow[x][channel] / denominator;
                output[offset + channel] = std::lerp(source[offset + channel],
                                                     cleanColor, colorAmount);
            }
            output[offset + 3] = std::lerp(source[offset + 3], refinedAlpha,
                                           impl_->mix);
        }
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty>
ArtifactMatteRefineEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    addCreativeFloat(properties, "growShrink", "Grow / Shrink", impl_->growShrink, -32.0f, 32.0f);
    addCreativeFloat(properties, "feather", "Feather", impl_->feather, 0.0f, 64.0f);
    addCreativeFloat(properties, "blackClip", "Black Clip", impl_->blackClip, 0.0f, 0.99f);
    addCreativeFloat(properties, "whiteClip", "White Clip", impl_->whiteClip, 0.01f, 1.0f);
    addCreativeFloat(properties, "decontaminate", "Edge Decontamination", impl_->decontaminate, 0.0f, 1.0f);
    addCreativeFloat(properties, "decontaminateRadius", "Decontamination Radius", impl_->decontaminateRadius, 0.1f, 64.0f);
    addCreativeFloat(properties, "mix", "Mix", impl_->mix, 0.0f, 1.0f);
    addCreativeBoolean(properties, "mattePreview", "Matte Preview", impl_->mattePreview);
    addCreativeString(properties, "matteInput", "Matte Input", impl_->matteInput);
    return properties;
}

void ArtifactMatteRefineEffect::setPropertyValue(
    const ArtifactCore::UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    const float raw = value.toFloat();
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("growShrink")) impl_->growShrink = std::clamp(number, -32.0f, 32.0f);
    else if (key == QStringLiteral("feather")) impl_->feather = std::clamp(number, 0.0f, 64.0f);
    else if (key == QStringLiteral("blackClip")) impl_->blackClip = std::clamp(number, 0.0f, 0.99f);
    else if (key == QStringLiteral("whiteClip")) impl_->whiteClip = std::clamp(number, 0.01f, 1.0f);
    else if (key == QStringLiteral("decontaminate")) impl_->decontaminate = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("decontaminateRadius")) impl_->decontaminateRadius = std::clamp(number, 0.1f, 64.0f);
    else if (key == QStringLiteral("mix")) impl_->mix = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("mattePreview")) impl_->mattePreview = value.toBool();
    else if (key == QStringLiteral("matteInput")) impl_->matteInput = value.toString();
    else ArtifactAbstractEffect::setPropertyValue(name, value);
    if (impl_->whiteClip <= impl_->blackClip) {
        impl_->whiteClip = std::min(1.0f, impl_->blackClip + 0.01f);
        impl_->blackClip = std::max(0.0f, impl_->whiteClip - 0.01f);
    }
}

EffectROIHint ArtifactMatteRefineEffect::roiHint() const {
    return {.kind = EffectROIHintKind::Matte,
            .expansionPixels = std::abs(impl_->growShrink) +
                               std::max(impl_->feather,
                                        impl_->decontaminateRadius) * 3.0f};
}

class ArtifactSpillKillerProEffect::Impl {
public:
    float amount = 1.0f;
    float threshold = 0.02f;
    float softness = 0.08f;
    float edgeBias = 0.65f;
    float preserveLuma = 0.8f;
    float neutralBias = 0.0f;
    float mix = 1.0f;
    int keyColor = 0;
};

ArtifactSpillKillerProEffect::ArtifactSpillKillerProEffect() : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.spill_killer_pro"));
    setDisplayName(ArtifactCore::UniString("Spill Killer Pro"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

ArtifactSpillKillerProEffect::~ArtifactSpillKillerProEffect() {
    delete impl_;
    impl_ = nullptr;
}

void ArtifactSpillKillerProEffect::apply(const ImageF32x4RGBAWithCache& src,
                                         ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) {
        dst = src;
        return;
    }
    auto result = image.DeepCopy();
    float* output = result.rgba32fData();
    const int keyChannel = impl_->keyColor == 1 ? 2 : 1;
    const int firstOther = keyChannel == 0 ? 1 : 0;
    const int secondOther = keyChannel == 2 ? 1 : 2;
    const std::size_t pixels = image.totalPixels();
    for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
        const std::size_t offset = pixel * 4u;
        const float rgb[3] = {source[offset], source[offset + 1],
                              source[offset + 2]};
        const float opponent = std::max(rgb[firstOther], rgb[secondOther]);
        const float excess = rgb[keyChannel] - opponent - impl_->threshold;
        const float detection = smoothMask(excess /
            std::max(impl_->softness, 1.0e-5f));
        const float edgeWeight = std::lerp(1.0f, 1.0f - source[offset + 3],
                                           impl_->edgeBias);
        const float correction = std::max(0.0f, excess) * detection *
                                 edgeWeight * impl_->amount * impl_->mix;
        float corrected[3] = {rgb[0], rgb[1], rgb[2]};
        corrected[keyChannel] -= correction;
        const float split = std::clamp(0.5f + impl_->neutralBias * 0.5f,
                                       0.0f, 1.0f);
        corrected[firstOther] += correction * split * 0.35f;
        corrected[secondOther] += correction * (1.0f - split) * 0.35f;
        const float originalLuma = rgb[0] * 0.2126f + rgb[1] * 0.7152f +
                                   rgb[2] * 0.0722f;
        const float correctedLuma = corrected[0] * 0.2126f +
                                    corrected[1] * 0.7152f +
                                    corrected[2] * 0.0722f;
        const float lumaRestore = (originalLuma - correctedLuma) *
                                  impl_->preserveLuma;
        for (int channel = 0; channel < 3; ++channel) {
            output[offset + channel] = corrected[channel] + lumaRestore;
        }
        output[offset + 3] = source[offset + 3];
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty>
ArtifactSpillKillerProEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    addCreativeInteger(properties, "keyColor", "Key Color", impl_->keyColor, 0, 1);
    addCreativeFloat(properties, "amount", "Suppression", impl_->amount, 0.0f, 2.0f);
    addCreativeFloat(properties, "threshold", "Spill Threshold", impl_->threshold, 0.0f, 1.0f);
    addCreativeFloat(properties, "softness", "Detection Softness", impl_->softness, 0.001f, 1.0f);
    addCreativeFloat(properties, "edgeBias", "Edge Bias", impl_->edgeBias, 0.0f, 1.0f);
    addCreativeFloat(properties, "preserveLuma", "Preserve Luminance", impl_->preserveLuma, 0.0f, 1.0f);
    addCreativeFloat(properties, "neutralBias", "Neutral Color Bias", impl_->neutralBias, -1.0f, 1.0f);
    addCreativeFloat(properties, "mix", "Mix", impl_->mix, 0.0f, 1.0f);
    return properties;
}

void ArtifactSpillKillerProEffect::setPropertyValue(
    const ArtifactCore::UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    const float raw = value.toFloat();
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("keyColor")) impl_->keyColor = std::clamp(value.toInt(), 0, 1);
    else if (key == QStringLiteral("amount")) impl_->amount = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("threshold")) impl_->threshold = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("softness")) impl_->softness = std::clamp(number, 0.001f, 1.0f);
    else if (key == QStringLiteral("edgeBias")) impl_->edgeBias = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("preserveLuma")) impl_->preserveLuma = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("neutralBias")) impl_->neutralBias = std::clamp(number, -1.0f, 1.0f);
    else if (key == QStringLiteral("mix")) impl_->mix = std::clamp(number, 0.0f, 1.0f);
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint ArtifactSpillKillerProEffect::roiHint() const {
    return {.kind = EffectROIHintKind::Matte};
}

class ArtifactPixelDustFixerEffect::Impl {
public:
    float threshold = 0.12f;
    float softness = 0.05f;
    float amount = 1.0f;
    float mix = 1.0f;
    int radius = 1;
    bool repairBright = true;
    bool repairDark = true;
    bool maskOnly = false;
    QString maskInput = QStringLiteral("repair_mask");
    IEffectFrameSampler* sampler = nullptr;
    std::int64_t frame = 0;
};

ArtifactPixelDustFixerEffect::ArtifactPixelDustFixerEffect() : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.pixel_dust_fixer"));
    setDisplayName(ArtifactCore::UniString("Pixel / Dust Fixer"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

ArtifactPixelDustFixerEffect::~ArtifactPixelDustFixerEffect() {
    delete impl_;
    impl_ = nullptr;
}

void ArtifactPixelDustFixerEffect::onContextUpdated(const EffectContext& context) {
    impl_->sampler = context.sampler;
    impl_->frame = context.compositionFrame;
}

void ArtifactPixelDustFixerEffect::apply(const ImageF32x4RGBAWithCache& src,
                                         ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) {
        dst = src;
        return;
    }
    cv::Mat rgba(height, width, CV_32FC4, const_cast<float*>(source));
    std::vector<cv::Mat> channels;
    cv::split(rgba, channels);
    std::vector<cv::Mat> medianChannels(3);
    const int kernel = impl_->radius * 2 + 1;
    for (int channel = 0; channel < 3; ++channel) {
        cv::medianBlur(channels[channel], medianChannels[channel], kernel);
    }
    cv::Mat luma = channels[0] * 0.2126f + channels[1] * 0.7152f +
                   channels[2] * 0.0722f;
    cv::Mat medianLuma = medianChannels[0] * 0.2126f +
                         medianChannels[1] * 0.7152f +
                         medianChannels[2] * 0.0722f;
    cv::Mat repairMask(height, width, CV_32F, cv::Scalar(0.0f));
    for (int y = 0; y < height; ++y) {
        const float* lumaRow = luma.ptr<float>(y);
        const float* medianRow = medianLuma.ptr<float>(y);
        float* maskRow = repairMask.ptr<float>(y);
        for (int x = 0; x < width; ++x) {
            const float residual = lumaRow[x] - medianRow[x];
            const bool eligible = (residual >= 0.0f && impl_->repairBright) ||
                                  (residual < 0.0f && impl_->repairDark);
            if (!eligible || impl_->maskOnly) continue;
            const float normalized = (std::abs(residual) - impl_->threshold) /
                                     std::max(impl_->softness, 1.0e-5f);
            maskRow[x] = smoothMask(normalized);
        }
    }
    ImageF32x4RGBAWithCache maskImage;
    cv::Mat maskRgba;
    if (impl_->sampler && !impl_->maskInput.trimmed().isEmpty() &&
        impl_->sampler->sampleNamedInput(impl_->maskInput, impl_->frame,
                                         maskImage) &&
        prepareAuxiliaryImage(maskImage, width, height, maskRgba)) {
        cv::Mat namedMask = extractAuxiliaryMask(maskRgba);
        if (impl_->maskOnly) repairMask = namedMask;
        else cv::max(repairMask, namedMask, repairMask);
    }

    auto result = image.DeepCopy();
    float* output = result.rgba32fData();
    for (int y = 0; y < height; ++y) {
        const float* maskRow = repairMask.ptr<float>(y);
        const float* medianRows[3] = {medianChannels[0].ptr<float>(y),
                                      medianChannels[1].ptr<float>(y),
                                      medianChannels[2].ptr<float>(y)};
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            const float blend = std::clamp(maskRow[x] * impl_->amount *
                                           impl_->mix, 0.0f, 1.0f);
            for (int channel = 0; channel < 3; ++channel) {
                output[offset + channel] = std::lerp(source[offset + channel],
                                                     medianRows[channel][x], blend);
            }
            output[offset + 3] = source[offset + 3];
        }
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty>
ArtifactPixelDustFixerEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    addCreativeInteger(properties, "radius", "Repair Radius", impl_->radius, 1, 2);
    addCreativeFloat(properties, "threshold", "Detection Threshold", impl_->threshold, 0.0f, 2.0f);
    addCreativeFloat(properties, "softness", "Detection Softness", impl_->softness, 0.001f, 1.0f);
    addCreativeFloat(properties, "amount", "Repair Amount", impl_->amount, 0.0f, 2.0f);
    addCreativeFloat(properties, "mix", "Mix", impl_->mix, 0.0f, 1.0f);
    addCreativeBoolean(properties, "repairBright", "Repair Bright Pixels", impl_->repairBright);
    addCreativeBoolean(properties, "repairDark", "Repair Dark Pixels", impl_->repairDark);
    addCreativeBoolean(properties, "maskOnly", "Use Mask Only", impl_->maskOnly);
    addCreativeString(properties, "maskInput", "Repair Mask Input", impl_->maskInput);
    return properties;
}

void ArtifactPixelDustFixerEffect::setPropertyValue(
    const ArtifactCore::UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    const float raw = value.toFloat();
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("radius")) impl_->radius = std::clamp(value.toInt(), 1, 2);
    else if (key == QStringLiteral("threshold")) impl_->threshold = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("softness")) impl_->softness = std::clamp(number, 0.001f, 1.0f);
    else if (key == QStringLiteral("amount")) impl_->amount = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("mix")) impl_->mix = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("repairBright")) impl_->repairBright = value.toBool();
    else if (key == QStringLiteral("repairDark")) impl_->repairDark = value.toBool();
    else if (key == QStringLiteral("maskOnly")) impl_->maskOnly = value.toBool();
    else if (key == QStringLiteral("maskInput")) impl_->maskInput = value.toString();
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint ArtifactPixelDustFixerEffect::roiHint() const {
    return {.kind = EffectROIHintKind::Matte,
            .expansionPixels = static_cast<float>(impl_->radius * 2 + 1)};
}

class ArtifactReflectionComposerEffect::Impl {
public:
    float horizon = 0.62f;
    float distance = 0.0f;
    float length = 0.85f;
    float opacity = 0.55f;
    float fade = 0.45f;
    float softness = 3.0f;
    float offsetX = 0.0f;
    float rippleAmplitude = 1.5f;
    float rippleFrequency = 0.035f;
    float rippleSpeed = 0.5f;
    float red = 0.92f;
    float green = 0.96f;
    float blue = 1.0f;
    float mix = 1.0f;
    double time = 0.0;
};

ArtifactReflectionComposerEffect::ArtifactReflectionComposerEffect()
    : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.reflection_composer"));
    setDisplayName(ArtifactCore::UniString("Reflection Composer"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setAllowOverscan(true);
}

ArtifactReflectionComposerEffect::~ArtifactReflectionComposerEffect() {
    delete impl_;
    impl_ = nullptr;
}

void ArtifactReflectionComposerEffect::onContextUpdated(
    const EffectContext& context) {
    impl_->time = context.timeSeconds;
}

void ArtifactReflectionComposerEffect::apply(
    const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) {
        dst = src;
        return;
    }
    cv::Mat reflection(height, width, CV_32FC4, cv::Scalar(0.0f, 0.0f, 0.0f, 0.0f));
    const float horizonY = impl_->horizon * (height - 1) + impl_->distance;
    const float fadeDistance = std::max(1.0f, impl_->fade * height);
    const float tint[3] = {impl_->red, impl_->green, impl_->blue};
    for (int y = std::max(0, static_cast<int>(std::floor(horizonY)));
         y < height; ++y) {
        const float dy = y - horizonY;
        const float sourceY = horizonY - dy / std::max(impl_->length, 0.05f);
        if (sourceY < 0.0f || sourceY >= height) continue;
        cv::Vec4f* row = reflection.ptr<cv::Vec4f>(y);
        const float fade = std::exp(-dy / fadeDistance) * impl_->opacity;
        const float ripple = std::sin(y * impl_->rippleFrequency +
                                      static_cast<float>(impl_->time *
                                                         impl_->rippleSpeed * 6.2831853)) *
                             impl_->rippleAmplitude;
        for (int x = 0; x < width; ++x) {
            const float sourceX = x + impl_->offsetX + ripple;
            const float reflectedAlpha = std::clamp(sampleHeatwaveChannel(
                source, width, height, sourceX, sourceY, 3) * fade,
                0.0f, 1.0f);
            for (int channel = 0; channel < 3; ++channel) {
                row[x][channel] = sampleHeatwaveChannel(
                    source, width, height, sourceX, sourceY, channel) *
                    tint[channel] * reflectedAlpha;
            }
            row[x][3] = reflectedAlpha;
        }
    }
    if (impl_->softness > 0.01f) {
        cv::GaussianBlur(reflection, reflection, cv::Size(), impl_->softness);
    }
    auto result = image.DeepCopy();
    float* output = result.rgba32fData();
    for (int y = 0; y < height; ++y) {
        const cv::Vec4f* reflectionRow = reflection.ptr<cv::Vec4f>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            const float reflectionAlpha = std::clamp(
                reflectionRow[x][3] * impl_->mix, 0.0f, 1.0f);
            const float sourceAlpha = std::clamp(source[offset + 3], 0.0f, 1.0f);
            const float available = 1.0f - sourceAlpha;
            const float outputAlpha = sourceAlpha + reflectionAlpha * available;
            for (int channel = 0; channel < 3; ++channel) {
                const float premultiplied = source[offset + channel] * sourceAlpha +
                    reflectionRow[x][channel] * impl_->mix * available;
                output[offset + channel] = outputAlpha > 1.0e-6f
                    ? premultiplied / outputAlpha : 0.0f;
            }
            output[offset + 3] = outputAlpha;
        }
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty>
ArtifactReflectionComposerEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    addCreativeFloat(properties, "horizon", "Reflection Horizon", impl_->horizon, 0.0f, 1.0f);
    addCreativeFloat(properties, "distance", "Distance", impl_->distance, -1024.0f, 1024.0f);
    addCreativeFloat(properties, "length", "Reflection Length", impl_->length, 0.05f, 4.0f);
    addCreativeFloat(properties, "opacity", "Opacity", impl_->opacity, 0.0f, 2.0f);
    addCreativeFloat(properties, "fade", "Distance Fade", impl_->fade, 0.01f, 2.0f);
    addCreativeFloat(properties, "softness", "Softness", impl_->softness, 0.0f, 64.0f);
    addCreativeFloat(properties, "offsetX", "Horizontal Offset", impl_->offsetX, -2048.0f, 2048.0f);
    addCreativeFloat(properties, "rippleAmplitude", "Ripple Amount", impl_->rippleAmplitude, 0.0f, 64.0f);
    addCreativeFloat(properties, "rippleFrequency", "Ripple Frequency", impl_->rippleFrequency, 0.0f, 1.0f);
    addCreativeFloat(properties, "rippleSpeed", "Ripple Speed", impl_->rippleSpeed, -8.0f, 8.0f);
    addCreativeFloat(properties, "red", "Tint Red", impl_->red, 0.0f, 2.0f);
    addCreativeFloat(properties, "green", "Tint Green", impl_->green, 0.0f, 2.0f);
    addCreativeFloat(properties, "blue", "Tint Blue", impl_->blue, 0.0f, 2.0f);
    addCreativeFloat(properties, "mix", "Mix", impl_->mix, 0.0f, 1.0f);
    return properties;
}

void ArtifactReflectionComposerEffect::setPropertyValue(
    const ArtifactCore::UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    const float raw = value.toFloat();
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("horizon")) impl_->horizon = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("distance")) impl_->distance = std::clamp(number, -1024.0f, 1024.0f);
    else if (key == QStringLiteral("length")) impl_->length = std::clamp(number, 0.05f, 4.0f);
    else if (key == QStringLiteral("opacity")) impl_->opacity = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("fade")) impl_->fade = std::clamp(number, 0.01f, 2.0f);
    else if (key == QStringLiteral("softness")) impl_->softness = std::clamp(number, 0.0f, 64.0f);
    else if (key == QStringLiteral("offsetX")) impl_->offsetX = std::clamp(number, -2048.0f, 2048.0f);
    else if (key == QStringLiteral("rippleAmplitude")) impl_->rippleAmplitude = std::clamp(number, 0.0f, 64.0f);
    else if (key == QStringLiteral("rippleFrequency")) impl_->rippleFrequency = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("rippleSpeed")) impl_->rippleSpeed = std::clamp(number, -8.0f, 8.0f);
    else if (key == QStringLiteral("red")) impl_->red = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("green")) impl_->green = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("blue")) impl_->blue = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("mix")) impl_->mix = std::clamp(number, 0.0f, 1.0f);
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint ArtifactReflectionComposerEffect::roiHint() const {
    return {.kind = EffectROIHintKind::Displacement,
            .expansionPixels = std::max(std::abs(impl_->offsetX) +
                                        impl_->rippleAmplitude,
                                        impl_->softness * 3.0f),
            .requiresFullFrame = true};
}

class ArtifactLensProfileMatcherEffect::Impl {
public:
    float sourceK1 = 0.0f;
    float sourceK2 = 0.0f;
    float targetK1 = 0.0f;
    float targetK2 = 0.0f;
    float centerX = 0.5f;
    float centerY = 0.5f;
    float zoom = 1.0f;
    float chromatic = 0.0f;
    float mix = 1.0f;
    int edgeMode = 1;
};

ArtifactLensProfileMatcherEffect::ArtifactLensProfileMatcherEffect()
    : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.lens_profile_matcher"));
    setDisplayName(ArtifactCore::UniString("Lens Profile Matcher"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setAllowOverscan(true);
}

ArtifactLensProfileMatcherEffect::~ArtifactLensProfileMatcherEffect() {
    delete impl_;
    impl_ = nullptr;
}

void ArtifactLensProfileMatcherEffect::apply(
    const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) {
        dst = src;
        return;
    }
    const float centerX = impl_->centerX * (width - 1);
    const float centerY = impl_->centerY * (height - 1);
    const float radiusScale = std::max(1.0f, 0.5f * std::min(width, height));
    const auto sample = [&](float x, float y, int channel) {
        if (impl_->edgeMode == 0 &&
            (x < 0.0f || y < 0.0f || x > width - 1 || y > height - 1)) {
            return 0.0f;
        }
        return sampleHeatwaveChannel(source, width, height, x, y, channel);
    };
    auto result = image.DeepCopy();
    float* output = result.rgba32fData();
    const float deltaK1 = impl_->targetK1 - impl_->sourceK1;
    const float deltaK2 = impl_->targetK2 - impl_->sourceK2;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float nx = (x - centerX) / radiusScale;
            const float ny = (y - centerY) / radiusScale;
            const float radius2 = nx * nx + ny * ny;
            const float baseFactor = (1.0f + deltaK1 * radius2 +
                                      deltaK2 * radius2 * radius2) /
                                     std::max(impl_->zoom, 0.01f);
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            for (int channel = 0; channel < 3; ++channel) {
                const float colorOffset = (channel - 1) * impl_->chromatic *
                                          radius2 * 0.01f;
                const float factor = baseFactor + colorOffset;
                const float sourceX = centerX + nx * factor * radiusScale;
                const float sourceY = centerY + ny * factor * radiusScale;
                const float warped = sample(sourceX, sourceY, channel);
                output[offset + channel] = std::lerp(source[offset + channel],
                                                     warped, impl_->mix);
            }
            const float sourceX = centerX + nx * baseFactor * radiusScale;
            const float sourceY = centerY + ny * baseFactor * radiusScale;
            output[offset + 3] = std::lerp(source[offset + 3],
                                           sample(sourceX, sourceY, 3),
                                           impl_->mix);
        }
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty>
ArtifactLensProfileMatcherEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    addCreativeFloat(properties, "sourceK1", "Source Distortion K1", impl_->sourceK1, -2.0f, 2.0f);
    addCreativeFloat(properties, "sourceK2", "Source Distortion K2", impl_->sourceK2, -2.0f, 2.0f);
    addCreativeFloat(properties, "targetK1", "Target Distortion K1", impl_->targetK1, -2.0f, 2.0f);
    addCreativeFloat(properties, "targetK2", "Target Distortion K2", impl_->targetK2, -2.0f, 2.0f);
    addCreativeFloat(properties, "centerX", "Optical Center X", impl_->centerX, 0.0f, 1.0f);
    addCreativeFloat(properties, "centerY", "Optical Center Y", impl_->centerY, 0.0f, 1.0f);
    addCreativeFloat(properties, "zoom", "Zoom Compensation", impl_->zoom, 0.1f, 4.0f);
    addCreativeFloat(properties, "chromatic", "Lateral Chromatic Match", impl_->chromatic, -10.0f, 10.0f);
    addCreativeFloat(properties, "mix", "Mix", impl_->mix, 0.0f, 1.0f);
    addCreativeInteger(properties, "edgeMode", "Edge Mode", impl_->edgeMode, 0, 1);
    return properties;
}

void ArtifactLensProfileMatcherEffect::setPropertyValue(
    const ArtifactCore::UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    const float raw = value.toFloat();
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("sourceK1")) impl_->sourceK1 = std::clamp(number, -2.0f, 2.0f);
    else if (key == QStringLiteral("sourceK2")) impl_->sourceK2 = std::clamp(number, -2.0f, 2.0f);
    else if (key == QStringLiteral("targetK1")) impl_->targetK1 = std::clamp(number, -2.0f, 2.0f);
    else if (key == QStringLiteral("targetK2")) impl_->targetK2 = std::clamp(number, -2.0f, 2.0f);
    else if (key == QStringLiteral("centerX")) impl_->centerX = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("centerY")) impl_->centerY = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("zoom")) impl_->zoom = std::clamp(number, 0.1f, 4.0f);
    else if (key == QStringLiteral("chromatic")) impl_->chromatic = std::clamp(number, -10.0f, 10.0f);
    else if (key == QStringLiteral("mix")) impl_->mix = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("edgeMode")) impl_->edgeMode = std::clamp(value.toInt(), 0, 1);
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint ArtifactLensProfileMatcherEffect::roiHint() const {
    return {.kind = EffectROIHintKind::Displacement,
            .expansionFraction = std::min(2.0f,
                std::abs(impl_->targetK1 - impl_->sourceK1) +
                std::abs(impl_->targetK2 - impl_->sourceK2)),
            .requiresFullFrame = true};
}

class ArtifactAtmosphericDepthEffect::Impl {
public:
    float nearDepth = 0.15f;
    float farDepth = 0.9f;
    float density = 1.1f;
    float curve = 1.5f;
    float depthSoftness = 1.5f;
    float red = 0.62f;
    float green = 0.72f;
    float blue = 0.82f;
    float glow = 0.18f;
    float glowThreshold = 0.75f;
    float glowRadius = 12.0f;
    float mix = 1.0f;
    bool invertDepth = false;
    bool useLumaFallback = true;
    QString depthInput = QStringLiteral("depth");
    IEffectFrameSampler* sampler = nullptr;
    std::int64_t frame = 0;
};

ArtifactAtmosphericDepthEffect::ArtifactAtmosphericDepthEffect()
    : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.atmospheric_depth"));
    setDisplayName(ArtifactCore::UniString("Atmospheric Depth"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setAllowOverscan(true);
}

ArtifactAtmosphericDepthEffect::~ArtifactAtmosphericDepthEffect() {
    delete impl_;
    impl_ = nullptr;
}

void ArtifactAtmosphericDepthEffect::onContextUpdated(
    const EffectContext& context) {
    impl_->sampler = context.sampler;
    impl_->frame = context.compositionFrame;
}

void ArtifactAtmosphericDepthEffect::apply(
    const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) {
        dst = src;
        return;
    }
    cv::Mat depth(height, width, CV_32F);
    bool hasDepth = false;
    ImageF32x4RGBAWithCache depthImage;
    cv::Mat depthRgba;
    if (impl_->sampler && !impl_->depthInput.trimmed().isEmpty() &&
        impl_->sampler->sampleNamedInput(impl_->depthInput, impl_->frame,
                                         depthImage) &&
        prepareAuxiliaryImage(depthImage, width, height, depthRgba)) {
        std::vector<cv::Mat> channels;
        cv::split(depthRgba, channels);
        depth = channels[0].clone();
        hasDepth = true;
    } else if (impl_->useLumaFallback) {
        for (int y = 0; y < height; ++y) {
            float* row = depth.ptr<float>(y);
            for (int x = 0; x < width; ++x) {
                const std::size_t offset =
                    (static_cast<std::size_t>(y) * width + x) * 4u;
                row[x] = source[offset] * 0.2126f +
                         source[offset + 1] * 0.7152f +
                         source[offset + 2] * 0.0722f;
            }
        }
        hasDepth = true;
    }
    if (!hasDepth) {
        dst = src;
        return;
    }
    cv::min(depth, 1.0, depth);
    cv::max(depth, 0.0, depth);
    if (impl_->invertDepth) depth = 1.0f - depth;
    if (impl_->depthSoftness > 0.01f) {
        cv::GaussianBlur(depth, depth, cv::Size(), impl_->depthSoftness);
    }

    cv::Mat highlights(height, width, CV_32F);
    for (int y = 0; y < height; ++y) {
        float* row = highlights.ptr<float>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            const float luma = source[offset] * 0.2126f +
                               source[offset + 1] * 0.7152f +
                               source[offset + 2] * 0.0722f;
            row[x] = std::max(0.0f, luma - impl_->glowThreshold);
        }
    }
    cv::GaussianBlur(highlights, highlights, cv::Size(),
                     std::max(0.1f, impl_->glowRadius));
    const float fogColor[3] = {impl_->red, impl_->green, impl_->blue};
    const float depthRange = std::max(impl_->farDepth - impl_->nearDepth,
                                      1.0e-5f);
    auto result = image.DeepCopy();
    float* output = result.rgba32fData();
    for (int y = 0; y < height; ++y) {
        const float* depthRow = depth.ptr<float>(y);
        const float* highlightRow = highlights.ptr<float>(y);
        for (int x = 0; x < width; ++x) {
            float normalized = std::clamp((depthRow[x] - impl_->nearDepth) /
                                          depthRange, 0.0f, 1.0f);
            normalized = std::pow(normalized, impl_->curve);
            const float fog = (1.0f - std::exp(-impl_->density * normalized)) *
                              impl_->mix;
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            for (int channel = 0; channel < 3; ++channel) {
                const float atmospheric = fogColor[channel] +
                    highlightRow[x] * impl_->glow;
                output[offset + channel] = std::lerp(source[offset + channel],
                                                     atmospheric, fog);
            }
            output[offset + 3] = source[offset + 3];
        }
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty>
ArtifactAtmosphericDepthEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    addCreativeFloat(properties, "nearDepth", "Near Depth", impl_->nearDepth, 0.0f, 0.99f);
    addCreativeFloat(properties, "farDepth", "Far Depth", impl_->farDepth, 0.01f, 1.0f);
    addCreativeFloat(properties, "density", "Atmosphere Density", impl_->density, 0.0f, 8.0f);
    addCreativeFloat(properties, "curve", "Depth Curve", impl_->curve, 0.1f, 8.0f);
    addCreativeFloat(properties, "depthSoftness", "Depth Softness", impl_->depthSoftness, 0.0f, 32.0f);
    addCreativeFloat(properties, "red", "Atmosphere Red", impl_->red, 0.0f, 4.0f);
    addCreativeFloat(properties, "green", "Atmosphere Green", impl_->green, 0.0f, 4.0f);
    addCreativeFloat(properties, "blue", "Atmosphere Blue", impl_->blue, 0.0f, 4.0f);
    addCreativeFloat(properties, "glow", "Aerial Glow", impl_->glow, 0.0f, 4.0f);
    addCreativeFloat(properties, "glowThreshold", "Glow Threshold", impl_->glowThreshold, 0.0f, 4.0f);
    addCreativeFloat(properties, "glowRadius", "Glow Radius", impl_->glowRadius, 0.1f, 256.0f);
    addCreativeFloat(properties, "mix", "Mix", impl_->mix, 0.0f, 1.0f);
    addCreativeBoolean(properties, "invertDepth", "Invert Depth", impl_->invertDepth);
    addCreativeBoolean(properties, "useLumaFallback", "Use Luma Fallback", impl_->useLumaFallback);
    addCreativeString(properties, "depthInput", "Depth Input", impl_->depthInput);
    return properties;
}

void ArtifactAtmosphericDepthEffect::setPropertyValue(
    const ArtifactCore::UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    const float raw = value.toFloat();
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("nearDepth")) impl_->nearDepth = std::clamp(number, 0.0f, 0.99f);
    else if (key == QStringLiteral("farDepth")) impl_->farDepth = std::clamp(number, 0.01f, 1.0f);
    else if (key == QStringLiteral("density")) impl_->density = std::clamp(number, 0.0f, 8.0f);
    else if (key == QStringLiteral("curve")) impl_->curve = std::clamp(number, 0.1f, 8.0f);
    else if (key == QStringLiteral("depthSoftness")) impl_->depthSoftness = std::clamp(number, 0.0f, 32.0f);
    else if (key == QStringLiteral("red")) impl_->red = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("green")) impl_->green = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("blue")) impl_->blue = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("glow")) impl_->glow = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("glowThreshold")) impl_->glowThreshold = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("glowRadius")) impl_->glowRadius = std::clamp(number, 0.1f, 256.0f);
    else if (key == QStringLiteral("mix")) impl_->mix = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("invertDepth")) impl_->invertDepth = value.toBool();
    else if (key == QStringLiteral("useLumaFallback")) impl_->useLumaFallback = value.toBool();
    else if (key == QStringLiteral("depthInput")) impl_->depthInput = value.toString();
    else ArtifactAbstractEffect::setPropertyValue(name, value);
    if (impl_->farDepth <= impl_->nearDepth) {
        impl_->farDepth = std::min(1.0f, impl_->nearDepth + 0.01f);
        impl_->nearDepth = std::max(0.0f, impl_->farDepth - 0.01f);
    }
}

EffectROIHint ArtifactAtmosphericDepthEffect::roiHint() const {
    return {.kind = EffectROIHintKind::Glow,
            .expansionPixels = std::max(impl_->depthSoftness,
                                        impl_->glowRadius) * 3.0f};
}

class ArtifactEdgeColorCompositeEffect::Impl {
public:
    float edgeWidth = 4.0f;
    float softness = 2.0f;
    float backgroundBlur = 3.0f;
    float luminanceMatch = 0.75f;
    float chromaMatch = 0.55f;
    float decontaminate = 0.65f;
    float mix = 1.0f;
    bool edgePreview = false;
    QString backgroundInput = QStringLiteral("background");
    IEffectFrameSampler* sampler = nullptr;
    std::int64_t frame = 0;
};

ArtifactEdgeColorCompositeEffect::ArtifactEdgeColorCompositeEffect()
    : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.edge_color_composite"));
    setDisplayName(ArtifactCore::UniString("Edge Color Composite"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setAllowOverscan(true);
}

ArtifactEdgeColorCompositeEffect::~ArtifactEdgeColorCompositeEffect() {
    delete impl_;
    impl_ = nullptr;
}

void ArtifactEdgeColorCompositeEffect::onContextUpdated(
    const EffectContext& context) {
    impl_->sampler = context.sampler;
    impl_->frame = context.compositionFrame;
}

void ArtifactEdgeColorCompositeEffect::apply(
    const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    ImageF32x4RGBAWithCache backgroundImage;
    cv::Mat background;
    if (!source || width <= 0 || height <= 0 || !impl_->sampler ||
        impl_->backgroundInput.trimmed().isEmpty() ||
        !impl_->sampler->sampleNamedInput(impl_->backgroundInput, impl_->frame,
                                          backgroundImage) ||
        !prepareAuxiliaryImage(backgroundImage, width, height, background)) {
        dst = src;
        return;
    }
    cv::Mat foreground(height, width, CV_32FC4, const_cast<float*>(source));
    std::vector<cv::Mat> foregroundChannels;
    std::vector<cv::Mat> backgroundChannels;
    cv::split(foreground, foregroundChannels);
    cv::split(background, backgroundChannels);
    const int radius = std::clamp(static_cast<int>(std::round(impl_->edgeWidth)),
                                  1, 64);
    const cv::Mat kernel = cv::getStructuringElement(
        cv::MORPH_ELLIPSE, cv::Size(radius * 2 + 1, radius * 2 + 1));
    cv::Mat erodedAlpha;
    cv::erode(foregroundChannels[3], erodedAlpha, kernel);
    cv::Mat edge = foregroundChannels[3] - erodedAlpha;
    cv::max(edge, 0.0, edge);
    if (impl_->softness > 0.01f) {
        cv::GaussianBlur(edge, edge, cv::Size(), impl_->softness);
    }
    cv::Mat backgroundRgb;
    cv::merge(std::vector<cv::Mat>{backgroundChannels[0], backgroundChannels[1],
                                  backgroundChannels[2]}, backgroundRgb);
    if (impl_->backgroundBlur > 0.01f) {
        cv::GaussianBlur(backgroundRgb, backgroundRgb, cv::Size(),
                         impl_->backgroundBlur);
    }
    auto result = image.DeepCopy();
    float* output = result.rgba32fData();
    for (int y = 0; y < height; ++y) {
        const float* edgeRow = edge.ptr<float>(y);
        const cv::Vec3f* backgroundRow = backgroundRgb.ptr<cv::Vec3f>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            const float edgeAmount = std::clamp(edgeRow[x] * impl_->mix,
                                                0.0f, 1.0f);
            if (impl_->edgePreview) {
                output[offset] = edgeAmount;
                output[offset + 1] = edgeAmount;
                output[offset + 2] = edgeAmount;
                output[offset + 3] = 1.0f;
                continue;
            }
            const float sourceLuma = source[offset] * 0.2126f +
                                     source[offset + 1] * 0.7152f +
                                     source[offset + 2] * 0.0722f;
            const cv::Vec3f backgroundColor = backgroundRow[x];
            const float backgroundLuma = backgroundColor[0] * 0.2126f +
                                         backgroundColor[1] * 0.7152f +
                                         backgroundColor[2] * 0.0722f;
            for (int channel = 0; channel < 3; ++channel) {
                const float sourceChroma = source[offset + channel] - sourceLuma;
                const float backgroundChroma = backgroundColor[channel] -
                                               backgroundLuma;
                const float matched = source[offset + channel] +
                    (backgroundLuma - sourceLuma) * impl_->luminanceMatch +
                    (backgroundChroma - sourceChroma) * impl_->chromaMatch;
                const float decontaminated = std::lerp(
                    source[offset + channel], matched, impl_->decontaminate);
                output[offset + channel] = std::lerp(source[offset + channel],
                                                     decontaminated, edgeAmount);
            }
            output[offset + 3] = source[offset + 3];
        }
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty>
ArtifactEdgeColorCompositeEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    addCreativeFloat(properties, "edgeWidth", "Edge Width", impl_->edgeWidth, 1.0f, 64.0f);
    addCreativeFloat(properties, "softness", "Edge Softness", impl_->softness, 0.0f, 32.0f);
    addCreativeFloat(properties, "backgroundBlur", "Background Sample Blur", impl_->backgroundBlur, 0.0f, 64.0f);
    addCreativeFloat(properties, "luminanceMatch", "Luminance Match", impl_->luminanceMatch, 0.0f, 2.0f);
    addCreativeFloat(properties, "chromaMatch", "Chroma Match", impl_->chromaMatch, 0.0f, 2.0f);
    addCreativeFloat(properties, "decontaminate", "Edge Decontamination", impl_->decontaminate, 0.0f, 1.0f);
    addCreativeFloat(properties, "mix", "Mix", impl_->mix, 0.0f, 1.0f);
    addCreativeBoolean(properties, "edgePreview", "Edge Preview", impl_->edgePreview);
    addCreativeString(properties, "backgroundInput", "Background Input", impl_->backgroundInput);
    return properties;
}

void ArtifactEdgeColorCompositeEffect::setPropertyValue(
    const ArtifactCore::UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    const float raw = value.toFloat();
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("edgeWidth")) impl_->edgeWidth = std::clamp(number, 1.0f, 64.0f);
    else if (key == QStringLiteral("softness")) impl_->softness = std::clamp(number, 0.0f, 32.0f);
    else if (key == QStringLiteral("backgroundBlur")) impl_->backgroundBlur = std::clamp(number, 0.0f, 64.0f);
    else if (key == QStringLiteral("luminanceMatch")) impl_->luminanceMatch = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("chromaMatch")) impl_->chromaMatch = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("decontaminate")) impl_->decontaminate = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("mix")) impl_->mix = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("edgePreview")) impl_->edgePreview = value.toBool();
    else if (key == QStringLiteral("backgroundInput")) impl_->backgroundInput = value.toString();
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint ArtifactEdgeColorCompositeEffect::roiHint() const {
    return {.kind = EffectROIHintKind::Matte,
            .expansionPixels = impl_->edgeWidth +
                               std::max(impl_->softness,
                                        impl_->backgroundBlur) * 3.0f};
}

namespace {

bool runCreativeCompute(const ImageF32x4RGBAWithCache& src,
                        ImageF32x4RGBAWithCache& dst,
                        const char* label,
                        const char* hlsl) {
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    if (!acquireSharedRenderDeviceForCurrentBackend(device, context)) return false;
    const auto& image = src.image();
    const auto upload = ArtifactCore::makeGpuImageUploadBuffer(image.surfaceView());
    if (!upload.isValid() || image.width() <= 0 || image.height() <= 0) return false;

    Diligent::TextureDesc inputDesc;
    inputDesc.Name = label;
    inputDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    inputDesc.Width = image.width(); inputDesc.Height = image.height();
    inputDesc.Format = upload.format == ArtifactCore::GpuImageFormat::Rgba16Float
        ? Diligent::TEX_FORMAT_RGBA16_FLOAT
        : Diligent::TEX_FORMAT_RGBA32_FLOAT;
    inputDesc.MipLevels = 1; inputDesc.ArraySize = 1; inputDesc.SampleCount = 1;
    inputDesc.Usage = Diligent::USAGE_IMMUTABLE;
    inputDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData inputData{};
    inputData.pData = upload.bytes.data();
    inputData.Stride = upload.rowStride;
    Diligent::TextureData textureData{};
    textureData.pSubResources = &inputData;
    textureData.NumSubresources = 1;
    Diligent::RefCntAutoPtr<Diligent::ITexture> input;
    device->CreateTexture(inputDesc, &textureData, &input);
    if (!input) return false;

    Diligent::TextureDesc outputDesc = inputDesc;
    outputDesc.Name = label;
    outputDesc.Usage = Diligent::USAGE_DEFAULT;
    outputDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
    Diligent::RefCntAutoPtr<Diligent::ITexture> output;
    device->CreateTexture(outputDesc, nullptr, &output);
    if (!output) return false;

    static Diligent::ShaderResourceVariableDesc vars[] = {
        {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
    ArtifactCore::GpuContext gpuContext{device, context};
    ArtifactCore::ComputeExecutor executor{gpuContext};
    ArtifactCore::ComputePipelineDesc pipeline{};
    pipeline.name = label; pipeline.shaderSource = hlsl; pipeline.entryPoint = "main";
    pipeline.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    pipeline.variables = vars; pipeline.variableCount = 2;
    pipeline.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    if (!executor.build(pipeline) || !executor.createShaderResourceBinding(true) ||
        !executor.setTextureView("g_InputTexture", input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
        !executor.setTextureView("g_OutputTexture", output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) return false;
    executor.dispatch(context, ArtifactCore::ComputeExecutor::makeDispatchAttribs(outputDesc.Width, outputDesc.Height, 1, 8, 8, 1), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Diligent::TextureDesc stagingDesc = outputDesc;
    stagingDesc.Name = label; stagingDesc.Usage = Diligent::USAGE_STAGING;
    stagingDesc.BindFlags = Diligent::BIND_NONE; stagingDesc.CPUAccessFlags = Diligent::CPU_ACCESS_READ;
    Diligent::RefCntAutoPtr<Diligent::ITexture> staging;
    device->CreateTexture(stagingDesc, nullptr, &staging);
    if (!staging) return false;
    Diligent::CopyTextureAttribs copy(
        output, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context->CopyTexture(copy);
    context->Flush(); context->WaitForIdle();
    Diligent::MappedTextureSubresource mapped{};
    context->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, mapped);
    if (!mapped.pData || !mapped.Stride) return false;
    cv::Mat result(static_cast<int>(outputDesc.Height), static_cast<int>(outputDesc.Width), CV_32FC4, mapped.pData, mapped.Stride);
    auto outputDescriptor = image.colorDescriptor();
    outputDescriptor.channelOrder = ArtifactCore::SurfaceChannelOrder::RGBA;
    dst.image().setFromCVMat(result, outputDescriptor);
    context->UnmapTextureSubresource(staging, 0, 0);
    return true;
}

constexpr char kGlitchComputeHlsl[] = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
uint hash(uint v) { v ^= v >> 16; v *= 0x7feb352du; v ^= v >> 15; v *= 0x846ca68bu; return v ^ (v >> 16); }
float random01(uint x, uint y, uint salt) { return (hash(x * 73856093u ^ y * 19349663u ^ salt) & 0x00ffffffu) / 16777215.0f; }
[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) { uint w,h; g_OutputTexture.GetDimensions(w,h); if(id.x>=w||id.y>=h)return; float rowOffset=(id.y%15u<4u)?(random01(0u,id.y,1u)*24.0f-12.0f):0.0f; float shift=3.0f+random01(0u,id.y,2u)*5.0f; int sx=clamp((int)id.x+(int)rowOffset,0,(int)w-1); int rx=clamp(sx+(int)shift,0,(int)w-1); int bx=clamp(sx-(int)shift,0,(int)w-1); float4 c=g_InputTexture[uint2(sx,id.y)]; g_OutputTexture[id.xy]=float4(g_InputTexture[uint2(rx,id.y)].r,c.g,g_InputTexture[uint2(bx,id.y)].b,c.a); })";

constexpr char kOldTVComputeHlsl[] = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
uint hash(uint v) { v ^= v >> 16; v *= 0x7feb352du; v ^= v >> 15; v *= 0x846ca68bu; return v ^ (v >> 16); }
float random01(uint x,uint y,uint salt) { return (hash(x*73856093u ^ y*19349663u ^ salt) & 0x00ffffffu) / 16777215.0f; }
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID) { uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return; float scanline=(id.y%4u==0u)?0.7f:1.0f; float jitter=random01(0u,id.y,17u)<0.08f?(random01(0u,id.y,18u)*10.0f-5.0f):0.0f; int sx=clamp((int)id.x+(int)jitter,0,(int)w-1); float4 c=g_InputTexture[uint2(sx,id.y)]; float noise=(random01(id.x,id.y,19u)*2.0f-1.0f)*0.005f; g_OutputTexture[id.xy]=float4(saturate(c.rgb*scanline+noise),c.a); })";

constexpr char kHalftoneComputeHlsl[] = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID) { uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return; uint bx=(id.x/8u)*8u,by=(id.y/8u)*8u; float lum=0;uint count=0;[unroll]for(uint dy=0;dy<8u;++dy){[unroll]for(uint dx=0;dx<8u;++dx){uint x=bx+dx,y=by+dy;if(x<w&&y<h){float3 c=g_InputTexture[uint2(x,y)].rgb;lum+=(c.r+c.g+c.b)/3.0f;++count;}}}lum/=max(1u,count);float2 d=float2(id.xy)-float2(bx+4u,by+4u);float radius=4.0f*lum;float v=length(d)<radius?0.0f:1.0f;g_OutputTexture[id.xy]=float4(v,v,v,1); })";

} // namespace

ArtifactGlitchEffect::ArtifactGlitchEffect() {
    setDisplayName("Glitch");
    setEffectID("builtin.glitch");
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

void ArtifactGlitchEffect::apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    if (computeMode() != ComputeMode::CPU && runCreativeCompute(src, dst, "CreativeGlitch", kGlitchComputeHlsl)) {
        return;
    }
    int w = src.width();
    int h = src.height();
    auto srcImage = src.image();
    auto dstImage = srcImage.DeepCopy();
    const float* srcPixels = srcImage.rgba32fData();
    float* dstPixels = dstImage.rgba32fData();

    ArtifactCore::RandomStream rng(0x474C49544348ull);
    
    // Each row owns a disjoint destination range.  Keep the per-row RNG fork so
    // CPU and future GPU/reference comparisons remain deterministic.
    ArtifactCore::Parallel::For(0, h, w * h, [&](int y) {
        auto rowRng = rng.fork(static_cast<uint64_t>(y));
        float rowOffset = 0.0f;
        if (y % 15 < 4) {
            rowOffset = rowRng.range(-12.0f, 12.0f);
        }
        float shiftX = 3.0f + rowRng.range(0.0f, 5.0f);
        
        for (int x = 0; x < w; ++x) {
            int sx = std::clamp(x + (int)rowOffset, 0, w - 1);
            const float* c = srcPixels + (static_cast<size_t>(y) * w + sx) * 4u;
            
            int rsx = std::clamp(sx + (int)shiftX, 0, w - 1);
            const float* cr = srcPixels + (static_cast<size_t>(y) * w + rsx) * 4u;
            
            int bsx = std::clamp(sx - (int)shiftX, 0, w - 1);
            const float* cb = srcPixels + (static_cast<size_t>(y) * w + bsx) * 4u;
            
            float* dst = dstPixels + (static_cast<size_t>(y) * w + x) * 4u;
            dst[0] = cr[0];
            dst[1] = c[1];
            dst[2] = cb[2];
            dst[3] = c[3];
        }
    });
    dst = ImageF32x4RGBAWithCache(dstImage);
}

ArtifactHalftoneEffect::ArtifactHalftoneEffect() {
    setDisplayName("Halftone");
    setEffectID("builtin.halftone");
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

void ArtifactHalftoneEffect::apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    if (computeMode() != ComputeMode::CPU && runCreativeCompute(src, dst, "CreativeHalftone", kHalftoneComputeHlsl)) {
        return;
    }
    int w = src.width();
    int h = src.height();
    auto srcImage = src.image();
    auto dstImage = srcImage.DeepCopy();
    const float* srcPixels = srcImage.rgba32fData();
    float* dstPixels = dstImage.rgba32fData();
    
    int dotSize = 8;
    
    const int tileRows = (h + dotSize - 1) / dotSize;
    // A tile row never overlaps another tile row in dstImage.
    ArtifactCore::Parallel::For(0, tileRows, w * h, [&](int tileRow) {
        const int y = tileRow * dotSize;
        for (int x = 0; x < w; x += dotSize) {
            float lum = 0;
            int count = 0;
            for (int dy = 0; dy < dotSize && y + dy < h; ++dy) {
                for (int dx = 0; dx < dotSize && x + dx < w; ++dx) {
                    const float* c = srcPixels +
                        (static_cast<size_t>(y + dy) * w + x + dx) * 4u;
                    lum += (c[0] + c[1] + c[2]) / 3.0f;
                    count++;
                }
            }
            lum /= (count > 0 ? count : 1);
            
            float radius = (dotSize / 2.0f) * lum;
            float cx = x + dotSize / 2.0f;
            float cy = y + dotSize / 2.0f;
            
            for (int dy = 0; dy < dotSize && y + dy < h; ++dy) {
                for (int dx = 0; dx < dotSize && x + dx < w; ++dx) {
                    float dist = std::sqrt((x + dx - cx)*(x + dx - cx) + (y + dy - cy)*(y + dy - cy));
                    float* dst = dstPixels +
                        (static_cast<size_t>(y + dy) * w + x + dx) * 4u;
                    const float value = dist < radius ? 0.0f : 1.0f;
                    dst[0] = value;
                    dst[1] = value;
                    dst[2] = value;
                    dst[3] = 1.0f;
                }
            }
        }
    });
    dst = ImageF32x4RGBAWithCache(dstImage);
}

ArtifactOldTVEffect::ArtifactOldTVEffect() {
    setDisplayName("Old TV");
    setEffectID("builtin.old_tv");
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

void ArtifactOldTVEffect::apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    if (computeMode() != ComputeMode::CPU && runCreativeCompute(src, dst, "CreativeOldTV", kOldTVComputeHlsl)) {
        return;
    }
    int w = src.width();
    int h = src.height();
    auto srcImage = src.image();
    auto dstImage = srcImage.DeepCopy();
    const float* srcPixels = srcImage.rgba32fData();
    float* dstPixels = dstImage.rgba32fData();
    
    ArtifactCore::RandomStream rng(42);
    
    // rowRng is forked from y, so scheduling does not affect the Old TV noise.
    ArtifactCore::Parallel::For(0, h, w * h, [&](int y) {
        auto rowRng = rng.fork(static_cast<uint64_t>(y));
        float scanline = (y % 4 == 0) ? 0.7f : 1.0f;
        float jitter = (rowRng.chance(0.08f)) ? rowRng.range(-5.0f, 5.0f) : 0.0f;
        
        for (int x = 0; x < w; ++x) {
            int sx = std::clamp(x + (int)jitter, 0, w - 1);
            const float* c = srcPixels + (static_cast<size_t>(y) * w + sx) * 4u;
            float noise = rowRng.range(-0.1f, 0.1f) * 0.05f;
            float* dst = dstPixels + (static_cast<size_t>(y) * w + x) * 4u;
            dst[0] = std::clamp(c[0] * scanline + noise, 0.0f, 1.0f);
            dst[1] = std::clamp(c[1] * scanline + noise, 0.0f, 1.0f);
            dst[2] = std::clamp(c[2] * scanline + noise, 0.0f, 1.0f);
            dst[3] = c[3];
        }
    });
    dst = ImageF32x4RGBAWithCache(dstImage);
}

} // namespace Artifact
