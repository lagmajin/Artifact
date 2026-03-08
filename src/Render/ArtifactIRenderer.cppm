module;
#include <QImage>
#include <QWidget>
#include <windows.h>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <RefCntAutoPtr.hpp>

#include <memory>

module Artifact.Render.IRenderer;

namespace Artifact {

class ArtifactIRenderer::Impl {
public:
    Impl() = default;
    Impl(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> pDevice,
         Diligent::RefCntAutoPtr<Diligent::IDeviceContext> pContext,
         QWidget* widget)
        : pDevice(pDevice), pContext(pContext)
    {
        (void)widget;
    }

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> pContext;

    void initialize(QWidget* parent) { (void)parent; }
    void createSwapChain(QWidget* widget) { (void)widget; }
    void recreateSwapChain(QWidget* widget) { (void)widget; }
    void clear() {}
    void flushAndWait() {}
    void flush() {}
    void destroy() {}

    void setClearColor(const FloatColor& color) { (void)color; }
    void setViewportSize(float w, float h) { (void)w; (void)h; }
    void setCanvasSize(float w, float h) { (void)w; (void)h; }
    void setPan(float x, float y) { (void)x; (void)y; }
    void setZoom(float zoom) { (void)zoom; }
    void panBy(float dx, float dy) { (void)dx; (void)dy; }
    void resetView() {}
    void fitToViewport(float margin) { (void)margin; }
    void zoomAroundViewportPoint(Detail::float2 viewportPos, float newZoom) { (void)viewportPos; (void)newZoom; }

    Detail::float2 canvasToViewport(Detail::float2 pos) const { return pos; }
    Detail::float2 viewportToCanvas(Detail::float2 pos) const { return pos; }

    void drawRectOutline(float x, float y, float w, float h, const FloatColor& color)
    {
        (void)x; (void)y; (void)w; (void)h; (void)color;
    }
    void drawSolidLine(Detail::float2 start, Detail::float2 end, const FloatColor& color, float thickness)
    {
        (void)start; (void)end; (void)color; (void)thickness;
    }
    void drawSolidRect(float x, float y, float w, float h)
    {
        (void)x; (void)y; (void)w; (void)h;
    }
    void drawSolidRect(Detail::float2 pos, Detail::float2 size, const FloatColor& color)
    {
        (void)pos; (void)size; (void)color;
    }
    void drawSprite(float x, float y, float w, float h)
    {
        (void)x; (void)y; (void)w; (void)h;
    }
    void drawSprite(Detail::float2 pos, Detail::float2 size)
    {
        (void)pos; (void)size;
    }
    void drawRectLocal(float x, float y, float w, float h, const FloatColor& color)
    {
        (void)x; (void)y; (void)w; (void)h; (void)color;
    }
    void drawSpriteLocal(float x, float y, float w, float h, const QImage& image)
    {
        (void)x; (void)y; (void)w; (void)h; (void)image;
    }
    void drawThickLineLocal(Detail::float2 p1, Detail::float2 p2, float thickness, const FloatColor& color)
    {
        (void)p1; (void)p2; (void)thickness; (void)color;
    }
    void drawDotLineLocal(Detail::float2 p1, Detail::float2 p2, float thickness, float spacing, const FloatColor& color)
    {
        (void)p1; (void)p2; (void)thickness; (void)spacing; (void)color;
    }
    void drawBezierLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, float thickness, const FloatColor& color)
    {
        (void)p0; (void)p1; (void)p2; (void)thickness; (void)color;
    }
    void drawBezierLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, Detail::float2 p3, float thickness, const FloatColor& color)
    {
        (void)p0; (void)p1; (void)p2; (void)p3; (void)thickness; (void)color;
    }
    void drawSolidTriangleLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, const FloatColor& color)
    {
        (void)p0; (void)p1; (void)p2; (void)color;
    }
    void drawRectOutlineLocal(float x, float y, float w, float h, const FloatColor& color)
    {
        (void)x; (void)y; (void)w; (void)h; (void)color;
    }
    void drawCheckerboard(float x, float y, float w, float h, float tileSize, const FloatColor& c1, const FloatColor& c2)
    {
        (void)x; (void)y; (void)w; (void)h; (void)tileSize; (void)c1; (void)c2;
    }
    void drawGrid(float x, float y, float w, float h, float spacing, float thickness, const FloatColor& color)
    {
        (void)x; (void)y; (void)w; (void)h; (void)spacing; (void)thickness; (void)color;
    }
    void drawParticles() {}
    void present() {}
};

ArtifactIRenderer::ArtifactIRenderer() : impl_(std::make_unique<Impl>()) {}

ArtifactIRenderer::ArtifactIRenderer(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> pDevice,
                                     Diligent::RefCntAutoPtr<Diligent::IDeviceContext> pContext,
                                     QWidget* widget)
    : impl_(std::make_unique<Impl>(pDevice, pContext, widget))
{
}

ArtifactIRenderer::~ArtifactIRenderer() = default;

void ArtifactIRenderer::initialize(QWidget* widget) { if (impl_) impl_->initialize(widget); }
void ArtifactIRenderer::createSwapChain(QWidget* widget) { if (impl_) impl_->createSwapChain(widget); }
void ArtifactIRenderer::recreateSwapChain(QWidget* widget) { if (impl_) impl_->recreateSwapChain(widget); }
void ArtifactIRenderer::clear() { if (impl_) impl_->clear(); }
void ArtifactIRenderer::flush() { if (impl_) impl_->flush(); }
void ArtifactIRenderer::flushAndWait() { if (impl_) impl_->flushAndWait(); }
void ArtifactIRenderer::destroy() { if (impl_) impl_->destroy(); }

