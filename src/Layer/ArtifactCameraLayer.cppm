module;
#include <utility>
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

    // Projection mode
    ProjectionMode projectionMode_ = ProjectionMode::Perspective;

    // Perspective-specific
    float fov_ = 0.0f; // 0 = auto from zoom, >0 = manual FOV
    bool useManualFov_ = false;

    // Orthographic-specific
    float orthoWidth_ = 1920.0f;
    float orthoHeight_ = 1080.0f;

    // Clipping planes
    float nearClipPlane_ = 1.0f;
    float farClipPlane_ = 100000.0f;
};

ArtifactCameraLayer::ArtifactCameraLayer()
    : camImpl_(new Impl())
{
    setLayerName("Camera 1");
    setIs3D(true);
}

ArtifactCameraLayer::~ArtifactCameraLayer()
{
    delete camImpl_;
}

void ArtifactCameraLayer::draw(ArtifactIRenderer* renderer)
{
    if (!renderer || !isVisible()) return;

    // Get current state
    const auto pos = transform3D().position();
    const auto type = projectionMode();
    const auto zoom = renderer->getZoom();
    
    // Scale-independent gizmo size
    const float s = 25.0f / (zoom > 0.001f ? zoom : 1.0f);
    const ArtifactCore::FloatColor camColor{ 1.0f, 0.8f, 0.0f, 1.0f }; // Amber/Yellow
    
    using namespace Artifact::Detail;
    float3 p{ pos.x(), pos.y(), pos.z() };

    // 1. Draw Camera Body (Small Cube)
    renderer->drawGizmoCube(p, s, camColor);

    // 2. Draw Lens Direction
    // Camera forward is +Z in our coordinate system
    QMatrix4x4 globalMat = getGlobalTransform4x4();
    QVector3D forward = globalMat.mapVector(QVector3D(0, 0, 1)).normalized();
    QVector3D right = globalMat.mapVector(QVector3D(1, 0, 0)).normalized();
    QVector3D up = globalMat.mapVector(QVector3D(0, 1, 0)).normalized();
    
    QVector3D tip = pos + forward * (s * 2.5f);
    renderer->drawGizmoArrow(p, float3{ tip.x(), tip.y(), tip.z() }, camColor, s * 0.8f);

    // 3. Draw Frustum (Simplified)
    // We draw local corners to form the viewing pyramid
    float fovV = fov();
    float fovH = fovV; // Simplified, ideally uses aspect
    float dist = zoom; // Use zoom value as a proxy for frustum length in gizmo
    
    float tanHalfV = std::tan(fovV * 0.5f * 0.0174532925f);
    float h = dist * tanHalfV;
    float w = h * 1.777f; // 16:9 approx
    
    auto drawCorner = [&](float x, float y) {
        QVector3D cp = pos + forward * dist + right * x + up * y;
        renderer->drawGizmoLine(p, float3{ cp.x(), cp.y(), cp.z() }, camColor, 0.5f);
        return cp;
    };
    
    auto c1 = drawCorner(-w, -h);
    auto c2 = drawCorner(w, -h);
    auto c3 = drawCorner(w, h);
    auto c4 = drawCorner(-w, h);
    
    // Connect corners
    renderer->drawGizmoLine(float3{c1.x(), c1.y(), c1.z()}, float3{c2.x(), c2.y(), c2.z()}, camColor, 0.5f);
    renderer->drawGizmoLine(float3{c2.x(), c2.y(), c2.z()}, float3{c3.x(), c3.y(), c3.z()}, camColor, 0.5f);
    renderer->drawGizmoLine(float3{c3.x(), c3.y(), c3.z()}, float3{c4.x(), c4.y(), c4.z()}, camColor, 0.5f);
    renderer->drawGizmoLine(float3{c4.x(), c4.y(), c4.z()}, float3{c1.x(), c1.y(), c1.z()}, camColor, 0.5f);
}

float ArtifactCameraLayer::zoom() const { return camImpl_->zoom_; }
void ArtifactCameraLayer::setZoom(float z) { camImpl_->zoom_ = z; changed(); }

float ArtifactCameraLayer::focusDistance() const { return camImpl_->focusDistance_; }
void ArtifactCameraLayer::setFocusDistance(float d) { camImpl_->focusDistance_ = d; changed(); }

float ArtifactCameraLayer::aperture() const { return camImpl_->aperture_; }
void ArtifactCameraLayer::setAperture(float a) { camImpl_->aperture_ = a; changed(); }

