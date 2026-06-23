# QADS Upstream Comparison Investigation (2026-06-23)

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
- ArtifactStudio currently has app-side QADS overlay intervention that does not exist in upstream.
- This makes the overlay path a strong suspect for:
  - missing insertion preview during drag
  - behavior that feels different from default QADS

## Additional context
- Earlier project notes show that Artifact previously experimented with vendored QADS source and direct QADS patches:
  - `Artifact/docs/INVESTIGATION_PROJECT_TREE_FLOATING_RESIZE_2026-03-14.md`
- Current build path no longer appears to use that local vendored source directly for normal builds.
- Current app code links through:
  - `find_package(qtadvanceddocking-qt6 REQUIRED)`

## Working hypothesis
- The most suspicious local divergence is forcing extra window behavior onto `DockOverlay` widgets:
  - `prepareDockDropOverlayWindow(...)`
- If QADS expects its overlay widgets to remain plain internal helper widgets, changing flags such as top-most behavior may interfere with normal preview display or cursor-area detection.

## Suggested next steps
1. Temporarily remove or bypass Artifact-side overlay post-processing and compare against stock behavior.
2. If the issue remains, reduce local `CDockManager::setConfigFlag(...)` customizations to the smallest stock-like set and compare again.
3. If needed, switch back to a local vendored QADS source for direct instrumentation inside:
   - `DockOverlay.cpp`
   - `FloatingDragPreview.cpp`
   - `FloatingDockContainer.cpp`

## Current status
- Investigation note only.
- No build or runtime verification performed in this step.
