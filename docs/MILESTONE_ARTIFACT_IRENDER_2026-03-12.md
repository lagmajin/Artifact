# ArtifactIRender Milestone

`ArtifactIRenderer` を「一応描ける薄い wrapper」から、backend 差し替えと editor 描画の基盤として使える段階へ持っていくためのマイルストーン。

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

## First Notes

- 2026-03-12 時点の `ArtifactIRenderer` は、public API は広いが backend 実装はかなり uneven。
- software fallback は `QPainter` ベースで primitive はあるが、transform 系はほぼ no-op。
- Diligent backend parity と text API は未整備。
