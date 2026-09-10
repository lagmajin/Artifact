module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <QString>
#include <QPointF>
#include <QTransform>
#include <QJsonArray>
#include <QJsonObject>

export module Artifact.Layer.Shape;

import Color.Float;
import Artifact.Layer.InitParams;
import Artifact.Layers.Abstract._2D;
import Artifact.Mask.LayerMask;
import Artifact.Render.IRenderer;
import Shape.Operator;
import Shape.Path;
import Shape.Layer;
import Shape.Types;
import Memory.SharedPtr;

export namespace Artifact {
using namespace ArtifactCore;

enum class ShapeType { Rect = 0, Ellipse = 1, Star = 2, Polygon = 3, Line = 4, Triangle = 5, Square = 6 };

// Phase 3: Stroke style enums
enum class StrokeCap  { Flat = 0, Round = 1, Square = 2 };
enum class StrokeJoin { Miter = 0, Round = 1, Bevel = 2 };
enum class StrokeAlign { Center = 0, Inside = 1, Outside = 2 };

// Phase 5: Bezier vertex
struct CustomPathVertex {
  QPointF pos;
  QPointF inTangent;   // relative to pos
  QPointF outTangent;  // relative to pos
  bool smooth = false;
};

// Multi-content shape model: one layer holds N independently styled
// paths (geometry + fill + stroke + visibility + merge). Empty list =
// legacy single-primitive behavior. Boolean ops are resolved on CPU
// geometry; painting stays on the existing GPU triangle/line path.
enum class ShapeContentMerge { Add = 0, Subtract = 1, Intersect = 2, Difference = 3 };

// Ordered shape-group entries mirror the evaluation order used by After
// Effects: paths feed later operators, while fills and strokes consume the
// path result at their own position.  Content and operator indices refer to
// the existing owned arrays, so reordering the stack never duplicates style
// or operator state.
enum class ShapeStackNodeType { Path = 0, Fill = 1, Stroke = 2, Operator = 3 };

struct ShapeStackNode {
  ShapeStackNodeType type = ShapeStackNodeType::Path;
  int contentIndex = -1;
  int operatorIndex = -1;
  bool enabled = true;
};

struct ShapeGradientStop {
  float offset = 0.0f;
  FloatColor color = FloatColor(1.0f, 1.0f, 1.0f, 1.0f);
};

struct ShapeContentFill {
  bool enabled = true;
  FloatColor color = FloatColor(1.0f, 1.0f, 1.0f, 1.0f);
  ArtifactSolidFillType type = ArtifactSolidFillType::Solid;
  FloatColor gradientStart = FloatColor(1.0f, 1.0f, 1.0f, 1.0f);
  FloatColor gradientEnd = FloatColor(0.0f, 0.0f, 0.0f, 1.0f);
  // F10: optional multi-stop gradient. Empty = legacy 2-stop behaviour.
  std::vector<ShapeGradientStop> gradientStops;
  float gradientAngleDegrees = 0.0f;
  float gradientCenterX = 0.5f;
  float gradientCenterY = 0.5f;
  float gradientRadius = 0.5f;
};

struct ShapeContentStroke {
  bool enabled = false;
  FloatColor color = FloatColor(0.0f, 0.0f, 0.0f, 1.0f);
  float width = 0.0f;
  StrokeCap cap = StrokeCap::Flat;
  StrokeJoin join = StrokeJoin::Miter;
  StrokeAlign align = StrokeAlign::Center;
  std::vector<float> dashPattern;
  float dashOffset = 0.0f;
  float taperStart = 1.0f;
  float taperEnd = 1.0f;
  // F11b: taper ease exponent bias [-0.9, 3]. 0 = linear width ramp.
  float taperEase = 0.0f;
  // F11a: perpendicular sine wave along the path spine.
  bool waveEnabled = false;
  float waveAmount = 0.0f;     // px displacement, >= 0
  float waveFrequency = 1.0f;  // sine cycles along the path, >= 0
  float wavePhase = 0.0f;      // sine phase in cycles
  bool gradientEnabled = false;
  FloatColor gradientStart = FloatColor(0.0f, 0.0f, 0.0f, 1.0f);
  FloatColor gradientEnd = FloatColor(0.0f, 0.0f, 0.0f, 1.0f);
};

struct ShapeContentGeometry {
  ShapeType type = ShapeType::Rect;
  int width = 200;
  int height = 200;
  float cornerRadius = 0.0f;
  int starPoints = 5;
  float starInnerRadius = 0.382f;
  int polygonSides = 6;
  std::vector<QPointF> polygonPoints;
  bool polygonClosed = true;
  std::vector<CustomPathVertex> pathVertices;
  bool pathClosed = true;
  ArtifactCore::PathFillRule fillRule = ArtifactCore::PathFillRule::Winding;
};

struct ShapeContentTransform {
  QPointF anchor = QPointF(0.0, 0.0);
  QPointF position = QPointF(0.0, 0.0);
  QPointF scale = QPointF(1.0, 1.0);
  double rotation = 0.0;
  double skew = 0.0;
  double skewAxis = 0.0;
};

struct ShapeContent {
  QString name;
  ShapeContentGeometry geometry;
  ShapeContentTransform transform;
  ShapeContentFill fill;
  ShapeContentStroke stroke;
  bool visible = true;
  float opacity = 1.0f;
  ShapeContentMerge merge = ShapeContentMerge::Add;
};

class ArtifactShapeLayer : public ArtifactAbstract2DLayer {
private:
  class Impl;
  Impl *impl_;

public:
  ArtifactShapeLayer();
  ~ArtifactShapeLayer();

