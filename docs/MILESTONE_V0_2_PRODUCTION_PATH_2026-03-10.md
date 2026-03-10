# Artifact v0.2 Production Path (2026-03-10)

このマイルストーンは、v0.1 の「使える最低限」から、実運用に耐える編集体験へ進めるための短期計画です。

## Goal

次の 8 項目を満たしたら v0.2 を達成とする。

1. タイムライン操作の境界不具合ゼロ化
2. コンポジション選択と全ビュー同期の完全化
3. レイヤー作成ダイアログ導線の統一
4. レンダーキューのジョブ編集（出力先・範囲）対応
5. ダミー出力から最小実出力（1フレーム）へ拡張
6. Dockレイアウト初期配置の固定プリセット化
7. メニュー/コンテキストメニューの有効条件厳格化
8. クラッシュしやすい経路のガード追加（防御的実装）

## Definition Of Done

- 主要操作でクラッシュしない
- 無効操作は必ず disabled か警告を返す
- 失敗時は UI 上で原因が分かる（ステータス/ダイアログ）
- 同じ操作で結果が揺れない（再現性）

## Work Breakdown

### 1. タイムライン境界の安定化

- 対象:
  - `src/Widgets/Timeline/ArtifactTimelineObjects.cpp`
  - `src/Widgets/ArtifactTimelineWidget.cpp`
- 完了条件:
  - 左右ハンドルが逆転しない
  - クリック領域競合（シーク/ハンドル）が再発しない
  - 1フレーム単位移動が常に一致

### 2. コンポ選択同期の完全化

- 対象:
  - `src/Service/ArtifactProjectService.cpp`
  - `src/AppMain.cppm`
  - `src/Widgets/ArtifactProjectManagerWidget.cppm`
- 完了条件:
  - Project View 選択 -> Timeline / Composition Viewer / Layer View が同時追従
  - `currentCompositionChanged` を単一ソースとして使用

### 3. レイヤー作成導線統一

- 対象:
  - `src/Widgets/Menu/ArtifactLayerMenu.cppm`
  - `src/Widgets/Dialog/CreatePlaneLayerDialog.cppm`
  - `src/Service/ArtifactProjectService.cpp`
- 完了条件:
  - メニュー/右クリックのどちらでも同じダイアログが開く
  - 作成後に Project / Timeline / Inspector へ反映

### 4. レンダーキュー編集機能

