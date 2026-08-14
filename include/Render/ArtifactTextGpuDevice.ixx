module;
#include <EngineFactoryD3D12.h>
#include <DeviceContext.h>
#include <RenderDevice.h>
#include <RefCntAutoPtr.hpp>

export module Artifact.Render.TextGpuDevice;

export namespace Artifact {

/// Minimal D3D12 device boundary for text-only GPU experiments.
/// It intentionally does not depend on DiligentDeviceManager or renderer state.
class ArtifactTextGpuDevice final {
public:
    bool initialize() {
        device_.Release();
        context_.Release();
        auto* factory = Diligent::GetEngineFactoryD3D12();
        if (!factory) return false;

        Diligent::EngineD3D12CreateInfo createInfo{};
        createInfo.EnableValidation = true;
        createInfo.Features.MultithreadedResourceCreation =
            Diligent::DEVICE_FEATURE_STATE_DISABLED;
        createInfo.Features.RayTracing = Diligent::DEVICE_FEATURE_STATE_DISABLED;
        factory->CreateDeviceAndContextsD3D12(createInfo, &device_, &context_);
        return device_ != nullptr && context_ != nullptr;
    }

    bool isValid() const noexcept { return device_ && context_; }
    Diligent::IRenderDevice* device() const noexcept { return device_.RawPtr(); }
    Diligent::IDeviceContext* context() const noexcept { return context_.RawPtr(); }

    void flushAndWait() {
        if (context_) context_->Flush();
    }

    void destroy() noexcept {
        context_.Release();
        device_.Release();
    }

private:
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
};

}
