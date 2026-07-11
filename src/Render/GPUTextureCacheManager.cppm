module;
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <variant>
#include <vector>
#include <utility>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QDebug>
#include <QMutexLocker>
#include <vulkan/vulkan.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/RenderDeviceVk.h>

module Artifact.Render.GPUTextureCacheManager;

import Image.ImageF32x4_RGBA;
import Video.VideoFrame;

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

struct UploadImageData
{
    std::vector<uint8_t> bytes;
    Uint32 width = 0;
    Uint32 height = 0;
    Uint64 stride = 0;
};

UploadImageData makeUploadImageData(const ArtifactCore::ImageF32x4_RGBA& image)
{
    UploadImageData upload;
    const int width = image.width();
    const int height = image.height();
    if (width <= 0 || height <= 0) {
        return upload;
    }

    if (const float* rgba32 = image.rgba32fData()) {
        // Internal mat is CV_32FC4 stored as BGRA (from qImageToCvMat+BGR2BGRA).
        // GPU texture is TEX_FORMAT_RGBA8_UNORM_SRGB, so convert float->uint8 and swap B<->R.
        upload.width  = static_cast<Uint32>(width);
        upload.height = static_cast<Uint32>(height);
        upload.stride = static_cast<Uint64>(upload.width) * 4ull;  // 4 bytes/pixel (RGBA8)
        const size_t totalBytes = static_cast<size_t>(upload.stride) * static_cast<size_t>(upload.height);
        upload.bytes.resize(totalBytes);
        const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
        for (size_t i = 0; i < pixelCount; ++i) {
            // Source layout: [B, G, R, A] as float
            const float srcB = rgba32[i * 4u + 0];
            const float srcG = rgba32[i * 4u + 1];
            const float srcR = rgba32[i * 4u + 2];
            const float srcA = rgba32[i * 4u + 3];
            // Dest layout: [R, G, B, A] as uint8 (RGBA8_UNORM)
            upload.bytes[i * 4u + 0] = static_cast<uint8_t>(std::clamp(srcR, 0.0f, 1.0f) * 255.0f + 0.5f);
            upload.bytes[i * 4u + 1] = static_cast<uint8_t>(std::clamp(srcG, 0.0f, 1.0f) * 255.0f + 0.5f);
            upload.bytes[i * 4u + 2] = static_cast<uint8_t>(std::clamp(srcB, 0.0f, 1.0f) * 255.0f + 0.5f);
            upload.bytes[i * 4u + 3] = static_cast<uint8_t>(std::clamp(srcA, 0.0f, 1.0f) * 255.0f + 0.5f);
        }
        return upload;
    }

    if (const std::uint8_t* rgba8 = image.rgba8Data()) {
        upload.width = static_cast<Uint32>(width);
        upload.height = static_cast<Uint32>(height);
        upload.stride = static_cast<Uint64>(upload.width) * 4ull;
        upload.bytes.resize(static_cast<size_t>(upload.stride) * static_cast<size_t>(upload.height));
        std::memcpy(upload.bytes.data(),
                    rgba8,
                    static_cast<size_t>(upload.stride) * static_cast<size_t>(upload.height));
        return upload;
    }

    return upload;
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


GPUTextureCacheManager::GPUTextureCacheManager() = default;
GPUTextureCacheManager::~GPUTextureCacheManager()
{
    clearDevice();
}

void GPUTextureCacheManager::setDevice(RefCntAutoPtr<IRenderDevice> device,
                                       TEXTURE_FORMAT format)
{
    QMutexLocker locker(&mutex_);
    clearLocked();
    device_ = std::move(device);
    textureFormat_ = format;
}

void GPUTextureCacheManager::clearDevice()
{
    QMutexLocker locker(&mutex_);
    clearLocked();
    device_.Release();
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

QString GPUTextureCacheManager::makeKey(const QString& ownerId, const QString& cacheKey) const
{
    return ownerId + QStringLiteral("|") + cacheKey;
}

GPUTextureCacheHandle GPUTextureCacheManager::acquireOrCreate(const QString& ownerId,
                                                             const QString& cacheKey,
                                                             const QImage& image)
{
    const QImage rgba = (image.format() == QImage::Format_RGBA8888)
                            ? image
                            : image.convertToFormat(QImage::Format_RGBA8888);
    return acquireOrCreateFromRgbaBytes(ownerId,
                                        cacheKey,
                                        static_cast<Uint32>(rgba.width()),
                                        static_cast<Uint32>(rgba.height()),
                                        static_cast<Uint64>(rgba.bytesPerLine()),
                                        rgba.constBits(),
                                        bytesForImage(rgba));
}

GPUTextureCacheHandle GPUTextureCacheManager::acquireOrCreate(const QString& ownerId,
                                                             const QString& cacheKey,
                                                             const ArtifactCore::ImageF32x4_RGBA& image)
{
    const UploadImageData upload = makeUploadImageData(image);
    return acquireOrCreateFromRgbaBytes(ownerId,
                                        cacheKey,
                                        upload.width,
                                        upload.height,
                                        upload.stride,
                                        upload.bytes.data(),
                                        upload.bytes.size());
}

GPUTextureCacheHandle GPUTextureCacheManager::acquireOrCreate(const QString& ownerId,
                                                             const QString& cacheKey,
                                                             const ArtifactCore::GpuVideoFrame& frame)
{
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

    RefCntAutoPtr<IRenderDeviceVk> deviceVk{device_, IID_RenderDeviceVk};
    if (!deviceVk) {
        ++missCount_;
        return {};
    }

    const QString currentVersionToken =
        (ownerId.startsWith(QStringLiteral("asset:")) &&
         (cacheKey.startsWith(QStringLiteral("video-gpu:v")) ||
          cacheKey.startsWith(QStringLiteral("image-f32:v"))))
            ? cacheKey.section(QLatin1Char(':'), 1, 1)
            : QString();
    if (!currentVersionToken.isEmpty()) {
        const auto ownerIds = ownerToIds_.value(ownerId);
        for (const quint64 id : ownerIds) {
            const auto entryIt = entries_.constFind(id);
            if (entryIt != entries_.cend() &&
                entryIt->cacheKey.section(QLatin1Char(':'), 1, 1) != currentVersionToken) {
                eraseEntryByIdLocked(id);
            }
        }
    }

    const QString key = makeKey(ownerId, cacheKey);
    const auto existingIdIt = keyToId_.find(key);
    if (existingIdIt != keyToId_.end()) {
        auto entryIt = entries_.find(existingIdIt.value());
        if (entryIt != entries_.end() && entryIt->generation == generation_ && entryIt->texture) {
            entryIt->lastUsedTick = usageTick_++;
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
    entry.ownerId = ownerId;
    entry.cacheKey = cacheKey;
    entry.texture = texture;
    entry.srv = texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    entry.sourceGpuFrame = frame;
    entry.memoryBytes = 0;
    entry.lastUsedTick = usageTick_++;

    entries_.insert(entry.id, entry);
    keyToId_.insert(key, entry.id);
    ownerToIds_[ownerId].insert(entry.id);
    ++missCount_;

    pruneLocked();
    return {entry.id, entry.generation};
}

GPUTextureCacheHandle GPUTextureCacheManager::acquireOrCreateFromRgbaBytes(const QString& ownerId,
                                                                           const QString& cacheKey,
                                                                           Uint32 width,
                                                                           Uint32 height,
                                                                           Uint64 stride,
                                                                           const void* bytes,
                                                                           size_t memoryBytes)
{
    if (ownerId.isEmpty() || cacheKey.isEmpty() || !bytes || width == 0 || height == 0 || stride == 0) {
        QMutexLocker locker(&mutex_);
        ++missCount_;
        return {};
    }

    QMutexLocker locker(&mutex_);
    if (!device_) {
        ++missCount_;
        return {};
    }

    const QString currentVersionToken =
        (ownerId.startsWith(QStringLiteral("asset:")) &&
         (cacheKey.startsWith(QStringLiteral("video-gpu:v")) ||
          cacheKey.startsWith(QStringLiteral("image-f32:v"))))
            ? cacheKey.section(QLatin1Char(':'), 1, 1)
            : QString();
    if (!currentVersionToken.isEmpty()) {
        const auto ownerIds = ownerToIds_.value(ownerId);
        for (const quint64 id : ownerIds) {
            const auto entryIt = entries_.constFind(id);
            if (entryIt != entries_.cend() &&
                entryIt->cacheKey.section(QLatin1Char(':'), 1, 1) != currentVersionToken) {
                eraseEntryByIdLocked(id);
            }
        }
    }

    const QString key = makeKey(ownerId, cacheKey);
    const auto existingIdIt = keyToId_.find(key);
    if (existingIdIt != keyToId_.end()) {
        auto entryIt = entries_.find(existingIdIt.value());
        if (entryIt != entries_.end() && entryIt->generation == generation_ && entryIt->texture) {
            entryIt->lastUsedTick = usageTick_++;
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
    texDesc.Name = "GPUTextureCacheManager.Texture";
    texDesc.Type = RESOURCE_DIM_TEX_2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.Format = textureFormat_;
    texDesc.Usage = USAGE_IMMUTABLE;
    texDesc.BindFlags = BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = CPU_ACCESS_NONE;

    TextureSubResData subRes;
    subRes.pData = bytes;
    subRes.Stride = stride;

    TextureData initData;
    initData.pSubResources = &subRes;
    initData.NumSubresources = 1;

    RefCntAutoPtr<ITexture> texture;
    device_->CreateTexture(texDesc, &initData, &texture);
    if (!texture) {
        qWarning() << "[GPUTextureCache] CreateTexture failed"
                   << "owner=" << ownerId
                   << "cacheKey=" << cacheKey
                   << "size=" << width << "x" << height
                   << "stride=" << stride
                   << "bytes=" << static_cast<qulonglong>(memoryBytes);
        ++missCount_;
        return {};
    }

    Entry entry;
    entry.id = nextId_++;
    entry.generation = generation_;
    entry.ownerId = ownerId;
    entry.cacheKey = cacheKey;
    entry.texture = texture;
    entry.srv = texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    entry.memoryBytes = memoryBytes;
    entry.lastUsedTick = usageTick_++;

    entries_.insert(entry.id, entry);
    keyToId_.insert(key, entry.id);
    ownerToIds_[ownerId].insert(entry.id);
    currentBytes_ += entry.memoryBytes;
    ++missCount_;

    pruneLocked();
    return {entry.id, entry.generation};
}

Diligent::ITextureView* GPUTextureCacheManager::textureView(const GPUTextureCacheHandle& handle) const
{
    QMutexLocker locker(&mutex_);
    auto it = entries_.find(handle.id);
    if (it == entries_.end() || it->generation != handle.generation || !it->srv) {
        return nullptr;
    }
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
    QMutexLocker locker(&mutex_);
    eraseEntryByIdLocked(handle.id);
}

void GPUTextureCacheManager::invalidateOwner(const QString& ownerId)
{
    QMutexLocker locker(&mutex_);
    auto it = ownerToIds_.find(ownerId);
    if (it == ownerToIds_.end()) {
        return;
    }
    const auto ids = it.value();
    for (quint64 id : ids) {
        eraseEntryByIdLocked(id);
    }
    ownerToIds_.remove(ownerId);
}

void GPUTextureCacheManager::clear()
{
    QMutexLocker locker(&mutex_);
    clearLocked();
}

void GPUTextureCacheManager::clearLocked()
{
    entries_.clear();
    keyToId_.clear();
    ownerToIds_.clear();
    currentBytes_ = 0;
    ++generation_;
}

GPUTextureCacheStats GPUTextureCacheManager::stats() const
{
    QMutexLocker locker(&mutex_);
    return GPUTextureCacheStats{
        currentBytes_,
        static_cast<int>(entries_.size()),
        hitCount_,
        missCount_
    };
}

int GPUTextureCacheManager::ownerEntryCount(const QString& ownerId) const
{
    if (ownerId.isEmpty()) {
        return 0;
    }
    QMutexLocker locker(&mutex_);
    return ownerToIds_.value(ownerId).size();
}

size_t GPUTextureCacheManager::ownerMemoryBytes(const QString& ownerId) const
{
    if (ownerId.isEmpty()) {
        return 0;
    }
    QMutexLocker locker(&mutex_);
    size_t bytes = 0;
    const auto ids = ownerToIds_.value(ownerId);
    for (const quint64 id : ids) {
        const auto it = entries_.constFind(id);
        if (it != entries_.cend()) {
            bytes += it->memoryBytes;
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

    const QString key = makeKey(it->ownerId, it->cacheKey);
    keyToId_.remove(key);
    auto ownerIt = ownerToIds_.find(it->ownerId);
    if (ownerIt != ownerToIds_.end()) {
        ownerIt.value().remove(id);
        if (ownerIt.value().isEmpty()) {
            ownerToIds_.remove(it->ownerId);
        }
    }
    currentBytes_ = (currentBytes_ > it->memoryBytes) ? (currentBytes_ - it->memoryBytes) : 0;
    entries_.erase(it);
}

void GPUTextureCacheManager::pruneLocked()
{
    while ((entries_.size() > static_cast<size_t>(maxEntries_) || currentBytes_ > budgetBytes_) &&
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
        eraseEntryByIdLocked(lruId);
    }
}

} // namespace Artifact
