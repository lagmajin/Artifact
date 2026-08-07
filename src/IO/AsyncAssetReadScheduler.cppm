module;
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QString>
#include <QThreadPool>

module Artifact.IO.AsyncAssetReadScheduler;

import Artifact.IO.DirectStorageReader;

namespace Artifact {

namespace {

std::size_t saturatingAdd(std::size_t lhs, std::size_t rhs)
{
    if (rhs > std::numeric_limits<std::size_t>::max() - lhs) {
        return std::numeric_limits<std::size_t>::max();
    }
    return lhs + rhs;
}

std::size_t estimatedRequestBytes(const AsyncAssetReadRequest& request)
{
    if (request.size > 0) {
        return request.size;
    }
    const qint64 fileSize = QFileInfo(request.path).size();
    if (fileSize <= 0 ||
        request.offset >= static_cast<std::uint64_t>(fileSize)) {
        return 0;
    }
    const std::uint64_t remaining =
        static_cast<std::uint64_t>(fileSize) - request.offset;
    return remaining > std::numeric_limits<std::size_t>::max()
        ? std::numeric_limits<std::size_t>::max()
        : static_cast<std::size_t>(remaining);
}

QString normalizedRequestKey(const AsyncAssetReadRequest& request)
{
    if (!request.key.trimmed().isEmpty()) {
        return request.key.trimmed() + QStringLiteral("|g:%1|r:%2|o:%3|s:%4")
            .arg(request.generation)
            .arg(request.sourceRevision)
            .arg(request.offset)
            .arg(static_cast<qulonglong>(request.size));
    }
    return QFileInfo(request.path).absoluteFilePath() +
        QStringLiteral("|g:%1|r:%2|o:%3|s:%4")
            .arg(request.generation)
            .arg(request.sourceRevision)
            .arg(request.offset)
            .arg(static_cast<qulonglong>(request.size));
}

} // namespace

struct AsyncAssetReadScheduler::Impl {
    struct Job {
        AsyncAssetReadRequest request;
        AsyncAssetReadTicket ticket;
        QString normalizedKey;
        std::size_t estimatedBytes = 0;
        bool inFlight = false;
        bool canceled = false;
        bool complete = false;
        std::size_t consumerCount = 1;
        AsyncAssetReadResult result;
    };

    struct SharedState {
        mutable std::mutex mutex;
        std::condition_variable completionCondition;
        QHash<std::uint64_t, std::shared_ptr<Job>> jobs;
        QHash<QString, std::uint64_t> keyToTicket;
        DirectStorageReader reader;
        AsyncAssetReadSchedulerStats stats;
        std::atomic_uint64_t minimumGeneration{0};
        std::uint64_t nextTicketId = 1;
        std::size_t queuedByteBudget = 512ull * 1024ull * 1024ull;
        std::size_t completedByteBudget = 512ull * 1024ull * 1024ull;
        std::size_t maxQueuedJobs = 256;
        bool stopping = false;
    };

    QThreadPool pool;
    std::shared_ptr<SharedState> state = std::make_shared<SharedState>();

    explicit Impl(int workerCount)
    {
        pool.setObjectName(QStringLiteral("ArtifactAsyncAssetReadPool"));
        pool.setMaxThreadCount(std::clamp(workerCount, 1, 8));
        pool.setExpiryTimeout(30000);
    }

    ~Impl()
    {
        {
            const std::scoped_lock lock(state->mutex);
            state->stopping = true;
            for (auto it = state->jobs.begin(); it != state->jobs.end(); ++it) {
                it.value()->canceled = true;
            }
            state->completionCondition.notify_all();
        }
        pool.clear();
        pool.waitForDone();
    }

