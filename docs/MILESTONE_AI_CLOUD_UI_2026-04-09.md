# AI Cloud UI / Provider Milestone (2026-04-09)

`ローカル AI を既定に保ちつつ、必要なときだけクラウド AI を選べるようにする`
ためのマイルストーン。

## Goal

- `AIChatWidget` を local-only 前提から provider-aware に拡張する
- `OpenRouter` や `Kilo Gateway` のような cloud provider を UI から選べるようにする
- 既存の local backend (`llama.cpp` / `ONNX + DirectML`) を壊さず共存させる
- `AIClient` の cloud path を整理して、provider 切替と API key 管理を明確にする
- 将来的に provider を追加しやすい構造へ寄せる

## Definition Of Done

- UI から local / cloud provider を切り替えられる
- cloud provider 選択時は model path 入力を出さず、API key / base URL / model を扱える
- `OpenRouter` で chat が送受信できる
- `Kilo Gateway` のような OpenAI-compatible provider を追加しやすい
- provider 切替後に古い設定が残って誤動作しない
- local backend と cloud backend を同じチャット UI で扱える
- 設定保存・復元で provider state が崩れない

## Scope

- `Artifact/src/Widgets/AIChatWidget.cppm`
- `Artifact/include/Widgets/AIChatWidget.ixx`
- `Artifact/src/AI/AIClient.cppm`
- `Artifact/include/AI/AIClient.ixx`
- `ArtifactCore/src/AI/CloudAgent.cppm`
- `ArtifactCore/include/AI/ICloudAIAgent.ixx`
- `ArtifactCore/src/AI/APIKeyManager.cppm`
- `ArtifactCore/include/AI/APIKeyManager.ixx`
- 必要に応じて `Artifact/src/AppMain.cppm`

## Recommended Design

- local backend と cloud backend を別物として扱うのではなく、`AIClient` の provider 切替として扱う
- `AIChatWidget` は provider-dependent な設定 UI を持つ
- cloud provider は `OpenRouter` を先に実装し、その後 `Kilo Gateway` のような OpenAI-compatible provider を増やす
- provider ごとの API key / proxy / base URL / default model を設定層に集約する
- 送受信の実行経路は `AIClient` に寄せ、UI は state management と入力導線に集中する

## Milestones

### AI-1 Provider Inventory And UI Contract

- 現在の local-only provider 選択 UI を棚卸しする
- provider ごとに必要な入力項目を定義する
- local 用に必要な項目と cloud 用に必要な項目を分ける
- 既定 provider を local のまま維持する
- provider 名の正規化ルールを決める

### AI-2 Cloud Provider Adapter

- `OpenRouter` を first-class provider として UI / client に露出する
- OpenAI-compatible provider を追加しやすい共通 adapter を用意する
- `Kilo Gateway` のように base URL が必要な provider を扱えるようにする
- API key / proxy / base URL / model を provider ごとに保持する
- 失敗時の error message を UI へ戻せるようにする

### AI-3 Chat Widget Refactor

- `AIChatWidget` の provider combo を local / cloud 両対応にする
- cloud 選択時は model path を隠し、API key と endpoint 設定を見せる
- local 選択時は今までどおり model path ベースで操作できる
- provider によって browse / load / unload の導線を出し分ける
- チャット送信自体の操作感は変えない

### AI-4 Settings Persistence

- provider, api key, proxy, base URL, model の保存先を整理する
- 起動時に last used provider を復元する
- local / cloud の切替で互換性のある既存設定を読み替える
- 設定ファイルに残る古いキー名の migration 方針を決める

### AI-5 Validation And Safety

- API key 未設定時の案内を分かりやすくする
- 不正な base URL / provider 名を弾く
- cloud provider と local backend の状態遷移を安定させる
- エラー表示をログだけでなく UI にも出す
- 既存 local AI の初期化と競合しないことを確認する

## Implementation Breakdown

### 1. UI Contract

- `AIChatWidget` に provider-dependent form を追加する
- local 用の model picker と cloud 用の credential fields を分離する
- provider 変更時に表示項目を切り替える

### 2. Client Contract

- `AIClient` に provider descriptor を渡せるようにする
- cloud provider が選ばれたときの `initialize / sendMessage / postMessage` の経路を整理する
- cloud agent の生成と key injection を UI から隠蔽する

### 3. Cloud Adapter

- `OpenRouter` を既存実装の基準 provider とする
- OpenAI-compatible provider を共通化する
- provider ごとの base URL と default model を設定できるようにする

### 4. Settings And Migration

- `QSettings` に provider state を保存する
- 既存の local-only 設定と共存させる
- 後方互換を壊さない migration を入れる

### 5. UX Polishing

- local / cloud の切替時に不要な入力欄を隠す
- 未設定の項目はプレースホルダで案内する
- cloud 接続中 / ロード中 / エラー中の state を見せる

## Suggested Order

1. `AI-1 Provider Inventory And UI Contract`
2. `AI-2 Cloud Provider Adapter`
3. `AI-3 Chat Widget Refactor`
4. `AI-4 Settings Persistence`
5. `AI-5 Validation And Safety`

## Validation Checklist

- `local` provider で今までどおりチャットできる
- `OpenRouter` で会話できる
- `Kilo Gateway` のような OpenAI-compatible provider を追加しやすい
- provider を切り替えても UI が破綻しない
- API key 未設定時に適切なガイドが出る
- local model の読み込みと cloud chat が競合しない
- 再起動後も provider / key / endpoint の設定が復元される

## Current Status

- `2026-04-09`
  - `AIClient` に local / cloud の分岐がすでにある
  - `OpenRouter` 向け cloud agent 実装がすでにある
  - `AIChatWidget` は local backend 選択 UI が中心で、cloud UI は未整備
  - cloud provider の追加は新規基盤ではなく既存 AI UI の拡張として進められる

## Notes

- `AIChatWidget` の provider combo は現在 local 前提なので、ここを最初の分岐点にすると影響範囲が小さい
- `AIClient` は cloud agent を持てるので、UI 側の切替だけでなく設定保存の整理が重要になる
- `OpenRouter` を先に通すと、OpenAI-compatible provider の抽象化がそのまま `Kilo Gateway` に流用できる
