module;
#include <QWidget>
#include <QDebug>
#include <QString>
#include <QStringList>
#include <atomic>
#include <mutex>
#include <vector>
#include <vulkan/vulkan.h>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <SwapChain.h>
#include <RefCntAutoPtr.hpp>
#include <QElapsedTimer>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <windows.h>

module Artifact.Render.DiligentDeviceManager;

import Artifact.Render.Config;

namespace Artifact {

using namespace Diligent;

namespace {
    enum class RenderBackendPreference {
        Auto,
        D3D12,
        Vulkan
    };

    struct SharedRenderDeviceState {
        std::mutex mutex;
        RefCntAutoPtr<IRenderDevice> device;
        RefCntAutoPtr<IDeviceContext> immediateContext;
        RENDER_DEVICE_TYPE type = RENDER_DEVICE_TYPE_UNDEFINED;
        std::atomic_uint32_t refCount{0};
    };

    struct VulkanValidationInfo {
        bool loaderAvailable = false;
        bool enumerateLayersAvailable = false;
        QStringList layers;
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

    const char* backendPreferenceName(const RenderBackendPreference pref)
    {
        switch (pref) {
            case RenderBackendPreference::D3D12: return "d3d12";
            case RenderBackendPreference::Vulkan: return "vulkan";
            case RenderBackendPreference::Auto:
            default: return "auto";
        }
    }

    const char* deviceTypeName(const RENDER_DEVICE_TYPE type)
    {
        switch (type) {
            case RENDER_DEVICE_TYPE_D3D12: return "d3d12";
            case RENDER_DEVICE_TYPE_VULKAN: return "vulkan";
            case RENDER_DEVICE_TYPE_D3D11: return "d3d11";
            case RENDER_DEVICE_TYPE_GL: return "gl";
            case RENDER_DEVICE_TYPE_GLES: return "gles";
            case RENDER_DEVICE_TYPE_METAL: return "metal";
            case RENDER_DEVICE_TYPE_WEBGPU: return "webgpu";
            default: return "unknown";
        }
    }

    SharedRenderDeviceState& sharedRenderDeviceState()
    {
        static SharedRenderDeviceState state;
        return state;
    }

    bool backendAllowsReuse(const RenderBackendPreference pref,
                            const RENDER_DEVICE_TYPE type)
    {
        switch (pref) {
            case RenderBackendPreference::D3D12:
                return type == RENDER_DEVICE_TYPE_D3D12;
            case RenderBackendPreference::Vulkan:
                return type == RENDER_DEVICE_TYPE_VULKAN;
            case RenderBackendPreference::Auto:
            default:
                return type == RENDER_DEVICE_TYPE_D3D12 ||
                       type == RENDER_DEVICE_TYPE_VULKAN;
        }
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

    VulkanValidationInfo queryVulkanValidationInfo()
    {
        VulkanValidationInfo info;
#if VULKAN_SUPPORTED
        HMODULE loader = ::GetModuleHandleW(L"vulkan-1.dll");
        if (!loader) {
            loader = ::LoadLibraryW(L"vulkan-1.dll");
        }
        if (!loader) {
            return info;
        }

        info.loaderAvailable = true;
        const auto enumerateLayers = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
            ::GetProcAddress(loader, "vkEnumerateInstanceLayerProperties"));
        if (!enumerateLayers) {
            return info;
        }

        info.enumerateLayersAvailable = true;
        uint32_t layerCount = 0;
        if (enumerateLayers(&layerCount, nullptr) != VK_SUCCESS || layerCount == 0) {
            return info;
        }

        std::vector<VkLayerProperties> layerProps(layerCount);
        if (enumerateLayers(&layerCount, layerProps.data()) != VK_SUCCESS) {
            info.layers.clear();
            return info;
        }

        for (uint32_t i = 0; i < layerCount; ++i) {
            info.layers.push_back(QString::fromLatin1(layerProps[i].layerName));
        }
#endif
        return info;
    }

