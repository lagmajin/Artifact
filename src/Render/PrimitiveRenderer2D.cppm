module;
#include <array>
#include <cmath>
#include <cstring>
#include <QImage>
#include <QPointF>
#include <QTransform>
#include <QDebug>
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

namespace Artifact {

using namespace Diligent;
using namespace ArtifactCore;

class PrimitiveRenderer2D::Impl {
public:
    IDeviceContext* pCtx_ = nullptr;
    ISwapChain*     pSwapChain_ = nullptr;

    RefCntAutoPtr<IBuffer> m_draw_sprite_vertex_buffer;
    RefCntAutoPtr<IBuffer> m_draw_sprite_index_buffer;
    RefCntAutoPtr<IBuffer> m_draw_sprite_cb;
    RefCntAutoPtr<IBuffer> m_draw_sprite_transform_matrix_cb;

    RefCntAutoPtr<IBuffer> m_draw_solid_rect_vertex_buffer;
    RefCntAutoPtr<IBuffer> m_draw_solid_rect_cb;
    RefCntAutoPtr<IBuffer> m_draw_solid_rect_trnsform_cb;
    RefCntAutoPtr<IBuffer> m_draw_solid_rect_transform_matrix_cb;
    RefCntAutoPtr<IBuffer> m_draw_solid_rect_index_buffer;

    RefCntAutoPtr<IBuffer> m_draw_thick_line_vertex_buffer;
    RefCntAutoPtr<IBuffer> m_draw_solid_triangle_vertex_buffer;
    RefCntAutoPtr<IBuffer> m_draw_dot_line_vertex_buffer;
    RefCntAutoPtr<IBuffer> m_draw_dot_line_cb;
    RefCntAutoPtr<IBuffer> m_draw_viewer_helper_cb;

    PSOAndSRB m_draw_sprite_pso_and_srb;
    PSOAndSRB m_draw_sprite_transform_pso_and_srb;
    RefCntAutoPtr<ISampler> m_sprite_sampler;
    RefCntAutoPtr<IRenderDevice> pDevice_;

    struct CachedTexture {
        RefCntAutoPtr<ITexture> pTexture;
        qint64 lastUsedFrame = 0;
    };
    std::unordered_map<qint64, CachedTexture> m_spriteTexCache;
    qint64 m_frameCount = 0;

