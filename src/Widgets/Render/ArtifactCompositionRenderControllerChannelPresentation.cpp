module;

#define QT_NO_KEYWORDS

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <QImage>
#include <QRectF>
#include <QtGlobal>

module Artifact.Widgets.CompositionRenderController:ChannelPresentation;

import Artifact.Widgets.CompositionRenderController;
import Artifact.Render.IRenderer;
import Image.Cryptomatte.Pixel;
import Image.MultiChannelImage;

namespace Artifact {

static QImage readbackViewportChannelImage(
    ArtifactIRenderer *renderer, ViewportChannelDisplayMode mode) {

  if (!renderer ||
      mode == ViewportChannelDisplayMode::Color) {
    return {};
  }

  const auto composeRgb = [](const QImage &rImage, const QImage &gImage,
                             const QImage &bImage, int fallbackBlue = 0) {
    if (rImage.isNull() || gImage.isNull() || bImage.isNull() ||
        rImage.size() != gImage.size() || rImage.size() != bImage.size()) {
      return QImage{};
    }
    QImage out(rImage.size(), QImage::Format_RGBA8888);
    const int height = out.height();
    const auto composeRows = [&](int yBegin, int yEnd) {
      for (int y = yBegin; y < yEnd; ++y) {
        const auto *rSrc = rImage.constScanLine(y);
        const auto *gSrc = gImage.constScanLine(y);
        const auto *bSrc = bImage.constScanLine(y);
        auto *dst = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x) {
          dst[x * 4 + 0] = rSrc[x];
          dst[x * 4 + 1] = gSrc[x];
          dst[x * 4 + 2] = bSrc[x];
          dst[x * 4 + 3] = 255;
        }
      }
    };
    composeRows(0, height);

    Q_UNUSED(fallbackBlue);
    return out;
  };

  const auto composeRgbAlpha = [](const QImage &rgbImage, const QImage &alphaImage) {
    if (rgbImage.isNull() || alphaImage.isNull() ||
        rgbImage.size() != alphaImage.size()) {
      return QImage{};
    }
    const QImage rgba = (rgbImage.format() == QImage::Format_RGBA8888)
                            ? rgbImage
                            : rgbImage.convertToFormat(QImage::Format_RGBA8888);
    const QImage alpha = (alphaImage.format() == QImage::Format_Grayscale8)
                             ? alphaImage
                             : alphaImage.convertToFormat(QImage::Format_Grayscale8);
    QImage out(rgbImage.width() * 2, rgbImage.height(), QImage::Format_RGBA8888);
    const int height = out.height();
    const auto composeRows = [&](int yBegin, int yEnd) {
      for (int y = yBegin; y < yEnd; ++y) {
        const auto *rgbSrc = rgba.constScanLine(y);
        const auto *alphaSrc = alpha.constScanLine(y);
        auto *dst = out.scanLine(y);
        for (int x = 0; x < rgbImage.width(); ++x) {
          const int dstX = x * 4;
          dst[dstX + 0] = rgbSrc[x * 4 + 0];
          dst[dstX + 1] = rgbSrc[x * 4 + 1];
          dst[dstX + 2] = rgbSrc[x * 4 + 2];
          dst[dstX + 3] = rgbSrc[x * 4 + 3];
        }
        for (int x = 0; x < alpha.width(); ++x) {
          const int v = alphaSrc[x];
          const int dstX = (x + rgbImage.width()) * 4;
          dst[dstX + 0] = static_cast<uchar>(v);
          dst[dstX + 1] = static_cast<uchar>(v);
          dst[dstX + 2] = static_cast<uchar>(v);
          dst[dstX + 3] = 255;
        }
      }
    };
    composeRows(0, height);
    return out;
  };

