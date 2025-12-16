module ;
#include <qforeach.h>
#include <wobjectimpl.h>
#include <QHash>
#include <QVector>
#include <QMultiMap>
#include <typeindex>
module Artifact.Composition.Abstract;


import std;

import Container;
import Frame.Position;

import Composition.Settings;


namespace Artifact {
 using namespace ArtifactCore;
	
 using LayerContainer = MultiIndexContainer<ArtifactAbstractLayerPtr, LayerID>;

 W_OBJECT_IMPL(ArtifactAbstractComposition)

 class ArtifactAbstractComposition::Impl {
 private:
  QVector<ArtifactAbstractLayerPtr> layers_;
  //QHash<LayerID, ArtifactAbstractLayerPtr> layersById_;
  //QMultiHash<std::type_index, ArtifactAbstractLayerPtr> layersByType_;
  LayerContainer layerMultiIndex_;
  CompositionSettings settings_;
  FramePosition position_;
 public:
  Impl();
  ~Impl();
  void addLayer(ArtifactAbstractLayerPtr layer);
  bool containsLayerById(const LayerID& id) const;

  void removeAllLayers();
  const FramePosition framePosition() const;
  void setFramePosition(const FramePosition& position);
  void goToStartFrame();
  void goToEndFrame();
  void goToFrame(int64_t frame=0);
 };

 ArtifactAbstractComposition::Impl::Impl()
 {

 }

 ArtifactAbstractComposition::Impl::~Impl()
 {

 }

 void ArtifactAbstractComposition::Impl::addLayer(ArtifactAbstractLayerPtr layer)
 {
  if (!layer) return;
  auto id = layer->id();

  layerMultiIndex_.add(layer,id,layer->type_index());

 }

 bool ArtifactAbstractComposition::Impl::containsLayerById(const LayerID& id) const
 {
  return true;
 }

 void ArtifactAbstractComposition::Impl::removeAllLayers()
 {

 }

 void ArtifactAbstractComposition::Impl::goToStartFrame()
 {
  //goToFrame(0);
 }

 void ArtifactAbstractComposition::Impl::goToEndFrame()
 {

 }

 void ArtifactAbstractComposition::Impl::setFramePosition(const FramePosition& position)
 {

 	
 }

 const FramePosition ArtifactAbstractComposition::Impl::framePosition() const
 {
  return position_;
 }

 void ArtifactAbstractComposition::Impl::goToFrame(int64_t frame/*=0*/)
 {
  
 }

 ArtifactAbstractComposition::ArtifactAbstractComposition():impl_(new Impl())
 {

 }
 ArtifactAbstractComposition::~ArtifactAbstractComposition()
 {
  delete impl_;
 }

 bool ArtifactAbstractComposition::containsLayerById(const LayerID& id)
 {
  return impl_->containsLayerById(id);
 }

 ArtifactAbstractLayerPtr ArtifactAbstractComposition::layerById(const LayerID& id)
 {

  return nullptr;
 }

 void ArtifactAbstractComposition::addLayer(ArtifactAbstractLayerPtr layer)
 {
  return impl_->addLayer(layer);
 }

 void ArtifactAbstractComposition::setBackGroundColor(const FloatColor& color)
 {

 }
 
 void ArtifactAbstractComposition::setFramePosition(const FramePosition& position)
 {

 }

 void ArtifactAbstractComposition::goToStartFrame()
 {
  impl_->goToEndFrame();
 }

 void ArtifactAbstractComposition::goToEndFrame()
 {
  impl_->goToEndFrame();
 }

 void ArtifactAbstractComposition::goToFrame(int64_t frameNumber /*= 0*/)
 {

 }

 bool ArtifactAbstractComposition::hasVideo() const
 {
  return true;
 }

 bool ArtifactAbstractComposition::hasAudio() const
 {
  return true;
 }


};