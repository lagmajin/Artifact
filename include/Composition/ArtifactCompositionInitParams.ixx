module;

export module Artifact.Composition.InitParams;

import std;
import Utils.String.UniString;
import Size;
import Frame.Rate;
import Color.Float;
import Time.Code;
import Time.Rational;
import Core.AspectRatio;
import Preview.Quality;


export namespace Artifact {

 using namespace ArtifactCore;

 // Initialization parameters for an artifact composition
 class ArtifactCompositionInitParams {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactCompositionInitParams();
  explicit ArtifactCompositionInitParams(const UniString& name,const FloatColor& backgroundColor);
  ~ArtifactCompositionInitParams();
  FloatColor backgroundColor() const;
  void setBackgroundColor(const FloatColor& color);
  FrameRate framerate() const;
  void setFrameRate(const FrameRate& framerate);
  void setStartTimeCode();
  void setStopTimeCode();
  PreviewQuality quality() const;
  void setQuality(const PreviewQuality& quality);
  UniString compositionName() const;
  void setCompositionName(const UniString& name);
  //set duration

 };


}
