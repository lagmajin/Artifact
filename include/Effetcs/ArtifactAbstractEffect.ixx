
module;
#include <QString>
export module Artifact.Effects;

import std;

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

 };

};