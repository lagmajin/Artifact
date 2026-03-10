module;
#include <QObject>
#include <QPointF>
#include <wobjectdefs.h>

export module Artifact.Widgets.CompositionRenderController;

import Color.Float;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
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
  void panBy(const QPointF& viewportDelta);

  void setComposition(ArtifactCompositionPtr composition);
  ArtifactCompositionPtr composition() const;
  void setSelectedLayerId(const LayerID& id);

  void setClearColor(const FloatColor& color);
  void resetView();
  void zoomInAt(const QPointF& viewportPos);
  void zoomOutAt(const QPointF& viewportPos);
  void zoomFit();
  void zoom100();

  void renderOneFrame();
 };
}
