module;
#include <utility>

#include <wobjectdefs.h>
#include <QObject>
#include <QCursor>
#include <QImage>
#include <QMouseEvent>
#include <QPointF>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>
#include <QVector>
#include <QRectF>
#include <QString>
#include <QStringList>

export module Artifact.Widgets.CompositionRenderController;

import Color.Float;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
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
import Artifact.Widgets.PointTrackerGizmo;
import UI.View.Orientation.Navigator;

export namespace Artifact {
 using namespace ArtifactCore;

 enum class CompositionCompareMode {
  Off,
  A,
  B,
  Diff
 };

 enum class CompositionBackgroundMode {
  Solid,
  Checkerboard,
  MayaGradient,
  Skybox
 };

 enum class ViewportChannelDisplayMode {
 Color,
 Alpha,
  ColorAlpha,
 Red,
 Green,
  Blue,
  Depth,
  Emission,
  ObjectId,
  MaterialId,
  Albedo,
  AlbedoR,
  AlbedoG,
  AlbedoB,
  Normal,
  NormalX,
  NormalY,
  NormalZ,
  Velocity,
  VelocityX,
  VelocityY
 };

 enum class LineDebugKind : uint8_t {
  Grid = 0,
  Axis,
  Bounds,
  MaskPath,
  MaskHandle,
  SelectionRect,
  MotionPath,
  DebugProbe,
  Unknown
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
void setCompareMode(CompositionCompareMode mode);
CompositionCompareMode compareMode() const;
void setReferencePinned(bool pinned);
bool isReferencePinned() const;
void setReferenceFrame(int frame);
int referenceFrame() const;
void setClearColor(const FloatColor& color);
FloatColor clearColor() const;
void setCompositionBackgroundMode(int mode);
int compositionBackgroundMode() const;

void setShowGrid(bool show);
bool isShowGrid() const;
void setLineDebugKindVisible(LineDebugKind kind, bool visible);
bool isLineDebugKindVisible(LineDebugKind kind) const;
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
void setShowEffectHitboxOverlay(bool show);
bool isShowEffectHitboxOverlay() const;
void setShowDensityHeatmapOverlay(bool show);
bool isShowDensityHeatmapOverlay() const;
void setReferenceOverlayImage(const QImage& image);
void clearReferenceOverlayImage();
bool hasReferenceOverlayImage() const;
void setShowReferenceOverlay(bool show);
bool isShowReferenceOverlay() const;
void setShowColorSamplerOverlay(bool show);
bool isShowColorSamplerOverlay() const;
void setShowAutoColorPaletteOverlay(bool show);
bool isShowAutoColorPaletteOverlay() const;
void setViewportChannelDisplayMode(ViewportChannelDisplayMode mode);
ViewportChannelDisplayMode viewportChannelDisplayMode() const;
bool setSelectedLayerMotionPathKeyframeAtCurrentFrame();
bool removeSelectedLayerMotionPathKeyframeAtCurrentFrame();
bool setSelectedLayerMotionPathInterpolationAtCurrentFrame(int interpolationType);
void setShowAnchorCenterOverlay(bool show);
bool isShowAnchorCenterOverlay() const;
void setShowCameraFrustumOverlay(bool show);
bool isShowCameraFrustumOverlay() const;
void setShowGizmoOverlay(bool show);
bool isShowGizmoOverlay() const;
void setShowXRayOverlay(bool show);
bool isShowXRayOverlay() const;
void setShowIsolationOverlay(bool show);
bool isShowIsolationOverlay() const;
void setShowOnionSkin(bool show);
bool isShowOnionSkin() const;
void setOnionSkinFrameCount(int count);
int onionSkinFrameCount() const;
void setOnionSkinOpacity(int percent);
int onionSkinOpacity() const;

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
bool placeWorkCursorAtViewportPos(const QPointF& viewportPos);
void setWorkCursorCanvasPosition(const QPointF& canvasPos);
QPointF workCursorCanvasPosition() const;
void setWorkCursorVisible(bool visible);
bool isWorkCursorVisible() const;
void clearWorkCursor();
void hideViewportOverlay();
bool isViewportOverlayVisible() const;
bool isContextMenuOverlayVisible() const;
int viewportOverlayItemAt(const QPointF& viewportPos) const;
QString confirmPieMenuOverlaySelection();
void updatePieMenuOverlayMousePos(const QPointF& viewportPos);
void updateContextMenuOverlayMousePos(const QPointF& viewportPos);
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
  bool createFullLayerMaskForLayer(const ArtifactAbstractLayerPtr& layer);
  bool cyclePresetLayerMaskForLayer(const ArtifactAbstractLayerPtr& layer, bool reverse = false);
  void setGizmoMode(TransformGizmo::Mode mode);
  TransformGizmo::Mode gizmoMode() const;
  LayerID layerAtViewportPos(const QPointF& viewportPos) const;
  ArtifactIRenderer* renderer() const;
  QImage captureCurrentFrameImage() const;
  ArtifactCore::FrameDebugSnapshot frameDebugSnapshot() const;
  double lastFrameTimeMs() const;
  double averageFrameTimeMs() const;

void handleMousePress(QMouseEvent* event);
void handleMouseMove(const QPointF& viewportPos);
  void handleMouseRelease();
  bool hasPendingMaskEdit() const;

TransformGizmo* gizmo() const;
 class Artifact3DGizmo* gizmo3D() const;
 ArtifactPointTrackerGizmo* trackerGizmo() const;
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
 QQuaternion viewportOrientationQuaternion() const;
 void setViewportOrientationQuaternion(const QQuaternion& orientation);
 Ray createPickingRay(const QPointF& viewportPos) const;
  Qt::CursorShape cursorShapeForViewportPos(const QPointF& viewportPos) const;

  // Tracker operations (TrackPoint tool)
  void trackerInitialize();
  void trackerTrackForward();
  void trackerTrackBackward();
  void trackerTrackAll();
  void trackerApplyToPosition();
  void trackerApplyToAnchor();
  void trackerDelete();

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


