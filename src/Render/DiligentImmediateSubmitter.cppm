module;
#include <cmath>
#include <cstring>
#include <vector>
#include <type_traits>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Common/interface/BasicMath.hpp>
module Artifact.Render.DiligentImmediateSubmitter;

import Graphics;
import std;
import VertexBuffer;
import Artifact.Render.ShaderManager;
import Artifact.Render.RenderCommandBuffer;
import ArtifactCore.Utils.PerformanceProfiler;

namespace Artifact {

using namespace Diligent;
using namespace ArtifactCore;

static void mapWriteDiscard(IDeviceContext* ctx, IBuffer* buf, const void* data, size_t size)
{
    void* pData = nullptr;
    ctx->MapBuffer(buf, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    std::memcpy(pData, data, size);
    ctx->UnmapBuffer(buf, MAP_WRITE);
}

void DiligentImmediateSubmitter::createBuffers(RefCntAutoPtr<IRenderDevice> device, TEXTURE_FORMAT /*rtvFormat*/)
{
    if (!device) return;

    {
        BufferDesc desc;
        desc.Name           = "DIS Sprite VB";
        desc.Usage          = USAGE_DYNAMIC;
        desc.BindFlags      = BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        desc.Size           = sizeof(SpriteVertex) * 4;
        device->CreateBuffer(desc, nullptr, &m_draw_sprite_vertex_buffer);
    }

    {
        // Sprite CB must be at least sizeof(DrawSpriteConstants) to match original
        struct DrawSpriteConstantsPad { float4x4 a, b; };
        BufferDesc desc;
        desc.Name           = "DIS SpriteCB";
        desc.Size           = sizeof(DrawSpriteConstantsPad);
        desc.Usage          = USAGE_DYNAMIC;
        desc.BindFlags      = BIND_UNIFORM_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        device->CreateBuffer(desc, nullptr, &m_draw_sprite_cb);
    }

    {
        BufferDesc desc;
        desc.Name           = "DIS SpriteTransformMatrixCB";
        desc.Size           = sizeof(RenderSolidRectTransform2D);
        desc.Usage          = USAGE_DYNAMIC;
        desc.BindFlags      = BIND_UNIFORM_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        device->CreateBuffer(desc, nullptr, &m_draw_sprite_transform_matrix_cb);
    }

    {
        BufferDesc desc;
        desc.Name           = "DIS SolidRectCB";
        desc.Size           = sizeof(float4);
        desc.Usage          = USAGE_DYNAMIC;
        desc.BindFlags      = BIND_UNIFORM_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        device->CreateBuffer(desc, nullptr, &m_draw_solid_rect_cb);
    }

    {
        BufferDesc desc;
        desc.Name           = "DIS SolidRectTransformCB";
        desc.Size           = sizeof(RenderSolidTransform2D);
        desc.Usage          = USAGE_DYNAMIC;
        desc.BindFlags      = BIND_UNIFORM_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        device->CreateBuffer(desc, nullptr, &m_draw_solid_rect_trnsform_cb);
    }

    {
        BufferDesc desc;
        desc.Name           = "DIS SolidRectTransformMatrixCB";
        desc.Size           = sizeof(RenderSolidRectTransform2D);
        desc.Usage          = USAGE_DYNAMIC;
        desc.BindFlags      = BIND_UNIFORM_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        device->CreateBuffer(desc, nullptr, &m_draw_solid_rect_transform_matrix_cb);
    }

    {
        struct CBViewerHelper { float a, b; float2 pad; float4 c1, c2; };
        BufferDesc desc;
        desc.Name           = "DIS ViewerHelperCB";
        desc.Size           = sizeof(CBViewerHelper);
        desc.Usage          = USAGE_DYNAMIC;
        desc.BindFlags      = BIND_UNIFORM_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        device->CreateBuffer(desc, nullptr, &m_draw_viewer_helper_cb);
    }

    {
        struct DotLineShaderCB { float thickness; float spacing; float2 padding; };
        BufferDesc desc;
        desc.Name           = "DIS DotLineCB";
        desc.Size           = sizeof(DotLineShaderCB);
        desc.Usage          = USAGE_DYNAMIC;
        desc.BindFlags      = BIND_UNIFORM_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        device->CreateBuffer(desc, nullptr, &m_draw_dot_line_cb);
    }

    {
        struct OutlineParamsCB { float outlineThickness; float padding[3]; };
        BufferDesc desc;
        desc.Name           = "DIS OutlineParamsCB";
        desc.Size           = sizeof(OutlineParamsCB);
        desc.Usage          = USAGE_DYNAMIC;
        desc.BindFlags      = BIND_UNIFORM_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        device->CreateBuffer(desc, nullptr, &m_draw_outline_params_cb);
    }

    {
        BufferDesc desc;
        desc.Name           = "DIS SolidRect VB";
        desc.BindFlags      = BIND_VERTEX_BUFFER;
        desc.Usage          = USAGE_DYNAMIC;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        desc.Size           = sizeof(RectVertex) * 4;
        device->CreateBuffer(desc, nullptr, &m_draw_solid_rect_vertex_buffer);

        desc.Name = "DIS SolidTriangle VB";
        desc.Size = sizeof(RectVertex) * 3;
        device->CreateBuffer(desc, nullptr, &m_draw_solid_triangle_vertex_buffer);

        desc.Name = "DIS SolidCircle VB";
        desc.Size = sizeof(RectVertex) * 96;
        device->CreateBuffer(desc, nullptr, &m_draw_solid_circle_vertex_buffer);
    }

    {
        const Uint32 indices[6] = { 0, 1, 2, 2, 1, 3 };
        BufferDesc desc;
        desc.Name           = "DIS SolidRectIB";
        desc.Usage          = USAGE_IMMUTABLE;
        desc.BindFlags      = BIND_INDEX_BUFFER;
        desc.Size           = sizeof(indices);
        desc.CPUAccessFlags = CPU_ACCESS_NONE;
        BufferData initData;
        initData.pData    = indices;
        initData.DataSize = sizeof(indices);
        device->CreateBuffer(desc, &initData, &m_draw_solid_rect_index_buffer);
    }

    {
        BufferDesc desc;
        desc.Name           = "DIS ThickLine VB";
        desc.BindFlags      = BIND_VERTEX_BUFFER;
        desc.Usage          = USAGE_DYNAMIC;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        desc.Size           = sizeof(RectVertex) * 4;
        device->CreateBuffer(desc, nullptr, &m_draw_thick_line_vertex_buffer);
    }

    {
        BufferDesc desc;
        desc.Name           = "DIS DotLine VB";
        desc.BindFlags      = BIND_VERTEX_BUFFER;
        desc.Usage          = USAGE_DYNAMIC;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        desc.Size           = sizeof(DotLineVertex) * 4;
        device->CreateBuffer(desc, nullptr, &m_draw_dot_line_vertex_buffer);
    }
}

void DiligentImmediateSubmitter::setPSOs(ShaderManager& sm)
{
    m_draw_sprite_pso_and_srb               = sm.spritePsoAndSrb();
    m_draw_sprite_transform_pso_and_srb     = sm.spriteTransformPsoAndSrb();
    m_draw_masked_sprite_pso_and_srb        = sm.maskedSpritePsoAndSrb();
    m_sprite_sampler                        = sm.spriteSampler();
    m_draw_solid_rect_pso_and_srb           = sm.solidRectPsoAndSrb();
    m_draw_solid_rect_transform_pso_and_srb = sm.solidRectTransformPsoAndSrb();
    m_draw_line_pso_and_srb                 = sm.linePsoAndSrb();
    m_draw_thick_line_pso_and_srb           = sm.thickLinePsoAndSrb();
    m_draw_dot_line_pso_and_srb             = sm.dotLinePsoAndSrb();
    m_draw_solid_triangle_pso_and_srb       = sm.solidTrianglePsoAndSrb();
    m_draw_checkerboard_pso_and_srb         = sm.checkerboardPsoAndSrb();
    m_draw_grid_pso_and_srb                 = sm.gridPsoAndSrb();
    m_draw_rect_outline_pso_and_srb         = sm.outlinePsoAndSrb();
}

void DiligentImmediateSubmitter::destroy()
{
    m_draw_sprite_vertex_buffer             = nullptr;
    m_draw_solid_rect_vertex_buffer         = nullptr;
    m_draw_solid_triangle_vertex_buffer     = nullptr;
    m_draw_solid_circle_vertex_buffer       = nullptr;
    m_draw_thick_line_vertex_buffer         = nullptr;
    m_draw_dot_line_vertex_buffer           = nullptr;
    m_draw_solid_rect_index_buffer          = nullptr;
    m_draw_sprite_cb                        = nullptr;
    m_draw_sprite_transform_matrix_cb       = nullptr;
    m_draw_solid_rect_cb                    = nullptr;
    m_draw_solid_rect_trnsform_cb           = nullptr;
    m_draw_solid_rect_transform_matrix_cb   = nullptr;
    m_draw_viewer_helper_cb                 = nullptr;
    m_draw_dot_line_cb                      = nullptr;
    m_draw_outline_params_cb                = nullptr;
    m_sprite_sampler                        = nullptr;
    m_draw_sprite_pso_and_srb               = {};
    m_draw_sprite_transform_pso_and_srb     = {};
    m_draw_masked_sprite_pso_and_srb        = {};
    m_draw_solid_rect_pso_and_srb           = {};
    m_draw_solid_rect_transform_pso_and_srb = {};
    m_draw_line_pso_and_srb                 = {};
    m_draw_thick_line_pso_and_srb           = {};
    m_draw_dot_line_pso_and_srb             = {};
    m_draw_solid_triangle_pso_and_srb       = {};
    m_draw_checkerboard_pso_and_srb         = {};
    m_draw_grid_pso_and_srb                 = {};
    m_draw_rect_outline_pso_and_srb         = {};
}

void DiligentImmediateSubmitter::submit(RenderCommandBuffer& buf, IDeviceContext* ctx)
{
    ArtifactCore::ScopedPerformanceTimer _profSubmit2D("Submit2D");
    if (!ctx || buf.empty()) { buf.reset(); return; }
    auto* pRTV = buf.targetRTV;
    for (auto& pkt : buf.packets()) {
        std::visit([&](auto& p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr      (std::is_same_v<T, SolidRectPkt>)       submitSolidRect(p, ctx, pRTV);
            else if constexpr (std::is_same_v<T, SolidRectXformPkt>)  submitSolidRectXform(p, ctx, pRTV);
            else if constexpr (std::is_same_v<T, LinePkt>)            submitLine(p, ctx, pRTV);
            else if constexpr (std::is_same_v<T, QuadPkt>)            submitQuad(p, ctx, pRTV);
            else if constexpr (std::is_same_v<T, DotLinePkt>)         submitDotLine(p, ctx, pRTV);
            else if constexpr (std::is_same_v<T, SolidTriPkt>)        submitSolidTri(p, ctx, pRTV);
            else if constexpr (std::is_same_v<T, SolidCirclePkt>)     submitSolidCircle(p, ctx, pRTV);
            else if constexpr (std::is_same_v<T, CheckerboardPkt>)    submitCheckerboard(p, ctx, pRTV);
            else if constexpr (std::is_same_v<T, GridPkt>)            submitGrid(p, ctx, pRTV);
            else if constexpr (std::is_same_v<T, RectOutlinePkt>)     submitRectOutline(p, ctx, pRTV);
            else if constexpr (std::is_same_v<T, SpritePkt>)          submitSprite(p, ctx, pRTV);
            else if constexpr (std::is_same_v<T, SpriteXformPkt>)     submitSpriteXform(p, ctx, pRTV);
            else if constexpr (std::is_same_v<T, MaskedSpritePkt>)    submitMaskedSprite(p, ctx, pRTV);
        }, pkt);
    }
    buf.reset();
}

void DiligentImmediateSubmitter::submitSolidRect(const SolidRectPkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !m_draw_solid_rect_pso_and_srb.pPSO) return;
    if (!m_draw_solid_rect_vertex_buffer || !m_draw_solid_rect_cb ||
        !m_draw_solid_rect_trnsform_cb   || !m_draw_solid_rect_index_buffer) return;

    RectVertex vertices[4] = {
        {{0.0f, 0.0f}, p.color}, {{1.0f, 0.0f}, p.color},
        {{0.0f, 1.0f}, p.color}, {{1.0f, 1.0f}, p.color},
    };
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    mapWriteDiscard(ctx, m_draw_solid_rect_vertex_buffer, vertices, sizeof(vertices));
    mapWriteDiscard(ctx, m_draw_solid_rect_cb,           &p.color, sizeof(p.color));
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb,  &p.xform, sizeof(p.xform));

