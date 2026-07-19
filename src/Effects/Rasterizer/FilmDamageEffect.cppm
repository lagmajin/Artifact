module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.FilmDamage;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;

namespace Artifact {

using namespace ArtifactCore;

class FilmDamageCPUImpl final : public ArtifactEffectImplBase {
public:
    float grain = 0.12f;
    float dust = 0.08f;
    float scratches = 0.1f;
    float gateWeave = 1.5f;
    float flicker = 0.08f;
    float filmBurn = 0.12f;
    float evolution = 0.0f;
    int seed = 1977;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const auto& image = src.image();
        const float* pixels = image.rgba32fData();
        const int width = image.width();
        const int height = image.height();
        if (!pixels || width <= 0 || height <= 0) { dst = src; return; }

        const int frameKey = static_cast<int>(std::floor(evolution * 24.0f));
        std::mt19937 rng(static_cast<std::uint32_t>(seed) ^
                         (static_cast<std::uint32_t>(frameKey) * 0x9e3779b9u));
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        std::normal_distribution<float> normal(0.0f, 1.0f);

        cv::Mat input(height, width, CV_32FC4, const_cast<float*>(pixels));
        cv::Mat working;
        const float weaveX = normal(rng) * gateWeave;
        const float weaveY = normal(rng) * gateWeave * 0.45f;
        const cv::Mat transform = (cv::Mat_<double>(2, 3) <<
            1.0, 0.0, weaveX, 0.0, 1.0, weaveY);
        cv::warpAffine(input, working, transform, input.size(), cv::INTER_LINEAR,
                       cv::BORDER_REFLECT101);

        cv::Mat damage(height, width, CV_32FC4, cv::Scalar(0, 0, 0, 0));
        const int dustCount = static_cast<int>(dust * width * height / 180.0f);
        for (int i = 0; i < dustCount; ++i) {
            const int x = static_cast<int>(unit(rng) * width);
            const int y = static_cast<int>(unit(rng) * height);
            const int radius = 1 + static_cast<int>(unit(rng) * 4.0f);
            const float bright = unit(rng) > 0.55f ? 0.82f : 0.03f;
            cv::circle(damage, cv::Point(x, y), radius,
                       cv::Scalar(bright, bright, bright, 0.25f + unit(rng) * 0.5f),
                       -1, cv::LINE_AA);
        }
        const int scratchCount = static_cast<int>(scratches * 18.0f);
        for (int i = 0; i < scratchCount; ++i) {
            const int x = static_cast<int>(unit(rng) * width);
            const int startY = static_cast<int>(unit(rng) * height * 0.35f);
            const int endY = std::min(height - 1, startY +
                static_cast<int>((0.25f + unit(rng) * 0.7f) * height));
            const float bright = unit(rng) > 0.45f ? 0.9f : 0.04f;
            cv::line(damage, cv::Point(x, startY),
                     cv::Point(x + static_cast<int>(normal(rng) * 2.0f), endY),
                     cv::Scalar(bright, bright, bright, 0.18f + unit(rng) * 0.45f),
                     1, cv::LINE_AA);
        }

