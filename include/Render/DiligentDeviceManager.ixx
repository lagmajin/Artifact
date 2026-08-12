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
#include <vector>
export module Artifact.Render.DiligentDeviceManager;


export namespace Artifact {

using namespace Diligent;

bool acquireSharedRenderDeviceForCurrentBackend(
    RefCntAutoPtr<IRenderDevice>& outDevice,
    RefCntAutoPtr<IDeviceContext>& outImmediateContext);
void releaseSharedRenderDevice();

// Owns exactly one acquireSharedRenderDeviceForCurrentBackend() reference.
// Effects must use this instead of pairing the raw acquire/release calls by hand.
class SharedRenderDeviceLease final {
public:
    SharedRenderDeviceLease() = default;
    ~SharedRenderDeviceLease();
    SharedRenderDeviceLease(const SharedRenderDeviceLease&) = delete;
    SharedRenderDeviceLease& operator=(const SharedRenderDeviceLease&) = delete;

    bool acquire(RefCntAutoPtr<IRenderDevice>& outDevice,
                 RefCntAutoPtr<IDeviceContext>& outImmediateContext);
    bool isActive() const { return active_; }

private:
    bool active_ = false;
};

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
    QString selectionPolicy;
    QString requestedAdapter;
};

struct GpuAdapterCandidate {
    Uint32 adapterId = 0;
    QString name;
    QString vendor;
    QString type;
    QString backend;
    Uint32 vendorId = 0;
    Uint32 deviceId = 0;
    Uint64 localMemoryBytes = 0;
    Uint64 unifiedMemoryBytes = 0;
    bool rayTracingSupported = false;
    bool selected = false;
    int autoScore = 0;
};

struct D3D12AgilityCapabilitySnapshot {
    bool available = false;
    bool agilityRuntimeLoaded = false;
    bool deviceShaderModelKnown = false;
    bool deviceShaderModel69Supported = false;
    bool dxcAvailable = false;
    bool dxcShaderModel69Supported = false;
    bool options22Available = false;
    bool tightAlignmentAvailable = false;
    bool device15Available = false;
    bool periodicTrimNotificationAvailable = false;
    bool periodicTrimNotificationRegistered = false;
    bool cpuTimelineQueryResolveAvailable = false;
    bool revisedViewCreationAvailable = false;
    Uint32 requestedSdkVersion = 619;
    Uint32 headerSdkVersion = 0;
    Uint32 deviceShaderModelMajor = 0;
    Uint32 deviceShaderModelMinor = 0;
    Uint32 dxcVersionMajor = 0;
    Uint32 dxcVersionMinor = 0;
    Uint32 dxcShaderModelMajor = 0;
    Uint32 dxcShaderModelMinor = 0;
    Uint32 max1DDispatchSize = 65535;
    Uint32 max1DDispatchMeshSize = 65535;
    Uint32 tightAlignmentTier = 0;
    QString runtimePath;
};

D3D12AgilityCapabilitySnapshot queryD3D12AgilityCapabilities(
    IRenderDevice* device);
D3D12AgilityCapabilitySnapshot sharedD3D12AgilityCapabilities();

struct D3D12TrimRequestSnapshot {
    Uint64 generation = 0;
    Uint64 requestedBytes = 0;
    bool pending = false;
};

D3D12TrimRequestSnapshot currentD3D12TrimRequest();
D3D12TrimRequestSnapshot claimD3D12TrimRequest();

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
    D3D12AgilityCapabilitySnapshot d3d12AgilityCapabilities() const;
    QString d3d12AgilityDebugState() const;
    std::vector<GpuAdapterCandidate> availableAdapters() const;
    QString availableAdaptersDebugState() const;

private:
    class Impl;
    Impl* impl_;
};

}
