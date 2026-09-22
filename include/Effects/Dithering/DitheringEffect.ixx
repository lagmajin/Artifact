module;
#include <cstdint>
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Dithering;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

enum class DitherAlgorithm {
    Bayer2x2,
    Bayer4x4,
    Bayer8x8,
    Bayer16x16,
    FloydSteinberg,
    Atkinson,
    Sierra,
    Stucki
};

class DitheringEffect : public ArtifactAbstractEffect {
private:
    DitherAlgorithm algorithm_ = DitherAlgorithm::Bayer4x4;
    int colorCount_ = 16;
    float amount_ = 1.0f;
    float patternScale_ = 1.0f;
    void syncImpls();

public:
    DitheringEffect();
    ~DitheringEffect() override;

    DitherAlgorithm algorithm() const;
    void setAlgorithm(DitherAlgorithm v);
    int colorCount() const;
    void setColorCount(int v);
    float amount() const;
    void setAmount(float v);
    float patternScale() const;
    void setPatternScale(float v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    static constexpr const char* kGpuGenericKeyString = "ordered_dither";
    static constexpr std::uint32_t kGpuGenericKey =
        gpuGenericKeyFromString(kGpuGenericKeyString);
    std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        // The resident single-pass path is intentionally limited to the two
        // ordered Bayer modes. The larger matrices and error diffusion retain
        // the CPU reference path until their own GPU contracts exist.
        if (algorithm_ != DitherAlgorithm::Bayer2x2 &&
            algorithm_ != DitherAlgorithm::Bayer4x4) {
            return false;
        }
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Generic;
        node.genericKey = kGpuGenericKey;
        node.parameters[0] = static_cast<float>(static_cast<int>(algorithm_));
        node.parameters[1] = static_cast<float>(colorCount_);
        node.parameters[2] = amount_;
        node.parameters[3] = patternScale_;
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

}
