module;
#include <QDebug>
#include <QColor>
#include <QImage>
#include <QLoggingCategory>
#include <QRectF>
#include <QTimer>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <memory>
#include <wobjectimpl.h>

module Artifact.Widgets.CompositionRenderController;

import Artifact.Render.IRenderer;
import Artifact.Render.CompositionRenderer;
import Artifact.Preview.Pipeline;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Video;
import Artifact.Layer.Solid2D;
import Artifact.Layers.SolidImage;
import Artifact.Layer.Text;
import Artifact.Application.Manager;
import Artifact.Widgets.TransformGizmo;
import Utils.Id;
import Artifact.Service.Project;
import Artifact.Service.Playback; // 追加
import Frame.Position;
import Color.Float;
import Image;
import CvUtils;

namespace Artifact {

W_OBJECT_IMPL(CompositionRenderController)

namespace {
Q_LOGGING_CATEGORY(compositionViewLog, "artifact.compositionview")

QColor toQColor(const FloatColor& color)
{
  return QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
}

ArtifactCompositionPtr
resolvePreferredComposition(ArtifactProjectService *service) {
  // ProjectService を最優先
  if (service) {
    if (auto comp = service->currentComposition().lock()) {
      return comp;
    }
  }

  // フォールバック: ActiveContextService
  if (auto *app = ArtifactApplicationManager::instance()) {
    if (auto *active = app->activeContextService()) {
      if (auto comp = active->activeComposition()) {
        return comp;
      }
    }
  }

  // フォールバック: PlaybackService
  if (auto *playback = ArtifactPlaybackService::instance()) {
    if (auto comp = playback->currentComposition()) {
      return comp;
    }
  }

  return ArtifactCompositionPtr();
}

void drawLayerForCompositionView(const ArtifactAbstractLayerPtr &layer,
                                 ArtifactIRenderer *renderer) {
  if (!layer || !renderer) {
    qCDebug(compositionViewLog) << "[CompositionView] drawLayerForCompositionView: invalid "
                "layer/renderer";
    return;
  }

  const QRectF localRect = layer->localBounds();
  if (!localRect.isValid() || localRect.width() <= 0.0 ||
      localRect.height() <= 0.0) {
    qCDebug(compositionViewLog) << "[CompositionView] skip layer: invalid local bounds"
             << "id=" << layer->id().toString() << "rect=" << localRect;
    return;
  }

  const QRectF worldRect = layer->getGlobalTransform().mapRect(localRect);
  if (!worldRect.isValid() || worldRect.width() <= 0.0 ||
      worldRect.height() <= 0.0) {
    qCDebug(compositionViewLog) << "[CompositionView] skip layer: invalid world rect"
             << "id=" << layer->id().toString() << "rect=" << worldRect;
    return;
  }

  const auto viewportTopLeft =
      renderer->canvasToViewport({static_cast<float>(worldRect.left()),
                                  static_cast<float>(worldRect.top())});
  const auto viewportBottomRight =
      renderer->canvasToViewport({static_cast<float>(worldRect.right()),
                                  static_cast<float>(worldRect.bottom())});
  qCDebug(compositionViewLog) << "[CompositionView] layer geometry"
           << "id=" << layer->id().toString()
           << "type=" << layer->type_index().name() << "local=" << localRect
           << "worldRect=" << worldRect
           << "viewportRect=(" << viewportTopLeft.x << "," << viewportTopLeft.y
           << ")-(" << viewportBottomRight.x << "," << viewportBottomRight.y
           << ")";

  auto applyRasterizerEffectsToSurface = [](const ArtifactAbstractLayerPtr& targetLayer,
                                            QImage& surface) {
    if (!targetLayer || surface.isNull()) {
      return;
    }

    const auto effects = targetLayer->getEffects();
    bool hasRasterizerEffect = false;
    for (const auto& effect : effects) {
      if (effect && effect->isEnabled() &&
          effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
        hasRasterizerEffect = true;
        break;
      }
    }

    if (!hasRasterizerEffect) {
      return;
    }

    ArtifactCore::ImageF32x4_RGBA cpuImage;
    cpuImage.setFromCVMat(ArtifactCore::CvUtils::qImageToCvMat(surface, true));
    ArtifactCore::ImageF32x4RGBAWithCache current(cpuImage);

    for (const auto& effect : effects) {
      if (!effect || !effect->isEnabled() ||
          effect->pipelineStage() != EffectPipelineStage::Rasterizer) {
        continue;
      }

      ArtifactCore::ImageF32x4RGBAWithCache next;
      effect->applyCPUOnly(current, next);
      current = next;
    }

    surface = current.image().toQImage();
  };

  auto applySurfaceAndDraw = [&](QImage surface, const QRectF& targetRect) {
    if (surface.isNull()) {
      return false;
    }
    applyRasterizerEffectsToSurface(layer, surface);
    renderer->drawSprite(static_cast<float>(targetRect.x()),
                         static_cast<float>(targetRect.y()),
                         static_cast<float>(targetRect.width()),
                         static_cast<float>(targetRect.height()), surface,
                         layer->opacity());
    return true;
  };

  if (const auto solid2D =
          std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer)) {
    const auto color = solid2D->color();
    qCDebug(compositionViewLog) << "[CompositionView] draw solid2d"
             << "id=" << layer->id().toString() << "color=(" << color.r() << ","
             << color.g() << "," << color.b() << "," << color.a() << ")";
    const QSize surfaceSize(
        std::max(1, static_cast<int>(std::ceil(localRect.width()))),
        std::max(1, static_cast<int>(std::ceil(localRect.height()))));
    QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
    surface.fill(toQColor(color));
    if (!applySurfaceAndDraw(surface, worldRect)) {
      renderer->drawSolidRect(static_cast<float>(worldRect.x()),
                              static_cast<float>(worldRect.y()),
                              static_cast<float>(worldRect.width()),
                              static_cast<float>(worldRect.height()),
                              solid2D->color(), layer->opacity());
    }
    return;
  }

