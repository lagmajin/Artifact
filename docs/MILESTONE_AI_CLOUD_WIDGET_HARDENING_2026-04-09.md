# Milestone: AI Cloud Widget Hardening / OpenRouter-Kilo Gateway Trial (2026-04-09)

**Status:** Draft  
**Goal:** まだ実運用に耐えないクラウド AI ウィジェットを、`Kilo Gateway` 優先で試せる程度まで整備し、あわせてまともな設定ウィジェットを用意する。

---

## Why This Now

クラウド AI 用の UI はすでに存在するが、現状は「触れる」段階で、日常利用できる安定性には届いていない。  
特に `OpenRouter` や `Kilo Gateway` のような OpenAI-compatible provider を試すには、

- provider 切替の責務
- API key / base URL / model の保持
- エラー表示と復帰
- local AI との共存

が整理されている必要がある。

この milestone は、クラウド利用の入口を「試験的」から「実用前提の試験」に引き上げるための整備計画。

---

## Scope

- `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm`
- `Artifact/include/Widgets/AI/ArtifactAICloudWidget.ixx`
- `Artifact/src/Widgets/AIChatWidget.cppm`
- `Artifact/include/Widgets/AIChatWidget.ixx`
- `Artifact/src/AI/AIClient.cppm`
- `Artifact/include/AI/AIClient.ixx`
- `ArtifactCore/src/AI/CloudAgent.cppm`
- `ArtifactCore/include/AI/ICloudAIAgent.ixx`
- `ArtifactCore/src/AI/APIKeyManager.cppm`
- `ArtifactCore/include/AI/APIKeyManager.ixx`
- 必要に応じて `Artifact/src/AppMain.cppm`

---

## Non-Goals

- クラウド AI を完全な一般化 provider framework にする
- local AI を置き換える
- 全 provider の完全サポートを一気に完成させる
- UI を大改造する

---

## Design Principles

- `Kilo Gateway` を最初の実用 provider として扱う
- `OpenRouter` は OpenAI-compatible provider の比較対象・代替候補として扱う
- `Kilo Gateway` のような OpenAI-compatible provider を追加しやすくする
- UI は「接続先の切替」と「必要情報の入力」に集中させる
- 安定性の責務は UI だけでなく client / settings / error handling に分散させる
- local AI の動作を壊さない

---

## Phases

### Phase 1: Widget Contract Cleanup

- `ArtifactAICloudWidget` の役割を明確化する
- provider 切替時に表示する項目を整理する
- local 用と cloud 用の入力を混ぜない
- 不要な UI state を削る

**Done when:**

- UI が「何を入力すべきか」を迷わせない
- local / cloud の見た目の切替が安定する

### Phase 2: Kilo Gateway First Pass

- `Kilo Gateway` の接続情報を UI から渡せるようにする
- API key / model / base URL の最小セットを確認する
- 失敗時に分かる error message を返す

**Done when:**

- `Kilo Gateway` で最小の送受信ができる
- 失敗したときに UI が固まらない

### Phase 3: OpenRouter Compatibility

- `OpenRouter` のような base URL 必須 provider を試せるようにする
- OpenAI-compatible provider の差分を吸収する
- provider ごとの default model を設定しやすくする

**Done when:**

- `OpenRouter` が同じ UI で試せる
- provider 追加が base URL 差し替え程度で済む

### Phase 4: Settings Widget And Persistence

- provider state を扱う settings widget を追加する
- API key / base URL / model / proxy / default model を見通しよく編集できるようにする
- last used provider を戻せるようにする

**Done when:**

- `AIClient` の設定をまとめて編集できる widget がある
- local / cloud の切替を settings からも操作できる
- 設定の入口がチャット widget と分離される

### Phase 5: Persistence And Recovery

- provider state の保存と復元を整理する
- last used provider を戻せるようにする
- API key と endpoint の組み合わせが崩れないようにする

**Done when:**

- 再起動しても provider state が再現される
- local / cloud の切替で古い値が残りにくい

### Phase 6: Error Handling And UX Polish

- 接続失敗時のメッセージを UI に戻す
- key 未設定 / URL 不正 / model 不明 を見分ける
- cloud 接続中の state を分かりやすくする

**Done when:**

- 何が足りないか UI から分かる
- 再試行しやすい

---

## Implementation Breakdown

### 1. UI Layer

- `ArtifactAICloudWidget` を provider-aware にする
- local / cloud で入力欄を切り替える
- `OpenRouter` と `Kilo Gateway` を先に選べるようにする

### 2. Client Layer

- `AIClient` に cloud provider の差分を渡せるようにする
- provider ごとの base URL と key を扱う
- cloud agent の初期化失敗を UI に返す

### 3. Settings Layer

- provider ごとの設定を保存する
- 設定の互換性を壊さない
- last used provider を復元する

### 4. Validation Layer

- API key 未設定を検出する
- base URL の破綻を検出する
- local AI と cloud AI の状態遷移を分離する

---

## Suggested Order

1. Phase 1: Widget Contract Cleanup
2. Phase 2: Kilo Gateway First Pass
3. Phase 3: OpenRouter Compatibility
4. Phase 4: Settings Widget And Persistence
5. Phase 5: Persistence And Recovery
6. Phase 6: Error Handling And UX Polish

---

## Validation Checklist

- `Kilo Gateway` で chat を送れる
- `OpenRouter` で同じ UI を試せる
- provider 切替で UI が壊れない
- settings widget から provider state をまとめて編集できる
- 再起動後に last used provider が復元される
- API key / URL / model の欠損が UI で分かる
- local AI が cloud UI の変更で壊れない

---

## Current Status

- `AIChatWidget` は local 前提の UI が中心
- cloud 側は試作状態で、まだ実用には足りない
- `Kilo Gateway` を先に通すと、UI/設定/接続の最小構成を早く固められる
- `OpenRouter` はその後の互換 provider 検証として扱うと分かりやすい
- まずは widget contract を整えてから provider 実装へ進むのが安全
