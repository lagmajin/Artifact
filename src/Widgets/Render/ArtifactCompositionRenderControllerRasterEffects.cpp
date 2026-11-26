module;

#include <DeviceContext.h>

#define NOMINMAX
#define QT_NO_KEYWORDS

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include <QDebug>
#include <QImage>
#include <QLoggingCategory>
#include <QPointF>
#include <QRectF>
#include <QRgb>
#include <QSize>
#include <QString>
#include <QtGlobal>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

module Artifact.Widgets.CompositionRenderController:RasterEffects;

import Artifact.Animation.LayerEffectEnvelope;
import Artifact.Composition.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.FrameSampler;
import Artifact.Layer.Abstract;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Render.Pipeline;
import Artifact.Render.PointwiseEffectFusion;
import Frame.Position;
import Frame.Rate;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import Utils.Id;

namespace Artifact {

Q_LOGGING_CATEGORY(compositionViewLog, "artifact.compositionview")

cv::Mat compositionQImageToCvMat(const QImage& image,
                                const bool cloneData = true) {
  QImage converted;
  const QImage* source = &image;
  switch (image.format()) {
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
    case QImage::Format_RGB32:
      break;
    case QImage::Format_RGB888: {
      cv::Mat rgb(image.height(), image.width(), CV_8UC3,
                  const_cast<uchar*>(image.constBits()), image.bytesPerLine());
      cv::Mat bgr;
      cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
      return bgr;
    }
    case QImage::Format_Grayscale8:
    case QImage::Format_Indexed8: {
      cv::Mat gray(image.height(), image.width(), CV_8UC1,
                   const_cast<uchar*>(image.constBits()), image.bytesPerLine());
      return cloneData ? gray.clone() : gray;
    }
    default:
      converted = image.convertToFormat(QImage::Format_ARGB32);
      source = &converted;
      break;
  }
  cv::Mat bgra(source->height(), source->width(), CV_8UC4,
               const_cast<uchar*>(source->constBits()), source->bytesPerLine());
  return cloneData ? bgra.clone() : bgra;
}

EffectContext makeControllerEffectContext(
    ArtifactAbstractLayer* layer, const QRectF& roi = QRectF(),
    const bool interactivePreview = false,
    const float resolutionScale = 1.0f) {

  EffectContext ctx;

  ctx.roi = roi;

  ctx.isInteractive = interactivePreview;

  ctx.resolutionScale = std::clamp(resolutionScale, 0.125f, 1.0f);

  if (!layer) {

    return ctx;

  }



  if (auto* composition =

          static_cast<ArtifactAbstractComposition*>(layer->composition())) {

    const auto compositionFrame = composition->framePosition().framePosition();

    ctx.compositionFrame = compositionFrame;

    ctx.layerFrame = compositionFrame - layer->inPoint().framePosition() +

                     layer->startTime().framePosition();

    ctx.frameRate = composition->frameRate().framerate();

    if (ctx.frameRate > 0.0) {

      ctx.timeSeconds = static_cast<double>(compositionFrame) / ctx.frameRate;

    }

  } else {

    ctx.layerFrame = layer->currentFrame();

    ctx.compositionFrame = ctx.layerFrame;

  }



  ctx.effectStrength = layer->effectEnvelope()

      .sample(ctx.layerFrame).effectStrength;



  auto& frameSampler = ArtifactEffectFrameSampler::instance();
  frameSampler.setActiveLayerId(layer->id().toString());
  frameSampler.setCurrentCompositionFrame(ctx.compositionFrame);
  ctx.sampler = &frameSampler;

  return ctx;

}

float layerOverscanPixels(const ArtifactAbstractLayer *layer) {
  if (!layer || layer->isAdjustmentLayer()) {
    return 0.0f;
  }
  return layer->enabledRasterizerOverscanPixels();
}

// Legacy standalone stack builder.  Superseded by buildGpuRasterEffectPlan,
  // which also carries the per-effect mask onto the pass; no caller remains.
  // Kept in step with that function so a future resurrection does not
  // reintroduce the stale "mask forces the CPU" rule.
  [[maybe_unused]] bool buildGpuSpatialEffectStack(
    ArtifactAbstractLayer* layer, GpuSpatialEffectStack* outStack) {
  if (!layer || !outStack || layer->isAdjustmentLayer()) return false;
  GpuSpatialEffectStack stack;
  for (const auto& effect : layer->getEffects()) {
   if (!effect || !effect->isEnabled()) continue;
   if (effect->pipelineStage() != EffectPipelineStage::Rasterizer ||
       effect->computeMode() == ComputeMode::CPU || effect->hasEffectRegion() ||
       effect->effectMaskImageCount() > 0 ||
       std::abs(effect->mix() - 1.0f) > 1.0e-6f ||
       !effect->appendGpuSpatialNodes(stack)) {
     return false;
   }
  }
  *outStack = stack;
  return stack.count > 0;
}

enum class GpuRasterPassKind : std::uint8_t {
  Pointwise,
  Spatial,
};

// Per-effect mask carried alongside a spatial node.  The mask images are
  // CPU-resident ImageF32x4_RGBA owned by the effect, so the plan only records
  // enough to upload and configure them; RenderPipeline::applyEffectMaskComposite
  // performs the blend after the node runs.
  //
  // This mirrors the CPU blend in ArtifactAbstractEffect::applyConfigured:
  // coverage starts at mix, is zeroed outside the region, is scaled by the
  // primary mask (with inversion and opacity), and is multiplied by each
  // additional mask image's alpha.
  //
  // Everything here is a fixed-capacity value: the plan is rebuilt every frame
  // on the render hot path, so a std::vector for the additional masks would be
  // a per-frame heap allocation. ArtifactAbstractEffect has no hard cap on
  // addEffectMaskImage, so an overflowing list rejects the plan (CPU path)
  // rather than silently truncating the blend.
  struct EffectMaskDescriptor {
    static constexpr std::size_t kMaxAdditionalMasks = 4;

