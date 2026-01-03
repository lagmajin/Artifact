module;
#include <QJsonObject>
module Artifact.Composition._2D;

import Artifact.Layer.Abstract;

import std;
//import ArtifactCore
import Composition.Settings;
import Artifact.Composition.Abstract;

namespace Artifact {

 using namespace ArtifactCore;

 class ArtifactComposition::Impl
 {
 private:
  //QVector<ArtifactAbstractLayerPtr> layers_;
  //MultiIndexLayerContainer layers_;
  
 public:
  Impl();
  ~Impl();
 };

 ArtifactComposition::Impl::Impl()
 {

 }

 ArtifactComposition::Impl::~Impl()
 {

 }



ArtifactComposition::ArtifactComposition():impl_(new Impl)
{

}

ArtifactComposition::~ArtifactComposition()
{
 delete impl_;
}





};
