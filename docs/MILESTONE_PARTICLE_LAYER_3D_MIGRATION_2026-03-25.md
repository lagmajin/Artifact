# ParticleLayer 3D Migration Milestone (2026-03-25)

`ParticleLayer` は現状すでに「画像としては描画できる」が、実体は
`ParticleSystem -> software frame (QImage) -> layer sprite draw`
の 2D 合成経路に寄っている。

この文書は、`ParticleLayer` を壊さずに 3D 対応へ段階移行するための実装計画である。
複数の AI が並行で作業しても衝突しにくいように、責務境界、段階、完了条件を固定する。

## Current Facts

コード上で確認できる現状は次の通り。

- `ArtifactParticleLayer::draw()` はフレームごとに `QImage` をキャッシュし、最終的に `renderer->drawSprite(...)` で描く
- `ArtifactParticleLayer::renderFrame()` / `renderToImage()` は software frame 生成前提
- `ParticleRenderSettings` にはすでに `billboardMode / sortMode / depthTest / depthWrite / softParticles` がある
- `ParticleSystem` には software camera API と depth sort を含む 3D 風の CPU render path がある
- `ArtifactCore` 側には `Graphics.ParticleRenderer` と `Graphics.ParticleCompute` があり、GPU path の土台は別に存在する
- しかし layer と composition への統合はまだ `3D particle draw pass` ではなく `2D image layer` 扱い

要するに、`authoring data はある程度 3D 寄り` だが、`layer integration はまだ 2D sprite path` である。

## Goal

- `ParticleLayer` を「2D 画像レイヤー」ではなく「3D particle scene を持つレイヤー」として扱えるようにする
- software path と Diligent path で共通の particle scene data を使う
- editor 上では最初に billboard particle の solid viewport を成立させる
- 既存の project 保存形式と preset authoring を壊さない
- 段階移行中も `QImage fallback` を保持し、作業途中で表示不能にしない

## Non-Goals

- 初期段階での full fluid / volumetric / shadow / GI 完成
- 既存の particle authoring UI を全面作り直すこと
- 初手から compute-only path に一本化すること
- Diligent backend の低レベル全面改修

## Design Principles

### 1. Simulation と Rendering を分ける

`ParticleSystem` の責務は:

- emitter / effector / particle state 更新
- deterministic stepping
- scene snapshot 生成

に寄せる。

`render(QImage)` や `renderGPU(float* vertexBuffer, ...)` のような描画都合 API は、
最終的には renderer 側へ寄せる。

### 2. Layer は「描画方式」ではなく「particle scene の所有者」

`ArtifactParticleLayer` は:

- particle system の所有
- time / cache / serialization
- composition との接続

を担当する。

`QImage を作るレイヤー` から `scene snapshot を供給するレイヤー` へ移行する。

### 3. Software と Diligent は同じ snapshot を食う

3D 移行で最も避けたいのは、

- software は 2D particle 実装
- Diligent は別の particle 実装

になって仕様が分裂すること。

そのため、まず `ParticleRenderData` を共通化し、

- software particle renderer
- Diligent particle renderer

が同じ入力を描く形にする。

### 4. 段階中は fallback を消さない

既存 `QImage` path は、少なくとも `P3-3` までは保持する。

新しい 3D path が未完成でも:

- project を開ける
- particle layer が見える
- export や preview が完全に死なない

状態を維持する。

## Canonical Architecture

最終形の責務は以下を想定する。

- `ParticleSystem`
  シミュレーション本体。emitters / effectors / deterministic update を管理
- `ParticleRenderData`
  フレーム時点の描画用 snapshot。粒子位置、色、サイズ、回転、寿命、velocity、sort key など
- `ParticleRenderContext`
  camera / projection / viewport / blend / depth / billboard mode など描画条件
- `SoftwareParticleRenderer`
  `ParticleRenderData + ParticleRenderContext -> QImage` または software framebuffer
- `DiligentParticleRenderer`
  `ParticleRenderData + ParticleRenderContext -> GPU draw`
- `ArtifactParticleLayer`
  time 管理、serialization、renderer selection、composition 接続

