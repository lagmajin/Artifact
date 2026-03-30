# Milestone: OIIO Image Pipeline Migration (2026-03-30)

**Status:** Planned
**Goal:** Introduce an `OIIO`-based image pipeline alongside the existing `QImage` path, then gradually move load/metadata/processing/export responsibilities to the new path. The legacy `QImage` route stays alive until coverage is complete. Color management is the final phase, not the starting point.

## Summary

Current image handling is split across several responsibilities:

- `QImage` is still the dominant in-memory representation in layer code and renderer-facing code.
- `OIIO` already exists in export and utility code, but it is not the canonical ingestion path.
- Color space information is not consistently preserved through the current `QImage`-centric flow.

This milestone defines a staged migration so we can:

- keep the working `QImage` path as a fallback,
- add an `OIIO` pipeline without breaking existing editor workflows,
- preserve image metadata earlier in the flow,
- move internal processing toward a linear working space,
- and only then finish with full color management integration.
- keep `RawImage` as a thin I/O boundary, but not as the final render-path model.

## Scope

- image file loading
- image metadata preservation
- internal image buffer abstraction
- conversion boundary between `QImage` and the new pipeline
- GPU upload inputs for static image layers
- export path alignment
- color management integration as the final phase

## Non-Goals

- replacing every `QImage` usage at once
- changing unrelated video/audio pipelines
- redesigning the whole compositor in one step
- introducing a new UI model before the pipeline is stable

## Guiding Rules

- Keep `QImage` as the compatibility and UI boundary format for now.
- Use `OIIO` as the new canonical image I/O path.
- Treat colorspace as metadata, not as an implicit assumption.
- Move processing to a linear working space before any final display transform.
- Prefer small adapters over large cross-cutting rewrites.

## Proposed Model

- `QImage`
  - UI-facing compatibility format
  - legacy fallback
  - preview boundary when needed
- `RawImage`
  - file I/O / importer boundary
  - metadata-carrying source snapshot
  - not the compositor's final in-memory format
- `OIIO::ImageBuf` or a project-owned buffer abstraction
  - canonical file decode / encode path
  - metadata-rich intermediate representation
- Linear internal buffer (`ImageF32x4_RGBA` / `ImageF32xN` or a sibling typed buffer)
  - compositor / effect / GPU upload source
  - working space representation
- Color management service
  - input space conversion
  - working space policy
  - output/display transform

## Phases

### Phase 1: Boundary Audit

Inventory every place that currently assumes `QImage` is the source of truth.

Targets:

- `ArtifactImageLayer`
- asset browser previews
- composition preview upload
- software render inspectors
- export utilities

Deliverables:

- a clear map of where `QImage` must remain
- a clear map of where `OIIO` can be introduced first
- a list of metadata that must survive the pipeline

### Phase 2: OIIO Ingest Path

Add an `OIIO`-based load path while keeping the existing `QImageReader` path.

Requirements:

- decode image files through `OIIO`
- preserve file colorspace / metadata when available
- keep the existing `QImage` load path as fallback
- normalize only at the boundary where the consumer needs it

Deliverables:

- new loader API for image layers
- metadata-preserving import result type
- fallback behavior for formats or situations where `OIIO` is not enough

### Phase 3: Dual-Path Buffer Bridge

Introduce a bridge layer so internal code can consume either path.

Requirements:

- one conversion point from `OIIO` or `RawImage` or `QImage` into the internal buffer
- one conversion point back to `QImage` when the UI needs it
- no duplicate ad hoc conversions in render code

Deliverables:

- a shared linear image buffer abstraction
- reduced `convertToFormat()` scatter
- explicit channel / format handling

### Phase 4: Linear Working Space

Move internal processing to a linear working space before final output.

Requirements:

- define the working space for compositor and effects
- convert input images into that working space exactly once
- keep display conversion separate from processing conversion

Deliverables:

- documented working-space policy
- input-to-working transform helpers
- renderer and compositor updates to consume the linear buffer

### Phase 5: Color Management Integration

This is the final phase.

Requirements:

- integrate the project color science layer with the new image pipeline
- preserve input space information from decode to display
- make output/display transforms explicit
- support future expansion to ACES / OCIO-style workflows if needed

Deliverables:

- colorspace-aware import flow
- colorspace-aware export flow
- explicit input / working / output transform policy
- validation for sRGB and wide-gamut images

## Recommended Order

1. Phase 1: Boundary Audit
2. Phase 2: OIIO Ingest Path
3. Phase 3: Dual-Path Buffer Bridge
4. Phase 4: Linear Working Space
5. Phase 5: Color Management Integration

## Dependencies

- `Artifact/docs/MILESTONE_STATIC_LAYER_GPU_CACHE_2026-03-26.md`
- `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_CACHE_SYSTEM_2026-03-26.md`
- `docs/planned/MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md`

## Notes

This milestone intentionally does not force a big-bang replacement of `QImage`.
The goal is to use `OIIO` to make image metadata and color handling explicit first, then migrate the compositor and GPU-facing paths once the behavior is stable.
`RawImage` is treated as a compatibility and ingest boundary, not the final render-path image model.

The practical success condition is:

- existing editor workflows keep working,
- new imports can preserve more metadata than the old path,
- and the final output respects a well-defined color pipeline instead of relying on implicit `QImage` behavior.