bool ArtifactCameraLayer::depthOfField() const { return camImpl_->depthOfField_; }
void ArtifactCameraLayer::setDepthOfField(bool e) { camImpl_->depthOfField_ = e; changed(); }

ProjectionMode ArtifactCameraLayer::projectionMode() const { return camImpl_->projectionMode_; }
void ArtifactCameraLayer::setProjectionMode(ProjectionMode mode) {
    camImpl_->projectionMode_ = mode;
    changed();
}

float ArtifactCameraLayer::fov() const {
    if (camImpl_->useManualFov_) {
        return camImpl_->fov_;
    }
    
    // AE behavior: Zoom corresponds to the distance where the comp height is exactly mapped.
    // vFov = 2 * atan( (height/2) / zoom )
    // Default to 1080p height (540 half) if comp is unknown, 
    // but typically zoom is set relative to the composition height in AE.
    return static_cast<float>(2.0 * std::atan(540.0 / camImpl_->zoom_) * 180.0 / 3.14159265358979);
}
void ArtifactCameraLayer::setFov(float fovDegrees) {
    camImpl_->fov_ = fovDegrees;
    camImpl_->useManualFov_ = true;
    changed();
}

float ArtifactCameraLayer::orthoWidth() const { return camImpl_->orthoWidth_; }
void ArtifactCameraLayer::setOrthoWidth(float w) { camImpl_->orthoWidth_ = w; changed(); }

float ArtifactCameraLayer::orthoHeight() const { return camImpl_->orthoHeight_; }
void ArtifactCameraLayer::setOrthoHeight(float h) { camImpl_->orthoHeight_ = h; changed(); }

float ArtifactCameraLayer::nearClipPlane() const { return camImpl_->nearClipPlane_; }
void ArtifactCameraLayer::setNearClipPlane(float d) {
    camImpl_->nearClipPlane_ = std::max(0.01f, d);
    changed();
}

float ArtifactCameraLayer::farClipPlane() const { return camImpl_->farClipPlane_; }
void ArtifactCameraLayer::setFarClipPlane(float d) {
    camImpl_->farClipPlane_ = std::max(camImpl_->nearClipPlane_ + 0.01f, d);
    changed();
}

QMatrix4x4 ArtifactCameraLayer::viewMatrix() const
{
    // In AE, camera looks along +Z, and -Z is towards the viewer.
    // The view matrix is the inverse of the camera's global transform.
    return getGlobalTransform4x4().inverted();
}

QMatrix4x4 ArtifactCameraLayer::projectionMatrix(float aspect) const
{
    QMatrix4x4 proj;
    
    if (camImpl_->projectionMode_ == ProjectionMode::Orthographic) {
        // Orthographic projection
        float halfW = camImpl_->orthoWidth_ * 0.5f;
        float halfH = camImpl_->orthoHeight_ * 0.5f;
        
        // Adjust for aspect ratio if needed
        if (aspect > 1.0f) {
            halfW *= aspect;
        } else {
            halfH /= aspect;
        }
        
        proj.ortho(-halfW, halfW, -halfH, halfH,
                   camImpl_->nearClipPlane_,
                   camImpl_->farClipPlane_);
    } else {
        // Perspective projection
        float vFov = fov();
        proj.perspective(vFov, aspect,
                        camImpl_->nearClipPlane_,
                        camImpl_->farClipPlane_);
    }
    
    return proj;
}

QRectF ArtifactCameraLayer::localBounds() const
{
    // Return a virtual selection area for the camera icon
    return QRectF(-30.0, -30.0, 60.0, 60.0);
}