  if (const auto solidImage =
          std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer)) {
    const auto color = solidImage->color();
    qCDebug(compositionViewLog) << "[CompositionView] draw solid-image"
             << "id=" << layer->id().toString() << "color=(" << color.r() << ","
             << color.g() << "," << color.b() << "," << color.a() << ")";
    const QSize surfaceSize(
        std::max(1, static_cast<int>(std::ceil(localRect.width()))),
        std::max(1, static_cast<int>(std::ceil(localRect.height()))));
    QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
    surface.fill(toQColor(color));
    if (!applySurfaceAndDraw(surface, worldRect)) {
      renderer->drawSolidRect(static_cast<float>(worldRect.x()),
                              static_cast<float>(worldRect.y()),
                              static_cast<float>(worldRect.width()),
                              static_cast<float>(worldRect.height()),
                              solidImage->color(), layer->opacity());
    }
    return;
  }

  if (const auto imageLayer =
          std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
    const QImage img = imageLayer->toQImage();
    if (!img.isNull()) {
      qCDebug(compositionViewLog) << "[CompositionView] draw image"
               << "id=" << layer->id().toString() << "surface=" << img.width()
               << "x" << img.height();
      if (!applySurfaceAndDraw(img, worldRect)) {
        renderer->drawSprite(
            static_cast<float>(worldRect.x()),
            static_cast<float>(worldRect.y()),
            static_cast<float>(worldRect.width()),
            static_cast<float>(worldRect.height()), img, layer->opacity());
      }
      return;
    }
  }

  if (const auto videoLayer =
          std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
    const QImage frame = videoLayer->currentFrameToQImage();
    if (!frame.isNull()) {
      qCDebug(compositionViewLog) << "[CompositionView] draw video"
               << "id=" << layer->id().toString() << "surface=" << frame.width()
               << "x" << frame.height();
      if (!applySurfaceAndDraw(frame, worldRect)) {
        renderer->drawSprite(
            static_cast<float>(worldRect.x()),
            static_cast<float>(worldRect.y()),
            static_cast<float>(worldRect.width()),
            static_cast<float>(worldRect.height()), frame, layer->opacity());
      }
      return;
    }
  }

