# QADS Upstream Comparison Investigation (2026-06-23)

**最終更新:** 2026-10-16

## Purpose
- Compare the current ArtifactStudio QADS usage against the upstream Qt Advanced Docking System behavior.
- Focus areas:
  - missing dock drop overlay / insertion preview
  - startup layout feeling different from stock QADS
  - whether local helper logic is fighting upstream assumptions

## Local dependency source
- Current project uses `vcpkg` package `qt-advanced-docking-system`.
- Installed package metadata shows:
  - upstream repository: `https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System`
  - version: `4.5.0`
- Evidence:
  - `build/vcpkg_installed/x64-windows/share/qt-advanced-docking-system/vcpkg.spdx.json`

## Upstream checkout used for comparison
- Local comparison checkout created at:
  - `X:\Dev\Qt-Advanced-Docking-System-4.5.0`
- Checkout command used:
  - `git clone --depth 1 --branch 4.5.0 https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System.git X:\Dev\Qt-Advanced-Docking-System-4.5.0`
- Resolved commit:
  - `87cffe5d40307c61141c88b88c10c5e7d181d880`

## Relevant local code
- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
  - custom overlay preparation:
    - `prepareDockDropOverlayWindow(...)`
    - `prepareDockDropOverlays(...)`
    - `enableDockDropPreview(...)`
  - extra handling currently includes:
    - scanning `DockOverlay` children
    - forcing `WindowStaysOnTopHint` on overlay-like widgets
    - re-running overlay preparation after dock additions / floating creation

## Relevant upstream behavior
- In upstream QADS `4.5.0`:
  - `CDockManager` constructs `CDockOverlay` internally in `src/DockManager.cpp`
  - drag/drop overlay behavior is driven by normal internal flow:
    - `FloatingDragPreview.cpp`
    - `FloatingDockContainer.cpp`
    - `DockContainerWidget.cpp`
- Upstream does not rely on application-side post-processing that:
  - finds overlay widgets after creation
  - mutates their window flags
  - repeatedly re-enables preview from outside the library

## Main observation
- ArtifactStudio retains app-side QADS overlay intervention in the optional compatibility path; upstream QADS does not need this application-side post-processing.
- The default MainWindow path is Native, so this compatibility-path behavior does not explain Native drag/drop differences. It remains a suspect only when the QADS compatibility backend is enabled, particularly for:
  - missing insertion preview during drag
  - behavior that feels different from default QADS

