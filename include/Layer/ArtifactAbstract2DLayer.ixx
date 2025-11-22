module;

#include "Define/DllExportMacro.hpp"
export module Artifact.Layers.Abstract._2D;

import std;

import Artifact.Layers.Abstract;


export namespace Artifact
{

 class   ArtifactAbstract2DLayer : public ArtifactAbstractLayer {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactAbstract2DLayer();
  ~ArtifactAbstract2DLayer();

 };












};