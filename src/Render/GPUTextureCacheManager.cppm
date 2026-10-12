module;
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <variant>
#include <vector>
#include <utility>
#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QStringView>
#include <QDebug>
#include <QMutexLocker>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <vulkan/vulkan.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/CommandQueueD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/RenderDeviceD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/RenderDeviceVk.h>
#include "../../include/Render/DiligentUploadCoordinator.hpp"

module Artifact.Render.GPUTextureCacheManager;

import Image.ImageF32x4_RGBA;
import Image.UploadConversion;
import Video.VideoFrame;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

using namespace Diligent;

namespace {
size_t bytesForImage(const QImage& image)
{
    if (image.isNull()) {
        return 0;
    }
    return static_cast<size_t>(image.bytesPerLine()) * static_cast<size_t>(image.height());
}

void appendSignedDecimal(QString& output, int value)
{
    char digits[std::numeric_limits<unsigned>::digits10 + 1];
    int digitCount = 0;
    const bool negative = value < 0;
    unsigned magnitude = negative
        ? static_cast<unsigned>(-static_cast<std::int64_t>(value))
        : static_cast<unsigned>(value);
    do {
        digits[digitCount++] = static_cast<char>('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0u);

    if (negative) {
        output.append(QLatin1Char('-'));
    }
    while (digitCount > 0) {
        output.append(QLatin1Char(digits[--digitCount]));
    }
}

QString colorAwareImageCacheKey(
    const QString& cacheKey,
    const ArtifactCore::ImageF32x4_RGBA& image)
{
    const auto descriptor = image.colorDescriptor();
    QString result = cacheKey;
    const qsizetype cacheKeySize = cacheKey.size();
    if (cacheKeySize <= std::numeric_limits<qsizetype>::max() - 48) {
        result.reserve(cacheKeySize + 48);
    }
    result.append(QStringLiteral("|color:"));
    appendSignedDecimal(result, static_cast<int>(descriptor.storage));
    result.append(QLatin1Char(','));
    appendSignedDecimal(result, static_cast<int>(descriptor.channelOrder));
    result.append(QLatin1Char(','));
    appendSignedDecimal(result, static_cast<int>(descriptor.primaries));
    result.append(QLatin1Char(','));
    appendSignedDecimal(result, static_cast<int>(descriptor.transfer));
    result.append(QLatin1Char(','));
    appendSignedDecimal(result, static_cast<int>(descriptor.alphaMode));
    result.append(QLatin1Char(','));
    appendSignedDecimal(result, static_cast<int>(descriptor.range));
    result.append(QLatin1Char(','));
    appendSignedDecimal(result, descriptor.transferKnown ? 1 : 0);
    return result;
}

QStringView versionedAssetCacheToken(const QString& cacheKey)
{
    if (!cacheKey.startsWith(QStringLiteral("video-gpu:v")) &&
        !cacheKey.startsWith(QStringLiteral("image-f32:v"))) {
        return {};
    }

    const QStringView key{cacheKey};
    const qsizetype versionStart = key.indexOf(u':') + 1;
    if (versionStart <= 0) {
        return {};
    }
    const qsizetype variantSeparator = key.indexOf(u'|', versionStart);
    const qsizetype frameSeparator = key.indexOf(u':', versionStart);
    qsizetype tokenEnd = key.size();
    if (variantSeparator >= 0) {
        tokenEnd = std::min(tokenEnd, variantSeparator);
    }
    if (frameSeparator >= 0) {
        tokenEnd = std::min(tokenEnd, frameSeparator);
    }
    return key.first(tokenEnd);
}

Diligent::TEXTURE_FORMAT textureFormatFromVulkanNativeFormat(std::uint32_t nativeFormat)
{
    switch (static_cast<VkFormat>(nativeFormat)) {
    case VK_FORMAT_R8G8B8A8_UNORM:
        return Diligent::TEX_FORMAT_RGBA8_UNORM;
    case VK_FORMAT_R8G8B8A8_SRGB:
        return Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM:
        return Diligent::TEX_FORMAT_BGRA8_UNORM;
    case VK_FORMAT_B8G8R8A8_SRGB:
        return Diligent::TEX_FORMAT_BGRA8_UNORM_SRGB;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return Diligent::TEX_FORMAT_RGBA32_FLOAT;
    default:
        return Diligent::TEX_FORMAT_UNKNOWN;
    }
}

Diligent::TEXTURE_FORMAT textureFormatFromGpuFrame(const ArtifactCore::GpuVideoFrame& frame,
                                                   const ArtifactCore::VulkanVideoFrameHandle& handle)
{
    if (handle.nativeFormat != 0u) {
        return textureFormatFromVulkanNativeFormat(handle.nativeFormat);
    }

    switch (frame.meta.pixelFormat) {
    case ArtifactCore::VideoFramePixelFormat::RGBA8:
        return Diligent::TEX_FORMAT_RGBA8_UNORM;
    case ArtifactCore::VideoFramePixelFormat::BGRA8:
        return Diligent::TEX_FORMAT_BGRA8_UNORM;
    case ArtifactCore::VideoFramePixelFormat::RGBA32F:
        return Diligent::TEX_FORMAT_RGBA32_FLOAT;
    default:
        return Diligent::TEX_FORMAT_UNKNOWN;
    }
}

Diligent::RESOURCE_STATE resourceStateFromVulkanLayout(std::uint32_t layout)
{
    if (static_cast<VkImageLayout>(layout) == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        return Diligent::RESOURCE_STATE_SHADER_RESOURCE;
    }
    return Diligent::RESOURCE_STATE_UNKNOWN;
}

} // namespace

class D3D12VideoFrameConversionPool final {
public:
    static constexpr int SlotCount = 3;
    static constexpr size_t MaxSlotBytes = 128ull * 1024ull * 1024ull;

    struct Slot {
        ID3D12DescriptorHeap* descriptors = nullptr;
        ID3D12CommandAllocator* allocator = nullptr;
        ID3D12GraphicsCommandList* commandList = nullptr;
        ID3D12Resource* output = nullptr;
        RefCntAutoPtr<ITexture> texture;
        RefCntAutoPtr<ITextureView> srv;
        ArtifactCore::GpuVideoFrame sourceFrame;
        UINT width = 0;
        UINT height = 0;
        D3D12_RESOURCE_STATES outputState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        UINT64 conversionValue = 0;
        UINT64 renderRetireValue = 0;
        quint64 cacheEntryId = 0;
        bool usedThisFrame = false;

        ~Slot()
        {
            srv.Release();
            texture.Release();
            if (output) output->Release();
            if (commandList) commandList->Release();
            if (allocator) allocator->Release();
            if (descriptors) descriptors->Release();
        }
    };

    D3D12VideoFrameConversionPool(
        RefCntAutoPtr<IRenderDevice> device,
        RefCntAutoPtr<IDeviceContext> context)
        : context_(std::move(context))
    {
        deviceD3D12_ = RefCntAutoPtr<IRenderDeviceD3D12>{
            device, IID_RenderDeviceD3D12};
        if (!deviceD3D12_ || !context_) return;
        nativeDevice_ = deviceD3D12_->GetD3D12Device();
        if (!nativeDevice_) return;
        nativeDevice_->AddRef();
        if (FAILED(nativeDevice_->CreateFence(
                0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)))) {
            return;
        }
        if (!createPipeline()) return;
        initialized_ = true;
    }

    ~D3D12VideoFrameConversionPool()
    {
        // This path is reached only during cache/device teardown. Drain the
        // immediate context before releasing slot textures or retained decoder
        // frames; per-frame slot selection remains nonblocking.
        if (context_ && fenceValue_ != 0) context_->WaitForIdle();
        for (Slot& slot : slots_) {
            slot.sourceFrame = {};
        }
        if (rootSignature_) rootSignature_->Release();
        if (pipeline_) pipeline_->Release();
        if (shader_) shader_->Release();
        if (fence_) fence_->Release();
        if (nativeDevice_) nativeDevice_->Release();
    }

    bool isInitialized() const { return initialized_; }
    void* nativeDevice() const { return nativeDevice_; }

    size_t memoryBytes() const
    {
        size_t total = 0;
        for (const Slot& slot : slots_) {
            total += static_cast<size_t>(slot.width) * slot.height * 8u;
        }
        return total;
    }

    void detachCacheEntries()
    {
        for (Slot& slot : slots_) slot.cacheEntryId = 0;
    }

    quint64 cacheEntryId(int slotIndex) const
    {
        return slotIndex >= 0 && slotIndex < SlotCount
            ? slots_[static_cast<size_t>(slotIndex)].cacheEntryId : 0;
    }

    void setCacheEntryId(int slotIndex, quint64 id)
    {
        if (slotIndex >= 0 && slotIndex < SlotCount)
            slots_[static_cast<size_t>(slotIndex)].cacheEntryId = id;
    }

    void clearCacheEntryId(int slotIndex, quint64 id)
    {
        if (slotIndex < 0 || slotIndex >= SlotCount) return;
        Slot& slot = slots_[static_cast<size_t>(slotIndex)];
        if (slot.cacheEntryId == id) slot.cacheEntryId = 0;
    }

    int findReusableSlot()
    {
        if (!initialized_) return -1;
        retireCompletedSources();
        const UINT64 completed = fence_->GetCompletedValue();
        for (int i = 0; i < SlotCount; ++i) {
            const Slot& slot = slots_[static_cast<size_t>(i)];
            if (slot.conversionValue <= completed &&
                slot.renderRetireValue <= completed && !slot.usedThisFrame) {
                return i;
            }
        }
        return -1;
    }

    bool submit(int slotIndex,
                const ArtifactCore::GpuVideoFrame& frame,
                RefCntAutoPtr<ITexture>& outTexture,
                RefCntAutoPtr<ITextureView>& outSrv)
    {
        if (!initialized_ || slotIndex < 0 || slotIndex >= SlotCount ||
            !frame.isValid() ||
            frame.storage != ArtifactCore::VideoFrameStorageKind::D3D12Texture ||
            frame.meta.pixelFormat != ArtifactCore::VideoFramePixelFormat::NV12 ||
            frame.meta.width <= 0 || frame.meta.height <= 0 ||
            (frame.meta.width & 1) || (frame.meta.height & 1)) {
            return false;
        }
        const auto* handle = std::get_if<ArtifactCore::D3D12VideoFrameHandle>(
            &frame.handle);
        auto* source = handle
            ? static_cast<ID3D12Resource*>(handle->texture) : nullptr;
        auto* sourceFence = handle
            ? static_cast<ID3D12Fence*>(handle->fence) : nullptr;
        if (!source || !sourceFence) return false;
        const D3D12_RESOURCE_DESC sourceDesc = source->GetDesc();
        const UINT sourceSlice = handle->subresourceIndex;
        if (sourceDesc.Format != DXGI_FORMAT_NV12 ||
            sourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            sourceSlice >= sourceDesc.DepthOrArraySize) {
            return false;
        }
        const UINT width = static_cast<UINT>(frame.meta.width);
        const UINT height = static_cast<UINT>(frame.meta.height);
        const size_t slotBytes = static_cast<size_t>(width) * height * 8u;
        if (slotBytes == 0 || slotBytes > MaxSlotBytes) return false;
        const int colorSpace = frame.meta.color.colorSpace;
        const UINT matrix = (colorSpace == 5 || colorSpace == 6 || colorSpace == 4)
            ? 1u : colorSpace == 9 ? 2u : 0u;
        const int transfer = frame.meta.color.colorTransfer;
        if ((transfer != 0 && transfer != 1 && transfer != 2 && transfer != 13) ||
            (frame.meta.color.colorPrimaries != 0 &&
             frame.meta.color.colorPrimaries != 1 &&
             frame.meta.color.colorPrimaries != 2)) {
            return false;
        }
        const UINT fullRange = frame.meta.color.colorRange == 2 ? 1u : 0u;
        const UINT transferKind = transfer == 13 ? 1u : 0u;

        Slot& slot = slots_[static_cast<size_t>(slotIndex)];
        if (slot.conversionValue > fence_->GetCompletedValue() ||
            slot.renderRetireValue > fence_->GetCompletedValue() ||
            slot.usedThisFrame) {
            return false;
        }
        if (!prepareSlot(slot, width, height)) {
            return false;
        }
        if (!prepareCommandObjects(slot)) {
            return false;
        }

        const UINT descriptorIncrement = nativeDevice_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_CPU_DESCRIPTOR_HANDLE cpu =
            slot.descriptors->GetCPUDescriptorHandleForHeapStart();
        for (UINT plane = 0; plane < 2; ++plane) {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Shader4ComponentMapping =
                D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = plane == 0 ? DXGI_FORMAT_R8_UNORM
                                        : DXGI_FORMAT_R8G8_UNORM;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.MostDetailedMip = 0;
            srvDesc.Texture2DArray.MipLevels = 1;
            srvDesc.Texture2DArray.FirstArraySlice = sourceSlice;
            srvDesc.Texture2DArray.ArraySize = 1;
            srvDesc.Texture2DArray.PlaneSlice = plane;
            nativeDevice_->CreateShaderResourceView(source, &srvDesc, cpu);
            cpu.ptr += descriptorIncrement;
        }
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        nativeDevice_->CreateUnorderedAccessView(slot.output, nullptr,
                                                 &uavDesc, cpu);

        if (FAILED(slot.allocator->Reset()) ||
            FAILED(slot.commandList->Reset(slot.allocator, pipeline_))) {
            return false;
        }
        D3D12_RESOURCE_BARRIER inputBarrier{};
        inputBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        inputBarrier.Transition.pResource = source;
        inputBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        inputBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        inputBarrier.Transition.StateAfter =
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        slot.commandList->ResourceBarrier(1, &inputBarrier);

        if (slot.outputState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
            D3D12_RESOURCE_BARRIER outputBarrier{};
            outputBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            outputBarrier.Transition.pResource = slot.output;
            outputBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            outputBarrier.Transition.StateBefore = slot.outputState;
            outputBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            slot.commandList->ResourceBarrier(1, &outputBarrier);
            slot.outputState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
        ID3D12DescriptorHeap* heaps[] = {slot.descriptors};
        slot.commandList->SetDescriptorHeaps(1, heaps);
        slot.commandList->SetComputeRootSignature(rootSignature_);
        slot.commandList->SetPipelineState(pipeline_);
        D3D12_GPU_DESCRIPTOR_HANDLE gpu =
            slot.descriptors->GetGPUDescriptorHandleForHeapStart();
        slot.commandList->SetComputeRootDescriptorTable(0, gpu);
        gpu.ptr += 2u * descriptorIncrement;
        slot.commandList->SetComputeRootDescriptorTable(1, gpu);
        const UINT constants[] = {width, height, sourceSlice,
                                  fullRange, matrix, transferKind};
        slot.commandList->SetComputeRoot32BitConstants(2, 6, constants, 0);
        slot.commandList->Dispatch((width + 15u) / 16u,
                                   (height + 15u) / 16u, 1);
        D3D12_RESOURCE_BARRIER outputBarrier{};
        outputBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        outputBarrier.Transition.pResource = slot.output;
        outputBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        outputBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        outputBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        slot.commandList->ResourceBarrier(1, &outputBarrier);
        slot.outputState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        inputBarrier.Transition.StateBefore =
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        inputBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        slot.commandList->ResourceBarrier(1, &inputBarrier);
        if (FAILED(slot.commandList->Close())) {
            return false;
        }

        RefCntAutoPtr<ICommandQueue> queueInterface{context_->LockCommandQueue()};
        if (!queueInterface) return false;
        struct QueueUnlock final {
            IDeviceContext* context;
            ~QueueUnlock() { if (context) context->UnlockCommandQueue(); }
        } queueUnlock{context_};
        RefCntAutoPtr<ICommandQueueD3D12> queue{
            queueInterface, IID_CommandQueueD3D12};
        if (!queue) return false;
        queue->WaitFence(sourceFence, handle->fenceValue);
        ID3D12CommandList* commandLists[] = {slot.commandList};
        queue->Submit(1, commandLists);
        const UINT64 signalValue = ++fenceValue_;
        queue->EnqueueSignal(fence_, signalValue);
        slot.conversionValue = signalValue;
        slot.renderRetireValue = 0;
        slot.sourceFrame = frame;
        outTexture = slot.texture;
        outSrv = slot.srv;
        return outTexture && outSrv;
    }

    void markUsed(int slotIndex)
    {
        if (slotIndex >= 0 && slotIndex < SlotCount)
            slots_[static_cast<size_t>(slotIndex)].usedThisFrame = true;
    }

    void endFrame()
    {
        bool anyUsed = false;
        for (const Slot& slot : slots_) anyUsed = anyUsed || slot.usedThisFrame;
        if (!anyUsed || !initialized_) return;
        RefCntAutoPtr<ICommandQueue> queueInterface{context_->LockCommandQueue()};
        if (!queueInterface) return;
        struct QueueUnlock final {
            IDeviceContext* context;
            ~QueueUnlock() { if (context) context->UnlockCommandQueue(); }
        } queueUnlock{context_};
        RefCntAutoPtr<ICommandQueueD3D12> queue{
            queueInterface, IID_CommandQueueD3D12};
        if (!queue) return;
        const UINT64 retireValue = ++fenceValue_;
        queue->EnqueueSignal(fence_, retireValue);
        for (Slot& slot : slots_) {
            if (!slot.usedThisFrame) continue;
            slot.renderRetireValue = retireValue;
            slot.usedThisFrame = false;
        }
    }

private:
    bool createPipeline()
    {
        using CompileFunction = decltype(&D3DCompile);
        HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
        auto compile = compiler ? reinterpret_cast<CompileFunction>(
            GetProcAddress(compiler, "D3DCompile")) : nullptr;
        if (!compile) {
            if (compiler) FreeLibrary(compiler);
            return false;
        }
        static constexpr char source[] = R"(
Texture2DArray<float> Luma : register(t0);
Texture2DArray<float2> Chroma : register(t1);
RWTexture2D<float4> Output : register(u0);
cbuffer Params : register(b0) {
    uint Width; uint Height; uint Slice; uint FullRange; uint MatrixKind; uint TransferKind;
}
float3 ToLinear(float3 c) {
    c = saturate(c);
    if (TransferKind == 1) {
        return float3(c.r <= 0.04045 ? c.r / 12.92 : pow((c.r + 0.055) / 1.055, 2.4),
                      c.g <= 0.04045 ? c.g / 12.92 : pow((c.g + 0.055) / 1.055, 2.4),
                      c.b <= 0.04045 ? c.b / 12.92 : pow((c.b + 0.055) / 1.055, 2.4));
    }
    return float3(c.r < 0.081 ? c.r / 4.5 : pow((c.r + 0.099) / 1.099, 1.0 / 0.45),
                  c.g < 0.081 ? c.g / 4.5 : pow((c.g + 0.099) / 1.099, 1.0 / 0.45),
                  c.b < 0.081 ? c.b / 4.5 : pow((c.b + 0.099) / 1.099, 1.0 / 0.45));
}
[numthreads(16,16,1)]
void main(uint3 p : SV_DispatchThreadID) {
    if (p.x >= Width || p.y >= Height) return;
    float y = Luma.Load(int4(p.xy, 0, 0));
    float2 uv = Chroma.Load(int4(p.xy / 2, 0, 0));
    if (FullRange == 0) { y = (y - 16.0 / 255.0) * (255.0 / 219.0); uv = (uv - 128.0 / 255.0) * (255.0 / 224.0); }
    else { uv -= float2(0.5, 0.5); }
    float3 rgb;
    if (MatrixKind == 1) rgb = float3(y + 1.402 * uv.y, y - 0.344136 * uv.x - 0.714136 * uv.y, y + 1.772 * uv.x);
    else if (MatrixKind == 2) rgb = float3(y + 1.4746 * uv.y, y - 0.16455 * uv.x - 0.57135 * uv.y, y + 1.8814 * uv.x);
    else rgb = float3(y + 1.5748 * uv.y, y - 0.187324 * uv.x - 0.468124 * uv.y, y + 1.8556 * uv.x);
    Output[p.xy] = float4(ToLinear(rgb), 1.0);
})";
        ID3DBlob* errors = nullptr;
        const HRESULT compileResult = compile(
            source, sizeof(source) - 1, "Artifact.Video.NV12ToLinearRGBA16F",
            nullptr, nullptr, "main", "cs_5_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &shader_, &errors);
        if (errors) errors->Release();
        FreeLibrary(compiler);
        if (FAILED(compileResult) || !shader_) return false;

        D3D12_DESCRIPTOR_RANGE ranges[2]{};
        ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[0].NumDescriptors = 2;
        ranges[0].BaseShaderRegister = 0;
        ranges[0].OffsetInDescriptorsFromTableStart = 0;
        ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        ranges[1].NumDescriptors = 1;
        ranges[1].BaseShaderRegister = 0;
        ranges[1].OffsetInDescriptorsFromTableStart = 0;
        D3D12_ROOT_PARAMETER params[3]{};
        for (UINT i = 0; i < 2; ++i) {
            params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            params[i].DescriptorTable.NumDescriptorRanges = 1;
            params[i].DescriptorTable.pDescriptorRanges = &ranges[i];
            params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }
        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[2].Constants.ShaderRegister = 0;
        params[2].Constants.RegisterSpace = 0;
        params[2].Constants.Num32BitValues = 6;
        params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = 3;
        rootDesc.pParameters = params;
        ID3DBlob* signature = nullptr;
        ID3DBlob* signatureErrors = nullptr;
        const HRESULT serializeResult = D3D12SerializeRootSignature(
            &rootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
            &signature, &signatureErrors);
        if (signatureErrors) signatureErrors->Release();
        if (FAILED(serializeResult) || !signature) return false;
        const HRESULT rootResult = nativeDevice_->CreateRootSignature(
            0, signature->GetBufferPointer(), signature->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature_));
        signature->Release();
        if (FAILED(rootResult) || !rootSignature_) return false;
        D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
        pipelineDesc.pRootSignature = rootSignature_;
        pipelineDesc.CS.pShaderBytecode = shader_->GetBufferPointer();
        pipelineDesc.CS.BytecodeLength = shader_->GetBufferSize();
        return SUCCEEDED(nativeDevice_->CreateComputePipelineState(
            &pipelineDesc, IID_PPV_ARGS(&pipeline_))) && pipeline_;
    }

    bool prepareSlot(Slot& slot, UINT width, UINT height)
    {
        if (slot.output && slot.width == width && slot.height == height &&
            slot.texture && slot.srv) return true;
        if (slot.conversionValue > fence_->GetCompletedValue() ||
            slot.renderRetireValue > fence_->GetCompletedValue()) return false;
        slot.srv.Release();
        slot.texture.Release();
        if (slot.output) { slot.output->Release(); slot.output = nullptr; }
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        if (FAILED(nativeDevice_->CreateCommittedResource(
                &heap, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                IID_PPV_ARGS(&slot.output)))) return false;
        deviceD3D12_->CreateTextureFromD3DResource(
            slot.output, RESOURCE_STATE_SHADER_RESOURCE, &slot.texture);
        if (!slot.texture) return false;
        slot.srv = slot.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        if (!slot.srv) return false;
        slot.width = width;
        slot.height = height;
        slot.outputState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        return true;
    }

    bool prepareCommandObjects(Slot& slot)
    {
        if (!slot.descriptors) {
            D3D12_DESCRIPTOR_HEAP_DESC desc{};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.NumDescriptors = 3;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            if (FAILED(nativeDevice_->CreateDescriptorHeap(
                    &desc, IID_PPV_ARGS(&slot.descriptors)))) return false;
        }
        if (!slot.allocator && FAILED(nativeDevice_->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&slot.allocator)))) {
            return false;
        }
        if (!slot.commandList) {
            if (FAILED(nativeDevice_->CreateCommandList(
                    0, D3D12_COMMAND_LIST_TYPE_DIRECT, slot.allocator, pipeline_,
                    IID_PPV_ARGS(&slot.commandList)))) return false;
            // CreateCommandList returns an open list. Close it once so the
            // first conversion can Reset the list just like subsequent uses.
            if (FAILED(slot.commandList->Close())) return false;
        }
        return true;
    }

    void retireCompletedSources()
    {
        const UINT64 completed = fence_->GetCompletedValue();
        for (Slot& slot : slots_) {
            if (slot.conversionValue != 0 &&
                slot.conversionValue <= completed) {
                slot.sourceFrame = {};
            }
        }
    }

    RefCntAutoPtr<IDeviceContext> context_;
    RefCntAutoPtr<IRenderDeviceD3D12> deviceD3D12_;
    ID3D12Device* nativeDevice_ = nullptr;
    ID3D12Fence* fence_ = nullptr;
    ID3DBlob* shader_ = nullptr;
    ID3D12RootSignature* rootSignature_ = nullptr;
    ID3D12PipelineState* pipeline_ = nullptr;
    UINT64 fenceValue_ = 0;
    std::array<Slot, SlotCount> slots_{};
    bool initialized_ = false;
};


