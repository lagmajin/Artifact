# 3D Particle 完成度マイルストーン (2026-08-29)

**最終更新:** 2026-09-05

> 2026-08-30 方針更新: 2D/3Dを単一Particle Layerの`is3D`切替として扱わず、
> `LayerType::Particle`（2D）と`LayerType::Particle3D`（3D）へ分離する。
> 以下の旧記述にある切替方式は履歴として残すが、正規設計は末尾の追補を優先する。

`ArtifactParticleLayer` および `ArtifactFormParticleLayer` を「QImage を 1 枚作ってスプライトで貼る 2D レイヤー」から「composition 内の 3D camera / light / depth / AOV と協調する 3D パーティクルレイヤー」へ段階移行するためのマイルストーン。`ArtifactCore::ParticleRenderer` は Core 側で view/proj を受ける PSO + 構造化バッファ + indirect draw + GPU cull を既に実装済み (2026-04-21 stabilization)、Composition 側にも 3D camera 行列を layer へ bundle する `Scoped3DLayerCamera` 配線済み (2026-03-25 3D Migration / 2026-06-05 Preview Freeze)。本マイルストーンは**その中間に残る未接続項目**を P4-1 〜 P4-6 として固定する。

## Goals

- `ArtifactParticleLayer` を `is3D()=true` として composition に登録し、3D camera / light / depth / AOV パスの bundle 対象に含める
- 2D 2D 想定の `transformParticleRenderData` 経路 (QTransform) を **3D camera 経路** (`viewMatrix * projMatrix`) に切替え、Camera Layer の orbit 時にパーティクルが画面上で立体的に動く
- software fallback (QImage キャッシュ) は緊急時のみ保持し、通常は GPU billboard 経路を使う
- `ArtifactFormParticleLayer` も同じ 3D フラグ付け + 3D camera 経路対応
- `VelocityAligned` billboard を実用レベルにする (現状 PSO はあるが、Artifact 側 transform で Z が潰れていないか要確認)
- 「ParticleLayer が壊れても project 全体は表示不能にしない」 invariant を維持

## Non-Goals

- フル PBR / Subsurface / Volumetric particle (P3-8 / Phase 5)
- Particle emitter / effector authoring UI の全面改修
- GPU simulation bridge (P3-7)
- particle 専用の temporal AOV 拡張 (現状の motion blur / velocity ターゲットに相乗り)

## Current Facts (2026-08-29 確認)

- `Artifact/src/Layer/ArtifactParticleLayer.cppm:437-442` で `Impl()` 構築時に `createParticleSystem()` を呼ぶが `setIs3D(true)` は未呼出。`Artifact3DLayer` / `ArtifactCameraLayer` / `ArtifactLightLayer` / `ArtifactSpatialAudioLayer` / `ArtifactProcedural3DLayer` / `ArtifactImageLayer` (3D-card path) は `setIs3D(true)` 呼出済み。
- 同様に `Artifact/src/Layer/ArtifactFormParticleLayer.cppm` の `Impl` 構築にも `setIs3D(true)` 呼出なし。
- `ArtifactCompositionRenderController.cppm:8913` の `if (layer->is3D())` 分岐が 3D camera / light 配線の gate。Particle 系レイヤは `is3D()=false` のため、ここを通らず常に 2D camera (view/proj = identity + NDC) で `draw()` される。
- 同じ gate を使う `drawGpuLayerEmissionToTarget` (line 11613) / `drawGpuLayerNormalToTarget` (line 11642) も Particle をスキップ → AOV (emission / normal) 経路に Particle が乗らず、3D シーンで Particle だけ AOV 不参加になる。
- `ArtifactParticleLayer::draw()` (line 449) は **GPU 経路 (`renderer->drawParticles(renderData)`) と Software fallback (`renderFrame` -> `cachedFrame` -> `drawSprite`) の二段持ち**。GPU 経路でも `transformParticleRenderData` (line 123) が `QTransform` で (px, py) だけ写像し、pz / vx / vy / vz は source のまま流される。
- `ArtifactIRenderer::drawParticles` (cppm:1483) は `particle3DCameraActive_` (line 589) を見て view/proj 切替を行うが、Particle から `drawParticles` への到達は `draw()` 内で常時 2D 扱い (Composition が 3D camera を配らないため)。
- `ArtifactCore::ParticleRenderer` (`ArtifactCore/src/Graphics/ParticleRenderer.cppm`) は `setViewMatrix` / `setProjectionMatrix` / `setRenderOptions` (billboard + blend + depth test/write) / GPU cull / indirect draw を実装済み。`ArtifactIRenderer::Impl` (line 1504) で lazy initialize (100k particles max) され、`DiligentImmediateSubmitter::submit` (line 879) の `ParticlePkt` 経路で 1 draw call として発行される。
- `ArtifactCore::ParticleData.ixx` の `ParticleVertex` は `px,py,pz / vx,vy,vz` を持つが、現在の `ParticleLayer` 経路では Z 軸が意味を持たないまま GPU へ送られている。
- `ArtifactCore::ParticleBillboardPolicy::VelocityAligned` (line 45) は enum として存在し、`ParticleLayer::coreRenderOptionsFromSettings` (line 300) で App 設定から Core へ写像済み。ただし velocity を使った実描画は Core 側シェーダに依存し、現状 runtime 検証は限定情報のみ。
- `ArtifactCompositionRenderController.cppm:32809` 周辺で `has3DCamera` が composition 内の 3D layer / 3D camera 存在で true になる。Particle が `is3D()=true` なら Particle 自身も `has3DCamera` の判定材料に追加できる (現状は `Artifact3DLayer` / `ArtifactCameraLayer` 等のみ参照)。

