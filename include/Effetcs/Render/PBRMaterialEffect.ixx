module;
#include <QString>
#include <QColor>

export module Artifact.Effect.Render.PBRMaterial;

import std;
import Artifact.Effect.Abstract;
import Utils.String.UniString;

export namespace Artifact {

    using namespace ArtifactCore;

    class PBRMaterialEffect : public ArtifactAbstractEffect {
    private:
        QColor albedoColor_ = QColor(255, 255, 255);
        float metallic_ = 0.0f;
        float roughness_ = 0.5f;

    public:
        PBRMaterialEffect() {
            setDisplayName(ArtifactCore::UniString("PBR Material (Render)"));
            setPipelineStage(EffectPipelineStage::MaterialRender);
        }
        virtual ~PBRMaterialEffect() = default;

        QColor albedoColor() const { return albedoColor_; }
        void setAlbedoColor(const QColor& color) { albedoColor_ = color; }

        float metallic() const { return metallic_; }
        void setMetallic(float m) { metallic_ = m; }

        float roughness() const { return roughness_; }
        void setRoughness(float r) { roughness_ = r; }

        std::vector<AbstractProperty> getProperties() const override {
            std::vector<AbstractProperty> props;
            props.reserve(3);

            auto& albedoProp = props.emplace_back();
            albedoProp.setName("Albedo Color");
            albedoProp.setType(PropertyType::Color);
            albedoProp.setValue(albedoColor_);

            auto& metalProp = props.emplace_back();
            metalProp.setName("Metallic");
            metalProp.setType(PropertyType::Float);
            metalProp.setValue(metallic_);

            auto& roughProp = props.emplace_back();
            roughProp.setName("Roughness");
            roughProp.setType(PropertyType::Float);
            roughProp.setValue(roughness_);

            return props;
        }

        void setPropertyValue(const UniString& name, const QVariant& value) override {
            if (name == UniString("Metallic")) {
                metallic_ = value.toFloat();
            } else if (name == UniString("Roughness")) {
                roughness_ = value.toFloat();
            } else if (name == UniString("Albedo Color")) {
                // Not supported via generic variant binding in this mock yet
            }
        }
    };

}