GPUTextureCacheManager::GPUTextureCacheManager()
    : uploadCoordinator_(new DiligentUploadCoordinator())
{
}
GPUTextureCacheManager::~GPUTextureCacheManager()
{
    clearDevice();
    delete uploadCoordinator_;
    uploadCoordinator_ = nullptr;
}

void GPUTextureCacheManager::setDevice(RefCntAutoPtr<IRenderDevice> device,
                                       RefCntAutoPtr<IDeviceContext> context,
                                       TEXTURE_FORMAT format)
{
    QMutexLocker locker(&mutex_);
    recordInvalidationLocked(GPUTextureCacheInvalidationReason::DeviceReset);
    clearLocked();
    device_ = std::move(device);
    context_ = context;
    textureFormat_ = format;
    if (uploadCoordinator_) {
        uploadCoordinator_->setDevice(device_, context);
        uploadCoordinator_->setPendingByteBudget(256ull * 1024ull * 1024ull);
        uploadCoordinator_->setMaxPendingJobs(128);
    }
    delete d3d12VideoFramePool_;
    d3d12VideoFramePool_ = nullptr;
    if (device_ && context_) {
        auto* pool = new D3D12VideoFrameConversionPool(device_, context_);
        if (pool->isInitialized()) {
            d3d12VideoFramePool_ = pool;
        }
        else {
            delete pool;
        }
    }
}

