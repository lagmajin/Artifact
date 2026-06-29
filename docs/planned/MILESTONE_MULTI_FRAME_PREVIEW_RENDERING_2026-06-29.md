# Milestone: Multi-Frame Preview Rendering

**Date:** 2026-06-29  
**Status:** Planned  
**Primary goal:** 同一 Diligent device / immediate context のままフレーム別 render target を持ち、RAM Preview の複数フレームをパイプライン処理できるようにする

## Goal

重いコンポジションでプレビュー生成が1フレームずつ完了待ちになる状態を改善する。

最初の到達点は、GPU context の複製や複数スレッドからの同時操作ではない。

1. CPU preparation を複数フレーム並行で進める
2. GPU command submission は単一 immediate context 上で直列化する
3. frame ごとに color/depth render target と transient state を分離する
4. readback 完了を待たず次の frame を投入する
5. 完成した final composition frame を既存 RAM Preview cache へ渡す

これにより、CPU preparation、GPU render、readback、cache 格納を重ねて進行させる。

## Current State

### Preview render is single-frame serialized

- `ArtifactCompositionRenderWidget` は単一 `tbb::task_group` の render loop を持つ
- `renderMutex_` の内側で `renderOneFrame()` を完了まで実行する
- `CompositionRenderController` は `renderInProgress_` で concurrent render を抑止する
- interactive viewport の描画と RAM Preview build の ownership が完全には分離されていない

### RAM Preview has scheduling state, but not a multi-frame renderer

- `ArtifactPlaybackService` には range、priority、generation、pending frame queue がある
- queue は frame readiness の管理を行うが、複数 frame の render job を同時に進める worker pool ではない
- final frame cache と disk writer の土台は既にある

### Renderer already has reusable groundwork

- `ArtifactIRenderer` は offscreen render target の作成、push/pop、clear を持つ
- synchronous color readback は 2-slot ring を持つ
- asynchronous color readback は 3-slot ring と fence を持つ
- `RenderContext` は `currentFrame`、quality、resolution、ROI、purpose に相当する状態を既に持つ

このため、最初から renderer/device を複製せず、既存 offscreen/readback 機構を frame slot に結び付ける。

## Phase 0 Audit Notes (2026-06-29)

### Confirmed serialization points

- `ArtifactCompositionRenderWidget` は `renderMutex_` の内側で1フレーム全体を処理する
- `CompositionRenderController` は `renderInProgress_` で再入を禁止する
- renderer は単一 `m_layerRT`、単一 layer depth target、単一 immediate context を中心に状態を持つ
- controller は単一 `RenderPipeline` を持ち、その内部の accum/temp/layer/layer-float target をフレーム間で共有する
- composition sampling の一部は共有 `framePosition` の変更と復元に依存する

### Confirmed reusable pipeline pieces

- synchronous readback ring: 2 slots
- asynchronous readback ring: 3 slots
- async readback は copy、fence signal、flush まで submit thread で行い、fence wait と pixel conversion を worker 側へ送る
- RAM Preview queue は generation と pending frame order を既に持つ
- disk persistence は専用 writer thread を既に持つ

### First correctness gaps

- async readback completion が worker thread から `ArtifactPlaybackService` の RAM cache state を直接変更していた
- readback開始時に capture した composition ID が保存前に検証されていなかった
- build generation は診断文字列へ出るだけで、readback completion の accept/reject 判定に使われていなかった
- async readback worker の `MapTextureSubresource` / `UnmapTextureSubresource` が immediate context thread ownership と整合するかは追加確認が必要
- 現行 `createOffscreenTexture()` は color RTV/SRV だけを作り、frame slot に必要な depth と pipeline scratch 一式は所有しない
- `pushRenderTarget()` は任意RTVをbindできるが、`clearRenderTarget()` と `popRenderTarget()` は単一 `m_layerRT` 前提のため、そのまま frame slot APIには使えない

### Phase 2 resource boundary

Phase 2で複製する単位は最終 color RT 1枚だけではない。

