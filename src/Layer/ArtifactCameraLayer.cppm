module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <QMatrix4x4>
#include <QJsonObject>
#include <QVariant>
#include <wobjectimpl.h>

module Artifact.Layer.Camera;

import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Property.Group;
import Property;
import Animation.Transform3D;
import Time.Rational;

namespace Artifact {

namespace {

constexpr float kDegreesToRadians = 0.017453292519943295769f;

int64_t cameraTimelineFps(const ArtifactCameraLayer* layer)
{
    if (!layer) {
        return 30;
    }
    if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {
        const double fps = comp->frameRate().framerate();
        if (fps > 0.0) {
            return std::max<int64_t>(1, static_cast<int64_t>(std::llround(fps)));
        }
    }
    return 30;
}

}

W_OBJECT_IMPL(ArtifactCameraLayer)

struct ArtifactCameraLayer::Impl {
    float zoom_ = 1000.0f;
    float focusDistance_ = 1000.0f;
    float aperture_ = 4.0f;
    bool depthOfField_ = false;
    bool motionBlur_ = false;
    float blurAmount_ = 100.0f;

    // Projection mode
    ProjectionMode projectionMode_ = ProjectionMode::Perspective;
    StereoMode stereoMode_ = StereoMode::Mono;
    float ipd_ = 0.064f;

    // Perspective-specific
    float fov_ = 0.0f; // 0 = auto from zoom, >0 = manual FOV
    bool useManualFov_ = false;

    // Orthographic-specific
    float orthoWidth_ = 1920.0f;
    float orthoHeight_ = 1080.0f;

    // Clipping planes
    float nearClipPlane_ = 1.0f;
    float farClipPlane_ = 100000.0f;

