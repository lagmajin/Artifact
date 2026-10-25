module;
#include <memory>
#include <algorithm>
#include <utility>
#include <QSize>
#include <QRectF>
#include <QLoggingCategory>
#include <QString>
#include <QVector3D>
module Artifact.Preview.Pipeline;

import Artifact.Composition.Abstract;

import Artifact.Layer.Abstract;

import Color.Float;

import Artifact.Render.Context;
import Artifact.Render.ROI;
import ArtifactCore.Crowd.Boids;
import Frame.Position;
import Memory.SharedPtr;
import Particle;

namespace Artifact
{
void drawCloneSelectionOverlay(ArtifactIRenderer* renderer,
                               const ArtifactAbstractLayerPtr& layer);

bool tryDrawSolid2DPreview(ArtifactIRenderer*, ArtifactAbstractLayer*,
                             const QRectF&, const ArtifactCore::LayerID&);
bool tryDrawSolidImagePreview(ArtifactIRenderer*, ArtifactAbstractLayer*,
                                const QRectF&, const ArtifactCore::LayerID&);
bool tryDrawImagePreview(ArtifactIRenderer*, ArtifactAbstractLayer*,
                           const QRectF&, const ArtifactCore::LayerID&);
bool tryDrawSvgPreview(ArtifactIRenderer*, ArtifactAbstractLayer*);
bool tryDrawVideoPreview(ArtifactIRenderer*, ArtifactAbstractLayer*,
                           const QRectF&, const ArtifactCore::LayerID&);
bool tryDrawTextPreview(ArtifactIRenderer*, ArtifactAbstractLayer*,
                          const QRectF&, const ArtifactCore::LayerID&);
bool tryDrawOtherLayerPreview(ArtifactIRenderer*, ArtifactAbstractLayer*);

 namespace
 {
  Q_LOGGING_CATEGORY(previewPipelineLog, "artifact.previewpipeline")
  void drawLayerForPreviewView(const ArtifactAbstractLayerPtr& layer,
                               ArtifactIRenderer* renderer,
                               const ArtifactCore::LayerID& selectedLayerId)
  {
    ArtifactAbstractLayer* layerPtr = layer.get();
    if (!layerPtr || !renderer) return;

    const QRectF localRect = layerPtr->localBounds();
    if (!localRect.isValid() || localRect.width() <= 0.0 ||
        localRect.height() <= 0.0) {
      return;
    }

    if (tryDrawSolid2DPreview(renderer, layerPtr, localRect, selectedLayerId) ||
        tryDrawSolidImagePreview(renderer, layerPtr, localRect, selectedLayerId) ||
        tryDrawImagePreview(renderer, layerPtr, localRect, selectedLayerId) ||
        tryDrawSvgPreview(renderer, layerPtr) ||
        tryDrawVideoPreview(renderer, layerPtr, localRect, selectedLayerId) ||
        tryDrawTextPreview(renderer, layerPtr, localRect, selectedLayerId) ||
        tryDrawOtherLayerPreview(renderer, layerPtr)) {
      return;
    }

    layerPtr->draw(renderer);
  }

  void registerPreviewContextSnapshot(const ArtifactCompositionPtr& composition,
                                      int64_t currentFrame)
  {
   if (!composition) {
    return;
   }

   const QSize compSize = composition->settings().compositionSize();
   const int compW = std::max(1, compSize.width());
   const int compH = std::max(1, compSize.height());
   auto ctx = createEditorPreviewContext(currentFrame,
                                         QSize(compW, compH),
                                         QSizeF(compW, compH),
                                         RenderROI(0.0f,
                                                   0.0f,
                                                   static_cast<float>(compW),
                                                   static_cast<float>(compH)));

    const QString key = RenderContextRegistry::instance().makeKey(
        RenderPurpose::EditorPreview,
        composition->id().toString(),
        currentFrame,
        ctx->resolutionScale);
    auto snapshot = createRenderContextSnapshot(*ctx, RenderPurpose::EditorPreview, key);
   RenderContextRegistry::instance().registerSnapshot(snapshot);
  }
 } // namespace

