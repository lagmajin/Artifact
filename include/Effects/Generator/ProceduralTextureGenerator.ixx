module;
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <cstdint>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Generator.ProceduralTexture;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;
import Property.Group;
import ImageProcessing.ProceduralTexture;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

export namespace Artifact
{
    using namespace ArtifactCore;

    class ProceduralTextureGeneratorEffect : public ArtifactAbstractEffect
    {
    public:
        ProceduralTextureGeneratorEffect()
            : settings_(ProceduralTextureGenerator::makePreset(ProceduralTexturePreset::Marble, 0))
        {
            setDisplayName(ArtifactCore::UniString("Procedural Texture (Generator)"));
            setPipelineStage(EffectPipelineStage::Generator);
        }

        ~ProceduralTextureGeneratorEffect() override = default;

        void apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override
        {
            (void)src;
            ImageF32x4_RGBA generated;
            if (ProceduralTextureGenerator::generate(settings_, generated))
            {
                dst = ImageF32x4RGBAWithCache(generated);
            }
            else
            {
                dst = src.DeepCopy();
            }
        }

        ProceduralTexturePreset preset() const { return preset_; }
        void setPreset(ProceduralTexturePreset preset)
        {
            const auto value = std::clamp(static_cast<std::uint32_t>(preset), 0u, 5u);
            preset_ = static_cast<ProceduralTexturePreset>(value);
            settings_ = ProceduralTextureGenerator::makePreset(preset_, settings_.primary.seed);
        }

        std::uint32_t seed() const { return settings_.primary.seed; }
        void setSeed(std::uint32_t seed)
        {
            settings_.primary.seed = seed;
            settings_.post.secondary.seed = seed + 101u;
            settings_.post.warp.seed = seed + 211u;
        }

        int width() const { return settings_.width; }
        void setWidth(int width) { settings_.width = std::clamp(width, 1, 8192); }

        int height() const { return settings_.height; }
        void setHeight(int height) { settings_.height = std::clamp(height, 1, 8192); }

        const ProceduralTextureSettings& settings() const { return settings_; }
        void setSettings(const ProceduralTextureSettings& settings)
        {
            settings_ = settings;
            settings_.width = std::clamp(settings_.width, 1, 8192);
            settings_.height = std::clamp(settings_.height, 1, 8192);
        }

        // The generator currently owns only the CPU preset pipeline.
        // Do not advertise GPU mode until a matching HLSL contract exists.
        bool supportsGPU() const override { return false; }

        std::vector<ArtifactCore::AbstractProperty> getProperties() const override
        {
            std::vector<ArtifactCore::AbstractProperty> props;
            auto& preset = props.emplace_back();
            preset.setName("Preset");
            preset.setType(PropertyType::Integer);
            preset.setValue(static_cast<int>(preset_));
            preset.setMinValue(QVariant(0));
            preset.setMaxValue(QVariant(5));

            auto& seed = props.emplace_back();
            seed.setName("Seed");
            seed.setType(PropertyType::Integer);
            seed.setValue(static_cast<qulonglong>(settings_.primary.seed));

            auto& width = props.emplace_back();
            width.setName("Width");
            width.setType(PropertyType::Integer);
            width.setValue(settings_.width);
            width.setMinValue(QVariant(1));
            width.setMaxValue(QVariant(8192));

            auto& height = props.emplace_back();
            height.setName("Height");
            height.setType(PropertyType::Integer);
            height.setValue(settings_.height);
            height.setMinValue(QVariant(1));
            height.setMaxValue(QVariant(8192));
            return props;
        }

        void setPropertyValue(const UniString& name, const QVariant& value) override
        {
            const QString key = name.toQString();
            if (key == QStringLiteral("Preset")) {
                setPreset(static_cast<ProceduralTexturePreset>(value.toUInt()));
            } else if (key == QStringLiteral("Seed")) {
                setSeed(value.toUInt());
            } else if (key == QStringLiteral("Width")) {
                setWidth(value.toInt());
            } else if (key == QStringLiteral("Height")) {
                setHeight(value.toInt());
            }
        }

    private:
        ProceduralTexturePreset preset_ = ProceduralTexturePreset::Marble;
        ProceduralTextureSettings settings_;
    };
}
