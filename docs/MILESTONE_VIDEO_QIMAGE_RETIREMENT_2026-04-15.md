# Milestone: Video QImage Retirement (2026-04-15)

**Status:** Planned

## Goal

Remove `QImage` from the video layer hot path while keeping the editor responsive, the composition view stable, and the video pipeline suitable for professional color-managed work.

This milestone is specifically about:

- `ArtifactVideoLayer`
- composition/preview rendering of video
- GPU upload and cache boundaries for moving footage
- decoder metadata preservation
- explicit input/working/output color handling for video

## Current State

Video is still fundamentally `QImage`-first today.

- `ArtifactVideoLayer` stores decoded frames in `currentQImage_`.
- `ArtifactVideoLayer::currentFrameToQImage()` is the main composition-facing entry point.
- `ArtifactCompositionRenderController` and `ArtifactPreviewCompositionPipeline` both render video by pulling a `QImage`.
- GPU texture caching happens after the `QImage` stage, not before it.

This keeps the path simple, but it also means:

- the render path is tied to a UI-oriented image type,
- source color metadata is easy to lose,
- higher bit depth and YUV-native workflows are harder to preserve,
- CPU conversion cost stays in the hot path longer than necessary.

## Non-Goals

- rewriting every decoder backend at once
- removing `QImage` from all image and UI code in one pass
- redesigning the whole compositor around video first
- blocking current shipping workflows until the final color pipeline lands

## Guiding Rules

- Keep `QImage` only as a compatibility boundary during migration, not as the canonical runtime video frame type.
- Preserve source metadata from decode onward: primaries, transfer, matrix, range, bit depth, chroma model, alpha mode, and frame timing.
- Separate decode conversion from compositing conversion and from display conversion.
- Define one explicit working-space policy for the compositor instead of inheriting implicit `QImage` behavior.
- Ship with fallback paths during rollout; do not require a big-bang cutover.

## Proposed Runtime Model

### Canonical frame model

Introduce a project-owned video frame surface type in Core, for example `VideoFrameSurface`, carrying:

- frame size
- pixel format / plane layout
- timestamps / frame number
- source color metadata
- either CPU-visible planes or GPU-ready upload data

This becomes the canonical output of video decode.

### Boundary model

- `QImage`
  - UI/debug compatibility only
  - optional fallback adapter
- `VideoFrameSurface`
  - canonical decoded video frame representation
  - metadata-preserving bridge between decoder and renderer
- linear working buffer / GPU texture
  - compositor-facing representation
  - explicit conversion target for effects and blending

## Work Packages

### WP-1 Video Boundary Audit

Map every place where video still assumes `QImage` is the source of truth.

Targets:

- `ArtifactVideoLayer`
- `ArtifactCompositionRenderController`
- `ArtifactPreviewCompositionPipeline`
- GPU texture cache integration
- proxy generation / proxy playback
- export and debug-frame helpers

Deliverables:

- a precise inventory of `QImage` dependencies
- a list of required metadata that must survive migration
- a list of places where `QImage` may remain as a compatibility adapter

### WP-2 Core VideoFrameSurface

Add a Core-owned frame representation for decoded video.

Requirements:

- represent packed RGB and multi-plane YUV inputs
- preserve frame timing and decode identifiers
- preserve input color metadata explicitly
- support conversion adapters to `QImage` only when a UI consumer requires it

Deliverables:

- `VideoFrameSurface` type
- conversion helpers for legacy callers
- one explicit adapter from `VideoFrameSurface` to existing cache/upload code

### WP-3 Decoder Bridge Migration

Change the video decode path so decoders produce `VideoFrameSurface` first.

Requirements:

- keep existing playback backends working
- allow backend-specific native formats internally
- avoid silently collapsing everything to 8-bit sRGB `QImage`
- keep async decode / scrub responsiveness

Deliverables:

- decoder-to-surface bridge
- legacy `QImage` fallback path
- metadata-preserving frame handoff from decoder to layer

### WP-4 Renderer Upload And Cache Path

Move video rendering to a GPU-native upload path.

Requirements:

- upload from `VideoFrameSurface` without routing through `QImage`
- integrate with `GPUTextureCacheManager` or a dedicated video texture path
- support both CPU-converted RGB upload and future YUV-native sampling
- keep proxy-quality switching and cache invalidation explicit

Deliverables:

- shared upload helper for video surfaces
- composition/preview render path using the new surface
- reduced `QImage` upload usage in the video hot path

### WP-5 Color-Managed Video Pipeline

Make video color handling explicit enough for professional workflows.

Requirements:

- preserve source tags such as Rec.709 / sRGB / Display P3 / Rec.2020 where available
- preserve transfer characteristics and full-vs-limited range
- define the compositor working space and when linearization happens
- keep display/output transforms separate from decode and compositing
- do not bake display assumptions into cache keys or decode outputs

Deliverables:

- explicit input-to-working conversion policy
- explicit working-to-display/output conversion policy
- validation assets for SDR and wide-gamut footage
- a migration path toward OCIO/ACES-style workflow integration

### WP-6 Composition Integration Cutover

Switch composition and preview video rendering to the new surface path.

Targets:

- `ArtifactVideoLayer::draw()`
- `ArtifactCompositionRenderController`
- `ArtifactPreviewCompositionPipeline`

Requirements:

- first visible frame appears without waiting for unrelated repaint triggers
- scrubbing and playback stay responsive
- opacity, masks, effects, and transforms still work
- fallback path remains available during rollout

### WP-7 QImage Retirement

Remove `QImage` as the canonical render-path representation for video.

Keep only narrow adapters where justified, such as:

- debug snapshot export
- legacy inspectors that have not migrated yet
- temporary compatibility shims guarded by clear ownership

## Recommended Order

1. `WP-1 Video Boundary Audit`
2. `WP-2 Core VideoFrameSurface`
3. `WP-3 Decoder Bridge Migration`
4. `WP-4 Renderer Upload And Cache Path`
5. `WP-5 Color-Managed Video Pipeline`
6. `WP-6 Composition Integration Cutover`
7. `WP-7 QImage Retirement`

## Dependencies

- `Artifact/docs/MILESTONE_VIDEO_LAYER_UNIFICATION_2026-03-13.md`
- `Artifact/docs/MILESTONE_OIIO_IMAGE_PIPELINE_MIGRATION_2026-03-30.md`
- `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_CACHE_SYSTEM_2026-03-26.md`

## Validation Checklist

- first frame of an imported video appears reliably in the composition view
- timeline scrubbing updates visible frames without requiring incidental repaint
- playback/proxy switching keeps frame identity and cache invalidation correct
- video opacity, transforms, and masks still affect rendered output
- source color metadata is preserved instead of silently dropped at decode
- mixed-footage comps do not assume every source is sRGB 8-bit RGB
- the hot path no longer requires `QImage` for normal video rendering
