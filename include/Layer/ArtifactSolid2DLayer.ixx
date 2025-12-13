module;

export module Artifact.Layer.Solid2D;

import std;
import Color.Float;
import Artifact.Layers.Abstract;

export namespace Artifact
{
 using namespace ArtifactCore;
	
 class ArtifactSolid2DLayer:public ArtifactAbstractLayer
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactSolid2DLayer();
  ~ArtifactSolid2DLayer();
  FloatColor color() const;
  void setColor(const FloatColor& color);

  void draw() override;

 };




}