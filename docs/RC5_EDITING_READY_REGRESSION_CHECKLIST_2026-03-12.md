# RC-5 Editing Ready Regression Checklist (2026-03-12)

**最終更新:** 2026-08-15

## Update 2026-08-15

- M-QA-2 用に timeline / render / audio / dock の補足確認表を追加。
- 各項目は `Pass` / `Fail (P0/P1/P2)` / `N/A` と観測メモを残す前提に統一。

## M-QA-2 Supplemental Matrix

| ID | Area | Manual check | Result | Observation / evidence |
|---|---|---|---|---|
| TL-01 | Timeline | current composition と current layer の切替が timeline / inspector に同期する |  |  |
| TL-02 | Timeline | play / stop / seek / scrub で playhead と preview が一致する |  |  |
| TL-03 | Timeline | layer reorder / duplicate / delete / trim 後に表示と選択が壊れない |  |  |
| RN-01 | Render | software preview と render queue の同一 frame / size を比較する |  |  |
| RN-02 | Render | transform / mask / matte / blend を含む frame の出力を確認する |  |  |
| RN-03 | Render | failure 時に Frame Debug / App Debugger の failure reason を記録できる |  |  |
| AU-01 | Audio | audio layer の mute / volume / pan が preview に反映する |  |  |
| AU-02 | Audio | playback / seek / stop 後も audio clock と timeline frame が追従する |  |  |
| AU-03 | Audio | waveform / mixer の current composition・layer 表示が切替に追従する |  |  |
| DK-01 | Dock | timeline / property / project / asset / render queue dock の表示切替が復元される |  |  |
| DK-02 | Dock | dock resize / tab切替後も active composition・layer の表示が一致する |  |  |
| DK-03 | Dock | close / reopen 後に stale selection や空表示が残らない |  |  |

## Scope

Validate the "editing-ready" user flow for `M-RC-1`:

1. new project
2. import and organize assets
3. create/open composition
4. timeline edit + playback
5. save/reopen consistency

## Result Rule

1. `P0`: crash/data loss/hard-blocker -> must be `0`
2. `P1`: major functional break -> must be `<= 2`
3. All checklist items should be marked with:
   - `Pass`
   - `Fail (P0/P1/P2)`
   - `N/A`

## Preconditions

1. Start from a clean app launch.
2. Use a fresh test project directory.
3. Prepare mixed test assets:
   - image (`png/jpg`)
   - video (`mp4/mov`)
   - audio (`wav/mp3`)
   - font (`ttf/otf`)

## Checklist

### A. Project Boot / Composition

1. Create new project and confirm Project View is visible.
2. Create new composition.
3. Double-click composition in Project View and confirm timeline/viewer opens.
4. Set active composition from context menu and verify current composition sync.

### B. Import / Organization

1. Drag single file into Project View -> asset is imported.
2. Drag multiple files into Project View -> all import candidates are imported once.
3. Drag folder into Project View -> recursive import runs for supported file types.
4. Rename project item via `F2` and context menu.
5. Move project item with `Move to Folder` context menu.
6. Move project item by internal drag-and-drop into a folder.
7. Invalid move (self/descendant cycle) is blocked.
8. Delete project item and verify confirmation flow.

### C. Timeline / Playback

1. Add at least two layers to current composition.
2. Reorder layer(s) and confirm visual order update.
3. Remove layer and verify state update without crash.
4. Play/stop works from playback controls.
5. Seek updates viewer/timeline consistently.
6. Frame range and frame rate changes in composition settings are reflected in playback.

### D. Save / Reopen

1. Save project.
2. Close app and reopen project.
3. Confirm Project View hierarchy is restored.
4. Confirm current composition can be opened and played.
5. Confirm renamed items and moved folders remain as saved.
6. Confirm missing assets are shown as missing (if path intentionally broken).

### E. Diagnostics / Stability

1. No crash during repeated import/delete/move/reopen loop (3 cycles).
2. No obvious UI deadlock during drag-and-drop and context actions.
3. Error dialogs are actionable and do not leave broken selection state.
4. Place a pending crash report in the configured crash directory and confirm startup ingestion records it without blocking the project.
5. Launch with `--safe-mode` and confirm the application reaches an editable recovery surface.
6. Confirm the latest recovery point / session ledger entry and diagnostic report can be exported or copied for support.
7. Record whether the report contains an explicit failure reason, timestamp, and source path; mark missing stack symbols as a known limitation.

## Report Template

Use this structure when reporting run results:

1. Build/commit hash
2. Environment (OS, GPU, backend)
3. Summary counts: `Pass`, `Fail(P0/P1/P2)`, `N/A`
4. Failed cases list:
   - checklist ID
   - observed behavior
   - expected behavior
   - severity
   - repro steps
