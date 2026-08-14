module;
#include <cstring>
#include <QImage>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <RefCntAutoPtr.hpp>
#include <Texture.h>
export module Artifact.Render.TextRenderTarget;

export namespace Artifact {

/// Owns the color target used by ArtifactTextRenderer.
/// It is intentionally independent of swap chains, layers, and composition.
class ArtifactTextRenderTarget final {
public:
    ArtifactTextRenderTarget() = default;
    ~ArtifactTextRenderTarget();
    ArtifactTextRenderTarget(const ArtifactTextRenderTarget&) = delete;
    ArtifactTextRenderTarget& operator=(const ArtifactTextRenderTarget&) = delete;

    bool create(Diligent::IRenderDevice* device, int width, int height);
    bool isValid() const;
    int width() const;
    int height() const;
    Diligent::ITextureView* renderTargetView() const;
    void clear(Diligent::IDeviceContext* context, float r, float g, float b, float a);
    bool readback(Diligent::IDeviceContext* context, QImage& outImage) const;
    void destroy();

private:
    class Impl;
    Impl* impl_ = nullptr;
};

class ArtifactTextRenderTarget::Impl {
public:
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::ITexture> color;
    Diligent::ITextureView* rtv = nullptr;
    int width = 0;
    int height = 0;
};

inline ArtifactTextRenderTarget::~ArtifactTextRenderTarget() { destroy(); }

inline bool ArtifactTextRenderTarget::create(Diligent::IRenderDevice* device, int width, int height) {
    destroy();
    if (!device || width <= 0 || height <= 0) return false;
    impl_ = new Impl();
    impl_->device = device;
    Diligent::TextureDesc desc;
    desc.Name = "ArtifactTextRenderTarget";
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = static_cast<Diligent::Uint32>(width);
    desc.Height = static_cast<Diligent::Uint32>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    desc.Usage = Diligent::USAGE_DEFAULT;
    desc.BindFlags = Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;
    device->CreateTexture(desc, nullptr, &impl_->color);
    if (!impl_->color) { destroy(); return false; }
    impl_->rtv = impl_->color->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
    impl_->width = width;
    impl_->height = height;
    return impl_->rtv != nullptr;
}

inline bool ArtifactTextRenderTarget::isValid() const { return impl_ && impl_->rtv; }
inline int ArtifactTextRenderTarget::width() const { return impl_ ? impl_->width : 0; }
inline int ArtifactTextRenderTarget::height() const { return impl_ ? impl_->height : 0; }
inline Diligent::ITextureView* ArtifactTextRenderTarget::renderTargetView() const { return impl_ ? impl_->rtv : nullptr; }

inline void ArtifactTextRenderTarget::clear(Diligent::IDeviceContext* context, float r, float g, float b, float a) {
    if (!context || !isValid()) return;
    const float color[4] = {r, g, b, a};
    context->ClearRenderTarget(impl_->rtv, color, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

inline bool ArtifactTextRenderTarget::readback(Diligent::IDeviceContext* context, QImage& outImage) const {
    outImage = {};
    if (!context || !isValid()) return false;
    const auto desc = impl_->color->GetDesc();
    Diligent::TextureDesc stagingDesc = desc;
    stagingDesc.Name = "ArtifactTextRenderTargetReadback";
    stagingDesc.Usage = Diligent::USAGE_STAGING;
    stagingDesc.BindFlags = Diligent::BIND_NONE;
    stagingDesc.CPUAccessFlags = Diligent::CPU_ACCESS_READ;
    Diligent::RefCntAutoPtr<Diligent::ITexture> staging;
    impl_->device->CreateTexture(stagingDesc, nullptr, &staging);
    if (!staging) return false;
    context->SetRenderTargets(0, nullptr, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_NONE);
    context->CopyTexture(Diligent::CopyTextureAttribs{impl_->color, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                                       staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION});
    context->Flush();
    context->WaitForIdle();
    Diligent::MappedTextureSubresource mapped{};
    context->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, mapped);
    if (!mapped.pData || mapped.Stride < static_cast<Diligent::Uint64>(desc.Width) * 4u) return false;
    outImage = QImage(static_cast<int>(desc.Width), static_cast<int>(desc.Height), QImage::Format_RGBA8888);
    for (Diligent::Uint32 row = 0; row < desc.Height; ++row)
        std::memcpy(outImage.scanLine(static_cast<int>(row)), static_cast<const char*>(mapped.pData) + mapped.Stride * row,
                    static_cast<size_t>(desc.Width) * 4u);
    context->UnmapTextureSubresource(staging, 0, 0);
    return true;
}

inline void ArtifactTextRenderTarget::destroy() {
    delete impl_;
    impl_ = nullptr;
}

}
