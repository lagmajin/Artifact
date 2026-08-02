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
import Memory.SharedPtr;

namespace Artifact {

W_OBJECT_IMPL(FrameCache)
W_OBJECT_IMPL(ProgressiveRenderer)
W_OBJECT_IMPL(RenderPerformanceMonitor)

// ==================== FrameCache::Impl ====================

namespace {
struct FramePositionHash {
    size_t operator()(const ArtifactCore::FramePosition& fp) const {
        return std::hash<int64_t>{}(fp.framePosition());
    }
};
} // namespace

class FrameCache::Impl {
public:
    struct AccessCandidate {
        uint64_t key = 0;
        int64_t frame = 0;
        bool operator>(const AccessCandidate& other) const {
            if (key != other.key) return key > other.key;
            return frame > other.frame;
        }
    };

    struct SizeCandidate {
        size_t key = 0;
        int64_t frame = 0;
        bool operator<(const SizeCandidate& other) const {
            if (key != other.key) return key < other.key;
            return frame < other.frame;
        }
    };

    // Cache storage
    std::unordered_map<FramePosition, ArtifactCore::SharedPtr<FrameCacheEntry>, FramePositionHash> entries_;
    
    // Access tracking for LRU
    std::unordered_map<FramePosition, uint64_t, FramePositionHash> accessTimes_;
    std::unordered_map<FramePosition, int, FramePositionHash> accessCounts_;  // For LFU
    std::deque<FramePosition> insertionOrder_;    // For FIFO
    std::priority_queue<AccessCandidate, std::vector<AccessCandidate>,
                        std::greater<AccessCandidate>> lruCandidates_;
    std::priority_queue<AccessCandidate, std::vector<AccessCandidate>,
                        std::greater<AccessCandidate>> lfuCandidates_;
    std::priority_queue<SizeCandidate> sizeCandidates_;
    
    // Configuration
    size_t maxMemory_ = 512 * 1024 * 1024; // 512 MB default
    int maxFrameCount_ = 100;
    CachePolicy policy_ = CachePolicy::LRU;
    
    // Statistics
    size_t hitCount_ = 0;
    size_t missCount_ = 0;
    size_t memoryUsage_ = 0;
    uint64_t generation_ = 1;
    
    // Thread safety
    mutable QMutex mutex_;
    QWaitCondition waitCondition_;
    
    // Prefetch queue
    std::set<FramePosition> prefetchQueue_;
    bool prefetchEnabled_ = true;
    
    size_t currentMemoryUsage() const {
        return memoryUsage_;
    }

    void rebuildCandidates() {
        lruCandidates_ = {};
        lfuCandidates_ = {};
        sizeCandidates_ = {};
        for (const auto& [frame, timestamp] : accessTimes_) {
            if (entries_.find(frame) != entries_.end()) {
                lruCandidates_.push({timestamp, frame.framePosition()});
            }
        }
        for (const auto& [frame, count] : accessCounts_) {
            if (entries_.find(frame) != entries_.end()) {
                lfuCandidates_.push(
                    {static_cast<uint64_t>(count), frame.framePosition()});
            }
        }
        for (const auto& [frame, entry] : entries_) {
            if (entry) {
                sizeCandidates_.push(
                    {entry->memorySize, frame.framePosition()});
            }
        }
    }

    void maybeRebuildCandidates() {
        const size_t liveEntries = entries_.size();
        const size_t threshold = liveEntries * 8u + 64u;
        if (lruCandidates_.size() > threshold ||
            lfuCandidates_.size() > threshold ||
            sizeCandidates_.size() > threshold) {
            rebuildCandidates();
        }
    }

    void recordAccess(const FramePosition& frame, const uint64_t timestamp) {
        accessTimes_[frame] = timestamp;
        accessCounts_[frame]++;
        lruCandidates_.push({timestamp, frame.framePosition()});
        lfuCandidates_.push({static_cast<uint64_t>(accessCounts_[frame]),
                             frame.framePosition()});
        maybeRebuildCandidates();
    }
    
