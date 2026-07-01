module;

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <vector>

module Artifact.Render.RamPreviewController;

namespace Artifact {

// --- Frame state bookkeeping ---

struct FrameState {
    RamPreviewFrameStatus status = RamPreviewFrameStatus::None;
    RamPreviewPriority priority = RamPreviewPriority::Background;
    int retryCount = 0;
    QString failReason;
};

// --- Priority queue entry ---

struct BuildJob {
    int64_t frame = 0;
    RamPreviewPriority priority = RamPreviewPriority::Background;

    bool operator<(const BuildJob& other) const {
        // Higher priority value = lower numeric = higher urgency
        // For same priority, lower frame number first
        if (priority != other.priority)
            return static_cast<uint8_t>(priority) > static_cast<uint8_t>(other.priority);
        return frame > other.frame;
    }
};

// --- Impl ---

class ArtifactRamPreviewController::Impl {
public:
    RenderFrameFn renderFn_;

    int64_t frameRangeStart_ = 0;
    int64_t frameRangeEnd_ = 0;
    int64_t previewStart_ = 0;
    int64_t previewEnd_ = 0;
    int64_t playheadFrame_ = 0;
    int playbackDirection_ = 1;

    std::atomic<bool> building_{false};
    std::atomic<bool> cancelRequested_{false};

    // Per-frame state (indexed by frame number - only within preview range)
    std::vector<FrameState> frameStates_;

    // Priority queue for build jobs
    std::priority_queue<BuildJob> jobQueue_;

    // Stats
    std::atomic<int> readyCount_{0};
    std::atomic<int> failedCount_{0};
    QString lastError_;

    int frameIndex(int64_t frame) const {
        if (frame < previewStart_ || frame >= previewEnd_) return -1;
        return static_cast<int>(frame - previewStart_);
    }

    int64_t frameAt(int index) const {
        return previewStart_ + index;
    }

    void ensureFrameStates() {
        const int count = static_cast<int>(previewEnd_ - previewStart_);
        if (count <= 0) return;
        if (static_cast<int>(frameStates_.size()) != count) {
            frameStates_.assign(count, FrameState{});
        }
    }

    void recalculatePriorities() {
        const int count = static_cast<int>(previewEnd_ - previewStart_);
        for (int i = 0; i < count; ++i) {
            auto& fs = frameStates_[i];
            if (fs.status != RamPreviewFrameStatus::None &&
                fs.status != RamPreviewFrameStatus::Pending) {
                continue; // Already assigned or done
            }
            const int64_t f = frameAt(i);
            const int64_t dist = std::abs(f - playheadFrame_);

            if (f == playheadFrame_)
                fs.priority = RamPreviewPriority::Playhead;
            else if (dist <= 3)
                fs.priority = RamPreviewPriority::Directional;
            else if (dist <= 15)
                fs.priority = RamPreviewPriority::Near;
            else
                fs.priority = RamPreviewPriority::WorkAreaFill;

            fs.status = RamPreviewFrameStatus::Pending;
        }
    }

    void rebuildQueue() {
        // Clear existing queue
        jobQueue_ = std::priority_queue<BuildJob>();
        
        // Recalculate priorities and push pending frames
        recalculatePriorities();
        const int count = static_cast<int>(previewEnd_ - previewStart_);
        for (int i = 0; i < count; ++i) {
            const auto& fs = frameStates_[i];
            if (fs.status == RamPreviewFrameStatus::Pending) {
                jobQueue_.push({frameAt(i), fs.priority});
            }
        }
    }

