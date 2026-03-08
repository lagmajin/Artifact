module;
#include <QObject>
#include <QSize>
#include <QString>
#include <QMutex>
#include <QWaitCondition>
#include <map>
#include <set>
#include <memory>
#include <wobjectdefs.h>

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
export module Artifact.Render.FrameCache;




import Frame.Position;

export namespace Artifact {

struct FrameRange {
    int start = 0;
    int end = 0;
};

W_REGISTER_ARGTYPE(ArtifactCore::FramePosition)

export namespace Artifact {

using namespace ArtifactCore;

/**
 * @brief Cache entry for a rendered frame
 */
struct FrameCacheEntry {
    std::vector<float> pixels;     // RGBA float pixels
    int width = 0;
    int height = 0;
    FramePosition frame;
    uint64_t generation = 0;
    uint64_t timestamp = 0;
    size_t memorySize = 0;
    bool isDirty = false;

    FrameCacheEntry() = default;

    FrameCacheEntry(int w, int h, const FramePosition& pos)
        : width(w), height(h), frame(pos) {
        // Validate dimensions to prevent integer overflow
        if (w <= 0 || h <= 0 || w > 65536 || h > 65536) {
            throw std::invalid_argument("Invalid frame dimensions");
        }
        // Check for overflow: w * h * 4 (RGBA)
        if (w > SIZE_MAX / 4 || h > SIZE_MAX / (w * 4)) {
            throw std::invalid_argument("Frame size would overflow");
        }
        pixels.resize(w * h * 4);
        memorySize = pixels.size() * sizeof(float);
    }
};

/**
 * @brief Cache policy for frame eviction
 */
enum class CachePolicy {
    LRU,            // Least Recently Used
    LFU,            // Least Frequently Used
    FIFO,           // First In First Out
    Priority,       // Priority-based
    Size            // Largest first
};

/**
 * @brief Frame cache for storing rendered frames
 * 
 * Features:
 * - LRU/LFU/FIFO eviction policies
 * - Memory budget management
 * - Thread-safe access
 * - Async prefetching
 */
class FrameCache : public QObject {
    W_OBJECT(FrameCache)
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
public:
    explicit FrameCache(QObject* parent = nullptr);
    ~FrameCache();
    
    // Configuration
    void setMaxMemoryBytes(size_t bytes);
    size_t maxMemoryBytes() const;
    
    void setMaxFrameCount(int count);
    int maxFrameCount() const;
    
    void setPolicy(CachePolicy policy);
    CachePolicy policy() const;
    
    // Cache operations
    bool contains(const FramePosition& frame) const;
    
    std::shared_ptr<FrameCacheEntry> get(const FramePosition& frame);
    
    void put(std::shared_ptr<FrameCacheEntry> entry);
    
    void invalidate(const FramePosition& frame);
    void invalidateRange(const FrameRange& range);
    void invalidateStaleGenerations(uint64_t minGenerationToKeep);
    void invalidateAll();
    uint64_t generation() const;
    uint64_t bumpGeneration(const QString& reason = QStringLiteral("manual"));
    
    // Statistics
    size_t currentMemoryUsage() const;
    int currentFrameCount() const;
    float hitRate() const;
    size_t cacheMissCount() const;
    size_t cacheHitCount() const;
    
    // Prefetch
    void prefetch(const FramePosition& frame);
    void prefetchRange(const FrameRange& range);
    void cancelPrefetch(const FrameRange& range);
    
    // Memory management
    void trimToSize(size_t targetBytes);
    void clear();
    
    // Access tracking
    void touch(const FramePosition& frame);
    
signals:
    void frameAdded(const FramePosition& frame) W_SIGNAL(frameAdded, frame);
    void frameRemoved(const FramePosition& frame) W_SIGNAL(frameRemoved, frame);
    void frameEvicted(const FramePosition& frame) W_SIGNAL(frameEvicted, frame);
    void cacheCleared() W_SIGNAL(cacheCleared);
    void generationChanged(uint64_t generation, const QString& reason) W_SIGNAL(generationChanged, generation, reason);
    void memoryPressure(size_t used, size_t max) W_SIGNAL(memoryPressure, used, max);
    void hitRateChanged(float rate) W_SIGNAL(hitRateChanged, rate);
};

/**
 * @brief Render quality levels
 */
enum class RenderQuality {
    Draft,      // Low quality, fast
    Preview,    // Medium quality
    Final,      // High quality, slow
    Custom      // User-defined
};

/**
 * @brief Progressive renderer for adaptive quality rendering
 * 
 * Renders at lower quality first for quick preview,
 * then upgrades to higher quality in background
 */
class ProgressiveRenderer : public QObject {
    W_OBJECT(ProgressiveRenderer)
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
public:
    explicit ProgressiveRenderer(QObject* parent = nullptr);
    ~ProgressiveRenderer();
    
    // Quality settings
    void setQuality(RenderQuality quality);
    RenderQuality quality() const;
    
    void setDraftQuality(int downsampling);
    void setPreviewQuality(int downsampling);
    
    // Rendering
    // Note: This would integrate with the actual renderer
    // For now it's a placeholder for the interface
    
    // Progress tracking
    float currentProgress() const;
    RenderQuality currentRenderedQuality() const;
    bool isUpgrading() const;
    
    // Manual control
    void requestUpgrade();
    void cancelUpgrade();
    void forceFinalRender();
    
    // Callbacks would be set externally to connect to actual render engine
    
signals:
    void qualityChanged(RenderQuality quality) W_SIGNAL(qualityChanged, quality);
    void upgradeStarted() W_SIGNAL(upgradeStarted);
    void upgradeCompleted() W_SIGNAL(upgradeCompleted);
    void progressChanged(float progress) W_SIGNAL(progressChanged, progress);
};

/**
 * @brief Render performance metrics
 */
struct RenderMetrics {
    double lastFrameTime = 0;           // ms
    double averageFrameTime = 0;        // ms
    double minFrameTime = 0;
    double maxFrameTime = 0;
    int framesRendered = 0;
    int framesDropped = 0;
    size_t gpuMemoryUsed = 0;
    size_t systemMemoryUsed = 0;
    float gpuUtilization = 0;
    float cpuUtilization = 0;
    
    // Quality breakdown
    int draftFrames = 0;
    int previewFrames = 0;
    int finalFrames = 0;
};

/**
 * @brief Performance monitor for rendering
 */
class RenderPerformanceMonitor : public QObject {
    W_OBJECT(RenderPerformanceMonitor)
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
public:
    explicit RenderPerformanceMonitor(QObject* parent = nullptr);
    ~RenderPerformanceMonitor();
    
    // Metrics collection
    void recordFrameRender(double timeMs, RenderQuality quality);
    void recordFrameDrop();
    void recordGPUAlloc(size_t bytes);
    void recordSystemAlloc(size_t bytes);
    
    // Queries
    RenderMetrics getMetrics() const;
    double currentFPS() const;
    double averageFPS() const;
    bool isPerformanceAcceptable() const;
    
    // Alerts
    void setTargetFPS(double fps);
    double targetFPS() const;
    
    void setFrameTimeBudget(double ms);
    double frameTimeBudget() const;
    
    // Reset
    void reset();
    
signals:
    void performanceWarning(const QString& message) W_SIGNAL(performanceWarning, message);
    void fpsChanged(double current, double average) W_SIGNAL(fpsChanged, current, average);
    void memoryWarning(size_t used, size_t max) W_SIGNAL(memoryWarning, used, max);
};

} // namespace Artifact