## Canonical Architecture

最終形の責務分担。

- `ArtifactCore::ParticleRenderer` 既存 API: `setViewMatrix` / `setProjectionMatrix` / `setRenderOptions` / `updateBuffer` / `prepare` / `draw` (維持)
- `ArtifactParticleLayer::Impl` に `is3D_` フラグ + `transformParticleRenderData3D(modelMatrix, view, proj)` を追加。`is3D()=true` 時は `QTransform` 経路をスキップして **particle position を model space のまま GPU に渡す** ことで GPU 側 billboard 計算が view/proj と整合する
- `ArtifactAbstractComposition` 側の `evaluateRenderPath` で Particle を 3D 要素として判定するロジック (現状 `Artifact3DLayer` / `ArtifactCameraLayer` ベース) に `ArtifactParticleLayer` / `ArtifactFormParticleLayer` を追加
- `ArtifactIRenderer::drawParticles` の 2D fallback 経路 (line 1538-1551) は **`is3D()=true` の layer には適用しない** ガードを追加。3D 経路が 3D camera 行列で billboard 描画する

## Milestones

### P4-1 Particle Layer の 3D フラグ付け

目的:
Composition の 3D camera bundle gate (`if (layer->is3D())`) に Particle 系を通す。

実装内容:

- `ArtifactParticleLayer::Impl` 構築 (`Artifact/src/Layer/ArtifactParticleLayer.cppm:437` 付近) で `setIs3D(true)` を呼出。`ArtifactFormParticleLayer` は **既に `Grid3D` preset 選択時に `setIs3D(true)` 呼出済** (`ArtifactFormParticleLayer.cppm:514` 内の `syncSourceSize` で `generatorMode == Grid3D` のとき自動切替)。`ArtifactFormParticleLayer` の JSON 復元経路 (`applyPropertiesFromJson` line 1173) も `syncSourceSize` を呼ぶため完了済み。
- `ArtifactAbstractLayer::fromJsonProperties` (`Artifact/src/Layer/ArtifactAbstractLayer.cppm:4990`) は layer type ごとに `is3D` を JSON 復元するが、既存 particle プロジェクトは `is3D=false` で保存されている可能性がある。デフォルトは `true` (新規作成) / `false` (既存復元) で、Property Widget に 3D 切替 UI を追加してユーザが任意に切替可能にする。
- 互換性: 既存 Particle プロジェクトが 2D 表示のまま開かれることを許容 (default false for legacy JSON)。新規作成 Particle は `is3D=true` を default とする。
- 検証: 3D camera 行列が `drawParticles` の `particleViewMatrix_ / particleProjMatrix_` にセットされ、`particle3DCameraActive_=true` で GPU 経路に入ること。

Done:
- Composition 上の Particle Layer が 3D camera に追従して billboard 表示
- 既存 Particle プロジェクトの表示が 2D のまま (回帰なし)

