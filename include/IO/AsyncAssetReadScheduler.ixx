module;
#include <cstddef>
#include <cstdint>
#include <QByteArray>
#include <QString>

export module Artifact.IO.AsyncAssetReadScheduler;

export namespace Artifact {

enum class AsyncAssetReadPriority : int {
    BackgroundWarm = -2,
    PreviewBuild = -1,
    PlaybackNext = 1,
    Interactive = 2,
};

struct AsyncAssetReadTicket {
    std::uint64_t id = 0;
    std::uint64_t generation = 0;

    bool isValid() const { return id != 0; }
};

struct AsyncAssetReadRequest {
    QString key;
    QString path;
    std::uint64_t offset = 0;
    std::size_t size = 0;
    std::uint64_t generation = 0;
    std::uint64_t sourceRevision = 0;
    AsyncAssetReadPriority priority = AsyncAssetReadPriority::BackgroundWarm;
};

struct AsyncAssetReadResult {
    AsyncAssetReadTicket ticket;
    QByteArray bytes;
    QString error;
    std::uint64_t sourceRevision = 0;
    bool usedDirectStorage = false;
    bool canceled = false;
    bool stale = false;

    bool succeeded() const
    {
        return error.isEmpty() && !canceled && !stale;
    }
};

struct AsyncAssetReadSchedulerStats {
    std::uint64_t submitted = 0;
    std::uint64_t coalesced = 0;
    std::uint64_t rejected = 0;
    std::uint64_t completed = 0;
    std::uint64_t canceled = 0;
    std::uint64_t stale = 0;
    std::uint64_t failed = 0;
    std::size_t queuedBytes = 0;
    std::size_t completedBytes = 0;
    std::size_t queuedJobs = 0;
    std::size_t inFlightJobs = 0;
    std::size_t completedJobs = 0;
};

class AsyncAssetReadScheduler final {
public:
    explicit AsyncAssetReadScheduler(int workerCount = 3);
    ~AsyncAssetReadScheduler();

    AsyncAssetReadScheduler(const AsyncAssetReadScheduler&) = delete;
    AsyncAssetReadScheduler& operator=(const AsyncAssetReadScheduler&) = delete;
    AsyncAssetReadScheduler(AsyncAssetReadScheduler&&) = delete;
    AsyncAssetReadScheduler& operator=(AsyncAssetReadScheduler&&) = delete;

    void setQueuedByteBudget(std::size_t bytes);
    void setCompletedByteBudget(std::size_t bytes);
    void setMaxQueuedJobs(std::size_t jobs);

    AsyncAssetReadTicket enqueue(const AsyncAssetReadRequest& request);
    bool tryGetResult(const AsyncAssetReadTicket& ticket,
                      AsyncAssetReadResult& result) const;
    bool waitForResult(const AsyncAssetReadTicket& ticket,
                       AsyncAssetReadResult& result,
                       int timeoutMs = -1) const;
    void release(const AsyncAssetReadTicket& ticket);
    void cancel(const AsyncAssetReadTicket& ticket);
    void invalidateBeforeGeneration(std::uint64_t generation);
    void clearCompleted();

    AsyncAssetReadSchedulerStats stats() const;
    QString backendDescription() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace Artifact