  void addShape();
  bool isShapeLayer() const;

  // Shape type
  void setShapeType(ShapeType type);
  ShapeType shapeType() const;

  // Size
  void setSize(int width, int height);
  int shapeWidth() const;
  int shapeHeight() const;

  // Style
  void setFillColor(const FloatColor &color);
  FloatColor fillColor() const;
  void setFillType(ArtifactSolidFillType type);
  ArtifactSolidFillType fillType() const;
  void setFillGradientStartColor(const FloatColor &color);
  FloatColor fillGradientStartColor() const;
  void setFillGradientEndColor(const FloatColor &color);
  FloatColor fillGradientEndColor() const;
  void setFillGradientAngleDegrees(float degrees);
  float fillGradientAngleDegrees() const;
  void setFillGradientCenterX(float value);
  float fillGradientCenterX() const;
  void setFillGradientCenterY(float value);
  float fillGradientCenterY() const;
  void setFillGradientRadius(float value);
  float fillGradientRadius() const;
  // F10: legacy multi-stop gradient (empty = 2-stop start/end behaviour).
  void setFillGradientStops(const std::vector<ShapeGradientStop>& stops);
  std::vector<ShapeGradientStop> fillGradientStops() const;
  void setStrokeColor(const FloatColor &color);
  FloatColor strokeColor() const;
  void setStrokeWidth(float width);
  float strokeWidth() const;
  void setFillEnabled(bool enabled);
  bool fillEnabled() const;
  void setStrokeEnabled(bool enabled);
  bool strokeEnabled() const;
  void setStrokeTaper(float startScale, float endScale);
  float strokeTaperStart() const;
  float strokeTaperEnd() const;
  // F11b: taper ease exponent bias [-0.9, 3]. 0 = linear width ramp.
  void setStrokeTaperEase(float ease);
  float strokeTaperEase() const;
  // F11a: perpendicular sine wave along the stroke spine.
  void setStrokeWaveEnabled(bool enabled);
  bool strokeWaveEnabled() const;
  void setStrokeWaveAmount(float px);
  float strokeWaveAmount() const;
  void setStrokeWaveFrequency(float cycles);
  float strokeWaveFrequency() const;
  void setStrokeWavePhase(float cycles);
  float strokeWavePhase() const;
  void setStrokeGradientEnabled(bool enabled);
  bool strokeGradientEnabled() const;
  void setStrokeGradientStartColor(const FloatColor &color);
  FloatColor strokeGradientStartColor() const;
  void setStrokeGradientEndColor(const FloatColor &color);
  FloatColor strokeGradientEndColor() const;
  std::vector<QPointF> direct3DCardFillPoints() const;
  FloatColor direct3DCardFillColor() const;
  std::vector<QPointF> direct3DCardStrokePoints() const;
  FloatColor direct3DCardStrokeColor() const;
  bool direct3DCardStrokeClosed() const;

