module ;
export module Artifact.Layer.Shape;

import std;

export namespace Artifact
{
 class ArtifactShapeLayer
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactShapeLayer();
  ~ArtifactShapeLayer();
  void addShape();
  bool isShapeLayer() const;
 };









};