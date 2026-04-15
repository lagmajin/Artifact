# GPU Direct Text Draw on CompositeEditor (2026-04-14)

DiligentEngine 上の `CompositeEditor` に、Qt オーバーレイを経由せず  
**直接文字を描画（GPU text draw）** するためのマイルストーン。

---

## 背景・動機

現状のテキスト描画は以下のフローで行われている：

```
ArtifactTextLayer::toQImage()
    └─ QPainter + QTextDocument でラスタライズ → QImage
        └─ QImage を Diligent Texture にアップロード
            └─ drawSpriteTransformed() で GPU 描画
```

このパスには次の問題がある：

- **毎フレーム CPU ラスタライズ**：`QPainter::drawText` ベースで、フレーム単位の変更に対してコストが高い
- **Qt 依存が深く残る**：`QTextDocument`, `QPainterPath`, `QFont` が描画コアに存在
- **GPU glyph atlas がゼロ**：テキスト専用の GPU リソースが一切存在しない
- **CompositeEditor 上の editor UI テキスト**（ラベル・デバッグオーバーレイ等）は  
  現在も Qt `QPainter::drawText()` または Qt overlay widget 経由であり、  
  Diligent の描画ループと混在して管理が複雑

---

## Goal

1. Diligent の render loop 内で glyph を GPU に直接送って描画できるようにする
2. `ArtifactIRenderer` に `drawText()` インターフェースを追加する
3. `PrimitiveRenderer2D` に glyph atlas 管理と quad 発行を追加する
4. `ShaderManager` に glyph quad 描画用の PSO/Shader を追加する
5. `ArtifactTextLayer` の描画パスを段階的に GPU 直描に移行できる土台を作る
6. CompositeEditor の editor UI テキスト（セーフエリアラベル等）を GPU draw に移せるようにする

---

## 現状の進捗（2026-04-14 時点）

| 項目 | 状態 |
|------|------|
| `TextStyle` / `ParagraphStyle` | ✅ Core 整備済み |
| `TextLayoutEngine` (GlyphItem 配列生成) | ✅ 実装済み (Qt `QFontMetricsF` ベース) |
| `FontManager` (CJK fallback, `makeFont`) | ✅ 実装済み |
| `ArtifactTextLayer::toQImage()` | ✅ `QPainter` + `QTextDocument` ベースで動作中 |
| `ArtifactTextLayer::draw()` | ✅ `drawSpriteTransformed` 経由で GPU upload 動作中 |
| GPU side `GlyphAtlas` | ❌ 未着手 |
| GPU side glyph quad shader | ❌ 未着手 |
| `PrimitiveRenderer2D::drawText()` | ❌ 未着手 |
| `ArtifactIRenderer::drawText()` | ❌ 未着手 |
| `ShaderManager` への glyph PSO 追加 | ❌ 未着手 |
| CompositeEditor UI テキストの GPU draw 移行 | ❌ 未着手 |

---

## Work Packages

### WP-1：GPU GlyphAtlas クラスの追加（ArtifactCore）

**目的**：glyph をアトラステクスチャに詰めて GPU で再利用できるようにする

**作業ファイル**：
- `ArtifactCore/include/Text/GlyphAtlas.ixx` ← 新規
- `ArtifactCore/src/Text/GlyphAtlas.cppm` ← 新規

**設計方針**：
- atlas サイズ：2048×2048（初期）、`BIND_SHADER_RESOURCE | BIND_RENDER_TARGET` 不要、`USAGE_DEFAULT`
- glyph を `FT_Bitmap` または `QRawFont::alphaMapForGlyph()` でラスタライズして詰める
- 初期は `QRawFont::alphaMapForGlyph()` を使い、FreeType を直接は叩かない
- atlas 更新は `UpdateTextureSubresource` で差分更新
- atlas が満杯になったら新しい atlas を追加（multi-atlas）、または単純に破棄してリビルド

**入力**：
```
char32_t code + TextStyle (font family, size, weight, italic)
→ UVRect (atlas 内の UV 座標と atlas インデックス)
```

**完了条件**：
- atlas texture が Diligent `ITexture*` として取得できる
- UV マッピングが返せる
- 同じ glyph を 2 回渡したときにキャッシュヒットする

---

### WP-2：glyph quad shader と PSO の追加（ShaderManager）

**目的**：glyph atlas テクスチャをサンプリングする最小シェーダを追加する

**作業ファイル**：
- `Artifact/src/Render/ShaderManager.cppm`
- `ArtifactCore/include/Graphics/` 以下に HLSL ソースを追加

**シェーダ設計（HLSL）**：
```hlsl
// Vertex Layout:
//   ATTRIB0: float2 pos   (NDC or canvas space)
//   ATTRIB1: float2 uv    (glyph atlas UV)
//   ATTRIB2: float4 color (fill color * opacity)

// Pixel Shader:
//   sample atlas texture の alpha チャンネルで color をモジュレート
//   out.rgba = color.rgb * atlas.a
//   alpha blend: SRC_ALPHA / INV_SRC_ALPHA
```

**PSO 登録名**：`glyphQuadPsoAndSrb_`

**SRB 変数**：
- `SHADER_TYPE_VERTEX`：`TransformCB`（既存の viewport CB と共用可）
- `SHADER_TYPE_PIXEL`：`g_atlas`（glyph atlas SRV）、`g_sampler`

