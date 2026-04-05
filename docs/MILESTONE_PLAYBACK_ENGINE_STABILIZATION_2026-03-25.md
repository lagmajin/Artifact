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

## Status (Completed 2026-04-04)

 Gemini CLI により、Playback Engine の安定化と契約の統一が完了しました。

### 実施内容

1.  **PlaybackState 契約の統一 (WP-1)**
    *   `ArtifactCore::PlaybackState` を `ArtifactCore/include/Playback/PlaybackState.ixx` (`Playback.State` モジュール) へ抽出し、プロジェクト全体で共有可能にしました。
    *   `Stopped, Playing, Paused, Buffering, Error` の 5 つの状態を持つ完全な enum として定義しました。
    *   `MediaPlaybackController`, `PlaybackClock`, `ArtifactCompositionPlaybackController` から重複した定義を削除し、この統一 enum を参照するように修正しました。

2.  **Engine State Management の刷新 (WP-2)**
    *   `ArtifactPlaybackEngine` の内部実装 (`Impl`) にあった 3 つの bool フラグ (`playing_`, `paused_`, `stopped_`) を廃止し、単一の `std::atomic<PlaybackState> state_` による管理へ移行しました。これにより、状態の矛盾が発生しない堅牢な構造になりました。

3.  **シグナル発火スレッドの明文化 (WP-2)**
    *   `ArtifactPlaybackEngine` からのシグナルは、ワーカースレッドから直接ではなく、UI スレッドへ `invokeMethod` (QueuedConnection) を介して発火されるよう統一しました。これにより、UI 側でのスレッドセーフな受信が保証されます。

4.  **Observability の向上 (WP-5)**
    *   `[PlaybackEngine]` および `[PlaybackController]` プレフィックスを用いた一貫したログ出力を追加しました。
    *   状態遷移 (`state transition: X -> Y`)、ループラップ (`Loop wrap (end -> start)` )、オーディオ同期補正のログを詳細化し、再生の振る舞いを追跡可能にしました。

### 完了定義の達成状況

- [x] `ArtifactPlaybackEngine` の public signal / state contract が 1 つに定まった
- [x] `ArtifactPlaybackService` 側の変換責務が整理され、二重通知が防止された
- [x] `play/pause/stop/toggle/seek` の実行仕様が engine/controller で統一された
- [x] 再生状態の変更が `engine -> service -> UI` で一貫して伝播する
- [x] 契約ズレが再発しにくい構造（一箇所定義の enum）が確立された

---

## 完了の詳細

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
