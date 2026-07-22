module;
#include <QString>
#include <QVariant>
#include <vector>
#include <algorithm>

export module HDRDisplayEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

/**
 * @brief HDR Display Output effect.
 *
 * Controls the final output transfer function for HDR displays.
 * Supports HDR10 (PQ/ST.2084), HLG, scRGB, and SDR modes.
 * Should be the last effect in the post-processing chain.
 *
 * Peak brightness and paper white are calibrated per-display.
 * Gamut expansion preserves hue while boosting saturation for HDR.
 */
class HDRDisplayEffect : public ArtifactAbstractEffect {
public:
    enum Mode { SDR = 0, HDR10_PQ = 1, HLG = 2, scRGB = 3 };

private:
    Mode mode_ = SDR;
    float peakNits_ = 1000.0f;
    float paperWhiteNits_ = 80.0f;
    float saturationBoost_ = 1.2f;

public:
    HDRDisplayEffect() = default;
    ~HDRDisplayEffect() override = default;

    void setMode(Mode m) { mode_ = m; }
    Mode mode() const { return mode_; }

    void setPeakNits(float v) { peakNits_ = std::clamp(v, 100.0f, 10000.0f); }
    float peakNits() const { return peakNits_; }

    void setPaperWhite(float v) { paperWhiteNits_ = std::clamp(v, 10.0f, 500.0f); }
    float paperWhite() const { return paperWhiteNits_; }

    void setSaturationBoost(float v) { saturationBoost_ = std::clamp(v, 0.5f, 2.0f); }
    float saturationBoost() const { return saturationBoost_; }

    std::vector<AbstractProperty> getProperties() const override {
        return {
            makeFloatProperty("PeakNits", peakNits_, 100.0f, 10000.0f),
            makeFloatProperty("PaperWhite", paperWhiteNits_, 10.0f, 500.0f),
            makeFloatProperty("SaturationBoost", saturationBoost_, 0.5f, 2.0f),
            makeIntProperty("DisplayMode", static_cast<int>(mode_), 0, 3),
        };
    }

    void setPropertyValue(const UniString& name, const QVariant& value) override {
        if (name == "PeakNits") peakNits_ = value.toFloat();
        else if (name == "PaperWhite") paperWhiteNits_ = value.toFloat();
        else if (name == "SaturationBoost") saturationBoost_ = value.toFloat();
        else if (name == "DisplayMode") mode_ = static_cast<Mode>(value.toInt());
    }

    bool supportsGPU() const override { return true; }
    EffectType type() const override { return EffectType::Color; }
};

} // namespace Artifact
