# Project Tree Floating Resize Investigation (2026-03-14)

## Reported symptom
- `ArtifactProjectView` (Project Tree) becomes unstable or visually broken when the dock is in floating mode and repeatedly resized.

## What was checked
- `ArtifactMainWindow` floating refresh path:
  - `refreshFloatingWidgetTree(...)`
  - `scheduleFloatingRefresh(...)`
  - floating container `eventFilter(...)`
- `ArtifactProjectView` refresh path:
  - `resizeEvent(...)`
  - `showEvent(...)`
  - `event(...)`
  - `refreshVisibleContent(...)`
- `ArtifactProjectManagerWidget` parent refresh path:
  - `resizeEvent(...)`
  - `showEvent(...)`
  - `event(...)`

## Findings / concern
- `ArtifactProjectView` had `refreshVisibleContent()` on `show` and activation-like events, but not reliably on resize burst.
- In floating mode, resize events are frequent and can leave `QTreeView` internals (`doItemsLayout`/`updateGeometries`) behind if only `viewport()->update()` is called.
- This mismatch can look like temporary layout breakage (row/viewport/header geometry desync).

## Change applied
- Added deferred/coalesced refresh scheduling:
  - `scheduleProjectViewRefresh(ArtifactProjectView* view)` (single-shot, coalesced with QObject property flag)
- Replaced direct repaint-only paths with deferred full refresh:
  - `ArtifactProjectView::resizeEvent`
  - `ArtifactProjectView::showEvent`
  - `ArtifactProjectView::event` (activation/polish)
  - `ArtifactProjectManagerWidget::resizeEvent`
  - `ArtifactProjectManagerWidget::showEvent`
  - `ArtifactProjectManagerWidget::event` (activation/polish)

## Why this direction
- Keeps refresh cost bounded (coalesced once per event burst).
- Forces `QTreeView` to run:
  - `doItemsLayout()`
  - `updateGeometries()`
  instead of relying on paint-only updates.

## Manual verification checklist
1. Undock `Project` panel (floating window).
2. Drag-resize quickly in both width/height directions for 10+ seconds.
3. Check for header/row overlap, stale blank zones, or delayed redraw.
4. Expand/collapse large folders while resizing.
5. Re-dock then float again and repeat.

## Remaining risk
- No runtime verification was done in this pass.
- If instability remains, next step is to instrument resize/refresh counts and inspect `QHeaderView` section geometry at each refresh.

---

## Follow-up note (same day, additional report)
- Additional user symptom:
  - expanding floating window increases black area
  - shrinking shows many residual lines
- This pattern suggests partial viewport update artifacts under heavy resize bursts.

### Additional mitigation applied
- `ArtifactProjectView` switched to safer redraw settings for resize-heavy scenarios:
  - `setUniformRowHeights(true)`
  - `setAnimated(false)`
  - `setVerticalScrollMode(QAbstractItemView::ScrollPerPixel)`
  - `setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel)`
  - viewport `AutoFillBackground` enabled, `WA_OpaquePaintEvent` enabled, and `WA_StaticContents` disabled
  - stylesheet explicitly sets `QTreeView::viewport` background
- `ArtifactProjectView::resizeEvent` now performs immediate `refreshVisibleContent()` and `viewport()->repaint()`.

### Rationale
- Forces full viewport repaint and geometry refresh during interactive resize.
- Reduces stale scanline/empty-region artifacts caused by incremental painting.

---

## Follow-up note 2 (same day, model-side fix)
- After the redraw-only changes did not materially improve the issue, the internal tree model became the primary suspect.

### Model issues found
- `ArtifactProjectModel::rowCount(...)` used only the top-level row to derive the parent source index.
- `ArtifactProjectModel::columnCount(...)` had the same top-level-only assumption.
- `ArtifactProjectModel::index(...)` ignored nested parent mapping and rebuilt children from the wrong source parent.
- `ArtifactProjectModel::parent(...)` and `mapToSource(...)` relied on inconsistent reconstruction paths.
- `flags(...)` checked the wrong role when deciding folder drop support.

