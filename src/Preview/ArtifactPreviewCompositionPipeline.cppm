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
#include <QSize>
#include <QSizeF>
#include <QRectF>
#include <QTransform>
#include <QDebug>
#include <QLoggingCategory>
#include <QString>
#include <Layer/ArtifactCloneEffectSupport.hpp>
module Artifact.Preview.Pipeline;




import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Solid2D;
import Artifact.Layer.Text;
import Artifact.Layer.Video;
import Artifact.Layer.Particle;
import Image.ImageF32x4_RGBA;
import Artifact.Layers.SolidImage;
import Artifact.Render.IRenderer;
import Artifact.Render.Context;
import Color.Float;
import Frame.Position;

namespace Artifact
{
 namespace
 {
  Q_LOGGING_CATEGORY(previewPipelineLog, "artifact.previewpipeline")

  QColor toQColor(const FloatColor& color)
  {
   return QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
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

   const QRectF worldRect = layerPtr->getGlobalTransform().mapRect(localRect);
   if (!worldRect.isValid() || worldRect.width() <= 0.0 || worldRect.height() <= 0.0) {
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

   if (const auto solid2D = dynamic_cast<ArtifactSolid2DLayer*>(layerPtr)) {
    if (hasRasterizerEffects(layerPtr)) {
     const QSize surfaceSize(
         std::max(1, static_cast<int>(std::ceil(localRect.width()))),
         std::max(1, static_cast<int>(std::ceil(localRect.height()))));
     QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
     surface.fill(toQColor(solid2D->color()));
     const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
     drawWithClonerEffect(layerPtr, baseTransform, [renderer, worldRect, surface, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
      const float opacity = layerPtr->opacity() * ((selectedLayerId.isNil() || layerPtr->id() == selectedLayerId) ? 1.0f : 0.22f);
      renderer->drawSpriteTransformed(static_cast<float>(worldRect.x()),
                                      static_cast<float>(worldRect.y()),
                                      static_cast<float>(worldRect.width()),
                                      static_cast<float>(worldRect.height()),
                                      transform, surface,
                                      opacity * weight);
     });
    } else {
     const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
     drawWithClonerEffect(layerPtr, baseTransform, [renderer, worldRect, solid2D, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
      const float opacity = layerPtr->opacity() * ((selectedLayerId.isNil() || layerPtr->id() == selectedLayerId) ? 1.0f : 0.22f);
      const FloatColor c = solid2D->color();
      renderer->drawSolidRectTransformed(static_cast<float>(worldRect.x()),
                                         static_cast<float>(worldRect.y()),
                                         static_cast<float>(worldRect.width()),
                                         static_cast<float>(worldRect.height()),
                                         transform,
                                         c,
                                         opacity * weight);
     });
    }
    return;
   }

   if (const auto solidImage = dynamic_cast<ArtifactSolidImageLayer*>(layerPtr)) {
    if (hasRasterizerEffects(layerPtr)) {
     const QSize surfaceSize(
         std::max(1, static_cast<int>(std::ceil(localRect.width()))),
         std::max(1, static_cast<int>(std::ceil(localRect.height()))));
     QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
     surface.fill(toQColor(solidImage->color()));
     const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
     drawWithClonerEffect(layerPtr, baseTransform, [renderer, worldRect, surface, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
      const float opacity = layerPtr->opacity() * ((selectedLayerId.isNil() || layerPtr->id() == selectedLayerId) ? 1.0f : 0.22f);
      renderer->drawSpriteTransformed(static_cast<float>(worldRect.x()),
                                      static_cast<float>(worldRect.y()),
                                      static_cast<float>(worldRect.width()),
                                      static_cast<float>(worldRect.height()),
                                      transform, surface,
                                      opacity * weight);
     });
    } else {
     const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
     drawWithClonerEffect(layerPtr, baseTransform, [renderer, worldRect, solidImage, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
      const float opacity = layerPtr->opacity() * ((selectedLayerId.isNil() || layerPtr->id() == selectedLayerId) ? 1.0f : 0.22f);
      renderer->drawSolidRectTransformed(static_cast<float>(worldRect.x()),
                                         static_cast<float>(worldRect.y()),
                                         static_cast<float>(worldRect.width()),
                                         static_cast<float>(worldRect.height()),
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
     drawWithClonerEffect(layerPtr, baseTransform, [renderer, worldRect, img, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
      const float opacity = layerPtr->opacity() * ((selectedLayerId.isNil() || layerPtr->id() == selectedLayerId) ? 1.0f : 0.22f);
      renderer->drawSpriteTransformed(static_cast<float>(worldRect.x()),
                                      static_cast<float>(worldRect.y()),
                                      static_cast<float>(worldRect.width()),
                                      static_cast<float>(worldRect.height()),
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
    if (!hasRasterizerEffects(layerPtr) && !layerPtr->hasMasks() && videoLayer->hasCurrentFrameBuffer()) {
     const ArtifactCore::ImageF32x4_RGBA& frameBuffer = videoLayer->currentFrameBuffer();
     const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
     drawWithClonerEffect(layerPtr, baseTransform, [&frameBuffer, renderer, worldRect, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
      const float opacity = layerPtr->opacity() * ((selectedLayerId.isNil() || layerPtr->id() == selectedLayerId) ? 1.0f : 0.22f);
      renderer->drawSpriteTransformed(static_cast<float>(worldRect.x()),
                                      static_cast<float>(worldRect.y()),
                                      static_cast<float>(worldRect.width()),
                                      static_cast<float>(worldRect.height()),
                                      transform, frameBuffer,
                                      opacity * weight);
     });
     return;
    }
    const QImage frame = videoLayer->currentFrameToQImage();
    if (!frame.isNull()) {
     const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
     drawWithClonerEffect(layerPtr, baseTransform, [renderer, worldRect, frame, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
      const float opacity = layerPtr->opacity() * ((selectedLayerId.isNil() || layerPtr->id() == selectedLayerId) ? 1.0f : 0.22f);
      renderer->drawSpriteTransformed(static_cast<float>(worldRect.x()),
                                      static_cast<float>(worldRect.y()),
                                      static_cast<float>(worldRect.width()),
                                      static_cast<float>(worldRect.height()),
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
     drawWithClonerEffect(layerPtr, baseTransform, [renderer, worldRect, textImage, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
      const float opacity = layerPtr->opacity() * ((selectedLayerId.isNil() || layerPtr->id() == selectedLayerId) ? 1.0f : 0.22f);
      renderer->drawSpriteTransformed(static_cast<float>(worldRect.x()),
                                      static_cast<float>(worldRect.y()),
                                      static_cast<float>(worldRect.width()),
                                      static_cast<float>(worldRect.height()),
                                      transform, textImage,
                                      opacity * weight);
     });
     return;
    }
   }

   if (const auto particleLayer = dynamic_cast<ArtifactParticleLayer*>(layerPtr)) {
    particleLayer->draw(renderer);
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
  
 public:
  Impl() = default;
  ~Impl() = default;

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
   const auto layers = composition_->allLayer();
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
    } else if (layer) {
     qCDebug(previewPipelineLog) << "[PreviewPipeline][Gizmo] skip draw: inactive frame"
                                 << "id=" << layer->id().toString()
                                 << "frame=" << currentFrame.framePosition();
    }
   }

   // 4. Grid
   renderer->drawGrid(0, 0, compW, compH, 100.0f, 1.0f, FloatColor(1.0f, 1.0f, 1.0f, 0.1f));
   
   renderer->flush();
  }

  void setComposition(Artifact::ArtifactCompositionPtr composition) { composition_ = composition; }
  void setSelectedLayerId(const ArtifactCore::LayerID& id) { selectedLayerId_ = id; }
  void setCurrentFrame(int64_t frame) { currentFrame_ = frame; }
  Artifact::ArtifactCompositionPtr composition() const { return composition_; }
 };

 ArtifactPreviewCompositionPipeline::ArtifactPreviewCompositionPipeline() :impl_(new Impl()) {}
 ArtifactPreviewCompositionPipeline::~ArtifactPreviewCompositionPipeline() { delete impl_; }
 void ArtifactPreviewCompositionPipeline::render(Artifact::ArtifactIRenderer* renderer) { impl_->render(renderer); }
 void ArtifactPreviewCompositionPipeline::setComposition(Artifact::ArtifactCompositionPtr composition) { impl_->setComposition(composition); }
 Artifact::ArtifactCompositionPtr ArtifactPreviewCompositionPipeline::composition() const { return impl_->composition(); }
 void ArtifactPreviewCompositionPipeline::setSelectedLayerId(const ArtifactCore::LayerID& id) { impl_->setSelectedLayerId(id); }
 void ArtifactPreviewCompositionPipeline::setCurrentFrame(int64_t frame) { impl_->setCurrentFrame(frame); }

}
