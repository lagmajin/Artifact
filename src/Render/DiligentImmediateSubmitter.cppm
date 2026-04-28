module;
#include <cmath>
#include <cstring>
#include <algorithm>
#include <array>
#include <vector>
#include <type_traits>
#include <QImage>
#include <QFont>
#include <QRectF>
#include <QMatrix4x4>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/CommandList.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Common/interface/BasicMath.hpp>
module Artifact.Render.DiligentImmediateSubmitter;

import Graphics;
import FloatRGBA;
import Color.Float;
import std;
import VertexBuffer;
import Text.Style;
import Utils.String.UniString;
import Text.GlyphLayout;
import Text.GlyphAtlas;
import Font.FreeFont;
import Artifact.Render.ShaderManager;
import Artifact.Render.RenderCommandBuffer;
import Frame.Debug;
import ArtifactCore.Utils.PerformanceProfiler;

namespace Artifact {

using namespace Diligent;
using namespace ArtifactCore;

static void mapWriteDiscard(IDeviceContext* ctx, IBuffer* buf, const void* data, size_t size,
                            ArtifactCore::RenderCostStats* stats = nullptr)
{
    void* pData = nullptr;
    ctx->MapBuffer(buf, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    std::memcpy(pData, data, size);
    ctx->UnmapBuffer(buf, MAP_WRITE);
    if (stats) {
        ++stats->bufferUpdates;
    }
}

static void recordDrawCall(ArtifactCore::RenderCostStats* stats, bool indexed = false)
{
    if (!stats) {
        return;
    }
    ++stats->drawCalls;
    if (indexed) {
        ++stats->indexedDrawCalls;
    }
}

static void recordPipelineStateSwitch(ArtifactCore::RenderCostStats* stats)
{
    if (stats) {
        ++stats->psoSwitches;
    }
}

static void recordShaderResourceCommit(ArtifactCore::RenderCostStats* stats)
{
    if (stats) {
        ++stats->srbCommits;
    }
}

static TextStyle textStyleFromQFont(const QFont& font)
{
    TextStyle style;
    style.fontFamily = UniString(font.family());
    const float size = font.pointSizeF() > 0.0f
                           ? static_cast<float>(font.pointSizeF())
                           : (font.pixelSize() > 0 ? static_cast<float>(font.pixelSize())
                                                   : 12.0f);
    style.fontSize = size;
    style.pixelSize = size;
    style.tracking = static_cast<float>(font.letterSpacing());
    style.fontWeight = font.bold() ? FontWeight::Bold : FontWeight::Normal;
    style.fontStyle = font.italic() ? FontStyle::Italic : FontStyle::Normal;
    style.allCaps = font.capitalization() == QFont::AllUppercase;
    style.underline = font.underline();
    style.strikethrough = font.strikeOut();
    return style;
}

static ParagraphStyle paragraphStyleFromRectAndAlignment(const QRectF& rect,
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

    paragraph.wrapMode = ((static_cast<int>(alignment) & static_cast<int>(Qt::TextWordWrap)) != 0)
                             ? TextWrapMode::WordWrap
                             : TextWrapMode::NoWrap;
    return paragraph;
}

static QImage atlasImageToRgba(const QImage& image)
{
    return image.isNull() ? QImage{} : image.convertToFormat(QImage::Format_RGBA8888);
}

static RefCntAutoPtr<ITexture> createAtlasTexture(RefCntAutoPtr<IRenderDevice> device,
                                                 const QImage& image)
{
    if (!device || image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return {};
    }

    const QImage rgba = atlasImageToRgba(image);
    TextureDesc texDesc;
    texDesc.Name = "GlyphAtlasTexture";
    texDesc.Type = RESOURCE_DIM_TEX_2D;
    texDesc.Width = static_cast<Uint32>(rgba.width());
    texDesc.Height = static_cast<Uint32>(rgba.height());
    texDesc.MipLevels = 1;
    texDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
    texDesc.Usage = USAGE_IMMUTABLE;
    texDesc.BindFlags = BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = CPU_ACCESS_NONE;

    TextureSubResData subRes;
    subRes.pData = rgba.constBits();
    subRes.Stride = static_cast<Uint64>(rgba.bytesPerLine());

    TextureData initData;
    initData.pSubResources = &subRes;
    initData.NumSubresources = 1;

    RefCntAutoPtr<ITexture> texture;
    device->CreateTexture(texDesc, &initData, &texture);
    return texture;
}

void DiligentImmediateSubmitter::createBuffers(RefCntAutoPtr<IRenderDevice> device, TEXTURE_FORMAT /*rtvFormat*/)
{
    if (!device) return;
    m_device = device;

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

    // H5: Immutable unit-quad VB for opacity==1.0f sprites
    {
        const SpriteVertex unitVerts[4] = {
            {{0.0f,0.0f},{0.0f,0.0f},{1,1,1,1}},
            {{1.0f,0.0f},{1.0f,0.0f},{1,1,1,1}},
            {{0.0f,1.0f},{0.0f,1.0f},{1,1,1,1}},
            {{1.0f,1.0f},{1.0f,1.0f},{1,1,1,1}},
        };
        BufferDesc desc;
        desc.Name           = "DIS Sprite Unit Quad VB";
        desc.BindFlags      = BIND_VERTEX_BUFFER;
        desc.Usage          = USAGE_IMMUTABLE;
        desc.CPUAccessFlags = CPU_ACCESS_NONE;
        desc.Size           = sizeof(unitVerts);
        BufferData initData;
        initData.pData    = unitVerts;
        initData.DataSize = sizeof(unitVerts);
        device->CreateBuffer(desc, &initData, &m_sprite_unit_quad_vb_);
    }

    // Phase 3: dynamic VB for batch solid rects + immutable IB
    {
        BufferDesc desc;
        desc.Name           = "DIS BatchSolidRect VB";
        desc.BindFlags      = BIND_VERTEX_BUFFER;
        desc.Usage          = USAGE_DYNAMIC;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        desc.Size           = sizeof(BatchRectVertexAA) * 4 * k_batch_solid_rect_max;
        device->CreateBuffer(desc, nullptr, &m_batch_solid_rect_vb_);
    }
    {
        std::vector<Uint32> batchIdx(k_batch_solid_rect_max * 6);
        for (Uint32 i = 0; i < k_batch_solid_rect_max; ++i) {
            const Uint32 b = i * 4;
            batchIdx[i*6+0]=b;   batchIdx[i*6+1]=b+1; batchIdx[i*6+2]=b+2;
            batchIdx[i*6+3]=b+2; batchIdx[i*6+4]=b+1; batchIdx[i*6+5]=b+3;
        }
        BufferDesc desc;
        desc.Name           = "DIS BatchSolidRect IB";
        desc.Usage          = USAGE_IMMUTABLE;
        desc.BindFlags      = BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_NONE;
        desc.Size           = sizeof(Uint32) * Uint32(batchIdx.size());
        BufferData initData;
        initData.pData    = batchIdx.data();
        initData.DataSize = desc.Size;
        device->CreateBuffer(desc, &initData, &m_batch_solid_rect_ib_);
    }
    m_batch_solid_rect_cpu_.resize(k_batch_solid_rect_max * 4);
}

void DiligentImmediateSubmitter::setPSOs(ShaderManager& sm)
{
    m_draw_sprite_pso_and_srb               = sm.spritePsoAndSrb();
    m_draw_sprite_transform_pso_and_srb     = sm.spriteTransformPsoAndSrb();
    m_draw_masked_sprite_pso_and_srb        = sm.maskedSpritePsoAndSrb();
    m_draw_glyph_pso_and_srb                = sm.glyphQuadPsoAndSrb();
    m_draw_glyph_transform_pso_and_srb      = sm.glyphQuadTransformPsoAndSrb();
    m_sprite_sampler                        = sm.spriteSampler();
    m_glyph_sampler                         = sm.glyphAtlasSampler();
    m_draw_solid_rect_pso_and_srb           = sm.solidRectPsoAndSrb();
    m_draw_solid_rect_transform_pso_and_srb = sm.solidRectTransformPsoAndSrb();
    m_draw_line_pso_and_srb                 = sm.linePsoAndSrb();
    m_draw_thick_line_pso_and_srb           = sm.thickLinePsoAndSrb();
    m_draw_dot_line_pso_and_srb             = sm.dotLinePsoAndSrb();
    m_draw_solid_triangle_pso_and_srb       = sm.solidTrianglePsoAndSrb();
    m_draw_checkerboard_pso_and_srb         = sm.checkerboardPsoAndSrb();
    m_draw_grid_pso_and_srb                 = sm.gridPsoAndSrb();
    m_draw_rect_outline_pso_and_srb         = sm.outlinePsoAndSrb();
    m_draw_batch_solid_rect_pso_and_srb     = sm.batchSolidRectAAPsoAndSrb();

    // H1: Pre-bind all static CBs and samplers to their SRBs once
    {
        auto bindVar = [](IShaderResourceBinding* srb, SHADER_TYPE type, const char* name, IDeviceObject* res) {
            if (srb && res)
                if (auto* v = srb->GetVariableByName(type, name)) v->Set(res);
        };
        bindVar(m_draw_solid_rect_pso_and_srb.pSRB,           SHADER_TYPE_VERTEX, "TransformCB",    m_draw_solid_rect_trnsform_cb);
        bindVar(m_draw_solid_rect_pso_and_srb.pSRB,           SHADER_TYPE_PIXEL,  "ColorBuffer",    m_draw_solid_rect_cb);
        bindVar(m_draw_solid_rect_transform_pso_and_srb.pSRB, SHADER_TYPE_VERTEX, "TransformCB",    m_draw_solid_rect_transform_matrix_cb);
        bindVar(m_draw_solid_rect_transform_pso_and_srb.pSRB, SHADER_TYPE_PIXEL,  "ColorBuffer",    m_draw_solid_rect_cb);
        bindVar(m_draw_line_pso_and_srb.pSRB,                 SHADER_TYPE_VERTEX, "TransformCB",    m_draw_solid_rect_trnsform_cb);
        bindVar(m_draw_thick_line_pso_and_srb.pSRB,           SHADER_TYPE_VERTEX, "TransformCB",    m_draw_solid_rect_trnsform_cb);
        bindVar(m_draw_dot_line_pso_and_srb.pSRB,             SHADER_TYPE_VERTEX, "TransformCB",    m_draw_solid_rect_trnsform_cb);
        bindVar(m_draw_dot_line_pso_and_srb.pSRB,             SHADER_TYPE_PIXEL,  "DotLineCB",      m_draw_dot_line_cb);
        bindVar(m_draw_solid_triangle_pso_and_srb.pSRB,       SHADER_TYPE_VERTEX, "TransformCB",    m_draw_solid_rect_trnsform_cb);
        bindVar(m_draw_checkerboard_pso_and_srb.pSRB,         SHADER_TYPE_VERTEX, "TransformCB",    m_draw_solid_rect_trnsform_cb);
        bindVar(m_draw_checkerboard_pso_and_srb.pSRB,         SHADER_TYPE_PIXEL,  "ColorBuffer",    m_draw_solid_rect_cb);
        bindVar(m_draw_checkerboard_pso_and_srb.pSRB,         SHADER_TYPE_PIXEL,  "ViewerHelperCB", m_draw_viewer_helper_cb);
        bindVar(m_draw_grid_pso_and_srb.pSRB,                 SHADER_TYPE_VERTEX, "TransformCB",    m_draw_solid_rect_trnsform_cb);
        bindVar(m_draw_grid_pso_and_srb.pSRB,                 SHADER_TYPE_PIXEL,  "ColorBuffer",    m_draw_solid_rect_cb);
        bindVar(m_draw_grid_pso_and_srb.pSRB,                 SHADER_TYPE_PIXEL,  "ViewerHelperCB", m_draw_viewer_helper_cb);
        bindVar(m_draw_sprite_pso_and_srb.pSRB,               SHADER_TYPE_VERTEX, "TransformCB",    m_draw_sprite_cb);
        if (m_sprite_sampler)
            bindVar(m_draw_sprite_pso_and_srb.pSRB,           SHADER_TYPE_PIXEL,  "g_sampler",      m_sprite_sampler);
        bindVar(m_draw_sprite_transform_pso_and_srb.pSRB,     SHADER_TYPE_VERTEX, "TransformCB",    m_draw_sprite_transform_matrix_cb);
        if (m_sprite_sampler)
            bindVar(m_draw_sprite_transform_pso_and_srb.pSRB, SHADER_TYPE_PIXEL,  "g_sampler",      m_sprite_sampler);
        bindVar(m_draw_masked_sprite_pso_and_srb.pSRB,        SHADER_TYPE_VERTEX, "TransformCB",    m_draw_sprite_cb);
        if (m_sprite_sampler)
            bindVar(m_draw_masked_sprite_pso_and_srb.pSRB,    SHADER_TYPE_PIXEL,  "g_sampler",      m_sprite_sampler);
        bindVar(m_draw_glyph_pso_and_srb.pSRB,                SHADER_TYPE_VERTEX, "TransformCB",    m_draw_sprite_cb);
        bindVar(m_draw_glyph_transform_pso_and_srb.pSRB,      SHADER_TYPE_VERTEX, "TransformCB",    m_draw_sprite_transform_matrix_cb);
        ISampler* gSampler = m_glyph_sampler ? m_glyph_sampler.RawPtr()
                           : (m_sprite_sampler ? m_sprite_sampler.RawPtr() : nullptr);
        if (gSampler) {
            bindVar(m_draw_glyph_pso_and_srb.pSRB,           SHADER_TYPE_PIXEL, "g_sampler", gSampler);
            bindVar(m_draw_glyph_transform_pso_and_srb.pSRB, SHADER_TYPE_PIXEL, "g_sampler", gSampler);
        }
    }
    // H1: Cache variable pointers for per-draw texture SRV updates
    m_var_sprite_gTexture_      = m_draw_sprite_pso_and_srb.pSRB
        ? m_draw_sprite_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_texture") : nullptr;
    m_var_spriteXform_gTexture_ = m_draw_sprite_transform_pso_and_srb.pSRB
        ? m_draw_sprite_transform_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_texture") : nullptr;
    m_var_maskedSprite_gScene_  = m_draw_masked_sprite_pso_and_srb.pSRB
        ? m_draw_masked_sprite_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_scene") : nullptr;
    m_var_maskedSprite_gMask_   = m_draw_masked_sprite_pso_and_srb.pSRB
        ? m_draw_masked_sprite_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_mask") : nullptr;
    m_var_glyph_gTexture_       = m_draw_glyph_pso_and_srb.pSRB
        ? m_draw_glyph_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_texture") : nullptr;
    m_var_glyphXform_gTexture_  = m_draw_glyph_transform_pso_and_srb.pSRB
        ? m_draw_glyph_transform_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_texture") : nullptr;
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
    m_glyph_sampler                         = nullptr;
    m_glyph_atlas_texture                   = nullptr;
    m_glyph_atlas_srv                       = nullptr;
    m_device                                = nullptr;
    m_draw_sprite_pso_and_srb               = {};
    m_draw_sprite_transform_pso_and_srb     = {};
    m_draw_masked_sprite_pso_and_srb        = {};
    m_draw_glyph_pso_and_srb                = {};
    m_draw_glyph_transform_pso_and_srb      = {};
    m_draw_solid_rect_pso_and_srb           = {};
    m_draw_solid_rect_transform_pso_and_srb = {};
    m_draw_line_pso_and_srb                 = {};
    m_draw_thick_line_pso_and_srb           = {};
    m_draw_dot_line_pso_and_srb             = {};
    m_draw_solid_triangle_pso_and_srb       = {};
    m_draw_checkerboard_pso_and_srb         = {};
    m_draw_grid_pso_and_srb                 = {};
    m_draw_rect_outline_pso_and_srb         = {};
    m_frameCostStats_                       = nullptr;
    m_primitiveRenderer3D_                  = nullptr;
    m_particleRenderer_                     = nullptr;
    m_sprite_unit_quad_vb_                  = nullptr;
    m_currentPSO_                           = nullptr;
    m_var_sprite_gTexture_                  = nullptr;
    m_var_spriteXform_gTexture_             = nullptr;
    m_var_maskedSprite_gScene_              = nullptr;
    m_var_maskedSprite_gMask_               = nullptr;
    m_var_glyph_gTexture_                   = nullptr;
    m_var_glyphXform_gTexture_              = nullptr;
    m_deferredCtx_                          = nullptr;
    m_batch_solid_rect_vb_                  = nullptr;
    m_batch_solid_rect_ib_                  = nullptr;
    m_draw_batch_solid_rect_pso_and_srb     = {};
    m_batch_solid_rect_cpu_.clear();
    m_batchSolidRectCount_                  = 0;
}

void DiligentImmediateSubmitter::setFrameCostStats(ArtifactCore::RenderCostStats* stats)
{
    m_frameCostStats_ = stats;
}

void DiligentImmediateSubmitter::setDeferredContext(RefCntAutoPtr<IDeviceContext> deferred)
{
    m_deferredCtx_ = std::move(deferred);
}

void DiligentImmediateSubmitter::setPrimitiveRenderer3D(PrimitiveRenderer3D* renderer)
{
    m_primitiveRenderer3D_ = renderer;
}

void DiligentImmediateSubmitter::setParticleRenderer(ArtifactCore::ParticleRenderer* renderer)
{
    m_particleRenderer_ = renderer;
}

void DiligentImmediateSubmitter::submit(RenderCommandBuffer& buf, IDeviceContext* ctx)
{
    ArtifactCore::ScopedPerformanceTimer _profSubmit2D("Submit2D");
    if (!ctx || buf.empty()) { buf.reset(); return; }
    auto* pRTV = buf.targetRTV;
    if (!pRTV) { buf.reset(); return; }
    // Phase 7a: record into deferred context, then execute on immediate
    IDeviceContext* recordCtx = ctx;
    if (m_deferredCtx_) {
        m_deferredCtx_->Begin(ctx->GetDesc().ContextId);
        recordCtx = m_deferredCtx_.RawPtr();
    }
    recordCtx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION); // H2
    m_currentPSO_ = nullptr; // H3: reset PSO dedup state per submit
    m_batchSolidRectCount_ = 0;
    const bool batchReady = m_batch_solid_rect_vb_ && m_batch_solid_rect_ib_
                         && m_draw_batch_solid_rect_pso_and_srb.pPSO
                         && !m_batch_solid_rect_cpu_.empty();

    // Flush all accumulated SolidRectPkts as a single batched draw call
    auto flushSolidRectBatch = [&]() {
        if (m_batchSolidRectCount_ == 0) return;
        mapWriteDiscard(recordCtx, m_batch_solid_rect_vb_,
            m_batch_solid_rect_cpu_.data(),
            sizeof(BatchRectVertexAA) * Uint32(m_batchSolidRectCount_) * 4,
            m_frameCostStats_);
        if (m_currentPSO_ != m_draw_batch_solid_rect_pso_and_srb.pPSO) {
            recordPipelineStateSwitch(m_frameCostStats_);
            recordCtx->SetPipelineState(m_draw_batch_solid_rect_pso_and_srb.pPSO);
            m_currentPSO_ = m_draw_batch_solid_rect_pso_and_srb.pPSO;
        }
        IBuffer* pBufs[] = { m_batch_solid_rect_vb_.RawPtr() };
        Uint64   offs[]  = { 0 };
        recordCtx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        recordCtx->SetIndexBuffer(m_batch_solid_rect_ib_, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        recordShaderResourceCommit(m_frameCostStats_);
        recordCtx->CommitShaderResources(m_draw_batch_solid_rect_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        DrawIndexedAttribs da(Uint32(m_batchSolidRectCount_) * 6, VT_UINT32, DRAW_FLAG_NONE);
        recordDrawCall(m_frameCostStats_, true);
        recordCtx->DrawIndexed(da);
        m_batchSolidRectCount_ = 0;
    };

    for (auto& pkt : buf.packets()) {
        std::visit([&](auto& p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, SolidRectPkt>) {
                if (batchReady) {
                    // Overflow: flush before adding a new rect
                    if (m_batchSolidRectCount_ >= static_cast<int>(k_batch_solid_rect_max))
                        flushSolidRectBatch();
                    const auto& x = p.xform;
                    const float invW = 2.0f / x.screenSize.x;
                    const float invH = 2.0f / x.screenSize.y;
                    auto* v = &m_batch_solid_rect_cpu_[m_batchSolidRectCount_ * 4];
                    for (int i = 0; i < 4; ++i) {
                        const float u  = float(i & 1);
                        const float vv = float(i >> 1);
                        const float wx = u  * x.scale.x + x.offset.x;
                        const float wy = vv * x.scale.y + x.offset.y;
                        v[i].pos.x = wx * invW - 1.0f;
                        v[i].pos.y = -(wy * invH - 1.0f);
                        v[i].color = p.color;
                        v[i].uv    = {u, vv};
                    }
                    ++m_batchSolidRectCount_;
                } else {
                    submitSolidRect(p, recordCtx, pRTV);
                }
            } else {
                flushSolidRectBatch(); // flush before switching to a different draw type
                if constexpr      (std::is_same_v<T, SolidRectXformPkt>)  submitSolidRectXform(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, LinePkt>)            submitLine(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, QuadPkt>)            submitQuad(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, DotLinePkt>)         submitDotLine(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, SolidTriPkt>)        submitSolidTri(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, SolidCirclePkt>)     submitSolidCircle(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, CheckerboardPkt>)    submitCheckerboard(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, GridPkt>)            submitGrid(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, RectOutlinePkt>)     submitRectOutline(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, SpritePkt>)          submitSprite(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, SpriteXformPkt>)     submitSpriteXform(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, MaskedSpritePkt>)    submitMaskedSprite(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, BillboardPkt>)       submitBillboard(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, BillboardImagePkt>)  submitBillboardImage(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, ParticlePkt>)        submitParticles(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, GlyphTextPkt>)       submitGlyphText(p, recordCtx, pRTV);
                else if constexpr (std::is_same_v<T, GlyphTextXformPkt>)  submitGlyphTextTransformed(p, recordCtx, pRTV);
            }
        }, pkt);
    }
    flushSolidRectBatch(); // flush any remaining
    if (m_deferredCtx_) {
        RefCntAutoPtr<ICommandList> pCmdList;
        m_deferredCtx_->FinishCommandList(&pCmdList);
        ICommandList* raw = pCmdList.RawPtr();
        ctx->ExecuteCommandLists(1, &raw);
        m_deferredCtx_->FinishFrame();
    }
    buf.reset();
}

void DiligentImmediateSubmitter::submitBillboard(const BillboardPkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!ctx || !pRTV || !m_primitiveRenderer3D_) {
        return;
    }
    FloatRGBA tint{p.tint.x, p.tint.y, p.tint.z, p.tint.w};
    m_primitiveRenderer3D_->drawBillboardQuad(p.center, p.size, p.pSRV, tint, p.opacity, p.rollDegrees);
}

void DiligentImmediateSubmitter::submitBillboardImage(const BillboardImagePkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!ctx || !pRTV || !m_primitiveRenderer3D_) {
        return;
    }
    FloatRGBA tint{p.tint.x, p.tint.y, p.tint.z, p.tint.w};
    m_primitiveRenderer3D_->drawBillboardQuad(p.center, p.size, p.image, tint, p.opacity, p.rollDegrees);
}

