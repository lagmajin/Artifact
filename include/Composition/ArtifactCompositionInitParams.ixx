module;

export module Artifact.Composition.InitParams;

import std;
import Frame.Rate;
import Color.Float;
import Time.Rational;
import Core.AspectRatio;



export namespace Artifact {

 using namespace ArtifactCore;

 // Initialization parameters for an artifact composition
 class ArtifactCompositionInitParams {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactCompositionInitParams();
  ~ArtifactCompositionInitParams();
  FloatColor backgroundColor() const;
  void setBackgroundColor(const FloatColor& color);
  
  //set duration

 };


}