## Milestones

### P3-1 Data Contract Freeze

目的:
描画経路ごとの差分を増やさないために、particle draw の共通データ契約を先に固定する。

実装内容:

- `ParticleRenderData` を定義
- 最低限の per-particle 属性を固定
  - position (float3)
  - velocity (float3)
  - color (float4)
  - size / sizeEnd
  - rotation
  - age / lifetime
  - alive flag
  - emitter id
- `ParticleRenderContext` を定義
  - camera view / projection
  - viewport size
  - billboard mode
  - blend mode
  - depth test / write
  - soft particle params
- `ParticleSystem` から snapshot を作る API を追加

Done:

- software / Diligent どちらも snapshot ベースへ移行可能
- `render(QImage)` 直結の前に共通入力がある

### P3-2 Software 3D Renderer Extraction

目的:
既存の software frame path を `ParticleSystem` から外し、3D renderer として独立させる。

実装内容:

- `updateAndRenderSoftwareFrame()` の中身を段階的に renderer 側へ移す
- depth sort / perspective projection / billboarding を `SoftwareParticleRenderer` に集約
- `ParticleSystem` から QPainter / QImage 依存を減らす

Done:

- `ParticleSystem` が描画 API を知らなくても software preview が成立する
- 現在の見た目を保ったまま renderer 分離が終わる

### P3-3 Layer Integration Split

目的:
`ArtifactParticleLayer` を `QImage-producing layer` から脱却させる。

実装内容:

- `ArtifactParticleLayer` に render mode を導入
  - `SoftwareSpriteFallback`
  - `Software3D`
  - `Diligent3D`
- `draw()` の中で直接 `renderFrame()` を抱え込まず、専用 renderer へ委譲
- frame cache を `image cache` だけでなく `snapshot cache` へ拡張
- backward compatibility のため、fallback mode は残す

Done:

- layer の public 振る舞いは維持
- 内部では描画方式を差し替え可能

### P3-4 Diligent Billboard Path

目的:
まず最も壊れにくい 3D path として、billboard particle を Diligent で表示する。

実装内容:

- `ArtifactCore::ParticleRenderer` を layer 経由で実際に使う
- `ParticleRenderData` から GPU buffer upload を行う
- `ScreenAligned` と `ViewPlane` billboard を先に実装
- blend mode は `Normal / Additive / Alpha` の最小集合から開始
- depth test/write は `ParticleRenderSettings` と連動

Done:

- Composition Viewer 上で 3D billboard particle が見える
- camera 移動で `QImage` 再生成ではなく GPU draw が走る

### P3-5 Composition Integration

目的:
particle layer を composition 全体の中で 3D 要素として扱う。

実装内容:

- composition render path に particle 3D pass を追加
- 2D layer と 3D particle の重なり順ルールを定義
- 最低限の render order を固定
  - background / 2D underlay
  - particle 3D pass
  - gizmo / guides / overlay
- 必要なら particle layer を一旦 offscreen で描き、composition に戻す

Done:

- particle layer が composition editor 上で独立した 3D draw path を持つ
- overlay と干渉しにくい

### P3-6 Camera / Editor Controls

目的:
3D 移行後も authoring が破綻しないように、camera と見え方の制御を整理する。

実装内容:

- particle preview camera を layer camera と editor camera に分離
- orbit / pan / dolly の基礎導線を追加
- billboard mode / sort mode / depth mode の UI 意味を固定

Done:

- particle を「置いたが見えない」状態を減らせる
- 編集者が camera 起因の不具合を切り分けやすい

### P3-7 GPU Simulation Bridge

目的:
描画だけでなく simulation も GPU path へ接続できるようにする。

実装内容:

- `ParticleCompute` と `ParticleSystem` の橋渡し層を追加
- CPU authoritative mode と GPU authoritative mode を分離
- deterministic / export / scrub 時は CPU authoritative を維持
- realtime preview 時のみ GPU simulate を opt-in にする

Done:

- GPU simulation を入れても project save / scrub / export が壊れにくい
- CPU と GPU の役割が混ざらない

