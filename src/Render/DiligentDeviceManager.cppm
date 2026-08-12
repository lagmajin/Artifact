module;
#include <utility>
#include <algorithm>
#include <limits>
#include <iterator>
#include <QWidget>
#include <QDebug>
#include <QSize>
#include <QString>
#include <QStringList>
#include <atomic>
#include <mutex>
#include <vector>
#include <vulkan/vulkan.h>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <CommandQueue.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/RenderDeviceVk.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/CommandQueueVk.h>
#include <SwapChain.h>
#include <RefCntAutoPtr.hpp>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/CommandQueueD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/RenderDeviceD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/SwapChainD3D12.h>
#include <DiligentCore/Graphics/ShaderTools/include/DXCompiler.hpp>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <d3d12sdklayers.h>
#include <windows.h>
#include <wrl/client.h>

module Artifact.Render.DiligentDeviceManager;

import Artifact.Render.Config;

namespace Artifact {

using namespace Diligent;
using Microsoft::WRL::ComPtr;

namespace {
    void configureHdrColorSpace(ISwapChain* swapChain, bool hdrEnabled)
    {
        if (!swapChain) {
            return;
        }
        RefCntAutoPtr<ISwapChainD3D12> d3d12SwapChain{
            swapChain, IID_SwapChainD3D12};
        if (!d3d12SwapChain) {
            return;
        }
        IDXGISwapChain* nativeSwapChain = d3d12SwapChain->GetDXGISwapChain();
        if (!nativeSwapChain) {
            return;
        }
        ComPtr<IDXGISwapChain3> swapChain3;
        if (FAILED(nativeSwapChain->QueryInterface(
                IID_PPV_ARGS(&swapChain3))) || !swapChain3) {
            return;
        }
        const DXGI_COLOR_SPACE_TYPE colorSpace =
            hdrEnabled ? DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709
                       : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
        const HRESULT result = swapChain3->SetColorSpace1(colorSpace);
        if (FAILED(result)) {
            qWarning() << "[DiligentDeviceManager] SetColorSpace1 failed"
                       << Qt::hex << static_cast<unsigned long>(result);
        }
    }

    D3D12AgilityCapabilitySnapshot queryD3D12AgilityCapabilitiesInternal(
        IRenderDevice* device)
    {
        D3D12AgilityCapabilitySnapshot snapshot;
#if D3D12_SUPPORTED
        snapshot.headerSdkVersion = D3D12_SDK_VERSION;
        if (!device ||
            device->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_D3D12) {
            return snapshot;
        }

        RefCntAutoPtr<IRenderDeviceD3D12> deviceD3D12{
            device, IID_RenderDeviceD3D12};
        if (!deviceD3D12) {
            return snapshot;
        }
        ID3D12Device* nativeDevice = deviceD3D12->GetD3D12Device();
        if (!nativeDevice) {
            return snapshot;
        }
        snapshot.available = true;

        HMODULE agilityModule = ::GetModuleHandleW(L"D3D12Core.dll");
        snapshot.agilityRuntimeLoaded = agilityModule != nullptr;
        if (agilityModule) {
            wchar_t modulePath[MAX_PATH] = {};
            const DWORD pathLength = ::GetModuleFileNameW(
                agilityModule, modulePath, static_cast<DWORD>(std::size(modulePath)));
            if (pathLength > 0 && pathLength < std::size(modulePath)) {
                snapshot.runtimePath = QString::fromWCharArray(
                    modulePath, static_cast<qsizetype>(pathLength));
            }
        }

        D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = {
            D3D_SHADER_MODEL_6_9};
        if (SUCCEEDED(nativeDevice->CheckFeatureSupport(
                D3D12_FEATURE_SHADER_MODEL, &shaderModel,
                sizeof(shaderModel)))) {
            snapshot.deviceShaderModelKnown = true;
            snapshot.deviceShaderModelMajor =
                (static_cast<Uint32>(shaderModel.HighestShaderModel) >> 4u) & 0x0fu;
            snapshot.deviceShaderModelMinor =
                static_cast<Uint32>(shaderModel.HighestShaderModel) & 0x0fu;
            snapshot.deviceShaderModel69Supported =
                shaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_9;
        }

        if (auto* dxc = deviceD3D12->GetDXCompiler()) {
            snapshot.dxcAvailable = dxc->IsLoaded();
            if (snapshot.dxcAvailable) {
                const Version compilerVersion = dxc->GetVersion();
                const ShaderVersion shaderVersion = dxc->GetMaxShaderModel();
                snapshot.dxcVersionMajor = compilerVersion.Major;
                snapshot.dxcVersionMinor = compilerVersion.Minor;
                snapshot.dxcShaderModelMajor = shaderVersion.Major;
                snapshot.dxcShaderModelMinor = shaderVersion.Minor;
                snapshot.dxcShaderModel69Supported =
                    shaderVersion >= ShaderVersion{6, 9};
            }
        }

#if D3D12_SDK_VERSION >= 619
        D3D12_FEATURE_DATA_TIGHT_ALIGNMENT tightAlignment = {};
        if (SUCCEEDED(nativeDevice->CheckFeatureSupport(
                D3D12_FEATURE_D3D12_TIGHT_ALIGNMENT, &tightAlignment,
                sizeof(tightAlignment)))) {
            snapshot.tightAlignmentTier =
                static_cast<Uint32>(tightAlignment.SupportTier);
            snapshot.tightAlignmentAvailable =
                tightAlignment.SupportTier >= D3D12_TIGHT_ALIGNMENT_TIER_1;
        }

        D3D12_FEATURE_DATA_D3D12_OPTIONS22 options22 = {};
        if (SUCCEEDED(nativeDevice->CheckFeatureSupport(
                D3D12_FEATURE_D3D12_OPTIONS22, &options22,
                sizeof(options22)))) {
            snapshot.options22Available = true;
            if (options22.Max1DDispatchSize > 0) {
                snapshot.max1DDispatchSize = options22.Max1DDispatchSize;
            }
            if (options22.Max1DDispatchMeshSize > 0) {
                snapshot.max1DDispatchMeshSize =
                    options22.Max1DDispatchMeshSize;
            }
        }

        ComPtr<ID3D12Device15> device15;
        snapshot.device15Available = SUCCEEDED(nativeDevice->QueryInterface(
            IID_PPV_ARGS(&device15))) && device15 != nullptr;
        snapshot.periodicTrimNotificationAvailable =
            snapshot.device15Available;
        snapshot.cpuTimelineQueryResolveAvailable =
            snapshot.device15Available;
        snapshot.revisedViewCreationAvailable =
            snapshot.device15Available;
#endif
#else
        (void)device;
#endif
        return snapshot;
    }

