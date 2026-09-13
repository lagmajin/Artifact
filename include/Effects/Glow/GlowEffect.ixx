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
import Image.ImageF32x4RGBAWithCache;

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
    GlintStarFilterEffect();
    ~GlintStarFilterEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name,
                          const QVariant& value) override;
    EffectROIHint roiHint() const override;
};

};
