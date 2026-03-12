# Editing Ready Cut Milestone (M-RC-1)

## Goal

Create a stable "editing-ready" baseline where a user can:

1. Create a project and composition
2. Import and organize assets
3. Build a simple cut in timeline
4. Preview playback reliably
5. Save and reopen with consistent state

This milestone is focused on perceived product completeness rather than adding many isolated features.

## Definition of Done

1. The following flow works end-to-end without crash:
   - new project -> import assets -> create/open composition -> add layers -> play/seek -> save -> reopen
2. Project View and Timeline selection/current-composition behavior stay consistent.
3. Import, rename, delete, and basic organization actions are available from Project View.
4. Save/Load preserves composition, layer, and project item structure with no major mismatch.
5. Manual regression checklist for this flow passes (P0 = 0, P1 <= 2).

## Workstreams

### RC-1 Project View / Asset Ingestion

1. External drag & drop import in Project View
2. Folder-drop recursive import for supported asset types
3. Missing/unused visibility consistency with Asset Browser

### RC-2 Composition / Timeline Bridge

1. Double-click/open from Project View to active composition/timeline
2. Current composition and selection synchronization
3. Basic layer operations stability (add/reorder/remove/rename)

### RC-3 Playback Reliability

1. Play/stop/seek behavior bound to current composition frame range/rate
2. No desync between timeline state and playback state

### RC-4 Save/Load Integrity

1. Persist/restore composition metadata (size, frame range, frame rate)
2. Persist/restore project tree and asset links
3. Clear missing-asset indication after reopen

### RC-5 Validation Gate

1. Manual regression checklist dedicated to Editing Ready Cut
2. Crash/diagnostic checkpoints on import/delete/reopen paths

## Initial Slice Completed on 2026-03-12

1. Project View drop import supports:
   - image/video/audio/font files
   - directory drop with recursive scan
2. Duplicate paths are removed before batch import.
3. Import handling logic is aligned between context menu import and direct drop paths.
4. Project View context menu supports "Move to Folder" for project items.
5. Reparent safety guards are enabled (no self/descendant move, root fixed).
6. Project View internal drag-and-drop supports moving items into folders.

## Next Slice

1. Add explicit regression checklist document for RC-5.
2. Add save/load verification cases for project tree integrity.
