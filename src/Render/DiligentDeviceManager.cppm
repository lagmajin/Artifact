module;
#include <QWidget>
#include <QDebug>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <SwapChain.h>
#include <RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <windows.h>

module Artifact.Render.DiligentDeviceManager;

namespace Artifact {

using namespace Diligent;

namespace {
    enum class RenderBackendPreference {
        Auto,
        D3D12,
        Vulkan
    };

    RenderBackendPreference getBackendPreferenceFromEnv()
    {
        const QString value = qEnvironmentVariable("ARTIFACT_RENDER_BACKEND").trimmed().toLower();
        if (value == "vulkan" || value == "vk") {
            return RenderBackendPreference::Vulkan;
        }
        if (value == "d3d12" || value == "dx12") {
            return RenderBackendPreference::D3D12;
        }
        return RenderBackendPreference::Auto;
    }

    Diligent::IEngineFactoryD3D12* resolveD3D12Factory()
    {
#if D3D12_SUPPORTED
#if DILIGENT_D3D12_SHARED
        return Diligent::LoadAndGetEngineFactoryD3D12();
#else
        return Diligent::GetEngineFactoryD3D12();
#endif
#else
        return nullptr;
#endif
    }

    Diligent::IEngineFactoryVk* resolveVkFactory()
    {
#if VULKAN_SUPPORTED
#if DILIGENT_VK_EXPLICIT_LOAD
        return Diligent::LoadAndGetEngineFactoryVk();
#else
        return Diligent::GetEngineFactoryVk();
#endif
#else
        return nullptr;
#endif
    }

    bool hasUsableVulkanLoader()
    {
#if VULKAN_SUPPORTED
        HMODULE loader = ::GetModuleHandleW(L"vulkan-1.dll");
        if (!loader) {
            loader = ::LoadLibraryW(L"vulkan-1.dll");
        }
        if (!loader) {
            return false;
        }
        return ::GetProcAddress(loader, "vkGetInstanceProcAddr") != nullptr;
#else
        return false;
#endif
    }
}

class DiligentDeviceManager::Impl {
public:
    RefCntAutoPtr<IRenderDevice> device_;
    RefCntAutoPtr<IDeviceContext> immediateContext_;
    RefCntAutoPtr<IDeviceContext> deferredContext_;
    RefCntAutoPtr<ISwapChain> swapChain_;
    HWND renderHwnd_ = nullptr;
    QWidget* widget_ = nullptr;
    bool initialized_ = false;
    int currentPhysicalWidth_ = 0;
    int currentPhysicalHeight_ = 0;
    qreal currentDevicePixelRatio_ = 1.0;
    const TEXTURE_FORMAT MAIN_RTV_FORMAT = TEX_FORMAT_RGBA8_UNORM_SRGB;

    Impl() = default;

    Impl(RefCntAutoPtr<IRenderDevice> device, RefCntAutoPtr<IDeviceContext> context)
        : device_(device), immediateContext_(context), initialized_(true)
    {
        if (device_ && immediateContext_) {
            device_->CreateDeferredContext(&deferredContext_);
        }
    }

    ~Impl()
    {
        destroy();
    }

