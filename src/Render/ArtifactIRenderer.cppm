module;
#include <QImage>
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <RefCntAutoPtr.hpp>

#include <memory>

module Artifact.Render.IRenderer;

namespace Artifact {

class ArtifactIRenderer::Impl {
public:
    Impl() : useSoftwareFallback(true) {}
    Impl(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> pDevice,
         Diligent::RefCntAutoPtr<Diligent::IDeviceContext> pContext,
         QWidget* widget)
        : pDevice(pDevice), pContext(pContext), useSoftwareFallback(false)
    {
        targetWidget = widget;
        if (!pDevice || !pContext) {
            useSoftwareFallback = true;
        }
    }

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> pContext;
    QWidget* targetWidget = nullptr;
    bool useSoftwareFallback = false;

    // Software fallback data
    QImage canvas;
    std::unique_ptr<QPainter> painter;
    FloatColor clearColor_{0.0f, 0.0f, 0.0f, 1.0f};

    void initialize(QWidget* parent) {
        targetWidget = parent;
        if (useSoftwareFallback && targetWidget) {
            canvas = QImage(targetWidget->size(), QImage::Format_ARGB32_Premultiplied);
            canvas.fill(Qt::black);
        }
    }

    void createSwapChain(QWidget* widget) {
        targetWidget = widget;
    }

    void recreateSwapChain(QWidget* widget) {
        targetWidget = widget;
        if (useSoftwareFallback && targetWidget) {
            if (canvas.size() != targetWidget->size()) {
                canvas = QImage(targetWidget->size(), QImage::Format_ARGB32_Premultiplied);
                canvas.fill(Qt::black);
            }
        }
    }

    void clear() {
        if (useSoftwareFallback) {
            if (painter) painter->end();
            QColor qc(
                static_cast<int>(clearColor_.r() * 255),
                static_cast<int>(clearColor_.g() * 255),
                static_cast<int>(clearColor_.b() * 255),
                static_cast<int>(clearColor_.a() * 255)
            );
            canvas.fill(qc);
            painter = std::make_unique<QPainter>(&canvas);
            painter->setRenderHint(QPainter::Antialiasing);
        }
    }

    void flushAndWait() {}
    void flush() {
        if (useSoftwareFallback && painter) {
            painter->end();
            painter.reset();
        }
    }

    void destroy() {
        if (painter) {
            painter->end();
            painter.reset();
        }
    }

