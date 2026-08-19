#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>
#include <QHash>
#include <QString>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsTools/interface/GPUUploadManager.h>
#include "../../include/Render/DiligentUploadCoordinator.hpp"

namespace Artifact {

using namespace Diligent;

struct DiligentUploadCoordinator::Impl {
    struct Job {
        DiligentTextureUploadRequest request;
        DiligentUploadTicket ticket;
        bool canceled = false;
        std::size_t consumerCount = 1;
    };

    mutable std::mutex mutex;
    std::mutex uploadMutex;
    RefCntAutoPtr<IRenderDevice> device;
    RefCntAutoPtr<IDeviceContext> context;
    RefCntAutoPtr<IGPUUploadManager> uploadManager;
    std::deque<std::shared_ptr<Job>> pending;
    QHash<QString, std::shared_ptr<Job>> pendingByKey;
    QHash<std::uint64_t, DiligentTextureUploadResult> completed;
    QHash<std::uint64_t, QString> ticketKeys;
    DiligentUploadCoordinatorStats stats;
    std::atomic_uint64_t minimumGeneration{0};
    std::uint64_t nextTicketId = 1;
    std::size_t pendingByteBudget = 256ull * 1024ull * 1024ull;
    std::size_t maxPendingJobs = 128;

    void clearLocked()
    {
        for (auto& job : pending) {
            job->canceled = true;
        }
        pending.clear();
        pendingByKey.clear();
        completed.clear();
        ticketKeys.clear();
        stats.pendingBytes = 0;
        stats.pendingJobs = 0;
        stats.completedJobs = 0;
    }
};

DiligentUploadCoordinator::DiligentUploadCoordinator()
    : impl_(new Impl())
{
}

DiligentUploadCoordinator::~DiligentUploadCoordinator()
{
    delete impl_;
    impl_ = nullptr;
}

void DiligentUploadCoordinator::setDevice(
    RefCntAutoPtr<IRenderDevice> device,
    RefCntAutoPtr<IDeviceContext> context)
{
    if (!impl_) return;
    const std::scoped_lock lock(impl_->mutex);
    const std::scoped_lock uploadLock(impl_->uploadMutex);
    if (impl_->uploadManager && impl_->context) {
        impl_->uploadManager->Stop(impl_->context);
    }
    impl_->clearLocked();
    impl_->device = std::move(device);
    impl_->context = std::move(context);
    impl_->uploadManager.Release();
    if (impl_->device && impl_->context) {
        GPUUploadManagerCreateInfo createInfo;
        createInfo.pDevice = impl_->device;
        createInfo.pContext = impl_->context;
        CreateGPUUploadManager(createInfo, &impl_->uploadManager);
    }
    impl_->minimumGeneration.fetch_add(1, std::memory_order_acq_rel);
}

void DiligentUploadCoordinator::clearDevice()
{
    if (!impl_) return;
    const std::scoped_lock lock(impl_->mutex);
    const std::scoped_lock uploadLock(impl_->uploadMutex);
    impl_->clearLocked();
    if (impl_->uploadManager && impl_->context) {
        impl_->uploadManager->Stop(impl_->context);
    }
    impl_->uploadManager.Release();
    impl_->context.Release();
    impl_->device.Release();
    impl_->minimumGeneration.fetch_add(1, std::memory_order_acq_rel);
}

void DiligentUploadCoordinator::setPendingByteBudget(std::size_t bytes)
{
    if (!impl_) return;
    const std::scoped_lock lock(impl_->mutex);
    impl_->pendingByteBudget = std::max<std::size_t>(bytes, 1);
}

void DiligentUploadCoordinator::setMaxPendingJobs(std::size_t jobs)
{
    if (!impl_) return;
    const std::scoped_lock lock(impl_->mutex);
    impl_->maxPendingJobs = std::max<std::size_t>(jobs, 1);
}

DiligentUploadTicket DiligentUploadCoordinator::enqueue(
    const DiligentTextureUploadRequest& request)
{
    if (!impl_ || request.key.isEmpty() || request.bytes.isEmpty() ||
        request.width == 0 || request.height == 0 || request.stride == 0 ||
        request.format == TEX_FORMAT_UNKNOWN) {
        return {};
    }
    const std::scoped_lock lock(impl_->mutex);
    if (!impl_->device || !impl_->context || !impl_->uploadManager ||
        request.generation <
            impl_->minimumGeneration.load(std::memory_order_acquire)) {
        ++impl_->stats.rejected;
        return {};
    }
    const QString normalizedKey =
        request.key + QStringLiteral("|g:%1|f:%2")
                          .arg(request.generation)
                          .arg(static_cast<int>(request.format));
    const auto existing = impl_->pendingByKey.constFind(normalizedKey);
    if (existing != impl_->pendingByKey.cend()) {
        ++existing.value()->consumerCount;
        ++impl_->stats.coalesced;
        return existing.value()->ticket;
    }
    const std::size_t byteCount = static_cast<std::size_t>(request.bytes.size());
    if (impl_->stats.pendingJobs >= impl_->maxPendingJobs ||
        byteCount > impl_->pendingByteBudget -
            std::min(impl_->pendingByteBudget, impl_->stats.pendingBytes)) {
        ++impl_->stats.rejected;
        return {};
    }
    auto job = std::make_shared<Impl::Job>();
    job->request = request;
    job->ticket.id = impl_->nextTicketId++;
    job->ticket.generation = request.generation;
    impl_->pending.push_back(job);
    impl_->pendingByKey.insert(normalizedKey, job);
    impl_->ticketKeys.insert(job->ticket.id, normalizedKey);
    ++impl_->stats.submitted;
    ++impl_->stats.pendingJobs;
    impl_->stats.pendingBytes += byteCount;
    return job->ticket;
}

std::size_t DiligentUploadCoordinator::processPending(std::size_t maxJobs,
                                                      std::size_t maxBytes)
{
    if (!impl_ || maxJobs == 0 || maxBytes == 0) return 0;
    {
        RefCntAutoPtr<IDeviceContext> context;
        RefCntAutoPtr<IGPUUploadManager> uploadManager;
        {
            const std::scoped_lock lock(impl_->mutex);
            context = impl_->context;
            uploadManager = impl_->uploadManager;
        }
        if (!context || !uploadManager) return 0;
        const std::scoped_lock uploadLock(impl_->uploadMutex);
        uploadManager->RenderThreadUpdate(context);
    }
    std::size_t processed = 0;
    std::size_t processedBytes = 0;
    while (processed < maxJobs) {
        std::shared_ptr<Impl::Job> job;
        RefCntAutoPtr<IRenderDevice> device;
        RefCntAutoPtr<IDeviceContext> context;
        RefCntAutoPtr<IGPUUploadManager> uploadManager;
        {
            const std::scoped_lock lock(impl_->mutex);
            if (impl_->pending.empty() || !impl_->device ||
                !impl_->context || !impl_->uploadManager) break;
            const auto candidate = impl_->pending.front();
            const std::size_t candidateBytes =
                static_cast<std::size_t>(candidate->request.bytes.size());
            if (processed > 0 && candidateBytes > maxBytes -
                    std::min(maxBytes, processedBytes)) {
                break;
            }
            impl_->pending.pop_front();
            job = candidate;
            device = impl_->device;
            context = impl_->context;
            uploadManager = impl_->uploadManager;
            impl_->stats.pendingJobs -=
                std::min<std::size_t>(impl_->stats.pendingJobs, 1);
            impl_->stats.pendingBytes -= std::min(
                impl_->stats.pendingBytes, candidateBytes);
            processedBytes += candidateBytes;
        }

        DiligentTextureUploadResult result;
        result.ticket = job->ticket;
        const bool stale =
            job->request.generation <
            impl_->minimumGeneration.load(std::memory_order_acquire);
        result.canceled = job->canceled;
        result.stale = stale;
        if (!result.canceled && !result.stale) {
            TextureDesc desc;
            desc.Name = "Artifact.AsyncTextureUpload";
            desc.Type = RESOURCE_DIM_TEX_2D;
            desc.Width = job->request.width;
            desc.Height = job->request.height;
            desc.MipLevels = 1;
            desc.Format = job->request.format;
            desc.Usage = USAGE_DEFAULT;
            desc.BindFlags = BIND_SHADER_RESOURCE;
            desc.CPUAccessFlags = CPU_ACCESS_NONE;

            device->CreateTexture(desc, nullptr, &result.texture);
            if (result.texture) {
                ScheduleTextureUpdateInfo updateInfo;
                updateInfo.pContext = context;
                updateInfo.pDstTexture = result.texture;
                updateInfo.Format = job->request.format;
                updateInfo.DstBox = Box{0, job->request.width, 0,
                                        job->request.height, 0, 1};
                updateInfo.pSrcData = job->request.bytes.constData();
                updateInfo.Stride = job->request.stride;
                {
                    const std::scoped_lock uploadLock(impl_->uploadMutex);
                    if (!uploadManager->ScheduleTextureUpdate(updateInfo)) {
                        result.error = QStringLiteral(
                            "Diligent texture upload scheduling failed.");
                    } else {
                        uploadManager->RenderThreadUpdate(context);
                    }
                }
            }
            if (result.texture && result.error.isEmpty()) {
                result.srv = result.texture->GetDefaultView(
                    TEXTURE_VIEW_SHADER_RESOURCE);
            }
            if (!result.texture || !result.srv) {
                result.error = QStringLiteral("Diligent texture upload failed.");
            }
        }

        {
            const std::scoped_lock lock(impl_->mutex);
            const QString normalizedKey = impl_->ticketKeys.value(job->ticket.id);
            impl_->pendingByKey.remove(normalizedKey);
            impl_->completed.insert(job->ticket.id, std::move(result));
            ++impl_->stats.completedJobs;
            const auto& stored = impl_->completed[job->ticket.id];
            if (stored.canceled || stored.stale) ++impl_->stats.canceled;
            else if (!stored.error.isEmpty()) ++impl_->stats.failed;
            else ++impl_->stats.completed;
        }
        ++processed;
    }
    return processed;
}

bool DiligentUploadCoordinator::tryTakeResult(
    const DiligentUploadTicket& ticket,
    DiligentTextureUploadResult& result)
{
    if (!impl_ || !ticket.isValid()) return false;
    const std::scoped_lock lock(impl_->mutex);
    const auto it = impl_->completed.find(ticket.id);
    if (it == impl_->completed.end() ||
        it.value().ticket.generation != ticket.generation) {
        return false;
    }
    result = std::move(it.value());
    impl_->completed.erase(it);
    impl_->ticketKeys.remove(ticket.id);
    impl_->stats.completedJobs -=
        std::min<std::size_t>(impl_->stats.completedJobs, 1);
    return true;
}

void DiligentUploadCoordinator::cancel(const DiligentUploadTicket& ticket)
{
    if (!impl_ || !ticket.isValid()) return;
    const std::scoped_lock lock(impl_->mutex);
    for (auto& job : impl_->pending) {
        if (job->ticket.id == ticket.id &&
            job->ticket.generation == ticket.generation) {
            job->canceled = true;
            return;
        }
    }
}

void DiligentUploadCoordinator::invalidateBeforeGeneration(
    std::uint64_t generation)
{
    if (!impl_) return;
    auto& minimum = impl_->minimumGeneration;
    std::uint64_t current = minimum.load(std::memory_order_acquire);
    while (current < generation &&
           !minimum.compare_exchange_weak(current, generation,
                                          std::memory_order_acq_rel)) {
    }
    const std::scoped_lock lock(impl_->mutex);
    for (auto& job : impl_->pending) {
        if (job->request.generation < generation) job->canceled = true;
    }
}

void DiligentUploadCoordinator::clear()
{
    if (!impl_) return;
    const std::scoped_lock lock(impl_->mutex);
    impl_->clearLocked();
}

DiligentUploadCoordinatorStats DiligentUploadCoordinator::stats() const
{
    if (!impl_) return {};
    const std::scoped_lock lock(impl_->mutex);
    auto stats = impl_->stats;
    if (impl_->uploadManager) {
        GPUUploadManagerStats uploadStats;
        const std::scoped_lock uploadLock(impl_->uploadMutex);
        impl_->uploadManager->GetStats(uploadStats);
        stats.gpuOperations = uploadStats.NumInFlightPages;
    }
    return stats;
}

} // namespace Artifact