void GPUTextureCacheManager::clearDevice()
{
    QMutexLocker locker(&mutex_);
    recordInvalidationLocked(GPUTextureCacheInvalidationReason::DeviceReset);
    clearLocked();
    if (uploadCoordinator_) {
        uploadCoordinator_->clearDevice();
    }
    delete d3d12VideoFramePool_;
    d3d12VideoFramePool_ = nullptr;
    device_.Release();
    context_.Release();
}

void GPUTextureCacheManager::setBudgetBytes(size_t bytes)
{
    QMutexLocker locker(&mutex_);
    budgetBytes_ = (bytes > 0) ? bytes : 1u;
    pruneLocked();
}

size_t GPUTextureCacheManager::budgetBytes() const
{
    QMutexLocker locker(&mutex_);
    return budgetBytes_;
}

void GPUTextureCacheManager::setMaxEntries(int count)
{
    QMutexLocker locker(&mutex_);
    maxEntries_ = (count > 1) ? count : 1;
    pruneLocked();
}

int GPUTextureCacheManager::maxEntries() const
{
    QMutexLocker locker(&mutex_);
    return maxEntries_;
}

void GPUTextureCacheManager::beginFrame(quint64 frameIndex)
{
    QMutexLocker locker(&mutex_);
    if (frameIndex < currentFrameIndex_) {
        // A renderer/device restart may reset its local frame counter. Treat
        // existing entries as used on the new epoch instead of expiring every
        // resource at once from an invalid age comparison.
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            it->lastUsedFrame = frameIndex;
        }
    }
    if (frameIndex != currentFrameIndex_) {
        pendingUploadWorkProcessedThisFrame_ = false;
        expirationWorkProcessedThisFrame_ = false;
        expiredEvictionsThisFrame_ = 0;
    }
    currentFrameIndex_ = frameIndex;
    processPendingUploadsLocked();
    if (!expirationWorkProcessedThisFrame_) {
        expirationWorkProcessedThisFrame_ = true;
        pruneExpiredLocked();
    }
}

