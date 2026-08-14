# GPU Text Animator Transform Buffer / Instancing

**最終更新:** 2026-08-13
**ステータス:** Not Started

## 目的

テキストアニメーターのレイアウト・セレクター評価はCPUで行い、確定したGlyphごとの変形値をGPU描画へ効率よく渡す。AEの挙動を盲目的に再現するのではなく、Software/GPUで同じ評価結果になること、値の意味が一貫していること、長いテキストでも安定することを優先する。

## 現状

現在も次のGPU経路は存在する。

```
Text shaping / selector / animator evaluation (CPU)
  → GlyphItem offsets
  → GlyphAtlas acquire
  → PrimitiveRenderer2D::drawGlyphsTransformed()
  → GPU glyph quad transform / draw
```

ただし、`drawGlyphsTransformed()` はGlyphごとに描画パケットと変換行列をCPU側で組み立てる。Glyph数が増えるとCPU発行コストとコマンド数が増えるため、Transform Bufferまたはinstanced drawへ移行する余地がある。

関連箇所：

- `Artifact/src/Layer/ArtifactTextLayer.cppm`
- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
- `Artifact/src/Render/DiligentImmediateSubmitter.cppm`
- `ArtifactCore/include/Text/TextLayoutContract.ixx`
- `ArtifactCore/src/Text/TextAnimator.cppm`

## 方針

### CPUに残す責務

- Unicode / grapheme / cluster / ligatureの解決
- shaping、改行、行・パス配置
- Range / Regex / Wigglyなどのセレクター評価
- Animatorの値、合成順序、Software経路と共有する最終評価値の生成
- Glyph Atlasの取得要求とキャッシュ管理

### GPUへ渡す責務

- Glyphごとの位置、回転、スケール、Skew、Z
- Opacity、色Override、Stroke幅、Blurの描画パラメータ
- Glyph quadの変形、Atlasサンプリング、アルファ合成

## Work Packages

### WP-0: Design Simulation / Reference Model

GPU実装前に、テキストアニメーターの設計モデルをfixtureへ通し、仕様そのものの矛盾を検出する。これは実装後の単体テストではなく、実装に依存しないリファレンスモデルによる設計シミュレーションである。

#### Fixtureマトリクス

単独の文字列リストではなく、文字構造・レイアウト・Selector・Animator・時間・保存復元の組み合わせを段階的に監査する。

##### Smoke（20〜30ケース）

基本経路と、壊れやすいUnicode境界を短時間で確認する。

- 空文字、空白のみ、`Text1`、`Text Sample1`
- 連続空白、タブ、改行、複数行
- `Text 😀 Sample`
- `😀👍🏽 Text`
- `日本語テキスト`、韓国語、タイ語
- RTL文字列、LTR + RTL混在、数字 + RTL
- precomposed文字とcombining mark（`é` / `e + ◌́`）
- ZWJ、variation selector、regional indicator、Indic conjunct
- font fallback混在（Latin + CJK + Emoji）
- Point text、Box text、中央揃え、縦書き

##### Contract（50〜100ケース）

設計上の契約とAnimator合成を確認する。

- Selector Units：Percentage / Index / Cluster / Line / Tag
- Shape：Square / RampUp / RampDown / Triangle / Round / Smooth
- Order：Natural / Reverse / RandomStable / CenterOut / EdgeIn
- Start = End、Start > End、Offset、範囲外Offset
- Regex match / no-match / invalid regex
- Anchor Groupingの全種類
- Position、Scale 0、Scale < 1、Rotation、Skew
- Opacity 0 / 1、Trackingの正負、Z
- Fill / Stroke override、Stroke width、Blur
- Wiggly無効 / 有効、異なるSeedとCorrelation
- Animator 1個、複数Animator、同一Glyphへの重複適用
- キーフレーム開始・中間・終了・境界直前直後
- Expression / Envelope / 非整数フレーム / FPS変更
- Animator削除後のキーフレーム再参照
- プリセット適用後の編集と再保存

##### Stress（数百ケース）

長文、複雑なUnicode、Animator数、レイアウト負荷を確認する。

- 100 / 500 / 1000 / 5000 Glyph相当の長文
- 16 Animator上限
- 複数行Box textと折り返し
- Path text、縦書き、RTL混在の長文
- CJK、Emoji、combining mark、Ligatureの大量混在
- Atlas miss、Atlas拡張、同一Glyph再利用
- Software相当入力とGPU instance入力の大量比較
- 壊れたJSON、欠落フィールド、範囲外数値、範囲外RGBA
- 長時間再生、Seek、逆再生、FPS変更

