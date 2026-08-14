# Cloud AI Widget - Extended API Reference (Phases 1-5)

**最終更新:** 2026-08-14
**Phases**: 1 (Complete), 2 (Implemented), 3 (Partial), 4-5 (Framework Ready)

## Overview

The Cloud AI widget API now supports comprehensive composition and layer manipulation:

- **Phase 1**: Composition and layer notes ✅
- **Phase 2**: Layer properties (position, scale, rotation, opacity) ✅
- **Phase 3**: Effects and masks 🟡 (core effect operations available)
- **Phase 4**: Keyframe animation ✅ (Layer and effect keyframe read/write/delete APIs)
- **Phase 5**: Group layer management ✅ (Create, move, and ungroup operations)

## AI Agent Safety Contract

Before an agent edits the project, it should call the read-only preflight endpoint:

```json
{
  "tool": "WorkspaceAutomation",
  "method": "agentPreflight",
  "arguments": []
}
```

The result contains the current `workspace` snapshot, `diagnostics`, an
`observedAtUtc` timestamp, and the versioned `contract`. The contract describes
the required order:

1. Inspect the current state and resolve stable target IDs.
2. Validate the command with `validateCommand`.
3. Use a preview/dry-run and explicit confirmation for high-risk writes.
4. Execute the command.
5. Verify the result with a snapshot or diagnostics before reporting completion.

`agentContract()` can be called independently when only the protocol is needed.

The Python bridge exposes the same read-only handshake as
`artifact.workspace.agentPreflight()`, returning the compact JSON result.

---

## Phase 1: Notes API

### Composition Note API

#### `getCompositionNote(compositionId: string) → string`
Retrieve composition note text.

#### `setCompositionNote(compositionId: string, note: string) → bool`
Set composition note text.

### Layer Note API

#### `getLayerNote(layerId: string) → string`
Retrieve layer note from active composition.

#### `setLayerNote(layerId: string, note: string) → bool`
Set layer note in active composition.

See detailed Phase 1 docs in `AI_API_CLOUD_WIDGET_NOTES.md`.

---

## Phase 2: Layer Properties API

### Position

#### `getLayerPosition(layerId: string) → { x: double, y: double }`

**Description**: Get the X/Y position of a layer in the active composition.

**Returns**:
```json
{
  "x": 100.5,
  "y": 200.0
}
```

**Example**:
```python
{
    "tool": "WorkspaceAutomation",
    "method": "getLayerPosition",
    "args": ["layer-001"]
}
```

#### `setLayerPosition(layerId: string, x: double, y: double) → bool`

**Description**: Set the X/Y position of a layer.

**Parameters**:
- `layerId`: Layer ID
- `x`: New X position (pixels)
- `y`: New Y position (pixels)

**Example**:
```python
{
    "tool": "WorkspaceAutomation",
    "method": "setLayerPosition",
    "args": ["layer-001", 150.0, 250.5]
}
# Returns: true
```

**Side Effects**:
- Updates layer transform immediately
- Does NOT create keyframes (sets base value)
- Triggers render update via existing debounce

---

### Scale

#### `getLayerScale(layerId: string) → { x: double, y: double }`

**Description**: Get the X/Y scale factors of a layer (1.0 = 100%).

**Returns**:
```json
{
  "x": 1.5,
  "y": 1.0
}
```

#### `setLayerScale(layerId: string, sx: double, sy: double) → bool`

**Description**: Set the X/Y scale factors of a layer.

**Parameters**:
- `sx`: Scale X (1.0 = 100%)
- `sy`: Scale Y (1.0 = 100%)

**Example**:
```python
{
    "tool": "WorkspaceAutomation",
    "method": "setLayerScale",
    "args": ["layer-001", 2.0, 1.5]
}
# Returns: true
```

---

### Rotation

#### `getLayerRotation(layerId: string) → double`

**Description**: Get the rotation angle of a layer in degrees (0-360 or beyond).

**Returns**: Rotation in degrees as double

**Example**:
```python
{
    "tool": "WorkspaceAutomation",
    "method": "getLayerRotation",
    "args": ["layer-001"]
}
# Returns: 45.5
```

#### `setLayerRotation(layerId: string, rotation: double) → bool`

**Description**: Set the rotation angle of a layer in degrees.

**Parameters**:
- `rotation`: Angle in degrees (can be > 360 or negative)

**Example**:
```python
{
    "tool": "WorkspaceAutomation",
    "method": "setLayerRotation",
    "args": ["layer-001", 90.0]
}
# Returns: true
```

---

### Opacity

#### `getLayerOpacity(layerId: string) → double`

**Description**: Get the opacity of a layer (0-100, where 100 = fully opaque).

**Returns**: Opacity percentage as double

**Example**:
```python
{
    "tool": "WorkspaceAutomation",
    "method": "getLayerOpacity",
    "args": ["layer-001"]
}
# Returns: 75.0
```

