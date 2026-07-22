module;
#include <QString>
#include <QVariant>
#include <vector>
#include <algorithm>

export module AutoExposureEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Property.Abstract;
import Image.ImageF32x4RGBAWithCache;
import Utils.String.UniString;

export namespace Artifact {

/**
 * @brief Auto Exposure / Eye Adaptation effect.
 *
 * Dynamically adjusts image brightness by analyzing the luminance
 * histogram and adapting exposure to match a target middle gray (18%).
 * Mimics how the human eye adjusts to changing light conditions.
 *
 * Uses the existing luminance pass compute shaders for analysis
 * and applies an exposure multiplier during tone mapping.
 */
class AutoExposureEffect : public ArtifactAbstractEffect {
private:
    float middleGray_ = 0.18f;
    float minExposure_ = -8.0f;
    float maxExposure_ = 8.0f;
    float adaptationSpeed_ = 1.5f;
    bool enabled_ = true;

public:
    AutoExposureEffect() = default;
    ~AutoExposureEffect() override = default;

    void setMiddleGray(float v) { middleGray_ = std::clamp(v, 0.01f, 1.0f); }
    float middleGray() const { return middleGray_; }

    void setMinExposure(float v) { minExposure_ = v; }
    float minExposure() const { return minExposure_; }

    void setMaxExposure(float v) { maxExposure_ = v; }
    float maxExposure() const { return maxExposure_; }

    void setAdaptationSpeed(float v) { adaptationSpeed_ = std::max(v, 0.01f); }
    float adaptationSpeed() const { return adaptationSpeed_; }

    void setEnabled(bool e) { enabled_ = e; }
    bool isEnabled() const { return enabled_; }

    std::vector<AbstractProperty> getProperties() const override {
        return {
            makeFloatProperty("MiddleGray", middleGray_, 0.01f, 1.0f),
            makeFloatProperty("MinExposure", minExposure_, -16.0f, 0.0f),
            makeFloatProperty("MaxExposure", maxExposure_, 0.0f, 16.0f),
            makeFloatProperty("AdaptationSpeed", adaptationSpeed_, 0.01f, 10.0f),
            makeBoolProperty("Enabled", enabled_),
        };
    }

    void setPropertyValue(const UniString& name, const QVariant& value) override {
        if (name == "MiddleGray") middleGray_ = value.toFloat();
        else if (name == "MinExposure") minExposure_ = value.toFloat();
        else if (name == "MaxExposure") maxExposure_ = value.toFloat();
        else if (name == "AdaptationSpeed") adaptationSpeed_ = value.toFloat();
        else if (name == "Enabled") enabled_ = value.toBool();
    }

    bool supportsGPU() const override { return true; }
    EffectType type() const override { return EffectType::Color; }
};

} // namespace Artifact
