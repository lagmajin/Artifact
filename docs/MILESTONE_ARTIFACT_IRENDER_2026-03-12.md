# ArtifactIRender Milestone

**最終更新:** 2026-08-15
**現状:** 2D／3D primitive、software／Diligent 経路、overlay／diagnostics の基盤は拡張済み。backend parity、旧 painter 経路の整理、性能受入れは未完了。

`ArtifactIRenderer` を「一応描ける薄い wrapper」から、backend 差し替えと editor 描画の基盤として使える段階へ持っていくためのマイルストーン。

後続の renderer boundary 整理として、`ImmediateContext` の直叩きを外側から減らす更新は
`docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md` を参照。

## M-IR-1 API Surface Cleanup

- 目標:
  `ArtifactIRenderer` の責務を明確化し、canvas/view transform と primitive API を安定させる。
- 対象:
  viewport、canvas、pan、zoom、local/global draw API、clear/present lifecycle。
- 完了条件:
  - software fallback と Diligent backend で同じ public API が通る
  - transform 系が no-op ではなく意味を持つ
  - preview/timeline overlay から利用しても座標系が破綻しない

## M-IR-2 Software Backend Hardening

- 目標:
  Qt painter fallback を editor 用 2D renderer として実用にする。
- 対象:
  line / bezier / sprite / checkerboard / grid / clipping / text / image draw。
- 完了条件:
  - layer preview / gizmo / overlay の基本描画を software で賄える
  - resize / present / repaint で描画崩れが出にくい
  - missing backend 時の fallback として安定する

## M-IR-3 Diligent Backend Parity

- 目標:
  Diligent 側でも software と近い 2D primitive を出せるようにする。
- 対象:
  PSO/SRB 管理、sprite、solid rect、line、grid、texture upload。
- 完了条件:
  - preview widget で backend を切っても primitive レベルの差異が小さい
  - image draw と alpha blend が一致する

## M-IR-4 Editor Overlay Support

- 目標:
  viewer / timeline / gizmo で共通利用できる editor overlay 描画基盤を整える。
- 対象:
  playhead、selection box、guide、transform gizmo、anchor、bounds。
- 完了条件:
  - overlay は widget 依存ロジックを減らして renderer API へ寄せる
  - hit test 以外の見た目は renderer 側に統一できる

## M-IR-5 Text / Diagnostics

- 目標:
  debug HUD と editor annotation を renderer API で出せるようにする。
- 対象:
  drawText、fps/debug overlay、safe area label、bounds info。
- 完了条件:
  - preview 側の debug 表示を painter 直書きから減らせる

## M-IR-6 Performance Optimization

- 目標:
  フレームキャッシュ、マルチスレッド（例: `std::jthread`）、GPUプロファイリングを導入し、レンダリング性能を向上させる。
- 対象:
  キャッシュ戦略、非同期レンダリング、バックエンド切り替え時のアーティファクト回避。
- 完了条件:
  4Kレンダーを30fpsでベンチマーク可能。

## M-IR-7 Advanced Primitives

- 目標:
  3Dギズモ、ベクトルパス、アンチエイリアスラインをサポートし、VFX機能を強化。
- 対象:
  プリミティブ拡張、シェーダー統合。
- 完了条件:
  複雑なエフェクト（例: Object Fracture）がレンダラーで処理可能。

## First Notes

- 2026-03-12 時点の `ArtifactIRenderer` は、public API は広いが backend 実装はかなり uneven。
- software fallback は `QPainter` ベースで primitive はあるが、transform 系はほぼ no-op。
- Diligent backend parity と text API は未整備。
- 2026-03-30: M-IR-6/7 を追加し、性能と拡張性を強化。

## Current Code Audit (2026-08-15)

