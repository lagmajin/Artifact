module;
#include <QVector>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QImage>
#include <QColor>
#include <QPainter>
#include <QPointF>
#include <QMatrix4x4>
#include <QVector4D>
#include <QSize>
#include <QSizeF>
#include <QRectF>
#include <QTransform>
#include <QDebug>
#include <QLoggingCategory>
#include <QString>
#include <opencv2/opencv.hpp>
#include <Layer/ArtifactCloneEffectSupport.hpp>
module Artifact.Preview.Pipeline;




import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Clone;
import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Solid2D;
import Artifact.Layer.Text;
import Artifact.Layer.Video;
import Artifact.Layer.FormParticle;
import Image.ImageF32x4_RGBA;
import Artifact.Layers.SolidImage;
import Artifact.Render.IRenderer;
import Artifact.Render.Context;
import Color.Float;
import Frame.Position;
import CvUtils;
import ArtifactCore.Crowd.Boids;

namespace Artifact
{
 namespace
 {
  Q_LOGGING_CATEGORY(previewPipelineLog, "artifact.previewpipeline")

  QColor toQColor(const FloatColor& color)
  {
   return QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
  }

  float previewLayerOpacity(const ArtifactAbstractLayer* layerPtr)
  {
   if (!layerPtr) {
    return 0.0f;
   }
   return layerPtr->opacity();
  }

  EffectContext makePreviewEffectContext(ArtifactAbstractLayer* layer,
                                         const QRectF& roi = QRectF())
  {
   EffectContext ctx;
   ctx.roi = roi;
   ctx.isInteractive = true;
   ctx.layerFrame = layer ? layer->currentFrame() : 0;
   if (auto* composition =
           layer ? dynamic_cast<ArtifactAbstractComposition*>(
                       layer->compositionObject())
                 : nullptr) {
    ctx.compositionFrame = composition->framePosition().framePosition();
    ctx.frameRate = std::max(
        1.0f, static_cast<float>(composition->frameRate().framerate()));
   } else {
    ctx.compositionFrame = ctx.layerFrame;
    ctx.frameRate = 30.0;
   }
   ctx.timeSeconds = ctx.frameRate > 0.0
                         ? static_cast<double>(ctx.compositionFrame) / ctx.frameRate
                         : 0.0;
   return ctx;
  }

  void drawCloneSelectionOverlay(ArtifactIRenderer* renderer,
                                 const ArtifactAbstractLayerPtr& layer)
  {
   if (!renderer || !layer) {
    return;
   }

   const auto cloneLayer = std::dynamic_pointer_cast<ArtifactCloneLayer>(layer);
   if (!cloneLayer) {
    return;
   }

   const QRectF localBounds = layer->localBounds();
   if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
       localBounds.height() <= 0.0) {
    return;
   }

   const auto clones = cloneLayer->generateCloneData();
   if (clones.empty()) {
    return;
   }

   const QTransform globalTransform = layer->getGlobalTransform();
   const FloatColor outerColor(0.96f, 0.56f, 0.18f, 0.90f);
   const FloatColor innerColor(0.18f, 0.10f, 0.04f, 0.64f);

   const auto mapClonePoint = [&](const QMatrix4x4& cloneTransform,
                                  const QPointF& point) -> QPointF {
    const QVector4D mapped =
        cloneTransform * QVector4D(static_cast<float>(point.x()),
                                   static_cast<float>(point.y()),
                                   0.0f, 1.0f);
    return globalTransform.map(QPointF(static_cast<qreal>(mapped.x()),
                                       static_cast<qreal>(mapped.y())));
   };

