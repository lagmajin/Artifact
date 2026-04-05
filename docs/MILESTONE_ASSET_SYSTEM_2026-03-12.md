# Asset System Milestone

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

- missing / relinked / imported / unused が見える
- image は resolution、video は fps / duration、audio は basic info を表示
- `Project View` と `AssetBrowser` の表示差が大きくない

主な作業:

- `Project View` の status presentation
- `AssetBrowser` の metadata panel 拡張
- missing / unused badge

### M-ASSET-4 Relink and Recovery

壊れた asset 参照を実用的に戻せるようにする。

完了条件:

- selected asset relink
- bulk relink
- missing asset search root
- relink 結果の表示

主な作業:

- relink UI 共通化
- status refresh
- missing asset filtering

### M-ASSET-5 Organization

asset を整理しやすくする。

完了条件:

- folder / bin 整理
- unused / fonts / media などの view
- search と type filter の共存

主な作業:

- virtual sections
- folder organization polish
- browser / project tree の責務整理

### M-ASSET-6 Save and Restore Integrity

asset 状態が保存再読込で落ちないようにする。

完了条件:

- imported asset path
- relinked path
- missing status
- folder organization
- selected / active composition との整合

主な作業:

- serialization coverage
- reload validation
- regression checklist

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
