# MILESTONE_V1_4_CREATION_FLOW_AND_MODULE_ROLLOUT_2026-03-11

## Goal
プロジェクト未作成時でも編集開始できる導線を確立し、`import std;` 置換を段階的に進めてモジュール移行の安全性を高める。

## Scope
1. コンポジション作成のゼロセットアップ化
2. メニュー有効条件の実態整合（作成可能操作を不必要に無効化しない）
3. `import std;` を小さな単位で継続展開（10ファイル単位）

## Deliverables
- `ArtifactProjectService` が「プロジェクトなし -> 自動プロジェクト作成 -> コンポジション作成」を許容
- `File` / `Composition` メニューから常時コンポジション作成を実行可能
- `import std;` 適用ファイルを10件追加

## Done Criteria
- プロジェクト未作成状態で以下が成功する:
  - `File > 新規コンポジション`
  - `Composition > 新規コンポジション`
  - `Composition > プリセットから作成`
- 既存の複製/改名/削除など「現在コンポジション依存操作」は従来通り current composition がある時のみ有効
- 対象10ファイルで `import std;` が導入済み

## Next (V1.5 candidate)
1. `import std;` 展開を Render/Project/Timeline の高頻度編集ファイルへ拡大（+20〜30 files）
2. stdヘッダ残存の棚卸しと方針統一（完全置換 or 併用）
3. Composition 作成ダイアログのデフォルトプリセット/命名規則の整備
4. 失敗時診断ログ（自動作成失敗、作成途中例外）の可視化