        const float flickerGain = 1.0f + normal(rng) * flicker;
        const float burnCenterX = unit(rng) > 0.5f ? -0.08f : 1.08f;
        const float burnCenterY = unit(rng);
        const float burnRadius = 0.18f + filmBurn * 0.8f;
        cv::Mat output = working.clone();
        ArtifactCore::Parallel::For(0, height, [&](int y) {
            std::mt19937 rowRng(static_cast<std::uint32_t>(seed) ^
                                (static_cast<std::uint32_t>(frameKey) * 0x9e3779b9u) ^
                                (static_cast<std::uint32_t>(y) * 0x85ebca6bu));
            std::normal_distribution<float> rowNormal(0.0f, 1.0f);
            const auto* sourceRow = working.ptr<cv::Vec4f>(y);
            const auto* damageRow = damage.ptr<cv::Vec4f>(y);
            auto* outputRow = output.ptr<cv::Vec4f>(y);
            for (int x = 0; x < width; ++x) {
                const float nx = static_cast<float>(x) / std::max(1, width - 1);
                const float ny = static_cast<float>(y) / std::max(1, height - 1);
                const float dx = nx - burnCenterX;
                const float dy = ny - burnCenterY;
                const float distance = std::sqrt(dx * dx + dy * dy);
                const float burn = filmBurn * std::clamp(
                    1.0f - std::abs(distance - burnRadius) / 0.12f, 0.0f, 1.0f);
                const float damageAlpha = std::clamp(damageRow[x][3], 0.0f, 1.0f);
                const float grainNoise = rowNormal(rowRng) * grain;
                for (int c = 0; c < 3; ++c) {
                    float value = sourceRow[x][c] * flickerGain + grainNoise;
                    value = value * (1.0f - damageAlpha) + damageRow[x][c] * damageAlpha;
                    if (c == 0) value += burn * 1.35f;
                    else if (c == 1) value += burn * 0.48f;
                    else value += burn * 0.08f;
                    outputRow[x][c] = value;
                }
                outputRow[x][3] = sourceRow[x][3];
            }
        });
        dst = src;
        dst.image().setFromCVMat(output, src.image().colorDescriptor());
    }
};

FilmDamageEffect::FilmDamageEffect() {
    setDisplayName(UniString("Film Damage"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<FilmDamageCPUImpl>());
    syncImpl();
}

FilmDamageEffect::~FilmDamageEffect() = default;

void FilmDamageEffect::syncImpl() {
    if (auto* impl = dynamic_cast<FilmDamageCPUImpl*>(cpuImpl().get())) {
        impl->grain = grain_; impl->dust = dust_; impl->scratches = scratches_;
        impl->gateWeave = gateWeave_; impl->flicker = flicker_;
        impl->filmBurn = filmBurn_; impl->evolution = evolution_; impl->seed = seed_;
    }
}

std::vector<AbstractProperty> FilmDamageEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& grain = props.emplace_back(); grain.setName("Grain"); grain.setType(PropertyType::Float); grain.setValue(grain_);
    auto& dust = props.emplace_back(); dust.setName("Dust"); dust.setType(PropertyType::Float); dust.setValue(dust_);
    auto& scratches = props.emplace_back(); scratches.setName("Scratches"); scratches.setType(PropertyType::Float); scratches.setValue(scratches_);
    auto& weave = props.emplace_back(); weave.setName("Gate Weave"); weave.setType(PropertyType::Float); weave.setValue(gateWeave_);
    auto& flicker = props.emplace_back(); flicker.setName("Flicker"); flicker.setType(PropertyType::Float); flicker.setValue(flicker_);
    auto& burn = props.emplace_back(); burn.setName("Film Burn"); burn.setType(PropertyType::Float); burn.setValue(filmBurn_);
    auto& evolution = props.emplace_back(); evolution.setName("Evolution"); evolution.setType(PropertyType::Float); evolution.setValue(evolution_);
    auto& seed = props.emplace_back(); seed.setName("Seed"); seed.setType(PropertyType::Integer); seed.setValue(seed_);
    return props;
}

void FilmDamageEffect::setPropertyValue(const UniString& name,
                                        const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Grain")) grain_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
    else if (key == QStringLiteral("Dust")) dust_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
    else if (key == QStringLiteral("Scratches")) scratches_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
    else if (key == QStringLiteral("Gate Weave")) gateWeave_ = std::clamp(value.toFloat(), 0.0f, 20.0f);
    else if (key == QStringLiteral("Flicker")) flicker_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
    else if (key == QStringLiteral("Film Burn")) filmBurn_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
    else if (key == QStringLiteral("Evolution")) evolution_ = value.toFloat();
    else if (key == QStringLiteral("Seed")) seed_ = value.toInt();
    syncImpl();
}

}