## Native overlay behavior checked against QADS 4.5.0
- QADS uses separate dock-area and container overlays. `CDockContainerWidgetPrivate::getDropMode()` reads both results and gives the dock-area overlay precedence only when it does not conflict with the container edge target.
- QADS copies the target `CDockAreaWidget::allowedAreas()` into the dock-area overlay before showing it. `CDockOverlayCross::reset()` hides disallowed area widgets, and cursor hit-testing ignores them. Native now exposes `setDockAreaAllowedDropAreas()` / `dockAreaAllowedDropAreas()` through `ArtifactMainWindow`, applies the mask to overlay rendering and hit-testing, and stores it as optional `allowedDropAreas` layout data; older or invalid values default to all five directions. Auto-Hide return also reapplies the stored mask when the original split leaf is rebuilt. Runtime parity remains unverified.
- The dock-area overlay targets the active dock area; the container overlay targets the outer container edges. `DockOverlayCross::areaGridPosition()` places the container targets along the outer edges, while dock-area targets remain around the target area's center.
- Native now mirrors this distinction during a drag: a central dock-area overlay proposes tab merge or local split; a separate outer-edge overlay proposes splitting the container root. This applies to both the main container and floating containers, whose root split is retained in the floating layout tree. When both overlays identify the same direction, the dock-area target wins as in QADS; when their directions differ, the container-edge target wins. The selected half is previewed and the drop wraps the current root widget in a new horizontal or vertical splitter, so the whole existing layout moves as one side of the split. Tab-strip insertion preview retains priority over the outer-edge target.
- Container-level binary paths and ratios are now stored separately from each panel's fixed `DockArea` and local split path. Auto-Hide return data also retains the outer path so a removed root leaf can be reconstructed when the panel is docked back.
- The built-in five-area root splitter proportions (top/center/bottom and left/center/right) are also serialized as optional normalized ratios, so resizing fixed dock areas survives layout restore without changing the layout version.
- Floating groups now also have a persisted `floatingGroupId`, independent of their member/order list. Existing layouts without the field retain the old `floating-tabs:` restoration path; restoration can merge members into one surviving floating window.
- QADS exposes per-dock closable, movable, floatable, pinnable, and focusable feature flags. Native now exposes get/set facade methods for all five through `ArtifactMainWindow`, backed by its per-panel metadata and serialized optional fields. The Auto-Hide capability maps to QADS `DockWidgetPinnable`; Native's separate Pin remains close protection and Lock remains an additional movement/float override.
- Native's per-panel `Allow closing`, `Allow moving`, `Allow floating`, `Allow Auto-hide`, and `Show focus highlight` controls correspond to QADS `DockWidgetClosable`, `DockWidgetMovable`, `DockWidgetFloatable`, `DockWidgetPinnable`, and `DockWidgetFocusable`. Existing layouts default to the same enabled state as QADS's `DefaultDockWidgetFeatures`.
- Native exposes `dockTabToolTip` / `setDockTabToolTip` through `ArtifactMainWindow`, corresponding to QADS `CDockWidget::setTabToolTip`. The widget's tooltip is reflected on docked/floating tabs and Auto-Hide sidebar tabs; it remains panel metadata rather than duplicated layout state.
- Native exposes `dockIcon` / `setDockIcon` through `ArtifactMainWindow`, corresponding to QADS `CDockWidget::icon` / `setIcon`. Icons are reflected on docked/floating tabs, the tab list, and Auto-Hide sidebar tabs; they remain application metadata rather than duplicated layout state.
- Native exposes `setDockTitle`, matching QADS's `CDockWidget::setWindowTitle` behavior across the tab, panel widget, floating header, and Auto-Hide labels. `ArtifactMainWindow` forwards the operation through its public wrapper.
- Native exposes `dockTitleBarActions()` / `setDockTitleBarActions()` through `ArtifactMainWindow`, corresponding to QADS `CDockWidget::titleBarActions()` / `setTitleBarActions()`. The active panel's actions are shown as action-backed tool buttons in its Area Title Bar, including the separated title row used with bottom tabs. Native keeps guarded references and leaves QAction ownership with the caller; QADS compatibility delegates to the upstream API. Static source review only; visual behavior and QAction lifetime changes are not runtime-verified.
- Native exposes `setDockAreaDynamicTabsMenuButtonVisibility()` / `dockAreaDynamicTabsMenuButtonVisibility()`, matching QADS `DockAreaDynamicTabsMenuButtonVisibility`. When enabled, the panel-list button appears only while a visible tab title is elided; resizing and tab-bar refreshes reevaluate the condition. QADS compatibility delegates to its global config flag. This option remains off by default, matching QADS; runtime resize/elision behavior is not verified.
- Native exposes `setDockAreaHideDisabledButtons()` / `dockAreaHideDisabledButtons()`, matching QADS `DockAreaHideDisabledButtons`. When enabled, disabled Close group, Float group, and Auto-Hide controls are hidden while their enablement rules remain unchanged. QADS compatibility delegates to its global config flag; runtime visibility transitions are not verified.
- Floating dialog and custom header titles now follow the current tab in each floating tab surface, matching QADS's default `FloatingContainerHasWidgetTitle` behavior. Existing mouse/key event-filter and active-surface paths perform the update; no new signal connection was added.
- Native exposes `setFloatingContainerHasWidgetIcon()` / `floatingContainerHasWidgetIcon()`, matching QADS `FloatingContainerHasWidgetIcon`. When enabled, each floating dialog uses its current panel icon and falls back to `QApplication::windowIcon()` if that panel has no icon. The option defaults off like QADS; QADS compatibility delegates to the global config flag. Runtime titlebar/icon refresh is not verified.
- Native now matches QADS's default close-button visibility: only the active tab shows its close button. `setDockAreaActiveTabHasCloseButton()`, `setDockAreaAllTabsHaveCloseButton()`, and `setDockAreaRetainTabSizeWhenCloseButtonHidden()` expose the matching three global flags; defaults match QADS `DefaultBaseConfig` (active=true, all=false, retain=false). Pin still disables the button without changing its visibility rule. Runtime tab switching and width recalculation are not verified.
- Native exposes `setDockAreaDisableTabTextEliding()`, matching QADS `DisableTabTextEliding`. Enabled mode uses full tab titles with the existing scroll buttons; when dynamic tab-list visibility is also enabled, the list button follows actual tab overflow instead of text elision. The default remains eliding, matching QADS. Runtime width/scroll behavior is not verified.
- Native exposes `setDockAreaShowTabTextOnlyForActiveTab()`, matching QADS `ShowTabTextOnlyForActiveTab`. When enabled, icon-bearing inactive tabs keep their icons but hide the title text; selecting the tab restores its title. Tabs without icons keep their text. The option defaults off like QADS and delegates to the global config flag in the compatibility backend. Runtime selection and title/icon update behavior is not verified.
- Native exposes `setDockAreaAlwaysShowTabs()` / `dockAreaAlwaysShowTabs()`, matching QADS `AlwaysShowTabs`. By default, a one-tab floating surface hides its tab strip when it is the only surface in that floating container and its active panel has no title-bar actions; bottom-positioned strips are shown only for multiple visible tabs. Enabling the flag keeps the strip visible whenever it has a visible tab. QADS compatibility delegates to the global config flag. Runtime visibility and transitions are not verified.
- Native exposes per-area `setDockAreaHideSingleWidgetTitleBar()` / `dockAreaHideSingleWidgetTitleBar()`, matching QADS `CDockAreaWidget::HideSingleWidgetTitleBar`. With the option enabled, a one-panel dock area hides its title/action row and tab strip when the active panel has no title actions; QADS `AlwaysShowTabs` takes precedence. The area setting is stored as an optional `hideSingleWidgetTitleBar` layout field and restored after split/floating placement. Runtime visibility, area migration, and persistence have not been verified.
- Native exposes `setDockAreaHideSingleCentralWidgetTitleBar()` / `dockAreaHideSingleCentralWidgetTitleBar()`, matching QADS `CDockManager::HideSingleCentralWidgetTitleBar`. It hides the title/action row and tab strip only when the entire main dock container has one open panel, that panel has no title actions, and `AlwaysShowTabs` is off. The QADS compatibility backend delegates to the same config flag. Runtime transitions between one and multiple open panels have not been verified.
- Native exposes `setDockEqualSplitOnInsertion()` / `dockEqualSplitOnInsertion()`, matching QADS `CDockManager::EqualSplitOnInsertion`. It defaults off like QADS and affects later insertions only. When enabled, connected same-orientation branches are sized by their dock-area leaf counts, while a perpendicular parent retains its prior sizes and the inserted branch is equalized. The binary split tree remains structurally different from QADS's multi-child splitter. Runtime insertion ratios have not been verified.
- Native exposes `setDockOpaqueSplitterResize()` / `dockOpaqueSplitterResize()`, matching QADS `CDockManager::OpaqueSplitterResize`. The default is off, matching QADS `DefaultNonOpaqueConfig`; splitter handles show Qt's rubber band while dragging and commit the resize on release. The option can update existing splitters and applies to splitters created later. Runtime handle dragging is not verified.
- Native exposes `setAutoHideButtonCheckable()` / `autoHideButtonCheckable()`, matching QADS `CDockManager::AutoHideButtonCheckable`. The option defaults off like QADS and updates docked area Auto-Hide buttons; QADS compatibility delegates to the same auto-hide config flag. Runtime checked-state appearance is not verified.
- Native exposes `setAutoHideSideBarsIconOnly()` / `autoHideSideBarsIconOnly()`, matching QADS `CDockManager::AutoHideSideBarsIconOnly`. When enabled, an Auto-Hide tab with an icon hides its title; tabs without icons retain their title. Title, icon, and tooltip updates share the same refresh path. The option defaults off and delegates to the QADS auto-hide config flag in the compatibility backend. Runtime sidebar rendering is not verified.
- Native exposes `setAutoHideShowOnMouseOver()` / `autoHideShowOnMouseOver()`, matching QADS `CDockManager::AutoHideShowOnMouseOver`. When enabled, hovering a sidebar tab opens its flyout after 500 ms, and leaving the tab/flyout closes it after 500 ms unless the pointer remains in the flyout. The option defaults off and delegates to the QADS auto-hide config flag in the compatibility backend. Runtime hover transitions are not verified.
- Native also supports QADS's optional `MiddleMouseButtonClosesTab` behavior. It defaults off like QADS's base config, respects both closable and local Pin, and persists through the optional layout-document flag.
- Native exposes `setDockAreaDoubleClickUndocksWidget()` / `dockAreaDoubleClickUndocksWidget()`, matching QADS `DoubleClickUndocksWidget`. The Native default is enabled like QADS `DefaultBaseConfig`; disabling it suppresses both per-tab detach and the Native whole-group double-click extension. QADS compatibility delegates to the global config flag. Runtime gesture behavior is not verified.
- Native exposes `setDockAreaHasTabsMenuButton()` / `dockAreaHasTabsMenuButton()`, matching QADS `DockAreaHasTabsMenuButton`. The default remains enabled like QADS `DefaultDockAreaButtons`; disabling it hides the panel-list button across docked, floating, and Auto-Hide tab surfaces while retaining the existing dynamic-visibility setting for when the button is enabled. Runtime visibility is not verified.
- Native exposes `setDockAreaHasCloseButton()` / `dockAreaHasCloseButton()` and `setDockAreaCloseButtonClosesTab()` / `dockAreaCloseButtonClosesTab()`, matching QADS `DockAreaHasCloseButton` and `DockAreaCloseButtonClosesTab`. Defaults match `DefaultDockAreaButtons` (shown, closes the area); active-tab mode closes only the current panel. Native keeps its closable, Pin, and close-protected guards for both modes. The close control is available on docked and floating tab surfaces; runtime behavior is not verified.
- Native exposes `setDockAreaHasUndockButton()` / `dockAreaHasUndockButton()`, matching QADS `DockAreaHasUndockButton`. The default is enabled as part of QADS `DefaultDockAreaButtons`; disabling it hides the docked tab surface's Float tab group control without changing per-panel floatability or drag-to-float behavior. QADS compatibility delegates to the global config flag. Runtime visibility is not verified.
- Native's temporary drop overlay now distinguishes the active dock area's center/four-side targets from container outer-edge targets, and also previews Auto-Hide sidebar insertion. Docked side-area tab surfaces now have a persistent Auto-Hide control; by default it moves the active panel, while the optional `AutoHideButtonTogglesArea` layout setting or Ctrl-click moves the open group. The setting is available through `ArtifactMainWindow` and the tab context menu. The group operation is disabled if any open member cannot be Auto-Hidden. A separate `Auto-hide tab group to` context submenu moves all open members together while retaining their common return group and split metadata. `Float tab group` and `Close tab group` controls also sit beside the overflow list. Float requires all members to be floatable and unlocked; Close requires every open member to be closable, unpinned, and not protected.
- Main-surface `dragEnter` / `dragMove` preflight now resolves container-edge versus dock-area targets from the overlay mode that is actually shown. This matches the later drop decision and avoids rejecting a valid area split when its arrow overlaps the outer edge band. Runtime drag behavior remains unverified.
- QADS 4.5.0 keeps its dock-area title bar above the panel when `TabsAtBottom` is enabled and moves only the tab bar below the content. Native keeps a separate upper action/title row in this mode, displays the active panel title, and starts a whole-group drag from that title area; this source-level change still needs QStyle and runtime layout validation.
- Native now keeps QADS's per-tab double-click detach and also treats a double-click on empty space in a docked tab bar as a request to float the whole tab group. The group path uses the existing all-members floatability/lock check and group layout operation; floating tab-bar empty space is ignored.
- Dragging empty space in a docked or floating tab bar now starts a whole-group drag. A floating window with one tab surface also starts that drag from its custom header. The temporary overlay previews tab insertion, dock-area center/four-side targets, container outer-edge targets, and Auto-Hide sidebar insertion. A split uses the existing single-panel split to create a new tab surface, then moves the other group members there in their original order and restores the selected tab. Auto-Hiding a floating group returns its open members to a docked tab group in the original order. Releasing outside an accepted target floats a docked group or repositions an already floating group. The custom header gesture is disabled for floating windows containing multiple split surfaces. These multi-panel split and overlay paths are source-level only and still need interactive validation.
- This is a source-level implementation only. Root edge drops, nested root splits, save/restore, and Auto-Hide return still require interactive validation; no parity claim is made for all QADS container behaviors.
- This is source-level comparison only. Native behavior still requires an interactive drag/drop check after the user authorizes runtime testing.