void GPUTextureCacheManager::endFrame()
{
    QMutexLocker locker(&mutex_);
    if (d3d12VideoFramePool_) d3d12VideoFramePool_->endFrame();
}

void* GPUTextureCacheManager::nativeD3D12Device() const
{
    QMutexLocker locker(&mutex_);
    return d3d12VideoFramePool_ ? d3d12VideoFramePool_->nativeDevice() : nullptr;
}

void GPUTextureCacheManager::setResourceExpirationFrames(quint64 frameCount)
{
    QMutexLocker locker(&mutex_);
    resourceExpirationFrames_ = frameCount;
}

quint64 GPUTextureCacheManager::resourceExpirationFrames() const
{
    QMutexLocker locker(&mutex_);
    return resourceExpirationFrames_;
}

void GPUTextureCacheManager::setMaxExpiredEvictionsPerFrame(int count)
{
    QMutexLocker locker(&mutex_);
    maxExpiredEvictionsPerFrame_ = std::clamp(
        count, 1, MaxExpiredEvictionsPerFrameLimit);
}

int GPUTextureCacheManager::maxExpiredEvictionsPerFrame() const
{
    QMutexLocker locker(&mutex_);
    return maxExpiredEvictionsPerFrame_;
}

QString GPUTextureCacheManager::makeKey(const QString& ownerId, const QString& cacheKey) const
{
    return ownerId + QStringLiteral("|") + cacheKey;
}

GPUTextureCacheHandle GPUTextureCacheManager::acquireOrCreate(const QString& ownerId,
                                                              const QString& cacheKey,
                                                              const QImage& image)
{
    TEXTURE_FORMAT configuredFormat = TEX_FORMAT_UNKNOWN;
    {
        QMutexLocker locker(&mutex_);
        configuredFormat = textureFormat_;
        GPUTextureCacheHandle existingHandle;
        const ExistingSourceState existingState =
            tryAcquireExistingLocked(ownerId, cacheKey, configuredFormat, existingHandle);
        if (existingState == ExistingSourceState::Hit) {
            return existingHandle;
        }
        if (existingState == ExistingSourceState::Pending) {
            return {};
        }
    }
    const QImage rgba = (image.format() == QImage::Format_RGBA8888)
                            ? image
                            : image.convertToFormat(QImage::Format_RGBA8888);
    return acquireOrCreateFromRgbaBytes(ownerId,
                                        cacheKey,
                                        static_cast<Uint32>(rgba.width()),
                                        static_cast<Uint32>(rgba.height()),
                                         static_cast<Uint64>(rgba.bytesPerLine()),
                                         rgba.constBits(),
                                         bytesForImage(rgba),
                                         configuredFormat);
}

GPUTextureCacheHandle GPUTextureCacheManager::acquireOrCreate(const QString& ownerId,
                                                              const QString& cacheKey,
                                                              const ArtifactCore::ImageF32x4_RGBA& image)
{
    const QString colorAwareCacheKey = colorAwareImageCacheKey(cacheKey, image);
    {
        QMutexLocker locker(&mutex_);
        GPUTextureCacheHandle existingHandle;
        const ExistingSourceState existingState =
            tryAcquireExistingLocked(ownerId, colorAwareCacheKey,
                                     TEX_FORMAT_RGBA32_FLOAT, existingHandle);
        if (existingState == ExistingSourceState::Hit) {
            return existingHandle;
        }
        if (existingState == ExistingSourceState::Pending) {
            return {};
        }
    }
    const ArtifactCore::ImageUploadBuffer upload =
        ArtifactCore::convertImageForUpload(
            image, ArtifactCore::ImageUploadTarget::Rgba32LinearStraight);
    const TEXTURE_FORMAT uploadFormat =
        upload.isValid() ? TEX_FORMAT_RGBA32_FLOAT : TEX_FORMAT_UNKNOWN;
    return acquireOrCreateFromRgbaBytes(ownerId,
                                        colorAwareCacheKey,
                                        upload.width,
                                        upload.height,
                                         upload.rowStride,
                                         upload.bytes.data(),
                                         upload.bytes.size(),
                                         uploadFormat);
}

