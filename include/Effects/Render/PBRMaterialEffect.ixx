module;
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QString>
#include <QColor>
#include <QVariant>
export module Artifact.Effect.Render.PBRMaterial;




import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Material.Material;

export namespace Artifact {

    using namespace ArtifactCore;

    class PBRMaterialEffect : public ArtifactAbstractEffect {
    private:
        QColor albedoColor_ = QColor(255, 255, 255);
        QColor emissiveColor_ = QColor(0, 0, 0);
        float metallic_ = 0.0f;
        float roughness_ = 0.5f;
        float ambientOcclusion_ = 1.0f;
        float emissiveIntensity_ = 0.0f;

    public:
        PBRMaterialEffect() {
            setDisplayName(ArtifactCore::UniString("PBR Material (Render)"));
            setPipelineStage(EffectPipelineStage::MaterialRender);
        }
        virtual ~PBRMaterialEffect() = default;

        QColor albedoColor() const { return albedoColor_; }
        void setAlbedoColor(const QColor& color) { albedoColor_ = color; }

        QColor emissiveColor() const { return emissiveColor_; }
        void setEmissiveColor(const QColor& color) { emissiveColor_ = color; }

        float metallic() const { return metallic_; }
        void setMetallic(float m) { metallic_ = std::clamp(m, 0.0f, 1.0f); }

        float roughness() const { return roughness_; }
        void setRoughness(float r) { roughness_ = std::clamp(r, 0.0f, 1.0f); }

        float ambientOcclusion() const { return ambientOcclusion_; }
        void setAmbientOcclusion(float ao) {
            ambientOcclusion_ = std::clamp(ao, 0.0f, 1.0f);
        }

        float emissiveIntensity() const { return emissiveIntensity_; }
        void setEmissiveIntensity(float intensity) {
            emissiveIntensity_ = std::max(intensity, 0.0f);
        }

        ArtifactCore::Material toMaterial() const {
            ArtifactCore::Material material(ArtifactCore::MaterialType::PBR);
            material.setName(ArtifactCore::UniString("PBR Material"));
            material.setBaseColor(albedoColor_);
            material.setMetallic(metallic_);
            material.setRoughness(roughness_);
            return material;
        }

        std::vector<AbstractProperty> getProperties() const override {
            std::vector<AbstractProperty> props;
            props.reserve(6);

            auto& albedoProp = props.emplace_back();
            albedoProp.setName("Albedo Color");
            albedoProp.setType(PropertyType::Color);
            albedoProp.setValue(albedoColor_);

            auto& metalProp = props.emplace_back();
            metalProp.setName("Metallic");
            metalProp.setType(PropertyType::Float);
            metalProp.setValue(metallic_);
            metalProp.setHardRange(0.0, 1.0);
            metalProp.setSoftRange(0.0, 1.0);

            auto& roughProp = props.emplace_back();
            roughProp.setName("Roughness");
            roughProp.setType(PropertyType::Float);
            roughProp.setValue(roughness_);
            roughProp.setHardRange(0.0, 1.0);
            roughProp.setSoftRange(0.0, 1.0);

            auto& aoProp = props.emplace_back();
            aoProp.setName("Ambient Occlusion");
            aoProp.setType(PropertyType::Float);
            aoProp.setValue(ambientOcclusion_);
            aoProp.setHardRange(0.0, 1.0);
            aoProp.setSoftRange(0.0, 1.0);

            auto& emissiveColorProp = props.emplace_back();
            emissiveColorProp.setName("Emissive Color");
            emissiveColorProp.setType(PropertyType::Color);
            emissiveColorProp.setValue(emissiveColor_);

            auto& emissiveIntensityProp = props.emplace_back();
            emissiveIntensityProp.setName("Emissive Intensity");
            emissiveIntensityProp.setType(PropertyType::Float);
            emissiveIntensityProp.setValue(emissiveIntensity_);
            emissiveIntensityProp.setHardRange(0.0, 100.0);
            emissiveIntensityProp.setSoftRange(0.0, 10.0);

            return props;
        }

        void setPropertyValue(const UniString& name, const QVariant& value) override {
            if (name == UniString("Metallic")) {
                setMetallic(value.toFloat());
            } else if (name == UniString("Roughness")) {
                setRoughness(value.toFloat());
            } else if (name == UniString("Albedo Color")) {
                if (value.canConvert<QColor>()) {
                    setAlbedoColor(value.value<QColor>());
                }
            } else if (name == UniString("Ambient Occlusion")) {
                setAmbientOcclusion(value.toFloat());
            } else if (name == UniString("Emissive Color")) {
                if (value.canConvert<QColor>()) {
                    setEmissiveColor(value.value<QColor>());
                }
            } else if (name == UniString("Emissive Intensity")) {
                setEmissiveIntensity(value.toFloat());
            }
        }
    };

}
