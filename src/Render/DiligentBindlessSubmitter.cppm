module;
#include <algorithm>
#include <cstring>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include <QString>
#include <Common/interface/BasicMath.hpp>
#include <Buffer.h>
#include <DeviceContext.h>
#include <PipelineState.h>
#include <RenderDevice.h>
#include <RefCntAutoPtr.hpp>
#include <Sampler.h>
#include <Shader.h>
#include <ShaderResourceBinding.h>
#include <ShaderResourceVariable.h>
#include <TextureView.h>

module Artifact.Render.DiligentBindlessSubmitter;

import Artifact.Render.ShaderManager;

namespace Artifact {

using namespace Diligent;

namespace {
constexpr const char* kBindlessSpriteVS = R"HLSL(
cbuffer TransformCB : register(b0)
{
    float2 offset;
    float2 scale;
    float2 screenSize;
};

struct VSInput
{
    float2 pos          : ATTRIB0;
    float2 uv           : ATTRIB1;
    float4 color        : ATTRIB2;
    uint   textureIndex : ATTRIB3;
};

struct PSInput
{
    float4 pos          : SV_POSITION;
    float2 uv           : TEXCOORD0;
    float4 color        : COLOR0;
    nointerpolation uint textureIndex : TEXCOORD1;
};

PSInput main(VSInput input)
{
    PSInput output;
    float2 pos = input.pos * scale + offset;
    float2 ndc = pos / screenSize * 2.0f - float2(1.0f, 1.0f);
    ndc.y = -ndc.y;
    output.pos = float4(ndc, 0.0f, 1.0f);
    output.uv = input.uv;
    output.color = input.color;
    output.textureIndex = input.textureIndex;
    return output;
}
)HLSL";

constexpr const char* kBindlessSpritePS = R"HLSL(
struct PSInput
{
    float4 pos          : SV_POSITION;
    float2 uv           : TEXCOORD0;
    float4 color        : COLOR0;
    nointerpolation uint textureIndex : TEXCOORD1;
};

Texture2D g_textures[128] : register(t0);
SamplerState g_sampler : register(s0);

float4 main(PSInput input) : SV_TARGET
{
    float4 sampled = g_textures[input.textureIndex].Sample(g_sampler, input.uv);
    float dx = min(input.uv.x, 1.0 - input.uv.x);
    float dy = min(input.uv.y, 1.0 - input.uv.y);
    float d = min(dx, dy);
    float2 fw = fwidth(input.uv);
    float edgeWidth = max(fw.x, fw.y);
    float alphaMultiplier = (edgeWidth > 0.0001) ? smoothstep(0.0, edgeWidth, d) : 1.0;
    return sampled * input.color * float4(1.0, 1.0, 1.0, alphaMultiplier);
}
)HLSL";

bool deviceCanUseBindless(IRenderDevice* device)
{
    if (!device) {
        return false;
    }

    const auto& info = device->GetDeviceInfo();
    return (info.Type == RENDER_DEVICE_TYPE_D3D12 ||
            info.Type == RENDER_DEVICE_TYPE_VULKAN) &&
           info.Features.BindlessResources != DEVICE_FEATURE_STATE_DISABLED;
}

void mapWriteDiscard(IDeviceContext* ctx, IBuffer* buf, const void* data, size_t size)
{
    void* mapped = nullptr;
    ctx->MapBuffer(buf, MAP_WRITE, MAP_FLAG_DISCARD, mapped);
    std::memcpy(mapped, data, size);
    ctx->UnmapBuffer(buf, MAP_WRITE);
}
} // namespace

void DiligentBindlessSubmitter::createBuffers(RefCntAutoPtr<IRenderDevice> device,
                                              TEXTURE_FORMAT rtvFormat)
{
    device_ = device;
    supported_ = deviceCanUseBindless(device.RawPtr());
    fallback_.createBuffers(device, rtvFormat);
    if (supported_) {
        createSpriteResources(rtvFormat);
    }
}

void DiligentBindlessSubmitter::setPSOs(ShaderManager& shaderManager)
{
    fallback_.setPSOs(shaderManager);
}