  // Stroke styles (cap, join, align, dash) — Phase 3
  void setStrokeCap(StrokeCap cap);
  StrokeCap strokeCap() const;
  void setStrokeJoin(StrokeJoin join);
  StrokeJoin strokeJoin() const;
  void setStrokeAlign(StrokeAlign align);
  StrokeAlign strokeAlign() const;
  void setDashPattern(const std::vector<float>& pattern);
  std::vector<float> dashPattern() const;
  void setDashOffset(float offset);
  float dashOffset() const;

  // Corner radius (Rect)
  void setCornerRadius(float radius);
  float cornerRadius() const;

  // Star params
  void setStarPoints(int points);
  int starPoints() const;
  void setStarInnerRadius(float ratio);
  float starInnerRadius() const;

  // Polygon params
  void setPolygonSides(int sides);
  int polygonSides() const;

  // Editable polygon override
  bool hasCustomPolygon() const;
  void setCustomPolygonPoints(const std::vector<QPointF>& points, bool closed = true);
  void clearCustomPolygonPoints();
  std::vector<QPointF> customPolygonPoints() const;
  bool customPolygonClosed() const;

  // Bezier vertex path — Phase 5
  // Path keyframe animation. Vertices are stored per frame on the
  // animatable property "shape.path.keyframes"; drawing evaluates the
  // interpolated vertex set at the current timeline frame.
  void setPathKeyframe(int64_t frame,
                       const std::vector<CustomPathVertex>& verts);
  bool hasPathKeyframes() const;
  std::vector<CustomPathVertex> evaluatePathAt(int64_t frame) const;
  bool hasCustomPath() const;
  void setCustomPathVertices(const std::vector<CustomPathVertex>& vertices, bool closed = true);
  void clearCustomPath();
  std::vector<CustomPathVertex> customPathVertices() const;
  bool customPathClosed() const;
  ArtifactCore::PathFillRule customPathFillRule() const;
  void setCustomPathFillRule(ArtifactCore::PathFillRule rule);

  // Parametric-alive: custom polygon/path overrides are edits layered on
  // top of the numeric parametric base (type/size/corner/star/polygon).
  // The base is never destroyed by vertex editing, so overrides can always
  // be dropped to restore the parametric shape (AE breaks this by forcing
  // a destructive Bezier conversion).
  bool hasParametricOverrides() const;
  bool revertToParametric();
  int parametricBasePointCount() const;

  // Multi-content (1レイヤー複数パス). Empty list = legacy single shape.
   // addShapeContent snapshots the legacy shape as contents[0] on first use
   // so the existing look is preserved.
   int shapeContentCount() const;
   bool hasMultiShapeContents() const;
   int addShapeContent(const ShapeContent& content);
   bool setShapeContentAt(int index, const ShapeContent& content);
   ShapeContent shapeContentAt(int index) const;
   bool removeShapeContentAt(int index);
   void clearShapeContents();
   ShapeContent makeContentFromLegacy() const;

   // Active content index for per-content editing proxying.
   // -1 = legacy single-primitive mode (not a valid content).
   int activeContentIndex() const;
   bool setActiveContentIndex(int index);

   // Lightweight non-owning proxy for editing a specific content.
   // Edits flow directly into shapeContents_[index] via setShapeContentAt.
   class ShapeContentProxy {
   public:
     explicit ShapeContentProxy(ArtifactShapeLayer* layer, int index);
     bool isValid() const;
     ShapeContent content() const;
     void setContent(const ShapeContent& content);
     QString name() const;
     void setName(const QString& name);
     bool visible() const;
     void setVisible(bool visible);
     float opacity() const;
     void setOpacity(float opacity);
     ShapeContentMerge merge() const;
     void setMerge(ShapeContentMerge merge);
      ShapeContentFill fill() const;
      void setFill(const ShapeContentFill& fill);
      ShapeContentStroke stroke() const;
      void setStroke(const ShapeContentStroke& stroke);
      ShapeContentGeometry geometry() const;
      void setGeometry(const ShapeContentGeometry& geometry);
      bool duplicate();
   private:
     ArtifactShapeLayer* layer_ = nullptr;
     int index_ = -1;
     ShapeContent pull() const;
   };
  ShapeContentProxy activeContent();

