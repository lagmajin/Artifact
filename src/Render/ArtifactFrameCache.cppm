module;

#include <algorithm>
#include <chrono>
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
module Artifact.Render.FrameCache;




import Frame.Position;

namespace Artifact {

W_OBJECT_IMPL(FrameCache)
W_OBJECT_IMPL(ProgressiveRenderer)
W_OBJECT_IMPL(RenderPerformanceMonitor)

// ==================== FrameCache::Impl ====================

class FrameCache::Impl {
public:
    // Cache storage
    std::map<FramePosition, std::shared_ptr<FrameCacheEntry>> entries_;
    
    // Access tracking for LRU
    std::map<FramePosition, uint64_t> accessTimes_;
    std::map<FramePosition, int> accessCounts_;  // For LFU
    std::deque<FramePosition> insertionOrder_;    // For FIFO
    
    // Configuration
    size_t maxMemory_ = 512 * 1024 * 1024; // 512 MB default
    int maxFrameCount_ = 100;
    CachePolicy policy_ = CachePolicy::LRU;
    
    // Statistics
    size_t hitCount_ = 0;
    size_t missCount_ = 0;
    
    // Thread safety
    mutable QMutex mutex_;
    QWaitCondition waitCondition_;
    
    // Prefetch queue
    std::set<FramePosition> prefetchQueue_;
    bool prefetchEnabled_ = true;
    
    size_t currentMemoryUsage() const {
        size_t total = 0;
        for (auto& [pos, entry] : entries_) {
            total += entry->memorySize;
        }
        return total;
    }
    
    std::shared_ptr<FrameCacheEntry> evictOne() {
        if (entries_.empty()) return nullptr;
        
        FramePosition toEvict;
        
        switch (policy_) {
            case CachePolicy::LRU: {
                // Find oldest access
                uint64_t oldest = UINT64_MAX;
                for (auto& [pos, time] : accessTimes_) {
                    if (time < oldest) {
                        oldest = time;
                        toEvict = pos;
                    }
                }
                break;
            }
            case CachePolicy::LFU: {
                // Find least frequently used
                int minCount = INT_MAX;
                for (auto& [pos, count] : accessCounts_) {
                    if (count < minCount) {
                        minCount = count;
                        toEvict = pos;
                    }
                }
                break;
            }
            case CachePolicy::FIFO: {
                toEvict = insertionOrder_.front();
                insertionOrder_.pop_front();
                break;
            }
            case CachePolicy::Size: {
                // Find largest
                size_t maxSize = 0;
                for (auto& [pos, entry] : entries_) {
                    if (entry->memorySize > maxSize) {
                        maxSize = entry->memorySize;
                        toEvict = pos;
                    }
                }
                break;
            }
            default:
                toEvict = entries_.begin()->first;
        }
        
        auto it = entries_.find(toEvict);
        if (it != entries_.end()) {
            auto entry = it->second;
            entries_.erase(it);
            accessTimes_.erase(toEvict);
            accessCounts_.erase(toEvict);
            return entry;
        }
        
        return nullptr;
    }
    