最小でも次を `PreviewFrameRenderSlot` の所有境界へ入れる。

- final preview color target
- depth target
- `RenderPipeline` の accum/temp/layer/layer-float targets
- readback staging slot association
- frame-local constants / camera-time snapshot

既存 group-layer 用 `createOffscreenTexture()` を preview frame slot に流用して責務を混ぜない。

### Phase 1 first slice applied

- async readback result の publish を `ArtifactPlaybackService` owner thread へ queued invoke する
- captured composition ID と current composition ID が一致する場合だけ保存する
- captured build generation と current generation が一致する場合だけ保存する
- completion時点でも対象 frame が pending build であることを確認する
- PlaybackEngine の queued frame publish も composition ID 不一致なら破棄する
- disk writer completion は task の composition ID が現在値と一致する場合だけ cache state を更新する

この slice は frame別RTをまだ追加しない。multi-frame化の前に stale result と cross-thread cache mutation を止めるための土台とする。

## Core Design

### 1. Preview Frame Request

各 job は共有 composition の可変 `framePosition` に依存せず、要求時点の識別情報を持つ。

```cpp
struct PreviewFrameRequest {
    int64_t frame = 0;
    uint64_t generation = 0;
    uint64_t compositionRevision = 0;
    RenderContext renderContext;
};
```

最低限の invalidation key:

- composition identity
- composition revision
- frame
- preview quality / resolution scale
- backend / output format
- ROI policy

結果受領時に generation または revision が一致しなければ cache へ入れず破棄する。

### 2. Frame Render Slot

最初は 2-slot の bounded ring とする。

```cpp
enum class PreviewFrameSlotState {
    Free,
    Preparing,
    ReadyToSubmit,
    Submitted,
    ReadbackPending,
    Completed,
    Failed
};

struct PreviewFrameRenderSlot {
    PreviewFrameRequest request;
    PreviewFrameSlotState state = PreviewFrameSlotState::Free;

    // Conceptual handles. Concrete ownership stays inside the renderer.
    void* colorRenderTarget = nullptr;
    void* depthRenderTarget = nullptr;
    uint64_t fenceValue = 0;
};
```

frame slot ごとに分離するもの:

- color render target
- depth target
- frame constants
- frame-local transient textures
- effect scratch resources
- readback staging association
- fence / completion state

共有してよいもの:

- Diligent device
- immediate context
- immutable pipeline state
- immutable shader/resource metadata
- revision が保証された source / texture cache

### 3. Single Submit Lane

同じ immediate context を複数 worker thread から同時操作しない。

```text
CPU prepare N ───────┐
CPU prepare N+1 ──┐  │
                  v  v
             single GPU submit lane
                  │
          RT slot A / RT slot B
                  │
         async readback / fence
                  │
          RAM Preview cache
```

GPU submit lane は次を直列化する。

- render target bind/unbind
- resource transition
- command recording/submission
- fence signal
- readback copy request

並行性は「context の同時操作」ではなく、CPU preparation、GPU execution、readback wait/conversion の overlap から得る。

### 4. Priority Policy

既存 RAM Preview priority を再利用する。

優先順:

1. interactive viewport が現在必要とする frame
2. playback direction の直近 frame
3. work area 内の前方 frame
4. 後方または遠方 frame

interactive viewport は RAM Preview build によって待たされてはならない。必要なら build job の submit 前キャンセル、または slot の一時返却を行う。

### 5. Temporal Capability

layer/effect を少なくとも次の3分類で扱う。

| Capability | Behavior |
|---|---|
| Stateless | 任意 frame を独立して並行 preparation 可能 |
| Snapshot-capable | checkpoint/snapshot から区間を開始可能 |
| Sequential-only | 前 frame 依存を保ち、区間内を順次処理 |

particle、simulation、previous-frame effect、motion accumulation を stateless と推測して並列化しない。

最初の slice では sequential-only を単一 lane に落とし、見た目の正しさを優先する。

## Implementation Phases

### Phase 0: Measurement and Context Audit

