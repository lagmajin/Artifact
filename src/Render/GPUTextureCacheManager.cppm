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
    recordInvalidationLocked(GPUTextureCacheInvalidationReason::DeviceReset);
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
    result.memoryBytes = currentBytes_;
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
