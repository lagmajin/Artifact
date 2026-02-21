module;
#include <QObject>
#include <QThread>
#include <QThreadPool>
#include <QFuture>
#include <QVector>
#include <atomic>
#include <functional>
#include <wobjectdefs.h>

export module Artifact.Render.Scheduler;

import std;
import Frame.Position;
import Frame.Range;

W_REGISTER_ARGTYPE(ArtifactCore::FramePosition)
W_REGISTER_ARGTYPE(ArtifactCore::FrameRange)

export namespace Artifact {

using namespace ArtifactCore;

/**
 * @brief Task priority levels
 */
enum class TaskPriority {
    Critical = 0,
    High = 1,
    Normal = 2,
    Low = 3,
    Background = 4
};

/**
 * @brief A render task for a single frame or range
 */
class RenderTask : public QObject {
    W_OBJECT(RenderTask)
private:
    FrameRange range_;
    TaskPriority priority_;
    bool isCancelled_ = false;
    int progress_ = 0;
    QString description_;
    
public:
    explicit RenderTask(const FrameRange& range, 
                      TaskPriority priority = TaskPriority::Normal,
                      QObject* parent = nullptr);
    ~RenderTask();
    
    FrameRange range() const { return range_; }
    TaskPriority priority() const { return priority_; }
    
    bool isCancelled() const { return isCancelled_; }
    void cancel() { isCancelled_ = true; }
    
    int progress() const { return progress_; }
    void setProgress(int p) { progress_ = p; emit progressChanged(p); }
    
    QString description() const { return description_; }
    void setDescription(const QString& desc) { description_ = desc; }
    
signals:
    void progressChanged(int progress) W_SIGNAL(progressChanged, progress);
    void completed() W_SIGNAL(completed);
    void failed(const QString& error) W_SIGNAL(failed, error);
};

/**
 * @brief Parallel rendering strategy
 */
enum class ParallelStrategy {
    Sequential,       // One frame at a time
    FrameParallel,    // Multiple frames in parallel
    TileParallel,     // Split single frame into tiles
    LayerParallel,    // Render layers in parallel
    Adaptive          // Auto-select best strategy
};

/**
 * @brief Multi-threaded render scheduler
 * 
 * Features:
 * - Thread pool management
 * - Task prioritization
 * - Adaptive parallel strategy
 * - Load balancing
 * - Progress tracking
 */
class RenderScheduler : public QObject {
    W_OBJECT(RenderScheduler)
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
public:
    explicit RenderScheduler(QObject* parent = nullptr);
    ~RenderScheduler();
    
    // Configuration
    void setThreadCount(int count);
    int threadCount() const;
    
    void setParallelStrategy(ParallelStrategy strategy);
    ParallelStrategy parallelStrategy() const;
    
    void setAdaptiveEnabled(bool enabled);
    bool isAdaptiveEnabled() const;
    
    // Task submission
    void submitTask(RenderTask* task);
    void submitTasks(const QVector<RenderTask*>& tasks);
    
    // Task management
    void cancelTask(RenderTask* task);
    void cancelAllTasks();
    
    void pauseTask(RenderTask* task);
    void resumeTask(RenderTask* task);
    
    // Priority
    void setTaskPriority(RenderTask* task, TaskPriority priority);
    
    // Execution control
    void startExecution();
    void pauseExecution();
    void stopExecution();
    
    // Queries
    int pendingTaskCount() const;
    int activeTaskCount() const;
    int completedTaskCount() const;
    
    bool isExecuting() const;
    bool isPaused() const;
    
    // Progress
    float overallProgress() const;
    FramePosition currentFrame() const;
    
    // Statistics
    double averageFrameTime() const;
    double estimatedTimeRemaining() const;
    size_t memoryUsage() const;
    
signals:
    void executionStarted() W_SIGNAL(executionStarted);
    void executionPaused() W_SIGNAL(executionPaused);
    void executionStopped() W_SIGNAL(executionStopped);
    void executionCompleted() W_SIGNAL(executionCompleted);
    
    void taskStarted(RenderTask* task) W_SIGNAL(taskStarted, task);
    void taskProgress(RenderTask* task, int progress) W_SIGNAL(taskProgress, task, progress);
    void taskCompleted(RenderTask* task) W_SIGNAL(taskCompleted, task);
    void taskFailed(RenderTask* task, const QString& error) W_SIGNAL(taskFailed, task, error);
    
    void progressChanged(float progress) W_SIGNAL(progressChanged, progress);
    void frameProcessed(const FramePosition& frame) W_SIGNAL(frameProcessed, frame);
    
    void performanceWarning(const QString& warning) W_SIGNAL(performanceWarning, warning);
};

/**
 * @brief Tile-based render task for parallel frame rendering
 */
class TileRenderTask {
public:
    int tileX, tileY;
    int width, height;
    int frameIndex;
    
    std::function<void(int x, int y, int w, int h, int frame)> renderFunc;
    
    TileRenderTask(int tx, int ty, int w, int h, int frame)
        : tileX(tx), tileY(ty), width(w), height(h), frameIndex(frame) {}
};

/**
 * @brief Batch renderer for rendering multiple frames
 */
class BatchRenderer : public QObject {
    W_OBJECT(BatchRenderer)
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
public:
    explicit BatchRenderer(QObject* parent = nullptr);
    ~BatchRenderer();
    
    // Configuration
    void setScheduler(RenderScheduler* scheduler);
    RenderScheduler* scheduler() const;
    
    // Batch operations
    void renderRange(const FrameRange& range, TaskPriority priority = TaskPriority::Normal);
    void renderComposition(const QString& compId, TaskPriority priority = TaskPriority::Normal);
    
    // Smart rendering
    void renderOnlyDirtyFrames();
    void renderMissingFrames();
    void renderAroundFrame(const FramePosition& frame, int radius = 5);
    
    // Stop
    void cancelBatch();
    
    // Progress
    float batchProgress() const;
    FrameRange batchRange() const;
    
signals:
    void batchStarted() W_SIGNAL(batchStarted);
    void batchProgress(float progress) W_SIGNAL(batchProgress, progress);
    void batchCompleted() W_SIGNAL(batchCompleted);
    void batchFailed(const QString& error) W_SIGNAL(batchFailed, error);
};

} // namespace Artifact