### Fix applied
- Reworked model/source mapping to prefer `QStandardItem*` from `internalPointer()`.
- `mapToSource(...)` now uses `indexFromItem(...)` when possible.
- `rowCount(...)`, `columnCount(...)`, `index(...)`, and `parent(...)` now resolve the actual nested source parent.
- `flags(...)` now checks `ProjectItemType` instead of the composition-id role.

### Why this matters
- Floating resize causes `QTreeView` to aggressively re-query parent/child geometry.
- If the model returns mismatched parent/index relations, the view can overpaint, leave stale lines, or show blank regions even when repaint logic is otherwise correct.

---

## Follow-up note 3 (same day, floating container repaint path)
- Even after the model-side fix, the user still reported no meaningful improvement.
- That shifts suspicion back to the floating dock repaint path rather than the tree model alone.

### Additional change applied
- `ArtifactMainWindow::refreshFloatingWidgetTree(...)` now:
  - activates the root layout once before repaint
  - repaints the floating content root immediately
  - explicitly refreshes `ArtifactProjectView` via `refreshVisibleContent()`
  - repaints `QTreeView` headers and their viewports
  - repaints `QAbstractScrollArea` viewports, not only `update()`
- `scheduleFloatingRefresh(...)` now performs a second delayed refresh pass after ~16 ms.

### Rationale
- The project panel is a vertically stacked composite widget.
- A single `update()` pass on the floating container may not repaint newly exposed regions consistently during rapid live resize.
- The second pass is intended to catch post-layout exposed regions after Qt/QADS completes the resize sequence.

---

## Follow-up note 4 (same day, composite widget structure)
- User reported the issue became "much better" but still reproducible.
- At this point, `QWidget + QVBoxLayout` itself does not appear to be the direct bug.
- The more likely issue is the number of stacked child surfaces with their own backgrounds inside the floating dock.

### Additional change applied
- Grouped the top section into a single opaque chrome widget:
  - info panel
  - section label
  - search field
  - filter/progress row
- Marked the main project widget, top chrome, info panel, and bottom toolbox as opaque/autofill-background widgets.
- Kept the central `ArtifactProjectView` as the primary expanding region.

### Interpretation
- The current direction is to reduce repaint seams, not to abandon `QVBoxLayout` categorically.
- If problems still remain after this, the next architectural step should be reducing child surfaces further or moving to a simpler two-region composition, for example:
  - top chrome widget
  - tree view
  with less independent panel chrome inside the floating container.

---

## Follow-up note 5 (same day, splitter-based structure)
- User still reported breakage after the opaque-surface cleanup.
- At that point the remaining likely issue was the root geometry propagation path during floating resize.

### Additional change applied
- Replaced the root multi-widget `QVBoxLayout` stack with a `QSplitter`-based composition:
  - top chrome area
  - bottom tree host
- The bottom tree host now contains:
  - `ArtifactProjectView`
  - `ArtifactProjectManagerToolBox`
- Initial splitter sizes are set after construction so the tree remains the dominant resize target.

### Why this is stronger
- `QSplitter` performs geometry redistribution more explicitly than a plain multi-child vertical box during aggressive live resize.
- This reduces the chance that several fixed-height siblings and one expanding tree race each other for exposed region repaint.

### Outcome
- This introduced visible layout regressions and was reverted.
- Conclusion: `QSplitter` is not a good drop-in fix for this panel in its current form.

---

## Follow-up note 6 (same day, dock-wide opaque config)
- After reverting the splitter attempt, the next likely layer was QADS itself rather than only the project widget subtree.

### Additional change applied
- Enabled `CDockManager::DefaultOpaqueConfig` in `ArtifactMainWindow`.
- Marked `ArtifactProjectView` itself as opaque/autofill-background, not only its viewport.
- Marked `QHeaderView` and its viewport as opaque/autofill-background as well.

### Rationale
- The remaining artifacts may come from dock/floating container backing-store behavior during live resize.
- If the dock framework repaints in a translucent or incremental mode, child widget fixes alone may never fully eliminate the issue.

---

