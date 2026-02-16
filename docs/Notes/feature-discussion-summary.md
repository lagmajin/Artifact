議論メモ — 開発トピック要約

日付: （自動記録なし）

1) 直近で実装・検討した内容
- Verdigris移行とビルド修正（STLのグローバルモジュール断片移動等）。
- Inspector: プロパティUIを双方向（編集 -> model）に変更。QDoubleSpinBox等を導入。
- Undo/Redo: `UndoManager` と `SetPropertyCommand` の骨組みを追加。
- goToFrame: `ArtifactAbstractLayer` に最小同期評価パスを実装（CPUバックエンドでeffects順次適用のフォールバック）。
- 画像コア: `ImageF32xN` 等の多チャネル対応、RGBA互換ユーティリティ、一部YUVヘルパーを追加。
- オーディオ: 抽象 `IAudioDevice` を導入（WASAPI/PortAudioの準備）。
- マスク/ロトスコープ: 新しい MaskPath / LayerMask モジュールを試作（ベジェ→アルファラスタライズ、合成モード、フェザー、膨張）。
- 簡易AIクライアントとチャットUIをダミー実装（`AIClient` + `AIChatWidget`）。
- シンプル翻訳モジュール `LocalizationManager`（eng/jpのミニ辞書）を追加。

2) 現状の課題 / ビルド障害（要注意）
- OpenCV/Qt/TBB のリンカ未解決シンボルが散見される（モジュール追加・削除の過程で発生）。
- 新規 `.cppm` を Visual Studio プロジェクトに手動で追加する必要あり（自動編集禁止ルール）。
- 一部モジュールでの `import` 順序やグローバル断片の不足によるコンパイルエラー。

3) 今後の優先事項（提案）
- 優先: Undo/Redo 完全統合（Inspectorから`SetPropertyCommand`をプッシュ）、プロジェクト永続化（effectプロパティの toJson/fromJson）。
- 次点: VideoLayer のフレームデコード統合 + プロキシワークフロー（大容量素材対応）。
- 中長期: マスク/ロトスコープのUI（パス編集）、モーショントラッキング、カラーグレーディング(スコープ表示) など。

4) AI・埋め込み（短期方針）
- 目的: プロパティの意味検索・類似提案のために、プロパティをテキスト化してメモリ上で埋め込みベクトル化する。
- 最小実装: ダミーの埋め込み関数（テキストハッシュ→floatベクトル）を用意し、`std::vector<std::vector<float>>` とメタデータ配列で保持。クエリは線形検索（内積 / 正規化済みベクトル）。
- 拡張: 余裕があれば sentence-transformers / OpenAI embeddings に差し替え、FAISS等に移行。

5) 翻訳ソリューション
- Qt tr に依存しないシンプルな `LocalizationManager` を導入（`eng`/`jp` の内蔵ミニ辞書）。

6) 次アクション（ユーザ選択待ち）
- 例: 「Undo 完全化」「VideoLayer 統合」「マスクUI」「AI埋め込みの in-memory 実装」を選択して着手可能。

---
このファイルはリポジトリ内 `docs/Notes/feature-discussion-summary.md` に保存しました。必要なら別フォーマット（Issue / TODO リスト / RFC）に変換します。