  const auto composeVelocity = [](const QImage &xImage, const QImage &yImage) {
    if (xImage.isNull() || yImage.isNull() || xImage.size() != yImage.size()) {
      return QImage{};
    }
    QImage out(xImage.size(), QImage::Format_RGBA8888);
    const int height = out.height();
    const auto composeRows = [&](int yBegin, int yEnd) {
      for (int y = yBegin; y < yEnd; ++y) {
        const auto *xSrc = xImage.constScanLine(y);
        const auto *ySrc = yImage.constScanLine(y);
        auto *dst = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x) {
          dst[x * 4 + 0] = xSrc[x];
          dst[x * 4 + 1] = ySrc[x];
          dst[x * 4 + 2] = 127;
          dst[x * 4 + 3] = 255;
        }
      }
    };
    composeRows(0, height);
    return out;
  };

  // UV composite diagnostic: R=U, G=V, B=0. Inputs are grayscale U/V images.
  const auto composeUv = [](const QImage &uImage, const QImage &vImage) {
    if (uImage.isNull() || vImage.isNull() || uImage.size() != vImage.size()) {
      return QImage{};
    }
    QImage out(uImage.size(), QImage::Format_RGBA8888);
    const int height = out.height();
    const auto composeRows = [&](int yBegin, int yEnd) {
      for (int y = yBegin; y < yEnd; ++y) {
        const auto *uSrc = uImage.constScanLine(y);
        const auto *vSrc = vImage.constScanLine(y);
        auto *dst = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x) {
          dst[x * 4 + 0] = uSrc[x];
          dst[x * 4 + 1] = vSrc[x];
          dst[x * 4 + 2] = 0;
          dst[x * 4 + 3] = 255;
        }
      }
    };
    composeRows(0, height);
    return out;
  };

  const auto pseudoColorGray = [](const QImage &grayImage, bool invert = false,
                                  bool transparentBlack = false) {
    if (grayImage.isNull()) {
      return QImage{};
    }
    const QImage gray = (grayImage.format() == QImage::Format_Grayscale8)
                            ? grayImage
                            : grayImage.convertToFormat(QImage::Format_Grayscale8);
    QImage out(gray.size(), QImage::Format_RGBA8888);
    const int height = out.height();
    const auto colorizeRows = [&](int yBegin, int yEnd) {
      for (int y = yBegin; y < yEnd; ++y) {
        const auto *src = gray.constScanLine(y);
        auto *dst = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x) {
          const int v = invert ? (255 - src[x]) : src[x];
          const int r = std::clamp(64 + v * 3 / 2, 0, 255);
          const int g = std::clamp((255 - std::abs(v - 128) * 2), 0, 255);
          const int b = std::clamp(255 - v / 2, 0, 255);
          dst[x * 4 + 0] = static_cast<uchar>(r);
          dst[x * 4 + 1] = static_cast<uchar>(g);
          dst[x * 4 + 2] = static_cast<uchar>(b);
          dst[x * 4 + 3] = transparentBlack && src[x] == 0 ? 0 : 255;
        }
      }
    };
    colorizeRows(0, height);
    return out;
  };

  // Cryptomatte IDs are uint32 payloads stored in float bits.  They must not
  // pass through the ordinary image readback path: that path clamps floats to
  // [0, 1] and permanently loses the identifier.  This cold display path
  // preserves the raw payload, then derives a stable, high-contrast swatch
  // solely for viewport inspection.  The raw float AOV remains untouched for
  // export and click-to-select.
  const auto pseudoColorCryptomatte = [renderer](ArtifactCore::ChannelType channel) {
    QImage out;
    const auto aov = renderer->readbackToMultiChannelImage();
    const auto samples = aov.getChannel(channel);
    if (!samples || aov.width() <= 0 || aov.height() <= 0 ||
        samples->size() != static_cast<std::size_t>(aov.width()) *
                               static_cast<std::size_t>(aov.height())) {
      return out;
    }

    out = QImage(aov.width(), aov.height(), QImage::Format_RGBA8888);
    for (int y = 0; y < aov.height(); ++y) {
      auto* dst = out.scanLine(y);
      for (int x = 0; x < aov.width(); ++x) {
        const auto sampleIndex = static_cast<std::size_t>(y) *
                                     static_cast<std::size_t>(aov.width()) +
                                 static_cast<std::size_t>(x);
        const std::uint32_t id =
            ArtifactCore::Image::Cryptomatte::CryptoSample::floatToId(
                samples->data()[sampleIndex]);
        if (id == 0u) {
          dst[x * 4 + 0] = 0;
          dst[x * 4 + 1] = 0;
          dst[x * 4 + 2] = 0;
          dst[x * 4 + 3] = 255;
          continue;
        }

        std::uint32_t mixed = id;
        mixed ^= mixed >> 16;
        mixed *= 0x7feb352du;
        mixed ^= mixed >> 15;
        mixed *= 0x846ca68bu;
        mixed ^= mixed >> 16;
        dst[x * 4 + 0] = static_cast<uchar>(72u + (mixed & 0x7fu));
        dst[x * 4 + 1] = static_cast<uchar>(72u + ((mixed >> 8) & 0x7fu));
        dst[x * 4 + 2] = static_cast<uchar>(72u + ((mixed >> 16) & 0x7fu));
        dst[x * 4 + 3] = 255;
      }
    }
    return out;
  };

  auto readChannel = [renderer](ArtifactIRenderer::ChannelType channel) {
    return renderer->readbackChannelToImage(channel);
  };

  switch (mode) {
  case ViewportChannelDisplayMode::Alpha:
    return readChannel(ArtifactIRenderer::ChannelType::Alpha);
  case ViewportChannelDisplayMode::ColorAlpha:
    return composeRgbAlpha(
        renderer->readbackToImage(),
        readChannel(ArtifactIRenderer::ChannelType::Alpha));
  case ViewportChannelDisplayMode::Red:
  case ViewportChannelDisplayMode::Green:
  case ViewportChannelDisplayMode::Blue:
    // These modes are presented from the composite SRV in the GPU path.
    // If that surface is unavailable, retain the beauty frame rather than
    // extracting a channel from the viewport background and UI overlays.
    return {};
  case ViewportChannelDisplayMode::Depth:
    return pseudoColorGray(
        readChannel(ArtifactIRenderer::ChannelType::Depth), true, true);
  case ViewportChannelDisplayMode::Emission:
    return readChannel(ArtifactIRenderer::ChannelType::Emission);
  case ViewportChannelDisplayMode::ObjectId:
    return pseudoColorCryptomatte(ArtifactCore::ChannelType::ObjectId);
  case ViewportChannelDisplayMode::MaterialId:
    return pseudoColorCryptomatte(ArtifactCore::ChannelType::MaterialId);
  case ViewportChannelDisplayMode::Albedo:
    return composeRgb(
        readChannel(ArtifactIRenderer::ChannelType::AlbedoR),
        readChannel(ArtifactIRenderer::ChannelType::AlbedoG),
        readChannel(ArtifactIRenderer::ChannelType::AlbedoB));
  case ViewportChannelDisplayMode::AlbedoR:
    return readChannel(ArtifactIRenderer::ChannelType::AlbedoR);
  case ViewportChannelDisplayMode::AlbedoG:
    return readChannel(ArtifactIRenderer::ChannelType::AlbedoG);
  case ViewportChannelDisplayMode::AlbedoB:
    return readChannel(ArtifactIRenderer::ChannelType::AlbedoB);
  case ViewportChannelDisplayMode::Normal:
    return composeRgb(
        readChannel(ArtifactIRenderer::ChannelType::NormalX),
        readChannel(ArtifactIRenderer::ChannelType::NormalY),
        readChannel(ArtifactIRenderer::ChannelType::NormalZ));
  case ViewportChannelDisplayMode::NormalX:
    return readChannel(ArtifactIRenderer::ChannelType::NormalX);
  case ViewportChannelDisplayMode::NormalY:
    return readChannel(ArtifactIRenderer::ChannelType::NormalY);
  case ViewportChannelDisplayMode::NormalZ:
    return readChannel(ArtifactIRenderer::ChannelType::NormalZ);
  case ViewportChannelDisplayMode::Velocity:
    return composeVelocity(
        readChannel(ArtifactIRenderer::ChannelType::VelocityX),
        readChannel(ArtifactIRenderer::ChannelType::VelocityY));
  case ViewportChannelDisplayMode::VelocityX:
    return readChannel(ArtifactIRenderer::ChannelType::VelocityX);
  case ViewportChannelDisplayMode::VelocityY:
    return readChannel(ArtifactIRenderer::ChannelType::VelocityY);
  case ViewportChannelDisplayMode::Position:
    return composeRgb(
        readChannel(ArtifactIRenderer::ChannelType::PositionX),
        readChannel(ArtifactIRenderer::ChannelType::PositionY),
        readChannel(ArtifactIRenderer::ChannelType::PositionZ));
  case ViewportChannelDisplayMode::PositionX:
    return readChannel(ArtifactIRenderer::ChannelType::PositionX);
  case ViewportChannelDisplayMode::PositionY:
    return readChannel(ArtifactIRenderer::ChannelType::PositionY);
  case ViewportChannelDisplayMode::PositionZ:
    return readChannel(ArtifactIRenderer::ChannelType::PositionZ);
  case ViewportChannelDisplayMode::UV:
    return composeUv(
        readChannel(ArtifactIRenderer::ChannelType::U),
        readChannel(ArtifactIRenderer::ChannelType::V));
  case ViewportChannelDisplayMode::U:
    return readChannel(ArtifactIRenderer::ChannelType::U);
  case ViewportChannelDisplayMode::V:
    return readChannel(ArtifactIRenderer::ChannelType::V);
  case ViewportChannelDisplayMode::Color:
    return {};
  }

  return {};

}

