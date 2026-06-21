module;
#include <array>
#include <DeviceContext.h>
#include <Buffer.h>
#include <Common/interface/BasicMath.hpp>
#include <PipelineState.h>
#include <RenderDevice.h>
#include <RefCntAutoPtr.hpp>
#include <Sampler.h>
#include <ShaderResourceBinding.h>
#include <ShaderResourceVariable.h>
#include <QString>
#include <TextureView.h>
#include <unordered_map>
#include <vector>
export module Artifact.Render.DiligentBindlessSubmitter;

import Artifact.Render.IRenderSubmitter;
import Artifact.Render.RenderCommandBuffer;
import Artifact.Render.DiligentImmediateSubmitter;
import Artifact.Render.PrimitiveRenderer3D;
import Artifact.Render.ShaderManager;
import Frame.Debug;
import Graphics.ParticleRenderer;

export namespace Artifact {

using namespace Diligent;

class DiligentBindlessSubmitter final : public IRenderSubmitter {
public:
    DiligentBindlessSubmitter() = default;
    ~DiligentBindlessSubmitter() override = default;

    void createBuffers(RefCntAutoPtr<IRenderDevice> device, TEXTURE_FORMAT rtvFormat);
    void setPSOs(ShaderManager& shaderManager);
    void destroy();
    void setFrameCostStats(ArtifactCore::RenderCostStats* stats);
    void setDeferredContext(RefCntAutoPtr<IDeviceContext> deferred);
    void setPrimitiveRenderer3D(PrimitiveRenderer3D* renderer);
    void setParticleRenderer(ArtifactCore::ParticleRenderer* renderer);

    bool isSupported() const;
    QString debugState() const;

    void submit(RenderCommandBuffer& buf, IDeviceContext* ctx) override;

private:
    static constexpr Uint32 kMaxBindlessTextures = 128;
    static constexpr Uint32 kMaxSpriteBatch = 512;

    struct BindlessSpriteVertex {
        Diligent::float2 pos;
        Diligent::float2 uv;
        Diligent::float4 color;
        Diligent::Uint32 textureIndex = 0;
        Diligent::float3 pad = {};
    };

    void createSpriteResources(TEXTURE_FORMAT rtvFormat);
    void resetTextureTable();
    bool ensureTextureIndex(ITextureView* srv, Uint32& index);
    void flushSpriteBatch(IDeviceContext* ctx, ITextureView* rtv);
    bool submitSprite(const SpritePkt& pkt, IDeviceContext* ctx, ITextureView* rtv);

    DiligentImmediateSubmitter fallback_;
    RefCntAutoPtr<IRenderDevice> device_;
    RefCntAutoPtr<IPipelineState> spritePso_;
    RefCntAutoPtr<IShaderResourceBinding> spriteSrb_;
    RefCntAutoPtr<IBuffer> spriteVertexBuffer_;
    RefCntAutoPtr<IBuffer> spriteTransformBuffer_;
    RefCntAutoPtr<ISampler> spriteSampler_;
    IShaderResourceVariable* spriteTexturesVar_ = nullptr;

    std::vector<BindlessSpriteVertex> spriteVertices_;
    std::array<IDeviceObject*, kMaxBindlessTextures> textureObjects_ = {};
    std::unordered_map<ITextureView*, Uint32> textureIndices_;
    ArtifactCore::RenderCostStats* frameCostStats_ = nullptr;
    float2 spriteBatchScreenSize_ = {0.0f, 0.0f};
    bool supported_ = false;
};

} // namespace Artifact
