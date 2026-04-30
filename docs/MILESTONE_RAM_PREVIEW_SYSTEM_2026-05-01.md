# RAM Preview System

## Goal

After Effects のように、再生前にフレームを確保し、確保済み範囲は安定再生できる RAM preview を Artifact の playback / cache / timeline に正式導入する。

## Current State

- `ArtifactPlaybackService` already owns early RAM-preview state:
  - enable flag
  - preview range
  - cache bitmap
  - `ramPreviewStateChanged`
- `ArtifactTimelineScrubBar` already visualizes cache occupancy.
- `ArtifactVideoLayer` already has per-frame image caching and async decode.
- `ArtifactCompositionRenderWidget` and `ArtifactCompositionRenderController` already have preview tick / render dirty infrastructure.

## Current Gaps

- cache ownership is fragmented across playback service, layer caches, and render-side state
- there is no authoritative preview job queue
- cached means different things depending on layer type
- no explicit pre-roll / warmup / fill-target state exists
- playback does not switch between:
  - uncached interactive playback
  - cached guaranteed playback

## Design Direction

Introduce a dedicated RAM preview controller that sits above layer-local caches.

```cpp
struct RamPreviewFrameState {
    bool requested = false;
    bool ready = false;
    bool failed = false;
    QString reason;
};
```

```cpp
class ArtifactRamPreviewController {
public:
    void setComposition(const ArtifactCompositionPtr&);
    void setPreviewRange(const FrameRange&);
    void requestBuild();
    void cancelBuild();
    bool isFrameReady(int64_t frame) const;
};
```

## Scope Rules

- RAM preview cache is composition-scoped, not widget-scoped.
- frame readiness must mean final preview frame is usable, not just one layer cache hit.
- cache fill work must be cancellable when:
  - composition changes
  - relevant layer properties change
  - frame range changes
  - preview quality preset changes
- timeline cache bar must reflect authoritative preview readiness only.

## Phases

### Phase 1: Audit and State Freeze

- document current cache owners and invalidation paths
- define one readiness contract for a frame
- stop silent cache desync between layer cache and playback cache bitmap

### Phase 2: Central Preview Build Queue

- add a composition-level RAM preview build queue
- request frames in range order with cancellation
- expose build progress and failure reasons

### Phase 3: Playback Integration

- when RAM preview is active, playback prefers ready frames
- if range is not ready yet:
  - either stall until ready
  - or run explicit uncached preview mode
- remove ambiguous mixed behavior

### Phase 4: Diagnostics

- surface preview fill state in DebugConsole / diagnostics
- show:
  - requested frame
  - ready frame count
  - failures
  - decode bottlenecks
  - render bottlenecks

### Phase 5: Policy and UX

- add commands:
  - Preview Work Area
  - Preview From Playhead
  - Stop Preview Build
  - Clear Preview Cache
- add cache invalidation policy for edits and quality changes

## First Implementation Targets

- `Artifact/src/Service/ArtifactPlaybackService.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`

## Completion Criteria

- work area preview can be built intentionally
- cached frames are visible on the cache bar
- playback from a fully ready range is stable and deterministic
- cache invalidation is explicit and explainable
- failures are visible in diagnostics instead of being silent
