# Artifact v0.9 Operations and Diagnostics (2026-03-11)

v0.9 は「RC品質」から「実運用で保守できる品質」へ進める段階。

## Goal

次の 6 項目を満たしたら v0.9 を達成とする。

1. 異常終了時の復旧導線をユーザーが迷わず使える
2. 問題発生時の診断情報を UI から収集できる
3. RenderQueue 失敗の分析ログを簡単に共有できる
4. 主要設定の整合性チェックを起動時に実行
5. 破壊操作の確認規約を全画面で統一
6. 運用手順（復旧/診断/再実行）をドキュメント化

## Definition Of Done

- 異常終了を検知した起動で復旧フォルダへ直接アクセス可能
- 診断ログ/履歴をファイル出力し、再現報告へ添付できる
- 主要な破壊操作で無確認削除が残っていない

## Work Breakdown

### 1. Recovery UX

- 対象:
  - `src/AppMain.cppm`
  - `src/Project/ArtifactProjectAutoSaveManager.cppm`
- 完了条件:
  - 異常終了警告から復旧フォルダを直接開ける
  - 復旧スナップショット読み込み導線を明示
- 進捗 (2026-03-11):
  - 異常終了検知ダイアログに `Open Recovery Folder` を追加

### 2. Diagnostics Export

- 対象:
  - `src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
  - `src/Widgets/ArtifactMainWindow.cppm`
- 完了条件:
  - 主要診断情報をワンクリック出力
  - RenderQueue 履歴との関連付け
- 進捗 (2026-03-11):
  - Help メニューに `Export Diagnostics...` を追加
  - 次の情報をテキスト出力:
    - 実行環境情報（OS / CPU Arch / AppData パス）
    - セッション状態ファイル/RenderQueue履歴ファイルの存在確認
    - Recovery スナップショット一覧（ファイル名/サイズ/更新日時）
    - セッション主要キー値（isRunning/start/lastCleanExit/pid）
    - 現在の Dock ウィジェット可視状態

### 3. Startup Validation

- 対象:
  - `src/AppMain.cppm`
  - `ArtifactCore/src/Core/FastSettingsStore.cppm`
- 完了条件:
  - 破損/不整合設定を検知し安全側へ自動復帰
- 進捗 (2026-03-11):
  - 起動時に `session_state.cbor` の主要キー型をサニタイズ
  - 起動時に `main_window_layout.cbor` の主要キー型をサニタイズ
  - 不正型のキーは削除して安全側へ復帰

### 4. Safety Rule Sweep

- 対象:
  - `src/Widgets/Menu/*`
  - `src/Widgets/ArtifactProjectManagerWidget.cppm`
- 完了条件:
  - 削除・上書き・全消去操作に確認導線を統一
- 進捗 (2026-03-11):
  - File メニューの終了系操作に確認導線を追加
    - `プロジェクトを閉じる`
    - `再起動`
    - `終了`

### 5. Operational Playbook

- 対象:
  - `docs/*`
- 完了条件:
  - 復旧手順・診断手順・再実行手順を1本化

## Suggested Order

1. `1 -> 2`（復旧導線と診断導線を先に固定）
2. `4`（安全規約を仕上げる）
3. `3 -> 5`（起動安全化と運用ドキュメント）