- 対象:
  - `src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
  - `src/Render/ArtifactRenderQueueService.cppm`
- 完了条件:
  - ジョブごとに出力先・開始/終了フレームを編集可能
  - UI表示と内部キューの内容が一致

### 5. 最小実出力

- 対象:
  - `src/Render/ArtifactRenderQueueService.cppm`
  - `src/Render/*` / `src/Export/*`
- 完了条件:
  - 1フレーム実レンダ結果を PNG 保存
  - 失敗時はエラー理由をジョブ単位で表示

### 6. Dock初期レイアウトプリセット

- 対象:
  - `src/AppMain.cppm`
  - `src/Widgets/ArtifactMainWindow.cppm`
- 完了条件:
  - 起動直後に主要パネルが意図位置に配置
  - 余計な補助パネルは初期非表示

### 7. メニュー有効条件の厳格化

- 対象:
  - `src/Widgets/Menu/*.cppm`
  - `src/Service/ArtifactProjectService.cpp`
- 完了条件:
  - 「対象なし」で実行できない項目は disabled
  - UI層は Service API のみ利用

### 8. クラッシュ予防ガード

- 対象:
  - `src/Widgets/*`
  - `src/Service/*`
- 完了条件:
  - null/expired参照の保護
  - 破棄後コールバックの解除徹底

### 9. Software Renderer R&D (Parallel Track)

- 対象:
  - `src/Widgets/Render/ArtifactSoftwareRenderTestWidget.cppm`
  - `src/Widgets/Menu/ArtifactTestMenu.cppm`
- 完了条件:
  - `QImage/QPainter` と `OpenCV` の合成バックエンドを切替可能
  - 画像2枚重ね + ブレンドモード切替 + 不透明度調整 + PNG保存が可能
  - OpenCVエフェクト（最低2種）をリアルタイム確認できる
- ルール:
  - 本トラックは v0.2 本線と並行実施
  - 本線のAPI/設計を汚染しない（テストウィジェット内に閉じる）

#### Progress Snapshot (2026-03-10)

- 実装済み:
  - 画像2枚重ね + ブレンドモード切替 + 不透明度調整 + PNG保存
  - バックエンド切替（`QImage/QPainter` / `OpenCV`）
  - OpenCVエフェクト2種（`GaussianBlur` / `EdgeOverlay`）
  - 共通合成モジュール `Artifact.Render.SoftwareCompositor` を追加
  - `RenderQueue` のダミー出力で共通合成モジュールを利用開始
  - `RenderQueue` ジョブごとの合成変形（X/Y, Scale, Rotation）編集UIを追加
  - `RenderQueue` 操作に `Duplicate / Move Up / Move Down` を追加
  - `RenderQueue` ジョブごとの出力先/開始フレーム/終了フレーム編集UIを追加
  - `RenderQueue` ジョブごとの出力フォーマット/コーデック/解像度編集UIを追加
  - `RenderQueue` ジョブごとの `FPS / Bitrate` 編集UIを追加
  - `RenderQueue` リスト表示を Service のジョブ情報（name/status/progress）同期ベースへ寄せる改善を追加
  - `RenderQueue` 履歴表示（開始/完了/失敗/キャンセル/出力先）を追加
  - `RenderQueue` 履歴のセッション永続化（再起動後復元）を追加
  - `RenderQueue` の再実行導線を追加（`Rerun Selected` / `Rerun Done/Failed`）
  - `RenderQueueService` に再実行リセットAPI（単体/一括）を追加
  - `RenderQueue` ジョブ設定プリセット保存/適用/削除を追加（FastSettingsStore）
  - 履歴永続化バックエンドを `QSettings` から `ArtifactCore::FastSettingsStore`（CBOR+キャッシュ）へ移行
  - MainWindow レイアウト保存/復元も `FastSettingsStore` 経由へ移行（段階的QSettings脱却）
  - Python Hook の有効/無効設定保存も `FastSettingsStore` へ移行
  - 旧 `QSettings` からの一回移行フォールバックを追加（互換維持）
  - `ArtifactCompositionMenu` のマイルストーン実行を `RenderManager` 直呼びから `RenderQueueService` 経由へ置換（UI->Serviceルール順守）
  - `Composition/Layer` メニューの有効条件を厳格化（プロジェクト未作成時は無効、作成系はプロジェクト単位で有効）
  - RenderQueue の実行中メッセージを強化（時刻付きステータス、サービスイベント、進捗25%刻み履歴）
- 次:
  - ジョブ並べ替え時の選択保持を Service 側インデックス変動込みで厳密化
  - Queue履歴にジョブ設定スナップショット（出力設定）を記録

## Execution Order

1. `1 -> 2`（編集体験の核を先に安定化）
2. `3 -> 7`（導線とUI整合）
3. `4 -> 5`（出力経路を実用化）
4. `6 -> 8`（仕上げと事故防止）
5. `9` は全期間で並行実施（短いループで効果検証）

## Risk Notes

- Timeline入力系は回帰しやすい。修正時はハンドル・シーク・選択をセットで確認する。
- Queue/UI同期は二重状態を作ると破綻する。Serviceを唯一の状態ソースに維持する。
- Dock見た目調整は機能実装と分離して小さく反復する。

## v0.2 Remaining Focus (Immediate)

次の 5 項目を先に閉じると v0.2 を実質完了にできる。

1. RenderQueue ジョブ編集に `FPS/Bitrate` を追加
2. RenderQueue 並び替えのUI同期を完全に Service 駆動へ寄せる
3. タイムライン左/右ペインの行・ヘッダ整列を最終固定
4. メニュー有効条件の統一（対象なし時 disabled）
5. 破棄後コールバック呼び出しの棚卸し（Widget/Service全体）