 class ArtifactPreviewCompositionPipeline::Impl
 {
 private:
  Artifact::ArtifactCompositionPtr composition_;
  ArtifactCore::LayerID selectedLayerId_ = ArtifactCore::LayerID::Nil();
  int64_t currentFrame_ = 0;

  // Crowd / Boids
  ArtifactCore::CrowdSettings crowdSettings_;
  std::unique_ptr<ArtifactCore::BoidsSwarmSystem> boidsSystem_;
  int64_t prevBoidsFrame_ = -1;

  void ensureBoidsSystem()
  {
   if (boidsSystem_) return;
   boidsSystem_ = std::make_unique<ArtifactCore::BoidsSwarmSystem>();
   // 160 normal boids + 20 predators (red) + 40 prey (green)
   boidsSystem_->initializeWithTypes(220, ArtifactCore::float3{800, 600, 400}, 20, 40);
   // Add some obstacles
   boidsSystem_->addObstacle(ArtifactCore::float3{200, 0, 0}, 60.0f);
   boidsSystem_->addObstacle(ArtifactCore::float3{-200, 150, 0}, 45.0f);
   boidsSystem_->addObstacle(ArtifactCore::float3{0, -200, 50}, 50.0f);
  }

  void updateBoids()
  {
   if (!crowdSettings_.enabled) return;
   ensureBoidsSystem();
   if (currentFrame_ == prevBoidsFrame_) return;
   float dt = std::max(0.001f, static_cast<float>(currentFrame_ - prevBoidsFrame_) / 60.0f);
   prevBoidsFrame_ = currentFrame_;

   boidsSystem_->separationWeight = crowdSettings_.separation * 3.0f;
   boidsSystem_->alignmentWeight = crowdSettings_.alignment * 2.0f;
   boidsSystem_->cohesionWeight = crowdSettings_.cohesion * 2.0f;
   // Target at center when no target set, with slight wander
   if (!boidsSystem_->hasTarget) {
    boidsSystem_->wanderWeight = crowdSettings_.jitter * 5.0f;
   }
   boidsSystem_->update(dt);
  }

  void renderBoids(Artifact::ArtifactIRenderer* renderer)
  {
   if (!crowdSettings_.enabled || !boidsSystem_) return;
   auto renderData = boidsSystem_->captureRenderData(currentFrame_);
   if (!renderData.particles.empty()) {
    renderer->drawParticles(std::move(renderData));
   }
   // Render obstacles as cyan circles
   for (const auto& obs : boidsSystem_->getObstacles()) {
    FloatColor cyan(0.0f, 0.8f, 1.0f, 0.4f);
    renderer->drawSolidRect(
     {obs.center.x - obs.radius, obs.center.y - obs.radius},
     {obs.radius * 2, obs.radius * 2},
     cyan);
    renderer->drawRectOutline(
     {obs.center.x - obs.radius, obs.center.y - obs.radius},
     {obs.radius * 2, obs.radius * 2},
     FloatColor(0.0f, 1.0f, 1.0f, 0.8f));
   }
   // Render target if set
   if (boidsSystem_->hasTarget) {
    FloatColor yellow(1.0f, 1.0f, 0.0f, 0.8f);
    auto& t = boidsSystem_->targetPosition;
    renderer->drawThickLineLocal({t.x - 10, t.y}, {t.x + 10, t.y}, 2.0f, yellow);
    renderer->drawThickLineLocal({t.x, t.y - 10}, {t.x, t.y + 10}, 2.0f, yellow);
   }
  }

 public:
  void setBoidsTarget(const QVector3D& pos)
  {
   ensureBoidsSystem();
   boidsSystem_->setTarget(ArtifactCore::float3{pos.x(), pos.y(), pos.z()});
  }

