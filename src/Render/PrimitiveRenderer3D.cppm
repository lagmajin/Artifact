module;
#include <array>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <QImage>
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <QDebug>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Sampler.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>

module Artifact.Render.PrimitiveRenderer3D;

namespace Artifact {

using namespace Diligent;
using namespace ArtifactCore;

namespace {
constexpr std::array<float, 16> kIdentityMatrix = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

constexpr const char* kBillboardVSSource = R"(
cbuffer BillboardCB : register(b0)
{
    float4x4 g_View;
    float4x4 g_Proj;
    float4   g_CenterAndRoll;
    float4   g_SizeAndOpacity;
    float4   g_Tint;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct VSOut
{
    float4 Pos   : SV_Position;
    float2 UV    : TEXCOORD0;
    float4 Color : COLOR0;
};

static const float2 kOffsets[4] = {
    float2(-0.5, -0.5),
    float2(-0.5,  0.5),
    float2( 0.5, -0.5),
    float2( 0.5,  0.5)
};

VSOut main(uint vertexID : SV_VertexID)
{
    VSOut Out;
    float2 offset = kOffsets[vertexID] * g_SizeAndOpacity.xy;

    float roll = g_CenterAndRoll.w;
    if (abs(roll) > 1e-5)
    {
        float s = sin(roll);
        float c = cos(roll);
        offset = float2(offset.x * c - offset.y * s,
                        offset.x * s + offset.y * c);
    }

    float4 viewPos = mul(float4(g_CenterAndRoll.xyz, 1.0), g_View);
    viewPos.xy += offset;

    Out.Pos = mul(viewPos, g_Proj);
    Out.UV = kOffsets[vertexID] + 0.5;
    Out.Color = g_Tint;
    Out.Color.a *= g_SizeAndOpacity.z;
    return Out;
}
)";

constexpr const char* kBillboardPSSource = R"(
struct PSIn
{
    float4 Pos   : SV_Position;
    float2 UV    : TEXCOORD0;
    float4 Color : COLOR0;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 main(PSIn In) : SV_Target
{
    float4 sampled = g_texture.Sample(g_sampler, In.UV);
    return sampled * In.Color;
}
)";

struct BillboardConstants
{
    float viewMatrix[16];
    float projMatrix[16];
    float centerAndRoll[4];
    float sizeAndOpacity[4];
    float tint[4];
};
} // namespace

class PrimitiveRenderer3D::Impl {
public:
    IDeviceContext* ctx_ = nullptr;
    ISwapChain* swapChain_ = nullptr;
    ITextureView* overrideRTV_ = nullptr;
    RefCntAutoPtr<IRenderDevice> device_;

    RefCntAutoPtr<IShader> vs_;
    RefCntAutoPtr<IShader> ps_;
    RefCntAutoPtr<IPipelineState> pso_;
    RefCntAutoPtr<IShaderResourceBinding> srb_;
    RefCntAutoPtr<IBuffer> constantBuffer_;
    RefCntAutoPtr<ISampler> sampler_;
    RefCntAutoPtr<ITexture> defaultTexture_;
    RefCntAutoPtr<ITextureView> defaultTextureSRV_;

    QMatrix4x4 viewMatrix_;
    QMatrix4x4 projMatrix_;

    BillboardConstants constants_{};

    struct CachedTexture {
        RefCntAutoPtr<ITexture> texture;
        qint64 lastUsedFrame = 0;
    };

    std::unordered_map<qint64, CachedTexture> textureCache_;
    qint64 frameCount_ = 0;
    TEXTURE_FORMAT rtvFormat_ = TEX_FORMAT_RGBA8_UNORM_SRGB;

    void pruneCache()
    {
        if (textureCache_.size() <= 50) {
            return;
        }
        for (auto it = textureCache_.begin(); it != textureCache_.end(); ) {
            if (it->second.lastUsedFrame + 60 < frameCount_) {
                it = textureCache_.erase(it);
            } else {
                ++it;
            }
        }
    }

    ITextureView* currentRTV() const
    {
        if (overrideRTV_) {
            return overrideRTV_;
        }
        return swapChain_ ? swapChain_->GetCurrentBackBufferRTV() : nullptr;
    }

    bool hasRenderTarget() const
    {
        return currentRTV() != nullptr;
    }

