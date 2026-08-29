# Asset System Milestone

**最終更新:** 2026-08-29

日付: 2026-03-12

## Goal

`AssetBrowser` と `Project View` を別物の UI ではなく、同じ asset system の別面として揃える。

最終的には、

- import した asset がすぐ見える
- metadata / missing / relink 状態が分かる
- browser から project へ追加できる
- project 側の整理結果が browser と矛盾しない

までを通す。

## Scope

- `ArtifactAssetBrowser`
- `ArtifactProjectManagerWidget`
- `ArtifactProjectModel`
- `ArtifactProjectService`
- asset import / relink / metadata presentation

## Milestones

### M-ASSET-1 Browser Navigation Baseline

`AssetBrowser` を最低限使える navigator にする。

完了条件:

- 左フォルダツリーと右ファイルビューが同期する
- search / type filter / folder navigation が破綻しない
- selection / double-click / filesDropped が外へ通知される
- thumbnail size と file details が実用レベルになる

主な作業:

- folder navigation の左右同期
- multiple selection 対応
- file details 強化
- `Up / Refresh` などの基本導線

### M-ASSET-2 Browser to Project Bridge

browser 上の asset を project に追加する導線を固める。

完了条件:

- double-click / context menu / drag&drop で import できる
- import 結果が `Project View` に即反映される
- font / image / video / audio が最低限判別される

主な作業:

- import 後 refresh
- import type summary
- add-to-project 操作の一貫化

### M-ASSET-3 Metadata and Status

asset の重要状態を UI に出す。

完了条件:

- [x] missing ファイルの赤色表示（Project View）
- [x] unused アセットの黄色表示と [Unused] プレフィックス
- [x] image/video/audio の metadata 表示（解像度、fps、形式）
- [x] Project View と AssetBrowser の表示整合

主な作業:

- [x] `Project View` の status presentation（赤色/黄色ForegroundRole）
- [x] `AssetBrowser` の metadata panel 拡張
- [x] missing / unused badge（両ビューで実装済み）

### M-ASSET-4 Relink and Recovery

壊れた asset 参照を実用的に戻せるようにする。

完了条件:

- [x] selected asset relink（AssetBrowser / ProjectManagerWidget）
- [x] bulk relink（ProjectManagerWidget::relinkMissingFootage）
- [x] missing asset search root（findByFileName）
- [x] relink 結果の表示（成功メッセージ）

主な作業:

- [x] relink UI 共通化（両ウィジェットで実装済み）
- [x] status refresh（relink後の更新）
- [x] missing asset filtering（AssetBrowserで実装済み）

### M-ASSET-5 Organization

asset を整理しやすくする。

完了条件:

- [x] folder / bin 整理
- [x] unused / fonts / media などの view
- [x] search と type filter の共存

主な作業:

- [x] virtual sections（AssetBrowserで実装済み）
- [x] folder organization polish
- [x] browser / project tree の責務整理

### M-ASSET-6 Save and Restore Integrity

asset 状態が保存再読込で落ちないようにする。

完了条件:

- [x] imported asset path（toJson/restoreProjectItems）
- [x] relinked path（filePath保存済み）
- [x] missing status（validate()で検証追加）
- [x] folder organization（project itemsで保持）
- [x] selected / active composition との整合

主な作業:

- [x] serialization coverage（toJson/restoreProjectItems実装済み）
- [x] reload validation（validate()実装済み）
- [x] regression checklist（missing footageチェック追加）

## Recommended Order

1. `M-ASSET-1 Browser Navigation Baseline`
2. `M-ASSET-2 Browser to Project Bridge`
3. `M-ASSET-3 Metadata and Status`
4. `M-ASSET-4 Relink and Recovery`
5. `M-ASSET-5 Organization`
6. `M-ASSET-6 Save and Restore Integrity`

## Current Status

2026-03-12 時点で `M-ASSET-1` の一部は着手済み。

入っているもの:

- `Up / Refresh`
- left tree と right view の folder sync
- multiple selection signal
- file details の basic metadata
- folder open context action