    void pruneCache() {
        if (m_spriteTexCache.size() > 50) {
            for (auto it = m_spriteTexCache.begin(); it != m_spriteTexCache.end(); ) {
                if (it->second.lastUsedFrame < m_frameCount - 60) {
                    it = m_spriteTexCache.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    PSOAndSRB m_draw_solid_rect_pso_and_srb;
    PSOAndSRB m_draw_solid_rect_transform_pso_and_srb;
    PSOAndSRB m_draw_line_pso_and_srb;
    PSOAndSRB m_draw_thick_line_pso_and_srb;
    PSOAndSRB m_draw_dot_line_pso_and_srb;
    PSOAndSRB m_draw_solid_triangle_pso_and_srb;
    PSOAndSRB m_draw_checkerboard_pso_and_srb;
    PSOAndSRB m_draw_grid_pso_and_srb;
    PSOAndSRB m_draw_rect_outline_pso_and_srb;

    ViewportTransformer viewport_;

    ITextureView* m_overrideRTV = nullptr;
    ITextureView* getCurrentRTV() const {
        if (m_overrideRTV) return m_overrideRTV;
        return pSwapChain_ ? pSwapChain_->GetCurrentBackBufferRTV() : nullptr;
    }
    bool hasRenderTarget() const { return m_overrideRTV != nullptr || pSwapChain_ != nullptr; }
};

PrimitiveRenderer2D::PrimitiveRenderer2D()
    : impl_(new Impl())
{
}

PrimitiveRenderer2D::~PrimitiveRenderer2D()
{
    delete impl_;
}

void PrimitiveRenderer2D::setContext(IDeviceContext* ctx, ISwapChain* swapChain)
{
    impl_->pCtx_       = ctx;
    impl_->pSwapChain_ = swapChain;
}

void PrimitiveRenderer2D::setPSOs(ShaderManager& shaderManager)
{
    impl_->m_draw_sprite_pso_and_srb           = shaderManager.spritePsoAndSrb();
    impl_->m_draw_sprite_transform_pso_and_srb = shaderManager.spriteTransformPsoAndSrb();
    impl_->m_sprite_sampler                    = shaderManager.spriteSampler();
    impl_->m_draw_solid_rect_pso_and_srb       = shaderManager.solidRectPsoAndSrb();
    impl_->m_draw_solid_rect_transform_pso_and_srb = shaderManager.solidRectTransformPsoAndSrb();
    impl_->m_draw_line_pso_and_srb            = shaderManager.linePsoAndSrb();
    impl_->m_draw_thick_line_pso_and_srb      = shaderManager.thickLinePsoAndSrb();
    impl_->m_draw_dot_line_pso_and_srb        = shaderManager.dotLinePsoAndSrb();
    impl_->m_draw_solid_triangle_pso_and_srb  = shaderManager.solidTrianglePsoAndSrb();
    impl_->m_draw_checkerboard_pso_and_srb    = shaderManager.checkerboardPsoAndSrb();
    impl_->m_draw_grid_pso_and_srb            = shaderManager.gridPsoAndSrb();
    impl_->m_draw_rect_outline_pso_and_srb    = shaderManager.outlinePsoAndSrb();
}

void PrimitiveRenderer2D::createBuffers(RefCntAutoPtr<IRenderDevice> device, TEXTURE_FORMAT /*rtvFormat*/)
{
    if (!device) return;
    impl_->pDevice_ = device;

    {
        BufferDesc VertDesc;
        VertDesc.Name           = "Sprite vertex buffer";
        VertDesc.Usage          = USAGE_DYNAMIC;
        VertDesc.BindFlags      = BIND_VERTEX_BUFFER;
        VertDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        VertDesc.Size           = sizeof(SpriteVertex) * 4;
        device->CreateBuffer(VertDesc, nullptr, &impl_->m_draw_sprite_vertex_buffer);
    }

    {
        BufferDesc CBDesc;
        CBDesc.Name           = "SpriteCB";
        CBDesc.Size           = sizeof(DrawSpriteConstants);
        CBDesc.Usage          = USAGE_DYNAMIC;
        CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        device->CreateBuffer(CBDesc, nullptr, &impl_->m_draw_sprite_cb);
    }

    {
        BufferDesc CBDesc;
        CBDesc.Name           = "DrawSolidColorCB";
        CBDesc.Usage          = USAGE_DYNAMIC;
        CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        CBDesc.Size           = sizeof(DrawSpriteConstants);
        device->CreateBuffer(CBDesc, nullptr, &impl_->m_draw_solid_rect_cb);
    }

    {
        BufferDesc CBDesc;
        CBDesc.Name           = "DrawSolidTransformCB";
        CBDesc.Usage          = USAGE_DYNAMIC;
        CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        CBDesc.Size           = sizeof(CBSolidTransform2D);
        device->CreateBuffer(CBDesc, nullptr, &impl_->m_draw_solid_rect_trnsform_cb);
    }

    {
        BufferDesc CBDesc;
        CBDesc.Name           = "DrawSolidRectTransformMatrixCB";
        CBDesc.Usage          = USAGE_DYNAMIC;
        CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        CBDesc.Size           = sizeof(CBSolidRectTransform2D);
        device->CreateBuffer(CBDesc, nullptr, &impl_->m_draw_solid_rect_transform_matrix_cb);
    }

    {
        BufferDesc CBDesc;
        CBDesc.Name           = "DrawSpriteTransformMatrixCB";
        CBDesc.Usage          = USAGE_DYNAMIC;
        CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        CBDesc.Size           = sizeof(CBSolidRectTransform2D);
        device->CreateBuffer(CBDesc, nullptr, &impl_->m_draw_sprite_transform_matrix_cb);
    }

    {
        BufferDesc vbDesc;
        vbDesc.Name           = "SolidRect Vertex Buffer";
        vbDesc.BindFlags      = BIND_VERTEX_BUFFER;
        vbDesc.Usage          = USAGE_DYNAMIC;
        vbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        vbDesc.Size           = sizeof(RectVertex) * 4;
        device->CreateBuffer(vbDesc, nullptr, &impl_->m_draw_solid_rect_vertex_buffer);

        vbDesc.Name = "SolidTriangle Vertex Buffer";
        vbDesc.Size = sizeof(RectVertex) * 3;
        device->CreateBuffer(vbDesc, nullptr, &impl_->m_draw_solid_triangle_vertex_buffer);
    }

    {
        uint32_t indices[6] = { 0, 1, 2, 2, 1, 3 };
        BufferDesc IndexBufferDesc;
        IndexBufferDesc.Name           = "SolidRectIndexBuffer";
        IndexBufferDesc.Usage          = USAGE_DEFAULT;
        IndexBufferDesc.BindFlags      = BIND_INDEX_BUFFER;
        IndexBufferDesc.Size           = sizeof(indices);
        IndexBufferDesc.CPUAccessFlags = CPU_ACCESS_NONE;
        BufferData InitData;
        InitData.pData    = indices;
        InitData.DataSize = sizeof(indices);
        device->CreateBuffer(IndexBufferDesc, &InitData, &impl_->m_draw_solid_rect_index_buffer);
    }

    {
        BufferDesc vbDesc;
        vbDesc.Name           = "ThickLine Vertex Buffer";
        vbDesc.BindFlags      = BIND_VERTEX_BUFFER;
        vbDesc.Usage          = USAGE_DYNAMIC;
        vbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        vbDesc.Size           = sizeof(RectVertex) * 4;
        device->CreateBuffer(vbDesc, nullptr, &impl_->m_draw_thick_line_vertex_buffer);
    }

    {
        BufferDesc vbDesc;
        vbDesc.Name           = "DotLine Vertex Buffer";
        vbDesc.BindFlags      = BIND_VERTEX_BUFFER;
        vbDesc.Usage          = USAGE_DYNAMIC;
        vbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        vbDesc.Size           = sizeof(DotLineVertex) * 4;
        device->CreateBuffer(vbDesc, nullptr, &impl_->m_draw_dot_line_vertex_buffer);
    }

    {
        struct DotLineShaderCB { float thickness; float spacing; float2 padding; };
        BufferDesc CBDesc;
        CBDesc.Name           = "DotLineCB";
        CBDesc.Usage          = USAGE_DYNAMIC;
        CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        CBDesc.Size           = sizeof(DotLineShaderCB);
        device->CreateBuffer(CBDesc, nullptr, &impl_->m_draw_dot_line_cb);
    }

    {
        struct CBViewerHelper { float v1; float v2; float2 p; float4 c1; float4 c2; };
        BufferDesc CBDesc;
        CBDesc.Name           = "ViewerHelperCB";
        CBDesc.Usage          = USAGE_DYNAMIC;
        CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        CBDesc.Size           = sizeof(CBViewerHelper);
        device->CreateBuffer(CBDesc, nullptr, &impl_->m_draw_viewer_helper_cb);
    }
}

void PrimitiveRenderer2D::destroy()
{
    impl_->m_draw_sprite_vertex_buffer          = nullptr;
    impl_->m_draw_sprite_index_buffer           = nullptr;
    impl_->m_draw_sprite_cb                     = nullptr;
    impl_->m_draw_sprite_transform_matrix_cb    = nullptr;
    impl_->m_draw_solid_rect_vertex_buffer      = nullptr;
    impl_->m_draw_solid_rect_cb                 = nullptr;
    impl_->m_draw_solid_rect_trnsform_cb        = nullptr;
    impl_->m_draw_solid_rect_transform_matrix_cb = nullptr;
    impl_->m_draw_solid_rect_index_buffer       = nullptr;
    impl_->m_draw_thick_line_vertex_buffer      = nullptr;
    impl_->m_draw_solid_triangle_vertex_buffer  = nullptr;
    impl_->m_draw_dot_line_vertex_buffer        = nullptr;
    impl_->m_draw_dot_line_cb                   = nullptr;
    impl_->m_draw_viewer_helper_cb              = nullptr;

    impl_->m_draw_sprite_pso_and_srb         = {};
    impl_->m_draw_sprite_transform_pso_and_srb = {};
    impl_->m_sprite_sampler                  = nullptr;
    impl_->m_spriteCacheKey                  = 0;
    impl_->m_spriteTexCache                  = nullptr;
    impl_->pDevice_                          = nullptr;
    impl_->m_draw_solid_rect_pso_and_srb     = {};
    impl_->m_draw_solid_rect_transform_pso_and_srb = {};
    impl_->m_draw_line_pso_and_srb           = {};
    impl_->m_draw_thick_line_pso_and_srb     = {};
    impl_->m_draw_dot_line_pso_and_srb       = {};
    impl_->m_draw_solid_triangle_pso_and_srb = {};
    impl_->m_draw_checkerboard_pso_and_srb   = {};
    impl_->m_draw_grid_pso_and_srb           = {};
    impl_->m_draw_rect_outline_pso_and_srb   = {};

    impl_->pCtx_        = nullptr;
    impl_->pSwapChain_  = nullptr;
    impl_->m_overrideRTV = nullptr;
}

void PrimitiveRenderer2D::setViewportSize(float w, float h)  { impl_->viewport_.SetViewportSize(w, h); }
void PrimitiveRenderer2D::setCanvasSize(float w, float h)    { impl_->viewport_.SetCanvasSize(w, h); }
void PrimitiveRenderer2D::setPan(float x, float y)           { impl_->viewport_.SetPan(x, y); }
void PrimitiveRenderer2D::setZoom(float zoom)                { impl_->viewport_.SetZoom(zoom); }
float PrimitiveRenderer2D::getZoom() const                   { return impl_->viewport_.GetZoom(); }
void PrimitiveRenderer2D::panBy(float dx, float dy)          { impl_->viewport_.PanBy(dx, dy); }
void PrimitiveRenderer2D::resetView()                        { impl_->viewport_.ResetView(); }
void PrimitiveRenderer2D::fitToViewport(float margin)        { impl_->viewport_.FitCanvasToViewport(margin); }

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

void PrimitiveRenderer2D::clear(const FloatColor& color)
{
    if (!impl_->hasRenderTarget() || !impl_->pCtx_) return;
    float clearColor[] = { color.r(), color.g(), color.b(), color.a() };
    auto* pRTV = impl_->getCurrentRTV();
    impl_->pCtx_->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    impl_->pCtx_->ClearRenderTarget(pRTV, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void PrimitiveRenderer2D::drawRectLocal(float x, float y, float w, float h, const FloatColor& color, float opacity)
{
    if (!impl_->hasRenderTarget() || !impl_->m_draw_solid_rect_pso_and_srb.pPSO) return;

    float alpha = color.a() * opacity;
    RectVertex vertices[4] = {
        {{0.0f, 0.0f}, {color.r(), color.g(), color.b(), alpha}},
        {{1.0f, 0.0f}, {color.r(), color.g(), color.b(), alpha}},
        {{0.0f, 1.0f}, {color.r(), color.g(), color.b(), alpha}},
        {{1.0f, 1.0f}, {color.r(), color.g(), color.b(), alpha}},
    };
    
    auto* pRTV = impl_->getCurrentRTV();
    impl_->pCtx_->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_rect_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, vertices, sizeof(vertices));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_rect_vertex_buffer, MAP_WRITE);
    }

    {
        CBSolidColor cb = { {color.r(), color.g(), color.b(), alpha} };
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_rect_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, &cb, sizeof(cb));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_rect_cb, MAP_WRITE);
    }

    {
        auto viewportCB = impl_->viewport_.GetViewportCB();
        const float zoom = std::max(viewportCB.zoom, 0.001f);
        CBSolidTransform2D cbTransform;
        cbTransform.offset     = { x * zoom + viewportCB.offset.x, y * zoom + viewportCB.offset.y };
        cbTransform.scale      = { w * zoom, h * zoom };
        cbTransform.screenSize = viewportCB.screenSize;
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_rect_trnsform_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, &cbTransform, sizeof(cbTransform));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_rect_trnsform_cb, MAP_WRITE);
    }

