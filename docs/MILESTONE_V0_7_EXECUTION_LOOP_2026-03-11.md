# Artifact v0.7 Execution Loop (2026-03-11)

v0.7 は「機能がある」状態から「毎日回せる制作ループ」へ進める段階。

## Goal

次の 7 項目を満たしたら v0.7 を達成とする。

1. Composition -> Layer -> Effect -> Queue -> Render の導線を 1 ループ化
2. Effect/Property 編集の対象同期を完全固定（選択ズレなし）
3. RenderQueue の実行中可観測性を運用レベルへ（進捗/失敗理由/再試行）
4. D3D12 / Software バックエンド切替を安全化
5. レイアウト復元と作業再開の再現性を改善
6. 破壊操作（削除/上書き）の確認ルールを全UIで統一
7. 主要フローのスモーク回帰を最小10本へ拡張

## Definition Of Done

- 新規プロジェクトからダミー画像出力まで 5 分以内で到達可能
- 対象選択のズレ（Timeline / Inspector / Property / LayerView）が再現しない
- RenderQueue 連続操作 15 分でクラッシュしない
- 失敗時に「どこで失敗したか」が UI に必ず残る

## Work Breakdown

### 1. Execution Path Presets

- 対象:
  - `src/Widgets/Menu/*`
  - `src/Service/ArtifactProjectService.cpp`
  - `src/Render/ArtifactRenderQueueService.cppm`
- 完了条件:
  - 主要フローをワンクリック補助（作成テンプレート/キュー追加補助）

### 2. Selection & Property Sync Finalize

- 対象:
  - `src/Widgets/ArtifactInspectorWidget.cppm`
  - `src/Widgets/ArtifactPropertyWidget.cppm`
  - `src/Widgets/ArtifactTimelineWidget.cpp`
- 完了条件:
  - 選択中 effect のみを Property でフォーカス表示可能
  - レイヤー切替時に古い effect 参照が残らない

### 3. RenderQueue Observability

- 対象:
  - `src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
  - `src/Render/ArtifactRenderQueueService.cppm`
- 完了条件:
  - ステータス履歴に実行ソース/再試行理由/失敗理由を残す
  - 進捗通知の閾値/頻度を設定可能にする
- 進捗 (2026-03-11):
  - 履歴/ステータスにイベントソース接頭辞を追加
    - `[UI] ...`
    - `[Service] ...`
  - 進捗通知粒度を UI 設定化（10/25/50/100%）
  - 履歴のエクスポート導線を追加（`.log` / `.txt`）

### 4. Backend Resilience

- 対象:
  - `src/Widgets/Render/ArtifactCompositionEditor.cppm`
  - `src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
  - `src/Render/Software/*`
- 完了条件:
  - D3D12 不可時の自動フォールバックと明示通知

### 5. Session Restore Reliability

- 対象:
  - `src/AppMain.cppm`
  - `ArtifactCore/src/Core/FastSettingsStore.cppm`
- 完了条件:
  - 前回作業レイアウト/選択状態の復元精度を向上

### 6. Safe Operations Consistency

- 対象:
  - `src/Widgets/ArtifactProjectManagerWidget.cppm`
  - `src/Widgets/Menu/*`
- 完了条件:
  - 削除/上書きの確認文言と判定条件を Service 経由で統一

### 7. Regression Suite Expansion

- 対象:
  - `src/Test/*`
- 完了条件:
  - 編集/出力ループを横断するスモークテスト 10 本

## Suggested Order

1. `2 -> 3`（対象同期と可観測性を先に固定）
2. `1 -> 6`（運用導線と安全性を整理）
3. `4 -> 5 -> 7`（安定運用と回帰防止を完成）
