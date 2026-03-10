# Artifact Milestone Roadmap (2026-03-10)

## Active

1. [v0.2 Production Path](./MILESTONE_V0_2_PRODUCTION_PATH_2026-03-10.md)
- 目的: 編集体験の安定化と出力導線の実用化
- 現在の主タスク:
  - RenderQueue に `FPS/Bitrate` 編集追加
  - RenderQueue の `Rerun Selected / Rerun Done-Failed` 導線追加
  - タイムライン境界バグの残件整理
  - メニュー有効条件の統一

2. [v0.3 Editor Core](./MILESTONE_V0_3_EDITOR_CORE_2026-03-10.md)
- 目的: Undo/Redo・選択同期・削除依存確認の完成
- 現在の主タスク:
  - 削除依存確認の文言/導線を Service 経由へ統一
  - Inspector Effects 操作性改善
  - View同期の取りこぼし解消

## Next

1. [v0.4 Pipeline Foundation](./MILESTONE_V0_4_PIPELINE_FOUNDATION_2026-03-10.md)
- 目的: 保存/出力/バックエンド抽象化/回帰テストの基盤整備

2. [v0.5 Playable Editor](./MILESTONE_V0_5_PLAYABLE_EDITOR_2026-03-11.md)
- 目的: 実作業で回せる編集安定性と運用導線の完成

3. [v0.6 Effects Pipeline Usability](./MILESTONE_V0_6_EFFECTS_PIPELINE_USABILITY_2026-03-11.md)
- 目的: エフェクト編集導線を実運用可能なUXへ改善

## Suggested Delivery Rhythm

1. 週前半: v0.2 残件の実装
2. 週後半: v0.3 先行タスク（Undo/Redo と View同期）の着手
3. 並行: v0.4 に向けた backend 抽象化の小スパイクを継続

## Operating Rule

- UI層からは `*Service` 経由のみを許可
- 破壊操作（削除/上書き）は必ず確認導線を持つ
- 新機能追加時は同時に「失敗時表示」を実装する
