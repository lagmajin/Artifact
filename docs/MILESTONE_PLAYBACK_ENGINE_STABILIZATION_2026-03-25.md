# Playback Engine Stabilization (2026-03-25)

`ArtifactPlaybackEngine` は新しい再生スレッド基盤として入っているが、
`ArtifactPlaybackService` と `ArtifactCompositionPlaybackController` の二重経路がまだ残っており、
シグナル契約や状態表現の不整合が再発しやすい。

今回の `PlaybackState` 参照崩れは、その整理不足が表面化したものとして扱う。

## Goal

- `ArtifactPlaybackEngine` と `ArtifactPlaybackService` の契約を固定する
- `play / pause / stop / seek / frameChanged` の意味を 1 か所で説明できるようにする
- enum と bool 三値のような状態表現の二重管理をなくす
- 旧 controller 経路を残す場合も、役割を明確に分離する

## Current Problem

現状コードでは次が混在している。

- `ArtifactPlaybackEngine` の signal は `playbackStateChanged(bool playing, bool paused, bool stopped)`
- `ArtifactPlaybackService` は `PlaybackState` enum を外部契約として持つ
- 実装側には古い `PlaybackState::Playing/Paused/Stopped` 参照が残りやすい
- `ArtifactCompositionPlaybackController` も後方互換経路として同時に接続されている

この状態だと、

- コンパイルエラー
- state の重複変換
- signal の意味ズレ
- どちらが authoritative か不明

が起きやすい。

## Definition Of Done

- `ArtifactPlaybackEngine` の public signal / state contract が 1 つに定まる
- `ArtifactPlaybackService` 側の変換責務が明文化される
- `play/pause/stop/toggle/seek` の実行元が engine/controller で二重化しない
- 再生状態の変更が `engine -> service -> UI` で一貫して伝播する
- 今回のような enum/bool 契約ズレが再発しにくい docs が残る

## Work Packages

### 1. State Contract Unification

対象:

- `Artifact/include/Playback/ArtifactPlaybackEngine.ixx`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
- `Artifact/include/Service/ArtifactPlaybackService.ixx`
- `Artifact/src/Service/ArtifactPlaybackService.cppm`

内容:

- `PlaybackState` を engine public API に上げるか、
  逆に service 側だけの契約として bool signal を正式化するかを決める
- 中途半端な併存をやめる

完了条件:

- 状態表現が 1 系統に揃う

### 2. Engine / Controller Authority Split

対象:

- `ArtifactPlaybackEngine`
- `ArtifactCompositionPlaybackController`
- `ArtifactPlaybackService`

内容:

- 新 engine を authoritative にするのか、
  controller を fallback として残すのかを決める
- 並列で同じ signal を forward する設計を見直す

完了条件:

- frame source と state source が 1 本に見える

### 3. Thread Boundary Rules

対象:

- `ArtifactPlaybackEngine.cppm`
- `ArtifactPlaybackService.cppm`

内容:

- `QueuedConnection` と `DirectConnection` の使い分けルールを固定
- UI スレッドで触るものと worker thread 内で閉じるものを明文化
- `frameChanged` と `playbackStateChanged` の発火スレッドを確認

完了条件:

- 再生中の state/frame signal が安全に扱える

### 4. Seek / Stop / Restart Semantics

対象:

- `play()`
- `pause()`
- `stop()`
- `goToFrame()`
- `togglePlayPause()`

内容:

- `stop()` 後の current frame を start に戻す仕様を固定
- seek 時に audio buffer をどう扱うかを決める
- pause から play 再開時の clock 基準を docs に残す

完了条件:

- フレーム位置と再生状態の仕様が曖昧でない

### 5. Playback Observability

対象:

- `ArtifactPlaybackEngine`
- `ArtifactPlaybackService`

内容:

- dropped frame, audio sync correction, loop wrap をログで観測可能にする
- state transition のログ粒度を一定にする

完了条件:

- 再生系の不具合をログから追いやすい

## Recommended Order

1. `State Contract Unification`
2. `Engine / Controller Authority Split`
3. `Thread Boundary Rules`
4. `Seek / Stop / Restart Semantics`
5. `Playback Observability`

## Immediate Follow-up

今回の修正では、`ArtifactPlaybackEngine.cppm` 内の古い
`PlaybackState::Playing/Paused/Stopped`
参照を、宣言側の bool signal 契約に合わせて置き換えた。

これは応急処置として正しいが、長期的には

- engine 側も `PlaybackState` を正式採用する
  または
- service 側だけが enum を持つ

のどちらかへ寄せるべき。

## Risks

- engine と controller の両方を authoritative に見せると、再生状態が二重通知になる
- signal 契約だけ直しても、thread boundary が曖昧だと別の不具合へ移る
- audio sync 補正と frame seek の責務が混ざると、scrub 時に破綻しやすい

## Exit Criteria

- `ArtifactPlaybackEngine` を見れば再生 state 契約がわかる
- `ArtifactPlaybackService` を見れば UI への変換責務がわかる
- playback 系の compile / state mismatch が再発しにくい
