module;
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QImage>
#include <QMatrix4x4>
#include <QRectF>
module Artifact.Preview.Pipeline;

import Artifact.Layer.CloneEffectSupport;
import Artifact.Layer.Solid2D;
import Artifact.Layers.SolidImage;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Color.Float;

namespace Artifact
{
float previewLayerOpacity(const ArtifactAbstractLayer* layerPtr);
QColor toPreviewQColor(const FloatColor& color);
bool previewLayerHasRasterizerEffects(const ArtifactAbstractLayer* layerPtr);
void applyPreviewRasterizerEffects(ArtifactAbstractLayer* targetLayer, QImage& surface);

bool tryDrawSolid2DPreview(ArtifactIRenderer* renderer,
                           ArtifactAbstractLayer* layerPtr,
                           const QRectF& localRect,
                           const ArtifactCore::LayerID& selectedLayerId)
{
  if (const auto solid2D = dynamic_cast<ArtifactSolid2DLayer*>(layerPtr)) {
    if (previewLayerHasRasterizerEffects(layerPtr)) {
     const QSize surfaceSize(
         std::max(1, static_cast<int>(std::ceil(localRect.width()))),
         std::max(1, static_cast<int>(std::ceil(localRect.height()))));
     QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
     surface.fill(toPreviewQColor(solid2D->color()));
     applyPreviewRasterizerEffects(layerPtr, surface);
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
    } else if (solid2D->isGradientEnabled()) {
     const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
     drawWithClonerEffect(layerPtr, baseTransform, [renderer, localRect, solid2D, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
      const float opacity = previewLayerOpacity(layerPtr);
      renderer->drawGradientRectTransformed(static_cast<float>(localRect.x()),
                                         static_cast<float>(localRect.y()),
                                         static_cast<float>(localRect.width()),
                                         static_cast<float>(localRect.height()),
                                         transform,
                                         solid2D->gradientStartColor(),
                                         solid2D->gradientEndColor(),
                                         static_cast<int>(solid2D->fillType()),
                                         solid2D->gradientAngleDegrees(),
                                         solid2D->gradientReverse(),
                                         solid2D->gradientCenterX(),
                                         solid2D->gradientCenterY(),
                                         solid2D->gradientScale(),
                                         solid2D->gradientOffset(),
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
    return true;
   }
  return false;
}

bool tryDrawSolidImagePreview(ArtifactIRenderer* renderer,
                           ArtifactAbstractLayer* layerPtr,
                           const QRectF& localRect,
                           const ArtifactCore::LayerID& selectedLayerId)
{
  if (const auto solidImage = dynamic_cast<ArtifactSolidImageLayer*>(layerPtr)) {
     const bool gradientEnabled = solidImage->isGradientEnabled();
     if (previewLayerHasRasterizerEffects(layerPtr)) {
      QImage surface = gradientEnabled
                           ? solidImage->toQImage()
                           : QImage(std::max(1, static_cast<int>(std::ceil(localRect.width()))),
                                    std::max(1, static_cast<int>(std::ceil(localRect.height()))),
                                    QImage::Format_ARGB32_Premultiplied);
      if (!gradientEnabled) {
       surface.fill(toPreviewQColor(solidImage->color()));
      }
      applyPreviewRasterizerEffects(layerPtr, surface);
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
      // GPU生成を正規とし、QImageはrasterizer時のfallback分岐に残す。
      const QMatrix4x4 baseTransform = layerPtr->getGlobalTransform4x4();
      drawWithClonerEffect(layerPtr, baseTransform, [renderer, localRect, solidImage, layerPtr, selectedLayerId](const QMatrix4x4& transform, float weight) {
       const float opacity = previewLayerOpacity(layerPtr);
       renderer->drawGradientRectTransformed(static_cast<float>(localRect.x()),
                                      static_cast<float>(localRect.y()),
                                      static_cast<float>(localRect.width()),
                                      static_cast<float>(localRect.height()),
                                      transform,
                                      solidImage->gradientStartColor(),
                                      solidImage->gradientEndColor(),
                                      static_cast<int>(solidImage->fillType()),
                                      solidImage->gradientAngleDegrees(),
                                      solidImage->gradientReverse(),
                                      solidImage->gradientCenterX(),
                                      solidImage->gradientCenterY(),
                                      solidImage->gradientScale(),
                                      solidImage->gradientOffset(),
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
    return true;
   }
  return false;
}
}
