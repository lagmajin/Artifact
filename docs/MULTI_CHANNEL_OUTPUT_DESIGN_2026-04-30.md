# Multi-Channel Output Design

## Current State

- `ArtifactCore::ChannelType` already defines `Alpha`, `Depth`, `NormalX/Y/Z`, `VelocityX/Y`, `ObjectId`, `MaterialId`, and `Emission`.
- `ArtifactCore::MultiChannelImage` already exists as the in-memory channel container.
- `ArtifactIRenderer::readbackToMultiChannelImage()` already exports beauty RGBA plus a basic depth channel from the current render target.
- Render Queue output is still effectively beauty-first. The renderer and exporter are not yet wired around named AOV contracts.
- `OpenEXR` support exists only as a placeholder abstraction right now, so true named-channel EXR output is not complete.

## Feasibility

The feature is practical. The project already has enough type-level groundwork to support staged delivery without redesigning the renderer from scratch.

The main gap is not data modeling. The main gap is the contract between:

1. render passes
2. per-frame output packaging
3. EXR serialization

## Recommended Contract

Introduce a render-result object that is more explicit than a flat `QImage`.

```cpp
struct RenderFrameResult {
    ArtifactCore::MultiChannelImage channels;
    QSize size;
    FramePosition frame;
    bool hasBeauty = true;
    bool hasDepth = false;
};
```

Then keep the pipeline staged:

1. Renderer populates beauty RGBA first.
2. Optional passes populate `Depth`, `Alpha`, `ObjectId`, `Velocity`, and other AOVs.
3. Render Queue chooses which channels to request.
4. Export layer writes:
   - flat image formats from beauty only
   - EXR from named channels

## Suggested Phases

### Phase 1: Beauty + Alpha + Depth

- keep current beauty output working
- formalize `readbackToMultiChannelImage()` as the render queue readback path
- expose output options for:
  - beauty only
  - beauty + alpha
  - beauty + alpha + depth

### Phase 2: EXR Channel Writer

- replace the placeholder `OpenEXR` abstraction with a real channel writer
- write named EXR channels from `MultiChannelImage`
- preserve flat-image export as a separate code path

### Phase 3: Renderer AOV Expansion

- add optional AOV generation for:
  - normals
  - motion vectors
  - object/material IDs
  - emission

### Phase 4: Per-Layer / Pass Policy

- let render queue jobs request channel sets explicitly
- define which layer/render paths can truly provide each AOV
- make unsupported channels explicit instead of silently fabricating them

## Rules

- Composition background fill is editor/view state and must not contaminate beauty or auxiliary channels.
- Flat preview background and render/export content must stay separate responsibilities.
- Channel generation must be explicit. No hidden readback or hidden flattening.