    ctx->SetPipelineState(m_draw_solid_rect_pso_and_srb.pPSO);
    IBuffer* pBufs[] = { m_draw_solid_rect_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    ctx->SetIndexBuffer(m_draw_solid_rect_index_buffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_draw_solid_rect_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(m_draw_solid_rect_trnsform_cb);
    m_draw_solid_rect_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL,  "ColorBuffer")->Set(m_draw_solid_rect_cb);
    ctx->CommitShaderResources(m_draw_solid_rect_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawIndexedAttribs drawAttrs(6, VT_UINT32, DRAW_FLAG_NONE);
    ctx->DrawIndexed(drawAttrs);
}

void DiligentImmediateSubmitter::submitSolidRectXform(const SolidRectXformPkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !m_draw_solid_rect_transform_pso_and_srb.pPSO) return;
    if (!m_draw_solid_rect_vertex_buffer || !m_draw_solid_rect_cb ||
        !m_draw_solid_rect_transform_matrix_cb || !m_draw_solid_rect_index_buffer) return;

    RectVertex vertices[4] = {
        {{0.0f, 0.0f}, p.color}, {{1.0f, 0.0f}, p.color},
        {{0.0f, 1.0f}, p.color}, {{1.0f, 1.0f}, p.color},
    };
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    mapWriteDiscard(ctx, m_draw_solid_rect_vertex_buffer,      vertices, sizeof(vertices));
    mapWriteDiscard(ctx, m_draw_solid_rect_cb,                 &p.color, sizeof(p.color));
    mapWriteDiscard(ctx, m_draw_solid_rect_transform_matrix_cb, &p.mat,  sizeof(p.mat));

    ctx->SetPipelineState(m_draw_solid_rect_transform_pso_and_srb.pPSO);
    IBuffer* pBufs[] = { m_draw_solid_rect_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    ctx->SetIndexBuffer(m_draw_solid_rect_index_buffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    if (auto* v = m_draw_solid_rect_transform_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB"))
        v->Set(m_draw_solid_rect_transform_matrix_cb);
    if (auto* v = m_draw_solid_rect_transform_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "ColorBuffer"))
        v->Set(m_draw_solid_rect_cb);
    ctx->CommitShaderResources(m_draw_solid_rect_transform_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawIndexedAttribs drawAttrs(6, VT_UINT32, DRAW_FLAG_NONE);
    ctx->DrawIndexed(drawAttrs);
}

void DiligentImmediateSubmitter::submitLine(const LinePkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !m_draw_line_pso_and_srb.pPSO) return;
    if (!m_draw_solid_rect_vertex_buffer || !m_draw_solid_rect_trnsform_cb) return;

    RectVertex vertices[2] = {
        { p.p1, p.c1 },
        { p.p2, p.c2 },
    };
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    mapWriteDiscard(ctx, m_draw_solid_rect_vertex_buffer, vertices, sizeof(vertices));
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb,   &p.xform, sizeof(p.xform));

    ctx->SetPipelineState(m_draw_line_pso_and_srb.pPSO);
    IBuffer* pBufs[] = { m_draw_solid_rect_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_draw_line_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(m_draw_solid_rect_trnsform_cb);
    ctx->CommitShaderResources(m_draw_line_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 2;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    ctx->Draw(drawAttrs);
}

void DiligentImmediateSubmitter::submitQuad(const QuadPkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !m_draw_thick_line_pso_and_srb.pPSO) return;
    if (!m_draw_thick_line_vertex_buffer || !m_draw_solid_rect_trnsform_cb) return;

    RectVertex vertices[4] = {
        { p.p0, p.color }, { p.p1, p.color },
        { p.p2, p.color }, { p.p3, p.color },
    };
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    mapWriteDiscard(ctx, m_draw_thick_line_vertex_buffer, vertices, sizeof(vertices));
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb,   &p.xform, sizeof(p.xform));

    ctx->SetPipelineState(m_draw_thick_line_pso_and_srb.pPSO);
    IBuffer* pBufs[] = { m_draw_thick_line_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_draw_thick_line_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(m_draw_solid_rect_trnsform_cb);
    ctx->CommitShaderResources(m_draw_thick_line_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    ctx->Draw(drawAttrs);
}

void DiligentImmediateSubmitter::submitDotLine(const DotLinePkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !m_draw_dot_line_pso_and_srb.pPSO) return;
    if (!m_draw_dot_line_vertex_buffer || !m_draw_solid_rect_trnsform_cb || !m_draw_dot_line_cb) return;

    DotLineVertex vertices[4];
    for (int i = 0; i < 4; ++i) {
        vertices[i].position = p.verts[i].pos;
        vertices[i].color    = p.verts[i].color;
        vertices[i].dist     = p.verts[i].dist;
    }
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    mapWriteDiscard(ctx, m_draw_dot_line_vertex_buffer, vertices, sizeof(vertices));
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb, &p.xform, sizeof(p.xform));

    struct DotLineShaderCB { float thickness; float spacing; float2 padding; };
    DotLineShaderCB cb = { p.thickness, p.spacing, {0.0f, 0.0f} };
    mapWriteDiscard(ctx, m_draw_dot_line_cb, &cb, sizeof(cb));

    ctx->SetPipelineState(m_draw_dot_line_pso_and_srb.pPSO);
    IBuffer* pBufs[] = { m_draw_dot_line_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_draw_dot_line_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(m_draw_solid_rect_trnsform_cb);
    m_draw_dot_line_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL,  "DotLineCB")->Set(m_draw_dot_line_cb);
    ctx->CommitShaderResources(m_draw_dot_line_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    ctx->Draw(drawAttrs);
}

void DiligentImmediateSubmitter::submitSolidTri(const SolidTriPkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !m_draw_solid_triangle_pso_and_srb.pPSO) return;
    if (!m_draw_solid_triangle_vertex_buffer || !m_draw_solid_rect_cb || !m_draw_solid_rect_trnsform_cb) return;

    RectVertex vertices[3] = {
        { p.p0, p.color }, { p.p1, p.color }, { p.p2, p.color },
    };
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    mapWriteDiscard(ctx, m_draw_solid_triangle_vertex_buffer, vertices, sizeof(vertices));
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb,       &p.xform, sizeof(p.xform));
    mapWriteDiscard(ctx, m_draw_solid_rect_cb,                &p.color, sizeof(p.color));

    ctx->SetPipelineState(m_draw_solid_triangle_pso_and_srb.pPSO);
    IBuffer* pBufs[] = { m_draw_solid_triangle_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_draw_solid_triangle_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(m_draw_solid_rect_trnsform_cb);
    m_draw_solid_triangle_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL,  "ColorBuffer")->Set(m_draw_solid_rect_cb);
    ctx->CommitShaderResources(m_draw_solid_triangle_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 3;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    ctx->Draw(drawAttrs);
}

void DiligentImmediateSubmitter::submitSolidCircle(const SolidCirclePkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !m_draw_solid_triangle_pso_and_srb.pPSO) return;
    if (!m_draw_solid_circle_vertex_buffer || !m_draw_solid_rect_cb || !m_draw_solid_rect_trnsform_cb) return;

    constexpr int segments   = 32;
    const int     vertexCount = segments * 3;
    std::vector<RectVertex> vertices(vertexCount);

    const float4 c = p.color;
    for (int i = 0; i < segments; ++i) {
        const float a0 = 2.0f * 3.14159265358979323846f * i / segments;
        const float a1 = 2.0f * 3.14159265358979323846f * (i + 1) / segments;
        vertices[i * 3 + 0] = {{ p.cx,                               p.cy                              }, c};
        vertices[i * 3 + 1] = {{ p.cx + p.radius * std::cos(a0),     p.cy + p.radius * std::sin(a0)    }, c};
        vertices[i * 3 + 2] = {{ p.cx + p.radius * std::cos(a1),     p.cy + p.radius * std::sin(a1)    }, c};
    }

    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    mapWriteDiscard(ctx, m_draw_solid_circle_vertex_buffer, vertices.data(), sizeof(RectVertex) * vertexCount);
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb,     &p.xform,        sizeof(p.xform));
    mapWriteDiscard(ctx, m_draw_solid_rect_cb,              &p.color,        sizeof(p.color));

