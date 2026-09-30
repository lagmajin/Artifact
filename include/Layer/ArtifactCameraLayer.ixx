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
import Math.Vec;
import Property.Group;
import Memory.SharedPtr;

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
  ArtifactCore::Units::Pixels focusDistance{1000.0f};
  ArtifactCore::Units::FStop apertureSize{4.0f};
  ArtifactCore::Units::Pixels focalLength{1000.0f};
  ArtifactCore::Units::ScaleFactor cocScale{0.0f};
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
  ArtifactCore::Units::Pixels zoom() const;
  void setZoom(ArtifactCore::Units::Pixels zoom);

  ArtifactCore::Units::Pixels focusDistance() const;
  void setFocusDistance(ArtifactCore::Units::Pixels distance);

  ArtifactCore::Units::FStop aperture() const;
  void setAperture(ArtifactCore::Units::FStop aperture);

  bool depthOfField() const;
  void setDepthOfField(bool enabled);

  bool motionBlur() const;
  void setMotionBlur(bool enabled);

  ArtifactCore::Units::Percent blurAmount() const;
  void setBlurAmount(ArtifactCore::Units::Percent amount);

  // Normalized values consumed by the future depth-of-field render pass.
  CameraDOFParameters depthOfFieldParameters() const;

  // Projection mode
  ProjectionMode projectionMode() const;
  void setProjectionMode(ProjectionMode mode);

  StereoMode stereoMode() const;
  void setStereoMode(StereoMode mode);

  // Perspective-specific
  ArtifactCore::Units::Degrees fov() const;
  void setFov(ArtifactCore::Units::Degrees fov);
  bool useManualFov() const;
  void setUseManualFov(bool enable);
  void resetFovToZoom();

  // 35mm-equivalent focal length (AE-compatible unit system). Horizontal
  // FOV over a 36mm sensor width: fov = 2*atan(18/focalLength).
  // Reading derives from the effective FOV; writing switches to manual FOV.
  ArtifactCore::Units::Millimeters focalLength() const;
  void setFocalLength(ArtifactCore::Units::Millimeters mm);

  // Orthographic-specific
  ArtifactCore::Units::Pixels orthoWidth() const;
  void setOrthoWidth(ArtifactCore::Units::Pixels width);

  ArtifactCore::Units::Pixels orthoHeight() const;
  void setOrthoHeight(ArtifactCore::Units::Pixels height);

  // Clipping planes
  ArtifactCore::Units::Pixels nearClipPlane() const;
  void setNearClipPlane(ArtifactCore::Units::Pixels distance);

  ArtifactCore::Units::Pixels farClipPlane() const;
  void setFarClipPlane(ArtifactCore::Units::Pixels distance);

  ArtifactCore::Units::Meters ipd() const;
  void setIpd(ArtifactCore::Units::Meters ipd);

  // Composition camera selection.  Several cameras may be visible for
  // editing, while only the enabled camera with the highest priority drives
  // the composition render.
  bool isActiveCamera() const;
  void setActiveCamera(bool active);
  int cameraPriority() const;
  void setCameraPriority(int priority);

  // Two-node camera: the POI is authored in the camera parent's coordinate
  // space (the world space for an unparented camera).
  bool pointOfInterestEnabled() const;
  void setPointOfInterestEnabled(bool enabled);
  ArtifactCore::Coordinates::LayerParentPoint3 pointOfInterest() const;
  void setPointOfInterest(ArtifactCore::Coordinates::LayerParentPoint3 poi);
  ArtifactCore::Coordinates::WorldPoint3 pointOfInterestWorld() const;
  bool setPointOfInterestWorld(
      ArtifactCore::Coordinates::WorldPoint3 worldPoi);

  // Global transform with POI orientation applied (identity-equivalent to
  // getGlobalTransform4x4() when the POI is disabled).
  QMatrix4x4 effectiveGlobalTransform() const;

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

 using ArtifactCameraLayerPtr = SharedPtr<ArtifactCameraLayer>;

} // namespace Artifact
