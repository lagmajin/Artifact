# BUG: Property Widget – Selection Clears on Slider Commit / Keyframe Button Blue Fill

**Date:** 2026-04-20  
**Status:** Fixed (commit 80842dc on Artifact main)  
**Files Changed:**
- `src/Layer/ArtifactHierarchyModel.cppm`
- `src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`

---

## Bug 1 — Keyframe / Expression Button has Permanent Blue Background

### Symptom
The ◆ (keyframe) button and expression button in every property row had a solid blue background fill at all times, regardless of keyframe state. Only the diamond icon should indicate active/inactive state (gold = has keyframe, gray = no keyframe).

### Root Cause
`applyPropertyButtonPalette(btn, /*accent=*/true)` was called during button setup. The `accent=true` argument:
1. Sets `QPalette::Button` to the theme accent color (`#5E94C7`).
2. Calls `btn->setAutoFillBackground(true)`, locking in the blue background permanently.

The `updateKeyframeButtonIcon()` already handles the visual state via the diamond icon. The accent palette was redundant and wrong.

### Fix
Changed both calls to `applyPropertyButtonPalette(btn)` (no `accent` argument) so the button uses the default panel background palette:

```cpp
// Before
applyPropertyButtonPalette(keyframeButton_, true);
applyPropertyButtonPalette(expressionButton_, true);

// After
applyPropertyButtonPalette(keyframeButton_);
applyPropertyButtonPalette(expressionButton_);
```

---

## Bug 2 — Timeline Layer Selection Visually Clears on Every Property Edit

### Symptom
After moving a slider in the property widget and confirming the value, or after pressing the keyframe button, the highlighted row in the timeline layer list (QTreeView in `ArtifactLayerHierarchyWidget`) would lose its selection highlight. The property panel itself would also appear to lose context in some cases.

### Root Cause
`ArtifactHierarchyModel` subscribed to ALL `LayerChangedEvent` types and responded to every event — including `Modified` — with a full `beginResetModel()` / `endResetModel()` cycle:

```cpp
// OLD (wrong)
owner_->beginResetModel();
owner_->endResetModel();
```

`QAbstractItemModel::beginResetModel()` causes every attached `QAbstractItemView` (e.g. `QTreeView`) to call `reset()`, which calls `selectionModel()->reset()`. This wipes the visual selection in the hierarchy QTreeView on **every property change**, not just structural ones.

### Event Chain
1. Slider released → `commitCurrentValue()` → commit handler
2. `UndoManager::push(ChangeLayerOpacityCommand)` → `cmd->redo()` → `layer->setOpacity()` → `notifyLayerMutation()`
3. `globalEventBus().publish(LayerChangedEvent{Modified})` fires synchronously
4. **OLD:** `ArtifactHierarchyModel` subscriber → `beginResetModel/endResetModel` → QTreeView `reset()` → visual selection cleared

### Fix
For `Modified` events, instead of resetting the entire model, emit `dataChanged()` for the specific affected row only. Structural changes (`Created` / `Removed`) still do the full reset.

```cpp
// NEW: targeted dataChanged for Modified events
const QModelIndex idx = layerModelIndex(layer);
if (!idx.isValid()) return;
const QModelIndex left  = idx.sibling(idx.row(), 0);
const QModelIndex right = idx.sibling(idx.row(), owner_->columnCount({}) - 1);
emit owner_->dataChanged(left, right);
```

A `layerModelIndex()` helper was added to find the correct `QModelIndex` for both root layers and child layers inside group layers.

### Additional Guard (ArtifactLayerPanelWidget)
The `ArtifactLayerPanelWidget` already had a guard at its `LayerSelectionChangedEvent` subscriber to ignore nil-layer events when the selection manager still has a live selection. This prevents spurious clearing via the EventBus path. This guard was kept as-is.

---

## Remaining Investigation Notes

### Property Panel Going Blank (Unconfirmed Deeper Cause)
The `ArtifactLayerSelectionManager` `clearSelection()` was NOT found to be called during the slider commit path. All confirmed callers are:
- `ArtifactCompositionRenderWidget::mousePressEvent` — empty area click
- `ArtifactCompositionRenderController` — no-layer hit test
- `ArtifactEditMenu` — edit menu actions
- `ArtifactProjectService` — group/ungroup operations

If the property panel continues to go blank after this fix, the most likely hypothesis is:
- After the slider releases its mouse grab, Qt may deliver a synthetic mouse press to the widget under the cursor (potentially the Diligent native HWND overlay), which routes to `ArtifactCompositionRenderController::mousePressEvent` at line 2722 → `clearSelection()`.
- Mitigation: add a guard in `ArtifactCompositionRenderController::mousePressEvent` that skips `clearSelection()` when the event's `windowPos()` is outside the composition viewport bounds, or check if any property widget is currently handling an edit.

### ScopedPropertyEditGuard and EventBus asymmetry
`ScopedPropertyEditGuard` only blocks re-entrant `layer->changed()` Qt signal. It does NOT guard `globalEventBus().publish()`. This means `LayerChangedEvent::Modified` fires even during guarded property edits. All subscribers must be tolerant of `Modified` events at any time.

---

## Test Scenarios
1. Select a layer → drag opacity slider → release: hierarchy QTreeView selection must remain highlighted.
2. Select a layer → click ◆ keyframe button: selection must remain, button must show gold/gray icon without blue background.
3. Select a layer → press expression button: no blue background fill on the button.
4. Group layers → drag slider: child layer modifications must not clear selection.