  void clearBoidsTarget()
  {
   if (boidsSystem_) {
    boidsSystem_->clearTarget();
   }
  }

  Impl() = default;
  ~Impl() = default;

  void setCrowdSettings(const ArtifactCore::CrowdSettings& s) { crowdSettings_ = s; }
  const ArtifactCore::CrowdSettings& crowdSettings() const { return crowdSettings_; }
  ArtifactCore::CrowdSettings& crowdSettings() { return crowdSettings_; }

  void render(Artifact::ArtifactIRenderer* renderer)
  {
   if (!composition_ || !renderer) return;

   registerPreviewContextSnapshot(composition_, currentFrame_);

   auto size = composition_->settings().compositionSize();
   float compW = (float)size.width();
   float compH = (float)size.height();
   const FramePosition currentFrame(currentFrame_);

   // 1. Background
   renderer->drawCheckerboard(0, 0, compW, compH, 16.0f,
                              { 0.3f, 0.3f, 0.3f, 1.0f },
                              { 0.4f, 0.4f, 0.4f, 1.0f });

   // 2. Layers
   const auto& layers = composition_->allLayerRef();
   const bool hasSoloLayer = std::any_of(layers.begin(), layers.end(), [](const ArtifactAbstractLayerPtr& layer) {
    return layer && layer->isVisible() && layer->isSolo();
   });
   for (const auto& layer : layers)
   {
    if (!layer || !layer->isVisible()) {
     continue;
    }
    if (hasSoloLayer && !layer->isSolo()) {
     continue;
    }
    if (!layer->isActiveAt(currentFrame)) {
     continue;
    }
    layer->goToFrame(currentFrame_);
    drawLayerForPreviewView(layer, renderer, selectedLayerId_);
   }

   // 3. UI Overlays (Gizmos)
   if (!selectedLayerId_.isNil())
   {
    auto layer = composition_->layerById(selectedLayerId_);
    if (layer && layer->isActiveAt(currentFrame))
    {
     if (layer->isCloneLayer()) {
      drawCloneSelectionOverlay(renderer, layer);
     } else {
      auto global = layer->getGlobalTransform();
      auto localBounds = layer->localBounds();
      if (!localBounds.isValid() || localBounds.width() <= 0.0 || localBounds.height() <= 0.0) {
       qCDebug(previewPipelineLog) << "[PreviewPipeline][Gizmo] skip draw: invalid local bounds"
                                   << "id=" << layer->id().toString()
                                   << "bounds=" << localBounds;
      } else {
       float w = (float)localBounds.width();
       float h = (float)localBounds.height();

       // Draw Bounding Box (transformed)
       // Since IRenderer doesn't have a drawPolygon, we draw 4 lines
       auto p0 = global.map(QPointF(localBounds.left(), localBounds.top()));
       auto p1 = global.map(QPointF(localBounds.right(), localBounds.top()));
       auto p2 = global.map(QPointF(localBounds.right(), localBounds.bottom()));
       auto p3 = global.map(QPointF(localBounds.left(), localBounds.bottom()));

       FloatColor cyan(0.0f, 0.7f, 1.0f, 1.0f);
       renderer->drawThickLineLocal({(float)p0.x(), (float)p0.y()}, {(float)p1.x(), (float)p1.y()}, 1.0f, cyan);
       renderer->drawThickLineLocal({(float)p1.x(), (float)p1.y()}, {(float)p2.x(), (float)p2.y()}, 1.0f, cyan);
       renderer->drawThickLineLocal({(float)p2.x(), (float)p2.y()}, {(float)p3.x(), (float)p3.y()}, 1.0f, cyan);
       renderer->drawThickLineLocal({(float)p3.x(), (float)p3.y()}, {(float)p0.x(), (float)p0.y()}, 1.0f, cyan);

       // Draw Anchor Point
       auto& t3d = layer->transform3D();
       auto pAnchor = global.map(QPointF(t3d.anchorX(), t3d.anchorY()));
       FloatColor white(1.0f, 1.0f, 1.0f, 1.0f);
       renderer->drawThickLineLocal({(float)pAnchor.x() - 5, (float)pAnchor.y()}, {(float)pAnchor.x() + 5, (float)pAnchor.y()}, 1.0f, white);
       renderer->drawThickLineLocal({(float)pAnchor.x(), (float)pAnchor.y() - 5}, {(float)pAnchor.x(), (float)pAnchor.y() + 5}, 1.0f, white);

       // Draw Corner Handles
       QPointF corners[4] = { {0,0}, {w,0}, {w,h}, {0,h} };
       for (auto& c : corners) {
        auto pc = global.map(c);
        renderer->drawSolidRect({(float)pc.x() - 3, (float)pc.y() - 3}, {6, 6}, white);
        renderer->drawRectOutline({(float)pc.x() - 3, (float)pc.y() - 3}, {6, 6}, FloatColor(0,0,0,1));
       }
      }
     }
    } else if (layer) {
     qCDebug(previewPipelineLog) << "[PreviewPipeline][Gizmo] skip draw: inactive frame"
                                 << "id=" << layer->id().toString()
                                 << "frame=" << currentFrame.framePosition();
    }
   }

   // 4. Crowd / Boids
   updateBoids();
   renderBoids(renderer);

   // 5. Grid
   renderer->drawGrid(0, 0, compW, compH, 100.0f, 1.0f, FloatColor(1.0f, 1.0f, 1.0f, 0.1f));

   renderer->flush();
  }

