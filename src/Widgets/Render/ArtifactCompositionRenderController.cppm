module;
#define NOMINMAX
#define QT_NO_KEYWORDS
#include <opencv2/opencv.hpp>

#include <QDebug>
#include <QColor>
#include <QImage>
#include <QLoggingCategory>
#include <QTransform>
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
import Artifact.Layers.Selection.Manager;
import Artifact.Widgets.TransformGizmo;
import Artifact.Tool.Manager;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Utils.Id;
import Artifact.Render.Pipeline;
import Graphics.LayerBlendPipeline;
import Graphics.GPUcomputeContext;

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

  const QTransform globalTransform = layer->getGlobalTransform();

  auto applyRasterizerEffectsAndMasksToSurface = [&](const ArtifactAbstractLayerPtr& targetLayer,
                                            QImage& surface) {
    if (!targetLayer || surface.isNull()) {
      return;
    }

    const bool hasMasks = targetLayer->hasMasks();
    const auto effects = targetLayer->getEffects();
    bool hasRasterizerEffect = false;
    for (const auto& effect : effects) {
      if (effect && effect->isEnabled() &&
          effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
        hasRasterizerEffect = true;
        break;
      }
    }

    if (!hasRasterizerEffect && !hasMasks) {
      return;
    }

    // Convert to float mat for processing
    cv::Mat mat = ArtifactCore::CvUtils::qImageToCvMat(surface, true);
    if (mat.type() != CV_32FC4) {
        mat.convertTo(mat, CV_32FC4, 1.0 / 255.0);
    }

    // Apply Effects
    if (hasRasterizerEffect) {
        ArtifactCore::ImageF32x4_RGBA cpuImage;
        cpuImage.setFromCVMat(mat);
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
        mat = current.image().toCVMat();
    }

    // Apply Masks
    if (hasMasks) {
        for (int m = 0; m < targetLayer->maskCount(); ++m) {
            LayerMask mask = targetLayer->mask(m);
            mask.applyToImage(mat.cols, mat.rows, &mat);
        }
    }

    surface = ArtifactCore::CvUtils::cvMatToQImage(mat);
  };

  auto hasRasterizerEffectsOrMasks = [](const ArtifactAbstractLayerPtr& targetLayer) {
    if (!targetLayer) {
      return false;
    }
    if (targetLayer->hasMasks()) {
        return true;
    }

    for (const auto& effect : targetLayer->getEffects()) {
      if (effect && effect->isEnabled() &&
          effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
        return true;
      }
    }
    return false;
  };

  auto applySurfaceAndDraw = [&](QImage surface, const QRectF& rect) {
    if (surface.isNull()) {
      return false;
    }
    applyRasterizerEffectsAndMasksToSurface(layer, surface);
    renderer->drawSpriteTransformed(static_cast<float>(rect.x()),
                         static_cast<float>(rect.y()),
                         static_cast<float>(rect.width()),
                         static_cast<float>(rect.height()), 
                         globalTransform,
                         surface,
                         layer->opacity());
    return true;
  };

  if (const auto solid2D =
          std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer)) {
    const auto color = solid2D->color();
    if (hasRasterizerEffectsOrMasks(layer)) {
      const QSize surfaceSize(
          std::max(1, static_cast<int>(std::ceil(localRect.width()))),
          std::max(1, static_cast<int>(std::ceil(localRect.height()))));
      QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
      surface.fill(toQColor(color));
      applySurfaceAndDraw(surface, localRect);
    } else {
      renderer->drawSolidRectTransformed(static_cast<float>(localRect.x()),
                              static_cast<float>(localRect.y()),
                              static_cast<float>(localRect.width()),
                              static_cast<float>(localRect.height()),
                              globalTransform,
                              color, layer->opacity());
    }
    return;
  }

  if (const auto solidImage =
          std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer)) {
    const auto color = solidImage->color();
    if (hasRasterizerEffectsOrMasks(layer)) {
      const QSize surfaceSize(
          std::max(1, static_cast<int>(std::ceil(localRect.width()))),
          std::max(1, static_cast<int>(std::ceil(localRect.height()))));
      QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
      surface.fill(toQColor(color));
      applySurfaceAndDraw(surface, localRect);
    } else {
      renderer->drawSolidRectTransformed(static_cast<float>(localRect.x()),
                              static_cast<float>(localRect.y()),
                              static_cast<float>(localRect.width()),
                              static_cast<float>(localRect.height()),
                              globalTransform,
                              color, layer->opacity());
    }
    return;
  }

  if (const auto imageLayer =
          std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
    const QImage img = imageLayer->toQImage();
    if (!img.isNull()) {
      applySurfaceAndDraw(img, localRect);
      return;
    }
  }

  if (const auto videoLayer =
          std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
    const QImage frame = videoLayer->currentFrameToQImage();
    if (!frame.isNull()) {
      applySurfaceAndDraw(frame, localRect);
      return;
    }
  }

  if (const auto textLayer =
          std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
    const QImage textImage = textLayer->toQImage();
    if (!textImage.isNull()) {
      applySurfaceAndDraw(textImage, localRect);
      return;
    }
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
  std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
  std::unique_ptr<ArtifactCore::LayerBlendPipeline> blendPipeline_;
  RenderPipeline renderPipeline_;
  QTimer *renderTimer_ = nullptr;
  bool initialized_ = false;
  bool running_ = false;
  bool blendPipelineReady_ = false;
  QVector<QMetaObject::Connection> layerChangedConnections_;
  QMetaObject::Connection compositionChangedConnection_;

  LayerID selectedLayerId_;
  bool showGrid_ = false;
  bool showCheckerboard_ = false;
  bool showGuides_ = false;
  bool showSafeMargins_ = false;
  bool showFrameInfo_ = true;
  int currentFrameForOverlay_ = 0;

  // Guide positions (composition-space pixels)
  QVector<float> guideVerticals_;   // X positions
  QVector<float> guideHorizontals_; // Y positions
  float lastCanvasWidth_ = 1920.0f;
  float lastCanvasHeight_ = 1080.0f;

  // Mask editing state
  int hoveredMaskIndex_ = -1;
  int hoveredPathIndex_ = -1;
  int hoveredVertexIndex_ = -1;
  int draggingMaskIndex_ = -1;
  int draggingPathIndex_ = -1;
  int draggingVertexIndex_ = -1;
  bool isDraggingVertex_ = false;

  void applyCompositionState(const ArtifactCompositionPtr &composition) {
    if (!renderer_ || !composition) {
      return;
    }

    const auto size = composition->settings().compositionSize();
    const float cw = static_cast<float>(size.width() > 0 ? size.width() : 1920);
    const float ch =
        static_cast<float>(size.height() > 0 ? size.height() : 1080);
    lastCanvasWidth_ = cw;
    lastCanvasHeight_ = ch;
    if (compositionRenderer_) {
      compositionRenderer_->SetCompositionSize(cw, ch);
      compositionRenderer_->ApplyCompositionSpace();
      renderer_->setCanvasSize(cw, ch);
    } else {
      renderer_->setCanvasSize(cw, ch);
    }

    // レンダーパイプラインの中間テクスチャを初期化
    if (auto device = renderer_->device()) {
      renderPipeline_.initialize(device, static_cast<Uint32>(cw), static_cast<Uint32>(ch),
                                 TEX_FORMAT_RGBA8_UNORM_SRGB);
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

  // ブレンドパイプライン初期化
  if (auto device = impl_->renderer_->device()) {
    if (auto ctx = impl_->renderer_->immediateContext()) {
      impl_->gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(device, ctx);
      impl_->blendPipeline_ = std::make_unique<ArtifactCore::LayerBlendPipeline>(*impl_->gpuContext_);
      impl_->blendPipelineReady_ = impl_->blendPipeline_->initialize();
      qCDebug(compositionViewLog) << "[CompositionView] blend pipeline"
               << (impl_->blendPipelineReady_ ? "initialized" : "FAILED");
    }
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
void CompositionRenderController::setShowCheckerboard(bool show) {
  impl_->showCheckerboard_ = show;
  renderOneFrame();
}
bool CompositionRenderController::isShowCheckerboard() const {
  return impl_->showCheckerboard_;
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

void CompositionRenderController::handleMousePress(QMouseEvent *event) {
  if (!event || !impl_->renderer_) return;

  const QPointF viewportPos = event->position();

  // Gizmo hit test first
  if (impl_->gizmo_) {
    impl_->gizmo_->handleMousePress(viewportPos, impl_->renderer_.get());
    if (impl_->gizmo_->isDragging()) {
      return;
    }
  }

  if (event->button() == Qt::LeftButton) {
    auto toolManager = ArtifactApplicationManager::instance()->toolManager();
    auto activeTool = toolManager ? toolManager->activeTool() : ToolType::Selection;

    auto comp = impl_->previewPipeline_.composition();
    if (comp && impl_->renderer_) {
      const auto cPos = impl_->renderer_->viewportToCanvas(
          {(float)viewportPos.x(), (float)viewportPos.y()});
      
      // Get selected layer for Pen tool operations
      auto selectedLayer = (!impl_->selectedLayerId_.isNil())
                               ? comp->layerById(impl_->selectedLayerId_)
                               : ArtifactAbstractLayerPtr{};

      if (activeTool == ToolType::Pen && selectedLayer) {
          // Convert canvas position to layer local position
          const QTransform globalTransform = selectedLayer->getGlobalTransform();
          bool invertible = false;
          const QTransform invTransform = globalTransform.inverted(&invertible);
          
          if (invertible) {
              const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));
              
              // 1. Hit test existing vertices for dragging or closing path
              const float hitThreshold = 8.0f / impl_->renderer_->getZoom(); // 8px in viewport space
              for (int m = 0; m < selectedLayer->maskCount(); ++m) {
                  LayerMask mask = selectedLayer->mask(m);
                  for (int p = 0; p < mask.maskPathCount(); ++p) {
                      MaskPath path = mask.maskPath(p);
                      for (int v = 0; v < path.vertexCount(); ++v) {
                          MaskVertex vertex = path.vertex(v);
                          if (QVector2D(vertex.position - localPos).length() < hitThreshold) {
                              // If it's the first vertex and we have more than 2, close the path
                              if (v == 0 && !path.isClosed() && path.vertexCount() > 2) {
                                  path.setClosed(true);
                                  mask.setMaskPath(p, path);
                                  selectedLayer->setMask(m, mask);
                                  qDebug() << "[PenTool] Closed path" << p;
                                  selectedLayer->changed();
                                  renderOneFrame();
                                  return;
                              }
                              
                              // Start dragging vertex
                              impl_->isDraggingVertex_ = true;
                              impl_->draggingMaskIndex_ = m;
                              impl_->draggingPathIndex_ = p;
                              impl_->draggingVertexIndex_ = v;
                              qDebug() << "[PenTool] Started dragging vertex" << v;
                              return;
                          }
                      }
                  }
              }

              // 2. Add new vertex if no existing vertex was hit
              if (selectedLayer->maskCount() == 0) {
                  LayerMask newMask;
                  MaskPath newPath;
                  newMask.addMaskPath(newPath);
                  selectedLayer->addMask(newMask);
              }
              
              LayerMask mask = selectedLayer->mask(0);
              if (mask.maskPathCount() == 0) {
                  mask.addMaskPath(MaskPath());
              }
              
              MaskPath path = mask.maskPath(0);
              // Don't add more vertices if already closed
              if (path.isClosed()) {
                  // TODO: Logic to start a new path or insert vertex into existing edge
                  return;
              }

              MaskVertex vertex;
              vertex.position = localPos;
              vertex.inTangent = QPointF(0, 0);
              vertex.outTangent = QPointF(0, 0);
              
              path.addVertex(vertex);
              mask.setMaskPath(0, path);
              selectedLayer->setMask(0, mask);
              
              qDebug() << "[PenTool] Added vertex at local:" << localPos << "layer:" << selectedLayer->id().toString();
              selectedLayer->changed();
              renderOneFrame();
              return; // Handled
          }
      }

      const auto layers = comp->allLayer();

      // 現在フレームを取得（描画ループと同じソース）
      FramePosition currentFrame = comp->framePosition();
      if (auto *playback = ArtifactPlaybackService::instance()) {
        const auto playbackComp = playback->currentComposition();
        if (!playbackComp || playbackComp->id() == comp->id()) {
          currentFrame = playback->currentFrame();
        }
      }

      ArtifactAbstractLayerPtr hitLayer = nullptr;
      // allLayer() returns [backmost, ..., frontmost]
      // Traverse from end to beginning to find the frontmost layer first
      qCDebug(compositionViewLog) << "[HitTest] canvasPos=(" << cPos.x << "," << cPos.y << ")"
               << "layers=" << layers.size();
      
      for (int i = (int)layers.size() - 1; i >= 0; --i) {
        auto& layer = layers[i];
        if (!layer || !layer->isVisible()) continue;
        if (!layer->isActiveAt(currentFrame)) continue;

        // Accurate hit test using inverse transform
        const QTransform globalTransform = layer->getGlobalTransform();
        bool invertible = false;
        const QTransform invTransform = globalTransform.inverted(&invertible);
        
        if (invertible) {
          const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));
          if (layer->localBounds().contains(localPos)) {
            hitLayer = layer;
            qCDebug(compositionViewLog) << "[HitTest] HIT (Accurate) index=" << i
                     << "id=" << layer->id().toString();
            break;
          }
        } else {
          // Fallback to bounding box if transform is not invertible (e.g. zero scale)
          auto bbox = layer->transformedBoundingBox();
          if (bbox.contains(cPos.x, cPos.y)) {
            hitLayer = layer;
            qCDebug(compositionViewLog) << "[HitTest] HIT (BBox Fallback) index=" << i
                     << "id=" << layer->id().toString();
            break;
          }
        }
      }

      if (hitLayer) {
        if (event->modifiers() & Qt::ShiftModifier) {
          ArtifactApplicationManager::instance()->layerSelectionManager()->addToSelection(hitLayer);
        } else {
          ArtifactApplicationManager::instance()->layerSelectionManager()->selectLayer(hitLayer);
        }
        impl_->selectedLayerId_ = hitLayer->id();
        impl_->gizmo_->setLayer(hitLayer);
      } else {
        if (!(event->modifiers() & Qt::ShiftModifier)) {
          ArtifactApplicationManager::instance()->layerSelectionManager()->clearSelection();
          impl_->selectedLayerId_ = LayerID::Nil();
          impl_->gizmo_->setLayer(nullptr);
        }
      }
    }
  }
}

void CompositionRenderController::handleMouseMove(const QPointF &viewportPos) {
  auto toolManager = ArtifactApplicationManager::instance()->toolManager();
  auto activeTool = toolManager ? toolManager->activeTool() : ToolType::Selection;

  if (activeTool == ToolType::Pen && impl_->isDraggingVertex_) {
      auto comp = impl_->previewPipeline_.composition();
      if (comp && impl_->renderer_) {
          auto selectedLayer = comp->layerById(impl_->selectedLayerId_);
          if (selectedLayer) {
              const auto cPos = impl_->renderer_->viewportToCanvas(
                  {(float)viewportPos.x(), (float)viewportPos.y()});
              const QTransform globalTransform = selectedLayer->getGlobalTransform();
              bool invertible = false;
              const QTransform invTransform = globalTransform.inverted(&invertible);
              
              if (invertible) {
                  const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));
                  LayerMask mask = selectedLayer->mask(impl_->draggingMaskIndex_);
                  MaskPath path = mask.maskPath(impl_->draggingPathIndex_);
                  MaskVertex vertex = path.vertex(impl_->draggingVertexIndex_);
                  
                  vertex.position = localPos;
                  path.setVertex(impl_->draggingVertexIndex_, vertex);
                  mask.setMaskPath(impl_->draggingPathIndex_, path);
                  selectedLayer->setMask(impl_->draggingMaskIndex_, mask);
                  
                  selectedLayer->changed();
                  renderOneFrame();
                  return;
              }
          }
      }
  }

  // Hover detection for Pen tool
  if (activeTool == ToolType::Pen) {
      impl_->hoveredMaskIndex_ = -1;
      impl_->hoveredPathIndex_ = -1;
      impl_->hoveredVertexIndex_ = -1;

      auto comp = impl_->previewPipeline_.composition();
      if (comp && impl_->renderer_) {
          auto selectedLayer = comp->layerById(impl_->selectedLayerId_);
          if (selectedLayer) {
              const auto cPos = impl_->renderer_->viewportToCanvas(
                  {(float)viewportPos.x(), (float)viewportPos.y()});
              const QTransform globalTransform = selectedLayer->getGlobalTransform();
              bool invertible = false;
              const QTransform invTransform = globalTransform.inverted(&invertible);
              
              if (invertible) {
                  const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));
                  const float hitThreshold = 8.0f / impl_->renderer_->getZoom();
                  
                  for (int m = 0; m < selectedLayer->maskCount(); ++m) {
                      LayerMask mask = selectedLayer->mask(m);
                      for (int p = 0; p < mask.maskPathCount(); ++p) {
                          MaskPath path = mask.maskPath(p);
                          for (int v = 0; v < path.vertexCount(); ++v) {
                              MaskVertex vertex = path.vertex(v);
                              if (QVector2D(vertex.position - localPos).length() < hitThreshold) {
                                  impl_->hoveredMaskIndex_ = m;
                                  impl_->hoveredPathIndex_ = p;
                                  impl_->hoveredVertexIndex_ = v;
                                  renderOneFrame();
                                  break;
                              }
                          }
                          if (impl_->hoveredVertexIndex_ != -1) break;
                      }
                      if (impl_->hoveredVertexIndex_ != -1) break;
                  }
              }
          }
      }
  }

  if (impl_->gizmo_) {
    impl_->gizmo_->handleMouseMove(viewportPos, impl_->renderer_.get());
  }
}