- frame cost を `decode / prepare / effect / GPU submit / GPU wait / readback / cache store` に分解
- `ArtifactIRenderer` 内の immediate context 使用箇所を監査
- async readback worker からの `MapTextureSubresource` / `UnmapTextureSubresource` が backend contract 上安全か確認
- shared mutable state を一覧化:
  - composition frame position
  - renderer frame constants
  - camera/time state
  - effect scratch textures
  - layer caches
- current heavy-frame reason を diagnostics へ出せる状態にする

**Exit criteria**

- single-frame stall の内訳が計測できる
- frame slot へ分離すべき resource が列挙されている
- immediate context の owner thread が明確である

### Phase 1: Immutable Request and Cancellation

- `PreviewFrameRequest` 相当の内部 request を追加
- composition revision と queue generation を結果まで伝播
- edit、seek、quality change、composition switch で古い request を cancel
- shared composition の一時的な `setFramePosition()` を background job contract から外す
- 新しい global signal/slot は追加せず、既存 EventBus / service path を使う

**Exit criteria**

- stale frame が RAM Preview cache に入らない
- cancellation reason を diagnostics で確認できる

### Phase 2: Two-Slot Offscreen Render Targets

- renderer 内部に preview frame slot を2つ追加
- slot ごとに color/depth target を所有
- slot acquire / submit / retire / release を renderer 内部 API として閉じる
- resize、quality、format change 時は in-flight 完了後に安全に再作成
- swap chain target と preview build target を明確に分離
- first slice では immediate context submission を1本に限定

**Exit criteria**

- frame N と N+1 が別 RT を使用する
- N の readback wait 中に N+1 の submit 準備へ進める
- viewport back buffer を preview cache build が上書きしない

Current implementation note (2026-06-29):

- `CompositionRenderController` の GPU blend scratch path は 2-slot `RenderPipeline` rotation に移行した
- これにより frame N と N+1 は別 accum/temp/layer/layer-float target を使う
- RAM Preview async readback は GPU blend path で `finalPresentSRV` を明示指定して読むようになった
- preview slot ごとの offscreen depth target を追加し、GPU pipeline path では 3D draw / clear depth も slot 側へ向けるようにした
- まだ `ArtifactIRenderer` の preview-build 専用 color/depth slot と back buffer 完全分離は未着手

### Phase 3: CPU Preparation Workers

- bounded worker count を導入し、初期値は2
- decode / stateless CPU effect / render item preparation を frame request 単位で進める
- prepared result は single submit lane へ渡す
- queue pressure 時は遠方 frame を増やさず、現在 frame 近傍を優先
- nested TBB oversubscription を計測し、worker 数を固定上限にする

**Exit criteria**

- CPU preparation N+1 が GPU render N と overlap する
- worker が UI thread と immediate context を直接操作しない

### Phase 4: Async Readback and RAM Preview Integration

- frame slot と既存 async readback ring の対応を明示
- readback result は `ImageF32x4_RGBA` を authoritative cache representation とする
- 新しい QImage ベースの本流を追加しない
- QImage が必要な互換境界は明示変換に限定
- completion callback は generation/revision を再検証してから cache へ格納
- RAM Preview frame state を `Submitted / ReadbackPending / Ready / Failed` と診断可能にする

**Exit criteria**

- GPU fence wait が render submit lane を恒常的に止めない
- completed frame のみ timeline cache bar で ready になる
- canceled/stale frame が表示されない

### Phase 5: Temporal Layer Policy

- layer/effect capability を内部 policy として定義
- stateless path から適用開始
- snapshot-capable simulation の checkpoint contract を別 milestone へ切り出せる形にする
- sequential-only が混在する composition は安全な single-frame fallback を使う
- fallback reason を diagnostics に表示

**Exit criteria**

- temporal effect の結果が frame scheduling order に依存して壊れない
- multi-frame eligible / fallback の理由を説明できる

### Phase 6: Adaptive In-Flight Policy

