module;

#include "Define/DllExportMacro.hpp"
export module Artifact.Layers.Abstract._2D;

import std;

import Artifact.Layers.Abstract;


export namespace Artifact
{

 class   Artifact2DLayer : public ArtifactAbstractLayer {
 private:
  class Impl;
  Impl* impl_;
 public:
  Artifact2DLayer();
  ~Artifact2DLayer();
 };

 Artifact2DLayer::Artifact2DLayer()
 {

 }

 Artifact2DLayer::~Artifact2DLayer()
 {

 }












};