void CompositionRenderController::handleMouseRelease() {
  impl_->isDraggingVertex_ = false;
  impl_->draggingMaskIndex_ = -1;
  impl_->draggingPathIndex_ = -1;
  impl_->draggingVertexIndex_ = -1;

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
  if (!comp) {
    impl_->renderer_->clear();
    impl_->renderer_->flush();
    impl_->renderer_->present();
    return;
  }

  auto size = comp->settings().compositionSize();
  const float cw = static_cast<float>(size.width() > 0 ? size.width() : 1920);
  const float ch = static_cast<float>(size.height() > 0 ? size.height() : 1080);
  impl_->lastCanvasWidth_ = cw;
  impl_->lastCanvasHeight_ = ch;
  if (impl_->compositionRenderer_) {
    impl_->compositionRenderer_->SetCompositionSize(cw, ch);
    impl_->compositionRenderer_->ApplyCompositionSpace();
    impl_->renderer_->setCanvasSize(cw, ch);
  } else {
    impl_->renderer_->setCanvasSize(cw, ch);
  }

  impl_->renderer_->clear();

  const FloatColor bgColor = comp->backgroundColor();
  qCDebug(compositionViewLog) << "[CompositionView] frame begin"
           << "compId=" << comp->id().toString() << "size=" << cw << "x" << ch
           << "bg=(" << bgColor.r() << "," << bgColor.g() << ","
           << bgColor.b() << "," << bgColor.a() << ")";
  if (impl_->showCheckerboard_) {
    qCDebug(compositionViewLog) << "[CompositionView] drawing checkerboard background";
    impl_->renderer_->drawCheckerboard(0.0f, 0.0f, cw, ch, 16.0f,
                                       {0.25f, 0.25f, 0.25f, 1.0f},
                                       {0.35f, 0.35f, 0.35f, 1.0f});
  } else if (impl_->compositionRenderer_) {
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

  if (impl_->showGrid_) {
    impl_->renderer_->drawGrid(0, 0, cw, ch, 100.0f, 1.0f,
                               {0.3f, 0.3f, 0.3f, 0.5f});
  }

  FramePosition currentFrame = comp->framePosition();
  if (auto *playback = ArtifactPlaybackService::instance()) {
    const auto playbackComp = playback->currentComposition();
    if (!playbackComp || playbackComp->id() == comp->id()) {
      currentFrame = playback->currentFrame();
    }
  }
  qCDebug(compositionViewLog) << "[CompositionView] frame source"
           << "compFrame=" << comp->framePosition().framePosition()
           << "renderFrame=" << currentFrame.framePosition()
           << "playbackComp="
           << (ArtifactPlaybackService::instance()
                   ? (ArtifactPlaybackService::instance()->currentComposition() != nullptr)
                   : false);

  const auto layers = comp->allLayer();
  int64_t effectiveEndFrame = 0;
  for (const auto &l : layers) {
    if (l) {
      effectiveEndFrame = std::max(effectiveEndFrame, l->outPoint().framePosition());
    }
  }
  const int64_t framePos = currentFrame.framePosition();
  const bool frameOutOfRange =
      (framePos < 0 || (effectiveEndFrame > 0 && framePos >= effectiveEndFrame));

  if (!frameOutOfRange) {
    qCDebug(compositionViewLog) << "[CompositionView] layers total=" << layers.size()
             << "currentFrame=" << currentFrame.framePosition();
    const bool hasSoloLayer =
        std::any_of(layers.begin(), layers.end(),
                    [](const ArtifactAbstractLayerPtr &layer) {
                      return layer && layer->isVisible() && layer->isSolo();
                    });

    // ブレンドパイプライン使用判定: 非 Normal ブレンドモードのレイヤーが存在するか
    const bool hasBlendModes = impl_->blendPipelineReady_ &&
        std::any_of(layers.begin(), layers.end(),
                    [](const ArtifactAbstractLayerPtr &layer) {
                      return layer && layer->isVisible() &&
                             layer->layerBlendType() != LAYER_BLEND_TYPE::BLEND_NORMAL;
                    });

    if (hasBlendModes) {
      // ブレンドパイプライン使用: レイヤー毎にオフスクリーン描画 → 合成
      auto ctx = impl_->renderer_->immediateContext();
      if (ctx && impl_->renderPipeline_.ready()) {
        auto accumSRV = impl_->renderPipeline_.accumSRV();
        bool isFirst = true;

        for (const auto &layer : layers) {
          if (!layer || !layer->isVisible()) continue;
          if (hasSoloLayer && !layer->isSolo()) continue;
          if (!layer->isActiveAt(currentFrame)) continue;

          layer->goToFrame(currentFrame.framePosition());

          const auto blendMode = layer->layerBlendType();
          const float opacity = layer->opacity();

          // opacity 最適化
          if (opacity <= 0.0f) continue;

          // TODO: temp テクスチャにレイヤーを描画
          // TODO: blendPipeline_->blend() で合成
          // TODO: accum と temp をスワップ

          isFirst = false;
        }
      } else {
        // フォールバック: painter's algorithm
        for (const auto &layer : layers) {
          if (!layer || !layer->isVisible()) continue;
          if (hasSoloLayer && !layer->isSolo()) continue;
          if (!layer->isActiveAt(currentFrame)) continue;
          layer->goToFrame(currentFrame.framePosition());

          qCDebug(compositionViewLog) << "[CompositionView] drawing layer"
                   << "id=" << layer->id().toString()
                   << "isActive=" << layer->isActiveAt(currentFrame)
                   << "globalFrame=" << currentFrame.framePosition()
                   << "inPoint=" << layer->inPoint().framePosition()
                   << "outPoint=" << layer->outPoint().framePosition()
                   << "layerCurrentFrame=" << layer->currentFrame();

          drawLayerForCompositionView(layer, impl_->renderer_.get());        }
      }
    } else {
      // 通常描画: painter's algorithm
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
                   << "frame=" << currentFrame.framePosition()
                   << "in=" << layer->inPoint().framePosition()
                   << "out=" << layer->outPoint().framePosition()
                   << "startTime=" << layer->startTime().framePosition();
          continue;
        }
        layer->goToFrame(currentFrame.framePosition());
        qCDebug(compositionViewLog) << "[CompositionView] draw layer"
                 << "id=" << layer->id().toString()
                 << "opacity=" << layer->opacity()
                 << "blend=" << static_cast<int>(layer->layerBlendType())
                 << "frame=" << currentFrame.framePosition()
                 << "in=" << layer->inPoint().framePosition()
                 << "out=" << layer->outPoint().framePosition()
                 << "startTime=" << layer->startTime().framePosition();
        drawLayerForCompositionView(layer, impl_->renderer_.get());
      }
    }
  }

  if (impl_->gizmo_) {
    auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
                             ? comp->layerById(impl_->selectedLayerId_)
                             : ArtifactAbstractLayerPtr{};
    if (selectedLayer && selectedLayer->isVisible() &&
        selectedLayer->isActiveAt(currentFrame)) {
      impl_->gizmo_->setLayer(selectedLayer);
      impl_->gizmo_->draw(impl_->renderer_.get());

      // Mask Overlay Drawing
      const int maskCount = selectedLayer->maskCount();
      if (maskCount > 0 && impl_->renderer_) {
          const QTransform globalTransform = selectedLayer->getGlobalTransform();
          const FloatColor maskPointColor = {1.0f, 1.0f, 0.0f, 1.0f}; // Yellow
          const FloatColor maskLineColor = {0.0f, 1.0f, 1.0f, 0.8f}; // Cyan
          const FloatColor hoverColor = {1.0f, 0.5f, 0.0f, 1.0f}; // Orange
          const FloatColor dragColor = {1.0f, 0.0f, 0.0f, 1.0f};   // Red

          for (int m = 0; m < maskCount; ++m) {
              LayerMask mask = selectedLayer->mask(m);
              if (!mask.isEnabled()) continue;

              for (int p = 0; p < mask.maskPathCount(); ++p) {
                  MaskPath path = mask.maskPath(p);
                  const int vertexCount = path.vertexCount();
                  if (vertexCount == 0) continue;

                  Detail::float2 lastCanvasPos;
                  for (int v = 0; v < vertexCount; ++v) {
                      MaskVertex vertex = path.vertex(v);
                      QPointF canvasPos = globalTransform.map(vertex.position);
                      Detail::float2 currentCanvasPos = {(float)canvasPos.x(), (float)canvasPos.y()};

                      // Draw line from previous vertex
                      if (v > 0) {
                          impl_->renderer_->drawSolidLine(lastCanvasPos, currentCanvasPos, maskLineColor, 1.0f);
                      }

                      // Determine point color based on state
                      FloatColor currentColor = maskPointColor;
                      float currentPointSize = 6.0f;
                      
                      if (impl_->isDraggingVertex_ && impl_->draggingMaskIndex_ == m && 
                          impl_->draggingPathIndex_ == p && impl_->draggingVertexIndex_ == v) {
                          currentColor = dragColor;
                          currentPointSize = 8.0f;
                      } else if (impl_->hoveredMaskIndex_ == m && impl_->hoveredPathIndex_ == p && 
                                 impl_->hoveredVertexIndex_ == v) {
                          currentColor = hoverColor;
                          currentPointSize = 8.0f;
                      }

                      // Draw vertex point
                      impl_->renderer_->drawPoint(currentCanvasPos.x, currentCanvasPos.y, currentPointSize, currentColor);
                      lastCanvasPos = currentCanvasPos;
                  }

                  // Draw closing line if path is closed
                  if (path.isClosed() && vertexCount > 1) {
                      MaskVertex firstVertex = path.vertex(0);
                      QPointF firstCanvasPos = globalTransform.map(firstVertex.position);
                      impl_->renderer_->drawSolidLine(lastCanvasPos, {(float)firstCanvasPos.x(), (float)firstCanvasPos.y()}, maskLineColor, 1.0f);
                  }
              }
          }
      }
    } else {
      impl_->gizmo_->setLayer(nullptr);
    }
  }

  if (impl_->showFrameInfo_ && impl_->renderer_) {
    const float infoW = 60.0f;
    const float infoH = 14.0f;
    const float infoX = 4.0f;
    const float infoY = impl_->lastCanvasHeight_ - infoH - 4.0f;
    impl_->renderer_->drawSolidRect(infoX, infoY, infoW, infoH, {0.0f, 0.0f, 0.0f, 0.6f}, 0.8f);
    const int frame = currentFrame.framePosition();
    const float barRatio = (frame > 0) ? std::min(1.0f, static_cast<float>(frame) / 1000.0f) : 0.0f;
    const float barW = infoW * barRatio;
    if (barW > 1.0f) {
      impl_->renderer_->drawSolidRect(infoX, infoY, barW, infoH, {0.2f, 0.6f, 1.0f, 0.5f}, 0.6f);
    }
  }

  if (impl_->showSafeMargins_) {
    const float actionSafeW = cw * 0.9f;
    const float actionSafeH = ch * 0.9f;
    const float titleSafeW = cw * 0.8f;
    const float titleSafeH = ch * 0.8f;
    const FloatColor marginColor = {0.5f, 0.5f, 0.5f, 0.6f};

    impl_->renderer_->drawRectOutline((cw - actionSafeW) * 0.5f,
                                      (ch - actionSafeH) * 0.5f, actionSafeW,
                                      actionSafeH, marginColor);
    impl_->renderer_->drawRectOutline((cw - titleSafeW) * 0.5f,
                                      (ch - titleSafeH) * 0.5f, titleSafeW,
                                      titleSafeH, marginColor);

    const float crossSize = 20.0f;
    impl_->renderer_->drawSolidLine({cw * 0.5f - crossSize, ch * 0.5f},
                                    {cw * 0.5f + crossSize, ch * 0.5f},
                                    marginColor, 1.0f);
    impl_->renderer_->drawSolidLine({cw * 0.5f, ch * 0.5f - crossSize},
                                    {cw * 0.5f, ch * 0.5f + crossSize},
                                    marginColor, 1.0f);
  }

  if (impl_->showGuides_) {
    const FloatColor guideColor = {0.2f, 0.8f, 1.0f, 0.7f};
    for (float x : impl_->guideVerticals_) {
      if (x >= 0 && x <= cw) {
        impl_->renderer_->drawSolidLine({x, 0}, {x, ch}, guideColor, 1.0f);
      }
    }
    for (float y : impl_->guideHorizontals_) {
      if (y >= 0 && y <= ch) {
        impl_->renderer_->drawSolidLine({0, y}, {cw, y}, guideColor, 1.0f);
      }
    }
    if (impl_->guideVerticals_.isEmpty() && impl_->guideHorizontals_.isEmpty()) {
      impl_->renderer_->drawSolidLine({cw * 0.5f, 0}, {cw * 0.5f, ch}, guideColor, 1.0f);
      impl_->renderer_->drawSolidLine({0, ch * 0.5f}, {cw, ch * 0.5f}, guideColor, 1.0f);
    }
  }

  impl_->renderer_->flush();
  impl_->renderer_->present();
}

} // namespace Artifact