    const float* primaryData = nullptr;
    int primaryWidth = 0;
    int primaryHeight = 0;
    float opacity = 1.0f;
    bool inverted = false;
    float mix = 1.0f;
    bool regionEnabled = false;
    float regionX = 0.0f;
    float regionY = 0.0f;
    float regionWidth = 0.0f;
    float regionHeight = 0.0f;
    std::array<const float*, kMaxAdditionalMasks> additionalData{};
    std::size_t additionalCount = 0;

    // A pass needs the composite only when at least one of the terms the CPU
    // blend uses is active. A fully-mixed, unregioned, unmasked effect needs
    // no blend at all.
    bool isActive() const {
      return primaryData != nullptr || additionalCount > 0 || regionEnabled ||
             mix < 1.0f;
    }

    bool isValid() const { return isActive(); }
  };

struct GpuRasterPass {
  GpuRasterPassKind kind = GpuRasterPassKind::Pointwise;
  ArtifactCore::PointwiseEffectStack pointwise;
  GpuSpatialEffectNode spatial;
  // Set only on a Spatial pass whose effect declared a usable mask.  The
  // controller applies it immediately after the node's dispatch.
  EffectMaskDescriptor mask;
};

// Builds the composite parameters a pass carries. Kept next to the descriptor
// so the plan and the dispatch cannot drift apart.
RenderPipeline::EffectMaskCompositeParams
toCompositeParams(const EffectMaskDescriptor& mask) {
  RenderPipeline::EffectMaskCompositeParams params;
  params.primaryMaskData = mask.primaryData;
  params.primaryMaskWidth = static_cast<Diligent::Uint32>(mask.primaryWidth);
  params.primaryMaskHeight = static_cast<Diligent::Uint32>(mask.primaryHeight);
  params.maskOpacity = mask.opacity;
  params.maskInverted = mask.inverted;
  params.mix = mask.mix;
  params.regionEnabled = mask.regionEnabled;
  params.regionX = mask.regionX;
  params.regionY = mask.regionY;
  params.regionWidth = mask.regionWidth;
  params.regionHeight = mask.regionHeight;
  params.additionalMaskData = mask.additionalData.data();
  params.additionalMaskCount = static_cast<Diligent::Uint32>(mask.additionalCount);
  return params;
}

struct GpuRasterEffectPlan {
  static constexpr std::size_t kCapacity = GpuSpatialEffectStack::kCapacity;
  std::array<GpuRasterPass, kCapacity> passes{};
  std::size_t count = 0;

