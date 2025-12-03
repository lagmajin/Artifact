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

 class ArtifactComposition2D :public ArtifactAbstractComposition{
 private:
  class Impl;
  Impl* impl_;
  ArtifactComposition2D(const ArtifactComposition2D&) = delete;
  ArtifactComposition2D& operator=(const ArtifactComposition2D&) = delete;
 public:
  ArtifactComposition2D();
  ~ArtifactComposition2D();
  void addLayer();
  void resize(int width, int height);
  void removeLayer(ArtifactAbstractLayer* layer);
  void removeAllLayer();

  int layerCount() const;
  QJsonDocument toJson() const;


  ArtifactAbstractLayer* frontMostLayer() const;
  ArtifactAbstractLayer* backMostLayer() const;

  void bringToFront(ArtifactAbstractLayer* layer);
  void sendToBack(ArtifactAbstractLayer* layer);
 };

 typedef std::shared_ptr<ArtifactComposition2D> ArtifactComposition2DPtr;





};