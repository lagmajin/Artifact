module;
#include <utility>
#include <QObject>
#include <QJsonObject>


export module Artifact.Layer.AdjustableLayer;

import Artifact.Layers.Abstract._2D;

export namespace Artifact
{

class ArtifactAdjustableLayer:public ArtifactAbstract2DLayer
{
public:
  ArtifactAdjustableLayer();
  ~ArtifactAdjustableLayer();
  void setComposition(QObject* comp) override;
  void setComposition(void *comp) override;
  void draw(ArtifactIRenderer* renderer) override;
  QJsonObject toJson() const override;
  void fromJsonProperties(const QJsonObject& obj) override;
  bool isAdjustmentLayer() const override;
  bool isNullLayer() const override;

 };


}
