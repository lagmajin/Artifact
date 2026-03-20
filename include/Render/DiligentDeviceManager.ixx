module;
#include <QWidget>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <SwapChain.h>
#include <RefCntAutoPtr.hpp>
#include <windows.h>

export module Artifact.Render.DiligentDeviceManager;

export namespace Artifact {

using namespace Diligent;

bool acquireSharedRenderDeviceForCurrentBackend(
    RefCntAutoPtr<IRenderDevice>& outDevice,
    RefCntAutoPtr<IDeviceContext>& outImmediateContext);
void releaseSharedRenderDevice();
RENDER_DEVICE_TYPE sharedRenderDeviceType();

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
    void destroy();
    bool createSwapChainForCurrentBackend(QWidget* widget, HWND hwnd, 
                                          RefCntAutoPtr<IRenderDevice> device,
                                          RefCntAutoPtr<ISwapChain>& outSwapChain);

    RefCntAutoPtr<IRenderDevice> device() const;
    RefCntAutoPtr<IDeviceContext> immediateContext() const;
    RefCntAutoPtr<IDeviceContext> deferredContext() const;
    RefCntAutoPtr<ISwapChain> swapChain() const;
    HWND renderHwnd() const;

    bool isInitialized() const;

private:
    class Impl;
    Impl* impl_;
};

}
