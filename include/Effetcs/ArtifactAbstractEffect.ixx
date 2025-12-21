
module;
#include <QString>
export module Artifact.Effect.Abstract;

import std;
import Image.ImageF32x4RGBAWithCache;

export namespace Artifact {

 enum class ComputeMode {
  CPU,
  GPU,
  AUTO // おまかせモード
 };

 class ArtifactAbstractEffect {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactAbstractEffect();
  virtual ~ArtifactAbstractEffect();
  ComputeMode computeMode() const;
  void setComputeMode(ComputeMode mode);
  virtual bool supportsGPU() const = 0;
 };

};