#### 直交する監査軸

各fixtureは必要に応じて次の軸と組み合わせる。

```text
text fixture
 × layout mode
 × selector unit / shape / order
 × animator stack
 × timeline frame
 × serialization state
 × render backend input
```

全組み合わせを無制限に生成せず、Smoke → Contract → Stressの順で段階的に増やす。各段階で失敗した契約を修正してから次段階へ進む。

#### シミュレーション段階

```
fixture text
  → 仮想 code point / grapheme / cluster / line 構造
  → selector evaluation
  → animator stack evaluation
  → per-glyph final state
  → GPU instance data相当
```

#### 検証する設計不変条件

- Glyph、Cluster、Line、Tagの各選択単位が混同されない
- Percentage / Index / Cluster / Line / Tagの範囲定義が明確
- Start / End逆転、空文字、1文字、空白だけの入力が破綻しない
- Emoji ZWJ、modifier、combining mark、ligatureが意図せず分割されない
- 複数Animatorの適用順と合成規則が決定的である
- Trackingが後続Glyphへ与える影響が定義されている
- Scale、Opacity、Rotation、Colorの結果が有限値かつ許容範囲内である
- Software経路とGPU経路へ渡す最終Glyph状態が一致する
- GPU instance dataの数と対象Glyphの数が一致する
- 保存前後でGlyph / Cluster / Animatorの意味が変わらない
- Seek、逆再生、FPS変更で評価結果が不連続に壊れない
- Atlas missやGPUリソース不足がGlyphの欠落・クラッシュに直結しない

#### シミュレーション出力

各fixtureとフレームについて、少なくとも以下をJSONで保存する。

- source text / code point情報
- glyph / cluster / line数
- selector weight
- final position / scale / rotation / skew / opacity
- tracking後の位置
- color / stroke / blur
- atlas key / UV相当値
- Software/GPU入力差分
- pass / warning / errorと診断理由

#### WP-0完了条件

- fixtureごとに期待される選択単位とGlyph構造が明文化されている
- Smoke / Contract / Stressの各段階にケース数と合格基準がある
- すべてのAnimatorパラメータについて合成結果を手計算またはモデルで再現できる
- 少なくとも1つ以上、現在の設計上の矛盾または未決定事項を検出できる
- 未決定事項を解消するまでWP-1以降を実装開始しない

### WP-1: GPU向けGlyph instance dataの定義

`GlyphItem`全体をGPUへ渡さず、描画に必要なPOD形式の最小構造を定義する。

候補フィールド：

- base position / bearing / size
- offset position / rotation / scale / skew / z
- opacity
- fill / stroke color and weights
- atlas UV rectangle

完了条件：

- CPU評価結果からGPU instance dataを決定的に生成できる
- Software描画が従来の`GlyphItem`を引き続き利用できる
- NaN、Inf、負の不正スケールがGPUバッファへ入らない

### WP-2: 既存Glyph描画へのTransform Buffer接続

まず既存のGlyph Atlas、PSO、描画結果を維持したまま、Glyphごとの変換値をStructured Bufferまたは同等のGPU入力へ移す。

完了条件：

- 1文字、短文、長文で既存GPU描画と同じ位置・回転・スケールになる
- draw call数とCPU packet生成時間を計測できる
- Atlas更新とTransform更新の責務が分離される

### WP-3: Instanced Glyph quad描画

Glyph quadの形状を共有し、instance dataだけを更新して複数Glyphをまとめて描画する。StrokeやBlurが別パスを必要とする場合は、最初から無理に一つへ統合せず、通常Glyph、Stroke、Shadowを段階的に分ける。

完了条件：

- Glyph数に対するCPU draw packet数の増加を抑えられる
- Fill、Stroke、Color Override、Opacityが維持される
- Z順序と透明合成順が既存経路と一致する

### WP-4: Software / GPU parity

同じAnimator評価結果を使い、Software経路とGPU経路の差分を検証する。

最低限のケース：

- Start / End逆転
- Percentage / Index / Cluster / Line / Tag
- Regex選択
- Anchor Grouping
- 複数Animator
- Scale 0、Opacity 0、Rotation、Skew、Tracking
- Fill / Stroke Color Override
- Wiggly
- RTL、絵文字、結合文字、Ligature

