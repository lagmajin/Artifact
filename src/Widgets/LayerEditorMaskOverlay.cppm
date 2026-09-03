module;

#include <QPointF>
#include <QTransform>

#include <algorithm>

module Artifact.Widgets.LayerEditor.MaskOverlay;

import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.Geometry;
import Color.Float;

namespace Artifact {
namespace {

FloatColor brighten(const FloatColor& color, float factor)
{
 return {std::clamp(color.r() * factor, 0.0f, 1.0f),
         std::clamp(color.g() * factor, 0.0f, 1.0f),
         std::clamp(color.b() * factor, 0.0f, 1.0f), color.a()};
}

FloatColor alpha(const FloatColor& color, float value)
{
 return {color.r(), color.g(), color.b(), std::clamp(value, 0.0f, 1.0f)};
}

FloatColor mix(const FloatColor& a, const FloatColor& b, float amount)
{
 const float t = std::clamp(amount, 0.0f, 1.0f);
 return {a.r() + (b.r() - a.r()) * t, a.g() + (b.g() - a.g()) * t,
         a.b() + (b.b() - a.b()) * t, a.a() + (b.a() - a.a()) * t};
}

FloatColor modeColor(MaskMode mode)
{
 switch (mode) {
 case MaskMode::Subtract: return {0.96f, 0.50f, 0.30f, 1.0f};
 case MaskMode::Intersect: return {0.30f, 0.82f, 0.52f, 1.0f};
 case MaskMode::Difference: return {0.82f, 0.50f, 0.90f, 1.0f};
 case MaskMode::Add:
 default: return {0.18f, 0.72f, 0.90f, 1.0f};
 }
}

void drawSolidHandle(ArtifactIRenderer* renderer, const Detail::float2& center,
                     float size, const FloatColor& fill, bool active)
{
 const float half = size * 0.5f;
 const float glowPad = active ? 2.6f : 1.4f;
 const float inset = std::clamp(size * 0.18f, 1.0f, 2.6f);
 renderer->drawSolidRect(center.x - half - glowPad, center.y - half - glowPad,
                         size + glowPad * 2.0f, size + glowPad * 2.0f,
                         alpha(fill, active ? 0.26f : 0.12f), 1.0f);
 renderer->drawSolidRect(center.x - half + 1.2f, center.y - half + 1.2f,
                         size, size, {0.0f, 0.0f, 0.0f, active ? 0.42f : 0.30f}, 1.0f);
 renderer->drawSolidRect(center.x - half, center.y - half, size, size,
                         brighten(fill, active ? 1.14f : 1.04f), 1.0f);
 renderer->drawSolidRect(center.x - half + inset, center.y - half + inset,
                         std::max(1.0f, size - inset * 2.0f),
                         std::max(1.0f, size - inset * 2.0f),
                         brighten(fill, active ? 1.28f : 1.14f), 1.0f);
 renderer->drawRectOutline(center.x - half, center.y - half, size, size,
                           active ? FloatColor{1.0f, 1.0f, 1.0f, 1.0f}
                                  : FloatColor{0.10f, 0.12f, 0.14f, 0.98f});
}

bool matches(int mask, int path, int vertex, int handle,
             int expectedMask, int expectedPath, int expectedVertex,
             MaskHandleType expectedHandle)
{
 return mask == expectedMask && path == expectedPath && vertex == expectedVertex &&
        handle == static_cast<int>(expectedHandle);
}

}

void drawLayerEditorMaskOverlay(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    const LayerEditorMaskOverlayState& state)
{
 if (!renderer || !layer || layer->maskCount() <= 0) return;
 const QTransform transform = layer->getGlobalTransform();
 const FloatColor pointShadow{0.0f, 0.0f, 0.0f, 0.42f};
 const FloatColor hover{0.98f, 0.72f, 0.24f, 1.0f};
 const FloatColor drag{0.98f, 0.42f, 0.18f, 1.0f};

 for (int maskIndex = 0; maskIndex < layer->maskCount(); ++maskIndex) {
  const LayerMask mask = layer->mask(maskIndex);
  if (!mask.isEnabled()) continue;
  for (int pathIndex = 0; pathIndex < mask.maskPathCount(); ++pathIndex) {
   const MaskPath path = mask.maskPath(pathIndex);
   const int count = path.vertexCount();
   if (count == 0) continue;
   const FloatColor base = modeColor(path.mode());
   const FloatColor lineShadow{0.0f, 0.0f, 0.0f, 0.36f};
   const FloatColor line = brighten(base, 1.04f);
   const FloatColor lineHighlight = brighten(base, 1.32f);
   const FloatColor handlePoint = alpha(mix(base, {0.86f, 0.92f, 0.98f, 1.0f}, 0.52f), 0.98f);
   const FloatColor handleHover = mix(base, hover, 0.72f);
   const FloatColor handleDrag = mix(base, drag, 0.82f);
   const FloatColor vertexBase = alpha(mix(base, {0.88f, 0.94f, 0.98f, 1.0f}, 0.46f), 1.0f);
   const FloatColor tangentIn = alpha(brighten(base, 1.16f), 0.92f);
   const FloatColor tangentOut = alpha(mix(base, {0.92f, 0.84f, 0.58f, 1.0f}, 0.26f), 0.92f);

   Detail::float2 previous{};
   for (int vertexIndex = 0; vertexIndex < count; ++vertexIndex) {
    const MaskVertex vertex = path.vertex(vertexIndex);
    const QPointF point = transform.map(vertex.position);
    const Detail::float2 current{static_cast<float>(point.x()), static_cast<float>(point.y())};
    const QPointF inPoint = transform.map(vertex.position + vertex.inTangent);
    const QPointF outPoint = transform.map(vertex.position + vertex.outTangent);
    const Detail::float2 inHandle{static_cast<float>(inPoint.x()), static_cast<float>(inPoint.y())};
    const Detail::float2 outHandle{static_cast<float>(outPoint.x()), static_cast<float>(outPoint.y())};

    const auto drawTangent = [&](const Detail::float2& endpoint,
                                 MaskHandleType handleType,
                                 const FloatColor& defaultStem,
                                 const FloatColor& defaultHandle) {
     const bool dragging = state.draggingHandle && matches(
         state.draggingMask, state.draggingPath, state.draggingVertexIndex,
         state.draggingHandleType, maskIndex, pathIndex, vertexIndex, handleType);
     const bool hovering = matches(
         state.hoveredMask, state.hoveredPath, state.hoveredVertex,
         state.hoveredHandleType, maskIndex, pathIndex, vertexIndex, handleType);
     const FloatColor stem = dragging ? handleDrag : hovering ? handleHover : defaultStem;
     const FloatColor handle = dragging ? handleDrag : hovering ? handleHover : defaultHandle;
     const float thickness = dragging ? 3.2f : hovering ? 2.8f : 2.2f;
     const float shadowThickness = dragging ? 7.8f : hovering ? 7.0f : 5.8f;
     renderer->drawThickLineLocal(current, endpoint, shadowThickness, lineShadow);
     renderer->drawThickLineLocal(current, endpoint, thickness, stem);
     renderer->drawThickLineLocal(current, endpoint,
                                  std::max(1.0f, thickness * 0.42f), brighten(stem, 1.20f));
     drawSolidHandle(renderer, endpoint, dragging || hovering ? 11.0f : 8.2f,
                     handle, dragging || hovering);
    };
    if (vertex.inTangent != QPointF())
     drawTangent(inHandle, MaskHandleType::InTangent, tangentIn, tangentIn);
    if (vertex.outTangent != QPointF())
     drawTangent(outHandle, MaskHandleType::OutTangent, tangentOut, handlePoint);

    if (vertexIndex > 0) {
     const bool segmentActive =
         (state.draggingVertex && state.draggingMask == maskIndex &&
          state.draggingPath == pathIndex &&
          (state.draggingVertexIndex == vertexIndex || state.draggingVertexIndex == vertexIndex - 1)) ||
         (state.hoveredMask == maskIndex && state.hoveredPath == pathIndex &&
          (state.hoveredVertex == vertexIndex || state.hoveredVertex == vertexIndex - 1));
     const FloatColor activeLine = segmentActive
         ? mix(line, hover, state.draggingVertex ? 0.72f : 0.58f) : line;
     renderer->drawThickLineLocal(previous, current, segmentActive ? 8.8f : 7.0f, lineShadow);
     renderer->drawThickLineLocal(previous, current, segmentActive ? 5.2f : 4.1f, activeLine);
     renderer->drawThickLineLocal(previous, current, segmentActive ? 2.2f : 1.5f,
                                  brighten(segmentActive ? activeLine : lineHighlight,
                                           segmentActive ? 1.16f : 1.0f));
    }
    previous = current;
   }

   if (path.isClosed() && count > 1) {
    const QPointF first = transform.map(path.vertex(0).position);
    const Detail::float2 firstPoint{static_cast<float>(first.x()), static_cast<float>(first.y())};
    renderer->drawThickLineLocal(previous, firstPoint, 7.4f, lineShadow);
    renderer->drawThickLineLocal(previous, firstPoint, 4.3f, line);
    renderer->drawThickLineLocal(previous, firstPoint, 1.6f, lineHighlight);
   }

   for (int vertexIndex = 0; vertexIndex < count; ++vertexIndex) {
    const QPointF point = transform.map(path.vertex(vertexIndex).position);
    const Detail::float2 current{static_cast<float>(point.x()), static_cast<float>(point.y())};
    const bool dragging = state.draggingVertex && state.draggingMask == maskIndex &&
                          state.draggingPath == pathIndex && state.draggingVertexIndex == vertexIndex;
    const bool hovering = state.hoveredMask == maskIndex && state.hoveredPath == pathIndex &&
                          state.hoveredVertex == vertexIndex;
    const FloatColor color = dragging ? drag : hovering ? hover : vertexBase;
    const float size = dragging ? 15.0f : hovering ? 14.0f : 12.0f;
    renderer->drawSolidRect(current.x - (size + 3.2f) * 0.5f,
                            current.y - (size + 3.2f) * 0.5f,
                            size + 3.2f, size + 3.2f, pointShadow, 1.0f);
    drawSolidHandle(renderer, current, size, color, dragging || hovering);
   }
  }
 }
}

}