    void reportLiveD3D12Objects(IRenderDevice* device)
    {
#if D3D12_SUPPORTED
        if (device == nullptr) {
            return;
        }
        RefCntAutoPtr<IRenderDeviceD3D12> deviceD3D12{device, IID_RenderDeviceD3D12};
        if (!deviceD3D12) {
            return;
        }
        ID3D12Device* d3d12Device = deviceD3D12->GetD3D12Device();
        if (d3d12Device == nullptr) {
            return;
        }
        ComPtr<ID3D12DebugDevice> debugDevice;
        if (FAILED(d3d12Device->QueryInterface(IID_PPV_ARGS(&debugDevice))) || !debugDevice) {
            return;
        }
        debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
#else
        (void)device;
#endif
    }

    constexpr Uint32 kAsyncShaderCompileThreads = 2;

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
        D3D12AgilityCapabilitySnapshot agilityCapabilities;
        std::atomic_uint32_t refCount{0};
        std::atomic_uint64_t trimRequestGeneration{0};
        std::atomic_uint64_t trimRequestedBytes{0};
        Uint64 consumedTrimGeneration = 0;
#if D3D12_SDK_VERSION >= 619
        ComPtr<ID3D12Device15> trimDevice;
        DWORD trimCallbackCookie = 0;
        bool trimCallbackRegistered = false;
#endif
    };

#if D3D12_SDK_VERSION >= 619
    void __stdcall onD3D12TrimNotification(
        const D3D12_TRIM_NOTIFICATION* notification)
    {
        if (!notification || !notification->pContext) {
            return;
        }
        auto* shared = static_cast<SharedRenderDeviceState*>(
            notification->pContext);
        constexpr Uint64 kDefaultPeriodicTrimBytes = 64ull * 1024ull * 1024ull;
        const Uint64 requestedBytes = notification->NumBytesToTrim > 0
            ? notification->NumBytesToTrim
            : kDefaultPeriodicTrimBytes;
        shared->trimRequestGeneration.fetch_add(1, std::memory_order_release);
        Uint64 current = shared->trimRequestedBytes.load(
            std::memory_order_relaxed);
        while (current < requestedBytes &&
               !shared->trimRequestedBytes.compare_exchange_weak(
                   current, requestedBytes, std::memory_order_release,
                   std::memory_order_relaxed)) {
        }
    }
#endif

    void unregisterSharedTrimNotification(SharedRenderDeviceState& shared)
    {
#if D3D12_SDK_VERSION >= 619
        if (shared.trimDevice && shared.trimCallbackRegistered) {
            shared.trimDevice->UnregisterTrimNotificationCallback(
                shared.trimCallbackCookie);
        }
        shared.trimCallbackCookie = 0;
        shared.trimCallbackRegistered = false;
        shared.trimDevice.Reset();
#else
        (void)shared;
#endif
        shared.agilityCapabilities.periodicTrimNotificationRegistered = false;
    }

    void registerSharedTrimNotification(SharedRenderDeviceState& shared)
    {
#if D3D12_SDK_VERSION >= 619
        unregisterSharedTrimNotification(shared);
        if (!shared.device ||
            shared.type != RENDER_DEVICE_TYPE_D3D12 ||
            !shared.agilityCapabilities.periodicTrimNotificationAvailable) {
            return;
        }
        RefCntAutoPtr<IRenderDeviceD3D12> diligentDevice{
            shared.device, IID_RenderDeviceD3D12};
        if (!diligentDevice) {
            return;
        }
        ID3D12Device* nativeDevice = diligentDevice->GetD3D12Device();
        if (!nativeDevice || FAILED(nativeDevice->QueryInterface(
                IID_PPV_ARGS(&shared.trimDevice))) || !shared.trimDevice) {
            shared.trimDevice.Reset();
            return;
        }
        D3D12_REGISTER_TRIM_NOTIFICATION registration = {};
        registration.pfnCallback = onD3D12TrimNotification;
        registration.pContext = &shared;
        if (FAILED(shared.trimDevice->RegisterTrimNotificationCallback(
                &registration))) {
            shared.trimDevice.Reset();
            return;
        }
        shared.trimCallbackCookie = registration.CallbackCookie;
        shared.trimCallbackRegistered = true;
        shared.trimRequestedBytes.store(0, std::memory_order_release);
        shared.agilityCapabilities.periodicTrimNotificationRegistered = true;
#else
        (void)shared;
#endif
    }

    struct VulkanValidationInfo {
        bool loaderAvailable = false;
        bool enumerateLayersAvailable = false;
        QStringList layers;
    };

    bool rayTracingEnabledByConfig()
    {
        const QString value = qEnvironmentVariable("ARTIFACT_ENABLE_RAY_TRACING").trimmed().toLower();
        if (value.isEmpty()) {
            return false;
        }
        return value != "0" &&
               value != "false" &&
               value != "off" &&
               value != "disabled" &&
               value != "no";
    }

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

    QString adapterVendorName(const Uint32 vendorId)
    {
        switch (vendorId) {
            case 0x10de: return QStringLiteral("NVIDIA");
            case 0x1002:
            case 0x1022: return QStringLiteral("AMD");
            case 0x8086: return QStringLiteral("Intel");
            case 0x106b: return QStringLiteral("Apple");
            case 0x1414: return QStringLiteral("Microsoft");
            default: return QStringLiteral("Unknown");
        }
    }

    QString adapterTypeName(const ADAPTER_TYPE type)
    {
        switch (type) {
            case ADAPTER_TYPE_SOFTWARE: return QStringLiteral("Software");
            case ADAPTER_TYPE_INTEGRATED: return QStringLiteral("Integrated");
            case ADAPTER_TYPE_DISCRETE: return QStringLiteral("Discrete");
            case ADAPTER_TYPE_UNKNOWN:
            default: return QStringLiteral("Unknown");
        }
    }

    int adapterAutoScore(const GraphicsAdapterInfo& adapter)
    {
        int score = 0;
        switch (adapter.Type) {
            case ADAPTER_TYPE_DISCRETE: score += 4000; break;
            case ADAPTER_TYPE_INTEGRATED: score += 2000; break;
            case ADAPTER_TYPE_SOFTWARE: score -= 10000; break;
            case ADAPTER_TYPE_UNKNOWN:
            default: score += 1000; break;
        }
        constexpr Uint64 bytesPerGiB = 1024ull * 1024ull * 1024ull;
        const Uint64 localMemoryGiB = adapter.Memory.LocalMemory / bytesPerGiB;
        const Uint64 unifiedMemoryGiB = adapter.Memory.UnifiedMemory / bytesPerGiB;
        score += static_cast<int>(std::min<Uint64>(localMemoryGiB * 128ull, 2048ull));
        score += static_cast<int>(std::min<Uint64>(unifiedMemoryGiB * 32ull, 512ull));
        if (adapter.Features.RayTracing != DEVICE_FEATURE_STATE_DISABLED) {
            score += 1000;
        }
        if (adapter.NumOutputs > 0) {
            score += 50;
        }
        return score;
    }