    impl_->pCtx_->SetPipelineState(impl_->m_draw_solid_rect_pso_and_srb.pPSO);

    IBuffer* pBuffers[] = { impl_->m_draw_solid_rect_vertex_buffer };
    Uint64 offsets[] = { 0 };
    impl_->pCtx_->SetVertexBuffers(0, 1, pBuffers, offsets,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    impl_->pCtx_->SetIndexBuffer(impl_->m_draw_solid_rect_index_buffer, 0,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    impl_->m_draw_solid_rect_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(impl_->m_draw_solid_rect_trnsform_cb);
    impl_->m_draw_solid_rect_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "ColorBuffer")->Set(impl_->m_draw_solid_rect_cb);
    impl_->pCtx_->CommitShaderResources(impl_->m_draw_solid_rect_pso_and_srb.pSRB,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs drawAttrs(6, VT_UINT32, DRAW_FLAG_VERIFY_ALL);
    impl_->pCtx_->DrawIndexed(drawAttrs);
}

void PrimitiveRenderer2D::drawSolidRectTransformed(float x, float y, float w, float h, const QTransform& transform, const FloatColor& color, float opacity)
{
    if (!impl_->hasRenderTarget() || !impl_->m_draw_solid_rect_transform_pso_and_srb.pPSO) return;

    float alpha = color.a() * opacity;
    RectVertex vertices[4] = {
        {{0.0f, 0.0f}, {color.r(), color.g(), color.b(), alpha}},
        {{1.0f, 0.0f}, {color.r(), color.g(), color.b(), alpha}},
        {{0.0f, 1.0f}, {color.r(), color.g(), color.b(), alpha}},
        {{1.0f, 1.0f}, {color.r(), color.g(), color.b(), alpha}},
    };

    auto* pRTV = impl_->getCurrentRTV();
    impl_->pCtx_->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_rect_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, vertices, sizeof(vertices));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_rect_vertex_buffer, MAP_WRITE);
    }

    {
        auto viewportCB = impl_->viewport_.GetViewportCB();
        const float screenW = std::max(viewportCB.screenSize.x, 0.001f);
        const float screenH = std::max(viewportCB.screenSize.y, 0.001f);
        const float zoom = std::max(viewportCB.zoom, 0.001f);
        const float panX = viewportCB.offset.x;
        const float panY = viewportCB.offset.y;

        const float a  = transform.m11();
        const float b  = transform.m12();
        const float c  = transform.m21();
        const float d  = transform.m22();
        const float tx = transform.dx();
        const float ty = transform.dy();
        
        const float localX = a * x + c * y + tx;
        const float localY = b * x + d * y + ty;

        CBSolidRectTransform2D cbTransform;
        cbTransform.row0 = {
            2.0f * (a * w * zoom) / screenW,
            2.0f * (c * h * zoom) / screenW,
            0.0f,
            2.0f * ((localX * zoom) + panX) / screenW - 1.0f
        };
        cbTransform.row1 = {
            -2.0f * (b * w * zoom) / screenH,
            -2.0f * (d * h * zoom) / screenH,
            0.0f,
            1.0f - 2.0f * ((localY * zoom) + panY) / screenH
        };
        cbTransform.row2 = { 0.0f, 0.0f, 0.0f, 0.0f };
        cbTransform.row3 = { 0.0f, 0.0f, 0.0f, 1.0f };

        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_rect_transform_matrix_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, &cbTransform, sizeof(cbTransform));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_rect_transform_matrix_cb, MAP_WRITE);
    }

    impl_->pCtx_->SetPipelineState(impl_->m_draw_solid_rect_transform_pso_and_srb.pPSO);

    IBuffer* pBuffers[] = { impl_->m_draw_solid_rect_vertex_buffer };
    Uint64 offsets[] = { 0 };
    impl_->pCtx_->SetVertexBuffers(0, 1, pBuffers, offsets,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    impl_->pCtx_->SetIndexBuffer(impl_->m_draw_solid_rect_index_buffer, 0,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    impl_->m_draw_solid_rect_transform_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(impl_->m_draw_solid_rect_transform_matrix_cb);
    impl_->m_draw_solid_rect_transform_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "ColorBuffer")->Set(impl_->m_draw_solid_rect_cb);
    impl_->pCtx_->CommitShaderResources(impl_->m_draw_solid_rect_transform_pso_and_srb.pSRB,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs drawAttrs(6, VT_UINT32, DRAW_FLAG_VERIFY_ALL);
    impl_->pCtx_->DrawIndexed(drawAttrs);
}

void PrimitiveRenderer2D::drawSolidRect(float x, float y, float w, float h, const FloatColor& color, float opacity)
{
    this->drawRectLocal(x, y, w, h, color, opacity);
}

void PrimitiveRenderer2D::drawLineLocal(float2 p1, float2 p2, const FloatColor& c1, const FloatColor& c2)
{
    if (!impl_->hasRenderTarget() || !impl_->m_draw_line_pso_and_srb.pPSO) return;

    LineVertex vertices[2] = {
        {{p1.x, p1.y}, {c1.r(), c1.g(), c1.b(), 1}},
        {{p2.x, p2.y}, {c2.r(), c2.g(), c2.b(), 1}},
    };

    auto* pRTV = impl_->getCurrentRTV();
    impl_->pCtx_->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_rect_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, vertices, sizeof(vertices));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_rect_vertex_buffer, MAP_WRITE);
    }

    {
        auto viewportCB = impl_->viewport_.GetViewportCB();
        const float zoom = std::max(viewportCB.zoom, 0.001f);
        CBSolidTransform2D cbTransform;
        cbTransform.offset     = viewportCB.offset;
        cbTransform.scale      = { zoom, zoom };
        cbTransform.screenSize = viewportCB.screenSize;
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_rect_trnsform_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, &cbTransform, sizeof(cbTransform));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_rect_trnsform_cb, MAP_WRITE);
    }

    impl_->pCtx_->SetPipelineState(impl_->m_draw_line_pso_and_srb.pPSO);

    IBuffer* pBuffers[] = { impl_->m_draw_solid_rect_vertex_buffer };
    Uint64 offsets[] = { 0 };
    impl_->pCtx_->SetVertexBuffers(0, 1, pBuffers, offsets,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

    impl_->m_draw_line_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(impl_->m_draw_solid_rect_trnsform_cb);
    impl_->pCtx_->CommitShaderResources(impl_->m_draw_line_pso_and_srb.pSRB,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 2;
    drawAttrs.Flags       = DRAW_FLAG_VERIFY_ALL;
    impl_->pCtx_->Draw(drawAttrs);
}

void PrimitiveRenderer2D::drawThickLineLocal(float2 p1, float2 p2, float thickness, const FloatColor& color)
{
    if (!impl_->hasRenderTarget() || !impl_->m_draw_thick_line_pso_and_srb.pPSO) return;
    if (thickness <= 0.0f) return;

    float2 d   = { p2.x - p1.x, p2.y - p1.y };
    float  len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-5f) return;

    float2 nd   = { d.x / len, d.y / len };
    float  half = thickness * 0.5f;
    float2 n    = { -nd.y * half, nd.x * half };

    float4 c = { color.r(), color.g(), color.b(), color.a() };
    RectVertex vertices[4] = {
        { { p1.x + n.x, p1.y + n.y }, c },
        { { p1.x - n.x, p1.y - n.y }, c },
        { { p2.x + n.x, p2.y + n.y }, c },
        { { p2.x - n.x, p2.y - n.y }, c },
    };

    auto* pRTV = impl_->getCurrentRTV();
    impl_->pCtx_->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_thick_line_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, vertices, sizeof(vertices));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_thick_line_vertex_buffer, MAP_WRITE);
    }

    {
        auto viewportCB = impl_->viewport_.GetViewportCB();
        const float zoom = std::max(viewportCB.zoom, 0.001f);
        CBSolidTransform2D cbTransform;
        cbTransform.offset     = viewportCB.offset;
        cbTransform.scale      = { zoom, zoom };
        cbTransform.screenSize = viewportCB.screenSize;
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_rect_trnsform_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, &cbTransform, sizeof(cbTransform));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_rect_trnsform_cb, MAP_WRITE);
    }

    impl_->pCtx_->SetPipelineState(impl_->m_draw_thick_line_pso_and_srb.pPSO);

    IBuffer* pBuffers[] = { impl_->m_draw_thick_line_vertex_buffer };
    Uint64 offsets[] = { 0 };
    impl_->pCtx_->SetVertexBuffers(0, 1, pBuffers, offsets,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

    impl_->m_draw_thick_line_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(impl_->m_draw_solid_rect_trnsform_cb);
    impl_->pCtx_->CommitShaderResources(impl_->m_draw_thick_line_pso_and_srb.pSRB,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_VERIFY_ALL;
    impl_->pCtx_->Draw(drawAttrs);
}

void PrimitiveRenderer2D::drawDotLineLocal(float2 p1, float2 p2, float thickness, float spacing, const FloatColor& color)
{
    if (!impl_->hasRenderTarget() || !impl_->m_draw_dot_line_pso_and_srb.pPSO) return;
    if (thickness <= 0.0f) return;

    float2 d   = { p2.x - p1.x, p2.y - p1.y };
    float  len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-5f) return;

    float2 nd   = { d.x / len, d.y / len };
    float  half = thickness * 0.5f;
    float2 n    = { -nd.y * half, nd.x * half };

    float4 c = { color.r(), color.g(), color.b(), color.a() };
    DotLineVertex vertices[4] = {
        { { p1.x + n.x, p1.y + n.y }, c, 0.0f },
        { { p1.x - n.x, p1.y - n.y }, c, 0.0f },
        { { p2.x + n.x, p2.y + n.y }, c, len  },
        { { p2.x - n.x, p2.y - n.y }, c, len  },
    };

    auto* pRTV = impl_->getCurrentRTV();
    impl_->pCtx_->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_dot_line_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, vertices, sizeof(vertices));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_dot_line_vertex_buffer, MAP_WRITE);
    }

    {
        auto viewportCB = impl_->viewport_.GetViewportCB();
        const float zoom = std::max(viewportCB.zoom, 0.001f);
        CBSolidTransform2D cbTransform;
        cbTransform.offset     = viewportCB.offset;
        cbTransform.scale      = { zoom, zoom };
        cbTransform.screenSize = viewportCB.screenSize;
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_rect_trnsform_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, &cbTransform, sizeof(cbTransform));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_rect_trnsform_cb, MAP_WRITE);
    }

    {
        struct DotLineShaderCB { float thickness; float spacing; float2 padding; };
        DotLineShaderCB cb = { thickness, spacing, {0, 0} };
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_dot_line_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, &cb, sizeof(cb));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_dot_line_cb, MAP_WRITE);
    }

    impl_->pCtx_->SetPipelineState(impl_->m_draw_dot_line_pso_and_srb.pPSO);

    IBuffer* pBuffers[] = { impl_->m_draw_dot_line_vertex_buffer };
    Uint64 offsets[] = { 0 };
    impl_->pCtx_->SetVertexBuffers(0, 1, pBuffers, offsets,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

    impl_->m_draw_dot_line_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(impl_->m_draw_solid_rect_trnsform_cb);
    impl_->m_draw_dot_line_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "DotLineCB")->Set(impl_->m_draw_dot_line_cb);
    impl_->pCtx_->CommitShaderResources(impl_->m_draw_dot_line_pso_and_srb.pSRB,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_VERIFY_ALL;
    impl_->pCtx_->Draw(drawAttrs);
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
    if (!impl_->hasRenderTarget() || !impl_->m_draw_solid_triangle_pso_and_srb.pPSO) return;

    float4 c = { color.r(), color.g(), color.b(), color.a() };
    RectVertex vertices[3] = {
        {{p0.x, p0.y}, c},
        {{p1.x, p1.y}, c},
        {{p2.x, p2.y}, c},
    };

    auto* pRTV = impl_->getCurrentRTV();
    impl_->pCtx_->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_triangle_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, vertices, sizeof(vertices));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_triangle_vertex_buffer, MAP_WRITE);
    }

    {
        auto viewportCB = impl_->viewport_.GetViewportCB();
        const float zoom = std::max(viewportCB.zoom, 0.001f);
        CBSolidTransform2D cbTransform;
        cbTransform.offset     = viewportCB.offset;
        cbTransform.scale      = { zoom, zoom };
        cbTransform.screenSize = viewportCB.screenSize;
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_rect_trnsform_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, &cbTransform, sizeof(cbTransform));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_rect_trnsform_cb, MAP_WRITE);
    }

    impl_->pCtx_->SetPipelineState(impl_->m_draw_solid_triangle_pso_and_srb.pPSO);

    IBuffer* pBuffers[] = { impl_->m_draw_solid_triangle_vertex_buffer };
    Uint64 offsets[] = { 0 };
    impl_->pCtx_->SetVertexBuffers(0, 1, pBuffers, offsets,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

    impl_->m_draw_solid_triangle_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(impl_->m_draw_solid_rect_trnsform_cb);
    impl_->pCtx_->CommitShaderResources(impl_->m_draw_solid_triangle_pso_and_srb.pSRB,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 3;
    drawAttrs.Flags       = DRAW_FLAG_VERIFY_ALL;
    impl_->pCtx_->Draw(drawAttrs);
}

void PrimitiveRenderer2D::drawCircle(float x, float y, float radius, const FloatColor& color, float thickness, bool fill)
{
    if (radius <= 0.0f) return;

    if (fill) {
        // Simple approximation: draw many circles to fill
        for (float r = 0.5f; r <= radius; r += 1.0f) {
            drawCircle(x, y, r, color, 1.5f, false);
        }
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

void PrimitiveRenderer2D::drawCrosshair(float x, float y, float size, const FloatColor& color)
{
    const float half = size * 0.5f;
    drawThickLineLocal({x - half, y}, {x + half, y}, 1.5f, color);
    drawThickLineLocal({x, y - half}, {x, y + half}, 1.5f, color);
}

void PrimitiveRenderer2D::drawPoint(float x, float y, float size, const FloatColor& color)
{
    const float half = size * 0.5f;
    this->drawRectLocal(x - half, y - half, size, size, color, 1.0f);
}

void PrimitiveRenderer2D::drawCheckerboard(float x, float y, float w, float h, float tileSize, const FloatColor& c1, const FloatColor& c2)
{
    if (!impl_->hasRenderTarget() || !impl_->m_draw_checkerboard_pso_and_srb.pPSO) return;

    RectVertex vertices[4] = {
        {{0.0f, 0.0f}, {1,1,1,1}},
        {{1.0f, 0.0f}, {1,1,1,1}},
        {{0.0f, 1.0f}, {1,1,1,1}},
        {{1.0f, 1.0f}, {1,1,1,1}},
    };

    auto* pRTV = impl_->getCurrentRTV();
    impl_->pCtx_->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_rect_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, vertices, sizeof(vertices));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_rect_vertex_buffer, MAP_WRITE);
    }

    {
        struct CBViewerHelper { float tileSize; float thickness; float2 padding; float4 color1; float4 color2; };
        CBViewerHelper cb;
        cb.tileSize = tileSize;
        cb.color1   = { c1.r(), c1.g(), c1.b(), c1.a() };
        cb.color2   = { c2.r(), c2.g(), c2.b(), c2.a() };
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_viewer_helper_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, &cb, sizeof(cb));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_viewer_helper_cb, MAP_WRITE);
    }

    {
        auto viewportCB = impl_->viewport_.GetViewportCB();
        const float zoom = std::max(viewportCB.zoom, 0.001f);
        CBSolidTransform2D cbTransform;
        cbTransform.offset     = { x * zoom + viewportCB.offset.x, y * zoom + viewportCB.offset.y };
        cbTransform.scale      = { w * zoom, h * zoom };
        cbTransform.screenSize = viewportCB.screenSize;
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_rect_trnsform_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, &cbTransform, sizeof(cbTransform));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_rect_trnsform_cb, MAP_WRITE);
    }

    impl_->pCtx_->SetPipelineState(impl_->m_draw_checkerboard_pso_and_srb.pPSO);

    IBuffer* pBuffers[] = { impl_->m_draw_solid_rect_vertex_buffer };
    Uint64 offsets[] = { 0 };
    impl_->pCtx_->SetVertexBuffers(0, 1, pBuffers, offsets,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

    impl_->m_draw_checkerboard_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(impl_->m_draw_solid_rect_trnsform_cb);
    impl_->m_draw_checkerboard_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "ViewerHelperCB")->Set(impl_->m_draw_viewer_helper_cb);
    impl_->pCtx_->CommitShaderResources(impl_->m_draw_checkerboard_pso_and_srb.pSRB,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_VERIFY_ALL;
    impl_->pCtx_->Draw(drawAttrs);
}

