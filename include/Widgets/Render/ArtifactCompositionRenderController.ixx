module;
#include <QObject>
#include <QCursor>
#include <QMouseEvent>
#include <QPointF>
#include <QMatrix4x4>
#include <QVector3D>
#include <QVector>
#include <QRectF>
#include <QString>
#include <wobjectdefs.h>

export module Artifact.Widgets.CompositionRenderController;

import Color.Float;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Service.Project;
import Artifact.Preview.Pipeline;
import Artifact.Widgets.Gizmo3D;
import Geometry.CameraGuide;
import Utils.Id;
import Artifact.LOD.Manager;

export namespace Artifact {
 using namespace ArtifactCore;

 class CompositionRenderController : public QObject
 {
  W_OBJECT(CompositionRenderController)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit CompositionRenderController(QObject* parent = nullptr);
  ~CompositionRenderController();

  void initialize(QWidget* hostWidget);
  void destroy();
  bool isInitialized() const;

  void start();
  void stop();
  bool isRunning() const;

  void recreateSwapChain(QWidget* hostWidget);
  void setViewportSize(float width, float height);
void setPreviewQualityPreset(PreviewQualityPreset preset);
void panBy(const QPointF& viewportDelta);
void notifyViewportInteractionActivity();
void finishViewportInteraction();

  void setComposition(ArtifactCompositionPtr composition);
  ArtifactCompositionPtr composition() const;
  void setSelectedLayerId(const LayerID& id);
  LayerID selectedLayerId() const;
void setClearColor(const FloatColor& color);

void setShowGrid(bool show);
bool isShowGrid() const;
void setShowCheckerboard(bool show);
bool isShowCheckerboard() const;
void setShowGuides(bool show);
bool isShowGuides() const;
void setShowSafeMargins(bool show);
bool isShowSafeMargins() const;
void setGpuBlendEnabled(bool enabled);
bool isGpuBlendEnabled() const;
void setShowMotionPathOverlay(bool show);
bool isShowMotionPathOverlay() const;

// Ghost previews are composited in the controller render pass, not by a QWidget overlay.
void setDropGhostPreview(const QRectF& viewportRect,
                         const QString& title,
                         const QString& hint,
                         const QString& label);
void clearDropGhostPreview();

// LOD (Level of Detail)
LODManager* lodManager() const;
void setLODEnabled(bool enabled);
bool isLODEnabled() const;

// ROI Debug
void setDebugMode(bool enabled);
bool isDebugMode() const;

void resetView();
void zoomInAt(const QPointF& viewportPos);
void zoomOutAt(const QPointF& viewportPos);
void zoomFit();
  void zoom100();
  void focusSelectedLayer();
  LayerID layerAtViewportPos(const QPointF& viewportPos) const;
  ArtifactIRenderer* renderer() const;

void handleMousePress(QMouseEvent* event);
void handleMouseMove(const QPointF& viewportPos);
  void handleMouseRelease();
  bool hasPendingMaskEdit() const;

class TransformGizmo* gizmo() const;
 class Artifact3DGizmo* gizmo3D() const;
 struct CameraFrustumVisual {
  bool valid = false;
  LayerID layerId = LayerID::Nil();
  QVector3D cameraPosition;
  QMatrix4x4 viewMatrix;
  QMatrix4x4 projectionMatrix;
  ArtifactCore::CameraGuidePrimitive guide;
  QVector<QVector3D> nearPlaneCorners;
  QVector<QVector3D> farPlaneCorners;
  float aspect = 1.0f;
  float zoom = 0.0f;
 };
 CameraFrustumVisual cameraFrustumVisual() const;
 Ray createPickingRay(const QPointF& viewportPos) const;
 Qt::CursorShape cursorShapeForViewportPos(const QPointF& viewportPos) const;

public /*slots*/:

  void renderOneFrame();

signals:
  void videoDebugMessage(const QString& msg) W_SIGNAL(videoDebugMessage, msg);
 };
}
