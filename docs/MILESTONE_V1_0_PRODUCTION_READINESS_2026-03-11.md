# Artifact v1.0 Production Readiness (2026-03-11)

v1.0 は「開発者向け試作」から「日常運用できる制作ツール」へ移行するための節目。

## Goal

次の 5 項目を満たしたら v1.0 とする。

1. タイムライン編集の基本操作（選択/移動/伸縮）が破綻しない
2. レンダーキューの登録からダミー出力まで安定して完走できる
3. 異常終了時に復旧導線と診断導線が一貫して動作する
4. UI 操作は `*Service` 経由ルールに準拠し、直アクセスが残っていない
5. 主要画面レイアウト（Main/Timeline/Inspector/RenderQueue）が初期状態で作業可能

## Definition Of Done

- 主要クラッシュ再現ケースの未修正がない
- Help の診断レポートだけで一次切り分けが可能
- コンポ作成 -> レイヤー追加 -> RenderQueue登録 -> ダミー出力 が通る
- v1.0 運用手順を docs に一本化

## Work Packages

### 1. Timeline Interaction Hardening
- 左右ハンドル挙動の最終安定化
- シークバークリック有効領域の全面統一
- 左ペイン/右ペインの高さ・ライン整合の固定

### 2. Render Queue End-to-End
- キュー登録の対象名表示を統一（Composition名）
- 削除連動確認（Composition削除時）を全経路で保証
- ダミー出力の結果表示と失敗理由表示を固定

### 3. Recovery + Diagnostics Integration
- レイアウト復元成否の診断出力（導入済み）を運用手順へ反映
- Recovery フォルダ運用とログ採取を短手順化

### 4. Service Boundary Sweep
- UI からの直接 Model 参照を棚卸し
- Service API 未整備箇所の補完

### 5. Default Workspace Layout
- MainWindow 初期ドック配置の固定
- 不要ウィジェット初期非表示（Script/Undo など）の方針確定

## Suggested Order

1. `Timeline Interaction Hardening`
2. `Render Queue End-to-End`
3. `Recovery + Diagnostics Integration`
4. `Service Boundary Sweep`
5. `Default Workspace Layout`
