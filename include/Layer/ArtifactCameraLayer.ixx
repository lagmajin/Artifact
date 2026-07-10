module;
#include <utility>
#include <memory>
#include <wobjectdefs.h>
#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <QJsonObject>
#include <QString>
export module Artifact.Layer.Camera;


import Artifact.Layer.Abstract;
import Property.Group;

export namespace Artifact {

 enum class ProjectionMode : int {
  Perspective = 0,
  Orthographic = 1,
 };

 enum class StereoMode : int {
  Mono = 0,
  TopBottom = 1,
  SideBySide = 2,
 };

 struct CameraDOFParameters {
  bool enabled = false;
  float focusDistance = 1000.0f;
  float apertureSize = 4.0f;
  float focalLength = 1000.0f;
  float cocScale = 0.0f;
  float maxCoc = 0.0f;
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
  bool is3D() const { return true; }
  bool isNullLayer() const override { return true; }
  QRectF localBounds() const override;

  // Camera specific properties (AE standard)
  float zoom() const;
  void setZoom(float zoom);

  float focusDistance() const;
  void setFocusDistance(float distance);

  float aperture() const;
  void setAperture(float aperture);

  bool depthOfField() const;
  void setDepthOfField(bool enabled);

  bool motionBlur() const;
  void setMotionBlur(bool enabled);

  float blurAmount() const;
  void setBlurAmount(float amount);

  // Normalized values consumed by the future depth-of-field render pass.
  CameraDOFParameters depthOfFieldParameters() const;

  // Projection mode
  ProjectionMode projectionMode() const;
  void setProjectionMode(ProjectionMode mode);

  StereoMode stereoMode() const;
  void setStereoMode(StereoMode mode);

  // Perspective-specific
  float fov() const;
  void setFov(float fovDegrees);
  bool useManualFov() const;
  void setUseManualFov(bool enable);
  void resetFovToZoom();

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

  float ipd() const;
  void setIpd(float ipd);

  // Projection / View
  QMatrix4x4 viewMatrix() const;
  QMatrix4x4 projectionMatrix(float aspect) const;

  // Runtime-only shake offset/rotation. The authored shake parameters are
  // serialized; these evaluated offsets are not.
  QVector3D shakeOffset() const;
  void setShakeOffset(const QVector3D& offset);
  QVector3D shakeRotation() const;
  void setShakeRotation(const QVector3D& eulerDegrees);
  void clearShake();
  void addTrauma(float amount);
  float trauma() const;
  void setTraumaDecay(float decayPerSecond);
  float traumaDecay() const;
  void setShakeFrequency(float frequencyHz);
  float shakeFrequency() const;
  void setShakePositionAmplitude(const QVector3D& amplitude);
  QVector3D shakePositionAmplitude() const;
  void setShakeRotationAmplitude(const QVector3D& amplitudeDegrees);
  QVector3D shakeRotationAmplitude() const;
  void setShakeSeed(std::uint32_t seed);
  std::uint32_t shakeSeed() const;
  void advanceShake(double timeSeconds, double deltaSeconds);

  // Generic properties for Inspector
  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;
  QJsonObject toJson() const override;
  void fromJsonProperties(const QJsonObject& obj) override;

 private:
  struct Impl;
  Impl* camImpl_;
 };

 using ArtifactCameraLayerPtr = std::shared_ptr<ArtifactCameraLayer>;

} // namespace Artifact
