# FFmpeg Bridge Runtime Resolution (2026-03-25)

レンダーキューの `PipeFFmpegExeBackend` は `ffmpeg.exe` を直接起動するが、
実行環境によっては `PATH` に載っておらず、bridge mode だけ失敗する。

今回、実行ファイルディレクトリ基点の探索を追加したが、これは入口修正であって
ランタイム依存解決全体の設計はまだ未整理である。

## Goal

- `ffmpeg.exe bridge mode` が配布形態ごとに安定して起動する
- `PATH` 依存を減らし、アプリ同梱運用を前提にしても壊れない
- 見つからない時に、ユーザーと AI が原因をすぐ特定できる

## Definition Of Done

- 実行ファイル直下 / `bin` / `tools/ffmpeg` などの候補から `ffmpeg.exe` を解決できる
- 見つからない場合、試した探索先がログかエラー文に残る
- `pipe` backend と `native` backend の失敗理由が区別できる
- 将来の設定 UI から明示パス指定を追加できる構造になっている

## Work Packages

### 1. Search Policy Freeze

対象:

- `Artifact/src/Render/ArtifactRenderQueueService.cppm`

内容:

- 実行ファイルディレクトリ起点の探索候補を docs で固定
- 相対探索の優先順位を定義

完了条件:

- 「どこを探すか」がコードと docs で一致する

### 2. Diagnostics Improvement

対象:

- `PipeFFmpegExeBackend`
- RenderQueue の失敗表示

内容:

- 起動失敗時に使用した実パスを出す
- 可能なら候補一覧も保持する

完了条件:

- `ffmpeg がない` の調査が UI/ログから可能

### 3. Explicit Override Path

対象:

- Application settings
- Render output settings

内容:

- `ffmpeg.exe` の明示パス指定を追加
- override がある場合は最優先

完了条件:

- 配布/開発環境差を設定で吸収できる

### 4. Backend Selection UX

対象:

- RenderQueue backend selection

内容:

- `auto / native / pipe` の意味を UI で明示
- `native` は動くが `pipe` だけ失敗、のような切り分けをしやすくする

完了条件:

- backend 切替の失敗理由が見えやすい

## Recommended Order

1. `Search Policy Freeze`
2. `Diagnostics Improvement`
3. `Explicit Override Path`
4. `Backend Selection UX`

## Notes

2026-03-25 時点では、`applicationDirPath()` 起点の候補探索を追加済み。
ただし、明示設定、候補一覧の表示、配布時のパッケージルールは未整理。
