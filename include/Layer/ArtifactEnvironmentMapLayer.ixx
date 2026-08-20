module;
#include <memory>
#include <vector>
#include <cstdint>
#include <cmath>
#include <QRectF>
#include <QString>
#include <QVariant>
#include <QJsonObject>
#include <RefCntAutoPtr.hpp>
#include <Texture.h>
export module Artifact.Layer.EnvironmentMap;

import Artifact.Layer.Abstract;
import Property.Group;
import Memory.SharedPtr;

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

    QJsonObject toJson() const override {
        QJsonObject obj = ArtifactAbstractLayer::toJson();
        obj[QStringLiteral("type")] = static_cast<int>(LayerType::EnvironmentMap);
        obj[QStringLiteral("environmentMap.hdriPath")] = hdriPath_;
        obj[QStringLiteral("environmentMap.intensity")] = intensity_;
        obj[QStringLiteral("environmentMap.rotation")] = rotation_;
        obj[QStringLiteral("environmentMap.visibleAsBackground")] = visibleAsBackground_;
        return obj;
    }

    void fromJsonProperties(const QJsonObject& obj) override {
        ArtifactAbstractLayer::fromJsonProperties(obj);
        if (obj.contains(QStringLiteral("environmentMap.hdriPath"))) {
            setHdriPath(obj.value(QStringLiteral("environmentMap.hdriPath")).toString());
        } else if (obj.contains(QStringLiteral("hdriPath"))) {
            setHdriPath(obj.value(QStringLiteral("hdriPath")).toString());
        }
        if (obj.contains(QStringLiteral("environmentMap.intensity"))) {
            setIntensity(static_cast<float>(obj.value(QStringLiteral("environmentMap.intensity")).toDouble()));
        } else if (obj.contains(QStringLiteral("intensity"))) {
            setIntensity(static_cast<float>(obj.value(QStringLiteral("intensity")).toDouble()));
        }
        if (obj.contains(QStringLiteral("environmentMap.rotation"))) {
            setRotation(static_cast<float>(obj.value(QStringLiteral("environmentMap.rotation")).toDouble()));
        } else if (obj.contains(QStringLiteral("rotation"))) {
            setRotation(static_cast<float>(obj.value(QStringLiteral("rotation")).toDouble()));
        }
        if (obj.contains(QStringLiteral("environmentMap.visibleAsBackground"))) {
            setVisibleAsBackground(obj.value(QStringLiteral("environmentMap.visibleAsBackground")).toBool());
        } else if (obj.contains(QStringLiteral("visibleAsBackground"))) {
            setVisibleAsBackground(obj.value(QStringLiteral("visibleAsBackground")).toBool());
        }
    }

    // Environment map properties
    QString hdriPath() const { return hdriPath_; }
    void setHdriPath(const QString& path) {
        const QString normalizedPath = path.trimmed();
        if (hdriPath_ != normalizedPath) {
            hdriPath_ = normalizedPath;
            // A path change invalidates all derived GPU resources. The loader
            // will repopulate the cubemap and IBL products for the new asset.
            clearGpuResources();
            ++revision_;
            changed();
        }
    }

    float intensity() const { return intensity_; }
    void setIntensity(float intensity) {
        const float clamped = std::isfinite(intensity)
            ? (intensity < 0.0f ? 0.0f : intensity)
            : 0.0f;
        if (intensity_ != clamped) {
            intensity_ = clamped;
            ++revision_;
            changed();
        }
    }

    float rotation() const { return rotation_; }
    void setRotation(float rotationDegrees) {
        if (!std::isfinite(rotationDegrees)) return;
        float normalized = std::fmod(rotationDegrees, 360.0f);
        if (normalized < 0.0f) normalized += 360.0f;
        if (rotation_ != normalized) {
            rotation_ = normalized;
            ++revision_;
            changed();
        }
    }

    bool visibleAsBackground() const { return visibleAsBackground_; }
    void setVisibleAsBackground(bool visible) {
        if (visibleAsBackground_ != visible) {
            visibleAsBackground_ = visible;
            ++revision_;
            changed();
        }
    }

    std::uint64_t revision() const { return revision_; }

    // Access the loaded cubemap texture
    Diligent::ITexture* cubemapTexture() const { return cubemapTexture_.RawPtr(); }
    void setCubemapTexture(Diligent::ITexture* texture) {
        cubemapTexture_ = texture;
    }
    void clearGpuResources() { cubemapTexture_.Release(); }

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
    Diligent::RefCntAutoPtr<Diligent::ITexture> cubemapTexture_;
    std::uint64_t revision_ = 1;
};

using ArtifactEnvironmentMapLayerPtr = SharedPtr<ArtifactEnvironmentMapLayer>;

} // namespace Artifact
