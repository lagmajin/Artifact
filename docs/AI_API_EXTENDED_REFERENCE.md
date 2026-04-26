# Cloud AI Widget - Extended API Reference (Phases 1-5)

**Updated**: 2026-04-26  
**Phases**: 1 (Complete), 2 (Implemented), 3-5 (Framework Ready)

## Overview

The Cloud AI widget API now supports comprehensive composition and layer manipulation:

- **Phase 1**: Composition and layer notes ✅
- **Phase 2**: Layer properties (position, scale, rotation, opacity) ✅
- **Phase 3**: Effects and masks 🔧 (Framework ready, stub implementations)
- **Phase 4**: Keyframe animation 🔧 (Framework ready, stub implementations)
- **Phase 5**: Group layer management 🔧 (Framework ready, stub implementations)

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

## Phase 3: Effects & Masks API (Framework)

All Phase 3 methods are registered and callable. Implementations currently return stub values pending effect registry clarification.

### Effects

#### `getLayerEffects(layerId: string) → QVariantList`
List effects applied to a layer.

#### `addLayerEffect(layerId: string, effectType: string) → string`
Add an effect to a layer; returns effect ID.

#### `removeLayerEffect(layerId: string, effectId: string) → bool`
Remove an effect from a layer.

#### `setLayerEffectParameter(layerId: string, effectId: string, paramName: string, value: double) → bool`
Modify an effect parameter.

**TODO**: 
- Access effect stack API
- Build effect type registry
- Implement parameter getter/setter

---

## Phase 4: Keyframe Animation API (Framework)

All Phase 4 methods are registered and callable. Implementations currently return stub values pending keyframe API clarification.

### Keyframes

#### `setKeyframe(layerId: string, propertyPath: string, frameNumber: int, value: double) → bool`
Set a keyframe for a property at a specific frame.

#### `getKeyframes(layerId: string, propertyPath: string) → QVariantList`
Get all keyframes for a property.

#### `deleteKeyframe(layerId: string, propertyPath: string, frameNumber: int) → bool`
Delete a keyframe at a specific frame.

**TODO**:
- Access timeline/keyframe storage
- Define property path syntax
- Build keyframe curve interpolation

---

## Phase 5: Group Layer API (Framework)

All Phase 5 methods are registered and callable. Implementations currently return stub values pending group layer API clarification.

### Group Management

#### `createGroupLayer(name: string) → string`
Create a new group layer; returns group layer ID.

#### `moveLayersToGroup(layerIds: string[], groupLayerId: string) → bool`
Move multiple layers into a group.

#### `ungroupLayers(groupLayerId: string) → bool`
Ungroup all layers in a group.

**TODO**:
- Access group layer creation API
- Implement layer reparenting logic
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
| 3 | Effects/Masks | 🔧 Framework | Methods registered, stubs ready for implementation |
| 4 | Keyframes | 🔧 Framework | Methods registered, stubs ready for implementation |
| 5 | Groups | 🔧 Framework | Methods registered, stubs ready for implementation |

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

