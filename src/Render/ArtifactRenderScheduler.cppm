
module;
#include <QThreadPool>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QElapsedTimer>
#include <QDebug>
#include <wobjectimpl.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Render.Scheduler;





import Artifact.Render.Scheduler;
import Thread.Helper;
import Frame.Position;
import Frame.Range;

namespace Artifact {

namespace {
std::atomic<bool> g_renderSchedulerStartupWarmupComplete{false};
}

void setRenderSchedulerStartupWarmupComplete(bool ready) {
    g_renderSchedulerStartupWarmupComplete.store(ready, std::memory_order_release);
}

bool isRenderSchedulerStartupWarmupComplete() {
    return g_renderSchedulerStartupWarmupComplete.load(std::memory_order_acquire);
}

W_OBJECT_IMPL(RenderTask)
W_OBJECT_IMPL(RenderScheduler)
W_OBJECT_IMPL(BatchRenderer)

// ==================== RenderTask Implementation ====================

RenderTask::RenderTask(const FrameRange& range, 
                      TaskPriority priority,
                      QObject* parent)
    : QObject(parent)
    , range_(range)
    , priority_(priority)
{
}

RenderTask::~RenderTask() = default;

// ==================== RenderScheduler::Impl ====================

class RenderScheduler::Impl {
public:
    std::unique_ptr<QThreadPool> threadPool_;
    RenderScheduler* owner_ = nullptr;
    ParallelStrategy strategy_ = ParallelStrategy::Adaptive;
    bool adaptiveEnabled_ = true;
    int requestedThreadCount_ = 1;
    bool requestedThreadCountExplicit_ = false;
    
    // Task queues
    std::vector<RenderTask*> pendingTasks_;
    std::vector<RenderTask*> activeTasks_;
    std::vector<RenderTask*> completedTasks_;
    
    // Thread safety
    mutable QMutex mutex_;
    
    // Execution state
    std::atomic<bool> executing_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> stopRequested_{false};
    
    // Progress
    std::atomic<int> completedCount_{0};
    std::atomic<int> totalCount_{0};
    
    // Statistics
    QElapsedTimer executionTimer_;
    std::deque<double> frameTimes_;
    FramePosition currentFrame_;
    std::array<int, 5> dropCounts_ {0,0,0,0,0};
    double frameBudgetMs_ = 16.67;
    
    // Callbacks
    std::function<void(RenderTask*)> taskExecutor_;

    int defaultThreadCount() const {
        if (!isRenderSchedulerStartupWarmupComplete()) {
            return 1;
        }
        const int ideal = QThread::idealThreadCount();
        if (ideal <= 1) {
            return 1;
        }
        return std::max(1, ideal - 1);
    }

    QThreadPool* ensureThreadPool() {
        const bool explicitThreadCount = requestedThreadCountExplicit_;
        const int desiredThreadCount =
            explicitThreadCount ? requestedThreadCount_ : defaultThreadCount();
        if (!threadPool_) {
            threadPool_ = std::make_unique<QThreadPool>();
            threadPool_->setObjectName(QStringLiteral("ArtifactRenderSchedulerPool"));
            threadPool_->setMaxThreadCount(desiredThreadCount);
            requestedThreadCount_ = desiredThreadCount;
        } else if (!explicitThreadCount &&
                   threadPool_->maxThreadCount() != desiredThreadCount) {
            threadPool_->setMaxThreadCount(desiredThreadCount);
            requestedThreadCount_ = desiredThreadCount;
        }
        return threadPool_.get();
    }

    // Main-thread slots for signal emission (called via QMetaObject::invokeMethod)
    void onTaskCompleted(RenderTask* task, double elapsedMs) {
        Q_UNUSED(elapsedMs);
        if (!owner_) return;
        if (!task->isCancelled()) {
            emit owner_->taskCompleted(task);
            emit owner_->frameProcessed(task->range().startPosition());
        }
        // Update progress
        const float progress = static_cast<float>(completedCount_) / totalCount_;
        emit owner_->progressChanged(progress);
    }

    void onTaskFailed(RenderTask* task, const QString& error) {
        if (!owner_) return;
        dropCounts_[static_cast<int>(FrameDropReason::ExecutionError)]++;
        emit owner_->frameDropped(
            FrameDropReason::ExecutionError,
            dropCounts_[static_cast<int>(FrameDropReason::ExecutionError)]);
        emit owner_->taskFailed(task, error);
    }

