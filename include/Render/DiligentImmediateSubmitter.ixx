module;
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <Texture.h>
#include <Sampler.h>
#include <RefCntAutoPtr.hpp>
#include <BasicMath.hpp>
#include <QFont>
export module Artifact.Render.DiligentImmediateSubmitter;

import Artifact.Render.IRenderSubmitter;
import Artifact.Render.RenderCommandBuffer;
import Artifact.Render.ShaderManager;
import Text.GlyphAtlas;
import Text.GlyphLayout;
import Font.FreeFont;
import Graphics;

export namespace Artifact {

using namespace Diligent;
using namespace ArtifactCore;

class DiligentImmediateSubmitter final : public IRenderSubmitter {
public:
    DiligentImmediateSubmitter() = default;
    ~DiligentImmediateSubmitter() override = default;

    void createBuffers(RefCntAutoPtr<IRenderDevice> device, TEXTURE_FORMAT rtvFormat);
    void setPSOs(ShaderManager& shaderManager);
    void destroy();

    void submit(RenderCommandBuffer& buf, IDeviceContext* ctx) override;

private:
    RefCntAutoPtr<IRenderDevice> m_device;

    // ---- Vertex Buffers ----
    RefCntAutoPtr<IBuffer> m_draw_sprite_vertex_buffer;
    RefCntAutoPtr<IBuffer> m_draw_solid_rect_vertex_buffer;
    RefCntAutoPtr<IBuffer> m_draw_solid_triangle_vertex_buffer;
    RefCntAutoPtr<IBuffer> m_draw_solid_circle_vertex_buffer;
    RefCntAutoPtr<IBuffer> m_draw_thick_line_vertex_buffer;
    RefCntAutoPtr<IBuffer> m_draw_dot_line_vertex_buffer;

    // ---- Index Buffers ----
    RefCntAutoPtr<IBuffer> m_draw_solid_rect_index_buffer;

    // ---- Constant Buffers ----
    RefCntAutoPtr<IBuffer> m_draw_sprite_cb;
    RefCntAutoPtr<IBuffer> m_draw_sprite_transform_matrix_cb;
    RefCntAutoPtr<IBuffer> m_draw_solid_rect_cb;
    RefCntAutoPtr<IBuffer> m_draw_solid_rect_trnsform_cb;
    RefCntAutoPtr<IBuffer> m_draw_solid_rect_transform_matrix_cb;
    RefCntAutoPtr<IBuffer> m_draw_viewer_helper_cb;
    RefCntAutoPtr<IBuffer> m_draw_dot_line_cb;
    RefCntAutoPtr<IBuffer> m_draw_outline_params_cb;

    // ---- Sampler ----
    RefCntAutoPtr<ISampler> m_sprite_sampler;
    RefCntAutoPtr<ISampler> m_glyph_sampler;
    RefCntAutoPtr<ITexture> m_glyph_atlas_texture;
    RefCntAutoPtr<ITextureView> m_glyph_atlas_srv;
    GlyphAtlas m_glyph_atlas;

    // ---- PSOs ----
    PSOAndSRB m_draw_sprite_pso_and_srb;
    PSOAndSRB m_draw_sprite_transform_pso_and_srb;
    PSOAndSRB m_draw_masked_sprite_pso_and_srb;
    PSOAndSRB m_draw_glyph_pso_and_srb;
    PSOAndSRB m_draw_solid_rect_pso_and_srb;
    PSOAndSRB m_draw_solid_rect_transform_pso_and_srb;
    PSOAndSRB m_draw_line_pso_and_srb;
    PSOAndSRB m_draw_thick_line_pso_and_srb;
    PSOAndSRB m_draw_dot_line_pso_and_srb;
    PSOAndSRB m_draw_solid_triangle_pso_and_srb;
    PSOAndSRB m_draw_checkerboard_pso_and_srb;
    PSOAndSRB m_draw_grid_pso_and_srb;
    PSOAndSRB m_draw_rect_outline_pso_and_srb;

    // ---- Submit helpers (one per packet type) ----
    void submitSolidRect     (const SolidRectPkt&,      IDeviceContext*, ITextureView*);
    void submitSolidRectXform(const SolidRectXformPkt&, IDeviceContext*, ITextureView*);
    void submitLine          (const LinePkt&,           IDeviceContext*, ITextureView*);
    void submitQuad          (const QuadPkt&,           IDeviceContext*, ITextureView*);
    void submitDotLine       (const DotLinePkt&,        IDeviceContext*, ITextureView*);
    void submitSolidTri      (const SolidTriPkt&,       IDeviceContext*, ITextureView*);
    void submitSolidCircle   (const SolidCirclePkt&,    IDeviceContext*, ITextureView*);
    void submitCheckerboard  (const CheckerboardPkt&,   IDeviceContext*, ITextureView*);
    void submitGrid          (const GridPkt&,           IDeviceContext*, ITextureView*);
    void submitRectOutline   (const RectOutlinePkt&,    IDeviceContext*, ITextureView*);
    void submitSprite        (const SpritePkt&,         IDeviceContext*, ITextureView*);
    void submitSpriteXform   (const SpriteXformPkt&,    IDeviceContext*, ITextureView*);
    void submitMaskedSprite  (const MaskedSpritePkt&,   IDeviceContext*, ITextureView*);
    void submitGlyphText     (const GlyphTextPkt&,      IDeviceContext*, ITextureView*);
};

} // namespace Artifact