  if (const auto textLayer =
          std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
    const QImage textImage = textLayer->toQImage();
    if (!textImage.isNull()) {
      qCDebug(compositionViewLog) << "[CompositionView] draw text"
               << "id=" << layer->id().toString()
               << "surface=" << textImage.width() << "x" << textImage.height();
      if (!applySurfaceAndDraw(textImage, worldRect)) {
        renderer->drawSprite(
            static_cast<float>(worldRect.x()),
            static_cast<float>(worldRect.y()),
            static_cast<float>(worldRect.width()),
            static_cast<float>(worldRect.height()), textImage, layer->opacity());
      }
      return;
    }
    qCDebug(compositionViewLog) << "[CompositionView] skip text layer: no surface"
             << "id=" << layer->id().toString();
    return;
  }

  // Fallback for layer types without a direct surface accessor.
  qCDebug(compositionViewLog) << "[CompositionView] fallback layer draw"
           << "id=" << layer->id().toString()
           << "type=" << layer->type_index().name();
  layer->draw(renderer);
}

} // namespace

class CompositionRenderController::Impl {
public:
  std::unique_ptr<ArtifactIRenderer> renderer_;
  std::unique_ptr<CompositionRenderer> compositionRenderer_;
  ArtifactPreviewCompositionPipeline previewPipeline_;
  std::unique_ptr<TransformGizmo> gizmo_;
  QTimer *renderTimer_ = nullptr;
  bool initialized_ = false;
  bool running_ = false;
  QVector<QMetaObject::Connection> layerChangedConnections_;
  QMetaObject::Connection compositionChangedConnection_;

  LayerID selectedLayerId_;
  bool showGrid_ = false;
  bool showGuides_ = false;
  bool showSafeMargins_ = false;

  void applyCompositionState(const ArtifactCompositionPtr &composition) {
    if (!renderer_ || !composition) {
      return;
    }

    const auto size = composition->settings().compositionSize();
    const float cw = static_cast<float>(size.width() > 0 ? size.width() : 1920);
    const float ch =
        static_cast<float>(size.height() > 0 ? size.height() : 1080);
    if (compositionRenderer_) {
      compositionRenderer_->SetCompositionSize(cw, ch);
      compositionRenderer_->ApplyCompositionSpace();
      renderer_->setCanvasSize(cw, ch);
    } else {
      renderer_->setCanvasSize(cw, ch);
    }
  }

  void bindCompositionChanged(CompositionRenderController *owner,
                              const ArtifactCompositionPtr &composition) {
    if (compositionChangedConnection_) {
      QObject::disconnect(compositionChangedConnection_);
      compositionChangedConnection_ = {};
    }
    if (!owner || !composition) {
      return;
    }

    compositionChangedConnection_ = QObject::connect(
        composition.get(), &ArtifactAbstractComposition::changed, owner,
        [this, owner, composition]() {
          applyCompositionState(composition);
          owner->renderOneFrame();
        });
  }
};

CompositionRenderController::CompositionRenderController(QObject *parent)
    : QObject(parent), impl_(new Impl()) {
  impl_->gizmo_ = std::make_unique<TransformGizmo>();

  // Connect to project service to track layer selection
  if (auto *svc = ArtifactProjectService::instance()) {
    connect(svc, &ArtifactProjectService::layerSelected, this,
            [this](const LayerID &id) {
              setSelectedLayerId(id);
              auto comp = impl_->previewPipeline_.composition();
              if (comp) {
                impl_->gizmo_->setLayer(comp->layerById(id));
              } else {
                impl_->gizmo_->setLayer(nullptr);
              }
              renderOneFrame();
            });

    // Always follow the active composition even if upstream wiring misses one
    // path.
    connect(svc, &ArtifactProjectService::currentCompositionChanged, this,
            [this, svc](const CompositionID &id) {
              ArtifactCompositionPtr comp;
              if (!id.isNil()) {
                const auto found = svc->findComposition(id);
                if (found.success && !found.ptr.expired()) {
                  comp = found.ptr.lock();
                }
              }
              if (!comp) {
                comp = resolvePreferredComposition(svc);
              }
              setComposition(comp);
            });

    // Project-level mutations can replace composition/layer instances; resync
    // aggressively.
    connect(svc, &ArtifactProjectService::projectChanged, this, [this, svc]() {
      auto latest = resolvePreferredComposition(svc);
      auto current = impl_->previewPipeline_.composition();
      if (latest != current) {
        setComposition(latest);
      } else {
        renderOneFrame();
      }
    });

    // Ensure layers created after setComposition() are also bound to redraw.
    connect(svc, &ArtifactProjectService::layerCreated, this,
            [this](const CompositionID &compId, const LayerID &layerId) {
              auto comp = impl_->previewPipeline_.composition();
              if (!comp || comp->id() != compId) {
                return;
              }
              if (auto layer = comp->layerById(layerId)) {
                impl_->layerChangedConnections_.push_back(
                    connect(layer.get(), &ArtifactAbstractLayer::changed, this,
                            [this]() { renderOneFrame(); }));
              }
              renderOneFrame();
            });
  }
}

