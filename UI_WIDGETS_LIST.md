# UIウィジェット一覧と機能まとめ

## レイヤー・編集系
- ArtifactLayerEditorWidgetV2: レイヤー編集用のメインビュー。マウス・キーボードイベント、ズーム・パン・ターゲットレイヤー操作、スクリーンショット取得など。
- ArtifactRenderLayerEditor: ArtifactLayerEditorWidgetV2をラップし、下部にツールバー（再生・停止・スクリーンショット）を持つ複合ウィジェット。
- ArtifactLayerPanelWidget: タイムライン上のレイヤーリスト表示・編集。レイヤーの選択・表示・ペイント・マウス操作。
- ArtifactLayerPanelHeaderWidget: レイヤーパネルのヘッダー。ロック・ソロボタンなどの操作シグナル。
- ArtifactLayerTimelinePanelWrapper: タイムラインパネルのラッパー。複数コンポジション対応。
- ArtifactLayerEditorPanel: 2Dレイヤーエディタとフッター（スナップショットボタン等）を組み合わせた複合パネル。
- ArtifactLayerHierarchyHeaderContextMenu: レイヤーヒエラルキーのヘッダー用コンテキストメニュー。表示切替など。
- ArtifactLayerHierarchyHeaderView: レイヤーヒエラルキーのヘッダー表示・コンテキストメニュー呼び出し。

## プレビュー・表示系
- ArtifactRenderLayerWidgetv2: レイヤー描画・編集用のQWidget派生。イベントハンドラ多数。
- ArtifactCompositionViewerFooter: プレビュー下部のコントロールバー。解像度選択・スナップショットボタン等。

## プロパティ・ヘルプ系
- ArtifactInspectorWidget: レイヤーやプロパティの詳細表示・編集。ラベル・メニュー・コンテキストメニュー・プロジェクト連携。
- ArtifactHelpMenu: ヘルプメニュー（QMenu派生）。ヘルプ関連の操作。
- ArtifactMenuBar: メインメニューバー。ファイル・編集・表示・ヘルプ等のメニューを統括。
- ArtifactMenu: メニュー管理。各種メニュー（ファイル・編集・レイヤー・エフェクト等）を統括。

## コントロール・再生系
- ArtifactPlaybackControlWidget: 再生・停止・シーク・フレーム移動などの再生コントロール。

## ダイアログ・その他
- ArtifactCreateCompositionDialog: 新規コンポジション作成ダイアログ。初期設定入力。

---

このリストはAI/人間どちらにも分かりやすいように、クラス名と主な機能を簡潔にまとめています。