    void logVulkanValidationInfo()
    {
        const auto info = queryVulkanValidationInfo();
        const QString requestedBackend = qEnvironmentVariable("ARTIFACT_RENDER_BACKEND");
        const QString explicitInstanceLayers = qEnvironmentVariable("VK_INSTANCE_LAYERS");
        const QString explicitLayerPath = qEnvironmentVariable("VK_LAYER_PATH");
        const bool hasKhronosValidation = info.layers.contains(QStringLiteral("VK_LAYER_KHRONOS_validation"));

        qWarning() << "[DiligentDeviceManager][VulkanValidation]"
                   << "requestedBackend=" << (requestedBackend.isEmpty() ? QStringLiteral("<auto>") : requestedBackend)
                   << "loaderAvailable=" << info.loaderAvailable
                   << "enumerateLayersAvailable=" << info.enumerateLayersAvailable
                   << "hasKhronosValidation=" << hasKhronosValidation
                   << "VK_INSTANCE_LAYERS=" << (explicitInstanceLayers.isEmpty() ? QStringLiteral("<unset>") : explicitInstanceLayers)
                   << "VK_LAYER_PATH=" << (explicitLayerPath.isEmpty() ? QStringLiteral("<unset>") : explicitLayerPath)
                   << "availableLayers=" << info.layers;

        if (info.loaderAvailable && info.enumerateLayersAvailable && !hasKhronosValidation) {
            qWarning() << "[DiligentDeviceManager][VulkanValidation] VK_LAYER_KHRONOS_validation is not available."
                       << "Detailed Vulkan validation output may be missing.";
        }
    }

    bool tryCreateD3D12Device(RefCntAutoPtr<IRenderDevice>& outDevice,
                              RefCntAutoPtr<IDeviceContext>& outImmediateContext)
    {
        auto* pFactory = resolveD3D12Factory();
        if (!pFactory) {
            return false;
        }

        EngineD3D12CreateInfo creationAttribs = {};
        creationAttribs.EnableValidation = true;
        creationAttribs.SetValidationLevel(Diligent::VALIDATION_LEVEL_2);
        
        // 1. Try with Ray Tracing and VRS enabled
        creationAttribs.Features.RayTracing = DEVICE_FEATURE_STATE_ENABLED;
        creationAttribs.Features.VariableRateShading = DEVICE_FEATURE_STATE_ENABLED;
        pFactory->CreateDeviceAndContextsD3D12(creationAttribs, &outDevice, &outImmediateContext);
        
        if (!outDevice) {
            // 2. Fallback: VRS disabled
            qDebug() << "[DiligentDeviceManager] D3D12: VRS not supported, retrying without.";
            creationAttribs.Features.VariableRateShading = DEVICE_FEATURE_STATE_DISABLED;
            pFactory->CreateDeviceAndContextsD3D12(creationAttribs, &outDevice, &outImmediateContext);
        }
        
        if (!outDevice) {
            // 3. Fallback: Ray Tracing disabled
            qDebug() << "[DiligentDeviceManager] D3D12: Ray Tracing not supported, falling back.";
            creationAttribs.Features.RayTracing = DEVICE_FEATURE_STATE_DISABLED;
            pFactory->CreateDeviceAndContextsD3D12(creationAttribs, &outDevice, &outImmediateContext);
        } else {
            qDebug() << "[DiligentDeviceManager] D3D12: Ray Tracing ENABLED.";
            if (creationAttribs.Features.VariableRateShading != DEVICE_FEATURE_STATE_DISABLED) {
                qDebug() << "[DiligentDeviceManager] D3D12: VRS ENABLED.";
            }
        }

        return outDevice && outImmediateContext;
    }

    bool tryCreateVulkanDevice(RefCntAutoPtr<IRenderDevice>& outDevice,
                               RefCntAutoPtr<IDeviceContext>& outImmediateContext)
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
        creationAttribs.Features.VariableRateShading = Diligent::DEVICE_FEATURE_STATE_ENABLED;

        // 1. Try with Ray Tracing enabled
        creationAttribs.Features.RayTracing = DEVICE_FEATURE_STATE_ENABLED;
        pFactory->CreateDeviceAndContextsVk(creationAttribs, &outDevice, &outImmediateContext);

        if (!outDevice) {
            // 2. Fallback: Ray Tracing disabled
            qDebug() << "[DiligentDeviceManager] Vulkan: Ray Tracing not supported, falling back.";
            creationAttribs.Features.RayTracing = DEVICE_FEATURE_STATE_DISABLED;
            pFactory->CreateDeviceAndContextsVk(creationAttribs, &outDevice, &outImmediateContext);
        } else {
            qDebug() << "[DiligentDeviceManager] Vulkan: Ray Tracing ENABLED.";
        }

