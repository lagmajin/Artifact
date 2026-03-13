# Composition Menu Milestone

Date: 2026-03-13

## Goal

`ArtifactCompositionMenu` を「コンポ作成だけの補助メニュー」から、現在コンポジションの作成、複製、改名、削除、設定変更を一貫して扱う実運用メニューへ整理する。

この段階で重視するのは次の 3 点。

- `create / preset / duplicate / rename / delete` を `ArtifactProjectService` と整合した経路に揃える
- current composition の有無に応じた menu state を安定させる
- 開発用ダミー action や placeholder dialog を本番メニュー責務から切り離す

## Scope

- `ArtifactCompositionMenu`
- `ArtifactProjectService`
- composition creation dialog bridge
- rename / delete safety
- composition settings / background color
- current composition dependent enable state

## Non-Goals

- composition model 自体の再設計
- render queue 機能の新規追加
- timeline や project view の全面改修
- software render milestone 自体の実装
- composition inspector の全面置換

## Milestones

### M-COMP-1 Action Baseline And Menu Ownership

目的:
`Composition Menu` に置くべき action と開発用 action を切り分ける。

対象:

- create
- preset create
- duplicate
- rename
- delete
- settings
- background color
- milestone dummy action

実装方針:

- 本番 menu に残す action と、開発補助 action を分離する
- `milestoneDummyAction` は最終的に menu から外すか debug 条件付きにする
- `Composition Menu` は composition 管理に責務を限定する

Done:

- production menu に不要な action が整理される
- action 一覧が current composition workflow に集中する

### M-COMP-2 Create Composition Dialog Bridge

目的:
`新規コンポジション` を dialog と preset 作成で同じ service 経路に揃える。

対象:

- `CreateCompositionDialog`
- preset actions
- `ArtifactProjectService::createComposition`

実装方針:

- `showCreate()` で dialog の入力値を `ArtifactCompositionInitParams` に反映して `createComposition()` へ渡す
- preset action も同じ作成経路を通す
- project service 不在時のエラー表示を統一する
- create 後に current composition が適切に切り替わる前提を検証する

Done:

- dialog 作成が placeholder で終わらない
- preset と dialog 作成の挙動差が最小化される
- service 不在時の失敗表示が揃う

### M-COMP-3 Duplicate / Rename / Delete Safety

目的:
現在コンポジションに対する破壊系操作を安全に扱う。

対象:

- duplicate
- rename
- delete
- confirmation / failure messaging

実装方針:

- current composition がない場合は各 action を無効化する
- rename では現在名を初期値に使う
- delete は render queue cleanup を含む既存 service を継続利用する
- duplicate / rename / delete の成功後に current composition state を menu 側で再同期する

Done:

- current composition なしで誤って rename/delete できない
- rename の UX が placeholder 的でなくなる
- delete 後の current composition 状態が壊れない

### M-COMP-4 Composition Settings And Background

目的:
`レンダー出力設定` と `背景色` を current composition 編集機能として実装する。

対象:

- settings dialog
- background color picker
- current composition property update

実装方針:

- `showSettings()` の placeholder message box を composition settings dialog へ置き換える
- background color は current composition がある場合のみ有効にする
- color change 後の dirty state / redraw / inspector sync を必要に応じて発火する
- settings と color で project dirty 状態が更新されるようにする

Done:

- `レンダー出力設定` が実ダイアログにつながる
- `背景色` 変更が current composition に永続反映される
- 設定変更後に UI が取り残されない

### M-COMP-5 State Rebuild And Current Composition Sync

目的:
`rebuildMenu()` を current composition と project service 状態に正しく追従させる。

対象:

- create action
- preset submenu
- duplicate / rename / delete
- settings / color

実装方針:

- `service != nullptr` と `currentComposition` の条件を helper にまとめる
- current composition 切替後に menu state が古いまま残らないようにする
- project close / project open / composition delete の直後にも状態が崩れないようにする

Done:

- menu を開くたびに current composition 状態へ追従する
- project close 後に stale action が残らない
- create だけは service があれば有効、その他は current composition 依存になる

### M-COMP-6 Cleanup Of Development-Only Workflow

目的:
software pipeline 用の暫定 action を本番メニュー責務から外す。

対象:

- `runMilestoneDummyPipeline()`
- `ArtifactSoftwareCompositionTestWidget`
- milestone bootstrap flow

実装方針:

- 開発用 bootstrap は別メニュー、debug action、または test window 側へ移す
- `Composition Menu` には production workflow だけを残す
- どうしても残すなら compile flag か debug build 条件付きにする

Done:

- `Composition Menu` が本番用途に集中する
- software pipeline milestone 用の補助導線が分離される
- 将来のユーザー向け UI にダミー action が混ざらない

## Recommended Order

1. `M-COMP-1 Action Baseline And Menu Ownership`
2. `M-COMP-2 Create Composition Dialog Bridge`
3. `M-COMP-3 Duplicate / Rename / Delete Safety`
4. `M-COMP-4 Composition Settings And Background`
5. `M-COMP-5 State Rebuild And Current Composition Sync`
6. `M-COMP-6 Cleanup Of Development-Only Workflow`

## Current Status

2026-03-13 時点で `ArtifactCompositionMenu` は半分できている。

- preset 作成、duplicate、rename、delete、background color は `ArtifactProjectService` へ接続済み
- `rebuildMenu()` も current composition 有無に応じた大まかな enable/disable は入っている
- 一方で `showCreate()` は dialog を開くだけで、accept 後の作成処理が未接続
- `showSettings()` は placeholder の message box
- `milestoneDummyAction` は開発補助として残っており、本番メニュー責務からは外したい

したがって最優先は `showCreate()` と `showSettings()` の実接続、その後にダミー action 分離が妥当。

## Validation Checklist

- project service 不在時は create/preset が無効または明示失敗する
- create dialog から新規コンポが実際に作成される
- preset create と dialog create が同じ current composition 更新結果になる
- duplicate / rename / delete が current composition に対して正しく動く
- delete 後に current composition が壊れず menu state が更新される
- settings dialog で変更した値が current composition に反映される
- background color 変更後に preview / inspector / dirty state が必要に応じて更新される
- 本番メニューから development-only action が外れるか debug 限定になる