## Native Auto-Hide drop flow
- QADS 4.5.0's container overlay can target Auto-Hide side bars, and its drop path registers the dragged dock widget with the selected side and insertion index.
- Native now shows a temporary highlight and insertion marker over the selected Auto-Hide bar, then routes docked and floating panels through the same side/index placement. Clicking or keyboard-activating a sidebar tab toggles its flyout; its context menu can dock the panel back or choose another Auto-Hide edge. The return record retains the original tab group/order and split path; dock-back rejoins a surviving group member, or rebuilds the saved split leaf when that leaf collapsed. Layout serialization retains the return group and path.
- These interactions are source-level changes only. The visual overlay, tab ordering, floating-source drop, and undo/layout restoration still require interactive validation.

## Additional context
- Earlier project notes show that Artifact previously experimented with vendored QADS source and direct QADS patches:
  - `Artifact/docs/INVESTIGATION_PROJECT_TREE_FLOATING_RESIZE_2026-03-14.md`
- Current build path no longer appears to use that local vendored source directly for normal builds.
- The legacy compatibility build links through:
  - `find_package(qtadvanceddocking-qt6 REQUIRED)`
  - only when `ARTIFACT_QADS_COMPAT_BACKEND=ON`

## Working hypothesis
- The most suspicious local divergence is forcing extra window behavior onto `DockOverlay` widgets:
  - `prepareDockDropOverlayWindow(...)`
