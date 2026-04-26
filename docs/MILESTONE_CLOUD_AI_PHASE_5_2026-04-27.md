# Cloud AI Phase 5: Group Layers API (2026-04-27)

## Summary

Cloud AI Phase 5 implements layer grouping functionality for AI-driven composition manipulation. The API allows external tools to create group layers, move layers into groups, and ungroup layers back to the main composition.

**Status**: ✅ COMPLETED (initial implementation)

## Implementation Details

### Architecture

- **Backend**: Uses `ArtifactGroupLayer` - the existing group layer implementation
- **Frontend**: Exposes 3 methods in `WorkspaceAutomation` for AI external tool integration
- **Composition Integration**: Properly manages layer hierarchy and notifies composition of changes
- **Event Publishing**: Sends `LayerChangedEvent` for all group operations

### Supported Operations

#### createGroupLayer(name: QString) → QVariant

**Purpose**: Create a new empty group layer in the active composition

**Returns**:
```json
{
  "success": bool,
  "groupLayerId": string,
  "error": string (if failed)
}
```

**Example**:
```python
# Create a new group layer for background elements
result = WorkspaceAutomation.createGroupLayer("Background Group")
if result["success"]:
    group_id = result["groupLayerId"]
```

**Implementation**:
1. Creates new `ArtifactGroupLayer` instance
2. Sets layer name from parameter
3. Adds group to composition at top level
4. Publishes `LayerChangedEvent::Created`
5. Returns group layer ID

**Limitations**:
- Group is always added at top (composition layer stack)
- Default layer name is "Layer Group" if empty

#### moveLayersToGroup(layerIds: QStringList, groupLayerId: QString) → QVariant

**Purpose**: Move multiple layers from composition into a group layer

**Returns**:
```json
{
  "success": bool,
  "movedCount": int,
  "error": string (if failed)
}
```

**Example**:
```python
# Group multiple layers together
layer_ids = ["layer1", "layer2", "layer3"]
result = WorkspaceAutomation.moveLayersToGroup(layer_ids, group_id)
print(f"Moved {result['movedCount']} layers into group")
```

**Implementation**:
1. Validates group layer exists and is a `ArtifactGroupLayer`
2. For each layer in list:
   - Remove from composition
   - Add to group's children
   - Publish `LayerChangedEvent::Modified`
3. Publishes final group `LayerChangedEvent::Modified`
4. Returns count of successfully moved layers

**Limitations**:
- Silently skips non-existent layers
- Does not validate circular nesting (group inside group)

#### ungroupLayers(groupLayerId: QString) → QVariant

**Purpose**: Ungroup all layers in a group, moving them back to the composition

**Returns**:
```json
{
  "success": bool,
  "unGroupedCount": int,
  "error": string (if failed)
}
```

**Example**:
```python
# Dissolve a group layer and restore children
result = WorkspaceAutomation.ungroupLayers(group_id)
print(f"Ungrouped {result['unGroupedCount']} layers")
```

**Implementation**:
1. Validates group layer exists and is a `ArtifactGroupLayer`
2. Saves children list
3. For each child:
   - Remove from group
   - Add back to composition
   - Publish `LayerChangedEvent::Modified`
4. Removes the now-empty group layer
5. Publishes group `LayerChangedEvent::Deleted`
6. Returns count of ungrouped layers

**Result**:
- All child layers restored to composition layer stack
- Group layer removed entirely

## Technical Details

### Layer Hierarchy

Group layers form a simple two-level hierarchy:

```
Composition
├── Layer 1
├── Layer 2
├── Group Layer
│   ├── Child 1
│   ├── Child 2
│   └── Child 3
└── Layer 3
```

When ungrouping, all children move back to composition level:

```
Composition
├── Layer 1
├── Layer 2
├── Child 1
├── Child 2
├── Child 3
└── Layer 3
```

### Event System Integration

All operations publish events:

- **createGroupLayer**: `LayerChangedEvent::Created` (group layer)
- **moveLayersToGroup**: 
  - `LayerChangedEvent::Modified` (each moved layer)
  - `LayerChangedEvent::Modified` (group layer)
- **ungroupLayers**:
  - `LayerChangedEvent::Modified` (each ungrouped layer)
  - `LayerChangedEvent::Deleted` (group layer)

Events trigger:
- UI refresh in layer tree
- Timeline updates
- Composition state tracking

### Rendering

Group layers render via `ArtifactGroupLayer::draw()`:
- Child layers drawn into offscreen texture
- Group effects applied (future feature)
- Final composite blitted to parent target
- Supports blend modes and opacity

## How to Use (AI Tool Integration)

### Example: Organize layers by category

```python
# Create groups for different layer categories
bg_group_result = WorkspaceAutomation.createGroupLayer("Backgrounds")
fg_group_result = WorkspaceAutomation.createGroupLayer("Foreground")
fx_group_result = WorkspaceAutomation.createGroupLayer("Effects")

# Get all composition layers
all_layers = WorkspaceAutomation.allLayers()

# Organize into groups
bg_layers = [l for l in all_layers if "bg" in l["name"].lower()]
fg_layers = [l for l in all_layers if "fg" in l["name"].lower()]
fx_layers = [l for l in all_layers if "fx" in l["name"].lower()]

# Move to groups
WorkspaceAutomation.moveLayersToGroup([l["id"] for l in bg_layers], bg_group_result["groupLayerId"])
WorkspaceAutomation.moveLayersToGroup([l["id"] for l in fg_layers], fg_group_result["groupLayerId"])
WorkspaceAutomation.moveLayersToGroup([l["id"] for l in fx_layers], fx_group_result["groupLayerId"])
```

### Example: Flatten groups

```python
# Get all group layers
all_layers = WorkspaceAutomation.allLayers()
groups = [l for l in all_layers if l["isGroup"]]

# Ungroup all
for group in groups:
    WorkspaceAutomation.ungroupLayers(group["id"])
```

## Related Documentation

- **Group Layer Implementation**: `Artifact/include/Layer/ArtifactGroupLayer.ixx`
- **Composition Layer Management**: `Artifact/include/Composition/ArtifactAbstractComposition.ixx`
- **Event System**: `ArtifactCore/include/Event/Types.ixx`

## Next Steps (Phase 5+)

1. **Nested Groups** - Support groups within groups (recursive)
2. **Group Duplication** - Duplicate entire group with all children
3. **Group Locking** - Lock/unlock groups (prevent editing children)
4. **Group Visibility Toggle** - Control all children visibility at once
5. **Group Effects** - Apply effects to entire group
6. **Group Blending** - Group-level blend mode and opacity

## Known Limitations

- **No Circular Nesting Check** - Moving groups within groups not validated
- **Layer Position** - Ungrouped layers always append at top
- **No Group Hierarchy Query** - Can't directly query group children via AI API
- **No Nested Path** - Can't directly target nested layers via ID

---

**Implementation Date**: 2026-04-27  
**Implementer**: Copilot  
**Code Location**: 
- API Methods: `Artifact/include/AI/WorkspaceAutomation.ixx:1703-1870`
- Group Layer Backend: `Artifact/include/Layer/ArtifactGroupLayer.ixx`
- Group Implementation: `Artifact/src/Layer/ArtifactGroupLayer.cppm:29-62 (child management)`
