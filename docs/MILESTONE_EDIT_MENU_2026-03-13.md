# Edit Menu Milestone

Date: 2026-03-13

## Goal

`ArtifactEditMenu` を単なる action の置き場ではなく、選択状態、タイムライン状態、プロジェクト状態に応じて正しく振る舞う編集コマンドの入口として整理する。

この段階で目指すのは次の 3 点。

- `Undo` / `Redo` / `Copy` / `Paste` / `Delete` などの基本編集を実コマンドへ接続する
- layer / composition / project view のどこを対象にしているかを明確にする
- `Edit Menu` の enabled state を「プロジェクトがあるか」だけでなく、実際の selection / clipboard / undo stack に追従させる

## Scope

- `ArtifactEditMenu`
- selection bridge
- undo / redo bridge
- clipboard / duplicate / delete bridge
- timeline-oriented edit actions
- find / preferences action routing

## Non-Goals

- Undo system 自体の新規設計
- timeline widget 全体の再設計
- Property Editor の全面改修
- ショートカット体系の全面見直し
- View / Layer / File menu の整理

## Milestones

### M-EDIT-1 Action Inventory And Ownership

目的:
`Edit Menu` の action を棚卸しし、どの UI / service / command が責務を持つかを固定する。

対象:

- undo / redo
- copy / cut / paste / delete
- duplicate
- split / trim in / trim out
- select all
- find
- preferences

実装方針:

- 各 action の実行先を `ProjectService` / timeline selection / global application command に切り分ける
- `EditMenu` 自体はコマンドディスパッチに徹し、重い処理を持たせない
- 実装未接続 action は placeholder のままにせず、未実装理由を明確にした helper を介して止める
- `aboutToShow` 時の `rebuildMenu()` で状態を更新する前提を維持する

Done:

- すべての action に責務の持ち主が決まっている
- debug placeholder だけの action が一覧化されている
- `Edit Menu` が UI 依存ロジックの溜まり場にならない

### M-EDIT-2 Undo / Redo Bridge

目的:
`Undo` / `Redo` を実際の undo stack に接続し、menu label と enabled state を連動させる。

対象:

- undo stack provider
- menu label update
- enabled state

実装方針:

- `ArtifactEditMenu` は active document / project の undo stack を参照する
- `元に戻す` / `やり直し` の文言は可能なら現在の command 名を反映する
- undo stack 不在時は disabled にする
- project が存在しても undo 対象がなければ disabled にする

Done:

- `Undo` / `Redo` が実コマンドとして動作する
- stack 状態に応じて enabled が変わる
- project の有無だけで誤って有効化されない

### M-EDIT-3 Clipboard And Selection Commands

目的:
`Copy` / `Cut` / `Paste` / `Delete` / `Duplicate` を current selection に接続する。

対象:

- timeline layer selection
- project view selection
- internal clipboard
- duplicate / delete command path

実装方針:

- selection context を一段抽象化し、現在どの widget が編集対象かを判定する
- layer selection が主経路なら timeline command を優先する
- project view 選択中の delete など、同名でも意味が異なる操作は context 別に振り分ける
- `Paste` は clipboard 空時に disabled にする
- `Duplicate` は copy/paste ではなく専用 command として扱う

Done:

- selection がない状態で copy/cut/delete/duplicate が無効化される
- `Paste` が clipboard 状態に追従する
- timeline / project view で誤った対象を編集しない

### M-EDIT-4 Timeline Edit Commands

目的:
`Split` / `Trim In` / `Trim Out` / `Select All` を timeline 編集コマンドとして接続する。

対象:

- current time
- selected layers
- active composition
- timeline controller

実装方針:

- split / trim 系は active composition と selected layer を前提にする
- playhead が必要な操作は current time 不明時に無効化する
- `Select All` は active timeline / project view どちらを対象にするか仕様を固定する
- timeline command は menu action と shortcut の両方から同じ経路を通す

Done:

- split / trim in / trim out が playhead と selection に応じて動く
- `Select All` の対象が一貫している
- timeline 由来の操作が menu からも同じ結果になる

### M-EDIT-5 Find And Preferences Routing

目的:
`Find` と `Preferences` をグローバル UI 機能として正しい画面へ接続する。

対象:

- find dialog / search field
- preferences dialog
- main window command routing

実装方針:

- `Find` は active pane に応じて project search / timeline search / generic search を切り替えるか、最低限 1 系統に固定する
- `Preferences` は main window か application service 側の dialog 起動へ集約する
- 現状未実装なら no-op にせず、ユーザー向けの明確な未実装表示にする

Done:

- `Find` が debug log ではなく UI へつながる
- `Preferences` が実ダイアログ起動へつながる
- menu action と shortcut が同じ経路を使う

### M-EDIT-6 Context-Aware Rebuild Polish

目的:
`rebuildMenu()` を project 有無だけの粗い判定から、編集コンテキスト反映型へ改善する。

対象:

- active widget / focused pane
- current project
- current composition
- current selection
- clipboard
- undo stack

実装方針:

- `ArtifactProjectManager::isProjectCreated()` だけに依存しない
- focused widget か central selection service を参照して編集対象を決める
- 有効条件を helper に寄せて、action ごとの if を散らさない
- 後続の `Layer Menu` や shortcut dispatcher でも再利用できる判定にする

Done:

- `Edit Menu` を開くたびに状態が実編集対象へ追従する
- プロジェクトがあるだけで全部有効になる状態が解消される
- 判定ロジックが他メニューへ再利用可能な形になる

## Recommended Order

1. `M-EDIT-1 Action Inventory And Ownership`
2. `M-EDIT-2 Undo / Redo Bridge`
3. `M-EDIT-3 Clipboard And Selection Commands`
4. `M-EDIT-4 Timeline Edit Commands`
5. `M-EDIT-5 Find And Preferences Routing`
6. `M-EDIT-6 Context-Aware Rebuild Polish`

## Current Status

2026-03-13 時点では、`ArtifactEditMenu` は menu 構造と shortcut はあるが、実体はほぼ placeholder。

- `undo` / `redo` / `copy` / `cut` / `paste` / `delete` / `duplicate` / `split` / `trim` / `select all` / `find` / `preferences` は action 定義済み
- `rebuildMenu()` は `ArtifactProjectManager::isProjectCreated()` だけで一括 enable/disable している
- action handler は現状ほぼ `qDebug()` のみ
- selection context や undo stack との接続はまだない

したがって優先度が高いのは、まず `Undo/Redo` と `Clipboard/Delete` を本物のコマンドへつなぎ、その後に timeline 固有操作へ進む流れ。

## Validation Checklist

- project なしでは編集 action が無効化される
- project ありでも selection なしなら copy/cut/delete/duplicate が無効化される
- clipboard 空なら paste が無効化される
- undo stack が空なら undo/redo が無効化される
- split / trim in / trim out が current time と selected layer に追従する
- `Find` が debug log ではなく検索 UI を開く
- `Preferences` が実ダイアログを開く
- menu からの操作と shortcut からの操作が同じ結果になる