  // Content management: duplicate, insert, reorder, swap.
  int duplicateShapeContent(int index);
  bool moveShapeContent(int fromIndex, int toIndex);
  bool insertShapeContent(int index, const ShapeContent& content);
  bool swapShapeContents(int a, int b);

  // Ordered group evaluation.  An empty stack preserves the legacy content
  // and operator evaluation model for existing projects.  New stacks are
  // persisted with the layer and can place path operations between path and
  // paint entries.
  int shapeStackNodeCount() const;
  ShapeStackNode shapeStackNodeAt(int index) const;
  bool setShapeStackNodeAt(int index, const ShapeStackNode& node);
  bool moveShapeStackNode(int fromIndex, int toIndex);
  bool insertShapeStackNode(int index, const ShapeStackNode& node);
  bool removeShapeStackNodeAt(int index);
  void clearShapeStackNodes();
  void resetShapeStackOrder();

    // SVG interop (ベクター受渡し). Export bakes merge-resolved paths;
  // taper strokes fall back to plain strokes, conical fills to solid.
  // Import converts rect/circle/ellipse/polygon/path + linear/radial
  // gradients into editable contents (transforms baked, coordinates
  // normalized to each content's bounds). Clipboard transport itself is
  // plain SVG text left to the caller; file errors return -1.
  QString shapeContentsToSvg() const;
  int addShapeContentsFromSvg(const QString& svgText);
  int importSvgFileContents(const QString& filePath);
  static std::vector<ShapeContent> parseShapeContentsFromSvg(const QString& svgText);

  // Backend-neutral geometry after applying the current operator stack.
  std::vector<ArtifactCore::ShapePath> nativeShapePaths() const;

  // Convert the currently evaluated shape geometry into editable mask paths.
  // The shape layer itself remains unchanged; callers decide whether to add
  // the returned mask as a one-shot conversion or use it as a live source.
  LayerMask createMaskFromShape() const;

  // Shape->Mask live link: the mask slot at live index follows shape
  // geometry automatically (AE has one-shot Auto-trace only). Geometry is
  // owned by the shape; per-path style (feather/opacity/expansion/...) on
  // the linked slot is preserved across refreshes. Refreshes never create
  // undo entries; only the link setup itself is undoable via the menu.
  bool shapeMaskLiveLink() const;
  int shapeMaskLiveIndex() const;
  void setShapeMaskLiveLink(bool enabled, int maskIndex = -1);
  void clearShapeMaskLiveLink();
  bool syncLiveLinkedMask();

  // Convert to a core ShapeLayer (processed paths + fill/stroke settings)
  // for vector export pipelines (e.g. SvgFrameExporter).
  ArtifactCore::ShapeLayer toCoreShapeLayer() const;

  // Shape operators (AE-style path operators)
  void addShapeOperator(ArtifactCore::ShapeOperatorType type);
  bool removeShapeOperatorAt(int index);
  bool moveShapeOperator(int fromIndex, int toIndex);
  void clearShapeOperators();
  int shapeOperatorCount() const;
  ArtifactCore::ShapeOperatorType shapeOperatorTypeAt(int index) const;
  void restoreOperatorsFromJson(const QJsonArray& operators);
  // F5: read-only viewport access to the numeric operator fields the main VP
  // can drag (trim start/end/offset, repeater copies/offset/rotation,
  // offset-paths offset, pucker amount, rounded radius). Anything else
  // returns an invalid QVariant; writes stay on setLayerPropertyValue.
  QVariant shapeOperatorValue(int index, const QString& field) const;

  // Layer interface
  QRectF localBounds() const override;
  std::vector<QPointF> collisionOutlineLocalPoints() const override;
  std::vector<ArtifactCore::PropertyGroup>
  getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString &propertyPath,
                              const QVariant &value) override;
  void draw(ArtifactIRenderer *renderer) override;
  QImage toQImage() const;
  QImage getThumbnail(int width = 128, int height = 128) const override;
  QJsonObject toJson() const override;
  static SharedPtr<ArtifactShapeLayer> fromJson(const QJsonObject &obj);

 private:
  // Rebuilds the cached per-content visible paths (merge-resolved).
  void ensureContentVisPaths() const;
  // Software rasterization for thumbnails/toQImage when contents exist.
  QImage renderContentsToImage() const;
};

} // namespace Artifact