GPUTextureCacheHandle GPUTextureCacheManager::acquireOrCreate(const QString& ownerId,
                                                             const QString& cacheKey,
                                                             const ArtifactCore::GpuVideoFrame& frame)
{
    if (frame.storage == ArtifactCore::VideoFrameStorageKind::D3D12Texture) {
        QMutexLocker locker(&mutex_);
        if (ownerId.isEmpty() || cacheKey.isEmpty() || !frame.isValid() ||
            !device_ || !d3d12VideoFramePool_) {
            ++missCount_;
            return {};
        }
        processPendingUploadsLocked();
        const QString key = makeKey(ownerId, cacheKey) +
            QStringLiteral("|format:%1").arg(static_cast<int>(TEX_FORMAT_RGBA16_FLOAT));
        const auto existingIdIt = keyToId_.find(key);
        if (existingIdIt != keyToId_.end()) {
            auto entryIt = entries_.find(existingIdIt.value());
            if (entryIt != entries_.end() && entryIt->generation == generation_ &&
                entryIt->texture && entryIt->srv) {
                entryIt->lastUsedTick = usageTick_++;
                entryIt->lastUsedFrame = currentFrameIndex_;
                ++hitCount_;
                return {entryIt->id, entryIt->generation};
            }
            if (entryIt != entries_.end()) eraseEntryByIdLocked(entryIt->id);
            else keyToId_.erase(existingIdIt);
        }
        const int slotIndex = d3d12VideoFramePool_->findReusableSlot();
        if (slotIndex < 0) {
            ++missCount_;
            return {};
        }
        const quint64 previousSlotEntryId =
            d3d12VideoFramePool_->cacheEntryId(slotIndex);
        if (previousSlotEntryId != 0)
            eraseEntryByIdLocked(previousSlotEntryId);
        RefCntAutoPtr<ITexture> texture;
        RefCntAutoPtr<ITextureView> srv;
        if (!d3d12VideoFramePool_->submit(slotIndex, frame, texture, srv)) {
            ++missCount_;
            return {};
        }
        Entry entry;
        entry.id = nextId_++;
        entry.generation = generation_;
        entry.format = TEX_FORMAT_RGBA16_FLOAT;
        entry.ownerId = ownerId;
        entry.cacheKey = cacheKey;
        entry.fullKey = key;
        entry.texture = std::move(texture);
        entry.srv = std::move(srv);
        entry.d3d12SlotIndex = slotIndex;
        entry.lastUsedTick = usageTick_++;
        entry.lastUsedFrame = currentFrameIndex_;
        entries_.insert(entry.id, entry);
        keyToId_.insert(key, entry.id);
        ownerToIds_[ownerId].insert(entry.id);
        ownerCacheKeyToIds_[ownerId][cacheKey].insert(entry.id);
        d3d12VideoFramePool_->setCacheEntryId(slotIndex, entry.id);
        ++missCount_;
        pruneLocked();
        return {entry.id, entry.generation};
    }
    if (ownerId.isEmpty() || cacheKey.isEmpty() || !frame.isValid() ||
        frame.storage != ArtifactCore::VideoFrameStorageKind::VulkanImage) {
        QMutexLocker locker(&mutex_);
        ++missCount_;
        return {};
    }

    const auto* handle = std::get_if<ArtifactCore::VulkanVideoFrameHandle>(&frame.handle);
    if (!handle || !handle->image || handle->planeCount != 1u) {
        QMutexLocker locker(&mutex_);
        ++missCount_;
        return {};
    }

    const auto format = textureFormatFromGpuFrame(frame, *handle);
    if (format == TEX_FORMAT_UNKNOWN) {
        QMutexLocker locker(&mutex_);
        ++missCount_;
        return {};
    }

    QMutexLocker locker(&mutex_);
    if (!device_) {
        ++missCount_;
        return {};
    }

    processPendingUploadsLocked();

    RefCntAutoPtr<IRenderDeviceVk> deviceVk{device_, IID_RenderDeviceVk};
    if (!deviceVk) {
        ++missCount_;
        return {};
    }

    purgeStaleVersionEntriesLocked(ownerId, cacheKey);

    const QString key = makeKey(ownerId, cacheKey) +
                        QStringLiteral("|format:%1").arg(static_cast<int>(format));
    const auto existingIdIt = keyToId_.find(key);
    if (existingIdIt != keyToId_.end()) {
        auto entryIt = entries_.find(existingIdIt.value());
        if (entryIt != entries_.end() && entryIt->generation == generation_ && entryIt->texture) {
            entryIt->lastUsedTick = usageTick_++;
            entryIt->lastUsedFrame = currentFrameIndex_;
            ++hitCount_;
            return {entryIt->id, entryIt->generation};
        }
        if (entryIt != entries_.end()) {
            eraseEntryByIdLocked(entryIt->id);
        } else {
            keyToId_.erase(existingIdIt);
        }
    }

    TextureDesc texDesc;
    texDesc.Name = "GPUTextureCacheManager.VulkanVideoFrame";
    texDesc.Type = RESOURCE_DIM_TEX_2D;
    texDesc.Width = static_cast<Uint32>(frame.meta.width);
    texDesc.Height = static_cast<Uint32>(frame.meta.height);
    texDesc.MipLevels = 1;
    texDesc.Format = format;
    texDesc.Usage = USAGE_DEFAULT;
    texDesc.BindFlags = BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = CPU_ACCESS_NONE;

    RefCntAutoPtr<ITexture> texture;
    deviceVk->CreateTextureFromVulkanImage(reinterpret_cast<VkImage>(handle->image),
                                           texDesc,
                                           resourceStateFromVulkanLayout(handle->imageLayout),
                                           &texture);
    if (!texture) {
        qWarning() << "[GPUTextureCache] CreateTextureFromVulkanImage failed"
                   << "owner=" << ownerId
                   << "cacheKey=" << cacheKey
                   << "size=" << frame.meta.width << "x" << frame.meta.height
                   << "format=" << static_cast<int>(frame.meta.pixelFormat);
        ++missCount_;
        return {};
    }

    Entry entry;
    entry.id = nextId_++;
    entry.generation = generation_;
    entry.format = format;
    entry.ownerId = ownerId;
    entry.cacheKey = cacheKey;
    entry.fullKey = key;
    entry.texture = texture;
    entry.srv = texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    entry.sourceGpuFrame = frame;
    entry.memoryBytes = 0;
    entry.lastUsedTick = usageTick_++;
    entry.lastUsedFrame = currentFrameIndex_;

    entries_.insert(entry.id, entry);
    keyToId_.insert(key, entry.id);
    ownerToIds_[ownerId].insert(entry.id);
    ownerCacheKeyToIds_[ownerId][cacheKey].insert(entry.id);
    ++missCount_;

    pruneLocked();
    return {entry.id, entry.generation};
}

GPUTextureCacheHandle GPUTextureCacheManager::findExisting(
    const QString& ownerId, const QString& cacheKey,
    Diligent::TEXTURE_FORMAT format) const
{
    if (ownerId.isEmpty() || cacheKey.isEmpty()) {
        return {};
    }

    QMutexLocker locker(&mutex_);
    const auto ownerIt = ownerCacheKeyToIds_.constFind(ownerId);
    if (ownerIt == ownerCacheKeyToIds_.cend()) {
        return {};
    }
    const auto cacheKeyIt = ownerIt.value().constFind(cacheKey);
    if (cacheKeyIt == ownerIt.value().cend()) {
        return {};
    }
    for (const quint64 id : cacheKeyIt.value()) {
        auto it = entries_.find(id);
        if (it != entries_.end() && it->cacheKey == cacheKey &&
            (format == Diligent::TEX_FORMAT_UNKNOWN ||
             it->format == format) &&
            it->generation == generation_ && it->texture) {
            it->lastUsedTick = usageTick_++;
            it->lastUsedFrame = currentFrameIndex_;
            ++hitCount_;
            return {it->id, it->generation};
        }
    }
    return {};
}

GPUTextureCacheHandle GPUTextureCacheManager::findExisting(
    const QString& ownerId, const QString& cacheKey,
    const ArtifactCore::ImageF32x4_RGBA& image) const
{
    const QString colorAwareCacheKey = colorAwareImageCacheKey(cacheKey, image);
    return findExisting(ownerId, colorAwareCacheKey,
                        Diligent::TEX_FORMAT_RGBA32_FLOAT);
}