### P4-2 transformParticleRenderData の 2D/3D 分岐

目的:
GPU 経路の particle 座標を、3D フラグに応じて正しく model/view/proj する。

実装内容:

- `ArtifactParticleLayer::draw()` で `if (impl_->is3D_)` 分岐を追加。`true` のときは `transformParticleRenderData` (QTransform 経路) をスキップし、layer 3D transform (model matrix) を `ParticleRenderData` のメタ情報として GPU に渡す。
- `ArtifactCore::ParticleRenderData` に `QMatrix4x4 modelMatrix` フィールドを追加し、Core の `ParticleRenderer::setModelMatrix` を新設。`ParticlePkt` 経路 (DiligentImmediateSubmitter) で ConstantBuffer の model slot を更新。
- 2D 経路は `QTransform` を維持し、`is3D_=false` 時に従来挙動を保存。

Done:
- 3D 経路: model matrix が GPU へ届き、view/proj と組合せてスクリーン投影される
- 2D 経路: 既存挙動を完全保持

### P4-3 ArtifactFormParticleLayer の 3D 経路対応

目的:
`Form Particle` (グリッド/レイヤーソース生成) も 3D camera に対応させる。

実装内容:

- `Artifact/src/Layer/ArtifactFormParticleLayer.cppm:Impl` に `setIs3D(true)` 呼出追加 (default)
- `FormParticleSettings::GeneratorMode::Grid3D` (既存) を `is3D_=true` のとき自動採用する既定動作を追加
- `draw()` で `renderer->drawParticles` 経路に切替 (現状は `impl_->cachedRenderData` のみ？再確認)

Done:
- Grid3D preset 選択時に 3D camera 追従 / 2D fallback と整合

### P4-4 VelocityAligned / Stretched Billboard の runtime 検証

目的:
Advanced billboard policy を「enum はあるが見えない」状態から「実機で見える」状態にする。

実装内容:

- `ParticleLayer::coreRenderOptionsFromSettings` (line 300) で `VelocityAligned` を選んだとき、`Core::ParticleRenderOptions.billboard = VelocityAligned` が GPU シェーダで effective に反映されるか `ArtifactCore::Graphics.ParticleRenderer` 側で shader コード確認 + runtime 比較。
- 期待結果: 速度方向にスプライトが傾いて見える。傾きが見えなければ shader 修正 or `ParticleVertex` の `vx,vy,vz` が upload 時に正しい順序で並んでいない可能性を疑う。
- 検証失敗時: フォールバックとして `ViewPlane` にダウングレードする `P4-4b` サブタスク。

Done:
- `VelocityAligned` 選択時にプレビューで billboard が速度方向に傾いて見える

### P4-5 AOV 経路への参加 (Emission / Normal)

目的:
3D composition の emission / normal AOV に particle が寄与するようにする。

実装内容:

- `ArtifactCompositionRenderController::drawGpuLayerEmissionToTarget` (line 11613) / `drawGpuLayerNormalToTarget` (line 11642) の `if (!layer->is3D())` gate は **そのまま** (P4-1 完了後に Particle は gate を pass する)
- `ArtifactCore::ParticleRenderer` 側に emission / normal 出力を得る shader 経路があるかを棚卸し。なければ「emission のみ particle 寄与なし」と明文化し、normal は light response 用途なので P3-8 スコープに送る。

Done:
- 3D composition で Particle が emission AOV に 1 ピクセル以上寄与する / または「寄与しない」ことが docs で明示される

### P4-6 Diagnostics / Frame Debug 拡張

目的:
Particle の 3D 経路失敗を Frame Debug summary で追えるようにする。

実装内容:

- `ArtifactIRenderer::Impl::drawParticles` の `lastParticleDebug_` に `cameraMode=2d|3d` フィールドを追加
- `ARTIFACT_DISABLE_3D_RENDER_TRACE` 環境変数で粒子 draw 経路を trace できるようにする (既存 Article3DLayer の `ARTIFACT_DISABLE_3D_RENDER_TRACE` と並列)
- `ArtifactParticleLayer::debugState()` (line 551) に `renderMode=cpu-sprite|gpu-billboard(2d)|gpu-billboard(3d)` を追加

