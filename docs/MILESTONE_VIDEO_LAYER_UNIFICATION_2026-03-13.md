# Video Layer Unification Milestone

Date: 2026-03-13
**最終更新:** 2026-08-15

## Update 2026-08-15

- 現行コードを確認し、M-VIDEO-1〜5 の production code 上の統合状態を再監査。
- `LayerType::Media`、`ArtifactMediaLayer`、`Artifact.Layer.Media` の残存参照は確認されず、factory/import inference は `Video` / `Audio` に統一済み。
- `ArtifactVideoLayer::toJson()` は数値の `LayerType::Video` と `video.*` property を保存し、factory は `Video` / `VideoLayer` の旧形式を復元可能。timeline / inspector / Project View も `ArtifactVideoLayer` を共通経路として扱っている。
- 今回は動画機能の拡張は行わない。デコーダーや render backend の受入れは本マイルストーンの非ゴールかつ、静止画・連番・シェイプ基盤を優先する方針のため未検証のまま保留。

## Goal

- Remove the split between `Video` and `Media`.
- Keep a single runtime implementation for imported footage in `ArtifactVideoLayer`.
- Use `Video` as the only public layer name for new creation, import, serialization, and property editing.
- Remove `ArtifactMediaLayer` and `LayerType::Media` instead of preserving compatibility shims.

## Scope

- `Artifact/src/Layer`
- `Artifact/include/Layer`
- `Artifact/src/Project`
- `Artifact/src/Service`
- `Artifact/src/Widgets/Timeline`
- `Artifact/src/Widgets/PropertyEditor`
- `Artifact/src/Project/ArtifactProjectModel.cppm`

## Non-Goals

- Decoder backend rewrites
- A new dedicated audio-only layer class
- Render backend feature additions
- Full Asset Browser UI redesign
- Effect pipeline changes

## Milestones

### M-VIDEO-1 Type And Factory Collapse

- Remove `LayerType::Media`.
- Route footage creation through `LayerType::Video` and `LayerType::Audio` only.
- Keep `Audio` implemented by `ArtifactVideoLayer` with `hasVideo(false)`.
- Make import inference return `Video` instead of `Media`.

### M-VIDEO-2 Serialization Naming Cleanup

- Save footage layers as `VideoLayer`.
- Remove `MediaLayer` naming from new output.
- Keep one JSON implementation in `ArtifactVideoLayer`.

### M-VIDEO-3 Property And Inspector Cleanup

- Use `Video` as the property group label.
- Use `video.*` as the only property prefix.
- Keep source replacement and inspector editing aligned to `ArtifactVideoLayer`.

### M-VIDEO-4 Project And Timeline Alignment

- Show imported moving footage as `Video` in Project View.
- Ensure timeline layer creation and duplication stay on `Video`.
- Keep audio-only footage using the same runtime implementation.

### M-VIDEO-5 Redundant Type Removal

- Delete `ArtifactMediaLayer`.
- Remove `Artifact.Layer.Media` module references.
- Remove `Media`-specific backlog and milestone references.

## Recommended Order

1. `M-VIDEO-1 Type And Factory Collapse`
2. `M-VIDEO-2 Serialization Naming Cleanup`
3. `M-VIDEO-3 Property And Inspector Cleanup`
4. `M-VIDEO-4 Project And Timeline Alignment`
5. `M-VIDEO-5 Redundant Type Removal`

## Current Risks

- Some notes or comments may still mention `Media` as an older concept.
- Audio-only footage still uses `ArtifactVideoLayer`, which is intentional.

## Validation Checklist

- New video import creates `LayerType::Video`.
- New audio import creates `LayerType::Audio` backed by `ArtifactVideoLayer`.
- Saved footage layers emit `VideoLayer`.
- Inspector exposes `Video` properties through `video.*`.
- `ArtifactMediaLayer` and `LayerType::Media` are absent from production code.