- If QADS expects its overlay widgets to remain plain internal helper widgets, changing flags such as top-most behavior may interfere with normal preview display or cursor-area detection.

## Suggested next steps
1. When validating the optional QADS compatibility backend, temporarily bypass Artifact-side overlay post-processing and compare against stock behavior.
2. If the compatibility-path issue remains, reduce local `CDockManager::setConfigFlag(...)` customizations to the smallest stock-like set and compare again.
3. If needed, use a local vendored QADS source for direct instrumentation inside:
   - `DockOverlay.cpp`
   - `FloatingDragPreview.cpp`
   - `FloatingDockContainer.cpp`

## Current status
- Native dock migration is now the default main-window path. The native surface
  handles embedded left/right/top/bottom/center docks, local nested splits,
  tab-group prefix routing, floating docks through owned top-level dialogs,
  portable layout entries, splitter sizing, close/visibility/activation, and
  tab-bar drag/drop between native tab surfaces.
- The MainWindow now selects Native unconditionally; the former
  `ARTIFACT_DISABLE_NATIVE_DOCK` runtime escape hatch has been removed.
- QADS-specific `DockStyleManager` / `DockGlowStyle` compilation is opt-in via
  the CMake option `ARTIFACT_QADS_COMPAT_STYLE` (default `OFF`).