Done:
- 3D particle が見えないとき Frame Debug summary で原因が「camera が 2D fallback に落ちた」「PSO null」「empty buffer」のいずれかを即特定できる

## Recommended Order

1. `P4-1 Particle Layer の 3D フラグ付け` — 最大効果・最小リスク
2. `P4-2 transformParticleRenderData の 2D/3D 分岐` — フラグ付けだけでは Z が潰れたままなので必須
3. `P4-3 FormParticleLayer の 3D 経路対応` — 派生先の整合
4. `P4-5 AOV 経路への参加` — 3D scene での見栄え統合
5. `P4-4 VelocityAligned / Stretched Billboard の runtime 検証` — 高度 billboard ポリシー
6. `P4-6 Diagnostics` — 全体仕上げ

## Risks

- `setIs3D(true)` をデフォルトにすると、既存 particle プロジェクト (QTransform 2D 経路で構築) の表示が大きく変わる可能性。P4-1 で「default true for new / false for legacy JSON」と明示し、Property Widget に切替 UI を用意することでリスクを下げる。
- `transformParticleRenderData` 経路を 3D でスキップすると、layer の 2D offset / scale が GPU に行かない。layer 3D transform の position/scale を `ParticleRenderData.modelMatrix` に確実に写像することを P4-2 で保証。
- 3D camera 行列が届かない条件で `drawParticles` が呼ばれた場合 (例: 3D camera を持たない composition に Particle を置いた)、fallback で 2D camera に落とすと既存挙動を保つ。fallback の判定を `P4-2` で明示。
- `ArtifactCore::ParticleRenderer` の PSO は単一 billboard policy ごとに PSO を作る設計ではない。`VelocityAligned` の runtime 検証で shader 修正が必要になった場合、PSO の組合せ追加は別マイルストーンで扱う。

## Quality Gates

各段階で最低限これを満たすこと。

### Functional
- P4-1: 新規 Particle Layer が 3D camera orbit に追従して billboard 表示
- P4-1: 既存 Particle プロジェクトの 2D 表示が回帰しない
- P4-2: 3D camera 視点で Particle の奥行き感が読める (Z ソート / size depth fade が動く)
- P4-4: `VelocityAligned` 選択時にスプライトが速度方向に傾く

### Visual
- 2D fallback と 3D billboard の見た目が大きくズレない (size スケール 0.8 〜 1.2 倍に収まる)
- 3D composition で Particle が他の 3D layer (Plane / Model) と自然な前後関係で描画される

### Performance
- 3D 経路でも毎フレームの particle buffer 再 upload が発生しない (ParticleSystem 側で dirty 追跡されている前提)
- Camera orbit 中の frame time が 2D 経路と同等以下

## First Concrete Tasks

最初の着手候補。

1. `ArtifactParticleLayer::Impl` に `setIs3D(true)` 呼出追加 + JSON `is3D` デフォルト戦略を P4-1 で確定
2. `ArtifactCompositionRenderController` 11581 周辺の `has3DCamera` 判定に Particle Layer の `is3D()` を含める
3. `ArtifactCore::ParticleRenderData` に `modelMatrix` 追加 + `ParticleRenderer::setModelMatrix` API 新設
4. `DiligentImmediateSubmitter` の `ParticlePkt` 経路に model matrix を含める
5. runtime 検証: 3D camera orbit 時に Particle billboard が立体的に動くこと

## Exit Criteria

- `ArtifactParticleLayer` / `ArtifactFormParticleLayer` が composition 内で他の 3D layer と同じ 3D camera / light / AOV bundle に乗る
- 3D camera orbit 時に Particle billboard が立体的に動く (Z depth が見える)
- 既存 2D particle プロジェクトの表示が回帰しない
- Frame Debug summary で 3D particle 経路の失敗が追える

## Related

