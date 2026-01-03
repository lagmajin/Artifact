module;
#include <QString>
#include <QJsonObject>

#include <wobjectdefs.h>

export module Artifact.Composition._2D;

import std;

import Utils.Id;
import Color.Float;
import Artifact.Layers;
import Artifact.Composition.Abstract;

export namespace Artifact {

 using namespace ArtifactCore;

 //class ArtifactComposition2DPrivate;

 class ArtifactComposition :public ArtifactAbstractComposition{
 private:
  class Impl;
  Impl* impl_;
  ArtifactComposition(const ArtifactComposition&) = delete;
  ArtifactComposition& operator=(const ArtifactComposition&) = delete;
 public:
  ArtifactComposition();
  ~ArtifactComposition();





 };

 typedef std::shared_ptr<ArtifactComposition> ArtifactComposition2DPtr;





};