- When either QADS compatibility option is enabled, CMake resolves and links
  the QADS package; with both options disabled, no QADS package is required.
- QADS remains available only as an opt-in source compatibility backend. The
  default CMake configuration does not define `ARTIFACT_QADS_COMPAT_BACKEND`
  and does not require the QADS package. The Native-only MainWindow no longer
  constructs a QADS `CDockManager` at runtime. The backend data contract now
  exposes only `native`; QADS adapter/types remain behind the compatibility
  boundary for optional builds. The Native drop overlay previews center and
  side destinations; side drops over a populated dock area create a local split,
  while empty-area drops resolve to a root area. Split paths and current ratios
  use optional layout fields so older JSON remains readable. Empty split leaves
  collapse after panel moves and floating. Floating windows have their own
  local split trees, target overlay, and path/ratio persistence. These changes
  reject an unsuccessful overlay split instead of silently moving to another
  fixed area, and clear target previews on drag leave. They have not been built
  or runtime-verified. The overlay now also distinguishes outer-container edge
  drops from local dock-area splits, matching QADS's two-stage target decision.

- `ArtifactMainWindow::setDockNoTab()` maps to QADS 4.5.0's
  `CDockWidget::NoTab` in the optional compatibility backend and to an
  independent Native open/closed state otherwise. The open/closed state is
  stored separately from
  tab-header visibility, so hidden headers remain open through layout save,
  restore, floating, and split reconstruction, and NoTab panels are omitted
  from the tab overflow list. The JSON field is optional and defaults to the
  existing visible-tab behavior. Source-only change; no build or runtime
  verification was performed.
- Native's docked tab context menu now has QADS-style `Close other panels`.
  It affects open, closable, unpinned siblings in the same tab group and records
  the batch through the existing layout undo path. This is source-only and has
  not been interactively verified.
- Re-docking an individual panel from a floating group now preserves that
  panel's logical open state instead of inheriting the floating window's
  aggregate visibility. A closed sibling in a still-visible floating window
  therefore remains closed when moved. This path has not been interactively
  verified.