#### `setLayerOpacity(layerId: string, opacity: double) → bool`

**Description**: Set the opacity of a layer (0-100).

**Parameters**:
- `opacity`: Opacity percentage (0 = transparent, 100 = opaque)

**Example**:
```python
{
    "tool": "WorkspaceAutomation",
    "method": "setLayerOpacity",
    "args": ["layer-001", 50.0]
}
# Returns: true
```

---

## Phase 3: Effects & Masks API

The core effect surface is now callable through `WorkspaceAutomation`.

### Effects

#### `getLayerEffects(layerId: string) → QVariantList`
List effects applied to a layer.

#### `getEffectRegistryMetadata() → QVariantList`
List registered effects with availability, pipeline stage, GPU capability, and parameter count.

#### `getLayerEffectParameters(layerId: string, effectId: string) → QVariantList`
Return editable parameter descriptors, current values, ranges, expressions, and keyframe counts.

#### `addLayerEffect(layerId: string, effectType: string) → string`
Add an effect to a layer; returns effect ID.

#### `removeLayerEffect(layerId: string, effectId: string) → bool`
Remove an effect from a layer.

#### `setLayerEffectParameter(layerId: string, effectId: string, paramName: string, value: double) → bool`
Modify an effect parameter.

#### `setLayerEffectParameterKeyframe(layerId: string, effectId: string, paramName: string, frame: int, value: QVariant) → bool`
Write an animatable effect parameter at a timeline frame.

#### `getLayerEffectParameterKeyframes(layerId: string, effectId: string, paramName: string) → QVariantList`
Read effect parameter keyframes, including interpolation and handles.

#### `removeLayerEffectParameterKeyframe(layerId: string, effectId: string, paramName: string, frame: int) → bool`
Remove an effect parameter keyframe at a timeline frame.

#### `setLayerEffectParameterExpression(layerId: string, effectId: string, paramName: string, expression: string) → bool`
Set or clear an expression on an animatable effect parameter.

#### `setLayerEffectEnabled(layerId: string, effectId: string, enabled: bool) → bool`
Enable or disable an effect.

#### `moveLayerEffect(layerId: string, effectId: string, direction: int) → bool`
Move an effect within the stack. Use a negative or positive direction.

#### `duplicateLayerEffect(layerId: string, effectId: string) → string`
Duplicate an effect and return the new effect id.

#### `saveLayerEffectPreset(layerId: string, effectId: string, filePath: string) → bool`
Save the current effect state to a preset file.

#### `loadLayerEffectPreset(layerId: string, effectId: string, filePath: string) → bool`
Load a preset file into an existing layer effect.

#### `listLayerEffectPresets(directoryPath: string) → QVariantList`
List available preset files in a directory.

#### `recentLayerEffectPresets(limit: int) → QVariantList`
List recently used effect presets.

#### `workspaceDiagnostics() → QVariantMap`
Return a compact status summary for the current workspace.

**Remaining**:
- Registry metadata can be expanded with user-facing category and version fields.

---

## Phase 4: Keyframe Animation API

All Phase 4 methods are registered and callable; the initial keyframe storage path is implemented.

### Keyframes

The keyframe methods are backed by the active layer/property store and return
structured results; they are no longer placeholder registrations.

#### `setKeyframe(layerId: string, propertyPath: string, frameNumber: int, value: double) → bool`
Set a keyframe for a property at a specific frame.

#### `getKeyframes(layerId: string, propertyPath: string) → QVariantList`
Get all keyframes for a property.

#### `deleteKeyframe(layerId: string, propertyPath: string, frameNumber: int) → bool`
Delete a keyframe at a specific frame.

#### `getLayerKeyframeSummary(layerId: string) → QVariantMap`
Return a compact summary of keyframed properties for a layer.

#### `batchSetKeyframes(layerId: string, keyframes: QVariantList) → QVariantMap`
Set multiple keyframes from a JSON-style array of `{ propertyPath, frameNumber, value }` objects.

### Command IR / Automation Facade

The `WorkspaceAutomation` tool now also exposes a command-oriented facade for AI, MCP, and DSL layers.

#### `commandVocabulary() → QVariantList`
Return the supported command IR vocabulary and the required fields for each command.

#### `validateCommand(command: QVariantMap) → QVariantMap`
Validate a command IR request without mutating workspace state.

#### `executeCommand(command: QVariantMap) → QVariantMap`
Execute a validated command IR request through the automation facade.

Supported initial command types:

- `set_property`
- `set_keyframes`
- `batch_set_keyframes`
- `move_layer`
- `rename_layer`
- `add_effect`

For keyframe commands, the facade accepts compact `time` inputs for DSL/MCP convenience, but the underlying snapshot path preserves `timeValue` / `timeScale` so undo and round-tripping do not collapse the original time base.

Future optional helpers such as `preview` / `explain` can sit on top of the same command IR, but they are not required for the initial contract.

**Remaining**:
- Expand property-path coverage and curve interpolation options.