void PrimitiveRenderer2D::drawGrid(float x, float y, float w, float h,
    float spacing, float thickness, const FloatColor& color)
{
    if (!impl_->hasRenderTarget() || !impl_->m_draw_grid_pso_and_srb.pPSO) return;

    RectVertex vertices[4] = {
        {{0.0f, 0.0f}, {1,1,1,1}},
        {{1.0f, 0.0f}, {1,1,1,1}},
        {{0.0f, 1.0f}, {1,1,1,1}},
        {{1.0f, 1.0f}, {1,1,1,1}},
    };

    auto* pRTV = impl_->getCurrentRTV();
    impl_->pCtx_->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_rect_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, vertices, sizeof(vertices));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_rect_vertex_buffer, MAP_WRITE);
    }

    {
        struct CBViewerHelper { float spacing; float thickness; float2 padding; float4 color1; float4 color2; };
        CBViewerHelper cb;
        cb.spacing   = spacing;
        cb.thickness = thickness;
        cb.color1    = { color.r(), color.g(), color.b(), 1.0f };
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_viewer_helper_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, &cb, sizeof(cb));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_viewer_helper_cb, MAP_WRITE);
    }

    {
        auto viewportCB = impl_->viewport_.GetViewportCB();
        const float zoom = std::max(viewportCB.zoom, 0.001f);
        CBSolidTransform2D cbTransform;
        cbTransform.offset     = { x * zoom + viewportCB.offset.x, y * zoom + viewportCB.offset.y };
        cbTransform.scale      = { w * zoom, h * zoom };
        cbTransform.screenSize = viewportCB.screenSize;
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_rect_trnsform_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, &cbTransform, sizeof(cbTransform));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_rect_trnsform_cb, MAP_WRITE);
    }

    impl_->pCtx_->SetPipelineState(impl_->m_draw_grid_pso_and_srb.pPSO);

    IBuffer* pBuffers[] = { impl_->m_draw_solid_rect_vertex_buffer };
    Uint64 offsets[] = { 0 };
    impl_->pCtx_->SetVertexBuffers(0, 1, pBuffers, offsets,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

    impl_->m_draw_grid_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(impl_->m_draw_solid_rect_trnsform_cb);
    impl_->m_draw_grid_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "ViewerHelperCB")->Set(impl_->m_draw_viewer_helper_cb);
    impl_->pCtx_->CommitShaderResources(impl_->m_draw_grid_pso_and_srb.pSRB,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_VERIFY_ALL;
    impl_->pCtx_->Draw(drawAttrs);
}

