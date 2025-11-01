module;

export module Artifact.Layers.Solid2D;

import std;
import Color.Float;

export namespace Artifact
{
 using namespace ArtifactCore;
	
 class ArtifactSolid2DLayer
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactSolid2DLayer();
  ~ArtifactSolid2DLayer();

  void setColor(const FloatColor& color);
 };




}