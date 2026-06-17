module;
#include <QString>
#include <QJsonObject>
#include <QVariant>
#include <vector>

export module Artifact.Layer.CompositionBackground;

import Artifact.Layers.Abstract._2D;
import Artifact.Render.IRenderer;

export namespace Artifact {

class ArtifactCompositionBackgroundLayer : public ArtifactAbstract2DLayer {
private:
  class Impl;
  Impl* impl_;

public:
  ArtifactCompositionBackgroundLayer();
  ~ArtifactCompositionBackgroundLayer();

  void draw(ArtifactIRenderer* renderer) override;
  bool isNullLayer() const override;
  bool hasVideo() const override;
  QJsonObject toJson() const override;
  void fromJsonProperties(const QJsonObject& obj) override;
  bool isCompositionBackgroundLayer() const override;
  bool shouldIncludeInFinalRender() const override;
  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;
};

} // namespace Artifact