- default 2 slots
- resolution、format、VRAM budget、frame cost により 1～3 slots の範囲で調整
- interactive input 中は current-frame latency を優先して in-flight を絞る
- intentional RAM Preview build 中は throughput を優先
- slot memory cost と readback pressure を diagnostics に表示

**Exit criteria**

- VRAM pressure で無制限に RT が増えない
- interaction latency と preview fill throughput を policy で切り替えられる

## Initial Scope

最初の実装対象:

- `Artifact/src/Service/ArtifactPlaybackService.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- 必要最小限の既存 `.ixx` API

可能な限り実装を既存 `.cppm` に閉じる。公開型が不要なら `.ixx` を変更しない。

## Non-Goals

- Diligent device の複製
- immediate context の無条件な multi-thread access
- 複数 GPU 対応
- render queue / final export の同時全面移行
- temporal simulation の強制的な random access 化
- Qt `QPainter` composition への fallback 追加
- 新規 `QImage` 主体 render path
- Diligent fork / gitlink の変更

## Risks and Guardrails

### Shared State Corruption

RTだけ分けても frame constants、camera/time、effect scratch texture が共有なら結果は壊れる。

対策:

- frame-local state の明示
- immutable request
- single submit lane
- revision validation

### Immediate Context Thread Safety

fence wait を background に移しても、Map/Unmap を同じ immediate context へ複数 thread から行う設計は backend contract の確認が必要。

対策:

- Phase 0 で現状 async readback を監査
- 必要なら context operation 自体は owner lane に戻し、pixel conversion のみ worker 化
- Diligent backend を推測で変更しない

### VRAM Growth

RGBA16F 1920x1080 color target は1枚約16 MiB。depth、mask、effect scratch、staging を含む実コストはさらに大きい。

対策:

- 2-slot から開始
- bounded transient pool
- format/resolution change 時の明示 retire
- VRAM pressure で single-slot fallback

### CPU Oversubscription

frame-level worker と effect 内部 TBB が重なると、CPU使用率だけ増えて latency が悪化する。

対策:

- bounded frame worker
- nested parallelism の計測
- interactive 時の worker 抑制

## Diagnostics

最低限表示する項目:

- requested frame / generation / composition revision
- queue depth
- active CPU preparation count
- frame slot state
- current RT slot
- GPU submit time
- fence wait time
- readback time
- cache store time
- stale/canceled count
- multi-frame eligibility / fallback reason
- estimated slot VRAM

## Completion Criteria

- 同一 device / immediate context のまま2つの frame RT slot が動作する
- GPU submission は明確に直列化されている
- CPU preparation と GPU render が frame 間で overlap する
- readback wait 中にも次 frame の preparation/submit が進む
- interactive current frame が background preview build より優先される
- edit/seek 後に stale frame が表示されない
- temporal layer を含む場合に安全な fallback が働く
- RAM Preview ready state が final completed frame のみを示す
- single-slot 比較で preview range fill time が改善する
- UI responsiveness が悪化しない

## Related Milestones

- `Artifact/docs/MILESTONE_RAM_PREVIEW_SYSTEM_2026-05-01.md`
- `Artifact/docs/MILESTONE_PREVIEW_FREEZE_STOP_RESPONSIVENESS_2026-06-05.md`
- `docs/planned/MILESTONE_HIERARCHICAL_CACHE_SYSTEM_2026-05-16.md`
- `docs/planned/MILESTONE_DISK_CACHE_SYSTEM_2026-03-26.md`

## Recommended First Implementation Slice

Phase 0 と Phase 1 を先に行う。

その後、既存 `ArtifactIRenderer` の offscreen RT と async readback ring を利用し、次の限定条件で Phase 2 の proof of concept を作る。

- RAM Preview build のみ
- stateless composition のみ
- 2 frame slots
- 1 immediate context submit lane
- color output のみ
- current frame first
- generation mismatch は即破棄

この proof of concept で throughput と interaction latency の両方を計測してから、depth、multi-channel、temporal layer へ拡張する。