- `ArtifactIRenderer` は 2D／3D primitive、offscreen、readback、frame timing／diagnostics の利用基盤が現行コードに存在する。
- M-IR-2 の再監査では、`PrimitiveRenderer2D` に QPainter fallback はなく、`RenderCommandBuffer`／GPU primitive 経路が中心であることを確認した。QPainter は `OffscreenRenderer2D` と `ArtifactSoftwareImageCompositor` の別経路に残っており、これらを `ArtifactIRenderer` の共通 software backend と見なすことはできない。
- M-IR-3 の再監査では、GPU 側の `PrimitiveRenderer2D`／`ArtifactIRenderer` に rect、line、Bezier、grid、sprite、text、gradient の共通 façade がある一方、software 側には同じ primitive 契約を実装する renderer backend が存在しないことを確認した。したがって現状は「GPU primitive の実装拡張」であり、backend parity の完了条件を満たさない。
- M-IR-4 の再監査では、3D gizmo は `PrimitiveRenderer3D`／`ArtifactIRenderer` の draw façade を使用し、composition view の layer／surface 描画も `ArtifactIRenderer` を使用している。一方、timeline の playhead／panel／scrub 表示は `QPainter` による widget owner-draw のままで、viewer／timeline／gizmo の overlay を一つの renderer API に統合した状態ではない。timeline の hit-test と paint を一括移行するのは責務境界と座標系の設計変更を伴うため、現状は partial とする。
- M-IR-5 の再監査では、`PrimitiveRenderer2D::drawText()`／`drawTextTransformed()` と 3D grid／gizmo 周辺の renderer text façade は存在するが、`TraceTimelineWidget`、`ProfilerPanelWidget`、`DebugRenderHarnessWidget` などの diagnostics／HUD は `QPainter::drawText()` を直接使用している。したがって text primitive 自体は実装済みでも、preview 側の debug 表示を renderer API に集約する完了条件は未達である。
- M-IR-6 の再監査では、`FrameCache`／`RenderPerformanceMonitor`／`ArtifactRenderScheduler` の frame cache・frame time・FPS 計測、`ArtifactIRenderer` の環境変数 gated GPU duration query、shader／texture／mesh cache が存在することを確認した。一方、4K/30fps の再現可能な benchmark harness／記録、非同期 renderer pipeline の受入れ、backend 切替時の artifact 回避を証明する runtime データは存在しない。性能基盤は部分実装だが、完了条件は未達とする。
- M-IR-7 の再監査では、3D gizmo の line／arrow／ring／torus／cube、2D Bezier／arc／thick line、fwidth edge-AA 用 shader、fracture overlay の renderer 接続を確認した。一方、汎用 vector-path／fill rule API と複雑な VFX を renderer primitive として統一する契約はなく、Object Fracture も overlay／layer 経路の範囲に留まる。advanced primitive の基盤は部分実装で、完了条件は未達とする。
- M-IR-8 の再監査では、2D／3D の通常 primitive は packet／submitter／renderer façade に寄っているが、`ArtifactCompositionRenderController`、render queue、effect/pass、layer の一部が `renderer_->immediateContext()`／`IDeviceContext` を直接取得していることを確認した。`ArtifactIRenderer::immediateContext()` 自体も公開 API として残っており、context access narrowing と全上位入口の統合は未完了である。
- M-IR-9 の再監査では、`FrameDebug`／`Trace`、render context／frame snapshot、fallback／skip／crash の観測点と、façade → controller → particle → access narrowing の再開順が安全ゲート文書に固定されていることを確認した。これは変更再開の条件を整えた状態であり、低レベル依存の縮小、particle helper 化、backend／render-target 復帰、snapshot 並列の runtime 受入を完了したことを意味しない。
- `ArtifactCompositionViewDrawing` は `ImageF32x4_RGBA`、LOD、GPU texture cache、surface cache を利用しており、初期文書の「software fallback と Diligent backend が uneven」という記述だけでは現状を表せない。
- 最終コンポジションエフェクトに `applyCompositionFinalEffectsToBuffer()` を追加し、QImage境界を必要としない typed-buffer 呼び出し口を整備した。既存のQImage APIは互換維持のラッパーとして残している。
- GPU Render Queue のbeauty readbackに `ArtifactIRenderer::readbackToImageF32()` を接続し、crop／最終エフェクトをtyped buffer上で処理してから出力QImageへ変換する経路を追加した。
- 一方、QPainter／QImage を含む旧互換・surface・matte・effect 経路と renderer readback は残っている。M-IR-2／M-IR-3 の parity、M-IR-4／M-IR-5 の全面的な renderer API 集約、M-IR-6 の 4K/30fps ベンチマークはコード存在だけでは完了と判定できない。
- 判定: M-IR-1〜5 は基盤実装が進展した partial、M-IR-6〜7 は未受入れ。runtime／性能測定なしで完了扱いにはしない。

## Update 2026-08-15 — M-IR-1 API Surface

`ArtifactIRenderer` の現行 façade と `PrimitiveRenderer2D`／`PrimitiveRenderer3D` の委譲を再確認した。viewport／canvas size、pan／zoom、zoom-around-point、canvas↔viewport 変換、2D／3D primitive、offscreen／readback は public surface として成立している。通常の描画呼び出しは primitive renderer／submitter 側へ寄っており、初期資料の「transform はほぼ no-op」という記述は現状には当たらない。

残る M-IR-1 の課題は、`immediateContext()` を含む低レベル API の公開縮小、software backend との同一契約、QImage readback／sprite 境界の整理、backend parity の runtime 確認である。したがって API surface は実装進展済み、cleanup の完了判定は pending とする。
