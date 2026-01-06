module;

export module Artifact.Composition.InitParams;

import std;
import Size;
import Frame.Rate;
import Color.Float;
import Time.Code;
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

  FrameRate framerate() const;
  void setFrameRate(const FrameRate& framerate);
  void setStartTimeCode();
  void setStopTimeCode();
  void setQuality();
  //set duration

 };


}