# Artifact Operations Playbook v0.9 (2026-03-11)

このドキュメントは、障害時の一次対応を UI だけで完結するための最短手順を定義する。

## 1. 異常終了後の復旧

1. アプリ起動時に「前回セッションが正常終了していません」の警告が出たら `Open Recovery Folder` を押す
2. `Recovery` 配下の最新ファイル（更新日時）を確認する
3. 必要であれば `Recovery Snapshot Found` ダイアログで `Recover` を選択する
4. 復旧後、すぐに別名保存して現行プロジェクトを保全する

## 2. 診断ログ採取

1. `Help -> Export Diagnostics...` を実行する
2. 保存された `artifact_diagnostics_*.txt` を確認する
3. バグ報告には次を添付する
   - 診断 txt
   - 再現手順
   - 失敗した操作の日時

## 3. RenderQueue 失敗時

1. RenderQueue マネージャーで失敗ジョブを確認する
2. 履歴をエクスポートする（履歴 UI の Export）
3. 診断ログの `RenderQueue History Exists` が `true` であることを確認する

## 4. レイアウト破損時

1. 起動直後にレイアウトが崩れる場合は `Help -> Export Diagnostics...` を実行する
2. 診断ログの `Layout Restore Result` を確認する
   - `layoutResetApplied: true` の場合、互換性のない保存レイアウトが自動リセットされた
3. 必要ならウィンドウを再配置し、終了時保存で再学習させる

## 5. 破壊操作の基本規約

1. 削除・閉じる・再起動・終了は確認ダイアログ付き操作を使う
2. 重要データ変更前に Recovery スナップショットを残す

## 6. 問題切り分けテンプレート

- 発生日時:
- 操作:
- 期待結果:
- 実際結果:
- 直前の状態（編集中/再生中/レンダー中）:
- 添付:
  - diagnostics txt
  - 必要ならスクリーンショット/動画
