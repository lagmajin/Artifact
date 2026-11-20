module;
#include <cstdint>
#include <vector>
#include <QColor>
#include <QString>
#include <QVariant>

export module Artifact.Effect.BurnAway;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

// Burn Away consumes a layer the way burning paper does: the surface erodes
// behind a moving, noise-perturbed front, the surviving edge chars, and a
// narrow ember rim glows along the front.
//
// Every spatial parameter is normalized against the layer height so the
// preview and the final render describe the same burn at any resolution.
class BurnAwayEffect : public ArtifactAbstractEffect {
private:
    float progress_ = 0.0f;
    float direction_ = 0.0f;
    float edgeWidth_ = 0.05f;
    float charAmount_ = 0.85f;
    float emberIntensity_ = 0.8f;
    float emberWidth_ = 0.012f;
    QColor emberColor_ = QColor(255, 140, 40);
    float edgeSoftness_ = 0.004f;
    float noiseScale_ = 0.06f;
    int octaves_ = 4;
    int seed_ = 0;
    float spread_ = 0.35f;
    float flicker_ = 0.25f;
    // Faint ash film left behind in the burnt region.  Zero keeps the layer
    // fully transparent once it has burned away.
    float residue_ = 0.0f;
    // Density of the isolated spark points that break the ember rim up.
    float sparkDensity_ = 0.5f;
    // Seconds of composition time.  The generic resident shader does not
    // receive a time uniform, so the effect forwards the clock itself.
    float timeSeconds_ = 0.0f;
    void syncImpls();
    // Packs the ember colour into a single float so the sixteen-slot generic
    // parameter buffer keeps room for the residue and spark parameters.
    float packedEmberColor() const;

protected:
    void onContextUpdated(const EffectContext& context) override;

public:
    BurnAwayEffect();
    ~BurnAwayEffect() override;

    float progress() const;
    void setProgress(float value);
    float direction() const;
    void setDirection(float value);
    float edgeWidth() const;
    void setEdgeWidth(float value);
    float charAmount() const;
    void setCharAmount(float value);
    float emberIntensity() const;
    void setEmberIntensity(float value);
    float emberWidth() const;
    void setEmberWidth(float value);
    QColor emberColor() const;
    void setEmberColor(const QColor& value);
    float edgeSoftness() const;
    void setEdgeSoftness(float value);
    float noiseScale() const;
    void setNoiseScale(float value);
    int octaves() const;
    void setOctaves(int value);
    int seed() const;
    void setSeed(int value);
    float spread() const;
    void setSpread(float value);
    float flicker() const;
    void setFlicker(float value);
    float residue() const;
    void setResidue(float value);
    float sparkDensity() const;
    void setSparkDensity(float value);
    float timeSeconds() const;

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    static constexpr const char* kGpuGenericKeyString = "burn_away";
    static constexpr std::uint32_t kGpuGenericKey =
        gpuGenericKeyFromString(kGpuGenericKeyString);
    std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Generic;
        node.genericKey = kGpuGenericKey;
        node.parameters[0] = progress_;
        node.parameters[1] = direction_;
        node.parameters[2] = edgeWidth_;
        node.parameters[3] = charAmount_;
        node.parameters[4] = emberIntensity_;
        // The generic parameter buffer has sixteen slots, so the ember colour
        // travels as one packed 8-bit-per-channel value.  The packed value
        // stays below 2^24 and therefore survives the float round trip
        // exactly, which keeps the GPU output identical to the CPU output.
        node.parameters[5] = packedEmberColor();
        node.parameters[6] = emberWidth_;
        node.parameters[7] = edgeSoftness_;
        node.parameters[8] = noiseScale_;
        node.parameters[9] = static_cast<float>(octaves_);
        node.parameters[10] = static_cast<float>(seed_);
        node.parameters[11] = spread_;
        node.parameters[12] = flicker_;
        node.parameters[13] = timeSeconds_;
        node.parameters[14] = residue_;
        node.parameters[15] = sparkDensity_;
        // Every spatial parameter is normalized against the layer height, so
        // none of them is expressed in source pixels.
        node.resolutionScaledParameterMask = 0;
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

}