## Follow-up note 7 (same day, explicit paint fallback)
- Because the problem still persisted after opaque flags and dock config changes, the next fallback was to stop relying on stylesheet/background clearing alone.

### Additional change applied
- Added explicit background fill in paint handlers for:
  - `ArtifactProjectView`
  - `ArtifactProjectManagerWidget`
  - `ArtifactProjectManagerToolBox`
  - `ProjectInfoPanel`

### Rationale
- If exposed regions are not being cleared consistently during live floating resize, explicitly painting solid backgrounds is the most direct way to avoid black/stale backing-store remnants.

---

## Current conclusion (2026-03-15)
- Comparative testing showed the issue is specific to the QADS floating window path.
- The same project view logic did not reproduce the failure in the same way when hosted outside the QADS floating container path.
- That means the primary defect is no longer best explained by:
  - `ArtifactProjectView`
  - `ArtifactProjectManagerWidget`
  - the project model alone
- The most likely problem area is the Windows-specific floating behavior of QADS / `CFloatingDockContainer`.

## Most promising next fixes
1. Try QADS native-window mode first.
   - Upstream `CFloatingDockContainer` already has a `CDockManager::UseNativeWindows` path.
   - This is a better first experiment than ad-hoc child widget repaint workarounds.
   - If it helps, keep the change localized to floating containers only.

2. If native-window mode is ineffective, patch QADS itself rather than the project widget tree.
   - The most realistic permanent fix is inside `CFloatingDockContainer` / floating dock internals.
   - On Windows, upstream appears to rely mainly on `nativeEvent(...)`, not a dedicated `paintEvent(...)`.
   - If a local fork is acceptable, instrument and patch the QADS floating implementation directly.

3. If patching QADS is undesirable, reduce reliance on QADS floating for the Project panel.
   - Keep the Project panel docked in QADS.
   - For separate inspection workflows, open a standard Qt top-level window or `QDockWidget`-based diagnostic/project window instead of QADS floating mode.

## Recommended practical path
- Short term:
  - test native-window behavior for the floating root
- Medium term:
  - if not fixed, fork/patch the QADS floating container path
- Avoid:
  - continuing to pile repaint/layout workarounds into `ArtifactProjectView`
  - these mitigations improved symptoms but did not eliminate the root issue

## Follow-up note 8 (2026-03-15)
- The locally installed QADS headers in this environment do not expose a `CDockManager::UseNativeWindows` flag.
- As a substitute experiment, the application temporarily set `Qt::WA_NativeWindow` on the QADS floating root (`CFloatingDockContainer`) in `prepareFloatingDockContainer(...)`.
- Result:
  - did not fix the corruption
  - introduced a new regression where the upper portion of the floating content became obscured/clipped
- This experiment was reverted.

---

## Follow-up note 8 (WA_OpaquePaintEvent 全面除去)
- Follow-up 6–7 で追加した `WA_OpaquePaintEvent` と明示的 `paintEvent` fill を全面除去するアプローチを試行。

### 仮説
- `WA_OpaquePaintEvent` が設定されていると、Qt のバッキングストアはリサイズ時に新規露出領域をクリアしない。
- フローティングドックのライブリサイズ中、DWM がフレームを合成するタイミングに `paintEvent` が間に合わず、未初期化（黒）の領域がそのまま表示される。
- `WA_OpaquePaintEvent` を除去すれば、バッキングストアが自動でパレット背景色でクリアするため解消されるはず。

### 変更内容
- `ArtifactProjectView`、viewport、header、`ProjectInfoPanel`、`ArtifactProjectManagerWidget`、chromePanel、filterBarHost、`ArtifactProjectManagerToolBox` の全てから `WA_OpaquePaintEvent` を除去。
- viewport に明示的パレット設定（`QPalette::Window` / `QPalette::Base` = `#1E1E1E`）を追加。
- `ArtifactProjectView::paintEvent` は `QTreeView::paintEvent` への単純デリゲートに簡素化。
- `refreshFloatingWidgetTree` から `layout->activate()`、全子ウィジェットへの強制 `repaint()`、`header->updateGeometry()` を除去し、`refreshVisibleContent()` + `widget->update()` のみに簡素化。
- `refreshDockWidgetSurface` から `updateGeometry()` の連鎖呼び出しを除去。

