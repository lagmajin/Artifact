module;
#include <wobjectdefs.h>

#include <QObject>
export module Artifact.Composition.Abstract;

import std;
import Utils;

import Color.Float;

import Frame.Rate;
import Frame.Range;
import Frame.Position;
import Container.MultiIndex;
import Artifact.Layers;
import Artifact.Composition.InitParams;
//import Artifact.Layer.Abstract;
//import Artifact.Composition.Result;
//import Artifact.Preview.Controller;



export namespace Artifact {

 using namespace ArtifactCore;
 struct AppendLayerToCompositionResult;

 class ArtifactAbstractComposition:public QObject {
  W_OBJECT(ArtifactAbstractComposition)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactAbstractComposition(const CompositionID& id,const ArtifactCompositionInitParams& params);
  ~ArtifactAbstractComposition();
  AppendLayerToCompositionResult appendLayerTop(ArtifactAbstractLayerPtr layer);
  AppendLayerToCompositionResult appendLayerBottom(ArtifactAbstractLayerPtr layer);
  void insertLayerAt(ArtifactAbstractLayerPtr layer, int index=0);
  void removeLayer(const LayerID& id);
  void removeAllLayers();
  CompositionID id() const;
  int layerCount() const;

  ArtifactAbstractLayerPtr layerById(const LayerID& id);
  bool containsLayerById(const LayerID& id);

  ArtifactAbstractLayerPtr frontMostLayer() const;
  ArtifactAbstractLayerPtr backMostLayer() const;

  void bringToFront(ArtifactAbstractLayer* layer);
  void sendToBack(ArtifactAbstractLayer* layer);

  void setBackGroundColor(const FloatColor& color);
  FramePosition framePosition() const;
  void setFramePosition(const FramePosition& position);
  void setTimeCode();

  void goToStartFrame();
  void goToEndFrame();
  void goToFrame(int64_t frameNumber = 0);
   
  bool isPlaying() const;
 	
  bool hasVideo() const;
  bool hasAudio() const;
  bool isAudioOnly() const;
  bool isVisual() const;

  QJsonDocument toJson() const;

  QVector<ArtifactAbstractLayerPtr> allLayer();
 };

 typedef std::shared_ptr<ArtifactAbstractComposition> ArtifactCompositionPtr;
 typedef std::weak_ptr<ArtifactAbstractComposition>	  ArtifactCompositionWeakPtr;

 typedef MultiIndexContainer<ArtifactCompositionPtr, CompositionID> ArtifactCompositionMultiIndexContainer;
 using MultiIndexLayerContainer = MultiIndexContainer<ArtifactAbstractLayerPtr, LayerID>;
};
