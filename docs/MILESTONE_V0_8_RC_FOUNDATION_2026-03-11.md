# Artifact v0.8 RC Foundation (2026-03-11)

v0.8 は「機能開発中心」から「リリース候補品質」へ移行する段階。

## Goal

次の 8 項目を満たしたら v0.8 を達成とする。

1. 編集から出力までの主要ループが連続稼働で破綻しない
2. クラッシュ/異常終了時の復旧導線を実装
3. 破壊操作（削除/上書き）の確認を全導線で統一
4. RenderQueue の失敗可観測性を運用レベルへ
5. D3D12 不可環境で Software 経路へ確実にフォールバック
6. UI設定/履歴/レイアウト復元の信頼性を上げる
7. 自動スモークテストを 10 本以上に拡張
8. 配布向けビルド手順（preset/package）を固定

## Definition Of Done

- 30 分連続の編集 + キュー操作 + 出力で致命クラッシュ 0
- 再起動後に作業復帰できる（レイアウト・履歴・主要設定）
- 失敗時に原因候補と発生箇所が UI 履歴で追跡可能
- リリース手順がドキュメントのみで再現できる

## Work Breakdown

### 1. Recovery Backbone

- 対象:
  - `src/AppMain.cppm`
  - `ArtifactCore/src/Core/FastSettingsStore.cppm`
- 完了条件:
  - 自動保存/復元の基本導線
  - 前回クラッシュ検知と復元確認ダイアログ
- 進捗 (2026-03-11):
  - セッション状態マーカーを `FastSettingsStore` で保存
    - 起動時: `Session/isRunning=true` を記録
    - 正常終了時: `Session/isRunning=false` に更新
  - 起動時に前回異常終了を検知した場合の警告ダイアログを追加
  - 既存の Recovery Snapshot 導線と併用

### 2. Operation Safety

- 対象:
  - `src/Service/ArtifactProjectService.cpp`
  - `src/Widgets/Menu/*`
  - `src/Widgets/ArtifactProjectManagerWidget.cppm`
- 完了条件:
  - 破壊操作の確認文言と条件を Service 側に一本化
- 進捗 (2026-03-11):
  - `ArtifactProjectService` に削除確認/削除実行 API を追加
    - `layerRemovalConfirmationMessage(...)`
    - `projectItemRemovalConfirmationMessage(...)`
    - `removeProjectItem(...)`
  - LayerMenu のレイヤー削除を確認付きに変更
  - ProjectManagerWidget の Delete を Service 経由へ統一

### 3. RenderQueue Diagnostics

- 対象:
  - `src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
  - `src/Render/ArtifactRenderQueueService.cppm`
- 完了条件:
  - 履歴に UI/Service ソースが明示される
  - 進捗通知粒度を設定可能
  - 履歴の外部エクスポートが可能

### 4. Backend Fallback

- 対象:
  - `src/Widgets/Render/ArtifactCompositionEditor.cppm`
  - `src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
  - `src/Render/Software/*`
- 完了条件:
  - D3D12 初期化失敗時に Software 経路へ移行
  - ユーザーへ明示通知

### 5. Regression Net

- 対象:
  - `src/Test/*`
  - CMake test 設定
- 完了条件:
  - 最低 10 本のスモーク/回帰テスト

### 6. Release Ops

- 対象:
  - `CMakePresets.json`
  - `docs/*`
- 完了条件:
  - Debug/Release の preset を整理
  - パッケージング手順を固定

## Suggested Order

1. `3 -> 2`（可観測性と安全性を先に固定）
2. `1 -> 4`（復旧とフォールバック）
3. `5 -> 6`（回帰網と配布整備）
