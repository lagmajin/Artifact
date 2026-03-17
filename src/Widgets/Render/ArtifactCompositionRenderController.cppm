module;
#include <memory>
#include <QTimer>
#include <wobjectimpl.h>

module Artifact.Widgets.CompositionRenderController;

import Artifact.Render.IRenderer;
import Artifact.Preview.Pipeline;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Widgets.TransformGizmo;
import Utils.Id;
import Artifact.Service.Project;

namespace Artifact {

W_OBJECT_IMPL(CompositionRenderController)

class CompositionRenderController::Impl {
public:
 std::unique_ptr<ArtifactIRenderer> renderer_;
 ArtifactPreviewCompositionPipeline previewPipeline_;
 std::unique_ptr<TransformGizmo> gizmo_;
 QTimer* renderTimer_ = nullptr;
 bool initialized_ = false;
 bool running_ = false;
};

CompositionRenderController::CompositionRenderController(QObject* parent)
 : QObject(parent), impl_(new Impl())
{
 impl_->gizmo_ = std::make_unique<TransformGizmo>();
 
 // Connect to project service to track layer selection
 if (auto* svc = ArtifactProjectService::instance()) {
  connect(svc, &ArtifactProjectService::layerSelected, this, [this](const LayerID& id) {
   setSelectedLayerId(id);
   auto comp = impl_->previewPipeline_.composition();
   if (comp) {
    impl_->gizmo_->setLayer(comp->layerById(id));
   } else {
    impl_->gizmo_->setLayer(nullptr);
   }
  });
 }
}

CompositionRenderController::~CompositionRenderController()
{
 destroy();
 delete impl_;
}

void CompositionRenderController::initialize(QWidget* hostWidget)
{
 if (impl_->initialized_ || hostWidget == nullptr) {
  return;
 }

 impl_->renderer_ = std::make_unique<ArtifactIRenderer>();
 impl_->renderer_->initialize(hostWidget);
 impl_->renderer_->setViewportSize((float)hostWidget->width(), (float)hostWidget->height());
 const auto comp = impl_->previewPipeline_.composition();
 if (comp) {
  auto size = comp->settings().compositionSize();
  impl_->renderer_->setCanvasSize((float)size.width(), (float)size.height());
  impl_->renderer_->fitToViewport();
 }

 impl_->renderTimer_ = new QTimer(this);
 impl_->renderTimer_->setTimerType(Qt::PreciseTimer);
 connect(impl_->renderTimer_, &QTimer::timeout, this, &CompositionRenderController::renderOneFrame);

 impl_->initialized_ = true;
}

void CompositionRenderController::destroy()
{
 stop();
 if (impl_->renderer_) {
  impl_->renderer_->destroy();
  impl_->renderer_.reset();
 }
 impl_->initialized_ = false;
}

bool CompositionRenderController::isInitialized() const
{
 return impl_->initialized_;
}

void CompositionRenderController::start()
{
 if (!impl_->initialized_ || impl_->running_ || impl_->renderTimer_ == nullptr) {
  return;
 }
 impl_->running_ = true;
 impl_->renderTimer_->start(16);
}

void CompositionRenderController::stop()
{
 if (!impl_->running_) {
  return;
 }
 impl_->running_ = false;
 if (impl_->renderTimer_) {
  impl_->renderTimer_->stop();
 }
 if (impl_->renderer_) {
  impl_->renderer_->flushAndWait();
 }
}

bool CompositionRenderController::isRunning() const
{
 return impl_->running_;
}

void CompositionRenderController::recreateSwapChain(QWidget* hostWidget)
{
 if (!impl_->initialized_ || !impl_->renderer_ || hostWidget == nullptr) {
  return;
 }
 impl_->renderer_->recreateSwapChain(hostWidget);
}

void CompositionRenderController::setViewportSize(float width, float height)
{
 if (!impl_->renderer_) {
  return;
 }
 impl_->renderer_->setViewportSize(width, height);
}

void CompositionRenderController::panBy(const QPointF& viewportDelta)
{
 if (!impl_->renderer_) {
  return;
 }
 impl_->renderer_->panBy((float)viewportDelta.x(), (float)viewportDelta.y());
}

void CompositionRenderController::setComposition(ArtifactCompositionPtr composition)
{
 impl_->previewPipeline_.setComposition(composition);
 if (composition && impl_->renderer_) {
  auto size = composition->settings().compositionSize();
  impl_->renderer_->setCanvasSize((float)size.width(), (float)size.height());
  impl_->renderer_->fitToViewport();
 }
}

ArtifactCompositionPtr CompositionRenderController::composition() const
{
 return impl_->previewPipeline_.composition();
}

void CompositionRenderController::setSelectedLayerId(const LayerID& id)
{
 impl_->previewPipeline_.setSelectedLayerId(id);
}

void CompositionRenderController::setClearColor(const FloatColor& color)
{
 if (!impl_->renderer_) {
  return;
 }
 impl_->renderer_->setClearColor(color);
}

void CompositionRenderController::resetView()
{
 if (!impl_->renderer_) {
  return;
 }
 impl_->renderer_->resetView();
}

void CompositionRenderController::zoomInAt(const QPointF& viewportPos)
{
 if (!impl_->renderer_) {
  return;
 }
 impl_->renderer_->zoomAroundViewportPoint({(float)viewportPos.x(), (float)viewportPos.y()}, 1.1f);
}

void CompositionRenderController::zoomOutAt(const QPointF& viewportPos)
{
 if (!impl_->renderer_) {
  return;
 }
 impl_->renderer_->zoomAroundViewportPoint({(float)viewportPos.x(), (float)viewportPos.y()}, 0.909f);
}

void CompositionRenderController::zoomFit()
{
 if (!impl_->renderer_) {
  return;
 }
 impl_->renderer_->fitToViewport();
}

void CompositionRenderController::zoom100()
{
 if (!impl_->renderer_) {
  return;
 }
 impl_->renderer_->setZoom(1.0f);
}

void CompositionRenderController::handleMousePress(const QPointF& viewportPos)
{
 if (impl_->gizmo_ && impl_->renderer_) {
  impl_->gizmo_->handleMousePress(viewportPos, impl_->renderer_.get());
 }
}

void CompositionRenderController::handleMouseMove(const QPointF& viewportPos)
{
 if (impl_->gizmo_ && impl_->renderer_ && impl_->gizmo_->isDragging()) {
  impl_->gizmo_->handleMouseMove(viewportPos, impl_->renderer_.get());
 }
}

void CompositionRenderController::handleMouseRelease()
{
 if (impl_->gizmo_) {
  impl_->gizmo_->handleMouseRelease();
 }
}

TransformGizmo* CompositionRenderController::gizmo() const
{
 return impl_->gizmo_.get();
}

void CompositionRenderController::renderOneFrame()
{
 if (!impl_->initialized_ || !impl_->renderer_) {
  return;
 }

 auto comp = impl_->previewPipeline_.composition();
 if (comp) {
  auto size = comp->settings().compositionSize();
  impl_->renderer_->setCanvasSize((float)size.width(), (float)size.height());
  impl_->previewPipeline_.setCurrentFrame(comp->framePosition().framePosition());
 }

 impl_->previewPipeline_.render(impl_->renderer_.get());
 
 // Draw Gizmo on top
 if (impl_->gizmo_) {
  impl_->gizmo_->draw(impl_->renderer_.get());
 }

 impl_->renderer_->present();
}

} // namespace Artifact