    void resetIdentityMatrix(float* dst)
    {
        std::memcpy(dst, kIdentityMatrix.data(), sizeof(float) * 16);
    }

    void compileShaders()
    {
        if (!device_) {
            return;
        }

        ShaderCreateInfo vsInfo;
        vsInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        vsInfo.Desc.ShaderType = SHADER_TYPE_VERTEX;
        vsInfo.Desc.Name = "PrimitiveRenderer3D_BillboardVS";
        vsInfo.Source = kBillboardVSSource;
        vsInfo.SourceLength = std::strlen(kBillboardVSSource);
        vsInfo.EntryPoint = "main";

        ShaderCreateInfo psInfo;
        psInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        psInfo.Desc.ShaderType = SHADER_TYPE_PIXEL;
        psInfo.Desc.Name = "PrimitiveRenderer3D_BillboardPS";
        psInfo.Source = kBillboardPSSource;
        psInfo.SourceLength = std::strlen(kBillboardPSSource);
        psInfo.EntryPoint = "main";

        device_->CreateShader(vsInfo, &vs_);
        device_->CreateShader(psInfo, &ps_);
    }

    void createDefaultTexture()
    {
        if (!device_) {
            return;
        }

        const Uint8 whitePixel[4] = { 255, 255, 255, 255 };

        TextureDesc texDesc;
        texDesc.Name = "PrimitiveRenderer3D_WhiteTexture";
        texDesc.Type = RESOURCE_DIM_TEX_2D;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
        texDesc.Usage = USAGE_IMMUTABLE;
        texDesc.BindFlags = BIND_SHADER_RESOURCE;

        TextureSubResData subRes;
        subRes.pData = whitePixel;
        subRes.Stride = 4;

        TextureData initData;
        initData.pSubResources = &subRes;
        initData.NumSubresources = 1;

        device_->CreateTexture(texDesc, &initData, &defaultTexture_);
        if (defaultTexture_) {
            defaultTextureSRV_ = defaultTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        }
    }

    void createBuffers()
    {
        if (!device_) {
            return;
        }

        BufferDesc cbDesc;
        cbDesc.Name = "PrimitiveRenderer3D Billboard CB";
        cbDesc.Usage = USAGE_DYNAMIC;
        cbDesc.Size = sizeof(BillboardConstants);
        cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        device_->CreateBuffer(cbDesc, nullptr, &constantBuffer_);

        SamplerDesc samplerDesc;
        samplerDesc.MinFilter = FILTER_TYPE_LINEAR;
        samplerDesc.MagFilter = FILTER_TYPE_LINEAR;
        samplerDesc.MipFilter = FILTER_TYPE_LINEAR;
        samplerDesc.AddressU = TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = TEXTURE_ADDRESS_CLAMP;
        samplerDesc.ComparisonFunc = COMPARISON_FUNC_ALWAYS;
        samplerDesc.MaxAnisotropy = 1;
        device_->CreateSampler(samplerDesc, &sampler_);
    }

