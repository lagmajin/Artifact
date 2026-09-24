module;
#include <QString>

module Composition3D;
import Core.ArtifactMath;

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
  if (!impl_ || !ArtifactCore::artifactIsFinite(position.x()) || !ArtifactCore::artifactIsFinite(position.y()) ||
      !ArtifactCore::artifactIsFinite(position.z())) return;
  impl_->cameraPosition = position;
  changed();
}

QVector3D ArtifactComposition3D::cameraTarget() const {
  return impl_ ? impl_->cameraTarget : QVector3D();
}

void ArtifactComposition3D::setCameraTarget(const QVector3D& target) {
  if (!impl_ || !ArtifactCore::artifactIsFinite(target.x()) || !ArtifactCore::artifactIsFinite(target.y()) ||
      !ArtifactCore::artifactIsFinite(target.z())) return;
  impl_->cameraTarget = target;
  changed();
}

QVector3D ArtifactComposition3D::cameraUp() const {
  return impl_ ? impl_->cameraUp : QVector3D(0.0f, 1.0f, 0.0f);
}

void ArtifactComposition3D::setCameraUp(const QVector3D& up) {
  if (!impl_ || !ArtifactCore::artifactIsFinite(up.x()) || !ArtifactCore::artifactIsFinite(up.y()) ||
      !ArtifactCore::artifactIsFinite(up.z()) || up.lengthSquared() < 1.0e-8f) return;
  impl_->cameraUp = up.normalized();
  changed();
}

float ArtifactComposition3D::cameraFieldOfView() const {
  return impl_ ? impl_->cameraFieldOfView : 45.0f;
}

void ArtifactComposition3D::setCameraFieldOfView(const float degrees) {
  if (!impl_ || !ArtifactCore::artifactIsFinite(degrees)) return;
  const float clamped = ArtifactCore::artifactClamp(degrees, 1.0f, 179.0f);
  if (impl_->cameraFieldOfView == clamped) return;
  impl_->cameraFieldOfView = clamped;
  changed();
}

} // namespace Artifact
