module;
#include <QString>
#include <QVariant>
#include <vector>
#include <algorithm>

export module SSSMaterialEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

/**
 * @brief Subsurface Scattering material effect.
 *
 * Adds skin/wax/marble-like subsurface light transport to PBR materials.
 * Uses Burley (Disney) normalized diffusion profile with separable
 * kernel for real-time performance.
 *
 * Presets: Skin, Wax, Marble, Custom
 */
class SSSMaterialEffect : public ArtifactAbstractEffect {
public:
    enum Preset { Skin, Wax, Marble, Custom };

private:
    float strengthR_ = 0.45f;
    float strengthG_ = 0.25f;
    float strengthB_ = 0.15f;
    float radiusMM_ = 3.5f;
    float maxRadiusMM_ = 10.0f;
    Preset preset_ = Skin;

    void applyPreset() {
        switch (preset_) {
        case Skin:   strengthR_=0.45f; strengthG_=0.25f; strengthB_=0.15f; radiusMM_=3.5f; maxRadiusMM_=10.0f; break;
        case Wax:    strengthR_=0.8f;  strengthG_=0.7f;  strengthB_=0.5f;  radiusMM_=8.0f; maxRadiusMM_=25.0f; break;
        case Marble: strengthR_=0.3f;  strengthG_=0.3f;  strengthB_=0.3f;  radiusMM_=2.0f; maxRadiusMM_=5.0f;  break;
        default: break;
        }
    }

public:
    SSSMaterialEffect() = default;
    ~SSSMaterialEffect() override = default;

    void setPreset(Preset p) { preset_ = p; applyPreset(); }
    Preset preset() const { return preset_; }

    void setStrengthR(float v) { strengthR_ = std::clamp(v, 0.0f, 1.0f); preset_ = Custom; }
    void setStrengthG(float v) { strengthG_ = std::clamp(v, 0.0f, 1.0f); preset_ = Custom; }
    void setStrengthB(float v) { strengthB_ = std::clamp(v, 0.0f, 1.0f); preset_ = Custom; }
    void setRadius(float v) { radiusMM_ = std::max(v, 0.1f); preset_ = Custom; }

    float strengthR() const { return strengthR_; }
    float strengthG() const { return strengthG_; }
    float strengthB() const { return strengthB_; }
    float radiusMM() const { return radiusMM_; }

    std::vector<AbstractProperty> getProperties() const override {
        std::vector<AbstractProperty> props;
        auto addFloat = [&props](const QString& name, float value) {
            AbstractProperty prop;
            prop.setName(name);
            prop.setType(PropertyType::Float);
            prop.setValue(value);
            props.push_back(prop);
        };
        addFloat(QStringLiteral("StrengthR"), strengthR_);
        addFloat(QStringLiteral("StrengthG"), strengthG_);
        addFloat(QStringLiteral("StrengthB"), strengthB_);
        addFloat(QStringLiteral("Radius"), radiusMM_);
        AbstractProperty preset;
        preset.setName(QStringLiteral("Preset"));
        preset.setType(PropertyType::Integer);
        preset.setValue(static_cast<int>(preset_));
        props.push_back(preset);
        return props;
    }

    void setPropertyValue(const UniString& name, const QVariant& value) override {
        if (name == "StrengthR") strengthR_ = value.toFloat();
        else if (name == "StrengthG") strengthG_ = value.toFloat();
        else if (name == "StrengthB") strengthB_ = value.toFloat();
        else if (name == "Radius") radiusMM_ = value.toFloat();
        else if (name == "Preset") { preset_ = static_cast<Preset>(value.toInt()); applyPreset(); }
    }

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
