# Timeline Panel Specification

## Scope
この仕様はタイムラインパネルの左側（レイヤーパネル）と右側（クリップ/タイムライン領域）のUI仕様と挙動ルールを定義する。

## Terminology
- Left Panel: Layer Panel（レイヤー名、スイッチ、親、ブレンド、階層表示）
- Right Panel: Timeline Track Area（時間軸、クリップ表示、シークバー）
- Row: 左右で対応する1本の行（同じY座標帯）
- Clip Item: 右側に表示される時間範囲オブジェクト

## Global Layout Rules
- 左右の `Row Height` は常に一致させる。
- 左右のヘッダー下端は同一Y位置に揃える。
- 左右は同じ垂直スクロール量で同期する。
- 行の選択状態は左右で一貫（左で選択すると右も対象行が選択表示）。

## Left Panel Specification

### Purpose
- レイヤー構造の把握
- レイヤー単位操作（可視、ロック、ソロ、シャイ）
- レイヤー名編集
- 親レイヤー・ブレンドモード編集
- ツリー展開/折りたたみ

### Columns
- Switches: Visibility / Lock / Solo / Audio / Shy
- Layer Name
- Parent
- Blend

### Row Types
- Layer Row: 実レイヤーを表す行
- Group Row: 展開時の下位カテゴリ行（例: `Transform`）

### Interaction Rules
- 単クリック:
  - Layer Row: 選択
  - Switch列: 該当スイッチ切替
- ダブルクリック:
  - Layer Row の名前領域: インライン名前編集
- ツリー操作:
  - 展開トグルは `hasChildren=true` の行のみ表示
  - Group Row にはレイヤースイッチ操作を適用しない
- コンテキストメニュー:
  - 選択レイヤーに対する操作を提供
  - 条件に合わない項目は disabled

### Data Rules
- 親候補一覧には自分自身を含めない。
- 親解除は `Parent=<None>` で指定可能。
- シャイ非表示ON時、`isShy=true` レイヤー行は非表示。

## Right Panel Specification

### Purpose
- 時間軸上のレイヤー状態を可視化
- クリップの移動/トリム
- シーク位置の参照・変更

### Core Rules
- **1ライン（1 Row）に配置できる Clip Item は最大1つ。**
- Clip Item は対応する左側 Layer Row と同じY帯に描画する。
- Group Row（例: Transform）はクリップを持たない。
- クリップの長さは `minDuration` 以上を維持する。

### Clip Geometry Rules
- 右ハンドルは左ハンドルを追い越してはならない。
- 左ハンドルは右ハンドルを追い越してはならない。
- ハンドルはY方向に移動不可（X方向のみ）。
- クリップ本体ドラッグとハンドルドラッグを明確に判定する。
- スケール変更時もハンドルの見た目サイズは一定を基本とする。

### Seekbar Rules
- シークバーは右ペイン上部で全幅表示する。
- 右ペイン上部の任意Xクリックでシーク位置を変更可能。
- ハンドル操作中はシークバー入力を抑制し、誤操作を防ぐ。

### Interaction Priority
- Pointer hit test 優先度:
  1. ハンドル
  2. クリップ本体
  3. シークバー
  4. 背景クリック
- 同一フレーム位置へのスナップを許可する（必要なら将来オプション化）。

## Synchronization Rules (Left <-> Right)
- 行追加/削除/並び替え時は左右同時に再レイアウト。
- 選択IDは LayerID を単一ソースにする。
- 展開/折りたたみで可視行が変わったら右側も同じ行マップで再生成。

## Enable/Disable Policy
- UI層は `*Service` インスタンス経由で状態変更を行う。
- 条件不成立の操作は実行時に弾く前にUIで disabled 表示する。

## Future Extensions
- 1ライン複数クリップ（レイヤー内セグメント分割）は現仕様では非対応。
- 将来対応する場合は `Track Segment Model` を別途導入し、本仕様の 1ライン1クリップ制約を改訂する。