  bool appendPointwise(ArtifactCore::PointwiseEffectStack&& stack) {
   if (count >= passes.size()) return false;
   passes[count].kind = GpuRasterPassKind::Pointwise;
   passes[count].pointwise = std::move(stack);
   ++count;
   return true;
  }

bool appendSpatial(const GpuSpatialEffectNode& node,
                     const EffectMaskDescriptor& mask = EffectMaskDescriptor{}) {
   if (count >= passes.size()) return false;
   passes[count].kind = GpuRasterPassKind::Spatial;
   passes[count].spatial = node;
   passes[count].mask = mask;
   ++count;
   return true;
  }
};

bool buildGpuRasterEffectPlan(
    ArtifactAbstractLayer* layer, GpuRasterEffectPlan* outPlan) {
  if (!layer || !outPlan || layer->isAdjustmentLayer() ||
      !layer->hasEnabledRasterizerEffect()) {
    return false;
  }
  GpuRasterEffectPlan plan;
  ArtifactCore::PointwiseEffectStack pointwise;
  std::uint32_t parameterSlot = 0;
  for (const auto& effect : layer->getEffects()) {
    if (!effect || !effect->isEnabled()) continue;
    if (effect->pipelineStage() != EffectPipelineStage::Rasterizer ||
        effect->computeMode() == ComputeMode::CPU) {
      return false;
    }

    // Mix, the effect region, the primary mask and the additional mask images
    // are all carried to the GPU composite, which reproduces the CPU blend in
    // applyConfigured. Nothing here forces the layer back to the CPU.
    EffectMaskDescriptor mask;
    mask.mix = effect->mix();
    mask.opacity = effect->maskOpacity();
    mask.inverted = effect->maskInverted();
    if (effect->hasEffectRegion()) {
      const QRectF region = effect->effectRegion();
      mask.regionEnabled = true;
      mask.regionX = static_cast<float>(region.x());
      mask.regionY = static_cast<float>(region.y());
      mask.regionWidth = static_cast<float>(region.width());
      mask.regionHeight = static_cast<float>(region.height());
    }
    if (effect->maskEnabled()) {
      const auto primary = effect->maskImage();
      // A mask of a different resolution than the surface is stretched by the
      // composite shader, matching the CPU path, so only a missing or empty
      // image disqualifies the layer.
      if (primary && primary->rgba32fData() && primary->width() > 0 &&
          primary->height() > 0) {
        mask.primaryData = primary->rgba32fData();
        mask.primaryWidth = primary->width();
        mask.primaryHeight = primary->height();
      }
    }
    // The additional masks are authored at the surface resolution and the CPU
    // path matches them 1:1, so one whose resolution differs is skipped here
    // too rather than stretched. The surface size is not known to this
    // builder, so the check is deferred to the composite, which compares
    // against the real surface; here only the capacity limit is enforced.
    if (effect->effectMaskImageCount() >
        static_cast<int>(EffectMaskDescriptor::kMaxAdditionalMasks)) {
      return false;
    }
    for (int index = 0; index < effect->effectMaskImageCount(); ++index) {
      const auto extra = effect->effectMaskImage(index);
      if (!extra || !extra->rgba32fData() || extra->width() <= 0 ||
          extra->height() <= 0) {
        continue;
      }
      mask.additionalData[mask.additionalCount++] = extra->rgba32fData();
    }

    if (effect->gpuRasterEffectDomain() == GpuRasterEffectDomain::Pointwise) {
      // Pointwise runs through LayerBlendPipeline, whose mask-mix contract is a
      // separate one: a single-layer mask carried in the reserved
      // kMaskMixParameterSlot, not this per-effect blend. Applying this
      // effect's mask on top would double-apply a layer mask and ignore the
      // effect's own, so a pointwise effect that needs the per-effect blend
      // stays on the CPU.
      if (mask.isActive()) {
        return false;
      }
      if (!effect->appendGpuPointwiseNodes(pointwise, parameterSlot) ||
          parameterSlot >= ArtifactCore::PointwiseEffectStack::kParameterSlotCount) {
        return false;
      }
      continue;
    }

    if (effect->gpuRasterEffectDomain() != GpuRasterEffectDomain::Spatial) {
      return false;
    }
    if (!pointwise.nodes().empty()) {
      if (!plan.appendPointwise(std::move(pointwise))) return false;
      pointwise = {};
      parameterSlot = 0;
    }
    GpuSpatialEffectStack spatial;
    if (!effect->appendGpuSpatialNodes(spatial) || spatial.count == 0) {
      return false;
    }
    // One effect can contribute several nodes (Blur emits one separable node
    // per iteration). Its mask must blend the FINAL result against the
    // original source, so it rides on the last node only; attaching it to
    // every node would composite an intermediate.
    for (std::size_t index = 0; index < spatial.count; ++index) {
      const bool isLast = (index + 1 == spatial.count);
      if (!plan.appendSpatial(spatial.nodes[index], isLast ? mask : EffectMaskDescriptor{})) {
        return false;
      }
    }
  }
  if (!pointwise.nodes().empty() && !plan.appendPointwise(std::move(pointwise))) {
    return false;
  }
  *outPlan = std::move(plan);
  return outPlan->count > 0;
}

// Identifies a cached adjustment-mask rasterization.  Mask content is
// fingerprinted from the resolved vertices so an unchanged mask reuses the last
// image instead of re-running OpenCV fillPoly on the full frame every time.
struct AdjustmentMaskCacheKey {
    QString layerId;
    int64_t frame = -1;
    int width = 0;
    int height = 0;
    quint64 contentHash = 0;

