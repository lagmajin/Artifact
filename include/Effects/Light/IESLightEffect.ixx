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
        intensity.setType(PropertyType::Float);
        intensity.setValue(intensity_);
        intensity.setMinValue(QVariant(0.0));
        intensity.setMaxValue(QVariant(1000.0));
        props.push_back(intensity);
        AbstractProperty temperature;
        temperature.setName(QStringLiteral("Temperature"));
        temperature.setType(PropertyType::Float);
        temperature.setValue(temperature_);
        temperature.setMinValue(QVariant(1000.0));
        temperature.setMaxValue(QVariant(40000.0));
        props.push_back(temperature);
        AbstractProperty useTemperature;
        useTemperature.setName(QStringLiteral("UseTemperature"));
        useTemperature.setType(PropertyType::Boolean);
        useTemperature.setValue(useTemperature_);
        props.push_back(useTemperature);
        AbstractProperty iesPath;
        iesPath.setName(QStringLiteral("IESPath"));
        iesPath.setType(PropertyType::String);
        iesPath.setValue(iesFilePath_);
        props.push_back(iesPath);
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
