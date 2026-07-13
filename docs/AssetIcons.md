# Asset Icons List

このドキュメントは、AssetBrowserやAssetDirectoryModel等で使用するアイコンの種類と対応するファイル名を列挙するためのものです。

## 格納場所

Asset Browser と Project View で使うオリジナル SVG は
[`App/Icon/Studio/`](../App/Icon/Studio/) に格納する。

## アイコン種類とファイル名

| アイコン種別 | ファイル |
|---|---|
| 画像アセット欠損サムネイル / プレビュー | [`asset_missing.svg`](../App/Icon/Studio/asset_missing.svg) |
| 画像アセット欠損小アイコン | [`asset_missing_small.svg`](../App/Icon/Studio/asset_missing_small.svg) |

## Menu Icon Refresh Log

AI が同じメニューを何度も刷新しないための作業ログ。

| 日付 | 対象 | プレフィックス | 状態 | メモ |
|---|---|---|---|---|
| 2026-06-03 | File menu | `filemenu_` | 完了 | 承認済み VS-like File Menu アイコン。ユーザーが明示しない限り編集・置換しない。 |
| 2026-06-03 | Edit menu | `editmenu_` | 完了 | 専用 SVG セット化済み。 |
| 2026-06-03 | Composition menu | `compositionmenu_` | 完了 | 専用 SVG セット化済み。 |
| 2026-06-03 | Render menu | `rendermenu_` | 完了 | 専用 SVG セット化済み。 |
| 2026-06-03 | Script menu | `scriptmenu_` | 完了 | 専用 SVG セット化済み。 |
| 2026-06-03 | Help menu | `helpmenu_` | 完了 | 専用 SVG セット化済み。 |
| 2026-06-03 | Time menu | `timemenu_` | 完了 | Playback / step / marker / In-Out / loop / timer を専用 SVG 化。欠落参照だった `timer.svg` と `keyboard_double_arrow_*.svg` も解消。 |
| 2026-07-13 | Animation menu | `animationmenu_` | 完了 | 旧式の色付きタイルを廃止し、透明背景・グレー骨格・意味色アクセントの専用 SVG へ統一。 |

### 再刷新ルール

- 上の `完了` メニューは、ユーザーが明示的に「再デザイン」「差し替え」「統一調整」を依頼した場合だけ触る。
- 新しいメニュー項目が追加された場合は、既存プレフィックスに合わせて不足アイコンだけ追加する。
- 汎用 `Studio/*.svg` を使っているメニューを専用プレフィックス化する場合は、このログに先に追記する。

### Visual Direction

- VS / VSCode の file icon theme に近い、小さなフラット glyph を優先する。
- 大きな外枠、過剰な stroke、パネル絵のような重い構成は避ける。
- 16px 表示で判別できる単純なシルエットにする。
- 色は意味別に絞る。例: blue = navigation / surface, green = play / enabled, red = stop / delete, yellow = marker / warning, gray = neutral structure.
- File Menu の `filemenu_*.svg` は承認済み基準。新規メニュー専用アイコンはこの方向性に寄せる。

## Future Non-Menu Icon Candidates

メニュー以外で今後使う可能性が高い候補。まずは `MaterialVS` 参照や空アイコンが残っている UI から置き換える。

| 優先度 | 対象 UI | 候補プレフィックス | 欲しいアイコン |
|---|---|---|---|
| 高 | Composition Editor top toolbar / tool menu | `tool_` | `tool_select.svg`, `tool_hand.svg`, `tool_pen.svg`, `tool_fit.svg`, `tool_zoom_100.svg`, `tool_reset_view.svg` |
| 高 | 3D gizmo actions | `gizmo_` | `gizmo_move.svg`, `gizmo_rotate.svg`, `gizmo_scale.svg`, `gizmo_local_global.svg`, `gizmo_crop.svg` |
| 高 | Viewer footer playback / capture | `viewer_` | `viewer_snapshot.svg`, `viewer_capture.svg`, `viewer_play.svg`, `viewer_pause.svg`, `viewer_stop.svg` |
| 高 | Timeline / layer panel switches | `timeline_` | `timeline_visibility_on.svg`, `timeline_visibility_off.svg`, `timeline_audio.svg`, `timeline_shy.svg`, `timeline_link.svg`, `timeline_lock.svg` |
| 中 | Render Queue job panels | `queue_` | `queue_expand.svg`, `queue_collapse.svg`, `queue_start.svg`, `queue_pause.svg`, `queue_error.svg`, `queue_complete.svg` |
| 中 | Asset Browser / Project View asset kinds | `asset_` | `asset_image.svg`, `asset_video.svg`, `asset_audio.svg`, `asset_comp.svg`, `asset_folder.svg`, `asset_script.svg`, `asset_model3d.svg` |
| 中 | Property Editor keyframe / expression controls | `property_` | `property_keyframe_off.svg`, `property_keyframe_on.svg`, `property_expression.svg`, `property_reset.svg`, `property_prev_key.svg`, `property_next_key.svg` |
| 低 | Diagnostics / debugger surfaces | `diagnostics_` | `diagnostics_event.svg`, `diagnostics_warning.svg`, `diagnostics_error.svg`, `diagnostics_perf.svg`, `diagnostics_log.svg` |

## アイコン方針

- 新規アイコンは `Material` 系の参照を増やさず、`Artifact/App/Icon/Studio/` に置くオリジナル SVG を優先する。
- 見た目は VS / VSCode の file icon theme に近いソリッド寄り、太めのシルエット、高コントラストを優先する。
- 16px でも読めることを優先し、細い線や装飾過多は避ける。
- アイコン未設定のメニューや辞書項目があれば、この方針で補完する。
