module;
#include <QString>
export module Artifact.Composition.Setting;
import std;
import Color.Float;
import Core.AspectRatio;
import Utils.String.UniString;

export namespace Artifact {

 class ArtifactCompositionSetting {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactCompositionSetting();
  ~ArtifactCompositionSetting();
 };



};