# Primitive 3D Render Path Milestone

`primitive2d` 側で行っている「描画の下請け化」を、3D でも同じ思想で扱えるようにするためのマイルストーン。
2D の延長で 3D を雑に載せるのではなく、`mesh / camera / material / light / pass / overlay` を分けて、後から editor へ接続しやすい形に整理する。

## Goal

- 3D 描画の責務を、2D primitive と同じく「呼び出し側が欲しい見た目を出す最小 API」に寄せる
- 3D レイヤー、3D プレビュー、3D viewer、ソフトウェア検証経路を共通化しやすくする
- Diligent / software fallback / offscreen preview で、3D の基本表示が崩れないようにする
- 将来の gizmo、camera orbit、depth sorting、PBR 以前の solid shading を段階的に入れられる土台を作る

## Scope

- `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cpp`
- `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `ArtifactCore/src/Mesh/*`
- `ArtifactCore/src/Scene/*`
- `ArtifactCore/src/Transform/*`
- 3D 対応の layer / preview / overlay 接続

## Non-Goals

- フル PBR マテリアルシステムの完成
- ノードベースの 3D レンダラー再設計
- Diligent / DX12 低レベル API の全面置換
- Blender 相当の complete compositor 再現

## Canonical Model

このマイルストーンでは、3D 描画を以下の要素に分ける。

- `Scene`
  3D オブジェクト、カメラ、ライト、背景、描画順を保持する上位単位
- `Camera`
  view / projection / orbit / pan / dolly を扱う
- `Mesh`
  頂点、法線、UV、インデックス、bounding box を持つ
- `Material`
  色、粗さ、法線有無、簡易シェーディング条件を持つ
- `Pass`
  solid / wireframe / depth / overlay を分離して実装する
- `Overlay`
  gizmo、bounds、grid、safe area 相当の補助表示

`primitive2d` が「2D 図形の描画委譲」なら、この milestone は「3D 形状の描画委譲」を担当する。

## Milestones

### P3D-1 Inventory And Responsibility Split

目的:
現状の 3D 関連コードが、どの widget / renderer / core に何を持っているかを分解する。

対象:

- `Artifact3DModelViewer`
- `ArtifactDiligentEngineRenderWindow`
- 3D レイヤー描画経路
- `Mesh` / `SceneNode` / `Transform3D`

実装方針:

- 2D preview と 3D viewer の責務差を明示する
- `draw` が direct render なのか、surface 生成なのか、pass 実行なのかを分類する
- 3D で必要な最小データ構造を列挙する
- 既存の 2D primitive API を流用してよい箇所と分けるべき箇所を切り分ける

Done:

- 3D 関連の責務一覧が docs に残る
- `primitive2d` の 3D 対応先が明確になる
- どの API を拡張すべきかが決まる

### P3D-2 Solid Shading Path

目的:
Blender の Solid に近い、単純で壊れにくい 3D viewport を作る。

対象:

- solid viewport shading
- flat color / unlit / simple light
- backface / depth / face normal の最低限の見え方

実装方針:

- まずは `solid` と `wireframe` を分ける
- material は最小限の色と shading flag から始める
- 2D overlay との重なり順を固定する
- 3D レイヤーがない場合の fallback mesh を定義する

Done:

- 3D 物体が solid で読める
- wireframe と切り替えても破綻しない
- 2D overlay を上から重ねられる

### P3D-3 Mesh Upload And Cache

目的:
メッシュの再描画を毎フレームの重い処理にしない。

対象:

- vertex / index buffer upload
- bounding box cache
- transform dirty tracking
- mesh reuse

実装方針:

- 形状が変わらない限り GPU へ再 upload しない
- transform 変更と geometry 変更を分ける
- source mesh と render mesh を分離する

Done:

- カメラ移動や回転で再生成が走りすぎない
- 同じ mesh を複数レイヤーで共有しやすい

### P3D-4 Camera And Gizmo Parity

目的:
2D composition editor と同様に、3D editor でも camera / selection / gizmo を扱えるようにする。

対象:

- orbit / pan / zoom / dolly
- selection bbox
- anchor / rotation / move handles
- 3D overlay annotations

実装方針:

- camera controls は widget 側に閉じすぎない
- gizmo 描画は renderer API に寄せる
- selection state と render state を分離する

Done:

- 3D で selection と camera が共存できる
- overlay が per-widget の手描きに戻りにくい

### P3D-5 Software And Backend Parity

目的:
software fallback と Diligent backend の見え方の差を詰める。

対象:

- solid mesh draw
- wireframe
- depth sort
- alpha blend
- fallback screenshot / preview

実装方針:

- まず software で仕様を固定する
- その後 backend 側を合わせる
- 3D 用の diagnostic widget を用意する

Done:

- backend 差で見た目が大きく崩れない
- preview / viewer / test widget で再現できる

## Recommended Order

1. `P3D-1 Inventory And Responsibility Split`
2. `P3D-2 Solid Shading Path`
3. `P3D-3 Mesh Upload And Cache`
4. `P3D-4 Camera And Gizmo Parity`
5. `P3D-5 Software And Backend Parity`

## Risks

- 2D の `primitive2d` に 3D を無理に寄せると、責務が曖昧なまま肥大化する
- Diligent / DX12 の差を隠しすぎると、backend parity の検証が難しくなる
- camera / mesh / material / overlay を一枚岩にすると後で分離しにくい
- 既存の preview widget と 3D viewer で更新タイミングがずれると、見た目だけ整って内部が壊れる

## Exit Criteria

- 3D の描画責務が docs で説明できる
- solid viewport が安定して動く
- mesh cache と camera control の境界が分かれる
- 2D primitive と 3D primitive の役割が混ざらない

## Notes

- この milestone は「3D を本格的に美しくする」より先に、「3D 描画の下請け化をきちんと作る」ことを優先する
- 実装順は solid viewport を先にして、wireframe と overlay をその次に置くと詰まりにくい