### P3-8 Advanced 3D Features

目的:
3D particle として最低限の質感を上げる。

候補:

- velocity aligned billboard
- soft particles
- stretched particles
- trail
- light response
- depth fade
- scene collider integration

Done:

- billboard 以上の表現を段階的に拡張可能

## Recommended Order

1. `P3-1 Data Contract Freeze`
2. `P3-2 Software 3D Renderer Extraction`
3. `P3-3 Layer Integration Split`
4. `P3-4 Diligent Billboard Path`
5. `P3-5 Composition Integration`
6. `P3-6 Camera / Editor Controls`
7. `P3-7 GPU Simulation Bridge`
8. `P3-8 Advanced 3D Features`

## Parallel Workstreams For Multiple AI

複数 AI で進める場合は、以下の分担が衝突しにくい。

### Workstream A: Data / Core

担当:

- `ParticleRenderData`
- `ParticleRenderContext`
- `ParticleSystem` snapshot API
- serialization compatibility

主なファイル:

- `Artifact/include/Generator/ArtifactParticleGenerator.ixx`
- `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- `Artifact/include/Layer/ArtifactParticleLayer.ixx`

### Workstream B: Software Renderer

担当:

- software 3D particle renderer 分離
- CPU billboarding / sort / depth behavior 固定
- reference image 生成

主なファイル:

- `Artifact/src/Generator/ArtifactParticleGenerator.cppm`
- 新規 `SoftwareParticleRenderer` 系

### Workstream C: Diligent Renderer

担当:

- `ArtifactCore::ParticleRenderer` 実運用化
- GPU buffer upload
- billboard PSO / draw path
- depth/blend state 接続

主なファイル:

- `ArtifactCore/include/Graphics/ParticleRenderer.ixx`
- `ArtifactCore/src/Graphics/ParticleRenderer.cppm`
- composition integration 側の呼び出し点

### Workstream D: Composition / Editor Integration

担当:

- `ArtifactParticleLayer::draw()` の委譲整理
- composition render pass 統合
- editor camera / overlay / visibility

主なファイル:

- `Artifact/src/Layer/ArtifactParticleLayer.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

### Workstream E: Validation / Docs

担当:

- parity checklist
- perf checklist
- known limitations
- migration notes

主なファイル:

- `docs/bugs/*`
- `Artifact/docs/*`

## Quality Gates

各段階で最低限これを満たすこと。

### Functional

- project save/load で particle layer が壊れない
- current frame scrub で見え方が破綻しない
- empty / one emitter / many emitters の 3 ケースが見える

### Visual

- software と Diligent で billboard 向きが極端にズレない
- additive / alpha 系の見え方が大きく崩れない
- camera 操作で particle の奥行き感が読める

### Performance

- camera 操作で毎フレーム `QImage` 再生成に戻らない
- static emitter で geometry upload を毎回やり直さない
- particle count 増加時に CPU/GPU のボトルネックが観測できる

## Risks

- `ParticleSystem` に描画責務を残したまま 3D path を足すと、software / GPU / export の三重実装になる
- `ArtifactParticleLayer` が `QImage cache` と `GPU state` を同時に抱えると保守不能になる
- CPU authoritative / GPU authoritative の切り替えを曖昧にすると、scrub と playback で結果が変わる
- composition 全体の 2D / 3D render order を決めずに進むと、表示は出ても editor で破綻する

## First Concrete Tasks

最初の着手候補は次の順がよい。

1. `ParticleRenderData` / `ParticleRenderContext` を新設し、software path がそれを使う下準備を入れる
2. `ArtifactParticleLayer::draw()` から `renderFrame()` 直結を 1 段抽象化する
3. software renderer を reference implementation として固定する
4. Diligent billboard path を `ScreenAligned` 限定でつなぐ

## Exit Criteria

- `ParticleLayer` が `QImage layer` ではなく `3D particle layer` として説明できる
- software / Diligent が同じ snapshot 契約を共有する
- composition editor 上で billboard particle が GPU draw される
- fallback path を保持したまま段階移行できる
