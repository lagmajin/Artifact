module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <QVariant>
#include <wobjectimpl.h>

module Artifact.Layer.EnvironmentMap;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Property.Group;
import Property;
import Color.Float;

namespace Artifact {

W_OBJECT_IMPL(ArtifactEnvironmentMapLayer)

struct ArtifactEnvironmentMapLayer::Impl {
    QString hdriPath_;
    float intensity_ = 1.0f;
    float rotation_ = 0.0f; // degrees around Y axis
    bool visibleAsBackground_ = true;
    Diligent::ITexture* cubemapTexture_ = nullptr; // weak ref, owned by GPUTextureCacheManager
};

ArtifactEnvironmentMapLayer::ArtifactEnvironmentMapLayer()
    : envImpl_(new Impl())
{
    setLayerName("Environment Map 1");
    setIs3D(true);
}

ArtifactEnvironmentMapLayer::~ArtifactEnvironmentMapLayer()
{
    delete envImpl_;
}

void ArtifactEnvironmentMapLayer::draw(ArtifactIRenderer* renderer) {
    // Environment map layer doesn't draw itself.
    // The CompositionRenderController uses its cubemap data
    // when the background mode is set to Skybox.
    Q_UNUSED(renderer);
}

QRectF ArtifactEnvironmentMapLayer::localBounds() const {
    return QRectF();
}

QString ArtifactEnvironmentMapLayer::hdriPath() const {
    return envImpl_->hdriPath_;
}

void ArtifactEnvironmentMapLayer::setHdriPath(const QString& path) {
    envImpl_->hdriPath_ = path;
    // Loading is handled externally via setCubemapTexture
}

float ArtifactEnvironmentMapLayer::intensity() const {
    return envImpl_->intensity_;
}

void ArtifactEnvironmentMapLayer::setIntensity(float intensity) {
    envImpl_->intensity_ = std::max(0.0f, intensity);
}

float ArtifactEnvironmentMapLayer::rotation() const {
    return envImpl_->rotation_;
}

void ArtifactEnvironmentMapLayer::setRotation(float rotationDegrees) {
    envImpl_->rotation_ = rotationDegrees;
}

bool ArtifactEnvironmentMapLayer::visibleAsBackground() const {
    return envImpl_->visibleAsBackground_;
}

void ArtifactEnvironmentMapLayer::setVisibleAsBackground(bool visible) {
    envImpl_->visibleAsBackground_ = visible;
}

Diligent::ITexture* ArtifactEnvironmentMapLayer::cubemapTexture() const {
    return envImpl_->cubemapTexture_;
}

void ArtifactEnvironmentMapLayer::setCubemapTexture(Diligent::ITexture* texture) {
    envImpl_->cubemapTexture_ = texture;
}

std::vector<ArtifactCore::PropertyGroup> ArtifactEnvironmentMapLayer::getLayerPropertyGroups() const {
    std::vector<ArtifactCore::PropertyGroup> groups;

    ArtifactCore::PropertyGroup envGroup("Environment Map");
    auto hdriPathProp = persistentLayerProperty(
        QStringLiteral("environmentMap.hdriPath"),
        ArtifactCore::PropertyType::String,
        envImpl_->hdriPath_,
        -120);
    hdriPathProp->setDisplayLabel(QStringLiteral("HDRI Path"));
    envGroup.addProperty(hdriPathProp);

    auto intensityProp = persistentLayerProperty(
        QStringLiteral("environmentMap.intensity"),
        ArtifactCore::PropertyType::Float,
        envImpl_->intensity_,
        -119);
    intensityProp->setDisplayLabel(QStringLiteral("Intensity"));
    envGroup.addProperty(intensityProp);

    auto rotationProp = persistentLayerProperty(
        QStringLiteral("environmentMap.rotation"),
        ArtifactCore::PropertyType::Float,
        envImpl_->rotation_,
        -118);
    rotationProp->setDisplayLabel(QStringLiteral("Rotation"));
    envGroup.addProperty(rotationProp);

    auto visibleProp = persistentLayerProperty(
        QStringLiteral("environmentMap.visibleAsBackground"),
        ArtifactCore::PropertyType::Boolean,
        envImpl_->visibleAsBackground_,
        -117);
    visibleProp->setDisplayLabel(QStringLiteral("Visible as Background"));
    envGroup.addProperty(visibleProp);
    groups.push_back(envGroup);

    return groups;
}

bool ArtifactEnvironmentMapLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value) {
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

} // namespace Artifact