void ArtifactIRenderer::setClearColor(const FloatColor& color) { if (impl_) impl_->setClearColor(color); }
void ArtifactIRenderer::setViewportSize(float w, float h) { if (impl_) impl_->setViewportSize(w, h); }
void ArtifactIRenderer::setCanvasSize(float w, float h) { if (impl_) impl_->setCanvasSize(w, h); }
void ArtifactIRenderer::setPan(float x, float y) { if (impl_) impl_->setPan(x, y); }
void ArtifactIRenderer::setZoom(float zoom) { if (impl_) impl_->setZoom(zoom); }
void ArtifactIRenderer::panBy(float dx, float dy) { if (impl_) impl_->panBy(dx, dy); }
void ArtifactIRenderer::resetView() { if (impl_) impl_->resetView(); }
void ArtifactIRenderer::fitToViewport(float margin) { if (impl_) impl_->fitToViewport(margin); }
void ArtifactIRenderer::zoomAroundViewportPoint(Detail::float2 viewportPos, float newZoom)
{
    if (impl_) impl_->zoomAroundViewportPoint(viewportPos, newZoom);
}

Detail::float2 ArtifactIRenderer::canvasToViewport(Detail::float2 pos) const
{
    return impl_ ? impl_->canvasToViewport(pos) : Detail::float2{};
}

Detail::float2 ArtifactIRenderer::viewportToCanvas(Detail::float2 pos) const
{
    return impl_ ? impl_->viewportToCanvas(pos) : Detail::float2{};
}

void ArtifactIRenderer::drawRectOutline(float x, float y, float w, float h, const FloatColor& color)
{
    if (impl_) impl_->drawRectOutline(x, y, w, h, color);
}
void ArtifactIRenderer::drawRectOutline(Detail::float2 pos, Detail::float2 size, const FloatColor& color)
{
    if (impl_) impl_->drawRectOutline(pos.x, pos.y, size.x, size.y, color);
}
void ArtifactIRenderer::drawSolidLine(Detail::float2 start, Detail::float2 end, const FloatColor& color, float thickness)
{
    if (impl_) impl_->drawSolidLine(start, end, color, thickness);
}
void ArtifactIRenderer::drawSolidRect(float x, float y, float w, float h) { if (impl_) impl_->drawSolidRect(x, y, w, h); }
void ArtifactIRenderer::drawSolidRect(Detail::float2 pos, Detail::float2 size, const FloatColor& color)
{
    if (impl_) impl_->drawSolidRect(pos, size, color);
}
void ArtifactIRenderer::drawSprite(float x, float y, float w, float h) { if (impl_) impl_->drawSprite(x, y, w, h); }
void ArtifactIRenderer::drawSprite(Detail::float2 pos, Detail::float2 size) { if (impl_) impl_->drawSprite(pos, size); }
void ArtifactIRenderer::drawSprite(float x, float y, float w, float h, const QImage& image)
{
    if (impl_) impl_->drawSpriteLocal(x, y, w, h, image);
}
void ArtifactIRenderer::drawRectLocal(float x, float y, float w, float h, const FloatColor& color)
{
    if (impl_) impl_->drawRectLocal(x, y, w, h, color);
}
void ArtifactIRenderer::drawRectOutlineLocal(float x, float y, float w, float h, const FloatColor& color)
{
    if (impl_) impl_->drawRectOutlineLocal(x, y, w, h, color);
}
void ArtifactIRenderer::drawThickLineLocal(Detail::float2 p1, Detail::float2 p2, float thickness, const FloatColor& color)
{
    if (impl_) impl_->drawThickLineLocal(p1, p2, thickness, color);
}
void ArtifactIRenderer::drawDotLineLocal(Detail::float2 p1, Detail::float2 p2, float thickness, float spacing, const FloatColor& color)
{
    if (impl_) impl_->drawDotLineLocal(p1, p2, thickness, spacing, color);
}
void ArtifactIRenderer::drawBezierLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, float thickness, const FloatColor& color)
{
    if (impl_) impl_->drawBezierLocal(p0, p1, p2, thickness, color);
}
void ArtifactIRenderer::drawBezierLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, Detail::float2 p3, float thickness, const FloatColor& color)
{
    if (impl_) impl_->drawBezierLocal(p0, p1, p2, p3, thickness, color);
}
void ArtifactIRenderer::drawSolidTriangleLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, const FloatColor& color)
{
    if (impl_) impl_->drawSolidTriangleLocal(p0, p1, p2, color);
}
void ArtifactIRenderer::drawCheckerboard(float x, float y, float w, float h, float tileSize, const FloatColor& c1, const FloatColor& c2)
{
    if (impl_) impl_->drawCheckerboard(x, y, w, h, tileSize, c1, c2);
}
void ArtifactIRenderer::drawGrid(float x, float y, float w, float h, float spacing, float thickness, const FloatColor& color)
{
    if (impl_) impl_->drawGrid(x, y, w, h, spacing, thickness, color);
}
void ArtifactIRenderer::drawParticles() { if (impl_) impl_->drawParticles(); }
void ArtifactIRenderer::setUpscaleConfig(bool enable, float sharpness)
{
    (void)enable;
    (void)sharpness;
}
void ArtifactIRenderer::present() { if (impl_) impl_->present(); }

} // namespace Artifact
