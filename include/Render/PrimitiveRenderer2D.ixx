module;
#include <QImage>
#include <QTransform>
#include <QMatrix4x4>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <SwapChain.h>
#include <Texture.h>
#include <RefCntAutoPtr.hpp>
#include <BasicMath.hpp>

export module Artifact.Render.PrimitiveRenderer2D;

import Graphics;
import Color.Float;
import Artifact.Render.ShaderManager;

export namespace Artifact {

using namespace Diligent;
using namespace ArtifactCore;

class PrimitiveRenderer2D {
public:
    PrimitiveRenderer2D();
    ~PrimitiveRenderer2D();

    void createBuffers(RefCntAutoPtr<IRenderDevice> device, TEXTURE_FORMAT rtvFormat);
    void setPSOs(ShaderManager& shaderManager);
    void setContext(IDeviceContext* ctx, ISwapChain* swapChain);
    void setOverrideRTV(ITextureView* rtv);
    void destroy();

    void clear(const FloatColor& color);

    void setViewportSize(float w, float h);
    void setCanvasSize(float w, float h);
    void setPan(float x, float y);
    void getPan(float& x, float& y) const;
    void setZoom(float zoom);
    float getZoom() const;
    void panBy(float dx, float dy);
    void resetView();
    void fitToViewport(float margin);
    void setViewMatrix(const QMatrix4x4& view);
    void setProjectionMatrix(const QMatrix4x4& proj);
    void setUseExternalMatrices(bool use);
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix() const;
    void zoomAroundViewportPoint(float2 viewportPos, float newZoom);
    float2 canvasToViewport(float2 pos) const;
    float2 viewportToCanvas(float2 pos) const;

    void drawRectLocal(float x, float y, float w, float h, const FloatColor& color, float opacity = 1.0f);
    void drawSolidRectTransformed(float x, float y, float w, float h, const QTransform& transform, const FloatColor& color, float opacity = 1.0f);
    void drawSolidRectTransformed(float x, float y, float w, float h, const QMatrix4x4& transform, const FloatColor& color, float opacity = 1.0f);
    void drawRectOutlineLocal(float x, float y, float w, float h, const FloatColor& color);
    void drawLineLocal(float2 p1, float2 p2, const FloatColor& c1, const FloatColor& c2);
    void drawThickLineLocal(float2 p1, float2 p2, float thickness, const FloatColor& color);
    void drawDotLineLocal(float2 p1, float2 p2, float thickness, float spacing, const FloatColor& color);
    void drawBezierLocal(float2 p0, float2 p1, float2 p2, float thickness, const FloatColor& color);
    void drawBezierLocal(float2 p0, float2 p1, float2 p2, float2 p3, float thickness, const FloatColor& color);
    void drawSolidTriangleLocal(float2 p0, float2 p1, float2 p2, const FloatColor& color);
    void drawCircle(float x, float y, float radius, const FloatColor& color, float thickness = 1.0f, bool fill = false);
    void drawCrosshair(float x, float y, float size, const FloatColor& color);
    void drawSolidRect(float x, float y, float w, float h, const FloatColor& color, float opacity = 1.0f);
    void drawPoint(float x, float y, float size, const FloatColor& color);
    void drawCheckerboard(float x, float y, float w, float h, float tileSize, const FloatColor& c1, const FloatColor& c2);
    void drawGrid(float x, float y, float w, float h, float spacing, float thickness, const FloatColor& color);
    void drawSpriteLocal(float x, float y, float w, float h, const QImage& image, float opacity = 1.0f);
    void drawSpriteTransformed(float x, float y, float w, float h, const QTransform& transform, const QImage& image, float opacity = 1.0f);
    void drawSpriteTransformed(float x, float y, float w, float h, const QMatrix4x4& transform, const QImage& image, float opacity = 1.0f);
    void drawTextureLocal(float x, float y, float w, float h, ITextureView* pSRV, float opacity = 1.0f);

private:
    class Impl;
    Impl* impl_;
};

}
