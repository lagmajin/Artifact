module;
#include <utility>
#include <vector>

#include <QImage>
export module Artifact.Layers.SolidImage;
import Color.Float;
import Artifact.Layer.InitParams;
import Artifact.Layers.Abstract._2D;

export namespace Artifact {
using namespace ArtifactCore;

class ArtifactSolidImageLayerSettings {
public:
  ArtifactSolidImageLayerSettings();
  ~ArtifactSolidImageLayerSettings();
};

class ArtifactSolidImageLayer : public ArtifactAbstract2DLayer {
private:
  class Impl;
  Impl *impl_;

public:
  ArtifactSolidImageLayer();
  ~ArtifactSolidImageLayer();

  FloatColor color() const;
  void setColor(const FloatColor &color);
  ArtifactSolidFillType fillType() const;
  void setFillType(ArtifactSolidFillType fillType);
  bool isGradientEnabled() const;
  FloatColor gradientStartColor() const;
  void setGradientStartColor(const FloatColor& color);
  FloatColor gradientEndColor() const;
  void setGradientEndColor(const FloatColor& color);
  float gradientAngleDegrees() const;
  void setGradientAngleDegrees(float degrees);
  bool gradientReverse() const;
  void setGradientReverse(bool reverse);
  float gradientCenterX() const;
  void setGradientCenterX(float value);
  float gradientCenterY() const;
  void setGradientCenterY(float value);
  float gradientScale() const;
  void setGradientScale(float value);
  float gradientOffset() const;
  void setGradientOffset(float value);
  void setSize(int width, int height);
  QJsonObject toJson() const override;
  void fromJsonProperties(const QJsonObject &obj) override;
  std::vector<ArtifactCore::PropertyGroup>
  getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString &propertyPath,
                             const QVariant &value) override;
  void draw(ArtifactIRenderer *renderer) override;
  QImage toQImage() const;
};
} // namespace Artifact