        return outDevice && outImmediateContext;
    }
}

bool acquireSharedRenderDeviceForCurrentBackend(
    RefCntAutoPtr<IRenderDevice>& outDevice,
    RefCntAutoPtr<IDeviceContext>& outImmediateContext)
{
    auto& shared = sharedRenderDeviceState();
    std::lock_guard<std::mutex> lock(shared.mutex);

    const auto backendPreference = getBackendPreferenceFromEnv();
    if (shared.device && shared.immediateContext) {
        if (!backendAllowsReuse(backendPreference, shared.type)) {
            qWarning() << "[DiligentDeviceManager] shared device already initialized as"
                       << deviceTypeName(shared.type)
                       << "while requested backend is"
                       << backendPreferenceName(backendPreference)
                       << ". Reusing shared device.";
        }
        ++shared.refCount;
        outDevice = shared.device;
        outImmediateContext = shared.immediateContext;
        return true;
    }

    bool created = false;
    switch (backendPreference) {
        case RenderBackendPreference::Vulkan:
            created = tryCreateVulkanDevice(shared.device, shared.immediateContext);
            if (!created) {
                qWarning() << "[DiligentDeviceManager] Vulkan device creation failed. Falling back to d3d12.";
                created = tryCreateD3D12Device(shared.device, shared.immediateContext);
            }
            break;
        case RenderBackendPreference::D3D12:
            created = tryCreateD3D12Device(shared.device, shared.immediateContext);
            break;
        case RenderBackendPreference::Auto:
        default:
            created = tryCreateD3D12Device(shared.device, shared.immediateContext);
            if (!created) {
                qWarning() << "[DiligentDeviceManager] D3D12 device creation failed. Trying Vulkan.";
                created = tryCreateVulkanDevice(shared.device, shared.immediateContext);
            }
            break;
    }

    if (!created || !shared.device || !shared.immediateContext) {
        shared.device.Release();
        shared.immediateContext.Release();
        shared.type = RENDER_DEVICE_TYPE_UNDEFINED;
        return false;
    }

    shared.type = shared.device->GetDeviceInfo().Type;
    shared.refCount = 1;
    outDevice = shared.device;
    outImmediateContext = shared.immediateContext;
    qDebug() << "[DiligentDeviceManager] shared device acquired type="
             << deviceTypeName(shared.type);
    return true;
}

void releaseSharedRenderDevice()
{
    auto& shared = sharedRenderDeviceState();
    std::lock_guard<std::mutex> lock(shared.mutex);

    const auto previous = shared.refCount.load();
    if (previous == 0) {
        return;
    }

    const auto remaining = --shared.refCount;
    if (remaining > 0) {
        return;
    }

    shared.immediateContext.Release();
    shared.device.Release();
    shared.type = RENDER_DEVICE_TYPE_UNDEFINED;
}

RENDER_DEVICE_TYPE sharedRenderDeviceType()
{
    auto& shared = sharedRenderDeviceState();
    std::lock_guard<std::mutex> lock(shared.mutex);
    return shared.type;
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
    bool usingSharedDevice_ = false;
    int currentPhysicalWidth_ = 0;
    int currentPhysicalHeight_ = 0;
    qreal currentDevicePixelRatio_ = 1.0;
    bool rtSupported_ = false;

    Impl() = default;

    Impl(RefCntAutoPtr<IRenderDevice> device, RefCntAutoPtr<IDeviceContext> context)
        : device_(device), immediateContext_(context), initialized_(true)
    {
        if (device_ && immediateContext_) {
            device_->CreateDeferredContext(&deferredContext_);
            rtSupported_ = device_->GetDeviceInfo().Features.RayTracing != DEVICE_FEATURE_STATE_DISABLED;
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
    QElapsedTimer timer;
    timer.start();
    const auto backendPreference = getBackendPreferenceFromEnv();
    qDebug() << "[DiligentDeviceManager] initialize requested backend="
             << backendPreferenceName(backendPreference);
    logVulkanValidationInfo();

    if (!acquireSharedRenderDeviceForCurrentBackend(device_, immediateContext_)) {
        qWarning() << "Failed to create Diligent Engine device and contexts.";
        return;
    }
    qInfo() << "[DiligentDeviceManager][Init] acquire device ms=" << timer.elapsed();
    usingSharedDevice_ = true;

    qDebug() << "[DiligentDeviceManager] device created type="
             << deviceTypeName(device_->GetDeviceInfo().Type);

    device_->CreateDeferredContext(&deferredContext_);
    rtSupported_ = device_->GetDeviceInfo().Features.RayTracing != DEVICE_FEATURE_STATE_DISABLED;

    currentPhysicalWidth_ = static_cast<int>(widget_->width() * widget_->devicePixelRatio());
    currentPhysicalHeight_ = static_cast<int>(widget_->height() * widget_->devicePixelRatio());
    currentDevicePixelRatio_ = widget_->devicePixelRatio();

    HWND parentHwnd = reinterpret_cast<HWND>(widget_->winId());
    timer.restart();
    renderHwnd_ = CreateWindowEx(
        0, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, currentPhysicalWidth_, currentPhysicalHeight_,
        parentHwnd, nullptr, GetModuleHandle(nullptr), nullptr);

    if (!createSwapChainForBackend(renderHwnd_, currentPhysicalWidth_, currentPhysicalHeight_)) {
        qWarning() << "Failed to create swap chain for the current backend.";
        return;
    }
    qInfo() << "[DiligentDeviceManager][Init] create swapchain ms=" << timer.elapsed();

    Diligent::Viewport VP;
    VP.Width = static_cast<float>(currentPhysicalWidth_);
    VP.Height = static_cast<float>(currentPhysicalHeight_);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0.0f;
    VP.TopLeftY = 0.0f;
    immediateContext_->SetViewports(1, &VP, currentPhysicalWidth_, currentPhysicalHeight_);
    qInfo() << "[DiligentDeviceManager][Init] final viewport ms=" << timer.elapsed();

    initialized_ = true;
}

void DiligentDeviceManager::Impl::initializeHeadless()
{
    if (!acquireSharedRenderDeviceForCurrentBackend(device_, immediateContext_) || !device_ || !immediateContext_) {
        qWarning() << "DiligentDeviceManager::initializeHeadless: failed to create device.";
        return;
    }

    usingSharedDevice_ = true;
    device_->CreateDeferredContext(&deferredContext_);
    qDebug() << "[DiligentDeviceManager] headless device created type="
             << deviceTypeName(device_->GetDeviceInfo().Type);
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
    if (newWidth == currentPhysicalWidth_ &&
        newHeight == currentPhysicalHeight_ &&
        qFuzzyCompare(newDevicePixelRatio, currentDevicePixelRatio_)) {
        return;
    }
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
    SCDesc.ColorBufferFormat = RenderConfig::MainRTVFormat;
    SCDesc.DepthBufferFormat = TEX_FORMAT_UNKNOWN;
    SCDesc.BufferCount = 2;
    SCDesc.Usage = SWAP_CHAIN_USAGE_RENDER_TARGET;

    Win32NativeWindow swapChainWindow;
    swapChainWindow.hWnd = hwnd;

    const auto deviceType = device_->GetDeviceInfo().Type;
    if (deviceType == RENDER_DEVICE_TYPE_VULKAN) {
        qDebug() << "[DiligentDeviceManager] creating Vulkan swapchain"
                 << width << "x" << height;
        auto* pFactoryVk = resolveVkFactory();
        if (!pFactoryVk) {
            return false;
        }
        pFactoryVk->CreateSwapChainVk(device_, immediateContext_, SCDesc, swapChainWindow, &swapChain_);
        return swapChain_ != nullptr;
    }

    qDebug() << "[DiligentDeviceManager] creating D3D12 swapchain"
             << width << "x" << height;
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
    if (usingSharedDevice_) {
        releaseSharedRenderDevice();
        usingSharedDevice_ = false;
    }
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
    SCDesc.ColorBufferFormat = RenderConfig::MainRTVFormat;
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

bool DiligentDeviceManager::isRayTracingSupported() const
{
    return impl_->rtSupported_;
}

}