void drawViewportChannelOverlayImage(
    ArtifactIRenderer *renderer, ViewportChannelDisplayMode mode,
    Diligent::ITextureView *channelSurface,
    CompositionViewportPresentationLayout layout, float hostWidth,
    float hostHeight, float canvasWidth, float canvasHeight) {

  if (!renderer ||
      mode == ViewportChannelDisplayMode::Color) {

    return;

  }

   if (channelSurface) {
    // RGB/Alpha component modes are already presented by
    // finalizeGpuRenderToViewport(). Re-drawing the same display-ready
    // surface here applies the viewport presentation path twice, making the
    // composition appear inset or recursively nested.
    const bool componentAlreadyPresented =
        mode == ViewportChannelDisplayMode::Red ||
        mode == ViewportChannelDisplayMode::Green ||
        mode == ViewportChannelDisplayMode::Blue ||
        mode == ViewportChannelDisplayMode::Alpha;
    if (componentAlreadyPresented) {
      return;
    }
    if (layout == CompositionViewportPresentationLayout::Quad &&
        hostWidth > 0.0f && hostHeight > 0.0f) {
      // Mirror finalizeGpuRenderToViewport's Quad tiling so the isolated
      // pass stays quad-tiled instead of covering the 4 beauty panes
      // with a single fullscreen sprite.
      const float savedZoom = renderer->getZoom();
      float savedPanX = 0.0f, savedPanY = 0.0f;
      renderer->getPan(savedPanX, savedPanY);
      renderer->setViewportRect(hostWidth, hostHeight);
      const float leftW = std::floor(hostWidth * 0.5f);
      const float topH = std::floor(hostHeight * 0.5f);
      const std::array<QRectF, 4> panes{
          QRectF(0.0, 0.0, leftW, topH),
          QRectF(leftW, 0.0, hostWidth - leftW, topH),
          QRectF(0.0, topH, leftW, hostHeight - topH),
          QRectF(leftW, topH, hostWidth - leftW, hostHeight - topH)};
      for (const QRectF& pane : panes) {
        if (pane.width() <= 0.0 || pane.height() <= 0.0) continue;
        renderer->setViewportRect(
            static_cast<float>(pane.x()), static_cast<float>(pane.y()),
            static_cast<float>(pane.width()), static_cast<float>(pane.height()),
            hostWidth, hostHeight);
        renderer->setCanvasSize(static_cast<float>(pane.width()),
                                 static_cast<float>(pane.height()));
        renderer->setZoom(1.0f);
        renderer->setPan(0.0f, 0.0f);
        renderer->drawSprite(0.0f, 0.0f, static_cast<float>(pane.width()),
                              static_cast<float>(pane.height()),
                              channelSurface, 1.0f);
        renderer->flush();
      }
      renderer->setViewportRect(hostWidth, hostHeight);
      renderer->setCanvasSize(canvasWidth, canvasHeight);
      renderer->setZoom(savedZoom);
      renderer->setPan(savedPanX, savedPanY);
      return;
    }
    // The channel surface is derived from the already resolved presentation
    // texture. Present it in viewport space exactly once, just like the beauty
    // surface in finalizeGpuRenderToViewport(). Drawing it in composition space
    // applies the composition pan/zoom a second time and makes the isolated
    // channel look inset or recursive.
    const float savedZoom = renderer->getZoom();
    float savedPanX = 0.0f;
    float savedPanY = 0.0f;
    renderer->getPan(savedPanX, savedPanY);
    const float viewportWidth = hostWidth > 0.0f ? hostWidth : canvasWidth;
    const float viewportHeight = hostHeight > 0.0f ? hostHeight : canvasHeight;
    renderer->setViewportRect(viewportWidth, viewportHeight);
    renderer->setCanvasSize(viewportWidth, viewportHeight);
    renderer->setZoom(1.0f);
    renderer->setPan(0.0f, 0.0f);
    renderer->drawSprite(0.0f, 0.0f, viewportWidth, viewportHeight,
                          channelSurface, 1.0f);
    renderer->flush();
    renderer->setCanvasSize(canvasWidth, canvasHeight);
    renderer->setZoom(savedZoom);
    renderer->setPan(savedPanX, savedPanY);
    return;
  }

  const QImage channelImage = readbackViewportChannelImage(renderer, mode);
  if (channelImage.isNull()) {

    return;

  }

  if (channelImage.width() <= 0 || channelImage.height() <= 0 ||
      canvasWidth <= 0.0f || canvasHeight <= 0.0f) {

    return;

  }

  if (layout == CompositionViewportPresentationLayout::Quad &&
      hostWidth > 0.0f && hostHeight > 0.0f) {
    // Same Quad tiling as the SRV path above: each pane shows the full
    // channel image, mirroring the beauty tiling in finalize.
    const float savedZoom = renderer->getZoom();
    float savedPanX = 0.0f, savedPanY = 0.0f;
    renderer->getPan(savedPanX, savedPanY);
    renderer->setViewportRect(hostWidth, hostHeight);
    const float leftW = std::floor(hostWidth * 0.5f);
    const float topH = std::floor(hostHeight * 0.5f);
    const std::array<QRectF, 4> panes{
        QRectF(0.0, 0.0, leftW, topH),
        QRectF(leftW, 0.0, hostWidth - leftW, topH),
        QRectF(0.0, topH, leftW, hostHeight - topH),
        QRectF(leftW, topH, hostWidth - leftW, hostHeight - topH)};
    for (const QRectF& pane : panes) {
      if (pane.width() <= 0.0 || pane.height() <= 0.0) continue;
      renderer->setViewportRect(
          static_cast<float>(pane.x()), static_cast<float>(pane.y()),
          static_cast<float>(pane.width()), static_cast<float>(pane.height()),
          hostWidth, hostHeight);
      renderer->setCanvasSize(static_cast<float>(pane.width()),
                               static_cast<float>(pane.height()));
      renderer->setZoom(1.0f);
      renderer->setPan(0.0f, 0.0f);
      renderer->drawSprite(0.0f, 0.0f, static_cast<float>(pane.width()),
                            static_cast<float>(pane.height()), channelImage,
                            1.0f);
      renderer->flush();
    }
    renderer->setViewportRect(hostWidth, hostHeight);
    renderer->setCanvasSize(canvasWidth, canvasHeight);
    renderer->setZoom(savedZoom);
    renderer->setPan(savedPanX, savedPanY);
    return;
  }

  // R/G/B/Alpha read back the presented swap-chain image, which is already
  // viewport-sized. Drawing that image in composition space applies the
  // current zoom/pan again and produces a nested copy of the viewport.
  const bool viewportSizedChannel =
      mode == ViewportChannelDisplayMode::Red ||
      mode == ViewportChannelDisplayMode::Green ||
      mode == ViewportChannelDisplayMode::Blue ||
      mode == ViewportChannelDisplayMode::Alpha;
  if (viewportSizedChannel) {
    const float savedZoom = renderer->getZoom();
    float savedPanX = 0.0f;
    float savedPanY = 0.0f;
    renderer->getPan(savedPanX, savedPanY);
    const float viewportWidth = hostWidth > 0.0f ? hostWidth : canvasWidth;
    const float viewportHeight = hostHeight > 0.0f ? hostHeight : canvasHeight;
    renderer->setViewportRect(viewportWidth, viewportHeight);
    renderer->setCanvasSize(viewportWidth, viewportHeight);
    renderer->setZoom(1.0f);
    renderer->setPan(0.0f, 0.0f);
    renderer->drawSprite(0.0f, 0.0f, viewportWidth, viewportHeight,
                          channelImage, 1.0f);
    renderer->flush();
    renderer->setCanvasSize(canvasWidth, canvasHeight);
    renderer->setZoom(savedZoom);
    renderer->setPan(savedPanX, savedPanY);
    return;
  }

  // Auxiliary AOVs that do not yet have a display-ready GPU surface (such as
  // Depth) are composition-sized images. Keep those in composition space.
  renderer->drawSprite(0.0f, 0.0f, canvasWidth, canvasHeight,
                        channelImage, 1.0f);
  renderer->flush();

}

} // namespace Artifact
