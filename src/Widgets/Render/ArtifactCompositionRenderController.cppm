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
import Artifact.Service.Playback; // 追加
import Color.Float;

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

 LayerID selectedLayerId_;
 bool showGrid_ = false;
 bool showGuides_ = false;
 bool showSafeMargins_ = false;
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
   renderOneFrame();
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

 // PlaybackService のフレーム変更に合わせて再描画
 if (auto* playback = ArtifactPlaybackService::instance()) {
  connect(playback, &ArtifactPlaybackService::frameChanged, this, [this]() {
   renderOneFrame();
  });
 }

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
  
  // 各レイヤーの変更を監視
  for (auto& layer : composition->allLayer()) {
   if (layer) {
    connect(layer.get(), &ArtifactAbstractLayer::changed, this, [this]() {
     renderOneFrame();
    });
   }
  }

  // コンポジションがセットされた瞬間に1フレーム描画
  renderOneFrame();
 }
}

ArtifactCompositionPtr CompositionRenderController::composition() const
{
 return impl_->previewPipeline_.composition();
}

void CompositionRenderController::setSelectedLayerId(const LayerID& id)
{
 impl_->selectedLayerId_ = id;
}

void CompositionRenderController::setClearColor(const FloatColor& color)
{
 if (impl_->renderer_) {
  impl_->renderer_->setClearColor(color);
 }
}

void CompositionRenderController::setShowGrid(bool show) { impl_->showGrid_ = show; renderOneFrame(); }
bool CompositionRenderController::isShowGrid() const { return impl_->showGrid_; }
void CompositionRenderController::setShowGuides(bool show) { impl_->showGuides_ = show; renderOneFrame(); }
bool CompositionRenderController::isShowGuides() const { return impl_->showGuides_; }
void CompositionRenderController::setShowSafeMargins(bool show) { impl_->showSafeMargins_ = show; renderOneFrame(); }
bool CompositionRenderController::isShowSafeMargins() const { return impl_->showSafeMargins_; }

void CompositionRenderController::resetView()
{
 if (impl_->renderer_) {
  impl_->renderer_->resetView();
  renderOneFrame();
 }
}

void CompositionRenderController::zoomInAt(const QPointF& viewportPos)
{
 if (impl_->renderer_) {
  impl_->renderer_->zoomAroundViewportPoint({ (float)viewportPos.x(), (float)viewportPos.y() }, -1.0f);
  renderOneFrame();
 }
}

void CompositionRenderController::zoomOutAt(const QPointF& viewportPos)
{
 if (impl_->renderer_) {
  impl_->renderer_->zoomAroundViewportPoint({ (float)viewportPos.x(), (float)viewportPos.y() }, -1.0f);
  renderOneFrame();
 }
}

void CompositionRenderController::zoomFit()
{
 if (impl_->renderer_) {
  impl_->renderer_->fitToViewport();
  renderOneFrame();
 }
}

void CompositionRenderController::zoom100()
{
 if (impl_->renderer_) {
  impl_->renderer_->setZoom(1.0f);
  renderOneFrame();
 }
}

void CompositionRenderController::handleMousePress(const QPointF& viewportPos)
{
 if (impl_->gizmo_) {
  impl_->gizmo_->handleMousePress(viewportPos, impl_->renderer_.get());
 }
}

void CompositionRenderController::handleMouseMove(const QPointF& viewportPos)
{
 if (impl_->gizmo_) {
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
  const float cw = static_cast<float>(size.width());
  const float ch = static_cast<float>(size.height());

  // コンポジションの背景色でクリア
  const FloatColor bgColor = comp->backgroundColor();
  impl_->renderer_->setClearColor(bgColor);
  impl_->renderer_->clear();

  // 1. 背景の描画 (チェッカーボード)
  // 1. 背景の描画 (チェッカーボードは透過部分の確認用に残す場合は背景色の上に描く)
  impl_->renderer_->drawCheckerboard(0, 0, cw, ch, 16.0f,
                                     { 0.15f, 0.15f, 0.15f, 1.0f },
                                     { 0.20f, 0.20f, 0.20f, 1.0f });

  // 2. グリッド描画
  if (impl_->showGrid_) {
      impl_->renderer_->drawGrid(0, 0, cw, ch, 100.0f, 1.0f, { 0.3f, 0.3f, 0.3f, 0.5f });
  }

  // 3. 全レイヤーの描画 (奥から手前へ)
  auto layers = comp->allLayer();
  for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
   auto& layer = *it;
   if (layer && layer->isVisible()) {
    layer->draw(impl_->renderer_.get());
   }
  }

  // 4. セーフマージンの描画
  if (impl_->showSafeMargins_) {
      const float actionSafeW = cw * 0.9f;
      const float actionSafeH = ch * 0.9f;
      const float titleSafeW = cw * 0.8f;
      const float titleSafeH = ch * 0.8f;
      const FloatColor marginColor = { 0.5f, 0.5f, 0.5f, 0.6f };

      // Action Safe (90%)
      impl_->renderer_->drawRectOutline((cw - actionSafeW) * 0.5f, (ch - actionSafeH) * 0.5f,
                                        actionSafeW, actionSafeH, marginColor);
      // Title Safe (80%)
      impl_->renderer_->drawRectOutline((cw - titleSafeW) * 0.5f, (ch - titleSafeH) * 0.5f,
                                        titleSafeW, titleSafeH, marginColor);
      
      // 中央の十字
      const float crossSize = 20.0f;
      impl_->renderer_->drawSolidLine({cw*0.5f - crossSize, ch*0.5f}, {cw*0.5f + crossSize, ch*0.5f}, marginColor, 1.0f);
      impl_->renderer_->drawSolidLine({cw*0.5f, ch*0.5f - crossSize}, {cw*0.5f, ch*0.5f + crossSize}, marginColor, 1.0f);
  }
 } else {
  impl_->renderer_->clear();
 }
 
 // 最前面にギズモを描画
 if (impl_->gizmo_) {
  impl_->gizmo_->draw(impl_->renderer_.get());
 }

 impl_->renderer_->present();
}

} // namespace Artifact