    void setClearColor(const FloatColor& color) {
        clearColor_ = color;
    }

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
        if (useSoftwareFallback && painter) {
            painter->setPen(QColor(color.r()*255, color.g()*255, color.b()*255, color.a()*255));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(QRectF(x, y, w, h));
        }
    }
    void drawSolidLine(Detail::float2 start, Detail::float2 end, const FloatColor& color, float thickness)
    {
        if (useSoftwareFallback && painter) {
            QPen pen(QColor(color.r()*255, color.g()*255, color.b()*255, color.a()*255));
            pen.setWidthF(thickness);
            painter->setPen(pen);
            painter->drawLine(QPointF(start.x, start.y), QPointF(end.x, end.y));
        }
    }
    void drawSolidRect(float x, float y, float w, float h)
    {
        if (useSoftwareFallback && painter) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(Qt::white);
            painter->drawRect(QRectF(x, y, w, h));
        }
    }
    void drawSolidRect(Detail::float2 pos, Detail::float2 size, const FloatColor& color)
    {
        if (useSoftwareFallback && painter) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(color.r()*255, color.g()*255, color.b()*255, color.a()*255));
            painter->drawRect(QRectF(pos.x, pos.y, size.x, size.y));
        }
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
        if (useSoftwareFallback && painter) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(color.r()*255, color.g()*255, color.b()*255, color.a()*255));
            painter->drawRect(QRectF(x, y, w, h));
        }
    }
    void drawSpriteLocal(float x, float y, float w, float h, const QImage& image)
    {
        if (useSoftwareFallback && painter) {
            painter->drawImage(QRectF(x, y, w, h), image);
        }
    }
    void drawThickLineLocal(Detail::float2 p1, Detail::float2 p2, float thickness, const FloatColor& color)
    {
        drawSolidLine(p1, p2, color, thickness);
    }
    void drawDotLineLocal(Detail::float2 p1, Detail::float2 p2, float thickness, float spacing, const FloatColor& color)
    {
        if (useSoftwareFallback && painter) {
            QPen pen(QColor(color.r()*255, color.g()*255, color.b()*255, color.a()*255));
            pen.setWidthF(thickness);
            QVector<qreal> dashes;
            qreal space = spacing / thickness;
            dashes << 1 << space;
            pen.setDashPattern(dashes);
            painter->setPen(pen);
            painter->drawLine(QPointF(p1.x, p1.y), QPointF(p2.x, p2.y));
        }
    }
    void drawBezierLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, float thickness, const FloatColor& color)
    {
        if (useSoftwareFallback && painter) {
            QPainterPath path;
            path.moveTo(p0.x, p0.y);
            path.quadTo(p1.x, p1.y, p2.x, p2.y);
            QPen pen(QColor(color.r()*255, color.g()*255, color.b()*255, color.a()*255));
            pen.setWidthF(thickness);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(path);
        }
    }
    void drawBezierLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, Detail::float2 p3, float thickness, const FloatColor& color)
    {
        if (useSoftwareFallback && painter) {
            QPainterPath path;
            path.moveTo(p0.x, p0.y);
            path.cubicTo(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
            QPen pen(QColor(color.r()*255, color.g()*255, color.b()*255, color.a()*255));
            pen.setWidthF(thickness);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(path);
        }
    }
    void drawSolidTriangleLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2, const FloatColor& color)
    {
        if (useSoftwareFallback && painter) {
            QPainterPath path;
            path.moveTo(p0.x, p0.y);
            path.lineTo(p1.x, p1.y);
            path.lineTo(p2.x, p2.y);
            path.closeSubpath();
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(color.r()*255, color.g()*255, color.b()*255, color.a()*255));
            painter->drawPath(path);
        }
    }
    void drawRectOutlineLocal(float x, float y, float w, float h, const FloatColor& color)
    {
        drawRectOutline(x, y, w, h, color);
    }
    void drawCheckerboard(float x, float y, float w, float h, float tileSize, const FloatColor& c1, const FloatColor& c2)
    {
        if (useSoftwareFallback && painter) {
            painter->setPen(Qt::NoPen);
            for (float ty = y; ty < y + h; ty += tileSize) {
                for (float tx = x; tx < x + w; tx += tileSize) {
                    bool isC1 = (static_cast<int>(tx / tileSize) + static_cast<int>(ty / tileSize)) % 2 == 0;
                    const auto& c = isC1 ? c1 : c2;
                    painter->setBrush(QColor(c.r()*255, c.g()*255, c.b()*255, c.a()*255));
                    painter->drawRect(QRectF(tx, ty, std::min(tileSize, x + w - tx), std::min(tileSize, y + h - ty)));
                }
            }
        }
    }
    void drawGrid(float x, float y, float w, float h, float spacing, float thickness, const FloatColor& color)
    {
        if (useSoftwareFallback && painter) {
            QPen pen(QColor(color.r()*255, color.g()*255, color.b()*255, color.a()*255));
            pen.setWidthF(thickness);
            painter->setPen(pen);
            for (float ty = y; ty <= y + h; ty += spacing) {
                painter->drawLine(QPointF(x, ty), QPointF(x + w, ty));
            }
            for (float tx = x; tx <= x + w; tx += spacing) {
                painter->drawLine(QPointF(tx, y), QPointF(tx, y + h));
            }
        }
    }
    void drawParticles() {}
    void present() {
        if (useSoftwareFallback && targetWidget && !canvas.isNull()) {
            QPainter p(targetWidget);
            p.drawImage(0, 0, canvas);
        }
    }
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
