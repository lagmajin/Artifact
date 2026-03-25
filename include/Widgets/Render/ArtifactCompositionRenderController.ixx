module;
#include <QObject>
#include <QCursor>
#include <QMouseEvent>
#include <QPointF>
#include <wobjectdefs.h>

export module Artifact.Widgets.CompositionRenderController;

import Color.Float;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Service.Project;
import Artifact.Preview.Pipeline;
import Utils.Id;

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

void resetView();
void zoomInAt(const QPointF& viewportPos);
void zoomOutAt(const QPointF& viewportPos);
void zoomFit();
void zoom100();
void focusSelectedLayer();
LayerID layerAtViewportPos(const QPointF& viewportPos) const;

void handleMousePress(QMouseEvent* event);
void handleMouseMove(const QPointF& viewportPos);
  void handleMouseRelease();

class TransformGizmo* gizmo() const;
 Qt::CursorShape cursorShapeForViewportPos(const QPointF& viewportPos) const;

public /*slots*/:

  void renderOneFrame();

signals:
  void videoDebugMessage(const QString& msg) W_SIGNAL(videoDebugMessage, msg);
 };
}