    bool matches(const AdjustmentMaskCacheKey& other) const {
        return layerId == other.layerId && frame == other.frame &&
               width == other.width && height == other.height &&
               contentHash == other.contentHash;
    }
};

// Cheap order-sensitive fingerprint of every mask vertex of a layer at the
// given frame.  This is O(vertices) with no allocation, so it is far cheaper
// than the fillPoly rasterization it guards, and it changes whenever an edit,
// a keyframe, a feather, an expansion, a mode or a path count changes.
quint64 adjustmentMaskContentHash(ArtifactAbstractLayer *targetLayer,
                                  const int64_t frame) {
  if (!targetLayer) {
    return 0;
  }
  quint64 hash = 1469598103934665603ull; // FNV-1a offset basis
  const auto mix = [&hash](quint64 value) {
    hash ^= value;
    hash *= 1099511628211ull; // FNV-1a prime
  };
  mix(static_cast<quint64>(targetLayer->maskCount()));
  for (int m = 0; m < targetLayer->maskCount(); ++m) {
    LayerMask resolvedMask;
    const LayerMask *maskView =
        targetLayer->resolvedMaskView(m, resolvedMask);
    if (!maskView) {
      mix(0x9e3779b97f4a7c15ull);
      continue;
    }
    mix(static_cast<quint64>(maskView->maskPathCount()));
    for (int p = 0; p < maskView->maskPathCount(); ++p) {
      const MaskPath path = (frame >= 0 && maskView->maskPath(p).hasAnimationKeyframes())
                                ? maskView->maskPath(p).sampleAtFrame(frame)
                                : maskView->maskPath(p);
      mix(static_cast<quint64>(path.vertexCount()));
      mix(path.isClosed() ? 1ull : 0ull);
      mix(path.isInverted() ? 1ull : 0ull);
      mix(static_cast<quint64>(path.mode()));
      mix(std::hash<float>{}(static_cast<float>(path.expansion().value)));
      mix(std::hash<float>{}(static_cast<float>(path.feather().value)));
      for (int v = 0; v < path.vertexCount(); ++v) {
        const MaskVertex vertex = path.vertex(v);
        mix(std::hash<float>{}(static_cast<float>(vertex.position.x())));
        mix(std::hash<float>{}(static_cast<float>(vertex.position.y())));
        mix(std::hash<float>{}(static_cast<float>(vertex.inTangent.x())));
        mix(std::hash<float>{}(static_cast<float>(vertex.inTangent.y())));
        mix(std::hash<float>{}(static_cast<float>(vertex.outTangent.x())));
        mix(std::hash<float>{}(static_cast<float>(vertex.outTangent.y())));
      }
    }
  }
  return hash;
}

// Rasterizes an adjustment layer's masks into a composition-sized RGBA8 image
// whose alpha carries the mask coverage.  The pointwise shader reads that alpha
// as the lerp factor, so the image must be white with the coverage in alpha.
//
// This delegates to LayerMask::compositeAlphaMask, which is the established
// path: it honours MaskMode (Add/Subtract/Intersect/Difference), inverted,
// feather and expansion, samples animated paths at the current frame, and
// maps layer-local coordinates through the supplied offset/scale.  A hand
// rolled QPainter fill would silently drop all of those.
QImage renderAdjustmentMaskToImage(ArtifactAbstractLayer *targetLayer,
                                   const int width, const int height,
                                   const int64_t frame) {
  if (!targetLayer || !targetLayer->hasMasks() || width <= 0 || height <= 0) {
    return QImage();
  }
  // Mask vertices live in layer-local space.  Map the composition rectangle back
  // into that space so the rasterizer receives the offset/scale it expects.
  const QRectF localRect = targetLayer->localBounds();
  if (!localRect.isValid() || localRect.width() <= 0.0 ||
      localRect.height() <= 0.0) {
    return QImage();
  }
  const float scaleX = static_cast<float>(width) / localRect.width();
  const float scaleY = static_cast<float>(height) / localRect.height();
  const float offsetX = static_cast<float>(-localRect.x() * scaleX);
  const float offsetY = static_cast<float>(-localRect.y() * scaleY);

  QImage result(width, height, QImage::Format_RGBA8888);
  if (result.isNull()) {
    return QImage();
  }

  // compositeAlphaMask writes a single-channel float coverage map.  Compose
  // every mask on the layer first (an adjustment layer may carry several),
  // then paint that coverage into the white image's alpha channel.
  cv::Mat combined = cv::Mat::ones(height, width, CV_32FC1);
  for (int m = 0; m < targetLayer->maskCount(); ++m) {
    LayerMask resolvedMask;
    const LayerMask *maskView =
        targetLayer->resolvedMaskView(m, resolvedMask);
    if (!maskView) {
      continue;
    }
    cv::Mat maskAlpha;
    maskView->compositeAlphaMask(width, height, &maskAlpha, offsetX, offsetY,
                                 scaleX, scaleY, frame);
    if (maskAlpha.empty()) {
      continue;
    }
    if (maskAlpha.type() != CV_32FC1) {
      maskAlpha.convertTo(maskAlpha, CV_32FC1);
    }
    // Layer masks compose by their own mode; the resolved view already folds
    // each path's mode, so multiplying the per-mask results is only correct
    // when there is a single mask.  With several, honour Add by union.
    if (m == 0) {
      combined = maskAlpha.clone();
    } else {
      cv::max(combined, maskAlpha, combined);
    }
  }
  if (combined.empty()) {
    return QImage();
  }
  cv::min(combined, 1.0f, combined);
  cv::max(combined, 0.0f, combined);

  for (int y = 0; y < height; ++y) {
    auto *out = reinterpret_cast<QRgb *>(result.scanLine(y));
    const float *row = combined.ptr<float>(y);
    for (int x = 0; x < width; ++x) {
      const int coverage = static_cast<int>(
          std::lround(std::clamp(row[x], 0.0f, 1.0f) * 255.0f));
      out[x] = qRgba(255, 255, 255, coverage);
    }
  }
  return result;
}

// Uploads an all-opaque mask so an opacity-only adjustment still has a view to
// mix against.  The shader multiplies the mask by the opacity parameter, so a
// constant 1.0 mask yields a plain opacity fade.  The caller supplies the
// device context because there is no renderer singleton to reach for.
Diligent::ITextureView *uploadOpaqueAdjustmentMask(
    RenderPipeline &pipeline, Diligent::IDeviceContext *context) {
  if (!context) {
    return nullptr;
  }
  const int width = static_cast<int>(pipeline.width());
  const int height = static_cast<int>(pipeline.height());
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  std::vector<std::uint8_t> opaque(
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u,
      255u);
  if (!pipeline.updateMatteSourceFromData(
          context, 0, opaque.data(),
          static_cast<Diligent::Uint32>(width),
          static_cast<Diligent::Uint32>(height),
          static_cast<Diligent::Uint32>(width * 4))) {
    return nullptr;
  }
  return pipeline.matteSourceSRV(0);
}

bool buildRasterizedSurfaceBuffer(ArtifactAbstractLayer *targetLayer,

                                  const QImage &surface,

                                  ArtifactCore::ImageF32x4_RGBA *outBuffer,

                                  const bool interactivePreview = false,

                                  const float resolutionScale = 1.0f,
                                  const bool deferRasterizerEffectsToGpu = false) {

  if (!targetLayer || surface.isNull() || !outBuffer) {

    return false;

  }



  const bool hasMasks = targetLayer->hasMasks();

  const bool hasRasterizerEffect = !deferRasterizerEffectsToGpu &&
      targetLayer->hasEnabledRasterizerEffect();

  if (!hasRasterizerEffect && !hasMasks) {

    return false;

  }



  cv::Mat mat = compositionQImageToCvMat(surface, true);

  // Keep the legacy QImage boundary in the same internal layout as the
  // float-image path. qImageToCvMat() returns BGRA for ARGB32 on Windows and
  // setFromCVMat() preserves CV_32FC4 channel order. The GPU image pipeline
  // uploads this canonical BGRA backing directly, so do not swizzle it here.
  if (mat.type() != CV_32FC4) {

    mat.convertTo(mat, CV_32FC4, 1.0 / 255.0);

  }



  if (hasMasks) {

    // Mask vertices are in layer-local space (centered at 0,0).

    // Translate to pixel space: pixel = localPos - localBounds.topLeft()

    const QRectF lb = targetLayer->localBounds();

    const float scaleX = static_cast<float>(mat.cols) / std::max(1.0f, static_cast<float>(lb.width()));

    const float scaleY = static_cast<float>(mat.rows) / std::max(1.0f, static_cast<float>(lb.height()));

    const float maskOffsetX = static_cast<float>(-lb.x() * scaleX);

    const float maskOffsetY = static_cast<float>(-lb.y() * scaleY);

    if (compositionViewLog().isDebugEnabled()) {

      qCDebug(compositionViewLog)

          << "[MaskTrace] rasterize begin"

          << "layer=" << targetLayer->id().toString()

          << "name=" << targetLayer->layerName()

          << "size=" << QSize(mat.cols, mat.rows)

          << "localBounds=" << lb

          << "maskCount=" << targetLayer->maskCount()

          << "offset=" << QPointF(maskOffsetX, maskOffsetY)

          << "scale=" << QPointF(scaleX, scaleY);

    }

    for (int m = 0; m < targetLayer->maskCount(); ++m) {

      LayerMask resolvedMask;
      const LayerMask* maskView =
          targetLayer->resolvedMaskView(m, resolvedMask);
      if (!maskView) continue;

      std::int64_t maskFrame = targetLayer->currentFrame();
      if (auto *maskComposition = static_cast<ArtifactAbstractComposition *>(
              targetLayer->composition())) {
        maskFrame = maskComposition->framePosition().framePosition();
      }
      maskView->applyToImage(mat.cols, mat.rows, &mat, maskOffsetX, maskOffsetY,
                             scaleX, scaleY, maskFrame);

    }

    if (compositionViewLog().isDebugEnabled()) {

      cv::Mat alpha;

      std::vector<cv::Mat> channels;

      cv::split(mat, channels);

      if (!channels.empty()) {

        alpha = channels.back();

        double alphaMin = 0.0;

        double alphaMax = 0.0;

        cv::minMaxLoc(alpha, &alphaMin, &alphaMax);

        qCDebug(compositionViewLog)

            << "[MaskTrace] rasterize end"

            << "layer=" << targetLayer->id().toString()

            << "name=" << targetLayer->layerName()

            << "alphaMin=" << alphaMin

            << "alphaMax=" << alphaMax

            << "hasMasks=" << hasMasks

            << "hasRasterizerEffect=" << hasRasterizerEffect;

      }

    }





  }

  if (hasRasterizerEffect) {
    const auto effects = targetLayer->getEffects();
    // Overscan is expressed in layer-space pixels.  It must remain stable
    // while the viewport uses an interactive preview scale: outputRect keeps
    // the same logical extent, so reducing this padding would stretch the
    // processed surface into that extent and make the layer appear to shrink
    // when the full-quality effect result arrives.
    const float overscanPixels = layerOverscanPixels(targetLayer);
    if (overscanPixels > 0.0f) {
      const QRectF localBounds = targetLayer->localBounds();
      const float scaleX = static_cast<float>(mat.cols) /
          std::max(1.0f, static_cast<float>(localBounds.width()));
      const float scaleY = static_cast<float>(mat.rows) /
          std::max(1.0f, static_cast<float>(localBounds.height()));
      const int paddingX = std::max(1, static_cast<int>(std::ceil(overscanPixels * scaleX)));
      const int paddingY = std::max(1, static_cast<int>(std::ceil(overscanPixels * scaleY)));
      cv::copyMakeBorder(mat, mat, paddingY, paddingY, paddingX, paddingX,
                         cv::BORDER_CONSTANT, cv::Scalar(0.0, 0.0, 0.0, 0.0));
    }
    ArtifactCore::ImageF32x4_RGBA cpuImage;

    cpuImage.setFromCVMat(mat);

    ArtifactCore::ImageF32x4RGBAWithCache current(cpuImage);



    // A temporal effect must not look back into frames that were rendered before
    // the layer's effect parameters or content changed, and must not look back
    // across a discontinuous time jump.  Both make the retained pixels a wrong
    // neighbour.  The sampler's own entry point (makeControllerEffectContext)
    // binds the active layer and frame; here we only drop stale entries before
    // the effect stack runs so the first temporal effect of the frame already
    // sees a consistent history.
    auto& frameSampler = ArtifactEffectFrameSampler::instance();
    frameSampler.setActiveLayerId(targetLayer->id().toString());
    frameSampler.setCurrentCompositionFrame(
        makeControllerEffectContext(targetLayer).compositionFrame);
    frameSampler.invalidateIfRevisionChanged(targetLayer->id().toString(),
                                            targetLayer->effectRevision());

    for (const auto &effect : effects) {

      if (!effect || !effect->isEnabled() ||

          effect->pipelineStage() != EffectPipelineStage::Rasterizer) {

        continue;

      }



      ArtifactCore::ImageF32x4RGBAWithCache next;

      effect->setContext(makeControllerEffectContext(

          targetLayer,

          QRectF(0.0, 0.0, static_cast<qreal>(current.width()),

                 static_cast<qreal>(current.height())),

          interactivePreview, resolutionScale));

      // This surface builder owns a CPU-resident F32 image and its caller
      // uploads the completed surface only after the whole effect stack has
      // finished. Running an AUTO/GPU implementation here would upload each
      // intermediate, synchronously WaitForIdle for readback, and then upload
      // the result again. Keep this CPU boundary explicit; effects that can
      // remain GPU-resident are handled by the render-pipeline fast paths.
      // Include override-based CPU references such as Glitch/Halftone/Old TV,
      // which do not install a cpuImpl adapter. GPU-only effects retain their
      // configured backend until they gain a texture-native contract.
      effect->applyToCpuSurface(current, next);

      current.swap(next);

    }

    // Publish only the completed rasterizer result.  Temporal effects can
    // sample deterministic, layer-local history without recursively reading
    // an intermediate effect stage from the frame currently being evaluated.
    // The revision stamps the entry so a later parameter or content change can
    // invalidate exactly these frames instead of leaking stale pixels.
    // Publish an independent output before handing the completed CPU buffer
    // to history. This preserves named-input/lookback retention without an
    // additional full-frame clone, and retains no intermediate GPU resources.
    outBuffer->setFromCVMat(current.image().toCVMat());
    frameSampler.storeLayerFrameOwned(targetLayer->id().toString(),
        makeControllerEffectContext(targetLayer).compositionFrame,
        std::move(current.image()),
        targetLayer->effectRevision());
    return true;

  }



  outBuffer->setFromCVMat(mat);

  return true;

}

} // namespace Artifact
