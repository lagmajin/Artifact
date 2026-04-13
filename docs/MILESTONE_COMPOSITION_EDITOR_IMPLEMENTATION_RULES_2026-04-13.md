# Composition Editor Implementation Rules

`ArtifactCompositionEditor` 周辺の実装で迷いを減らすための、入力解釈と状態管理のルール集。

この文書の対象は次の範囲に絞る。

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/TransformGizmo.cppm`
- `Artifact/src/Widgets/Render/PrimitiveRenderer3D.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`

関連するレンダーマネージャ資料は、最新の分析を優先して次を参照する。

- `docs/planned/RENDER_QUEUE_MANAGER_GAP_ANALYSIS_2026-04-13.md`
- `Artifact/docs/MILESTONE_RENDER_MANAGER_2026-03-17.md`

## Core Principles

- 1 つの入力に対して 1 つの主責務だけを持たせる。
  - gizmo 操作
  - viewport pan / zoom
  - selection
  - direct edit
  - context menu
- 状態の真実は 1 箇所に寄せる。
  - tool mode
  - gizmo mode
  - active composition
  - current layer / selection
  - viewport transform
- UI の表示と内部状態は分離する。
  - 見た目は controller / style 側
  - 操作判断は editor / rules 側

## Input Priority

入力の優先順位は原則として次の順に扱う。

1. gizmo / direct manipulation
2. direct edit surface
3. viewport pan / zoom
4. selection
5. keyboard shortcut
6. context menu

これにより、同じマウス操作が複数の機能に同時に吸われることを防ぐ。

### Mouse Rules

- 左ドラッグは、まず gizmo / direct edit に渡す。
- gizmo がヒットしているときは、viewport pan を開始しない。
- 中ボタンパンは、viewport の空き領域でのみ有効にする。
- wheel は、カーソル下の viewport に対する zoom / scrub / scroll のどれか 1 つに限定する。
- 右クリックは、選択を壊さずコンテキストメニューを出すだけにする。

### Gizmo Rules

- gizmo mode は UI からの設定だけでなく、描画時にも controller へ反映されること。
- `Move` / `Rotate` / `Scale` / `All` の表示切替は、描画と hit test の両方に一致させる。
- gizmo の見た目変更は hit test を変えない。
- rotate gizmo の見た目と操作半径は分離してよい。

## State Synchronization

- timeline / inspector / composition editor の同期は、できるだけ同じイベント経路を使う。
- 直接 Qt signal を多段でつなぎすぎない。
- selection 変更は deferred event または bus 経由で集約する。
- property widget は current layer を勝手に nil に落とさず、同一 composition 内なら維持を優先する。

## Rendering Rules

- `ArtifactCompositionRenderController` は描画の責務に集中する。
- `ArtifactCompositionEditor` は入力と状態遷移に集中する。
- `PrimitiveRenderer3D` / `TransformGizmo` の負荷は、1 イベント 1 再描画にしない。
- mouse move のたびに重い再構築をしない。
- 大きい再計算は debounce / dirty flag / cached geometry を使う。

## Gizmo Eligibility Rules

- レイヤーやクリップが選択されていても、表示範囲外なら gizmo は出さない。
- 画面外にある selection は、選択状態と gizmo 表示を分離して扱う。
- 音声のみのレイヤーには transform 系 gizmo を出さない。
- transform gizmo は、編集対象に空間的な変換意味がある場合だけ表示する。
- `Move` / `Rotate` / `Scale` は、それぞれ対象 layer の種類と編集可能状態を確認してから出す。
- クリップの選択と layer の選択が両方ある場合は、優先対象を 1 つに固定する。

## Layout / Overlay Rules

- viewport 上の補助表示は、邪魔にならない最小限にする。
- 常時表示ラベルは増やしすぎない。
- オーバーレイは編集対象の操作を邪魔しない位置に置く。
- 迷う表示は右クリックの簡易メニューより、常設の明示 UI に寄せる。

## Timeline / Property Coupling Rules

- timeline と property widget は、現在 layer と current property path を共有する。
- 左ペインの簡易メニューで状態を隠さない。
- property row の表示は、レイヤー名より優先して「いま編集中の値」が読めることを重視する。
- property row には、編集対象の現在フレーム keyframe の有無を示す最小限の入口を置いてよい。
- keyframe toggle は、property editor と timeline 左ペインのどちらから触っても同じ結果になるようにする。
- layer 補助情報（Parent / Blend など）は layer 行に寄せ、property 行では見せない。

## Forbidden Patterns

- gizmo mode を描画のたびに UI から上書きすること
- mouse move 内で同期的に重いレンダリング再構築を行うこと
- 似た責務の Qt signal を複数箇所で直結すること
- context menu にだけしか存在しない操作導線を増やすこと
- 画面上の補助情報を gradient や過剰な装飾で目立たせること

## Implementation Checklist

- [ ] tool / gizmo / pan の優先順位がコード上で読める
- [ ] gizmo mode が controller と editor で一致する
- [ ] selection / current layer / property path の同期が一本化されている
- [ ] mouse move 中の重い処理が抑制されている
- [ ] viewport overlay が操作を妨げていない
- [ ] timeline / inspector / composition editor の状態が破綻しない

## Notes

- この文書は「機能仕様」ではなく「実装時の判断基準」。
- 挙動を増やす前に、まずどの入力がどの責務に属するかを決める。
- 迷ったら、`ArtifactCompositionEditor` は入力、`CompositionRenderController` は描画、`ArtifactTimelineWidget` は同期、`ArtifactPropertyWidget` は編集に寄せる。
- gizmo の表示条件は、選択有無よりも「今見えているか」「今編集可能か」を優先して判定する。
- render queue manager の改善方針は、古い milestone より最新の gap analysis を優先する。
