
module;
#include <QString>
export module Artifact.Effect.Abstract;

import std;
import Utils.Id;
import Utils.String.UniString;
import Artifact.Effect.Context;
import Image.ImageF32x4RGBAWithCache;

export namespace Artifact {

 using namespace ArtifactCore;

 enum class ComputeMode {
  CPU,
  GPU,
  AUTO // おまかせモード
 };



 class ArtifactAbstractEffect {
 private:
  class Impl;
  Impl* impl_;
 protected:
  virtual void apply();
 public:
  ArtifactAbstractEffect();
  virtual ~ArtifactAbstractEffect();
  ComputeMode computeMode() const;
  void setComputeMode(ComputeMode mode);
  virtual bool supportsGPU() const = 0;
 
  UniString effectID() const;
  UniString displayName() const;
   
 };

};