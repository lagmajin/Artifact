# Cloud AI Widget - Note API Reference

**Added**: 2026-04-26  
**Phase**: 1 (Priority)

## Overview

The Cloud AI widget now supports reading and writing composition and layer notes through the WorkspaceAutomation API. This enables AI agents to annotate projects and layers with metadata, comments, and procedural notes.

---

## New Methods

### Composition Note API

#### `getCompositionNote(compositionId: string) → string`

**Description**: Retrieve the note text of a composition.

**Parameters**:
- `compositionId` (string): The unique identifier of the composition

**Returns**: 
- Note text (string), or empty string if composition not found

**Example**:
```python
# Via JSON-RPC or chat interface
{
    "tool": "WorkspaceAutomation",
    "method": "getCompositionNote",
    "args": ["comp-001"]
}

# Returns:
"Scene 1: Wide shot of landscape. Color correction applied."
```

#### `setCompositionNote(compositionId: string, note: string) → bool`

**Description**: Set the note text of a composition.

**Parameters**:
- `compositionId` (string): The unique identifier of the composition
- `note` (string): The note text to set (can be empty)

**Returns**: 
- `true` if successful, `false` if composition not found or error occurred

**Example**:
```python
{
    "tool": "WorkspaceAutomation",
    "method": "setCompositionNote",
    "args": ["comp-001", "REVISION 2: Added color grading, removed blur effect"]
}

# Returns: true
```

**Side Effects**:
- Emits `compositionNoteChanged(QString note)` signal
- Updates internal composition state
- Does NOT trigger render or invalidate cache

---

### Layer Note API

#### `getLayerNote(layerId: string) → string`

**Description**: Retrieve the note text of a layer in the active composition.

**Parameters**:
- `layerId` (string): The unique identifier of the layer

**Returns**: 
- Note text (string), or empty string if layer not found or no active composition

**Example**:
```python
{
    "tool": "WorkspaceAutomation",
    "method": "getLayerNote",
    "args": ["layer-bg-001"]
}

# Returns:
"Background plate. Duration: 2 seconds at 24fps"
```

**Requirements**:
- An active composition must be open in the editor
- If no composition is active, returns empty string

#### `setLayerNote(layerId: string, note: string) → bool`

**Description**: Set the note text of a layer in the active composition.

**Parameters**:
- `layerId` (string): The unique identifier of the layer
- `note` (string): The note text to set (can be empty)

**Returns**: 
- `true` if successful, `false` if layer not found, no active composition, or error occurred

**Example**:
```python
{
    "tool": "WorkspaceAutomation",
    "method": "setLayerNote",
    "args": ["layer-bg-001", "Keep aspect ratio 16:9. Applied Levels effect."]
}

# Returns: true
```

**Side Effects**:
- Emits `layerNoteChanged(QString note)` signal
- Updates internal layer state
- Does NOT trigger render or invalidate cache

**Requirements**:
- An active composition must be open in the editor

---

## Use Cases

### AI-Driven Documentation
Annotate layers with AI-generated descriptions:
```
"layer-001": "Character face close-up, color corrected for warm tone"
"layer-002": "Background blur effect, 15px radius"
```

### Procedural Notes
Record AI decisions during automation:
```
"comp-002": "Generated 8-layer animation. Keyframes set at 0, 30, 60, 90 frames."
```

### Workflow Metadata
Tag compositions and layers for downstream processing:
```
"comp-003": "Ready for export. Format: ProRes422 HQ, 1920x1080, 30fps"
```

---

## Implementation Details

### Architecture

- **Composition Notes**: Stored in `ArtifactAbstractComposition` instance
  - Accessed via `ArtifactProjectService::compositionById(CompositionID)`
  - Persisted when project is saved
  
- **Layer Notes**: Stored in `ArtifactAbstractLayer` instance
  - Accessed via active composition's layer lookup
  - Persisted when project is saved

### Error Handling

All methods gracefully degrade:
- Missing composition/layer → return empty/false (never throw)
- No active composition (layer ops) → return empty/false
- Service unavailable → return empty/false

### Performance

- Note operations are **O(1)** lookups + string assignment
- No cache invalidation or render pipeline invocation
- Safe to call frequently without performance impact

---

## API Availability

These methods appear in the AI tool descriptions registry:
- Tool name: `WorkspaceAutomation`
- Method names: `getCompositionNote`, `setCompositionNote`, `getLayerNote`, `setLayerNote`
- Available to: Cloud AI agents, local AI agents, or any IDescribable client

---

## Related Methods

### Composition Management
- `createComposition(name, width, height) → QVariantMap`
- `renameComposition(compositionId, newName) → bool`
- `duplicateComposition(compositionId) → QVariantMap`
- `removeCompositionWithRenderQueueCleanup(compositionId) → bool`

### Layer Management
- `renameLayerInCurrentComposition(layerId, newName) → bool`
- `selectLayer(layerId) → bool`
- `setLayerVisibleInCurrentComposition(layerId, visible) → bool`
- `setLayerLockedInCurrentComposition(layerId, locked) → bool`

### Snapshots (for context)
- `currentCompositionSnapshot() → QVariantMap`
- `listCurrentCompositionLayers() → QVariantList`

---

## Future Enhancements (Phase 2+)

Candidate follow-up methods:
- `getLayerProperties(compositionId, layerId, properties) → QVariantMap`
  - Position, scale, rotation, opacity
- `setLayerProperties(compositionId, layerId, properties) → bool`
- `getLayerEffects(compositionId, layerId) → QVariantList`
- `addLayerEffect(compositionId, layerId, effectType, params) → QString`

---

## Testing

**Manual Verification**:
1. Open a project with composition and layers
2. In Cloud AI widget, send: `"Get the note of composition X"`
3. AI should call `getCompositionNote()` and return the note text
4. Send: `"Set composition X note to 'Test Note'"`
5. AI should call `setCompositionNote()` and return true
6. Verify notes persist after project reload

**Automated Tests** (if applicable):
- Unit tests in `tests/AI/WorkspaceAutomationTest.cpp`
- Integration tests with active composition fixtures

---

## Implementation Files

- **Interface**: `Artifact/include/AI/WorkspaceAutomation.ixx` (lines 125-128, 289-300, 1147-1220)
- **Implementation**: All methods are inline static in `.ixx`
- **Signals**: `ArtifactAbstractComposition::compositionNoteChanged()`, `ArtifactAbstractLayer::layerNoteChanged()`

---

## Version History

| Version | Date | Change |
|---------|------|--------|
| 1.0 | 2026-04-26 | Initial implementation: 4 methods (composition/layer note get/set) |

