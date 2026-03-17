# Milestone: Creative Workflow & Inspector Refinement

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
- Expose the following effects from `ArtifactCore` to the "Add Effect" menu in the Inspector:
  - `Halftone`, `Pixelate`, `Posterize`, `Mirror`, `Kaleidoscope`, `Glitch`, `OldTV`, `Fisheye`.
- Ensure these effects are correctly registered in the `EffectPipelineStage::Rasterizer` or a new `Creative` stage if appropriate.
- Verify that all parameters for these effects are visible and editable in the `ArtifactPropertyWidget`.

### M-CW-2 Inspector-Property Sync
- Implement a signal-slot connection such that selecting an effect in the `ArtifactInspectorWidget` rack automatically:
  - Calls `ArtifactPropertyWidget::setFocusedEffectId()`.
  - Scrolls the `ArtifactPropertyWidget` to the corresponding effect group.
  - Highlights the focused effect group in the Property Editor.

### M-CW-3 Enhanced Effect Management in Properties
- Add "Enable/Disable" (checkbox) and "Remove" (button) actions directly into the `QGroupBox` header or the first row of each effect in the `ArtifactPropertyWidget`.
- This reduces the need to jump back and forth between the two panels for basic management tasks.

### M-CW-4 Unified Search & Filtering
- Synchronize the search/filter text between the Inspector and the Property Editor.
- When a user searches for "Blur" in the Inspector, both the effect stack should be filtered and the Property Editor should show only Blur-related properties.

### M-CW-5 Drag-and-Drop Reordering (Visual)
- Replace or augment the "Up/Down" buttons in the Inspector racks with a drag-and-drop implementation for reordering effects.

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
