# Milestone: ImGuizmo Direct Code Integration / Gizmo Port to Artifact Render API (2026-04-09)

**Status:** Draft  
**Goal:** `ImGuizmo` を外部ライブラリとして組み込むのではなく、描画プリミティブと操作ロジックを Artifact のコードとして移植し、既存の render / overlay 基盤へ直接つなぐ。

---

## Why This Now

`ImGuizmo` は 3D gizmo の形と操作ロジックが比較的独立していて、`AddLine` / `AddTriangleFilled` / `AddCircle` のような描画プリミティブを置き換えれば、Artifact 側の renderer API に直接移植しやすい。  
一方で、ライブラリとしてそのまま持ち込むと、

- 描画 backend への依存が増える
- editor overlay の責務が外部 API に引っ張られる
- gizmo / hit test / camera / selection の契約が曖昧になる

という問題が起きやすい。

この milestone は、ImGuizmo の「形状構造」と「操作ロジック」を Artifact 側へ写し、`ArtifactIRenderer` と `TransformGizmo` 系の責務に合わせて再構成するためのもの。

---

## Scope

- `docs/ImGuizmo_Gizmo_Structure_Analysis.md`
- `Artifact/src/Widgets/Render/TransformGizmo.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/include/Render/ArtifactIRenderer.ixx`
- `ArtifactCore/include/Transform/*`
- `ArtifactCore/include/Geometry/*`
- 必要に応じて `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cpp`

---

## Non-Goals

- `ImGuizmo` のライブラリをそのまま submodule として使うこと
- Dear ImGui 依存を editor 全体へ広げること
- 3D gizmo の完全な Blender / Maya 再現
- low-level backend の全面置換

---

## Design Principles

- 形状データは direct code として移植する
- 描画は `ArtifactIRenderer` / overlay API に寄せる
- gizmo 操作は widget の個別描画よりも共通 state に寄せる
- hit test と draw の座標系を一致させる
- camera / selection / gizmo の責務を分離する
- `ImGui` のイベントループ前提を持ち込まない

---

## Phases

### Phase 1: Structure Extraction

- `ImGuizmo` の描画関数と操作ロジックを棚卸しする
- translation / rotation / scale のそれぞれで必要な primitive を抽出する
- Artifact 側に必要な math / state / input 契約を決める

**Done when:**

- 移植対象の gizmo 種類が整理される
- どの primitive を Artifact の renderer に置き換えるか決まる

### Phase 2: Direct Primitive Port

- `ImGuizmo` 相当の線 / 円 / 三角 / quad 描画を Artifact API に置き換える
- まずは translation gizmo から direct code 化する
- rotation / scale はその後に続ける

**Done when:**

- 外部ライブラリ依存なしで gizmo の基本形状を描ける
- renderer API だけで gizmo が描ける

### Phase 3: Interaction And Hit Test Parity

- gizmo の hit test を direct code へ移す
- selection / hover / active state の更新を統一する
- camera / viewport 変換と一致することを確認する

**Done when:**

- drag / hover / select が描画と一致する
- gizmo の軸や面の選択が安定する

### Phase 4: Editor Overlay Integration

- composition editor の overlay 描画に直接接続する
- playhead / selection / guide と gizmo の順序を固定する
- widget-local の手描きを減らす

**Done when:**

- gizmo が editor overlay として安定表示される
- overlay の責務が widget ごとに分散しない

### Phase 5: Backend Parity And Diagnostics

- software backend と Diligent backend の差を確認する
- gizmo の座標ズレ / 太さ / 重なり順を診断できるようにする
- 3D editor で再現しやすい小さな検証導線を用意する

**Done when:**

- backend を切っても gizmo が大きく崩れない
- 問題が出たときに原因箇所を切り分けやすい

---

## Implementation Breakdown

### 1. Code Port

- `ImGuizmo.cpp` の描画ロジックを direct code として移植する
- `AddLine` / `AddCircle` / `AddTriangleFilled` 相当を Artifact API へ差し替える
- まず translation gizmo を基準実装にする

### 2. Geometry And Math

- gizmo axis / plane / ring の geometry を Artifact 側の math 型に合わせる
- world / view / projection の変換契約を固定する
- screen-space と world-space の混在を避ける

### 3. Interaction Model

- hover / active / drag state を共通 state にする
- hit test を draw と同じ幾何に基づかせる
- mouse input を widget-local に閉じ込めすぎない

### 4. Overlay Integration

- `ArtifactIRenderer` の overlay 経路に乗せる
- gizmo / bounds / guide / playhead の重なり順を固定する
- viewer / timeline / editor で共通利用しやすくする

### 5. Validation And Diagnostics

- gizmo のズレを可視化する debug mode を用意する
- selection / hover / active の状態をログ化する
- backend 差異の再現条件を docs に残す

---

## Suggested Order

1. `Phase 1: Structure Extraction`
2. `Phase 2: Direct Primitive Port`
3. `Phase 3: Interaction And Hit Test Parity`
4. `Phase 4: Editor Overlay Integration`
5. `Phase 5: Backend Parity And Diagnostics`

---

## Validation Checklist

- `ImGuizmo` 由来の gizmo が Artifact のコードだけで描ける
- translation gizmo が direct code で動く
- rotation / scale の移植先が明確になる
- hit test と draw が同じ座標系を見ている
- overlay / gizmo / playhead の重なり順が崩れない
- software / Diligent で大きな表示差が出ない

---

## Relationship To Existing Milestones

- [Primitive 3D Render Path](/x:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_PRIMITIVE3D_RENDER_PATH_2026-03-21.md)
- [ArtifactIRender](/x:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_ARTIFACT_IRENDER_2026-03-12.md)
- [Composition Editor](/x:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_COMPOSITION_EDITOR_2026-03-21.md)
- [Gizmo Visibility Investigation](/x:/Dev/ArtifactStudio/docs/bugs/BUG_INVESTIGATION_GIZMO_VISIBILITY_2026-03-24.md)

この milestone は、`ImGuizmo` の構造分析を受けて「外部ライブラリを足す」のではなく「Artifact の renderer / overlay / gizmo の責務へ直接移植する」実行計画として機能する。

---

## Current Status

- `ImGuizmo` の構造は分析済み
- 描画プリミティブの置換先候補も見えている
- ただし現状は分析ノート止まりで、実装用の direct-code milestone が不足している
- そのため、まずは translation gizmo を direct code で 1 本通すのが最短

