# Milestone: Style Surface Kit / QProxyStyle-Backed Widget Primitives (2026-04-09)

**Status:** Draft  
**Goal:** `QSS` に頼らず、`QProxyStyle` と軽量な汎用 widget で再利用できる見た目の surface を作る。

---

## Why This Now

最近の UI 調整で、個別 widget の `paintEvent()` に見た目が散らばると壊れやすいことが分かってきた。  
一方で、すでに `ArtifactCommonStyle` のような `QProxyStyle` の土台があり、ここに property-driven な描画ルールを載せれば、  
**「widget は役割を宣言するだけ、見た目は style が持つ」** 方向へ寄せられる。

この milestone は、再生コントロールやタイムライン周辺でよく使う chrome を、再利用可能な surface primitives に分けるためのもの。

---

## Scope

- `Artifact/src/Widgets/CommonStyle.cppm`
- `Artifact/include/Widgets/CommonStyle.ixx`
- `Artifact/include/Widgets/StyleSurface.ixx`
- `Artifact/src/Widgets/StyleSurface.cppm`
- `Artifact/src/Widgets/Control/ArtifactPlaybackControlWidget.cppm`
- 将来的に `Artifact/src/Widgets/Timeline/*`
- 将来的に `Artifact/src/Widgets/Render/*`
- 将来的に `Artifact/src/Widgets/Diagnostics/*`

---

## Non-Goals

- UI 全体を一気に共通 widget に置き換える
- `QSS` を全面撤廃する
- `QProxyStyle` で何でも描く
- 既存の widget を大改造する

---

## Design Principles

- 見た目の主責務は `QProxyStyle` に寄せる
- widget は `artifact*` という property や軽量な型で役割を宣言する
- 何度も使う chrome は「小さくて薄い」汎用 widget に切る
- color token は theme 由来を基本に、例外だけ tone で上書きする
- `QSS` は新規追加しない

---

## Proposed Surface Primitives

### 1. Framed Tool Button

- 再生コントロール、ツールバー、タイムライン操作に使う
- `artifactFramedToolButton=true` のような property で `QProxyStyle` が枠線を描く
- hover / pressed / checked の状態だけを style 側で共通化する

### 2. Tone Label

- タイムコード、補助 readout、短い status text に使う
- `Gold`, `MutedGold`, `Accent`, `Success`, `Warning`, `Danger` のような tone を用意する
- `QPalette` で済む場面は palette を使い、それでも不安定なら owner-draw に逃がす

### 3. Status Chip

- `RUN`, `DONE`, `ERROR`, `LIVE` のような短い状態ラベルに使う
- 背景、枠、角丸、padding を共通化する

### 4. Section Header

- ツールバー上段、パネル見出し、サブヘッダーに使う
- 下線、余白、背景を共通化する

---

## Phases

### Phase 1: Style Hook Stabilization

- `ArtifactCommonStyle` の button/label 周りの責務を固定する
- `artifactFramedToolButton` などの property 名を決める
- 既存 widget の custom paint を減らす

**Done when:**

- button の枠描画が style 側に集約される
- 各 widget が custom paint で frame を持たなくなる

### Phase 2: Shared Widget Primitives

- `ArtifactFramedToolButton` を導入する
- `ArtifactToneLabel` を導入する
- 再生コントロールなどで使い始める

**Done when:**

- 再利用可能な小さな surface primitives が 2 つ以上できる
- メディアコントロールの色や枠が個別実装から抜ける

### Phase 3: Rollout

- Timeline / Render Queue / Diagnostics へ展開する
- button, label, chip, header を順次差し替える
- 見た目のばらつきを減らす

**Done when:**

- 主要な chrome が共通 primitives で説明できる
- `QSS` ではなく property と style で見た目が揃う

---

## Suggested Order

1. Phase 1: Style Hook Stabilization
2. Phase 2: Shared Widget Primitives
3. Phase 3: Rollout

---

## Validation Checklist

- `ArtifactPlaybackControlWidget` の frame button が共通枠で表示される
- タイムコード系の readout が tone で色付けされる
- hover / pressed / checked が style 経由で安定して見える
- widget ごとに個別の `paintEvent()` を増やさない
- `QSS` を使わずに同等の表現ができる

---

## Current Status

- `ArtifactCommonStyle` はすでに `QProxyStyle` として存在する
- framed tool button の property hook は先に入っている
- これから `ToneLabel` のような薄い共通 widget を足していく段階

