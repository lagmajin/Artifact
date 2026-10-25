module;
#include <QMatrix4x4>
#include <QRectF>
module Artifact.Preview.Pipeline;

import Artifact.Layer.CloneEffectSupport;
import Artifact.Layer.Video;
import Image.ImageF32x4_RGBA;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Color.Float;
import Utils.Id;

namespace Artifact
{
float previewLayerOpacity(const ArtifactAbstractLayer* layerPtr);
bool previewLayerHasRasterizerEffects(const ArtifactAbstractLayer* layerPtr);

bool tryDrawVideoPreview(ArtifactIRenderer* renderer,
                         ArtifactAbstractLayer* layerPtr,
                         const QRectF& localRect,
                         const ArtifactCore::LayerID& selectedLayerId)
{
  if (const auto videoLayer = dynamic_cast<ArtifactVideoLayer*>(layerPtr)) {
    const ArtifactCore::ImageF32x4_RGBA cachedFrame =
        videoLayer->cachedFrameImageBuffer(layerPtr->currentFrame());
    if (!previewLayerHasRasterizerEffects(layerPtr) && !layerPtr->hasMasks() && !cachedFrame.isEmpty()) {
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
     return true;
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
     return true;
    }
   }
  return false;
}
}
