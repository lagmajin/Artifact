
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
import Frame.Position;
import Frame.Range;

namespace Artifact {

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
    QThreadPool* threadPool_ = nullptr;
    ParallelStrategy strategy_ = ParallelStrategy::Adaptive;
    bool adaptiveEnabled_ = true;
    
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
    
    // Callbacks
    std::function<void(RenderTask*)> taskExecutor_;
    
    Impl() {
        threadPool_ = new QThreadPool();
        threadPool_->setMaxThreadCount(QThread::idealThreadCount());
    }
    
    ~Impl() {
        delete threadPool_;
    }
    
    void executeTask(RenderTask* task) {
        if (task->isCancelled() || stopRequested_) {
            return;
        }
        
        // Execute the task
        if (taskExecutor_) {
            try {
                taskExecutor_(task);
            } catch (const std::exception& e) {
                emit task->failed(QString::fromUtf8(e.what()));
                return;
            }
        }
        
        // Mark complete
        completedCount_++;
    }
};

RenderScheduler::RenderScheduler(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

RenderScheduler::~RenderScheduler() = default;

void RenderScheduler::setThreadCount(int count) {
    impl_->threadPool_->setMaxThreadCount(count);
}

int RenderScheduler::threadCount() const {
    return impl_->threadPool_->maxThreadCount();
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
    
    impl_->executing_ = true;
    impl_->paused_ = false;
    impl_->stopRequested_ = false;
    impl_->executionTimer_.start();
    
    emit executionStarted();
    
    // Start processing tasks
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
    if (!impl_->executing_ || impl_->paused_ || impl_->stopRequested_) {
        return;
    }
    
    RenderTask* task = nullptr;
    
    {
        QMutexLocker locker(&impl_->mutex_);
        
        if (impl_->pendingTasks_.empty()) {
            if (impl_->completedCount_ >= impl_->totalCount_) {
                impl_->executing_ = false;
                emit executionCompleted();
            }
            return;
        }
        
        task = impl_->pendingTasks_.front();
        impl_->pendingTasks_.erase(impl_->pendingTasks_.begin());
        impl_->activeTasks_.push_back(task);
    }
    
    if (task && !task->isCancelled()) {
        emit taskStarted(task);
        
        // Execute task (in real implementation, this would run in thread)
        impl_->executeTask(task);
        
        if (!task->isCancelled()) {
            emit taskCompleted(task);
            emit frameProcessed(task->range().startPosition());
        }
        
        {
            QMutexLocker locker(&impl_->mutex_);
            auto it = std::find(impl_->activeTasks_.begin(), impl_->activeTasks_.end(), task);
            if (it != impl_->activeTasks_.end()) {
                impl_->activeTasks_.erase(it);
            }
            impl_->completedTasks_.push_back(task);
        }
        
        // Update progress
        float progress = (float)impl_->completedCount_ / impl_->totalCount_;
        emit progressChanged(progress);
    }
    
    // Process next task
    if (impl_->executing_ && !impl_->paused_) {
        processNextTask();
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
