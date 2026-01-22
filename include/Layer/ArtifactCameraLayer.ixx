module ;
#include <QVector3D>
#include <QQuaternion>
#include <QMatrix4x4>
#include <QSizeF>
#include <QString>
#include <memory>

export module Artifact.Layer.Camera;

import Core.Camera;

export namespace ArtifactCore {

 struct CameraTransform {
  QVector3D position{0.0f, 0.0f, 0.0f};
  QQuaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};
  QVector3D scale{1.0f, 1.0f, 1.0f};

  QMatrix4x4 toMatrix() const {
    QMatrix4x4 m;
    m.translate(position);
    m.rotate(rotation);
    m.scale(scale);
    return m;
  }
 };

 // AfterEffects 風のカメラレイヤー API（最小限）
 class ArtifactCameraLayer {
 public:
  ArtifactCameraLayer();
  ~ArtifactCameraLayer();

  // identity
  void setName(const QString& name);
  QString name() const;

  // enable/visibility
  void setEnabled(bool e);
  bool isEnabled() const;

  // transform
  void setTransform(const CameraTransform& t);
  CameraTransform transform() const;

  // camera settings (use core Camera)
  void setCamera(const Camera& c);
  Camera camera() const;

  // matrices (world -> view, projection)
  QMatrix4x4 viewMatrix() const;         // camera world -> view
  QMatrix4x4 projectionMatrix() const;   // projection for current settings
  QMatrix4x4 viewProjectionMatrix() const;

  // evaluate at time (placeholder for animated parameters)
  void evaluateAtTime(double timeSeconds);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
 };

 using ArtifactCameraLayerPtr = std::shared_ptr<ArtifactCameraLayer>;

}