GPUTextureCacheManager::ExistingSourceState GPUTextureCacheManager::tryAcquireExistingLocked(
    const QString& ownerId,
    const QString& cacheKey,
    Diligent::TEXTURE_FORMAT format,
    GPUTextureCacheHandle& outHandle)
{
    processPendingUploadsLocked();
    purgeStaleVersionEntriesLocked(ownerId, cacheKey);
    if (format == Diligent::TEX_FORMAT_UNKNOWN) {
        return ExistingSourceState::NeedUpload;
    }

    // The owner/cache-key index already identifies the small set of format
    // variants for this source. Resolve ordinary hits here before building the
    // composite QString key used by pending-upload and insertion bookkeeping.
    const auto ownerIt = ownerCacheKeyToIds_.constFind(ownerId);
    if (ownerIt != ownerCacheKeyToIds_.cend()) {
        const auto cacheKeyIt = ownerIt.value().constFind(cacheKey);
        if (cacheKeyIt != ownerIt.value().cend()) {
            for (const quint64 id : cacheKeyIt.value()) {
                auto entryIt = entries_.find(id);
                if (entryIt == entries_.end() ||
                    entryIt->format != format ||
                    entryIt->generation != generation_ ||
                    !entryIt->texture) {
                    continue;
                }
                entryIt->lastUsedTick = usageTick_++;
                entryIt->lastUsedFrame = currentFrameIndex_;
                ++hitCount_;
                outHandle = GPUTextureCacheHandle{entryIt->id,
                                                   entryIt->generation};
                return ExistingSourceState::Hit;
            }
        }
    }

    const QString key = makeKey(ownerId, cacheKey) +
                        QStringLiteral("|format:%1").arg(static_cast<int>(format));
    const auto existingIdIt = keyToId_.find(key);
    if (existingIdIt != keyToId_.end()) {
        auto entryIt = entries_.find(existingIdIt.value());
        if (entryIt != entries_.end() && entryIt->generation == generation_ && entryIt->texture) {
            entryIt->lastUsedTick = usageTick_++;
            entryIt->lastUsedFrame = currentFrameIndex_;
            ++hitCount_;
            outHandle = GPUTextureCacheHandle{entryIt->id, entryIt->generation};
            return ExistingSourceState::Hit;
        }
        if (entryIt != entries_.end()) {
            eraseEntryByIdLocked(entryIt->id);
        } else {
            keyToId_.erase(existingIdIt);
        }
    }

    if (pendingKeyToTicket_.contains(key)) {
        return ExistingSourceState::Pending;
    }
    return ExistingSourceState::NeedUpload;
}

void GPUTextureCacheManager::purgeStaleVersionEntriesLocked(const QString& ownerId,
                                                            const QString& cacheKey)
{
    const QStringView currentVersionToken =
        ownerId.startsWith(QStringLiteral("asset:"))
            ? versionedAssetCacheToken(cacheKey)
            : QStringView{};
    if (currentVersionToken.isEmpty()) {
        return;
    }
    const auto latestVersionIt =
        latestAssetVersionTokenByOwner_.constFind(ownerId);
    if (latestVersionIt != latestAssetVersionTokenByOwner_.cend() &&
        QStringView{latestVersionIt.value()} == currentVersionToken) {
        return;
    }

    const auto ownerIds = ownerToIds_.value(ownerId);
    for (const quint64 id : ownerIds) {
        const auto entryIt = entries_.constFind(id);
        if (entryIt != entries_.cend()) {
            const QStringView entryVersionToken =
                versionedAssetCacheToken(entryIt->cacheKey);
            if (!entryVersionToken.isEmpty() &&
                entryVersionToken != currentVersionToken) {
                eraseEntryByIdLocked(id);
            }
        }
    }
    bool hasCurrentVersionPendingUpload = false;
    auto pendingIt = pendingUploads_.begin();
    while (pendingIt != pendingUploads_.end()) {
        const QStringView pendingVersionToken =
            versionedAssetCacheToken(pendingIt->cacheKey);
        if (pendingIt->ownerId != ownerId || pendingVersionToken.isEmpty()) {
            ++pendingIt;
            continue;
        }
        if (pendingVersionToken == currentVersionToken) {
            hasCurrentVersionPendingUpload = true;
            ++pendingIt;
            continue;
        }
        if (uploadCoordinator_) {
            uploadCoordinator_->cancel(
                {pendingIt->ticketId, pendingIt->ticketGeneration});
        }
        pendingKeyToTicket_.remove(pendingIt->fullKey);
        pendingIt = pendingUploads_.erase(pendingIt);
    }
    if (ownerToIds_.contains(ownerId) || hasCurrentVersionPendingUpload) {
        latestAssetVersionTokenByOwner_.insert(ownerId,
                                              currentVersionToken.toString());
    } else {
        latestAssetVersionTokenByOwner_.remove(ownerId);
    }
}

GPUTextureCacheHandle GPUTextureCacheManager::acquireOrCreateFromRgbaBytes(const QString& ownerId,
                                                                           const QString& cacheKey,
                                                                           Uint32 width,
                                                                           Uint32 height,
                                                                           Uint64 stride,
                                                                           const void* bytes,
                                                                           size_t memoryBytes,
                                                                           TEXTURE_FORMAT format)
{
    if (ownerId.isEmpty() || cacheKey.isEmpty() || !bytes || width == 0 ||
        height == 0 || stride == 0 || format == TEX_FORMAT_UNKNOWN) {
        QMutexLocker locker(&mutex_);
        ++missCount_;
        return {};
    }

    QMutexLocker locker(&mutex_);
    if (!device_) {
        ++missCount_;
        return {};
    }

    GPUTextureCacheHandle existingHandle;
    const ExistingSourceState existingState =
        tryAcquireExistingLocked(ownerId, cacheKey, format, existingHandle);
    if (existingState == ExistingSourceState::Hit) {
        return existingHandle;
    }
    if (existingState == ExistingSourceState::Pending) {
        return {};
    }

    if (!uploadCoordinator_ ||
        memoryBytes > static_cast<size_t>(std::numeric_limits<qsizetype>::max())) {
        ++missCount_;
        return {};
    }

    const QString key = makeKey(ownerId, cacheKey) +
                        QStringLiteral("|format:%1").arg(static_cast<int>(format));
    DiligentTextureUploadRequest uploadRequest;
    uploadRequest.key = key;
    uploadRequest.bytes = QByteArray(
        static_cast<const char*>(bytes), static_cast<qsizetype>(memoryBytes));
    uploadRequest.width = width;
    uploadRequest.height = height;
    uploadRequest.stride = stride;
    uploadRequest.format = format;
    uploadRequest.generation = generation_;
    const DiligentUploadTicket ticket =
        uploadCoordinator_->enqueue(uploadRequest);
    if (!ticket.isValid()) {
        ++missCount_;
        return {};
    }

    PendingUpload pending;
    pending.ticketId = ticket.id;
    pending.ticketGeneration = ticket.generation;
    pending.format = format;
    pending.ownerId = ownerId;
    pending.cacheKey = cacheKey;
    pending.fullKey = key;
    pending.memoryBytes = memoryBytes;
    pendingUploads_.insert(ticket.id, pending);
    pendingKeyToTicket_.insert(key, ticket.id);
    ++missCount_;
    return {};
}

Diligent::ITextureView* GPUTextureCacheManager::textureView(const GPUTextureCacheHandle& handle) const
{
    QMutexLocker locker(&mutex_);
    auto it = entries_.find(handle.id);
    if (it == entries_.end() || it->generation != handle.generation || !it->srv) {
        return nullptr;
    }
    it->lastUsedTick = usageTick_++;
    it->lastUsedFrame = currentFrameIndex_;
    if (it->d3d12SlotIndex >= 0 && d3d12VideoFramePool_)
        d3d12VideoFramePool_->markUsed(it->d3d12SlotIndex);
    return it->srv.RawPtr();
}

GPUTextureBindingRecord GPUTextureCacheManager::bindingRecord(const GPUTextureCacheHandle& handle) const
{
    QMutexLocker locker(&mutex_);
    GPUTextureBindingRecord record;
    record.handle = handle;
    auto it = entries_.find(handle.id);
    if (it == entries_.end() || it->generation != handle.generation || !it->texture || !it->srv) {
        return record;
    }
    it->lastUsedTick = usageTick_++;
    it->lastUsedFrame = currentFrameIndex_;
    if (it->d3d12SlotIndex >= 0 && d3d12VideoFramePool_)
        d3d12VideoFramePool_->markUsed(it->d3d12SlotIndex);
    record.texture = it->texture.RawPtr();
    record.srv = it->srv.RawPtr();
    record.preferredMode = GPUTextureBindingMode::LegacySRV;
    return record;
}

