module;
#include <utility>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <vector>
#include <QImage>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QTransform>
#include <QDebug>
#include <QLoggingCategory>
#include <opencv2/opencv.hpp>
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
import Core.Transform.Viewport;
import Math.Bezier;
import Render.Shader.ThickLine;
import Render.Shader.ViewerHelpers;
import Image.ImageF32x4_RGBA;
import Color.Float;
import FloatRGBA;
import Font.FreeFont;
import Text.Style;
import Text.GlyphAtlas;
import Text.GlyphLayout;
import Utils.String.UniString;
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

qint64 computeImageContentKey(const auto& image)
{
    const float* data32 = image.rgba32fData();
    const std::uint8_t* data8 = image.rgba8Data();
    if (!data32 && !data8) {
        return 0;
    }

    const bool isFloat = data32 != nullptr;
    const size_t bytesPerChannel = isFloat ? sizeof(float) : sizeof(std::uint8_t);
    const size_t totalBytes = static_cast<size_t>(image.width()) * static_cast<size_t>(image.height()) * 4u * bytesPerChannel;
    const size_t sampleBytes = std::min<size_t>(totalBytes, 4096u);
    quint32 h = qHashMulti(0, image.width(), image.height(), 4, bytesPerChannel);
    const quint8* bytes = isFloat
        ? reinterpret_cast<const quint8*>(data32)
        : reinterpret_cast<const quint8*>(data8);
    for (size_t i = 0; i < sampleBytes; ++i) {
        h ^= static_cast<quint32>(bytes[i]);
        h *= 16777619u;
    }
    if (sampleBytes > 0) {
        h ^= static_cast<quint32>(bytes[sampleBytes - 1]);
        h *= 16777619u;
    }
    return static_cast<qint64>(h);
}

QImage imageToQImageAdapter(const ArtifactCore::ImageF32x4_RGBA& image)
{
    return image.toQImage();
}

TextStyle textStyleFromQFont(const QFont& font)
{
    TextStyle style;
    style.fontFamily = UniString(font.family());

    const float pointSize = font.pointSizeF() > 0.0f
                                ? static_cast<float>(font.pointSizeF())
                                : (font.pixelSize() > 0
                                       ? static_cast<float>(font.pixelSize())
                                       : 12.0f);
    style.fontSize = pointSize;
    style.pixelSize = pointSize;
    style.tracking = static_cast<float>(font.letterSpacing());
    style.fontWeight = font.bold() ? FontWeight::Bold : FontWeight::Normal;
    style.fontStyle = font.italic() ? FontStyle::Italic : FontStyle::Normal;
    style.allCaps = font.capitalization() == QFont::AllUppercase;
    style.underline = font.underline();
    style.strikethrough = font.strikeOut();
    return style;
}

ParagraphStyle paragraphStyleFromRectAndAlignment(const QRectF& rect,
                                                  Qt::Alignment alignment)
{
    ParagraphStyle paragraph;
    paragraph.boxWidth = static_cast<float>(rect.width());
    paragraph.boxHeight = static_cast<float>(rect.height());

    if (alignment & Qt::AlignHCenter) {
        paragraph.horizontalAlignment = TextHorizontalAlignment::Center;
    } else if (alignment & Qt::AlignRight) {
        paragraph.horizontalAlignment = TextHorizontalAlignment::Right;
    } else {
        paragraph.horizontalAlignment = TextHorizontalAlignment::Left;
    }

    if (alignment & Qt::AlignVCenter) {
        paragraph.verticalAlignment = TextVerticalAlignment::Middle;
    } else if (alignment & Qt::AlignBottom) {
        paragraph.verticalAlignment = TextVerticalAlignment::Bottom;
    } else {
        paragraph.verticalAlignment = TextVerticalAlignment::Top;
    }

    if ((static_cast<int>(alignment) & static_cast<int>(Qt::TextWordWrap)) != 0) {
        paragraph.wrapMode = TextWrapMode::WordWrap;
    } else {
        paragraph.wrapMode = TextWrapMode::NoWrap;
    }

    return paragraph;
}
} // namespace

Q_LOGGING_CATEGORY(primitiveRenderer2DLog, "artifact.primitiverenderer2d")

using namespace Diligent;
using namespace ArtifactCore;

class PrimitiveRenderer2D::Impl {
public:
    ISwapChain*                  pSwapChain_ = nullptr;
    RefCntAutoPtr<IRenderDevice> pDevice_;
    
    std::unique_ptr<GlyphAtlas>  pGlyphAtlas_;
    RefCntAutoPtr<ITexture>      pGlyphAtlasTexture_;

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
    float devicePixelRatio_ = 1.0f;

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
    
    // Initialize GlyphAtlas
    if (!impl_->pGlyphAtlas_) {
        impl_->pGlyphAtlas_ = std::make_unique<GlyphAtlas>();
    }
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
void PrimitiveRenderer2D::setDevicePixelRatio(float dpr)     { impl_->devicePixelRatio_ = dpr > 0.0f ? dpr : 1.0f; }
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
    if (impl_->cmdBuf_) {
        impl_->cmdBuf_->targetRTV = impl_->getCurrentRTV();
    }
}

void PrimitiveRenderer2D::setCommandBuffer(RenderCommandBuffer* cmdBuf)
{
    impl_->cmdBuf_ = cmdBuf;
}

