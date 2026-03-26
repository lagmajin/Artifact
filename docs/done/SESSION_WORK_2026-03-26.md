# Session Work 2026-03-26

## Dock Tab Polish

- `DockStyleManager` の tab text への inline style 付与をやめて、tab child を repolish する形に変更した。
- 目的は `ArtifactMainWindow` の stylesheet にある active / floating / hover の selector を潰さないため。
- 後で戻すなら `repolishTabTextWidgets()` 呼び出しを外し、旧 `applyTabLabelColors()` 系に戻すのが復元点。

## Notes

- この修正は見た目の状態管理を stylesheet 側に寄せるためのもの。
- tab label の色を inline で固定しないので、hover と active の競合を避けやすい。