## Status (2026-04-04)
- [x] Asset Browser Navigator Phase 1-4 (Splitter, Breadcrumb, History, Search, Presentation, Intelligence, Favorites, Context Menu) - Completed.

未着手寄り:

- imported / missing / unused badge
- browser と project の選択同期
- relink workflow の一体化
- save / restore 整合

## Status (2026-04-09)

- [x] browser ↔ project selection sync を往復方向で接続
- [x] Project View 起点の footage selection を Asset Browser に返す導線を追加
- [x] 両ペインに sync chip を出し、同期状態を画面で読めるようにした
- [x] Project View に missing ファイルの赤色表示を追加（ForegroundRole）
- [x] Project View に unused アセットの黄色表示と [Unused] プレフィックスを追加
- [x] プロジェクト検証に missing footage チェックを追加（validate()）
- [x] Relink workflow は既存実装済み（AssetBrowser / ProjectManagerWidget）

※ Project View の項目はprojectにimport済みなのでimported badgeは本質的に不要

次の slice: (全て完了)

## Update 2026-08-15

Project View／Project Model の tree、folder/bin、検索、type／unused／missing filter、列 sort／幅保存、Asset Browser との同期基盤を追加監査した。M-AS-2 では ProjectItem 個別の tag metadata／編集 UI／JSON保存復元／`tag:`検索を接続した。tag と cross-view runtime 受入れは未検証とする。

## Update 2026-08-15

`AppMain` の selection guard 付き Project View ↔ Asset Browser 双方向同期、Asset Browser／Project View の sync chip、imported／missing／unused status、metadata／waveform 表示、selected／bulk relink と Undo、Project View import の非同期経路を追加確認した。統合基盤は実装済み相当だが、import／relink／missing／unused の cross-view 即時反映、再読込後の選択・active composition 維持、実データでの通し runtime 受入れは未検証とする。

## Update 2026-08-28 — Input Sources

Project View の Footage に `Production`／`Render Input` の用途分類と、Alpha Matte／Luma Matte／Displacement／Depth／Normal／Texture の入力役割を追加した。分類は既存の Project Item ID と source path を維持したまま JSON 保存・復元され、コピー／送信バンドルにも含まれる。Render Input はレイヤーを自動生成せず、Project View の専用フィルター、一覧／タイルの用途表示、右クリックの `Input Source Role` から管理し、通常の unused 判定から除外する。

Inspector の既存 Matte 行から Input Source を新規 matte として追加、または既存 matte source と置換できる導線を接続した。参照は `ChangeLayerMatteReferencesCommand` を通るため Undo／Redo 対象となり、Project Item ID と明示的な互換 source path を layer JSON に保存する。Alpha Matte／Luma Matte の用途分類は初回選択時の MatteType 既定値にも反映する。

Composition Preview と Render Queue の既存 matte source resolver は、composition layer が見つからない場合に Project Item ID から Render Input を解決し、`ImageF32x4_RGBA`／OIIO decode と明示的な既存 QImage upload 境界を経て、既存の Diligent track-matte pipeline へ渡す。D3D12／Vulkan固有コード、shader、resource lifetime、同期方式は変更していない。runtime の保存再読込、Inspector操作、Preview／Render Queue parity は未検証。
### 2026-08-29: Timeline transition quick-add

- Timeline のレイヤーコンテキストメニューに「トランジションを追加」を追加。2 レイヤー選択時、現在フレームをカット中心として Crossfade / Wipe / Slide を 1 秒 span で `CompositionTimelineTransition` に登録する。
- 追加データは既存の composition timeline metadata と JSON 保存経路を再利用し、レイヤーは新規作成しない。
- 各トランジションは安定した ID を持つ独立編集単位になり、ID 指定の削除と enabled 切替 API を提供する。
- ID 指定の範囲変更 API (`setTimelineTransitionRange`) を追加し、将来のタイムライン trim／長さ編集を同じオブジェクトに接続できるようにした。
- UI/runtime のビルド検証は未実施（ユーザー許可待ち）。
