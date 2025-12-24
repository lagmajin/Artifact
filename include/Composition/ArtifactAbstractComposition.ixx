module;
#include <wobjectdefs.h>

#include <QObject>
export module Artifact.Composition.Abstract;

import std;
import Utils;

import Color;

import Frame.Rate;
import Frame.Range;
import Frame.Position;
import Container.MultiIndex;
import Artifact.Layers;
//import Artifact.Preview.Controller;

export namespace Artifact {

 using namespace ArtifactCore;

 class ArtifactAbstractComposition:public QObject {
  W_OBJECT(ArtifactAbstractComposition)
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactAbstractComposition();
  ~ArtifactAbstractComposition();
  void addLayer(ArtifactAbstractLayerPtr layer);

  ArtifactAbstractLayerPtr layerById(const LayerID& id);
  bool containsLayerById(const LayerID& id);

  void setBackGroundColor(const FloatColor& color);

  void setFramePosition(const FramePosition& position);
  void setTimeCode();

  void goToStartFrame();
  void goToEndFrame();
  void goToFrame(int64_t frameNumber = 0);
 	
  bool hasVideo() const;
  bool hasAudio() const;
 	
  QVector<ArtifactAbstractLayerPtr> allLayer();
 };

 typedef std::shared_ptr<ArtifactAbstractComposition> ArtifactCompositionPtr;
 typedef std::weak_ptr<ArtifactAbstractComposition>	  ArtifactCompositionWeakPtr;

 typedef MultiIndexContainer<ArtifactCompositionPtr, CompositionID> ArtifactCompositionMultiIndexContainer;

};