  void setComposition(Artifact::ArtifactCompositionPtr composition) { composition_ = composition; }
  void setSelectedLayerId(const ArtifactCore::LayerID& id) { selectedLayerId_ = id; }
  void setCurrentFrame(int64_t frame) { currentFrame_ = frame; }
  Artifact::ArtifactCompositionPtr composition() const { return composition_; }
 };

 ArtifactPreviewCompositionPipeline::ArtifactPreviewCompositionPipeline() :impl_(new Impl()) {}
 ArtifactPreviewCompositionPipeline::~ArtifactPreviewCompositionPipeline() { delete impl_; impl_ = nullptr; }
 void ArtifactPreviewCompositionPipeline::render(Artifact::ArtifactIRenderer* renderer) { if (impl_) impl_->render(renderer); }
 void ArtifactPreviewCompositionPipeline::setComposition(Artifact::ArtifactCompositionPtr composition) { if (impl_) impl_->setComposition(composition); }
 Artifact::ArtifactCompositionPtr ArtifactPreviewCompositionPipeline::composition() const { return impl_ ? impl_->composition() : Artifact::ArtifactCompositionPtr(); }
 void ArtifactPreviewCompositionPipeline::setSelectedLayerId(const ArtifactCore::LayerID& id) { if (impl_) impl_->setSelectedLayerId(id); }
 void ArtifactPreviewCompositionPipeline::setCurrentFrame(int64_t frame) { if (impl_) impl_->setCurrentFrame(frame); }
 void ArtifactPreviewCompositionPipeline::setCrowdSettings(const ArtifactCore::CrowdSettings& s) { if (impl_) impl_->setCrowdSettings(s); }
 const ArtifactCore::CrowdSettings& ArtifactPreviewCompositionPipeline::crowdSettings() const { static const ArtifactCore::CrowdSettings defaultSettings; return impl_ ? impl_->crowdSettings() : defaultSettings; }
 ArtifactCore::CrowdSettings& ArtifactPreviewCompositionPipeline::crowdSettings() { static ArtifactCore::CrowdSettings defaultSettings; return impl_ ? impl_->crowdSettings() : defaultSettings; }
 void ArtifactPreviewCompositionPipeline::setBoidsTarget(const QVector3D& pos) { if (impl_) impl_->setBoidsTarget(pos); }
 void ArtifactPreviewCompositionPipeline::clearBoidsTarget() { if (impl_) impl_->clearBoidsTarget(); }

}