    void onOverBudget(double elapsedMs) {
        if (!owner_) return;
        dropCounts_[static_cast<int>(FrameDropReason::OverBudget)]++;
        emit owner_->frameDropped(
            FrameDropReason::OverBudget,
            dropCounts_[static_cast<int>(FrameDropReason::OverBudget)]);
        emit owner_->performanceWarning(
            QString("Frame budget exceeded: %1 ms").arg(elapsedMs, 0, 'f', 2));
    }
    
    Impl()
        : requestedThreadCount_(1)
    {
    }

    ~Impl() = default;
    
    bool executeTask(RenderTask* task, QString* outError) {
        if (task->isCancelled() || stopRequested_) {
            return false;
        }
        
        // Execute the task
        if (taskExecutor_) {
            try {
                taskExecutor_(task);
            } catch (const std::exception& e) {
                emit task->failed(QString::fromUtf8(e.what()));
                if (outError) *outError = QString::fromUtf8(e.what());
                return false;
            }
        }
        
        // Mark complete
        completedCount_++;
        return true;
    }
};

RenderScheduler::RenderScheduler(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
    impl_->owner_ = this;
}

RenderScheduler::~RenderScheduler() = default;

void RenderScheduler::setThreadCount(int count) {
    impl_->requestedThreadCount_ = std::max(1, count);
    impl_->requestedThreadCountExplicit_ = true;
    if (auto* pool = impl_->ensureThreadPool()) {
        pool->setMaxThreadCount(impl_->requestedThreadCount_);
    }
}

int RenderScheduler::threadCount() const {
    if (impl_->threadPool_) {
        return impl_->threadPool_->maxThreadCount();
    }
    if (!impl_->requestedThreadCountExplicit_) {
        return isRenderSchedulerStartupWarmupComplete()
                   ? std::max(1, QThread::idealThreadCount() - 1)
                   : 1;
    }
    return impl_->requestedThreadCount_;
}

void RenderScheduler::setParallelStrategy(ParallelStrategy strategy) {
    impl_->strategy_ = strategy;
}

ParallelStrategy RenderScheduler::parallelStrategy() const {
    return impl_->strategy_;
}

void RenderScheduler::setAdaptiveEnabled(bool enabled) {
    impl_->adaptiveEnabled_ = enabled;
}

bool RenderScheduler::isAdaptiveEnabled() const {
    return impl_->adaptiveEnabled_;
}

void RenderScheduler::submitTask(RenderTask* task) {
    QMutexLocker locker(&impl_->mutex_);
    impl_->pendingTasks_.push_back(task);
    impl_->totalCount_++;
    
    // Sort by priority
    std::sort(impl_->pendingTasks_.begin(), impl_->pendingTasks_.end(),
        [](RenderTask* a, RenderTask* b) {
            return (int)a->priority() < (int)b->priority();
        });
}

void RenderScheduler::submitTasks(const QVector<RenderTask*>& tasks) {
    for (RenderTask* task : tasks) {
        submitTask(task);
    }
}

void RenderScheduler::cancelTask(RenderTask* task) {
    task->cancel();
    
    QMutexLocker locker(&impl_->mutex_);
    
    // Remove from pending
    auto it = std::find(impl_->pendingTasks_.begin(), impl_->pendingTasks_.end(), task);
    if (it != impl_->pendingTasks_.end()) {
        impl_->pendingTasks_.erase(it);
    }
}

void RenderScheduler::cancelAllTasks() {
    impl_->stopRequested_ = true;
    
    QMutexLocker locker(&impl_->mutex_);
    
    for (auto* task : impl_->pendingTasks_) {
        task->cancel();
    }
    for (auto* task : impl_->activeTasks_) {
        task->cancel();
    }
    
    impl_->pendingTasks_.clear();
    impl_->activeTasks_.clear();
}

void RenderScheduler::pauseTask(RenderTask* task) {
    // Implementation would suspend task execution
    Q_UNUSED(task);
}

void RenderScheduler::resumeTask(RenderTask* task) {
    // Implementation would resume task execution
    Q_UNUSED(task);
}

void RenderScheduler::setTaskPriority(RenderTask* task, TaskPriority priority) {
    QMutexLocker locker(&impl_->mutex_);
    
    auto it = std::find(impl_->pendingTasks_.begin(), impl_->pendingTasks_.end(), task);
    if (it != impl_->pendingTasks_.end()) {
        // Re-sort after priority change
    }
}

void RenderScheduler::startExecution() {
    if (impl_->executing_) return;

    if (!impl_->ensureThreadPool()) {
        return;
    }

    impl_->executing_ = true;
    impl_->paused_ = false;
    impl_->stopRequested_ = false;
    impl_->executionTimer_.start();

    emit executionStarted();

    // Start processing tasks in parallel using QThreadPool
    processNextTask();
}

void RenderScheduler::pauseExecution() {
    impl_->paused_ = true;
    emit executionPaused();
}

void RenderScheduler::stopExecution() {
    impl_->stopRequested_ = true;
    impl_->executing_ = false;

    cancelAllTasks();

    emit executionStopped();
}

void RenderScheduler::processNextTask() {
    // Dispatch tasks to thread pool (parallel execution)
    auto* threadPool = impl_->ensureThreadPool();
    if (!threadPool) {
        return;
    }
    const int maxConcurrent = threadPool->maxThreadCount();

    while (impl_->executing_ && !impl_->paused_ && !impl_->stopRequested_) {
        RenderTask* task = nullptr;

        {
            QMutexLocker locker(&impl_->mutex_);

            // Check if we can dispatch more tasks
            const int currentActive = static_cast<int>(impl_->activeTasks_.size());
            if (currentActive >= maxConcurrent) {
                return;  // Wait for tasks to complete
            }

            if (impl_->pendingTasks_.empty()) {
                // No more tasks - check if we're done
                if (impl_->activeTasks_.empty() && impl_->completedCount_ >= impl_->totalCount_) {
                    impl_->executing_ = false;
                    emit executionCompleted();
                }
                return;
            }

            task = impl_->pendingTasks_.front();
            impl_->pendingTasks_.erase(impl_->pendingTasks_.begin());
            impl_->activeTasks_.push_back(task);
        }

        if (!task) {
            break;
        }

        if (task->isCancelled()) {
            // Skip cancelled tasks
            QMutexLocker locker(&impl_->mutex_);
            auto it = std::find(impl_->activeTasks_.begin(), impl_->activeTasks_.end(), task);
            if (it != impl_->activeTasks_.end()) {
                impl_->activeTasks_.erase(it);
            }
            impl_->dropCounts_[static_cast<int>(FrameDropReason::Cancelled)]++;
            emit frameDropped(FrameDropReason::Cancelled,
                              impl_->dropCounts_[static_cast<int>(FrameDropReason::Cancelled)]);
            impl_->completedCount_++;
            continue;
        }

        emit taskStarted(task);

        // Run task in thread pool
        auto self = this;
        const QString taskName = !task->description().trimmed().isEmpty()
                                     ? task->description().trimmed()
                                     : QStringLiteral("frames:%1-%2")
                                            .arg(task->range().start())
                                            .arg(task->range().end());
        threadPool->start([self, task, taskName]() {
            if (!self || self->impl_->stopRequested_) {
                return;
            }

            const QString threadName =
                QStringLiteral("RenderScheduler/%1").arg(taskName);
            ArtifactCore::ScopedThreadName threadScope(threadName);
            const auto startNs = std::chrono::steady_clock::now().time_since_epoch().count();
            QString execError;
            const bool ok = self->impl_->executeTask(task, &execError);
            const auto endNs = std::chrono::steady_clock::now().time_since_epoch().count();
            const double elapsedMs = (endNs - startNs) / 1000000.0;

            {
                QMutexLocker locker(&self->impl_->mutex_);
                self->impl_->frameTimes_.push_back(elapsedMs);
                if (self->impl_->frameTimes_.size() > 240) {
                    self->impl_->frameTimes_.pop_back();
                }
            }

            // Post result back to main thread via signal/slot
            if (!ok) {
                if (!task->isCancelled()) {
                    QMetaObject::invokeMethod(self, "onTaskFailed", Qt::QueuedConnection,
                                              Q_ARG(RenderTask*, task), Q_ARG(QString, execError));
                }
            } else if (!task->isCancelled()) {
                if (elapsedMs > self->impl_->frameBudgetMs_) {
                    QMetaObject::invokeMethod(self, "onOverBudget", Qt::QueuedConnection,
                                              Q_ARG(double, elapsedMs));
                }
                QMetaObject::invokeMethod(self, "onTaskCompleted", Qt::QueuedConnection,
                                          Q_ARG(RenderTask*, task), Q_ARG(double, elapsedMs));
            }

            // Remove from active tasks
            {
                QMutexLocker locker(&self->impl_->mutex_);
                auto it = std::find(self->impl_->activeTasks_.begin(), self->impl_->activeTasks_.end(), task);
                if (it != self->impl_->activeTasks_.end()) {
                    self->impl_->activeTasks_.erase(it);
                }
                self->impl_->completedTasks_.push_back(task);
                self->impl_->completedCount_++;
            }

            // Try to dispatch next task
            QMetaObject::invokeMethod(self, "processNextTask", Qt::QueuedConnection);
        });
    }
}

int RenderScheduler::pendingTaskCount() const {
    QMutexLocker locker(&impl_->mutex_);
    return (int)impl_->pendingTasks_.size();
}

int RenderScheduler::activeTaskCount() const {
    QMutexLocker locker(&impl_->mutex_);
    return (int)impl_->activeTasks_.size();
}

int RenderScheduler::completedTaskCount() const {
    return impl_->completedCount_.load();
}

bool RenderScheduler::isExecuting() const {
    return impl_->executing_.load();
}

bool RenderScheduler::isPaused() const {
    return impl_->paused_.load();
}

float RenderScheduler::overallProgress() const {
    int total = impl_->totalCount_.load();
    if (total == 0) return 0;
    return (float)impl_->completedCount_ / total;
}

FramePosition RenderScheduler::currentFrame() const {
    return impl_->currentFrame_;
}

double RenderScheduler::averageFrameTime() const {
    if (impl_->frameTimes_.empty()) return 0;
    double sum = 0;
    for (double t : impl_->frameTimes_) sum += t;
    return sum / impl_->frameTimes_.size();
}

double RenderScheduler::estimatedTimeRemaining() const {
    double avgTime = averageFrameTime();
    int remaining = impl_->totalCount_.load() - impl_->completedCount_.load();
    return avgTime * remaining;
}

size_t RenderScheduler::memoryUsage() const {
    // Would query actual memory usage
    return 0;
}

QMap<FrameDropReason, int> RenderScheduler::frameDropStats() const {
    QMap<FrameDropReason, int> map;
    map.insert(FrameDropReason::Cancelled, impl_->dropCounts_[static_cast<int>(FrameDropReason::Cancelled)]);
    map.insert(FrameDropReason::ExecutionError, impl_->dropCounts_[static_cast<int>(FrameDropReason::ExecutionError)]);
    map.insert(FrameDropReason::OverBudget, impl_->dropCounts_[static_cast<int>(FrameDropReason::OverBudget)]);
    map.insert(FrameDropReason::QueueStarvation, impl_->dropCounts_[static_cast<int>(FrameDropReason::QueueStarvation)]);
    return map;
}

void RenderScheduler::resetFrameDropStats() {
    impl_->dropCounts_ = {0,0,0,0,0};
}

// ==================== BatchRenderer::Impl ====================

class BatchRenderer::Impl {
public:
    RenderScheduler* scheduler_ = nullptr;
    FrameRange batchRange_;
    std::atomic<bool> batchRunning_{false};
    std::atomic<float> batchProgress_{0};
};

BatchRenderer::BatchRenderer(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

BatchRenderer::~BatchRenderer() = default;

void BatchRenderer::setScheduler(RenderScheduler* scheduler) {
    impl_->scheduler_ = scheduler;
}

RenderScheduler* BatchRenderer::scheduler() const {
    return impl_->scheduler_;
}

void BatchRenderer::renderRange(const FrameRange& range, TaskPriority priority) {
    if (!impl_->scheduler_) return;
    
    impl_->batchRange_ = range;
    impl_->batchRunning_ = true;
    impl_->batchProgress_ = 0;
    
    emit batchStarted();
    
    // Create tasks for each frame in range
        for (long long f = range.startPosition().framePosition(); f <= range.endPosition().framePosition(); ++f) {
        auto* task = new RenderTask(FrameRange(FramePosition(f), FramePosition(f)), priority);
        impl_->scheduler_->submitTask(task);
    }
    
    impl_->scheduler_->startExecution();
}

void BatchRenderer::renderComposition(const QString& compId, TaskPriority priority) {
    // Would query composition for frame range
    Q_UNUSED(compId);
    Q_UNUSED(priority);
}

void BatchRenderer::renderOnlyDirtyFrames() {
    // Would render only frames marked as dirty
}

void BatchRenderer::renderMissingFrames() {
    // Would render frames not in cache
}

void BatchRenderer::renderAroundFrame(const FramePosition& frame, int radius) {
    FrameRange range(FramePosition(frame.framePosition() - radius),
                   FramePosition(frame.framePosition() + radius));
    renderRange(range, TaskPriority::High);
}

void BatchRenderer::cancelBatch() {
    if (impl_->scheduler_) {
        impl_->scheduler_->cancelAllTasks();
    }
    impl_->batchRunning_ = false;
}

float BatchRenderer::currentBatchProgress() const {
    return impl_->batchProgress_.load();
}

FrameRange BatchRenderer::batchRange() const {
    return impl_->batchRange_;
}

} // namespace Artifact
