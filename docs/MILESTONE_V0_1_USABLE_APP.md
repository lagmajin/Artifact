# Artifact v0.1 Usable Milestone

このドキュメントは「使える状態」の定義を固定し、実装順を明確にするための最短マイルストーンです。

## Goal

次の 7 項目を満たしたら v0.1 を「使える」と判定する。

1. 新規プロジェクト作成
2. コンポジション作成
3. 平面レイヤー追加
4. タイムラインで移動/長さ変更
5. プレビュー反映
6. レンダーキュー登録
7. 静止画 1 枚の書き出し（ダミー実装可）

## Definition Of Done

- すべての項目で UI 操作時にクラッシュしない
- 主要操作が Menu / Context Menu / Shortcut のいずれかで到達可能
- エラー時は無言失敗せず、ログまたはダイアログで通知される

## Work Breakdown

### 1. 新規プロジェクト作成

- 対象:
  - `src/Project/ArtifactProjectManager.cppm`
  - `src/Widgets/Menu/ArtifactFileMenu.cppm`
- 完了条件:
  - File > 新規プロジェクトで作成できる
  - プロジェクトツリーに root が表示される
  - 作成後の `projectCreated/projectChanged` が UI まで伝搬する

### 2. コンポジション作成

- 対象:
  - `src/Service/ArtifactProjectService.cpp`
  - `src/Widgets/Menu/ArtifactCompositionMenu.cppm`
  - `src/Widgets/ArtifactProjectManagerWidget.cppm`
- 完了条件:
  - 作成直後にプロジェクトビューへ反映
  - 初回作成が無視されない

### 3. 平面レイヤー追加

- 対象:
  - `src/Widgets/Menu/ArtifactLayerMenu.cppm`
  - `src/Widgets/Dialog/CreatePlaneLayerDialog.cppm`
  - `src/Project/ArtifactProjectManager.cppm`
- 完了条件:
  - Layer > 平面 でダイアログ表示
  - OK でレイヤー追加され、ツリー/タイムライン双方に反映

### 4. タイムラインで移動/長さ変更

- 対象:
  - `src/Widgets/Timeline/ArtifactTimelineObjects.cpp`
  - `src/Widgets/ArtifactTimelineScene.cppm`
  - `src/Widgets/ArtifactTimelineWidget.cpp`
- 完了条件:
  - 左右ハンドルの動作がフレーム単位で一致
  - 左右ハンドルが相互に追い越さない
  - ハンドル操作で Y 方向にずれない

### 5. プレビュー反映

- 対象:
  - `src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
  - `src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
  - `src/Service/ArtifactPlaybackService.cppm`
- 完了条件:
  - タイムライン変更後に再描画される
  - 選択レイヤー時に Layer View が追従する

### 6. レンダーキュー登録

- 対象:
  - `src/Render/ArtifactRenderQueueService.cppm`
  - `src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
- 完了条件:
  - 現在コンポジション名でキュー追加される
  - コンポジション削除時にキュー登録有無を確認できる

### 7. 静止画 1 枚の書き出し（ダミー可）

- 対象:
  - `src/Render/ArtifactRenderQueueService.cppm`
  - `src/Export/*` または `src/Render/*`
- 完了条件:
  - キュー実行で 1 フレーム分の PNG を保存
  - 失敗時は理由を UI に表示

## Execution Order

1. 1 -> 2 -> 3 を先に固める（データ整合）
2. 4 -> 5 で体験の中核を安定化
3. 6 -> 7 で「出力できる」を成立

## Current Status Snapshot

- 6 は基盤実装あり（RenderQueue へ composition 名で登録する経路あり）
- 4 と 5 は UI/入力処理の不整合が残るため優先度高
- 7 はダミーでも到達しておくと完了感が出る

