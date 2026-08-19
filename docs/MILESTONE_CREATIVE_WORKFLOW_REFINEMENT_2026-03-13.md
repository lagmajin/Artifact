# Milestone: Creative Workflow & Inspector Refinement

**最終更新:** 2026-08-15

> 状態: Partial（M-CW-1〜5 の実装・静的確認済み。composition effect の追加・削除・enable・移動は Undo 接続済み。実ランタイム検証とコンポーネント面の統合は未完了）

Date: 2026-03-13

## Goal

- Enhance the core creative editing loop by bridging advanced effects from `ArtifactCore` to `ArtifactStudio`.
- Improve the synchronization and usability between the `ArtifactInspectorWidget` (Effect Stack) and `ArtifactPropertyWidget` (Property Editor).
- Provide a more seamless experience for searching and managing effects and their parameters.

## Scope

- `ArtifactStudio/Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `ArtifactStudio/Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `ArtifactStudio/Artifact/include/Widgets/ArtifactInspectorWidget.ixx`
- `ArtifactStudio/Artifact/include/Widgets/ArtifactPropertyWidget.ixx`
- `ArtifactStudio/Artifact/src/AppMain.cppm` (for signal connections)

## Milestones

### M-CW-1 Creative Effects Bridge
- ✅ Halftone / Posterize / Pixelate / Mirror 系を含む catalog entry と EffectService factory の接続を確認した。Mirror は Core `CreativeEffectFactory` bridge 経由で CPU 利用可能。
- Expose the following effects from `ArtifactCore` to the "Add Effect" menu in the Inspector:
  - `Halftone`, `Pixelate`, `Posterize`, `Mirror`, `Kaleidoscope`, `Glitch`, `OldTV`, `Fisheye`.
- Ensure these effects are correctly registered in the `EffectPipelineStage::Rasterizer` or a new `Creative` stage if appropriate.
- Verify that all parameters for these effects are visible and editable in the `ArtifactPropertyWidget`.

### 2026-07-29 Implementation Loop

- ✅ Layer and Composition effect groups in `ArtifactPropertyWidget` now expose `Enabled` and `Remove` controls.
- ✅ Layer operations use `ArtifactEffectService`, preserving the existing undo-capable service path; removal also asks for confirmation. Composition-target add/remove/enable/reorder operations are now routed through dedicated Undo commands in `ArtifactProjectService`; removal preserves stack position and reorder preserves source/target indices.
- ✅ Inspector-Property focus synchronization, unified filtering, drag-and-drop ordering, and Property 側管理を実装済み。
- ⏳ 実ランタイム検証とコンポーネント面の統合は未完了。

### M-CW-4 Implementation Loop

- ✅ Added an Inspector Effects filter field and connected it to the existing `ArtifactPropertyWidget::setFilterText()` path.
- ✅ The filter is retained when the effect property surface is created or resynchronized for layer/composition targets.
- ✅ The same filter now narrows the effect rack by effect ID, display name, or editable property name, and reports when no effect matches.
- ⏳ Component surfaces and runtime verification remain incomplete.

### M-CW-5 Implementation Loop

- ✅ Effect racks accept internal drag-and-drop and translate the drop distance into repeated existing move operations.
- ✅ Reordering is applied through `moveEffectById()`, preserving composition/layer ownership and the existing Undo path; the visual list is not independently reordered.
- ✅ Multi-selection drag semantics now move selected effects as a stable block while preserving their relative order.
- ⏳ Runtime verification remains incomplete.

### M-CW-2 Inspector-Property Sync
- ✅ Selecting an effect in the `ArtifactInspectorWidget` rack automatically:
  - Calls `ArtifactPropertyWidget::setFocusedEffectId()`.
  - Scrolls the `ArtifactPropertyWidget` to the corresponding effect group.
  - Highlights the focused effect group in the Property Editor.

### M-CW-3 Enhanced Effect Management in Properties
- ✅ "Enable/Disable" and "Remove" actions are directly available in each effect group of the `ArtifactPropertyWidget`.
- This reduces the need to jump back and forth between the two panels for basic management tasks.

### M-CW-4 Unified Search & Filtering
- ✅ Synchronize the search/filter text between the Inspector and the Property Editor.
- When a user searches for "Blur" in the Inspector, both the effect stack should be filtered and the Property Editor should show only Blur-related properties.

### M-CW-5 Drag-and-Drop Reordering (Visual)
- ✅ Effect racks support single- and multi-selection drag-and-drop reordering through the existing service/Undo path; selected effects move as a stable block while preserving relative order.
- ✅ Composition effect add/remove/enable/move now uses dedicated service Undo commands; removal restores the original stack index and reordering restores the exact source/target indices.
- ⏳ Runtime verification remains incomplete.

## Recommended Order

1. `M-CW-1 Creative Effects Bridge` (High value, low risk)
2. `M-CW-2 Inspector-Property Sync` (High usability impact)
3. `M-CW-3 Enhanced Effect Management in Properties` (Workflow polish)
4. `M-CW-4 Unified Search & Filtering` (Consistency)
5. `M-CW-5 Drag-and-Drop Reordering` (UX refinement)

## Validation Checklist

- [ ] New creative effects (Halftone, etc.) appear in the "Add Effect" menu.
- [ ] Adding a creative effect shows its properties in the Property Editor.
- [ ] Clicking an effect in the rack scrolls the Property Editor to that effect.
- [ ] Effects can be enabled/disabled from the Property Editor headers.
- [ ] Searching in the Inspector filters the Property Editor accordingly.