void DiligentImmediateSubmitter::submitParticles(const ParticlePkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!ctx || !pRTV || !m_particleRenderer_ || p.data.particles.empty()) {
        return;
    }
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_particleRenderer_->setViewMatrix(p.viewMatrix.constData());
    m_particleRenderer_->setProjectionMatrix(p.projMatrix.constData());
    m_particleRenderer_->setFrameCostStats(m_frameCostStats_);
    m_particleRenderer_->updateBuffer(p.data);
    m_particleRenderer_->prepare(ctx);
    m_particleRenderer_->draw(ctx, p.data.particles.size());
    m_particleRenderer_->setFrameCostStats(nullptr);
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
    mapWriteDiscard(ctx, m_draw_solid_rect_vertex_buffer, vertices, sizeof(vertices), m_frameCostStats_);
    mapWriteDiscard(ctx, m_draw_solid_rect_cb,           &p.color, sizeof(p.color), m_frameCostStats_);
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb,  &p.xform, sizeof(p.xform), m_frameCostStats_);

    if (m_currentPSO_ != m_draw_solid_rect_pso_and_srb.pPSO) {
        recordPipelineStateSwitch(m_frameCostStats_);
        ctx->SetPipelineState(m_draw_solid_rect_pso_and_srb.pPSO);
        m_currentPSO_ = m_draw_solid_rect_pso_and_srb.pPSO;
    }
    IBuffer* pBufs[] = { m_draw_solid_rect_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    ctx->SetIndexBuffer(m_draw_solid_rect_index_buffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    recordShaderResourceCommit(m_frameCostStats_);
    ctx->CommitShaderResources(m_draw_solid_rect_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawIndexedAttribs drawAttrs(6, VT_UINT32, DRAW_FLAG_NONE);
    recordDrawCall(m_frameCostStats_, true);
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
    mapWriteDiscard(ctx, m_draw_solid_rect_vertex_buffer,      vertices, sizeof(vertices), m_frameCostStats_);
    mapWriteDiscard(ctx, m_draw_solid_rect_cb,                 &p.color, sizeof(p.color), m_frameCostStats_);
    mapWriteDiscard(ctx, m_draw_solid_rect_transform_matrix_cb, &p.mat,  sizeof(p.mat), m_frameCostStats_);

    if (m_currentPSO_ != m_draw_solid_rect_transform_pso_and_srb.pPSO) {
        recordPipelineStateSwitch(m_frameCostStats_);
        ctx->SetPipelineState(m_draw_solid_rect_transform_pso_and_srb.pPSO);
        m_currentPSO_ = m_draw_solid_rect_transform_pso_and_srb.pPSO;
    }
    IBuffer* pBufs[] = { m_draw_solid_rect_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    ctx->SetIndexBuffer(m_draw_solid_rect_index_buffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    recordShaderResourceCommit(m_frameCostStats_);
    ctx->CommitShaderResources(m_draw_solid_rect_transform_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawIndexedAttribs drawAttrs(6, VT_UINT32, DRAW_FLAG_NONE);
    recordDrawCall(m_frameCostStats_, true);
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
    mapWriteDiscard(ctx, m_draw_solid_rect_vertex_buffer, vertices, sizeof(vertices), m_frameCostStats_);
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb,   &p.xform, sizeof(p.xform), m_frameCostStats_);

    if (m_currentPSO_ != m_draw_line_pso_and_srb.pPSO) {
        recordPipelineStateSwitch(m_frameCostStats_);
        ctx->SetPipelineState(m_draw_line_pso_and_srb.pPSO);
        m_currentPSO_ = m_draw_line_pso_and_srb.pPSO;
    }
    IBuffer* pBufs[] = { m_draw_solid_rect_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    recordShaderResourceCommit(m_frameCostStats_);
    ctx->CommitShaderResources(m_draw_line_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 2;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    recordDrawCall(m_frameCostStats_);
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
    mapWriteDiscard(ctx, m_draw_thick_line_vertex_buffer, vertices, sizeof(vertices), m_frameCostStats_);
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb,   &p.xform, sizeof(p.xform), m_frameCostStats_);

    if (m_currentPSO_ != m_draw_thick_line_pso_and_srb.pPSO) {
        recordPipelineStateSwitch(m_frameCostStats_);
        ctx->SetPipelineState(m_draw_thick_line_pso_and_srb.pPSO);
        m_currentPSO_ = m_draw_thick_line_pso_and_srb.pPSO;
    }
    IBuffer* pBufs[] = { m_draw_thick_line_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    recordShaderResourceCommit(m_frameCostStats_);
    ctx->CommitShaderResources(m_draw_thick_line_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    recordDrawCall(m_frameCostStats_);
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
    mapWriteDiscard(ctx, m_draw_dot_line_vertex_buffer, vertices, sizeof(vertices), m_frameCostStats_);
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb, &p.xform, sizeof(p.xform), m_frameCostStats_);

    struct DotLineShaderCB { float thickness; float spacing; float2 padding; };
    DotLineShaderCB cb = { p.thickness, p.spacing, {0.0f, 0.0f} };
    mapWriteDiscard(ctx, m_draw_dot_line_cb, &cb, sizeof(cb), m_frameCostStats_);

    if (m_currentPSO_ != m_draw_dot_line_pso_and_srb.pPSO) {
        recordPipelineStateSwitch(m_frameCostStats_);
        ctx->SetPipelineState(m_draw_dot_line_pso_and_srb.pPSO);
        m_currentPSO_ = m_draw_dot_line_pso_and_srb.pPSO;
    }
    IBuffer* pBufs[] = { m_draw_dot_line_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    recordShaderResourceCommit(m_frameCostStats_);
    ctx->CommitShaderResources(m_draw_dot_line_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    recordDrawCall(m_frameCostStats_);
    ctx->Draw(drawAttrs);
}

void DiligentImmediateSubmitter::submitSolidTri(const SolidTriPkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !m_draw_solid_triangle_pso_and_srb.pPSO) return;
    if (!m_draw_solid_triangle_vertex_buffer || !m_draw_solid_rect_trnsform_cb) return;

    RectVertex vertices[3] = {
        { p.p0, p.color }, { p.p1, p.color }, { p.p2, p.color },
    };
    mapWriteDiscard(ctx, m_draw_solid_triangle_vertex_buffer, vertices, sizeof(vertices), m_frameCostStats_);
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb,       &p.xform, sizeof(p.xform), m_frameCostStats_);

    if (m_currentPSO_ != m_draw_solid_triangle_pso_and_srb.pPSO) {
        recordPipelineStateSwitch(m_frameCostStats_);
        ctx->SetPipelineState(m_draw_solid_triangle_pso_and_srb.pPSO);
        m_currentPSO_ = m_draw_solid_triangle_pso_and_srb.pPSO;
    }
    IBuffer* pBufs[] = { m_draw_solid_triangle_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    recordShaderResourceCommit(m_frameCostStats_);
    ctx->CommitShaderResources(m_draw_solid_triangle_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 3;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    recordDrawCall(m_frameCostStats_);
    ctx->Draw(drawAttrs);
}

void DiligentImmediateSubmitter::submitSolidCircle(const SolidCirclePkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !m_draw_solid_triangle_pso_and_srb.pPSO) return;
    if (!m_draw_solid_circle_vertex_buffer || !m_draw_solid_rect_trnsform_cb) return;

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

    mapWriteDiscard(ctx, m_draw_solid_circle_vertex_buffer, vertices.data(), sizeof(RectVertex) * vertexCount, m_frameCostStats_);
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb,     &p.xform,        sizeof(p.xform), m_frameCostStats_);

    if (m_currentPSO_ != m_draw_solid_triangle_pso_and_srb.pPSO) {
        recordPipelineStateSwitch(m_frameCostStats_);
        ctx->SetPipelineState(m_draw_solid_triangle_pso_and_srb.pPSO);
        m_currentPSO_ = m_draw_solid_triangle_pso_and_srb.pPSO;
    }
    IBuffer* pBufs[] = { m_draw_solid_circle_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    recordShaderResourceCommit(m_frameCostStats_);
    ctx->CommitShaderResources(m_draw_solid_triangle_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = static_cast<Uint32>(vertexCount);
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    recordDrawCall(m_frameCostStats_);
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
    mapWriteDiscard(ctx, m_draw_solid_rect_vertex_buffer, vertices, sizeof(vertices), m_frameCostStats_);

    struct CBViewerHelper { float tileSize; float thickness; float2 padding; float4 color1; float4 color2; };
    CBViewerHelper cb;
    cb.tileSize  = p.helper.param0;
    cb.thickness = p.helper.param1;
    cb.padding   = {0.0f, 0.0f};
    cb.color1    = p.helper.color1;
    cb.color2    = p.helper.color2;
    mapWriteDiscard(ctx, m_draw_viewer_helper_cb,      &cb,         sizeof(cb), m_frameCostStats_);
    mapWriteDiscard(ctx, m_draw_solid_rect_cb,         &p.baseColor, sizeof(p.baseColor), m_frameCostStats_);
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb, &p.xform,   sizeof(p.xform), m_frameCostStats_);

    if (m_currentPSO_ != m_draw_checkerboard_pso_and_srb.pPSO) {
        recordPipelineStateSwitch(m_frameCostStats_);
        ctx->SetPipelineState(m_draw_checkerboard_pso_and_srb.pPSO);
        m_currentPSO_ = m_draw_checkerboard_pso_and_srb.pPSO;
    }
    IBuffer* pBufs[] = { m_draw_solid_rect_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    recordShaderResourceCommit(m_frameCostStats_);
    ctx->CommitShaderResources(m_draw_checkerboard_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    recordDrawCall(m_frameCostStats_);
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
    mapWriteDiscard(ctx, m_draw_solid_rect_vertex_buffer, vertices, sizeof(vertices), m_frameCostStats_);

    struct CBViewerHelper { float spacing; float thickness; float2 padding; float4 color1; float4 color2; };
    CBViewerHelper cb;
    cb.spacing   = p.helper.param0;
    cb.thickness = p.helper.param1;
    cb.padding   = {0.0f, 0.0f};
    cb.color1    = p.helper.color1;
    cb.color2    = {};
    mapWriteDiscard(ctx, m_draw_viewer_helper_cb,       &cb,          sizeof(cb), m_frameCostStats_);
    mapWriteDiscard(ctx, m_draw_solid_rect_cb,          &p.baseColor, sizeof(p.baseColor), m_frameCostStats_);
    mapWriteDiscard(ctx, m_draw_solid_rect_trnsform_cb,  &p.xform,    sizeof(p.xform), m_frameCostStats_);

    if (m_currentPSO_ != m_draw_grid_pso_and_srb.pPSO) {
        recordPipelineStateSwitch(m_frameCostStats_);
        ctx->SetPipelineState(m_draw_grid_pso_and_srb.pPSO);
        m_currentPSO_ = m_draw_grid_pso_and_srb.pPSO;
    }
    IBuffer* pBufs[] = { m_draw_solid_rect_vertex_buffer };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    recordShaderResourceCommit(m_frameCostStats_);
    ctx->CommitShaderResources(m_draw_grid_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    recordDrawCall(m_frameCostStats_);
    ctx->Draw(drawAttrs);
}

void DiligentImmediateSubmitter::submitRectOutline(const RectOutlinePkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !m_draw_solid_rect_pso_and_srb.pPSO) return;
    if (!m_draw_solid_rect_vertex_buffer || !m_draw_solid_rect_cb ||
        !m_draw_solid_rect_trnsform_cb || !m_draw_solid_rect_index_buffer) return;

    const float w = p.xform.scale.x;
    const float h = p.xform.scale.y;
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }

    const float thickness = std::max(1.0f, std::min({1.0f, w * 0.5f, h * 0.5f}));
    const float left = p.xform.offset.x;
    const float top = p.xform.offset.y;

    auto emitStrip = [&](float x, float y, float rw, float rh) {
        if (rw <= 0.0f || rh <= 0.0f) {
            return;
        }
        SolidRectPkt strip{};
        strip.xform = p.xform;
        strip.xform.offset = { x, y };
        strip.xform.scale = { rw, rh };
        strip.color = p.color;
        submitSolidRect(strip, ctx, pRTV);
    };

    emitStrip(left, top, w, thickness);
    if (h > thickness) {
        emitStrip(left, top + h - thickness, w, thickness);
    }

    const float innerH = h - thickness * 2.0f;
    if (innerH > 0.0f) {
        emitStrip(left, top + thickness, thickness, innerH);
        if (w > thickness) {
            emitStrip(left + w - thickness, top + thickness, thickness, innerH);
        }
    }
}

void DiligentImmediateSubmitter::submitSprite(const SpritePkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !p.pSRV || !m_draw_sprite_pso_and_srb.pPSO) return;
    if (!m_draw_sprite_vertex_buffer || !m_draw_sprite_cb) return;

    IBuffer* vb;
    if (p.opacity == 1.0f && m_sprite_unit_quad_vb_) {
        vb = m_sprite_unit_quad_vb_;
    } else {
        SpriteVertex vertices[4] = {
            {{0.0f,0.0f},{0.0f,0.0f},{1,1,1,p.opacity}},
            {{1.0f,0.0f},{1.0f,0.0f},{1,1,1,p.opacity}},
            {{0.0f,1.0f},{0.0f,1.0f},{1,1,1,p.opacity}},
            {{1.0f,1.0f},{1.0f,1.0f},{1,1,1,p.opacity}},
        };
        mapWriteDiscard(ctx, m_draw_sprite_vertex_buffer, vertices, sizeof(vertices), m_frameCostStats_);
        vb = m_draw_sprite_vertex_buffer;
    }
    mapWriteDiscard(ctx, m_draw_sprite_cb, &p.xform, sizeof(p.xform), m_frameCostStats_);

    if (m_currentPSO_ != m_draw_sprite_pso_and_srb.pPSO) {
        recordPipelineStateSwitch(m_frameCostStats_);
        ctx->SetPipelineState(m_draw_sprite_pso_and_srb.pPSO);
        m_currentPSO_ = m_draw_sprite_pso_and_srb.pPSO;
    }
    if (m_var_sprite_gTexture_) m_var_sprite_gTexture_->Set(p.pSRV);
    recordShaderResourceCommit(m_frameCostStats_);
    ctx->CommitShaderResources(m_draw_sprite_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    IBuffer* pBufs[] = { vb };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    recordDrawCall(m_frameCostStats_);
    ctx->Draw(drawAttrs);
}

void DiligentImmediateSubmitter::submitSpriteXform(const SpriteXformPkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !p.pSRV || !m_draw_sprite_transform_pso_and_srb.pPSO) return;
    if (!m_draw_sprite_vertex_buffer || !m_draw_sprite_transform_matrix_cb) return;

    IBuffer* vb;
    if (p.opacity == 1.0f && m_sprite_unit_quad_vb_) {
        vb = m_sprite_unit_quad_vb_;
    } else {
        SpriteVertex vertices[4] = {
            {{0,0},{0,0},{1,1,1,p.opacity}}, {{1,0},{1,0},{1,1,1,p.opacity}},
            {{0,1},{0,1},{1,1,1,p.opacity}}, {{1,1},{1,1},{1,1,1,p.opacity}},
        };
        mapWriteDiscard(ctx, m_draw_sprite_vertex_buffer, vertices, sizeof(vertices), m_frameCostStats_);
        vb = m_draw_sprite_vertex_buffer;
    }
    mapWriteDiscard(ctx, m_draw_sprite_transform_matrix_cb, &p.mat, sizeof(p.mat), m_frameCostStats_);

    if (m_currentPSO_ != m_draw_sprite_transform_pso_and_srb.pPSO) {
        recordPipelineStateSwitch(m_frameCostStats_);
        ctx->SetPipelineState(m_draw_sprite_transform_pso_and_srb.pPSO);
        m_currentPSO_ = m_draw_sprite_transform_pso_and_srb.pPSO;
    }
    if (m_var_spriteXform_gTexture_) m_var_spriteXform_gTexture_->Set(p.pSRV);
    recordShaderResourceCommit(m_frameCostStats_);
    ctx->CommitShaderResources(m_draw_sprite_transform_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    IBuffer* pBufs[] = { vb };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    ctx->SetIndexBuffer(m_draw_solid_rect_index_buffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawIndexedAttribs drawAttrs(6, VT_UINT32, DRAW_FLAG_NONE);
    recordDrawCall(m_frameCostStats_, true);
    ctx->DrawIndexed(drawAttrs);
}

void DiligentImmediateSubmitter::submitMaskedSprite(const MaskedSpritePkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !p.sceneSRV || !p.maskSRV || !m_draw_masked_sprite_pso_and_srb.pPSO) return;
    if (!m_draw_sprite_vertex_buffer || !m_draw_sprite_cb) return;

    IBuffer* vb;
    if (p.opacity == 1.0f && m_sprite_unit_quad_vb_) {
        vb = m_sprite_unit_quad_vb_;
    } else {
        SpriteVertex vertices[4] = {
            {{0,0},{0,0},{1,1,1,p.opacity}}, {{1,0},{1,0},{1,1,1,p.opacity}},
            {{0,1},{0,1},{1,1,1,p.opacity}}, {{1,1},{1,1},{1,1,1,p.opacity}},
        };
        mapWriteDiscard(ctx, m_draw_sprite_vertex_buffer, vertices, sizeof(vertices), m_frameCostStats_);
        vb = m_draw_sprite_vertex_buffer;
    }
    mapWriteDiscard(ctx, m_draw_sprite_cb, &p.xform, sizeof(p.xform), m_frameCostStats_);

    if (m_currentPSO_ != m_draw_masked_sprite_pso_and_srb.pPSO) {
        recordPipelineStateSwitch(m_frameCostStats_);
        ctx->SetPipelineState(m_draw_masked_sprite_pso_and_srb.pPSO);
        m_currentPSO_ = m_draw_masked_sprite_pso_and_srb.pPSO;
    }
    if (m_var_maskedSprite_gScene_) m_var_maskedSprite_gScene_->Set(p.sceneSRV);
    if (m_var_maskedSprite_gMask_)  m_var_maskedSprite_gMask_->Set(p.maskSRV);
    recordShaderResourceCommit(m_frameCostStats_);
    ctx->CommitShaderResources(m_draw_masked_sprite_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    IBuffer* pBufs[] = { vb };
    Uint64   offs[]  = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags       = DRAW_FLAG_NONE;
    recordDrawCall(m_frameCostStats_);
    ctx->Draw(drawAttrs);
}

void DiligentImmediateSubmitter::submitGlyphText(const GlyphTextPkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !ctx || !m_draw_glyph_pso_and_srb.pPSO || !m_draw_glyph_pso_and_srb.pSRB || !m_device) {
        return;
    }
    if (p.text.isEmpty() || p.rect.width() <= 0.0 || p.rect.height() <= 0.0) {
        return;
    }

    const TextStyle style = textStyleFromQFont(p.font);
    const ParagraphStyle paragraph =
        paragraphStyleFromRectAndAlignment(p.rect, static_cast<Qt::Alignment>(p.alignment));
    const auto glyphs = TextLayoutEngine::layout(UniString(p.text), style, paragraph);
    if (glyphs.empty()) {
        return;
    }

    const QFont resolvedFont = FontManager::makeFont(style, p.text);
    const float zoom = std::max(0.001f, p.xform.scale.x);
    struct DrawGlyph {
        GlyphItem item;
        GlyphRect rect;
    };
    std::vector<DrawGlyph> drawGlyphs;
    drawGlyphs.reserve(glyphs.size());

    for (const auto& glyph : glyphs) {
        const GlyphKey key{
            glyph.charCode,
            style.fontSize,
            static_cast<uint32_t>((style.fontWeight == FontWeight::Bold ? 0x1u : 0u) |
                                  (style.fontStyle == FontStyle::Italic ? 0x2u : 0u)),
            style.fontFamily.toQString().toStdString()
        };
        const GlyphRect glyphRect = m_glyph_atlas.acquire(key, resolvedFont);
        if (glyphRect.valid) {
            drawGlyphs.push_back({glyph, glyphRect});
        }
    }

    if (m_glyph_atlas.isDirty() || !m_glyph_atlas_texture) {
        m_glyph_atlas_texture = createAtlasTexture(m_device, m_glyph_atlas.atlasImage());
        m_glyph_atlas_srv = m_glyph_atlas_texture
                                ? m_glyph_atlas_texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)
                                : nullptr;
        if (m_glyph_atlas_texture) {
            m_glyph_atlas.clearDirty();
        }
    }
    if (!m_glyph_atlas_srv) {
        return;
    }

    if (m_currentPSO_ != m_draw_glyph_pso_and_srb.pPSO) {
        recordPipelineStateSwitch(m_frameCostStats_);
        ctx->SetPipelineState(m_draw_glyph_pso_and_srb.pPSO);
        m_currentPSO_ = m_draw_glyph_pso_and_srb.pPSO;
    }
    if (m_var_glyph_gTexture_) m_var_glyph_gTexture_->Set(m_glyph_atlas_srv);
    recordShaderResourceCommit(m_frameCostStats_);
    ctx->CommitShaderResources(m_draw_glyph_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    IBuffer* pBufs[] = { m_draw_sprite_vertex_buffer };
    Uint64 offs[] = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    ctx->SetIndexBuffer(m_draw_solid_rect_index_buffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // ---- Outline pass (8-direction offset): drawn first, behind the fill ----
    if (p.outlineThickness > 0.0f && p.outlineColor.w > 0.0f) {
        const float th = p.outlineThickness;
        // Diagonal directions use th * ~0.707 for uniform perceived thickness
        constexpr float K = 0.7071067811865476f;
        const float2 dirs[8] = {
            { th,    0.0f }, { -th,   0.0f },
            { 0.0f,  th   }, { 0.0f, -th   },
            { th*K,  th*K }, { -th*K, th*K },
            { th*K, -th*K }, { -th*K,-th*K },
        };
        for (const auto& glyph : drawGlyphs) {
            const float left = static_cast<float>(glyph.item.basePosition.x() + glyph.item.offsetPosition.x()) + glyph.rect.bearingX;
            const float top  = static_cast<float>(glyph.item.basePosition.y() + glyph.item.offsetPosition.y()) - glyph.rect.bearingY;
            const float w = std::max(0.0f, static_cast<float>(glyph.rect.width));
            const float h = std::max(0.0f, static_cast<float>(glyph.rect.height));
            if (w <= 0.0f || h <= 0.0f) continue;

            const float alpha = std::clamp(p.opacity * static_cast<float>(glyph.item.offsetOpacity), 0.0f, 1.0f);
            const float4 oc  = { p.outlineColor.x * alpha, p.outlineColor.y * alpha,
                                  p.outlineColor.z * alpha, p.outlineColor.w * alpha };
            SpriteVertex ov[4] = {
                {{0.0f, 0.0f}, {glyph.rect.u0(m_glyph_atlas.width()), glyph.rect.v0(m_glyph_atlas.height())}, oc},
                {{1.0f, 0.0f}, {glyph.rect.u1(m_glyph_atlas.width()), glyph.rect.v0(m_glyph_atlas.height())}, oc},
                {{0.0f, 1.0f}, {glyph.rect.u0(m_glyph_atlas.width()), glyph.rect.v1(m_glyph_atlas.height())}, oc},
                {{1.0f, 1.0f}, {glyph.rect.u1(m_glyph_atlas.width()), glyph.rect.v1(m_glyph_atlas.height())}, oc},
            };
            mapWriteDiscard(ctx, m_draw_sprite_vertex_buffer, ov, sizeof(ov), m_frameCostStats_);
            for (const auto& d : dirs) {
                RenderSolidTransform2D ox{};
                ox.offset = { (left + d.x) * zoom + p.xform.offset.x,
                               (top  + d.y) * zoom + p.xform.offset.y };
                ox.scale     = { w * zoom, h * zoom };
                ox.screenSize = p.xform.screenSize;
                mapWriteDiscard(ctx, m_draw_sprite_cb, &ox, sizeof(ox), m_frameCostStats_);
                recordDrawCall(m_frameCostStats_, true);
                ctx->DrawIndexed(DrawIndexedAttribs(6, VT_UINT32, DRAW_FLAG_NONE));
            }
        }
    }
    // ---- Fill pass (original) ----
    for (const auto& glyph : drawGlyphs) {
        const float left = static_cast<float>(glyph.item.basePosition.x() + glyph.item.offsetPosition.x()) + glyph.rect.bearingX;
        const float top = static_cast<float>(glyph.item.basePosition.y() + glyph.item.offsetPosition.y()) - glyph.rect.bearingY;
        const float w = std::max(0.0f, static_cast<float>(glyph.rect.width));
        const float h = std::max(0.0f, static_cast<float>(glyph.rect.height));
        if (w <= 0.0f || h <= 0.0f) {
            continue;
        }

        const float alpha = std::clamp(p.opacity * static_cast<float>(glyph.item.offsetOpacity), 0.0f, 1.0f);
        const float4 color = { p.color.x * alpha, p.color.y * alpha, p.color.z * alpha, p.color.w * alpha };
        SpriteVertex vertices[4] = {
            {{0.0f, 0.0f}, {glyph.rect.u0(m_glyph_atlas.width()), glyph.rect.v0(m_glyph_atlas.height())}, color},
            {{1.0f, 0.0f}, {glyph.rect.u1(m_glyph_atlas.width()), glyph.rect.v0(m_glyph_atlas.height())}, color},
            {{0.0f, 1.0f}, {glyph.rect.u0(m_glyph_atlas.width()), glyph.rect.v1(m_glyph_atlas.height())}, color},
            {{1.0f, 1.0f}, {glyph.rect.u1(m_glyph_atlas.width()), glyph.rect.v1(m_glyph_atlas.height())}, color},
        };

        RenderSolidTransform2D glyphXform{};
        glyphXform.offset = { left * zoom + p.xform.offset.x, top * zoom + p.xform.offset.y };
        glyphXform.scale = { w * zoom, h * zoom };
        glyphXform.screenSize = p.xform.screenSize;

        mapWriteDiscard(ctx, m_draw_sprite_vertex_buffer, vertices, sizeof(vertices), m_frameCostStats_);
        mapWriteDiscard(ctx, m_draw_sprite_cb, &glyphXform, sizeof(glyphXform), m_frameCostStats_);
        recordDrawCall(m_frameCostStats_, true);
        ctx->DrawIndexed(DrawIndexedAttribs(6, VT_UINT32, DRAW_FLAG_NONE));
    }
}

void DiligentImmediateSubmitter::submitGlyphTextTransformed(const GlyphTextXformPkt& p, IDeviceContext* ctx, ITextureView* pRTV)
{
    if (!pRTV || !ctx || !m_draw_glyph_transform_pso_and_srb.pPSO || !m_draw_glyph_transform_pso_and_srb.pSRB || !m_device) {
        return;
    }
    if (p.text.isEmpty() || p.rect.width() <= 0.0 || p.rect.height() <= 0.0) {
        return;
    }

    const TextStyle style = textStyleFromQFont(p.font);
    const ParagraphStyle paragraph =
        paragraphStyleFromRectAndAlignment(p.rect, static_cast<Qt::Alignment>(p.alignment));
    const auto glyphs = TextLayoutEngine::layout(UniString(p.text), style, paragraph);
    if (glyphs.empty()) {
        return;
    }

    const QFont resolvedFont = FontManager::makeFont(style, p.text);
    struct DrawGlyph {
        GlyphItem item;
        GlyphRect rect;
    };
    std::vector<DrawGlyph> drawGlyphs;
    drawGlyphs.reserve(glyphs.size());

    for (const auto& glyph : glyphs) {
        const GlyphKey key{
            glyph.charCode,
            style.fontSize,
            static_cast<uint32_t>((style.fontWeight == FontWeight::Bold ? 0x1u : 0u) |
                                  (style.fontStyle == FontStyle::Italic ? 0x2u : 0u)),
            style.fontFamily.toQString().toStdString()
        };
        const GlyphRect glyphRect = m_glyph_atlas.acquire(key, resolvedFont);
        if (glyphRect.valid) {
            drawGlyphs.push_back({glyph, glyphRect});
        }
    }

    if (m_glyph_atlas.isDirty() || !m_glyph_atlas_texture) {
        m_glyph_atlas_texture = createAtlasTexture(m_device, m_glyph_atlas.atlasImage());
        m_glyph_atlas_srv = m_glyph_atlas_texture
                                ? m_glyph_atlas_texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)
                                : nullptr;
        if (m_glyph_atlas_texture) {
            m_glyph_atlas.clearDirty();
        }
    }
    if (!m_glyph_atlas_srv) {
        return;
    }

    if (m_currentPSO_ != m_draw_glyph_transform_pso_and_srb.pPSO) {
        recordPipelineStateSwitch(m_frameCostStats_);
        ctx->SetPipelineState(m_draw_glyph_transform_pso_and_srb.pPSO);
        m_currentPSO_ = m_draw_glyph_transform_pso_and_srb.pPSO;
    }
    if (m_var_glyphXform_gTexture_) m_var_glyphXform_gTexture_->Set(m_glyph_atlas_srv);
    recordShaderResourceCommit(m_frameCostStats_);
    ctx->CommitShaderResources(m_draw_glyph_transform_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    IBuffer* pBufs[] = { m_draw_sprite_vertex_buffer };
    Uint64 offs[] = { 0 };
    ctx->SetVertexBuffers(0, 1, pBufs, offs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    ctx->SetIndexBuffer(m_draw_solid_rect_index_buffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // ---- Outline pass (8-direction offset) for transformed text ----
    const float invDpr = (p.devicePixelRatio > 0.0f) ? (1.0f / p.devicePixelRatio) : 1.0f;
    if (p.outlineThickness > 0.0f && p.outlineColor.w > 0.0f) {
        const float th = p.outlineThickness;
        constexpr float K = 0.7071067811865476f;
        const float2 dirs[8] = {
            { th,    0.0f }, { -th,   0.0f },
            { 0.0f,  th   }, { 0.0f, -th   },
            { th*K,  th*K }, { -th*K, th*K },
            { th*K, -th*K }, { -th*K,-th*K },
        };
        for (const auto& glyph : drawGlyphs) {
            const float left = static_cast<float>(glyph.item.basePosition.x() + glyph.item.offsetPosition.x()) + glyph.rect.bearingX * invDpr;
            const float top  = static_cast<float>(glyph.item.basePosition.y() + glyph.item.offsetPosition.y()) - glyph.rect.bearingY * invDpr;
            const float w = std::max(0.0f, static_cast<float>(glyph.rect.width)  * invDpr);
            const float h = std::max(0.0f, static_cast<float>(glyph.rect.height) * invDpr);
            if (w <= 0.0f || h <= 0.0f) continue;

            const float alpha = std::clamp(p.opacity * static_cast<float>(glyph.item.offsetOpacity), 0.0f, 1.0f);
            const float4 oc = { p.outlineColor.x * alpha, p.outlineColor.y * alpha,
                                 p.outlineColor.z * alpha, p.outlineColor.w * alpha };
            SpriteVertex ov[4] = {
                {{0.0f, 0.0f}, {glyph.rect.u0(m_glyph_atlas.width()), glyph.rect.v0(m_glyph_atlas.height())}, oc},
                {{1.0f, 0.0f}, {glyph.rect.u1(m_glyph_atlas.width()), glyph.rect.v0(m_glyph_atlas.height())}, oc},
                {{0.0f, 1.0f}, {glyph.rect.u0(m_glyph_atlas.width()), glyph.rect.v1(m_glyph_atlas.height())}, oc},
                {{1.0f, 1.0f}, {glyph.rect.u1(m_glyph_atlas.width()), glyph.rect.v1(m_glyph_atlas.height())}, oc},
            };
            mapWriteDiscard(ctx, m_draw_sprite_vertex_buffer, ov, sizeof(ov), m_frameCostStats_);
            for (const auto& d : dirs) {
                QMatrix4x4 om = p.transform;
                om.translate(left + d.x, top + d.y, 0.0f);
                om.scale(w, h, 1.0f);
                RenderSolidRectTransform2D omat;
                omat.row0 = { om.row(0).x(), om.row(0).y(), om.row(0).z(), om.row(0).w() };
                omat.row1 = { om.row(1).x(), om.row(1).y(), om.row(1).z(), om.row(1).w() };
                omat.row2 = { om.row(2).x(), om.row(2).y(), om.row(2).z(), om.row(2).w() };
                omat.row3 = { om.row(3).x(), om.row(3).y(), om.row(3).z(), om.row(3).w() };
                mapWriteDiscard(ctx, m_draw_sprite_transform_matrix_cb, &omat, sizeof(omat), m_frameCostStats_);
                recordDrawCall(m_frameCostStats_, true);
                ctx->DrawIndexed(DrawIndexedAttribs(6, VT_UINT32, DRAW_FLAG_NONE));
            }
        }
    }
    // ---- Fill pass (original) ----
    for (const auto& glyph : drawGlyphs) {
        const float left = static_cast<float>(glyph.item.basePosition.x() + glyph.item.offsetPosition.x()) + glyph.rect.bearingX * invDpr;
        const float top = static_cast<float>(glyph.item.basePosition.y() + glyph.item.offsetPosition.y()) - glyph.rect.bearingY * invDpr;
        const float w = std::max(0.0f, static_cast<float>(glyph.rect.width)  * invDpr);
        const float h = std::max(0.0f, static_cast<float>(glyph.rect.height) * invDpr);
        if (w <= 0.0f || h <= 0.0f) {
            continue;
        }

        const float alpha = std::clamp(p.opacity * static_cast<float>(glyph.item.offsetOpacity), 0.0f, 1.0f);
        const float4 color = { p.color.x * alpha, p.color.y * alpha, p.color.z * alpha, p.color.w * alpha };
        SpriteVertex vertices[4] = {
            {{0.0f, 0.0f}, {glyph.rect.u0(m_glyph_atlas.width()), glyph.rect.v0(m_glyph_atlas.height())}, color},
            {{1.0f, 0.0f}, {glyph.rect.u1(m_glyph_atlas.width()), glyph.rect.v0(m_glyph_atlas.height())}, color},
            {{0.0f, 1.0f}, {glyph.rect.u0(m_glyph_atlas.width()), glyph.rect.v1(m_glyph_atlas.height())}, color},
            {{1.0f, 1.0f}, {glyph.rect.u1(m_glyph_atlas.width()), glyph.rect.v1(m_glyph_atlas.height())}, color},
        };

        QMatrix4x4 glyphMat = p.transform;
        glyphMat.translate(left, top, 0.0f);
        glyphMat.scale(w, h, 1.0f);

        // Pack rows explicitly: QMatrix4x4 data() is column-major, shader expects row-major
        RenderSolidRectTransform2D mat;
        mat.row0 = { glyphMat.row(0).x(), glyphMat.row(0).y(), glyphMat.row(0).z(), glyphMat.row(0).w() };
        mat.row1 = { glyphMat.row(1).x(), glyphMat.row(1).y(), glyphMat.row(1).z(), glyphMat.row(1).w() };
        mat.row2 = { glyphMat.row(2).x(), glyphMat.row(2).y(), glyphMat.row(2).z(), glyphMat.row(2).w() };
        mat.row3 = { glyphMat.row(3).x(), glyphMat.row(3).y(), glyphMat.row(3).z(), glyphMat.row(3).w() };
        mapWriteDiscard(ctx, m_draw_sprite_vertex_buffer, vertices, sizeof(vertices), m_frameCostStats_);
        mapWriteDiscard(ctx, m_draw_sprite_transform_matrix_cb, &mat, sizeof(mat), m_frameCostStats_);
        recordDrawCall(m_frameCostStats_, true);
        ctx->DrawIndexed(DrawIndexedAttribs(6, VT_UINT32, DRAW_FLAG_NONE));
    }
}

} // namespace Artifact
