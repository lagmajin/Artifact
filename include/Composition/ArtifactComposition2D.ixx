module;
#include <QString>
#include <QJsonObject>



export module Composition._2D;

import std;

import Utils.Id;
import Color.Float;
import Artifact.Layers;

namespace Artifact {

 using namespace ArtifactCore;

 //class ArtifactComposition2DPrivate;



 class ArtifactComposition2D {
 private:
  class Impl;
  Impl* impl_;
  ArtifactComposition2D(const ArtifactComposition2D&) = delete;
  ArtifactComposition2D& operator=(const ArtifactComposition2D&) = delete;
 public:
  ArtifactComposition2D();
  ~ArtifactComposition2D();
  void setCompositionBackgroundColor(const FloatColor& color);
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