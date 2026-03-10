# Artifact v1.1 Rendering Dual Backend (2026-03-11)

v1.1 は、DX12 依存のデバッグ難易度を下げるために
`DiligentEngine` と `Software/OpenCV` の二重バックエンドを実運用可能にする段階。

## Goal

次の 6 項目を満たしたら v1.1 を達成とする。

1. 1コンポジションを `Diligent` と `Software` の両経路で描画できる
2. RenderQueue でバックエンド選択が可能
3. ダミー出力だけでなく、画像重ね合わせの最小合成が可能
4. バックエンド差分を診断ログで比較できる
5. UI 層の操作は `*Service` 経由規約を維持
6. Vulkan 対応に向けた抽象化ポイントを固定

## Definition Of Done

- 同一入力で `Diligent` と `Software/OpenCV` の両結果を出力できる
- RenderQueue ジョブ単位で backend を指定できる
- 最小合成（2レイヤー + blend mode）の結果が確認できる
- `Help -> Export Diagnostics...` で backend 情報が採取できる

## Work Packages

### 1. Backend Abstraction in Render Queue
- ジョブ設定に `RenderBackendType` を追加
- Service/UI/CoreQueue のデータ経路を統一

### 2. Software/OpenCV Compositor Baseline
- 画像入力 + 平面レイヤーの合成パスを実装
- 最低限の Blend（Normal/Add/Multiply）を固定

### 3. Diligent Path Harmonization
- v1.0 で整えた Layer/Composition Editor 導線と接続
- バックエンド切替時の UI 不整合を解消

### 4. Diagnostics and A/B Comparison
- diagnostics に `backend`, `job config`, `output summary` を追加
- 失敗時のログ導線を統一

### 5. Vulkan Readiness (Preparation)
- エンジン初期化抽象の拡張ポイントを定義
- CMake オプション設計（ON/OFF）を確定

## Suggested Order

1. `Backend Abstraction in Render Queue`
2. `Software/OpenCV Compositor Baseline`
3. `Diagnostics and A/B Comparison`
4. `Diligent Path Harmonization`
5. `Vulkan Readiness (Preparation)`
