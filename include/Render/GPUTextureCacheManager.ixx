module;
#include <utility>

#include <memory>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

#include <wobjectdefs.h>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QSet>
#include <QSize>
#include <QString>
export module Artifact.Render.GPUTextureCacheManager;

import Image.ImageF32x4_RGBA;
import Video.VideoFrame;

export namespace Artifact {

enum class GPUTextureBindingMode {
    LegacySRV,
    BindlessCandidate
};

enum class GPUTextureCacheInvalidationReason {
    Explicit,
    OwnerChanged,
    BudgetEviction,
    DeviceReset,
    ClearAll,
};

inline QString gpuTextureCacheInvalidationReasonText(
    GPUTextureCacheInvalidationReason reason)
{
    switch (reason) {
    case GPUTextureCacheInvalidationReason::Explicit: return QStringLiteral("explicit");
    case GPUTextureCacheInvalidationReason::OwnerChanged: return QStringLiteral("owner-changed");
    case GPUTextureCacheInvalidationReason::BudgetEviction: return QStringLiteral("budget-eviction");
    case GPUTextureCacheInvalidationReason::DeviceReset: return QStringLiteral("device-reset");
    case GPUTextureCacheInvalidationReason::ClearAll: return QStringLiteral("clear-all");
    }
    return QStringLiteral("unknown");
}

struct GPUTextureCacheHandle {
    quint64 id = 0;
    quint64 generation = 0;

    bool isValid() const { return id != 0; }
};

struct GPUTextureBindingRecord {
    GPUTextureCacheHandle handle;
    Diligent::ITexture* texture = nullptr;
    Diligent::ITextureView* srv = nullptr;
    GPUTextureBindingMode preferredMode = GPUTextureBindingMode::LegacySRV;

    bool isValid() const { return handle.isValid() && texture != nullptr && srv != nullptr; }
};

struct GPUTextureCacheStats {
    size_t memoryBytes = 0;
    int entryCount = 0;
    size_t hitCount = 0;
    size_t missCount = 0;
    quint64 invalidationCount = 0;
    GPUTextureCacheInvalidationReason lastInvalidationReason =
        GPUTextureCacheInvalidationReason::Explicit;
    size_t pendingUploadBytes = 0;
    int pendingUploadCount = 0;
};

struct GPUTextureOwnerStats {
    int entryCount = 0;
    size_t memoryBytes = 0;
    int pendingUploadCount = 0;
    size_t pendingUploadBytes = 0;
};

class DiligentUploadCoordinator;

class GPUTextureCacheManager {
public:
    GPUTextureCacheManager();
    ~GPUTextureCacheManager();

    void setDevice(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device,
                   Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context,
                   Diligent::TEXTURE_FORMAT format = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB);
    void clearDevice();

    void setBudgetBytes(size_t bytes);
    size_t budgetBytes() const;

    void setMaxEntries(int count);
    int maxEntries() const;

    GPUTextureCacheHandle acquireOrCreate(const QString& ownerId,
                                          const QString& cacheKey,
                                          const QImage& image);
    GPUTextureCacheHandle acquireOrCreate(const QString& ownerId,
                                          const QString& cacheKey,
                                          const ArtifactCore::ImageF32x4_RGBA& image);
    GPUTextureCacheHandle acquireOrCreate(const QString& ownerId,
                                          const QString& cacheKey,
                                          const ArtifactCore::GpuVideoFrame& frame);

    GPUTextureCacheHandle findExisting(const QString& ownerId,
                                       const QString& cacheKey) const;

    Diligent::ITextureView* textureView(const GPUTextureCacheHandle& handle) const;
    GPUTextureBindingRecord bindingRecord(const GPUTextureCacheHandle& handle) const;
    bool isValid(const GPUTextureCacheHandle& handle) const;
    void invalidate(const GPUTextureCacheHandle& handle);
    void invalidate(const GPUTextureCacheHandle& handle,
                    GPUTextureCacheInvalidationReason reason);
    void invalidateOwner(const QString& ownerId);
    void invalidateOwner(const QString& ownerId,
                         GPUTextureCacheInvalidationReason reason);
    void clear();

    GPUTextureCacheStats stats() const;
    GPUTextureOwnerStats ownerStats(const QString& ownerId) const;
    int ownerEntryCount(const QString& ownerId) const;
    size_t ownerMemoryBytes(const QString& ownerId) const;

private:
    struct Entry {
        quint64 id = 0;
        quint64 generation = 0;
        QString ownerId;
        QString cacheKey;
        QString fullKey;
        Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
        Diligent::RefCntAutoPtr<Diligent::ITextureView> srv;
        ArtifactCore::GpuVideoFrame sourceGpuFrame;
        size_t memoryBytes = 0;
        quint64 lastUsedTick = 0;
    };

    struct PendingUpload {
        quint64 ticketId = 0;
        quint64 ticketGeneration = 0;
        QString ownerId;
        QString cacheKey;
        QString fullKey;
        size_t memoryBytes = 0;
    };

    QString makeKey(const QString& ownerId, const QString& cacheKey) const;
    GPUTextureCacheHandle acquireOrCreateFromRgbaBytes(const QString& ownerId,
                                                       const QString& cacheKey,
                                                       Diligent::Uint32 width,
                                                       Diligent::Uint32 height,
                                                       Diligent::Uint64 stride,
                                                       const void* bytes,
                                                       size_t memoryBytes,
                                                       Diligent::TEXTURE_FORMAT format);
    void pruneLocked();
    void applyPendingD3D12TrimLocked();
    void processPendingUploadsLocked();
    void clearLocked();
    void eraseEntryByIdLocked(quint64 id);

    mutable QMutex mutex_;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    Diligent::TEXTURE_FORMAT textureFormat_ = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
    QHash<quint64, Entry> entries_;
    QHash<QString, quint64> keyToId_;
    QHash<QString, QSet<quint64>> ownerToIds_;
    QHash<quint64, PendingUpload> pendingUploads_;
    QHash<QString, quint64> pendingKeyToTicket_;
    DiligentUploadCoordinator* uploadCoordinator_ = nullptr;
    quint64 nextId_ = 1;
    quint64 generation_ = 1;
    quint64 usageTick_ = 1;
    size_t budgetBytes_ = 512ull * 1024ull * 1024ull;
    Diligent::Uint64 lastD3D12TrimGeneration_ = 0;
    int maxEntries_ = 256;
    size_t currentBytes_ = 0;
    size_t hitCount_ = 0;
    size_t missCount_ = 0;
    size_t invalidationCount_ = 0;
    GPUTextureCacheInvalidationReason lastInvalidationReason_ =
        GPUTextureCacheInvalidationReason::Explicit;
};

} // namespace Artifact