完了条件：

- 差分が許容誤差内で説明可能
- Glyphの欠落、二重描画、順序逆転がない
- GPU経路だけで意味が変わるパラメータがない

### WP-5: 性能受け入れ

100、1000、5000 Glyph相当のテキストで、Animator有効時のCPU評価、GPUバッファ更新、描画時間を計測する。目標値は測定環境を固定してから決める。

完了条件：

- 代表的な文字数ごとの計測結果を記録
- Transform Buffer更新がボトルネックになった場合の対策を判断
- Glyph Atlas再構築とAnimator更新が不要に連動しない

## 非目標

- shapingや改行処理をGPUへ移すこと
- AE固有の曖昧な挙動をそのまま再現すること
- Software経路を先に削除すること
- GPU化を理由に新しいQt合成やQImageホットパスを追加すること

## Artifact独自機能の設計優先順位

AE互換の細かいパラメータを増やす前に、編集時間を直接削減する機能を優先する。

### Priority A: Content-aware continuity

文章変更後もGlyph indexだけに依存せず、安定したToken / Grapheme / Word identityを使って既存アニメーションを追従させる。

- 既存語のアニメーション状態を維持
- 追加された語だけ登場アニメーションを適用
- 削除された語を退場側へ解決
- 置換された語を変更部分として診断
- 差分が曖昧な場合は自動適用せず、Previewで警告する

### Priority B: Layout-preserving motion

Box / Line / Wordの制約を適用し、過度な重なりやBox外逸脱を警告または制限する。制約による補正量はExplain結果へ記録する。

### Priority C: Hierarchical and relational fields

Grapheme → Word → Line → Paragraphの階層、隣接Glyph、同一Tag、Emojiや数字などの属性をSelection Fieldへ提供する。

### Priority D: Procedural operators

Propagation、Spring / Inertia、Noise Field、音量・CSV・MIDIなどの外部Value Fieldを、低レベルAnimatorの追加ではなくSelection FieldとOperatorの組み合わせとして実装する。

## 独自機能の受け入れ条件

- 文章変更前後で、同一Tokenのidentityが追跡可能
- 追跡不能または曖昧な場合は自動適用せず警告する
- Layout constraintの補正前後の値を比較できる
- 制約適用後もSoftware / GPUへ同じ最終Glyph状態を渡す
- AI IntentのPreviewで変更対象、追従結果、制約補正、警告を説明できる
- 既存AE風Animatorを壊さず、独自機能は明示的なOperatorとして選択できる

## 受け入れ基準

1. CPU評価とGPU描画の責務境界がコード上で明確である。
2. 既存のGlyph Animatorの意味を変えずに、GPU入力がバッファ化されている。
3. Software/GPUで同一ケースを比較できる診断情報がある。
4. 長文でCPU packet発行が支配的にならないことを測定で確認できる。
5. GPU経路にフォールバックがあり、AtlasまたはGPUリソース失敗時もクラッシュしない。

## リスクと対策

| リスク | 対策 |
|---|---|
| 透明合成順が変わる | まず既存のGlyph順序をinstance dataの順序として維持する |
| Stroke / Blurでバッチ分割が増える | Fill、Stroke、Shadowを独立パスとして計測する |
| GPUとSoftwareでAnchorがずれる | Anchor計算はCPUで共有し、GPUは結果だけを受け取る |
| CJKや絵文字でAtlasが膨張する | Atlas管理をAnimator更新から分離し、既存キャッシュを再利用する |
| バッファ更新で毎フレーム割り当てが増える | 容量再利用、上限、frame allocatorの既存パターンを確認する |

## 実装順序

```
WP-0 design simulation / reference model
  → WP-1 instance data
    → WP-2 Transform Buffer
      → WP-3 instanced draw
        → WP-4 Software/GPU parity
          → WP-5 performance acceptance
```

## 関連マイルストーン

- `Artifact/docs/MILESTONE_GPU_DIRECT_TEXT_DRAW_2026-04-14.md`
- `Artifact/docs/MILESTONE_GPU_DIRECT_TEXT_WP3_PRIMITIVERENDERER_2D_2026-04-27.md`
- `ArtifactCore/docs/MILESTONE_GPU_TEXT_RENDERING_JA_2026-04-01.md`
- `docs/done/MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md`
