# Artifact v0.6 Effects Pipeline Usability (2026-03-11)

v0.6 は「機能はあるが扱いにくい」状態から、エフェクト編集を日常作業で回せる状態へ進める段階。

## Goal

次の 6 項目を満たしたら v0.6 を達成とする。

1. Effects Pipeline の操作導線を短縮（追加/切替/削除）
2. ステージ（Generator/Geo/Material/Rasterizer/LayerTransform）の可視性を改善
3. エフェクト選択時にプロパティ表示を確実に追従
4. エフェクト有効/無効・順序変更の誤操作を低減
5. Layer選択切替時の表示遅延/クラッシュを防止
6. Effect編集に対するUndo/Redo導線の接続準備

## Definition Of Done

- 主要操作（追加/削除/有効切替）が2クリック以内で実行できる
- レイヤー未選択時に編集操作が無効化される
- エフェクト選択中に Properties が一貫して同じ対象を示す
- 10分連続のエフェクト編集でクラッシュしない

## Work Breakdown

### 1. Inspector Effects UX

- 対象:
  - `src/Widgets/ArtifactInspectorWidget.cppm`
- 完了条件:
  - 選択状態連動の Add/Remove 有効化
  - ダブルクリックで Enable/Disable 切替
  - 削除確認とステータス表示

### 2. Effect-Property Sync

- 対象:
  - `src/Widgets/ArtifactInspectorWidget.cppm`
  - `src/Widgets/ArtifactPropertyWidget.cppm`
- 完了条件:
  - エフェクト選択でプロパティの表示対象が明示される
  - レイヤー切替時に古い参照が残らない
- 進捗 (2026-03-11):
  - PropertyWidget に `focusedEffectId` を追加
  - Inspector の effect 選択変更時に focusedEffectId を同期
  - レイヤー切替/未選択化で focusedEffectId をクリア

### 3. Ordering and Safety

- 対象:
  - `src/Layer/ArtifactAbstractLayer.cppm`
  - `src/Widgets/ArtifactInspectorWidget.cppm`
- 完了条件:
  - ステージ内の順序変更（Up/Down）導線を追加
  - 誤操作防止の確認・無効条件を統一
- 進捗 (2026-03-11):
  - Inspector の各ラックに `Up/Down` ボタンを追加
  - ラック右クリックメニューに `Move Up/Move Down` を追加
  - 並び替えは同一ステージ内のみ許可（境界越えを禁止）
  - UI からの effect 編集（追加/削除/有効切替/並び替え）を Service API 経由へ集約開始

### 4. Service Boundary Cleanup

- 対象:
  - `src/Service/ArtifactProjectService.cpp`
  - `include/Service/ArtifactProjectService.ixx`
- 完了条件:
  - UI層が直接レイヤー内部実装へ触れない補助APIを追加
  - Effects編集経路を Service 経由へ段階移行
- 進捗 (2026-03-11):
  - `ArtifactProjectService` に effect 編集 API を追加:
    - `addEffectToLayerInCurrentComposition`
    - `removeEffectFromLayerInCurrentComposition`
    - `setEffectEnabledInLayerInCurrentComposition`
    - `moveEffectInLayerInCurrentComposition`

### 5. Regression Scenarios

- 対象:
  - `docs/TimelinePanelImplementationChecklist.md`
  - `src/Test/*`
- 完了条件:
  - エフェクト編集の手動チェック項目を明文化
  - 最低2本のスモークテストを追加

## Suggested Order

1. `1 -> 2`（まず操作性と同期を固定）
2. `3`（順序変更を導入）
3. `4 -> 5`（境界整理と回帰防止）