- `Artifact/docs/MILESTONE_PARTICLE_LAYER_3D_MIGRATION_2026-03-25.md` — P3-1〜P3-8 (旧計画)
- `Artifact/docs/MILESTONE_PARTICLE_RENDER_PATH_STABILIZATION_2026-04-21.md` — 安定化 Phase 1〜4 (完了済み想定)
- `Artifact/docs/MILESTONE_PRIMITIVE3D_RENDER_PATH_2026-03-21.md` — 3D Primitive / カメラ / mesh 経路
- `ArtifactCore/include/Graphics/ParticleData.ixx` — Core API 契約
- `ArtifactCore/src/Graphics/ParticleRenderer.cppm` — Core GPU 実装
- `Artifact/src/Render/ArtifactIRenderer.cppm:1483` — `drawParticles` 実装
- `Artifact/src/Render/ArtifactIRenderer.cppm:1279` — `set3DCameraMatrices` (mesh + particle 共有)
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:8913` — `if (layer->is3D())` 3D bundle gate
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:11613` / 11642 — AOV gate
- `Artifact/src/Layer/ArtifactParticleLayer.cppm:437-442` — `Impl()` 構築箇所 (要 `setIs3D(true)` 追加)

## 2026-08-30 追補 — 2D / 3Dレイヤー種別の分離

- `LayerType::Particle`（既存ID 15）を2D専用として維持し、`LayerType::Particle3D`（ID 31）を追加する。既存数値IDは変更しない。
- `ArtifactParticleLayer`はcomposition-space 2Dを正規契約とし、`ArtifactParticle3DLayer`は同じsimulation／Diligent ParticleRendererを共有しつつ、3D camera／depth対象であることを型と保存形式で明示する。
- 作成メニューを「2D パーティクル」「3D パーティクル」に分ける。通常Propertyから`layer.is3D`を除き、モード切替による曖昧な状態を作らない。
- 旧`LayerType::Particle`で`is3D=true`のJSONは読み込み時に`Particle3D`へ移行する。2Dと3Dの保存後はそれぞれ`is3D=false/true`へ固定する。
- GPU PSO、resource、shader、同期は追加せず、D3D12／Vulkan共通の既存Diligent particle draw経路を共有する。
- build／runtimeでの新規作成、旧JSON移行、camera/depth表示確認は未実施。
- `Artifact/src/Layer/ArtifactFormParticleLayer.cppm` — FormParticleLayer 実装

## 進捗 (2026-08-29)

### P4-1 着手 — `ArtifactParticleLayer::Impl` に `setIs3D(true)` 追加

- 実施内容: `Artifact/src/Layer/ArtifactParticleLayer.cppm:441` の `ArtifactParticleLayer()` コンストラクタで `setIs3D(true)` を `createParticleSystem()` 直前に呼出 (1 行追加)。
- 既存 `ArtifactFormParticleLayer` は `Grid3D` preset 選択時に `syncSourceSize` 経由で `setIs3D(true)` が既に呼ばれていた (`ArtifactFormParticleLayer.cppm:514`)、`applyPropertiesFromJson` 復元経路も `syncSourceSize` を呼ぶ (line 1173) ため完了済み。
- JSON default 戦略: 新規作成 Particle は `is3D=true` (コンストラクタの `setIs3D(true)` が生きる)、既存 particle JSON は `obj["is3D"]` が保存されている場合のみ復元 (`ArtifactAbstractLayer.cppm:5194-5195`)、保存されていない場合は `is3D=true` のまま。`ArtifactAbstractLayer::toJson` (line 4533) で `obj["is3D"] = is3D()` が必ず書き込まれるため、1 度保存したプロジェクトは `is3D` が永続化される。**既存 particle プロジェクトは `is3D=false` で保存されている可能性が高く、復元時は `is3D=false` のまま** → 表示は回帰しない。
- Composition 側 (`has3DCamera` 判定): 触らない。`has3DCamera` は `activeCamera` (CameraLayer) 存在時のみ true になる設計 (`CompositionRenderController.cppm:32874`) で、Particle は consumer 側。3D camera 行列配信の `if (layer->is3D())` bundle gate (line 8913) は `is3D()` ベースなので、Particle の `is3D()=true` で自動的に gate を通過。
- 未検証 (AGENTS.md によりビルド・runtime はユーザー指示待ち):
  - 新規 Particle Layer を composition に追加 → 3D camera orbit で billboard が立体的に動くか
  - 既存 particle JSON (is3D=false 永続化) を開いたとき、表示が 2D 経路のまま無回帰か
  - AOV (emission / normal) gate (line 11613/11642) を Particle が pass するか
  - Property Widget からの `is3D` 切替 UI は未実装 (追加する場合は `getLayerPropertyGroups` 露出設計を別途設計レビュー)
