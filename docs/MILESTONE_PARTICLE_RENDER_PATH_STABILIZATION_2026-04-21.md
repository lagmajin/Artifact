# Particle Render Path Stabilization

`ArtifactParticleLayer` の GPU billboard 描画が「時々見えない / 状態依存で壊れる」問題を、render entry・matrix・RTV・PSO 前提の整理で安定化するためのマイルストーン。

## Goal

- particle layer を composition editor 上で安定して見えるようにする
- billboard draw path の前提条件を `ArtifactIRenderer` 側で局所化する
- `ParticleRenderer` の責務と `ArtifactParticleLayer` の責務を分け直す

## Current Understanding

particle 非表示の本命は、billboard shader 単独故障というより次の複合。

- RTV が particle draw 前に unbind される
- view / projection matrix が状態依存
- PSO / SRB / buffer 初期化失敗時のガードが弱い
- `drawParticles()` が target setup と draw を同時に抱える

## Phases

### Phase 1: Draw Entry Audit

- `ArtifactIRenderer::drawParticles()` の前提条件を固定
- pending submit flush / RTV bind / matrix upload / prepare / draw を整理

### Phase 2: Billboard Contract Freeze

- `ScreenAligned` / `ViewPlane` の最小契約を固定
- particle size / blend / depth / sort の意味を renderer 側で明文化

### Phase 3: Layer / Renderer Split

- `ArtifactParticleLayer` は scene / emission / simulation 所有に寄せる
- `ParticleRenderer` は draw 専用に寄せる

### Phase 4: Diagnostics

- `Trace` / `Frame Debug` / `Pipeline View` から particle draw path を追えるようにする
- particle draw skipped / no RTV / PSO null / empty buffer を summary で読めるようにする

## Completion Criteria

- particle layer が composition editor 上で安定表示される
- draw failure の原因がログと summary で追える
- `drawParticles()` の責務が renderer internal helper に分かれる

## Related

- `docs/bugs/BUG_PARTICLE_GPU_RENDER_2026-04-19.md`
- `docs/bugs/PARTICLE_BILLBOARD_ROOT_CAUSE_FIX_2026-03-27.md`
- `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_PHASE2_EXECUTION_2026-04-21.md`
- `Artifact/docs/MILESTONE_PARTICLE_LAYER_3D_MIGRATION_2026-03-25.md`
