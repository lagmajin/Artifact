# Milestone: Mask Keyframe Foundation

**Status:** Investigation complete, implementation not started
**Goal:** make layer mask parameters time-addressable without rebuilding the whole mask editor

## Why this exists

Mask editing is already present as a spatial editing system, but the mask itself is still stored as plain runtime state.
That means mask shape editing works, while time-based mask animation is still missing as a first-class feature.

This milestone separates the problem into a smaller first slice:

1. make mask parameters available as properties
2. evaluate those properties at the current timeline time
3. feed the evaluated mask back into the render path

## Current State

- `MaskPath` stores `closed`, `opacity`, `feather`, `expansion`, `inverted`, and `mode` as plain values.
- `LayerMask` stores a list of `MaskPath` objects and applies them directly.
- `ArtifactAbstractLayer` exposes mask containers, but not per-mask animatable properties.
- Timeline keyframe UI already knows how to work with `AbstractProperty`, but mask parameters are not wired into that system yet.

## Recommended First Slice

### Phase 1: Mask Property Exposure

- Add a small property group for each mask path.
- Expose the basic animatable parameters:
  - `enabled`
  - `opacity`
  - `feather`
  - `expansion`
  - `inverted`
  - `mode`
- Keep geometry editing separate for now.

### Phase 2: Evaluation Bridge

- Make the render path read the evaluated mask state for the current frame.
- Keep spatial editing and time evaluation separate so the existing editor does not get tangled.

### Phase 3: UI Visibility

- Show mask properties in the property widget.
- Let the timeline show mask parameter keyframes through the existing property/keyframe pipeline.

## Non-Goals for the First Pass

- Do not keyframe every vertex immediately.
- Do not redesign `MaskPath` into a full animation system.
- Do not mix mask parameter animation with geometry editing in the same step.

## Risk Notes

- If geometry and parameter animation are tackled together, the bug surface gets too large.
- The safest path is to animate small scalar mask parameters first.
- Vertex animation should be a later phase once the property pipeline is stable.

## Suggested Next Task

Implement the property exposure for one mask path first, then prove that the current timeline time can alter the rendered mask output without breaking the existing mask editor.
