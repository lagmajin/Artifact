# Multi-Layer Composite Editor Enhancement

## 概要
マルチレイヤー編集機能の強化。単一レイヤー編集に加え、複数レイヤーの同時編集、差分プレビュー、マスクスタック編集をサポート。

## 実装対象機能

### Phase 1: マスクスタック編集UI
- **ウィジェット**: `ArtifactLayerMaskStackPanel`
- **要件**:
  - マスク一覧表示 (アイコン + 名前 + 有効 / 無効 + モード)
  - 一括操作プルダウン (Enable/Disable/Invert/Type変更)
  - マスク並べ替え (Drag&Drop)
  - 新規マスク追加ボタン (+ シェイプマスク / ペナンルボックス)
- **適用範囲**: 現在レイヤー or 選択レイヤー全て

### Phase 2: 一括トランスフォーム (Multi-Select Transform)
- **対象**: `TransformGizmo` 拡張
- **要件**:
  - `setTargetLayers(std::vector<ArtifactAbstractLayerPtr>)` インターフェース追加
  - マルチ選択時: レイヤー重心に Gizmo 表示
  - 変更適用: 相対変換を全レイヤーに適用
- **Undo**: `BatchTransformEditCommand` 新規追加

### Phase 3: 差分プレビューモード
- **DisplayMode** 拡張:
  - `DiffComposite` - 選択レイヤーの合成差分
  - `DiffMask` - マスク適用前後の差分
  - `SplitView` - 左:フル / 右:選択のみ
- **実装**: 2パスレンダリングによる差分合成

## Timeline Keyframe Enhancement (追加)

### Phase 4: キーフレームスニペット
- よく使うパターン保存/適用
- ショートカットキーでワンクリック適用

### Phase 5: キーフレームブラケット編集
- Shift+ドラッグ: 対称編集
- Alt+ドラッグ: スムージング自動調整
- Ctrl+クリック: キーフレーム複製+移動

### Phase 6: プロパティチャネルフィルタ
- 表示/非表示フィルター (Transform/Audio/Effect)
- 検索バーでプロパティ名フィルタ
- 一度に非表示チャネルをキーフレーム化

### Phase 7: ベイク/フリンジ自動生成
- 選択フレーム範囲からベイクキーフレーム生成
- フリンジ: 近接キーフレームから自動補間

### Phase 8: キーフレームブロック移動
- Ctrl+G: 選択キーフレームをグループ化
- グループ単位で移動/複製/削除

## ArtifactInspectorWidget Enhancement (追加)

### Phase 9: プロパティブロックコピー/ペースト
- プロパティブロックをJSON形式で保存
- マルチレイヤーに一括適用
- コピーメニュー: Layer / Effect / 選択レイヤー全て

### Phase 10: 数値入力スピニング
- Shift+Wheel: 微調整モード (0.1xスケール)
- Ctrl+Wheel: 粗調整モード (10xスケール)
- Alt+Wheel: 極め細かい調整 (0.01xスケール)
- 対象: QDoubleSpinBox / QSpinBox所有小ウィジェット

### Phase 11: マルチプロパティ検索
- 検索バー: 正規表現フィルタ
- 表示/非表示: フィルターにマッチしたプロパティのみ
- 保存: よく使うフィルターをブックマーク

## 影鿿ファイル

| ファイル | 変更種別 |
|---------|---------|
| `ArtifactRenderLayerWidgetv2.cppm` | DisplayMode enum 拡張, diff描画追加 |
| `ArtifactRenderLayerEditor.cppm` | (新規) マスクスタックパネル |
| `TransformGizmo.cpp` | マルチレイヤー対応 |
| `UndoManager.cppm` | BatchTransformEditCommand 追加 |
| `ArtifactTimelineTrackPainterView.cpp` | スニペット/ブロック編集追加 |
| `ArtifactInspectorWidget.cppm` | プロパティブロック/スピニング/検索 |
| `ArtifactPropertyWidget.cpp` | スピニングWheelイベント追加 |

## 技術検討事項

- 既存シグナル/スロットパターンの変更禁止 (AGENTS.md参照)
- QtCSSの新規追加禁止 (QPalette/owner-drawを使用)
- QImageのホットパス採用禁止 (ImageF32x4_RGBA優先)

---

# AngelScript Integration Milestone (別ファイル)

## 概要
UnityのC#相当の役割をAngelScriptが担う。エフェクト・ジェネレータ・ツールのスクリプト化を目指す。

## 実装フェーズ

### Phase AS-1: AngelScriptエンジン組み込み
- **対象**: `ArtifactApplicationManager`
- AngelScript SDK追加 (src/AngelScript/)
- `AngelScriptEngine` singleton登録
- ホスト関数登録: log/print/ArtifactAPI

### Phase AS-2: レイヤー/エフェクトバインディング
- **対象**: `ArtifactAbstractLayer`, `ArtifactAbstractEffect`
- C++ APIをAngelScriptバインド
- 登録API: transform.position/scale/rotation, opacity, blendMode
- スクリプトコンパイル/実行インターフェース

### Phase AS-3: スクリプトウィジェット
- **ウィジェット**: `ArtifactScriptEditorWidget`
- シンタックスハイライト (QTextEditベース)
- コンパイル/エラー表示
- 再読み込みホットキー (F5)

### Phase AS-4: スクリプトツール登録
- **対象**: `ArtifactToolManager`
- スクリプトツールとして登録
- ショートカット/メニュー統合

### Phase AS-5: スクリプトアセット
- スクリプトファイル(.as)をアセットとして認識
- AssetBrowserに表示
- プロジェクト保存/読込統合

## 影鿿ファイル

| ファイル | 変更種別 |
|---------|---------|
| `src/AngelScript/AngelScriptEngine.cppm` | (新規) エンジンラッパー |
| `ArtifactApplicationManager.cppm` | AngelScriptEngine初期化 |
| `ArtifactToolManager.cppm` | スクリプトツール登録 |
| `ArtifactPropertyWidget.cpp` | スクリプトプロパティ編集 |

## 技術検討事項

- AngelScript SDKはthird_party/配下に配置
- C++20 modulesとの兼容性確認
- スクリプトエラーはEventBusで通知