CompositionRenderController::~CompositionRenderController() {
  destroy();
  delete impl_;
}

void CompositionRenderController::initialize(QWidget *hostWidget) {
  if (impl_->initialized_ || hostWidget == nullptr) {
    return;
  }

  impl_->renderer_ = std::make_unique<ArtifactIRenderer>();
  impl_->renderer_->initialize(hostWidget);

  if (!impl_->renderer_->isInitialized()) {
    qWarning() << "[CompositionRenderController] renderer initialize failed for"
               << hostWidget << "size=" << hostWidget->size()
               << "DPR=" << hostWidget->devicePixelRatio();
    impl_->renderer_.reset();
    return;
  }
  impl_->compositionRenderer_ =
      std::make_unique<CompositionRenderer>(*impl_->renderer_);
  impl_->renderer_->setViewportSize((float)hostWidget->width(),
                                    (float)hostWidget->height());

  const auto comp = impl_->previewPipeline_.composition();
  if (comp) {
    impl_->applyCompositionState(comp);
    impl_->renderer_->fitToViewport();
  }

  impl_->renderTimer_ = new QTimer(this);
  impl_->renderTimer_->setTimerType(Qt::PreciseTimer);
  connect(impl_->renderTimer_, &QTimer::timeout, this,
          &CompositionRenderController::renderOneFrame);

  // PlaybackService のフレーム変更に合わせて再描画
  if (auto *playback = ArtifactPlaybackService::instance()) {
    connect(playback, &ArtifactPlaybackService::frameChanged, this,
            [this]() { renderOneFrame(); });
  }

  impl_->initialized_ = true;
}

void CompositionRenderController::destroy() {
  stop();
  if (impl_->renderer_) {
    impl_->renderer_->destroy();
    impl_->renderer_.reset();
  }
  impl_->compositionRenderer_.reset();
  impl_->initialized_ = false;
}

bool CompositionRenderController::isInitialized() const {
  return impl_->initialized_;
}

void CompositionRenderController::start() {
  if (!impl_->initialized_ || impl_->running_ ||
      impl_->renderTimer_ == nullptr) {
    return;
  }
  impl_->running_ = true;
  impl_->renderTimer_->start(16);
}