ITextureView* PrimitiveRenderer2D::currentRTV() const
{
    return impl_->getCurrentRTV();
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

void PrimitiveRenderer2D::drawGradientRectTransformed(float x, float y, float w, float h,
                                                       const QMatrix4x4& transform,
                                                       const FloatColor& startColor,
                                                       const FloatColor& endColor,
                                                       int fillType,
                                                       float angleDegrees,
                                                       bool reverse,
                                                       float centerX,
                                                       float centerY,
                                                       float scale,
                                                       float offset,
                                                       float opacity)
{
    if (!impl_->cmdBuf_) return;
    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float screenW = std::max(viewportCB.screenSize.x, 0.001f);
    const float screenH = std::max(viewportCB.screenSize.y, 0.001f);
    const float zoom = std::max(viewportCB.zoom, 0.001f);
    const float panX = viewportCB.offset.x, panY = viewportCB.offset.y;

    QMatrix4x4 combined = transform;
    combined.translate(x, y, 0);
    combined.scale(w, h, 1.0f);
    QMatrix4x4 finalMat;
    if (impl_->useExternalMatrices_) {
        finalMat = impl_->externalProjMatrix_ * impl_->externalViewMatrix_ * combined;
    } else {
        QMatrix4x4 canvasToNdc;
        canvasToNdc.setToIdentity();
        canvasToNdc.translate(-1.0f, 1.0f, 0.0f);
        canvasToNdc.scale(2.0f / screenW, -2.0f / screenH, 1.0f);
        canvasToNdc.scale(zoom, zoom, 1.0f);
        canvasToNdc.translate(panX / zoom, panY / zoom, 0.0f);
        finalMat = canvasToNdc * combined;
    }

    GradientRectPkt pkt{};
    pkt.mat.row0 = {finalMat.row(0).x(), finalMat.row(0).y(), finalMat.row(0).z(), finalMat.row(0).w()};
    pkt.mat.row1 = {finalMat.row(1).x(), finalMat.row(1).y(), finalMat.row(1).z(), finalMat.row(1).w()};
    pkt.mat.row2 = {finalMat.row(2).x(), finalMat.row(2).y(), finalMat.row(2).z(), finalMat.row(2).w()};
    pkt.mat.row3 = {finalMat.row(3).x(), finalMat.row(3).y(), finalMat.row(3).z(), finalMat.row(3).w()};
    pkt.params.startColor = {startColor.r(), startColor.g(), startColor.b(), startColor.a()};
    pkt.params.endColor = {endColor.r(), endColor.g(), endColor.b(), endColor.a()};
    pkt.params.mode = {static_cast<float>(fillType), angleDegrees, reverse ? 1.0f : 0.0f, std::max(scale, 0.0001f)};
    pkt.params.centerOffset = {centerX, centerY, offset,
                               h > 0.0001f ? std::abs(w / h) : 1.0f};
    pkt.opacity = opacity;
    impl_->cmdBuf_->append(pkt);
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

    // Clamp to at least 1 screen pixel so lines remain visible when zoomed out.
    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float zoom = std::max(viewportCB.zoom, 0.001f);
    const float effectiveThickness = std::max(thickness, 1.0f / zoom);

    float2 d   = { p2.x - p1.x, p2.y - p1.y };
    float  len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-5f) return;

    float2 nd   = { d.x / len, d.y / len };
    float  half = effectiveThickness * 0.5f;
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

    // Grid thickness must cover at least 1 screen pixel at the current zoom.
    const float effectiveThickness = std::max(thickness, 1.0f / zoom);

    GridPkt pkt;
    pkt.xform.offset     = { x * zoom + viewportCB.offset.x, y * zoom + viewportCB.offset.y };
    pkt.xform.scale      = { w * zoom, h * zoom };
    pkt.xform.screenSize = viewportCB.screenSize;
    pkt.helper.param0    = spacing;
    pkt.helper.param1    = effectiveThickness;
    pkt.helper._pad[0]   = w;   // canvasSize.x (composition width)
    pkt.helper._pad[1]   = h;   // canvasSize.y (composition height)
    pkt.helper.color1    = { color.r(), color.g(), color.b(), color.a() };
    pkt.helper.color2    = {};
    pkt.baseColor        = { color.r(), color.g(), color.b(), color.a() };
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
    qCDebug(primitiveRenderer2DLog) << "drawSpriteLocal: enter" << "image.isNull()=" << image.isNull()
                                      << " pDevice?" << (impl_->pDevice_ != nullptr)
                                      << " cmdBuf?" << (impl_->cmdBuf_ != nullptr)
                                      << " overrideRTV?" << (impl_->m_overrideRTV != nullptr);
    if (!impl_->cmdBuf_ || image.isNull() || !impl_->pDevice_) {
        qCDebug(primitiveRenderer2DLog) << "drawSpriteLocal: early return" << "image.isNull()=" << image.isNull()
                                          << " pDevice?" << (impl_->pDevice_ != nullptr)
                                          << " cmdBuf?" << (impl_->cmdBuf_ != nullptr);
        return;
    }

    impl_->m_frameCount++;
    if (impl_->m_frameCount % 60 == 0) impl_->pruneCache();

    qint64 cacheKey = computeImageContentKey(image);
    RefCntAutoPtr<ITexture> pTexture;

    auto it = impl_->m_spriteTexCache.find(cacheKey);
    if (it != impl_->m_spriteTexCache.end()) {
        pTexture = it->second.pTexture;
        it->second.lastUsedFrame = impl_->m_frameCount;
        qCDebug(primitiveRenderer2DLog) << "drawSpriteLocal: cache hit for key" << cacheKey << "-> pTexture=" << (pTexture != nullptr);
    } else {
        qCDebug(primitiveRenderer2DLog) << "drawSpriteLocal: cache miss for key" << cacheKey;
        const QImage rgba = (image.format() == QImage::Format_RGBA8888)
                                ? image
                                : image.convertToFormat(QImage::Format_RGBA8888);
        const int imgW = rgba.width(), imgH = rgba.height();
        qCDebug(primitiveRenderer2DLog) << "drawSpriteLocal: creating texture" << imgW << "x" << imgH << " bytesPerLine=" << rgba.bytesPerLine();
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
        qCDebug(primitiveRenderer2DLog) << "drawSpriteLocal: CreateTexture returned pTexture=" << (pTexture != nullptr);
        if (!pTexture) {
            qCDebug(primitiveRenderer2DLog) << "drawSpriteLocal: texture creation failed";
            return;
        }
        impl_->m_spriteTexCache[cacheKey] = { pTexture, impl_->m_frameCount };
    }

    auto* pSRV = pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    if (!pSRV) {
        qCDebug(primitiveRenderer2DLog) << "drawSpriteLocal: GetDefaultView returned null SRV";
        return;
    }

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

void PrimitiveRenderer2D::drawText(const QRectF &rect, const QString &text,
                                   const QFont &font, const FloatColor &color,
                                   Qt::Alignment alignment, float opacity,
                                   const FloatColor &outlineColor, float outlineThickness)
{
    if (!impl_->cmdBuf_ || text.isEmpty() || rect.width() <= 0.0 || rect.height() <= 0.0) {
        return;
    }

    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float zoom = std::max(viewportCB.zoom, 0.001f);
    RenderSolidTransform2D xform{};
    xform.offset = {
        static_cast<float>(rect.x()) * zoom + viewportCB.offset.x,
        static_cast<float>(rect.y()) * zoom + viewportCB.offset.y
    };
    xform.scale = { zoom, zoom };
    xform.screenSize = viewportCB.screenSize;

    GlyphTextPkt pkt;
    pkt.rect             = rect;
    pkt.xform            = xform;
    pkt.text             = text;
    pkt.font             = font;
    pkt.color            = { color.r(), color.g(), color.b(), color.a() };
    pkt.outlineColor     = { outlineColor.r(), outlineColor.g(), outlineColor.b(), outlineColor.a() };
    pkt.alignment        = static_cast<int>(alignment);
    pkt.opacity          = opacity;
    pkt.outlineThickness = outlineThickness;
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawTextTransformed(const QRectF &rect, const QString &text,
                                              const QFont &font, const FloatColor &color,
                                              const QMatrix4x4 &transform,
                                              Qt::Alignment alignment, float opacity,
                                              const FloatColor &outlineColor, float outlineThickness)
{
    if (!impl_->cmdBuf_ || text.isEmpty() || rect.width() <= 0.0 || rect.height() <= 0.0) {
        return;
    }

    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float screenW = std::max(viewportCB.screenSize.x, 0.001f);
    const float screenH = std::max(viewportCB.screenSize.y, 0.001f);
    const float zoom = std::max(viewportCB.zoom, 0.001f);
    const float panX = viewportCB.offset.x, panY = viewportCB.offset.y;

    QMatrix4x4 finalMat;
    if (impl_->useExternalMatrices_) {
        QMatrix4x4 model = transform;
        model.translate(static_cast<float>(rect.x()), static_cast<float>(rect.y()), 0);
        finalMat = impl_->externalProjMatrix_ * impl_->externalViewMatrix_ * model;
    } else {
        QMatrix4x4 combined = transform;
        combined.translate(static_cast<float>(rect.x()), static_cast<float>(rect.y()), 0);
        QMatrix4x4 canvasToNdc;
        canvasToNdc.setToIdentity();
        canvasToNdc.translate(-1.0f, 1.0f, 0.0f);
        canvasToNdc.scale(2.0f / screenW, -2.0f / screenH, 1.0f);
        canvasToNdc.scale(zoom, zoom, 1.0f);
        canvasToNdc.translate(panX / zoom, panY / zoom, 0.0f);
        finalMat = canvasToNdc * combined;
    }

    GlyphTextXformPkt pkt;
    pkt.rect             = QRectF(0.0, 0.0, rect.width(), rect.height());
    pkt.transform.setToIdentity();
    pkt.transform        = finalMat;
    pkt.text             = text;
    pkt.font             = font;
    pkt.color            = { color.r(), color.g(), color.b(), color.a() };
    pkt.outlineColor     = { outlineColor.r(), outlineColor.g(), outlineColor.b(), outlineColor.a() };
    pkt.alignment        = static_cast<int>(alignment);
    pkt.opacity          = opacity;
    pkt.outlineThickness = outlineThickness;
    pkt.devicePixelRatio = impl_->devicePixelRatio_;
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
    qCDebug(primitiveRenderer2DLog) << "drawMaskedTextureLocal: enter" << "maskImage.isNull()=" << maskImage.isNull()
                                      << " sceneSRV?" << (sceneSRV != nullptr)
                                      << " pDevice?" << (impl_->pDevice_ != nullptr)
                                      << " cmdBuf?" << (impl_->cmdBuf_ != nullptr);
    if (!impl_->cmdBuf_ || !sceneSRV || maskImage.isNull() || !impl_->pDevice_) {
        qCDebug(primitiveRenderer2DLog) << "drawMaskedTextureLocal: early return" << "maskImage.isNull()=" << maskImage.isNull()
                                          << " sceneSRV?" << (sceneSRV != nullptr)
                                          << " pDevice?" << (impl_->pDevice_ != nullptr)
                                          << " cmdBuf?" << (impl_->cmdBuf_ != nullptr);
        return;
    }

    impl_->m_frameCount++;
    if (impl_->m_frameCount % 60 == 0) impl_->pruneCache();

    qint64 cacheKey = computeImageContentKey(maskImage);
    RefCntAutoPtr<ITexture> pMaskTexture;

    auto it = impl_->m_maskTexCache.find(cacheKey);
    if (it != impl_->m_maskTexCache.end()) {
        pMaskTexture = it->second.pTexture;
        it->second.lastUsedFrame = impl_->m_frameCount;
        qCDebug(primitiveRenderer2DLog) << "drawMaskedTextureLocal: cache hit for key" << cacheKey << "-> pMaskTexture=" << (pMaskTexture != nullptr);
    } else {
        qCDebug(primitiveRenderer2DLog) << "drawMaskedTextureLocal: cache miss for key" << cacheKey;
        const QImage rgba = (maskImage.format() == QImage::Format_RGBA8888)
                                ? maskImage
                                : maskImage.convertToFormat(QImage::Format_RGBA8888);
        const int imgW = rgba.width(), imgH = rgba.height();
        qCDebug(primitiveRenderer2DLog) << "drawMaskedTextureLocal: creating mask texture" << imgW << "x" << imgH << " bytesPerLine=" << rgba.bytesPerLine();
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
        qCDebug(primitiveRenderer2DLog) << "drawMaskedTextureLocal: CreateTexture returned pMaskTexture=" << (pMaskTexture != nullptr);
        if (!pMaskTexture) {
            qCDebug(primitiveRenderer2DLog) << "drawMaskedTextureLocal: mask texture creation failed";
            return;
        }
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
    qCDebug(primitiveRenderer2DLog) << "drawSpriteTransformed(Tx): enter" << "image.isNull()=" << image.isNull()
                                      << " pDevice?" << (impl_->pDevice_ != nullptr)
                                      << " cmdBuf?" << (impl_->cmdBuf_ != nullptr);
    if (!impl_->cmdBuf_ || image.isNull() || !impl_->pDevice_) {
        qCDebug(primitiveRenderer2DLog) << "drawSpriteTransformed(Tx): early return" << "image.isNull()=" << image.isNull()
                                          << " pDevice?" << (impl_->pDevice_ != nullptr)
                                          << " cmdBuf?" << (impl_->cmdBuf_ != nullptr);
        return;
    }

    impl_->m_frameCount++;
    if (impl_->m_frameCount % 60 == 0) impl_->pruneCache();

    qint64 cacheKey = computeImageContentKey(image);
    RefCntAutoPtr<ITexture> pTexture;

    auto it = impl_->m_spriteTexCache.find(cacheKey);
    if (it != impl_->m_spriteTexCache.end()) {
        pTexture = it->second.pTexture;
        it->second.lastUsedFrame = impl_->m_frameCount;
    } else {
        const QImage rgba = (image.format() == QImage::Format_RGBA8888)
                                ? image
                                : image.convertToFormat(QImage::Format_RGBA8888);
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

void PrimitiveRenderer2D::drawSpriteTransformed(float x, float y, float w, float h, const QMatrix4x4& transform, const QImage& image, float opacity, const QRectF& uvRect)
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
        qCDebug(primitiveRenderer2DLog) << "drawSpriteTransformed(Tx): cache hit for key" << cacheKey << "-> pTexture=" << (pTexture != nullptr);
    } else {
        qCDebug(primitiveRenderer2DLog) << "drawSpriteTransformed(Tx): cache miss for key" << cacheKey;
        const QImage rgba = (image.format() == QImage::Format_RGBA8888)
                                ? image : image.convertToFormat(QImage::Format_RGBA8888);
        const int imgW = rgba.width(), imgH = rgba.height();
        qCDebug(primitiveRenderer2DLog) << "drawSpriteTransformed(Tx): creating texture" << imgW << "x" << imgH << " bytesPerLine=" << rgba.bytesPerLine();
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
        qCDebug(primitiveRenderer2DLog) << "drawSpriteTransformed(Tx): CreateTexture returned newTex=" << (newTex != nullptr);
        if (!newTex) {
            qCDebug(primitiveRenderer2DLog) << "drawSpriteTransformed(Tx): texture creation failed";
            return;
        }
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

    const QRectF clampedUv = uvRect.normalized().intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    if (!clampedUv.isValid() || clampedUv.width() <= 0.0 || clampedUv.height() <= 0.0) {
        return;
    }

    AtlasSpriteXformPkt pkt;
    pkt.mat.row0 = { finalMat.row(0).x(), finalMat.row(0).y(), finalMat.row(0).z(), finalMat.row(0).w() };
    pkt.mat.row1 = { finalMat.row(1).x(), finalMat.row(1).y(), finalMat.row(1).z(), finalMat.row(1).w() };
    pkt.mat.row2 = { finalMat.row(2).x(), finalMat.row(2).y(), finalMat.row(2).z(), finalMat.row(2).w() };
    pkt.mat.row3 = { finalMat.row(3).x(), finalMat.row(3).y(), finalMat.row(3).z(), finalMat.row(3).w() };
    pkt.pSRV     = pSRV;
    pkt.uvRect = {static_cast<float>(clampedUv.left()),
                  static_cast<float>(clampedUv.top()),
                  static_cast<float>(clampedUv.right()),
                  static_cast<float>(clampedUv.bottom())};
    pkt.color = {1.0f, 1.0f, 1.0f, opacity};
    impl_->cmdBuf_->append(pkt);
}

void PrimitiveRenderer2D::drawSpriteTransformed(float x, float y, float w, float h, const QMatrix4x4& transform, const ArtifactCore::ImageF32x4_RGBA& image, float opacity, const QRectF& uvRect)
{
    if (!impl_->cmdBuf_ || image.isEmpty() || !impl_->pDevice_) {
        return;
    }

    impl_->m_frameCount++;
    if (impl_->m_frameCount % 60 == 0) {
        impl_->pruneCache();
    }

    const qint64 cacheKey = computeImageContentKey(image);
    RefCntAutoPtr<ITexture> pTexture;

    auto it = impl_->m_spriteTexCache.find(cacheKey);
    if (it != impl_->m_spriteTexCache.end()) {
        pTexture = it->second.pTexture;
        it->second.lastUsedFrame = impl_->m_frameCount;
    } else {
        const int width = image.width();
        const int height = image.height();
        const float* rgba32 = image.rgba32fData();
        const std::uint8_t* rgba8 = image.rgba8Data();
        if (width <= 0 || height <= 0 || (!rgba32 && !rgba8)) {
            return;
        }

        std::vector<uint8_t> uploadBytes;
        const uint8_t* uploadPtr = nullptr;
        if (rgba8) {
            uploadPtr = rgba8;
        } else {
            uploadBytes.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
            const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
            for (size_t i = 0; i < pixelCount; ++i) {
                // Source layout from ImageF32x4_RGBA internal mat: [B, G, R, A] as float
                const float srcB = rgba32[i * 4u + 0];
                const float srcG = rgba32[i * 4u + 1];
                const float srcR = rgba32[i * 4u + 2];
                const float srcA = rgba32[i * 4u + 3];
                // Dest layout: [R, G, B, A] as uint8 (TEX_FORMAT_RGBA8_UNORM_SRGB)
                uploadBytes[i * 4u + 0] = static_cast<uint8_t>(std::clamp(srcR, 0.0f, 1.0f) * 255.0f + 0.5f);
                uploadBytes[i * 4u + 1] = static_cast<uint8_t>(std::clamp(srcG, 0.0f, 1.0f) * 255.0f + 0.5f);
                uploadBytes[i * 4u + 2] = static_cast<uint8_t>(std::clamp(srcB, 0.0f, 1.0f) * 255.0f + 0.5f);
                uploadBytes[i * 4u + 3] = static_cast<uint8_t>(std::clamp(srcA, 0.0f, 1.0f) * 255.0f + 0.5f);
            }
            uploadPtr = uploadBytes.data();
        }

        TextureDesc texDesc;
        texDesc.Type = RESOURCE_DIM_TEX_2D;
        texDesc.Width = static_cast<Uint32>(width);
        texDesc.Height = static_cast<Uint32>(height);
        texDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
        texDesc.MipLevels = 1;
        texDesc.Usage = USAGE_IMMUTABLE;
        texDesc.BindFlags = BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = CPU_ACCESS_NONE;

        TextureSubResData subData;
        subData.pData = uploadPtr;
        subData.Stride = static_cast<Uint64>(width) * 4ull;
        TextureData initData;
        initData.pSubResources = &subData;
        initData.NumSubresources = 1;

        RefCntAutoPtr<ITexture> newTex;
        impl_->pDevice_->CreateTexture(texDesc, &initData, &newTex);
        if (!newTex) {
            return;
        }
        impl_->m_spriteTexCache[cacheKey] = { newTex, impl_->m_frameCount };
        pTexture = newTex;
    }

    auto* pSRV = pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    if (!pSRV) {
        return;
    }

    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float screenW = std::max(viewportCB.screenSize.x, 0.001f);
    const float screenH = std::max(viewportCB.screenSize.y, 0.001f);
    const float zoom = std::max(viewportCB.zoom, 0.001f);
    const float panX = viewportCB.offset.x;
    const float panY = viewportCB.offset.y;

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

    const QRectF clampedUv = uvRect.normalized().intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    if (!clampedUv.isValid() || clampedUv.width() <= 0.0 || clampedUv.height() <= 0.0) {
        return;
    }

    AtlasSpriteXformPkt pkt;
    pkt.mat.row0 = { finalMat.row(0).x(), finalMat.row(0).y(), finalMat.row(0).z(), finalMat.row(0).w() };
    pkt.mat.row1 = { finalMat.row(1).x(), finalMat.row(1).y(), finalMat.row(1).z(), finalMat.row(1).w() };
    pkt.mat.row2 = { finalMat.row(2).x(), finalMat.row(2).y(), finalMat.row(2).z(), finalMat.row(2).w() };
    pkt.mat.row3 = { finalMat.row(3).x(), finalMat.row(3).y(), finalMat.row(3).z(), finalMat.row(3).w() };
    pkt.pSRV = pSRV;
    pkt.uvRect = {static_cast<float>(clampedUv.left()),
                  static_cast<float>(clampedUv.top()),
                  static_cast<float>(clampedUv.right()),
                  static_cast<float>(clampedUv.bottom())};
    pkt.color = {1.0f, 1.0f, 1.0f, opacity};
    impl_->cmdBuf_->append(pkt);
}

// WP-3: GPU GlyphAtlas based text rendering
void PrimitiveRenderer2D::drawGlyphText(float x, float y, const UniString& text,
                                        const TextStyle& style,
                                        const FloatColor& color,
                                        float opacity)
{
    if (!impl_->pGlyphAtlas_ || text.length() == 0 || !impl_->cmdBuf_) return;

    const auto codePoints = text.toStdU32String();
    for (const char32_t codePoint : codePoints) {
        const QString glyphText = QString::fromUcs4(&codePoint, 1);
        const QFont resolvedFont = FontManager::makeFont(style, glyphText);
        GlyphKey key;
        key.codePoint = codePoint;
        key.fontSize = style.fontSize;
        key.fontFamily = resolvedFont.family().toStdString();
        key.styleFlags = (static_cast<uint32_t>(style.fontWeight) << 1) |
                         static_cast<uint32_t>(style.fontStyle);
        impl_->pGlyphAtlas_->acquire(key, resolvedFont);
    }
    
    // Manage GPU texture for GlyphAtlas
    if (impl_->pGlyphAtlas_->isDirty() || !impl_->pGlyphAtlasTexture_) {
        const QImage& img = impl_->pGlyphAtlas_->atlasImage();
        if (!img.isNull() && impl_->pDevice_) {
            TextureDesc texDesc;
            texDesc.Name           = "GlyphAtlasTexture";
            texDesc.Type           = RESOURCE_DIM_TEX_2D;
            texDesc.Width          = img.width();
            texDesc.Height         = img.height();
            texDesc.MipLevels      = 1;
            texDesc.Format         = TEX_FORMAT_RGBA8_UNORM;
            texDesc.Usage          = USAGE_IMMUTABLE;
            texDesc.BindFlags      = BIND_SHADER_RESOURCE;

            TextureSubResData subData;
            subData.pData  = img.constBits();
            subData.Stride = img.bytesPerLine();

            TextureData initData;
            initData.pSubResources = &subData;
            initData.NumSubresources = 1;

            impl_->pGlyphAtlasTexture_.Release(); // Release old texture
            impl_->pDevice_->CreateTexture(texDesc, &initData, &impl_->pGlyphAtlasTexture_);
            impl_->pGlyphAtlas_->clearDirty();
        }
    }
    
    if (!impl_->pGlyphAtlasTexture_) return;
    auto* pSRV = impl_->pGlyphAtlasTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    if (!pSRV) return;

    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float zoom = std::max(viewportCB.zoom, 0.001f);
    const float atlasW = static_cast<float>(impl_->pGlyphAtlas_->width());
    const float atlasH = static_cast<float>(impl_->pGlyphAtlas_->height());
    
    float currentX = x;
    for (const char32_t codePoint : codePoints) {
        const QString glyphText = QString::fromUcs4(&codePoint, 1);
        const QFont qfont = FontManager::makeFont(style, glyphText);
        GlyphKey key;
        key.codePoint = codePoint;
        key.fontSize = style.fontSize;
        key.fontFamily = qfont.family().toStdString();
        key.styleFlags = (static_cast<uint32_t>(style.fontWeight) << 1) |
                         (static_cast<uint32_t>(style.fontStyle) << 0);
        
        GlyphRect rect = impl_->pGlyphAtlas_->acquire(key, qfont);
        if (!rect.valid) continue;
        
        AtlasSpritePkt pkt;
        pkt.pSRV = pSRV;
        pkt.uvRect = { rect.u0(static_cast<int>(atlasW)), rect.v0(static_cast<int>(atlasH)), 
                       rect.u1(static_cast<int>(atlasW)), rect.v1(static_cast<int>(atlasH)) };
        pkt.color = { color.r(), color.g(), color.b(), opacity };

        // Position & Scale setup
        const float gw = static_cast<float>(rect.width);
        const float gh = static_cast<float>(rect.height);
        const float gx = currentX + rect.bearingX;
        const float gy = y - rect.bearingY;
        
        if (impl_->useExternalMatrices_) {
            // Simplified handling for external matrices (should really use Xform variant)
            pkt.xform.offset = { gx, gy };
            pkt.xform.scale  = { gw, gh };
            pkt.xform.screenSize = viewportCB.screenSize;
        } else {
            // Standard canvas zoom & pan
            pkt.xform.offset = { (gx * zoom) + viewportCB.offset.x, (gy * zoom) + viewportCB.offset.y };
            pkt.xform.scale  = { gw * zoom, gh * zoom };
            pkt.xform.screenSize = viewportCB.screenSize;
        }
        
        impl_->cmdBuf_->append(pkt);
        currentX += rect.advance;
    }
}

void PrimitiveRenderer2D::drawGlyphTextTransformed(float x, float y, const UniString& text,
                                                    const TextStyle& style,
                                                    const FloatColor& color,
                                                    const QMatrix4x4& transform,
                                                    float opacity)
{
    if (!impl_->pGlyphAtlas_ || text.length() == 0 || !impl_->cmdBuf_) return;

    // Build the packet the GPU path (DiligentImmediateSubmitter::submitGlyphTextTransformed)
    // consumes. It owns the full shaping / atlas acquire / outline loop, so we only need to
    // translate our high-level TextStyle/FloatColor/UniString inputs into its QFont/QString/
    // float4/QRectF payload. (x,y) anchor the paragraph box origin; the box size is taken
    // from the source text length as a generous single-line width estimate so shaping has room.
    GlyphTextXformPkt pkt;
    pkt.text     = text.toQString();
    pkt.font     = QFont(QString::fromStdString(style.fontFamily));
    pkt.font.setPointSizeF(style.fontSize);
    pkt.font.setBold(style.fontWeight == FontWeight::Bold);
    pkt.font.setItalic(style.fontStyle == FontStyle::Italic);
    pkt.color    = { color.r(), color.g(), color.b(), opacity };
    pkt.opacity  = opacity;
    pkt.transform = transform;

    // Paragraph box: origin at (x,y), width sized to the text so the shaper does not wrap.
    // The submitter applies `transform` on top of the shaped glyph positions, so passing
    // canvas-space coordinates here is correct for 3D-projected / rotated text.
    QFontMetricsF fm(pkt.font);
    const qreal boxW = fm.horizontalAdvance(pkt.text) + 4.0;
    const qreal boxH = fm.height() + 2.0;
    pkt.rect = QRectF(static_cast<qreal>(x), static_cast<qreal>(y), boxW, boxH);

    // devicePixelRatio is used by the submitter to convert atlas pixels (rendered at the
    // style's font size) back to device-independent units. Mirror the value used elsewhere.
    pkt.devicePixelRatio = impl_->devicePixelRatio_ > 0.0f ? impl_->devicePixelRatio_ : 1.0f;

    impl_->cmdBuf_->append(std::move(pkt));
}

void PrimitiveRenderer2D::drawGlyphs(std::span<const GlyphItem> glyphs,
                                     const TextStyle& style,
                                     const FloatColor& color,
                                     float opacity)
{
    if (!impl_->pGlyphAtlas_ || glyphs.empty() || !impl_->cmdBuf_) return;

    for (const GlyphItem& glyph : glyphs) {
        const QString glyphText = QString::fromUcs4(&glyph.charCode, 1);
        const QFont resolvedFont = FontManager::makeFont(style, glyphText);
        GlyphKey key;
        key.codePoint = glyph.charCode;
        key.fontSize = style.fontSize;
        key.fontFamily = resolvedFont.family().toStdString();
        key.styleFlags = (static_cast<uint32_t>(style.fontWeight) << 1) |
                         static_cast<uint32_t>(style.fontStyle);
        impl_->pGlyphAtlas_->acquire(key, resolvedFont);
    }

    // Pre-laid-out glyph path: callers (text animators, hand-shaped runs) supply GlyphItems
    // whose basePosition/offsetPosition already encode final placement. We acquire each glyph
    // from the atlas and emit an AtlasSpritePkt — the same primitive drawGlyphText() uses —
    // so no extra GPU PSO is needed. This keeps animated text on the fast 2D path.
    if (impl_->pGlyphAtlas_->isDirty() || !impl_->pGlyphAtlasTexture_) {
        const QImage& img = impl_->pGlyphAtlas_->atlasImage();
        if (!img.isNull() && impl_->pDevice_) {
            TextureDesc texDesc;
            texDesc.Name           = "GlyphAtlasTexture";
            texDesc.Type           = RESOURCE_DIM_TEX_2D;
            texDesc.Width          = img.width();
            texDesc.Height         = img.height();
            texDesc.MipLevels      = 1;
            texDesc.Format         = TEX_FORMAT_RGBA8_UNORM;
            texDesc.Usage          = USAGE_IMMUTABLE;
            texDesc.BindFlags      = BIND_SHADER_RESOURCE;

            TextureSubResData subData;
            subData.pData  = img.constBits();
            subData.Stride = img.bytesPerLine();

            TextureData initData;
            initData.pSubResources = &subData;
            initData.NumSubresources = 1;

            impl_->pGlyphAtlasTexture_.Release();
            impl_->pDevice_->CreateTexture(texDesc, &initData, &impl_->pGlyphAtlasTexture_);
            impl_->pGlyphAtlas_->clearDirty();
        }
    }
    if (!impl_->pGlyphAtlasTexture_) return;
    auto* pSRV = impl_->pGlyphAtlasTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    if (!pSRV) return;

    const auto viewportCB = impl_->viewport_.GetViewportCB();
    const float zoom = std::max(viewportCB.zoom, 0.001f);
    const float atlasW = static_cast<float>(impl_->pGlyphAtlas_->width());
    const float atlasH = static_cast<float>(impl_->pGlyphAtlas_->height());

    for (const GlyphItem& glyph : glyphs) {
        const QString glyphText = QString::fromUcs4(&glyph.charCode, 1);
        const QFont qfont = FontManager::makeFont(style, glyphText);
        GlyphKey key;
        key.codePoint = glyph.charCode;
        key.fontSize   = style.fontSize;
        key.fontFamily = qfont.family().toStdString();
        key.styleFlags = (static_cast<uint32_t>(style.fontWeight) << 1) |
                         (static_cast<uint32_t>(style.fontStyle) << 0);

        const GlyphRect rect = impl_->pGlyphAtlas_->acquire(key, qfont);
        if (!rect.valid) continue;

        // basePosition (line origin from layout) + offsetPosition (per-glyph animation offset)
        // give the final pen position in canvas space.
        const float penX = static_cast<float>(glyph.basePosition.x() + glyph.offsetPosition.x());
        const float penY = static_cast<float>(glyph.basePosition.y() + glyph.offsetPosition.y());
        const float gx = penX + rect.bearingX;
        const float gy = penY - rect.bearingY;
        const float gw = static_cast<float>(rect.width);
        const float gh = static_cast<float>(rect.height);

        AtlasSpritePkt pkt;
        pkt.pSRV = pSRV;
        pkt.uvRect = { rect.u0(static_cast<int>(atlasW)), rect.v0(static_cast<int>(atlasH)),
                       rect.u1(static_cast<int>(atlasW)), rect.v1(static_cast<int>(atlasH)) };
        // Per-glyph opacity animation folds into alpha; color comes from the run.
        const float a = std::clamp(opacity * static_cast<float>(glyph.offsetOpacity), 0.0f, 1.0f);
        pkt.color = { color.r(), color.g(), color.b(), a };

        if (impl_->useExternalMatrices_) {
            pkt.xform.offset    = { gx, gy };
            pkt.xform.scale     = { gw, gh };
            pkt.xform.screenSize = viewportCB.screenSize;
        } else {
            pkt.xform.offset    = { (gx * zoom) + viewportCB.offset.x,
                                    (gy * zoom) + viewportCB.offset.y };
            pkt.xform.scale     = { gw * zoom, gh * zoom };
            pkt.xform.screenSize = viewportCB.screenSize;
        }

        impl_->cmdBuf_->append(pkt);
    }
}

void PrimitiveRenderer2D::drawGlyphsTransformed(
    std::span<const GlyphItem> glyphs,
    const TextStyle& style,
    const FloatColor& color,
    const QMatrix4x4& transform,
    const QPointF& origin,
    float opacity,
    const FloatColor& outlineColor,
    float outlineThickness,
    float blurRadius,
    bool useGlyphColorOverrides)
{
    if (!impl_->pGlyphAtlas_ || glyphs.empty() || !impl_->cmdBuf_ ||
        !impl_->pDevice_) {
        return;
    }

    struct ResolvedGlyph {
        const GlyphItem* item = nullptr;
        GlyphRect rect;
    };

    std::vector<ResolvedGlyph> resolvedGlyphs;
    resolvedGlyphs.reserve(glyphs.size());
    for (const GlyphItem& glyph : glyphs) {
        const QString glyphText = QString::fromUcs4(&glyph.charCode, 1);
        if (glyphText.isEmpty() || glyphText.at(0).isSpace()) {
            continue;
        }

        const QFont resolvedFont = FontManager::makeFont(style, glyphText);
        GlyphKey key;
        key.codePoint = glyph.charCode;
        key.fontSize = style.fontSize;
        key.fontFamily = resolvedFont.family().toStdString();
        key.styleFlags = (static_cast<uint32_t>(style.fontWeight) << 1) |
                         static_cast<uint32_t>(style.fontStyle);
        const GlyphRect rect = impl_->pGlyphAtlas_->acquire(key, resolvedFont);
        if (rect.valid) {
            resolvedGlyphs.push_back({&glyph, rect});
        }
    }
    if (resolvedGlyphs.empty()) {
        return;
    }

    if (impl_->pGlyphAtlas_->isDirty() || !impl_->pGlyphAtlasTexture_) {
        const QImage& image = impl_->pGlyphAtlas_->atlasImage();
        if (image.isNull()) {
            return;
        }

        TextureDesc textureDesc;
        textureDesc.Name = "GlyphAtlasTexture";
        textureDesc.Type = RESOURCE_DIM_TEX_2D;
        textureDesc.Width = image.width();
        textureDesc.Height = image.height();
        textureDesc.MipLevels = 1;
        textureDesc.Format = TEX_FORMAT_RGBA8_UNORM;
        textureDesc.Usage = USAGE_IMMUTABLE;
        textureDesc.BindFlags = BIND_SHADER_RESOURCE;

        TextureSubResData subresource;
        subresource.pData = image.constBits();
        subresource.Stride = image.bytesPerLine();
        TextureData initialData;
        initialData.pSubResources = &subresource;
        initialData.NumSubresources = 1;

        impl_->pGlyphAtlasTexture_.Release();
        impl_->pDevice_->CreateTexture(textureDesc, &initialData,
                                       &impl_->pGlyphAtlasTexture_);
        if (impl_->pGlyphAtlasTexture_) {
            impl_->pGlyphAtlas_->clearDirty();
        }
    }

    if (!impl_->pGlyphAtlasTexture_) {
        return;
    }
    ITextureView* atlasView =
        impl_->pGlyphAtlasTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    if (!atlasView) {
        return;
    }

    const auto viewport = impl_->viewport_.GetViewportCB();
    const float screenWidth = std::max(viewport.screenSize.x, 0.001f);
    const float screenHeight = std::max(viewport.screenSize.y, 0.001f);
    const float zoom = std::max(viewport.zoom, 0.001f);
    const float atlasWidth = static_cast<float>(impl_->pGlyphAtlas_->width());
    const float atlasHeight = static_cast<float>(impl_->pGlyphAtlas_->height());

    const auto blendColor = [](const FloatColor& base,
                               const FloatRGBA& overrideColor,
                               float weight) {
        const float t = std::clamp(weight, 0.0f, 1.0f);
        return FloatColor(
            base.r() + (overrideColor.r() - base.r()) * t,
            base.g() + (overrideColor.g() - base.g()) * t,
            base.b() + (overrideColor.b() - base.b()) * t,
            base.a() + (overrideColor.a() - base.a()) * t);
    };

    const auto appendGlyphPacket =
        [&](const GlyphItem& glyph, const GlyphRect& rect,
            const FloatColor& packetColor, float xOffset, float yOffset) {
            const float width = static_cast<float>(rect.width);
            const float height = static_cast<float>(rect.height);
            if (width <= 0.0f || height <= 0.0f) {
                return;
            }

            const float penX = static_cast<float>(
                glyph.basePosition.x() + glyph.offsetPosition.x());
            const float penY = static_cast<float>(
                glyph.basePosition.y() + glyph.offsetPosition.y());
            const float left = static_cast<float>(origin.x()) + penX +
                               rect.bearingX + xOffset;
            const float top = static_cast<float>(origin.y()) + penY -
                              rect.bearingY + yOffset;
            const float glyphScale = std::max(
                0.0001f, glyph.baseScale * glyph.offsetScale);

            QMatrix4x4 model = transform;
            model.translate(left, top, glyph.offsetZ);
            model.translate(width * 0.5f, height * 0.5f, 0.0f);
            if (std::abs(glyph.offsetSkew) > 0.0001f) {
                QMatrix4x4 skew;
                skew.setToIdentity();
                skew(0, 1) = std::tan(
                    glyph.offsetSkew * std::numbers::pi_v<float> / 180.0f);
                model *= skew;
            }
            model.rotate(glyph.baseRotation + glyph.offsetRotation,
                         0.0f, 0.0f, 1.0f);
            model.scale(glyphScale, glyphScale, 1.0f);
            model.translate(-width * 0.5f, -height * 0.5f, 0.0f);
            model.scale(width, height, 1.0f);

            QMatrix4x4 finalMatrix;
            if (impl_->useExternalMatrices_) {
                finalMatrix = impl_->externalProjMatrix_ *
                              impl_->externalViewMatrix_ * model;
            } else {
                QMatrix4x4 canvasToNdc;
                canvasToNdc.setToIdentity();
                canvasToNdc.translate(-1.0f, 1.0f, 0.0f);
                canvasToNdc.scale(2.0f / screenWidth,
                                  -2.0f / screenHeight, 1.0f);
                canvasToNdc.scale(zoom, zoom, 1.0f);
                canvasToNdc.translate(viewport.offset.x / zoom,
                                      viewport.offset.y / zoom, 0.0f);
                finalMatrix = canvasToNdc * model;
            }

            const float alpha = std::clamp(
                opacity * glyph.offsetOpacity * packetColor.a(), 0.0f, 1.0f);
            AtlasSpriteXformPkt packet;
            packet.mat.row0 = {finalMatrix.row(0).x(), finalMatrix.row(0).y(),
                               finalMatrix.row(0).z(), finalMatrix.row(0).w()};
            packet.mat.row1 = {finalMatrix.row(1).x(), finalMatrix.row(1).y(),
                               finalMatrix.row(1).z(), finalMatrix.row(1).w()};
            packet.mat.row2 = {finalMatrix.row(2).x(), finalMatrix.row(2).y(),
                               finalMatrix.row(2).z(), finalMatrix.row(2).w()};
            packet.mat.row3 = {finalMatrix.row(3).x(), finalMatrix.row(3).y(),
                               finalMatrix.row(3).z(), finalMatrix.row(3).w()};
            packet.pSRV = atlasView;
            packet.uvRect = {
                rect.u0(static_cast<int>(atlasWidth)),
                rect.v0(static_cast<int>(atlasHeight)),
                rect.u1(static_cast<int>(atlasWidth)),
                rect.v1(static_cast<int>(atlasHeight))};
            packet.color = {packetColor.r(), packetColor.g(), packetColor.b(),
                            alpha};
            impl_->cmdBuf_->append(packet);
        };

    constexpr float diagonal = 0.7071067811865476f;
    for (const ResolvedGlyph& resolved : resolvedGlyphs) {
        const GlyphItem& glyph = *resolved.item;
        FloatColor fill = color;
        if (useGlyphColorOverrides && glyph.hasColorOverride) {
            fill = blendColor(fill, glyph.fillColorOverride,
                              glyph.fillColorOverrideWeight);
        }

        FloatColor stroke = outlineColor;
        if (useGlyphColorOverrides && glyph.hasStrokeOverride) {
            stroke = blendColor(stroke, glyph.strokeColorOverride,
                                glyph.strokeColorOverrideWeight);
        }
        const float strokeWidth = std::max(
            0.0f, outlineThickness + glyph.offsetStrokeWidth);
        if (strokeWidth > 0.01f && stroke.a() > 0.0f) {
            const std::array<QPointF, 8> offsets = {
                QPointF(strokeWidth, 0.0f), QPointF(-strokeWidth, 0.0f),
                QPointF(0.0f, strokeWidth), QPointF(0.0f, -strokeWidth),
                QPointF(strokeWidth * diagonal, strokeWidth * diagonal),
                QPointF(-strokeWidth * diagonal, strokeWidth * diagonal),
                QPointF(strokeWidth * diagonal, -strokeWidth * diagonal),
                QPointF(-strokeWidth * diagonal, -strokeWidth * diagonal)};
            for (const QPointF& offset : offsets) {
                appendGlyphPacket(glyph, resolved.rect, stroke,
                                  static_cast<float>(offset.x()),
                                  static_cast<float>(offset.y()));
            }
        }
        const float resolvedBlur =
            std::max(0.0f, blurRadius + glyph.offsetBlur);
        if (resolvedBlur > 0.1f) {
            const float radius = std::min(resolvedBlur, 32.0f);
            FloatColor tapColor(fill.r(), fill.g(), fill.b(),
                                fill.a() / 9.0f);
            const std::array<QPointF, 9> blurOffsets = {
                QPointF(0.0f, 0.0f), QPointF(radius, 0.0f),
                QPointF(-radius, 0.0f), QPointF(0.0f, radius),
                QPointF(0.0f, -radius),
                QPointF(radius * diagonal, radius * diagonal),
                QPointF(-radius * diagonal, radius * diagonal),
                QPointF(radius * diagonal, -radius * diagonal),
                QPointF(-radius * diagonal, -radius * diagonal)};
            for (const QPointF& offset : blurOffsets) {
                appendGlyphPacket(glyph, resolved.rect, tapColor,
                                  static_cast<float>(offset.x()),
                                  static_cast<float>(offset.y()));
            }
        } else {
            appendGlyphPacket(glyph, resolved.rect, fill, 0.0f, 0.0f);
        }
    }
}

QString PrimitiveRenderer2D::glyphAtlasDebugState() const
{
    if (!impl_->pGlyphAtlas_) {
        return QStringLiteral("<no atlas>");
    }
    return impl_->pGlyphAtlas_->debugState();
}

} // namespace Artifact
