module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

export module Artifact.Layer.Shape;

import Color.Float;
import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;

export namespace Artifact {
using namespace ArtifactCore;

enum class ShapeType { Rect = 0, Ellipse = 1, Star = 2, Polygon = 3, Line = 4, Triangle = 5, Square = 6 };

class ArtifactShapeLayer : public ArtifactAbstractLayer {
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
  void setStrokeColor(const FloatColor &color);
  FloatColor strokeColor() const;
  void setStrokeWidth(float width);
  float strokeWidth() const;
  void setFillEnabled(bool enabled);
  bool fillEnabled() const;
  void setStrokeEnabled(bool enabled);
  bool strokeEnabled() const;

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

  // Layer interface
  QRectF localBounds() const override;
  std::vector<ArtifactCore::PropertyGroup>
  getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString &propertyPath,
                              const QVariant &value) override;
  void draw(ArtifactIRenderer *renderer) override;
  QImage toQImage() const;
  QJsonObject toJson() const override;
  static std::shared_ptr<ArtifactShapeLayer> fromJson(const QJsonObject &obj);
};

} // namespace Artifact
