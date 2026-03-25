module;
#include <cmath>
#include <QMatrix4x4>
#include <QVariant>
#include <wobjectimpl.h>

module Artifact.Layer.Camera;

import Artifact.Layer.Abstract;
import Property.Group;
import Property;
import Animation.Transform3D;

namespace Artifact {

W_OBJECT_IMPL(ArtifactCameraLayer)

struct ArtifactCameraLayer::Impl {
    float zoom_ = 1000.0f;
    float focusDistance_ = 1000.0f;
    float aperture_ = 50.0f;
    bool depthOfField_ = false;
};

ArtifactCameraLayer::ArtifactCameraLayer()
    : camImpl_(new Impl())
{
    setLayerName("Camera 1");
}

ArtifactCameraLayer::~ArtifactCameraLayer()
{
    delete camImpl_;
}

void ArtifactCameraLayer::draw(ArtifactIRenderer* /*renderer*/)
{
    // Camera is invisible in the final render
}

float ArtifactCameraLayer::zoom() const { return camImpl_->zoom_; }
void ArtifactCameraLayer::setZoom(float z) { camImpl_->zoom_ = z; changed(); }

float ArtifactCameraLayer::focusDistance() const { return camImpl_->focusDistance_; }
void ArtifactCameraLayer::setFocusDistance(float d) { camImpl_->focusDistance_ = d; changed(); }

float ArtifactCameraLayer::aperture() const { return camImpl_->aperture_; }
void ArtifactCameraLayer::setAperture(float a) { camImpl_->aperture_ = a; changed(); }

bool ArtifactCameraLayer::depthOfField() const { return camImpl_->depthOfField_; }
void ArtifactCameraLayer::setDepthOfField(bool e) { camImpl_->depthOfField_ = e; changed(); }

QMatrix4x4 ArtifactCameraLayer::viewMatrix() const
{
    // In AE, camera looks along +Z, and -Z is towards the viewer.
    // The view matrix is the inverse of the camera's global transform.
    return getGlobalTransform4x4().inverted();
}

QMatrix4x4 ArtifactCameraLayer::projectionMatrix(float aspect) const
{
    // AE Zoom to FOV conversion:
    // fovY = 2 * atan((height/2) / zoom)
    // Since we use a generic canvas, we can assume a standard height or use the zoom directly.
    // Standard AE behavior: Zoom is in pixels.
    // If zoom = 1000 and canvas height = 1080:
    // verticalFOV = 2 * atan(540 / 1000) = ~56.7 degrees.
    
    float vFov = static_cast<float>(2.0 * std::atan(540.0 / camImpl_->zoom_) * 180.0 / 3.14159265358979);
    
    QMatrix4x4 proj;
    proj.perspective(vFov, aspect, 1.0f, 100000.0f);
    return proj;
}

std::vector<ArtifactCore::PropertyGroup> ArtifactCameraLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
    
    ArtifactCore::PropertyGroup cameraOptions("Camera Options");
    auto zoomProp = persistentLayerProperty(QStringLiteral("Camera Options/Zoom"),
                                            ArtifactCore::PropertyType::Float,
                                            static_cast<double>(camImpl_->zoom_), -140);
    zoomProp->setHardRange(10.0, 10000.0);
    zoomProp->setSoftRange(100.0, 5000.0);
    zoomProp->setUnit(QStringLiteral("px"));
    cameraOptions.addProperty(zoomProp);

    cameraOptions.addProperty(persistentLayerProperty(
        QStringLiteral("Camera Options/Depth of Field"),
        ArtifactCore::PropertyType::Boolean,
        camImpl_->depthOfField_, -130));

    auto focusProp = persistentLayerProperty(
        QStringLiteral("Camera Options/Focus Distance"),
        ArtifactCore::PropertyType::Float,
        static_cast<double>(camImpl_->focusDistance_), -120);
    focusProp->setHardRange(10.0, 10000.0);
    focusProp->setSoftRange(100.0, 5000.0);
    focusProp->setUnit(QStringLiteral("px"));
    cameraOptions.addProperty(focusProp);

    auto apertureProp = persistentLayerProperty(
        QStringLiteral("Camera Options/Aperture"),
        ArtifactCore::PropertyType::Float,
        static_cast<double>(camImpl_->aperture_), -110);
    apertureProp->setHardRange(0.0, 1000.0);
    apertureProp->setSoftRange(0.0, 250.0);
    apertureProp->setUnit(QStringLiteral("px"));
    cameraOptions.addProperty(apertureProp);
    
    groups.push_back(cameraOptions);
    return groups;
}

bool ArtifactCameraLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    if (propertyPath == "Camera Options/Zoom") {
        setZoom(value.toFloat());
        return true;
    } else if (propertyPath == "Camera Options/Depth of Field") {
        setDepthOfField(value.toBool());
        return true;
    } else if (propertyPath == "Camera Options/Focus Distance") {
        setFocusDistance(value.toFloat());
        return true;
    } else if (propertyPath == "Camera Options/Aperture") {
        setAperture(value.toFloat());
        return true;
    }
    
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

} // namespace Artifact