    void evictToFit(size_t targetMemory, int targetCount) {
        while ((currentMemoryUsage() > targetMemory || (int)entries_.size() > targetCount) 
               && !entries_.empty()) {
            auto evicted = evictOne();
            if (evicted) {
                emit frameEvicted(evicted->frame);
            }
        }
    }
};

W_OBJECT_IMPL(FrameCache)

FrameCache::FrameCache(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

FrameCache::~FrameCache() = default;

void FrameCache::setMaxMemoryBytes(size_t bytes) {
    QMutexLocker locker(&impl_->mutex_);
    impl_->maxMemory_ = bytes;
    impl_->evictToFit(impl_->maxMemory_, impl_->maxFrameCount_);
}

size_t FrameCache::maxMemoryBytes() const {
    QMutexLocker locker(&impl_->mutex_);
    return impl_->maxMemory_;
}

void FrameCache::setMaxFrameCount(int count) {
    QMutexLocker locker(&impl_->mutex_);
    impl_->maxFrameCount_ = count;
    impl_->evictToFit(impl_->maxMemory_, impl_->maxFrameCount_);
}

int FrameCache::maxFrameCount() const {
    QMutexLocker locker(&impl_->mutex_);
    return impl_->maxFrameCount_;
}

void FrameCache::setPolicy(CachePolicy policy) {
    QMutexLocker locker(&impl_->mutex_);
    impl_->policy_ = policy;
}

CachePolicy FrameCache::policy() const {
    QMutexLocker locker(&impl_->mutex_);
    return impl_->policy_;
}

bool FrameCache::contains(const FramePosition& frame) const {
    QMutexLocker locker(&impl_->mutex_);
    return impl_->entries_.count(frame) > 0;
}

std::shared_ptr<FrameCacheEntry> FrameCache::get(const FramePosition& frame) {
    QMutexLocker locker(&impl_->mutex_);
    
    auto it = impl_->entries_.find(frame);
    if (it != impl_->entries_.end()) {
        impl_->hitCount_++;
        
        // Update access tracking
        impl_->accessTimes_[frame] = std::chrono::steady_clock::now().time_since_epoch().count();
        impl_->accessCounts_[frame]++;
        
        return it->second;
    }
    
    impl_->missCount_++;
    return nullptr;
}

void FrameCache::put(std::shared_ptr<FrameCacheEntry> entry) {
    if (!entry) return;

    // Validate entry memory size
    if (entry->memorySize == 0 || entry->memorySize > impl_->maxMemory_) {
        throw std::invalid_argument("Frame entry memory size exceeds cache capacity");
    }

    QMutexLocker locker(&impl_->mutex_);

    // Remove old entry if exists
    auto it = impl_->entries_.find(entry->frame);
    if (it != impl_->entries_.end()) {
        impl_->entries_.erase(it);
    }

    // Evict if needed - prevent integer underflow
    size_t targetMem = impl_->maxMemory_ - entry->memorySize;
    size_t targetCount = impl_->maxFrameCount_ > 0 ? impl_->maxFrameCount_ - 1 : 0;
    impl_->evictToFit(targetMem, targetCount);

    // Add new entry
    impl_->entries_[entry->frame] = entry;
    impl_->accessTimes_[entry->frame] = std::chrono::steady_clock::now().time_since_epoch().count();
    impl_->accessCounts_[entry->frame] = 1;
    impl_->insertionOrder_.push_back(entry->frame);

    // Emit signals
    emit frameAdded(entry->frame);

    // Check memory pressure
    size_t currentMem = impl_->currentMemoryUsage();
    if (currentMem > impl_->maxMemory_ * 0.9) {
        emit memoryPressure(currentMem, impl_->maxMemory_);
    }

    // Update hit rate
    size_t total = impl_->hitCount_ + impl_->missCount_;
    if (total > 0) {
        emit hitRateChanged((float)impl_->hitCount_ / total);
    }
}

void FrameCache::invalidate(const FramePosition& frame) {
    QMutexLocker locker(&impl_->mutex_);
    
    auto it = impl_->entries_.find(frame);
    if (it != impl_->entries_.end()) {
        emit frameRemoved(frame);
        impl_->entries_.erase(it);
        impl_->accessTimes_.erase(frame);
        impl_->accessCounts_.erase(frame);
    }
}

void FrameCache::invalidateRange(const FrameRange& range) {
    QMutexLocker locker(&impl_->mutex_);
    
    std::vector<FramePosition> toRemove;
    for (auto& [pos, entry] : impl_->entries_) {
        if (range.contains(pos)) {
            toRemove.push_back(pos);
        }
    }
    
    for (auto& frame : toRemove) {
        emit frameRemoved(frame);
        impl_->entries_.erase(frame);
        impl_->accessTimes_.erase(frame);
        impl_->accessCounts_.erase(frame);
    }
}

void FrameCache::invalidateAll() {
    QMutexLocker locker(&impl_->mutex_);
    impl_->entries_.clear();
    impl_->accessTimes_.clear();
    impl_->accessCounts_.clear();
    impl_->insertionOrder_.clear();
    emit cacheCleared();
}

size_t FrameCache::currentMemoryUsage() const {
    QMutexLocker locker(&impl_->mutex_);
    return impl_->currentMemoryUsage();
}

int FrameCache::currentFrameCount() const {
    QMutexLocker locker(&impl_->mutex_);
    return (int)impl_->entries_.size();
}

float FrameCache::hitRate() const {
    QMutexLocker locker(&impl_->mutex_);
    size_t total = impl_->hitCount_ + impl_->missCount_;
    if (total == 0) return 0;
    return (float)impl_->hitCount_ / total;
}

size_t FrameCache::cacheMissCount() const {
    QMutexLocker locker(&impl_->mutex_);
    return impl_->missCount_;
}

size_t FrameCache::cacheHitCount() const {
    QMutexLocker locker(&impl_->mutex_);
    return impl_->hitCount_;
}

void FrameCache::prefetch(const FramePosition& frame) {
    QMutexLocker locker(&impl_->mutex_);
    impl_->prefetchQueue_.insert(frame);
}

void FrameCache::prefetchRange(const FrameRange& range) {
    QMutexLocker locker(&impl_->mutex_);
    for (long long f = range.start().value(); f <= range.end().value(); ++f) {
        impl_->prefetchQueue_.insert(FramePosition(f));
    }
}

void FrameCache::cancelPrefetch(const FrameRange& range) {
    QMutexLocker locker(&impl_->mutex_);
    for (long long f = range.start().value(); f <= range.end().value(); ++f) {
        impl_->prefetchQueue_.erase(FramePosition(f));
    }
}

void FrameCache::trimToSize(size_t targetBytes) {
    QMutexLocker locker(&impl_->mutex_);
    impl_->evictToFit(targetBytes, impl_->maxFrameCount_);
}

void FrameCache::clear() {
    invalidateAll();
}

void FrameCache::touch(const FramePosition& frame) {
    QMutexLocker locker(&impl_->mutex_);
    impl_->accessTimes_[frame] = std::chrono::steady_clock::now().time_since_epoch().count();
    impl_->accessCounts_[frame]++;
}

// ==================== ProgressiveRenderer::Impl ====================

class ProgressiveRenderer::Impl {
public:
    RenderQuality quality_ = RenderQuality::Preview;
    RenderQuality renderedQuality_ = RenderQuality::Draft;
    
    int draftDownsample_ = 4;
    int previewDownsample_ = 2;
    
    float progress_ = 0;
    bool upgrading_ = false;
    
    // Quality thresholds (in ms per frame)
    double draftThreshold_ = 16.67;   // 60 fps
    double previewThreshold_ = 33.33; // 30 fps
    double finalThreshold_ = 100;      // 10 fps minimum
};

ProgressiveRenderer::ProgressiveRenderer(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

ProgressiveRenderer::~ProgressiveRenderer() = default;

void ProgressiveRenderer::setQuality(RenderQuality quality) {
    impl_->quality_ = quality;
    emit qualityChanged(quality);
}

RenderQuality ProgressiveRenderer::quality() const {
    return impl_->quality_;
}

void ProgressiveRenderer::setDraftQuality(int downsampling) {
    impl_->draftDownsample_ = downsampling;
}

void ProgressiveRenderer::setPreviewQuality(int downsampling) {
    impl_->previewDownsample_ = downsampling;
}

float ProgressiveRenderer::currentProgress() const {
    return impl_->progress_;
}

RenderQuality ProgressiveRenderer::currentRenderedQuality() const {
    return impl_->renderedQuality_;
}

bool ProgressiveRenderer::isUpgrading() const {
    return impl_->upgrading_;
}

void ProgressiveRenderer::requestUpgrade() {
    if (impl_->quality_ == RenderQuality::Final) return;
    
    impl_->upgrading_ = true;
    emit upgradeStarted();
    
    // In real implementation, this would trigger background re-render
    // For now just simulate completion
    impl_->renderedQuality_ = impl_->quality_;
    impl_->progress_ = 1.0f;
    impl_->upgrading_ = false;
    
    emit upgradeCompleted();
}

void ProgressiveRenderer::cancelUpgrade() {
    impl_->upgrading_ = false;
}

void ProgressiveRenderer::forceFinalRender() {
    impl_->quality_ = RenderQuality::Final;
    impl_->renderedQuality_ = RenderQuality::Final;
    emit qualityChanged(RenderQuality::Final);
}

// ==================== RenderPerformanceMonitor::Impl ====================

class RenderPerformanceMonitor::Impl {
public:
    std::deque<double> frameTimes_;
    static const int maxFrameTimeHistory = 300;
    
    RenderMetrics metrics_;
    
    double targetFPS_ = 30.0;
    double frameTimeBudget_ = 33.33; // ms
    
    QElapsedTimer fpsTimer_;
    int frameCountForFPS_ = 0;
    double currentFPS_ = 0;
    
    void recordFrameTime(double timeMs) {
        frameTimes_.push_back(timeMs);
        if ((int)frameTimes_.size() > maxFrameTimeHistory) {
            frameTimes_.pop_front();
        }
        
        // Update metrics
        metrics_.lastFrameTime = timeMs;
        metrics_.framesRendered++;
        
        if (frameTimes_.size() > 0) {
            double sum = 0;
            for (double t : frameTimes_) sum += t;
            metrics_.averageFrameTime = sum / frameTimes_.size();
        }
        
        if (metrics_.minFrameTime == 0 || timeMs < metrics_.minFrameTime) {
            metrics_.minFrameTime = timeMs;
        }
        if (timeMs > metrics_.maxFrameTime) {
            metrics_.maxFrameTime = timeMs;
        }
    }
};

RenderPerformanceMonitor::RenderPerformanceMonitor(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
    impl_->fpsTimer_.start();
}

RenderPerformanceMonitor::~RenderPerformanceMonitor() = default;

void RenderPerformanceMonitor::recordFrameRender(double timeMs, RenderQuality quality) {
    impl_->recordFrameTime(timeMs);
    
    switch (quality) {
        case RenderQuality::Draft: impl_->metrics_.draftFrames++; break;
        case RenderQuality::Preview: impl_->metrics_.previewFrames++; break;
        case RenderQuality::Final: impl_->metrics_.finalFrames++; break;
        default: break;
    }
    
    // Update FPS calculation
    impl_->frameCountForFPS_++;
    if (impl_->fpsTimer_.elapsed() >= 1000) {
        impl_->currentFPS_ = impl_->frameCountForFPS_ * 1000.0 / impl_->fpsTimer_.elapsed();
        impl_->frameCountForFPS_ = 0;
        impl_->fpsTimer_.restart();
        
        emit fpsChanged(impl_->currentFPS_, 1000.0 / impl_->metrics_.averageFrameTime);
    }
    
    // Check performance
    if (timeMs > impl_->frameTimeBudget_) {
        emit performanceWarning(QString("Frame time exceeded budget: %1ms > %2ms")
            .arg(timeMs).arg(impl_->frameTimeBudget_));
    }
}

void RenderPerformanceMonitor::recordFrameDrop() {
    impl_->metrics_.framesDropped++;
}

void RenderPerformanceMonitor::recordGPUAlloc(size_t bytes) {
    impl_->metrics_.gpuMemoryUsed = bytes;
}

void RenderPerformanceMonitor::recordSystemAlloc(size_t bytes) {
    impl_->metrics_.systemMemoryUsed = bytes;
}

RenderMetrics RenderPerformanceMonitor::getMetrics() const {
    return impl_->metrics_;
}

double RenderPerformanceMonitor::currentFPS() const {
    return impl_->currentFPS_;
}

double RenderPerformanceMonitor::averageFPS() const {
    if (impl_->metrics_.averageFrameTime > 0) {
        return 1000.0 / impl_->metrics_.averageFrameTime;
    }
    return 0;
}

bool RenderPerformanceMonitor::isPerformanceAcceptable() const {
    return impl_->metrics_.averageFrameTime <= impl_->frameTimeBudget_;
}

void RenderPerformanceMonitor::setTargetFPS(double fps) {
    impl_->targetFPS_ = fps;
    impl_->frameTimeBudget_ = 1000.0 / fps;
}

double RenderPerformanceMonitor::targetFPS() const {
    return impl_->targetFPS_;
}

void RenderPerformanceMonitor::setFrameTimeBudget(double ms) {
    impl_->frameTimeBudget_ = ms;
}

double RenderPerformanceMonitor::frameTimeBudget() const {
    return impl_->frameTimeBudget_;
}

void RenderPerformanceMonitor::reset() {
    impl_->frameTimes_.clear();
    impl_->metrics_ = RenderMetrics();
    impl_->currentFPS_ = 0;
    impl_->frameCountForFPS_ = 0;
}

} // namespace Artifact
