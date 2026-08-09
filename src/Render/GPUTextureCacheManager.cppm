module;
#include <algorithm>
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
#include <QDebug>
#include <QMutexLocker>
#include <vulkan/vulkan.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
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
    ++invalidationCount_;
    lastInvalidationReason_ = GPUTextureCacheInvalidationReason::DeviceReset;
    clearLocked();
    device_ = std::move(device);
    textureFormat_ = format;
    if (uploadCoordinator_) {
        uploadCoordinator_->setDevice(device_, std::move(context));
        uploadCoordinator_->setPendingByteBudget(256ull * 1024ull * 1024ull);
        uploadCoordinator_->setMaxPendingJobs(128);
    }
}

void GPUTextureCacheManager::clearDevice()
{
    QMutexLocker locker(&mutex_);
    ++invalidationCount_;
    lastInvalidationReason_ = GPUTextureCacheInvalidationReason::DeviceReset;
    clearLocked();
    if (uploadCoordinator_) {
        uploadCoordinator_->clearDevice();
    }
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
    TEXTURE_FORMAT configuredFormat = TEX_FORMAT_UNKNOWN;
    {
        QMutexLocker locker(&mutex_);
        configuredFormat = textureFormat_;
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
    const auto descriptor = image.colorDescriptor();
    const QString colorAwareCacheKey =
        cacheKey + QStringLiteral("|color:%1,%2,%3,%4,%5,%6,%7")
                       .arg(static_cast<int>(descriptor.storage))
                       .arg(static_cast<int>(descriptor.channelOrder))
                       .arg(static_cast<int>(descriptor.primaries))
                       .arg(static_cast<int>(descriptor.transfer))
                       .arg(static_cast<int>(descriptor.alphaMode))
                       .arg(static_cast<int>(descriptor.range))
                       .arg(descriptor.transferKnown ? 1 : 0);
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
        QList<quint64> stalePendingIds;
        for (auto pendingIt = pendingUploads_.cbegin();
             pendingIt != pendingUploads_.cend(); ++pendingIt) {
            if (pendingIt->ownerId == ownerId &&
                pendingIt->cacheKey.section(QLatin1Char(':'), 1, 1) != currentVersionToken) {
                stalePendingIds.push_back(pendingIt.key());
            }
        }
        for (const quint64 ticketId : stalePendingIds) {
            const auto pendingIt = pendingUploads_.find(ticketId);
            if (pendingIt == pendingUploads_.end()) continue;
            if (uploadCoordinator_) {
                uploadCoordinator_->cancel(
                    {pendingIt->ticketId, pendingIt->ticketGeneration});
            }
            pendingKeyToTicket_.remove(pendingIt->fullKey);
            pendingUploads_.erase(pendingIt);
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
    entry.fullKey = key;
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

GPUTextureCacheHandle GPUTextureCacheManager::findExisting(
    const QString& ownerId, const QString& cacheKey) const
{
    if (ownerId.isEmpty() || cacheKey.isEmpty()) {
        return {};
    }

    QMutexLocker locker(&mutex_);
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
        if (it->ownerId == ownerId &&
            it->cacheKey == cacheKey &&
            it->generation == generation_ && it->texture) {
            return {it->id, it->generation};
        }
    }
    return {};
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

    processPendingUploadsLocked();

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
        QList<quint64> stalePendingIds;
        for (auto pendingIt = pendingUploads_.cbegin();
             pendingIt != pendingUploads_.cend(); ++pendingIt) {
            if (pendingIt->ownerId == ownerId &&
                pendingIt->cacheKey.section(QLatin1Char(':'), 1, 1) != currentVersionToken) {
                stalePendingIds.push_back(pendingIt.key());
            }
        }
        for (const quint64 ticketId : stalePendingIds) {
            const auto pendingIt = pendingUploads_.find(ticketId);
            if (pendingIt == pendingUploads_.end()) continue;
            if (uploadCoordinator_) {
                uploadCoordinator_->cancel(
                    {pendingIt->ticketId, pendingIt->ticketGeneration});
            }
            pendingKeyToTicket_.remove(pendingIt->fullKey);
            pendingUploads_.erase(pendingIt);
        }
    }

    const QString key = makeKey(ownerId, cacheKey) +
                        QStringLiteral("|format:%1").arg(static_cast<int>(format));
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

    if (pendingKeyToTicket_.contains(key)) {
        return {};
    }

    if (!uploadCoordinator_ ||
        memoryBytes > static_cast<size_t>(std::numeric_limits<qsizetype>::max())) {
        ++missCount_;
        return {};
    }

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
    invalidate(handle, GPUTextureCacheInvalidationReason::Explicit);
}

void GPUTextureCacheManager::invalidate(
    const GPUTextureCacheHandle& handle,
    GPUTextureCacheInvalidationReason reason)
{
    QMutexLocker locker(&mutex_);
    ++invalidationCount_;
    lastInvalidationReason_ = reason;
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
    auto it = ownerToIds_.find(ownerId);
    bool invalidated = false;
    if (it != ownerToIds_.end()) {
        const auto ids = it.value();
        for (quint64 id : ids) {
            eraseEntryByIdLocked(id);
        }
        ownerToIds_.remove(ownerId);
        invalidated = !ids.isEmpty();
    }

    QList<quint64> pendingIds;
    for (auto pendingIt = pendingUploads_.cbegin();
         pendingIt != pendingUploads_.cend(); ++pendingIt) {
        if (pendingIt->ownerId == ownerId) pendingIds.push_back(pendingIt.key());
    }
    for (const quint64 ticketId : pendingIds) {
        const auto pendingIt = pendingUploads_.find(ticketId);
        if (pendingIt == pendingUploads_.end()) continue;
        if (uploadCoordinator_) {
            uploadCoordinator_->cancel(
                {pendingIt->ticketId, pendingIt->ticketGeneration});
        }
        pendingKeyToTicket_.remove(pendingIt->fullKey);
        pendingUploads_.erase(pendingIt);
        invalidated = true;
    }
    if (invalidated) {
        ++invalidationCount_;
        lastInvalidationReason_ = reason;
    }
}

void GPUTextureCacheManager::clear()
{
    QMutexLocker locker(&mutex_);
    ++invalidationCount_;
    lastInvalidationReason_ = GPUTextureCacheInvalidationReason::ClearAll;
    clearLocked();
}

void GPUTextureCacheManager::clearLocked()
{
    if (uploadCoordinator_) {
        uploadCoordinator_->clear();
    }
    pendingUploads_.clear();
    pendingKeyToTicket_.clear();
    entries_.clear();
    keyToId_.clear();
    ownerToIds_.clear();
    currentBytes_ = 0;
    ++generation_;
}

void GPUTextureCacheManager::processPendingUploadsLocked()
{
    applyPendingD3D12TrimLocked();
    if (!uploadCoordinator_) return;
    uploadCoordinator_->processPending(8, 64ull * 1024ull * 1024ull);
    if (pendingUploads_.isEmpty()) return;

    const QList<quint64> ticketIds = pendingUploads_.keys();
    for (const quint64 ticketId : ticketIds) {
        auto pendingIt = pendingUploads_.find(ticketId);
        if (pendingIt == pendingUploads_.end()) continue;
        DiligentTextureUploadResult result;
        if (!uploadCoordinator_->tryTakeResult(
                {pendingIt->ticketId, pendingIt->ticketGeneration}, result)) {
            continue;
        }

        const PendingUpload pending = pendingIt.value();
        pendingKeyToTicket_.remove(pending.fullKey);
        pendingUploads_.erase(pendingIt);
        if (!result.succeeded() || pending.ticketGeneration != generation_) {
            if (!result.canceled && !result.stale) {
                qWarning() << "[GPUTextureCache] queued upload failed"
                           << "owner=" << pending.ownerId
                           << "cacheKey=" << pending.cacheKey
                           << "error=" << result.error;
            }
            continue;
        }

        Entry entry;
        entry.id = nextId_++;
        entry.generation = generation_;
        entry.ownerId = pending.ownerId;
        entry.cacheKey = pending.cacheKey;
        entry.fullKey = pending.fullKey;
        entry.texture = std::move(result.texture);
        entry.srv = std::move(result.srv);
        entry.memoryBytes = pending.memoryBytes;
        entry.lastUsedTick = usageTick_++;
        entries_.insert(entry.id, entry);
        keyToId_.insert(pending.fullKey, entry.id);
        ownerToIds_[pending.ownerId].insert(entry.id);
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
        ++invalidationCount_;
        lastInvalidationReason_ =
            GPUTextureCacheInvalidationReason::BudgetEviction;
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
    const auto uploadStats = uploadCoordinator_
        ? uploadCoordinator_->stats()
        : DiligentUploadCoordinatorStats{};
    return GPUTextureCacheStats{
        currentBytes_,
        static_cast<int>(entries_.size()),
        hitCount_,
        missCount_,
        invalidationCount_,
        lastInvalidationReason_,
        uploadStats.pendingBytes,
        static_cast<int>(uploadStats.pendingJobs + uploadStats.gpuOperations)
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

GPUTextureOwnerStats GPUTextureCacheManager::ownerStats(
    const QString& ownerId) const
{
    GPUTextureOwnerStats result;
    if (ownerId.isEmpty()) {
        return result;
    }
    QMutexLocker locker(&mutex_);
    const auto ids = ownerToIds_.value(ownerId);
    result.entryCount = ids.size();
    for (const quint64 id : ids) {
        const auto it = entries_.constFind(id);
        if (it != entries_.cend()) {
            result.memoryBytes += it->memoryBytes;
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

    keyToId_.remove(it->fullKey.isEmpty()
                        ? makeKey(it->ownerId, it->cacheKey)
                        : it->fullKey);
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
        ++invalidationCount_;
        lastInvalidationReason_ = GPUTextureCacheInvalidationReason::BudgetEviction;
        eraseEntryByIdLocked(lruId);
    }
}

} // namespace Artifact
