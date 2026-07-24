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

using namespace ArtifactCore;

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
        std::vector<AbstractProperty> props;
        AbstractProperty middleGray;
        middleGray.setName(QStringLiteral("MiddleGray"));
        middleGray.setType(PropertyType::Float);
        middleGray.setValue(middleGray_);
        props.push_back(middleGray);

        AbstractProperty minExposure;
        minExposure.setName(QStringLiteral("MinExposure"));
        minExposure.setType(PropertyType::Float);
        minExposure.setValue(minExposure_);
        props.push_back(minExposure);

        AbstractProperty maxExposure;
        maxExposure.setName(QStringLiteral("MaxExposure"));
        maxExposure.setType(PropertyType::Float);
        maxExposure.setValue(maxExposure_);
        props.push_back(maxExposure);

        AbstractProperty adaptationSpeed;
        adaptationSpeed.setName(QStringLiteral("AdaptationSpeed"));
        adaptationSpeed.setType(PropertyType::Float);
        adaptationSpeed.setValue(adaptationSpeed_);
        props.push_back(adaptationSpeed);

        AbstractProperty enabled;
        enabled.setName(QStringLiteral("Enabled"));
        enabled.setType(PropertyType::Boolean);
        enabled.setValue(enabled_);
        props.push_back(enabled);
        return props;
    }

    void setPropertyValue(const UniString& name, const QVariant& value) override {
        if (name == "MiddleGray") middleGray_ = value.toFloat();
        else if (name == "MinExposure") minExposure_ = value.toFloat();
        else if (name == "MaxExposure") maxExposure_ = value.toFloat();
        else if (name == "AdaptationSpeed") adaptationSpeed_ = value.toFloat();
        else if (name == "Enabled") enabled_ = value.toBool();
    }

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
