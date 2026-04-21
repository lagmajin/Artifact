# Artifact v0.4 Pipeline Foundation (2026-03-10)

v0.4 は「編集できる」状態から「制作パイプラインとして回せる」状態へ進める段階。

## Goal

次の 6 項目を満たしたら v0.4 を達成とする。

1. プロジェクト保存/読込の整合性向上（破損耐性）
2. レンダリングバックエンド切替準備（D3D12/Vulkan/Software）
3. エフェクト検証パスの標準化（OpenCV + 本線）
4. バッチ出力とキュー運用（複数ジョブ実務運用）
5. 最低限の自動テスト導入（回帰防止）
6. パフォーマンス計測とボトルネック可視化

## Definition Of Done

- 保存->再読込で主要データ（コンポ/レイヤー/キュー）が保持される
- バックエンド切替時にアプリが落ちず、フォールバックできる
- 同じ入力に対して出力差分が説明可能
- 少なくとも5つの主要回帰シナリオが自動で検出可能

## Work Breakdown

### 1. プロジェクトI/O堅牢化

- 対象:
  - `src/Project/ArtifactProjectExporter.cppm`
  - `src/Project/ArtifactProjectImporter.cppm`
  - `src/Project/ArtifactProjectHealthChecker.cppm`
- 完了条件:
  - 旧データ読込時の修復ルールを定義
  - 壊れた参照の検出と通知を標準化

### 2. バックエンド抽象化

- 対象:
  - `include/Render/ArtifactIRenderer.ixx`
  - `src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cpp`
  - `src/Render/Software/*`
- 完了条件:
  - 実行時に backend 選択可能（設定/起動引数）
  - D3D12 不可環境で Software fallback
  - Vulkan は「初期化成功/失敗判定」までを成立

### 3. エフェクト検証の二重系統運用

- 対象:
  - `src/Widgets/Render/ArtifactSoftwareRenderTestWidget.cppm`
  - `src/Render/Software/ArtifactSoftwareImageCompositor.cppm`
  - `src/Effects/*`
- 完了条件:
  - テストウィジェットで再現した設定を本線へ移植しやすいデータ形式に統一
  - 最低3つのブレンド/エフェクトで比較確認可能

### 4. RenderQueue 実務化

- 対象:
  - `src/Render/ArtifactRenderQueueService.cppm`
  - `src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
- 完了条件:
  - 複数ジョブ連続実行で停止しない
  - 失敗リトライとスキップを運用可能
  - 出力ディレクトリ管理と重複命名規則を明文化

### 5. 回帰テスト基盤

- 対象:
  - `src/Test/*`
  - CMake test 設定
- 完了条件:
  - プロジェクト作成->コンポ作成->レイヤー追加->キュー追加のスモークテスト
  - 主要Serviceの単体テストを最低5本

### 6. 計測と可視化

- 対象:
  - `src/Core/SystemStats.cppm`
  - `src/Widgets/ArtifactPerformanceProfilerWidget.cppm`
- 完了条件:
  - FPS/フレーム時間/キュー待ち時間の表示
  - 重い処理の上位3件をログで出力

## Execution Order

1. `1 -> 4`（データと出力の信頼性を先に確保）
2. `2 -> 3`（レンダーパス拡張を段階実装）
3. `5 -> 6`（回帰と最適化の土台を固定）

## Exit Criteria

- 「落ちない・戻せる・回せる」の3条件を満たす
- 主要機能の手動確認工数が現状の半分以下になる
