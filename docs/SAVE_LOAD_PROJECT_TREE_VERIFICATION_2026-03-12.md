# Save/Load Project Tree Verification Cases (2026-03-12)

## Goal

Define manual verification cases for `M-RC-1 / RC-4`:

1. project tree persistence
2. project item metadata consistency
3. composition bridge integrity after reopen

## Test Data Setup

1. Create project with at least:
   - 2 compositions
   - 2 folders (nested once)
   - 6+ footage items mixed type
2. Rename several items with non-default names.
3. Move items into folders using both:
   - context menu `Move to Folder`
   - internal drag-and-drop
4. Keep one asset path intentionally missing for missing-state verification.

## Verification Cases

### SL-01 Basic Tree Persistence

1. Save project.
2. Reopen project.
3. Verify top-level item count and names match pre-save snapshot.

Expected:

1. No item loss.
2. No unexpected duplicate root/folder items.

### SL-02 Parent/Child Relationship Persistence

1. Record folder membership before save.
2. Save and reopen.
3. Verify each moved item remains under expected parent folder.

Expected:

1. Parent pointers are preserved.
2. No flattened tree after reopen.

### SL-03 Composition Item Consistency

1. Rename composition and edit composition settings (size, frame rate, frame range).
2. Save and reopen.
3. Open each composition from Project View.

Expected:

1. Composition names match project tree.
2. Settings remain applied after reopen.
3. Playback uses restored frame range/rate.

### SL-04 Asset Link Integrity

1. Save and reopen project with mixed local assets.
2. Open reveal/copy-path actions from Project View.

Expected:

1. Existing asset links remain valid.
2. Missing assets are still marked missing and do not crash actions.

### SL-05 Rename/Delete Durability

1. Rename and delete a subset of items.
2. Save and reopen.

Expected:

1. Deleted items do not reappear.
2. Renamed items keep final names.

### SL-06 Repeated Save/Reopen Stability

1. Perform 3 cycles of:
   - move item
   - rename item
   - save
   - reopen

Expected:

1. No cumulative tree corruption.
2. No crash or unreadable project.

## Failure Severity Guide

1. `P0`: cannot reopen project, hard crash, or major data loss.
2. `P1`: tree relationships broken, wrong composition binding, or severe mismatch.
3. `P2`: minor label/order mismatch with workaround.

## Run Log Template

1. Commit hash:
2. Cases run: `SL-01..SL-06`
3. Failed cases:
   - ID:
   - Severity:
   - Repro:
   - Observed:
   - Expected:
4. Decision:
   - `Gate Pass`
   - `Gate Conditional`
   - `Gate Fail`
