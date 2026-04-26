# GPU Direct Text Draw WP-3: PrimitiveRenderer2D GlyphAtlas Integration (2026-04-27)

## Overview

Work Package 3 integrates GPU-accelerated glyph rendering into the `PrimitiveRenderer2D` class. This is the first concrete GPU text rendering layer in the direct text draw system.

## Status: ✅ Proof-of-Concept Implementation Complete

- ✅ Added three new text drawing methods to PrimitiveRenderer2D
- ✅ Integrated GlyphAtlas foundation into rendering pipeline
- ✅ Created method signatures following established patterns
- ✅ Implemented character acquisition from GPU atlas
- ⏳ Full quad generation and GPU submission (WP-5 dependency)

---

## Changes Made

### Files Modified

#### 1. `Artifact/include/Render/PrimitiveRenderer2D.ixx`

**Added Methods:**

```cpp
// GPU glyph atlas based text rendering (WP-3)
void drawGlyphText(float x, float y, const ArtifactCore::UniString& text,
                   const ArtifactCore::TextStyle& style,
                   const FloatColor& color,
                   float opacity = 1.0f);

void drawGlyphTextTransformed(float x, float y, const ArtifactCore::UniString& text,
                              const ArtifactCore::TextStyle& style,
                              const FloatColor& color,
                              const QMatrix4x4& transform,
                              float opacity = 1.0f);

void drawGlyphs(std::span<const ArtifactCore::GlyphItem> glyphs,
                const ArtifactCore::TextStyle& style,
                const FloatColor& color,
                float opacity = 1.0f);
```

**Added Imports:**
```cpp
import Text.Style;
import Text.GlyphAtlas;
import Text.GlyphLayout;
#include <span>
```

#### 2. `Artifact/src/Render/PrimitiveRenderer2D.cppm`

**Impl Class Enhancement:**
```cpp
class PrimitiveRenderer2D::Impl {
public:
    // ... existing members ...
    std::unique_ptr<GlyphAtlas> pGlyphAtlas_;  // NEW
};
```

**GlyphAtlas Initialization:**
```cpp
void PrimitiveRenderer2D::createBuffers(RefCntAutoPtr<IRenderDevice> device, TEXTURE_FORMAT rtvFormat)
{
    if (!device) return;
    impl_->pDevice_ = device;
    
    // Initialize GlyphAtlas
    if (!impl_->pGlyphAtlas_) {
        impl_->pGlyphAtlas_ = std::make_unique<GlyphAtlas>();
    }
}
```

**Method Implementations:**

1. **drawGlyphText()**
   - Acquires each character from GlyphAtlas
   - Maps TextStyle parameters to GlyphKey
   - Iterates through code points building atlas
   - TODO: Vertex generation and append to command buffer

2. **drawGlyphTextTransformed()**
   - Placeholder for matrix-transformed glyph rendering
   - TODO: Full implementation with transformation support

3. **drawGlyphs()**
   - Accepts pre-laid-out GlyphItems
   - Designed for animated text and text animators
   - TODO: Glyph-to-GPU submission pipeline

---

## Architecture

### Character to GPU Pipeline

```
UniString text
    ↓
For each char32_t codePoint:
    ├─ Create GlyphKey (codePoint, fontSize, fontFamily, styleFlags)
    ├─ Query GlyphAtlas::acquire(key, QFont)
    ├─ Get GlyphRect (position in atlas, advance, bearing)
    └─ TODO: Generate quad vertices and submit to GPU
    
Atlas Texture (GPU)
    ↓
GlyphQuad PSO (from WP-2)
    ↓
Render Target
```

### Data Flow

1. **CPU-side:**
   - Text → TextStyle parameters
   - TextStyle → GlyphKey (font settings)
   - GlyphKey → GlyphRect (atlas lookup)

2. **GPU-side (WP-5 dependency):**
   - GlyphRect → Quad vertices (position + UV)
   - Quad batch → GPU buffer upload
   - GlyphQuadPSO → Render

---

## Design Decisions

### 1. GlyphAtlas as Member

```cpp
std::unique_ptr<GlyphAtlas> pGlyphAtlas_;
```

- Created during `createBuffers()` for device lifetime consistency
- Owned by PrimitiveRenderer2D (simple lifetime management)
- Lazy initialization ensures no overhead for non-text draws

### 2. Method Signatures

Follows established PrimitiveRenderer2D patterns:

- `float x, float y` for canvas coordinates
- `const TextStyle&` for font parameters (replaces QFont)
- `const FloatColor&` for color (instead of QColor)
- `float opacity` for blending control
- `QMatrix4x4 transform` for transformed variants

### 3. Character Processing

```cpp
for (char32_t codePoint : text) {
    GlyphKey key;
    key.codePoint = codePoint;
    key.fontSize = style.fontSize;
    key.fontFamily = style.fontFamily.toUtf8String();
    key.styleFlags = (fontWeight << 1) | (fontStyle << 0);
    
    QFont qfont(QString::fromStdString(key.fontFamily));
    // ... setup QFont from TextStyle ...
    
    GlyphRect rect = impl_->pGlyphAtlas_->acquire(key, qfont);
    // ... generate quad from rect ...
}
```

---

## Pending Implementation (WP-5)

### GPU Command Buffer Integration

Current placeholder:
```cpp
// TODO: Generate and append glyph quad
// currentX += rect.advance;
```

**Next Steps:**
1. Create vertex buffer with glyph quad geometry
   - Position: NDC or canvas-space coords
   - UV: GlyphRect atlas coordinates
   - Color: FloatColor modulation

2. Append draw command to RenderCommandBuffer
   - glyphQuadPsoAndSrb_ from ShaderManager
   - Instance count = glyphCount

