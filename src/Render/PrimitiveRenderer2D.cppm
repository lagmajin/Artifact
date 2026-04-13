module;
#include <utility>
#include <array>
#include <cmath>
#include <cstring>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <QDebug>
#include <QLoggingCategory>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Sampler.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Common/interface/BasicMath.hpp>

module Artifact.Render.PrimitiveRenderer2D;

import Graphics;
import VertexBuffer;
import Graphics.CBuffer.Constants.PixelShader;
import Graphics.CBuffer.Constants.VertexShader;
import Core.Transform.Viewport;
import Math.Bezier;
import Render.Shader.ThickLine;
import Render.Shader.ViewerHelpers;
import Color.Float;
import Artifact.Render.ShaderManager;
import Artifact.Render.RenderCommandBuffer;

namespace Artifact {

namespace {
// Compute a stable cache key from image content (dimensions + pixel hash).
// QImage::cacheKey() changes on every new instance even with identical data,
// so we hash the actual bits instead.
qint64 computeImageContentKey(const QImage& image) {
    if (image.isNull()) return 0;
    // Hash dimensions + first 4 KB of pixel data for a fast fingerprint.
    const size_t sampleBytes = std::min<size_t>(
        static_cast<size_t>(image.sizeInBytes()), 4096u);
    quint32 h = qHashMulti(0, image.width(), image.height(),
                           image.format(), image.depth());
    const uchar* data = image.constBits();
    // FNV-1a style mix over sampled bytes
    for (size_t i = 0; i < sampleBytes; ++i) {
        h ^= data[i];
        h *= 16777619u;
    }
    // Also fold in the last byte to catch trailing differences
    if (sampleBytes > 0) {
        h ^= data[sampleBytes - 1];
        h *= 16777619u;
    }
    return static_cast<qint64>(h);
}
} // namespace

Q_LOGGING_CATEGORY(primitiveRenderer2DLog, "artifact.primitiverenderer2d")

using namespace Diligent;
using namespace ArtifactCore;

class PrimitiveRenderer2D::Impl {
public:
    ISwapChain*                  pSwapChain_ = nullptr;
    RefCntAutoPtr<IRenderDevice> pDevice_;

    struct CachedTexture {
        RefCntAutoPtr<ITexture> pTexture;
        qint64 lastUsedFrame = 0;
    };
    std::unordered_map<qint64, CachedTexture> m_spriteTexCache;
    std::unordered_map<qint64, CachedTexture> m_maskTexCache;
    qint64 m_frameCount = 0;

    void pruneCache() {
        if (m_spriteTexCache.size() > 50) {
            for (auto it = m_spriteTexCache.begin(); it != m_spriteTexCache.end(); ) {
                if (it->second.lastUsedFrame < m_frameCount - 60)
                    it = m_spriteTexCache.erase(it);
                else
                    ++it;
            }
        }
        if (m_maskTexCache.size() > 50) {
            for (auto it = m_maskTexCache.begin(); it != m_maskTexCache.end(); ) {
                if (it->second.lastUsedFrame < m_frameCount - 60)
                    it = m_maskTexCache.erase(it);
                else
                    ++it;
            }
        }
    }

    ViewportTransformer viewport_;

    QMatrix4x4 externalViewMatrix_;
    QMatrix4x4 externalProjMatrix_;
    bool useExternalMatrices_ = false;

    ITextureView*        m_overrideRTV = nullptr;
    RenderCommandBuffer* cmdBuf_       = nullptr;

