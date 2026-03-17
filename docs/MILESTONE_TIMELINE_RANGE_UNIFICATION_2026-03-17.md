# Timeline Range Unification Milestone

日付: 2026-03-17

## Goal

タイムライン上で分散している `in/out`、`work area`、`seek`、`render range` の責務を整理し、編集と出力で同じ時間範囲モデルを共有する。

最低限ここを満たす。

- ユーザーが「どの範囲を見ていて、どこを書き出すか」を迷わない
- `ArtifactTimelineWidget` 上の表示と `RenderQueueManagerWidget` の範囲指定が矛盾しない
- UI ごとの独自解釈ではなく service 側の共通ルールで時間範囲を扱う
- 将来の trim / split / preview range 追加時に破綻しない土台を作る

## Scope

- `ArtifactTimelineWidget`
- `ArtifactTimelineNavigatorWidget`
- `ArtifactTimelineScrubBar`
- `ArtifactWorkAreaControlWidget`
- `TimelineTrackView`
- `RenderQueueManagerWidget`
- timeline / composition / render 系 service の range 取得と更新

## Non-Goals

- Diligent / DX12 backend の低レベル描画修正
- 1 ライン複数クリップ化
- 本格的な playback engine の作り直し
- Render Queue 全体の UX 再設計

## Canonical Model

このマイルストーンでは、時間範囲を以下の 4 種に固定する。

- `Current Time`
  現在フレーム。`seek` 操作の結果として 1 点を持つ。
- `Visible Range`
  タイムライン右ペインで今表示している範囲。主に navigator / zoom が担当する。
- `Work Area`
  編集中に注目する範囲。preview や作業区間の基準になる。
- `Render Range`
  実際に書き出す範囲。既定では `Work Area` か `Composition Full Range` から選ぶ。

この 4 種を UI ごとに勝手に混ぜない。`Current Time` は点、他 3 つは区間として扱う。

## Milestones

### M-RANGE-1 Terminology And State Audit

目的:
既存コード上の range 用語と保持場所を洗い出し、どこが source of truth かを固定する。

対象:

- composition の frame range
- timeline widget 内の visible/start/end 管理
- work area bar の in/out
- render queue の from/to

実装方針:

- UI 名称は `docs/WIDGET_MAP.md` の呼称に合わせる
- `Current Time` / `Visible Range` / `Work Area` / `Render Range` の区別をドキュメント化する
- service を経由せず widget 内に閉じた range 状態を列挙する
- 暫定状態と正規状態を切り分ける

Done:

- 既存の range 状態一覧が docs に残る
- 各 range の source of truth が定義される
- あいまいな用語が減る

### M-RANGE-2 Shared Range Service Contract

目的:
timeline と render が共通で参照できる最小の range API を作る。

対象:

- composition current time
- work area get/set
- full composition range 取得
- render range resolve

実装方針:

- UI 直結の座標値ではなく frame ベースの API に寄せる
- set 時に clamp / normalize を共通化する
- `start > end` や composition 外の値を service 側で防ぐ
- Render Queue からも同じ API を参照できるようにする

Done:

- UI ごとの個別 clamp が減る
- work area 更新と render range 解決の規則が 1 箇所で分かる
- timeline と render queue が同じ frame range を見られる

### M-RANGE-3 Timeline UI Binding Cleanup

目的:
タイムライン各部品が同じ range モデルを表示し、相互更新でズレないようにする。

対象:

- `ArtifactTimelineNavigatorWidget`
- `ArtifactTimelineScrubBar`
- `ArtifactWorkAreaControlWidget`
- `TimelineTrackView`

実装方針:

- navigator は `Visible Range` のみ担当する
- scrub bar は `Current Time` 表示と変更に責務を限定する
- work area bar は `Work Area` の編集に限定する
- track view は `Current Time` と `Visible Range` を描画へ反映する

Done:

- スクロール、ズーム、シーク、work area 変更で UI 間の表示がずれない
- 「見えている範囲」と「作業範囲」が分離される
- playhead 表示が frame 表示と矛盾しない

### M-RANGE-4 Render Queue Bridge

目的:
Render Queue の範囲指定を timeline 側の range モデルと接続する。

対象:

- `RenderQueueManagerWidget`
- render job range preset
- current composition bridge

実装方針:

- render range は `Full Composition` / `Work Area` / `Custom` の 3 系統を基本にする
- `Work Area` を使う場合、timeline 側の変更が render UI に再反映される
- `Custom` は job 固有値として保持し、work area と区別する
- 現在 composition 未選択時の disabled 条件を明確にする

Done:

- render 範囲の既定値が分かりやすい
- work area を編集しても render queue が古い値を抱えにくい
- custom range と shared range が混線しない

### M-RANGE-5 Validation And Regression Checklist

目的:
時間範囲まわりの回帰を見逃しにくくする。

対象:

- manual regression checklist
- edge case 列挙
- basic diagnostics

実装方針:

- 端フレーム、負値、空範囲、1 フレーム範囲を確認項目に入れる
- composition 切り替え時の current time / work area 引き継ぎを確認する
- render queue job 作成直後の range 初期値を固定する
- timeline 操作後に queue を開いた時の表示一致を確認する

Done:

- 主要ケースの確認表がある
- 範囲のズレを再現しやすい
- 後続 milestone の前提として安全網になる

## Recommended Order

1. `M-RANGE-1 Terminology And State Audit`
2. `M-RANGE-2 Shared Range Service Contract`
3. `M-RANGE-3 Timeline UI Binding Cleanup`
4. `M-RANGE-4 Render Queue Bridge`
5. `M-RANGE-5 Validation And Regression Checklist`

## Current Risks

- widget ごとのローカル状態が残ると、見た目だけ直って内部矛盾が残る
- `RenderQueueManagerWidget` が job 固有状態を強く持っている場合、shared range と衝突する
- composition 切り替え時の current time / work area の所有者が曖昧だと regression が出やすい

## Validation Checklist

- navigator の表示範囲変更で work area が勝手に変わらない
- work area 変更で current time が不正に飛ばない
- scrub bar クリックと track view シークで同じ current frame になる
- composition の full range 外へ work area を設定できない
- render queue の `Work Area` preset が timeline の work area と一致する
- render queue の `Custom` preset が timeline 変更で上書きされない
- composition 切り替え時に前の composition の range が混入しない
