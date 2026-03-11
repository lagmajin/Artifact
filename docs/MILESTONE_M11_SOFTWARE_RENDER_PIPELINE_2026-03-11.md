# M11 Software Render Pipeline (2026-03-11)

`コンポ作成 -> 平面追加 -> Software Composition Test で確認 -> エフェクト適用 -> 静止画シーケンス書き出し`
を、ひとつの制作パスとして成立させるためのマイルストーン。

## Goal

- 新規コンポジションを作れる
- 平面レイヤーを追加すると current composition に即反映される
- `Software Composition Test` で現在コンポの見た目を確認できる
- 最低 1 系統の見た目変化をエフェクトとして確認できる
- Render Queue から静止画シーケンスを書き出せる

## Definition Of Done

- `Solid Layer` を追加すると software preview 上に色付き矩形として出る
- current composition の切替に test window が追従する
- `GaussianBlur` または `EdgeOverlay` のような簡易 effect がプレビューで確認できる
- `PNG sequence` などでワークエリア範囲の連番が出力できる
- 先頭フレームと末尾フレームが期待範囲と一致する

## Work Packages

### 1. Solid Layer Preview Fidelity
- `Software Composition Test` が平面レイヤーをカード表示ではなく実色に近い矩形で出す
- コンポサイズとレイヤーサイズを反映する
- current composition 追従時に再描画が崩れない

### 2. Software Test Entry Path
- コンポ作成直後に software composition test へ入る導線を用意する
- 平面 1 枚を追加した確認用の初期化アクションを用意する

### 3. Effect Validation Path
- software preview 上で effect を切り替えて見た目差分を確認できる
- 少なくとも 1 系統は current composition ベースで確認できる

### 4. Image Sequence Rendering
- Render Queue から静止画シーケンスを出力できる
- 出力範囲は `workAreaRange` を使う
- 出力失敗時のメッセージを固定する

## Current Status

- `2026-03-11`
  - current composition 追従付きの `Software Composition Test` / `Software Layer Test` は導入済み
  - 初手として `Solid Layer Preview Fidelity` に着手

## Suggested Order

1. `Solid Layer Preview Fidelity`
2. `Software Test Entry Path`
3. `Effect Validation Path`
4. `Image Sequence Rendering`
