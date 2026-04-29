module;
#include <utility>


export module Artifact.Layer.AdjustableLayer;

import Artifact.Layers.Abstract._2D;

export namespace Artifact
{

class ArtifactAdjustableLayer:public ArtifactAbstract2DLayer
{
private:
  class Impl;
  Impl* impl_;
public:
  ArtifactAdjustableLayer();
  ~ArtifactAdjustableLayer();
  void setComposition(void *comp) override;
  void draw(ArtifactIRenderer* renderer) override;
  bool isAdjustmentLayer() const override;
  bool isNullLayer() const override;

 };


}
