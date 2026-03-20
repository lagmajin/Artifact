# FFmpeg Backend Split Milestone

日付: 2026-03-20
目的: Render Queue の動画エンコード経路を `ffmpeg.exe` パイプ型と FFmpeg C API 直呼び型に分離し、用途ごとに明示的に選べるようにする。

## Goals

- `ffmpeg.exe` を `QProcess` 経由で使う互換経路を `Pipe` backend として保持する
- `ArtifactCore::FFmpegEncoder` を使う direct API 経路を `Native` backend として持つ
- `ArtifactRenderQueueService` は backend の差を吸収し、フレーム供給だけに集中する
- 将来的に UI から backend を job 単位で選択できる

## Phase 1: Foundation

- job に `encoderBackend = auto|pipe|native` を追加する
- backend 選択の setter/getter を `ArtifactRenderQueueService` に追加する
- 現状の `startAllJobs()` の動画出力を backend 経由に差し替える

## Phase 2: Pipe backend

- `ffmpeg.exe` 起動
- raw BGRA frame を stdin に流す
- stdout/stderr を必要なら収集する
- タイムアウトと中断を入れる

## Phase 3: Native backend

- `ArtifactCore::FFmpegEncoder` をラップする
- `QImage` から `ImageF32x4_RGBA` に変換して投入する
- container / codec / bitrate / fps を job settings から反映する

## Phase 4: UI / Policy

- Render Queue job editor に backend 選択 UI を足す
- `auto` の選択ポリシーを明確化する
- backend ごとの失敗メッセージを job に返す

## Non-goals

- 今回は音声多重化は対象外
- 今回は pipe/backend の完全な同等機能を保証しない
- 今回は image sequence 出力の経路を統合しない