    enum class GpuAdapterPolicy {
        Auto,
        HighPerformance,
        PowerSaving,
        Specific
    };

    GpuAdapterPolicy gpuAdapterPolicyFromEnv()
    {
        const QString value =
            qEnvironmentVariable("ARTIFACT_GPU_POLICY").trimmed().toLower();
        if (value == "high-performance" || value == "high_performance" ||
            value == "performance" || value == "discrete" || value == "dgpu") {
            return GpuAdapterPolicy::HighPerformance;
        }
        if (value == "power-saving" || value == "power_saving" ||
            value == "powersave" || value == "integrated" || value == "igpu") {
            return GpuAdapterPolicy::PowerSaving;
        }
        if (value == "specific" || value == "manual") {
            return GpuAdapterPolicy::Specific;
        }
        return GpuAdapterPolicy::Auto;
    }

    const char* gpuAdapterPolicyName(const GpuAdapterPolicy policy)
    {
        switch (policy) {
            case GpuAdapterPolicy::HighPerformance: return "high-performance";
            case GpuAdapterPolicy::PowerSaving: return "power-saving";
            case GpuAdapterPolicy::Specific: return "specific";
            case GpuAdapterPolicy::Auto:
            default: return "auto";
        }
    }

    struct GpuAdapterSelection {
        Uint32 adapterId = DEFAULT_ADAPTER_ID;
        QString description;
        GpuAdapterPolicy policy = GpuAdapterPolicy::Auto;
        bool resolved = false;
    };

    GpuAdapterSelection selectGpuAdapter(IEngineFactory* factory)
    {
        GpuAdapterSelection selection;
        selection.policy = gpuAdapterPolicyFromEnv();
        if (!factory) {
            return selection;
        }

        Uint32 adapterCount = 0;
        factory->EnumerateAdapters(Version{}, adapterCount, nullptr);
        if (adapterCount == 0) {
            return selection;
        }
        std::vector<GraphicsAdapterInfo> adapters(adapterCount);
        factory->EnumerateAdapters(Version{}, adapterCount, adapters.data());
        adapters.resize(adapterCount);

        int selectedIndex = -1;
        if (selection.policy == GpuAdapterPolicy::Specific) {
            const QString requested =
                qEnvironmentVariable("ARTIFACT_GPU_ADAPTER").trimmed();
            bool numericOk = false;
            const int numericIndex = requested.toInt(&numericOk);
            if (numericOk && numericIndex >= 0 &&
                numericIndex < static_cast<int>(adapterCount)) {
                selectedIndex = numericIndex;
            } else if (!requested.isEmpty()) {
                for (Uint32 index = 0; index < adapterCount; ++index) {
                    const QString name = QString::fromLatin1(
                        adapters[index].Description).trimmed();
                    if (name.contains(requested, Qt::CaseInsensitive)) {
                        selectedIndex = static_cast<int>(index);
                        break;
                    }
                }
            }
        }

        if (selectedIndex < 0) {
            int bestScore = (std::numeric_limits<int>::lowest)();
            for (Uint32 index = 0; index < adapterCount; ++index) {
                const auto& adapter = adapters[index];
                int score = adapterAutoScore(adapter);
                if (selection.policy == GpuAdapterPolicy::PowerSaving) {
                    score += adapter.Type == ADAPTER_TYPE_INTEGRATED ? 6000 : 0;
                    score -= adapter.Type == ADAPTER_TYPE_DISCRETE ? 3000 : 0;
                } else if (selection.policy == GpuAdapterPolicy::HighPerformance) {
                    score += adapter.Type == ADAPTER_TYPE_DISCRETE ? 2000 : 0;
                }
                if (score > bestScore) {
                    bestScore = score;
                    selectedIndex = static_cast<int>(index);
                }
            }
        }

        if (selectedIndex >= 0) {
            selection.adapterId = static_cast<Uint32>(selectedIndex);
            selection.description = QString::fromLatin1(
                adapters[selection.adapterId].Description).trimmed();
            selection.resolved = true;
        }
        return selection;
    }

