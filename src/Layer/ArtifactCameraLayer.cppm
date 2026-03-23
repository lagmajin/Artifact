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
    cameraOptions.addProperty(ArtifactCore::Property("Zoom", camImpl_->zoom_, 10.0f, 10000.0f, "px"));
    cameraOptions.addProperty(ArtifactCore::Property("Depth of Field", camImpl_->depthOfField_));
    cameraOptions.addProperty(ArtifactCore::Property("Focus Distance", camImpl_->focusDistance_, 10.0f, 10000.0f, "px"));
    cameraOptions.addProperty(ArtifactCore::Property("Aperture", camImpl_->aperture_, 0.0f, 1000.0f, "px"));
    
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
