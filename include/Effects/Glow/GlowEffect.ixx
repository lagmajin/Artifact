module;
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <QVariant>
#include <cstdint>
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
export module Artifact.Effect.Glow;




import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Artifact.Render.DiligentDeviceManager;
import Artifact.Render.ROI;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class GlowEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;
public:
    static constexpr const char* kGpuGenericKeyString = "glow";
    static constexpr std::uint32_t kGpuGenericKey =
        gpuGenericKeyFromString(kGpuGenericKeyString);

    GlowEffect();
    ~GlowEffect();

    // Expose properties via AbstractProperty API
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) override;

    void setGlowGain(float gain);
    float glowGain() const;

    void setLayerCount(int count);
    int layerCount() const;

    void setBaseSigma(float sigma);
    float baseSigma() const;

    void setSigmaGrowth(float growth);
    float sigmaGrowth() const;

    void setBaseAlpha(float alpha);
    float baseAlpha() const;

    void setAlphaFalloff(float falloff);
    float alphaFalloff() const;

    void setContributionPreview(bool enabled);
    bool contributionPreview() const;

    bool supportsGPU() const override {
        return true;
    }

    std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Generic;
        node.genericKey = kGpuGenericKey;
        node.parameters[0] = glowGain();
        node.parameters[1] = static_cast<float>(layerCount());
        node.parameters[2] = baseSigma();
        node.parameters[3] = sigmaGrowth();
        node.parameters[4] = baseAlpha();
        node.parameters[5] = alphaFalloff();
        node.parameters[6] = contributionPreview() ? 1.0f : 0.0f;
        // baseSigma is in pixels; scale it with the preview resolution.
        node.resolutionScaledParameterMask = (1u << 2);
        return stack.append(node);
    }

    /**
     * @brief ROI 拡張ヒント
     *
     * グローは複数のガウスブラー層を重ねる構造。
     * 最大 sigma = baseSigma * sigmaGrowth^(layerCount-1)。
     * その 3σ 分を ROI 拡張量とする。
     */
    EffectROIHint roiHint() const override;
};

class StillLightLeakEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;
    std::uint8_t look_ = 0;
    float strength_ = 0.55f;
    float streakSourceX_ = 0.01f;
    float streakSourceY_ = 0.105f;
    float streakAngleDegrees_ = 2.5783101f;
    std::uint8_t alphaPolicy_ = 0;

    void syncImpls();

public:
    static constexpr const char* kGpuGenericKeyString = "still_light_leak";
    static constexpr std::uint32_t kGpuGenericKey =
        gpuGenericKeyFromString(kGpuGenericKeyString);

    StillLightLeakEffect();
    ~StillLightLeakEffect() override;
    StillLightLeakEffect(const StillLightLeakEffect&) = delete;
    StillLightLeakEffect& operator=(const StillLightLeakEffect&) = delete;
    StillLightLeakEffect(StillLightLeakEffect&&) = delete;
    StillLightLeakEffect& operator=(StillLightLeakEffect&&) = delete;

    enum class Look : std::uint8_t {
        EdgeWash = 0,
        AnamorphicStreak = 1,
        OrganicGateLeak = 2,
    };
    enum class AlphaPolicy : std::uint8_t {
        PreserveSourceCoverage = 0,
        ClipLeakToSourceCoverage = 1,
        ExpandFromLeakEnergy = 2,
    };

    Look look() const { return static_cast<Look>(look_); }
    void setLook(Look value) {
        switch (value) {
        case Look::EdgeWash:
        case Look::AnamorphicStreak:
        case Look::OrganicGateLeak:
            look_ = static_cast<std::uint8_t>(value);
            break;
        default:
            look_ = static_cast<std::uint8_t>(Look::EdgeWash);
            break;
        }
        syncImpls();
    }

    float strength() const { return strength_; }
    void setStrength(float value) {
        strength_ = std::isfinite(value) ? std::clamp(value, 0.0f, 4.0f) : 0.55f;
        syncImpls();
    }

    float streakSourceX() const { return streakSourceX_; }
    void setStreakSourceX(float value) {
        streakSourceX_ = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.01f;
        syncImpls();
    }

    float streakSourceY() const { return streakSourceY_; }
    void setStreakSourceY(float value) {
        streakSourceY_ = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.105f;
        syncImpls();
    }

    float streakAngleDegrees() const { return streakAngleDegrees_; }
    void setStreakAngleDegrees(float value) {
        streakAngleDegrees_ = std::isfinite(value)
            ? std::clamp(value, -45.0f, 45.0f) : 2.5783101f;
        syncImpls();
    }

    AlphaPolicy alphaPolicy() const { return static_cast<AlphaPolicy>(alphaPolicy_); }
    void setAlphaPolicy(AlphaPolicy value) {
        switch (value) {
        case AlphaPolicy::PreserveSourceCoverage:
        case AlphaPolicy::ClipLeakToSourceCoverage:
        case AlphaPolicy::ExpandFromLeakEnergy:
            alphaPolicy_ = static_cast<std::uint8_t>(value);
            break;
        default:
            alphaPolicy_ = static_cast<std::uint8_t>(AlphaPolicy::PreserveSourceCoverage);
            break;
        }
        syncImpls();
    }

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name,
                          const QVariant& value) override;
    std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }
    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }
    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Generic;
        node.genericKey = kGpuGenericKey;
        node.parameters[0] = static_cast<float>(look_);
        node.parameters[1] = strength_;
        node.parameters[2] = static_cast<float>(alphaPolicy_);
        node.parameters[3] = streakSourceX_;
        node.parameters[4] = streakSourceY_;
        node.parameters[5] = streakAngleDegrees_;
        return stack.append(node);
    }
    bool supportsGPU() const override { return true; }
};

class OpticalGlowEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;

protected:
    void apply(const ImageF32x4RGBAWithCache& src,
               ImageF32x4RGBAWithCache& dst) override;

public:
    OpticalGlowEffect();
    ~OpticalGlowEffect() override;

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name,
                          const QVariant& value) override;
    EffectROIHint roiHint() const override;
};

class VolumetricShineEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache& src,
               ImageF32x4RGBAWithCache& dst) override;
public:
    static constexpr const char* kGpuGenericKeyString = "builtin.volumetric_shine";
    static constexpr std::uint32_t kGpuGenericKey =
        gpuGenericKeyFromString(kGpuGenericKeyString);
    std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override;

    bool supportsGPU() const override { return true; }

    VolumetricShineEffect();
    ~VolumetricShineEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name,
                          const QVariant& value) override;
    EffectROIHint roiHint() const override;
};

class GlintStarFilterEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache& src,
               ImageF32x4RGBAWithCache& dst) override;
public:
    static constexpr const char* kGpuGenericKeyString = "builtin.glint_star_filter";
    static constexpr std::uint32_t kGpuGenericKey =
        gpuGenericKeyFromString(kGpuGenericKeyString);

    GlintStarFilterEffect();
    ~GlintStarFilterEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name,
                          const QVariant& value) override;
    EffectROIHint roiHint() const override;

    bool supportsGPU() const override { return true; }
    std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override;
};

};
