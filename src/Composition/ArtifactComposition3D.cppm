module;
#include <QString>
#include <algorithm>
#include <cmath>

module Composition3D;

namespace Artifact {

class ArtifactComposition3D::Impl {
public:
  QVector3D cameraPosition{0.0f, 0.0f, 5.0f};
  QVector3D cameraTarget{0.0f, 0.0f, 0.0f};
  QVector3D cameraUp{0.0f, 1.0f, 0.0f};
  float cameraFieldOfView = 45.0f;
};

ArtifactComposition3D::ArtifactComposition3D(
    const CompositionID& id, const ArtifactCompositionInitParams& params)
    : ArtifactAbstractComposition(id, params), impl_(new Impl()) {}

ArtifactComposition3D::~ArtifactComposition3D() { delete impl_; }

QVector3D ArtifactComposition3D::cameraPosition() const {
  return impl_ ? impl_->cameraPosition : QVector3D();
}

void ArtifactComposition3D::setCameraPosition(const QVector3D& position) {
  if (!impl_ || !std::isfinite(position.x()) || !std::isfinite(position.y()) ||
      !std::isfinite(position.z())) return;
  impl_->cameraPosition = position;
  changed();
}

QVector3D ArtifactComposition3D::cameraTarget() const {
  return impl_ ? impl_->cameraTarget : QVector3D();
}

void ArtifactComposition3D::setCameraTarget(const QVector3D& target) {
  if (!impl_ || !std::isfinite(target.x()) || !std::isfinite(target.y()) ||
      !std::isfinite(target.z())) return;
  impl_->cameraTarget = target;
  changed();
}

QVector3D ArtifactComposition3D::cameraUp() const {
  return impl_ ? impl_->cameraUp : QVector3D(0.0f, 1.0f, 0.0f);
}

void ArtifactComposition3D::setCameraUp(const QVector3D& up) {
  if (!impl_ || !std::isfinite(up.x()) || !std::isfinite(up.y()) ||
      !std::isfinite(up.z()) || up.lengthSquared() < 1.0e-8f) return;
  impl_->cameraUp = up.normalized();
  changed();
}

float ArtifactComposition3D::cameraFieldOfView() const {
  return impl_ ? impl_->cameraFieldOfView : 45.0f;
}

void ArtifactComposition3D::setCameraFieldOfView(const float degrees) {
  if (!impl_ || !std::isfinite(degrees)) return;
  const float clamped = std::clamp(degrees, 1.0f, 179.0f);
  if (impl_->cameraFieldOfView == clamped) return;
  impl_->cameraFieldOfView = clamped;
  changed();
}

} // namespace Artifact