    void appendFactoryAdapters(IEngineFactory* factory,
                               const QString& backend,
                               std::vector<GpuAdapterCandidate>& output)
    {
        if (!factory) {
            return;
        }
        Uint32 adapterCount = 0;
        factory->EnumerateAdapters(Version{}, adapterCount, nullptr);
        if (adapterCount == 0) {
            return;
        }
        std::vector<GraphicsAdapterInfo> adapters(adapterCount);
        factory->EnumerateAdapters(Version{}, adapterCount, adapters.data());
        adapters.resize(adapterCount);
        for (Uint32 index = 0; index < adapterCount; ++index) {
            const auto& adapter = adapters[index];
            GpuAdapterCandidate candidate;
            candidate.adapterId = index;
            candidate.name = QString::fromLatin1(adapter.Description).trimmed();
            candidate.vendor = adapterVendorName(adapter.VendorId);
            candidate.type = adapterTypeName(adapter.Type);
            candidate.backend = backend;
            candidate.vendorId = adapter.VendorId;
            candidate.deviceId = adapter.DeviceId;
            candidate.localMemoryBytes = adapter.Memory.LocalMemory;
            candidate.unifiedMemoryBytes = adapter.Memory.UnifiedMemory;
            candidate.rayTracingSupported =
                adapter.Features.RayTracing != DEVICE_FEATURE_STATE_DISABLED;
            candidate.autoScore = adapterAutoScore(adapter);
            output.push_back(std::move(candidate));
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

        const QString requestedAdapter = qEnvironmentVariable("ARTIFACT_GPU_ADAPTER").trimmed();
        if (!requestedAdapter.isEmpty() && requestedAdapter.compare(QStringLiteral("auto"), Qt::CaseInsensitive) != 0) {
            ComPtr<IDXGIFactory6> dxgiFactory;
            if (SUCCEEDED(::CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)))) {
                ComPtr<IDXGIAdapter1> selectedAdapter;
                UINT hardwareIndex = 0;
                for (UINT adapterIndex = 0;; ++adapterIndex) {
                    ComPtr<IDXGIAdapter1> adapter;
                    if (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) == DXGI_ERROR_NOT_FOUND) {
                        break;
                    }

                    DXGI_ADAPTER_DESC1 desc{};
                    if (FAILED(adapter->GetDesc1(&desc)) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                        continue;
                    }

                    const QString description = QString::fromWCharArray(desc.Description);
                    const bool indexMatch = requestedAdapter == QString::number(hardwareIndex);
                    const bool nameMatch = description.contains(requestedAdapter, Qt::CaseInsensitive);
                    if (indexMatch || nameMatch) {
                        selectedAdapter = adapter;
                        qDebug() << "[DiligentDeviceManager] selected D3D12 adapter"
                                 << "index=" << hardwareIndex
                                 << "name=" << description;
                        break;
                    }
                    ++hardwareIndex;
                }

                if (selectedAdapter) {
                    ComPtr<ID3D12Device> nativeDevice;
                    HRESULT hr = ::D3D12CreateDevice(
                        selectedAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
                        IID_PPV_ARGS(&nativeDevice));
                    if (SUCCEEDED(hr) && nativeDevice) {
                        D3D12_COMMAND_QUEUE_DESC queueDesc{};
                        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
                        ComPtr<ID3D12CommandQueue> nativeQueue;
                        hr = nativeDevice->CreateCommandQueue(
                            &queueDesc, IID_PPV_ARGS(&nativeQueue));
                        if (SUCCEEDED(hr) && nativeQueue) {
                            EngineD3D12CreateInfo attachInfo = {};
                            attachInfo.EnableValidation = true;
                            attachInfo.SetValidationLevel(Diligent::VALIDATION_LEVEL_2);
                            attachInfo.Features.MultithreadedResourceCreation = DEVICE_FEATURE_STATE_DISABLED;
                            attachInfo.NumAsyncShaderCompilationThreads = kAsyncShaderCompileThreads;
                            attachInfo.Features.RayTracing = rayTracingEnabledByConfig()
                                ? DEVICE_FEATURE_STATE_ENABLED
                                : DEVICE_FEATURE_STATE_DISABLED;

                            ICommandQueueD3D12* diligentQueue = nullptr;
                            pFactory->CreateCommandQueueD3D12(
                                nativeDevice.Get(), nativeQueue.Get(), nullptr, &diligentQueue);
                            if (diligentQueue) {
                                ICommandQueueD3D12* queues[] = {diligentQueue};
                                pFactory->AttachToD3D12Device(
                                    nativeDevice.Get(), 1, queues, attachInfo,
                                    &outDevice, &outImmediateContext);
                                diligentQueue->Release();
                            }
                        }
                    }
                } else {
                    qWarning() << "[DiligentDeviceManager] requested D3D12 adapter was not found:"
                               << requestedAdapter << ". Falling back to automatic selection.";
                }
            } else {
                qWarning() << "[DiligentDeviceManager] failed to create DXGI factory."
                           << "Falling back to automatic D3D12 adapter selection.";
            }

            if (outDevice && outImmediateContext) {
                return true;
            }
        }

        EngineD3D12CreateInfo creationAttribs = {};
        const auto adapterSelection = selectGpuAdapter(pFactory);
        creationAttribs.AdapterId = adapterSelection.adapterId;
        creationAttribs.EnableValidation = true;
        creationAttribs.SetValidationLevel(Diligent::VALIDATION_LEVEL_2);
        creationAttribs.Features.MultithreadedResourceCreation = DEVICE_FEATURE_STATE_DISABLED;
        creationAttribs.NumAsyncShaderCompilationThreads = kAsyncShaderCompileThreads;

        if (rayTracingEnabledByConfig()) {
            // 1. Try with Ray Tracing enabled
            creationAttribs.Features.RayTracing = DEVICE_FEATURE_STATE_ENABLED;
            pFactory->CreateDeviceAndContextsD3D12(creationAttribs, &outDevice, &outImmediateContext);

            if (!outDevice) {
                // 2. Fallback: Ray Tracing disabled
                qDebug() << "[DiligentDeviceManager] D3D12: Ray Tracing not supported, falling back.";
                creationAttribs.Features.RayTracing = DEVICE_FEATURE_STATE_DISABLED;
                pFactory->CreateDeviceAndContextsD3D12(creationAttribs, &outDevice, &outImmediateContext);
            } else {
                qDebug() << "[DiligentDeviceManager] D3D12: Ray Tracing ENABLED.";
            }
        } else {
            creationAttribs.Features.RayTracing = DEVICE_FEATURE_STATE_DISABLED;
            qDebug() << "[DiligentDeviceManager] D3D12: Ray Tracing disabled by default. Set ARTIFACT_ENABLE_RAY_TRACING=1 to enable.";
            pFactory->CreateDeviceAndContextsD3D12(creationAttribs, &outDevice, &outImmediateContext);
        }

        if (!outDevice && creationAttribs.AdapterId != DEFAULT_ADAPTER_ID) {
            qWarning() << "[DiligentDeviceManager] D3D12 adapter selection failed; "
                          "retrying default adapter"
                       << "policy=" << gpuAdapterPolicyName(adapterSelection.policy)
                       << "adapterId=" << creationAttribs.AdapterId
                       << "adapter=" << adapterSelection.description;
            creationAttribs.AdapterId = DEFAULT_ADAPTER_ID;
            creationAttribs.Features.RayTracing = DEVICE_FEATURE_STATE_DISABLED;
            pFactory->CreateDeviceAndContextsD3D12(
                creationAttribs, &outDevice, &outImmediateContext);
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
        const auto adapterSelection = selectGpuAdapter(pFactory);
        creationAttribs.AdapterId = adapterSelection.adapterId;
        creationAttribs.EnableValidation = true;
        creationAttribs.SetValidationLevel(Diligent::VALIDATION_LEVEL_2);
        creationAttribs.Features.MultithreadedResourceCreation = DEVICE_FEATURE_STATE_DISABLED;
        creationAttribs.NumAsyncShaderCompilationThreads = kAsyncShaderCompileThreads;

        if (rayTracingEnabledByConfig()) {
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
        } else {
            creationAttribs.Features.RayTracing = DEVICE_FEATURE_STATE_DISABLED;
            qDebug() << "[DiligentDeviceManager] Vulkan: Ray Tracing disabled by default. Set ARTIFACT_ENABLE_RAY_TRACING=1 to enable.";
            pFactory->CreateDeviceAndContextsVk(creationAttribs, &outDevice, &outImmediateContext);
        }

        if (!outDevice && creationAttribs.AdapterId != DEFAULT_ADAPTER_ID) {
            qWarning() << "[DiligentDeviceManager] Vulkan adapter selection failed; "
                          "retrying default adapter"
                       << "policy=" << gpuAdapterPolicyName(adapterSelection.policy)
                       << "adapterId=" << creationAttribs.AdapterId
                       << "adapter=" << adapterSelection.description;
            creationAttribs.AdapterId = DEFAULT_ADAPTER_ID;
            creationAttribs.Features.RayTracing = DEVICE_FEATURE_STATE_DISABLED;
            pFactory->CreateDeviceAndContextsVk(
                creationAttribs, &outDevice, &outImmediateContext);
        }

        return outDevice && outImmediateContext;
    }
}

