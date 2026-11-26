module;

#define QT_NO_KEYWORDS

#include <algorithm>

#include <QColor>
#include <QImage>
#include <QLinearGradient>
#include <QMatrix4x4>
#include <QPainter>
#include <QSize>

module Artifact.Widgets.CompositionRenderController;

import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Color.Float;
import Composition.Settings;

namespace Artifact {

QImage makeMayaGradientSprite(const FloatColor &baseColor) {

  constexpr int kWidth = 4;

  constexpr int kHeight = 4096; // 1024→4096 to reduce banding

  QImage image(kWidth, kHeight, QImage::Format_RGBA64); // 16-bit per channel

  image.fill(Qt::transparent);



  QPainter painter(&image);

  QLinearGradient gradient(0.0, 0.0, 0.0, static_cast<qreal>(kHeight));

  // Use more color stops for smoother transitions

  gradient.setColorAt(0.00, QColor::fromRgbF(0.26f, 0.32f, 0.38f, 1.0f));

  gradient.setColorAt(0.07, QColor::fromRgbF(0.24f, 0.30f, 0.36f, 1.0f));

  gradient.setColorAt(0.14, QColor::fromRgbF(0.21f, 0.27f, 0.33f, 1.0f));

  gradient.setColorAt(0.22, QColor::fromRgbF(0.19f, 0.25f, 0.30f, 1.0f));

  gradient.setColorAt(0.30, QColor::fromRgbF(0.17f, 0.22f, 0.27f, 1.0f));

  gradient.setColorAt(0.41, QColor::fromRgbF(0.15f, 0.20f, 0.25f, 1.0f));

  gradient.setColorAt(0.52, QColor::fromRgbF(0.13f, 0.17f, 0.22f, 1.0f));

  gradient.setColorAt(0.64, QColor::fromRgbF(0.12f, 0.15f, 0.20f, 1.0f));

  gradient.setColorAt(0.76, QColor::fromRgbF(0.10f, 0.13f, 0.17f, 1.0f));

  gradient.setColorAt(0.88, QColor::fromRgbF(0.09f, 0.12f, 0.15f, 1.0f));

  gradient.setColorAt(1.00, QColor::fromRgbF(0.08f, 0.10f, 0.13f, 1.0f));

  painter.fillRect(image.rect(), gradient);



  QLinearGradient glow(0.0, 0.0, static_cast<qreal>(kWidth), static_cast<qreal>(kHeight));

  QColor tint = QColor::fromRgbF(baseColor.r(), baseColor.g(), baseColor.b(), 1.0f);

  tint.setAlpha(72);

  glow.setColorAt(0.0, tint.lighter(112));

  QColor tintDark = tint.darker(140);

  tintDark.setAlpha(28);

  glow.setColorAt(1.0, tintDark);

  painter.fillRect(image.rect(), glow);

  return image;

}





static QImage makeSkyboxGradientSprite()

{  constexpr int kWidth = 4;

  constexpr int kHeight = 4096;

  QImage image(kWidth, kHeight, QImage::Format_RGBA64);

  image.fill(Qt::transparent);



  QPainter painter(&image);

  QLinearGradient gradient(0.0, 0.0, 0.0, static_cast<qreal>(kHeight));

  gradient.setColorAt(0.00, QColor::fromRgbF(0.12f, 0.20f, 0.45f, 1.0f));

  gradient.setColorAt(0.20, QColor::fromRgbF(0.22f, 0.35f, 0.60f, 1.0f));

  gradient.setColorAt(0.40, QColor::fromRgbF(0.40f, 0.55f, 0.75f, 1.0f));

  gradient.setColorAt(0.60, QColor::fromRgbF(0.60f, 0.72f, 0.85f, 1.0f));

  gradient.setColorAt(0.80, QColor::fromRgbF(0.80f, 0.85f, 0.92f, 1.0f));

  gradient.setColorAt(1.00, QColor::fromRgbF(0.95f, 0.95f, 0.97f, 1.0f));

  painter.fillRect(image.rect(), gradient);

  return image;

}

// Draws checkerboard in Viewport Space so transparent regions of the

// composition reveal the pattern against the viewport background.

// This should be called before blitting the composition result to the

// visible framebuffer.

void drawViewportCheckerboardBackground(ArtifactIRenderer *renderer, float vw,

                                        float vh, float tileSize) {

  if (!renderer || vw <= 0.0f || vh <= 0.0f) {

    return;

  }



  // Save current state

  const float savedZoom = renderer->getZoom();

  float savedPanX = 0.0f;

  float savedPanY = 0.0f;

  renderer->getPan(savedPanX, savedPanY);



  renderer->setCanvasSize(vw, vh);

  renderer->setZoom(1.0f);

  renderer->setPan(0.0f, 0.0f);

  renderer->drawCheckerboard(0.0f, 0.0f, vw, vh, std::max(2.0f, tileSize),

                             {0.18f, 0.18f, 0.18f, 1.0f},

                             {0.28f, 0.28f, 0.28f, 1.0f});



  // Restore state

  renderer->setZoom(savedZoom);

  renderer->setPan(savedPanX, savedPanY);

}



// Draws checkerboard over the composition canvas in Composition Space.

// Must be called BEFORE layer drawing so transparent regions reveal the

// pattern.

void drawCompositionCheckerboard(ArtifactIRenderer *renderer,

                                 const ArtifactCompositionPtr &comp,

                                 float tileSize) {

  if (!renderer || !comp) {

    return;

  }



  const QSize compSize = comp->settings().compositionSize();

  const float cw =

      static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);

