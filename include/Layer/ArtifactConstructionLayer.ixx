module;
#include <utility>
#include <QString>
#include <QJsonObject>
#include <QVariant>
#include <vector>

export module Artifact.Layer.Construction;

import Artifact.Layers.Abstract._2D;
import Artifact.Render.IRenderer;

export namespace Artifact {

class ArtifactConstructionLayer : public ArtifactAbstract2DLayer {
private:
  class Impl;
  Impl* impl_;

public:
  ArtifactConstructionLayer();
  ~ArtifactConstructionLayer();

  void draw(ArtifactIRenderer* renderer) override;
  bool isNullLayer() const override;
  bool hasVideo() const override;
  QJsonObject toJson() const override;
  void fromJsonProperties(const QJsonObject& obj) override;
  bool isConstructionLayer() const override;
  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;
};

} // namespace Artifact
