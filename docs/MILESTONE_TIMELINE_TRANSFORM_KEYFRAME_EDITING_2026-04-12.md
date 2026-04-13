# Timeline Transform Keyframe Editing Milestone

日付: 2026-04-12

## Goal

`Position` / `Rotation` / `Scale` のような transform 系プロパティを、タイムライン上で keyframe として具体的に編集できるようにする。

最低限ここを満たす。

- タイムライン上で transform 系の keyframe が見える
- `Position X/Y`, `Rotation`, `Scale X/Y` などの具体チャンネルを選べる
- keyframe の追加、移動、削除が同じ導線で扱える
- Inspector と timeline の編集結果が同じ property に反映される
- `TransformGizmo` の操作結果と keyframe 編集が衝突しない

## Scope

- `ArtifactTimelineTrackPainterView`
- `ArtifactTimelineKeyframeModel`
- `ArtifactCompositionEditor`
- `ArtifactInspectorWidget`
- layer transform property groups
- transform channel の keyframe 表示と操作

## Non-Goals

- Diligent / Vulkan の描画 backend 修正
- 全 property の完全な keyframe editor 化
- ベジェ曲線編集の全面実装
- 1 トラックに全 transform を混ぜる曖昧な UI

## Canonical Transform Model

このマイルストーンでは、transform は以下のように扱う。

- `Position X`
- `Position Y`
- `Rotation`
- `Scale X`
- `Scale Y`

必要なら後続で `Anchor` や `Z` を追加するが、最初は上記 5 チャンネルを優先する。

`Position` を 1 項目として隠すのではなく、編集単位として明示する。これにより、

- どの値を keyframe 化しているかが分かる
- 移動・回転・拡縮のショートカットと整合しやすい
- Inspector の数値編集と timeline 上の marker 編集を一致させやすい

## Channel Inventory

現状のコードベースで公開されている transform property path は以下。

- `transform.position.x`
- `transform.position.y`
- `transform.rotation`
- `transform.scale.x`
- `transform.scale.y`
- `transform.anchor.x`
- `transform.anchor.y`

タイムライン上の表示は以下の粒度に寄せる。

- `Transform / Position X`
- `Transform / Position Y`
- `Transform / Rotation`
- `Transform / Scale X`
- `Transform / Scale Y`
- `Transform / Anchor X`
- `Transform / Anchor Y`

優先順位:

- まず `Position X/Y`, `Rotation`, `Scale X/Y` を編集可能にする
- `Anchor X/Y` は gizmo 操作と干渉しやすいので後続でもよい
- 3D 専用の transform チャンネルが必要になったら別グループで扱う

## Milestones

### M-TKF-1 Channel Inventory And Naming

目的:
transform 系 property が実際にどの名前で公開されているかを棚卸しし、タイムライン表示名を固定する。

対象:

- layer transform property group
- `Position` / `Scale` / `Rotation` 系 property 名
- UI 上の表示ラベル

実装方針:

- 既存の property path と display label を確認する
- `Position` を `X/Y` に分解して表示するか、複合項目として扱うかを決める
- UI 名称は `docs/WIDGET_MAP.md` のタイムライン呼称に合わせる
- `TransformGizmo` の mode 名と混同しないラベルを使う
- まずは `transform.position.x/y`, `transform.rotation`, `transform.scale.x/y` を優先する
- `transform.anchor.x/y` は必要なら後続で開放する

Done:

- どの keyframe が何の値かを追える
- timeline 上の channel 名が固定される
- 迷いやすい `Position` 表記の扱いが決まる
- 実 property path と UI ラベルの対応表がある

### M-TKF-2 Keyframe Model Wiring

目的:
timeline 側の共通 keyframe モデルを、transform 編集に使える状態へ接続する。

対象:

- `ArtifactTimelineKeyframeModel`
- layer property accessor
- change notification

実装方針:

- property path 単位で keyframe を取得・追加・削除できるようにする
- `addKeyframe` / `removeKeyframe` の後に timeline へ change を通知する
- selection 中の layer と property を正しく解決する
- model 側で keyframe の共通更新を担当し、UI は直接 property を触りすぎない

Done:

- model 経由で transform keyframe を操作できる
- layer と property の変更通知が timeline に届く
- Editor と Track View が同じ keyframe データを見る

### M-TKF-3 Timeline Marker Editing

目的:
track view 上で keyframe marker を直接扱えるようにする。

対象:

- `ArtifactTimelineTrackPainterView`
- marker hit test
- context menu

実装方針:

- marker の追加、削除、ドラッグ移動を導線として用意する
- 複数選択 keyframe の移動を扱えるようにする
- hover / selected / active の見た目を分ける
- `Position X/Y` など channel 別に marker を識別する

Done:

- marker をクリックして選択できる
- marker をドラッグして frame を変更できる
- 右クリックで追加・削除できる

### M-TKF-4 Inspector And Editor Sync

目的:
Inspector 側の数値編集と timeline 側の marker 編集を同期させる。

対象:

- `ArtifactInspectorWidget`
- `ArtifactCompositionEditor`
- selection manager

実装方針:

- property row の値変更が timeline marker に反映される
- timeline で移動した keyframe が Inspector の値と一致する
- selection 変更時に編集対象 channel を明確にする
- transform gizmo の drag 結果を keyframe 編集と矛盾させない

Done:

- Inspector で変えた値が timeline に反映される
- timeline で動かした marker が Inspector に反映される
- gizmo と keyframe editor が別々の state を持たない

### M-TKF-5 Validation And Regression Checklist

目的:
transform keyframe 編集の回帰を見つけやすくする。

対象:

- manual regression checklist
- selection / focus / drag
- frame range boundary

実装方針:

- `Position X/Y` で別々の keyframe が見えるか確認する
- rotation と scale の keyframe が同じ frame に重なった時の表示を確認する
- selection 変更で wrong property を触らないか確認する
- gizmo drag 後に marker の値が壊れないか確認する

Done:

- 代表的な transform 編集操作のチェック表がある
- regression を再現しやすい
- 後続の curve editor 化へつなげやすい

## Suggested Order

1. `M-TKF-1 Channel Inventory And Naming`
2. `M-TKF-2 Keyframe Model Wiring`
3. `M-TKF-3 Timeline Marker Editing`
4. `M-TKF-4 Inspector And Editor Sync`
5. `M-TKF-5 Validation And Regression Checklist`

## Current Status

- `2026-04-12`
  - `ArtifactTimelineKeyframeModel` の簡易モデルは既にある
  - `ArtifactTimelineTrackPainterView` には keyframe marker の収集と操作の土台がある
  - transform の公開 property path は `transform.position.x/y`, `transform.rotation`, `transform.scale.x/y`, `transform.anchor.x/y`
  - まずは `Position X/Y`, `Rotation`, `Scale X/Y` を明示して、timeline と gizmo の責務を分けるのがよい

## Follow-up Notes

- 変形操作は gizmo の responsibility、細かい数値編集は timeline / Inspector の responsibility に寄せる
- `Position` を 1 行に隠さず、編集対象 channel を visible にしておくと混乱が減る
- timeline 側の marker 編集が先に安定すると、curve editor 追加や easing 編集へ拡張しやすい