    void resetStates() {
        for (auto& fs : frameStates_) {
            fs = FrameState{};
        }
        readyCount_ = 0;
        failedCount_ = 0;
        lastError_.clear();
    }
};

ArtifactRamPreviewController::ArtifactRamPreviewController(QObject* parent)
    : QObject(parent), impl_(std::make_unique<Impl>()) {}

ArtifactRamPreviewController::~ArtifactRamPreviewController() = default;

void ArtifactRamPreviewController::setRenderFrameFn(RenderFrameFn fn) {
    impl_->renderFn_ = std::move(fn);
}

void ArtifactRamPreviewController::setFrameRange(int64_t start, int64_t end) {
    impl_->frameRangeStart_ = start;
    impl_->frameRangeEnd_ = end;
}

void ArtifactRamPreviewController::setPreviewRange(int64_t start, int64_t end) {
    impl_->previewStart_ = start;
    impl_->previewEnd_ = end;
    impl_->ensureFrameStates();
}

void ArtifactRamPreviewController::setPlayheadFrame(int64_t frame) {
    impl_->playheadFrame_ = frame;
}

void ArtifactRamPreviewController::setPlaybackDirection(int direction) {
    impl_->playbackDirection_ = (direction >= 0) ? 1 : -1;
}

void ArtifactRamPreviewController::startBuild() {
    if (impl_->building_.load()) return;
    if (!impl_->renderFn_) return;
    if (impl_->previewStart_ >= impl_->previewEnd_) return;

    impl_->cancelRequested_ = false;
    impl_->building_ = true;
    Q_EMIT buildStateChanged(true);

    // Reset states for the preview range
    impl_->resetStates();
    impl_->ensureFrameStates();
    impl_->rebuildQueue();

    // Process queue (synchronous for now - async can be added later)
    while (!impl_->jobQueue_.empty() && !impl_->cancelRequested_.load()) {
        const BuildJob job = impl_->jobQueue_.top();
        impl_->jobQueue_.pop();

        const int idx = impl_->frameIndex(job.frame);
        if (idx < 0) continue;

        auto& fs = impl_->frameStates_[idx];
        fs.status = RamPreviewFrameStatus::Rendering;

        // Call the render callback
        const bool success = impl_->renderFn_(job.frame);
        if (success) {
            fs.status = RamPreviewFrameStatus::Ready;
            impl_->readyCount_++;
            Q_EMIT frameReady(job.frame);
        } else {
            fs.status = RamPreviewFrameStatus::Failed;
            impl_->failedCount_++;
            fs.failReason = QStringLiteral("Render returned false");
            Q_EMIT frameFailed(job.frame, fs.failReason);
        }

        Q_EMIT buildProgress(impl_->readyCount_.load(),
            static_cast<int>(impl_->previewEnd_ - impl_->previewStart_));
    }

    impl_->building_ = false;

    if (!impl_->cancelRequested_.load() && isRangeFullyReady()) {
        Q_EMIT buildComplete();
    }

    Q_EMIT buildStateChanged(false);
}

void ArtifactRamPreviewController::stopBuild() {
    impl_->cancelRequested_ = true;
    impl_->building_ = false;
    // Clear pending queue
    impl_->jobQueue_ = std::priority_queue<BuildJob>();
    Q_EMIT buildStateChanged(false);
}

void ArtifactRamPreviewController::toggleBuild() {
    if (impl_->building_.load()) {
        stopBuild();
    } else {
        startBuild();
    }
}

void ArtifactRamPreviewController::clearCache() {
    impl_->resetStates();
    impl_->jobQueue_ = std::priority_queue<BuildJob>();
    impl_->frameStates_.clear();
    impl_->ensureFrameStates();
}

bool ArtifactRamPreviewController::isFrameReady(int64_t frame) const {
    const int idx = impl_->frameIndex(frame);
    if (idx < 0) return false;
    return impl_->frameStates_[idx].status == RamPreviewFrameStatus::Ready;
}

RamPreviewFrameStatus ArtifactRamPreviewController::frameStatus(int64_t frame) const {
    const int idx = impl_->frameIndex(frame);
    if (idx < 0) return RamPreviewFrameStatus::None;
    return impl_->frameStates_[idx].status;
}

int ArtifactRamPreviewController::readyCount() const {
    return impl_->readyCount_.load();
}

RamPreviewState ArtifactRamPreviewController::state() const {
    RamPreviewState s;
    s.totalFrames = static_cast<int>(impl_->previewEnd_ - impl_->previewStart_);
    s.readyFrames = impl_->readyCount_.load();
    s.failedFrames = impl_->failedCount_.load();
    s.isBuilding = impl_->building_.load();
    s.currentPlayheadFrame = impl_->playheadFrame_;
    s.lastErrorMessage = impl_->lastError_;

    int pending = 0, rendering = 0;
    for (const auto& fs : impl_->frameStates_) {
        switch (fs.status) {
        case RamPreviewFrameStatus::Pending: pending++; break;
        case RamPreviewFrameStatus::Rendering: rendering++; break;
        default: break;
        }
    }
    s.pendingFrames = pending;
    s.renderingFrames = rendering;
    return s;
}

bool ArtifactRamPreviewController::isBuilding() const {
    return impl_->building_.load();
}

bool ArtifactRamPreviewController::isRangeFullyReady() const {
    return impl_->readyCount_.load() >= static_cast<int>(impl_->previewEnd_ - impl_->previewStart_);
}

QVector<uint8_t> ArtifactRamPreviewController::readyBitmap() const {
    const int count = static_cast<int>(impl_->frameStates_.size());
    QVector<uint8_t> bitmap;
    bitmap.reserve(count);
    for (const auto& fs : impl_->frameStates_) {
        bitmap.push_back(fs.status == RamPreviewFrameStatus::Ready ? 1 : 0);
    }
    return bitmap;
}

} // namespace Artifact