void DiligentBindlessSubmitter::destroy()
{
    fallback_.destroy();
    spritePso_ = nullptr;
    spriteSrb_ = nullptr;
    spriteVertexBuffer_ = nullptr;
    spriteTransformBuffer_ = nullptr;
    spriteSampler_ = nullptr;
    spriteTexturesVar_ = nullptr;
    spriteVertices_.clear();
    spriteBatchScreenSize_ = {0.0f, 0.0f};
    resetTextureTable();
    device_ = nullptr;
    supported_ = false;
}

void DiligentBindlessSubmitter::setFrameCostStats(ArtifactCore::RenderCostStats* stats)
{
    frameCostStats_ = stats;
    fallback_.setFrameCostStats(stats);
}

void DiligentBindlessSubmitter::setDeferredContext(RefCntAutoPtr<IDeviceContext> deferred)
{
    fallback_.setDeferredContext(std::move(deferred));
}

void DiligentBindlessSubmitter::setPrimitiveRenderer3D(PrimitiveRenderer3D* renderer)
{
    fallback_.setPrimitiveRenderer3D(renderer);
}

void DiligentBindlessSubmitter::setParticleRenderer(ArtifactCore::ParticleRenderer* renderer)
{
    fallback_.setParticleRenderer(renderer);
}

bool DiligentBindlessSubmitter::isSupported() const
{
    return supported_;
}

QString DiligentBindlessSubmitter::debugState() const
{
    return supported_
               ? QStringLiteral("mode=bindless state=sprite-only fallback=legacy")
               : QStringLiteral("mode=bindless state=unsupported fallback=legacy");
}

void DiligentBindlessSubmitter::submit(RenderCommandBuffer& buf, IDeviceContext* ctx)
{
    if (!supported_ || !ctx || buf.empty() || !buf.targetRTV || !spritePso_ || !spriteSrb_) {
        fallback_.submit(buf, ctx);
        return;
    }

    auto* rtv = buf.targetRTV;
    const bool spriteOnly = std::all_of(
        buf.packets().begin(), buf.packets().end(), [](const DrawPacket& packet) {
            return std::holds_alternative<SpritePkt>(packet);
        });
    if (!spriteOnly) {
        fallback_.submit(buf, ctx);
        return;
    }

    for (const auto& packet : buf.packets()) {
        const auto* sprite = std::get_if<SpritePkt>(&packet);
        if (!sprite || !submitSprite(*sprite, ctx, rtv)) {
            flushSpriteBatch(ctx, rtv);
            fallback_.submit(buf, ctx);
            return;
        }
    }

    flushSpriteBatch(ctx, rtv);
    buf.reset();
}