### 結果
- **改善なし。** 症状は従来と同一で、フローティングリサイズ時の黒領域・残像は解消されなかった。
- バッキングストアのクリアタイミングの問題ではなく、別のレイヤーに原因がある可能性。

### 考察
- これまでに試行した全アプローチ（refresh coalescing、model fix、aggressive repaint、opaque surface、splitter、DefaultOpaqueConfig、explicit paintEvent fill、WA_OpaquePaintEvent 除去）のいずれも根本解決に至っていない。
- 次の調査方向としては：
  1. **QADS の CFloatingDockContainer 内部のバッキングストア管理** — QADS が独自に backing store を管理している可能性。
  2. **Qt item-view 系 (`QTreeView`) と QADS floating の相性** — view / header / viewport の多層描画が live resize に弱い可能性。
  3. **Project View を単一描画面へ置き換えることによる回避** — `QAbstractScrollArea` ベースの custom tree view で、header/row/selection/expand を 1 面描画に寄せる。

---

## Follow-up note 9 (2026-03-15, custom tree view 実装着手)
- `ProjectView` については、`QTreeView` ベースの回避策を積み増すより、独自ビューへ切り替える方針に変更。

### 実装方針
- `ArtifactProjectView` の基底を `QTreeView` から `QAbstractScrollArea` へ変更。
- 可視行をフラット化して保持し、以下を 1 面描画する:
  - header
  - row background
  - expand/collapse glyph
  - icon
  - text
  - selection / hover
- モデル層は既存 `QAbstractItemModel` / `QSortFilterProxyModel` を継続利用。
- `ProjectManagerWidget` 側の `header()` 依存は除去し、列幅だけ custom view へ直接設定。

### 期待している効果
- `QTreeView` の header / viewport / delegate / item-view geometry 更新に依存しなくなる。
- QADS floating resize 中でも、再描画責務を `ArtifactProjectView` 単体へ閉じ込められる。

### 現時点の位置づけ
- これは QADS の根本不具合修正ではなく、`ProjectView` 側の依存面を減らす回避策。
- ただし、比較テストの結果として QADS floating で壊れる条件に `QTreeView` が強く絡んでいる可能性があるため、実装価値は高い。

---

## Follow-up note 10 (2026-03-15, local QADS source vendoring)
- `CFloatingDockContainer` を直接調査・修正できるように、QADS を vcpkg バイナリ参照だけでなくローカルソースからも組める構成へ変更。

### 変更内容
- `third_party/Qt-Advanced-Docking-System` を追加。
- root `CMakeLists.txt` に `ARTIFACT_USE_LOCAL_QADS` を追加。
- ローカル QADS ソースが存在する場合は:
  - `add_subdirectory(third_party/Qt-Advanced-Docking-System EXCLUDE_FROM_ALL)`
  - `ads::qtadvanceddocking-qt6` をローカル target から供給
- `Artifact` / `ArtifactWidgets` 側は、既に target がある場合は `find_package(qtadvanceddocking-qt6)` を呼ばないよう変更。

### 目的
- 次段階で `CFloatingDockContainer.cpp` の Windows 経路を直接修正できるようにするため。
- アプリ側 workaround ではなく、QADS floating container 本体へ責務を戻すため。

---

## Follow-up note 11 (2026-03-15, direct QADS floating refresh patch)
- vendored QADS の `FloatingDockContainer.cpp` に対して、Windows の floating resize 後の再描画を強めるパッチを追加。

### 変更内容
- `CFloatingDockContainer::nativeEvent(...)` の以下で refresh を coalescing:
  - `WM_MOVING`
  - `WM_ENTERSIZEMOVE`
  - `WM_SIZE`
  - `WM_SIZING`
  - `WM_WINDOWPOSCHANGED`
  - `WM_EXITSIZEMOVE`
