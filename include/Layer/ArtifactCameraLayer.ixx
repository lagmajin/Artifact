module ;
#include <QMatrix4x4>
#include <QString>
#include <memory>
#include <wobjectdefs.h>

export module Artifact.Layer.Camera;

import Artifact.Layer.Abstract;
import Property.Group;

export namespace Artifact {

 enum class ProjectionMode : int {
  Perspective = 0,
  Orthographic = 1,
 };

 // AfterEffects compatible Camera Layer
 class ArtifactCameraLayer : public ArtifactAbstractLayer {
  W_OBJECT(ArtifactCameraLayer)
 public:
  ArtifactCameraLayer();
  virtual ~ArtifactCameraLayer();

  // ArtifactAbstractLayer overrides
  void draw(ArtifactIRenderer* renderer) override;
  UniString className() const override { return "ArtifactCameraLayer"; }

  // Camera specific properties (AE standard)
  float zoom() const;
  void setZoom(float zoom);

  float focusDistance() const;
  void setFocusDistance(float distance);

  float aperture() const;
  void setAperture(float aperture);

  bool depthOfField() const;
  void setDepthOfField(bool enabled);

  // Projection mode
  ProjectionMode projectionMode() const;
  void setProjectionMode(ProjectionMode mode);

  // Perspective-specific
  float fov() const;
  void setFov(float fovDegrees);

  // Orthographic-specific
  float orthoWidth() const;
  void setOrthoWidth(float width);

  float orthoHeight() const;
  void setOrthoHeight(float height);

  // Clipping planes
  float nearClipPlane() const;
  void setNearClipPlane(float distance);

  float farClipPlane() const;
  void setFarClipPlane(float distance);

  // Projection / View
  QMatrix4x4 viewMatrix() const;
  QMatrix4x4 projectionMatrix(float aspect) const;

  // Generic properties for Inspector
  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

 private:
  struct Impl;
  Impl* camImpl_;
 };

 using ArtifactCameraLayerPtr = std::shared_ptr<ArtifactCameraLayer>;

} // namespace Artifact
