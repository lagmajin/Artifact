# v0.1 Diagnostic (2026-03-10)

対象: `docs/MILESTONE_V0_1_USABLE_APP.md` の 1〜7

## Summary

- 達成: 2 / 7
- 部分達成: 5 / 7
- 未達: 0 / 7

## Status By Item

1. 新規プロジェクト作成: 達成
- 根拠:
  - `src/Widgets/Menu/ArtifactFileMenu.cppm` の `handleCreateProject()`
  - `src/Project/ArtifactProjectManager.cppm` の `createProject(...)`
- 判定:
  - UI から作成導線あり
  - シグナル転送あり (`projectCreated/projectChanged`)

2. コンポジション作成: 部分達成
- 根拠:
  - `src/Widgets/Menu/ArtifactFileMenu.cppm` `handleNewComposition()`
  - `src/Widgets/Menu/ArtifactCompositionMenu.cppm` `createComposition(...)`
  - `src/Service/ArtifactProjectService.cpp` `createComposition(...)`
- 未達ポイント:
  - 現在コンポの確定/選択状態の管理が弱く、後続処理の前提になりにくい
 - 2026-03-10 更新:
   - `ArtifactProjectService` で `currentCompositionId_` を保持するよう修正
   - `changeCurrentComposition()` を実装
   - `createComposition()` 成功時に新規コンポへ自動切替
   - `currentCompositionChanged` シグナルを追加し、UI側（Composition Viewer）との同期経路を明確化

3. 平面レイヤー追加: 部分達成
- 根拠:
  - `src/Widgets/Menu/ArtifactLayerMenu.cppm` `handleCreateSolid()`
  - `src/Widgets/Dialog/CreatePlaneLayerDialog.cppm`
- 本ターンの改善:
  - 現在コンポ未確定時は `ensureCurrentComposition()` で自動確保を試行
  - 失敗時は警告表示
  - `ArtifactProjectManager::addLayerToCurrentComposition()` で `layerCreated/projectChanged` 通知を復帰
- 未達ポイント:
  - 「どのコンポに追加されたか」の UI 反映保証を追加確認必要

4. タイムライン移動/長さ変更: 部分達成
- 根拠:
  - `src/Widgets/Timeline/ArtifactTimelineObjects.cpp`
  - `src/Widgets/ArtifactTimelineWidget.cpp`
- 未達ポイント:
  - 左右ハンドル同期と座標制約に継続バグ報告あり（左ハンドル追従、Y 方向ぶれ等）
 - 2026-03-10 更新:
   - `ClipItem` にハンドルリサイズ状態を追加し、ドラッグ差分ベースで左/右リサイズを計算
   - `setStartDuration/setDuration` の幾何更新順序を修正（`prepareGeometryChange`）
   - リサイズ中の Y 位置固定を明示化
   - 右ペイン上部シーク優先がハンドル操作を奪う問題を修正（clip/handle 優先）
   - 左右ペインの行開始位置を合わせるため、左側に右サブヘッダ相当のスペーサーを追加
   - 中央スプリッタで片側の極端な潰れを防止（`setChildrenCollapsible(false)` と最小幅設定）

5. プレビュー反映: 部分達成
- 根拠:
  - `src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
  - `src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
- 2026-03-10 更新:
  - `src/Widgets/ArtifactTimelineWidget.cpp` で `seekBar::frameChanged` から `ArtifactActiveContextService::seekToFrame(...)` へ接続
  - `setComposition(...)` 時に `ArtifactProjectService::changeCurrentComposition(...)` を呼び、`ActiveContext` 側の active composition も同期
  - コンポ切替直後の初期フレーム未反映を避けるため `seekToFrame(0)` を明示
  - `ArtifactProjectService` に `currentCompositionChanged` を追加し、`AppMain` で `ArtifactCompositionEditor` を現在コンポへ同期
  - `ArtifactRenderLayerWidgetv2::setTargetLayer(...)` の空実装を解消し、選択レイヤー変更が描画（色）に反映されるよう修正
- 未達ポイント:
  - 実描画経路がモック寄りで、レイヤー編集結果の完全反映が未統合

6. レンダーキュー登録: 部分達成
- 根拠:
  - `src/Render/ArtifactRenderQueueService.cppm`
  - `addRenderQueueForComposition(compositionId, compositionName)` 実装あり
- 本ターンの改善:
  - `currentComposition().lock()` の型不整合を修正（`shared_ptr` 化に追従）
  - `CompositionMenu` の削除導線で「キュー登録件数確認 -> 該当キュー削除 -> コンポ削除」を実装
  - `ArtifactProjectManagerWidget` の右クリック `Delete`（Composition対象）にも同じ確認/連動削除を適用
  - `ArtifactRenderQueueService` に `jobStatusAt(int)` と `setJobStatusChangedCallback(...)` を追加し、UI の状態表示をサービス状態に同期
  - `ArtifactRenderQueueManagerWidget` の状態更新を `"Rendering"` 固定からサービス参照へ修正
  - `RenderQueueManagerWidget` 破棄時に Service コールバックを解除し、ダングリング呼び出しを防止
  - `Start/Pause/Cancel` 操作時の UI ローカル先行更新を廃止し、Serviceイベントを唯一の状態ソースへ統一
  - `cancelAllJobs()` が Pending を取り残す問題を修正
- 未達ポイント:
  - 実ジョブ進行/完了の状態遷移が最小実装

7. 静止画1枚書き出し: 達成（ダミー）
- 根拠:
  - `src/Render/ArtifactRenderQueueService.cppm` の `startAllJobs()`
  - `renderSingleFrameDummy(...)` で PNG 1 枚保存
  - 進捗/完了ステータス更新を `setJobProgress/setJobCompleted` で通知
  - `ArtifactRenderQueueService` に `jobOutputPathAt/jobErrorMessageAt` を追加し、`RenderQueueManagerWidget` のステータスラベルで保存先/失敗理由を表示

## Blockers (High)

- タイムラインの入力系（ハンドル・シークバー）境界条件が不安定
- 現在コンポジションの選択/反映の一貫性は改善中（`currentCompositionChanged` 導入済み）
- レンダーキューは登録中心で、出力完了までの縦通しが未成立

## Next Sprint (Shortest Path)

1. タイムライン入力安定化
- 左右ハンドル座標拘束とフレームスナップを再整理

2. 現在コンポジション確定ロジックを一本化
- Service 層で「active composition を必ず返せる」経路を固める

3. ダミー静止画出力を実装
- レンダーキュー先頭ジョブから PNG 1 枚保存まで通す

## Parallel R&D (Software Renderer)

- `ArtifactSoftwareRenderTestWidget` に以下を追加:
  - 画像2枚の重ね合わせ合成（BG/Overlay）
  - ブレンドモード切替（Normal/Add/Multiply/Screen）
  - バックエンド切替（QImage/QPainter <-> OpenCV）
  - OpenCVエフェクト切替（GaussianBlur / EdgeOverlay）
  - 合成結果のPNG保存
- 目的:
  - エフェクト検証を高速化し、将来の本線レンダーパイプライン実装へ仕様フィードバックする
