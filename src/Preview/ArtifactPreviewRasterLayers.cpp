module;
#include <QImage>
#include <QMatrix4x4>
#include <QRectF>
module Artifact.Preview.Pipeline;

import Artifact.Layer.CloneEffectSupport;
import Artifact.Layer.Image;
import Artifact.Layer.Text;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Color.Float;

namespace Artifact
{
float previewLayerOpacity(const ArtifactAbstractLayer* layerPtr);
QColor toPreviewQColor(const FloatColor& color);
bool previewLayerHasRasterizerEffects(const ArtifactAbstractLayer* layerPtr);
void applyPreviewRasterizerEffects(ArtifactAbstractLayer* targetLayer, QImage& surface);

bool tryDrawImagePreview(ArtifactIRenderer* renderer,
                           ArtifactAbstractLayer* layerPtr,
                           const QRectF& localRect,
                           const ArtifactCore::LayerID& selectedLayerId)
{
  if (const auto imageLayer = dynamic_cast<ArtifactImageLayer*>(layerPtr)) {
    // GPU描画 (ImageF32バッファ) を正規とし、QImageはrasterizer/mask時のfallbackに残す。
    if (!previewLayerHasRasterizerEffects(layerPtr) && !layerPtr->hasMasks()) {
     imageLayer->draw(renderer);
     return true;
    }
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
     return true;
    }
   }
  return false;
}

bool tryDrawTextPreview(ArtifactIRenderer* renderer,
                           ArtifactAbstractLayer* layerPtr,
                           const QRectF& localRect,
                           const ArtifactCore::LayerID& selectedLayerId)
{
  if (const auto textLayer = dynamic_cast<ArtifactTextLayer*>(layerPtr)) {
    if (!previewLayerHasRasterizerEffects(layerPtr) && !layerPtr->hasMasks()) {
     textLayer->draw(renderer);
     return true;
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
     return true;
    }
   }
  return false;
}
}