  const float ch =

      static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);

  if (cw <= 0.0f || ch <= 0.0f) {

    return;

  }



  renderer->drawCheckerboard(0.0f, 0.0f, cw, ch, std::max(2.0f, tileSize),

                             {0.18f, 0.18f, 0.18f, 1.0f},

                             {0.28f, 0.28f, 0.28f, 1.0f});

}



// Draws the composition background in Composition Space (0,0)-(cw,ch).

// renderer must already be configured with setCanvasSize(cw,ch), zoom, and pan.

// The vertex shader handles the Composition→View→NDC transform automatically.

// MayaGradient is handled separately in viewport space so it can cover the

// full visible viewport instead of being clipped to the composition rect.

void drawCompositionBackgroundDirect(ArtifactIRenderer *renderer, float cw,

                                     float ch, const FloatColor &bgColor,

                                     CompositionBackgroundMode mode,

                                     float checkerboardTileSize,

                                     const QImage &mayaGradientSprite,
                                     const QMatrix4x4 *viewportView,
                                     const QMatrix4x4 *viewportProjection) {

  if (!renderer || cw <= 0.0f || ch <= 0.0f) {

    return;

  }

  const auto drawCanvasFill =
      [&](const FloatColor &color) {
        if (viewportView && viewportProjection) {
          QMatrix4x4 identity;
          renderer->setViewMatrix(*viewportView);
          renderer->setProjectionMatrix(*viewportProjection);
          renderer->setUseExternalMatrices(true);
          renderer->drawSolidRectTransformed(0.0f, 0.0f, cw, ch, identity,
                                             color, 1.0f);
          renderer->setUseExternalMatrices(false);
          return;
        }
        renderer->drawRectLocal(0.f, 0.f, cw, ch, color, 1.0f);
      };

  if (mode == CompositionBackgroundMode::Solid) {

    if (bgColor.a() > 0.0f) {

      drawCanvasFill(bgColor);

    }

    return;

  }

  if (mode == CompositionBackgroundMode::Checkerboard) {

    // Checkerboard is the viewport background; the composition area itself

    // should stay filled with the composition bg color.

    if (bgColor.a() > 0.0f) {

      drawCanvasFill(bgColor);

    }

    return;

  }

  // MayaGradient only affects the viewport background; the composition area

  // itself is always rendered with the solid bgColor.

  if (mode == CompositionBackgroundMode::MayaGradient) {

    if (bgColor.a() > 0.0f) {

      drawCanvasFill(bgColor);

    }

    return;

  }

  // Skybox renders a sky-like gradient covering the composition area

  if (mode == CompositionBackgroundMode::Skybox)

  {

    QImage skyGradient = makeSkyboxGradientSprite();

    if (!skyGradient.isNull())

    {

      renderer->drawSpriteTransformed(0.f, 0.f, cw, ch, QMatrix4x4{}, skyGradient, 1.0f);

    }

    return;

  }

  if (bgColor.a() > 0.0f) {

    renderer->drawRectLocal(0.f, 0.f, cw, ch, bgColor, 1.0f);

  }

}



void drawViewportMayaGradientBackground(ArtifactIRenderer *renderer, float vw,

                                        float vh, const FloatColor &bgColor,

                                        const QImage &mayaGradientSprite) {

  if (!renderer || vw <= 0.0f || vh <= 0.0f) {

    return;

  }

  if (!mayaGradientSprite.isNull()) {

    renderer->drawSpriteTransformed(0.f, 0.f, vw, vh, QMatrix4x4{},

                                    mayaGradientSprite, 1.0f);

    return;

  }

  renderer->drawRectLocal(0.f, 0.f, vw, vh, bgColor, 1.0f);

}

} // namespace Artifact
