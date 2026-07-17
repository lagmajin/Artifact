module;
#include <utility>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <SwapChain.h>
#include <RefCntAutoPtr.hpp>
#include <vulkan/vulkan_core.h>
#include <windows.h>
#include <QWidget>
#include <QString>
export module Artifact.Render.DiligentDeviceManager;


export namespace Artifact {

using namespace Diligent;

bool acquireSharedRenderDeviceForCurrentBackend(
    RefCntAutoPtr<IRenderDevice>& outDevice,
    RefCntAutoPtr<IDeviceContext>& outImmediateContext);
void releaseSharedRenderDevice();
bool invalidateSharedRenderDeviceIfExclusive(IRenderDevice* expectedDevice);
RENDER_DEVICE_TYPE sharedRenderDeviceType();

struct SelectedGpuAdapterInfo {
    bool available = false;
    QString name;
    QString vendor;
    QString backend;
    Uint32 vendorId = 0;
    Uint32 deviceId = 0;
    bool rayTracingSupported = false;
};

class DiligentDeviceManager {
public:
    DiligentDeviceManager();
    explicit DiligentDeviceManager(RefCntAutoPtr<IRenderDevice> device, 
                                   RefCntAutoPtr<IDeviceContext> context);
    ~DiligentDeviceManager();

    void initialize(QWidget* widget);
    void initializeHeadless();
    void createSwapChain(QWidget* widget);
    void recreateSwapChain(QWidget* widget);
    void markDeviceLost();
    void destroy();
    bool createSwapChainForCurrentBackend(QWidget* widget, HWND hwnd, 
                                          RefCntAutoPtr<IRenderDevice> device,
                                          RefCntAutoPtr<ISwapChain>& outSwapChain);

    RefCntAutoPtr<IRenderDevice> device() const;
    RefCntAutoPtr<IDeviceContext> immediateContext() const;
    RefCntAutoPtr<IDeviceContext> deferredContext() const;
    RefCntAutoPtr<ISwapChain> swapChain() const;
    HWND renderHwnd() const;
    VkDevice vkDevice() const;
    VkPhysicalDevice vkPhysicalDevice() const;
    VkInstance vkInstance() const;
    VkQueue vkQueue() const;
    uint32_t vkQueueFamilyIndex() const;

    bool isInitialized() const;
    bool isRayTracingSupported() const;
    SelectedGpuAdapterInfo selectedAdapterInfo() const;
    QString selectedAdapterDebugState() const;

private:
    class Impl;
    Impl* impl_;
};

}
