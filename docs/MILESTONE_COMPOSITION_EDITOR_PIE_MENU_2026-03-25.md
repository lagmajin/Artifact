# Composition Editor Pie Menu (2026-03-25)

`ArtifactCompositionEditor` で、ツール切替や viewport 操作補助を素早く呼べる
`pie menu` を導入するためのマイルストーン。

目的は単なる見た目追加ではなく、

- マウス移動量が少ない
- viewport 上で完結する
- 編集中の flow を切りにくい

という操作導線を作ることにある。

## Goal

- `Composition Viewer` 上で pie menu を開ける
- ツール切替、view 操作、表示切替の最低限を pie menu から実行できる
- 既存 toolbar / shortcut / tool manager と矛盾しない
- 将来的に 3D view や layer view にも流用できる責務分割にする

## Non-Goals

- 初手で全機能を pie menu に押し込むこと
- 複雑なアニメーション演出を先に作ること
- timeline / inspector まで同時に広げること
- 独自の巨大 UI framework を pie menu 用に作ること

## User Value

pie menu で先に解決したい操作は次の 3 系統。

- `Tool`
  - Selection
  - Hand / Pan
  - Pen / Mask 系
- `View`
  - Fit
  - 100%
  - Reset
- `Display`
  - Grid
  - Guides
  - Safe Area

この 3 系統を viewport 上で直接開けるだけでも、
toolbar 往復が減って編集テンポが改善する。

## Entry Trigger Candidates

pie menu を開くトリガー候補は複数あるが、最初から全部やらない。

候補:

- `Right Click Hold`
- `Space + Right Click`
- `Tab`
- `Q`

推奨初期案:

- `Tab` で開く
- 押している間だけ表示、離したら確定

理由:

- 既存の右クリック選択や context menu と衝突しにくい
- マウスドラッグだけで方向選択しやすい
- 実装時に mouse event の奪い合いが少ない

## Canonical Responsibilities

pie menu の責務は次のように分ける。

- `PieMenuModel`
  表示項目、アイコン、ラベル、コマンド ID、セクタ配置
- `PieMenuWidget` または `PieMenuOverlay`
  表示、hover、選択ハイライト、確定
- `PieMenuController`
  開始/更新/終了、入力イベント解釈、現在 tool/view state との同期
- `CommandBridge`
  選択結果を既存 API に流す

重要なのは、pie menu 自体が editor 機能を直接抱え込まないこと。

## Milestones

### PIE-1 Command Inventory

目的:
何を pie menu に入れるかを固定する。

対象:

- `ArtifactCompositionEditor`
- `CompositionRenderController`
- `ArtifactToolManager`

内容:

- 既存ショートカット、toolbar、display toggle を棚卸し
- pie menu 向きのコマンドだけを選ぶ
- 初期 6〜8 項目に絞る

Done:

- 初期 pie menu 項目一覧が docs に残る
- 将来追加候補と分離される

### PIE-2 Input Contract

目的:
開く/閉じる/確定/キャンセルのルールを固定する。

対象:

- `CompositionViewport`
- key / mouse event flow

内容:

- trigger key
- press / hold / release semantics
- drag distance threshold
- cancel 条件
- 他操作との優先順位

Done:

- `Space pan`
- `gizmo drag`
- `wheel zoom`
- `mouse selection`

との衝突ルールが決まる

### PIE-3 Overlay Rendering

目的:
pie menu を viewport 上に描けるようにする。

対象:

- `ArtifactCompositionEditor`
- overlay widget or popup widget

内容:

- 中央ノード
- 扇形セクタ
- hover highlight
- ラベルとアイコン配置
- DPI / resize / focus loss 対応

Done:

- パイメニューが viewport 上に安定表示される
- 選択方向が視覚的に読める

### PIE-4 Command Execution Bridge

目的:
pie menu の選択結果を既存 editor 機能へ接続する。

対象:

- tool switch
- zoom fit / reset / 100
- grid / guides / safe area toggle

内容:

- menu command ID と既存 action/API を結ぶ
- toolbar state と二重管理しない
- toggle 項目は現在 state を反映

Done:

- pie menu 経由の操作で既存 UI と状態不整合が出ない

### PIE-5 Operation Feel

目的:
「使える」ではなく「速い」と感じる操作感へ寄せる。

対象:

- activation delay
- radius
- dead zone
- sector angle
- release confirm

内容:

- 誤爆しにくい中心 dead zone
- 斜め方向の判定安定化
- release 時の確定感
- escape で確実にキャンセル

Done:

- 連続編集中でも邪魔に感じにくい
- 誤選択率が低い

### PIE-6 Extensibility

目的:
Composition Editor 以外にも流用できる土台にする。

対象:

- layer view
- 3D view
- asset browser quick actions

内容:

- menu model を editor 固有ロジックから分離
- command binding を差し替え可能にする

Done:

- pie menu を別 widget にも持っていきやすい

## Recommended First Menu

最初の 8 項目案:

- Select Tool
- Hand Tool
- Pen Tool
- Fit View
- 100%
- Reset View
- Grid Toggle
- Safe Area Toggle

`Guides` は 2nd ring か別レイアウト候補でもよい。

## UX Rules

- 中央から一定距離未満では未選択
- release で確定
- `Esc` でキャンセル
- focus loss で自動クローズ
- 既存選択を壊さない
- 開いている間は viewport cursor を pie menu 主体へ切り替える

## Risks

- right click 系操作と衝突すると selection UX が壊れる
- viewport event を pie menu が奪いすぎると pan/gizmo が不安定になる
- 項目数を増やしすぎると pie menu の利点が消える
- toggle 状態と toolbar 状態が別管理になると破綻する

## Recommended Order

1. `PIE-1 Command Inventory`
2. `PIE-2 Input Contract`
3. `PIE-3 Overlay Rendering`
4. `PIE-4 Command Execution Bridge`
5. `PIE-5 Operation Feel`
6. `PIE-6 Extensibility`

## Exit Criteria

- `Composition Viewer` 上で pie menu が開く
- 主要操作を mouse travel 少なく実行できる
- 既存の toolbar / shortcut / tool state と矛盾しない
- 他の AI が見ても、入力責務と表示責務の境界が明確
