# PropertyEditor Audit

Date: 2026-03-11

## Summary

The old `Knob` naming still exists in `ArtifactWidgets`, but the current app-side property UI is driven by `ArtifactPropertyWidget`.

The old `Knob` pieces are not a complete reusable property editing system anymore. Most of them are either skeletal or disconnected from the current `Artifact` property pipeline.

The current direction is:

- keep `ArtifactWidgets/include/Knob/*` as legacy remnants for now
- standardize new app-side property rows under `PropertyEditor`
- let `ArtifactPropertyWidget` compose those reusable parts
- keep `Inspector` focused on effect stack management
- make `Properties` a standalone current-layer editor

## Legacy Knob Remnants

Observed in `ArtifactWidgets`:

- `AbstractKnobEditor`
- `KnobSlider`
- `KnobCheckBox`
- `RotaryKnob`
- `KnobEditorWidget`
- `KnobColorPicker`

### Assessment

- `AbstractKnobEditor`
  Exists, but only provides a drop-enabled `QWidget` shell.
- `KnobSlider`
  Exists, but mouse/wheel/paint behavior is effectively empty.
- `KnobCheckBox`
  Exists, but is a thin wrapper over `QCheckBox`.
- `RotaryKnob`
  Exists, but has no meaningful interaction or rendering implementation.
- `KnobEditorWidget`
  Exists as a name, but not as a usable modern property row system.
- `KnobColorPicker`
  Exists as a stub, not as a production-ready editor.

## Current App-side Reality

Current live editor path:

- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`

This widget currently:

- reads layer property groups from `getLayerPropertyGroups()`
- reads effect properties from `effect->getProperties()`
- builds Qt editors dynamically

## New PropertyEditor Direction

New reusable pieces introduced under `Artifact`:

- `Artifact.Widgets.PropertyEditor`

These are intended to replace the one-off editor creation inside the property panel.

Current reusable parts:

- `ArtifactAbstractPropertyEditor`
- `ArtifactFloatPropertyEditor`
- `ArtifactIntPropertyEditor`
- `ArtifactBoolPropertyEditor`
- `ArtifactStringPropertyEditor`
- `ArtifactColorPropertyEditor`
- `ArtifactPropertyEditorRowWidget`

## Next Steps

1. Add slider-capable numeric editors for float/int rows.
2. Add reset/default buttons per property row.
3. Add keyframe/expression affordances to the row widget itself.
4. Move more property panel layout logic out of `ArtifactPropertyWidget`.
5. Decide whether old `Knob` remnants should be migrated or deleted.
