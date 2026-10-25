module;
#include <algorithm>
#include <QColor>
#include <QImage>
#include <QRectF>
#include <opencv2/core/mat.hpp>
module Artifact.Preview.Pipeline;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import CvUtils;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Color.Float;

namespace Artifact
{
QColor toPreviewQColor(const FloatColor& color)
{
  return QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
}

float previewLayerOpacity(const ArtifactAbstractLayer* layerPtr)
{
  return layerPtr ? layerPtr->opacity() : 0.0f;
}

namespace
{
EffectContext makePreviewEffectContext(ArtifactAbstractLayer* layer,
                                       const QRectF& roi)
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
  if (layer) {
    ctx.effectStrength = layer->effectEnvelope()
        .sample(ctx.layerFrame).effectStrength;
  }
  return ctx;
}
} // namespace

bool previewLayerHasRasterizerEffects(const ArtifactAbstractLayer* targetLayer)
{
  if (!targetLayer) return false;
  for (const auto& effect : targetLayer->getEffects()) {
    if (effect && effect->isEnabled() &&
        effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
      return true;
    }
  }
  return false;
}

void applyPreviewRasterizerEffects(ArtifactAbstractLayer* targetLayer,
                                   QImage& surface)
{
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

  const auto stageEffects = ArtifactAbstractEffect::sortedByStage(effects);
  for (const auto& effect : stageEffects) {
    if (!effect || !effect->isEnabled() ||
        effect->pipelineStage() != EffectPipelineStage::Rasterizer) {
      continue;
    }
    ArtifactCore::ImageF32x4RGBAWithCache next;
    effect->setContext(makePreviewEffectContext(
        targetLayer,
        QRectF(0.0, 0.0, static_cast<qreal>(current.width()),
               static_cast<qreal>(current.height()))));
    effect->applyToCpuSurface(current, next);
    current.swap(next);
  }
  surface = current.image().toQImage();
}
}
