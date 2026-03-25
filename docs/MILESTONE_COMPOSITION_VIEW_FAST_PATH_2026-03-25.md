# Composition View Fast Path (2026-03-25)

`Composition Viewer` は viewport 操作中に重くなりやすい。
現在は interaction 中の downsample と描画間引きを入れてあるが、
根本は `camera 操作だけでも重い全再描画 path` に入る構造にある。

## Goal

- pan / zoom / wheel 中の追従感を改善する
- interaction 時の render cost を「通常描画」と分離する
- 画質を落としても editor として破綻しない fast path を持つ

## Definition Of Done

- pan / zoom 中に毎イベントのフル再合成へ戻らない
- interaction 停止後に full quality frame へ復帰する
- CPU raster layer / upload-heavy layer が多くても最低限の追従感を保つ
- fast path の条件と制限が docs で説明できる

## Work Packages

### 1. Interaction State Formalization

対象:

- `ArtifactCompositionEditor`
- `CompositionRenderController`

内容:

- 操作中フラグ、終了タイマー、間引き条件を formalize
- wheel / pan / gizmo / scrub のどれが fast path 対象かを決める

完了条件:

- interaction state の入口が 1 箇所で追える

### 2. Camera-Only Fast Path

対象:

- `CompositionRenderController`
- render pipeline

内容:

- レイヤー内容が変わっていない時は、既存合成結果の再利用を検討
- まずは overlay-less / gizmo-less でもよいので fast path を定義

完了条件:

- pan/zoom だけで全レイヤー再描画しない経路がある

### 3. Surface Upload Reduction

対象:

- `drawLayerForCompositionView()`
- `PrimitiveRenderer2D`

内容:

- `QImage -> texture` 再生成の回数を減らす
- static image / text / cached frame の扱いを改善

完了条件:

- interaction 中の upload-heavy cost が減る

### 4. Flush / Present Pacing Review

対象:

- `CompositionRenderController`
- `ArtifactIRenderer`

内容:

- 毎フレーム `flush/present` の必要性を整理
- interaction 中だけでも pacing を見直す

完了条件:

- GPU/CPU 同期が体感の主因かどうか切り分けできる

### 5. Perf Diagnostics

対象:

- `CompositionRenderController`
- docs/bugs

内容:

- `surfaceUploadLayers`, `cpuRasterLayers`, `flushMs`, `presentMs` などの見方を固定
- 再現条件をメモ化

完了条件:

- 他の AI でも perf log を読んでボトルネックを判断しやすい

## Recommended Order

1. `Interaction State Formalization`
2. `Perf Diagnostics`
3. `Camera-Only Fast Path`
4. `Surface Upload Reduction`
5. `Flush / Present Pacing Review`

## Notes

2026-03-25 時点では、interaction 中の

- `16ms` スケジューリング
- `effectivePreviewDownsample = max(previewDownsample, 2)`

を導入済み。これは fast path の入口であり、根治ではない。