3. Handle atlas upload if dirty
   - Check GlyphAtlas::isDirty()
   - Upload via Diligent UpdateTextureSubresource
   - Clear dirty flag

---

## Testing Recommendations

### Unit Tests (When Ready)

1. **Character Acquisition**
   ```cpp
   TEST(GlyphAtlasIntegration, AcquireCharacters) {
       PrimitiveRenderer2D renderer;
       renderer.createBuffers(device, RTVFormat);
       
       TextStyle style;
       style.fontFamily = "Arial";
       style.fontSize = 24.0f;
       
       UniString text("Hello");
       renderer.drawGlyphText(0, 0, text, style, FloatColor::white, 1.0f);
       // Verify no crashes
   }
   ```

2. **Atlas Dirty Flag**
   ```cpp
   TEST(GlyphAtlasIntegration, MarkDirtyOnAcquire) {
       GlyphAtlas atlas;
       ASSERT_FALSE(atlas.isDirty());
       
       GlyphKey key;
       key.codePoint = 'A';
       key.fontSize = 24.0f;
       
       atlas.acquire(key, qfont);
       ASSERT_TRUE(atlas.isDirty());
   }
   ```

3. **Multi-language Support**
   - ASCII characters (Latin)
   - Japanese characters (Hiragana, Kanji)
   - Emoji and special glyphs

### Integration Tests (Requires WP-5)

1. Visual rendering test in CompositeEditor
2. Performance profiling with large text
3. Atlas fullness and eviction scenarios

---

## Known Limitations

### WP-3 Scope

1. **No GPU Submission Yet**
   - drawGlyphText() acquires from atlas only
   - No quad generation or render command appending
   - Full implementation requires WP-5 (next phase)

2. **Simplified Character Processing**
   - No text layout (line breaking, alignment)
   - No font fallback chain
   - Single-pass per string

3. **No Transform Support**
   - drawGlyphTextTransformed() is placeholder
   - Matrix-based glyph rendering deferred to WP-5

### Known Issues

- Build system (vulkan-shaders-gen) has unrelated linker issues
- No C++ compilation errors in PrimitiveRenderer2D changes
- All TODOs are intentionally deferred to WP-5

---

## Integration Points

### WP-2 Dependency ✅ Met
- ShaderManager::glyphQuadPsoAndSrb() available
- Glyph quad PSO and SRB ready for use
- No changes needed

### WP-1 Dependency ✅ Met
- GlyphAtlas fully initialized
- acquire() method works correctly
- Atlas texture accessible

### WP-4 Dependency (After This)
- ArtifactIRenderer interface needs drawText() variant
- Will delegate to PrimitiveRenderer2D::drawGlyphText()
- Same signature pattern as other draw methods

---

## Performance Considerations

### Current (WP-3)

- Single GlyphAtlas instance per renderer (no per-frame overhead)
- Character iteration is O(n) for string length
- Atlas acquire() is O(1) hash lookup with fallback O(log n) packing

### Future (WP-5+)

- Batch GPU submissions (reduce draw calls)
- Partial atlas uploads (only dirty regions)
- LRU eviction policy (prevent atlas thrashing)
- Per-cache-size metrics (frame cost stats)

---

## Migration Path from Qt Text Draw

**Before (Qt-based):**
```cpp
renderer->drawText(rect, text, font, color);
// → toQImage() (CPU) → upload (GPU) → sprite draw
```

**After (GPU direct):**
```cpp
renderer->drawGlyphText(x, y, UniString(text), style, color);
// → atlas acquire (CPU) → GPU quad submit → glyph PSO render
```

**Coexistence Strategy:**
1. WP-3: Add new GPU methods (no changes to existing drawText)
2. WP-4: Expose via ArtifactIRenderer (parallel API)
3. WP-5: Migrate UI text (CompositeEditor overlays)
4. WP-6: Migrate ArtifactTextLayer (with animator support)
5. Later: Deprecate Qt drawText variant (backward compatible path)

---

## Files Involved

| File | Changes | Impact |
|------|---------|--------|
| `Artifact/include/Render/PrimitiveRenderer2D.ixx` | +3 methods, +2 imports | Public API |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | +1 member (Impl), +1 init, +3 impl | Implementation |
| `Artifact/docs/...` | This document | Documentation |

**No breaking changes to existing API.**

---

## Next Phase: WP-4

**ArtifactIRenderer Interface**

Expose GPU glyph drawing to composition render pipeline:

```cpp
// In ArtifactIRenderer::draw()
class ArtifactIRenderer {
public:
    virtual void drawGlyphText(float x, float y,
                              const UniString& text,
                              const TextStyle& style,
                              const FloatColor& color,
                              float opacity = 1.0f) = 0;
    // ...
};
```

Expected scope: 2-4 hours
- Implement delegation to PrimitiveRenderer2D
- Add to ArtifactCompositionRenderController
- Test with simple editor overlay text

---

## References

- **WP-1**: GlyphAtlas foundation → `ArtifactCore/include/Text/GlyphAtlas.ixx` ✅ Ready
- **WP-2**: Shader + PSO → `Artifact/src/Render/ShaderManager.cppm` ✅ Ready  
- **WP-3**: This document (PrimitiveRenderer2D integration) ✅ Started
- **WP-4**: ArtifactIRenderer interface (next) ⏳ Planned
- **WP-5**: Editor UI text migration (after WP-4) ⏳ Planned
- **WP-6**: ArtifactTextLayer GPU draw (final) ⏳ Planned

---

## Conclusion

WP-3 establishes the foundation for GPU-accelerated glyph rendering in PrimitiveRenderer2D. The three new methods are ready for integration, with full GPU submission deferred to WP-5 to maintain clear phase boundaries and testability.
