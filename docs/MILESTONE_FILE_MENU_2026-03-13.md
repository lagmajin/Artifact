# File Menu Milestone

日付: 2026-03-13

## Goal

`ArtifactFileMenu` を単なる action の寄せ集めではなく、プロジェクトライフサイクルの正規 UI 入口として整理する。

最低限ここを満たす。

- project create / open / save / close / restart / quit の責務を固定する
- composition 作成と asset import を project state に応じて正しく制御する
- recent projects / unsaved changes / destructive confirmation を File Menu 側で一貫して扱う
- `ArtifactProjectManager` と `ArtifactProjectService` の橋渡しを明確にする

## Scope

- `ArtifactFileMenu`
- `ArtifactMenuBar`
- `ArtifactProjectManager`
- `ArtifactProjectService`
- project path / recent projects / save prompts / restart flow

## Non-Goals

- メニューバー全体の再設計
- `Edit` / `View` / `Layer` 系 menu の整理
- autosave 実装そのもの
- project serialization format の刷新

## Milestones

### M-FILE-1 Action Baseline Cleanup

目的:
今の `ArtifactFileMenu` の action 構成と有効/無効判定を安定化する。

対象:

- create/open/save/save as/close
- new composition
- import assets
- reveal project folder
- restart / quit

実装方針:

- action の責務を `ArtifactFileMenu::Impl` に集約する
- `rebuildMenu()` で project state に応じた enabled 状態を一本化する
- project 未オープン時に使えない action を明示的に無効化する
- shortcut と menu label を固定し、他 menu と競合しないようにする

Done:

- File Menu を開くたびに action 状態が current project に追従する
- project が無い状態で save/import/reveal が押せない
- action 名と shortcut が安定する

### M-FILE-2 Project Lifecycle Commands

目的:
project create / open / close / save の遷移を File Menu 経由で破綻なくする。

対象:

- `ArtifactProjectManager::createProject`
- `ArtifactProjectManager::loadFromFile`
- `ArtifactProjectManager::saveToFile`
- `ArtifactProjectManager::closeCurrentProject`

実装方針:

- create/open/save/save as の UI と manager 呼び出しを明確に分ける
- project close は destructive confirmation を通す
- save は current project path が無い場合に save as へ自然に落とす
- open/create 前に未保存変更確認を差し込める構造へ寄せる

Done:

- create/open/save/save as/close の導線が File Menu だけで完結する
- close 前に確認が出る
- current project path 無しの save が壊れない

### M-FILE-3 Recent Projects And Reopen

目的:
`最近使ったプロジェクト` を placeholder ではなく実用機能にする。

対象:

- recent projects menu
- settings persistence
- reopen action

実装方針:

- recent projects 一覧を保存する
- open/create/save as 成功時に recent projects を更新する
- missing path は menu rebuild 時に除外または disabled 表示する
- recent project 選択で `loadFromFile()` を呼べるようにする

Done:

- recent projects に実ファイル一覧が出る
- クリックで reopen できる
- 壊れた path が menu を汚さない

### M-FILE-4 Unsaved Changes And Safety Guards

目的:
destructive command と application exit を未保存変更前提で安全にする。

対象:

- close project
- open project
- create project
- restart
- quit

実装方針:

- unsaved changes 判定 API を File Menu から参照できるようにする
- close/open/create/restart/quit で共通 confirmation helper を使う
- 文言は「何が失われるか」が分かる形に固定する
- 将来 autosave を入れても差し替えやすい確認導線にする

Done:

- project を閉じる/切り替える/終了する際に確認が一貫する
- restart と quit だけ特別挙動にならない
- destructive command ごとの確認重複が減る

### M-FILE-5 Composition And Import Bridge

目的:
File Menu からの composition 作成と asset import を project workflow の正規導線として扱う。

対象:

- `ArtifactProjectService::createComposition`
- `ArtifactProjectService::importAssetsFromPaths`
- import file filter

実装方針:

- new composition は current project 必須の action として扱う
- import assets は supported filter と拡張子判定を 1 箇所に寄せる
- import 成功後に project view 側の更新が自然に反映される前提を固定する
- `FileMenu` が import policy を持ちすぎないよう service 呼び出しに責務を戻す

Done:

- project オープン中のみ composition/import が有効
- import filter と supported extension 判定がズレない
- File Menu から imported asset が project workflow に乗る

### M-FILE-6 Restart, Reveal, And Polish

目的:
補助 action を実運用レベルに整えて File Menu を閉じる。

対象:

- reveal project folder
- restart
- quit
- menu rebuild polish

実装方針:

- reveal project folder は current project path 基準で folder を正しく開く
- restart は current process path / args を安全に引き継ぐ
- restart launch failure を user-visible に扱えるようにする
- separator / submenu / disabled placeholder の見え方を整理する

Done:

- reveal/restart/quit が project state と矛盾しない
- restart failure が無言失敗しない
- File Menu の見た目と責務が安定する

## Recommended Order

1. `M-FILE-1 Action Baseline Cleanup`
2. `M-FILE-2 Project Lifecycle Commands`
3. `M-FILE-3 Recent Projects And Reopen`
4. `M-FILE-4 Unsaved Changes And Safety Guards`
5. `M-FILE-5 Composition And Import Bridge`
6. `M-FILE-6 Restart, Reveal, And Polish`

## Current Status

2026-03-13 時点では、以下はすでに入っている。

- create/open/save/save as/close/new composition/import/reveal/restart/quit の action 定義
- project 有無に応じた一部 enabled 切り替え
- close/restart/quit の destructive confirmation
- composition 作成と asset import の service bridge

未対応または弱い点は以下。

- recent projects が `(近日対応)` の placeholder
- open/create 前の未保存変更確認が未整理
- save failure / restart failure の UI フィードバックが弱い
- project dirty state を File Menu がまだ見ていない

## Validation Checklist

- project 未オープン状態で save/import/reveal/new composition が無効
- create project が成功すると project state と menu state が更新される
- open project が成功すると recent projects に反映される
- save/save as が current path 有無に応じて正しく分岐する
- close/restart/quit で destructive confirmation が一貫する
- recent projects の reopen が成功する
- reveal project folder が project file の親 folder を開く
