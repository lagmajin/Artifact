# Project View Milestone

Project View / Project panel を独立して改善するための実装メモ。

## M-PV-1 Basic Operations

- 目標:
  Project View 上で日常操作に必要な最小限の編集を破綻なく行えるようにする。
- 対象:
  選択、カレントコンポ同期、rename、delete、double-click 導線、検索/基本 filter。
- 完了条件:
  - current composition が Project View selection と一致する
  - composition / footage / folder を rename できる
  - delete が keyboard と context menu の両方で通る
  - double-click で composition は timeline を開き、footage は source location へ飛べる

## M-PV-2 Asset Presentation

- 目標:
  Project View でアイテムの種類と主要情報を把握しやすくする。
- 対象:
  thumbnail、type icon、size、duration、fps、missing 状態、updated 表示。

## M-PV-3 Organization

- 目標:
  project 内の folder/bin 整理を進めやすくする。
- 対象:
  folder 作成、drag & drop 整理、expand/collapse、unused view、tag/filter。

## M-PV-4 Import Flow

- 目標:
  file import と relink を Project View 主体で完結しやすくする。
- 対象:
  drag & drop、multi import、sequence 認識、missing footage relink、explorer reveal。

## M-PV-5 Composition Bridge

- 目標:
  Project View から composition / timeline / viewer を自然に開けるようにする。
- 対象:
  current composition 同期、double-click、context menu、new comp from selection。

## First Pass Notes

- 2026-03-12:
  - current composition と Project View selection の同期を追加
  - rename を `model()->setData()` の疑似編集から service / project 更新へ修正
  - `F2` rename、`Delete` remove の keyboard shortcut を追加
  - double-click on footage は source location を開く形で整理
  - composition metadata を placeholder ではなく actual `compositionSize / frameRange / frameRate` から表示
  - project item type icon を folder / composition / solid / image / video / audio / missing で描き分け

  - context menu に `Repeat Last Command` / `Repeat Last New Command` を追加

## Next Slice for M-PV-3

- folder/bin の配置先選択
- nested folder 作成 API
- drag & drop で project item 整理
- unused / recent / tag の仮想 view

## Follow-up Notes

- `2026-03-15`
  - `ArtifactProjectView` に `Up/Down/Home/End/PageUp/PageDown/Left/Right/Enter` の keyboard navigation を追加
  - current composition 同期時、対象 composition の親 folder を 1 段だけでなく root まで展開するよう修正
  - 選択変更時に `ensureIndexVisible()` を通し、深い階層でも current composition が viewport 外へ取り残されないよう修正
  - これにより、custom `QAbstractScrollArea` ベース Project View でも `M-PV-1 Basic Operations` の「selection と current composition が一致する」を見失いにくくした
