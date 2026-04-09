# Audio Bus Routing UI Milestone

Date: 2026-04-09

## Goal

- Extend the existing audio mixer work so that bus routing can be edited from the UI.
- Keep the current composition audio mixer usable while adding bus send / return / master routing controls.
- Reuse the current `ArtifactAudioMixer` / `AudioMixerWidget` direction instead of introducing a second parallel mixer system.
- Preserve playback stability and avoid introducing routing changes that would disturb the current audio callback path.

## Scope

- `ArtifactCore/include/Audio`
- `ArtifactCore/src/Audio`
- `Artifact/src/Audio`
- `Artifact/src/Widgets`
- `ArtifactWidgets/include/Audio`

## Recommended Design

- Treat `AudioMixer` as the routing graph authority.
- Treat `AudioBus` as the per-bus state holder for volume, pan, mute, solo, effects, and sidechain input.
- Treat `AudioMixerWidget` as the main editor surface for bus routing.
- Keep `ArtifactCompositionAudioMixerWidget` as the composition-focused summary view, then add an optional routing expansion or a dedicated routing panel from there.
- Do not build a new bus system in parallel with the existing mixer classes.

## Milestones

### AUR-1 Bus Graph Contract

- Finalize bus graph ownership rules.
- Define stable identifiers for buses so UI state can survive refreshes.
- Decide how master bus, normal buses, and sidechain sends should be represented in the graph.
- Add validation for invalid self-routing and routing cycles.
- Expose UI-friendly queries such as bus ordering, routing targets, and send slots.

### AUR-2 Mixer Surface Expansion

- Expand `AudioMixerWidget` from a flat strip list into a routing surface.
- Add bus creation, deletion, rename, and routing target selection.
- Add send / return controls for sidechain or auxiliary routing.
- Keep the current channel strip controls visible so existing mute / solo / pan workflows do not regress.
- Reuse the existing strip layout as the primary interaction model, then add routing affordances beside or above each strip.

### AUR-3 Composition Sync And Persistence

- Serialize bus topology into composition or project state.
- Restore bus graph and routing links when a composition is reopened.
- Keep layer-to-strip synchronization stable when the active composition changes.
- Make sure routing edits are undoable or at least go through the same command path as other composition edits.

### AUR-4 Runtime Safety And Validation

- Ensure the routing snapshot used by playback is thread-safe.
- Avoid touching live callback state directly from the widget thread.
- Add validation for missing bus targets, deleted buses, and stale send references.
- Confirm that mute / solo / master gain still behave correctly when routing is enabled.

## Implementation Breakdown

### 1. Core Routing Model

- Keep graph operations in `ArtifactCore::AudioMixer`.
- Add or confirm APIs for routing enumeration and safe mutation.
- Use a deterministic default route from created buses to master.
- Keep sidechain sends separate from primary routing.

### 2. UI Shell

- Extend `ArtifactWidgets/include/Audio/AudioBusWidget.ixx` from stub to real editor widget.
- Reuse the current mixer card language instead of inventing a new interaction metaphor.
- Prefer one routing editor entry point per composition, with optional detail expander for advanced routing.

### 3. Composition Mixer Integration

- Wire the composition mixer widget to the bus editor.
- Show the current composition's buses and allow the selected layer to be assigned to a bus.
- Keep the existing `master` strip visible and stable.

### 4. Persistence / Restore

- Add composition serialization for bus topology after the UI is stable.
- Restore name, routing target, send amount, mute, solo, and pan.
- Avoid partially restoring a broken graph; fall back to master if needed.

## Validation Checklist

- A composition with multiple buses can be opened, edited, saved, and reopened without losing routing.
- Creating or deleting a bus updates the UI immediately.
- Re-routing a bus changes playback behavior without requiring an app restart.
- Mute, solo, pan, and master volume continue to work when routing is enabled.
- Sidechain routing does not create cycles or dead routes.
- The mixer UI remains usable at smaller window sizes.

## Notes

- `ArtifactWidgets/include/Audio/AudioBusWidget.ixx` is currently a stub, so it is a natural entry point for the bus editor shell.
- The existing `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm` already provides a composition-aware surface and can be used as the first integration target.
- If the routing UI grows too large, split the advanced graph editor into a secondary panel while keeping the current strip view as the main surface.
