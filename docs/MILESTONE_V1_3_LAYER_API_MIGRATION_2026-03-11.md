# Artifact v1.3 Layer API Migration (2026-03-11)

v1.3 は、Layer 関連 API の新旧混在（例: `BlendMode` と `LAYER_BLEND_TYPE`）を
計画的に整理し、互換性を保ちながら段階移行するためのフェーズ。

## Goal

1. 新API（`BlendMode`）と旧API（`LAYER_BLEND_TYPE`）の互換ルールを明文化
2. ビルド破壊を起こさない互換レイヤーを先に固定
3. UI/Service/Render の型利用を一本化
4. 移行完了後に旧APIを安全に削除できる状態にする

## Definition Of Done

- 主要 call-site で型の混在によるコンパイルエラーが発生しない
- 互換層（変換関数・deprecated 導線）が docs とコードで一致
- 最低1系統（Layer Blend）で移行完了手順が確立

## Work Packages

### 1. Compatibility Layer Baseline
- `Layer.Blend` に新旧互換型/変換関数を置く
- 既存UIコードを壊さない最低ラインを確保

### 2. Service Boundary Unification
- Service で扱う型を新APIに寄せる
- UI入力は必要に応じて互換層で変換

### 3. Render Path Harmonization
- Render 側の blend 参照を新APIへ統一
- 旧型受け口は互換層で吸収

### 4. Deprecation Plan
- 旧API利用箇所の棚卸し
- 段階削除スケジュールをロードマップへ反映

## Suggested Order

1. `Compatibility Layer Baseline`
2. `Service Boundary Unification`
3. `Render Path Harmonization`
4. `Deprecation Plan`
