module;

module Artifact.Layer.Camera;

import std;

namespace ArtifactCore {

struct ArtifactCameraLayer::Impl {
    QString name{"Camera"};
    bool enabled{true};
    CameraTransform transform;
    Camera cameraCore;

    QMatrix4x4 cachedView;
    QMatrix4x4 cachedProj;
    bool dirtyView{true};
    bool dirtyProj{true};
};

ArtifactCameraLayer::ArtifactCameraLayer(): impl_(new Impl()) {}
ArtifactCameraLayer::~ArtifactCameraLayer() = default;

void ArtifactCameraLayer::setName(const QString& name) { impl_->name = name; }
QString ArtifactCameraLayer::name() const { return impl_->name; }

void ArtifactCameraLayer::setEnabled(bool e) { impl_->enabled = e; }
bool ArtifactCameraLayer::isEnabled() const { return impl_->enabled; }

void ArtifactCameraLayer::setTransform(const CameraTransform& t) { impl_->transform = t; impl_->dirtyView = true; }
CameraTransform ArtifactCameraLayer::transform() const { return impl_->transform; }

void ArtifactCameraLayer::setCamera(const Camera& c) { impl_->cameraCore = c; impl_->dirtyProj = true; }
Camera ArtifactCameraLayer::camera() const { return impl_->cameraCore; }

QMatrix4x4 ArtifactCameraLayer::viewMatrix() const {
    if (impl_->dirtyView) {
        impl_->cachedView = impl_->transform.toMatrix().inverted();
        impl_->dirtyView = false;
    }
    return impl_->cachedView;
}

QMatrix4x4 ArtifactCameraLayer::projectionMatrix() const {
    if (impl_->dirtyProj) {
        QMatrix4x4 p;
        // Build projection from core Camera
        p.perspective(impl_->cameraCore.getFovY(), impl_->cameraCore.getAspect(), impl_->cameraCore.getNearZ(), impl_->cameraCore.getFarZ());
        impl_->cachedProj = p;
        impl_->dirtyProj = false;
    }
    return impl_->cachedProj;
}

QMatrix4x4 ArtifactCameraLayer::viewProjectionMatrix() const {
    return projectionMatrix() * viewMatrix();
}

void ArtifactCameraLayer::evaluateAtTime(double /*timeSeconds*/) {
    // Placeholder: animations would update transform/settings per time
}

}
