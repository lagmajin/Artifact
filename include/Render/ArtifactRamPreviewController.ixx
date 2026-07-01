module;

#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include <QObject>
#include <QString>
#include <QVector>
#include <wobjectdefs.h>

export module Artifact.Render.RamPreviewController;

export namespace Artifact {

/// Per-frame preview state.
enum class RamPreviewFrameStatus {
    None,       // Not in the build range
    Pending,    // Queued but not started
    Rendering,  // Currently being rendered
    Ready,      // Successfully rendered and cached
    Failed      // Render returned false
};

/// Reason/priority for wanting a frame rendered.
enum class RamPreviewPriority : uint8_t {
    Playhead,       // Current frame - highest
    Directional,    // Playback direction (next few frames)
    Near,           // Close to playhead
    WorkAreaFill,   // Filling the work area
    Background,     // Out-of-range / low priority
    Count           // Sentinel for array sizing
};

/// Snapshot of controller state for diagnostics.
struct RamPreviewState {
    int totalFrames = 0;
    int readyFrames = 0;
    int pendingFrames = 0;
    int renderingFrames = 0;
    int failedFrames = 0;
    int64_t currentPlayheadFrame = -1;
    bool isBuilding = false;
    QString lastErrorMessage;
};

/// Standalone RAM Preview Controller.
///
/// Manages per-frame state tracking, priority-based build ordering,
/// cancellation, and progress reporting.
///
/// Designed to work without being connected to the full render pipeline:
/// - Accepts a `RenderFrameFn` callback for actual rendering
/// - All state is internal; consume via `state()` or signals
///
/// Connect later to `ArtifactPlaybackService` and `ArtifactCompositionRenderController`.
class ArtifactRamPreviewController : public QObject {
    W_OBJECT(ArtifactRamPreviewController)

public:
    /// Signature: bool(int64_t frame) — returns true on success.
    using RenderFrameFn = std::function<bool(int64_t frame)>;

    explicit ArtifactRamPreviewController(QObject* parent = nullptr);
    ~ArtifactRamPreviewController() override;

    // --- Configuration ---

    /// Set the render callback. Called when a frame needs to be built.
    void setRenderFrameFn(RenderFrameFn fn);

    /// Set the total frame range (composition frame count).
    void setFrameRange(int64_t start, int64_t end);

    /// Set the work area (the range to preview).
    void setPreviewRange(int64_t start, int64_t end);

    /// Set the current playhead frame (for priority calculation).
    void setPlayheadFrame(int64_t frame);

    /// Set playback direction (1 = forward, -1 = backward).
    void setPlaybackDirection(int direction);

    // --- Lifecycle ---

    /// Start building the preview (fill the range).
    void startBuild();

    /// Stop building (clears pending queue).
    void stopBuild();

    /// Toggle between build and stop.
    void toggleBuild();

    /// Clear all cached state (resets frame states).
    void clearCache();

    // --- Queries ---

    /// Check if a specific frame is ready.
    bool isFrameReady(int64_t frame) const;

    /// Get the status of a specific frame.
    RamPreviewFrameStatus frameStatus(int64_t frame) const;

    /// Get the total number of ready frames.
    int readyCount() const;

    /// Get a snapshot of the current state.
    RamPreviewState state() const;

    /// Whether the controller is currently building.
    bool isBuilding() const;

    /// Whether the entire preview range is ready.
    bool isRangeFullyReady() const;

    /// Get ready frames as a bitmap (for timeline visualization).
    /// Each bit represents one frame: 1 = ready, 0 = not ready.
    QVector<uint8_t> readyBitmap() const;

    // --- Signals ---

    /// Emitted when the build queue advances.
    void buildProgress(int readyCount, int totalCount) W_SIGNAL(buildProgress, readyCount, totalCount);

    /// Emitted when the build is complete (all frames in range are ready).
    void buildComplete() W_SIGNAL(buildComplete);

    /// Emitted when a frame completes rendering.
    void frameReady(int64_t frame) W_SIGNAL(frameReady, frame);

    /// Emitted when a frame fails to render.
    void frameFailed(int64_t frame, const QString& reason) W_SIGNAL(frameFailed, frame, reason);

    /// Emitted when building starts/stops.
    void buildStateChanged(bool isBuilding) W_SIGNAL(buildStateChanged, isBuilding);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Artifact
