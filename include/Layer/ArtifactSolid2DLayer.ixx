module;

export module Artifact.Layers.Solid2D;

import std;
import Color.Float;

export namespace Artifact
{
 class ArtifactSolid2DLayer
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactSolid2DLayer();
  ~ArtifactSolid2DLayer();
 };




}