    QVector3D shakeOffset_;
    QVector3D shakeRotation_;
    float trauma_ = 0.0f;
    float traumaDecay_ = 2.5f;
    float shakeFrequency_ = 8.0f;
    QVector3D shakePositionAmplitude_{1.0f, 1.0f, 1.0f};
    QVector3D shakeRotationAmplitude_{1.0f, 1.0f, 1.0f};
    std::uint32_t shakeSeed_ = 1;
    bool hasLastShakeTime_ = false;
    double lastShakeTime_ = 0.0;
};

ArtifactCameraLayer::ArtifactCameraLayer()
    : camImpl_(new Impl())
{
    setLayerName("Camera 1");
    setIs3D(true);
    // A usable default camera must sit in front of the Z=0 composition plane.
    // Explicit project transforms loaded afterwards still override this value.
    setPosition3D(QVector3D(0.0f, 0.0f, camImpl_->zoom_));
}

ArtifactCameraLayer::~ArtifactCameraLayer()
{
    delete camImpl_;
}

void ArtifactCameraLayer::draw(ArtifactIRenderer* renderer)
{
    if (!renderer || !isVisible()) return;

    // Get current state
    const auto frameTime = RationalTime(currentFrame(), cameraTimelineFps(this));
    const auto &t3 = transform3D();
    const QVector3D pos(
        t3.positionXAt(frameTime),
        t3.positionYAt(frameTime),
        t3.positionZAt(frameTime));
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
    // QMatrix4x4's perspective convention looks down local -Z.
    QMatrix4x4 globalMat = getGlobalTransform4x4();
    QVector3D forward = globalMat.mapVector(QVector3D(0, 0, -1)).normalized();
    QVector3D right = globalMat.mapVector(QVector3D(1, 0, 0)).normalized();
    QVector3D up = globalMat.mapVector(QVector3D(0, 1, 0)).normalized();
    
    QVector3D tip = pos + forward * (s * 2.5f);
    renderer->drawGizmoArrow(p, float3{ tip.x(), tip.y(), tip.z() }, camColor, s * 0.8f);

    // 3. Draw Frustum / Ortho frame
    const float dist = std::max(10.0f, zoom * 0.75f);
    float frameHalfH = 0.0f;
    float frameHalfW = 0.0f;
    if (type == ProjectionMode::Orthographic) {
        frameHalfW = std::max(20.0f, camImpl_->orthoWidth_ * 0.02f);
        frameHalfH = std::max(20.0f, camImpl_->orthoHeight_ * 0.02f);
    } else {
        const float fovV = fov();
        const float tanHalfV = std::tan(fovV * 0.5f * kDegreesToRadians);
        frameHalfH = std::max(20.0f, dist * tanHalfV);
        frameHalfW = frameHalfH * 1.777f;
    }
    if (depthOfField()) {
        renderer->drawGizmoRing(p, float3{0, 1, 0}, s * 1.6f,
                                ArtifactCore::FloatColor{camColor.r(), camColor.g(),
                                                         camColor.b(), 0.28f},
                                1.0f);
    }

    auto drawCorner = [&](float x, float y) {
        QVector3D cp = pos + forward * dist + right * x + up * y;
        renderer->drawGizmoLine(p, float3{ cp.x(), cp.y(), cp.z() }, camColor, 0.5f);
        return cp;
    };
    
    auto c1 = drawCorner(-frameHalfW, -frameHalfH);
    auto c2 = drawCorner(frameHalfW, -frameHalfH);
    auto c3 = drawCorner(frameHalfW, frameHalfH);
    auto c4 = drawCorner(-frameHalfW, frameHalfH);
    
    // Connect corners
    renderer->drawGizmoLine(float3{c1.x(), c1.y(), c1.z()}, float3{c2.x(), c2.y(), c2.z()}, camColor, 0.5f);
    renderer->drawGizmoLine(float3{c2.x(), c2.y(), c2.z()}, float3{c3.x(), c3.y(), c3.z()}, camColor, 0.5f);
    renderer->drawGizmoLine(float3{c3.x(), c3.y(), c3.z()}, float3{c4.x(), c4.y(), c4.z()}, camColor, 0.5f);
    renderer->drawGizmoLine(float3{c4.x(), c4.y(), c4.z()}, float3{c1.x(), c1.y(), c1.z()}, camColor, 0.5f);
}

float ArtifactCameraLayer::zoom() const { return camImpl_->zoom_; }
void ArtifactCameraLayer::setZoom(float z) {
    camImpl_->zoom_ = std::isfinite(z) ? std::max(0.001f, z) : 0.001f;
    changed();
}

float ArtifactCameraLayer::focusDistance() const { return camImpl_->focusDistance_; }
void ArtifactCameraLayer::setFocusDistance(float d) {
    camImpl_->focusDistance_ = std::isfinite(d) ? std::max(0.001f, d) : 0.001f;
    changed();
}

float ArtifactCameraLayer::aperture() const { return camImpl_->aperture_; }
void ArtifactCameraLayer::setAperture(float a) {
    camImpl_->aperture_ = std::isfinite(a) ? std::max(0.0f, a) : 0.0f;
    changed();
}

bool ArtifactCameraLayer::depthOfField() const { return camImpl_->depthOfField_; }
void ArtifactCameraLayer::setDepthOfField(bool e) { camImpl_->depthOfField_ = e; changed(); }

bool ArtifactCameraLayer::motionBlur() const { return camImpl_->motionBlur_; }
void ArtifactCameraLayer::setMotionBlur(bool e) { camImpl_->motionBlur_ = e; changed(); }

float ArtifactCameraLayer::blurAmount() const { return camImpl_->blurAmount_; }
void ArtifactCameraLayer::setBlurAmount(float a) {
    camImpl_->blurAmount_ = std::isfinite(a) ? std::clamp(a, 0.0f, 100.0f) : 0.0f;
    changed();
}

CameraDOFParameters ArtifactCameraLayer::depthOfFieldParameters() const {
    CameraDOFParameters parameters;
    parameters.enabled = camImpl_->depthOfField_;
    parameters.focusDistance = std::max(0.001f, camImpl_->focusDistance_);
    parameters.apertureSize = std::max(0.0f, camImpl_->aperture_);
    parameters.focalLength = std::max(0.001f, camImpl_->zoom_);

    // Keep authored blur as a normalized scale. Disabled DOF produces zero
    // CoC values so a renderer can bypass the pass without image changes.
    const float blurScale = std::clamp(camImpl_->blurAmount_ / 100.0f, 0.0f, 1.0f);
    parameters.cocScale = parameters.enabled ? blurScale : 0.0f;
    parameters.maxCoc = parameters.enabled
        ? parameters.apertureSize * blurScale
        : 0.0f;
    return parameters;
}

ProjectionMode ArtifactCameraLayer::projectionMode() const { return camImpl_->projectionMode_; }
void ArtifactCameraLayer::setProjectionMode(ProjectionMode mode) {
    camImpl_->projectionMode_ = mode;
    changed();
}

StereoMode ArtifactCameraLayer::stereoMode() const { return camImpl_->stereoMode_; }
void ArtifactCameraLayer::setStereoMode(StereoMode mode) {
    camImpl_->stereoMode_ = mode;
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

bool ArtifactCameraLayer::useManualFov() const {
    return camImpl_->useManualFov_;
}

void ArtifactCameraLayer::setUseManualFov(bool enable) {
    if (camImpl_->useManualFov_ == enable) {
        return;
    }
    camImpl_->useManualFov_ = enable;
    changed();
}

void ArtifactCameraLayer::resetFovToZoom() {
    camImpl_->useManualFov_ = false;
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

float ArtifactCameraLayer::ipd() const { return camImpl_->ipd_; }
void ArtifactCameraLayer::setIpd(float ipd) {
    camImpl_->ipd_ = std::max(0.0f, ipd);
    changed();
}

QMatrix4x4 ArtifactCameraLayer::viewMatrix() const
{
    // The view matrix is the inverse of the camera's global transform.
    const QMatrix4x4 global = getGlobalTransform4x4();
    if (camImpl_->shakeOffset_.isNull() &&
        camImpl_->shakeRotation_.isNull()) {
        return global.inverted();
    }

    QMatrix4x4 shaken = global;
    shaken.translate(camImpl_->shakeOffset_);
    shaken.rotate(camImpl_->shakeRotation_.x(), 1.0f, 0.0f, 0.0f);
    shaken.rotate(camImpl_->shakeRotation_.y(), 0.0f, 1.0f, 0.0f);
    shaken.rotate(camImpl_->shakeRotation_.z(), 0.0f, 0.0f, 1.0f);
    return shaken.inverted();
}

QVector3D ArtifactCameraLayer::shakeOffset() const {
    return camImpl_->shakeOffset_;
}

void ArtifactCameraLayer::setShakeOffset(const QVector3D& offset) {
    camImpl_->shakeOffset_ = offset;
    changed();
}

QVector3D ArtifactCameraLayer::shakeRotation() const {
    return camImpl_->shakeRotation_;
}

void ArtifactCameraLayer::setShakeRotation(const QVector3D& eulerDegrees) {
    camImpl_->shakeRotation_ = eulerDegrees;
    changed();
}

void ArtifactCameraLayer::clearShake() {
    if (camImpl_->shakeOffset_.isNull() &&
        camImpl_->shakeRotation_.isNull()) {
        return;
    }
    camImpl_->shakeOffset_ = QVector3D();
    camImpl_->shakeRotation_ = QVector3D();
    changed();
}

void ArtifactCameraLayer::addTrauma(float amount) {
    camImpl_->trauma_ = std::clamp(camImpl_->trauma_ + std::max(0.0f, amount),
                                   0.0f, 1.0f);
    changed();
}

float ArtifactCameraLayer::trauma() const { return camImpl_->trauma_; }

void ArtifactCameraLayer::setTraumaDecay(float decayPerSecond) {
    camImpl_->traumaDecay_ = std::max(0.0f, decayPerSecond);
}

float ArtifactCameraLayer::traumaDecay() const { return camImpl_->traumaDecay_; }

void ArtifactCameraLayer::setShakeFrequency(float frequencyHz) {
    camImpl_->shakeFrequency_ = std::max(0.0f, frequencyHz);
}

float ArtifactCameraLayer::shakeFrequency() const { return camImpl_->shakeFrequency_; }

void ArtifactCameraLayer::setShakePositionAmplitude(const QVector3D& amplitude) {
    camImpl_->shakePositionAmplitude_ = amplitude;
}

QVector3D ArtifactCameraLayer::shakePositionAmplitude() const {
    return camImpl_->shakePositionAmplitude_;
}

void ArtifactCameraLayer::setShakeRotationAmplitude(const QVector3D& amplitudeDegrees) {
    camImpl_->shakeRotationAmplitude_ = amplitudeDegrees;
}

QVector3D ArtifactCameraLayer::shakeRotationAmplitude() const {
    return camImpl_->shakeRotationAmplitude_;
}

void ArtifactCameraLayer::setShakeSeed(std::uint32_t seed) {
    camImpl_->shakeSeed_ = seed;
}

std::uint32_t ArtifactCameraLayer::shakeSeed() const { return camImpl_->shakeSeed_; }

void ArtifactCameraLayer::advanceShake(double timeSeconds, double deltaSeconds) {
    double effectiveDelta = 0.0;
    if (!camImpl_->hasLastShakeTime_ || timeSeconds > camImpl_->lastShakeTime_) {
        effectiveDelta = std::max(0.0, deltaSeconds);
        camImpl_->lastShakeTime_ = timeSeconds;
        camImpl_->hasLastShakeTime_ = true;
    }
    const float dt = static_cast<float>(effectiveDelta);
    camImpl_->trauma_ = std::max(0.0f, camImpl_->trauma_ -
                                           camImpl_->traumaDecay_ * dt);
    const float intensity = camImpl_->trauma_ * camImpl_->trauma_;
    const float phase = static_cast<float>(timeSeconds) * camImpl_->shakeFrequency_;
    const float seed = static_cast<float>(camImpl_->shakeSeed_ & 0xffffu) * 0.0137f;
    const auto noise = [phase, seed](float channel) {
        return std::sin(phase * (1.0f + channel * 0.17f) + seed +
                        channel * 2.31f);
    };
    camImpl_->shakeOffset_ = QVector3D(
        noise(1.0f) * camImpl_->shakePositionAmplitude_.x() * intensity,
        noise(2.0f) * camImpl_->shakePositionAmplitude_.y() * intensity,
        noise(3.0f) * camImpl_->shakePositionAmplitude_.z() * intensity);
    camImpl_->shakeRotation_ = QVector3D(
        noise(4.0f) * camImpl_->shakeRotationAmplitude_.x() * intensity,
        noise(5.0f) * camImpl_->shakeRotationAmplitude_.y() * intensity,
        noise(6.0f) * camImpl_->shakeRotationAmplitude_.z() * intensity);
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
    // Match the viewport-orientation and fallback camera paths used by the
    // Diligent render target coordinate convention.
    proj(1, 1) = -proj(1, 1);
    
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
    
    ArtifactCore::PropertyGroup projectionOptions("Projection");
    ArtifactCore::PropertyGroup lensOptions("Lens / DOF");
    ArtifactCore::PropertyGroup clippingOptions("Clipping");
    
    // Projection Mode selector
    auto modeProp = persistentLayerProperty(
        QStringLiteral("Camera Options/Projection Mode"),
        ArtifactCore::PropertyType::Integer,
        static_cast<int>(camImpl_->projectionMode_), -150);
    modeProp->setTooltip(QStringLiteral("0 = Perspective, 1 = Orthographic"));
    projectionOptions.addProperty(modeProp);

    auto stereoModeProp = persistentLayerProperty(
        QStringLiteral("Camera Options/Stereo Mode"),
        ArtifactCore::PropertyType::Integer,
        static_cast<int>(camImpl_->stereoMode_), -149);
    stereoModeProp->setTooltip(QStringLiteral("0 = Mono, 1 = TopBottom, 2 = SideBySide"));
    projectionOptions.addProperty(stereoModeProp);

    auto manualFovProp = persistentLayerProperty(
        QStringLiteral("Camera Options/Manual FOV"),
        ArtifactCore::PropertyType::Boolean,
        camImpl_->useManualFov_, -145);
    manualFovProp->setTooltip(QStringLiteral("Use an explicit FOV instead of deriving it from Zoom"));
    projectionOptions.addProperty(manualFovProp);

    // Perspective: Zoom / FOV
    auto zoomProp = persistentLayerProperty(QStringLiteral("Camera Options/Zoom"),
                                            ArtifactCore::PropertyType::Float,
                                            static_cast<double>(camImpl_->zoom_), -140);
    zoomProp->setHardRange(10.0, 10000.0);
    zoomProp->setSoftRange(100.0, 5000.0);
    zoomProp->setUnit(QStringLiteral("px"));
    zoomProp->setTooltip(QStringLiteral("Perspective zoom distance"));
    projectionOptions.addProperty(zoomProp);

    auto fovProp = persistentLayerProperty(QStringLiteral("Camera Options/FOV"),
                                           ArtifactCore::PropertyType::Float,
                                           static_cast<double>(fov()), -135);
    fovProp->setHardRange(1.0, 179.0);
    fovProp->setSoftRange(10.0, 120.0);
    fovProp->setUnit(QStringLiteral("deg"));
    fovProp->setTooltip(QStringLiteral("Manual perspective field of view"));
    projectionOptions.addProperty(fovProp);

    // Orthographic: Width / Height
    auto orthoWProp = persistentLayerProperty(QStringLiteral("Camera Options/Ortho Width"),
                                              ArtifactCore::PropertyType::Float,
                                              static_cast<double>(camImpl_->orthoWidth_), -130);
    orthoWProp->setHardRange(10.0, 100000.0);
    orthoWProp->setSoftRange(100.0, 10000.0);
    orthoWProp->setUnit(QStringLiteral("px"));
    orthoWProp->setTooltip(QStringLiteral("Orthographic frame width"));
    projectionOptions.addProperty(orthoWProp);

    auto orthoHProp = persistentLayerProperty(QStringLiteral("Camera Options/Ortho Height"),
                                              ArtifactCore::PropertyType::Float,
                                              static_cast<double>(camImpl_->orthoHeight_), -125);
    orthoHProp->setHardRange(10.0, 100000.0);
    orthoHProp->setSoftRange(100.0, 10000.0);
    orthoHProp->setUnit(QStringLiteral("px"));
    orthoHProp->setTooltip(QStringLiteral("Orthographic frame height"));
    projectionOptions.addProperty(orthoHProp);

    // Clipping planes
    auto nearProp = persistentLayerProperty(QStringLiteral("Camera Options/Near Clip"),
                                            ArtifactCore::PropertyType::Float,
                                            static_cast<double>(camImpl_->nearClipPlane_), -120);
    nearProp->setHardRange(0.01f, 10000.0);
    nearProp->setSoftRange(1.0, 1000.0);
    nearProp->setUnit(QStringLiteral("px"));
    clippingOptions.addProperty(nearProp);

    auto farProp = persistentLayerProperty(QStringLiteral("Camera Options/Far Clip"),
                                           ArtifactCore::PropertyType::Float,
                                           static_cast<double>(camImpl_->farClipPlane_), -115);
    farProp->setHardRange(1.0, 1000000.0);
    farProp->setSoftRange(1000.0, 100000.0);
    farProp->setUnit(QStringLiteral("px"));
    clippingOptions.addProperty(farProp);

    auto ipdProp = persistentLayerProperty(QStringLiteral("Camera Options/IPD"),
                                           ArtifactCore::PropertyType::Float,
                                           static_cast<double>(camImpl_->ipd_), -114);
    ipdProp->setHardRange(0.0, 1.0);
    ipdProp->setSoftRange(0.02, 0.10);
    ipdProp->setUnit(QStringLiteral("m"));
    ipdProp->setTooltip(QStringLiteral("Inter-pupillary distance for stereo preview"));
    clippingOptions.addProperty(ipdProp);

    // DOF related
    lensOptions.addProperty(persistentLayerProperty(
        QStringLiteral("Camera Options/Depth of Field"),
        ArtifactCore::PropertyType::Boolean,
        camImpl_->depthOfField_, -110));

    auto motionBlurProp = persistentLayerProperty(
        QStringLiteral("Camera Options/Motion Blur"),
        ArtifactCore::PropertyType::Boolean,
        camImpl_->motionBlur_, -108);
    motionBlurProp->setTooltip(QStringLiteral("Enable camera motion blur metadata"));
    lensOptions.addProperty(motionBlurProp);

    auto blurAmountProp = persistentLayerProperty(
        QStringLiteral("Camera Options/Blur Amount"),
        ArtifactCore::PropertyType::Float,
        static_cast<double>(camImpl_->blurAmount_), -107);
    blurAmountProp->setHardRange(0.0, 100.0);
    blurAmountProp->setSoftRange(0.0, 100.0);
    blurAmountProp->setUnit(QStringLiteral("%"));
    blurAmountProp->setTooltip(QStringLiteral("Motion blur amount"));
    lensOptions.addProperty(blurAmountProp);

    auto focusProp = persistentLayerProperty(
        QStringLiteral("Camera Options/Focus Distance"),
        ArtifactCore::PropertyType::Float,
        static_cast<double>(camImpl_->focusDistance_), -105);
    focusProp->setHardRange(10.0, 10000.0);
    focusProp->setSoftRange(100.0, 5000.0);
    focusProp->setUnit(QStringLiteral("px"));
    lensOptions.addProperty(focusProp);

    auto apertureProp = persistentLayerProperty(
        QStringLiteral("Camera Options/Aperture"),
        ArtifactCore::PropertyType::Float,
        static_cast<double>(camImpl_->aperture_), -100);
    apertureProp->setHardRange(0.0, 1000.0);
    apertureProp->setSoftRange(0.0, 250.0);
    apertureProp->setTooltip(QStringLiteral("Aperture / f-stop"));
    lensOptions.addProperty(apertureProp);

    ArtifactCore::PropertyGroup shakeOptions("Camera Shake");
    auto traumaProp = persistentLayerProperty(
        QStringLiteral("Camera Shake/Trauma"), ArtifactCore::PropertyType::Float,
        static_cast<double>(camImpl_->trauma_), -90);
    traumaProp->setHardRange(0.0, 1.0);
    shakeOptions.addProperty(traumaProp);
    auto decayProp = persistentLayerProperty(
        QStringLiteral("Camera Shake/Trauma Decay"), ArtifactCore::PropertyType::Float,
        static_cast<double>(camImpl_->traumaDecay_), -89);
    decayProp->setHardRange(0.0, 100.0);
    shakeOptions.addProperty(decayProp);
    auto frequencyProp = persistentLayerProperty(
        QStringLiteral("Camera Shake/Frequency"), ArtifactCore::PropertyType::Float,
        static_cast<double>(camImpl_->shakeFrequency_), -88);
    frequencyProp->setHardRange(0.0, 240.0);
    frequencyProp->setUnit(QStringLiteral("Hz"));
    shakeOptions.addProperty(frequencyProp);
    auto positionAmplitudeProp = persistentLayerProperty(
        QStringLiteral("Camera Shake/Position Amplitude"), ArtifactCore::PropertyType::Float,
        static_cast<double>(camImpl_->shakePositionAmplitude_.x()), -87);
    positionAmplitudeProp->setHardRange(0.0, 10000.0);
    positionAmplitudeProp->setUnit(QStringLiteral("px"));
    shakeOptions.addProperty(positionAmplitudeProp);
    auto rotationAmplitudeProp = persistentLayerProperty(
        QStringLiteral("Camera Shake/Rotation Amplitude"), ArtifactCore::PropertyType::Float,
        static_cast<double>(camImpl_->shakeRotationAmplitude_.x()), -86);
    rotationAmplitudeProp->setHardRange(0.0, 360.0);
    rotationAmplitudeProp->setUnit(QStringLiteral("deg"));
    shakeOptions.addProperty(rotationAmplitudeProp);
    auto seedProp = persistentLayerProperty(
        QStringLiteral("Camera Shake/Seed"), ArtifactCore::PropertyType::Integer,
        static_cast<int>(camImpl_->shakeSeed_), -85);
    shakeOptions.addProperty(seedProp);
    
    groups.push_back(projectionOptions);
    groups.push_back(lensOptions);
    groups.push_back(clippingOptions);
    groups.push_back(shakeOptions);
    return groups;
}

bool ArtifactCameraLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    if (propertyPath == "Camera Options/Projection Mode") {
        setProjectionMode(static_cast<ProjectionMode>(value.toInt()));
        return true;
    } else if (propertyPath == "Camera Options/Stereo Mode") {
        setStereoMode(static_cast<StereoMode>(value.toInt()));
        return true;
    } else if (propertyPath == "Camera Options/Manual FOV") {
        setUseManualFov(value.toBool());
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
    } else if (propertyPath == "Camera Options/IPD") {
        setIpd(value.toFloat());
        return true;
    } else if (propertyPath == "Camera Options/Depth of Field") {
        setDepthOfField(value.toBool());
        return true;
    } else if (propertyPath == "Camera Options/Motion Blur") {
        setMotionBlur(value.toBool());
        return true;
    } else if (propertyPath == "Camera Options/Blur Amount") {
        setBlurAmount(value.toFloat());
        return true;
    } else if (propertyPath == "Camera Options/Focus Distance") {
        setFocusDistance(value.toFloat());
        return true;
    } else if (propertyPath == "Camera Options/Aperture") {
        setAperture(value.toFloat());
        return true;
    } else if (propertyPath == "Camera Shake/Trauma") {
        camImpl_->trauma_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
        changed();
        return true;
    } else if (propertyPath == "Camera Shake/Trauma Decay") {
        setTraumaDecay(value.toFloat());
        return true;
    } else if (propertyPath == "Camera Shake/Frequency") {
        setShakeFrequency(value.toFloat());
        return true;
    } else if (propertyPath == "Camera Shake/Position Amplitude") {
        const float amplitude = std::max(0.0f, value.toFloat());
        setShakePositionAmplitude(QVector3D(amplitude, amplitude, amplitude));
        return true;
    } else if (propertyPath == "Camera Shake/Rotation Amplitude") {
        const float amplitude = std::max(0.0f, value.toFloat());
        setShakeRotationAmplitude(QVector3D(amplitude, amplitude, amplitude));
        return true;
    } else if (propertyPath == "Camera Shake/Seed") {
        setShakeSeed(static_cast<std::uint32_t>(std::max(0, value.toInt())));
        return true;
    }
    
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

QJsonObject ArtifactCameraLayer::toJson() const
{
    QJsonObject obj = ArtifactAbstractLayer::toJson();
    obj["cameraProjectionMode"] = static_cast<int>(camImpl_->projectionMode_);
    obj["cameraStereoMode"] = static_cast<int>(camImpl_->stereoMode_);
    obj["cameraUseManualFov"] = camImpl_->useManualFov_;
    obj["cameraFov"] = static_cast<double>(camImpl_->fov_);
    obj["cameraZoom"] = static_cast<double>(camImpl_->zoom_);
    obj["cameraFocusDistance"] = static_cast<double>(camImpl_->focusDistance_);
    obj["cameraAperture"] = static_cast<double>(camImpl_->aperture_);
    obj["cameraDepthOfField"] = camImpl_->depthOfField_;
    obj["cameraMotionBlur"] = camImpl_->motionBlur_;
    obj["cameraBlurAmount"] = static_cast<double>(camImpl_->blurAmount_);
    obj["cameraOrthoWidth"] = static_cast<double>(camImpl_->orthoWidth_);
    obj["cameraOrthoHeight"] = static_cast<double>(camImpl_->orthoHeight_);
    obj["cameraNearClip"] = static_cast<double>(camImpl_->nearClipPlane_);
    obj["cameraFarClip"] = static_cast<double>(camImpl_->farClipPlane_);
    obj["cameraIpd"] = static_cast<double>(camImpl_->ipd_);
    obj["cameraShakeTrauma"] = static_cast<double>(camImpl_->trauma_);
    obj["cameraShakeTraumaDecay"] = static_cast<double>(camImpl_->traumaDecay_);
    obj["cameraShakeFrequency"] = static_cast<double>(camImpl_->shakeFrequency_);
    obj["cameraShakePositionAmplitude"] = static_cast<double>(camImpl_->shakePositionAmplitude_.x());
    obj["cameraShakeRotationAmplitude"] = static_cast<double>(camImpl_->shakeRotationAmplitude_.x());
    obj["cameraShakeSeed"] = static_cast<int>(camImpl_->shakeSeed_);
    return obj;
}

void ArtifactCameraLayer::fromJsonProperties(const QJsonObject& obj)
{
    ArtifactAbstractLayer::fromJsonProperties(obj);
    if (obj.contains("cameraProjectionMode")) {
        setProjectionMode(static_cast<ProjectionMode>(obj.value("cameraProjectionMode").toInt()));
    }
    if (obj.contains("cameraStereoMode")) {
        setStereoMode(static_cast<StereoMode>(obj.value("cameraStereoMode").toInt()));
    }
    if (obj.contains("cameraUseManualFov")) {
        setUseManualFov(obj.value("cameraUseManualFov").toBool());
    }
    if (obj.contains("cameraFov")) {
        camImpl_->fov_ = static_cast<float>(obj.value("cameraFov").toDouble(camImpl_->fov_));
    }
    if (obj.contains("cameraZoom")) {
        setZoom(static_cast<float>(obj.value("cameraZoom").toDouble(camImpl_->zoom_)));
    }
    if (obj.contains("cameraFocusDistance")) {
        setFocusDistance(static_cast<float>(obj.value("cameraFocusDistance").toDouble(camImpl_->focusDistance_)));
    }
    if (obj.contains("cameraAperture")) {
        setAperture(static_cast<float>(obj.value("cameraAperture").toDouble(camImpl_->aperture_)));
    }
    if (obj.contains("cameraDepthOfField")) {
        setDepthOfField(obj.value("cameraDepthOfField").toBool());
    }
    if (obj.contains("cameraMotionBlur")) {
        setMotionBlur(obj.value("cameraMotionBlur").toBool());
    }
    if (obj.contains("cameraBlurAmount")) {
        setBlurAmount(static_cast<float>(obj.value("cameraBlurAmount").toDouble(camImpl_->blurAmount_)));
    }
    if (obj.contains("cameraOrthoWidth")) {
        setOrthoWidth(static_cast<float>(obj.value("cameraOrthoWidth").toDouble(camImpl_->orthoWidth_)));
    }
    if (obj.contains("cameraOrthoHeight")) {
        setOrthoHeight(static_cast<float>(obj.value("cameraOrthoHeight").toDouble(camImpl_->orthoHeight_)));
    }
    if (obj.contains("cameraNearClip")) {
        setNearClipPlane(static_cast<float>(obj.value("cameraNearClip").toDouble(camImpl_->nearClipPlane_)));
    }
    if (obj.contains("cameraFarClip")) {
        setFarClipPlane(static_cast<float>(obj.value("cameraFarClip").toDouble(camImpl_->farClipPlane_)));
    }
    if (obj.contains("cameraIpd")) {
        setIpd(static_cast<float>(obj.value("cameraIpd").toDouble(camImpl_->ipd_)));
    }
    if (obj.contains("cameraShakeTrauma")) {
        camImpl_->trauma_ = std::clamp(static_cast<float>(obj.value("cameraShakeTrauma").toDouble()), 0.0f, 1.0f);
    }
    if (obj.contains("cameraShakeTraumaDecay")) {
        setTraumaDecay(static_cast<float>(obj.value("cameraShakeTraumaDecay").toDouble()));
    }
    if (obj.contains("cameraShakeFrequency")) {
        setShakeFrequency(static_cast<float>(obj.value("cameraShakeFrequency").toDouble()));
    }
    if (obj.contains("cameraShakePositionAmplitude")) {
        const float amplitude = static_cast<float>(obj.value("cameraShakePositionAmplitude").toDouble());
        setShakePositionAmplitude(QVector3D(amplitude, amplitude, amplitude));
    }
    if (obj.contains("cameraShakeRotationAmplitude")) {
        const float amplitude = static_cast<float>(obj.value("cameraShakeRotationAmplitude").toDouble());
        setShakeRotationAmplitude(QVector3D(amplitude, amplitude, amplitude));
    }
    if (obj.contains("cameraShakeSeed")) {
        setShakeSeed(static_cast<std::uint32_t>(std::max(0, obj.value("cameraShakeSeed").toInt())));
    }
}

} // namespace Artifact