---

## Phase 5: Group Layer API

All Phase 5 methods are registered and callable; the initial group/project-item operation path is implemented.

### Group Management

Group creation, reparenting, ungrouping, and batch project-item operations are
implemented through the workspace automation facade.

#### `createGroupLayer(name: string) → string`
Create a new group layer; returns group layer ID.

#### `moveLayersToGroup(layerIds: string[], groupLayerId: string) → bool`
Move multiple layers into a group.

#### `ungroupLayers(groupLayerId: string) → bool`
Ungroup all layers in a group.

#### `batchRenameProjectItems(items: QVariantList) → QVariantMap`
Rename multiple project items from a JSON-style array of `{ itemId, newName }` objects.

#### `batchMoveProjectItemsToFolder(itemIds: string[], parentFolderId: string) → QVariantMap`
Move multiple project items into a folder.

- Handle nested groups

---

## Design Decisions

### Phase 2: Why Transform2D Only?

Layer transforms are in 2D space for most operations. 3D transforms (position3D, rotation3D) will be added in Phase 2b if needed.

### Phase 2: No Keyframe Creation

Transform methods set the **base value**, not keyframes:
- AI can adjust static layer properties without animation complexity
- Keyframe creation deferred to Phase 4
- Aligns with user mental model: "Set layer position to X"

### Phase 3-5: Stub Implementations

Phases 3-5 have:
- Full method signatures registered in tool descriptions
- Stub implementations (return empty/false)
- TODO comments indicating what's needed
- Design locked, allowing incremental implementation

This enables:
- API surface complete now
- AI sees all methods immediately
- Implementation can proceed without breaking API contracts

---

## Usage Examples

### AI Automation: Pan and Fade

```
User: "Create a 3-second pan from left to right and fade out the background layer"

AI executes:
1. Get composition frame rate → 24fps, so 3 sec = 72 frames
2. Get current background layer position → (0, 360)
3. Set keyframes:
   - Frame 0: position (0, 360)
   - Frame 72: position (1920, 360)
4. Get opacity → 100
5. Set keyframes:
   - Frame 0: opacity 100
   - Frame 72: opacity 0
6. Return confirmation: "Pan and fade set for background layer"
```

### AI Analysis: Report Layer Properties

```
User: "Tell me the current state of each layer"

AI executes:
1. List layers → ["bg", "char", "text"]
2. For each layer:
   - Get position
   - Get scale
   - Get rotation
   - Get opacity
3. Return summary:
   "bg: pos(0, 360), scale(1.0, 1.0), rot(0°), opacity(100%)
    char: pos(500, 200), scale(1.2, 1.0), rot(0°), opacity(100%)
    text: pos(300, 100), scale(1.0, 1.0), rot(5°), opacity(80%)"
```

---

## Implementation Status

| Phase | Feature | Status | Details |
|-------|---------|--------|---------|
| 1 | Notes | ✅ Complete | Both composition and layer notes working |
| 2 | Properties | ✅ Implemented | Position, scale, rotation, opacity working |
| 3 | Effects/Masks | 🟡 Partial | Core effect operations exposed |
| 4 | Keyframes | ✅ Implemented | Layer and effect keyframe operations are registered and callable |
| 5 | Groups | ✅ Implemented | Group creation, layer moves, and ungrouping are registered and callable |

---

## Performance Notes

**Phase 2 Performance**:
- Each get/set is O(1) lookup + property access
- No render invalidation (uses existing debounce)
- Safe to batch multiple property changes

**Recommended Pattern**:
```python
# Good: Set multiple properties at once
setLayerPosition(layerId, 100, 200)
setLayerScale(layerId, 2.0, 1.5)
setLayerOpacity(layerId, 80.0)
# Result: Single render update

# Avoid: Separate each operation if possible
# (Each method completes in <1ms anyway)
```

---

## Files

- **Implementation**: `Artifact/include/AI/WorkspaceAutomation.ixx` (modified)
- **API Reference**: `docs/AI_API_CLOUD_WIDGET_NOTES.md` (Phase 1 details)
- **This Document**: `docs/AI_API_EXTENDED_REFERENCE.md` (all phases)

---

## Next: Implementation Priorities

### Phase 3 (Effects): High Priority
- Effects are frequently used in composition editing
- Effect stack API is available; just needs wrapping
- Estimated time: 30-60 minutes for core operations

### Phase 4 (Keyframes): Medium Priority
- Keyframe animation is powerful but complex
- Requires timeline management; bigger implementation
- Estimated time: 60-120 minutes

### Phase 5 (Groups): Lower Priority
- Group layers less frequently used in AI workflows
- Design decisions needed around nested groups
- Can defer until user requests

---

## Version History

| Version | Date | Change |
|---------|------|--------|
| 2.0 | 2026-04-26 | Added Phase 2 (layer properties) + Phases 3-5 framework |
| 1.0 | 2026-04-26 | Phase 1 (composition/layer notes) |

