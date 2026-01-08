// ReSharper disable All
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
import Artifact.Composition.Result;

//struct AppendLayerToCompositionResult;

namespace Artifact {
 using namespace ArtifactCore;
	


 W_OBJECT_IMPL(ArtifactAbstractComposition)

 class ArtifactAbstractComposition::Impl {
 private:
  

 public:
  Impl();
  ~Impl();
  AppendLayerToCompositionResult appendLayerTop(ArtifactAbstractLayerPtr layer);
  AppendLayerToCompositionResult appendLayerBottom(ArtifactAbstractLayerPtr layer);
  bool containsLayerById(const LayerID& id) const;

  void removeAllLayers();
  const FramePosition framePosition() const;
  void setFramePosition(const FramePosition& position);
  void goToStartFrame();
  void goToEndFrame();
  void goToFrame(int64_t frame=0);
  QVector<ArtifactAbstractLayerPtr> allLayer() const;
  QVector<ArtifactAbstractLayerPtr> allLayerBackToFront() const;

  ArtifactAbstractLayerPtr frontMostLayer() const;
  ArtifactAbstractLayerPtr backMostLayer() const;
  bool hasVideo() const;
  bool hasAudio() const;
  MultiIndexLayerContainer layerMultiIndex_;
  CompositionSettings settings_;
  FramePosition position_;
  CompositionID id_;
 };

 ArtifactAbstractComposition::Impl::Impl()
 {

 }

 ArtifactAbstractComposition::Impl::~Impl()
 {

 }

 AppendLayerToCompositionResult ArtifactAbstractComposition::Impl::appendLayerTop(ArtifactAbstractLayerPtr layer)
 {
  AppendLayerToCompositionResult result;

  if (!layer) return {
	.success = false,
	.error = AppendLayerToCompositionError::LayerNotFound,
	.message = "Layer not found"
  };

  auto id = layer->id();

  layerMultiIndex_.add(layer,id,layer->type_index());

  return {
	.success = true,
	.error = AppendLayerToCompositionError::None,
	.message = "Layer added successfully"
  };
 }

 void ArtifactAbstractComposition::Impl::removeAllLayers()
 {
  layerMultiIndex_.clear();
 }

 bool ArtifactAbstractComposition::Impl::containsLayerById(const LayerID& id) const
 {
  return layerMultiIndex_.containsId(id);
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

 QVector<ArtifactAbstractLayerPtr> ArtifactAbstractComposition::Impl::allLayer() const
 {
  return QVector<ArtifactAbstractLayerPtr>();
 }

 AppendLayerToCompositionResult ArtifactAbstractComposition::Impl::appendLayerBottom(ArtifactAbstractLayerPtr layer)
 {
     AppendLayerToCompositionResult result;


	 return result;
 }

 bool ArtifactAbstractComposition::Impl::hasVideo() const
 {
  for (const auto& layer : layerMultiIndex_) {
   if (layer->hasVideo())
   {
	return true;
   }
  }
 	
  return false;
 }

 bool ArtifactAbstractComposition::Impl::hasAudio() const
 {
  for (const auto& layer : layerMultiIndex_) {
   if (layer->hasAudio())
   {
	return true;
   }
  }
  return false;
 }
	
 ArtifactAbstractComposition::ArtifactAbstractComposition(const CompositionID& id, const ArtifactCompositionInitParams& params) :impl_(new Impl())
 {
  impl_->id_ = id;
 	
 	
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

 QVector<Artifact::ArtifactAbstractLayerPtr> ArtifactAbstractComposition::allLayer()
 {

  return QVector<ArtifactAbstractLayerPtr>();
 }

 AppendLayerToCompositionResult ArtifactAbstractComposition::appendLayerTop(ArtifactAbstractLayerPtr layer)
 {
  return impl_->appendLayerTop(layer);
 }

 AppendLayerToCompositionResult ArtifactAbstractComposition::appendLayerBottom(ArtifactAbstractLayerPtr layer)
 {
   return impl_->appendLayerBottom(layer);
 }

 void ArtifactAbstractComposition::insertLayerAt(ArtifactAbstractLayerPtr layer, int index/*=0*/)
 {

 }

 void ArtifactAbstractComposition::removeLayer(const LayerID& id)
 {

 }

 void ArtifactAbstractComposition::removeAllLayers()
 {

 }

 ArtifactAbstractLayerPtr ArtifactAbstractComposition::frontMostLayer() const
 {
    return impl_->frontMostLayer();
 }

CompositionID ArtifactAbstractComposition::id() const
 {
 return CompositionID();
 }

};