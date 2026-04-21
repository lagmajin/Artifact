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
            preset_ = preset;
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
        void setWidth(int width) { settings_.width = width; }

        int height() const { return settings_.height; }
        void setHeight(int height) { settings_.height = height; }

        const ProceduralTextureSettings& settings() const { return settings_; }
        void setSettings(const ProceduralTextureSettings& settings)
        {
            settings_ = settings;
        }

        bool supportsGPU() const override { return true; }

        std::vector<ArtifactCore::AbstractProperty> getProperties() const override
        {
            return {};
        }

    private:
        ProceduralTexturePreset preset_ = ProceduralTexturePreset::Marble;
        ProceduralTextureSettings settings_;
    };
}
