# Timeline Panel Implementation Checklist

## 0. Goal
左パネル（Layer Panel）と右パネル（Timeline Track Area）の仕様を実装で満たしているかを確認するためのチェックリスト。

## 1. Layout / Alignment
- [ ] 左右の `Row Height` が常に一致している。
- [ ] 左右ヘッダー下端が同じY座標で揃っている。
- [ ] 左右の垂直スクロールが同期している。
- [ ] 画面幅が狭い場合でも左側UIが破綻せず、描画がはみ出ない。

## 2. Left Panel (Layer Panel)
- [ ] 列構成（Switches / Name / Parent / Blend）が表示される。
- [ ] Layer Row と Group Row が区別される。
- [ ] Structural Row や Source Row が型バッジで見分けやすい。
- [ ] NullLayer 展開時に `Transform` グループ行が表示される。
- [ ] Group Row はレイヤースイッチ対象にならない。
- [ ] 名前ダブルクリック編集（F2含む）が動作する。
- [ ] Parentコンボに自分自身が入らない。
- [ ] Parent=`<None>` で親解除できる。
- [ ] Blendコンボの選択変更がレイヤーへ反映される。
- [ ] シャイ非表示ONで shy レイヤーが隠れる。

## 3. Right Panel (Timeline Track Area)
- [ ] 1行に Clip Item は最大1つ（1ライン1クリップ）を満たす。
- [ ] Clip Item のY位置が対応 Layer Row と一致する。
- [ ] Group Row に Clip Item を出さない。
- [ ] ハンドルはX方向のみ移動（Y方向移動なし）。
- [ ] 右ハンドルが左ハンドルを追い越せない。
- [ ] 左ハンドルが右ハンドルを追い越せない。
- [ ] 最小長（minDuration）制約が守られる。
- [ ] クリップドラッグとハンドルドラッグの判定が分離されている。
- [ ] スケール変更でハンドル見た目サイズが過度に変わらない。

## 4. Seekbar
- [ ] 右ペイン上部全幅で表示される。
- [ ] 上部領域クリックでシーク移動できる。
- [ ] ハンドル操作中のシークバー誤反応を抑制できる。
- [ ] シーク位置表示が左右/再生状態と矛盾しない。

## 5. Input Priority / HitTest
- [ ] 優先度が `Handle > Clip Body > Seekbar > Background` になっている。
- [ ] 重なり時に誤操作（意図しない移動/トリム）が起きない。

## 6. Left-Right Synchronization
- [ ] レイヤー追加で左右同時反映される。
- [ ] レイヤー削除で左右同時反映される。
- [ ] 展開/折りたたみ時に右側行マップも再生成される。
- [ ] 選択状態は LayerID を基準に一意に同期される。

## 7. Menu / Context Menu Enable Rules
- [ ] 条件未成立のアクションは disabled 表示。
- [ ] 選択レイヤーなしでレイヤー編集系を押せない。
- [ ] コンポジション未選択で新規レイヤー作成が押せない。

## 8. Architecture Rule
- [ ] UI層は `*Service` インスタンス経由のみで状態変更する。
- [ ] UI層から `ProjectManager` 直呼びをしない。
- [ ] UI層からドメインオブジェクトへ直接書き込みを最小化する。

## 9. Stability / Regression
- [ ] NullLayer 展開/折りたたみでクラッシュしない。
- [ ] プロパティ表示更新時にアクセス違反が発生しない。
- [ ] 連続操作（追加/削除/展開/ドラッグ）で状態が壊れない。

## 10. Done Criteria
- [ ] 主要ケースを手動検証してチェック完了。
- [ ] 既知の制限事項を `docs` に記載済み。
- [ ] 変更点が追跡可能なコミットメッセージで整理されている。
