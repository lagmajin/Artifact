module;
#include <QString>
#include <QVariant>
#include <vector>

export module DecalEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Property.Abstract;
import Image.ImageF32x4RGBAWithCache;
import Utils.String.UniString;

export namespace Artifact {

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
        return {
            makeFloatProperty("Opacity", opacity_, 0.0f, 1.0f),
            makeFloatProperty("NormalFade", normalFade_, 0.0f, 1.0f),
        };
    }

    void setPropertyValue(const UniString& name, const QVariant& value) override {
        if (name == "Opacity") opacity_ = value.toFloat();
        else if (name == "NormalFade") normalFade_ = value.toFloat();
    }

    bool supportsGPU() const override { return true; }
    EffectType type() const override { return EffectType::Rasterizer; }
};

} // namespace Artifact
