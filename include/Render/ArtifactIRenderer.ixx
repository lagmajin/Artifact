module;
#include <QImage>
#include <QWidget>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <RefCntAutoPtr.hpp>
#include <memory>

export module Artifact.Render.IRenderer;

import Color.Float;

export namespace Artifact {

using namespace ArtifactCore;

export namespace Detail {
 export struct RenderShaderPair {
  void* VS = nullptr;
  void* PS = nullptr;
 };

 export struct PSOAndSRB {
  void* pPSO = nullptr;
  void* pSRB = nullptr;
 };

 export struct float2 {
  float x = 0.0f;
  float y = 0.0f;
  float2() = default;
  float2(float x_, float y_) : x(x_), y(y_) {}
 };
}

export class ArtifactIRenderer {
public:
 class Impl;

 ArtifactIRenderer();
 explicit ArtifactIRenderer(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> pDevice,
                            Diligent::RefCntAutoPtr<Diligent::IDeviceContext> pContext,
                            QWidget* widget);
 ~ArtifactIRenderer();

 void initialize(QWidget* widget);
 void createSwapChain(QWidget* widget);
 void recreateSwapChain(QWidget* widget);

 void clear();
 void flush();
 void flushAndWait();
 void destroy();
 void present();

 void setClearColor(const FloatColor& color);
 void setViewportSize(float w, float h);
 void setCanvasSize(float w, float h);
 void setPan(float x, float y);
 void setZoom(float zoom);
 void panBy(float dx, float dy);
 void resetView();
 void fitToViewport(float margin = 50.0f);
 void zoomAroundViewportPoint(Detail::float2 viewportPos, float newZoom);

 Detail::float2 canvasToViewport(Detail::float2 pos) const;
 Detail::float2 viewportToCanvas(Detail::float2 pos) const;

 void drawRectOutline(float x, float y, float w, float h, const FloatColor& color);
 void drawRectOutline(Detail::float2 pos, Detail::float2 size, const FloatColor& color);
 void drawSolidLine(Detail::float2 start, Detail::float2 end, const FloatColor& color, float thickness);
 void drawSolidRect(float x, float y, float w, float h);
 void drawSolidRect(Detail::float2 pos, Detail::float2 size, const FloatColor& color);
 void drawSprite(float x, float y, float w, float h);
 void drawSprite(Detail::float2 pos, Detail::float2 size);
 void drawSprite(float x, float y, float w, float h, const QImage& image);
 void drawRectLocal(float x, float y, float w, float h, const FloatColor& color);
 void drawRectOutlineLocal(float x, float y, float w, float h, const FloatColor& color);
 void drawThickLineLocal(Detail::float2 p1, Detail::float2 p2, float thickness, const FloatColor& color);
 void drawDotLineLocal(Detail::float2 p1, Detail::float2 p2, float thickness, float spacing, const FloatColor& color);
 void drawBezierLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, float thickness, const FloatColor& color);
 void drawBezierLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, Detail::float2 p3, float thickness, const FloatColor& color);
 void drawSolidTriangleLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, const FloatColor& color);
 void drawCheckerboard(float x, float y, float w, float h, float tileSize, const FloatColor& c1, const FloatColor& c2);
 void drawGrid(float x, float y, float w, float h, float spacing, float thickness, const FloatColor& color);
 void drawParticles();
 void setUpscaleConfig(bool enable, float sharpness);

private:
 std::unique_ptr<Impl> impl_;
};

}
