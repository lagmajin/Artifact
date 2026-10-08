# Particle Render Path Stabilization

**最終更新:** 2026-10-08

`ArtifactParticleLayer` の GPU billboard 描画が「時々見えない / 状態依存で壊れる」問題を、render entry・matrix・RTV・PSO 前提の整理で安定化するためのマイルストーン。

## Goal

- particle layer を composition editor 上で安定して見えるようにする
- billboard draw path の前提条件を `ArtifactIRenderer` 側で局所化する
- `ParticleRenderer` の責務と `ArtifactParticleLayer` の責務を分け直す

## Current Understanding

particle 非表示の本命は、billboard shader 単独故障というより次の複合。

- RTV が particle draw 前に unbind される
- view / projection matrix が状態依存
- PSO / SRB / buffer 初期化失敗時のガードが弱い
- `drawParticles()` が target setup と draw を同時に抱える

## Phases

### Phase 1: Draw Entry Audit

- `ArtifactIRenderer::drawParticles()` の前提条件を固定
- pending submit flush / RTV bind / matrix upload / prepare / draw を整理

### Phase 2: Billboard Contract Freeze

- `ScreenAligned` / `ViewPlane` の最小契約を固定
- particle size / blend / depth / sort の意味を renderer 側で明文化

### Phase 3: Layer / Renderer Split

- `ArtifactParticleLayer` は scene / emission / simulation 所有に寄せる
- `ParticleRenderer` は draw 専用に寄せる

### Phase 4: Diagnostics

- `Trace` / `Frame Debug` / `Pipeline View` から particle draw path を追えるようにする
- particle draw skipped / no RTV / PSO null / empty buffer を summary で読めるようにする

### 2026-10-08: 再生診断の自動保存（実機未確認）

粒子を描画した再生は、遅延・描画失敗の有無によらず、既存の750ms診断観測後の一時停止／停止で自動保存する。保存先はアプリデータの `PlaybackDebugReports/playback_<日時>_<コンポジション名>/`。`frame-debug-bundle.json` と、取得できた `layer-rt.png`、`accum-rt.png`、`viewport-final.png` を保存する。JSONの `imageFiles` に画像名と保存成否を記録する。既存のテキストレポートは同ディレクトリの親に残す。

自動観測中の `frameDebugSnapshot(false)` は画像のGPU読み戻しを行わず、一時停止／停止後に1回取得する。画像保存は既存QImage読み戻し境界の出力処理として使用する。途中フレームを連続録画する機能ではなく、750ms以内に再生を終えた場合は開始を観測できない場合がある。通常停止がフレームを戻す場合は、一時停止で異常の出たフレームを保持する。ビルド・実機検証は未実施。

直接描画経路では中間パイプラインが存在しないため、Layer RT／Accum RTは `unavailable=no-intermediate-pipeline` とする。最終SRVがない場合のスクリーンショット境界は、ネイティブDiligentサーフェスを取得できない `QWidget::grab()` の代わりに、表示中のビューポート矩形を画面から取得する。この場合はウィンドウが他のウィンドウに隠れていないことが必要。粒子診断には描画先と固定PSOのフォーマット、および一致判定を追加した。これらの変更は実機未確認。

### 2026-10-08: GPU検証メッセージの保存（未ビルド）

停止後の画像付き診断に `GPU Validation` リソースを追加した。D3D12では既存デバイスの `ID3D12InfoQueue` を参照し、直近最大32件から警告以上のメッセージ、ID、デバイス削除理由を保存する。検証要求の設定、キューの利用可否、蓄積件数、破棄件数、取得不能件数を明示する。共有デバイス全体の履歴であり、粒子の描画だけに帰属する情報ではない。Vulkanはこのネイティブ診断の対象外と明示する。

再生自動観測の `frameDebugSnapshot(false)` ではキューを参照しない。停止後に固定8KiBの作業領域で取得し、本文は各1024バイトまでとする。GPU待機・読み戻し・キュー消去・新規コールバック配線は行わない。QStringの整形と診断resourceの追加は停止後／手動取得時だけ行う。キューが利用不能、32件より古い、非同期のGPU検証が未完了、またはエラーが出ないまま誤ったリソースを使う場合は、この診断だけでは原因を確定できない。

既存保存例は粒子数64未満の直接描画だったため、そのフレームではcompute culling／indirect drawを使用していない。CPU送信値とHLSLの構造体は72バイトの契約に揃い、PSは入力RGBをそのまま出力するが、これは実GPUのSRV内容・PSO結合の証拠にはならない。ビルド・テスト・実機確認は行っていない。

## Completion Criteria

- particle layer が composition editor 上で安定表示される
- draw failure の原因がログと summary で追える
- `drawParticles()` の責務が renderer internal helper に分かれる

## Related

- `docs/bugs/BUG_PARTICLE_GPU_RENDER_2026-04-19.md`
- `docs/bugs/PARTICLE_BILLBOARD_ROOT_CAUSE_FIX_2026-03-27.md`
- `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_PHASE2_EXECUTION_2026-04-21.md`
- `Artifact/docs/MILESTONE_PARTICLE_LAYER_3D_MIGRATION_2026-03-25.md`
