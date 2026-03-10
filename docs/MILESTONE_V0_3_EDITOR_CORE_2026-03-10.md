# Artifact v0.3 Editor Core (2026-03-10)

v0.3 は「プロトタイプの見た目」から「編集コアの成立」へ進める段階。
v0.2 の安定化が前提。

## Goal

次の 6 項目を満たしたら v0.3 を達成とする。

1. レイヤー編集操作の一貫した Undo/Redo
2. タイムライン左ペインの展開ツリー仕様を実用化
3. プロパティ編集とレンダー結果の反映遅延を短縮
4. コンポジション/レイヤー削除時の依存確認を統一
5. RenderQueue の実行履歴と再実行導線を追加
6. レイヤー表示（Layer View）とコンポ表示（Composition View）の同期完成

## Definition Of Done

- 主要編集操作（作成/削除/移動/名前変更）が Undo/Redo できる
- タイムライン上の選択状態が Inspector/Layer View/Project View と不一致にならない
- 削除系操作は依存関係を警告し、無言破壊しない
- RenderQueue を「追加して終わり」ではなく「管理して回せる」状態にする

## Work Breakdown

### 1. Undo/Redo コア統合

- 対象:
  - `src/Undo/UndoManager.cppm`
  - `src/Service/ArtifactProjectService.cpp`
  - `src/Widgets/*`
- 完了条件:
  - レイヤー追加/削除/移動/名前変更がコマンド化
  - メニュー、ショートカット、UIボタンから同じ経路で実行

### 2. タイムライン左ペインのツリー運用

- 対象:
  - `src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
  - `src/Widgets/ArtifactTimelineWidget.cpp`
- 完了条件:
  - Null/Group の展開収縮が安定
  - ヘッダ定義と行描画の列整合が固定
  - 横幅不足時に崩れない（描画クリップ）

### 3. プロパティ反映パス最適化

- 対象:
  - `src/Widgets/ArtifactInspectorWidget.cppm`
  - `src/Widgets/ArtifactPropertyWidget.cppm`
  - `src/Service/ArtifactPlaybackService.cppm`
- 完了条件:
  - 値変更から表示更新までの遅延が体感で即時
  - レイヤー選択切替時のクラッシュ/破棄バグがない
- 進捗 (2026-03-11):
  - Inspector の Effects Pipeline で操作性改善
  - ラックの Add/Remove を選択状態に応じて有効化
  - エフェクト項目ダブルクリックで Enable/Disable 切替
  - エフェクト削除に確認ダイアログを追加

### 4. 依存削除確認の統一

- 対象:
  - `src/Widgets/Menu/ArtifactCompositionMenu.cppm`
  - `src/Widgets/ArtifactProjectManagerWidget.cppm`
  - `src/Render/ArtifactRenderQueueService.cppm`
- 完了条件:
  - Composition削除時に RenderQueue/参照レイヤーを確認
  - どの導線から削除しても同一メッセージ・同一結果
- 進捗 (2026-03-11):
  - `ArtifactProjectService::compositionRemovalConfirmationMessage(...)` を追加
  - CompositionMenu / ProjectManagerWidget の削除確認文言を Service 経由へ統一

### 5. RenderQueue 管理機能

- 対象:
  - `src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
  - `src/Render/ArtifactRenderQueueService.cppm`
- 完了条件:
  - 実行履歴（成功/失敗/出力先）を表示
  - 失敗ジョブ再実行、完了ジョブ再実行が可能
  - ジョブ設定プリセット（最低1つ）を保存/再利用
- 進捗 (2026-03-11):
  - `Completed/Failed/Canceled` を `Pending` に戻す `resetJobForRerun()` を Service に追加
  - `Completed/Failed` 一括再実行リセット API を追加
  - RenderQueue UI に `Rerun Selected` / `Rerun Done/Failed` ボタンを追加
  - RenderQueue UI にジョブ設定プリセット保存/読込/削除を追加（FastSettingsStore永続化）
  - プリセット保存時の同名上書き確認ダイアログを追加

### 6. View 同期完成

- 対象:
  - `src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
  - `src/Widgets/Render/ArtifactCompositionEditor.cppm`
  - `src/Service/ArtifactProjectService.cpp`
- 完了条件:
  - レイヤー選択で Layer View が確実に追従
  - コンポ切替で Composition View が確実に追従
  - どちらも Service API 経由で更新される

## Execution Order

1. `1 -> 6`（選択同期と編集コマンドの核を先に固定）
2. `2 -> 3`（タイムライン体験を詰める）
3. `4 -> 5`（破壊操作と出力管理を実務レベルへ）

## Gate to v0.4

v0.3 完了時点で、次の2条件を満たして v0.4 へ進む。

- 1時間連続操作でクラッシュしない
- 再起動後にレイアウト・最近の作業状態が復元できる