    void initialize(QWidget* widget);
    void initializeHeadless();
    void createSwapChain(QWidget* widget);
    void recreateSwapChain(QWidget* widget);
    void destroy();
    bool createSwapChainForBackend(HWND hwnd, int width, int height);
};

void DiligentDeviceManager::Impl::initialize(QWidget* widget)
{
    if (!widget) {
        return;
    }

    widget_ = widget;
    const auto backendPreference = getBackendPreferenceFromEnv();

    auto tryInitD3D12 = [&]() -> bool
    {
        auto* pFactory = resolveD3D12Factory();
        if (!pFactory) {
            return false;
        }

        EngineD3D12CreateInfo creationAttribs = {};
        creationAttribs.EnableValidation = true;
        creationAttribs.SetValidationLevel(Diligent::VALIDATION_LEVEL_2);
        pFactory->CreateDeviceAndContextsD3D12(creationAttribs, &device_, &immediateContext_);
        return device_ && immediateContext_;
    };

    auto tryInitVulkan = [&]() -> bool
    {
        if (!hasUsableVulkanLoader()) {
            return false;
        }

        auto* pFactory = resolveVkFactory();
        if (!pFactory) {
            return false;
        }

        EngineVkCreateInfo creationAttribs = {};
        creationAttribs.EnableValidation = true;
        creationAttribs.SetValidationLevel(Diligent::VALIDATION_LEVEL_2);
        pFactory->CreateDeviceAndContextsVk(creationAttribs, &device_, &immediateContext_);
        return device_ && immediateContext_;
    };

    bool deviceInitialized = false;
    switch (backendPreference) {
        case RenderBackendPreference::Vulkan:
            deviceInitialized = tryInitVulkan() || tryInitD3D12();
            break;
        case RenderBackendPreference::D3D12:
            deviceInitialized = tryInitD3D12();
            break;
        case RenderBackendPreference::Auto:
        default:
            deviceInitialized = tryInitD3D12() || tryInitVulkan();
            break;
    }

    if (!deviceInitialized)
    {
        qWarning() << "Failed to create Diligent Engine device and contexts.";
        return;
    }

    device_->CreateDeferredContext(&deferredContext_);

    currentPhysicalWidth_ = static_cast<int>(widget_->width() * widget_->devicePixelRatio());
    currentPhysicalHeight_ = static_cast<int>(widget_->height() * widget_->devicePixelRatio());
    currentDevicePixelRatio_ = widget_->devicePixelRatio();

    HWND parentHwnd = reinterpret_cast<HWND>(widget_->winId());
    renderHwnd_ = CreateWindowEx(
        0, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, currentPhysicalWidth_, currentPhysicalHeight_,
        parentHwnd, nullptr, GetModuleHandle(nullptr), nullptr);

    if (!createSwapChainForBackend(renderHwnd_, currentPhysicalWidth_, currentPhysicalHeight_)) {
        qWarning() << "Failed to create swap chain for the current backend.";
        return;
    }

    Diligent::Viewport VP;
    VP.Width = static_cast<float>(currentPhysicalWidth_);
    VP.Height = static_cast<float>(currentPhysicalHeight_);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0.0f;
    VP.TopLeftY = 0.0f;
    immediateContext_->SetViewports(1, &VP, currentPhysicalWidth_, currentPhysicalHeight_);

    initialized_ = true;
}

void DiligentDeviceManager::Impl::initializeHeadless()
{
    const auto backendPreference = getBackendPreferenceFromEnv();

    auto tryInitD3D12 = [&]() -> bool
    {
        auto* pFactory = resolveD3D12Factory();
        if (!pFactory) return false;
        EngineD3D12CreateInfo creationAttribs = {};
        pFactory->CreateDeviceAndContextsD3D12(creationAttribs, &device_, &immediateContext_);
        return device_ && immediateContext_;
    };

    auto tryInitVulkan = [&]() -> bool
    {
        if (!hasUsableVulkanLoader()) return false;
        auto* pFactory = resolveVkFactory();
        if (!pFactory) return false;
        EngineVkCreateInfo creationAttribs = {};
        pFactory->CreateDeviceAndContextsVk(creationAttribs, &device_, &immediateContext_);
        return device_ && immediateContext_;
    };

    bool ok = false;
    switch (backendPreference) {
        case RenderBackendPreference::Vulkan:
            ok = tryInitVulkan() || tryInitD3D12();
            break;
        case RenderBackendPreference::D3D12:
            ok = tryInitD3D12();
            break;
        case RenderBackendPreference::Auto:
        default:
            ok = tryInitD3D12() || tryInitVulkan();
            break;
    }

    if (!ok) {
        qWarning() << "DiligentDeviceManager::initializeHeadless: failed to create device.";
        return;
    }

    device_->CreateDeferredContext(&deferredContext_);
    initialized_ = true;
}

void DiligentDeviceManager::Impl::createSwapChain(QWidget* window)
{
    if (!window || !device_) {
        return;
    }

    widget_ = window;
    currentPhysicalWidth_ = static_cast<int>(window->width() * window->devicePixelRatio());
    currentPhysicalHeight_ = static_cast<int>(window->height() * window->devicePixelRatio());
    currentDevicePixelRatio_ = window->devicePixelRatio();

    if (!renderHwnd_) {
        HWND parentHwnd = reinterpret_cast<HWND>(window->winId());
        renderHwnd_ = CreateWindowEx(
            0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0, 0, currentPhysicalWidth_, currentPhysicalHeight_,
            parentHwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    }

    if (!swapChain_) {
        if (!renderHwnd_ || !createSwapChainForBackend(renderHwnd_, currentPhysicalWidth_, currentPhysicalHeight_)) {
            return;
        }

        if (immediateContext_) {
            Diligent::Viewport VP;
            VP.Width = static_cast<float>(currentPhysicalWidth_);
            VP.Height = static_cast<float>(currentPhysicalHeight_);
            VP.MinDepth = 0.0f;
            VP.MaxDepth = 1.0f;
            VP.TopLeftX = 0.0f;
            VP.TopLeftY = 0.0f;
            immediateContext_->SetViewports(1, &VP, currentPhysicalWidth_, currentPhysicalHeight_);
        }
    }

    initialized_ = true;
}

void DiligentDeviceManager::Impl::recreateSwapChain(QWidget* widget)
{
    if (!widget || !device_ || !swapChain_) {
        return;
    }

    const int newWidth = static_cast<int>(widget->width() * widget->devicePixelRatio());
    const int newHeight = static_cast<int>(widget->height() * widget->devicePixelRatio());
    if (newWidth <= 0 || newHeight <= 0) {
        return;
    }

    const qreal newDevicePixelRatio = widget->devicePixelRatio();
    currentPhysicalWidth_ = newWidth;
    currentPhysicalHeight_ = newHeight;
    currentDevicePixelRatio_ = newDevicePixelRatio;

    qDebug() << "DiligentDeviceManager::recreateSwapChain - Logical:" << widget->width() << "x" << widget->height()
             << ", DPI:" << newDevicePixelRatio
             << ", Physical:" << newWidth << "x" << newHeight;
    qDebug() << "Before Resize - SwapChain Desc:" << swapChain_->GetDesc().Width << "x" << swapChain_->GetDesc().Height;

    swapChain_->Resize(newWidth, newHeight);

    if (renderHwnd_)
        SetWindowPos(renderHwnd_, nullptr, 0, 0, newWidth, newHeight,
                     SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);

    qDebug() << "After Resize - SwapChain Desc:" << swapChain_->GetDesc().Width << "x" << swapChain_->GetDesc().Height;

    Diligent::Viewport VP;
    VP.Width = static_cast<float>(newWidth);
    VP.Height = static_cast<float>(newHeight);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0.0f;
    VP.TopLeftY = 0.0f;
    immediateContext_->SetViewports(1, &VP, newWidth, newHeight);

    qDebug() << "After SetViewports - Viewport WxH: " << VP.Width << "x" << VP.Height;
    qDebug() << "After SetViewports - Viewport TopLeftXY: " << VP.TopLeftX << ", " << VP.TopLeftY;
}

bool DiligentDeviceManager::Impl::createSwapChainForBackend(HWND hwnd, int width, int height)
{
    if (!device_ || !immediateContext_ || !hwnd) {
        return false;
    }

    SwapChainDesc SCDesc;
    SCDesc.Width = width;
    SCDesc.Height = height;
    SCDesc.ColorBufferFormat = MAIN_RTV_FORMAT;
    SCDesc.DepthBufferFormat = TEX_FORMAT_UNKNOWN;
    SCDesc.BufferCount = 2;
    SCDesc.Usage = SWAP_CHAIN_USAGE_RENDER_TARGET;

    Win32NativeWindow swapChainWindow;
    swapChainWindow.hWnd = hwnd;

    const auto deviceType = device_->GetDeviceInfo().Type;
    if (deviceType == RENDER_DEVICE_TYPE_VULKAN) {
        auto* pFactoryVk = resolveVkFactory();
        if (!pFactoryVk) {
            return false;
        }
        pFactoryVk->CreateSwapChainVk(device_, immediateContext_, SCDesc, swapChainWindow, &swapChain_);
        return swapChain_ != nullptr;
    }

    auto* pFactoryD3D12 = resolveD3D12Factory();
    if (!pFactoryD3D12) {
        return false;
    }

    FullScreenModeDesc fullScreenDesc;
    fullScreenDesc.Fullscreen = false;
    pFactoryD3D12->CreateSwapChainD3D12(device_, immediateContext_, SCDesc, fullScreenDesc, swapChainWindow, &swapChain_);
    return swapChain_ != nullptr;
}

void DiligentDeviceManager::Impl::destroy()
{
    if (renderHwnd_) {
        DestroyWindow(renderHwnd_);
        renderHwnd_ = nullptr;
    }

    swapChain_.Release();
    deferredContext_.Release();
    immediateContext_.Release();
    device_.Release();
    initialized_ = false;
}

DiligentDeviceManager::DiligentDeviceManager()
    : impl_(new Impl())
{
}

DiligentDeviceManager::DiligentDeviceManager(RefCntAutoPtr<IRenderDevice> device, 
                                             RefCntAutoPtr<IDeviceContext> context)
    : impl_(new Impl(device, context))
{
}

DiligentDeviceManager::~DiligentDeviceManager()
{
    delete impl_;
}

void DiligentDeviceManager::initialize(QWidget* widget)
{
    impl_->initialize(widget);
}

void DiligentDeviceManager::initializeHeadless()
{
    impl_->initializeHeadless();
}

void DiligentDeviceManager::createSwapChain(QWidget* widget)
{
    impl_->createSwapChain(widget);
}

void DiligentDeviceManager::recreateSwapChain(QWidget* widget)
{
    impl_->recreateSwapChain(widget);
}

void DiligentDeviceManager::destroy()
{
    impl_->destroy();
}

bool DiligentDeviceManager::createSwapChainForCurrentBackend(QWidget* widget, HWND hwnd, 
                                                              RefCntAutoPtr<IRenderDevice> device,
                                                              RefCntAutoPtr<ISwapChain>& outSwapChain)
{
    if (!device || !hwnd || !widget) {
        return false;
    }

    const int width = static_cast<int>(widget->width() * widget->devicePixelRatio());
    const int height = static_cast<int>(widget->height() * widget->devicePixelRatio());

    SwapChainDesc SCDesc;
    SCDesc.Width = width;
    SCDesc.Height = height;
    SCDesc.ColorBufferFormat = impl_->MAIN_RTV_FORMAT;
    SCDesc.DepthBufferFormat = TEX_FORMAT_UNKNOWN;
    SCDesc.BufferCount = 2;
    SCDesc.Usage = SWAP_CHAIN_USAGE_RENDER_TARGET;

    Win32NativeWindow swapChainWindow;
    swapChainWindow.hWnd = hwnd;

    const auto deviceType = device->GetDeviceInfo().Type;
    if (deviceType == RENDER_DEVICE_TYPE_VULKAN) {
        auto* pFactoryVk = resolveVkFactory();
        if (!pFactoryVk) {
            return false;
        }
        pFactoryVk->CreateSwapChainVk(device, impl_->immediateContext_, SCDesc, swapChainWindow, &outSwapChain);
        return outSwapChain != nullptr;
    }

    auto* pFactoryD3D12 = resolveD3D12Factory();
    if (!pFactoryD3D12) {
        return false;
    }

    FullScreenModeDesc fullScreenDesc;
    fullScreenDesc.Fullscreen = false;
    pFactoryD3D12->CreateSwapChainD3D12(device, impl_->immediateContext_, SCDesc, fullScreenDesc, swapChainWindow, &outSwapChain);
    return outSwapChain != nullptr;
}

RefCntAutoPtr<IRenderDevice> DiligentDeviceManager::device() const
{
    return impl_->device_;
}

RefCntAutoPtr<IDeviceContext> DiligentDeviceManager::immediateContext() const
{
    return impl_->immediateContext_;
}

RefCntAutoPtr<IDeviceContext> DiligentDeviceManager::deferredContext() const
{
    return impl_->deferredContext_;
}

RefCntAutoPtr<ISwapChain> DiligentDeviceManager::swapChain() const
{
    return impl_->swapChain_;
}

HWND DiligentDeviceManager::renderHwnd() const
{
    return impl_->renderHwnd_;
}

bool DiligentDeviceManager::isInitialized() const
{
    return impl_->initialized_;
}

}