    ITextureView* getCurrentRTV() const {
        if (m_overrideRTV) return m_overrideRTV;
        return pSwapChain_ ? pSwapChain_->GetCurrentBackBufferRTV() : nullptr;
    }
    bool hasRenderTarget() const {
        return cmdBuf_ != nullptr && (m_overrideRTV != nullptr || pSwapChain_ != nullptr);
    }
};

PrimitiveRenderer2D::PrimitiveRenderer2D()
    : impl_(new Impl())
{
}

PrimitiveRenderer2D::~PrimitiveRenderer2D()
{
    delete impl_;
}

void PrimitiveRenderer2D::setContext(IDeviceContext* /*ctx*/, ISwapChain* swapChain)
{
    impl_->pSwapChain_ = swapChain;
}

void PrimitiveRenderer2D::setPSOs(ShaderManager& /*shaderManager*/)
{
    // GPU resources are now owned by DiligentImmediateSubmitter — no-op here.
}

void PrimitiveRenderer2D::createBuffers(RefCntAutoPtr<IRenderDevice> device, TEXTURE_FORMAT /*rtvFormat*/)
{
    if (!device) return;
    impl_->pDevice_ = device;
}

void PrimitiveRenderer2D::destroy()
{
    impl_->m_spriteTexCache.clear();
    impl_->m_maskTexCache.clear();
    impl_->pDevice_      = nullptr;
    impl_->pSwapChain_   = nullptr;
    impl_->m_overrideRTV = nullptr;
    impl_->cmdBuf_       = nullptr;
}

void PrimitiveRenderer2D::setViewportSize(float w, float h)  { impl_->viewport_.SetViewportSize(w, h); }
void PrimitiveRenderer2D::setCanvasSize(float w, float h)    { impl_->viewport_.SetCanvasSize(w, h); }
void PrimitiveRenderer2D::setPan(float x, float y)           { impl_->viewport_.SetPan(x, y); }
void PrimitiveRenderer2D::getPan(float& x, float& y) const
{
 auto p = impl_->viewport_.GetPan();
 x = p.x;
 y = p.y;
}
void PrimitiveRenderer2D::setZoom(float zoom)                { impl_->viewport_.SetZoom(zoom); }float PrimitiveRenderer2D::getZoom() const                   { return impl_->viewport_.GetZoom(); }
void PrimitiveRenderer2D::panBy(float dx, float dy)          { impl_->viewport_.PanBy(dx, dy); }
void PrimitiveRenderer2D::resetView()                        { impl_->viewport_.ResetView(); }
void PrimitiveRenderer2D::fitToViewport(float margin)        { impl_->viewport_.FitCanvasToViewport(margin); }
void PrimitiveRenderer2D::fillToViewport(float margin)       { impl_->viewport_.FillCanvasToViewport(margin); }

void PrimitiveRenderer2D::setViewMatrix(const QMatrix4x4& view) { impl_->externalViewMatrix_ = view; }
void PrimitiveRenderer2D::setProjectionMatrix(const QMatrix4x4& proj) { impl_->externalProjMatrix_ = proj; }
void PrimitiveRenderer2D::setUseExternalMatrices(bool use) { impl_->useExternalMatrices_ = use; }

QMatrix4x4 PrimitiveRenderer2D::viewMatrix() const { return impl_->externalViewMatrix_; }
QMatrix4x4 PrimitiveRenderer2D::projectionMatrix() const { return impl_->externalProjMatrix_; }
QMatrix4x4 PrimitiveRenderer2D::getViewMatrix() const { return impl_->externalViewMatrix_; }
QMatrix4x4 PrimitiveRenderer2D::getProjectionMatrix() const { return impl_->externalProjMatrix_; }

void PrimitiveRenderer2D::zoomAroundViewportPoint(float2 viewportPos, float newZoom)
{
    impl_->viewport_.ZoomAroundViewportPoint(viewportPos, newZoom);
}

float2 PrimitiveRenderer2D::canvasToViewport(float2 pos) const
{
    return impl_->viewport_.CanvasToViewport(pos);
}

float2 PrimitiveRenderer2D::viewportToCanvas(float2 pos) const
{
    return impl_->viewport_.ViewportToCanvas(pos);
}

void PrimitiveRenderer2D::setOverrideRTV(ITextureView* rtv)
{
    impl_->m_overrideRTV = rtv;
}

void PrimitiveRenderer2D::setCommandBuffer(RenderCommandBuffer* cmdBuf)
{
    impl_->cmdBuf_ = cmdBuf;
}

void PrimitiveRenderer2D::clear(IDeviceContext* ctx, const FloatColor& color)
{
    if (!ctx) return;
    auto* pRTV = impl_->getCurrentRTV();
    if (!pRTV) return;

    const auto vp = impl_->viewport_.GetViewportCB();
    const float vpW = std::max(vp.screenSize.x, 1.0f);
    const float vpH = std::max(vp.screenSize.y, 1.0f);
    Viewport VP;
    VP.TopLeftX = 0.0f; VP.TopLeftY = 0.0f;
    VP.Width    = vpW;  VP.Height   = vpH;
    VP.MinDepth = 0.0f; VP.MaxDepth = 1.0f;
    ctx->SetViewports(1, &VP, static_cast<Uint32>(vpW), static_cast<Uint32>(vpH));

    float clearColor[] = { color.r(), color.g(), color.b(), color.a() };
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ctx->ClearRenderTarget(pRTV, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    if (impl_->cmdBuf_) {
        impl_->cmdBuf_->reset();
        impl_->cmdBuf_->targetRTV = pRTV;
    }
}

void PrimitiveRenderer2D::drawRectLocal(float x, float y, float w, float h, const FloatColor& color, float opacity)
{
    if (!impl_->cmdBuf_) return;
    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float zoom = std::max(viewportCB.zoom, 0.001f);

    SolidRectPkt pkt;
    pkt.xform.offset     = { x * zoom + viewportCB.offset.x, y * zoom + viewportCB.offset.y };
    pkt.xform.scale      = { w * zoom, h * zoom };
    pkt.xform.screenSize = viewportCB.screenSize;
    pkt.color            = { color.r(), color.g(), color.b(), color.a() * opacity };
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawSolidRectTransformed(float x, float y, float w, float h, const QTransform& transform, const FloatColor& color, float opacity)
{
    if (!impl_->cmdBuf_) return;
    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float screenW = std::max(viewportCB.screenSize.x, 0.001f);
    const float screenH = std::max(viewportCB.screenSize.y, 0.001f);
    const float zoom = std::max(viewportCB.zoom, 0.001f);
    const float panX = viewportCB.offset.x, panY = viewportCB.offset.y;

    const float a  = (float)transform.m11(), b = (float)transform.m12();
    const float c  = (float)transform.m21(), d = (float)transform.m22();
    const float tx = (float)transform.dx(),  ty = (float)transform.dy();
    const float localX = a * x + c * y + tx;
    const float localY = b * x + d * y + ty;

    SolidRectXformPkt pkt;
    pkt.mat.row0 = { 2.0f*(a*w*zoom)/screenW, 2.0f*(c*h*zoom)/screenW, 0.0f, 2.0f*((localX*zoom)+panX)/screenW-1.0f };
    pkt.mat.row1 = { -2.0f*(b*w*zoom)/screenH, -2.0f*(d*h*zoom)/screenH, 0.0f, 1.0f-2.0f*((localY*zoom)+panY)/screenH };
    pkt.mat.row2 = { 0,0,0,0 };
    pkt.mat.row3 = { 0,0,0,1 };
    pkt.color    = { color.r(), color.g(), color.b(), color.a() * opacity };
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawSolidRectTransformed(float x, float y, float w, float h, const QMatrix4x4& transform, const FloatColor& color, float opacity)
{
    if (!impl_->cmdBuf_) return;
    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float screenW = std::max(viewportCB.screenSize.x, 0.001f);
    const float screenH = std::max(viewportCB.screenSize.y, 0.001f);
    const float zoom = std::max(viewportCB.zoom, 0.001f);
    const float panX = viewportCB.offset.x, panY = viewportCB.offset.y;

    QMatrix4x4 finalMat;
    if (impl_->useExternalMatrices_) {
        QMatrix4x4 model = transform;
        model.translate(x, y, 0);
        model.scale(w, h, 1.0f);
        finalMat = impl_->externalProjMatrix_ * impl_->externalViewMatrix_ * model;
    } else {
        QMatrix4x4 combined = transform;
        combined.translate(x, y, 0);
        combined.scale(w, h, 1.0f);
        QMatrix4x4 canvasToNdc;
        canvasToNdc.setToIdentity();
        canvasToNdc.translate(-1.0f, 1.0f, 0.0f);
        canvasToNdc.scale(2.0f / screenW, -2.0f / screenH, 1.0f);
        canvasToNdc.scale(zoom, zoom, 1.0f);
        canvasToNdc.translate(panX / zoom, panY / zoom, 0.0f);
        finalMat = canvasToNdc * combined;
    }

    SolidRectXformPkt pkt;
    pkt.mat.row0 = { finalMat.row(0).x(), finalMat.row(0).y(), finalMat.row(0).z(), finalMat.row(0).w() };
    pkt.mat.row1 = { finalMat.row(1).x(), finalMat.row(1).y(), finalMat.row(1).z(), finalMat.row(1).w() };
    pkt.mat.row2 = { finalMat.row(2).x(), finalMat.row(2).y(), finalMat.row(2).z(), finalMat.row(2).w() };
    pkt.mat.row3 = { finalMat.row(3).x(), finalMat.row(3).y(), finalMat.row(3).z(), finalMat.row(3).w() };
    pkt.color    = { color.r(), color.g(), color.b(), color.a() * opacity };
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawSolidRect(float x, float y, float w, float h, const FloatColor& color, float opacity)
{
    this->drawRectLocal(x, y, w, h, color, opacity);
}

void PrimitiveRenderer2D::drawLineLocal(float2 p1, float2 p2, const FloatColor& c1, const FloatColor& c2)
{
    if (!impl_->cmdBuf_) return;
    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float zoom = std::max(viewportCB.zoom, 0.001f);

    LinePkt pkt;
    pkt.xform.offset     = viewportCB.offset;
    pkt.xform.scale      = { zoom, zoom };
    pkt.xform.screenSize = viewportCB.screenSize;
    pkt.p1 = p1; pkt.p2 = p2;
    pkt.c1 = { c1.r(), c1.g(), c1.b(), 1.0f };
    pkt.c2 = { c2.r(), c2.g(), c2.b(), 1.0f };
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawQuadLocal(float2 p0, float2 p1, float2 p2, float2 p3, const FloatColor& color)
{
    if (!impl_->cmdBuf_) return;
    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float zoom = std::max(viewportCB.zoom, 0.001f);

    QuadPkt pkt;
    pkt.xform.offset     = viewportCB.offset;
    pkt.xform.scale      = { zoom, zoom };
    pkt.xform.screenSize = viewportCB.screenSize;
    pkt.p0 = p0; pkt.p1 = p1; pkt.p2 = p2; pkt.p3 = p3;
    pkt.color = { color.r(), color.g(), color.b(), color.a() };
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawThickLineLocal(float2 p1, float2 p2, float thickness, const FloatColor& color)
{
    if (thickness <= 0.0f) return;

    float2 d   = { p2.x - p1.x, p2.y - p1.y };
    float  len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-5f) return;

    float2 nd   = { d.x / len, d.y / len };
    float  half = thickness * 0.5f;
    float2 n    = { -nd.y * half, nd.x * half };

    drawQuadLocal({ p1.x + n.x, p1.y + n.y },
                  { p1.x - n.x, p1.y - n.y },
                  { p2.x + n.x, p2.y + n.y },
                  { p2.x - n.x, p2.y - n.y },
                  color);
}

void PrimitiveRenderer2D::drawDotLineLocal(float2 p1, float2 p2, float thickness, float spacing, const FloatColor& color)
{
    if (!impl_->cmdBuf_ || thickness <= 0.0f) return;

    float2 d   = { p2.x - p1.x, p2.y - p1.y };
    float  len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-5f) return;

    float2 nd  = { d.x / len, d.y / len };
    float  half = thickness * 0.5f;
    float2 n   = { -nd.y * half, nd.x * half };

    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float zoom = std::max(viewportCB.zoom, 0.001f);
    const float4 c = { color.r(), color.g(), color.b(), color.a() };

    DotLinePkt pkt;
    pkt.xform.offset     = viewportCB.offset;
    pkt.xform.scale      = { zoom, zoom };
    pkt.xform.screenSize = viewportCB.screenSize;
    pkt.verts[0] = { { p1.x + n.x, p1.y + n.y }, c, 0.0f, 0.0f };
    pkt.verts[1] = { { p1.x - n.x, p1.y - n.y }, c, 0.0f, 0.0f };
    pkt.verts[2] = { { p2.x + n.x, p2.y + n.y }, c, len,  0.0f };
    pkt.verts[3] = { { p2.x - n.x, p2.y - n.y }, c, len,  0.0f };
    pkt.thickness = thickness;
    pkt.spacing   = spacing;
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawDashedLineLocal(float2 p1, float2 p2, float thickness, float dashLength, float gapLength, const FloatColor& color)
{
    if (thickness <= 0.0f) {
        return;
    }
    if (dashLength <= 0.0f) {
        dashLength = thickness * 2.0f;
    }
    if (gapLength < 0.0f) {
        gapLength = 0.0f;
    }

    const float2 d = { p2.x - p1.x, p2.y - p1.y };
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-5f) {
        return;
    }

    const float2 nd = { d.x / len, d.y / len };
    const float step = std::max(1e-3f, dashLength + gapLength);

    for (float cur = 0.0f; cur < len; cur += step) {
        const float dashEnd = std::min(cur + dashLength, len);
        if (dashEnd <= cur + 1e-4f) {
            continue;
        }
        const float2 a = { p1.x + nd.x * cur, p1.y + nd.y * cur };
        const float2 b = { p1.x + nd.x * dashEnd, p1.y + nd.y * dashEnd };
        drawThickLineLocal(a, b, thickness, color);
    }
}

void PrimitiveRenderer2D::drawBezierLocal(float2 p0, float2 p1, float2 p2, float thickness, const FloatColor& color)
{
    const int segments = 24;
    float2 lastPos = p0;
    for (int i = 1; i <= segments; ++i) {
        float t = float(i) / float(segments);
        QPointF qp = BezierCalculator::evaluateQuadratic({ p0.x, p0.y }, { p1.x, p1.y }, { p2.x, p2.y }, t);
        float2 currentPos = { float(qp.x()), float(qp.y()) };
        drawThickLineLocal(lastPos, currentPos, thickness, color);
        lastPos = currentPos;
    }
}

void PrimitiveRenderer2D::drawBezierLocal(float2 p0, float2 p1, float2 p2, float2 p3, float thickness, const FloatColor& color)
{
    const int segments = 32;
    float2 lastPos = p0;
    for (int i = 1; i <= segments; ++i) {
        float t = float(i) / float(segments);
        QPointF qp = BezierCalculator::evaluateCubic({ p0.x, p0.y }, { p1.x, p1.y }, { p2.x, p2.y }, { p3.x, p3.y }, t);
        float2 currentPos = { float(qp.x()), float(qp.y()) };
        drawThickLineLocal(lastPos, currentPos, thickness, color);
        lastPos = currentPos;
    }
}

void PrimitiveRenderer2D::drawSolidTriangleLocal(float2 p0, float2 p1, float2 p2, const FloatColor& color)
{
    if (!impl_->cmdBuf_) return;
    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float zoom = std::max(viewportCB.zoom, 0.001f);

    SolidTriPkt pkt;
    pkt.xform.offset     = viewportCB.offset;
    pkt.xform.scale      = { zoom, zoom };
    pkt.xform.screenSize = viewportCB.screenSize;
    pkt.p0 = p0; pkt.p1 = p1; pkt.p2 = p2;
    pkt.color = { color.r(), color.g(), color.b(), color.a() };
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawCircle(float x, float y, float radius, const FloatColor& color, float thickness, bool fill)
{
    if (radius <= 0.0f) return;

    if (fill) {
        // 最適化：ファン状の三角形で 1 回の描画
        drawSolidCircle(x, y, radius, color);
        return;
    }

    // Draw circle using 4 cubic bezier segments
    const float k = 0.552284749831f; // control point offset
    const float r = radius;

    float2 p0 = {x, y - r};
    float2 p1 = {x + r, y};
    float2 p2 = {x, y + r};
    float2 p3 = {x - r, y};

    drawBezierLocal(p0, {x + r * k, y - r}, {x + r, y - r * k}, p1, thickness, color);
    drawBezierLocal(p1, {x + r, y + r * k}, {x + r * k, y + r}, p2, thickness, color);
    drawBezierLocal(p2, {x - r * k, y + r}, {x - r, y + r * k}, p3, thickness, color);
    drawBezierLocal(p3, {x - r, y - r * k}, {x - r * k, y - r}, p0, thickness, color);
}

void PrimitiveRenderer2D::drawSolidCircle(float cx, float cy, float radius, const FloatColor& color)
{
    if (!impl_->cmdBuf_ || radius <= 0.0f) return;
    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float zoom = std::max(viewportCB.zoom, 0.001f);

    SolidCirclePkt pkt;
    pkt.xform.offset     = viewportCB.offset;
    pkt.xform.scale      = { zoom, zoom };
    pkt.xform.screenSize = viewportCB.screenSize;
    pkt.cx = cx; pkt.cy = cy; pkt.radius = radius;
    pkt.color = { color.r(), color.g(), color.b(), color.a() };
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawCrosshair(float x, float y, float size, const FloatColor& color)
{
    const float half = size * 0.5f;
    // 1. 背景用の太い黒線（シャドウ効果）
    FloatColor shadow = {0.0f, 0.0f, 0.0f, 0.6f};
    drawThickLineLocal({x - half, y}, {x + half, y}, 3.0f, shadow);
    drawThickLineLocal({x, y - half}, {x, y + half}, 3.0f, shadow);
    
    // 2. メインのカラー線
    drawThickLineLocal({x - half, y}, {x + half, y}, 1.0f, color);
    drawThickLineLocal({x, y - half}, {x, y + half}, 1.0f, color);
}

void PrimitiveRenderer2D::drawPoint(float x, float y, float size, const FloatColor& color)
{
    const float half = size * 0.5f;
    this->drawRectLocal(x - half, y - half, size, size, color, 1.0f);
}

void PrimitiveRenderer2D::drawCheckerboard(float x, float y, float w, float h, float tileSize, const FloatColor& c1, const FloatColor& c2)
{
    if (!impl_->cmdBuf_) return;
    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float zoom = std::max(viewportCB.zoom, 0.001f);

    CheckerboardPkt pkt;
    pkt.xform.offset     = { x * zoom + viewportCB.offset.x, y * zoom + viewportCB.offset.y };
    pkt.xform.scale      = { w * zoom, h * zoom };
    pkt.xform.screenSize = viewportCB.screenSize;
    pkt.helper.param0    = tileSize;
    pkt.helper.param1    = 0.0f;
    pkt.helper.color1    = { c1.r(), c1.g(), c1.b(), c1.a() };
    pkt.helper.color2    = { c2.r(), c2.g(), c2.b(), c2.a() };
    pkt.baseColor        = { c1.r(), c1.g(), c1.b(), c1.a() };
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawGrid(float x, float y, float w, float h,
    float spacing, float thickness, const FloatColor& color)
{
    if (!impl_->cmdBuf_) return;
    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float zoom = std::max(viewportCB.zoom, 0.001f);

    GridPkt pkt;
    pkt.xform.offset     = { x * zoom + viewportCB.offset.x, y * zoom + viewportCB.offset.y };
    pkt.xform.scale      = { w * zoom, h * zoom };
    pkt.xform.screenSize = viewportCB.screenSize;
    pkt.helper.param0    = spacing;
    pkt.helper.param1    = thickness;
    pkt.helper.color1    = { color.r(), color.g(), color.b(), 1.0f };
    pkt.helper.color2    = {};
    pkt.baseColor        = { color.r(), color.g(), color.b(), 1.0f };
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawRectOutlineLocal(float x, float y, float w, float h, const FloatColor& color)
{
    if (!impl_->cmdBuf_) return;
    const auto viewportCB = impl_->viewport_.GetViewportCB();

    RectOutlinePkt pkt;
    pkt.xform.offset     = { x, y };   // screen-space: no zoom
    pkt.xform.scale      = { w, h };
    pkt.xform.screenSize = viewportCB.screenSize;
    pkt.color            = { color.r(), color.g(), color.b(), color.a() };
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawSpriteLocal(float x, float y, float w, float h, const QImage& image, float opacity)
{
    if (!impl_->cmdBuf_ || image.isNull() || !impl_->pDevice_) return;

    impl_->m_frameCount++;
    if (impl_->m_frameCount % 60 == 0) impl_->pruneCache();

    qint64 cacheKey = computeImageContentKey(image);
    RefCntAutoPtr<ITexture> pTexture;

    auto it = impl_->m_spriteTexCache.find(cacheKey);
    if (it != impl_->m_spriteTexCache.end()) {
        pTexture = it->second.pTexture;
        it->second.lastUsedFrame = impl_->m_frameCount;
    } else {
        const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
        const int imgW = rgba.width(), imgH = rgba.height();
        if (imgW <= 0 || imgH <= 0) return;
        TextureDesc texDesc;
        texDesc.Type = RESOURCE_DIM_TEX_2D;
        texDesc.Width = static_cast<Uint32>(imgW);
        texDesc.Height = static_cast<Uint32>(imgH);
        texDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
        texDesc.MipLevels = 1;
        texDesc.Usage = USAGE_IMMUTABLE;
        texDesc.BindFlags = BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = CPU_ACCESS_NONE;
        TextureSubResData subData;
        subData.pData = rgba.constBits();
        subData.Stride = static_cast<Uint64>(rgba.bytesPerLine());
        TextureData initData;
        initData.pSubResources = &subData;
        initData.NumSubresources = 1;
        impl_->pDevice_->CreateTexture(texDesc, &initData, &pTexture);
        if (!pTexture) return;
        impl_->m_spriteTexCache[cacheKey] = { pTexture, impl_->m_frameCount };
    }

    auto* pSRV = pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    if (!pSRV) return;

    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float zoom = std::max(viewportCB.zoom, 0.001f);

    SpritePkt pkt;
    pkt.xform.offset     = { x * zoom + viewportCB.offset.x, y * zoom + viewportCB.offset.y };
    pkt.xform.scale      = { w * zoom, h * zoom };
    pkt.xform.screenSize = viewportCB.screenSize;
    pkt.pSRV             = pSRV;
    pkt.opacity          = opacity;
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawTextureLocal(float x, float y, float w, float h, ITextureView* pSRV, float opacity)
{
    if (!impl_->cmdBuf_ || !pSRV) return;
    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float zoom = std::max(viewportCB.zoom, 0.001f);

    SpritePkt pkt;
    pkt.xform.offset     = { x * zoom + viewportCB.offset.x, y * zoom + viewportCB.offset.y };
    pkt.xform.scale      = { w * zoom, h * zoom };
    pkt.xform.screenSize = viewportCB.screenSize;
    pkt.pSRV             = pSRV;
    pkt.opacity          = opacity;
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawMaskedTextureLocal(float x, float y, float w, float h, ITextureView* sceneSRV, const QImage& maskImage, float opacity)
{
    if (!impl_->cmdBuf_ || !sceneSRV || maskImage.isNull() || !impl_->pDevice_) return;

    impl_->m_frameCount++;
    if (impl_->m_frameCount % 60 == 0) impl_->pruneCache();

    qint64 cacheKey = computeImageContentKey(maskImage);
    RefCntAutoPtr<ITexture> pMaskTexture;

    auto it = impl_->m_maskTexCache.find(cacheKey);
    if (it != impl_->m_maskTexCache.end()) {
        pMaskTexture = it->second.pTexture;
        it->second.lastUsedFrame = impl_->m_frameCount;
    } else {
        const QImage rgba = maskImage.convertToFormat(QImage::Format_RGBA8888);
        const int imgW = rgba.width(), imgH = rgba.height();
        if (imgW <= 0 || imgH <= 0) return;
        TextureDesc texDesc;
        texDesc.Type = RESOURCE_DIM_TEX_2D;
        texDesc.Width = static_cast<Uint32>(imgW);
        texDesc.Height = static_cast<Uint32>(imgH);
        texDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
        texDesc.MipLevels = 1;
        texDesc.Usage = USAGE_IMMUTABLE;
        texDesc.BindFlags = BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = CPU_ACCESS_NONE;
        TextureSubResData subData;
        subData.pData = rgba.constBits();
        subData.Stride = static_cast<Uint64>(rgba.bytesPerLine());
        TextureData initData;
        initData.pSubResources = &subData;
        initData.NumSubresources = 1;
        impl_->pDevice_->CreateTexture(texDesc, &initData, &pMaskTexture);
        if (!pMaskTexture) return;
        impl_->m_maskTexCache[cacheKey] = { pMaskTexture, impl_->m_frameCount };
    }

    auto* maskSRV = pMaskTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    if (!maskSRV) return;

    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float zoom = std::max(viewportCB.zoom, 0.001f);

    MaskedSpritePkt pkt;
    pkt.xform.offset     = { x * zoom + viewportCB.offset.x, y * zoom + viewportCB.offset.y };
    pkt.xform.scale      = { w * zoom, h * zoom };
    pkt.xform.screenSize = viewportCB.screenSize;
    pkt.sceneSRV         = sceneSRV;
    pkt.maskSRV          = maskSRV;
    pkt.opacity          = opacity;
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawSpriteTransformed(float x, float y, float w, float h, const QTransform& transform, const QImage& image, float opacity)
{
    if (!impl_->cmdBuf_ || image.isNull() || !impl_->pDevice_) return;

    impl_->m_frameCount++;
    if (impl_->m_frameCount % 60 == 0) impl_->pruneCache();

    qint64 cacheKey = computeImageContentKey(image);
    RefCntAutoPtr<ITexture> pTexture;

    auto it = impl_->m_spriteTexCache.find(cacheKey);
    if (it != impl_->m_spriteTexCache.end()) {
        pTexture = it->second.pTexture;
        it->second.lastUsedFrame = impl_->m_frameCount;
    } else {
        const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
        const int imgW = rgba.width(), imgH = rgba.height();
        if (imgW <= 0 || imgH <= 0) return;
        TextureDesc texDesc;
        texDesc.Type = RESOURCE_DIM_TEX_2D;
        texDesc.Width = static_cast<Uint32>(imgW);
        texDesc.Height = static_cast<Uint32>(imgH);
        texDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
        texDesc.MipLevels = 1;
        texDesc.Usage = USAGE_IMMUTABLE;
        texDesc.BindFlags = BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = CPU_ACCESS_NONE;
        TextureSubResData subData;
        subData.pData = rgba.constBits();
        subData.Stride = static_cast<Uint64>(rgba.bytesPerLine());
        TextureData initData;
        initData.pSubResources = &subData;
        initData.NumSubresources = 1;
        impl_->pDevice_->CreateTexture(texDesc, &initData, &pTexture);
        if (!pTexture) return;
        impl_->m_spriteTexCache[cacheKey] = { pTexture, impl_->m_frameCount };
    }

    auto* pSRV = pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    if (!pSRV) return;

    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float screenW = std::max(viewportCB.screenSize.x, 0.001f);
    const float screenH = std::max(viewportCB.screenSize.y, 0.001f);
    const float zoom = std::max(viewportCB.zoom, 0.001f);
    const float panX = viewportCB.offset.x, panY = viewportCB.offset.y;

    const float a  = (float)transform.m11(), b  = (float)transform.m12();
    const float c  = (float)transform.m21(), d  = (float)transform.m22();
    const float tx = (float)transform.dx(),  ty = (float)transform.dy();
    const float localX = a * x + c * y + tx;
    const float localY = b * x + d * y + ty;

    SpriteXformPkt pkt;
    pkt.mat.row0 = { 2.0f*(a*w*zoom)/screenW, 2.0f*(c*h*zoom)/screenW, 0.0f, 2.0f*((localX*zoom)+panX)/screenW - 1.0f };
    pkt.mat.row1 = { -2.0f*(b*w*zoom)/screenH, -2.0f*(d*h*zoom)/screenH, 0.0f, 1.0f - 2.0f*((localY*zoom)+panY)/screenH };
    pkt.mat.row2 = { 0.0f, 0.0f, 0.0f, 0.0f };
    pkt.mat.row3 = { 0.0f, 0.0f, 0.0f, 1.0f };
    pkt.pSRV     = pSRV;
    pkt.opacity  = opacity;
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawSpriteTransformed(float x, float y, float w, float h, const QMatrix4x4& transform, ITextureView* pSRV, float opacity)
{
    if (!impl_->cmdBuf_ || !pSRV) return;

    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float screenW = std::max(viewportCB.screenSize.x, 0.001f);
    const float screenH = std::max(viewportCB.screenSize.y, 0.001f);
    const float zoom = std::max(viewportCB.zoom, 0.001f);
    const float panX = viewportCB.offset.x, panY = viewportCB.offset.y;

    QMatrix4x4 finalMat;
    if (impl_->useExternalMatrices_) {
        QMatrix4x4 model = transform;
        model.translate(x, y, 0);
        model.scale(w, h, 1.0f);
        finalMat = impl_->externalProjMatrix_ * impl_->externalViewMatrix_ * model;
    } else {
        QMatrix4x4 combined = transform;
        combined.translate(x, y, 0);
        combined.scale(w, h, 1.0f);
        QMatrix4x4 canvasToNdc;
        canvasToNdc.setToIdentity();
        canvasToNdc.translate(-1.0f, 1.0f, 0.0f);
        canvasToNdc.scale(2.0f / screenW, -2.0f / screenH, 1.0f);
        canvasToNdc.scale(zoom, zoom, 1.0f);
        canvasToNdc.translate(panX / zoom, panY / zoom, 0.0f);
        finalMat = canvasToNdc * combined;
    }

    SpriteXformPkt pkt;
    pkt.mat.row0 = { finalMat.row(0).x(), finalMat.row(0).y(), finalMat.row(0).z(), finalMat.row(0).w() };
    pkt.mat.row1 = { finalMat.row(1).x(), finalMat.row(1).y(), finalMat.row(1).z(), finalMat.row(1).w() };
    pkt.mat.row2 = { finalMat.row(2).x(), finalMat.row(2).y(), finalMat.row(2).z(), finalMat.row(2).w() };
    pkt.mat.row3 = { finalMat.row(3).x(), finalMat.row(3).y(), finalMat.row(3).z(), finalMat.row(3).w() };
    pkt.pSRV     = pSRV;
    pkt.opacity  = opacity;
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawSpriteTransformed(float x, float y, float w, float h, const QMatrix4x4& transform, const QImage& image, float opacity)
{
    if (!impl_->cmdBuf_ || image.isNull() || !impl_->pDevice_) return;

    impl_->m_frameCount++;
    if (impl_->m_frameCount % 60 == 0) impl_->pruneCache();

    const qint64 cacheKey = computeImageContentKey(image);
    RefCntAutoPtr<ITexture> pTexture;

    auto it = impl_->m_spriteTexCache.find(cacheKey);
    if (it != impl_->m_spriteTexCache.end()) {
        pTexture = it->second.pTexture;
        it->second.lastUsedFrame = impl_->m_frameCount;
    } else {
        const QImage rgba = (image.format() == QImage::Format_RGBA8888)
                                ? image : image.convertToFormat(QImage::Format_RGBA8888);
        const int imgW = rgba.width(), imgH = rgba.height();
        if (imgW <= 0 || imgH <= 0) return;
        TextureDesc texDesc;
        texDesc.Type = RESOURCE_DIM_TEX_2D;
        texDesc.Width = static_cast<Uint32>(imgW);
        texDesc.Height = static_cast<Uint32>(imgH);
        texDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
        texDesc.MipLevels = 1;
        texDesc.Usage = USAGE_IMMUTABLE;
        texDesc.BindFlags = BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = CPU_ACCESS_NONE;
        TextureSubResData subData;
        subData.pData = rgba.constBits();
        subData.Stride = static_cast<Uint64>(rgba.bytesPerLine());
        TextureData initData;
        initData.pSubResources = &subData;
        initData.NumSubresources = 1;
        RefCntAutoPtr<ITexture> newTex;
        impl_->pDevice_->CreateTexture(texDesc, &initData, &newTex);
        if (!newTex) return;
        impl_->m_spriteTexCache[cacheKey] = { newTex, impl_->m_frameCount };
        pTexture = newTex;
    }

    auto* pSRV = pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    if (!pSRV) return;

    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float screenW = std::max(viewportCB.screenSize.x, 0.001f);
    const float screenH = std::max(viewportCB.screenSize.y, 0.001f);
    const float zoom = std::max(viewportCB.zoom, 0.001f);
    const float panX = viewportCB.offset.x, panY = viewportCB.offset.y;

    QMatrix4x4 finalMat;
    if (impl_->useExternalMatrices_) {
        QMatrix4x4 model = transform;
        model.translate(x, y, 0);
        model.scale(w, h, 1.0f);
        finalMat = impl_->externalProjMatrix_ * impl_->externalViewMatrix_ * model;
    } else {
        QMatrix4x4 combined = transform;
        combined.translate(x, y, 0);
        combined.scale(w, h, 1.0f);
        QMatrix4x4 canvasToNdc;
        canvasToNdc.setToIdentity();
        canvasToNdc.translate(-1.0f, 1.0f, 0.0f);
        canvasToNdc.scale(2.0f / screenW, -2.0f / screenH, 1.0f);
        canvasToNdc.scale(zoom, zoom, 1.0f);
        canvasToNdc.translate(panX / zoom, panY / zoom, 0.0f);
        finalMat = canvasToNdc * combined;
    }

    SpriteXformPkt pkt;
    pkt.mat.row0 = { finalMat.row(0).x(), finalMat.row(0).y(), finalMat.row(0).z(), finalMat.row(0).w() };
    pkt.mat.row1 = { finalMat.row(1).x(), finalMat.row(1).y(), finalMat.row(1).z(), finalMat.row(1).w() };
    pkt.mat.row2 = { finalMat.row(2).x(), finalMat.row(2).y(), finalMat.row(2).z(), finalMat.row(2).w() };
    pkt.mat.row3 = { finalMat.row(3).x(), finalMat.row(3).y(), finalMat.row(3).z(), finalMat.row(3).w() };
    pkt.pSRV     = pSRV;
    pkt.opacity  = opacity;
    impl_->cmdBuf_->append(pkt);
}

} // namespace Artifact