    ArtifactCore::SharedPtr<FrameCacheEntry> evictOne() {
        if (entries_.empty()) return nullptr;
        
        FramePosition toEvict;
        bool found = false;
        
        switch (policy_) {
            case CachePolicy::LRU: {
                while (!lruCandidates_.empty()) {
                    const auto candidate = lruCandidates_.top();
                    lruCandidates_.pop();
                    const FramePosition frame(candidate.frame);
                    auto timeIt = accessTimes_.find(frame);
                    if (timeIt != accessTimes_.end() &&
                        timeIt->second == candidate.key &&
                        entries_.find(frame) != entries_.end()) {
                        toEvict = frame;
                        found = true;
                        break;
                    }
                }
                break;
            }
            case CachePolicy::LFU: {
                while (!lfuCandidates_.empty()) {
                    const auto candidate = lfuCandidates_.top();
                    lfuCandidates_.pop();
                    const FramePosition frame(candidate.frame);
                    auto countIt = accessCounts_.find(frame);
                    if (countIt != accessCounts_.end() &&
                        countIt->second == static_cast<int>(candidate.key) &&
                        entries_.find(frame) != entries_.end()) {
                        toEvict = frame;
                        found = true;
                        break;
                    }
                }
                break;
            }
            case CachePolicy::FIFO: {
                while (!insertionOrder_.empty()) {
                    const FramePosition frame = insertionOrder_.front();
                    insertionOrder_.pop_front();
                    if (entries_.find(frame) != entries_.end()) {
                        toEvict = frame;
                        found = true;
                        break;
                    }
                }
                break;
            }
            case CachePolicy::Size: {
                while (!sizeCandidates_.empty()) {
                    const auto candidate = sizeCandidates_.top();
                    sizeCandidates_.pop();
                    const FramePosition frame(candidate.frame);
                    auto entryIt = entries_.find(frame);
                    if (entryIt != entries_.end() && entryIt->second &&
                        entryIt->second->memorySize == candidate.key) {
                        toEvict = frame;
                        found = true;
                        break;
                    }
                }
                break;
            }
            default:
                toEvict = entries_.begin()->first;
                found = true;
        }

        // Candidate queues are intentionally lazy and may contain only stale
        // records after invalidation/replacement. Never let eviction spin
        // forever when that bookkeeping has no live candidate.
        if (!found && !entries_.empty()) {
            toEvict = entries_.begin()->first;
        }
        
        auto it = entries_.find(toEvict);
        if (it != entries_.end()) {
            auto entry = it->second;
            memoryUsage_ -= entry ? entry->memorySize : 0;
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
    impl_->maxFrameCount_ = std::max(0, count);
    impl_->evictToFit(impl_->maxMemory_, impl_->maxFrameCount_);
}

int FrameCache::maxFrameCount() const {
    QMutexLocker locker(&impl_->mutex_);
    return impl_->maxFrameCount_;
}

void FrameCache::setPolicy(CachePolicy policy) {
    QMutexLocker locker(&impl_->mutex_);
    const int value = std::clamp(static_cast<int>(policy), 0, 4);
    impl_->policy_ = static_cast<CachePolicy>(value);
    impl_->rebuildCandidates();
}

CachePolicy FrameCache::policy() const {
    QMutexLocker locker(&impl_->mutex_);
    return impl_->policy_;
}

bool FrameCache::contains(const FramePosition& frame) const {
    QMutexLocker locker(&impl_->mutex_);
    const auto it = impl_->entries_.find(frame);
    return it != impl_->entries_.end() && it->second &&
           it->second->generation == impl_->generation_;
}

ArtifactCore::SharedPtr<FrameCacheEntry> FrameCache::get(const FramePosition& frame) {
    QMutexLocker locker(&impl_->mutex_);
    
    auto it = impl_->entries_.find(frame);
    if (it != impl_->entries_.end()) {
        if (!it->second) {
            impl_->entries_.erase(it);
            impl_->missCount_++;
            return nullptr;
        }
        if (it->second->generation != impl_->generation_) {
            impl_->memoryUsage_ -= it->second->memorySize;
            impl_->entries_.erase(it);
            impl_->accessTimes_.erase(frame);
            impl_->accessCounts_.erase(frame);
            impl_->missCount_++;
            return nullptr;
        }
        impl_->hitCount_++;
        
        // Update access tracking
        impl_->recordAccess(
            frame, std::chrono::steady_clock::now().time_since_epoch().count());
        
        return it->second;
    }
    
    impl_->missCount_++;
    return nullptr;
}

void FrameCache::put(ArtifactCore::SharedPtr<FrameCacheEntry> entry) {
    if (!entry) return;

    QMutexLocker locker(&impl_->mutex_);

    if (impl_->maxFrameCount_ <= 0) {
        throw std::invalid_argument("Frame cache capacity is zero");
    }

    // Validate entry memory size
    if (entry->memorySize == 0 || entry->memorySize > impl_->maxMemory_) {
        throw std::invalid_argument("Frame entry memory size exceeds cache capacity");
    }

    // Remove old entry if exists
    auto it = impl_->entries_.find(entry->frame);
    if (it != impl_->entries_.end()) {
        impl_->memoryUsage_ -= it->second ? it->second->memorySize : 0;
        impl_->entries_.erase(it);
    }

    // Evict if needed - prevent integer underflow
    size_t targetMem = impl_->maxMemory_ - entry->memorySize;
    size_t targetCount = impl_->maxFrameCount_ > 0 ? impl_->maxFrameCount_ - 1 : 0;
    impl_->evictToFit(targetMem, targetCount);

    // Add new entry
    entry->generation = impl_->generation_;
    impl_->entries_[entry->frame] = entry;
    impl_->memoryUsage_ += entry->memorySize;
    impl_->accessCounts_[entry->frame] = 0;
    impl_->recordAccess(
        entry->frame, std::chrono::steady_clock::now().time_since_epoch().count());
    impl_->sizeCandidates_.push(
        {entry->memorySize, entry->frame.framePosition()});
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
        impl_->memoryUsage_ -= it->second ? it->second->memorySize : 0;
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
        auto it = impl_->entries_.find(frame);
        if (it != impl_->entries_.end()) {
            impl_->memoryUsage_ -= it->second ? it->second->memorySize : 0;
        }
        impl_->entries_.erase(frame);
        impl_->accessTimes_.erase(frame);
        impl_->accessCounts_.erase(frame);
    }
}

void FrameCache::invalidateStaleGenerations(uint64_t minGenerationToKeep) {
    QMutexLocker locker(&impl_->mutex_);
    std::vector<FramePosition> toRemove;
    for (const auto& [pos, entry] : impl_->entries_) {
        if (entry && entry->generation < minGenerationToKeep) {
            toRemove.push_back(pos);
        }
    }
    for (const auto& frame : toRemove) {
        emit frameRemoved(frame);
        auto it = impl_->entries_.find(frame);
        if (it != impl_->entries_.end()) {
            impl_->memoryUsage_ -= it->second ? it->second->memorySize : 0;
        }
        impl_->entries_.erase(frame);
        impl_->accessTimes_.erase(frame);
        impl_->accessCounts_.erase(frame);
    }
}

void FrameCache::invalidateAll() {
    QMutexLocker locker(&impl_->mutex_);
    impl_->entries_.clear();
    impl_->memoryUsage_ = 0;
    impl_->accessTimes_.clear();
    impl_->accessCounts_.clear();
    impl_->insertionOrder_.clear();
    impl_->lruCandidates_ = {};
    impl_->lfuCandidates_ = {};
    impl_->sizeCandidates_ = {};
    impl_->generation_++;
    emit cacheCleared();
    emit generationChanged(impl_->generation_, QStringLiteral("invalidateAll"));
}

uint64_t FrameCache::generation() const {
    QMutexLocker locker(&impl_->mutex_);
    return impl_->generation_;
}

uint64_t FrameCache::bumpGeneration(const QString& reason) {
    QMutexLocker locker(&impl_->mutex_);
    impl_->generation_++;
    emit generationChanged(impl_->generation_, reason);
    return impl_->generation_;
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
    const auto first = range.start().value();
    const auto last = range.end().value();
    if (first > last) return;
    for (long long f = first;; ++f) {
        impl_->prefetchQueue_.insert(FramePosition(f));
        if (f == last) break;
    }
}

void FrameCache::cancelPrefetch(const FrameRange& range) {
    QMutexLocker locker(&impl_->mutex_);
    const auto first = range.start().value();
    const auto last = range.end().value();
    if (first > last) return;
    for (long long f = first;; ++f) {
        impl_->prefetchQueue_.erase(FramePosition(f));
        if (f == last) break;
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
    if (impl_->entries_.find(frame) == impl_->entries_.end()) {
        return;
    }
    impl_->recordAccess(
        frame, std::chrono::steady_clock::now().time_since_epoch().count());
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
    const int value = std::clamp(static_cast<int>(quality), 0, 3);
    impl_->quality_ = static_cast<RenderQuality>(value);
    emit qualityChanged(impl_->quality_);
}

RenderQuality ProgressiveRenderer::quality() const {
    return impl_->quality_;
}

void ProgressiveRenderer::setDraftQuality(int downsampling) {
    impl_->draftDownsample_ = std::clamp(downsampling, 1, 64);
}

void ProgressiveRenderer::setPreviewQuality(int downsampling) {
    impl_->previewDownsample_ = std::clamp(downsampling, 1, 64);
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
    if (!std::isfinite(timeMs) || timeMs < 0.0) {
        return;
    }
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
        
        const double averageFPS = impl_->metrics_.averageFrameTime > 0.0
            ? 1000.0 / impl_->metrics_.averageFrameTime
            : 0.0;
        emit fpsChanged(impl_->currentFPS_, averageFPS);
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
    return impl_->metrics_.averageFrameTime > 0.0 &&
           impl_->metrics_.averageFrameTime <= impl_->frameTimeBudget_;
}

void RenderPerformanceMonitor::setTargetFPS(double fps) {
    if (!std::isfinite(fps) || fps <= 0.0) {
        return;
    }
    impl_->targetFPS_ = fps;
    impl_->frameTimeBudget_ = 1000.0 / fps;
}

double RenderPerformanceMonitor::targetFPS() const {
    return impl_->targetFPS_;
}

void RenderPerformanceMonitor::setFrameTimeBudget(double ms) {
    if (!std::isfinite(ms) || ms < 0.0) {
        return;
    }
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
    impl_->fpsTimer_.restart();
}

} // namespace Artifact
