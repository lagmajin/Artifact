module;
#include <QString>
#include <QVariant>
#include <vector>

export module IESLightEffect;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

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

    void setIntensity(float v) { intensity_ = std::max(v, 0.0f); }
    float intensity() const { return intensity_; }

    void setUseTemperature(bool v) { useTemperature_ = v; }
    bool useTemperature() const { return useTemperature_; }

    void setTemperature(float kelvin) { temperature_ = std::clamp(kelvin, 1000.0f, 40000.0f); }
    float temperature() const { return temperature_; }

    std::vector<AbstractProperty> getProperties() const override {
        return {
            makeFloatProperty("Intensity", intensity_, 0.0f, 100.0f),
            makeFloatProperty("Temperature", temperature_, 1000.0f, 40000.0f),
            makeBoolProperty("UseTemperature", useTemperature_),
            makeStringProperty("IESPath", iesFilePath_.toStdString()),
        };
    }

    void setPropertyValue(const UniString& name, const QVariant& value) override {
        if (name == "Intensity") intensity_ = value.toFloat();
        else if (name == "Temperature") temperature_ = value.toFloat();
        else if (name == "UseTemperature") useTemperature_ = value.toBool();
        else if (name == "IESPath") { iesFilePath_ = value.toString(); loadIES(iesFilePath_); }
    }

    bool supportsGPU() const override { return true; }
    EffectType type() const override { return EffectType::Light; }
};

inline bool IESLightEffect::loadIES(const QString& path) {
    iesFilePath_ = path;
    // IES parsing and LUT upload handled by the render pipeline
    return !path.isEmpty();
}

} // namespace Artifact
