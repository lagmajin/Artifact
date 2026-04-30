# Composition Editor Milestone (2026-03-21)

`コンポジションを開く -> レイヤーを見ながら編集する -> セーフエリアやグリッドを切り替える -> タイムラインと同期する`
という日常導線を、破綻なく使えるレベルまで固めるためのマイルストーン。

## Goal

- `ArtifactCompositionEditor` を中心に、編集時に最もよく触る表示と操作を安定させる
- コンポジションビューとタイムラインの同期を自然に保つ
- セーフエリア、グリッド、ズーム、フィット、再生制御を迷わず扱えるようにする
- プレビュー時の見え方を、ソフトウェアレンダリングと Diligent 側で近づける

## Definition Of Done

- 新規コンポ作成後、composition editor がそのコンポを表示する
- current composition の切替に view が追従する
- zoom / fit / 100% / reset / play / stop が壊れずに動く
- grid / guides / safe area を editor 側で切り替えられる
- スマートガイドで整列スナップと距離表示が扱える
- selection と hit test が editor 内で破綻しない
- image / solid / video / text の基本表示が editor 上で確認できる
- pivot の切り替えと gizmo space の選択が editor 内で扱える

## Work Packages

### 1. Viewport Behavior
- `CompositionViewport` の focus / mouse / wheel / resize の挙動を安定させる
- active composition の切替時に view が追従する
- zoom 操作中にカーソルと描画位置がずれないようにする

### 2. Editor Controls
- `Reset / Zoom+ / Zoom- / Fit / 100%` の操作を整理する
- bottom bar の表示切替をわかりやすくする
- safe area / grid / guides の切替を editor 内で見つけやすくする
- Figma 風スマートガイドを追加し、レイヤー端・中心・コンポ中心/端へのスナップを見える化する
- レイヤー間距離のリアルタイム表示を追加し、等間隔や揃いの検出を補助する
- screenshot / snapshot 導線を editor 内に持ち、Quick と Advanced を分けて保存できるようにする
- Advanced 側は再利用可能な独立 dialog に切り出して、他の編集画面からも呼べるようにする

### 3. Composition Visibility
- 画像、平面、動画、テキストの基本レイヤー表示を安定化する
- layer order 変更が editor 上の描画順と一致するようにする
- selection 時のヒット判定を editor と timeline で揃える
- ドラッグ中のスマートガイドで整列候補を強調する

### 4. Timeline Bridge
- composition 作成後に新しい timeline dock を前面に出す
- editor と timeline の current composition を同期する
- layer selection から inspector / property panel への流れを維持する

### 5. Preview Parity
- software preview と editor preview の見え方をできるだけ一致させる
- safe area / grid / guides の見え方を共通化する
- CPU 側エフェクトを editor preview で確認できる経路を保つ

### 6. Pivot / Gizmo Space
- 変形基準点を切り替えられるようにする
- global / local の gizmo space を editor 内で切り替えられるようにする
- 3D cursor 的な pivot を置いて、選択レイヤーの操作基準を明示できるようにする
- pivot 切替が move / rotate / scale の各 gizmo と破綻しないようにする
- pivot / gizmo の補助線と smart guide が干渉しないようにする

## Current Status

- `2026-03-21`
  - `ArtifactCompositionEditor` に toolbar / bottom bar / safe area toggle がある
  - `CompositionViewport` は current composition 追従と zoom 操作を持つ
  - timeline 作成後の dock activation と focus の改善を進めている
  - layer drag reorder、selection、preview log の整理を同時に進めている
  - pivot 変更の最小導線として `Pivot: Center / Top Left` を toolbar から呼べるようにした
  - `F11` で immersive fullscreen を切り替えられるようにした
  - screenshot は `Quick` と `Advanced` に分け、Advanced は独立 dialog として再利用できるようにした
  - pivot / gizmo space 切替は今後の編集 UX 拡張として追加する
  - レイヤー移動中のスマートガイドは今後の編集 UX 拡張として追加する

## Suggested Order

1. `Viewport Behavior`
2. `Editor Controls`
3. `Timeline Bridge`
4. `Composition Visibility`
5. `Preview Parity`
6. `Pivot / Gizmo Space`

## Follow-up Notes

- 2026-03-21:
  - safe area は既存の `Safe Margins` toggle を editor 内で露出している
  - 新規 composition 生成後は timeline dock を activate して、編集対象の流れを切らさない方向で調整中
  - 左 timeline pane の layer reorder は internal drag で扱うように整理し始めている
  - timeline の `inPoint` / `outPoint` は表示有効区間、`startTime` はソース側オフセットとして扱う
  - 右パネルのクリップ移動は `in/out` の移動、エッジ操作は `in/out` のトリムとして分離する
  - right panel の clip 編集は `layerClipMoved` / `layerClipTrimmed` 経由で実データへ反映し、`projectChanged()` で再描画を起こす