void DiligentBindlessSubmitter::createSpriteResources(TEXTURE_FORMAT rtvFormat)
{
    if (!device_) {
        return;
    }

    ShaderCreateInfo vsInfo;
    vsInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    vsInfo.Desc.ShaderType = SHADER_TYPE_VERTEX;
    vsInfo.Desc.Name = "BindlessSprite VS";
    vsInfo.EntryPoint = "main";
    vsInfo.Source = kBindlessSpriteVS;
    vsInfo.SourceLength = static_cast<Uint32>(std::strlen(kBindlessSpriteVS));

    ShaderCreateInfo psInfo;
    psInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    psInfo.Desc.ShaderType = SHADER_TYPE_PIXEL;
    psInfo.Desc.Name = "BindlessSprite PS";
    psInfo.EntryPoint = "main";
    psInfo.Source = kBindlessSpritePS;
    psInfo.SourceLength = static_cast<Uint32>(std::strlen(kBindlessSpritePS));

    RefCntAutoPtr<IShader> vs;
    RefCntAutoPtr<IShader> ps;
    device_->CreateShader(vsInfo, &vs);
    device_->CreateShader(psInfo, &ps);
    if (!vs || !ps) {
        supported_ = false;
        return;
    }

    static const LayoutElement layout[] = {
        LayoutElement{0, 0, 2, VT_FLOAT32, false},
        LayoutElement{1, 0, 2, VT_FLOAT32, false},
        LayoutElement{2, 0, 4, VT_FLOAT32, false},
        LayoutElement{3, 0, 1, VT_UINT32, false}
    };
    static const ShaderResourceVariableDesc vars[] = {
        {SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_textures", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_sampler", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    };

    GraphicsPipelineStateCreateInfo psoInfo;
    psoInfo.PSODesc.Name = "BindlessSprite PSO";
    psoInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    psoInfo.pVS = vs;
    psoInfo.pPS = ps;
    auto& gp = psoInfo.GraphicsPipeline;
    gp.NumRenderTargets = 1;
    gp.RTVFormats[0] = rtvFormat;
    gp.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    gp.RasterizerDesc.CullMode = CULL_MODE_NONE;
    gp.DepthStencilDesc.DepthEnable = False;
    auto& blend = gp.BlendDesc.RenderTargets[0];
    blend.BlendEnable = True;
    blend.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    blend.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    blend.BlendOp = BLEND_OPERATION_ADD;
    blend.SrcBlendAlpha = BLEND_FACTOR_ONE;
    blend.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
    blend.BlendOpAlpha = BLEND_OPERATION_ADD;
    blend.RenderTargetWriteMask = COLOR_MASK_ALL;
    gp.InputLayout.LayoutElements = layout;
    gp.InputLayout.NumElements = _countof(layout);
    psoInfo.PSODesc.ResourceLayout.Variables = vars;
    psoInfo.PSODesc.ResourceLayout.NumVariables = _countof(vars);

    device_->CreateGraphicsPipelineState(psoInfo, &spritePso_);
    if (!spritePso_) {
        supported_ = false;
        return;
    }
    spritePso_->CreateShaderResourceBinding(&spriteSrb_, true);
    if (!spriteSrb_) {
        supported_ = false;
        return;
    }

    BufferDesc vbDesc;
    vbDesc.Name = "BindlessSprite VB";
    vbDesc.Usage = USAGE_DYNAMIC;
    vbDesc.BindFlags = BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    vbDesc.Size = sizeof(BindlessSpriteVertex) * kMaxSpriteBatch * 6;
    device_->CreateBuffer(vbDesc, nullptr, &spriteVertexBuffer_);

    BufferDesc cbDesc;
    cbDesc.Name = "BindlessSprite TransformCB";
    cbDesc.Usage = USAGE_DYNAMIC;
    cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    cbDesc.Size = sizeof(RenderSolidTransform2D);
    device_->CreateBuffer(cbDesc, nullptr, &spriteTransformBuffer_);

    SamplerDesc samplerDesc;
    samplerDesc.MinFilter = FILTER_TYPE_LINEAR;
    samplerDesc.MagFilter = FILTER_TYPE_LINEAR;
    samplerDesc.MipFilter = FILTER_TYPE_LINEAR;
    samplerDesc.AddressU = TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = TEXTURE_ADDRESS_CLAMP;
    device_->CreateSampler(samplerDesc, &spriteSampler_);

    if (auto* var = spriteSrb_->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")) {
        var->Set(spriteTransformBuffer_);
    }
    if (auto* var = spriteSrb_->GetVariableByName(SHADER_TYPE_PIXEL, "g_sampler")) {
        var->Set(spriteSampler_);
    }
    spriteTexturesVar_ = spriteSrb_->GetVariableByName(SHADER_TYPE_PIXEL, "g_textures");
    spriteVertices_.reserve(kMaxSpriteBatch * 6);
    resetTextureTable();
    supported_ = spriteVertexBuffer_ && spriteTransformBuffer_ && spriteTexturesVar_;
}

void DiligentBindlessSubmitter::resetTextureTable()
{
    textureObjects_.fill(nullptr);
    textureIndices_.clear();
}

bool DiligentBindlessSubmitter::ensureTextureIndex(ITextureView* srv, Uint32& index)
{
    if (!srv) {
        return false;
    }
    const auto it = textureIndices_.find(srv);
    if (it != textureIndices_.end()) {
        index = it->second;
        return true;
    }
    if (textureIndices_.size() >= kMaxBindlessTextures) {
        return false;
    }
    index = static_cast<Uint32>(textureIndices_.size());
    textureIndices_.emplace(srv, index);
    textureObjects_[index] = srv;
    return true;
}

bool DiligentBindlessSubmitter::submitSprite(const SpritePkt& pkt, IDeviceContext* ctx, ITextureView* rtv)
{
    if (!ctx || !rtv || !pkt.pSRV || !spriteVertexBuffer_ || !spriteTransformBuffer_) {
        return false;
    }
    if (!spriteVertices_.empty() &&
        (spriteBatchScreenSize_.x != pkt.xform.screenSize.x ||
         spriteBatchScreenSize_.y != pkt.xform.screenSize.y)) {
        flushSpriteBatch(ctx, rtv);
    }
    if (spriteVertices_.size() + 6 > kMaxSpriteBatch * 6) {
        flushSpriteBatch(ctx, rtv);
    }

    Uint32 textureIndex = 0;
    if (!ensureTextureIndex(pkt.pSRV, textureIndex)) {
        flushSpriteBatch(ctx, rtv);
        resetTextureTable();
        if (!ensureTextureIndex(pkt.pSRV, textureIndex)) {
            return false;
        }
    }

    if (spriteVertices_.empty()) {
        spriteBatchScreenSize_ = pkt.xform.screenSize;
    }

    const float4 color{1.0f, 1.0f, 1.0f, pkt.opacity};
    const float left = pkt.xform.offset.x;
    const float top = pkt.xform.offset.y;
    const float right = pkt.xform.offset.x + pkt.xform.scale.x;
    const float bottom = pkt.xform.offset.y + pkt.xform.scale.y;
    const BindlessSpriteVertex v0{{left, top}, {0.0f, 0.0f}, color, textureIndex};
    const BindlessSpriteVertex v1{{right, top}, {1.0f, 0.0f}, color, textureIndex};
    const BindlessSpriteVertex v2{{left, bottom}, {0.0f, 1.0f}, color, textureIndex};
    const BindlessSpriteVertex v3{{right, bottom}, {1.0f, 1.0f}, color, textureIndex};
    spriteVertices_.push_back(v0);
    spriteVertices_.push_back(v1);
    spriteVertices_.push_back(v2);
    spriteVertices_.push_back(v2);
    spriteVertices_.push_back(v1);
    spriteVertices_.push_back(v3);

    if (spriteVertices_.size() == 6) {
        RenderSolidTransform2D batchXform;
        batchXform.offset = {0.0f, 0.0f};
        batchXform.scale = {1.0f, 1.0f};
        batchXform.screenSize = spriteBatchScreenSize_;
        mapWriteDiscard(ctx, spriteTransformBuffer_, &batchXform, sizeof(batchXform));
        if (frameCostStats_) {
            ++frameCostStats_->bufferUpdates;
        }
    }
    return true;
}

void DiligentBindlessSubmitter::flushSpriteBatch(IDeviceContext* ctx, ITextureView* rtv)
{
    if (spriteVertices_.empty() || !ctx || !rtv || !spriteTexturesVar_ || !spriteSrb_ || !spritePso_) {
        spriteVertices_.clear();
        spriteBatchScreenSize_ = {0.0f, 0.0f};
        resetTextureTable();
        return;
    }

    mapWriteDiscard(ctx, spriteVertexBuffer_, spriteVertices_.data(),
                    sizeof(BindlessSpriteVertex) * spriteVertices_.size());
    if (frameCostStats_) {
        ++frameCostStats_->bufferUpdates;
    }
    spriteTexturesVar_->SetArray(textureObjects_.data(), 0, static_cast<Uint32>(textureIndices_.size()));
    ctx->SetRenderTargets(1, &rtv, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ctx->SetPipelineState(spritePso_);
    if (frameCostStats_) {
        ++frameCostStats_->psoSwitches;
        ++frameCostStats_->srbCommits;
    }
    ctx->CommitShaderResources(spriteSrb_, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    IBuffer* buffers[] = {spriteVertexBuffer_};
    Uint64 offsets[] = {0};
    ctx->SetVertexBuffers(0, 1, buffers, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    DrawAttribs draw;
    draw.NumVertices = static_cast<Uint32>(spriteVertices_.size());
    draw.Flags = DRAW_FLAG_NONE;
    if (frameCostStats_) {
        ++frameCostStats_->drawCalls;
    }
    ctx->Draw(draw);
    spriteVertices_.clear();
    spriteBatchScreenSize_ = {0.0f, 0.0f};
    resetTextureTable();
}

} // namespace Artifact
