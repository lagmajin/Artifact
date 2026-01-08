module;
#include <QJsonObject>
export module Composition3D;

import std;

import Artifact.Composition.Abstract;

export namespace Artifact {

 enum eCompositionType {
  OriginalComposition,
  PreCompose,
 };

 //class ArtifactCompositionPrivate;

 class ArtifactComposition3D :public ArtifactAbstractComposition{
 private: 
  class Impl;
 	
  //std::unique_ptr<ArtifactCompositionPrivate> pImpl_;
 public:
 

 };

}