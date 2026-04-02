module;
#include <QImage>

export module Artifact.Layers.SolidImage;

import std;
import Color.Float;
import Artifact.Layer.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

class ArtifactSolidImageLayerSettings {
public:
  ArtifactSolidImageLayerSettings();
  ~ArtifactSolidImageLayerSettings();
};

class ArtifactSolidImageLayer : public ArtifactAbstractLayer {
private:
  class Impl;
  Impl *impl_;

public:
  ArtifactSolidImageLayer();
  ~ArtifactSolidImageLayer();

  FloatColor color() const;
  void setColor(const FloatColor &color);
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