    void pruneCompletedLocked()
    {
        if (state->stats.completedBytes <= state->completedByteBudget) {
            return;
        }
        QList<std::uint64_t> completedIds;
        completedIds.reserve(state->jobs.size());
        for (auto it = state->jobs.cbegin(); it != state->jobs.cend(); ++it) {
            if (it.value()->complete) completedIds.push_back(it.key());
        }
        std::sort(completedIds.begin(), completedIds.end());
        for (const std::uint64_t id : completedIds) {
            if (state->stats.completedBytes <= state->completedByteBudget) break;
            const auto it = state->jobs.find(id);
            if (it == state->jobs.end() || !it.value()->complete) continue;
            const auto job = it.value();
            state->stats.completedBytes -= std::min(
                state->stats.completedBytes,
                static_cast<std::size_t>(job->result.bytes.size()));
            state->stats.completedJobs -=
                std::min<std::size_t>(state->stats.completedJobs, 1);
            state->keyToTicket.remove(job->normalizedKey);
            state->jobs.erase(it);
        }
    }
};

AsyncAssetReadScheduler::AsyncAssetReadScheduler(int workerCount)
    : impl_(new Impl(workerCount))
{
}

AsyncAssetReadScheduler::~AsyncAssetReadScheduler()
{
    delete impl_;
    impl_ = nullptr;
}

void AsyncAssetReadScheduler::setQueuedByteBudget(std::size_t bytes)
{
    if (!impl_) return;
    const std::scoped_lock lock(impl_->state->mutex);
    impl_->state->queuedByteBudget = std::max<std::size_t>(bytes, 1);
}

void AsyncAssetReadScheduler::setCompletedByteBudget(std::size_t bytes)
{
    if (!impl_) return;
    const std::scoped_lock lock(impl_->state->mutex);
    impl_->state->completedByteBudget = std::max<std::size_t>(bytes, 1);
    impl_->pruneCompletedLocked();
}

void AsyncAssetReadScheduler::setMaxQueuedJobs(std::size_t jobs)
{
    if (!impl_) return;
    const std::scoped_lock lock(impl_->state->mutex);
    impl_->state->maxQueuedJobs = std::max<std::size_t>(jobs, 1);
}

AsyncAssetReadTicket AsyncAssetReadScheduler::enqueue(
    const AsyncAssetReadRequest& request)
{
    if (!impl_ || request.path.trimmed().isEmpty()) return {};
    const auto state = impl_->state;
    const QString key = normalizedRequestKey(request);
    const std::size_t estimatedBytes = estimatedRequestBytes(request);
    std::shared_ptr<Impl::Job> job;
    {
        const std::scoped_lock lock(state->mutex);
        if (state->stopping ||
            request.generation <
                state->minimumGeneration.load(std::memory_order_acquire)) {
            ++state->stats.rejected;
            return {};
        }
        const auto existing = state->keyToTicket.constFind(key);
        if (existing != state->keyToTicket.cend()) {
            const auto existingJob = state->jobs.constFind(existing.value());
            if (existingJob != state->jobs.cend()) {
                ++existingJob.value()->consumerCount;
                ++state->stats.coalesced;
                return existingJob.value()->ticket;
            }
        }
        if (state->stats.queuedJobs + state->stats.inFlightJobs >=
                state->maxQueuedJobs ||
            saturatingAdd(state->stats.queuedBytes, estimatedBytes) >
                state->queuedByteBudget) {
            ++state->stats.rejected;
            return {};
        }

        job = std::make_shared<Impl::Job>();
        job->request = request;
        job->request.path = QFileInfo(request.path).absoluteFilePath();
        job->ticket.id = state->nextTicketId++;
        job->ticket.generation = request.generation;
        job->normalizedKey = key;
        job->estimatedBytes = estimatedBytes;
        state->jobs.insert(job->ticket.id, job);
        state->keyToTicket.insert(key, job->ticket.id);
        ++state->stats.submitted;
        ++state->stats.queuedJobs;
        state->stats.queuedBytes =
            saturatingAdd(state->stats.queuedBytes, estimatedBytes);
    }

    impl_->pool.start(
        [state, job]() {
            {
                const std::scoped_lock lock(state->mutex);
                if (state->stopping || job->canceled) {
                    job->canceled = true;
                } else {
                    job->inFlight = true;
                    state->stats.queuedJobs -=
                        std::min<std::size_t>(state->stats.queuedJobs, 1);
                    state->stats.queuedBytes -= std::min(
                        state->stats.queuedBytes, job->estimatedBytes);
                    ++state->stats.inFlightJobs;
                }
            }

            DirectStorageReadResult readResult;
            if (!job->canceled) {
                readResult = state->reader.readFile(
                    job->request.path, job->request.offset, job->request.size);
            }

            const std::scoped_lock lock(state->mutex);
            if (job->inFlight) {
                state->stats.inFlightJobs -=
                    std::min<std::size_t>(state->stats.inFlightJobs, 1);
            } else {
                state->stats.queuedJobs -=
                    std::min<std::size_t>(state->stats.queuedJobs, 1);
                state->stats.queuedBytes -= std::min(
                    state->stats.queuedBytes, job->estimatedBytes);
            }
            job->result.ticket = job->ticket;
            job->result.sourceRevision = job->request.sourceRevision;
            job->result.canceled = state->stopping || job->canceled;
            job->result.stale =
                job->request.generation <
                state->minimumGeneration.load(std::memory_order_acquire);
            if (!job->result.canceled && !job->result.stale) {
                job->result.bytes = std::move(readResult.bytes);
                job->result.error = std::move(readResult.error);
                job->result.usedDirectStorage = readResult.usedDirectStorage;
            }
            job->complete = true;
            ++state->stats.completedJobs;
            state->stats.completedBytes = saturatingAdd(
                state->stats.completedBytes,
                static_cast<std::size_t>(job->result.bytes.size()));
            if (job->result.canceled) ++state->stats.canceled;
            else if (job->result.stale) ++state->stats.stale;
            else if (!job->result.error.isEmpty()) ++state->stats.failed;
            else ++state->stats.completed;

            if (state->stats.completedBytes > state->completedByteBudget) {
                QList<std::uint64_t> completedIds;
                for (auto it = state->jobs.cbegin(); it != state->jobs.cend(); ++it) {
                    if (it.value()->complete) completedIds.push_back(it.key());
                }
                std::sort(completedIds.begin(), completedIds.end());
                for (const std::uint64_t id : completedIds) {
                    if (state->stats.completedBytes <=
                        state->completedByteBudget) break;
                    const auto completedIt = state->jobs.find(id);
                    if (completedIt == state->jobs.end() ||
                        !completedIt.value()->complete) continue;
                    const auto completedJob = completedIt.value();
                    state->stats.completedBytes -= std::min(
                        state->stats.completedBytes,
                        static_cast<std::size_t>(
                            completedJob->result.bytes.size()));
                    state->stats.completedJobs -=
                        std::min<std::size_t>(state->stats.completedJobs, 1);
                    state->keyToTicket.remove(completedJob->normalizedKey);
                    state->jobs.erase(completedIt);
                }
            }
            state->completionCondition.notify_all();
        },
        static_cast<int>(request.priority));
    return job->ticket;
}

bool AsyncAssetReadScheduler::tryGetResult(
    const AsyncAssetReadTicket& ticket,
    AsyncAssetReadResult& result) const
{
    if (!impl_ || !ticket.isValid()) return false;
    const std::scoped_lock lock(impl_->state->mutex);
    const auto it = impl_->state->jobs.constFind(ticket.id);
    if (it == impl_->state->jobs.cend() ||
        it.value()->ticket.generation != ticket.generation ||
        !it.value()->complete) {
        return false;
    }
    result = it.value()->result;
    return true;
}

bool AsyncAssetReadScheduler::waitForResult(
    const AsyncAssetReadTicket& ticket,
    AsyncAssetReadResult& result,
    int timeoutMs) const
{
    if (!impl_ || !ticket.isValid()) return false;
    auto state = impl_->state;
    std::unique_lock lock(state->mutex);
    const auto ready = [&]() {
        const auto it = state->jobs.constFind(ticket.id);
        return state->stopping || it == state->jobs.cend() ||
            (it.value()->ticket.generation == ticket.generation &&
             it.value()->complete);
    };
    bool signaled = false;
    if (timeoutMs < 0) {
        state->completionCondition.wait(lock, ready);
        signaled = true;
    } else {
        signaled = state->completionCondition.wait_for(
            lock, std::chrono::milliseconds(timeoutMs), ready);
    }
    if (!signaled) return false;
    const auto it = state->jobs.constFind(ticket.id);
    if (it == state->jobs.cend() ||
        it.value()->ticket.generation != ticket.generation ||
        !it.value()->complete) {
        return false;
    }
    result = it.value()->result;
    return true;
}

void AsyncAssetReadScheduler::release(const AsyncAssetReadTicket& ticket)
{
    if (!impl_ || !ticket.isValid()) return;
    const std::scoped_lock lock(impl_->state->mutex);
    const auto it = impl_->state->jobs.find(ticket.id);
    if (it == impl_->state->jobs.end() ||
        it.value()->ticket.generation != ticket.generation ||
        !it.value()->complete) {
        return;
    }
    const auto job = it.value();
    if (job->consumerCount > 1) {
        --job->consumerCount;
        return;
    }
    impl_->state->stats.completedBytes -= std::min(
        impl_->state->stats.completedBytes,
        static_cast<std::size_t>(job->result.bytes.size()));
    impl_->state->stats.completedJobs -=
        std::min<std::size_t>(impl_->state->stats.completedJobs, 1);
    impl_->state->keyToTicket.remove(job->normalizedKey);
    impl_->state->jobs.erase(it);
}

void AsyncAssetReadScheduler::cancel(const AsyncAssetReadTicket& ticket)
{
    if (!impl_ || !ticket.isValid()) return;
    const std::scoped_lock lock(impl_->state->mutex);
    const auto it = impl_->state->jobs.find(ticket.id);
    if (it != impl_->state->jobs.end() &&
        it.value()->ticket.generation == ticket.generation &&
        !it.value()->complete) {
        it.value()->canceled = true;
    }
}

void AsyncAssetReadScheduler::invalidateBeforeGeneration(
    std::uint64_t generation)
{
    if (!impl_) return;
    auto& minimum = impl_->state->minimumGeneration;
    std::uint64_t current = minimum.load(std::memory_order_acquire);
    while (current < generation &&
           !minimum.compare_exchange_weak(current, generation,
                                          std::memory_order_acq_rel)) {
    }
    const std::scoped_lock lock(impl_->state->mutex);
    for (auto it = impl_->state->jobs.begin();
         it != impl_->state->jobs.end(); ++it) {
        if (!it.value()->complete &&
            it.value()->request.generation < generation) {
            it.value()->canceled = true;
        }
    }
}

void AsyncAssetReadScheduler::clearCompleted()
{
    if (!impl_) return;
    const std::scoped_lock lock(impl_->state->mutex);
    QList<std::uint64_t> ids;
    for (auto it = impl_->state->jobs.cbegin();
         it != impl_->state->jobs.cend(); ++it) {
        if (it.value()->complete) ids.push_back(it.key());
    }
    for (const auto id : ids) {
        const auto it = impl_->state->jobs.find(id);
        if (it == impl_->state->jobs.end()) continue;
        impl_->state->keyToTicket.remove(it.value()->normalizedKey);
        impl_->state->jobs.erase(it);
    }
    impl_->state->stats.completedBytes = 0;
    impl_->state->stats.completedJobs = 0;
}

AsyncAssetReadSchedulerStats AsyncAssetReadScheduler::stats() const
{
    if (!impl_) return {};
    const std::scoped_lock lock(impl_->state->mutex);
    return impl_->state->stats;
}

QString AsyncAssetReadScheduler::backendDescription() const
{
    if (!impl_) return QStringLiteral("uninitialized");
    return impl_->state->reader.backendDescription();
}

} // namespace Artifact