    ctx->SetPipelineState(m_draw_solid_triangle_pso_and_srb.pPSO);
    IBuffer* pBufs[] = { m_draw_solid_circle_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_draw_solid_triangle_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(m_draw_solid_rect_trnsform_cb);
    m_draw_solid_triangle_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL,  "ColorBuffer")->Set(m_draw_solid_rect_cb);
    ctx->CommitShaderResources(m_draw_solid_triangle_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = static_cast<Uint32>(vertexCount);
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    ctx->Draw(drawAttrs);
}

void DiligentImmediateSubmitter::submitCheckerboard(const CheckerboardPkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !m_draw_checkerboard_pso_and_srb.pPSO) return;
    if (!m_draw_solid_rect_vertex_buffer || !m_draw_solid_rect_cb ||
        !m_draw_viewer_helper_cb         || !m_draw_solid_rect_trnsform_cb) return;

    RectVertex vertices[4] = {
        {{0,0},{1,1,1,1}}, {{1,0},{1,1,1,1}},
        {{0,1},{1,1,1,1}}, {{1,1},{1,1,1,1}},
    };
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    mapWriteDiscard(ctx, m_draw_solid_rect_vertex_buffer, vertices, sizeof(vertices));

    struct CBViewerHelper { float tileSize; float thickness; float2 padding; float4 color1; float4 color2; };
    CBViewerHelper cb;
    cb.tileSize  = p.helper.param0;
    cb.thickness = p.helper.param1;
    cb.padding   = {0.0f, 0.0f};
    cb.color1    = p.helper.color1;
    cb.color2    = p.helper.color2;
    mapWriteDiscard(ctx, m_draw_viewer_helper_cb,      &cb,         sizeof(cb));
    mapWriteDiscard(ctx, m_draw_solid_rect_cb,         &p.baseColor, sizeof(p.baseColor));
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb, &p.xform,   sizeof(p.xform));

    ctx->SetPipelineState(m_draw_checkerboard_pso_and_srb.pPSO);
    IBuffer* pBufs[] = { m_draw_solid_rect_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_draw_checkerboard_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(m_draw_solid_rect_trnsform_cb);
    if (auto* v = m_draw_checkerboard_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "ColorBuffer"))
        v->Set(m_draw_solid_rect_cb);
    m_draw_checkerboard_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "ViewerHelperCB")->Set(m_draw_viewer_helper_cb);
    ctx->CommitShaderResources(m_draw_checkerboard_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    ctx->Draw(drawAttrs);
}

