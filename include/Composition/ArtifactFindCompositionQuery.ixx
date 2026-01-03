module;
#include <QString>
export module Artifact.Composition.FindQuery;

import std;

export namespace Artifact {

 class ArtifactFindCompositionQuery {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactFindCompositionQuery();
  ~ArtifactFindCompositionQuery();
};


};