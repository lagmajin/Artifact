# Static Layer GPU Cache (2026-03-26)

`Composition Editor` には部分的な surface cache は入ってきたが、
静止レイヤーの `GPU texture` を長く使い回す専用層はまだない。

この milestone では、`Image` / `Solid` / `Text` / `SVG` / 静止 `Video frame`
のようなレイヤーについて、GPU 上の中間結果を使い回せる構造を定義する。

## Goal

- 静止レイヤーの `QImage -> GPU texture` upload を最小化する
- `mask` / `Rasterizer effect` が変わらない限り GPU texture を再利用する
- `Composition Viewer` の pan / zoom / selection では cache を壊さない
- GPU cache の invalidation 入口を layer state へ固定する

## Scope

- layer surface GPU cache
- GPU texture lifetime / eviction
- stale 条件の formalize
- diagnostics / hit rate / memory budget

## Non-Goals

- 全 effect / 全 layer を初回から対象にすること
- disk cache と同一層にすること
- render queue の最終出力を直接 GPU cache に置き換えること

## Background

現状は、

- `Surface Cache`
  - CPU 側で加工済み surface を layer 単位で再利用
- `PrimitiveRenderer2D`
  - `QImage.cacheKey()` ベースで texture を部分的に reuse

まではあるが、static layer を跨いだ `GPU texture` の明示的な cache policy はない。

そのため、見た目が変わらない静止レイヤーでも、
viewport 再描画や overlay 更新のたびに GPU upload に近い経路へ落ちやすい。

## Proposed Model

- `StaticGpuCacheKey`
  - layer id
  - source cache key
  - effect hash
  - mask hash
  - resolution
  - backend
  - color space / format
- `StaticGpuCacheEntry`
  - GPU texture
  - last used frame
  - byte size
  - source revision
- `StaticGpuCachePolicy`
  - `NeverEvictForSession`
  - `LRU`
  - `Age`
  - `BudgetAware`

## Phases

### Phase 1: Cache Key Formalization

- source / effect / mask / resolution の hash 入力を固定
- 静止レイヤーかどうかを判定する条件を整理

完了条件:

- 同じ静止レイヤーの GPU 再利用可否が deterministic に判断できる

### Phase 2: Upload Avoidance

- `Image` / `Text` / `SVG` / `Solid` の静止状態を優先的に再利用
- `QImage -> GPU texture` 再生成を避ける

完了条件:

- selection / gizmo / overlay 更新で GPU upload が増えない

### Phase 3: Eviction / Budget

- VRAM 予算
- session LRU
- stale entry cleanup

完了条件:

- cache が増え続けない

### Phase 4: Diagnostics / UI

- hit / miss
- texture count
- memory usage
- clear action

完了条件:

- GPU cache の状態を追える

### Phase 5: Static Scene Fast Path

- 静止レイヤーが多い comp では GPU cache hit を前提にする
- `Composition Result Cache` と連動して full redraw を避ける

完了条件:

- 画像 + 平面だけの comp で体感が改善する

## Recommended Order

1. `Phase 1: Cache Key Formalization`
2. `Phase 2: Upload Avoidance`
3. `Phase 3: Eviction / Budget`
4. `Phase 4: Diagnostics / UI`
5. `Phase 5: Static Scene Fast Path`

## Dependencies

- `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_CACHE_SYSTEM_2026-03-26.md`
- `docs/planned/MILESTONE_DISK_CACHE_SYSTEM_2026-03-26.md`

## Notes

この milestone は `surface cache` の置き換えではない。
CPU 側 surface を守りつつ、静止レイヤーだけ GPU 上の中間結果を長く維持する層を足すのが目的。

特に `pan / zoom / selection / gizmo` のたびに upload を繰り返さないことが重要で、
その意味では `Composition Editor Cache` よりもさらに low-level な cache になる。