void DiligentImmediateSubmitter::submitGrid(const GridPkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !m_draw_grid_pso_and_srb.pPSO) return;
    if (!m_draw_solid_rect_vertex_buffer || !m_draw_solid_rect_cb ||
        !m_draw_viewer_helper_cb         || !m_draw_solid_rect_trnsform_cb) return;

    RectVertex vertices[4] = {
        {{0,0},{1,1,1,1}}, {{1,0},{1,1,1,1}},
        {{0,1},{1,1,1,1}}, {{1,1},{1,1,1,1}},
    };
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    mapWriteDiscard(ctx, m_draw_solid_rect_vertex_buffer, vertices, sizeof(vertices));

    struct CBViewerHelper { float spacing; float thickness; float2 padding; float4 color1; float4 color2; };
    CBViewerHelper cb;
    cb.spacing   = p.helper.param0;
    cb.thickness = p.helper.param1;
    cb.padding   = {0.0f, 0.0f};
    cb.color1    = p.helper.color1;
    cb.color2    = {};
    mapWriteDiscard(ctx, m_draw_viewer_helper_cb,       &cb,          sizeof(cb));
    mapWriteDiscard(ctx, m_draw_solid_rect_cb,          &p.baseColor, sizeof(p.baseColor));
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb,  &p.xform,    sizeof(p.xform));

    ctx->SetPipelineState(m_draw_grid_pso_and_srb.pPSO);
    IBuffer* pBufs[] = { m_draw_solid_rect_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_draw_grid_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(m_draw_solid_rect_trnsform_cb);
    if (auto* v = m_draw_grid_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "ColorBuffer"))
        v->Set(m_draw_solid_rect_cb);
    m_draw_grid_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "ViewerHelperCB")->Set(m_draw_viewer_helper_cb);
    ctx->CommitShaderResources(m_draw_grid_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    ctx->Draw(drawAttrs);
}

