module;
#include <cmath>
#include <algorithm>

module GridRenderer;

import Artifact.Render.IRenderer;
import Color.Float;

namespace Artifact {

GridRenderer::GridRenderer() {}
GridRenderer::~GridRenderer() {}

void GridRenderer::draw(ArtifactIRenderer* renderer,
                         float x, float y, float w, float h,
                         float spacing, float thickness,
                         const FloatColor& color,
                         GridStyle style)
{
 if (!renderer || !std::isfinite(x) || !std::isfinite(y) ||
     !std::isfinite(w) || !std::isfinite(h) || !std::isfinite(spacing) ||
     !std::isfinite(thickness) || spacing <= 0 || w <= 0 || h <= 0) return;

 const float safeThickness = std::max(0.0f, thickness);
 const float halfTick = safeThickness * 2.0f;
 constexpr int kMaxGridSamples = 8192;
 const int xSamples = std::min(kMaxGridSamples,
                               std::max(1, static_cast<int>(std::ceil(w / spacing)) + 1));
 const int ySamples = std::min(kMaxGridSamples,
                               std::max(1, static_cast<int>(std::ceil(h / spacing)) + 1));

 if (style == GridStyle::Lines) {
  // Vertical lines
  for (int i = 0; i < xSamples; ++i) {
   const float gx = x + static_cast<float>(i) * spacing;
   if (gx > x + w) break;
   renderer->drawSolidLine({gx, y}, {gx, y + h}, color, safeThickness);
  }
  // Horizontal lines
  for (int i = 0; i < ySamples; ++i) {
   const float gy = y + static_cast<float>(i) * spacing;
   if (gy > y + h) break;
   renderer->drawSolidLine({x, gy}, {x + w, gy}, color, safeThickness);
  }
 } else if (style == GridStyle::Dots) {
  for (int ix = 0; ix < xSamples; ++ix) {
   const float gx = x + static_cast<float>(ix) * spacing;
   if (gx > x + w) break;
   for (int iy = 0; iy < ySamples; ++iy) {
    const float gy = y + static_cast<float>(iy) * spacing;
    if (gy > y + h) break;
    renderer->drawSolidRect(gx - halfTick, gy - halfTick,
                             halfTick * 2, halfTick * 2, color, 1.0f);
   }
  }
 } else if (style == GridStyle::Crosses) {
  for (int ix = 0; ix < xSamples; ++ix) {
   const float gx = x + static_cast<float>(ix) * spacing;
   if (gx > x + w) break;
   for (int iy = 0; iy < ySamples; ++iy) {
    const float gy = y + static_cast<float>(iy) * spacing;
    if (gy > y + h) break;
    renderer->drawSolidLine({gx - halfTick, gy}, {gx + halfTick, gy}, color, safeThickness);
    renderer->drawSolidLine({gx, gy - halfTick}, {gx, gy + halfTick}, color, safeThickness);
   }
  }
 }
}

void GridRenderer::drawSubdivided(ArtifactIRenderer* renderer,
                                    float x, float y, float w, float h,
                                    float majorSpacing, float minorSpacing,
                                    const FloatColor& majorColor,
                                    const FloatColor& minorColor)
{
 if (!renderer || majorSpacing <= 0 || minorSpacing <= 0) return;

 // Minor grid
 draw(renderer, x, y, w, h, minorSpacing, 0.5f, minorColor, GridStyle::Lines);
 // Major grid
 draw(renderer, x, y, w, h, majorSpacing, 1.0f, majorColor, GridStyle::Lines);
}

void GridRenderer::setSpacing(float spacing)
{
 spacing_ = std::max(1.0f, spacing);
}

float GridRenderer::spacing() const { return spacing_; }

void GridRenderer::setMinorRatio(int ratio)
{
 minorRatio_ = std::max(1, ratio);
}

int GridRenderer::minorRatio() const { return minorRatio_; }

}