void CompositionRenderController::stop() {
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

bool CompositionRenderController::isRunning() const { return impl_->running_; }

void CompositionRenderController::recreateSwapChain(QWidget *hostWidget) {
  if (!impl_->initialized_ || !impl_->renderer_ || hostWidget == nullptr) {
    return;
  }
  impl_->renderer_->recreateSwapChain(hostWidget);
}

void CompositionRenderController::setViewportSize(float width, float height) {
  if (!impl_->renderer_) {
    return;
  }
  impl_->renderer_->setViewportSize(width, height);
}

void CompositionRenderController::panBy(const QPointF &viewportDelta) {
  if (!impl_->renderer_) {
    return;
  }
  impl_->renderer_->panBy((float)viewportDelta.x(), (float)viewportDelta.y());
}

void CompositionRenderController::setComposition(
    ArtifactCompositionPtr composition) {
  qCDebug(compositionViewLog) << "[CompositionView] setComposition"
           << "isNull=" << (composition == nullptr) << "id="
           << (composition ? composition->id().toString()
                           : QStringLiteral("<null>"));

  auto currentComposition = impl_->previewPipeline_.composition();
  if (currentComposition == composition) {
    if (auto *playback = ArtifactPlaybackService::instance()) {
      if (playback->currentComposition() != composition) {
        playback->setCurrentComposition(composition);
      }
    }
    if (composition && impl_->renderer_) {
      impl_->applyCompositionState(composition);
    }
    if (composition && !impl_->selectedLayerId_.isNil()) {
      impl_->gizmo_->setLayer(composition->layerById(impl_->selectedLayerId_));
    } else if (!composition) {
      impl_->gizmo_->setLayer(nullptr);
    }
    renderOneFrame();
    return;
  }

  for (auto &connection : impl_->layerChangedConnections_) {
    disconnect(connection);
  }
  impl_->layerChangedConnections_.clear();
  if (impl_->compositionChangedConnection_) {
    disconnect(impl_->compositionChangedConnection_);
    impl_->compositionChangedConnection_ = {};
  }

  impl_->previewPipeline_.setComposition(composition);
  impl_->bindCompositionChanged(this, composition);

  if (auto *playback = ArtifactPlaybackService::instance()) {
    if (playback->currentComposition() != composition) {
      playback->setCurrentComposition(composition);
    }
  }

  if (composition && impl_->renderer_) {
    impl_->applyCompositionState(composition);
    impl_->renderer_->fitToViewport();

    // 各レイヤーの変更を監視
    for (auto &layer : composition->allLayer()) {
      if (layer) {
        impl_->layerChangedConnections_.push_back(
            connect(layer.get(), &ArtifactAbstractLayer::changed, this,
                    [this]() { renderOneFrame(); }));
      }
    }

    if (!impl_->selectedLayerId_.isNil()) {
      impl_->gizmo_->setLayer(composition->layerById(impl_->selectedLayerId_));
    } else {
      impl_->gizmo_->setLayer(nullptr);
    }

    // コンポジションがセットされた瞬間に1フレーム描画
    renderOneFrame();
  } else if (!composition) {
    impl_->gizmo_->setLayer(nullptr);
    renderOneFrame();
  }
}

ArtifactCompositionPtr CompositionRenderController::composition() const {
  return impl_->previewPipeline_.composition();
}

void CompositionRenderController::setSelectedLayerId(const LayerID &id) {
  impl_->selectedLayerId_ = id;
}

void CompositionRenderController::setClearColor(const FloatColor &color) {
  if (impl_->renderer_) {
    impl_->renderer_->setClearColor(color);
  }
}

void CompositionRenderController::setShowGrid(bool show) {
  impl_->showGrid_ = show;
  renderOneFrame();
}
bool CompositionRenderController::isShowGrid() const {
  return impl_->showGrid_;
}
void CompositionRenderController::setShowGuides(bool show) {
  impl_->showGuides_ = show;
  renderOneFrame();
}
bool CompositionRenderController::isShowGuides() const {
  return impl_->showGuides_;
}
void CompositionRenderController::setShowSafeMargins(bool show) {
  impl_->showSafeMargins_ = show;
  renderOneFrame();
}
bool CompositionRenderController::isShowSafeMargins() const {
  return impl_->showSafeMargins_;
}

void CompositionRenderController::resetView() {
  if (impl_->renderer_) {
    impl_->renderer_->resetView();
    renderOneFrame();
  }
}

void CompositionRenderController::zoomInAt(const QPointF &viewportPos) {
  if (impl_->renderer_) {
    const float currentZoom = impl_->renderer_->getZoom();
    const float newZoom = std::clamp(currentZoom * 1.1f, 0.05f, 64.0f);
    impl_->renderer_->zoomAroundViewportPoint(
        {(float)viewportPos.x(), (float)viewportPos.y()}, newZoom);
    renderOneFrame();
  }
}

void CompositionRenderController::zoomOutAt(const QPointF &viewportPos) {
  if (impl_->renderer_) {
    const float currentZoom = impl_->renderer_->getZoom();
    const float newZoom = std::clamp(currentZoom / 1.1f, 0.05f, 64.0f);
    impl_->renderer_->zoomAroundViewportPoint(
        {(float)viewportPos.x(), (float)viewportPos.y()}, newZoom);
    renderOneFrame();
  }
}

void CompositionRenderController::zoomFit() {
  if (impl_->renderer_) {
    impl_->renderer_->fitToViewport();
    renderOneFrame();
  }
}

void CompositionRenderController::zoom100() {
  if (impl_->renderer_) {
    impl_->renderer_->setZoom(1.0f);
    renderOneFrame();
  }
}

void CompositionRenderController::handleMousePress(const QPointF &viewportPos) {
  if (impl_->gizmo_) {
    impl_->gizmo_->handleMousePress(viewportPos, impl_->renderer_.get());
  }
}

void CompositionRenderController::handleMouseMove(const QPointF &viewportPos) {
  if (impl_->gizmo_) {
    impl_->gizmo_->handleMouseMove(viewportPos, impl_->renderer_.get());
  }
}

void CompositionRenderController::handleMouseRelease() {
  if (impl_->gizmo_) {
    impl_->gizmo_->handleMouseRelease();
  }
}

TransformGizmo *CompositionRenderController::gizmo() const {
  return impl_->gizmo_.get();
}

Qt::CursorShape CompositionRenderController::cursorShapeForViewportPos(const QPointF& viewportPos) const
{
  if (!impl_->gizmo_ || !impl_->renderer_) {
    return Qt::ArrowCursor;
  }
  return impl_->gizmo_->cursorShapeForViewportPos(viewportPos, impl_->renderer_.get());
}

void CompositionRenderController::renderOneFrame() {
  if (!impl_->initialized_ || !impl_->renderer_) {
    qCDebug(compositionViewLog) << "[CompositionView] renderOneFrame skipped: not initialized";
    return;
  }

  auto comp = impl_->previewPipeline_.composition();
  if (auto *service = ArtifactProjectService::instance()) {
    const auto preferred = resolvePreferredComposition(service);
    if (preferred && preferred != comp) {
      comp = preferred;
      impl_->previewPipeline_.setComposition(comp);
      qCDebug(compositionViewLog)
          << "[CompositionView] renderOneFrame resynced preferred composition"
          << "id=" << comp->id().toString()
          << "layers=" << comp->allLayer().size();
    }
  }
  FramePosition currentFrame = comp ? comp->framePosition() : FramePosition(0);
  if (comp) {
    auto size = comp->settings().compositionSize();
    const float cw = static_cast<float>(size.width() > 0 ? size.width() : 1920);
    const float ch =
        static_cast<float>(size.height() > 0 ? size.height() : 1080);
    if (impl_->compositionRenderer_) {
      impl_->compositionRenderer_->SetCompositionSize(cw, ch);
      impl_->compositionRenderer_->ApplyCompositionSpace();
      impl_->renderer_->setCanvasSize(cw, ch);
    } else {
      impl_->renderer_->setCanvasSize(cw, ch);
    }

    // 1) 画面クリア（Screen Space）
    impl_->renderer_->clear();

    // 2) コンポ背景を Composition Space で描画
    const FloatColor bgColor = comp->backgroundColor();
    qCDebug(compositionViewLog) << "[CompositionView] frame begin"
             << "compId=" << comp->id().toString() << "size=" << cw << "x" << ch
             << "bg=(" << bgColor.r() << "," << bgColor.g() << ","
             << bgColor.b() << "," << bgColor.a() << ")";
    if (impl_->compositionRenderer_) {
      qCDebug(compositionViewLog) << "[CompositionView] drawing background via CompositionRenderer"
               << "color=(" << bgColor.r() << "," << bgColor.g() << ","
               << bgColor.b() << "," << bgColor.a() << ")";
      impl_->compositionRenderer_->DrawCompositionBackground(bgColor);
    } else {
      qCDebug(compositionViewLog)
          << "[CompositionView] drawing background via renderer_->drawRectLocal"
          << "color=(" << bgColor.r() << "," << bgColor.g() << ","
          << bgColor.b() << "," << bgColor.a() << ")";
      impl_->renderer_->drawRectLocal(0.0f, 0.0f, cw, ch, bgColor, 1.0f);
    }

    // 3) グリッド描画（Composition Space）
    if (impl_->showGrid_) {
      impl_->renderer_->drawGrid(0, 0, cw, ch, 100.0f, 1.0f,
                                 {0.3f, 0.3f, 0.3f, 0.5f});
    }

    // 4) レイヤー描画（Composition Space 基準）
    currentFrame = comp->framePosition();
    if (auto *playback = ArtifactPlaybackService::instance()) {
      const auto playbackComp = playback->currentComposition();
      if (!playbackComp || playbackComp->id() == comp->id()) {
        currentFrame = playback->currentFrame();
      }
    }
    const auto layers = comp->allLayer();
    qCDebug(compositionViewLog) << "[CompositionView] layers total=" << layers.size()
             << "currentFrame=" << currentFrame.framePosition();
    const bool hasSoloLayer =
        std::any_of(layers.begin(), layers.end(),
                    [](const ArtifactAbstractLayerPtr &layer) {
                      return layer && layer->isVisible() && layer->isSolo();
                    });
    for (const auto &layer : layers) {
      if (!layer || !layer->isVisible()) {
        if (layer) {
          qCDebug(compositionViewLog) << "[CompositionView] skip layer: invisible"
                   << "id=" << layer->id().toString();
        }
        continue;
      }
      if (hasSoloLayer && !layer->isSolo()) {
        qCDebug(compositionViewLog) << "[CompositionView] skip layer: solo filter"
                 << "id=" << layer->id().toString();
        continue;
      }
      if (!layer->isActiveAt(currentFrame)) {
        qCDebug(compositionViewLog) << "[CompositionView] skip layer: inactive at frame"
                 << "id=" << layer->id().toString()
                 << "frame=" << currentFrame.framePosition();
        continue;
      }
      layer->goToFrame(currentFrame.framePosition());
      qCDebug(compositionViewLog) << "[CompositionView] draw layer"
               << "id=" << layer->id().toString()
               << "opacity=" << layer->opacity();
      drawLayerForCompositionView(layer, impl_->renderer_.get());
    }

    // 5) セーフマージンの描画（Composition Space）
    if (impl_->showSafeMargins_) {
      const float actionSafeW = cw * 0.9f;
      const float actionSafeH = ch * 0.9f;
      const float titleSafeW = cw * 0.8f;
      const float titleSafeH = ch * 0.8f;
      const FloatColor marginColor = {0.5f, 0.5f, 0.5f, 0.6f};

      // Action Safe (90%)
      impl_->renderer_->drawRectOutline((cw - actionSafeW) * 0.5f,
                                        (ch - actionSafeH) * 0.5f, actionSafeW,
                                        actionSafeH, marginColor);
      // Title Safe (80%)
      impl_->renderer_->drawRectOutline((cw - titleSafeW) * 0.5f,
                                        (ch - titleSafeH) * 0.5f, titleSafeW,
                                        titleSafeH, marginColor);

      // 中央の十字
      const float crossSize = 20.0f;
      impl_->renderer_->drawSolidLine({cw * 0.5f - crossSize, ch * 0.5f},
                                      {cw * 0.5f + crossSize, ch * 0.5f},
                                      marginColor, 1.0f);
      impl_->renderer_->drawSolidLine({cw * 0.5f, ch * 0.5f - crossSize},
                                      {cw * 0.5f, ch * 0.5f + crossSize},
                                      marginColor, 1.0f);
    }
  } else {
    impl_->renderer_->clear();
  }

  // 最前面にギズモを描画。ただし選択レイヤーがそのフレームで有効な時だけ。
  if (impl_->gizmo_) {
    auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
                             ? comp->layerById(impl_->selectedLayerId_)
                             : ArtifactAbstractLayerPtr{};
    if (selectedLayer && selectedLayer->isActiveAt(currentFrame)) {
      impl_->gizmo_->setLayer(selectedLayer);
      impl_->gizmo_->draw(impl_->renderer_.get());
    } else {
      impl_->gizmo_->setLayer(nullptr);
    }
  }

  impl_->renderer_->flush();
  impl_->renderer_->present();
}

} // namespace Artifact
