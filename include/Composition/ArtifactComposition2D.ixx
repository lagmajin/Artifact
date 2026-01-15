module;
#include <QList>
#include <QString>
#include <QJsonObject>

#include <wobjectdefs.h>

export module Artifact.Composition._2D;

import std;

import Utils.Id;
import Color.Float;
import Artifact.Layers;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;


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
  explicit ArtifactComposition(const CompositionID& id, const ArtifactCompositionInitParams& params);
  ~ArtifactComposition();





 };

 typedef std::shared_ptr<ArtifactComposition> ArtifactComposition2DPtr;





};