std::vector<ArtifactCore::PropertyGroup> ArtifactCameraLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
    
    ArtifactCore::PropertyGroup cameraOptions("Camera Options");
    
    // Projection Mode selector
    auto modeProp = persistentLayerProperty(
        QStringLiteral("Camera Options/Projection Mode"),
        ArtifactCore::PropertyType::Integer,
        static_cast<int>(camImpl_->projectionMode_), -150);
    modeProp->setTooltip(QStringLiteral("0 = Perspective, 1 = Orthographic"));
    cameraOptions.addProperty(modeProp);

    // Perspective: Zoom / FOV
    auto zoomProp = persistentLayerProperty(QStringLiteral("Camera Options/Zoom"),
                                            ArtifactCore::PropertyType::Float,
                                            static_cast<double>(camImpl_->zoom_), -140);
    zoomProp->setHardRange(10.0, 10000.0);
    zoomProp->setSoftRange(100.0, 5000.0);
    zoomProp->setUnit(QStringLiteral("px"));
    cameraOptions.addProperty(zoomProp);

    auto fovProp = persistentLayerProperty(QStringLiteral("Camera Options/FOV"),
                                           ArtifactCore::PropertyType::Float,
                                           static_cast<double>(fov()), -135);
    fovProp->setHardRange(1.0, 179.0);
    fovProp->setSoftRange(10.0, 120.0);
    fovProp->setUnit(QStringLiteral("deg"));
    cameraOptions.addProperty(fovProp);

    // Orthographic: Width / Height
    auto orthoWProp = persistentLayerProperty(QStringLiteral("Camera Options/Ortho Width"),
                                              ArtifactCore::PropertyType::Float,
                                              static_cast<double>(camImpl_->orthoWidth_), -130);
    orthoWProp->setHardRange(10.0, 100000.0);
    orthoWProp->setSoftRange(100.0, 10000.0);
    orthoWProp->setUnit(QStringLiteral("px"));
    cameraOptions.addProperty(orthoWProp);

    auto orthoHProp = persistentLayerProperty(QStringLiteral("Camera Options/Ortho Height"),
                                              ArtifactCore::PropertyType::Float,
                                              static_cast<double>(camImpl_->orthoHeight_), -125);
    orthoHProp->setHardRange(10.0, 100000.0);
    orthoHProp->setSoftRange(100.0, 10000.0);
    orthoHProp->setUnit(QStringLiteral("px"));
    cameraOptions.addProperty(orthoHProp);

    // Clipping planes
    auto nearProp = persistentLayerProperty(QStringLiteral("Camera Options/Near Clip"),
                                            ArtifactCore::PropertyType::Float,
                                            static_cast<double>(camImpl_->nearClipPlane_), -120);
    nearProp->setHardRange(0.01f, 10000.0);
    nearProp->setSoftRange(1.0, 1000.0);
    nearProp->setUnit(QStringLiteral("px"));
    cameraOptions.addProperty(nearProp);

    auto farProp = persistentLayerProperty(QStringLiteral("Camera Options/Far Clip"),
                                           ArtifactCore::PropertyType::Float,
                                           static_cast<double>(camImpl_->farClipPlane_), -115);
    farProp->setHardRange(1.0, 1000000.0);
    farProp->setSoftRange(1000.0, 100000.0);
    farProp->setUnit(QStringLiteral("px"));
    cameraOptions.addProperty(farProp);

    // DOF related
    cameraOptions.addProperty(persistentLayerProperty(
        QStringLiteral("Camera Options/Depth of Field"),
        ArtifactCore::PropertyType::Boolean,
        camImpl_->depthOfField_, -110));

    auto focusProp = persistentLayerProperty(
        QStringLiteral("Camera Options/Focus Distance"),
        ArtifactCore::PropertyType::Float,
        static_cast<double>(camImpl_->focusDistance_), -105);
    focusProp->setHardRange(10.0, 10000.0);
    focusProp->setSoftRange(100.0, 5000.0);
    focusProp->setUnit(QStringLiteral("px"));
    cameraOptions.addProperty(focusProp);

    auto apertureProp = persistentLayerProperty(
        QStringLiteral("Camera Options/Aperture"),
        ArtifactCore::PropertyType::Float,
        static_cast<double>(camImpl_->aperture_), -100);
    apertureProp->setHardRange(0.0, 1000.0);
    apertureProp->setSoftRange(0.0, 250.0);
    apertureProp->setUnit(QStringLiteral("px"));
    cameraOptions.addProperty(apertureProp);
    
    groups.push_back(cameraOptions);
    return groups;
}

bool ArtifactCameraLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    if (propertyPath == "Camera Options/Projection Mode") {
        setProjectionMode(static_cast<ProjectionMode>(value.toInt()));
        return true;
    } else if (propertyPath == "Camera Options/Zoom") {
        setZoom(value.toFloat());
        return true;
    } else if (propertyPath == "Camera Options/FOV") {
        setFov(value.toFloat());
        return true;
    } else if (propertyPath == "Camera Options/Ortho Width") {
        setOrthoWidth(value.toFloat());
        return true;
    } else if (propertyPath == "Camera Options/Ortho Height") {
        setOrthoHeight(value.toFloat());
        return true;
    } else if (propertyPath == "Camera Options/Near Clip") {
        setNearClipPlane(value.toFloat());
        return true;
    } else if (propertyPath == "Camera Options/Far Clip") {
        setFarClipPlane(value.toFloat());
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
