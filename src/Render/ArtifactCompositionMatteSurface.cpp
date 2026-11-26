module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <utility>
#include <vector>

#include <QHash>
#include <QImage>
#include <QRgb>
#include <QSize>
#include <QtGlobal>

export module Artifact.Render.CompositionMatteSurface;

import Artifact.Layer.Abstract;
import Artifact.Layer.Matte;
import FloatRGBA;
import Image.ImageF32x4_RGBA;
import Utils.Id;

namespace Artifact {

export QImage fitMatteSourceToTarget(const QImage& source,
                                     const QSize& targetSize,
                                     MatteFitMode fitMode)
{
  if (source.isNull() || targetSize.width() <= 0 || targetSize.height() <= 0) {
    return {};
  }

  const QImage rgbaSource = source.convertToFormat(QImage::Format_RGBA8888);
  if (fitMode == MatteFitMode::Stretch) {
    return rgbaSource.scaled(targetSize, Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation);
  }

  const float sourceAspect = static_cast<float>(rgbaSource.width()) /
                             static_cast<float>(std::max(1, rgbaSource.height()));
  const float targetAspect = static_cast<float>(targetSize.width()) /
                             static_cast<float>(std::max(1, targetSize.height()));
  int scaledWidth = rgbaSource.width();
  int scaledHeight = rgbaSource.height();
  if (fitMode == MatteFitMode::Fit || fitMode == MatteFitMode::Fill) {
    const bool fitInside = fitMode == MatteFitMode::Fit;
    const float scale = fitInside
        ? (sourceAspect > targetAspect
               ? static_cast<float>(targetSize.width()) / rgbaSource.width()
               : static_cast<float>(targetSize.height()) / rgbaSource.height())
        : (sourceAspect > targetAspect
               ? static_cast<float>(targetSize.height()) / rgbaSource.height()
               : static_cast<float>(targetSize.width()) / rgbaSource.width());
    scaledWidth = std::max(1, qRound(rgbaSource.width() * scale));
    scaledHeight = std::max(1, qRound(rgbaSource.height() * scale));
  }

  const QImage scaled = rgbaSource.scaled(scaledWidth, scaledHeight,
                                          Qt::IgnoreAspectRatio,
                                          Qt::SmoothTransformation);
  QImage result(targetSize, QImage::Format_RGBA8888);
  result.fill(Qt::transparent);
  const int copyWidth = std::min(targetSize.width(), scaled.width());
  const int copyHeight = std::min(targetSize.height(), scaled.height());
  const int sourceX = std::max(0, (scaled.width() - copyWidth) / 2);
  const int sourceY = std::max(0, (scaled.height() - copyHeight) / 2);
  const int targetX = std::max(0, (targetSize.width() - copyWidth) / 2);
  const int targetY = std::max(0, (targetSize.height() - copyHeight) / 2);
  for (int y = 0; y < copyHeight; ++y) {
    const auto* sourceBits = scaled.constScanLine(sourceY + y) + sourceX * 4;
    auto* targetBits = result.scanLine(targetY + y) + targetX * 4;
    std::memcpy(targetBits, sourceBits, static_cast<size_t>(copyWidth) * 4u);
  }
  return result;
}

static float combineLayerMatteValue(float current, float next,
                                    MatteBlendMode blendMode,
                                    bool hasCurrent)
{
  if (!hasCurrent) {
    return std::clamp(next, 0.0f, 1.0f);
  }
  switch (blendMode) {
  case MatteBlendMode::Add:
    return std::min(1.0f, current + next);
  case MatteBlendMode::Subtract:
    return std::max(0.0f, current - next);
  case MatteBlendMode::Intersect:
    return std::min(current, next);
  case MatteBlendMode::Difference:
    return std::abs(current - next);
  }
  return current;
}

static std::vector<float> evaluateLayerMatteReferences(
    const std::vector<std::vector<float>>& sources,
    const std::vector<LayerMatteReference>& references,
    int width, int height)
{
  std::vector<float> result(static_cast<size_t>(std::max(0, width)) *
                            static_cast<size_t>(std::max(0, height)), 0.0f);
  if (width <= 0 || height <= 0) {
    return result;
  }

  size_t sourceIndex = 0;
  bool hasCurrent = false;
  for (const auto& ref : references) {
    if (!ref.enabled || ref.sourceLayerId.isNil()) {
      continue;
    }
    if (sourceIndex >= sources.size()) {
      break;
    }
    const auto& source = sources[sourceIndex++];
    for (size_t pixel = 0; pixel < result.size(); ++pixel) {
      const float next = pixel < source.size() ? source[pixel] : 0.0f;
      result[pixel] = combineLayerMatteValue(
          result[pixel], next, ref.blendMode, hasCurrent);
    }
    hasCurrent = true;
  }
  return result;
}

/// Apply track matte to a layer surface by evaluating its matte references.

/// Modifies the surface's alpha channel in-place.

export void applyLayerMatteToSurface(

    ArtifactAbstractLayer *layer, QImage &surface,

    const std::function<QImage(const ArtifactCore::Id &)> &sourceResolver)

{

    if (!layer || !layer->hasEnabledExternalMatteReference()) return;

    auto mattes = layer->matteReferences();



    const int w = surface.width();

    const int h = surface.height();

    if (w <= 0 || h <= 0) return;

    QHash<ArtifactCore::Id, QImage> resolvedSources;
    for (const auto& ref : mattes) {
        if (!ref.enabled || ref.sourceLayerId.isNil() ||
            ref.sourceLayerId == layer->id()) {
          continue;
        }
        const QImage source = sourceResolver(ref.sourceLayerId);
        if (source.isNull() ||
            fitMatteSourceToTarget(source, surface.size(), ref.fitMode).isNull()) {
            return;
        }
        resolvedSources.insert(ref.sourceLayerId, source);
    }



    // Build source alpha buffers from matte source layers

    std::vector<std::vector<float>> sources;
    std::vector<LayerMatteReference> activeMatteRefs;

    for (const auto &ref : mattes) {

        if (!ref.enabled || ref.sourceLayerId.isNil() ||
            ref.sourceLayerId == layer->id()) {
          continue;
        }



        QImage srcImage = fitMatteSourceToTarget(
            resolvedSources.value(ref.sourceLayerId), surface.size(), ref.fitMode);



        std::vector<float> alpha(static_cast<size_t>(w) * h, 0.0f);

        const bool useLuma = (ref.type == MatteType::Luma || ref.type == MatteType::InverseLuma);



        const auto buildAlphaRows = [&](int yBegin, int yEnd) {
          for (int y = yBegin; y < yEnd; ++y) {
            const auto* srcLine = srcImage.constScanLine(y);
            for (int x = 0; x < w; ++x) {
                const auto* px = srcLine + x * 4;

                if (useLuma) {

                    // ITU-R BT.601 luma

                    float luma = 0.299f * px[0] + 0.587f * px[1] + 0.114f * px[2];

                    alpha[static_cast<size_t>(y) * w + x] = luma / 255.0f;

                } else {

                    alpha[static_cast<size_t>(y) * w + x] = px[3] / 255.0f;

                }

            }
          }
        };
        if (static_cast<size_t>(w) * static_cast<size_t>(h) >= 256u * 1024u) {
          buildAlphaRows(0, h);
        } else {
          buildAlphaRows(0, h);
        }



        // Invert if needed

        if (ref.invert) {
            const auto invertAlpha = [&](size_t begin, size_t end) {
                for (size_t i = begin; i != end; ++i) {
                    alpha[i] = 1.0f - alpha[i];
                }
            };
            if (alpha.size() >= 256u * 1024u) {
                invertAlpha(0, alpha.size());
            } else {
                invertAlpha(0, alpha.size());
            }
        }

        const float matteOpacity = std::clamp(ref.opacity, 0.0f, 1.0f);
        for (float& value : alpha) {
            value *= matteOpacity;
        }



        sources.push_back(std::move(alpha));
        activeMatteRefs.push_back(ref);

    }



    if (sources.empty()) return;



    const auto matteValues = evaluateLayerMatteReferences(
        sources, activeMatteRefs, w, h);
    if (matteValues.empty()) return;



    // Apply matte mask to premultiplied RGBA surface.

    surface = surface.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    auto *surfaceBits = surface.bits();
    const int surfaceStride = surface.bytesPerLine();
    const auto applyResultRows = [&](int yBegin, int yEnd) {
      for (int y = yBegin; y < yEnd; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(surfaceBits + y * surfaceStride);
        for (int x = 0; x < w; ++x) {

            float maskAlpha = matteValues[static_cast<size_t>(y) * w + x];

            int currentAlpha = qAlpha(line[x]);

            int newAlpha = static_cast<int>(currentAlpha * maskAlpha + 0.5f);

            const int scaledRed = static_cast<int>(qRed(line[x]) * maskAlpha + 0.5f);

            const int scaledGreen =

                static_cast<int>(qGreen(line[x]) * maskAlpha + 0.5f);

            const int scaledBlue =

                static_cast<int>(qBlue(line[x]) * maskAlpha + 0.5f);

            line[x] = qRgba(std::clamp(scaledRed, 0, 255),

                            std::clamp(scaledGreen, 0, 255),

                            std::clamp(scaledBlue, 0, 255),

                            std::clamp(newAlpha, 0, 255));

        }
      }
    };
    if (static_cast<size_t>(w) * static_cast<size_t>(h) >= 256u * 1024u) {
      applyResultRows(0, h);
    } else {
      applyResultRows(0, h);
    }

}

export void applyLayerMatteToSurface(
    ArtifactAbstractLayer* layer, ArtifactCore::ImageF32x4_RGBA& surface,
    const std::function<QImage(const ArtifactCore::Id&)>& sourceResolver)
{
  if (!layer || surface.isEmpty()) {
    return;
  }

  if (!layer->hasEnabledExternalMatteReference()) {
    return;
  }

  const auto mattes = layer->matteReferences();

  const int w = surface.width();
  const int h = surface.height();
  if (w <= 0 || h <= 0) {
    return;
  }

  QHash<ArtifactCore::Id, QImage> resolvedSources;
  for (const auto& ref : mattes) {
    if (!ref.enabled || ref.sourceLayerId.isNil() ||
        ref.sourceLayerId == layer->id()) {
      continue;
    }
    const QImage source = sourceResolver(ref.sourceLayerId);
    if (source.isNull() ||
        fitMatteSourceToTarget(source, QSize(w, h), ref.fitMode).isNull()) {
      return;
    }
    resolvedSources.insert(ref.sourceLayerId, source);
  }

  std::vector<std::vector<float>> sources;
  std::vector<LayerMatteReference> activeMatteRefs;
  for (const auto& ref : mattes) {
    if (!ref.enabled || ref.sourceLayerId.isNil() ||
        ref.sourceLayerId == layer->id()) {
      continue;
    }

    QImage source = fitMatteSourceToTarget(
        resolvedSources.value(ref.sourceLayerId), QSize(w, h), ref.fitMode);

    std::vector<float> matteValues(static_cast<size_t>(w) * h, 0.0f);
    const bool useLuma =
        ref.type == MatteType::Luma || ref.type == MatteType::InverseLuma;
    const auto buildMatteRows = [&](int yBegin, int yEnd) {
      for (int y = yBegin; y < yEnd; ++y) {
        const auto* row = source.constScanLine(y);
        for (int x = 0; x < w; ++x) {
        const auto* pixel = row + x * 4;
        matteValues[static_cast<size_t>(y) * w + x] = useLuma
            ? (0.299f * pixel[0] + 0.587f * pixel[1] +
               0.114f * pixel[2]) / 255.0f
            : pixel[3] / 255.0f;
        }
      }
    };
    buildMatteRows(0, h);
    if (ref.invert) {
      const auto invertMatte = [&](size_t begin, size_t end) {
        for (size_t i = begin; i != end; ++i) {
          matteValues[i] = 1.0f - matteValues[i];
        }
      };
      invertMatte(0, matteValues.size());
    }
    const float matteOpacity = std::clamp(ref.opacity, 0.0f, 1.0f);
    for (float& value : matteValues) {
      value *= matteOpacity;
    }
    sources.push_back(std::move(matteValues));
    activeMatteRefs.push_back(ref);
  }
  if (sources.empty()) {
    return;
  }

  const auto matteValues = evaluateLayerMatteReferences(
      sources, activeMatteRefs, w, h);
  if (matteValues.empty()) {
    return;
  }

  if (float* pixels = surface.rgba32fData()) {
    const auto applyF32Rows = [&](int yBegin, int yEnd) {
      for (int y = yBegin; y < yEnd; ++y) {
        float* pixel = pixels + static_cast<size_t>(y) * w * 4;
        for (int x = 0; x < w; ++x, pixel += 4) {
        const float factor = matteValues[static_cast<size_t>(y) * w + x];
        pixel[0] *= factor;
        pixel[1] *= factor;
        pixel[2] *= factor;
        pixel[3] *= factor;
        }
      }
    };
    applyF32Rows(0, h);
    return;
  }

  const auto applyFallbackRows = [&](int yBegin, int yEnd) {
    for (int y = yBegin; y < yEnd; ++y) {
      for (int x = 0; x < w; ++x) {
        const float factor = matteValues[static_cast<size_t>(y) * w + x];
        const ArtifactCore::FloatRGBA pixel = surface.getPixel(x, y);
        surface.setPixel(x, y, ArtifactCore::FloatRGBA(
                                   pixel.r() * factor, pixel.g() * factor,
                                   pixel.b() * factor, pixel.a() * factor));
      }
    }
  };
  applyFallbackRows(0, h);
}

} // namespace Artifact
