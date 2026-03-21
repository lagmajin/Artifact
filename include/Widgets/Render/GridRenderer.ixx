module;
#include <cmath>
#include <algorithm>

export module GridRenderer;

import Artifact.Render.IRenderer;
import Color.Float;

export namespace Artifact {

 /// Configurable grid renderer for composition viewports.
 class GridRenderer {
 public:
  enum class GridStyle {
   Lines,      // 線グリッド
   Dots,       // ドットグリッド
   Crosses     // 十字グリッド
  };

  GridRenderer();
  ~GridRenderer();

  /// Draw a grid in the given rectangle.
  void draw(ArtifactIRenderer* renderer,
            float x, float y, float w, float h,
            float spacing, float thickness,
            const FloatColor& color,
            GridStyle style = GridStyle::Lines);

  /// Draw a subdivided grid (major + minor lines).
  void drawSubdivided(ArtifactIRenderer* renderer,
                      float x, float y, float w, float h,
                      float majorSpacing, float minorSpacing,
                      const FloatColor& majorColor,
                      const FloatColor& minorColor);

  /// Set/get grid spacing
  void setSpacing(float spacing);
  float spacing() const;

  /// Set/get minor grid ratio (minorSpacing = spacing / ratio)
  void setMinorRatio(int ratio);
  int minorRatio() const;

 private:
  float spacing_ = 100.0f;
  int minorRatio_ = 4;
 };

}
