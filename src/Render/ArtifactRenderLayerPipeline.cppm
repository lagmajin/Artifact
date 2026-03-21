module;
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <QDebug>

module Artifact.Render.Pipeline;

import Artifact.Layer.Abstract;
import Graphics.LayerBlendPipeline;

namespace Artifact {

struct RenderPipeline::Impl {
 RefCntAutoPtr<IRenderDevice> device;
 RefCntAutoPtr<ITexture> accumTex;
 RefCntAutoPtr<ITexture> tempTex;
 TEXTURE_FORMAT format = TEX_FORMAT_RGBA8_UNORM_SRGB;
 Uint32 width = 0;
 Uint32 height = 0;
};

RenderPipeline::RenderPipeline() : impl_(new Impl()) {}
RenderPipeline::~RenderPipeline() { delete impl_; }

bool RenderPipeline::initialize(IRenderDevice* device, Uint32 width, Uint32 height, TEXTURE_FORMAT format)
{
 if (!device) return false;
 impl_->device = device;
 impl_->format = format;
 return createTextures(device, width, height, format);
}

bool RenderPipeline::createTextures(IRenderDevice* device, Uint32 width, Uint32 height, TEXTURE_FORMAT format)
{
 if (width == 0 || height == 0) return false;

 impl_->width = width;
 impl_->height = height;

 TextureDesc texDesc;
 texDesc.Type      = RESOURCE_DIM_TEX_2D;
 texDesc.Width     = width;
 texDesc.Height    = height;
 texDesc.MipLevels = 1;
 texDesc.Format    = format;
 texDesc.Usage     = USAGE_DEFAULT;
 texDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;

 texDesc.Name = "Blend Accum";
 impl_->accumTex.Release();
 device->CreateTexture(texDesc, nullptr, &impl_->accumTex);

 texDesc.Name = "Blend Temp";
 impl_->tempTex.Release();
 device->CreateTexture(texDesc, nullptr, &impl_->tempTex);

 if (!impl_->accumTex || !impl_->tempTex) {
  qWarning() << "[RenderPipeline] Failed to create intermediate textures";
  return false;
 }

 return true;
}

void RenderPipeline::resize(Uint32 width, Uint32 height)
{
 if (width == impl_->width && height == impl_->height) return;
 if (impl_->device) {
  createTextures(impl_->device, width, height, impl_->format);
 }
}

void RenderPipeline::destroy()
{
 impl_->accumTex.Release();
 impl_->tempTex.Release();
 impl_->width = 0;
 impl_->height = 0;
}

bool RenderPipeline::ready() const
{
 return impl_->accumTex && impl_->tempTex;
}

ITextureView* RenderPipeline::accumSRV() const
{
 return impl_->accumTex ? impl_->accumTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE) : nullptr;
}

ITextureView* RenderPipeline::accumUAV() const
{
 return nullptr;
}

ITextureView* RenderPipeline::tempSRV() const
{
 return impl_->tempTex ? impl_->tempTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE) : nullptr;
}

ITextureView* RenderPipeline::tempUAV() const
{
 return nullptr;
}

ITextureView* RenderPipeline::tempRTV() const
{
 return impl_->tempTex ? impl_->tempTex->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET) : nullptr;
}

Uint32 RenderPipeline::width() const { return impl_->width; }
Uint32 RenderPipeline::height() const { return impl_->height; }

void RenderPipeline::swapAccumAndTemp()
{
 std::swap(impl_->accumTex, impl_->tempTex);
}

bool RenderPipeline::renderComposition(
 IDeviceContext* ctx,
 const std::vector<ArtifactAbstractLayerPtr>& layers,
 int64_t currentFrame,
 ITextureView* outputRTV
)
{
 if (!ctx || !outputRTV || !ready()) return false;

 auto* accumRTV = impl_->accumTex->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
 auto* accumSRV = impl_->accumTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
 auto* tempRTV  = impl_->tempTex->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
 auto* tempSRV  = impl_->tempTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

 if (!accumRTV || !accumSRV || !tempRTV || !tempSRV) return false;

 const float clearZero[] = {0.0f, 0.0f, 0.0f, 0.0f};
 ctx->ClearRenderTarget(accumRTV, clearZero, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

 // TODO: LayerBlendPipeline を使用した合成ループ
 // 現段階はレイヤーを painter's algorithm で合成
 // 将来的には各レイヤーを temp に描画 → blend(temp, accum) → accum に結果

 return true;
}

}
