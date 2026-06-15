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
 if (!renderer || spacing <= 0 || w <= 0 || h <= 0) return;

 const float halfTick = thickness * 2.0f;

 if (style == GridStyle::Lines) {
  // Vertical lines
  for (float gx = x; gx <= x + w; gx += spacing) {
   renderer->drawSolidLine({gx, y}, {gx, y + h}, color, thickness);
  }
  // Horizontal lines
  for (float gy = y; gy <= y + h; gy += spacing) {
   renderer->drawSolidLine({x, gy}, {x + w, gy}, color, thickness);
  }
 } else if (style == GridStyle::Dots) {
  for (float gx = x; gx <= x + w; gx += spacing) {
   for (float gy = y; gy <= y + h; gy += spacing) {
    renderer->drawSolidRect(gx - halfTick, gy - halfTick,
                             halfTick * 2, halfTick * 2, color, 1.0f);
   }
  }
 } else if (style == GridStyle::Crosses) {
  for (float gx = x; gx <= x + w; gx += spacing) {
   for (float gy = y; gy <= y + h; gy += spacing) {
    renderer->drawSolidLine({gx - halfTick, gy}, {gx + halfTick, gy}, color, thickness);
    renderer->drawSolidLine({gx, gy - halfTick}, {gx, gy + halfTick}, color, thickness);
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
