# Cloud AI Phase 4: Keyframe Animation API (2026-04-27)

## Summary

Cloud AI Phase 4 implements keyframe animation control for layer properties. The API allows external AI tools to set, query, and delete keyframes for animatable properties like position, rotation, scale, and anchor.

**Status**: ✅ COMPLETED (initial implementation)

## Implementation Details

### Architecture

- **Backend**: Uses `ArtifactTimelineKeyframeModel` - the existing timeline keyframe management system
- **Frontend**: Exposes 3 methods in `WorkspaceAutomation` for AI external tool integration
- **Property Paths**: Supports transform-based paths (position, rotation, scale, anchor)
- **Frame Basis**: Frame numbers (0, 1, 2, ...) converted to RationalTime internally

### Supported Property Paths

Transform properties (2D animations):
- `transform.position.x`
- `transform.position.y`
- `transform.rotation`
- `transform.scale.x`
- `transform.scale.y`
- `transform.anchor.x`
- `transform.anchor.y`

### API Methods

#### setKeyframe(layerId: QString, propertyPath: QString, frameNumber: int, value: double) → QVariant

**Purpose**: Create or update a keyframe at a specific frame

**Returns**:
```json
{
  "success": bool,
  "keyframeId": string (frame number),
  "error": string (if failed)
}
```

**Example**:
```python
# Move layer to (100, 200) at frame 120
result = WorkspaceAutomation.setKeyframe("layer1", "transform.position.x", 120, 100.0)
result = WorkspaceAutomation.setKeyframe("layer1", "transform.position.y", 120, 200.0)
```

**Implementation**:
1. Validates composition and layer exist
2. Uses `KeyframeModel.addKeyframe()` to store keyframe
3. Sets interpolation to Linear (default)
4. Triggers layer change event notification

#### getKeyframes(layerId: QString, propertyPath: QString) → QVariant

**Purpose**: Query all keyframes for a property

**Returns**:
```json
[
  {
    "frame": int,
    "value": double,
    "interpolation": string ("Linear", "Bezier", "EaseIn", "EaseOut")
  },
  ...
]
```

**Example**:
```python
# Get all position X keyframes
keyframes = WorkspaceAutomation.getKeyframes("layer1", "transform.position.x")
# Result: [{"frame": 0, "value": 50.0, "interpolation": "Linear"},
#          {"frame": 120, "value": 100.0, "interpolation": "Linear"}]
```

**Implementation**:
1. Validates composition and layer
2. Uses `KeyframeModel.getKeyframesFor()` to query
3. Converts KeyFrame structs to QVariantList JSON
4. Preserves interpolation type in output

#### deleteKeyframe(layerId: QString, propertyPath: QString, frameNumber: int) → QVariant

**Purpose**: Remove a keyframe at a specific frame

**Returns**:
```json
{
  "success": bool,
  "error": string (if failed)
}
```

**Example**:
```python
# Remove keyframe at frame 120
result = WorkspaceAutomation.deleteKeyframe("layer1", "transform.position.x", 120)
```

**Implementation**:
1. Validates composition and layer
2. Uses `KeyframeModel.removeKeyframe()` to delete
3. Triggers layer change event notification

## Technical Details

### Frame Number Handling

Frame numbers are converted to `RationalTime` with a base framerate of 30 fps:
```cpp
RationalTime time(frameNumber, 30);
```

This can be made configurable in future versions if needed for composition-specific frame rates.

### Integration with Existing Systems

- **Timeline**: Keyframes are stored in the same backend as the Timeline UI, ensuring consistency
- **Layer Properties**: Uses `layer->getProperty(propertyPath)` API from existing property system
- **Event Notifications**: Publishes `LayerChangedEvent` when keyframes are modified
- **Undo/Redo**: Leverages existing undo infrastructure via property API

### Limitations (Phase 4)

- Linear interpolation is hardcoded for `setKeyframe` (Bezier control points not exposed yet)
- Only transform properties supported (other property types can be added later)
- Frame rate is fixed at 30 fps (can be made composition-specific)

## How to Use (AI Tool Integration)

### Example: Animate layer opacity over 10 frames

```python
# AI widget pseudocode
composition = get_active_composition()
layer = composition.find_layer("background")

# Create keyframe at frame 0 (opacity 100%)
WorkspaceAutomation.setKeyframe(layer.id, "opacity", 0, 100.0)

# Create keyframe at frame 300 (opacity 0%)
WorkspaceAutomation.setKeyframe(layer.id, "opacity", 300, 0.0)

# Query animation curve
keyframes = WorkspaceAutomation.getKeyframes(layer.id, "opacity")
print(f"Animation: {keyframes}")
```

### Example: Modify animation

```python
# Check current keyframes
kfs = WorkspaceAutomation.getKeyframes("layer1", "transform.position.x")

# Update a keyframe value (delete + recreate)
for kf in kfs:
  if kf["frame"] == 120:
    WorkspaceAutomation.deleteKeyframe("layer1", "transform.position.x", 120)
    WorkspaceAutomation.setKeyframe("layer1", "transform.position.x", 120, 150.0)
    break
```

## Related Documentation

- **Timeline Keyframe Model**: `Artifact/include/Widgets/Timeline/ArtifactTimelineKeyframeModel.ixx`
- **Property System**: `ArtifactCore/include/Property/Abstract.ixx`
- **Event System**: `ArtifactCore/include/Event/Types.ixx`

## Next Steps (Phase 4+)

1. **Bezier Interpolation** - Expose `addKeyframeWithBezier()` via API
2. **Property Expansion** - Add opacity, blur, and other animated properties
3. **Ease Functions** - Support EaseIn, EaseOut, EaseInOut
4. **Composition Frame Rate** - Make frame rate configurable per composition
5. **Animation Curves UI** - Add overlay visualization in composition view

---

**Implementation Date**: 2026-04-27  
**Implementer**: Copilot  
**Code Location**: 
- API Methods: `Artifact/include/AI/WorkspaceAutomation.ixx:1546-1676`
- Backend Service: `Artifact/include/Widgets/Timeline/ArtifactTimelineKeyframeModel.ixx`
- Implementation: `Artifact/src/Widgets/Timeline/ArtifactTimelineKeyframeModel.cppm:75-216`
