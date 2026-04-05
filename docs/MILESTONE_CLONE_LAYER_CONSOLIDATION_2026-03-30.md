# Milestone: Clone Layer Consolidation (2026-03-30)

**Status:** Planned
**Goal:** Move MoGraph-style features into a new `Clone Layer`, and remove the legacy `LayerTransform` rack from the normal Inspector flow.

## Current State

1. `Generator` - keep
2. `Geometry Transform` - keep, but MoGraph elements may be separated
3. `Material & Render` - keep
4. `Rasterizer` - keep
5. `Layer Transform` - remove

## Open Questions

- Should MoGraph elements be split out from `Geometry Transform`?
  - Option A: add a 6th rack
  - Option B: keep them inside `Generator`
- What is the final `CloneLayer` design?
  - rack-based for now
  - or move to layer-centric editing
- How much of the post-clone effect chain should still be editable?

## Target Direction

- MoGraph behavior belongs to `Clone Layer`, not the old `LayerTransform` rack
- clone generation and clone-specific parameters should live in that layer
- the Inspector should only show supported racks
- legacy data should still open, but the old rack should not remain in the default editing path

## Scope

- `Clone Layer` design and runtime behavior
- Inspector rack cleanup
- clone editing / clone parameter flow
- preview and solo view behavior for clone output
- migration of old `LayerTransform` data

## Recommended Order

1. Decide MoGraph placement: `Geometry Transform` split vs `Generator` integration
2. Define `Clone Layer` editing model
3. Remove `LayerTransform` from the Inspector
4. Add migration for old projects

## Dependencies

- `Artifact/docs/MILESTONE_MOGRAPH_GENERATOR_2026-03-17.md`
- `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_CACHE_SYSTEM_2026-03-26.md`
- `Artifact/docs/MILESTONE_STATIC_LAYER_GPU_CACHE_2026-03-26.md`
