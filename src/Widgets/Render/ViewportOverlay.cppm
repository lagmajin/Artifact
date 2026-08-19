module;
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <QVector>

module Artifact.Widgets.ViewportOverlay;
import Artifact.Render.IRenderer;

namespace Artifact::ViewportOverlay {

void drawSafeAreaAndOrigin(ArtifactIRenderer *renderer, float canvasWidth,
                           float canvasHeight, bool showSafeArea,
                           bool showOrigin) {
  if (!renderer || canvasWidth <= 0.0f || canvasHeight <= 0.0f) return;

  if (showOrigin) {
    const auto origin = renderer->canvasToViewport({0.0f, 0.0f});
    const auto xAxis = renderer->canvasToViewport(
        {std::min(canvasWidth, std::max(48.0f, canvasWidth * 0.08f)), 0.0f});
    const auto yAxis = renderer->canvasToViewport(
        {0.0f, std::min(canvasHeight, std::max(48.0f, canvasHeight * 0.08f))});
    renderer->drawSolidLine({origin.x, origin.y}, {xAxis.x, xAxis.y},
                            {0.95f, 0.25f, 0.22f, 0.9f}, 1.5f);
    renderer->drawSolidLine({origin.x, origin.y}, {yAxis.x, yAxis.y},
                            {0.35f, 0.9f, 0.4f, 0.9f}, 1.5f);
    renderer->drawCrosshair(origin.x, origin.y, 8.0f,
                            {0.95f, 0.85f, 0.25f, 0.95f});
  }

  if (!showSafeArea) return;

  const std::array<Detail::float2, 4> canvasCorners{
      Detail::float2{0.0f, 0.0f}, Detail::float2{canvasWidth, 0.0f},
      Detail::float2{canvasWidth, canvasHeight},
      Detail::float2{0.0f, canvasHeight}};
  std::array<Detail::float2, 4> screenCorners{};
  for (size_t i = 0; i < canvasCorners.size(); ++i)
    screenCorners[i] = renderer->canvasToViewport(canvasCorners[i]);

  const Detail::float2 screenCenter = renderer->canvasToViewport(
      {canvasWidth * 0.5f, canvasHeight * 0.5f});
  const float screenW = std::hypot(screenCorners[1].x - screenCorners[0].x,
                                   screenCorners[1].y - screenCorners[0].y);
  const float screenH = std::hypot(screenCorners[3].x - screenCorners[0].x,
                                   screenCorners[3].y - screenCorners[0].y);
  if (screenW <= 0.0f || screenH <= 0.0f) return;

  const FloatColor outlineColor{0.0f, 0.0f, 0.0f, 0.72f};
  const FloatColor innerColor{0.95f, 0.97f, 1.0f, 0.42f};
  const auto snapScreen = [](float value) { return std::round(value) + 0.5f; };
  const auto drawSafeRect = [&](float ratio) {
    std::vector<Detail::float2> points;
    points.reserve(screenCorners.size() + 1);
    for (const auto &corner : screenCorners) {
      points.push_back({snapScreen(screenCenter.x +
                                   (corner.x - screenCenter.x) * ratio),
                        snapScreen(screenCenter.y +
                                   (corner.y - screenCenter.y) * ratio)});
    }
    points.push_back(points.front());
    if (screenW * ratio <= 2.0f || screenH * ratio <= 2.0f) return;
    renderer->drawPolyline(points, outlineColor, 1.0f);
    renderer->drawPolyline(points, innerColor, 1.0f);
  };

  drawSafeRect(0.9f);
  drawSafeRect(0.8f);
}

void drawGuides(ArtifactIRenderer *renderer, const QVector<float> &verticals,
                const QVector<float> &horizontals, bool showGuides,
                float canvasWidth, float canvasHeight) {
  if (!renderer || !showGuides || canvasWidth <= 0.0f || canvasHeight <= 0.0f)
    return;
  const FloatColor guideColor{0.2f, 0.8f, 1.0f, 0.7f};
  for (const float x : verticals)
    if (x >= 0.0f && x <= canvasWidth)
      renderer->drawSolidLine({x, 0.0f}, {x, canvasHeight}, guideColor, 1.0f);
  for (const float y : horizontals)
    if (y >= 0.0f && y <= canvasHeight)
      renderer->drawSolidLine({0.0f, y}, {canvasWidth, y}, guideColor, 1.0f);
  if (verticals.empty() && horizontals.empty()) {
    renderer->drawSolidLine({canvasWidth * 0.5f, 0.0f},
                            {canvasWidth * 0.5f, canvasHeight}, guideColor,
                            1.0f);
    renderer->drawSolidLine({0.0f, canvasHeight * 0.5f},
                            {canvasWidth, canvasHeight * 0.5f}, guideColor,
                            1.0f);
  }
}

} // namespace Artifact::ViewportOverlay
