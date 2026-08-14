module;
#include <cstring>
#include <QImage>
#include <DeviceContext.h>
#include <RenderDevice.h>
#include <Texture.h>
#include <RefCntAutoPtr.hpp>
module Artifact.Render.TextRenderTarget;

namespace Artifact {

using namespace Diligent;

class ArtifactTextRenderTarget::Impl {
public:
    RefCntAutoPtr<IRenderDevice> device;
    RefCntAutoPtr<ITexture> color;
    ITextureView* rtv = nullptr;
    int width = 0;
    int height = 0;
};

ArtifactTextRenderTarget::~ArtifactTextRenderTarget() { destroy(); }

bool ArtifactTextRenderTarget::create(IRenderDevice* device, int width, int height) {
    destroy();
    if (!device || width <= 0 || height <= 0) return false;
    impl_ = new Impl();
    impl_->device = device;
    TextureDesc desc;
    desc.Name = "ArtifactTextRenderTarget";
    desc.Type = RESOURCE_DIM_TEX_2D;
    desc.Width = static_cast<Uint32>(width);
    desc.Height = static_cast<Uint32>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = TEX_FORMAT_RGBA8_UNORM;
    desc.Usage = USAGE_DEFAULT;
    desc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    device->CreateTexture(desc, nullptr, &impl_->color);
    if (!impl_->color) {
        destroy();
        return false;
    }
    impl_->rtv = impl_->color->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    impl_->width = width;
    impl_->height = height;
    return impl_->rtv != nullptr;
}

bool ArtifactTextRenderTarget::isValid() const { return impl_ && impl_->rtv; }
int ArtifactTextRenderTarget::width() const { return impl_ ? impl_->width : 0; }
int ArtifactTextRenderTarget::height() const { return impl_ ? impl_->height : 0; }
ITextureView* ArtifactTextRenderTarget::renderTargetView() const { return impl_ ? impl_->rtv : nullptr; }

void ArtifactTextRenderTarget::clear(IDeviceContext* context, float r, float g, float b, float a) {
    if (!context || !isValid()) return;
    const float color[4] = {r, g, b, a};
    context->ClearRenderTarget(impl_->rtv, color, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

bool ArtifactTextRenderTarget::readback(IDeviceContext* context, QImage& outImage) const {
    outImage = {};
    if (!context || !isValid()) return false;
    const auto desc = impl_->color->GetDesc();
    TextureDesc stagingDesc = desc;
    stagingDesc.Name = "ArtifactTextRenderTargetReadback";
    stagingDesc.Usage = USAGE_STAGING;
    stagingDesc.BindFlags = BIND_NONE;
    stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
    RefCntAutoPtr<ITexture> staging;
    impl_->device->CreateTexture(stagingDesc, nullptr, &staging);
    if (!staging) return false;
    context->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
    context->CopyTexture(CopyTextureAttribs(
        impl_->color, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        staging, RESOURCE_STATE_TRANSITION_MODE_TRANSITION));
    context->Flush();
    context->WaitForIdle();
    MappedTextureSubresource mapped{};
    context->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
    if (!mapped.pData || mapped.Stride < static_cast<Uint64>(desc.Width) * 4u) return false;
    outImage = QImage(static_cast<int>(desc.Width), static_cast<int>(desc.Height), QImage::Format_RGBA8888);
    for (Uint32 row = 0; row < desc.Height; ++row) {
        std::memcpy(outImage.scanLine(static_cast<int>(row)),
                    static_cast<const char*>(mapped.pData) + mapped.Stride * row,
                    static_cast<size_t>(desc.Width) * 4u);
    }
    context->UnmapTextureSubresource(staging, 0, 0);
    return true;
}

void ArtifactTextRenderTarget::destroy() {
    delete impl_;
    impl_ = nullptr;
}

}