bool GPUTextureCacheManager::isValid(const GPUTextureCacheHandle& handle) const
{
    QMutexLocker locker(&mutex_);
    auto it = entries_.find(handle.id);
    return it != entries_.end() && it->generation == handle.generation && it->texture;
}

void GPUTextureCacheManager::invalidate(const GPUTextureCacheHandle& handle)
{
    invalidate(handle, GPUTextureCacheInvalidationReason::Explicit);
}

void GPUTextureCacheManager::invalidate(
    const GPUTextureCacheHandle& handle,
    GPUTextureCacheInvalidationReason reason)
{
    QMutexLocker locker(&mutex_);
    const auto entryIt = entries_.constFind(handle.id);
    if (entryIt == entries_.cend() ||
        entryIt->generation != handle.generation) {
        return;
    }
    recordInvalidationLocked(reason);
    eraseEntryByIdLocked(handle.id);
}

void GPUTextureCacheManager::invalidateOwner(const QString& ownerId)
{
    invalidateOwner(ownerId, GPUTextureCacheInvalidationReason::OwnerChanged);
}

void GPUTextureCacheManager::invalidateOwner(
    const QString& ownerId,
    GPUTextureCacheInvalidationReason reason)
{
    QMutexLocker locker(&mutex_);
    bool invalidated = false;
    auto ownerIt = ownerToIds_.find(ownerId);
    while (ownerIt != ownerToIds_.end() && !ownerIt.value().isEmpty()) {
        const quint64 id = *ownerIt.value().cbegin();
        eraseEntryByIdLocked(id);
        invalidated = true;
        ownerIt = ownerToIds_.find(ownerId);
    }

    auto pendingIt = pendingUploads_.begin();
    while (pendingIt != pendingUploads_.end()) {
        if (pendingIt->ownerId != ownerId) {
            ++pendingIt;
            continue;
        }
        if (uploadCoordinator_) {
            uploadCoordinator_->cancel(
                {pendingIt->ticketId, pendingIt->ticketGeneration});
        }
        pendingKeyToTicket_.remove(pendingIt->fullKey);
        pendingIt = pendingUploads_.erase(pendingIt);
        invalidated = true;
    }
    latestAssetVersionTokenByOwner_.remove(ownerId);
    if (invalidated) {
        recordInvalidationLocked(reason);
    }
}

void GPUTextureCacheManager::clear()
{
    QMutexLocker locker(&mutex_);
    recordInvalidationLocked(GPUTextureCacheInvalidationReason::ClearAll);
    clearLocked();
}

void GPUTextureCacheManager::clearLocked()
{
    if (d3d12VideoFramePool_) d3d12VideoFramePool_->detachCacheEntries();
    if (uploadCoordinator_) {
        uploadCoordinator_->clear();
    }
    pendingUploads_.clear();
    pendingKeyToTicket_.clear();
    entries_.clear();
    keyToId_.clear();
    ownerToIds_.clear();
    ownerCacheKeyToIds_.clear();
    latestAssetVersionTokenByOwner_.clear();
    currentBytes_ = 0;
    currentFrameIndex_ = 0;
    pendingUploadWorkProcessedThisFrame_ = false;
    expirationWorkProcessedThisFrame_ = false;
    expiredEvictionsThisFrame_ = 0;
    ++generation_;
}

size_t GPUTextureCacheManager::totalMemoryBytesLocked() const
{
    return currentBytes_ + (d3d12VideoFramePool_
        ? d3d12VideoFramePool_->memoryBytes() : 0u);
}

void GPUTextureCacheManager::processPendingUploadsLocked()
{
    if (pendingUploadWorkProcessedThisFrame_) {
        return;
    }
    pendingUploadWorkProcessedThisFrame_ = true;
    applyPendingD3D12TrimLocked();
    if (!uploadCoordinator_) return;
    uploadCoordinator_->processPending(8, 64ull * 1024ull * 1024ull);
    if (pendingUploads_.isEmpty()) return;

    auto pendingIt = pendingUploads_.begin();
    while (pendingIt != pendingUploads_.end()) {
        DiligentTextureUploadResult result;
        if (!uploadCoordinator_->tryTakeResult(
                {pendingIt->ticketId, pendingIt->ticketGeneration}, result)) {
            ++pendingIt;
            continue;
        }

        const PendingUpload pending = pendingIt.value();
        pendingKeyToTicket_.remove(pending.fullKey);
        pendingIt = pendingUploads_.erase(pendingIt);
        if (!result.succeeded() || pending.ticketGeneration != generation_) {
            if (!result.canceled && !result.stale) {
                qWarning() << "[GPUTextureCache] queued upload failed"
                           << "owner=" << pending.ownerId
                           << "cacheKey=" << pending.cacheKey
                           << "error=" << result.error;
            }
            pruneAssetVersionTokenIfOwnerUnusedLocked(pending.ownerId);
            continue;
        }

        Entry entry;
        entry.id = nextId_++;
        entry.generation = generation_;
        entry.format = pending.format;
        entry.ownerId = pending.ownerId;
        entry.cacheKey = pending.cacheKey;
        entry.fullKey = pending.fullKey;
        entry.texture = std::move(result.texture);
        entry.srv = std::move(result.srv);
        entry.memoryBytes = pending.memoryBytes;
        entry.lastUsedTick = usageTick_++;
        entry.lastUsedFrame = currentFrameIndex_;
        entries_.insert(entry.id, entry);
        keyToId_.insert(pending.fullKey, entry.id);
        ownerToIds_[pending.ownerId].insert(entry.id);
        ownerCacheKeyToIds_[pending.ownerId][pending.cacheKey].insert(entry.id);
        currentBytes_ += entry.memoryBytes;
    }
    pruneLocked();
}

void GPUTextureCacheManager::applyPendingD3D12TrimLocked()
{
    const D3D12TrimRequestSnapshot request = claimD3D12TrimRequest();
    if (!request.pending || request.generation <= lastD3D12TrimGeneration_) {
        return;
    }
    lastD3D12TrimGeneration_ = request.generation;

    const size_t requestedBytes = static_cast<size_t>(std::min<Diligent::Uint64>(
        request.requestedBytes,
        static_cast<Diligent::Uint64>((std::numeric_limits<size_t>::max)())));
    const size_t targetBytesToRelease = std::min(requestedBytes, currentBytes_);
    size_t releasedBytes = 0;
    while (releasedBytes < targetBytesToRelease && !entries_.isEmpty()) {
        quint64 lruId = 0;
        quint64 oldestTick = ~quint64(0);
        size_t entryBytes = 0;
        for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
            if (it->lastUsedTick < oldestTick) {
                oldestTick = it->lastUsedTick;
                lruId = it->id;
                entryBytes = it->memoryBytes;
            }
        }
        if (lruId == 0) {
            break;
        }
        recordInvalidationLocked(GPUTextureCacheInvalidationReason::BudgetEviction);
        eraseEntryByIdLocked(lruId);
        releasedBytes += entryBytes;
    }

    qInfo() << "[GPUTextureCacheManager] D3D12 trim request applied"
            << "generation=" << request.generation
            << "requestedBytes=" << request.requestedBytes
            << "releasedBytes=" << releasedBytes
            << "remainingBytes=" << currentBytes_;
}

GPUTextureCacheStats GPUTextureCacheManager::stats() const
{
    QMutexLocker locker(&mutex_);
    GPUTextureCacheStats result;
    result.memoryBytes = totalMemoryBytesLocked();
    result.entryCount = static_cast<int>(entries_.size());
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
        if (it->sourceGpuFrame.isValid()) {
            ++result.importedGpuFrameCount;
        }
    }
    result.hitCount = hitCount_;
    result.missCount = missCount_;
    result.invalidationCount = invalidationCount_;
    result.lastInvalidationReason = lastInvalidationReason_;
    result.explicitInvalidationCount = explicitInvalidationCount_;
    result.ownerChangedInvalidationCount = ownerChangedInvalidationCount_;
    result.budgetEvictionCount = budgetEvictionCount_;
    result.deviceResetCount = deviceResetCount_;
    result.clearAllCount = clearAllCount_;
    for (auto it = pendingUploads_.cbegin(); it != pendingUploads_.cend(); ++it) {
        ++result.pendingUploadCount;
        result.pendingUploadBytes += it->memoryBytes;
    }
    result.currentFrameIndex = currentFrameIndex_;
    result.resourceExpirationFrames = resourceExpirationFrames_;
    result.expiredEvictionCount = expiredEvictionCount_;
    result.expiredEvictionsThisFrame = expiredEvictionsThisFrame_;
    return result;
}

