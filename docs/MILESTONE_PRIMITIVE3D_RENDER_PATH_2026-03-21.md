# Primitive 3D Render Path Milestone

**進捗状態:** P3D-1〜4 は実装済み、P3D-5 は backend 側の基盤実装済みで parity の runtime 検証待ち。3D primitive 形状は Plane/Box/Sphere/Cylinder/Cone + Torus/Capsule/Pyramid (2026-08-29 追加)。

### 実装状況（2026-07-25 確認）

責務分離、Solid/Wireframe mesh 描画、Material／Depth pass、mesh geometry cache、camera／gizmo／selection overlay、offline depth 出力と Diligent 経路を確認した。残課題は software fallback と backend の画素単位 parity、診断 widget を使った比較、複雑 mesh／透明境界の実機検証。

### 2026-08-29 — Torus / Capsule / Pyramid を `FixedGeometry3D` に追加

- 3D レイヤーの固定形状プリミティブに Torus / Capsule / Pyramid を追加し、メニュー (Layer → New → 3D) と Composition Editor の右クリック New 3D メニューから生成可能にした。
- Torus: `width/height` を主半径 (X/Y 楕円可)、`depth` を管半径、`segments` を管円周分割、`rings` を主円周分割。法線は中心方向 + 軸方向の合成で算出、UV は (u, v) = (管円, 主円) に対応。
- Capsule: `width/depth` を半径、`height` を全体高さ (両半球含む) として解釈、円筒と上下半球のリングで構成。Rings は内部で segments 連動。`height < 2*radius` の極端入力はクランプ。
- Pyramid: `width/height/depth` で底面サイズと頂点高さを指定、`segments` で多角形底面 (3=三角錐、4=四角錐、5=五角錐 …) の側面数を指定。側面は平均ベース頂点 + 頂点から法線を計算。
- JSON `fromJsonProperties` の範囲チェック、Property Widget の `geometry.type` tooltip、Segments/Rings の露出条件、Torus/Capsule/Pyramid 用の `create*Mesh()` 関数を `Artifact3DLayer` に追加。
- 既存範囲チェックは `FixedGeometry3D::Pyramid` 上限まで拡張。
- ビルド・ランタイム検証はユーザー指示待ちで未実施。

### 2026-08-29 — 3D Shader Variant Phase 1 (L1) 境界を導入

- 3D Primitive mesh 描画を Provider 境界の先に置くため、`Artifact3DPrimitiveSubmitter` (Contract) / `Artifact3DPrimitivePipelineAdapter` を追加。先行例 (Text Glyph G2 移行) と同一形状の境界で、Submitter は ShaderManager を知らず、Adapter が薄い bridge になる。
- 想定 variant: `Unlit` / `FlatLit` / `Wire`。`Stage` enum と `SubmitPacket` (position/normal/index/model/view/projection + BaseColor/Emission/Opacity) を Contract に固定。Phase 1 では `uploadMesh` のみ動作し `submit` は no-op (provider が空のため)。
- `Artifact3DLayer::draw()` には **未接続**。既存 `draw3DLine` 経由の line 描画のみが活きており、回帰リスクなし。Phase 2 (L2) で ShaderManager に PSO getter を足し、Adapter を実体化、Phase 3 (L3) で Material 全部 PBR + ユーザシェーダ設計レビュー、のロードマップ。
- ビルド・ランタイム検証はユーザー指示待ちで未実施。CMake 登録 (`ArtifactSources.cmake` 4ファイル + `ArtifactRenderModuleReferences.cmake` 2エントリ) も反映済み。

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
- Artifact3DLayer が ArtifactAbstractLayer から継承
- is3D() メソッドが適切に実装
- 基本的な 3D wireframe 描画が可能

### P3D-2 Solid Shading Path (進行中)

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

- 3D 物体が solid で読める (face filling implemented)
- wireframe と切り替えても破綻しない
- 2D overlay を上から重ねられる (inherited from base class)
- material の最小限実装 (render mode プロパティ)

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

- Mesh データ構造の統合 (ArtifactCore::Mesh を使用)
- 基本的な mesh loading と rendering
- transform 変更と geometry 変更の分離 (transform は毎フレーム適用)

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