D3D12AgilityCapabilitySnapshot queryD3D12AgilityCapabilities(
    IRenderDevice* device)
{
    return queryD3D12AgilityCapabilitiesInternal(device);
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
        shared.agilityCapabilities = {};
        return false;
    }

    shared.type = shared.device->GetDeviceInfo().Type;
    shared.agilityCapabilities =
        queryD3D12AgilityCapabilitiesInternal(shared.device);
    registerSharedTrimNotification(shared);
    shared.refCount = 1;
    outDevice = shared.device;
    outImmediateContext = shared.immediateContext;
    qDebug() << "[DiligentDeviceManager] shared device acquired type="
             << deviceTypeName(shared.type);
    return true;
}

SharedRenderDeviceLease::~SharedRenderDeviceLease()
{
    if (active_) {
        releaseSharedRenderDevice();
    }
}

bool SharedRenderDeviceLease::acquire(
    RefCntAutoPtr<IRenderDevice>& outDevice,
    RefCntAutoPtr<IDeviceContext>& outImmediateContext)
{
    if (active_) {
        return false;
    }
    active_ = acquireSharedRenderDeviceForCurrentBackend(
        outDevice, outImmediateContext);
    return active_;
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

    if (shared.immediateContext) {
        shared.immediateContext->Flush();
        shared.immediateContext->WaitForIdle();
    }
    unregisterSharedTrimNotification(shared);
    shared.immediateContext.Release();
    shared.device.Release();
    shared.type = RENDER_DEVICE_TYPE_UNDEFINED;
    shared.agilityCapabilities = {};
}

bool invalidateSharedRenderDeviceIfExclusive(IRenderDevice* expectedDevice)
{
    auto& shared = sharedRenderDeviceState();
    std::lock_guard<std::mutex> lock(shared.mutex);
    if (!expectedDevice || shared.device.RawPtr() != expectedDevice ||
        shared.refCount.load() != 1) {
        return false;
    }

    // A lost device must not be flushed or waited. Drop the shared registry so
    // the next acquire creates a fresh backend device and context.
    unregisterSharedTrimNotification(shared);
    shared.immediateContext.Release();
    shared.device.Release();
    shared.type = RENDER_DEVICE_TYPE_UNDEFINED;
    shared.agilityCapabilities = {};
    shared.refCount = 0;
    return true;
}

RENDER_DEVICE_TYPE sharedRenderDeviceType()
{
    auto& shared = sharedRenderDeviceState();
    std::lock_guard<std::mutex> lock(shared.mutex);
    return shared.type;
}

D3D12AgilityCapabilitySnapshot sharedD3D12AgilityCapabilities()
{
    auto& shared = sharedRenderDeviceState();
    std::lock_guard<std::mutex> lock(shared.mutex);
    return shared.agilityCapabilities;
}

D3D12TrimRequestSnapshot currentD3D12TrimRequest()
{
    auto& shared = sharedRenderDeviceState();
    D3D12TrimRequestSnapshot snapshot;
    snapshot.generation = shared.trimRequestGeneration.load(
        std::memory_order_acquire);
    snapshot.requestedBytes = shared.trimRequestedBytes.load(
        std::memory_order_acquire);
    snapshot.pending = snapshot.generation != 0 && snapshot.requestedBytes != 0;
    return snapshot;
}

D3D12TrimRequestSnapshot claimD3D12TrimRequest()
{
    auto& shared = sharedRenderDeviceState();
    std::lock_guard<std::mutex> lock(shared.mutex);
    D3D12TrimRequestSnapshot snapshot;
    snapshot.generation = shared.trimRequestGeneration.load(
        std::memory_order_acquire);
    snapshot.requestedBytes = shared.trimRequestedBytes.exchange(
        0, std::memory_order_acq_rel);
    snapshot.pending = snapshot.generation > shared.consumedTrimGeneration &&
                       snapshot.requestedBytes != 0;
    if (snapshot.pending) {
        shared.consumedTrimGeneration = snapshot.generation;
    }
    return snapshot;
}

class DiligentDeviceManager::Impl {
public:
    RefCntAutoPtr<IRenderDevice> device_;
    RefCntAutoPtr<IDeviceContext> immediateContext_;
    RefCntAutoPtr<IDeviceContext> deferredContext_;
    RefCntAutoPtr<ISwapChain> swapChain_;
    HWND renderHwnd_ = nullptr;
    HWND renderParentHwnd_ = nullptr;
    QWidget* widget_ = nullptr;
    bool initialized_ = false;
    bool deviceLost_ = false;
    bool usingSharedDevice_ = false;
    int currentPhysicalWidth_ = 0;
    int currentPhysicalHeight_ = 0;
    qreal currentDevicePixelRatio_ = 1.0;
    bool rtSupported_ = false;
    D3D12AgilityCapabilitySnapshot agilityCapabilities_;

    Impl() = default;

    Impl(RefCntAutoPtr<IRenderDevice> device, RefCntAutoPtr<IDeviceContext> context)
        : device_(device), immediateContext_(context), initialized_(true)
    {
        if (device_ && immediateContext_) {
            device_->CreateDeferredContext(&deferredContext_);
            rtSupported_ = device_->GetDeviceInfo().Features.RayTracing != DEVICE_FEATURE_STATE_DISABLED;
            agilityCapabilities_ = queryD3D12AgilityCapabilities(device_);
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
    bool ensureRenderChildWindow(QWidget* widget, int width, int height,
                                 const char* reason);
    bool createSwapChainForBackend(HWND hwnd, int width, int height);
};

// Custom window class for the DX12/Vulkan render surface.
// Unlike the default "STATIC" class, this suppresses WM_ERASEBKGND
// and WM_PAINT so the system never overwrites presented GPU content
// with the default light-grey background.
static LRESULT CALLBACK RenderHwndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_NCHITTEST:
        // Return HTTRANSPARENT so Windows skips this child HWND during
        // hit-testing and delivers mouse messages to the parent
        // CompositionViewport HWND instead.  WS_EX_TRANSPARENT alone is
        // not sufficient for child windows — the WndProc must explicitly
        // opt out of hit-testing.  D3D12/Vulkan presentation is unaffected.
        return HTTRANSPARENT;
    case WM_ERASEBKGND:
        return 1; // Claim we erased — DX12/Vulkan owns the surface
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);   // Validate the region without drawing
        return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static const wchar_t* ensureRenderWindowClass()
{
    static const wchar_t* kClassName = L"DiligentRenderSurface";
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc   = RenderHwndProc;
        wc.hInstance      = GetModuleHandle(nullptr);
        wc.lpszClassName  = kClassName;
        wc.hbrBackground  = nullptr; // No GDI background brush
        RegisterClassW(&wc);
        registered = true;
    }
    return kClassName;
}