int GPUTextureCacheManager::ownerEntryCount(const QString& ownerId) const
{
    if (ownerId.isEmpty()) {
        return 0;
    }
    QMutexLocker locker(&mutex_);
    const auto ownerIt = ownerToIds_.constFind(ownerId);
    return ownerIt == ownerToIds_.cend() ? 0 : ownerIt.value().size();
}

GPUTextureOwnerStats GPUTextureCacheManager::ownerStats(
    const QString& ownerId) const
{
    GPUTextureOwnerStats result;
    if (ownerId.isEmpty()) {
        return result;
    }
    QMutexLocker locker(&mutex_);
    const auto ownerIt = ownerToIds_.constFind(ownerId);
    if (ownerIt != ownerToIds_.cend()) {
        const auto& ids = ownerIt.value();
        result.entryCount = ids.size();
        for (const quint64 id : ids) {
            const auto it = entries_.constFind(id);
            if (it != entries_.cend()) {
                result.memoryBytes += it->memoryBytes;
                if (it->sourceGpuFrame.isValid()) {
                    ++result.importedGpuFrameCount;
                }
            }
        }
    }
    for (auto it = pendingUploads_.cbegin(); it != pendingUploads_.cend(); ++it) {
        if (it->ownerId == ownerId) {
            ++result.pendingUploadCount;
            result.pendingUploadBytes += it->memoryBytes;
        }
    }
    return result;
}

size_t GPUTextureCacheManager::ownerMemoryBytes(const QString& ownerId) const
{
    if (ownerId.isEmpty()) {
        return 0;
    }
    QMutexLocker locker(&mutex_);
    size_t bytes = 0;
    const auto ownerIt = ownerToIds_.constFind(ownerId);
    if (ownerIt != ownerToIds_.cend()) {
        for (const quint64 id : ownerIt.value()) {
            const auto it = entries_.constFind(id);
            if (it != entries_.cend()) {
                bytes += it->memoryBytes;
            }
        }
    }
    return bytes;
}

void GPUTextureCacheManager::eraseEntryByIdLocked(quint64 id)
{
    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return;
    }
    if (it->d3d12SlotIndex >= 0 && d3d12VideoFramePool_)
        d3d12VideoFramePool_->clearCacheEntryId(it->d3d12SlotIndex, id);

    const QString fullKey = it->fullKey.isEmpty()
                                ? makeKey(it->ownerId, it->cacheKey) +
                                      QStringLiteral("|format:%1").arg(
                                          static_cast<int>(it->format))
                                : it->fullKey;
    keyToId_.remove(fullKey);
    auto ownerIt = ownerToIds_.find(it->ownerId);
    if (ownerIt != ownerToIds_.end()) {
        ownerIt.value().remove(id);
        if (ownerIt.value().isEmpty()) {
            ownerToIds_.remove(it->ownerId);
        }
    }
    auto ownerCacheIt = ownerCacheKeyToIds_.find(it->ownerId);
    if (ownerCacheIt != ownerCacheKeyToIds_.end()) {
        auto cacheKeyIt = ownerCacheIt.value().find(it->cacheKey);
        if (cacheKeyIt != ownerCacheIt.value().end()) {
            cacheKeyIt.value().remove(id);
            if (cacheKeyIt.value().isEmpty()) {
                ownerCacheIt.value().erase(cacheKeyIt);
            }
        }
        if (ownerCacheIt.value().isEmpty()) {
            ownerCacheKeyToIds_.erase(ownerCacheIt);
        }
    }
    currentBytes_ = (currentBytes_ > it->memoryBytes) ? (currentBytes_ - it->memoryBytes) : 0;
    const QString ownerId = it->ownerId;
    entries_.erase(it);
    pruneAssetVersionTokenIfOwnerUnusedLocked(ownerId);
}

void GPUTextureCacheManager::pruneAssetVersionTokenIfOwnerUnusedLocked(
    const QString& ownerId)
{
    if (ownerToIds_.contains(ownerId)) {
        return;
    }
    for (auto it = pendingUploads_.cbegin(); it != pendingUploads_.cend(); ++it) {
        if (it->ownerId == ownerId) {
            return;
        }
    }
    latestAssetVersionTokenByOwner_.remove(ownerId);
}

void GPUTextureCacheManager::pruneLocked()
{
    while ((entries_.size() > static_cast<size_t>(maxEntries_) || totalMemoryBytesLocked() > budgetBytes_) &&
           !entries_.isEmpty()) {
        quint64 lruId = 0;
        quint64 oldestTick = ~quint64(0);
        for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
            if (it->lastUsedTick < oldestTick) {
                oldestTick = it->lastUsedTick;
                lruId = it->id;
            }
        }
        if (lruId == 0) {
            break;
        }
        recordInvalidationLocked(GPUTextureCacheInvalidationReason::BudgetEviction);
        eraseEntryByIdLocked(lruId);
    }
}

void GPUTextureCacheManager::pruneExpiredLocked()
{
    if (resourceExpirationFrames_ == 0 || entries_.isEmpty()) {
        return;
    }

    std::array<quint64, MaxExpiredEvictionsPerFrameLimit> expiredIds{};
    std::array<quint64, MaxExpiredEvictionsPerFrameLimit> expiredFrames{};
    int expiredCount = 0;
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
        const quint64 age = currentFrameIndex_ >= it->lastUsedFrame
                                ? currentFrameIndex_ - it->lastUsedFrame
                                : 0;
        if (age <= resourceExpirationFrames_) {
            continue;
        }

        int insertionIndex = 0;
        while (insertionIndex < expiredCount &&
               expiredFrames[static_cast<size_t>(insertionIndex)] <=
                   it->lastUsedFrame) {
            ++insertionIndex;
        }

        if (insertionIndex >= maxExpiredEvictionsPerFrame_) {
            continue;
        }
        if (expiredCount < maxExpiredEvictionsPerFrame_) {
            ++expiredCount;
        }
        for (int moveIndex = expiredCount - 1;
             moveIndex > insertionIndex; --moveIndex) {
            const auto destination = static_cast<size_t>(moveIndex);
            const auto source = static_cast<size_t>(moveIndex - 1);
            expiredIds[destination] = expiredIds[source];
            expiredFrames[destination] = expiredFrames[source];
        }
        expiredIds[static_cast<size_t>(insertionIndex)] = it->id;
        expiredFrames[static_cast<size_t>(insertionIndex)] = it->lastUsedFrame;
    }

    for (int index = 0; index < expiredCount; ++index) {
        ++expiredEvictionCount_;
        ++expiredEvictionsThisFrame_;
        recordInvalidationLocked(GPUTextureCacheInvalidationReason::FrameExpiration);
        eraseEntryByIdLocked(expiredIds[static_cast<size_t>(index)]);
    }
}

void GPUTextureCacheManager::recordInvalidationLocked(
    GPUTextureCacheInvalidationReason reason)
{
    ++invalidationCount_;
    lastInvalidationReason_ = reason;
    switch (reason) {
    case GPUTextureCacheInvalidationReason::Explicit:
        ++explicitInvalidationCount_;
        break;
    case GPUTextureCacheInvalidationReason::OwnerChanged:
        ++ownerChangedInvalidationCount_;
        break;
    case GPUTextureCacheInvalidationReason::BudgetEviction:
        ++budgetEvictionCount_;
        break;
    case GPUTextureCacheInvalidationReason::DeviceReset:
        ++deviceResetCount_;
        break;
    case GPUTextureCacheInvalidationReason::ClearAll:
        ++clearAllCount_;
        break;
    case GPUTextureCacheInvalidationReason::FrameExpiration:
        break;
    }
}

} // namespace Artifact