void DiligentImmediateSubmitter::submitRectOutline(const RectOutlinePkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !m_draw_rect_outline_pso_and_srb.pPSO) return;
    if (!m_draw_solid_rect_vertex_buffer || !m_draw_solid_rect_trnsform_cb || !m_draw_outline_params_cb) return;

    RectVertex vertices[4] = {
        {{0.0f, 0.0f}, p.color}, {{1.0f, 0.0f}, p.color},
        {{1.0f, 1.0f}, p.color}, {{0.0f, 1.0f}, p.color},
    };
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    mapWriteDiscard(ctx, m_draw_solid_rect_vertex_buffer, vertices, sizeof(vertices));
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb,   &p.xform, sizeof(p.xform));

    struct OutlineParamsCB { float outlineThickness; float padding[3]; };
    OutlineParamsCB params{ 0.08f, {0.0f, 0.0f, 0.0f} };
    mapWriteDiscard(ctx, m_draw_outline_params_cb, &params, sizeof(params));

    ctx->SetPipelineState(m_draw_rect_outline_pso_and_srb.pPSO);
    IBuffer* pBufs[] = { m_draw_solid_rect_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    ctx->SetIndexBuffer(m_draw_solid_rect_index_buffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_draw_rect_outline_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(m_draw_solid_rect_trnsform_cb);
    if (auto* v = m_draw_rect_outline_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "OutlineParams"))
        v->Set(m_draw_outline_params_cb);
    ctx->CommitShaderResources(m_draw_rect_outline_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawIndexedAttribs drawAttrs(6, VT_UINT32, DRAW_FLAG_NONE);
    ctx->DrawIndexed(drawAttrs);
}

void DiligentImmediateSubmitter::submitSprite(const SpritePkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !p.pSRV || !m_draw_sprite_pso_and_srb.pPSO) return;
    if (!m_draw_sprite_vertex_buffer || !m_draw_sprite_cb) return;

    SpriteVertex vertices[4] = {
        {{0.0f,0.0f},{0.0f,0.0f},{1,1,1,p.opacity}},
        {{1.0f,0.0f},{1.0f,0.0f},{1,1,1,p.opacity}},
        {{0.0f,1.0f},{0.0f,1.0f},{1,1,1,p.opacity}},
        {{1.0f,1.0f},{1.0f,1.0f},{1,1,1,p.opacity}},
    };
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    mapWriteDiscard(ctx, m_draw_sprite_vertex_buffer, vertices, sizeof(vertices));
    mapWriteDiscard(ctx, m_draw_sprite_cb,            &p.xform, sizeof(p.xform));

    ctx->SetPipelineState(m_draw_sprite_pso_and_srb.pPSO);
    if (auto* v = m_draw_sprite_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_texture"))
        v->Set(p.pSRV);
    if (m_sprite_sampler) {
        if (auto* v = m_draw_sprite_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_sampler"))
            v->Set(m_sprite_sampler);
    }
    if (auto* v = m_draw_sprite_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB"))
        v->Set(m_draw_sprite_cb);
    ctx->CommitShaderResources(m_draw_sprite_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    IBuffer* pBufs[] = { m_draw_sprite_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    ctx->Draw(drawAttrs);
}

void DiligentImmediateSubmitter::submitSpriteXform(const SpriteXformPkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !p.pSRV || !m_draw_sprite_transform_pso_and_srb.pPSO) return;
    if (!m_draw_sprite_vertex_buffer || !m_draw_sprite_transform_matrix_cb) return;

    SpriteVertex vertices[4] = {
        {{0,0},{0,0},{1,1,1,p.opacity}}, {{1,0},{1,0},{1,1,1,p.opacity}},
        {{0,1},{0,1},{1,1,1,p.opacity}}, {{1,1},{1,1},{1,1,1,p.opacity}},
    };
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    mapWriteDiscard(ctx, m_draw_sprite_vertex_buffer,           vertices, sizeof(vertices));
    mapWriteDiscard(ctx, m_draw_sprite_transform_matrix_cb,     &p.mat,   sizeof(p.mat));

    ctx->SetPipelineState(m_draw_sprite_transform_pso_and_srb.pPSO);
    if (auto* v = m_draw_sprite_transform_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_texture"))
        v->Set(p.pSRV);
    if (m_sprite_sampler) {
        if (auto* v = m_draw_sprite_transform_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_sampler"))
            v->Set(m_sprite_sampler);
    }
    if (auto* v = m_draw_sprite_transform_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB"))
        v->Set(m_draw_sprite_transform_matrix_cb);
    ctx->CommitShaderResources(m_draw_sprite_transform_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    IBuffer* pBufs[] = { m_draw_sprite_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    ctx->SetIndexBuffer(m_draw_solid_rect_index_buffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawIndexedAttribs drawAttrs(6, VT_UINT32, DRAW_FLAG_NONE);
    ctx->DrawIndexed(drawAttrs);
}

void DiligentImmediateSubmitter::submitMaskedSprite(const MaskedSpritePkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !p.sceneSRV || !p.maskSRV || !m_draw_masked_sprite_pso_and_srb.pPSO) return;
    if (!m_draw_sprite_vertex_buffer || !m_draw_sprite_cb) return;

    SpriteVertex vertices[4] = {
        {{0,0},{0,0},{1,1,1,p.opacity}}, {{1,0},{1,0},{1,1,1,p.opacity}},
        {{0,1},{0,1},{1,1,1,p.opacity}}, {{1,1},{1,1},{1,1,1,p.opacity}},
    };
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    mapWriteDiscard(ctx, m_draw_sprite_vertex_buffer, vertices, sizeof(vertices));
    mapWriteDiscard(ctx, m_draw_sprite_cb,            &p.xform, sizeof(p.xform));

    ctx->SetPipelineState(m_draw_masked_sprite_pso_and_srb.pPSO);
    if (auto* v = m_draw_masked_sprite_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_scene"))
        v->Set(p.sceneSRV);
    if (auto* v = m_draw_masked_sprite_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_mask"))
        v->Set(p.maskSRV);
    if (m_sprite_sampler) {
        if (auto* v = m_draw_masked_sprite_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_sampler"))
            v->Set(m_sprite_sampler);
    }
    if (auto* v = m_draw_masked_sprite_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB"))
        v->Set(m_draw_sprite_cb);
    ctx->CommitShaderResources(m_draw_masked_sprite_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    IBuffer* pBufs[] = { m_draw_sprite_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    ctx->Draw(drawAttrs);
}

} // namespace Artifact
