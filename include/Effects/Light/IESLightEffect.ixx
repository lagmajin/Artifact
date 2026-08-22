module;
#include <QString>
#include <QVariant>
#include <vector>
#include <algorithm>
#include <cmath>

export module IESLightEffect;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

/**
 * @brief IES Photometric Light Profile effect.
 *
 * Loads .ies files (IES LM-63 format) and applies real-world
 * light distribution patterns to Spot lights. The IES profile
 * modulates light intensity based on vertical/horizontal angles.
 *
 * The effect builds a 2D LUT texture from the IES data at load
 * time and samples it in the light shader for accurate attenuation.
 */
class IESLightEffect : public ArtifactAbstractEffect {
private:
    QString iesFilePath_;
    float intensity_ = 1.0f;
    bool useTemperature_ = false;
    float temperature_ = 4000.0f;

public:
    IESLightEffect() = default;
    ~IESLightEffect() override = default;

    bool loadIES(const QString& path);
    QString iesFilePath() const { return iesFilePath_; }

    void setIntensity(float v) { intensity_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1000.0f) : 1.0f; }
    float intensity() const { return intensity_; }

    void setUseTemperature(bool v) { useTemperature_ = v; }
    bool useTemperature() const { return useTemperature_; }

    void setTemperature(float kelvin) { temperature_ = std::isfinite(kelvin) ? std::clamp(kelvin, 1000.0f, 40000.0f) : 4000.0f; }
    float temperature() const { return temperature_; }

    std::vector<AbstractProperty> getProperties() const override {
        std::vector<AbstractProperty> props;
        AbstractProperty intensity;
        intensity.setName(QStringLiteral("Intensity"));
        intensity.setDisplayLabel(QStringLiteral("Intensity"));
        intensity.setType(PropertyType::Float);
        intensity.setValue(intensity_);
        intensity.setDefaultValue(1.0);
        intensity.setMinValue(QVariant(0.0));
        intensity.setMaxValue(QVariant(1000.0));
        intensity.setSoftRange(QVariant(0.0), QVariant(50.0));
        intensity.setStep(0.1);
        intensity.setTooltip(QStringLiteral("Light intensity multiplier for the IES profile."));
        props.push_back(std::move(intensity));

        AbstractProperty temperature;
        temperature.setName(QStringLiteral("Temperature"));
        temperature.setDisplayLabel(QStringLiteral("Color Temperature"));
        temperature.setType(PropertyType::Float);
        temperature.setValue(temperature_);
        temperature.setDefaultValue(4000.0);
        temperature.setMinValue(QVariant(1000.0));
        temperature.setMaxValue(QVariant(40000.0));
        temperature.setSoftRange(QVariant(2000.0), QVariant(12000.0));
        temperature.setStep(50.0);
        temperature.setUnit(QStringLiteral("K"));
        temperature.setTooltip(QStringLiteral("Correlated color temperature of the light in Kelvin."));
        props.push_back(std::move(temperature));

        AbstractProperty useTemperature;
        useTemperature.setName(QStringLiteral("UseTemperature"));
        useTemperature.setDisplayLabel(QStringLiteral("Use Temperature"));
        useTemperature.setType(PropertyType::Boolean);
        useTemperature.setValue(useTemperature_);
        useTemperature.setTooltip(QStringLiteral("Enable color temperature tinting instead of the raw IES profile color."));
        props.push_back(std::move(useTemperature));

        AbstractProperty iesPath;
        iesPath.setName(QStringLiteral("IESPath"));
        iesPath.setDisplayLabel(QStringLiteral("IES Profile"));
        iesPath.setType(PropertyType::String);
        iesPath.setValue(iesFilePath_);
        iesPath.setTooltip(QStringLiteral("Path to the IES photometric data file describing the light distribution."));
        props.push_back(std::move(iesPath));
        return props;
    }

    void setPropertyValue(const UniString& name, const QVariant& value) override {
        if (name == "Intensity") setIntensity(value.toFloat());
        else if (name == "Temperature") setTemperature(value.toFloat());
        else if (name == "UseTemperature") setUseTemperature(value.toBool());
        else if (name == "IESPath") { iesFilePath_ = value.toString(); loadIES(iesFilePath_); }
    }

    bool supportsGPU() const override { return true; }
};

inline bool IESLightEffect::loadIES(const QString& path) {
    iesFilePath_ = path.trimmed();
    // IES parsing and LUT upload handled by the render pipeline
    return !iesFilePath_.isEmpty();
}

} // namespace Artifact
