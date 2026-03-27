# Composition Editor Cache System (2026-03-26)

`ArtifactCompositionEditor` は、単純な `Image` + `Solid` 構成でも重くなりやすい。
主因は `CompositionRenderController` が同一フレームでも再合成しやすく、
さらに `mask` / `Rasterizer effect` 付きレイヤーで CPU 側の `QImage` 加工を
毎回やり直している点にある。

## Goal

- `Composition Viewer` の静止時再描画コストを大きく下げる
- 単純な 2D レイヤー構成では `CS blend` の利益が薄い場面を自動で回避する
- `layer changed` を起点に最小限の invalidation で再利用できる構造を作る
- 将来の `RAM Preview` / `partial recompose` に繋がる cache 境界を固定する

## Definition Of Done

- 同一フレーム・同一 view 状態での無駄な full composite が抑制される
- `image/video/text + mask/rasterizer effect` の CPU surface 再生成が減る
- `CS blend` を有効化しても単純ケースで極端に重くならない
- cache invalidation の入口が `layer changed` / `composition changed` で追える
- perf log を見て cache hit 前後の差を判断しやすい

## Work Packages

### 1. Surface Cache

対象:

- `CompositionRenderController`
- `drawLayerForCompositionView()`

内容:

- `image` / `video` / `text` の加工済み surface を layer 単位で再利用
- `mask` / `Rasterizer effect` のあるレイヤーだけ対象にする
- `layer changed` で該当レイヤー cache を invalidate

完了条件:

- 同一 frame での CPU surface 再加工が減る

状態:

- 2026-03-26 時点で初版実装済み

### 2. Redundant Composite Suppression

対象:

- `CompositionRenderController::renderOneFrameImpl()`

内容:

- frame / zoom / pan / previewDownsample / show flags / selection / invalidation serial
  から render key を作る
- 同じ render key なら full composite をスキップ

完了条件:

- 同一状態の重複 redraw で再合成しない

状態:

- 2026-03-26 時点で初版実装済み

### 3. GPU Texture Cache Hardening

対象:

- `PrimitiveRenderer2D`
- upload-heavy layer path

内容:

- `QImage -> GPU texture` の寿命管理を強化
- texture cache eviction と stale texture 条件を整理

完了条件:

- static image / text / cached frame の再 upload がさらに減る

状態:

- 2026-03-27 時点で、video layer を GPU texture cache から除外し、毎フレーム churn しやすい経路を外した
- 静止系 layer の再 upload に cache を集中させる方向へ寄せた

### 4. CS Blend Fast Path

対象:

- `CompositionRenderController`
- `LayerBlendPipeline`

内容:

- `Normal` の単純 2D 構成では GPU blend path を bypass
- GPU blend 使用時は preview 解像度の下限を設ける

完了条件:

- `CS blend` ON でも simple scene で極端に重くならない

状態:

- 2026-03-26 時点で partial 実装済み

### 5. Composition Result Cache

対象:

- `CompositionRenderController`
- render pipeline intermediate

内容:

- camera-only change と layer-content change を分離
- final composite または layer-stack intermediate の再利用を検討

完了条件:

- pan / zoom / expose で full layer pass に戻りにくい

### 6. Dirty Layer / Partial Recompose

対象:

- layer invalidation
- composition render path

内容:

- dirty layer 単位で再合成範囲を縮小
- 将来的には dirty region まで拡張

完了条件:

- 大きい comp でも局所変更のコストが comp 全体に比例しない

## Recommended Order

1. `Surface Cache`
2. `Redundant Composite Suppression`
3. `CS Blend Fast Path`
4. `GPU Texture Cache Hardening`
5. `Composition Result Cache`
6. `Dirty Layer / Partial Recompose`

## Notes

2026-03-26 時点の実装では、

- layer 単位の processed surface cache
- 同一 render key の再合成スキップ
- simple scene での GPU blend bypass
- GPU blend 時の preview downsample floor 強化

まで入っている。

ただし、これはまだ `full cache system` ではない。
特に `Composition Result Cache` と `Dirty Layer / Partial Recompose` が
入らない限り、複雑な comp では再合成コストの頭打ちは残る。
