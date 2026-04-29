module;
#include <utility>

#include <wobjectdefs.h>
#include <QObject>
#include <QCursor>
#include <QMouseEvent>
#include <QPointF>
#include <QMatrix4x4>
#include <QVector3D>
#include <QVector>
#include <QRectF>
#include <QString>
#include <QStringList>

export module Artifact.Widgets.CompositionRenderController;

import Color.Float;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Grid.System;
import Frame.Debug;
import Artifact.Service.Project;
import Artifact.Render.Queue.Service;
import Artifact.Preview.Pipeline;
import Artifact.Widgets.Gizmo3D;
import Artifact.Widgets.PieMenu;
import Geometry.CameraGuide;
import Utils.Id;
import Artifact.LOD.Manager;
import Artifact.Widgets.TransformGizmo;
import UI.View.Orientation.Navigator;

export namespace Artifact {
 using namespace ArtifactCore;

 enum class CompositionBackgroundMode {
  Solid,
  Checkerboard,
  MayaGradient
 };

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
FloatColor clearColor() const;
void setCompositionBackgroundMode(int mode);
int compositionBackgroundMode() const;

void setShowGrid(bool show);
bool isShowGrid() const;
void setShowCheckerboard(bool show);
bool isShowCheckerboard() const;
void setCheckerboardSize(float size);
float checkerboardSize() const;
void setGridSettings(const Artifact::Grid::GridSettings& settings);
Artifact::Grid::GridSettings gridSettings() const;
void setShowGuides(bool show);
bool isShowGuides() const;
void setShowSafeMargins(bool show);
bool isShowSafeMargins() const;
void setGpuBlendEnabled(bool enabled);
bool isGpuBlendEnabled() const;
void setShowMotionPathOverlay(bool show);
bool isShowMotionPathOverlay() const;
bool setSelectedLayerMotionPathKeyframeAtCurrentFrame();
bool removeSelectedLayerMotionPathKeyframeAtCurrentFrame();
bool setSelectedLayerMotionPathInterpolationAtCurrentFrame(int interpolationType);
void setShowAnchorCenterOverlay(bool show);
bool isShowAnchorCenterOverlay() const;
void setShowCameraFrustumOverlay(bool show);
bool isShowCameraFrustumOverlay() const;

   // Render Queue support: when active, composition changed signals do not invalidate caches
   void setRenderQueueActive(bool active);
   bool isRenderQueueActive() const;

// Ghost previews are composited in the controller render pass, not by a QWidget overlay.
void setDropGhostPreview(const QRectF& viewportRect,
                         const QString& title,
                         const QString& hint,
                         const QString& label);
void clearDropGhostPreview();

// Lightweight info HUD used for selection / operation feedback.
void setInfoOverlayText(const QString& title, const QString& detail = QString());
void clearInfoOverlayText();
void showCommandPaletteOverlay(const QString& query, const QStringList& items);
void showContextMenuOverlay(const QPointF& viewportPos, const QStringList& items,
                           const QString& title = QString(),
                           const QString& subtitle = QString(),
                           const QVector<bool>& enabledStates = QVector<bool>());
void showPieMenuOverlay(const PieMenuModel& model, const QPointF& viewportPos);
void hideViewportOverlay();
bool isViewportOverlayVisible() const;
int viewportOverlayItemAt(const QPointF& viewportPos) const;
QString confirmPieMenuOverlaySelection();
void updatePieMenuOverlayMousePos(const QPointF& viewportPos);
void cancelPieMenuOverlay();
bool isPieMenuOverlayVisible() const;

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
void zoomAtFactor(const QPointF& viewportPos, float factor);
void zoomFit();
void zoomFill();
  void zoom100();
  void focusSelectedLayer();
  void setGizmoMode(TransformGizmo::Mode mode);
  TransformGizmo::Mode gizmoMode() const;
  LayerID layerAtViewportPos(const QPointF& viewportPos) const;
  ArtifactIRenderer* renderer() const;
  ArtifactCore::FrameDebugSnapshot frameDebugSnapshot() const;
  double lastFrameTimeMs() const;
  double averageFrameTimeMs() const;

void handleMousePress(QMouseEvent* event);
void handleMouseMove(const QPointF& viewportPos);
  void handleMouseRelease();
  bool hasPendingMaskEdit() const;

TransformGizmo* gizmo() const;
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
 void setViewportOrientation(ArtifactCore::ViewOrientationHotspot hotspot);
 ArtifactCore::ViewOrientationHotspot viewportOrientation() const;
 Ray createPickingRay(const QPointF& viewportPos) const;
 Qt::CursorShape cursorShapeForViewportPos(const QPointF& viewportPos) const;

public /*slots*/:

  void renderOneFrame();
   void onRenderDebounceTimeout();

  // Mark render state as dirty. The fixed-rate render tick (~60fps) will
  // pick up the change and render on the next tick. Use this instead of
  // renderOneFrame() for high-frequency events (mouse move, drag, etc.)
  // to avoid the forced 16ms scheduling delay.
  void markRenderDirty();

signals:
  void videoDebugMessage(const QString& msg) W_SIGNAL(videoDebugMessage, msg);
 };
}
