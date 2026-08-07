#pragma once

#include <cstddef>
#include <cstdint>
#include <QByteArray>
#include <QString>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

namespace Artifact {

struct DiligentUploadTicket {
    std::uint64_t id = 0;
    std::uint64_t generation = 0;
    bool isValid() const { return id != 0; }
};

struct DiligentTextureUploadRequest {
    QString key;
    QByteArray bytes;
    Diligent::Uint32 width = 0;
    Diligent::Uint32 height = 0;
    Diligent::Uint64 stride = 0;
    Diligent::TEXTURE_FORMAT format = Diligent::TEX_FORMAT_UNKNOWN;
    std::uint64_t generation = 0;
};

struct DiligentTextureUploadResult {
    DiligentUploadTicket ticket;
    Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> srv;
    QString error;
    bool canceled = false;
    bool stale = false;

    bool succeeded() const
    {
        return !canceled && !stale && error.isEmpty() && texture && srv;
    }
};

struct DiligentUploadCoordinatorStats {
    std::uint64_t submitted = 0;
    std::uint64_t coalesced = 0;
    std::uint64_t rejected = 0;
    std::uint64_t completed = 0;
    std::uint64_t failed = 0;
    std::uint64_t canceled = 0;
    std::size_t pendingBytes = 0;
    std::size_t pendingJobs = 0;
    std::size_t completedJobs = 0;
    std::size_t gpuOperations = 0;
};

class DiligentUploadCoordinator final {
public:
    DiligentUploadCoordinator();
    ~DiligentUploadCoordinator();

    DiligentUploadCoordinator(const DiligentUploadCoordinator&) = delete;
    DiligentUploadCoordinator& operator=(const DiligentUploadCoordinator&) = delete;

    void setDevice(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device,
                   Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context);
    void clearDevice();
    void setPendingByteBudget(std::size_t bytes);
    void setMaxPendingJobs(std::size_t jobs);

    DiligentUploadTicket enqueue(const DiligentTextureUploadRequest& request);
    std::size_t processPending(std::size_t maxJobs = 8,
                               std::size_t maxBytes = 64ull * 1024ull * 1024ull);
    bool tryTakeResult(const DiligentUploadTicket& ticket,
                       DiligentTextureUploadResult& result);
    void cancel(const DiligentUploadTicket& ticket);
    void invalidateBeforeGeneration(std::uint64_t generation);
    void clear();

    DiligentUploadCoordinatorStats stats() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace Artifact