- 次着手: P4-2 (`transformParticleRenderData` の 2D/3D 分岐 + `ParticleRenderData.modelMatrix` + `ParticleRenderer::setModelMatrix`)。`ArtifactCore::ParticleRenderData` は ABI 共有のため全利用箇所 (ArtifactCore/src/Graphics/ParticleRenderer.cppm を含む) を grep で確認してから着手。


### P4-2 着手 (2026-08-30) — ArtifactParticleLayer::draw() で 3D 時に 2D QTransform 適用を抑止

- 観察: 3D camera 行列は CompositionRenderController の if (layer->is3D()) gate (line 8913) 経由で set3DCameraMatrices() が呼ばれ、particle3DCameraActive_ が true になる (ArtifactIRenderer.cppm:1285)。ArtifactParticle3DLayer は ArtifactParticleLayer を継承し draw() をオーバーライドしないため、親 draw() (ArtifactParticleLayer.cppm:441) がそのまま実行される。L465 で QTransform globalTransform = getGlobalTransform() (2D) を取得し、L472 で 	ransformParticleRenderData(lodData, globalTransform, opacity()) を呼ぶ。この関数は L187-L196 で QTransform により src.px/src.py を 2D map し、L193-L196 で v.px/v.py/v.vx/v.vy を 2D 値で上書きする。v.pz/v.vz は L171/L174 で保持されるが、layer 自身の world 位置が 2D 扱いになり 3D camera orbit に追従しない。
- 対応: ArtifactParticleLayer.cppm:470-484 で is3D() による分岐を追加し、3D パーティクルは transformParticleRenderData をスキップして lodData をそのまま drawParticles に渡す。2D パーティクルは従来通り transformParticleRenderData で 2D QTransform を適用。AGENTS.md 2026-08-15「D3D12 / Diligent backend 触るときは慎重」「QImage の本流投入禁止」「QPainter::CompositionMode による合成実装禁止」を守り、.cppm のみの変更、.ixx 宣言追加なし、CMake 変更なし、module import 追加なし。
- 未検証 (AGENTS.md に従いビルド・runtime はユーザー指示待ち): (1) 3D camera orbit 時に Particle billboard が立体的に動くか (2) 既存 2D particle プロジェクトの表示が回帰しないか (3) AOV (emission / normal) gate (line 11613/11642) を Particle が pass するか。
- 次着手: P4-3 (model matrix 経路) — set3DCameraMatrices は view/proj のみ受け、model matrix を含まない。getGlobalTransform4x4() から取った model を ParticleRenderData に追加し、ParticleRenderer::setModelMatrix API を新設する。これは ArtifactCore::ParticleRenderData の ABI 共有を伴うため、grep で全利用箇所を確認してから着手。

### P4-3 実装 (2026-09-05) — model matrix をGPU particle drawへ接続

- `ParticleRenderData` に identity 初期値の row-major `modelMatrix` を追加し、`ArtifactParticle3DLayer` は `getGlobalTransform4x4()` を row-major で設定する。2D Particle は従来どおり canvas-space座標とidentity model matrixを使う。
- `DiligentImmediateSubmitter` は model / view / projection を `ParticleRenderer` に渡し、vertex shaderとGPU cull shaderはいずれも model → view → projection の順で座標を処理する。D3D12/Vulkan固有APIは追加していない。
- 3D経路で layer opacity もGPU upload前のvertex alphaへ明示適用した。
- **未検証:** build、3D layer の position / rotation / scale、camera orbit、GPU cull有効時、2D Particle回帰、D3D12/Vulkan parity は未実行（ユーザー許可待ち）。

### P4-3 追補 (2026-09-05) — Form Particle のGrid3Dへモデル行列を接続

- `ArtifactFormParticleLayer::draw()` は Grid3D 時だけ `getGlobalTransform4x4()` を row-major の `ParticleRenderData::modelMatrix` へ設定する。Grid2D は identity を明示し、従来の2D座標契約を保持する。
- Form Particle は既存の Diligent particle submitter / shader を共有するため、GPU resource、PSO、同期経路は追加しない。
- **未検証:** build、Grid3D のposition / rotation / scale、camera orbit、GPU cull有効時、Grid2D回帰、D3D12/Vulkan parity は未実行（ユーザー方針によりビルドは実施しない）。
