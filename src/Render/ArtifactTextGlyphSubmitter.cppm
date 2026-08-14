module;
#include <RefCntAutoPtr.hpp>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <Buffer.h>
#include <Texture.h>
#include <QFont>
#include <QImage>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
module Artifact.Render.TextGlyphSubmitter;

import Artifact.Render.TextGlyphSubmitter.Contract;
import Text.GlyphAtlas;
import Font.FreeFont;

namespace Artifact {

class ArtifactTextGlyphSubmitter::Impl {
public:
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    ArtifactTextGlyphPipelineProvider pipelines;
    ArtifactCore::GlyphAtlas atlas;
    Diligent::RefCntAutoPtr<Diligent::ITexture> atlasTexture;
};

struct SubmitVertex { float pos[2]; float uv[2]; float color[4]; };
struct SubmitTransform { float offset[2]; float scale[2]; float screenSize[2]; };

ArtifactTextGlyphSubmitter::ArtifactTextGlyphSubmitter() = default;
ArtifactTextGlyphSubmitter::~ArtifactTextGlyphSubmitter() { destroy(); }

bool ArtifactTextGlyphSubmitter::initialize(
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device,
    Diligent::TEXTURE_FORMAT,
    ArtifactTextGlyphPipelineProvider pipelines) {
    destroy();
    if (!device || !pipelines.isValid()) return false;
    impl_ = new Impl();
    impl_->device = std::move(device);
    impl_->pipelines = pipelines;
    return true;
}

bool ArtifactTextGlyphSubmitter::isInitialized() const { return impl_ != nullptr; }
void ArtifactTextGlyphSubmitter::clear(Diligent::IDeviceContext* context,
                                       Diligent::ITextureView* target,
                                       const ArtifactCore::FloatColor& color) {
    if (!isInitialized() || !context || !target) return;
    const float value[4] = {color.r(), color.g(), color.b(), color.a()};
    context->ClearRenderTarget(target, value, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}
bool ArtifactTextGlyphSubmitter::submit(Diligent::IDeviceContext* context, Diligent::ITextureView* target,
                                        std::span<const ArtifactCore::GlyphItem> glyphs,
                                        const ArtifactCore::TextStyle& style,
                                        const ArtifactCore::FloatColor& color, float opacity) {
    if (!isInitialized() || !context || !target || glyphs.empty()) return false;
    impl_->atlas.clear();
    const auto targetDesc = target->GetTexture()->GetDesc();
    const float screenW = static_cast<float>(targetDesc.Width);
    const float screenH = static_cast<float>(targetDesc.Height);
    std::vector<SubmitVertex> vertices;
    for (const auto& glyph : glyphs) {
        if (glyph.isEmojiSequence && glyph.shapedGlyphIndex == 0) {
            continue;
        }
        const bool regional = glyph.charCode >= 0x1F1E6 && glyph.charCode <= 0x1F1FF;
        if (glyph.isEmojiSequence && glyph.clusterIndex >= 0 &&
            !glyph.shapedGlyphIndices.empty() && glyph.shapedGlyphIndex != glyph.shapedGlyphIndices.front()) {
            continue;
        }
        const QString text = glyph.clusterText.isEmpty()
                                  ? QString::fromUcs4(&glyph.charCode, 1)
                                  : glyph.clusterText;
        const QFont font = ArtifactCore::FontManager::makeFont(style, text);
        ArtifactCore::GlyphKey key;
        key.codePoint = glyph.charCode; key.fontSize = style.fontSize;
        key.fontFamily = font.family().toStdString();
        // Sequence rasterization is not enabled until the full DirectWrite
        // color run has been shaped; scalar color glyphs remain the explicit
        // fallback and ZWJ controls are skipped above.
        key.sequenceUtf8 = {};
        key.shapedGlyphIndex = glyph.shapedGlyphIndex;
        if (glyph.shapedGlyphIndices.size() > 1) {
            key.shapedGlyphIndices = glyph.shapedGlyphIndices;
        }
        key.renderMode = glyph.renderMode;
        const auto rect = impl_->atlas.acquire(key, font);
        if (!rect.valid) continue;
        const float x0 = static_cast<float>(glyph.basePosition.x() + glyph.offsetPosition.x() + rect.bearingX);
        const float y0 = static_cast<float>(glyph.basePosition.y() + glyph.offsetPosition.y() - rect.bearingY);
        const float x1 = x0 + rect.width * glyph.offsetScale;
        const float y1 = y0 + rect.height * glyph.offsetScale;
        const float u0 = rect.u0(impl_->atlas.width()), v0 = rect.v0(impl_->atlas.height());
        const float u1 = rect.u1(impl_->atlas.width()), v1 = rect.v1(impl_->atlas.height());
        const float alpha = rect.colorPreserved ? -std::clamp(opacity * glyph.offsetOpacity, 0.0f, 1.0f)
                                                : std::clamp(opacity * glyph.offsetOpacity, 0.0f, 1.0f);
        const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
        const float radians = glyph.offsetRotation * 0.0174532925199433f;
        const float cs = std::cos(radians), sn = std::sin(radians);
        const auto rotate = [cx, cy, cs, sn](float x, float y) {
            const float dx = x - cx, dy = y - cy;
            return std::array<float, 2>{cx + dx * cs - dy * sn, cy + dx * sn + dy * cs};
        };
        const auto p0 = rotate(x0, y0), p1 = rotate(x1, y0), p2 = rotate(x0, y1), p3 = rotate(x1, y1);
        vertices.push_back({{p0[0],p0[1]},{u0,v0},{color.r(),color.g(),color.b(),alpha}});
        vertices.push_back({{p1[0],p1[1]},{u1,v0},{color.r(),color.g(),color.b(),alpha}});
        vertices.push_back({{p2[0],p2[1]},{u0,v1},{color.r(),color.g(),color.b(),alpha}});
        vertices.push_back({{p3[0],p3[1]},{u1,v1},{color.r(),color.g(),color.b(),alpha}});
    }
    if (vertices.empty()) return false;
    const QImage& image = impl_->atlas.atlasImage();
    Diligent::TextureDesc td; td.Name = "ArtifactTextSubmitterAtlas";
    td.Type = Diligent::RESOURCE_DIM_TEX_2D; td.Width = image.width(); td.Height = image.height();
    td.MipLevels = 1; td.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    td.Usage = Diligent::USAGE_IMMUTABLE; td.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData sub{image.constBits(), static_cast<Diligent::Uint32>(image.bytesPerLine())};
    Diligent::TextureData texData{&sub, 1};
    impl_->atlasTexture.Release(); impl_->device->CreateTexture(td, &texData, &impl_->atlasTexture);
    auto* atlasView = impl_->atlasTexture ? impl_->atlasTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE) : nullptr;
    if (!atlasView) return false;
    Diligent::BufferDesc vbDesc; vbDesc.Name = "ArtifactTextSubmitterVB";
    vbDesc.Size = vertices.size() * sizeof(SubmitVertex); vbDesc.Usage = Diligent::USAGE_IMMUTABLE; vbDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
    Diligent::BufferData vbData{vertices.data(), vbDesc.Size}; Diligent::RefCntAutoPtr<Diligent::IBuffer> vb;
    impl_->device->CreateBuffer(vbDesc, &vbData, &vb);
    SubmitTransform transform{{0,0},{1,1},{screenW,screenH}};
    Diligent::BufferDesc cbDesc; cbDesc.Name = "ArtifactTextSubmitterCB"; cbDesc.Size = sizeof(transform);
    cbDesc.Usage = Diligent::USAGE_IMMUTABLE; cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
    Diligent::BufferData cbData{&transform, sizeof(transform)}; Diligent::RefCntAutoPtr<Diligent::IBuffer> cb;
    impl_->device->CreateBuffer(cbDesc, &cbData, &cb);
    if (!vb || !cb) return false;
    auto* srb = impl_->pipelines.glyphBinding;
    srb->GetVariableByName(Diligent::SHADER_TYPE_VERTEX, "TransformCB")->Set(cb);
    srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_texture")->Set(atlasView);
    srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_sampler")->Set(impl_->pipelines.atlasSampler);
    context->SetRenderTargets(1, &target, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context->SetPipelineState(impl_->pipelines.glyphPipeline);
    context->CommitShaderResources(srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Diligent::IBuffer* buffers[] = {vb.RawPtr()}; Diligent::Uint64 offsets[] = {0};
    context->SetVertexBuffers(0, 1, buffers, offsets, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                              Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    for (Diligent::Uint32 i = 0; i < vertices.size() / 4; ++i)
        context->Draw(Diligent::DrawAttribs{4, Diligent::DRAW_FLAG_VERIFY_ALL, 1, i * 4});
    return true;
}
void ArtifactTextGlyphSubmitter::flush(Diligent::IDeviceContext* context) { if (context) context->Flush(); }
void ArtifactTextGlyphSubmitter::destroy() { delete impl_; impl_ = nullptr; }

}
