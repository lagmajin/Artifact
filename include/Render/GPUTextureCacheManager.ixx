module;
#include <utility>

#include <memory>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
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

export namespace Artifact {

enum class GPUTextureBindingMode {
    LegacySRV,
    BindlessCandidate
};

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
};

class GPUTextureCacheManager {
public:
    GPUTextureCacheManager();
    ~GPUTextureCacheManager();

    void setDevice(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device,
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

    Diligent::ITextureView* textureView(const GPUTextureCacheHandle& handle) const;
    GPUTextureBindingRecord bindingRecord(const GPUTextureCacheHandle& handle) const;
    bool isValid(const GPUTextureCacheHandle& handle) const;
    void invalidate(const GPUTextureCacheHandle& handle);
    void invalidateOwner(const QString& ownerId);
    void clear();

    GPUTextureCacheStats stats() const;

private:
    struct Entry {
        quint64 id = 0;
        quint64 generation = 0;
        QString ownerId;
        QString cacheKey;
        Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
        Diligent::RefCntAutoPtr<Diligent::ITextureView> srv;
        size_t memoryBytes = 0;
        quint64 lastUsedTick = 0;
    };

    QString makeKey(const QString& ownerId, const QString& cacheKey) const;
    GPUTextureCacheHandle acquireOrCreateFromRgbaBytes(const QString& ownerId,
                                                       const QString& cacheKey,
                                                       Diligent::Uint32 width,
                                                       Diligent::Uint32 height,
                                                       Diligent::Uint64 stride,
                                                       const void* bytes,
                                                       size_t memoryBytes);
    void pruneLocked();
    void eraseEntryByIdLocked(quint64 id);

    mutable QMutex mutex_;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    Diligent::TEXTURE_FORMAT textureFormat_ = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
    QHash<quint64, Entry> entries_;
    QHash<QString, quint64> keyToId_;
    QHash<QString, QSet<quint64>> ownerToIds_;
    quint64 nextId_ = 1;
    quint64 generation_ = 1;
    quint64 usageTick_ = 1;
    size_t budgetBytes_ = 512ull * 1024ull * 1024ull;
    int maxEntries_ = 256;
    size_t currentBytes_ = 0;
    size_t hitCount_ = 0;
    size_t missCount_ = 0;
};

} // namespace Artifact