**完了条件**：
- PSO が ShaderManager のビルドで通る
- `ShaderManager::glyphQuadPsoAndSrb()` で取得できる

---

### WP-3：PrimitiveRenderer2D::drawText() の実装

**目的**：canvas 座標系でテキストを直接描画できる API を追加する

**作業ファイル**：
- `Artifact/src/Render/PrimitiveRenderer2D.cppm`

**API 設計**：
```cpp
// シンプルな 1 文字列描画（位置・色・フォントスタイル指定）
void drawText(float x, float y,
              const ArtifactCore::UniString& text,
              const ArtifactCore::TextStyle& style,
              const ArtifactCore::FloatColor& color,
              float opacity = 1.0f);

// GlyphItem 配列を直接受け取るバリアント（TextAnimator との統合用）
void drawGlyphs(std::span<const ArtifactCore::GlyphItem> glyphs,
                const ArtifactCore::TextStyle& style,
                const ArtifactCore::FloatColor& color);
```

**内部実装**：
1. `GlyphAtlas` にキャラコードを submit → UV を取得
2. 各 glyph に quad（2 triangle）を生成、vertex buffer に書き込み
3. glyph quad PSO をセット → DrawIndexed

**完了条件**：
- `drawText("Hello", ...)` で Diligent surface にテキストが見える
- 既存の sprite/solid rect draw と同フレーム内で混在できる

---

### WP-4：ArtifactIRenderer に drawText() を露出

**目的**：`ArtifactIRenderer` のインターフェース経由で呼べるようにする

**作業ファイル**：
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `ArtifactCore/include/Render/RendererInterface.ixx` など（宣言があればそちらも）

**追加する API**：
```cpp
void drawText(float x, float y,
              const ArtifactCore::UniString& text,
              const ArtifactCore::TextStyle& style,
              const ArtifactCore::FloatColor& color,
              float opacity = 1.0f);
```

**完了条件**：
- `CompositionViewport` や `CompositeEditor` の描画コードから呼べる

---

### WP-5：CompositeEditor 上の UI テキストを GPU draw へ移行

**目的**：安全エリアラベル・ズーム表示・デバッグオーバーレイなど  
editor overlay 上のテキストを Diligent の描画ループ内に統合する

**作業ファイル**：
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

**移行対象**：
- `p.drawText(...)` を使っている editor 内の `paintEvent` / `drawOverlay` 系コード
- 安全エリアマージンラベル、zoom 率表示など

**移行方針**：
- 既存の Qt `paintEvent` 上の `drawText` はそのまま残す（互換維持）
- Diligent render pass 内で `renderer->drawText(...)` を呼ぶパスを追加
- GPU draw パスが安定したら Qt overlay のテキストを削除 or 無効化

**完了条件**：
- `CompositeEditor` 上に表示される文字列の 1 つ以上が Qt overlay なしで描画される

---

### WP-6（任意）：ArtifactTextLayer の GPU 直描パス

**目的**：`toQImage()` → texture upload の経路を段階的に置き換える

**作業ファイル**：
- `Artifact/src/Layer/ArtifactTextLayer.cppm`
- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`

**移行条件**：
- WP-1〜4 が完成している
- per-glyph animator が glyph quad 単位でも正しく動く

**移行方針**：
```cpp
void drawLayerForCompositionView(...) {
    // TextLayer のみ: GPU draw パスへ分岐
    if (auto* textLayer = dynamic_cast<ArtifactTextLayer*>(layer)) {
        renderer->drawTextLayer(textLayer);  // 新パス
        return;
    }
    // 既存 sprite upload パスは残す（フォールバック）
}
```

**完了条件**：
- `ArtifactTextLayer` がテクスチャ upload をせず glyph quad で表示される

---

## 実装順序（推奨）

```
WP-1 (GlyphAtlas)
  → WP-2 (Shader + PSO)
    → WP-3 (PrimitiveRenderer2D::drawText)
      → WP-4 (ArtifactIRenderer interface)
        → WP-5 (Editor UI text, 検証しやすい起点)
          → WP-6 (TextLayer GPU 直描 ← 最後)
```

WP-5 を WP-6 より先に着手することで、  
**Layer のテキスト内容とは切り離した形で GPU text draw の動作確認**ができる。

---

## Risks

| リスク | 対処方針 |
|--------|----------|
| glyph atlas が満杯になる | multi-atlas または dynamic resize、初期は 2048x2048 で様子見 |
| CJK 文字の glyph 数が多く atlas が圧迫される | per-font-size の atlas 分割、必要なら LRU evict |
| `QRawFont::alphaMapForGlyph` が正確でない | FreeType 直接 rasterization に将来差し替え |
| ShaderManager の PSO 追加でビルドが通らない | 最小 HLSL から始め、createPSOs() に独立タスクで追加 |
| WP-5 移行中に Qt overlay と GPU draw が二重描画 | feature flag で切り替え可能にしてから統合 |

---

## Related

- `ArtifactCore/docs/MILESTONE_GPU_TEXT_RENDERING_JA_2026-04-01.md`
- `ArtifactCore/docs/MILESTONE_TEXT_SYSTEM_2026-03-12.md`
- `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_2026-03-21.md`
- `Artifact/src/Render/ShaderManager.cppm`
- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `ArtifactCore/include/Text/GlyphLayout.ixx`
- `ArtifactCore/include/Font/FreeFont.ixx`