   for (const auto& clone : clones) {
    if (!clone.visible) {
     continue;
    }

    const QPointF tl = mapClonePoint(clone.transform, localBounds.topLeft());
    const QPointF tr = mapClonePoint(clone.transform, localBounds.topRight());
    const QPointF br = mapClonePoint(clone.transform, localBounds.bottomRight());
    const QPointF bl = mapClonePoint(clone.transform, localBounds.bottomLeft());

    renderer->drawThickLineLocal({static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                                 {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                                 1.7f, outerColor);
    renderer->drawThickLineLocal({static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                                 {static_cast<float>(br.x()), static_cast<float>(br.y())},
                                 1.7f, outerColor);
    renderer->drawThickLineLocal({static_cast<float>(br.x()), static_cast<float>(br.y())},
                                 {static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                                 1.7f, outerColor);
    renderer->drawThickLineLocal({static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                                 {static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                                 1.7f, outerColor);
    renderer->drawThickLineLocal({static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                                 {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                                 0.8f, innerColor);
    renderer->drawThickLineLocal({static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                                 {static_cast<float>(br.x()), static_cast<float>(br.y())},
                                 0.8f, innerColor);
    renderer->drawThickLineLocal({static_cast<float>(br.x()), static_cast<float>(br.y())},
                                 {static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                                 0.8f, innerColor);
    renderer->drawThickLineLocal({static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                                 {static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                                 0.8f, innerColor);
   }
  }

 void drawLayerForPreviewView(const ArtifactAbstractLayerPtr& layer,
                               ArtifactIRenderer* renderer,
                               const ArtifactCore::LayerID& selectedLayerId)
  {
   ArtifactAbstractLayer* layerPtr = layer.get();
   if (!layerPtr || !renderer) return;

   const QRectF localRect = layerPtr->localBounds();
   if (!localRect.isValid() || localRect.width() <= 0.0 || localRect.height() <= 0.0) {
    return;
   }

   const auto hasRasterizerEffects = [](const ArtifactAbstractLayer* targetLayer) {
    if (!targetLayer) return false;
    for (const auto& effect : targetLayer->getEffects()) {
     if (effect && effect->isEnabled() &&
         effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
      return true;
     }
    }
    return false;
   };

   const auto applyRasterizerEffects = [](ArtifactAbstractLayer* targetLayer, QImage& surface) {
    if (!targetLayer || surface.isNull()) return;
    const auto effects = targetLayer->getEffects();
    if (effects.empty()) return;

    cv::Mat mat = ArtifactCore::CvUtils::qImageToCvMat(surface, true);
    if (mat.type() != CV_32FC4) {
     mat.convertTo(mat, CV_32FC4, 1.0 / 255.0);
    }

    ArtifactCore::ImageF32x4_RGBA cpuImage;
    cpuImage.setFromCVMat(mat);
    ArtifactCore::ImageF32x4RGBAWithCache current(cpuImage);

    for (const auto& effect : effects) {
     if (!effect || !effect->isEnabled() ||
         effect->pipelineStage() != EffectPipelineStage::Rasterizer) {
      continue;
     }
     ArtifactCore::ImageF32x4RGBAWithCache next;
     effect->setContext(makePreviewEffectContext(
         targetLayer,
         QRectF(0.0, 0.0, static_cast<qreal>(current.width()),
                static_cast<qreal>(current.height()))));
     effect->applyConfigured(current, next);
     current = next;
    }
    surface = current.image().toQImage();
   };

   if (const auto solid2D = dynamic_cast<ArtifactSolid2DLayer*>(layerPtr)) {
    if (hasRasterizerEffects(layerPtr)) {
     const QSize surfaceSize(
         std::max(1, static_cast<int>(std::ceil(localRect.width()))),
         std::max(1, static_cast<int>(std::ceil(localRect.height()))));
     QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
     surface.fill(toQColor(solid2D->color()));
     applyRasterizerEffects(layerPtr, surface);
     const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
     drawWithClonerEffect(layerPtr, baseTransform, [renderer, localRect, surface, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
      const float opacity = previewLayerOpacity(layerPtr);
      renderer->drawSpriteTransformed(static_cast<float>(localRect.x()),
                                      static_cast<float>(localRect.y()),
                                      static_cast<float>(localRect.width()),
                                      static_cast<float>(localRect.height()),
                                      transform, surface,
                                      opacity * weight);
     });
    } else {
     const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
     drawWithClonerEffect(layerPtr, baseTransform, [renderer, localRect, solid2D, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
      const float opacity = previewLayerOpacity(layerPtr);
      const FloatColor c = solid2D->color();
      renderer->drawSolidRectTransformed(static_cast<float>(localRect.x()),
                                         static_cast<float>(localRect.y()),
                                         static_cast<float>(localRect.width()),
                                         static_cast<float>(localRect.height()),
                                         transform,
                                         c,
                                         opacity * weight);
     });
    }
    return;
   }

    if (const auto solidImage = dynamic_cast<ArtifactSolidImageLayer*>(layerPtr)) {
     const bool gradientEnabled = solidImage->isGradientEnabled();
     if (hasRasterizerEffects(layerPtr)) {
      QImage surface = gradientEnabled
                           ? solidImage->toQImage()
                           : QImage(std::max(1, static_cast<int>(std::ceil(localRect.width()))),
                                    std::max(1, static_cast<int>(std::ceil(localRect.height()))),
                                    QImage::Format_ARGB32_Premultiplied);
      if (!gradientEnabled) {
       surface.fill(toQColor(solidImage->color()));
      }
      applyRasterizerEffects(layerPtr, surface);
      const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
      drawWithClonerEffect(layerPtr, baseTransform, [renderer, localRect, surface, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
       const float opacity = previewLayerOpacity(layerPtr);
       renderer->drawSpriteTransformed(static_cast<float>(localRect.x()),
                                      static_cast<float>(localRect.y()),
                                      static_cast<float>(localRect.width()),
                                      static_cast<float>(localRect.height()),
                                       transform, surface,
                                       opacity * weight);
      });
     } else if (gradientEnabled) {
      const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
      const QImage surface = solidImage->toQImage();
      drawWithClonerEffect(layerPtr, baseTransform, [renderer, localRect, surface, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
       const float opacity = previewLayerOpacity(layerPtr);
       renderer->drawSpriteTransformed(static_cast<float>(localRect.x()),
                                       static_cast<float>(localRect.y()),
                                       static_cast<float>(localRect.width()),
                                       static_cast<float>(localRect.height()),
                                       transform, surface,
                                       opacity * weight);
      });
     } else {
      const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
      drawWithClonerEffect(layerPtr, baseTransform, [renderer, localRect, solidImage, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
       const float opacity = previewLayerOpacity(layerPtr);
       renderer->drawSolidRectTransformed(static_cast<float>(localRect.x()),
                                          static_cast<float>(localRect.y()),
                                         static_cast<float>(localRect.width()),
                                         static_cast<float>(localRect.height()),
                                         transform,
                                         solidImage->color(),
                                         opacity * weight);
     });
    }
    return;
   }

   if (const auto imageLayer = dynamic_cast<ArtifactImageLayer*>(layerPtr)) {
    const QImage img = imageLayer->toQImage();
    if (!img.isNull()) {
     const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
     drawWithClonerEffect(layerPtr, baseTransform, [renderer, localRect, img, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
      const float opacity = previewLayerOpacity(layerPtr);
      renderer->drawSpriteTransformed(static_cast<float>(localRect.x()),
                                      static_cast<float>(localRect.y()),
                                      static_cast<float>(localRect.width()),
                                      static_cast<float>(localRect.height()),
                                      transform, img,
                                      opacity * weight);
     });
     return;
    }
   }

   if (const auto svgLayer = dynamic_cast<ArtifactSvgLayer*>(layerPtr)) {
    if (svgLayer->isLoaded()) {
     svgLayer->draw(renderer);
     return;
    }
   }

   if (const auto videoLayer = dynamic_cast<ArtifactVideoLayer*>(layerPtr)) {
    const ArtifactCore::ImageF32x4_RGBA cachedFrame =
        videoLayer->cachedFrameImageBuffer(layerPtr->currentFrame());
    if (!hasRasterizerEffects(layerPtr) && !layerPtr->hasMasks() && !cachedFrame.isEmpty()) {
     const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
     drawWithClonerEffect(layerPtr, baseTransform, [cachedFrame, renderer, localRect, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
      const float opacity = previewLayerOpacity(layerPtr);
      renderer->drawSpriteTransformed(static_cast<float>(localRect.x()),
                                      static_cast<float>(localRect.y()),
                                      static_cast<float>(localRect.width()),
                                      static_cast<float>(localRect.height()),
                                      transform, cachedFrame,
                                      opacity * weight);
     });
     return;
    }
    ArtifactCore::ImageF32x4_RGBA frame = videoLayer->currentFrameBuffer();
    if (frame.isEmpty()) {
     frame = videoLayer->cachedFrameImageBuffer(layerPtr->currentFrame());
    }
    if (!frame.isEmpty()) {
     const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
     drawWithClonerEffect(layerPtr, baseTransform, [renderer, localRect, frame, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
      const float opacity = previewLayerOpacity(layerPtr);
      renderer->drawSpriteTransformed(static_cast<float>(localRect.x()),
                                      static_cast<float>(localRect.y()),
                                      static_cast<float>(localRect.width()),
                                      static_cast<float>(localRect.height()),
                                      transform, frame,
                                      opacity * weight);
     });
     return;
    }
   }

   if (const auto textLayer = dynamic_cast<ArtifactTextLayer*>(layerPtr)) {
    if (!hasRasterizerEffects(layerPtr) && !layerPtr->hasMasks()) {
     textLayer->draw(renderer);
     return;
    }
    const QImage textImage = textLayer->toQImage();
    if (!textImage.isNull()) {
     const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
     drawWithClonerEffect(layerPtr, baseTransform, [renderer, localRect, textImage, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
      const float opacity = previewLayerOpacity(layerPtr);
      renderer->drawSpriteTransformed(static_cast<float>(localRect.x()),
                                      static_cast<float>(localRect.y()),
                                      static_cast<float>(localRect.width()),
                                      static_cast<float>(localRect.height()),
                                      transform, textImage,
                                      opacity * weight);
     });
     return;
    }
   }

   if (layerPtr->isParticleLayer()) {
    layerPtr->draw(renderer);
    return;
   }
   if (const auto formParticleLayer = dynamic_cast<ArtifactFormParticleLayer*>(layerPtr)) {
    formParticleLayer->draw(renderer);
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
   boidsSystem_->initializeWithTypes(220, float3{800, 600, 400}, 20, 40);
   // Add some obstacles
   boidsSystem_->addObstacle(float3{200, 0, 0}, 60.0f);
   boidsSystem_->addObstacle(float3{-200, 150, 0}, 45.0f);
   boidsSystem_->addObstacle(float3{0, -200, 50}, 50.0f);
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
    renderer->drawParticles(renderData);
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
    qCDebug(previewPipelineLog) << "[PreviewPipeline][Gizmo]"
                                << "selectedLayerId=" << selectedLayerId_.toString()
                                << "hasLayer=" << static_cast<bool>(layer)
                                << "frame=" << currentFrame.framePosition()
                                << "active=" << (layer ? layer->isActiveAt(currentFrame) : false);
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
       qCDebug(previewPipelineLog) << "[PreviewPipeline][Gizmo] draw"
                                   << "id=" << layer->id().toString()
                                   << "bounds=" << localBounds
                                   << "m11=" << global.m11()
                                   << "m12=" << global.m12()
                                   << "m21=" << global.m21()
                                   << "m22=" << global.m22()
                                   << "dx=" << global.dx()
                                   << "dy=" << global.dy();
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
