module;
#include <QVector>
module Artifact.Preview.Pipeline;

import std;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Color.Float;

namespace Artifact
{
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

   auto size = composition_->settings().compositionSize();
   float compW = (float)size.width();
   float compH = (float)size.height();

   // 1. Background
   renderer->drawCheckerboard(0, 0, compW, compH, 16.0f,
                              { 0.3f, 0.3f, 0.3f, 1.0f },
                              { 0.4f, 0.4f, 0.4f, 1.0f });

   // 2. Layers (Rendering layers is omitted for brevity as it requires the full pipeline)
   
   // 3. UI Overlays (Gizmos)
   if (!selectedLayerId_.isNil())
   {
    auto layer = composition_->layerById(selectedLayerId_);
    if (layer)
    {
     auto global = layer->getGlobalTransform();
     auto sourceSize = layer->sourceSize();
     float w = (float)sourceSize.width;
     float h = (float)sourceSize.height;

     // Draw Bounding Box (transformed)
     // Since IRenderer doesn't have a drawPolygon, we draw 4 lines
     auto p0 = global.map(QPointF(0, 0));
     auto p1 = global.map(QPointF(w, 0));
     auto p2 = global.map(QPointF(w, h));
     auto p3 = global.map(QPointF(0, h));

     FloatColor cyan = { 0.0f, 0.7f, 1.0f, 1.0f };
     renderer->drawThickLineLocal({(float)p0.x(), (float)p0.y()}, {(float)p1.x(), (float)p1.y()}, 1.0f, cyan);
     renderer->drawThickLineLocal({(float)p1.x(), (float)p1.y()}, {(float)p2.x(), (float)p2.y()}, 1.0f, cyan);
     renderer->drawThickLineLocal({(float)p2.x(), (float)p2.y()}, {(float)p3.x(), (float)p3.y()}, 1.0f, cyan);
     renderer->drawThickLineLocal({(float)p3.x(), (float)p3.y()}, {(float)p0.x(), (float)p0.y()}, 1.0f, cyan);

     // Draw Anchor Point
     auto& t3d = layer->transform3D();
     auto pAnchor = global.map(QPointF(t3d.anchorX(), t3d.anchorY()));
     // Renderer should have a way to draw dots or small circles.
     // Let's use thick lines or small solid rects.
     float handleSize = 6.0f; // in units? No, ideally in pixels. 
     // For now, draw a small cross at anchor point
     renderer->drawThickLineLocal({(float)pAnchor.x() - 5, (float)pAnchor.y()}, {(float)pAnchor.x() + 5, (float)pAnchor.y()}, 1.0f, {1,1,1,1});
     renderer->drawThickLineLocal({(float)pAnchor.x(), (float)pAnchor.y() - 5}, {(float)pAnchor.x(), (float)pAnchor.y() + 5}, 1.0f, {1,1,1,1});

     // Draw Corner Handles
     QPointF corners[4] = { {0,0}, {w,0}, {w,h}, {0,h} };
     for (auto& c : corners) {
      auto pc = global.map(c);
      renderer->drawSolidRect({(float)pc.x() - 3, (float)pc.y() - 3}, {6, 6}, {1,1,1,1});
      renderer->drawRectOutline({(float)pc.x() - 3, (float)pc.y() - 3}, {6, 6}, {0,0,0,1});
     }
    }
   }

   // 4. Grid
   renderer->drawGrid(0, 0, compW, compH, 100.0f, 1.0f, { 1.0f, 1.0f, 1.0f, 0.1f });
   
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