void PrimitiveRenderer2D::drawRectOutlineLocal(float x, float y, float w, float h, const FloatColor& color)
{
    if (!impl_->hasRenderTarget() || !impl_->m_draw_rect_outline_pso_and_srb.pPSO) return;

    RectVertex vertices[4] = {
        {{0, 0}, {color.r(), color.g(), color.b(), color.a()}},
        {{w, 0}, {color.r(), color.g(), color.b(), color.a()}},
        {{w, h}, {color.r(), color.g(), color.b(), color.a()}},
        {{0, h}, {color.r(), color.g(), color.b(), color.a()}},
    };

    {
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_solid_rect_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, vertices, sizeof(vertices));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_solid_rect_vertex_buffer, MAP_WRITE);
    }

    auto* pRTV = impl_->getCurrentRTV();
    impl_->pCtx_->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    impl_->pCtx_->SetPipelineState(impl_->m_draw_rect_outline_pso_and_srb.pPSO);
    impl_->pCtx_->CommitShaderResources(impl_->m_draw_rect_outline_pso_and_srb.pSRB,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs drawAttrs(8, VT_UINT32, DRAW_FLAG_VERIFY_ALL);
    impl_->pCtx_->DrawIndexed(drawAttrs);
}

void PrimitiveRenderer2D::drawSpriteLocal(float x, float y, float w, float h, const QImage& image, float opacity)
{
    if (!impl_->hasRenderTarget() || !impl_->m_draw_sprite_pso_and_srb.pPSO) return;
    if (image.isNull()) return;
    if (!impl_->pDevice_ || !impl_->pCtx_) return;

    impl_->m_frameCount++;
    if (impl_->m_frameCount % 60 == 0) {
        impl_->pruneCache();
    }

    qint64 cacheKey = image.cacheKey();
    RefCntAutoPtr<ITexture> pTexture;
    
    auto it = impl_->m_spriteTexCache.find(cacheKey);
    if (it != impl_->m_spriteTexCache.end()) {
        pTexture = it->second.pTexture;
        it->second.lastUsedFrame = impl_->m_frameCount;
    } else {
        const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
        const int imgW = rgba.width();
        const int imgH = rgba.height();
        if (imgW <= 0 || imgH <= 0) return;

        TextureDesc texDesc;
        texDesc.Type             = RESOURCE_DIM_TEX_2D;
        texDesc.Width            = static_cast<Uint32>(imgW);
        texDesc.Height           = static_cast<Uint32>(imgH);
        texDesc.Format           = TEX_FORMAT_RGBA8_UNORM_SRGB;
        texDesc.MipLevels        = 1;
        texDesc.Usage            = USAGE_IMMUTABLE;
        texDesc.BindFlags        = BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags   = CPU_ACCESS_NONE;

        TextureSubResData subData;
        subData.pData  = rgba.constBits();
        subData.Stride = static_cast<Uint64>(rgba.bytesPerLine());
        TextureData initData;
        initData.pSubResources   = &subData;
        initData.NumSubresources = 1;

        impl_->pDevice_->CreateTexture(texDesc, &initData, &pTexture);
        if (!pTexture) return;

        impl_->m_spriteTexCache[cacheKey] = { pTexture, impl_->m_frameCount };
    }

    auto* pSRV = pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    if (!pSRV) return;

    auto* pRTV = impl_->getCurrentRTV();
    if (!pRTV) return;

    SpriteVertex vertices[4] = {
        { {0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, opacity} },
        { {1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, opacity} },
        { {0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, opacity} },
        { {1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, opacity} },
    };
    impl_->pCtx_->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_sprite_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, vertices, sizeof(vertices));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_sprite_vertex_buffer, MAP_WRITE);
    }

    {
        auto viewportCB = impl_->viewport_.GetViewportCB();
        const float zoom = std::max(viewportCB.zoom, 0.001f);
        CBSolidTransform2D cbTransform;
        cbTransform.offset     = { x * zoom + viewportCB.offset.x, y * zoom + viewportCB.offset.y };
        cbTransform.scale      = { w * zoom, h * zoom };
        cbTransform.screenSize = viewportCB.screenSize;
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_sprite_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, &cbTransform, sizeof(cbTransform));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_sprite_cb, MAP_WRITE);
    }

    impl_->pCtx_->SetPipelineState(impl_->m_draw_sprite_pso_and_srb.pPSO);

    auto* texVar = impl_->m_draw_sprite_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_texture");
    if (texVar) texVar->Set(pSRV);

    if (impl_->m_sprite_sampler) {
        auto* sampVar = impl_->m_draw_sprite_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_sampler");
        if (sampVar) sampVar->Set(impl_->m_sprite_sampler);
    }

    if (auto* transformVar = impl_->m_draw_sprite_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")) {
        transformVar->Set(impl_->m_draw_sprite_cb);
    }

    impl_->pCtx_->CommitShaderResources(impl_->m_draw_sprite_pso_and_srb.pSRB,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    IBuffer* pBuffers[] = { impl_->m_draw_sprite_vertex_buffer };
    Uint64 offsets[] = { 0 };
    impl_->pCtx_->SetVertexBuffers(0, 1, pBuffers, offsets,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_VERIFY_ALL;
    impl_->pCtx_->Draw(drawAttrs);
}

void PrimitiveRenderer2D::drawSpriteTransformed(float x, float y, float w, float h, const QTransform& transform, const QImage& image, float opacity)
{
    if (!impl_->hasRenderTarget() || !impl_->m_draw_sprite_transform_pso_and_srb.pPSO) return;
    if (image.isNull()) return;
    if (!impl_->pDevice_ || !impl_->pCtx_) return;

    if (image.cacheKey() != impl_->m_spriteCacheKey || !impl_->m_spriteTexCache) {
        const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
        const int imgW = rgba.width();
        const int imgH = rgba.height();
        if (imgW <= 0 || imgH <= 0) return;

        RefCntAutoPtr<ITexture> newTex;
        TextureDesc texDesc;
        texDesc.Type             = RESOURCE_DIM_TEX_2D;
        texDesc.Width            = static_cast<Uint32>(imgW);
        texDesc.Height           = static_cast<Uint32>(imgH);
        texDesc.Format           = TEX_FORMAT_RGBA8_UNORM_SRGB;
        texDesc.MipLevels        = 1;
        texDesc.Usage            = USAGE_IMMUTABLE;
        texDesc.BindFlags        = BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags   = CPU_ACCESS_NONE;

        TextureSubResData subData;
        subData.pData  = rgba.constBits();
        subData.Stride = static_cast<Uint64>(rgba.bytesPerLine());
        TextureData initData;
        initData.pSubResources   = &subData;
        initData.NumSubresources = 1;

        impl_->pDevice_->CreateTexture(texDesc, &initData, &newTex);
        if (!newTex) return;

        impl_->m_spriteTexCache = newTex;
        impl_->m_spriteCacheKey = image.cacheKey();
    }

    auto* pSRV = impl_->m_spriteTexCache->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    if (!pSRV) return;

    auto* pRTV = impl_->getCurrentRTV();
    if (!pRTV) return;

    SpriteVertex vertices[4] = {
        { {0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, opacity} },
        { {1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, opacity} },
        { {0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, opacity} },
        { {1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, opacity} },
    };
    impl_->pCtx_->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_sprite_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, vertices, sizeof(vertices));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_sprite_vertex_buffer, MAP_WRITE);
    }

    {
        auto viewportCB = impl_->viewport_.GetViewportCB();
        const float screenW = std::max(viewportCB.screenSize.x, 0.001f);
        const float screenH = std::max(viewportCB.screenSize.y, 0.001f);
        const float zoom = std::max(viewportCB.zoom, 0.001f);
        const float panX = viewportCB.offset.x;
        const float panY = viewportCB.offset.y;

        const float a  = transform.m11();
        const float b  = transform.m12();
        const float c  = transform.m21();
        const float d  = transform.m22();
        const float tx = transform.dx();
        const float ty = transform.dy();
        
        const float localX = a * x + c * y + tx;
        const float localY = b * x + d * y + ty;

        CBSolidRectTransform2D cbTransform;
        cbTransform.row0 = {
            2.0f * (a * w * zoom) / screenW,
            2.0f * (c * h * zoom) / screenW,
            0.0f,
            2.0f * ((localX * zoom) + panX) / screenW - 1.0f
        };
        cbTransform.row1 = {
            -2.0f * (b * w * zoom) / screenH,
            -2.0f * (d * h * zoom) / screenH,
            0.0f,
            1.0f - 2.0f * ((localY * zoom) + panY) / screenH
        };
        cbTransform.row2 = { 0.0f, 0.0f, 0.0f, 0.0f };
        cbTransform.row3 = { 0.0f, 0.0f, 0.0f, 1.0f };

        void* pData = nullptr;
        impl_->pCtx_->MapBuffer(impl_->m_draw_sprite_transform_matrix_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        std::memcpy(pData, &cbTransform, sizeof(cbTransform));
        impl_->pCtx_->UnmapBuffer(impl_->m_draw_sprite_transform_matrix_cb, MAP_WRITE);
    }

    impl_->pCtx_->SetPipelineState(impl_->m_draw_sprite_transform_pso_and_srb.pPSO);

    auto* texVar = impl_->m_draw_sprite_transform_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_texture");
    if (texVar) texVar->Set(pSRV);

    if (impl_->m_sprite_sampler) {
        auto* sampVar = impl_->m_draw_sprite_transform_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_sampler");
        if (sampVar) sampVar->Set(impl_->m_sprite_sampler);
    }

    if (auto* transformVar = impl_->m_draw_sprite_transform_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")) {
        transformVar->Set(impl_->m_draw_sprite_transform_matrix_cb);
    }

    impl_->pCtx_->CommitShaderResources(impl_->m_draw_sprite_transform_pso_and_srb.pSRB,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    IBuffer* pBuffers[] = { impl_->m_draw_sprite_vertex_buffer };
    Uint64 offsets[] = { 0 };
    impl_->pCtx_->SetVertexBuffers(0, 1, pBuffers, offsets,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    impl_->pCtx_->SetIndexBuffer(impl_->m_draw_solid_rect_index_buffer, 0,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs drawAttrs(6, VT_UINT32, DRAW_FLAG_VERIFY_ALL);
    impl_->pCtx_->DrawIndexed(drawAttrs);
}

} // namespace Artifact
