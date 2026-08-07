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
#include <DiligentCore/Graphics/GraphicsTools/interface/TextureUploader.hpp>
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
    RefCntAutoPtr<IRenderDevice> device;
    RefCntAutoPtr<IDeviceContext> context;
    RefCntAutoPtr<ITextureUploader> uploader;
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
    impl_->clearLocked();
    impl_->device = std::move(device);
    impl_->context = std::move(context);
    impl_->uploader.Release();
    if (impl_->device && impl_->context) {
        TextureUploaderDesc desc;
        desc.Mode = TEXTURE_UPLOADER_MODE_STAGING_RESOURCE;
        CreateTextureUploader(impl_->device, desc, &impl_->uploader);
    }
    impl_->minimumGeneration.fetch_add(1, std::memory_order_acq_rel);
}

void DiligentUploadCoordinator::clearDevice()
{
    if (!impl_) return;
    const std::scoped_lock lock(impl_->mutex);
    impl_->clearLocked();
    impl_->uploader.Release();
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
    if (!impl_->device || !impl_->context || !impl_->uploader ||
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
        RefCntAutoPtr<ITextureUploader> uploader;
        {
            const std::scoped_lock lock(impl_->mutex);
            context = impl_->context;
            uploader = impl_->uploader;
        }
        if (!context || !uploader) return 0;
        uploader->RenderThreadUpdate(context);
    }
    std::size_t processed = 0;
    std::size_t processedBytes = 0;
    while (processed < maxJobs) {
        std::shared_ptr<Impl::Job> job;
        RefCntAutoPtr<IRenderDevice> device;
        RefCntAutoPtr<IDeviceContext> context;
        RefCntAutoPtr<ITextureUploader> uploader;
        {
            const std::scoped_lock lock(impl_->mutex);
            if (impl_->pending.empty() || !impl_->device ||
                !impl_->context || !impl_->uploader) break;
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
            uploader = impl_->uploader;
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
            RefCntAutoPtr<IUploadBuffer> uploadBuffer;
            if (result.texture) {
                UploadBufferDesc uploadDesc;
                uploadDesc.Width = job->request.width;
                uploadDesc.Height = job->request.height;
                uploadDesc.Format = job->request.format;
                uploader->AllocateUploadBuffer(
                    context, uploadDesc, &uploadBuffer);
            }
            if (result.texture && uploadBuffer) {
                const MappedTextureSubresource mapped =
                    uploadBuffer->GetMappedData(0, 0);
                const std::size_t sourceStride =
                    static_cast<std::size_t>(job->request.stride);
                const std::size_t destinationStride =
                    static_cast<std::size_t>(mapped.Stride);
                const std::size_t requiredBytes = sourceStride *
                    static_cast<std::size_t>(job->request.height);
                if (!mapped.pData || sourceStride > destinationStride ||
                    requiredBytes > static_cast<std::size_t>(
                        job->request.bytes.size())) {
                    result.error = QStringLiteral(
                        "Invalid Diligent upload buffer layout.");
                } else {
                    const auto* source = reinterpret_cast<const std::byte*>(
                        job->request.bytes.constData());
                    auto* destination = static_cast<std::byte*>(mapped.pData);
                    for (Uint32 row = 0; row < job->request.height; ++row) {
                        std::memcpy(destination + destinationStride * row,
                                    source + sourceStride * row,
                                    sourceStride);
                    }
                    uploader->ScheduleGPUCopy(
                        context, result.texture, 0, 0, uploadBuffer, true);
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
    if (impl_->uploader) {
        stats.gpuOperations = impl_->uploader->GetStats().NumPendingOperations;
    }
    return stats;
}

} // namespace Artifact
