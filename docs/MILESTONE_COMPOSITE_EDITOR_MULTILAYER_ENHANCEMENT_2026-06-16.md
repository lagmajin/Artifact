# Multi-Layer Composite Editor Enhancement

**最終更新:** 2026-08-15

実装確認・追加修正（2026-08-15）: `TransformGizmo` のマルチターゲット基盤を確認し、ドラッグ中にレイヤー選択が差し替わった場合は進行中の変換を取消してから対象を更新するようにした。これにより、ドラッグ開始時の Undo スナップショットと新しい選択対象が混在しない。

同日追加: `setTargetLayers()` で null と同一レイヤー ID の重複を除去し、同一レイヤーへの二重変換・二重 Undo を防止。

同日追加: `ArtifactAbstractLayer::moveMask()` を追加し、マスク列の並べ替えを境界チェックと `maskRevision` 更新付きで行えるようにした。

同日追加: `MoveMaskCommand` を Undo 層へ追加し、マスク順変更の undo/redo を既存の `UndoManager` で扱えるようにした。Drag&Drop UI からの push 接続は次段階。

同日追加: Inspector の既存コンテキストメニューに、複数マスクの各項目の Up/Down 操作を接続。`UndoManager::push()` が変更を実行してから履歴化するため、操作成功時の順序変更と `changed()` 通知を一体化した。

同日追加: 同メニューに全マスク Enable/Disable/Invert を追加し、既存 `MaskEditCommand` のスナップショット Undo を利用。

同日追加: 全マスク内のパスを Add/Subtract/Intersect/Difference に一括変更する操作を追加し、同じく `MaskEditCommand` で Undo 対応。

Phase 3 準備（2026-08-15）: `CompositionRenderController` に `CompositionLayerRenderFilter::{All,SelectedOnly}` を追加。既定値は `All` で、比較用2パスの選択レイヤー描画を既存レイヤーループから開始できる。

同日追加: コンポジション差し替え時にフィルターを `All` へリセットし、選択限定描画状態が別コンポジションへ残らないようにした。

同日追加: `SelectedOnly` は複数選択集合が空の場合、`selectedLayerId_` の単一選択へフォールバックするよう修正。

同日追加: レイヤー選択判定を `passesLayerRenderFilter()` へ切り出し、将来の2パス描画で同じ対象判定を共有できるようにした。

Timeline Phase 4 着手（2026-08-15）: `ArtifactTimelineWidget` に名前付きキーフレームスニペットの保存／適用／削除 API を追加。既存 JSON シリアライズ、Clipboard、Paste の Undo 経路を再利用。

同日追加: Curve Editor ヘッダーに Snippet メニューを接続し、名前入力から保存、既存スニペットの適用／削除を操作可能にした。

同日追加: スニペットを `QSettings` の `Timeline/KeyframeSnippets` グループへ JSON 保存し、Timeline widget の生成時に復元するようにした。

Timeline Phase 5（2026-08-15）: Alt＋ドラッグ確定時に既存 Easy Ease の速度ベース計算を適用し、選択キーフレームを Bezier 化。既存のドラッグスナップショット Undo に含めた。

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

## Update 2026-08-15

- Timeline Phase 6 の基礎として、検索欄の `transform:` / `audio:` / `effect:` 接頭辞でキーフレームマーカーをチャンネル別に絞り込む機能を追加。
- 既存の検索欄とマーカー収集経路を再利用し、追加のシグナル配線やビルドは行っていない。
- 左ペインの Property 行構築にも同じチャンネル分類を適用し、表示内容をマーカーと同期した。
- Curve／Speed Graph のトラック収集にも同じチャンネル条件を適用し、カーブ表示を同期した。
- 検索欄のチャンネル変更時に Timeline／Curve Editor を即時再同期し、非表示キーの選択残留を抑止した。
- `transform:position` のようにチャンネル接頭辞とプロパティ名検索を併用可能にした。
- Phase 7 の導線として、Pattern メニューから選択レイヤーのアニメーションレイヤーを Work Area 全体へベイクできるようにした。既存のスナップショット Undo を利用する。
- 選択キーフレームの先頭／末尾に隣接するフリンジキー生成を追加し、既存のキーフレームスナップショット Undo に接続した。
- Phase 10 の数値入力スピニングを実装。相対 Double／Integer SpinBox で Shift／Ctrl／Alt 修飾ホイールを 0.1x／10x／0.01x として扱い、通常ホイールは無効を維持する。
- Phase 11 の基礎として、個別 Property 名／表示ラベル検索と `/正規表現/` による Property 行絞り込みを追加した。
- 検索欄のコンテキストメニューから名前付きフィルターを `QSettings` に保存／再適用できるようにした。
- Phase 8 を現状コードと照合。`collectKeyframeAreas()` と Area Body／Edge ドラッグにより、選択キーのブロック単位移動・伸縮・複数トラック連動は実装済み。`Ctrl+G` は既存の Curve Editor 切替に割り当て済みのため、ショートカット上書きは保留。
- Phase 9 を現状コードと照合。`serializeSelectedKeyframes()` はレコードごとに `propertyPath` を保持し、既存 Clipboard／Paste 経路で複数プロパティを選択レイヤーへ一括適用できるため、追加の Property Block 形式は作らない。
