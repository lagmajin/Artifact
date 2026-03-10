# Artifact v0.5 Playable Editor (2026-03-11)

v0.5 は「内部検証ツール」から「短尺案件を実際に回せるエディタ」へ進める段階。

## Goal

次の 7 項目を満たしたら v0.5 を達成とする。

1. タイムライン編集（追加/移動/トリム）のUndo/Redoを安定運用
2. レイヤープロパティ編集の即時反映とクラッシュゼロ化
3. RenderQueue の再実行/再利用（プリセット）を運用レベルへ
4. Composition/Layer/Queue の削除依存確認を全導線で統一
5. 起動時レイアウト・主要設定の復元精度を実運用水準へ
6. Software/D3D12 の切替検証フローを固定
7. 最低限の自動回帰テストを CI 相当に近づける

## Definition Of Done

- 30分連続の編集 + 出力操作で落ちない
- 操作不能項目は常に disabled または明確な警告
- 同一入力で同一結果が再現できる（履歴/設定が追跡可能）
- 主要フローの回帰をテストで検出できる

## Work Breakdown

### 1. Timeline Editing Reliability

- 対象:
  - `src/Widgets/ArtifactTimelineWidget.cpp`
  - `src/Widgets/Timeline/ArtifactTimelineObjects.cpp`
  - `src/Undo/UndoManager.cppm`
- 完了条件:
  - 左右ハンドル/シーク/選択の競合ゼロ
  - 1フレーム単位操作が常に一致

### 2. Inspector + Property Safety

- 対象:
  - `src/Widgets/ArtifactInspectorWidget.cppm`
  - `src/Widgets/ArtifactPropertyWidget.cppm`
  - `src/Service/ArtifactProjectService.cpp`
- 完了条件:
  - レイヤー切替時の破棄順クラッシュを解消
  - PropertyGroup再構築の寿命管理を明確化

### 3. RenderQueue Practical Ops

- 対象:
  - `src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
  - `src/Render/ArtifactRenderQueueService.cppm`
- 完了条件:
  - `Rerun Selected` / `Rerun Done-Failed` の運用を固定
  - ジョブ設定プリセット保存/読込（1ユーザプリセット以上）
  - 失敗理由の保存と再試行導線

### 4. Dependency-safe Deletion

- 対象:
  - `src/Widgets/Menu/ArtifactCompositionMenu.cppm`
  - `src/Widgets/ArtifactProjectManagerWidget.cppm`
  - `src/Service/ArtifactProjectService.cpp`
- 完了条件:
  - コンポ削除時にキュー参照を必ず確認
  - どこから削除しても同一メッセージ/同一結果

### 5. Settings Backbone Expansion

- 対象:
  - `ArtifactCore/src/Core/FastSettingsStore.cppm`
  - `src/AppMain.cppm`
  - `src/Widgets/*`
- 完了条件:
  - 主要UI設定の `FastSettingsStore` 移行
  - 旧 `QSettings` から一回移行後は新ストア優先

### 6. Backend Switch Validation

- 対象:
  - `src/Widgets/Render/ArtifactCompositionEditor.cppm`
  - `src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
  - `src/Render/Software/*`
- 完了条件:
  - D3D12不可時のフォールバック手順を確立
  - Software経路で基本表示と1フレーム出力が可能

### 7. Test Baseline

- 対象:
  - `src/Test/*`
  - CMake test 設定
- 完了条件:
  - 最低5本のスモーク/回帰テスト
  - 「プロジェクト作成→コンポ作成→レイヤー追加→キュー追加→実行」を自動確認

## Suggested Order

1. `1 -> 2`（編集安定化を先に固定）
2. `3 -> 4`（出力運用と破壊操作の安全化）
3. `5 -> 6 -> 7`（運用基盤を仕上げ）