void DiligentDeviceManager::Impl::initialize(QWidget* widget)
{
    if (!widget) {
        return;
    }

    widget_ = widget;
    const auto backendPreference = getBackendPreferenceFromEnv();
    qDebug() << "[DiligentDeviceManager] initialize requested backend="
             << backendPreferenceName(backendPreference);
    logVulkanValidationInfo();

    if (!acquireSharedRenderDeviceForCurrentBackend(device_, immediateContext_)) {
        qWarning() << "Failed to create Diligent Engine device and contexts.";
        return;
    }
    usingSharedDevice_ = true;

    qDebug() << "[DiligentDeviceManager] device created type="
             << deviceTypeName(device_->GetDeviceInfo().Type);

    device_->CreateDeferredContext(&deferredContext_);
    rtSupported_ = device_->GetDeviceInfo().Features.RayTracing != DEVICE_FEATURE_STATE_DISABLED;
    agilityCapabilities_ = sharedD3D12AgilityCapabilities();

    // Device is ready — mark initialized so shaders/PSOs can be created
    // even if the swapchain is deferred (widget may still be 0×0).
    initialized_ = true;

    currentPhysicalWidth_ = static_cast<int>(widget_->width() * widget_->devicePixelRatio());
    currentPhysicalHeight_ = static_cast<int>(widget_->height() * widget_->devicePixelRatio());
    currentDevicePixelRatio_ = widget_->devicePixelRatio();

    ensureRenderChildWindow(widget_, currentPhysicalWidth_,
                            currentPhysicalHeight_, "initialize");

    if (currentPhysicalWidth_ <= 0 || currentPhysicalHeight_ <= 0) {
        qWarning() << "[DiligentDeviceManager] swapchain deferred: widget is"
                   << currentPhysicalWidth_ << "x" << currentPhysicalHeight_
                   << "(will create on first resize)";
        return;
    }

    if (!createSwapChainForBackend(renderHwnd_, currentPhysicalWidth_, currentPhysicalHeight_)) {
        qWarning() << "[DiligentDeviceManager] swapchain creation failed — will retry on resize";
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
}

void DiligentDeviceManager::Impl::initializeHeadless()
{
    const auto backendPreference = getBackendPreferenceFromEnv();

    bool ok = false;
    switch (backendPreference) {
        case RenderBackendPreference::Vulkan:
            ok = tryCreateVulkanDevice(device_, immediateContext_) ||
                 tryCreateD3D12Device(device_, immediateContext_);
            break;
        case RenderBackendPreference::D3D12:
            ok = tryCreateD3D12Device(device_, immediateContext_);
            break;
        case RenderBackendPreference::Auto:
        default:
            ok = tryCreateD3D12Device(device_, immediateContext_) ||
                 tryCreateVulkanDevice(device_, immediateContext_);
            break;
    }

    if (!ok) {
        qWarning() << "DiligentDeviceManager::initializeHeadless: failed to create device.";
        return;
    }

    device_->CreateDeferredContext(&deferredContext_);
    agilityCapabilities_ = queryD3D12AgilityCapabilities(device_);
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

    ensureRenderChildWindow(window, currentPhysicalWidth_,
                            currentPhysicalHeight_, "createSwapChain");

    if (!swapChain_) {
        if (currentPhysicalWidth_ <= 0 || currentPhysicalHeight_ <= 0) {
            qWarning() << "[DiligentDeviceManager] createSwapChain deferred: size is"
                       << currentPhysicalWidth_ << "x" << currentPhysicalHeight_;
            return;
        }
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
    if (!widget || !device_) {
        return;
    }

    const int newWidth = static_cast<int>(widget->width() * widget->devicePixelRatio());
    const int newHeight = static_cast<int>(widget->height() * widget->devicePixelRatio());
    if (newWidth <= 0 || newHeight <= 0) {
        return;
    }

    // If no swapchain exists yet (deferred from 0×0 init), create from scratch
    if (!swapChain_) {
        qDebug() << "[DiligentDeviceManager] recreateSwapChain: no swapchain — creating fresh"
                 << newWidth << "x" << newHeight;
        createSwapChain(widget);
        return;
    }

    const qreal newDevicePixelRatio = widget->devicePixelRatio();
    currentPhysicalWidth_ = newWidth;
    currentPhysicalHeight_ = newHeight;
    currentDevicePixelRatio_ = newDevicePixelRatio;
    ensureRenderChildWindow(widget, currentPhysicalWidth_,
                            currentPhysicalHeight_, "recreateSwapChain");

    qDebug() << "DiligentDeviceManager::recreateSwapChain - Logical:" << widget->width() << "x" << widget->height()
             << ", DPI:" << newDevicePixelRatio
             << ", Physical:" << newWidth << "x" << newHeight;
    qDebug() << "Before Resize - SwapChain Desc:" << swapChain_->GetDesc().Width << "x" << swapChain_->GetDesc().Height;

    swapChain_->Resize(newWidth, newHeight);

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

bool DiligentDeviceManager::Impl::ensureRenderChildWindow(
    QWidget* widget, int width, int height, const char* reason)
{
    if (!widget) {
        return false;
    }

    const HWND parentHwnd = reinterpret_cast<HWND>(widget->winId());
    if (!parentHwnd) {
        qWarning() << "[DiligentDeviceManager] render HWND parent missing"
                   << "reason=" << reason << "widget=" << widget;
        return false;
    }

    const int safeWidth = (std::max)(width, 1);
    const int safeHeight = (std::max)(height, 1);
    if (!renderHwnd_) {
        // WS_EX_TRANSPARENT: mouse hit-testing skips this HWND and falls
        // through to the parent Qt widget HWND, so nativeEvent() on the
        // viewport receives all mouse messages.
        renderHwnd_ = CreateWindowEx(
            WS_EX_TRANSPARENT, ensureRenderWindowClass(), nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0, 0, safeWidth, safeHeight,
            parentHwnd, nullptr, GetModuleHandle(nullptr), nullptr);
        renderParentHwnd_ = parentHwnd;
        qInfo() << "[DiligentDeviceManager] render child HWND created"
                << "reason=" << reason << "parent="
                << reinterpret_cast<quintptr>(parentHwnd)
                << "child=" << reinterpret_cast<quintptr>(renderHwnd_)
                << "size=" << QSize(safeWidth, safeHeight);
    } else if (renderParentHwnd_ != parentHwnd ||
               GetParent(renderHwnd_) != parentHwnd) {
        qWarning() << "[DiligentDeviceManager] render child HWND reparent"
                   << "reason=" << reason
                   << "oldParent=" << reinterpret_cast<quintptr>(renderParentHwnd_)
                   << "actualParent="
                   << reinterpret_cast<quintptr>(GetParent(renderHwnd_))
                   << "newParent=" << reinterpret_cast<quintptr>(parentHwnd)
                   << "child=" << reinterpret_cast<quintptr>(renderHwnd_);
        SetParent(renderHwnd_, parentHwnd);
        renderParentHwnd_ = parentHwnd;
    }

    if (!renderHwnd_) {
        qWarning() << "[DiligentDeviceManager] render child HWND creation failed"
                   << "reason=" << reason
                   << "parent=" << reinterpret_cast<quintptr>(parentHwnd)
                   << "size=" << QSize(safeWidth, safeHeight);
        return false;
    }

    SetWindowPos(renderHwnd_, HWND_TOP, 0, 0, safeWidth, safeHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ShowWindow(renderHwnd_, SW_SHOWNA);
    return true;
}

bool DiligentDeviceManager::Impl::createSwapChainForBackend(HWND hwnd, int width, int height)
{
    if (!device_ || !immediateContext_ || !hwnd) {
        return false;
    }

    SwapChainDesc SCDesc;
    SCDesc.Width = width;
    SCDesc.Height = height;
    // Keep the swap-chain format aligned with the active display mode.  The
    // composition renderer already works in linear float textures; using an
    // sRGB UNORM back buffer for HDR silently clamps scene values above 1.0.
    // HDR modes therefore use a float16 target and leave the display transform
    // to the final post-process/OS display pipeline.
    const bool hdrEnabled = RenderConfig::hdrDisplayEnabled();
    SCDesc.ColorBufferFormat =
        hdrEnabled ? TEX_FORMAT_RGBA16_FLOAT : TEX_FORMAT_RGBA8_UNORM_SRGB;
    SCDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
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
    configureHdrColorSpace(swapChain_, hdrEnabled);
    return swapChain_ != nullptr;
}

void DiligentDeviceManager::Impl::destroy()
{
    if (immediateContext_ && !deviceLost_) {
        immediateContext_->Flush();
        immediateContext_->WaitForIdle();
    }

    if (!deviceLost_) {
        reportLiveD3D12Objects(device_.RawPtr());
    }

    swapChain_.Release();

    if (renderHwnd_) {
        DestroyWindow(renderHwnd_);
        renderHwnd_ = nullptr;
    }
    renderParentHwnd_ = nullptr;

    deferredContext_.Release();
    immediateContext_.Release();
    device_.Release();
    agilityCapabilities_ = {};
    if (usingSharedDevice_) {
        releaseSharedRenderDevice();
        usingSharedDevice_ = false;
    }
    initialized_ = false;
    deviceLost_ = false;
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
    qInfo().noquote() << "[DiligentDeviceManager][Agility]"
                      << d3d12AgilityDebugState();
}

void DiligentDeviceManager::initializeHeadless()
{
    impl_->initializeHeadless();
    qInfo().noquote() << "[DiligentDeviceManager][Agility]"
                      << d3d12AgilityDebugState();
}

void DiligentDeviceManager::createSwapChain(QWidget* widget)
{
    impl_->createSwapChain(widget);
}

void DiligentDeviceManager::recreateSwapChain(QWidget* widget)
{
    impl_->recreateSwapChain(widget);
}

void DiligentDeviceManager::markDeviceLost()
{
    impl_->deviceLost_ = true;
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
    SCDesc.ColorBufferFormat =
        RenderConfig::hdrDisplayEnabled() ? TEX_FORMAT_RGBA16_FLOAT
                                          : TEX_FORMAT_RGBA8_UNORM_SRGB;
    SCDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
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
    configureHdrColorSpace(outSwapChain, RenderConfig::hdrDisplayEnabled());
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

VkDevice DiligentDeviceManager::vkDevice() const
{
    if (!impl_ || !impl_->device_ || impl_->device_->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN) {
        return VK_NULL_HANDLE;
    }
    RefCntAutoPtr<IRenderDeviceVk> deviceVk{impl_->device_, IID_RenderDeviceVk};
    return deviceVk ? deviceVk->GetVkDevice() : VK_NULL_HANDLE;
}

VkPhysicalDevice DiligentDeviceManager::vkPhysicalDevice() const
{
    if (!impl_ || !impl_->device_ || impl_->device_->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN) {
        return VK_NULL_HANDLE;
    }
    RefCntAutoPtr<IRenderDeviceVk> deviceVk{impl_->device_, IID_RenderDeviceVk};
    return deviceVk ? deviceVk->GetVkPhysicalDevice() : VK_NULL_HANDLE;
}

VkInstance DiligentDeviceManager::vkInstance() const
{
    if (!impl_ || !impl_->device_ || impl_->device_->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN) {
        return VK_NULL_HANDLE;
    }
    RefCntAutoPtr<IRenderDeviceVk> deviceVk{impl_->device_, IID_RenderDeviceVk};
    return deviceVk ? deviceVk->GetVkInstance() : VK_NULL_HANDLE;
}

VkQueue DiligentDeviceManager::vkQueue() const
{
    if (!impl_ || !impl_->immediateContext_ || !impl_->device_ || impl_->device_->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN) {
        return VK_NULL_HANDLE;
    }

    RefCntAutoPtr<ICommandQueue> queue{impl_->immediateContext_->LockCommandQueue()};
    if (!queue) {
        return VK_NULL_HANDLE;
    }

    RefCntAutoPtr<ICommandQueueVk> queueVk{queue, IID_CommandQueueVk};
    const VkQueue vkQueue = queueVk ? queueVk->GetVkQueue() : VK_NULL_HANDLE;
    impl_->immediateContext_->UnlockCommandQueue();
    return vkQueue;
}

uint32_t DiligentDeviceManager::vkQueueFamilyIndex() const
{
    if (!impl_ || !impl_->immediateContext_ || !impl_->device_ || impl_->device_->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN) {
        return 0;
    }

    RefCntAutoPtr<ICommandQueue> queue{impl_->immediateContext_->LockCommandQueue()};
    if (!queue) {
        return 0;
    }

    RefCntAutoPtr<ICommandQueueVk> queueVk{queue, IID_CommandQueueVk};
    const uint32_t familyIndex = queueVk ? queueVk->GetQueueFamilyIndex() : 0;
    impl_->immediateContext_->UnlockCommandQueue();
    return familyIndex;
}

bool DiligentDeviceManager::isInitialized() const
{
    return impl_->initialized_;
}

bool DiligentDeviceManager::isRayTracingSupported() const
{
    return impl_->rtSupported_;
}

SelectedGpuAdapterInfo DiligentDeviceManager::selectedAdapterInfo() const
{
    SelectedGpuAdapterInfo info;
    if (!impl_ || !impl_->device_) {
        return info;
    }

    const auto& adapter = impl_->device_->GetAdapterInfo();
    info.available = true;
    info.name = QString::fromLatin1(adapter.Description).trimmed();
    info.vendorId = adapter.VendorId;
    info.deviceId = adapter.DeviceId;
    info.vendor = adapterVendorName(adapter.VendorId);
    info.backend = QString::fromLatin1(
        deviceTypeName(impl_->device_->GetDeviceInfo().Type));
    info.rayTracingSupported = impl_->rtSupported_;
    info.selectionPolicy = QString::fromLatin1(
        gpuAdapterPolicyName(gpuAdapterPolicyFromEnv()));
    info.requestedAdapter =
        qEnvironmentVariable("ARTIFACT_GPU_ADAPTER").trimmed();
    return info;
}

QString DiligentDeviceManager::selectedAdapterDebugState() const
{
    const auto info = selectedAdapterInfo();
    if (!info.available) {
        return QStringLiteral("adapter=<unavailable>");
    }
    return QStringLiteral(
               "adapter=%1 vendor=%2 vendorId=0x%3 deviceId=0x%4 "
               "backend=%5 rayTracing=%6 policy=%7 requested=%8")
        .arg(info.name.isEmpty() ? QStringLiteral("<unnamed>") : info.name)
        .arg(info.vendor)
        .arg(info.vendorId, 8, 16, QLatin1Char('0'))
        .arg(info.deviceId, 8, 16, QLatin1Char('0'))
        .arg(info.backend)
        .arg(info.rayTracingSupported)
        .arg(info.selectionPolicy)
        .arg(info.requestedAdapter.isEmpty()
                 ? QStringLiteral("<auto>")
                 : info.requestedAdapter);
}

D3D12AgilityCapabilitySnapshot
DiligentDeviceManager::d3d12AgilityCapabilities() const
{
    return impl_ ? impl_->agilityCapabilities_
                 : D3D12AgilityCapabilitySnapshot{};
}

QString DiligentDeviceManager::d3d12AgilityDebugState() const
{
    const auto caps = d3d12AgilityCapabilities();
    if (!caps.available) {
        return QStringLiteral("agility=<not-d3d12> headerSdk=%1 requestedSdk=%2")
            .arg(caps.headerSdkVersion)
            .arg(caps.requestedSdkVersion);
    }
    const QString deviceShaderModel = caps.deviceShaderModelKnown
        ? QStringLiteral("%1.%2")
              .arg(caps.deviceShaderModelMajor)
              .arg(caps.deviceShaderModelMinor)
        : QStringLiteral("unknown");
    const QString dxcVersion = caps.dxcAvailable
        ? QStringLiteral("%1.%2")
              .arg(caps.dxcVersionMajor)
              .arg(caps.dxcVersionMinor)
        : QStringLiteral("unavailable");
    const QString dxcShaderModel = caps.dxcAvailable
        ? QStringLiteral("%1.%2")
              .arg(caps.dxcShaderModelMajor)
              .arg(caps.dxcShaderModelMinor)
        : QStringLiteral("unknown");
    return QStringLiteral(
               "agilityRuntime=%1 requestedSdk=%2 headerSdk=%3 "
               "deviceSM=%4 deviceSM69=%5 dxc=%6 dxcSM=%7 dxcSM69=%8 "
               "options22=%9 tightAlignment=%10 tightAlignmentTier=%11 "
               "max1DDispatch=%12 max1DDispatchMesh=%13 device15=%14 periodicTrim=%15 "
               "trimRegistered=%16 cpuTimelineQuery=%17 revisedViews=%18 runtimePath=%19")
        .arg(caps.agilityRuntimeLoaded)
        .arg(caps.requestedSdkVersion)
        .arg(caps.headerSdkVersion)
        .arg(deviceShaderModel)
        .arg(caps.deviceShaderModel69Supported)
        .arg(dxcVersion)
        .arg(dxcShaderModel)
        .arg(caps.dxcShaderModel69Supported)
        .arg(caps.options22Available)
        .arg(caps.tightAlignmentAvailable)
        .arg(caps.tightAlignmentTier)
        .arg(caps.max1DDispatchSize)
        .arg(caps.max1DDispatchMeshSize)
        .arg(caps.device15Available)
        .arg(caps.periodicTrimNotificationAvailable)
        .arg(caps.periodicTrimNotificationRegistered)
        .arg(caps.cpuTimelineQueryResolveAvailable)
        .arg(caps.revisedViewCreationAvailable)
        .arg(caps.runtimePath.isEmpty() ? QStringLiteral("<none>")
                                        : caps.runtimePath);
}

std::vector<GpuAdapterCandidate> DiligentDeviceManager::availableAdapters() const
{
    std::vector<GpuAdapterCandidate> adapters;
    appendFactoryAdapters(resolveD3D12Factory(), QStringLiteral("d3d12"),
                          adapters);
    if (hasUsableVulkanLoader()) {
        appendFactoryAdapters(resolveVkFactory(), QStringLiteral("vulkan"),
                              adapters);
    }

    const auto selected = selectedAdapterInfo();
    for (auto& candidate : adapters) {
        candidate.selected = selected.available &&
            candidate.backend == selected.backend &&
            candidate.vendorId == selected.vendorId &&
            candidate.deviceId == selected.deviceId &&
            candidate.name == selected.name;
    }
    std::stable_sort(adapters.begin(), adapters.end(),
                     [](const GpuAdapterCandidate& lhs,
                        const GpuAdapterCandidate& rhs) {
        return lhs.autoScore > rhs.autoScore;
    });
    return adapters;
}

QString DiligentDeviceManager::availableAdaptersDebugState() const
{
    const auto adapters = availableAdapters();
    if (adapters.empty()) {
        return QStringLiteral("adapters=<none>");
    }
    QStringList descriptions;
    descriptions.reserve(static_cast<qsizetype>(adapters.size()));
    for (const auto& adapter : adapters) {
        descriptions.push_back(
            QStringLiteral(
                "%1:%2 id=%3 type=%4 vendor=%5 localMiB=%6 "
                "unifiedMiB=%7 rt=%8 score=%9 selected=%10")
                .arg(adapter.backend)
                .arg(adapter.name.isEmpty() ? QStringLiteral("<unnamed>")
                                            : adapter.name)
                .arg(adapter.adapterId)
                .arg(adapter.type)
                .arg(adapter.vendor)
                .arg(adapter.localMemoryBytes / (1024ull * 1024ull))
                .arg(adapter.unifiedMemoryBytes / (1024ull * 1024ull))
                .arg(adapter.rayTracingSupported)
                .arg(adapter.autoScore)
                .arg(adapter.selected));
    }
    return descriptions.join(QStringLiteral(" | "));
}

}