    void createPSO()
    {
        if (!device_ || !vs_ || !ps_ || !constantBuffer_) {
            return;
        }

        GraphicsPipelineStateCreateInfo psoCI;
        psoCI.PSODesc.Name = "PrimitiveRenderer3D Billboard PSO";
        psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
        psoCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        static ShaderResourceVariableDesc vars[] = {
            { SHADER_TYPE_VERTEX, "BillboardCB", SHADER_RESOURCE_VARIABLE_TYPE_STATIC },
            { SHADER_TYPE_PIXEL, "g_texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_PIXEL, "g_sampler", SHADER_RESOURCE_VARIABLE_TYPE_STATIC }
        };
        psoCI.PSODesc.ResourceLayout.Variables = vars;
        psoCI.PSODesc.ResourceLayout.NumVariables = _countof(vars);

        psoCI.pVS = vs_;
        psoCI.pPS = ps_;

        auto& gp = psoCI.GraphicsPipeline;
        gp.NumRenderTargets = 1;
        gp.RTVFormats[0] = rtvFormat_;
        gp.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        gp.RasterizerDesc.CullMode = CULL_MODE_NONE;
        gp.DepthStencilDesc.DepthEnable = False;
        gp.DepthStencilDesc.DepthWriteEnable = False;
        gp.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS_EQUAL;
        gp.InputLayout.NumElements = 0;

        auto& blend = gp.BlendDesc.RenderTargets[0];
        blend.BlendEnable = True;
        blend.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
        blend.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
        blend.BlendOp = BLEND_OPERATION_ADD;
        blend.SrcBlendAlpha = BLEND_FACTOR_ONE;
        blend.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
        blend.BlendOpAlpha = BLEND_OPERATION_ADD;
        blend.RenderTargetWriteMask = COLOR_MASK_ALL;

        device_->CreateGraphicsPipelineState(psoCI, &pso_);
        if (!pso_) {
            return;
        }

        if (auto* cbVar = pso_->GetStaticVariableByName(SHADER_TYPE_VERTEX, "BillboardCB")) {
            cbVar->Set(constantBuffer_);
        }
        if (sampler_) {
            if (auto* sampVar = pso_->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_sampler")) {
                sampVar->Set(sampler_);
            }
        }
        pso_->CreateShaderResourceBinding(&srb_, true);
    }

    RefCntAutoPtr<ITexture> textureFromImage(const QImage& image)
    {
        if (image.isNull() || !device_) {
            return {};
        }

        const qint64 cacheKey = image.cacheKey();
        auto it = textureCache_.find(cacheKey);
        if (it != textureCache_.end()) {
            it->second.lastUsedFrame = frameCount_;
            return it->second.texture;
        }

        const QImage rgba = (image.format() == QImage::Format_RGBA8888)
                                ? image
                                : image.convertToFormat(QImage::Format_RGBA8888);
        if (rgba.width() <= 0 || rgba.height() <= 0) {
            return {};
        }

        TextureDesc texDesc;
        texDesc.Type = RESOURCE_DIM_TEX_2D;
        texDesc.Width = static_cast<Uint32>(rgba.width());
        texDesc.Height = static_cast<Uint32>(rgba.height());
        texDesc.MipLevels = 1;
        texDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
        texDesc.Usage = USAGE_IMMUTABLE;
        texDesc.BindFlags = BIND_SHADER_RESOURCE;

        TextureSubResData subRes;
        subRes.pData = rgba.constBits();
        subRes.Stride = static_cast<Uint64>(rgba.bytesPerLine());

        TextureData initData;
        initData.pSubResources = &subRes;
        initData.NumSubresources = 1;

        RefCntAutoPtr<ITexture> texture;
        device_->CreateTexture(texDesc, &initData, &texture);
        if (!texture) {
            return {};
        }

        textureCache_[cacheKey] = { texture, frameCount_ };
        return texture;
    }

    void setConstants(const QVector3D& center, const QVector2D& size,
                      const FloatColor& tint, float opacity, float rollDegrees)
    {
        std::memcpy(constants_.viewMatrix, viewMatrix_.constData(), sizeof(float) * 16);
        std::memcpy(constants_.projMatrix, projMatrix_.constData(), sizeof(float) * 16);

        constants_.centerAndRoll[0] = center.x();
        constants_.centerAndRoll[1] = center.y();
        constants_.centerAndRoll[2] = center.z();
        constants_.centerAndRoll[3] = rollDegrees * 0.017453292519943295f;

        constants_.sizeAndOpacity[0] = size.x();
        constants_.sizeAndOpacity[1] = size.y();
        constants_.sizeAndOpacity[2] = opacity;
        constants_.sizeAndOpacity[3] = 0.0f;

        constants_.tint[0] = tint.r();
        constants_.tint[1] = tint.g();
        constants_.tint[2] = tint.b();
        constants_.tint[3] = tint.a();
    }

    void drawBillboard(const QVector3D& center, const QVector2D& size,
                       ITextureView* textureView, const FloatColor& tint,
                       float opacity, float rollDegrees)
    {
        if (!hasRenderTarget() || !ctx_ || !pso_ || !srb_ || !constantBuffer_) {
            return;
        }

        if (!textureView) {
            textureView = defaultTextureSRV_.RawPtr();
        }
        if (!textureView) {
            return;
        }

        frameCount_++;
        if (frameCount_ % 60 == 0) {
            pruneCache();
        }

        setConstants(center, size, tint, opacity, rollDegrees);

        void* mapped = nullptr;
        ctx_->MapBuffer(constantBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, mapped);
        if (!mapped) {
            return;
        }
        std::memcpy(mapped, &constants_, sizeof(BillboardConstants));
        ctx_->UnmapBuffer(constantBuffer_, MAP_WRITE);

        auto* rtv = currentRTV();
        ctx_->SetRenderTargets(1, &rtv, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx_->SetPipelineState(pso_);

        if (auto* texVar = srb_->GetVariableByName(SHADER_TYPE_PIXEL, "g_texture")) {
            texVar->Set(textureView);
        }

        if (auto* cbVar = srb_->GetVariableByName(SHADER_TYPE_VERTEX, "BillboardCB")) {
            cbVar->Set(constantBuffer_);
        }

        ctx_->CommitShaderResources(srb_, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawAttribs drawAttrs;
        drawAttrs.NumVertices = 4;
        drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
        ctx_->Draw(drawAttrs);
    }
};

PrimitiveRenderer3D::PrimitiveRenderer3D()
    : impl_(new Impl())
{
    impl_->resetIdentityMatrix(impl_->viewMatrix_.data());
    impl_->resetIdentityMatrix(impl_->projMatrix_.data());
}

PrimitiveRenderer3D::~PrimitiveRenderer3D()
{
    destroy();
    delete impl_;
}

void PrimitiveRenderer3D::createBuffers(RefCntAutoPtr<IRenderDevice> device, TEXTURE_FORMAT rtvFormat)
{
    impl_->device_ = device;
    impl_->rtvFormat_ = rtvFormat;
    impl_->createBuffers();
    impl_->createDefaultTexture();
    impl_->compileShaders();
    impl_->createPSO();
}

void PrimitiveRenderer3D::setContext(IDeviceContext* ctx, ISwapChain* swapChain)
{
    impl_->ctx_ = ctx;
    impl_->swapChain_ = swapChain;
}

void PrimitiveRenderer3D::setOverrideRTV(ITextureView* rtv)
{
    impl_->overrideRTV_ = rtv;
}

void PrimitiveRenderer3D::destroy()
{
    impl_->textureCache_.clear();
    impl_->defaultTextureSRV_ = nullptr;
    impl_->defaultTexture_ = nullptr;
    impl_->sampler_ = nullptr;
    impl_->constantBuffer_ = nullptr;
    impl_->srb_ = nullptr;
    impl_->pso_ = nullptr;
    impl_->ps_ = nullptr;
    impl_->vs_ = nullptr;
    impl_->ctx_ = nullptr;
    impl_->swapChain_ = nullptr;
    impl_->overrideRTV_ = nullptr;
    impl_->device_ = nullptr;
}

void PrimitiveRenderer3D::setViewMatrix(const QMatrix4x4& view)
{
    impl_->viewMatrix_ = view;
}

void PrimitiveRenderer3D::setProjectionMatrix(const QMatrix4x4& proj)
{
    impl_->projMatrix_ = proj;
}

void PrimitiveRenderer3D::setCameraMatrices(const QMatrix4x4& view, const QMatrix4x4& proj)
{
    impl_->viewMatrix_ = view;
    impl_->projMatrix_ = proj;
}

void PrimitiveRenderer3D::resetMatrices()
{
    impl_->resetIdentityMatrix(impl_->viewMatrix_.data());
    impl_->resetIdentityMatrix(impl_->projMatrix_.data());
}

void PrimitiveRenderer3D::drawBillboardQuad(const QVector3D& center, const QVector2D& size,
                                            const FloatColor& tint, float opacity, float rollDegrees)
{
    impl_->drawBillboard(center, size, impl_->defaultTextureSRV_.RawPtr(), tint, opacity, rollDegrees);
}

void PrimitiveRenderer3D::drawBillboardQuad(const QVector3D& center, const QVector2D& size,
                                            ITextureView* texture, const FloatColor& tint,
                                            float opacity, float rollDegrees)
{
    impl_->drawBillboard(center, size, texture, tint, opacity, rollDegrees);
}

void PrimitiveRenderer3D::drawBillboardQuad(const QVector3D& center, const QVector2D& size,
                                            const QImage& image, const FloatColor& tint,
                                            float opacity, float rollDegrees)
{
    auto texture = impl_->textureFromImage(image);
    impl_->drawBillboard(center, size, texture ? texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)
                                               : impl_->defaultTextureSRV_.RawPtr(),
                         tint, opacity, rollDegrees);
}

} // namespace Artifact