- `CFloatingDockContainer::showEvent(...)` 後にも refresh を追加。
- refresh 内容:
  - floating root の `layout()->activate()`
  - 子 `QWidget` 再帰 update
  - `QAbstractScrollArea::viewport()` の `update()/repaint()`
  - `RedrawWindow(..., RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW)`
  - 0ms / 16ms の 2 パス

### 狙い
- QADS floating root 自体の live resize 後に、backing store と child viewport の露出領域再描画を漏らさないこと。
  1. **QADS の CFloatingDockContainer 内部のバッキングストア管理** — QADS が独自に backing store を管理している可能性。
  2. **Windows DWM レベルの同期** — `nativeEvent` で `WM_PAINT` / `WM_ERASEBKGND` をフックし、同期的にビューポートを塗る。
  3. **フローティングコンテナの `WA_NativeWindow` 設定** — ネイティブウィンドウ化で DWM との同期を改善。
  4. **フローティングモード時のみツリー全体を単一の `QWidget::render()` で合成して描画する方式**。
  5. **QADS のフローティングリサイズ実装自体の調査** — `CFloatingDockContainer::event()` / `resizeEvent()` が子ウィジェットのレイアウト伝播をどう処理しているか。

---

## Follow-up note 12 (2026-03-15, resize ordering / layout propagation correction)
- 追加調査の結果、現行の Project View は `QTreeView` ではなく、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` の `ArtifactProjectView : QAbstractScrollArea` ベース独自ウィジェットであることを再確認。
- したがって、未解決分は `QTreeView` 固有不具合ではなく、**Windows live resize 中の `CFloatingDockContainer` で、Qt の `resizeEvent` 後段までレイアウト伝播を同期し切れていなかった**可能性が高い。

### 観測と推定
- 既存対策では `WM_SIZE` を `nativeEvent(...)` で受けた時点で `refreshFloatingWidgetSubtree(this)` を同期実行していた。
- ただし、このタイミングは Qt 側の `QWidget::resizeEvent()` / layout geometry 反映より前段である可能性があり、その場合:
  - ルート `QBoxLayout` (`CFloatingDockContainer` 自身の layout)
  - `CDockContainerWidget` の `QGridLayout`
  - その子の dock area / scroll area
  のサイズ伝播がまだ完了しておらず、同期 repaint が古い geometry を基準に走る。
- さらに、helper 内で子孫に対して `updateGeometry()` を再帰実行していたため、modal sizing loop 中に追加の `LayoutRequest` が積まれ、Qt event loop が止まっている間は処理が後ろ倒しになっていた可能性がある。

### 今回の修正
- vendored QADS の `third_party/Qt-Advanced-Docking-System/src/FloatingDockContainer.cpp` / `third_party/Qt-Advanced-Docking-System/src/FloatingDockContainer.h` を追加修正。
- Windows で `CFloatingDockContainer::resizeEvent(QResizeEvent*)` を override し、**`Super::resizeEvent(event)` の後**に、spontaneous resize に対して同期 refresh を実行。
- `nativeEvent(...)` の `WM_SIZE` では同期 refresh をやめ、従来どおり coalesced delayed refresh のみ残した。
- `refreshFloatingWidgetSubtree(...)` は以下へ整理:
  - `QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest)` で pending layout request を flush
  - subtree の各 widget の `layout()->activate()`
  - 再度 `LayoutRequest` flush
  - `update()` / `repaint()` と `QAbstractScrollArea::viewport()` の repaint
- これにより、Windows modal sizing loop 中でも、**Qt が実サイズを反映した後段で layout activation と paint を同期実行**できる形へ寄せた。

### 位置づけ
- これは「`QTreeView` 特有 workaround」の継続ではなく、QADS floating root の resize ordering を是正する修正。
- 特に `QVBoxLayout` / `QGridLayout` / custom `QAbstractScrollArea` を含む一般的な dock 内容物に効くことを狙っている。

### 検証メモ
- `cmake --build out/build/x64-Debug --target Artifact --config Debug` は実行したが、今回の変更箇所より前段で MSVC 環境が C++ 標準ヘッダ (`utility`, `type_traits`) を解決できず停止。
- したがって、このターンではローカル実行ビルドによる動作確認までは未完了。
