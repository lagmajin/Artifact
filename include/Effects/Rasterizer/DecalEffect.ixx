module;
#include <QString>
#include <QVariant>
#include <vector>
#include <algorithm>

export module DecalEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Property.Abstract;
import Image.ImageF32x4RGBAWithCache;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

/**
 * @brief Decal projector effect.
 *
 * Projects a texture onto surfaces using clip-space UV projection,
 * equivalent to a deferred decal. The projection uses the depth buffer
 * to reconstruct world positions for accurate surface mapping.
 */
class DecalEffect : public ArtifactAbstractEffect {
private:
    float opacity_ = 1.0f;
    float normalFade_ = 0.5f;

public:
    DecalEffect() = default;
    ~DecalEffect() override = default;

    void setOpacity(float v) { opacity_ = std::clamp(v, 0.0f, 1.0f); }
    float opacity() const { return opacity_; }

    void setNormalFade(float v) { normalFade_ = std::clamp(v, 0.0f, 1.0f); }
    float normalFade() const { return normalFade_; }

    std::vector<AbstractProperty> getProperties() const override {
        std::vector<AbstractProperty> props;
        AbstractProperty opacity;
        opacity.setName(QStringLiteral("Opacity"));
        opacity.setDisplayLabel(QStringLiteral("Opacity"));
        opacity.setType(PropertyType::Float);
        opacity.setValue(opacity_);
        opacity.setDefaultValue(1.0);
        opacity.setMinValue(QVariant(0.0));
        opacity.setMaxValue(QVariant(1.0));
        opacity.setStep(0.01);
        opacity.setTooltip(QStringLiteral("Decal texture blend strength."));
        props.push_back(std::move(opacity));
        AbstractProperty normalFade;
        normalFade.setName(QStringLiteral("NormalFade"));
        normalFade.setDisplayLabel(QStringLiteral("Normal Fade"));
        normalFade.setType(PropertyType::Float);
        normalFade.setValue(normalFade_);
        normalFade.setDefaultValue(0.5);
        normalFade.setMinValue(QVariant(0.0));
        normalFade.setMaxValue(QVariant(1.0));
        normalFade.setStep(0.01);
        normalFade.setTooltip(QStringLiteral("Fades the decal on surfaces perpendicular to the projection direction to prevent stretching."));
        props.push_back(std::move(normalFade));
        return props;
    }

    void setPropertyValue(const UniString& name, const QVariant& value) override {
        if (name == "Opacity") opacity_ = value.toFloat();
        else if (name == "NormalFade") normalFade_ = value.toFloat();
    }

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
