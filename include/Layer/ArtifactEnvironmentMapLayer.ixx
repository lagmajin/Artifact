module;
#include <memory>
#include <vector>
#include <cstdint>
#include <QRectF>
#include <QString>
#include <QVariant>
#include <RefCntAutoPtr.hpp>
#include <Texture.h>
export module Artifact.Layer.EnvironmentMap;

import Artifact.Layer.Abstract;
import Property.Group;

export namespace Artifact {

// Environment Map Layer: holds an HDRI / cubemap used for skybox background and IBL lighting.
class ArtifactEnvironmentMapLayer : public ArtifactAbstractLayer {
public:
    ArtifactEnvironmentMapLayer() {
        setLayerName("Environment Map 1");
        setIs3D(true);
    }
    ~ArtifactEnvironmentMapLayer() override = default;

    // ArtifactAbstractLayer overrides
    void draw(ArtifactIRenderer* renderer) override { (void)renderer; }
    UniString className() const override { return "ArtifactEnvironmentMapLayer"; }
    bool is3D() const { return true; }
    bool isNullLayer() const override { return true; }
    bool shouldIncludeInFinalRender() const override { return false; }
    QRectF localBounds() const override { return QRectF(); }

    // Environment map properties
    QString hdriPath() const { return hdriPath_; }
    void setHdriPath(const QString& path) {
        if (hdriPath_ != path) {
            hdriPath_ = path;
            ++revision_;
        }
    }

    float intensity() const { return intensity_; }
    void setIntensity(float intensity) {
        const float clamped = intensity < 0.0f ? 0.0f : intensity;
        if (intensity_ != clamped) {
            intensity_ = clamped;
            ++revision_;
        }
    }

    float rotation() const { return rotation_; }
    void setRotation(float rotationDegrees) {
        if (rotation_ != rotationDegrees) {
            rotation_ = rotationDegrees;
            ++revision_;
        }
    }

    bool visibleAsBackground() const { return visibleAsBackground_; }
    void setVisibleAsBackground(bool visible) {
        if (visibleAsBackground_ != visible) {
            visibleAsBackground_ = visible;
            ++revision_;
        }
    }

    std::uint64_t revision() const { return revision_; }

    // Access the loaded cubemap texture
    Diligent::ITexture* cubemapTexture() const { return cubemapTexture_; }
    void setCubemapTexture(Diligent::ITexture* texture) { cubemapTexture_ = texture; }

    // Generic properties for Inspector
    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override {
        std::vector<ArtifactCore::PropertyGroup> groups;
        ArtifactCore::PropertyGroup envGroup("Environment Map");

        auto hdriPathProp = persistentLayerProperty(
            QStringLiteral("environmentMap.hdriPath"), ArtifactCore::PropertyType::String,
            hdriPath_, -120);
        hdriPathProp->setDisplayLabel(QStringLiteral("HDRI Path"));
        envGroup.addProperty(hdriPathProp);

        auto intensityProp = persistentLayerProperty(
            QStringLiteral("environmentMap.intensity"), ArtifactCore::PropertyType::Float,
            intensity_, -119);
        intensityProp->setDisplayLabel(QStringLiteral("Intensity"));
        envGroup.addProperty(intensityProp);

        auto rotationProp = persistentLayerProperty(
            QStringLiteral("environmentMap.rotation"), ArtifactCore::PropertyType::Float,
            rotation_, -118);
        rotationProp->setDisplayLabel(QStringLiteral("Rotation"));
        envGroup.addProperty(rotationProp);

        auto visibleProp = persistentLayerProperty(
            QStringLiteral("environmentMap.visibleAsBackground"),
            ArtifactCore::PropertyType::Boolean, visibleAsBackground_, -117);
        visibleProp->setDisplayLabel(QStringLiteral("Visible as Background"));
        envGroup.addProperty(visibleProp);
        groups.push_back(envGroup);
        return groups;
    }

    bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override {
        if (propertyPath == QStringLiteral("environmentMap.hdriPath") ||
            propertyPath == QStringLiteral("hdriPath")) {
            setHdriPath(value.toString());
            return true;
        }
        if (propertyPath == QStringLiteral("environmentMap.intensity") ||
            propertyPath == QStringLiteral("intensity")) {
            setIntensity(value.toFloat());
            return true;
        }
        if (propertyPath == QStringLiteral("environmentMap.rotation") ||
            propertyPath == QStringLiteral("rotation")) {
            setRotation(value.toFloat());
            return true;
        }
        if (propertyPath == QStringLiteral("environmentMap.visibleAsBackground") ||
            propertyPath == QStringLiteral("visibleAsBackground")) {
            setVisibleAsBackground(value.toBool());
            return true;
        }
        return false;
    }

private:
    QString hdriPath_;
    float intensity_ = 1.0f;
    float rotation_ = 0.0f;
    bool visibleAsBackground_ = true;
    Diligent::ITexture* cubemapTexture_ = nullptr;
    std::uint64_t revision_ = 1;
};

using ArtifactEnvironmentMapLayerPtr = std::shared_ptr<ArtifactEnvironmentMapLayer>;

} // namespace Artifact
