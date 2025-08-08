
module;
#include <QString>
export module Artifact.Effects;

import std;

export namespace Artifact {



 class ArtifactAbstractEffect {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactAbstractEffect();
  virtual ~ArtifactAbstractEffect();

 };

};