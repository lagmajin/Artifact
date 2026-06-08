# Preview Freeze & Stop Responsiveness (2026-06-05)

## Goal

動画プレビュー再生中の UI フリーズと、Stop ボタンの応答不能を解消する。

## Problem Summary

再生中に UI スレッドが以下の重い処理で埋まり、操作応答が失われる：

1. **PNG エンコード on UI thread** — 毎フレーム `image.save(&file, "PNG")` が UI スレッドで実行される
2. **QImage 変換 × 3 回 on UI thread** — `convertToFormat` + `setFromRGBA8` + ハッシュマップ挿入
3. **Stop の同期ブロック** — `audioClient->Stop()` + `renderThread.join()` が UI スレッドをブロック
4. **renderMutex_ contention** — TBB レンダースレッドと UI 操作が同一 mutex を競合
5. **goToFrame フルレンダリング** — シーキング中に UI スレッドで全レイヤー描画

## Root Cause Analysis

### Critical: PNG エンコード on UI Thread

**ファイル**: `ArtifactPlaybackService.cppm:596-614`

```
再生中の毎フレーム (33ms ごと):
  engine_->frameChanged → DirectConnection lambda (UI thread)
    → publishFrame lambda (QueuedConnection, next event loop)
      → persistPreviewFrameToDisk()
        → image.save(&file, "PNG")   ← UI スレッドで PNG エンコード (数十 ms)
        → file.commit()              ← UI スレッドでディスク書き込み
```

1920×1080 フレームの PNG エンコードは **15-40 ms**。30fps (33ms/フレーム) で毎フレーム走るため、UI が完全に固まる。

### Critical: Stop の同期ブロック

**ファイル**: `ArtifactPlaybackEngine.cppm:148-175` → `AudioRenderer.cppm:361` → `WASAPIBackend.cppm:309-323`

```
UI Thread:
  Service::stop()
    → engine_->stop()
      → impl_->stop()
        → audioRenderer_->stop()
          → WASAPIBackend::stop()
            → audioClient->Stop()      ← COM 呼び出し (ドライバ依存で数百 ms ブロック)
            → renderThread.join()      ← WaitForMultipleObjects(INFINITE) 終了待ち
```

全てが UI スレッド上で**同期実行**。`audioClient->Stop()` や `renderThread.join()` が数百 ms ブロックすると、stop ボタンのビジュアルフィードバックが消え、UI がフリーズしたように見える。

### High: QImage 変換 on UI Thread

**ファイル**: `ArtifactPlaybackService.cppm:710-736`

```
publishFrame lambda (UI thread)
  → storeFrameImageInRam()
    → image.convertToFormat(Format_RGBA8888)   ← フルフレーム memcpy
    → cpuImage.setFromRGBA8(...)               ← RGBA8→float32x4 変換 (4倍メモリ)
    → ramPreviewImageCache_[frame] = ...       ← ハッシュマップ挿入
```

### High: renderMutex_ Contention

**ファイル**: `ArtifactCompositionRenderWidget.cppm`

```
TBB レンダースレッド:  renderMutex_ ロック → renderOneFrame() (5-30ms)
UI スレッド:          renderMutex_ ロック → mouseMoveEvent / zoomIn / setComposition ...
```

レンダリング中にマウス操作・ズーム操作・コンポジション切替が全てブロックされる。

### Medium: goToFrame フルレンダリング

**ファイル**: `ArtifactPlaybackEngine.cppm:701-714`

```
goToFrame() (UI thread)
  → renderPreviewFrame()
    → renderFrame()
      → generateCompositionThumbnail()  ← 全レイヤーを QPainter で描画
      → canvas.scaled(sz, SmoothTransformation)  ← バイキュービック補間
```

## Fix Plan

### Phase 1: Stop の非同期化 (Critical)

**対象**: `AudioRenderer::stop()`, `WASAPIBackend::stop()`

- `AudioRenderer::stop()` に **タイムアウト付き非同期版** を追加
- `WASAPIBackend::stop()` で `renderThread.join()` を ** detach + atomic flag** に変更
- UI スレッドは `audioRenderer_->requestStop()` で即座に制御を返す

```cpp
// 案: 非同期 stop
void AudioRenderer::requestStop() {
    active.store(false);
    backend->requestStop();  // stopEvent をセットするだけ、join しない
    ringBuffer->clear();
}
```

### Phase 2: PNG エンコードの非同期化 (Critical)

**対象**: `ArtifactPlaybackService.cppm`

- `persistPreviewFrameToDisk()` を **専用 I/O スレッド** に退避
- 再生中のみ無効化するオプションを追加
- フレームキュー + バックグラウンドワーカーで処理

### Phase 3: QImage 変換の退避 (High)

**対象**: `ArtifactPlaybackService.cppm`

- `storeFrameImageInRam()` を Phase 2 の I/O スレッドと共有
- `convertToFormat` + `setFromRGBA8` をバッチ処理

### Phase 4: renderMutex_ の改善 (High)

**対象**: `ArtifactCompositionRenderWidget.cppm`

- レンダリング中に UI 操作を **キューイング**（try_lock + retry）
- または `renderMutex_` を `std::shared_mutex` に変更し、読み取り操作は共有ロックに

### Phase 5: goToFrame の最適化 (Medium)

**対象**: `ArtifactPlaybackEngine.cppm`

- プレビュー用の **低解像度パス** を追加（1/4 サイズで描画 → スケール）
- シーキング中のみ低解像度に切り替え

## Success Criteria

- [ ] 再生中に UI 操作（ズーム、パニング、レイヤー選択）が応答する
- [ ] Stop ボタンを押してから 100ms 以内に UI が応答する
- [ ] 再生中の CPU 使用率が UI スレッドで 30% 以下
- [ ] プレビュー フレームレートが安定する（dropped frame が 5% 以下）

## Related Files

| ファイル | 影響 |
|----------|------|
| `ArtifactCore/src/Audio/WASAPIBackend.cppm` | Stop ブロック |
| `ArtifactCore/src/Audio/AudioRenderer.cppm` | Stop チェーン |
| `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` | stop() 呼び出し元 |
| `Artifact/src/Service/ArtifactPlaybackService.cppm` | PNG エンコード、QImage 変換 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm` | renderMutex_ |
