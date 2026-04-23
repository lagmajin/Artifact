module;
#include <algorithm>
#include <vector>
#include <utility>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QDebug>
#include <QMutexLocker>
#include <opencv2/core.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Render.GPUTextureCacheManager;

import Image.ImageF32x4_RGBA;

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
    const cv::Mat src = image.toCVMat();
    if (src.empty()) {
        return upload;
    }

    cv::Mat rgba32;
    if (src.type() == CV_32FC4) {
        rgba32 = src;
    } else {
        image.toCVMat().convertTo(rgba32, CV_32FC4);
    }

    upload.width = static_cast<Uint32>(rgba32.cols);
    upload.height = static_cast<Uint32>(rgba32.rows);
    upload.stride = static_cast<Uint64>(upload.width) * 4ull;
    upload.bytes.resize(static_cast<size_t>(upload.stride) * static_cast<size_t>(upload.height));

    for (int y = 0; y < rgba32.rows; ++y) {
        const auto* srcRow = rgba32.ptr<cv::Vec4f>(y);
        auto* dstRow = upload.bytes.data() + static_cast<size_t>(y) * static_cast<size_t>(upload.stride);
        for (int x = 0; x < rgba32.cols; ++x) {
            const cv::Vec4f pixel = srcRow[x];
            dstRow[x * 4 + 0] = static_cast<uint8_t>(std::clamp(pixel[0], 0.0f, 1.0f) * 255.0f + 0.5f);
            dstRow[x * 4 + 1] = static_cast<uint8_t>(std::clamp(pixel[1], 0.0f, 1.0f) * 255.0f + 0.5f);
            dstRow[x * 4 + 2] = static_cast<uint8_t>(std::clamp(pixel[2], 0.0f, 1.0f) * 255.0f + 0.5f);
            dstRow[x * 4 + 3] = static_cast<uint8_t>(std::clamp(pixel[3], 0.0f, 1.0f) * 255.0f + 0.5f);
        }
    }

    return upload;
}

} // namespace

GPUTextureCacheManager::GPUTextureCacheManager() = default;
GPUTextureCacheManager::~GPUTextureCacheManager() = default;

void GPUTextureCacheManager::setDevice(RefCntAutoPtr<IRenderDevice> device,
                                       TEXTURE_FORMAT format)
{
    QMutexLocker locker(&mutex_);
    device_ = std::move(device);
    textureFormat_ = format;
    clear();
}

void GPUTextureCacheManager::clearDevice()
{
    QMutexLocker locker(&mutex_);
    device_.Release();
    clear();
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
