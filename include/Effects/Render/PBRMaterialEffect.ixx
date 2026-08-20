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
        float specular_ = 0.5f;
        float ior_ = 1.5f;
        float transmission_ = 0.0f;
        float clearcoat_ = 0.0f;
        float clearcoatRoughness_ = 0.03f;
        float ambientOcclusion_ = 1.0f;
        float normalStrength_ = 1.0f;
        float sheen_ = 0.0f;
        MaterialAlphaMode alphaMode_ = MaterialAlphaMode::Opaque;
        float alphaCutoff_ = 0.5f;
        float emissiveIntensity_ = 0.0f;

    public:
        PBRMaterialEffect() {
            setDisplayName(ArtifactCore::UniString("PBR Material (Render)"));
            setPipelineStage(EffectPipelineStage::MaterialRender);
        }
        virtual ~PBRMaterialEffect() = default;

        QColor albedoColor() const { return albedoColor_; }
        void setAlbedoColor(const QColor& color) { if (color.isValid()) albedoColor_ = color; }

        QColor emissiveColor() const { return emissiveColor_; }
        void setEmissiveColor(const QColor& color) { if (color.isValid()) emissiveColor_ = color; }

        float metallic() const { return metallic_; }
        void setMetallic(float m) { metallic_ = std::isfinite(m) ? std::clamp(m, 0.0f, 1.0f) : 0.0f; }

        float roughness() const { return roughness_; }
        void setRoughness(float r) { roughness_ = std::isfinite(r) ? std::clamp(r, 0.0f, 1.0f) : 0.5f; }

        float specular() const { return specular_; }
        void setSpecular(float v) { specular_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f; }
        float ior() const { return ior_; }
        void setIOR(float v) { ior_ = std::isfinite(v) ? std::clamp(v, 1.0f, 3.0f) : 1.5f; }
        float transmission() const { return transmission_; }
        void setTransmission(float v) { transmission_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f; }
        float clearcoat() const { return clearcoat_; }
        void setClearcoat(float v) { clearcoat_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f; }
        float clearcoatRoughness() const { return clearcoatRoughness_; }
        void setClearcoatRoughness(float v) { clearcoatRoughness_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.03f; }

        float ambientOcclusion() const { return ambientOcclusion_; }
        void setAmbientOcclusion(float ao) {
            ambientOcclusion_ = std::isfinite(ao) ? std::clamp(ao, 0.0f, 1.0f) : 1.0f;
        }

        float normalStrength() const { return normalStrength_; }
        void setNormalStrength(float strength) {
            normalStrength_ = std::isfinite(strength)
                ? std::clamp(strength, 0.0f, 2.0f) : 1.0f;
        }
        float sheen() const { return sheen_; }
        void setSheen(float value) {
            sheen_ = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
        }

        MaterialAlphaMode alphaMode() const { return alphaMode_; }
        void setAlphaMode(MaterialAlphaMode mode) {
            switch (mode) {
            case MaterialAlphaMode::Masked:
            case MaterialAlphaMode::Blended:
                alphaMode_ = mode;
                break;
            case MaterialAlphaMode::Opaque:
            default:
                alphaMode_ = MaterialAlphaMode::Opaque;
                break;
            }
        }
        float alphaCutoff() const { return alphaCutoff_; }
        void setAlphaCutoff(float cutoff) {
            alphaCutoff_ = std::isfinite(cutoff)
                ? std::clamp(cutoff, 0.0f, 1.0f) : 0.5f;
        }

        float emissiveIntensity() const { return emissiveIntensity_; }
        void setEmissiveIntensity(float intensity) {
            emissiveIntensity_ = std::isfinite(intensity) ? std::clamp(intensity, 0.0f, 100.0f) : 0.0f;
        }

        ArtifactCore::Material toMaterial() const {
            ArtifactCore::Material material(ArtifactCore::MaterialType::PBR);
            material.setName(ArtifactCore::UniString("PBR Material"));
            material.setBaseColor(albedoColor_);
            material.setMetallic(metallic_);
            material.setRoughness(roughness_);
            material.setSpecular(specular_);
            material.setIOR(ior_);
            material.setTransmission(transmission_);
            material.setClearcoat(clearcoat_);
            material.setClearcoatRoughness(clearcoatRoughness_);
            material.setEmissionColor(emissiveColor_);
            material.setEmissionStrength(emissiveIntensity_);
            material.setOcclusionStrength(ambientOcclusion_);
            material.setNormalStrength(normalStrength_);
            material.setSheen(sheen_);
            material.setAlphaMode(alphaMode_);
            material.setAlphaCutoff(alphaCutoff_);
            return material;
        }

        std::vector<AbstractProperty> getProperties() const override {
            std::vector<AbstractProperty> props;
            props.reserve(15);

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

            auto addFloat = [&props](const char* name, float value, double maxValue) {
                auto& prop = props.emplace_back();
                prop.setName(name);
                prop.setType(PropertyType::Float);
                prop.setValue(value);
                prop.setHardRange(0.0, maxValue);
                prop.setSoftRange(0.0, maxValue);
            };
            addFloat("Specular", specular_, 1.0);
            addFloat("IOR", ior_, 3.0);
            addFloat("Transmission", transmission_, 1.0);
            addFloat("Clearcoat", clearcoat_, 1.0);
            addFloat("Clearcoat Roughness", clearcoatRoughness_, 1.0);

            auto& aoProp = props.emplace_back();
            aoProp.setName("Ambient Occlusion");
            aoProp.setType(PropertyType::Float);
            aoProp.setValue(ambientOcclusion_);
            aoProp.setHardRange(0.0, 1.0);
            aoProp.setSoftRange(0.0, 1.0);

            auto& normalProp = props.emplace_back();
            normalProp.setName("Normal Strength");
            normalProp.setType(PropertyType::Float);
            normalProp.setValue(normalStrength_);
            normalProp.setHardRange(0.0, 2.0);
            normalProp.setSoftRange(0.0, 1.0);

            auto& sheenProp = props.emplace_back();
            sheenProp.setName("Sheen");
            sheenProp.setType(PropertyType::Float);
            sheenProp.setValue(sheen_);
            sheenProp.setHardRange(0.0, 1.0);
            sheenProp.setSoftRange(0.0, 1.0);

            auto& alphaModeProp = props.emplace_back();
            alphaModeProp.setName("Alpha Mode");
            alphaModeProp.setType(PropertyType::Integer);
            alphaModeProp.setValue(static_cast<int>(alphaMode_));
            alphaModeProp.setHardRange(0, 2);
            alphaModeProp.setSoftRange(0, 2);

            auto& alphaCutoffProp = props.emplace_back();
            alphaCutoffProp.setName("Alpha Cutoff");
            alphaCutoffProp.setType(PropertyType::Float);
            alphaCutoffProp.setValue(alphaCutoff_);
            alphaCutoffProp.setHardRange(0.0, 1.0);
            alphaCutoffProp.setSoftRange(0.0, 1.0);

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
            } else if (name == UniString("Specular")) {
                setSpecular(value.toFloat());
            } else if (name == UniString("IOR")) {
                setIOR(value.toFloat());
            } else if (name == UniString("Transmission")) {
                setTransmission(value.toFloat());
            } else if (name == UniString("Clearcoat")) {
                setClearcoat(value.toFloat());
            } else if (name == UniString("Clearcoat Roughness")) {
                setClearcoatRoughness(value.toFloat());
            } else if (name == UniString("Albedo Color")) {
                if (value.canConvert<QColor>()) {
                    setAlbedoColor(value.value<QColor>());
                }
            } else if (name == UniString("Ambient Occlusion")) {
                setAmbientOcclusion(value.toFloat());
            } else if (name == UniString("Normal Strength")) {
                setNormalStrength(value.toFloat());
            } else if (name == UniString("Sheen")) {
                setSheen(value.toFloat());
            } else if (name == UniString("Alpha Mode")) {
                const int mode = std::clamp(value.toInt(), 0, 2);
                setAlphaMode(static_cast<MaterialAlphaMode>(mode));
            } else if (name == UniString("Alpha Cutoff")) {
                setAlphaCutoff(value.toFloat());
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
