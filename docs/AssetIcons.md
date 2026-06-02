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

## アイコン方針

- 新規アイコンは `Material` 系の参照を増やさず、`Artifact/App/Icon/Studio/` に置くオリジナル SVG を優先する。
- 見た目はソリッド寄り、太めのシルエット、高コントラストを優先する。
- 16px でも読めることを優先し、細い線や装飾過多は避ける。
- アイコン未設定のメニューや辞書項目があれば、この方針で補完する。
